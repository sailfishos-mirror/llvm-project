# Deep Dive: `tryCoexecSlot` Heuristic

## Overview

The `tryCoexecSlot` heuristic is part of the AMDGPU co-execution scheduling strategy (`AMDGPUCoExecSchedStrategy.cpp`). Its purpose is to select the best candidate instruction to schedule based on the preferences and avoidances defined for the current co-execution slot within an active multi-cycle instruction's window.

Multi-cycle instructions like WMMA (Wave Matrix Multiply-Accumulate), TRANS (transcendentals), and multi-cycle VALU create "co-execution windows" where other instructions can execute in parallel. Each slot in these windows has specific characteristics defining which instruction types can legally execute (correctness) and which types are preferred or should be avoided (performance optimization).

## CoExecInfo Structure

The `CoExecInfo` structure (defined in `AMDGPUCoExecInfo.h`) encapsulates the co-execution characteristics for a multi-cycle instruction:

```cpp
struct CoExecInfo {
  unsigned Occupancy = 0;      // Cycles until unit is free
  unsigned TotalWindow = 0;    // Total co-execution window size
  CoExecSlotInfo Slots[MaxCoExecStages];  // Per-stage slot info
  unsigned LastIStage = 0;     // Last I-stage index
  bool HasScaling = false;     // True for FP8/FP6/FP4 scaled variants
  StringRef Pattern;           // Pattern string (e.g., "0EIIEEIIV")
};
```

### CoExecSlotInfo

Each slot in the window has its own `CoExecSlotInfo`:

```cpp
struct CoExecSlotInfo {
  CoExecMaskT Mask = CoExecMask::All;       // What CAN execute (correctness)
  FlavorMask PreferredFlavors = FlavorMasks::None;  // Flavors to prefer
  FlavorMask AvoidedFlavors = FlavorMasks::None;    // Flavors to avoid
  uint8_t TypeIndex = 0;                    // Index within type (0=first E, etc)
};
```

### Slot Types

The pattern string defines slot types:
- `0` (E0): Issue cycle - control instructions only
- `E` (External): Memory and SALU can co-execute, no VALU
- `I` (Internal): VALU, TRANS, memory, and SALU can all co-execute  
- `S` (IS): Internal + scaled-WMMA absorb
- `V` (Vacant): Memory/SALU/next-WMMA ok, NO VALU/TRANS
- `T` (TR): TRANS co-exec - everything except TRANS

### Building CoExecInfo with Preferences

The `CoExecInfo::build()` and fluent interface methods allow defining preferences:

```cpp
return CoExecInfo::build(8, 10, "0EEIEEISVV", 7, HasScaling)
    .preferring(1, flavorBit(InstructionFlavor::DS))      // Prefer DS at slot 1
    .avoiding(2, flavorBit(InstructionFlavor::DS))        // Avoid DS at slot 2
    .preferring(3, flavorBit(InstructionFlavor::TRANS))   // Prefer TRANS at slot 3
    .preferring(7, flavorBit(InstructionFlavor::WMMA));   // Prefer WMMA at slot 7
```

## InstructionFlavor Classification

Instructions are classified into flavors that map to hardware execution characteristics:

```cpp
enum class InstructionFlavor : uint8_t {
  WMMA,            // WMMA/MFMA matrix operations
  SingleCycleVALU, // Single-cycle VALU (not TRANS, not multi-cycle CVT)
  TRANS,           // Transcendental ops (v_exp, v_log, etc.)
  MultiCycleVALU,  // VALU instructions with repeat rate > 1
  VMEM,            // FLAT/GLOBAL memory operations
  DS,              // LDS/GDS operations
  SALU,            // Scalar ALU
  DMA,             // Tensor DMA operations
  Fence,           // Fences and waits
  Other,           // Everything else
};
```

## Precondition: Equal Stall Cycles

**Important**: `tryCoexecSlot` is invoked *after* `tryEffectiveStall` in the heuristic chain. The `tryEffectiveStall` heuristic compares stall costs and selects the candidate with lower stalls when they differ. Therefore, by the time `tryCoexecSlot` is reached, **both candidates have equal stall cycles**. If one candidate had fewer stall cycles, `tryEffectiveStall` would have already selected it and `tryCoexecSlot` would never be called.

This precondition simplifies reasoning about `tryCoexecSlot`: both candidates will land at the same effective slot (`Stage + StallCycles`), so the decision is purely about which instruction's flavor better matches that slot's preferences and avoidances.

## The tryCoexecSlot Implementation

