# tryCriticalResource Heuristic Deep Dive

## Overview

The `tryCriticalResource` heuristic is a core scheduling decision mechanism in the AMDGPU CoExec Scheduler. It prioritizes instructions based on which hardware resource (HardwareUnitInfo) is most critical to the scheduling region's performance. The heuristic ensures that the scheduler keeps the most demanded hardware units busy, maximizing throughput and co-execution opportunities.

## Key Concepts

### HardwareUnitInfo (HWUI)

Each `HardwareUnitInfo` object tracks a specific hardware resource type, mapped via `InstructionFlavor`:

| InstructionFlavor | Hardware Resource | Description |
|-------------------|-------------------|-------------|
| `WMMA` | Matrix units | Wave Matrix Multiply-Accumulate operations |
| `TRANS` | Transcendental unit | Transcendental functions (sin, cos, exp, etc.) |
| `MultiCycleVALU` | VALU (multi-cycle) | Multi-cycle VALU operations (e.g., CVT) |
| `SingleCycleVALU` | VALU (single-cycle) | Standard single-cycle VALU operations |
| `DS` | LDS unit | Data share (LDS) load/store operations |
| `SALU` | SALU | Scalar ALU operations |
| `VMEM` | Vector memory | Vector memory load/store operations |
| `DMA` | DMA engine | LDS DMA operations |
| `Fence` | Synchronization | Memory fences, barriers |
| `Other` | Miscellaneous | Other instruction types |

### Key HWUI Metrics

```cpp
class HardwareUnitInfo {
  unsigned TotalCycles;       // Total busy cycles for this HW unit in the region
  unsigned RemainingCycles;   // Cycles still unscheduled
  unsigned RemainingExposed;  // Cycles that will execute OUTSIDE any co-exec shadow
  SmallSetVector<SUnit *, 16> AllSUs;       // All SUs consuming this resource
  SmallSetVector<SUnit *, 16> PrioritySUs;  // Highest-priority unscheduled SUs
};
```

**TotalCycles**: The aggregate busy time for this hardware unit. Computed by summing the blocking cycles of all instructions assigned to this HWUI.

**RemainingExposed**: The estimated number of instructions that cannot be hidden in another instruction's co-execution shadow. This is the critical metric for prioritization.

---

## sortHWUIResources: Determining Resource Priority

Before `tryCriticalResource` makes decisions, the HWUIInfo vector is sorted by `sortHWUIResources()` to establish which resources are most critical:

```cpp
void CandidateHeuristics::sortHWUIResources(SchedBoundary *Zone,
                                            bool UseDependencySort) {
  llvm::sort(HWUInfo, [UseExposed, UseDependencySort, this](
      HardwareUnitInfo &A, HardwareUnitInfo &B) {
       
    // 1. CoexecWindow producers first (WMMA, TRANS, MultiCycleVALU)
    if (A.producesCoexecWindow() != B.producesCoexecWindow())
      return A.producesCoexecWindow();
    
    // 2. Prefer resources with non-zero RemainingExposed
    if (UseExposed) {
      bool AExp = A.getRemainingExposed() > 0;
      bool BExp = B.getRemainingExposed() > 0;
      if (AExp != BExp)
        return AExp;  // Has exposed cycles = higher priority
      if (A.getRemainingExposed() != B.getRemainingExposed())
        return A.getRemainingExposed() > B.getRemainingExposed();
    }
    
    // 3. Higher TotalCycles = more demanded = higher priority
    if (A.getTotalCycles() != B.getTotalCycles())
      return A.getTotalCycles() > B.getTotalCycles();
    
    // 4. Fewer instructions = more cycles/instr = higher priority
    if (A.size() != B.size())
      return A.size() < B.size();
    
    // 5. Tie-break by flavor enum order
    return static_cast<unsigned>(A.getType()) < static_cast<unsigned>(B.getType());
  });
}
```

### Sorting Priority Summary

