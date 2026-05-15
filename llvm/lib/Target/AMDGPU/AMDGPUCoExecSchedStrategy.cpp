//===- AMDGPUCoExecSchedStrategy.cpp - CoExec Scheduling Strategy ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Coexecution-focused scheduling strategy for AMDGPU.
//
//===----------------------------------------------------------------------===//

#include "AMDGPUCoExecSchedStrategy.h"
#include "AMDGPUBarrierLatency.h"
#include "GCNHazardRecognizer.h"
#include "GCNSubtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MathExtras.h"
#include <queue>

using namespace llvm;
using namespace llvm::AMDGPU;

#define DEBUG_TYPE "machine-scheduler"

// BFS depth limit for lookahead search (search-budget, not hardware-derived).
static constexpr unsigned ShadowMixLookaheadDepth = 8;

// Default VGPR excess threshold percent for coexec scheduler.
static constexpr unsigned DefaultVGPRExcessThresholdPercent = 100;

namespace {
enum class CoexecExposedMode { Off, Greedy, Roofline };
} // namespace

static cl::opt<CoexecExposedMode> CoexecExposedSort(
    "amdgpu-coexec-exposed-sort", cl::Hidden,
    cl::init(CoexecExposedMode::Roofline),
    cl::desc("Prioritize HardwareUnits with non-zero exposed cycles in the "
             "coexec scheduler's critical-resource sort."),
    cl::values(
        clEnumValN(CoexecExposedMode::Off, "off",
                   "Disabled (default; sort behavior unchanged)."),
        clEnumValN(CoexecExposedMode::Greedy, "greedy",
                   "Hand-ordered slot allocation (DS, SALU, TRANS, VALU)."),
        clEnumValN(CoexecExposedMode::Roofline, "roofline",
                   "Per-class exposed cycles derived from the roofline "
                   "max-flow solution.")));

static cl::opt<bool> BlockCarriedLatency(
  "amdgpu-block-carried-latency",
  cl::desc("Whether or not to pad the beginning of blocks with latency from incoming loads"),
  cl::ReallyHidden,
  cl::init(false));

namespace {

// Used to disable post-RA scheduling with function level granularity.
class GCNNoopPostScheduleDAG final : public ScheduleDAGInstrs {
public:
  explicit GCNNoopPostScheduleDAG(MachineSchedContext *C)
      : ScheduleDAGInstrs(*C->MF, C->MLI, /*RemoveKillFlags=*/true) {}

  // Do nothing.
  void schedule() override {}
};

/// Map an InstructionFlavor to its CoExecMask bit for the roofline analysis.
/// Returns 0 for flavors that cannot fill co-exec slots.
uint8_t flavorToCoExecMask(InstructionFlavor F) {
  switch (F) {
  case InstructionFlavor::SingleCycleVALU:
  case InstructionFlavor::MultiCycleVALU:
    return CoExecMask::VALU;
  case InstructionFlavor::TRANS:
    return CoExecMask::TRANS;
  case InstructionFlavor::SALU:
    return CoExecMask::SALU;
  case InstructionFlavor::DS:
  case InstructionFlavor::DMA:
    return CoExecMask::DS;
  case InstructionFlavor::VMEM:
    return CoExecMask::VMEM;
  case InstructionFlavor::WMMA:
    return CoExecMask::WMMA;
  default:
    return 0;
  }
}

/// Get the bit index (0-7) for a single CoExecMask bit.
unsigned coexecBitIndex(uint8_t Bit) {
  assert(Bit && (Bit & (Bit - 1)) == 0 && "Must be a single bit");
  return llvm::countr_zero(Bit);
}

/// Tiny max-flow solver (Edmonds-Karp) for the roofline bipartite matching.
/// The graph has at most ~14 nodes so fixed-size arrays suffice.
struct TinyMaxFlow {
  static constexpr unsigned MaxNodes = 20;
  int Capacity[MaxNodes][MaxNodes];
  /// Snapshot of forward capacities recorded by addEdge(), used to recover
  /// per-edge flow after solve() (flow = Original - residual capacity).
  int Original[MaxNodes][MaxNodes];
  unsigned NumNodes = 0;

  void init(unsigned N) {
    assert(N <= MaxNodes);
    NumNodes = N;
    memset(Capacity, 0, sizeof(Capacity));
    memset(Original, 0, sizeof(Original));
  }

  void addEdge(unsigned From, unsigned To, int Cap) {
    Capacity[From][To] += Cap;
    Original[From][To] += Cap;
  }

  /// Forward flow on the original edge (From, To) after solve(): the BFS
  /// drains Capacity[From][To] by exactly the flow it pushes, so the
  /// difference recovers it.
  int getFlow(unsigned From, unsigned To) const {
    return Original[From][To] - Capacity[From][To];
  }

  int solve() {
    unsigned Source = 0;
    unsigned Sink = NumNodes - 1;
    int TotalFlow = 0;
    int Parent[MaxNodes];

    while (true) {
      // BFS to find augmenting path.
      memset(Parent, -1, sizeof(Parent));
      Parent[Source] = static_cast<int>(Source);
      std::queue<unsigned> Q;
      Q.push(Source);
      while (!Q.empty() && Parent[Sink] == -1) {
        unsigned U = Q.front();
        Q.pop();
        for (unsigned V = 0; V < NumNodes; ++V) {
          if (Parent[V] == -1 && Capacity[U][V] > 0) {
            Parent[V] = static_cast<int>(U);
            Q.push(V);
          }
        }
      }
      if (Parent[Sink] == -1)
        break;

      // Find bottleneck.
      int PathFlow = std::numeric_limits<int>::max();
      for (unsigned V = Sink; V != Source; V = Parent[V])
        PathFlow = std::min(PathFlow, Capacity[Parent[V]][V]);

      // Update residual.
      for (unsigned V = Sink; V != Source; V = Parent[V]) {
        unsigned U = Parent[V];
        Capacity[U][V] -= PathFlow;
        Capacity[V][U] += PathFlow;
      }
      TotalFlow += PathFlow;
    }
    return TotalFlow;
  }
};

} // namespace

unsigned
RooflineResult::getWMMACoexecByFlavor(AMDGPU::InstructionFlavor Flavor) {
  uint8_t Bit = flavorToCoExecMask(Flavor);
  unsigned Index = coexecBitIndex(Bit);
  return WMMACoexecByClass[Index];
}

static SUnit *pickOnlyChoice(SchedBoundary &Zone) {
  // pickOnlyChoice() releases pending instructions and checks for new hazards.
  SUnit *OnlyChoice = Zone.pickOnlyChoice();
  if (!Zone.Pending.empty())
    return nullptr;

  return OnlyChoice;
}

InstructionFlavor llvm::AMDGPU::classifyFlavor(const MachineInstr &MI,
                                               const SIInstrInfo &SII) {
  if (MI.isDebugInstr())
    return InstructionFlavor::Other;

  unsigned Opc = MI.getOpcode();

  // Check for specific opcodes first.
  if (Opc == AMDGPU::ATOMIC_FENCE || Opc == AMDGPU::S_WAIT_ASYNCCNT ||
      Opc == AMDGPU::S_WAIT_TENSORCNT || Opc == AMDGPU::S_BARRIER_WAIT ||
      Opc == AMDGPU::S_BARRIER_SIGNAL_IMM || SII.isWaitcnt(Opc))
    return InstructionFlavor::Fence;

  if (SII.isLDSDMA(MI))
    return InstructionFlavor::DMA;

  if (SII.isMFMAorWMMA(MI))
    return InstructionFlavor::WMMA;

  if (SII.isTRANS(MI))
    return InstructionFlavor::TRANS;

  if (SII.isVALU(MI)) {
    if (SII.getRepeatRate(MI) > 1)
      return InstructionFlavor::MultiCycleVALU;

    return InstructionFlavor::SingleCycleVALU;
  }

  if (SII.isDS(MI))
    return InstructionFlavor::DS;

  if (SII.isFLAT(MI) || SII.isFLATGlobal(MI) || SII.isFLATScratch(MI) || SII.isVMEM(MI))
    return InstructionFlavor::VMEM;

  if (SII.isSALU(MI))
    return InstructionFlavor::SALU;

  return InstructionFlavor::Other;
}

//===----------------------------------------------------------------------===//
// RegionMixInfo
//===----------------------------------------------------------------------===//

void RegionMixInfo::reset() {
  constexpr unsigned N =
      static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS);
  for (unsigned I = 0; I < N; ++I) {
    ReadyCount[I] = 0;
    ScheduledCount[I] = 0;
  }
  SnapshotDirty = true;
}

void RegionMixInfo::recordScheduled(InstructionFlavor Flavor) {
  ScheduledCount[static_cast<unsigned>(Flavor)]++;
}

void RegionMixInfo::refreshFromBoundary(SchedBoundary &Zone,
                                        const SIInstrInfo &SII) {
  if (!SnapshotDirty)
    return;
  constexpr unsigned N =
      static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS);
  for (unsigned I = 0; I < N; ++I)
    ReadyCount[I] = 0;

  // Snapshot Available + Pending: both have NumPredsLeft == 0 (DAG-ready).
  // Pending entries would cause a structural stall this cycle but are still
  // candidates the picker may select, so ShadowMix counts them as "ready".
  auto Bump = [&](SUnit *SU) {
    unsigned Idx =
        static_cast<unsigned>(classifyFlavor(*SU->getInstr(), SII));
    ReadyCount[Idx]++;
  };
  for (SUnit *SU : Zone.Available.elements())
    Bump(SU);
  for (SUnit *SU : Zone.Pending.elements())
    Bump(SU);
  SnapshotDirty = false;
}

//===----------------------------------------------------------------------===//
// WindowSlotDemand
//===----------------------------------------------------------------------===//

WindowSlotDemand
WindowSlotDemand::fromCoExecInfo(const CoExecInfo &Info) {
  WindowSlotDemand Demand;
  for (unsigned S = 0; S < Info.TotalWindow; ++S) {
    uint8_t Mask = Info.getMask(S);
    switch (Mask) {
    case CoExecMask::StageE0:
      // Control-only stage, no filler needed.
      break;
    case CoExecMask::StageE:
      Demand.ESlots++;
      break;
    case CoExecMask::StageI:
      Demand.ISlots++;
      break;
    case CoExecMask::StageV:
      Demand.VSlots++;
      break;
    case CoExecMask::StageTR:
      Demand.TRSlots++;
      break;
    default:
      // Permissive or unknown stage — count as I-slot (most permissive
      // non-trivial type).
      if (Mask & CoExecMask::VALU)
        Demand.ISlots++;
      else if (Mask != CoExecMask::None)
        Demand.ESlots++;
      break;
    }
  }
  return Demand;
}