```cpp
bool CandidateHeuristics::tryCoexecSlot(
    GenericSchedulerBase::SchedCandidate &Cand,
    GenericSchedulerBase::SchedCandidate &TryCand, SchedBoundary *Zone) {
  
  // 1. Get the current co-execution stage from the hazard recognizer
  auto HazardRec = static_cast<GCNHazardRecognizer *>(Zone->HazardRec);
  std::optional<unsigned> Stage = HazardRec->getCurrentCoExecStage();
  if (!Stage.has_value())
    return false;  // No active co-exec window

  // 2. Get the CoExecInfo for the active multi-cycle instruction
  const CoExecInfo &Info = HazardRec->getActiveCoExecInfo();

  // 3. Classify both candidates
  InstructionFlavor TryFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);
  InstructionFlavor CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);

  // 4. Compute stall costs for both candidates
  //    (Precondition: TryStall == CandStall, since tryEffectiveStall already ran)
  StallCosts TryStallCost;
  StallCosts CandStallCost;
  unsigned TryStall = getStallCosts(TryCand.SU, *Zone, TryStallCost);
  unsigned CandStall = getStallCosts(Cand.SU, *Zone, CandStallCost);

  // 5. First: prefer the candidate that AVOIDS the avoided slot
  //    (i.e., prefer the instruction whose flavor is NOT avoided)
  if (tryLess(Info.avoidsFlavor(*Stage + TryStall, TryFlavor),
              Info.avoidsFlavor(*Stage + CandStall, CandFlavor), TryCand, Cand,
              GenericSchedulerBase::CandReason::RegCritical))
    return true;

  // 6. Second: prefer the candidate that matches slot preferences
  if (tryGreater(Info.prefersFlavor(*Stage + TryStall, TryFlavor),
                 Info.prefersFlavor(*Stage + CandStall, CandFlavor), TryCand,
                 Cand, GenericSchedulerBase::CandReason::RegCritical))
    return true;

  return false;
}
```

### Key Mechanisms

1. **Stage Lookup**: The current co-execution stage is obtained from `GCNHazardRecognizer`, which tracks the progress through the active multi-cycle instruction's window.

2. **Stall Adjustment**: When an instruction would stall (due to dependencies, buffer pressure, or hazards), the effective slot it will land in is `Stage + StallCycles`. Since both candidates have equal stall cycles (see precondition above), they target the same effective slot.

3. **Avoidance Check (Higher Priority)**: The `tryLess` on `avoidsFlavor` means we prefer the candidate with `false` (not avoided) over `true` (avoided). This check happens first because avoiding bad placements is more important than matching preferences.

4. **Preference Check**: The `tryGreater` on `prefersFlavor` means we prefer the candidate with `true` (preferred) over `false` (not preferred).

## Examples

All examples assume the precondition holds: both candidates have equal stall cycles (otherwise `tryEffectiveStall` would have already decided).

### Example 1: Avoidance Decides (No Stalls)

**Scenario**: We are at stage 3 of an 8-cycle F16 16x16x32 WMMA with pattern `0EIIEEISV`. The `CoExecInfo` was built with:
```cpp
.preferring(3, flavorBit(InstructionFlavor::TRANS))
.avoiding(3, flavorBit(InstructionFlavor::DS))
```

**Candidates** (both stall for 0 cycles):
- `TryCand`: `v_exp_f32` (TRANS flavor)
- `Cand`: `ds_read_b128` (DS flavor)

**Evaluation**:

| Step | Check | TryCand (TRANS) | Cand (DS) | Result |
|------|-------|-----------------|-----------|--------|
| 1 | `EffectiveSlot = Stage + Stall` | 3 + 0 = 3 | 3 + 0 = 3 | Same slot |
| 2 | `avoidsFlavor(3, flavor)` | `false` (TRANS not avoided) | `true` (DS avoided) | - |
| 3 | `tryLess(false, true)` | - | - | **TryCand wins** |

**Outcome**: `TryCand` (`v_exp_f32`) is selected because DS is avoided at slot 3, while TRANS is not. The preference check for TRANS is never reached because the avoidance check already decided.

---

### Example 2: Avoidance with No Clear Preference

**Scenario**: Stage 2 of pattern `0EESVV` (FP8 16x16x64 WMMA). Configuration:
```cpp
.preferring(1, flavorBit(InstructionFlavor::DS))
.avoiding(2, flavorBit(InstructionFlavor::DS))
```

**Candidates** (both stall for 0 cycles):
- `TryCand`: `v_add_f32` (SingleCycleVALU flavor)
- `Cand`: `ds_write_b64` (DS flavor)

**Evaluation**:

| Step | Check | TryCand (VALU) | Cand (DS) | Result |
|------|-------|----------------|-----------|--------|
| 1 | `EffectiveSlot` | 2 | 2 | Same slot |
| 2 | `avoidsFlavor(2, flavor)` | `false` (VALU not avoided) | `true` (DS avoided) | - |
| 3 | `tryLess(false, true)` | - | - | **TryCand wins** |