1. **Co-exec producers first**: WMMA, TRANS, MultiCycleVALU produce co-execution windows
2. **RemainingExposed > 0**: Resources that will cause stalls if not prioritized
3. **Higher TotalCycles**: More demanded resources
4. **Fewer instructions**: Higher cycles per instruction (longer latency ops)

---

## RemainingExposed: The Key Metric

### What is RemainingExposed?

`RemainingExposed` represents the number of instructions of a given flavor that **cannot be hidden** inside another instruction's co-execution shadow. These are instructions that will contribute to the critical path and cause stalls.

In order to calculate what instructions can be hidden, we must have some modelling of the total number of coexecution slots in the entire scheduling region.

### Co-Execution Windows and CoExecInfo

Recall that certain instructions may produce a window in which other instructions can coexecute. Each cycle in this window is modelled by a CoexecSlot

Multi-cycle instructions like WMMA produce co-execution windows where other instructions can execute in parallel. Each cycle within the window has a specific **stage type** that determines which instruction classes can co-execute:

| Stage | Mask | Allowed Instructions | Description |
|-------|------|---------------------|-------------|
| **E0** | `CTRL` | Control only | Issue cycle: `s_delay_alu`, `s_set_vgpr_msb` |
| **E** | `CTRL \| SALU \| MEM` | SALU, DS, VMEM, SMEM | External: memory and scalar ALU |
| **I** | `CTRL \| SALU \| MEM \| VALU \| TRANS` | All ALU + memory | Internal: VALU and TRANS allowed |
| **IS** | `StageI \| WMMA` | All ALU + memory + WMMA | Internal + scaled WMMA absorb |
| **V** | `CTRL \| SALU \| MEM \| WMMA` | Memory, SALU, next WMMA | Vacant: NO VALU/TRANS |

#### Example: V_WMMA_F16_16x16x32 (F16/BF16)
- **Occupancy**: 8 cycles
- **Window**: 9 cycles
- **Pattern**: `0EIIEEIIV`

```
Cycle:  0   1   2   3   4   5   6   7   8
Stage: E0   E   I   I   E   E   I   I   V
       ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
      ctrl mem ALL ALL mem mem ALL ALL wmma
           salu         salu     valu only
                valu         trans
                trans
```

**Slot breakdown**:
- 1 E0 slot (control only)
- 4 E slots (SALU/DS/VMEM compatible)
- 3 I slots (VALU/TRANS/SALU/DS/VMEM compatible)  
- 1 V slot (next WMMA only, no VALU/TRANS)

#### Example: V_WMMA_F8_16x16x64 (FP8/BF8)
- **Occupancy**: 4 cycles
- **Window**: 6 cycles
- **Pattern**: `0EEIVV`

```
Cycle:  0   1   2   3   4   5
Stage: E0   E   E   I   V   V
       ↓   ↓   ↓   ↓   ↓   ↓
      ctrl mem mem ALL wmma wmma
           salu salu valu only only
                     trans
```

**Slot breakdown**:
- 1 E0 slot
- 2 E slots
- 1 I slot
- 2 V slots

#### Example: V_WMMA_I8_16x16x64 (INT8)
- **Occupancy**: 16 cycles
- **Window**: 17 cycles
- **Pattern**: `0EIIEEIIEEIIEEIIV`

```
Cycle:  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16
Stage: E0   E   I   I   E   E   I   I   E   E   I   I   E   E   I   I   V
```

**Slot breakdown**:
- 1 E0 slot
- 8 E slots
- 7 I slots
- 1 V slot

---

### Roofline Analysis: Computing Remaining Exposed

The roofline analysis (`computeRooflineCoexec`) uses a **bipartite max-flow algorithm** to determine the optimal assignment of filler instructions to co-execution slots. This gives an exact lower bound on unavoidable stalls.

#### Step 1: Enumerate Slot Supply

For each WMMA in the region, enumerate the co-execution slots from its stage pattern:

```cpp
for (auto &SU : DAG->SUnits) {
  if (Flavor == InstructionFlavor::WMMA) {
    CoExecInfo Info = getCoExecInfo(MI, *SII);
    for (unsigned S = 0; S < Info.TotalWindow; ++S) {
      CoExecMaskT Mask = Info.getMask(S);
      if (Mask == CoExecMask::StageE0)
        continue;  // Skip E0 (control-only)
      SlotTypes[Mask]++;  // Count slots by their capability mask
    }
  }
}
```

**Example**: For 4× `V_WMMA_F16_16x16x32` (pattern `0EIIEEIIV`):
```
SlotTypes[StageE] = 4 × 4 = 16 slots  (E slots from all 4 WMMAs)
SlotTypes[StageI] = 4 × 3 = 12 slots  (I slots from all 4 WMMAs)
SlotTypes[StageV] = 4 × 1 = 4 slots   (V slots from all 4 WMMAs)
```

#### Step 2: Count Consumer Instructions

Count how many instructions of each filler class exist in the region:

```cpp
// Consumer classes indexed by CoExecMask bit position:
// 0=CTRL, 1=VALU, 2=TRANS, 3=SALU, 4=DS, 5=VMEM, 6=SMEM, 7=WMMA
ConsumerCount[coexecBitIndex(flavorToCoExecMask(Flavor))]++;
```

**Example** region consumers:
```
ConsumerCount[VALU]  = 20  (SingleCycleVALU instructions)
ConsumerCount[TRANS] = 4   (TRANS instructions)
ConsumerCount[SALU]  = 8   (SALU instructions)
ConsumerCount[DS]    = 12  (DS instructions)
ConsumerCount[VMEM]  = 6   (VMEM instructions)
```

#### Step 3: Build Bipartite Flow Network

The max-flow network models the assignment problem:

```
        Source                                              Sink
          |                                                   |
          +---(20)---> [VALU] ---(12)---> [StageI] ---(12)--->+
          |               +-------------> [StageE] ---(16)--->+
          +---(4)----> [TRANS]---(12)---> [StageI] ---------->+
          +---(8)----> [SALU] ---(16)---> [StageE] ---------->+
          |               +----(12)----> [StageI] ----------->+
          +---(12)---> [DS]  ----(16)---> [StageE] ---------->+
          |               +----(12)----> [StageI] ----------->+
          +---(6)----> [VMEM] --(16)----> [StageE] ---------->+
          |               +----(12)----> [StageI] ----------->+
          +---(3)----> [WMMA] ---(4)----> [StageV] ---(4)---->+

Edges: Source -> ConsumerClass: capacity = ConsumerCount[class]
       ConsumerClass -> SlotType: capacity = min(ConsumerCount, SlotCount)
       SlotType -> Sink: capacity = SlotCount
```

#### Step 4: Solve Max-Flow (Edmonds-Karp)

The max-flow solver finds the maximum number of consumer instructions that can be assigned to slots:

```cpp
Roofline.MaxFilledSlots = MF.solve();
Roofline.LowerBoundStalls = Roofline.TotalSlots - Roofline.MaxFilledSlots;
```

#### Step 5: Recover Per-Class Exposed Counts

After solving, extract how many instructions of each class could NOT be placed:

```cpp
for (unsigned I = 0; I < C; ++I) {
  unsigned Fill = 0;
  for (unsigned J = 0; J < T; ++J)
    Fill += MF.getFlow(ClassNode, SlotNode);
  
  unsigned Nk = Roofline.ConsumerCount[BitIdx];
  Roofline.ExposedByClass[BitIdx] = (Fill >= Nk) ? 0 : Nk - Fill;
}
```

**Example Result**:
```
Total slots: 32 (16 E + 12 I + 4 V)
Total consumers: 53 (20 + 4 + 8 + 12 + 6 + 3)

Max-flow assigns:
  VALU:  12 -> StageI slots (8 exposed, cannot fit)
  TRANS: 4  -> StageI slots (0 exposed, all fit)
  SALU:  8  -> StageE slots (0 exposed, all fit)
  DS:    8  -> StageE slots (4 exposed)
  VMEM:  0  -> (no remaining E slots, 6 exposed)
  WMMA:  3  -> StageV slots (0 exposed, all fit in V slots of prior WMMAs)

ExposedByClass:
  VALU:  8   (20 - 12 = 8 cannot hide)
  TRANS: 0   (all 4 fit)
  SALU:  0   (all 8 fit)
  DS:    4   (12 - 8 = 4 cannot hide)
  VMEM:  6   (none could be placed)
```