bool WindowSlotDemand::isSatisfied(const RegionMixInfo &Mix) const {
  unsigned ReadyVALU1c = Mix.getReadyCount(InstructionFlavor::SingleCycleVALU);
  unsigned ReadyTRANS = Mix.getReadyCount(InstructionFlavor::TRANS);
  unsigned ReadyVMEM = Mix.getReadyCount(InstructionFlavor::VMEM);
  unsigned ReadyDS = Mix.getReadyCount(InstructionFlavor::DS);
  unsigned ReadySALU = Mix.getReadyCount(InstructionFlavor::SALU);

  // I-slots accept VALU/TRANS/VMEM (all take 1 issue cycle).
  unsigned ReadyICompat = ReadyVALU1c + ReadyTRANS + ReadyVMEM;

  // E-slots accept SALU/DS.
  unsigned ReadyECompat = ReadyDS + ReadySALU;

  // TR-slots accept all but TRANS.
  unsigned ReadyTRCompat = ReadyVALU1c + ReadyVMEM + ReadyDS + ReadySALU;

  if (ReadyICompat < ISlots)
    return false;
  if (ReadyECompat < ESlots)
    return false;
  if (ReadyTRCompat < TRSlots)
    return false;

  return true;
}

InstructionFlavor
WindowSlotDemand::getMostDeficientFlavor(const RegionMixInfo &Mix) const {
  unsigned ReadyVALU1c = Mix.getReadyCount(InstructionFlavor::SingleCycleVALU);
  unsigned ReadyTRANS = Mix.getReadyCount(InstructionFlavor::TRANS);
  unsigned ReadyVMEM = Mix.getReadyCount(InstructionFlavor::VMEM);
  unsigned ReadyDS = Mix.getReadyCount(InstructionFlavor::DS);
  unsigned ReadySALU = Mix.getReadyCount(InstructionFlavor::SALU);

  // Compute deficit for each filler class relative to slot demand.
  // I-slots need VALU/TRANS/VMEM.
  int IDeficit = static_cast<int>(ISlots) -
                 static_cast<int>(ReadyVALU1c + ReadyTRANS + ReadyVMEM);

  // E-slots need DS or SALU.
  int EDeficit =
      static_cast<int>(ESlots) - static_cast<int>(ReadyDS + ReadySALU);

  int TRDeficit =
      static_cast<int>(TRSlots) -
      static_cast<int>(ReadyVALU1c + ReadyDS + ReadySALU + ReadyVMEM);

  int MaxDeficit = std::max({IDeficit, EDeficit, TRDeficit});

  // Break ties: if all have equal deficit, prefer enabling VALU (typically
  // scarcer and higher-value for I-slots / TR-skits).
  if (MaxDeficit == EDeficit && EDeficit > IDeficit)
    return InstructionFlavor::DS;

  return InstructionFlavor::SingleCycleVALU;
}

//===----------------------------------------------------------------------===//
// CoexecWindow
//===----------------------------------------------------------------------===//

/// Get the co-execution window size for a HardwareUnitInfo by querying
/// CoExecInfo from a representative instruction.
static unsigned getCoexecWindowSize(SUnit *RepSU, const SIInstrInfo &SII) {
  if (!RepSU->getInstr())
    return 0;

  CoExecInfo Info = getCoExecInfo(*RepSU->getInstr(), SII);
  return Info.TotalWindow;
}

/// Compute the number of slots that would be missed (unfilled) if we select
/// the given producer flavor. Lower is better.
///
/// For WMMA: slots missed based on E + I slot demand vs ready compatible
/// fillers. For TRANS/MultiCycleVALU: query CoExecInfo to determine slot
/// preferences, ensuring each ready instruction is only counted once across
/// all slots.
static unsigned computeSlotsMissed(const RegionMixInfo &MixInfo, SUnit *RepSU,
                                   const SIInstrInfo &SII,
                                   RooflineResult &Roofline) {
  CoExecInfo Info = getCoExecInfo(*RepSU->getInstr(), SII);
  if (Info.TotalWindow == 0)
    return 0;

  InstructionFlavor Producer = classifyFlavor(*RepSU->getInstr(), SII);

  // Track remaining available instructions per flavor. Each instruction can
  // only fill one slot, so we decrement as we consume them.
  unsigned Remaining[static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS)];
  for (unsigned F = 0;
       F < static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS); ++F)
    Remaining[F] = MixInfo.getReadyCount(static_cast<InstructionFlavor>(F));

  unsigned SlotsMissed = 0;
  unsigned FilledESlots = 0;
  unsigned FilledISlots = 0;

  auto adjustFilledSlots = [&](uint8_t Mask) {
    if (Producer == InstructionFlavor::WMMA) {
      switch (Mask) {
      case CoExecMask::StageI: {
        ++FilledISlots;
        break;
      }
      case CoExecMask::StageE: {
        ++FilledESlots;
        break;
      }
      default:
        break;
      }
    }
  };

  for (unsigned S = 0; S < Info.TotalWindow; ++S) {
    uint8_t Mask = Info.getMask(S);
    if (Mask == CoExecMask::StageE0 || Mask == CoExecMask::None ||
        Mask == CoExecMask::StageV)
      continue;

    // Try to find one instruction to fill this slot.
    // First check preferred flavors, then fall back to any allowed flavor.
    bool Filled = false;
    FlavorMask Preferred = Info.getPreferredFlavors(S);

    // Helper to try consuming one instruction of a given flavor.
    auto TryConsume = [&](InstructionFlavor F) -> bool {
      unsigned Idx = static_cast<unsigned>(F);
      if (Remaining[Idx] > 0) {
        Remaining[Idx]--;
        return true;
      }
      return false;
    };

    // Try preferred flavors first.
    if (Preferred != FlavorMasks::None) {
      for (unsigned F = 0;
           F < static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS) && !Filled;
           ++F) {
        if ((Preferred & flavorBit(static_cast<InstructionFlavor>(F))) &&
            TryConsume(static_cast<InstructionFlavor>(F))) {
          Filled = true;
          break;
        }
      }
    }

    if (Filled) {
      adjustFilledSlots(Mask);
      continue;
    }

    // If no preferred filler found, try any flavor allowed by the mask.
    for (unsigned F = 0;
         F < static_cast<unsigned>(InstructionFlavor::NUM_FLAVORS) && !Filled;
         ++F) {
      if ((flavorBit(static_cast<InstructionFlavor>(F))) &&
          TryConsume(static_cast<InstructionFlavor>(F))) {
        Filled = true;
        break;
      }
    }

    if (Filled) {
      adjustFilledSlots(Mask);
      continue;
    }

    // We missed the slot
    if (Producer != InstructionFlavor::WMMA) {
      ++SlotsMissed;
      continue;
    }

    // Missed WMMA slot, check if our region roofline supports this slot.
    switch (Mask) {
    case CoExecMask::StageI: {
      unsigned SupportedISlots = Roofline.getWMMAISlotSupplySlots();
      if (FilledISlots < SupportedISlots) {
        ++SlotsMissed;
      }
      break;
    }
    case CoExecMask::StageE: {
      unsigned SupportedESlots = Roofline.getWMMAESlotSupplySlots();
      if (FilledESlots < SupportedESlots) {
        ++SlotsMissed;
      }
      break;
    }
    default: {
      ++SlotsMissed;
      break;
    }
    }
  }

  return SlotsMissed;
}

void CoexecWindow::populate(InstructionFlavor PreferredFlavor,
                            const SmallVectorImpl<HardwareUnitInfo> &HWUInfo,
                            const RegionMixInfo &MixInfo,
                            const SIInstrInfo &SII, RooflineResult &Roofline) {
  clear();

  // If the preferred flavor is a window producer, use it directly.
  for (const auto &HWUI : HWUInfo) {
    if (HWUI.getType() == PreferredFlavor && HWUI.producesCoexecWindow()) {
      SUnit *RepSU = HWUI.getNextTargetSU(true);
      if (RepSU) {
        ProducerFlavor = PreferredFlavor;
        Demand = WindowSlotDemand::fromCoExecInfo(
            getCoExecInfo(*RepSU->getInstr(), SII));
        if (HWUI.getType() == InstructionFlavor::WMMA)
          Demand.clamp(Roofline);
        IsPopulated = Demand.hasSlots();
        return;
      }
    }
  }

  // Otherwise, select the producer based on ready instructions and demand.
  // Priority:
  // 1. Producers that are ready AND have satisfied demand
  //    Tiebreak: first by fewest slots missed, then by largest window size.
  // 2. Fallback to fewest slots missed, then largest window size.

  InstructionFlavor BestSatisfiedFlavor = InstructionFlavor::Other;
  unsigned BestSatisfiedSlotsMissed = std::numeric_limits<unsigned>::max();
  unsigned BestSatisfiedSize = 0;

  InstructionFlavor BestFallbackFlavor = InstructionFlavor::Other;
  unsigned BestFallbackSlotsMissed = std::numeric_limits<unsigned>::max();
  unsigned BestFallbackSize = 0;

  for (const auto &HWUI : HWUInfo) {
    if (!HWUI.producesCoexecWindow())
      continue;

    SUnit *RepSU = HWUI.getNextTargetSU(true);
    if (!RepSU || !RepSU->getInstr())
      continue;

    InstructionFlavor Flavor = HWUI.getType();
    unsigned Size = getCoexecWindowSize(RepSU, SII);
    unsigned SlotsMissed = computeSlotsMissed(MixInfo, RepSU, SII, Roofline);

    // Check if this producer flavor has any ready instructions.
    unsigned ReadyCount = MixInfo.getReadyCount(Flavor);
    if (ReadyCount > 0) {
      // Prefer fewest slots missed, then largest window size.
      if (SlotsMissed < BestSatisfiedSlotsMissed ||
          (SlotsMissed == BestSatisfiedSlotsMissed &&
           Size > BestSatisfiedSize)) {
        BestSatisfiedSlotsMissed = SlotsMissed;
        BestSatisfiedSize = Size;
        BestSatisfiedFlavor = Flavor;
      }

      continue;
    }

    // Producer isn't ready, consider it as a fallback option.
    if (SlotsMissed < BestFallbackSlotsMissed ||
        (SlotsMissed == BestFallbackSlotsMissed && Size > BestFallbackSize)) {
      BestFallbackSlotsMissed = SlotsMissed;
      BestFallbackSize = Size;
      BestFallbackFlavor = Flavor;
    }
  }

  // If we didn't find any candidate producer, just return the cleared window
  if (!BestSatisfiedSize && !BestFallbackSize)
    return;

  ProducerFlavor =
      (BestSatisfiedSize > 0) ? BestSatisfiedFlavor : BestFallbackFlavor;
  // If the preferred flavor is a window producer, use it directly.
  for (const auto &HWUI : HWUInfo) {
    if (HWUI.getType() == ProducerFlavor && HWUI.producesCoexecWindow()) {
      SUnit *RepSU = HWUI.getNextTargetSU(true);
      if (RepSU) {
        Demand = WindowSlotDemand::fromCoExecInfo(
            getCoExecInfo(*RepSU->getInstr(), SII));
        IsPopulated = Demand.hasSlots();
        if (HWUI.getType() == InstructionFlavor::WMMA)
          Demand.clamp(Roofline);
        return;
      }
    }
  }
  return;
}