**Outcome**: `TryCand` (`v_add_f32`) wins. Even though VALU is not *preferred* at slot 2, it wins because DS is *avoided*.

---

### Example 3: Preference Decides When Nothing Avoided

**Scenario**: Stage 3 of pattern `0EEIEEIIVV` (FP8 16x16x128 WMMA). Configuration:
```cpp
.preferring(3, flavorBit(InstructionFlavor::TRANS))
```

**Candidates** (both stall for 0 cycles):
- `TryCand`: `v_exp_f32` (TRANS flavor)
- `Cand`: `v_add_f32` (SingleCycleVALU flavor)

**Evaluation**:

| Step | Check | TryCand (TRANS) | Cand (VALU) | Result |
|------|-------|-----------------|-------------|--------|
| 1 | `EffectiveSlot` | 3 | 3 | Same slot |
| 2 | `avoidsFlavor(3, flavor)` | `false` | `false` | Tie |
| 3 | `prefersFlavor(3, flavor)` | `true` (TRANS preferred) | `false` (VALU not preferred) | - |
| 4 | `tryGreater(true, false)` | - | - | **TryCand wins** |

**Outcome**: `TryCand` (`v_exp_f32`) wins because TRANS is preferred at slot 3, while SingleCycleVALU is not.

---

### Example 4: Stalled Instructions at Later Slot

**Scenario**: Stage 4 of pattern `0EEIEEIIVV` (FP8 16x16x128 WMMA). Configuration:
```cpp
.avoiding(4, flavorBit(InstructionFlavor::DS))
.preferring(6, flavorBit(InstructionFlavor::SingleCycleVALU))
.avoiding(6, flavorBit(InstructionFlavor::DS))
```

**Candidates** (both stall for 2 cycles due to pending dependencies):
- `TryCand`: `ds_read_b128` (DS flavor)
- `Cand`: `v_fma_f32` (SingleCycleVALU flavor)

**Evaluation**:

| Step | Check | TryCand (DS) | Cand (VALU) | Result |
|------|-------|--------------|-------------|--------|
| 1 | `EffectiveSlot = Stage + Stall` | 4 + 2 = 6 | 4 + 2 = 6 | Same slot |
| 2 | `avoidsFlavor(6, flavor)` | `true` (DS avoided at 6) | `false` (VALU not avoided) | - |
| 3 | `tryLess(true, false)` | - | - | **Cand wins** |

**Outcome**: `Cand` (`v_fma_f32`) wins. Both instructions stall equally, so they both land at slot 6. At slot 6, DS is avoided while VALU is preferred.

**Key insight**: The stall adjustment means we evaluate preferences at the slot where the instruction will *actually* execute. DS is avoided at slot 4, but since both instructions stall past it to slot 6, slot 4's avoidance is irrelevant.

---

### Example 5: Neither Preferred Nor Avoided (No Decision)

**Scenario**: Stage 5 of pattern `0EEIEEISVV`. Configuration:
```cpp
.preferring(4, flavorBit(InstructionFlavor::DS))
.preferring(7, flavorBit(InstructionFlavor::WMMA))
```

**Candidates** (both stall for 0 cycles):
- `TryCand`: `ds_read_b64` (DS flavor)
- `Cand`: `s_add_u32` (SALU flavor)

**Evaluation**:

| Step | Check | TryCand (DS) | Cand (SALU) | Result |
|------|-------|--------------|-------------|--------|
| 1 | `EffectiveSlot` | 5 | 5 | Same slot |
| 2 | `avoidsFlavor(5, flavor)` | `false` (DS not avoided at 5) | `false` (SALU not avoided at 5) | Tie |
| 3 | `prefersFlavor(5, flavor)` | `false` (DS not preferred at 5) | `false` (SALU not preferred at 5) | Tie |

**Outcome**: Neither candidate wins from `tryCoexecSlot`; later heuristics will decide. Slot 5 has no preferences or avoidances for either DS or SALU.

---

## Summary

The `tryCoexecSlot` heuristic:

1. **Precondition: equal stall cycles** - this heuristic runs after `tryEffectiveStall`, so both candidates stall for the same number of cycles and will land at the same effective slot
2. **Requires an active co-execution window** - returns `false` if no multi-cycle instruction is in flight
3. **Computes effective slot** - uses `Stage + StallCycles` to determine where the instruction will actually execute
4. **Prioritizes avoidance over preference** - first checks if a flavor should be avoided at the effective slot, then checks if preferred
5. **Uses the CoExecInfo structure** - which encodes per-slot masks, preferences, and avoidances defined for each WMMA/TRANS/MultiCycleVALU pattern

This heuristic is one component of a larger scheduling strategy that also considers critical resource dependencies, loop-carried latencies, shadow mix optimization, and other factors to maximize instruction throughput on AMDGPU hardware.