### Initialization: initExposedRoofline()

The exposed counts are copied to each HWUI:

```cpp
void CandidateHeuristics::initExposedRoofline() {
  if (!Roofline.isValid())
    return;
  for (auto &HWUI : HWUInfo) {
    CoExecMaskT Bit = flavorToCoExecMask(HWUI.getType());
    if (!Bit)
      continue;
    HWUI.setExposedCount(Roofline.ExposedByClass[coexecBitIndex(Bit)]);
  }
}
```

### Updating RemainingExposed During Scheduling

When an instruction is scheduled **outside** an active co-execution window, its HWUI's `RemainingExposed` is decremented:

```cpp
void CandidateHeuristics::updateForScheduling(SUnit *SU, SchedBoundary *Zone) {
  // ...
  if (CoexecExposedSort != CoexecExposedMode::Off) {
    bool InShadow = CurrentWindow.IsActive && !HWUI->producesCoexecWindow();
    if (!InShadow)
      HWUI->reduceRemainingExposed();
  }
}
```

---

## tryCriticalResource Implementation

```cpp
bool CandidateHeuristics::tryCriticalResource(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) const {
  
  // Iterate through HWUIs in priority order (sorted by sortHWUIResources)
  for (unsigned I = 0; I < HWUInfo.size(); I++) {
    const HardwareUnitInfo &HWUI = HWUInfo[I];

    bool CandUsesCrit = HWUI.contains(Cand.SU);
    bool TryCandUsesCrit = HWUI.contains(TryCand.SU);

    // Neither candidate uses this critical resource - check next
    if (!CandUsesCrit && !TryCandUsesCrit)
      continue;

    // One uses critical resource, other doesn't - prefer the one that does
    if (CandUsesCrit != TryCandUsesCrit) {
      if (CandUsesCrit) {
        Cand.Reason = GenericSchedulerBase::RegCritical;
        return true;
      }
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }

    // Both use the same critical resource
    // For shorter-latency flavors, defer to dependency-based prioritization
    if (HWUI.getType() == InstructionFlavor::DS ||
        HWUI.getType() == InstructionFlavor::SALU ||
        HWUI.getType() == InstructionFlavor::SingleCycleVALU) {
      if (tryCriticalResourceDependency(TryCand, Cand, Zone))
        return true;
    }

    // Use HWUI's internal priority ordering
    SUnit *Match = HWUI.getHigherPriority(Cand.SU, TryCand.SU);
    if (Match) {
      if (Match == Cand.SU) {
        Cand.Reason = GenericSchedulerBase::RegCritical;
        return true;
      }
      TryCand.Reason = GenericSchedulerBase::RegCritical;
      return true;
    }
  }

  return false;
}
```

### Tie-breaking On Same HardwareUnit

The primary mechanism that tryCriticalResource is to make decisions about `SchedCandidates` that use different HardwareUnitInfos. However, in the cases where we are comparing two `SchedCandidates` which use the same HardwareUnitInfo, we have several tiebreakers.

While populating the HardwareUnits based on the instructions in the scheduling region, we maintain a priority list of instructions, sorted by depth. This ordering is mainly so that different heuristics (e.g. tryCriticalResourceDependency) can agree on which instruction to prioritize scheudling next.

For example, in a typical GEMM kernel, we may have may ds_loads which are dependencies for the same WMMA. Since we have an ordering on the HardwareUnitInfo, when deciding which ds_loads to schedule as WMMA predecessors, tryCriticalResourceDependency and tryCriticalResource can agree on the `Target` SU.

However, for many HardwareUnitInfos, having this agreement isn't required (having good ordering between an instruction and its predecessors is not performance critical). For these, we tiebreak on tryCriticalResourceDependency.