SUnit *HardwareUnitInfo::getNextTargetSU(bool LookDeep) const {
  for (auto *PrioritySU : PrioritySUs) {
    if (!PrioritySU->isTopReady())
      return PrioritySU;
  }

  if (!LookDeep)
    return nullptr;

  unsigned MinDepth = std::numeric_limits<unsigned int>::max();
  SUnit *TargetSU = nullptr;
  for (auto *SU : AllSUs) {
    if (SU->isScheduled)
      continue;

    if (SU->isTopReady())
      continue;

    if (SU->getDepth() < MinDepth) {
      MinDepth = SU->getDepth();
      TargetSU = SU;
    }
  }
  return TargetSU;
}

void HardwareUnitInfo::insert(SUnit *SU, unsigned BlockingCycles) {
#ifndef NDEBUG
  bool Inserted = AllSUs.insert(SU);
  assert(Inserted);
#else
  AllSUs.insert(SU);
#endif

  TotalCycles += BlockingCycles;

  if (PrioritySUs.empty()) {
    PrioritySUs.insert(SU);
    return;
  }
  unsigned SUDepth = SU->getDepth();
  unsigned CurrDepth = (*PrioritySUs.begin())->getDepth();
  if (SUDepth > CurrDepth)
    return;

  if (SUDepth == CurrDepth) {
    PrioritySUs.insert(SU);
    return;
  }

  // SU is lower depth and should be prioritized.
  PrioritySUs.clear();
  PrioritySUs.insert(SU);
}

void HardwareUnitInfo::markScheduled(SUnit *SU, unsigned BlockingCycles) {
  // We may want to ignore some HWUIs (e.g. InstructionFlavor::Other). To do so,
  // we just clear the HWUI. However, we still have instructions which map to
  // this HWUI. Don't bother managing the state for these HWUI.
  if (TotalCycles == 0)
    return;

  ScheduledSUs.push_back(SU);
  AllSUs.remove(SU);
  PrioritySUs.remove(SU);

  if (BufferSize <= 1 || (ScheduledSUs.size() % BufferSize == 0))
    TotalCycles -= BlockingCycles;

  if (AllSUs.empty())
    return;
  if (PrioritySUs.empty()) {
    for (auto SU : AllSUs) {
      if (PrioritySUs.empty()) {
        PrioritySUs.insert(SU);
        continue;
      }
      unsigned SUDepth = SU->getDepth();
      unsigned CurrDepth = (*PrioritySUs.begin())->getDepth();
      if (SUDepth > CurrDepth)
        continue;

      if (SUDepth == CurrDepth) {
        PrioritySUs.insert(SU);
        continue;
      }

      // SU is lower depth and should be prioritized.
      PrioritySUs.clear();
      PrioritySUs.insert(SU);
    }
  }
}

void HardwareUnitInfo::finalizeCycles() {
  if (BufferSize <= 1 || AllSUs.empty())
    return;

  // We estimate the amount of cycles it takes to free up a slot in the buffer
  // as the average cycles per SU.
  BufferCycles = TotalCycles / AllSUs.size();
  // The TotalCycles is normalized against the BufferSize.
  // This provides an estimate of the TotalCycles which is not always accurate
  // -- particularly in cases where we have fewer instructions than the
  // BufferSize. For example, if we have 2 instructions which each take 50
  // cycles and a BufferSize of 16, then a TotalCycles of 51 cycles would be
  // somewhat accurate. This normalization calculates TotalCycles as 6. However,
  // if we have 64 of these instructions, our normalized estimate of 200 is more
  // reasonable, given the more accurate measure is 264. Having a completely
  // accurate measure is not very important, since this metric is mainly used to
  // compare the relative demand per HardwareUnit across the region. The simpler
  // estimate makes managing the metric incrementally during scheduling much
  // simpler.
  TotalCycles /= BufferSize;
}

HardwareUnitInfo *
CandidateHeuristics::getHWUIFromFlavor(InstructionFlavor Flavor) {
  for (auto &HWUICand : HWUInfo) {
    if (HWUICand.getType() == Flavor) {
      return &HWUICand;
    }
  }
  return nullptr;
}

unsigned CandidateHeuristics::getMaxBlockingCycles(const MCSchedClassDesc *SC,
                                                   const MachineInstr *MI) {
  // Loads and stores are not pipelined.
  if (MI->mayLoadOrStore())
    return SchedModel->computeInstrLatency(MI, false);

  unsigned ReleaseAtCycle = 0;
  for (TargetSchedModel::ProcResIter PI = SchedModel->getWriteProcResBegin(SC),
                                     PE = SchedModel->getWriteProcResEnd(SC);
       PI != PE; ++PI) {
    ReleaseAtCycle = std::max({ReleaseAtCycle, (unsigned)PI->ReleaseAtCycle,
                               (unsigned)PI->RepeatRate});
  }
  return ReleaseAtCycle;
}

unsigned CandidateHeuristics::getHWUICyclesForSU(SUnit *SU) {
  assert(SchedModel && SchedModel->hasInstrSchedModel());

  // DS load/store latency is variable depending on LDS contention.
  if (SII->getSubtarget().hasGFX1250Insts() && SII->isDS(*SU->getInstr())) {
    if (auto Latency = SIInstrInfo::getDSLatencyMode())
      return *Latency;
  }

  return getMaxBlockingCycles(DAG->getSchedClass(SU), SU->getInstr());
}

unsigned CandidateHeuristics::getHWUICyclesForMI(MachineInstr *MI) {
  assert(SchedModel && SchedModel->hasInstrSchedModel());

  // DS load/store latency is variable depending on LDS contention.
  if (SII->getSubtarget().hasGFX1250Insts() && SII->isDS(*MI)) {
    if (auto Latency = SIInstrInfo::getDSLatencyMode())
      return *Latency;
  }

  return getMaxBlockingCycles(SchedModel->resolveSchedClass(MI), MI);
}

void CandidateHeuristics::updateForScheduling(SUnit *SU,
                                              SchedBoundary *Zone) {
  auto Flavor = classifyFlavor(*SU->getInstr(), *SII);
  unsigned Latency = getHWUICyclesForSU(SU);
  HardwareUnitInfo *HWUI = getHWUIFromFlavor(Flavor);
  assert(HWUI);
  HWUI->markScheduled(SU, Latency);
  MixInfo.recordScheduled(Flavor);
  // Mix snapshot is now stale; tryShadowMix will refresh on its next call.
  MixInfo.invalidate();

  // Decrement RemainingExposed for this flavor only when this SU was not
  // hidden inside an active window. Producers are never "hidden" by
  // someone else's shadow.
  if (CoexecExposedSort != CoexecExposedMode::Off) {
    bool InShadow =
        CurrentWindow.IsActive && !HWUI->producesCoexecWindow();
    if (!InShadow)
      HWUI->reduceRemainingExposed();
  }

  // --- Window lifecycle management (bumpNode) ---

  // Check if the current window has expired.
  if (CurrentWindow.isExpired(SU->TopReadyCycle)) {
    CurrentWindow.clear();
    // Try to populate a new current window.
    CoexecWindow Temp;
    Temp.populate(InstructionFlavor::Other, HWUInfo, MixInfo, *SII, Roofline);
    if (Temp.IsPopulated) {
      CurrentWindow = Temp;
      NextWindow.clear();
    }
  }

  // If the scheduled instruction matches the current window's producer,
  // activate the window.
  if (CurrentWindow.IsPopulated && !CurrentWindow.IsActive &&
      HWUI->producesCoexecWindow() && Flavor == CurrentWindow.ProducerFlavor) {
    CurrentWindow.activate(SU->TopReadyCycle, std::max(Latency, 1u));
    LLVM_DEBUG(dbgs() << "CoexecWindow: activated " << getFlavorName(Flavor)
                      << " window [" << CurrentWindow.StartCycle << ", "
                      << CurrentWindow.EndCycle << "]\n");
  }
}

void CandidateHeuristics::initialize(ScheduleDAGMI *SchedDAG,
                                     const TargetSchedModel *TargetSchedModel,
                                     const TargetRegisterInfo *TRI) {
  DAG = SchedDAG;
  SchedModel = TargetSchedModel;
  assert(SchedModel && SchedModel->hasInstrSchedModel());

  SRI = static_cast<const SIRegisterInfo *>(TRI);
  SII = static_cast<const SIInstrInfo *>(DAG->TII);

  HWUInfo.resize((int)InstructionFlavor::NUM_FLAVORS);

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    HWUInfo[I].reset();
    HWUInfo[I].setType(I);
  }

  HWUInfo[(int)InstructionFlavor::WMMA].setProducesCoexecWindow(true);
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].setProducesCoexecWindow(true);
  HWUInfo[(int)InstructionFlavor::TRANS].setProducesCoexecWindow(true);
  HWUInfo[(int)InstructionFlavor::DS].setBufferSize(DefaultBufferSizes::DS);

  collectRegionSummary();
  MixInfo.reset();
  computeRooflineCoExec();
  switch (CoexecExposedSort) {
  case CoexecExposedMode::Off:
    break;
  case CoexecExposedMode::Greedy:
    initExposedGreedy();
    break;
  case CoexecExposedMode::Roofline:
    initExposedRoofline();
    break;
  }
  LLVM_DEBUG(dumpRegionSummary());
  CurrentWindow.clear();
  NextWindow.clear();

  // Populate the initial window for the first producer.
  // Note: At initialization time, MixInfo is reset so no ready counts yet.
  // The window will be re-populated when scheduling begins and MixInfo is
  // fresh.
  CurrentWindow.populate(InstructionFlavor::Other, HWUInfo, MixInfo, *SII,
                         Roofline);
  LLVM_DEBUG({
    if (CurrentWindow.IsPopulated)
      dbgs() << "  Initial window: producer="
             << getFlavorName(CurrentWindow.ProducerFlavor) << "\n";
  });
}

unsigned CandidateHeuristics::getCarriedLatency(SUnit *SU) {
  if (!BlockCarriedLatency)
    return 0;

  MachineInstr *MI = SU->getInstr();
  unsigned CarriedLatency = 0;

  const InstructionFlavor Flavor = classifyFlavor(*MI, *SII);
  if (Flavor == InstructionFlavor::Fence) {
    MachineBasicBlock *MBB = MI->getParent();
    // Check if we have DS instruciton after a fence in any of the predecessor
    // blocks, if so, this fence instruction has carried latency.
    for (auto PredMBB : MBB->predecessors()) {
      auto I = PredMBB->rbegin();
      auto E = PredMBB->rend();
      for (; I != E; I++) {
        const InstructionFlavor ItFlavor = classifyFlavor(*I, *SII);
        if (ItFlavor == InstructionFlavor::Fence)
          break;

        // Found carried latency.
        if (ItFlavor == InstructionFlavor::DS)
          return getHWUICyclesForMI(&*I);
      }
    }
  }

  for (auto &Op : MI->operands()) {
    if (!Op.isReg())
      continue;
    if (!Op.isUse())
      continue;
    auto Reg = Op.getReg();
    if (!Reg.isVirtual())
      continue;

    for (auto &Def : DAG->MRI.def_instructions(Reg)) {
      // We don't have the proper modelling to accurately measure all carried
      // latency. Just try to measure carried latency for long latency loads to
      // avoid long stalls.
      if (!Def.mayLoad())
        continue;

      unsigned Latency = getHWUICyclesForMI(&Def);

      // Load is carried across block
      if (Def.getParent() != MI->getParent()) {
        bool FoundUseInDefBlock = false;
        for (auto &Use : DAG->MRI.use_nodbg_instructions(Reg)) {
          if (Use.getParent() != Def.getParent())
            continue;

          SlotIndex DefIdx = DAG->getLIS()->getInstructionIndex(Def);
          SlotIndex UseIdx = DAG->getLIS()->getInstructionIndex(Use);
          // We have a use of this load in the def block that occurs after the
          // load. In this case we must wait for the load in the def block, and
          // we do not have any carried latency from this load.
          if (SlotIndex::isEarlierInstr(DefIdx, UseIdx)) {
            FoundUseInDefBlock = true;
            break;
          }
        }
        if (!FoundUseInDefBlock)
          CarriedLatency = std::max(Latency, CarriedLatency);

        continue;
      }

      assert(Def.getParent() == MI->getParent());
      // Load is in the same block
      SlotIndex LoadIdx = DAG->getLIS()->getInstructionIndex(Def);
      SlotIndex UseIdx = DAG->getLIS()->getInstructionIndex(*MI);
      // The load occurs after this use -- the latency is carried across loop
      // backedge.
      if (SlotIndex::isEarlierInstr(UseIdx, LoadIdx))
        CarriedLatency = std::max(Latency, CarriedLatency);
    }
  }
  return CarriedLatency;
}