---

## Step-by-Step Examples

### Example 1: F16 WMMA Region with Mixed Fillers

**Scenario**: A scheduling region with F16 WMMA operations

```
Region Contents:
  - 4x V_WMMA_F16_16x16x32 (pattern: 0EIIEEIIV, 8-cycle occupancy)
  - 16 DS loads
  - 8 SALU instructions
  - 24 SingleCycleVALU instructions
  - 4 TRANS instructions
```

**CoExecInfo for V_WMMA_F16_16x16x32**:
```
Pattern: 0EIIEEIIV
Cycle 0: E0 - Control only (s_delay_alu)
Cycle 1: E  - SALU, DS, VMEM allowed
Cycle 2: I  - VALU, TRANS, SALU, DS, VMEM allowed
Cycle 3: I  - VALU, TRANS, SALU, DS, VMEM allowed
Cycle 4: E  - SALU, DS, VMEM allowed
Cycle 5: E  - SALU, DS, VMEM allowed
Cycle 6: I  - VALU, TRANS, SALU, DS, VMEM allowed
Cycle 7: I  - VALU, TRANS, SALU, DS, VMEM allowed (last I slot)
Cycle 8: V  - Next WMMA only (no VALU/TRANS)
```

**Roofline Analysis**:

Step 1 - Slot Supply (4 WMMAs x slots each):
```
StageE slots: 4 x 4 = 16
StageI slots: 4 x 4 = 16  (cycles 2,3,6,7)
StageV slots: 4 x 1 = 4
Total: 36 fillable slots
```

Step 2 - Consumer Demand:
```
DS:    16 instructions (can use E or I slots)
SALU:  8 instructions  (can use E or I slots)
VALU:  24 instructions (can use I slots only)
TRANS: 4 instructions  (can use I slots only)
WMMA:  3 instructions  (can use V slots - WMMAs after first)
Total: 55 consumers
```

Step 3 - Max-Flow Assignment:
```
Flow network solution:
  StageI (16 slots): 16 VALU assigned
  StageE (16 slots): 16 DS assigned
  StageV (4 slots):  3 WMMA assigned, 1 empty

Remaining unassigned:
  VALU:  24 - 16 = 8 exposed
  DS:    16 - 16 = 0 exposed (all fit!)
  SALU:  8 - 0 = 8 exposed (I slots taken by VALU)
  TRANS: 4 - 0 = 4 exposed (I slots taken by VALU)
```

**Initial HardwareUnitInfo State**:
```
HWUI            TotalCycles  RemainingExposed  producesCoexecWindow
----------------------------------------------------------------------
WMMA            32           4                 true
SingleCycleVALU 24           8                 false
TRANS           8            4                 true
DS              960          0                 false
SALU            8            8                 false
```

**sortHWUIResources Result** (highest priority first):
1. **WMMA** (producer, TotalCycles=32)
2. **TRANS** (producer, TotalCycles=8)
3. **SingleCycleVALU** (RemainingExposed=8)
4. **SALU** (RemainingExposed=8, but lower TotalCycles)
5. **DS** (RemainingExposed=0, TotalCycles=960)

**Decision**: When choosing between a DS and a VALU instruction, `tryCriticalResource` prefers VALU because it has exposed cycles while DS does not.

---

### Example 2: Critical Resource Shift with FP8 WMMAs

**Scenario**: FP8 WMMA region where critical resource changes during scheduling

**WMMA Type**: `V_WMMA_F8_16x16x64` (pattern: `0EEIVV`)

```
Region Contents:
  - 8x V_WMMA_F8_16x16x64 (pattern: 0EEIVV, 4-cycle occupancy)
  - 20 DS loads
  - 16 SingleCycleVALU instructions
```

**CoExecInfo for V_WMMA_F8_16x16x64**:
```
Pattern: 0EEIVV
Cycle 0: E0 - Control only
Cycle 1: E  - SALU, DS, VMEM allowed
Cycle 2: E  - SALU, DS, VMEM allowed
Cycle 3: I  - VALU, TRANS, SALU, DS, VMEM allowed
Cycle 4: V  - Next WMMA only
Cycle 5: V  - Next WMMA only
```