void CandidateHeuristics::collectRegionSummary() {
  if (!SchedModel || !SchedModel->hasInstrSchedModel())
    return;

  for (auto &SU : DAG->SUnits) {
    MachineInstr *MI = SU.getInstr();
    const InstructionFlavor Flavor = classifyFlavor(*MI, *SII);
    HWUInfo[(int)(Flavor)].insert(&SU, getHWUICyclesForSU(&SU));
    unsigned CarriedLatency = getCarriedLatency(&SU);
    if (CarriedLatency)
      CarriedLatencies[MI] = CarriedLatency;
  }

  for (auto &HWUI : HWUInfo) {
    HWUI.finalizeCycles();
  }
}

void CandidateHeuristics::dumpRegionSummary() {
  MachineBasicBlock *BB = DAG->begin()->getParent();
  dbgs() << "\n=== Region: " << DAG->MF.getName() << " BB" << BB->getNumber()
         << " (" << DAG->SUnits.size() << " SUs) ===\n";

  dbgs() << "\nHWUI Resource Pressure:\n";
  bool ShowExposed = CoexecExposedSort != CoexecExposedMode::Off;
  for (auto &HWUI : HWUInfo) {
    if (HWUI.getTotalCycles() == 0)
      continue;

    StringRef Name = getFlavorName(HWUI.getType());
    dbgs() << "  " << Name << ": " << HWUI.getTotalCycles() << " cycles, "
           << HWUI.size() << " instrs";
    if (ShowExposed)
      dbgs() << ", exposed=" << HWUI.getRemainingExposed();
    dbgs() << "\n";
  }
  dbgs() << "\n";

  if (Roofline.isValid()) {
    dbgs() << "Roofline Co-Exec Analysis:\n";
    dbgs() << "  Total coexec slots: " << Roofline.TotalSlots << "\n";
    dbgs() << "  Max fillable slots: " << Roofline.MaxFilledSlots << "\n";
    dbgs() << "  Lower bound stalls: " << Roofline.LowerBoundStalls << "\n";
    dbgs() << "  Slot utilization:   "
           << format("%.1f%%", Roofline.getSlotUtilization() * 100) << "\n";
    static const char *Names[] = {"CTRL", "VALU", "TRANS", "SALU",
                                  "DS",   "VMEM", "SMEM",  "WMMA"};
    for (unsigned I = 0; I < 8; ++I) {
      if (Roofline.ConsumerCount[I])
        dbgs() << "  " << Names[I]
               << " consumers: " << Roofline.ConsumerCount[I] << "\n";
    }
    dbgs() << "\n";
  }
}

void CandidateHeuristics::computeRooflineCoExec() {
  Roofline = RooflineResult();

  // Slot types: map from stage bitmask to count of slots with that mask.
  // Use uint16_t key (rather than uint8_t) so 0xFF is a usable mask value
  // — the IS slot's StageIS = StageI | WMMA = 0xFF, and uint8_t's
  // DenseMapInfo reserves 0xFF as the empty-key sentinel.
  // FUTURE: if we add a separate ScaleWMMA flavor, the IS slot would be
  // restricted to it (rather than any WMMA), affecting how the roofline
  // distributes WMMA flow between IS and V slots.
  SmallDenseMap<uint16_t, unsigned, 16> SlotTypes;
  unsigned NumWMMAs = 0;

  for (auto &SU : DAG->SUnits) {
    const MachineInstr &MI = *SU.getInstr();
    InstructionFlavor Flavor = classifyFlavor(MI, *SII);

    if (Flavor == InstructionFlavor::WMMA) {
      // Producer: enumerate coexec slots from this WMMA's stage pattern.
      CoExecInfo Info = getCoExecInfo(MI, *SII);
      for (unsigned S = 0; S < Info.TotalWindow; ++S) {
        uint8_t Mask = Info.getMask(S);
        // Skip E0 stages (CTRL-only, filled by hazard recognizer).
        if (Mask == CoExecMask::StageE0)
          continue;
        SlotTypes[Mask]++;
      }
      NumWMMAs++;
      continue;
    }

    // Consumer: map to CoExecMask bit.
    uint8_t Bit = flavorToCoExecMask(Flavor);
    // Catch SMEM instructions that classifyFlavor maps to Other.
    if (!Bit && SII->isSMRD(MI))
      Bit = CoExecMask::SMEM;
    if (!Bit)
      continue;
    Roofline.ConsumerCount[coexecBitIndex(Bit)]++;
    Roofline.TotalConsumers++;
  }

  Roofline.WMMACount = NumWMMAs;

  // WMMAs can also consume V-stage slots of preceding WMMAs.
  if (NumWMMAs > 1)
    Roofline.ConsumerCount[coexecBitIndex(CoExecMask::WMMA)] += NumWMMAs - 1;

  // TRANS shadow producer (excess-capacity model).
  // Each TRANS executes in 2 cycles; its 2nd cycle is a free StageTR slot
  // when the TRANS doesn't fit into a WMMA I/IS slot (which would absorb
  // it into the window). Per SPG §5.3.5.2.2, the shadow accepts core/side
  // MACC + off-pipe ops (modeled by StageTR = All & ~TRANS). Count only
  // the excess so we don't double-count TRANS that land in WMMA windows.
  unsigned WMMAIcap =
      SlotTypes[CoExecMask::StageI] + SlotTypes[CoExecMask::StageIS];
  unsigned NumTRANS =
      Roofline.ConsumerCount[coexecBitIndex(CoExecMask::TRANS)];
  if (NumTRANS > WMMAIcap)
    SlotTypes[CoExecMask::StageTR] += NumTRANS - WMMAIcap;

  // Compute total slots.
  for (auto &[Mask, Count] : SlotTypes)
    Roofline.TotalSlots += Count;

  if (Roofline.TotalSlots == 0)
    return;

  // Identify active consumer classes (those with count > 0).
  SmallVector<unsigned, 8> ActiveClasses;
  for (unsigned I = 0; I < 8; ++I) {
    if (Roofline.ConsumerCount[I] > 0)
      ActiveClasses.push_back(I);
  }

  // Collect slot types into a vector for indexing.
  SmallVector<std::pair<uint16_t, unsigned>, 8> SlotTypeVec(SlotTypes.begin(),
                                                            SlotTypes.end());

  // Build flow network.
  // Nodes: 0=source, 1..C=consumer classes, C+1..C+T=slot types, C+T+1=sink.
  unsigned C = ActiveClasses.size();
  unsigned T = SlotTypeVec.size();
  unsigned NumNodes = 1 + C + T + 1;

  TinyMaxFlow MF;
  MF.init(NumNodes);

  unsigned Source = 0;
  unsigned Sink = NumNodes - 1;

  // Source -> consumer class edges.
  for (unsigned I = 0; I < C; ++I) {
    unsigned ClassNode = 1 + I;
    unsigned Nk = Roofline.ConsumerCount[ActiveClasses[I]];
    MF.addEdge(Source, ClassNode, Nk);
  }

  // Slot type -> sink edges, and consumer class -> slot type edges.
  for (unsigned J = 0; J < T; ++J) {
    unsigned SlotNode = 1 + C + J;
    unsigned Mt = SlotTypeVec[J].second;
    uint16_t Mask = SlotTypeVec[J].first;
    MF.addEdge(SlotNode, Sink, Mt);

    for (unsigned I = 0; I < C; ++I) {
      unsigned BitIdx = ActiveClasses[I];
      uint8_t ClassBit = 1u << BitIdx;
      if (Mask & ClassBit) {
        unsigned Nk = Roofline.ConsumerCount[BitIdx];
        MF.addEdge(1 + I, SlotNode, std::min(Nk, Mt));
      }
    }
  }

  Roofline.MaxFilledSlots = MF.solve();
  Roofline.LowerBoundStalls = Roofline.TotalSlots - Roofline.MaxFilledSlots;

  // Recover per-class fill from the solver and derive exposed counts.
  for (unsigned I = 0; I < C; ++I) {
    unsigned BitIdx = ActiveClasses[I];
    unsigned ClassNode = 1 + I;
    unsigned Fill = 0;
    for (unsigned J = 0; J < T; ++J)
      Fill += MF.getFlow(ClassNode, 1 + C + J);
    unsigned Nk = Roofline.ConsumerCount[BitIdx];
    Roofline.ExposedByClass[BitIdx] = (Fill >= Nk) ? 0 : Nk - Fill;
    Roofline.WMMACoexecByClass[BitIdx] = Fill;
  }
}