**Roofline Analysis**:

Slot Supply (8 WMMAs):
```
StageE slots: 8 x 2 = 16
StageI slots: 8 x 1 = 8
StageV slots: 8 x 2 = 16
Total: 40 fillable slots
```

Consumer Demand:
```
DS:   20 instructions (E or I compatible)
VALU: 16 instructions (I only)
WMMA: 7 instructions  (V slots - WMMAs 2-8)
```

Max-Flow Solution:
```
StageI (8 slots):  8 VALU assigned
StageE (16 slots): 16 DS assigned
StageV (16 slots): 7 WMMA assigned

Exposed:
  VALU: 16 - 8 = 8 exposed
  DS:   20 - 16 = 4 exposed
  WMMA: 0 exposed (all fit in V slots)
```

**Initial HardwareUnitInfo State**:
```
HWUI            TotalCycles  RemainingExposed  producesCoexecWindow
----------------------------------------------------------------------
WMMA            32           8                 true
SingleCycleVALU 16           8                 false
DS              1200         4                 false
```

**sortHWUIResources Result** (highest priority first):
1. WMMA (producer)
2. SingleCycleVALU (RemainingExposed=8)
3. DS (RemainingExposed=4)

**After Scheduling 6 VALU Outside Co-exec Window**:
```
SingleCycleVALU: RemainingExposed = 8 -> 2
DS:              RemainingExposed = 4 (unchanged)
```

**New Sorted Priority**:
1. WMMA (producer)
2. **DS** (RemainingExposed=4 > 2) <- **Now higher priority!**
3. SingleCycleVALU (RemainingExposed=2)

**Critical Resource Shift**: DS becomes the new critical filler resource because VALU's exposed count dropped below DS's.

---

### Example 3: INT8 WMMA with Large Co-exec Windows

**Scenario**: INT8 WMMA with many co-execution slots

**WMMA Type**: `V_WMMA_I8_16x16x64` (pattern: `0EIIEEIIEEIIEEIIV`)

```
Region Contents:
  - 2x V_WMMA_I8_16x16x64 (pattern: 0EIIEEIIEEIIEEIIV, 16-cycle occupancy)
  - 30 DS loads
  - 10 SALU instructions
  - 20 SingleCycleVALU instructions
  - 6 TRANS instructions
```

**CoExecInfo for V_WMMA_I8_16x16x64**:
```
Pattern: 0EIIEEIIEEIIEEIIV (17 cycles)
Cycle  0: E0
Cycles 1,4,5,8,9,12,13: E (7 E slots)
Cycles 2,3,6,7,10,11,14,15: I (8 I slots)
Cycle 16: V

Per WMMA: 7 E slots, 8 I slots, 1 V slot
```

**Roofline Analysis**:

Slot Supply (2 WMMAs):
```
StageE slots: 2 x 7 = 14
StageI slots: 2 x 8 = 16
StageV slots: 2 x 1 = 2
Total: 32 fillable slots
```

Consumer Demand:
```
DS:    30 (E or I)
SALU:  10 (E or I)
VALU:  20 (I only)
TRANS: 6  (I only)
WMMA:  1  (V slots)
```

Max-Flow Solution:
```
StageI (16 slots): 16 VALU assigned (VALU gets priority for I slots)
StageE (14 slots): 14 DS assigned
StageV (2 slots):  1 WMMA assigned

Exposed:
  VALU:  20 - 16 = 4 exposed
  TRANS: 6 - 0 = 6 exposed (no I slots left)
  DS:    30 - 14 = 16 exposed
  SALU:  10 - 0 = 10 exposed (no E slots left)
```

**Initial HardwareUnitInfo State**:
```
HWUI            TotalCycles  RemainingExposed  producesCoexecWindow
----------------------------------------------------------------------
WMMA            32           2                 true
TRANS           12           6                 true
DS              1800         16                false
SALU            10           10                false
SingleCycleVALU 20           4                 false
```

**sortHWUIResources Result** (highest priority first):
1. TRANS (producer, RemainingExposed=6)
2. WMMA (producer, RemainingExposed=2)
3. DS (RemainingExposed=16)
4. SALU (RemainingExposed=10)
5. TRANS (RemainingExposed=6)
6. SingleCycleVALU (RemainingExposed=4)

**Key Insight**: Even though the scheduling region is bounded by DS, we still prioritize WMMA and TRANS. While these allow us to hide DS cycles, this may be suggesting a limitation with the current approach.

---

### Example 4: Scaled WMMA with IS Slots

**Scenario**: Scaled FP8 WMMA using IS slots for WMMA chaining

**WMMA Type**: `V_WMMA_F8_16x16x64_SCALE` (pattern: `0EESVV`)

```
Region Contents:
  - 6x V_WMMA_F8_16x16x64_SCALE (pattern: 0EESVV)
  - 12 DS loads
  - 12 SingleCycleVALU instructions
```

**CoExecInfo for V_WMMA_F8_16x16x64_SCALE**:
```
Pattern: 0EESVV
Cycle 0: E0 - Control only
Cycle 1: E  - SALU, DS, VMEM
Cycle 2: E  - SALU, DS, VMEM  
Cycle 3: IS - VALU, TRANS, SALU, DS, VMEM + next scaled WMMA can issue
Cycle 4: V  - Next WMMA only
Cycle 5: V  - Next WMMA only

The IS slot allows the LD_SCALE of the next scaled WMMA to execute there,
enabling back-to-back scaled WMMA chaining.
```

**Roofline Analysis**:

Slot Supply (6 scaled WMMAs):
```
StageE slots:  6 x 2 = 12
StageIS slots: 6 x 1 = 6  (can absorb VALU/TRANS OR next scaled WMMA)
StageV slots:  6 x 2 = 12
Total: 30 fillable slots
```

Consumer Demand:
```
DS:   12 (E or IS)
VALU: 12 (IS only among these slots)
WMMA: 5  (V or IS - WMMAs 2-6 can chain)
```

Max-Flow Solution:
```
Key insight: WMMA-to-WMMA chaining uses IS slots, competing with VALU!

StageIS (6 slots): 5 WMMA assigned (for chaining), 1 VALU
StageE (12 slots): 12 DS assigned
StageV (12 slots): empty (WMMAs already placed in IS)

Exposed:
  VALU: 12 - 1 = 11 exposed
  DS:   12 - 12 = 0 exposed
  WMMA: 0 exposed (all chain via IS slots)
```

**Initial HardwareUnitInfo State**:
```
HWUI            TotalCycles  RemainingExposed  producesCoexecWindow
----------------------------------------------------------------------
WMMA            24           6                 true
SingleCycleVALU 12           11                false
DS              720          0                 false
```

**sortHWUIResources Result** (highest priority first):
1. WMMA (producer)
2. SingleCycleVALU (RemainingExposed=11)
3. DS (RemainingExposed=0)

**Key Insight**: In scaled WMMA regions, VALU competes with WMMA chaining for IS slots, often leaving VALU as the critical exposed resource.

---

## Summary

The `tryCriticalResource` heuristic works in concert with `sortHWUIResources` to:

1. **Track resource pressure** via `TotalCycles` per HWUI
2. **Compute optimal slot assignment** via roofline max-flow analysis
3. **Estimate exposed cycles** via `RemainingExposed` (instructions not hideable in shadows)
4. **Dynamically re-prioritize** as resources are consumed during scheduling
5. **Prefer instructions** that use the highest-priority (most critical) resource
6. **Shift critical resources** as the scheduling progresses and pressure changes

The key insight is that **critical resources change dynamically** as scheduling proceeds. A resource that starts as critical may become less so after its instructions are scheduled, causing another resource to become the new bottleneck. The roofline analysis provides the initial "budget" of exposed cycles, and the scheduler tracks consumption to maintain accurate priorities throughout the scheduling process.