void CandidateHeuristics::initExposedGreedy() {
  // Hand-ordered allocation of WMMA + MultiVALU shadow slots to filler
  // flavors, matching the prior PipelinedScheduler heuristic. Whatever
  // each flavor cannot fit into a shadow slot is its "exposed" count.

  unsigned WMMACycles = HWUInfo[(int)InstructionFlavor::WMMA].getTotalCycles();
  unsigned DSCycles = HWUInfo[(int)InstructionFlavor::DS].getTotalCycles();
  unsigned MultiVALUCycles =
      HWUInfo[(int)InstructionFlavor::MultiCycleVALU].getTotalCycles();
  unsigned SingleCycleVALUCycles =
      HWUInfo[(int)InstructionFlavor::SingleCycleVALU].getTotalCycles();
  unsigned TRANSCycles = HWUInfo[(int)InstructionFlavor::TRANS].getTotalCycles();

  unsigned WMMACount = HWUInfo[(int)InstructionFlavor::WMMA].size();
  unsigned MultiVALUCount =
      HWUInfo[(int)InstructionFlavor::MultiCycleVALU].size();
  unsigned DSCount = HWUInfo[(int)InstructionFlavor::DS].size();
  unsigned SALUCount = HWUInfo[(int)InstructionFlavor::SALU].size();
  // FIXME: the prior PipelinedScheduler reads EXPCount from
  // InstructionFlavor::DS; preserved verbatim for behavior parity. Almost
  // certainly intended to be TRANS::size().
  unsigned EXPCount = HWUInfo[(int)InstructionFlavor::DS].size();
  unsigned SingleCycleVALUCount =
      HWUInfo[(int)InstructionFlavor::SingleCycleVALU].size();

  // Slot subcategories from the first WMMA's stage template. Assumes a
  // single WMMA shape per region (matches prior FIXME). Our CoExecInfo
  // masks coalesce DS-eligible and SALU-eligible E-stages into StageE, and
  // VALU/TRANS-eligible I-stages into StageI, so we treat all E-slots as
  // DS-eligible and all I-slots as TRANS-eligible. Coarser than the prior
  // alternating MemCoExec0/2 split, but the same intent.
  unsigned ESlotCount = 0;
  unsigned ISlotCount = 0;
  if (WMMACount) {
    for (auto &SU : DAG->SUnits) {
      if (classifyFlavor(*SU.getInstr(), *SII) != InstructionFlavor::WMMA)
        continue;
      CoExecInfo Info = getCoExecInfo(*SU.getInstr(), *SII);
      for (unsigned S = 0; S < Info.TotalWindow; ++S) {
        uint8_t Mask = Info.getMask(S);
        if (Mask == CoExecMask::StageE0)
          continue;
        bool HasVALU = Mask & CoExecMask::VALU;
        bool HasTRANS = Mask & CoExecMask::TRANS;
        if (HasVALU || HasTRANS)
          ++ISlotCount;
        else if (Mask & (CoExecMask::SALU | CoExecMask::MEM))
          ++ESlotCount;
      }
      break;
    }
  }
  unsigned ESlotForDS = ESlotCount;
  unsigned ISlotForTrans = ISlotCount;

  unsigned WMMAESlot = WMMACount * ESlotCount;
  unsigned WMMAISlot = WMMACount * ISlotCount;
  unsigned WMMAESlotForDS = WMMACount * ESlotForDS;
  unsigned WMMAISlotForTRANS = WMMACount * ISlotForTrans;

  unsigned CoexecWithMultiVALU = MultiVALUCycles - MultiVALUCount;

  // DSBound predicate from the prior heuristic. If true, leave all
  // exposed counts at 0 (TODO from the original: properly model
  // DS/Mem-bound regions).
  bool IsDSBound = DSCycles > WMMACycles + DSCycles + SingleCycleVALUCycles +
                                  TRANSCycles - 2 * WMMACount;
  if (IsDSBound)
    return;

  // Producers are never hidden inside another flavor's shadow.
  HWUInfo[(int)InstructionFlavor::WMMA].setExposedCount(WMMACount);
  HWUInfo[(int)InstructionFlavor::MultiCycleVALU].setExposedCount(
      MultiVALUCount);

  // DS first: WMMA E-slots, then MultiVALU shadow.
  unsigned DSWMMACoexec = std::min(WMMAESlotForDS, DSCount);

  WMMAESlot -= DSWMMACoexec;
  DSCount -= DSWMMACoexec;
  if (DSCount) {
    unsigned DSMultiCoexec = std::min(CoexecWithMultiVALU, DSCount);
    DSCount -= DSMultiCoexec;
    CoexecWithMultiVALU -= DSMultiCoexec;
  }
  HWUInfo[(int)InstructionFlavor::DS].setExposedCount(DSCount);

  uint8_t Bit = flavorToCoExecMask(InstructionFlavor::DS);
  Roofline.WMMACoexecByClass[coexecBitIndex(Bit)] = DSWMMACoexec;

  // SALU next: remaining E-slots, then MultiVALU shadow.
  unsigned SALUWMMACoexec = std::min(WMMAESlot, SALUCount);
  SALUCount -= SALUWMMACoexec;
  if (SALUCount) {
    unsigned SALUMultiCoexec = std::min(CoexecWithMultiVALU, SALUCount);
    SALUCount -= SALUMultiCoexec;
    // Bug-for-bug port: prior code subtracted the post-decrement SALUCount
    // (i.e. zero) from CoexecWithMultiVALU, and decremented WMMAESlot
    // instead. Preserved.
    CoexecWithMultiVALU -= SALUCount;
    WMMAESlot -= SALUMultiCoexec;
  }

  Bit = flavorToCoExecMask(InstructionFlavor::SALU);
  Roofline.WMMACoexecByClass[coexecBitIndex(Bit)] = SALUWMMACoexec;

  HWUInfo[(int)InstructionFlavor::SALU].setExposedCount(SALUCount);

  // TRANS: WMMA I-slots that admit TRANS.
  unsigned EXPWMMACoexec = std::min(WMMAISlotForTRANS, EXPCount);
  WMMAISlot -= EXPWMMACoexec;
  EXPCount -= EXPWMMACoexec;

  Bit = flavorToCoExecMask(InstructionFlavor::TRANS);
  Roofline.WMMACoexecByClass[coexecBitIndex(Bit)] = EXPWMMACoexec;

  HWUInfo[(int)InstructionFlavor::TRANS].setExposedCount(EXPCount);

  // SingleCycleVALU last: remaining I-slots, then TRANS shadow.
  unsigned VALUWMMACoexec = std::min(WMMAISlot, SingleCycleVALUCount);
  WMMAISlot -= VALUWMMACoexec;
  SingleCycleVALUCount -= VALUWMMACoexec;
  unsigned VALUEXPCoexec = std::min(EXPCount, SingleCycleVALUCount);
  SingleCycleVALUCount -= VALUEXPCoexec;

  Bit = flavorToCoExecMask(InstructionFlavor::SingleCycleVALU);
  Roofline.WMMACoexecByClass[coexecBitIndex(Bit)] = VALUWMMACoexec;

  HWUInfo[(int)InstructionFlavor::SingleCycleVALU].setExposedCount(
      SingleCycleVALUCount);
}

void CandidateHeuristics::initExposedRoofline() {
  // For each HWUI whose flavor maps to a CoExecMask bit, copy the
  // per-class exposed count produced by the max-flow analysis. Naturally
  // covers VMEM and DMA (DS bucket) in addition to the greedy mode's
  // 6 flavors.
  if (!Roofline.isValid())
    return;
  for (auto &HWUI : HWUInfo) {
    uint8_t Bit = flavorToCoExecMask(HWUI.getType());
    if (!Bit)
      continue;
    HWUI.setExposedCount(Roofline.ExposedByClass[coexecBitIndex(Bit)]);
  }
}

void CandidateHeuristics::sortHWUIResources() {
  bool UseExposed = CoexecExposedSort != CoexecExposedMode::Off;

  // Highest priority should be first.
  llvm::sort(HWUInfo, [UseExposed](HardwareUnitInfo &A, HardwareUnitInfo &B) {
    // Prefer CoexecWindow producers
    if (A.producesCoexecWindow() != B.producesCoexecWindow())
      return A.producesCoexecWindow();

    // Prefer flavors with non-zero exposed cycles, and within that group
    // prefer more exposed first. Producers (handled above) are not part of
    // this comparison.
    if (UseExposed) {
      bool AExp = A.getRemainingExposed() > 0;
      bool BExp = B.getRemainingExposed() > 0;
      if (AExp != BExp)
        return AExp;
      if (A.getRemainingExposed() != B.getRemainingExposed())
        return A.getRemainingExposed() > B.getRemainingExposed();
    }

    // Prefer more demanded resources
    if (A.getTotalCycles() != B.getTotalCycles())
      return A.getTotalCycles() > B.getTotalCycles();

    // In ties -- prefer the resource with more instructions
    if (A.size() != B.size())
      return A.size() < B.size();

    // Default to Flavor order
    return (unsigned)A.getType() < (unsigned)B.getType();
  });
}

unsigned CandidateHeuristics::getStructuralStallCycles(SchedBoundary &Zone,
                                                       SUnit *SU) {
  // Only implemented for top-down scheduling currently.
  if (!Zone.isTop() || !SU)
    return 0;

  MachineInstr *MI = SU->getInstr();
  unsigned CurrCycle = Zone.getCurrCycle();
  unsigned Stall = 0;

  // Query SchedModel for resource stalls (unbuffered resources).
  if (SchedModel->hasInstrSchedModel() && SU->hasReservedResource) {
    const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
    for (const MCWriteProcResEntry &PE :
         make_range(SchedModel->getWriteProcResBegin(SC),
                    SchedModel->getWriteProcResEnd(SC))) {
      unsigned NextAvail =
          Zone.getNextResourceCycle(SC, PE.ProcResourceIdx, PE.ReleaseAtCycle,
                                    PE.AcquireAtCycle)
              .first;
      if (NextAvail > CurrCycle)
        Stall = std::max(Stall, NextAvail - CurrCycle);
    }
  }

  if (Zone.HazardRec && Zone.HazardRec->isEnabled()) {
    auto *HR = static_cast<GCNHazardRecognizer *>(Zone.HazardRec);
    Stall = std::max(Stall, HR->getHazardWaitStates(MI));
  }

  return Stall;
}

unsigned CandidateHeuristics::getStallCosts(SUnit *SU, SchedBoundary &Zone,
                                            StallCosts &Costs) {
  // Only implemented for top-down scheduling currently.
  if (!Zone.isTop())
    return 0;

  auto getBufferFullStalls = [this, &Zone](SUnit *SU) -> unsigned {
    InstructionFlavor Flavor = classifyFlavor(*SU->getInstr(), *SII);
    HardwareUnitInfo *HWUI = getHWUIFromFlavor(Flavor);
    if (!HWUI || HWUI->getBufferSize() <= 1)
      return 0;

    unsigned CurrCycle = Zone.getCurrCycle();
    unsigned BufferReadyCycle = HWUI->getBufferAvailableCycle(CurrCycle);
    if (BufferReadyCycle <= CurrCycle)
      return 0;

    return BufferReadyCycle - CurrCycle;
  };

  unsigned CurrCycle = Zone.getCurrCycle();

  auto getFenceStalls = [this, &CurrCycle, &Zone](SUnit *SU) -> unsigned {
    InstructionFlavor Flavor = classifyFlavor(*SU->getInstr(), *SII);

    bool IsTop = Zone.isTop();
    if ((Flavor != InstructionFlavor::Fence && IsTop) ||
        (Flavor != InstructionFlavor::DS && !IsTop))
      return 0;

    HardwareUnitInfo *ConsumerHWUI = getHWUIFromFlavor(Flavor);
    HardwareUnitInfo *ProducerHWUI = getHWUIFromFlavor(
        IsTop ? InstructionFlavor::DS : InstructionFlavor::Fence);

    SUnit *LastProducer = ProducerHWUI->getLastScheduledSU();
    if (!LastProducer)
      return 0;

    SUnit *LastConsumer = ConsumerHWUI->getLastScheduledSU();
    unsigned LastConsumerCycle = LastConsumer ? LastConsumer->TopReadyCycle : 0;
    unsigned LastProducerCycle = LastProducer->TopReadyCycle;

    if (LastProducerCycle < LastConsumerCycle)
      return 0;

    // Latency comes from DS regardless of bottom-up / top-down.
    unsigned FenceStallFinish =
        LastProducerCycle + getHWUICyclesForSU(IsTop ? LastProducer : SU);
    return FenceStallFinish <= CurrCycle ? 0 : FenceStallFinish - CurrCycle;
  };

  unsigned ReadyCycle = Zone.isTop() ? SU->TopReadyCycle : SU->BotReadyCycle;
  Costs.Ready = ReadyCycle > CurrCycle ? ReadyCycle - CurrCycle : 0;
  Costs.Structural = getStructuralStallCycles(Zone, SU);
  Costs.Latency = Zone.getLatencyStallCycles(SU);
  unsigned CarriedLatency = CarriedLatencies.lookup_or(SU->getInstr(), 0);
  Costs.Carried =
      CarriedLatency > CurrCycle ? CarriedLatency - CurrCycle : 0;
  Costs.Buffer = getBufferFullStalls(SU);
  Costs.Fence = getFenceStalls(SU);
  Costs.Effective = std::max({Costs.Ready, Costs.Structural, Costs.Latency,
                              Costs.Carried, Costs.Buffer, Costs.Fence});
  return Costs.Effective;
}

unsigned CandidateHeuristics::getMissedSlotCost(SUnit *SU,
                                                SchedBoundary &Zone) {
  auto *HazardRec = static_cast<GCNHazardRecognizer *>(Zone.HazardRec);

  // Only applies inside active WMMA coexec window.
  if (!HazardRec->inCoExecWindow())
    return 0;

  std::optional<unsigned> Stage = HazardRec->getCurrentCoExecStage();
  if (!Stage.has_value())
    return 0;

  auto CheckMSBSlot = [&HazardRec, Stage, this](SUnit *SU) -> unsigned {
    // Check if the instruction is DS.
    InstructionFlavor Flavor = classifyFlavor(*SU->getInstr(), *SII);
    if (Flavor != InstructionFlavor::DS)
      return 0;

    const CoExecInfo &Info = HazardRec->getActiveCoExecInfo();
    unsigned CurrentStage = *Stage;
    unsigned NextStage = CurrentStage + 1;

    // Check if the next slot is an I slot.
    if (NextStage >= Info.TotalWindow)
      return 0;

    uint8_t NextMask = Info.getMask(NextStage);
    bool IsISlot =
        (NextMask == CoExecMask::StageI || NextMask == CoExecMask::StageIS);
    if (!IsISlot)
      return 0;

    // DS can execute in I slots but doesn't benefit from the VALU/TRANS
    // capability. Scheduling DS when the next slot is an I slot misses
    // the opportunity to coexecute VALU/TRANS there.
    return 1;
  };

  auto CheckWideCopySlot = [&HazardRec, Stage, this](SUnit *SU) -> unsigned {
    MachineInstr *MI = SU->getInstr();
    if (!MI->isCopy())
      return 0;
    uint8_t RequiredMask = AMDGPU::getCoExecMaskForCopy(*MI, DAG->MRI, *SRI);
    if (RequiredMask != CoExecMask::SALU)
      return 0;

    unsigned CopyInstrs = SII->getSchedCyclesForCopy(*MI);

    const CoExecInfo &Info = HazardRec->getActiveCoExecInfo();
    unsigned CurrentStage = *Stage;
    unsigned MissedSlots = 0;
    unsigned Cutoff = std::min(CurrentStage + CopyInstrs, Info.TotalWindow);
    for (; CurrentStage < Cutoff; CurrentStage++) {
      uint8_t NextMask = Info.getMask(CurrentStage);
      if (NextMask & CoExecMask::VALU || NextMask & CoExecMask::WMMA)
        ++MissedSlots;
    }

    return MissedSlots;
  };

  unsigned MissedSlots = 0;
  MissedSlots = std::max(CheckMSBSlot(SU), MissedSlots);
  MissedSlots = std::max(CheckWideCopySlot(SU), MissedSlots);

  return MissedSlots;
}

bool CandidateHeuristics::tryEffectiveStall(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary &Zone) {

  StallCosts TryCosts;
  StallCosts CandCosts;
  getStallCosts(TryCand.SU, Zone, TryCosts);
  getStallCosts(Cand.SU, Zone, CandCosts);

  // Calculate missed slots for both candidates.
  unsigned TryMissedSlots = getMissedSlotCost(TryCand.SU, Zone);
  unsigned CandMissedSlots = getMissedSlotCost(Cand.SU, Zone);

  // Combined cost: stalls + missed slots.
  unsigned TryCost = TryCosts.Effective + TryMissedSlots;
  unsigned CandCost = CandCosts.Effective + CandMissedSlots;

  LLVM_DEBUG(if (TryCosts.Effective || CandCosts.Effective || TryMissedSlots ||
                 CandMissedSlots) {
    dbgs() << "Effective stalls: try=" << TryCosts.Effective
           << " (ready=" << TryCosts.Ready << ", struct=" << TryCosts.Structural
           << ", lat=" << TryCosts.Latency << ", carried=" << TryCosts.Carried
           << ", buffer=" << TryCosts.Buffer << ", fence=" << TryCosts.Fence
           << ", missedSlots=" << TryMissedSlots
           << ") cand=" << CandCosts.Effective << " (ready=" << CandCosts.Ready
           << ", struct=" << CandCosts.Structural
           << ", lat=" << CandCosts.Latency << ", carried=" << CandCosts.Carried
           << ", buffer=" << CandCosts.Buffer << ", fence=" << CandCosts.Fence
           << ", missedSlots=" << CandMissedSlots << ")\n";
  });

  // Prefer lower combined cost (stalls + missed slots).
  if (tryLess(TryCost, CandCost, TryCand, Cand,
              AMDGPUCoExecSchedStrategy::Stall))
    return true;

  // Tiebreak on missed slots when combined costs are equal.
  // For now, just prefer the candidate without any missed slots.
  if (TryCost == CandCost && TryMissedSlots != CandMissedSlots) {
    return tryLess(TryMissedSlots, CandMissedSlots, TryCand, Cand,
                   AMDGPUCoExecSchedStrategy::Stall);
  }

  return false;
}

bool CandidateHeuristics::tryMemoryPipeline(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) {

  InstructionFlavor TryFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);

  InstructionFlavor CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);

  bool TryIsMemoryPipeline = TryFlavor == InstructionFlavor::DMA ||
                             TryFlavor == InstructionFlavor::Fence;
  bool CandIsMemoryPipeline = CandFlavor == InstructionFlavor::DMA ||
                              CandFlavor == InstructionFlavor::Fence;

  if (!(TryIsMemoryPipeline || CandIsMemoryPipeline))
    return false;

  if (TryIsMemoryPipeline) {
    StallCosts Costs;
    getStallCosts(TryCand.SU, *Zone, Costs);
    TryIsMemoryPipeline &= (Costs.Effective == 0);
  }

  if (CandIsMemoryPipeline) {
    StallCosts Costs;
    getStallCosts(Cand.SU, *Zone, Costs);
    CandIsMemoryPipeline &= (Costs.Effective == 0);
  }

  if (TryIsMemoryPipeline == CandIsMemoryPipeline)
    return false;

  if (CandIsMemoryPipeline) {
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;

    return true;
  }

  TryCand.Reason = GenericSchedulerBase::RegCritical;
  return true;
}

bool CandidateHeuristics::tryCoexecSlot(
    GenericSchedulerBase::SchedCandidate &Cand,
    GenericSchedulerBase::SchedCandidate &TryCand, SchedBoundary *Zone) {
  auto HazardRec = static_cast<GCNHazardRecognizer *>(Zone->HazardRec);
  std::optional<unsigned> Stage = HazardRec->getCurrentCoExecStage();
  if (!Stage.has_value())
    return false;

  const CoExecInfo &Info = HazardRec->getActiveCoExecInfo();

  InstructionFlavor TryFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);
  InstructionFlavor CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);

  StallCosts TryStallCost;
  StallCosts CandStallCost;

  unsigned TryStall = getStallCosts(TryCand.SU, *Zone, TryStallCost);
  unsigned CandStall = getStallCosts(Cand.SU, *Zone, CandStallCost);

  if (tryLess(Info.avoidsFlavor(*Stage + TryStall, TryFlavor),
              Info.avoidsFlavor(*Stage + CandStall, CandFlavor), TryCand, Cand,
              GenericSchedulerBase::CandReason::RegCritical))
    return true;

  if (tryGreater(Info.prefersFlavor(*Stage + TryStall, TryFlavor),
                 Info.prefersFlavor(*Stage + CandStall, CandFlavor), TryCand,
                 Cand, GenericSchedulerBase::CandReason::RegCritical))
    return true;

  return false;
}

bool CandidateHeuristics::tryCriticalResourceDependency(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) const {

  auto HasPrioritySU = [this, &Cand, &TryCand](unsigned ResourceIdx) {
    const HardwareUnitInfo &HWUI = HWUInfo[ResourceIdx];

    auto CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);
    auto TryCandFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);
    bool LookDeep = (CandFlavor == InstructionFlavor::DS ||
                     TryCandFlavor == InstructionFlavor::DS) &&
                    HWUI.getType() == InstructionFlavor::WMMA;
    auto *TargetSU = HWUI.getNextTargetSU(LookDeep);

    // If we do not have a TargetSU for this resource, then it is not critical.
    if (!TargetSU)
      return false;

    return true;
  };

  auto TryEnablesResource = [&Cand, &TryCand, this](unsigned ResourceIdx) {
    const HardwareUnitInfo &HWUI = HWUInfo[ResourceIdx];
    auto CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);

    // We want to ensure our DS order matches WMMA order.
    bool LookDeep = CandFlavor == InstructionFlavor::DS &&
                    HWUI.getType() == InstructionFlavor::WMMA;
    auto *TargetSU = HWUI.getNextTargetSU(LookDeep);

    bool CandEnables =
        TargetSU != Cand.SU && DAG->IsReachable(TargetSU, Cand.SU);
    bool TryCandEnables =
        TargetSU != TryCand.SU && DAG->IsReachable(TargetSU, TryCand.SU);

    if (!CandEnables && !TryCandEnables)
      return false;

    if (CandEnables && !TryCandEnables) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    if (!CandEnables && TryCandEnables) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Both enable, prefer the critical path.
    unsigned CandHeight = Cand.SU->getHeight();
    unsigned TryCandHeight = TryCand.SU->getHeight();

    if (CandHeight > TryCandHeight) {
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;

      return true;
    }

    if (CandHeight < TryCandHeight) {
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Same critical path, just prefer original candidate.
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;

    return true;
  };

  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    // If we have encountered a resource that is not critical, then neither
    // candidate enables a critical resource
    if (!HasPrioritySU(I))
      continue;

    bool Enabled = TryEnablesResource(I);
    // If neither has enabled the resource, continue to the next resource
    if (Enabled)
      return true;
  }
  return false;
}

bool CandidateHeuristics::tryCriticalResource(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) const {
  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    const HardwareUnitInfo &HWUI = HWUInfo[I];

    bool CandUsesCrit = HWUI.contains(Cand.SU);
    bool TryCandUsesCrit = HWUI.contains(TryCand.SU);

    if (!CandUsesCrit && !TryCandUsesCrit)
      continue;

    if (CandUsesCrit != TryCandUsesCrit) {
      if (CandUsesCrit) {
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
        return true;
      }
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Otherwise, both use the critical resource
    // For longer latency InstructionFlavors, we should prioritize first by
    // their enablement of critical resources
    if (HWUI.getType() == InstructionFlavor::DS) {
      if (tryCriticalResourceDependency(TryCand, Cand, Zone))
        return true;
    }

    // Prioritize based on HWUI priorities.
    SUnit *Match = HWUI.getHigherPriority(Cand.SU, TryCand.SU);
    if (Match) {
      if (Match == Cand.SU) {
        if (Cand.Reason > GenericSchedulerBase::RegCritical)
          Cand.Reason = GenericSchedulerBase::RegCritical;
        return true;
      }
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
  }

  return false;
}

//===----------------------------------------------------------------------===//
// ShadowMix Helpers
//===----------------------------------------------------------------------===//

unsigned CandidateHeuristics::countDirectlyEnabledByFlavor(
    SUnit *SU, InstructionFlavor TargetFlavor) {
  unsigned Count = 0;
  for (const SDep &Succ : SU->Succs) {
    if (Succ.isWeak())
      continue;
    SUnit *SuccSU = Succ.getSUnit();
    if (SuccSU->isScheduled || SuccSU->NumPredsLeft != 1 || !SuccSU->getInstr())
      continue;
    if (classifyFlavor(*SuccSU->getInstr(), *SII) == TargetFlavor)
      ++Count;
  }
  return Count;
}

std::optional<unsigned> CandidateHeuristics::findNearestPendingByFlavor(
    InstructionFlavor TargetFlavor, unsigned MaxDepth) {
  // BFS from all currently ready SUs to find nearest pending SU of
  // TargetFlavor.
  SmallVector<std::pair<SUnit *, unsigned>, 32> Worklist;
  SmallPtrSet<SUnit *, 32> Visited;

  for (auto &SU : DAG->SUnits) {
    if (!SU.isScheduled && SU.isTopReady()) {
      Worklist.push_back({&SU, 0});
      Visited.insert(&SU);
    }
  }

  std::optional<unsigned> BestDist;
  for (unsigned I = 0; I < Worklist.size(); ++I) {
    auto [SU, Depth] = Worklist[I];
    if (Depth > MaxDepth)
      continue;

    if (!SU->isTopReady() && !SU->isScheduled && SU->getInstr()) {
      if (classifyFlavor(*SU->getInstr(), *SII) == TargetFlavor) {
        if (!BestDist || Depth < *BestDist)
          BestDist = Depth;
        continue; // Don't expand past target
      }
    }

    for (const SDep &Succ : SU->Succs) {
      if (Succ.isWeak())
        continue;
      SUnit *SuccSU = Succ.getSUnit();
      if (SuccSU->isScheduled || Visited.count(SuccSU))
        continue;
      Visited.insert(SuccSU);
      Worklist.push_back({SuccSU, Depth + 1});
    }
  }
  return BestDist;
}

bool CandidateHeuristics::wouldHelpEnable(SUnit *SU, SUnit *TargetSU) {
  return DAG->IsReachable(TargetSU, SU);
}

//===----------------------------------------------------------------------===//
// ShadowMix Heuristic
//===----------------------------------------------------------------------===//

bool CandidateHeuristics::tryShadowMix(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) {

  auto CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);
  auto TryCandFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);

  // Refresh ready-mix snapshot from the boundary's queues. No-op if the
  // snapshot is still valid (cleared on schedNode).
  MixInfo.refreshFromBoundary(*Zone, *SII);

  // Determine if either candidate is a window producer.
  HardwareUnitInfo *CandHWUI = getHWUIFromFlavor(CandFlavor);
  HardwareUnitInfo *TryCandHWUI = getHWUIFromFlavor(TryCandFlavor);
  bool CandIsProducer = CandHWUI && CandHWUI->producesCoexecWindow();
  bool TryCandIsProducer = TryCandHWUI && TryCandHWUI->producesCoexecWindow();

  // ---- Compute template-derived demand ----
  // Use the window's demand if populated; otherwise compute from the
  // producer's CoExecInfo template or fall back to region-aggregate demand.
  WindowSlotDemand Demand = WindowSlotDemand();
  SUnit *ProducerSU = nullptr;
  if (CandIsProducer)
    ProducerSU = Cand.SU;
  else if (TryCandIsProducer)
    ProducerSU = TryCand.SU;

  // Select the TargetWindow for demand checking. When CurrentWindow is
  // active and satisfied, switch to NextWindow for lookahead.
  CoexecWindow *TargetWindow = &CurrentWindow;
  if (CurrentWindow.IsActive && CurrentWindow.Demand.isSatisfied(MixInfo)) {
    // Populate NextWindow if needed.
    if (!NextWindow.IsPopulated)
      NextWindow.populate(InstructionFlavor::Other, HWUInfo, MixInfo, *SII,
                          Roofline);
    if (NextWindow.IsPopulated)
      TargetWindow = &NextWindow;
  }

  // Get the producer flavor from the target window if populated.
  InstructionFlavor ProducerFlavor = InstructionFlavor::Other;

  if (TargetWindow->IsPopulated) {
    Demand = TargetWindow->Demand;
    ProducerFlavor = TargetWindow->ProducerFlavor;
  } else if (ProducerSU) {
    Demand = WindowSlotDemand::fromCoExecInfo(
        getCoExecInfo(*ProducerSU->getInstr(), *SII));
  }

  if (!Demand.hasSlots())
    return false;

  // Verify producer candidates match the target window's producer flavor.
  // A producer of a different type should not be promoted by ShadowMix.
  if (CandFlavor == ProducerFlavor && TryCandFlavor == ProducerFlavor)
    return false;

  // Check if a candidate matches the target producer flavor.
  bool CandMatchesProducer = (CandFlavor == ProducerFlavor);
  bool TryCandMatchesProducer = (TryCandFlavor == ProducerFlavor);

  // ---- Producer vs non-producer ----
  // One candidate matches the target producer, the other doesn't.
  // Prefer producer when demand is satisfied — fillers are ready.
  // If neither are Producer and the Demand is satified, then tryShadowMix has
  // no opinion.
  if (Demand.isSatisfied(MixInfo)) {
    if (CandMatchesProducer) {
      LLVM_DEBUG(dbgs() << "ShadowMix: promote producer SU("
                        << Cand.SU->NodeNum << ") — fillers ready\n");
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
    if (TryCandMatchesProducer) {
      LLVM_DEBUG(dbgs() << "ShadowMix: promote producer SU("
                        << TryCand.SU->NodeNum << ") — fillers ready\n");
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
    return false;
  }

  // ---- Enablement when demand not satisfied ----
  // Fillers are insufficient. Try to find non-producer instructions that
  // enable the most deficient filler flavor.
  InstructionFlavor NeededFlavor = Demand.getMostDeficientFlavor(MixInfo);

  // Direct enablement — exclude producers from scoring (scheduling a
  // producer doesn't help make fillers ready).
  unsigned CandEnables =
      CandMatchesProducer ? 0
                          : countDirectlyEnabledByFlavor(Cand.SU, NeededFlavor);
  unsigned TryCandEnables =
      TryCandMatchesProducer
          ? 0
          : countDirectlyEnabledByFlavor(TryCand.SU, NeededFlavor);

  if (CandEnables != TryCandEnables) {
    if (TryCandEnables > CandEnables) {
      LLVM_DEBUG(dbgs() << "ShadowMix: SU(" << TryCand.SU->NodeNum
                        << ") enables " << TryCandEnables << " "
                        << getFlavorName(NeededFlavor) << " (vs " << CandEnables
                        << ")\n");
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
    LLVM_DEBUG(dbgs() << "ShadowMix: SU(" << Cand.SU->NodeNum << ") enables "
                      << CandEnables << " " << getFlavorName(NeededFlavor)
                      << " (vs " << TryCandEnables << ")\n");
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }

  // BFS lookahead — check if one candidate is on the path to a pending
  // filler of the needed flavor. Exclude producers from scoring.
  auto NearestDist =
      findNearestPendingByFlavor(NeededFlavor, ShadowMixLookaheadDepth);
  if (NearestDist) {
    for (auto &SU : DAG->SUnits) {
      if (!SU.getInstr() || SU.isScheduled || SU.isTopReady())
        continue;
      if (classifyFlavor(*SU.getInstr(), *SII) != NeededFlavor)
        continue;

      bool CandHelps = !CandMatchesProducer && wouldHelpEnable(Cand.SU, &SU);
      bool TryCandHelps =
          !TryCandMatchesProducer && wouldHelpEnable(TryCand.SU, &SU);

      if (CandHelps == TryCandHelps)
        continue;

      if (TryCandHelps) {
        LLVM_DEBUG(dbgs() << "ShadowMix lookahead: SU(" << TryCand.SU->NodeNum
                          << ") helps enable " << getFlavorName(NeededFlavor)
                          << " SU(" << SU.NodeNum << ")\n");
        TryCand.Reason = GenericSchedulerBase::RegCritical;
        return true;
      }
      LLVM_DEBUG(dbgs() << "ShadowMix lookahead: SU(" << Cand.SU->NodeNum
                        << ") helps enable " << getFlavorName(NeededFlavor)
                        << " SU(" << SU.NodeNum << ")\n");
      if (Cand.Reason > GenericSchedulerBase::RegCritical)
        Cand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
  }

  // ---- Greedy fallback ----
  // Enablement/lookahead found no path to fillers.
  // Prefer the producer rather than stalling indefinitely.
  if (CandMatchesProducer) {
    LLVM_DEBUG(dbgs() << "ShadowMix: greedy take producer SU("
                      << Cand.SU->NodeNum << ") — no enablement path\n");
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }
  if (TryCandMatchesProducer) {
    LLVM_DEBUG(dbgs() << "ShadowMix: greedy take producer SU("
                      << TryCand.SU->NodeNum << ") — no enablement path\n");
    TryCand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }

  return false;
}

AMDGPUCoExecSchedStrategy::AMDGPUCoExecSchedStrategy(
    const MachineSchedContext *C)
    : GCNSchedStrategy(C) {
  SchedStages.push_back(GCNSchedStageID::ILPInitialSchedule);
  SchedStages.push_back(GCNSchedStageID::LiveIntervalRPReschedule);
  SchedStages.push_back(GCNSchedStageID::PreRARematerialize);
  // Use more accurate GCN pressure trackers.
  UseGCNTrackers = true;
  // Default to using VGPRExcessThresholdPercent if it's not already
  // explicitly set.
  if (!VGPRExcessThresholdPercent) {
    VGPRExcessThresholdPercent = DefaultVGPRExcessThresholdPercent;
  }
}

void AMDGPUCoExecSchedStrategy::initPolicy(MachineBasicBlock::iterator Begin,
                                           MachineBasicBlock::iterator End,
                                           unsigned NumRegionInstrs) {
  GCNSchedStrategy::initPolicy(Begin, End, NumRegionInstrs);
  assert((PreRADirection == MISched::Unspecified ||
          PreRADirection == MISched::TopDown) &&
         "coexec scheduler only supports top-down scheduling");
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
}

void AMDGPUCoExecSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  // Coexecution scheduling strategy is only done top-down to support new
  // resource balancing heuristics.
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;

  GCNSchedStrategy::initialize(DAG);
  Heurs.initialize(DAG, SchedModel, TRI);

  // Replace the default hazard recognizer with our PreRA one.
  // This must happen after GCNSchedStrategy::initialize() because
  // GenericScheduler::initialize() calls SchedBoundary::reset() which
  // deletes and recreates the hazard recognizer each region.
  delete Top.HazardRec;
  Top.HazardRec = new GCNHazardRecognizer(
      DAG->MF, GCNHazardRecognizer::OperatingMode::PreRA);
}

void AMDGPUCoExecSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  Heurs.updateForScheduling(SU, &Top);
  GCNSchedStrategy::schedNode(SU, IsTopNode);
}

SUnit *AMDGPUCoExecSchedStrategy::pickNode(bool &IsTopNode) {
  assert(RegionPolicy.OnlyTopDown && !RegionPolicy.OnlyBottomUp &&
         "coexec scheduler only supports top-down scheduling");

  if (DAG->top() == DAG->bottom()) {
    assert(Top.Available.empty() && Top.Pending.empty() &&
           Bot.Available.empty() && Bot.Pending.empty() && "ReadyQ garbage");
    return nullptr;
  }

  bool PickedPending = false;
  SUnit *SU = nullptr;
#ifndef NDEBUG
  SchedCandidate *PickedCand = nullptr;
#endif
  do {
    PickedPending = false;
    SU = pickOnlyChoice(Top);
    if (!SU) {
      CandPolicy NoPolicy;
      TopCand.reset(NoPolicy);
      pickNodeFromQueue(Top, NoPolicy, DAG->getTopRPTracker(), TopCand,
                        PickedPending, /*IsBottomUp=*/false);
      assert(TopCand.Reason != NoCand && "failed to find a candidate");
      SU = TopCand.SU;
#ifndef NDEBUG
      PickedCand = &TopCand;
#endif
    }
    IsTopNode = true;
  } while (SU->isScheduled);

  LLVM_DEBUG(if (PickedCand) dumpPickSummary(SU, IsTopNode, *PickedCand));

  if (PickedPending) {
    unsigned ReadyCycle = SU->TopReadyCycle;
    unsigned CurrentCycle = Top.getCurrCycle();
    if (ReadyCycle > CurrentCycle)
      Top.bumpCycle(ReadyCycle);

    // checkHazard() does not expose the exact cycle where the hazard clears.
    while (Top.checkHazard(SU))
      Top.bumpCycle(Top.getCurrCycle() + 1);

    Top.releasePending();
  }

  if (SU->isTopReady())
    Top.removeReady(SU);
  if (SU->isBottomReady())
    Bot.removeReady(SU);

  LLVM_DEBUG(dbgs() << "Scheduling SU(" << SU->NodeNum << ") "
                    << *SU->getInstr());

  assert(IsTopNode && "coexec scheduler must only schedule from top boundary");
  return SU;
}

void AMDGPUCoExecSchedStrategy::pickNodeFromQueue(
    SchedBoundary &Zone, const CandPolicy &ZonePolicy,
    const RegPressureTracker &RPTracker, SchedCandidate &Cand,
    bool &PickedPending, bool IsBottomUp) {
  assert(Zone.isTop() && "coexec scheduler only supports top boundary");
  assert(!IsBottomUp && "coexec scheduler only supports top-down scheduling");

  const SIRegisterInfo *SRI = static_cast<const SIRegisterInfo *>(TRI);
  ArrayRef<unsigned> Pressure = RPTracker.getRegSetPressureAtPos();
  unsigned SGPRPressure = 0;
  unsigned VGPRPressure = 0;
  PickedPending = false;
  if (DAG->isTrackingPressure()) {
    if (!useGCNTrackers()) {
      SGPRPressure = Pressure[AMDGPU::RegisterPressureSets::SReg_32];
      VGPRPressure = Pressure[AMDGPU::RegisterPressureSets::VGPR_32];
    } else {
      SGPRPressure = DownwardTracker.getPressure().getSGPRNum();
      VGPRPressure = DownwardTracker.getPressure().getArchVGPRNum();
    }
  }

  auto EvaluateQueue = [&](ReadyQueue &Q, bool FromPending) {
    for (SUnit *SU : Q) {
      SchedCandidate TryCand(ZonePolicy);
      initCandidate(TryCand, SU, Zone.isTop(), RPTracker, SRI, SGPRPressure,
                    VGPRPressure, IsBottomUp);
      SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
      tryCandidateCoexec(Cand, TryCand, ZoneArg);
      if (TryCand.Reason != NoCand) {
        if (TryCand.ResDelta == SchedResourceDelta())
          TryCand.initResourceDelta(Zone.DAG, SchedModel);
        LLVM_DEBUG(printCandidateDecision(Cand, TryCand));
        PickedPending = FromPending;
        Cand.setBest(TryCand);
      } else {
        LLVM_DEBUG(printCandidateDecision(TryCand, Cand));
      }
    }
  };

  LLVM_DEBUG(dbgs() << "Available Q:\n");
  EvaluateQueue(Zone.Available, /*FromPending=*/false);

  LLVM_DEBUG(dbgs() << "Pending Q:\n");
  EvaluateQueue(Zone.Pending, /*FromPending=*/true);
}

#ifndef NDEBUG
void AMDGPUCoExecSchedStrategy::dumpPickSummary(SUnit *SU, bool IsTopNode,
                                                SchedCandidate &Cand) {
  const SIInstrInfo *SII = static_cast<const SIInstrInfo *>(DAG->TII);
  unsigned Cycle = IsTopNode ? Top.getCurrCycle() : Bot.getCurrCycle();

  dbgs() << "=== Pick @ Cycle " << Cycle << " ===\n";

  const InstructionFlavor Flavor = classifyFlavor(*SU->getInstr(), *SII);
  dbgs() << "Picked: SU(" << SU->NodeNum << ") ";
  SU->getInstr()->print(dbgs(), /*IsStandalone=*/true, /*SkipOpers=*/false,
                        /*SkipDebugLoc=*/true);
  dbgs() << " [" << getFlavorName(Flavor) << "]\n";

  dbgs() << "  Reason: ";
  if (LastAMDGPUReason != AMDGPUSchedReason::None)
    dbgs() << getReasonName(LastAMDGPUReason);
  else if (Cand.Reason != NoCand)
    dbgs() << GenericSchedulerBase::getReasonStr(Cand.Reason);
  else
    dbgs() << "Unknown";
  dbgs() << "\n\n";

  LastAMDGPUReason = AMDGPUSchedReason::None;
}
#endif

bool AMDGPUCoExecSchedStrategy::tryCandidateCoexec(SchedCandidate &Cand,
                                                   SchedCandidate &TryCand,
                                                   SchedBoundary *Zone) {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }

  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg))
    return TryCand.Reason != NoCand;

  // Avoid exceeding the target's limit.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.Excess, Cand.RPDelta.Excess, TryCand, Cand,
                  RegExcess, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // We only compare a subset of features when comparing nodes between
  // Top and Bottom boundary. Some properties are simply incomparable, in many
  // other instances we should only override the other boundary if something
  // is a clear good pick on one boundary. Skip heuristics that are more
  // "tie-breaking" in nature.
  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    // Compare candidates by the stall they would introduce if
    // scheduled in the current cycle.
    if (Heurs.tryEffectiveStall(TryCand, Cand, *Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::Stall;
      return TryCand.Reason != NoCand;
    }

    if (Heurs.tryMemoryPipeline(TryCand, Cand, Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::MemoryPipeline;
      return TryCand.Reason != NoCand;
    }

    if (Heurs.tryCoexecSlot(Cand, TryCand, Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::CoexecSlot;
      return TryCand.Reason != NoCand;
    }

    if (Heurs.tryShadowMix(TryCand, Cand, Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::ShadowMix;
      return TryCand.Reason != NoCand;
    }

    Heurs.sortHWUIResources();
    if (Heurs.tryCriticalResource(TryCand, Cand, Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceBalance;
      return TryCand.Reason != NoCand;
    }

    if (Heurs.tryCriticalResourceDependency(TryCand, Cand, Zone)) {
      LastAMDGPUReason = AMDGPUSchedReason::CritResourceDep;
      return TryCand.Reason != NoCand;
    }
  }

  // Keep clustered nodes together to encourage downstream peephole
  // optimizations which may reduce resource requirements.
  //
  // This is a best effort to set things up for a post-RA pass. Optimizations
  // like generating loads of multiple registers should ideally be done within
  // the scheduler pass by combining the loads during DAG postprocessing.
  unsigned CandZoneCluster = Cand.AtTop ? TopClusterID : BotClusterID;
  unsigned TryCandZoneCluster = TryCand.AtTop ? TopClusterID : BotClusterID;
  bool CandIsClusterSucc =
      isTheSameCluster(CandZoneCluster, Cand.SU->ParentClusterIdx);
  bool TryCandIsClusterSucc =
      isTheSameCluster(TryCandZoneCluster, TryCand.SU->ParentClusterIdx);

  if (tryGreater(TryCandIsClusterSucc, CandIsClusterSucc, TryCand, Cand,
                 Cluster))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak))
      return TryCand.Reason != NoCand;
  }

  // Avoid increasing the max pressure of the entire region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
                  Cand, RegMax, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Avoid serializing long latency dependence chains.
    // For acyclic path limited loops, latency was already checked above.
    if (!RegionPolicy.DisableLatencyHeuristic && TryCand.Policy.ReduceLatency &&
        !Rem.IsAcyclicLatencyLimited && tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
      TryCand.Reason = NodeOrder;
      return true;
    }
  }

  return false;
}

ScheduleDAGInstrs *
llvm::createGCNCoExecMachineScheduler(MachineSchedContext *C) {
  LLVM_DEBUG(dbgs() << "AMDGPU coexec preRA scheduler selected for "
                    << C->MF->getName() << '\n');

  // CoExecScheduler defaults to Loaded DS latency mode.
  SIInstrInfo::setDSLatencyMode(SIInstrInfo::DSLatencyMode::Loaded);

  ScheduleDAGMILive *DAG = new GCNScheduleDAGMILive(
      C, std::make_unique<AMDGPUCoExecSchedStrategy>(C));

  DAG->addMutation(createAMDGPUBarrierLatencyDAGMutation(C->MF));
  return DAG;
}

ScheduleDAGInstrs *
llvm::createGCNNoopPostMachineScheduler(MachineSchedContext *C) {
  LLVM_DEBUG(dbgs() << "AMDGPU nop postRA scheduler selected for "
                    << C->MF->getName() << '\n');
  return new GCNNoopPostScheduleDAG(C);
}
