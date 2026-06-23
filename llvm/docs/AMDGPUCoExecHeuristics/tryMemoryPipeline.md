# tryMemoryPipeline Heuristic Deep Dive

## Overview

The `tryMemoryPipeline` heuristic maintains the structure of software-pipelined memory operations while avoiding in-iteration stalls. On gfx1250, memory pipelines typically use tensor DMA operations (via `llvm.amdgcn.tensor.load.to.lds` intrinsics) with barrier synchronization to overlap data loading with computation across loop iterations.

The key insight is that **cross-iteration latency hiding is the responsibility of the kernel writer or upstream passes** (e.g., Triton's pipelining). The scheduler's job is to:

1. **Maintain the original pipeline structure** by scheduling memory pipeline instructions as soon as they become stall-free
2. **Avoid in-iteration stalls** by waiting until these instructions have minimal or zero stall cycles

## Instruction Classification

Memory pipeline instructions are identified by their `InstructionFlavor`:

| Flavor | Description | Examples |
|--------|-------------|----------|
| `DMA` | Tensor DMA operations (LDS DMA) | `llvm.amdgcn.tensor.load.to.lds` intrinsics, tracked by `S_WAIT_TENSORCNT` |
| `Fence` | Memory fences and waits | `S_BARRIER_SIGNAL_IMM`, `S_BARRIER_WAIT`, `S_WAIT_DSCNT`, `S_WAIT_TENSORCNT`, `ATOMIC_FENCE` |

The classification is performed by `classifyFlavor()`:

```cpp
// From AMDGPUCoExecSchedStrategy.cpp:202-213
if (Opc == AMDGPU::ATOMIC_FENCE || Opc == AMDGPU::S_WAIT_ASYNCCNT ||
    Opc == AMDGPU::S_WAIT_TENSORCNT || Opc == AMDGPU::S_BARRIER_WAIT ||
    Opc == AMDGPU::S_BARRIER_SIGNAL_IMM || SII.isWaitcnt(Opc))
  return InstructionFlavor::Fence;

if (SII.isLDSDMA(MI))
  return InstructionFlavor::DMA;
```

## The tryMemoryPipeline Algorithm

```cpp
bool CandidateHeuristics::tryMemoryPipeline(
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand, SchedBoundary *Zone) {

  InstructionFlavor TryFlavor = classifyFlavor(*TryCand.SU->getInstr(), *SII);
  InstructionFlavor CandFlavor = classifyFlavor(*Cand.SU->getInstr(), *SII);

  // Step 1: Check if either candidate is a memory pipeline instruction
  bool TryIsMemoryPipeline = TryFlavor == InstructionFlavor::DMA ||
                             TryFlavor == InstructionFlavor::Fence;
  bool CandIsMemoryPipeline = CandFlavor == InstructionFlavor::DMA ||
                              CandFlavor == InstructionFlavor::Fence;

  // Step 2: If neither is a memory pipeline instruction, no opinion
  if (!(TryIsMemoryPipeline || CandIsMemoryPipeline))
    return false;

  // Step 3: Check stall costs - only consider if stall-free
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

  // Step 4: If both or neither qualify after stall check, no decision
  if (TryIsMemoryPipeline == CandIsMemoryPipeline)
    return false;

  // Step 5: Prefer the stall-free memory pipeline instruction
  if (CandIsMemoryPipeline) {
    if (Cand.Reason > GenericSchedulerBase::RegCritical)
      Cand.Reason = GenericSchedulerBase::RegCritical;
    return true;
  }

  TryCand.Reason = GenericSchedulerBase::RegCritical;
  return true;
}
```

## Interaction with tryEffectiveStall

The `tryMemoryPipeline` heuristic is called **after** `tryEffectiveStall` in the heuristic ordering:

```cpp
// From tryCandidateCoexec(), lines 2704-2714
if (Heurs.tryEffectiveStall(TryCand, Cand, *Zone)) {
  LastAMDGPUReason = AMDGPUSchedReason::Stall;
  return TryCand.Reason != NoCand;
}

if (Heurs.tryMemoryPipeline(TryCand, Cand, Zone)) {
  LastAMDGPUReason = AMDGPUSchedReason::MemoryPipeline;
  return TryCand.Reason != NoCand;
}
```

This ordering is important:

1. **tryEffectiveStall** ensures we pick instructions with minimal overall stall
2. **tryMemoryPipeline** then gives priority to memory pipeline instructions that are stall-free

However, tryMemoryPipeline also internally checks stalls via `getStallCosts()` to ensure it only promotes memory pipeline instructions that are genuinely ready without stalls.

## StallCosts Structure

The `getStallCosts()` function (lines 1759-1885) computes multiple stall components:

```cpp
struct StallCosts {
  unsigned Ready = 0;       // Cycles until SU becomes DAG-ready
  unsigned Structural = 0;  // Resource/hazard stalls
  unsigned Latency = 0;     // Latency-based stalls
  unsigned Carried = 0;     // Cross-iteration carried latency
  unsigned Buffer = 0;      // Buffer-full stalls (e.g., DS queue)
  unsigned Fence = 0;       // Fence latency (barrier_signal → barrier_wait)
  unsigned RAWVdst = 0;     // VALU → VMEM/DS RAW hazard
  unsigned Effective = 0;   // max of all above
};
```

The **Effective** cost is the maximum of all components. For a memory pipeline instruction to be promoted by `tryMemoryPipeline`, its `Effective` cost must be **zero**.

## Controlling Latency

On gfx1250, one control for memory pipeline latency is:

### amdgpu-barrier-signal-wait-latency

This attribute controls the synthetic latency added between `S_BARRIER_SIGNAL_IMM` and `S_BARRIER_WAIT` instructions:

```cpp
// From AMDGPUBarrierLatency.cpp:32-36
static cl::opt<unsigned> BarrierSignalWaitLatencyOpt(
    "amdgpu-barrier-signal-wait-latency",
    cl::desc("Synthetic latency between S_BARRIER_SIGNAL and S_BARRIER_WAIT "
             "to encourage scheduling independent work between them"),
    cl::init(35), cl::Hidden);
```

The `AMDGPUBarrierLatencyDAGMutation` pass adds this latency to DAG edges:

```cpp
// From AMDGPUBarrierLatency.cpp:130-137
} else if (Op == AMDGPU::S_BARRIER_WAIT) {
  for (SDep &PredDep : SU.Preds) {
    SUnit *PredSU = PredDep.getSUnit();
    const MachineInstr *PredMI = PredSU->getInstr();
    if (TII->isBarrierStart(PredMI->getOpcode())) {
      addLatencyToEdge(PredDep, SU, BarrierSignalWaitLatency);
    }
  }
}
```

This creates scheduling pressure to place independent work between barrier_signal and barrier_wait, allowing the memory pipeline to hide latency.


### The DS Latency Mode Flag

Additionally, the `amdgpu-ds-latency-mode` flag controls the assumed latency for DS (LDS) operations, which directly affects when fences can be scheduled:

```cpp
// From SIInstrInfo.h:1231-1235
enum class DSLatencyMode {
  Fast,      // Use default/fast latency (no contention)
  Loaded,    // Use loaded latency (moderate contention, 60 cycles)
  Overloaded // Use overloaded latency (high contention, 100 cycles)
};
```

The CoExec scheduler defaults to `Loaded` mode (60 cycles):

```cpp
// From AMDGPUCoExecSchedStrategy.cpp:2791
SIInstrInfo::setDSLatencyMode(SIInstrInfo::DSLatencyMode::Loaded);
```

This latency is used when computing HWUI cycles for DS operations:

```cpp
// From AMDGPUCoExecSchedStrategy.cpp:781-785
if (SII->getSubtarget().hasGFX1250Insts() && SII->isDS(*SU->getInstr())) {
  if (auto Latency = SIInstrInfo::getDSLatencyMode())
    return *Latency;
}
```

## How DS Operations Push Back Fences

As mentioned earlier, one of the main goals of tryMemoryPipeline is to preserve the incoming memory pipeline. One of the main challenges with this is to preserve the clustering of all instructions that participate in a fence.

For a pipelined kernel, without explicit controls we will typically see that the ATOMIC_FENCE gets pushed back by predecessor DS_LOAD instructions, but other instruction that participate in the fence are unaffected.

### ATOMIC_FENCE and DS Load Dependencies

Consider the following case:

`S_WAIT_TENSORCNT`
`ATOMIC_FENCE`
`S_BARRIER_SIGNAL_IMM`
`S_BARRIER_WAIT`
`ATOMIC_FENCE`


These instructions have different DAG dependencies:

| Instruction | Has DS Load Dependencies | When Schedulable |
|-------------|-------------------------|------------------|
| `S_WAIT_TENSORCNT` | No | After independent stalls are resolved |
| `ATOMIC_FENCE` (release) | Yes - barrier edges to preceding DS loads | After DS latency elapses |
| `S_BARRIER_SIGNAL_IMM` | Depends on preceding fence | After fence is scheduled |
| `S_BARRIER_WAIT` | Has synthetic latency from signal (35 cycles) | After signal + latency |
| `ATOMIC_FENCE` (acquire) | Depends on barrier_wait | After barrier_wait is scheduled |

The `AMDGPUBarrierLatencyDAGMutation` pass adds latency to ATOMIC_FENCE's barrier edges:

```cpp
// From AMDGPUBarrierLatency.cpp:105-129
if (Op == AMDGPU::ATOMIC_FENCE) {
  for (SDep &PredDep : SU.Preds) {
    if (!PredDep.isBarrier())
      continue;
    SUnit *PredSU = PredDep.getSUnit();
    MachineInstr *MI = PredSU->getInstr();
    // Only consider memory loads
    if (!MI->mayLoad() || MI->mayStore())
      continue;

    unsigned Latency = SchedModel->computeInstrLatency(MI, false);
    // DS load/store latency is variable depending on LDS contention.
    if (TII->getSubtarget().hasGFX1250Insts() && TII->isDS(*MI)) {
      if (auto LatencyMode = SIInstrInfo::getDSLatencyMode())
        Latency = *LatencyMode;  // 60 cycles in Loaded mode
    }
    setLatencyForEdge(PredDep, SU, Latency);
  }
}
```

This means ATOMIC_FENCE (release) cannot be scheduled until the DS latency (e.g., 60 cycles) has elapsed since the preceding `DS_LOAD_TR16_B128` operations. 

### How DS Loads Affect ATOMIC_FENCE Scheduling

The `DS_LOAD_TR16_B128` operations have significant latency (60 cycles in Loaded mode). The ATOMIC_FENCE (release) has DAG barrier edges to these DS loads with this latency attached:

```
DAG Structure (as seen during scheduling):
  DS_LOAD_TR16_B128 ──barrier(latency=60)──► ATOMIC_FENCE(release) ──► S_BARRIER_SIGNAL_IMM
  DS_LOAD_TR16_B128 ──barrier(latency=60)──┘                                   │
  DS_LOAD_TR16_B128 ──barrier(latency=60)──┘                          (latency=35)
        ...                                                                    ▼
  (32 total DS loads)                                                  S_BARRIER_WAIT
                                                                               │
                                                                               ▼
                                                                   ATOMIC_FENCE(acquire)
```

Example Timeline:
```
  Cycle 100: Last DS_LOAD_TR16_B128 scheduled (TopReadyCycle = 100)
  Cycle 100-159: DS load latency window (60 cycles)
  Cycle 160: ATOMIC_FENCE (release) can now be scheduled (latency edge satisfied)
  Cycle 161: S_BARRIER_SIGNAL_IMM can be scheduled
  Cycle 161-195: barrier_signal_wait_latency window (35 cycles)
  Cycle 196: S_BARRIER_WAIT can be scheduled
  Cycle 197: ATOMIC_FENCE (acquire) can be scheduled
```


However, the `S_WAIT_TENSORCNT` (which also participates in the fence) may be available for scheduling much earlier.


The `getFenceStalls` lambda in `getStallCosts()` ensures that all InstructionFlavor::Fence instructions respect the latency of preceding DS operations:

```cpp
// From AMDGPUCoExecSchedStrategy.cpp:1781-1808
auto getFenceStalls = [this, &CurrCycle, &Zone](SUnit *SU) -> unsigned {
  InstructionFlavor Flavor = classifyFlavor(*SU->getInstr(), *SII);

  // Only applies to Fence instructions (top-down scheduling)
  if (Flavor != InstructionFlavor::Fence)
    return 0;

  // Find the last scheduled DS operation
  HardwareUnitInfo *ProducerHWUI = getHWUIFromFlavor(InstructionFlavor::DS);
  SUnit *LastProducer = ProducerHWUI->getLastScheduledSU();
  if (!LastProducer)
    return 0;

  // Calculate when the DS operation's latency will complete
  unsigned FenceStallFinish =
      LastProducer->TopReadyCycle + getHWUICyclesForSU(LastProducer);
  
  // Return remaining stall cycles
  return FenceStallFinish <= CurrCycle ? 0 : FenceStallFinish - CurrCycle;
};
```

This means: **a fence cannot be scheduled until the DS latency (e.g., 60 cycles in Loaded mode) has elapsed since the last DS operation**.

**Important**: This check applies to **all** instructions with `InstructionFlavor::Fence`, not just ATOMIC_FENCE. Recall that the Fence flavor includes:
- `ATOMIC_FENCE`
- `S_WAIT_TENSORCNT`
- `S_WAIT_DSCNT`
- `S_BARRIER_SIGNAL_IMM`
- `S_BARRIER_WAIT`
- Other wait/barrier instructions

The purpose of applying `getFenceStalls` to all Fence-flavor instructions is to **keep fence-related instructions scheduled together**. Consider the DAG dependencies:

```
DS_LOAD_TR16_B128 ──barrier(latency=60)──► ATOMIC_FENCE
DS_LOAD_TR16_B128 ──barrier(latency=60)──┘
        ...

S_WAIT_TENSORCNT  (no DS_LOAD dependencies!)
```

The ATOMIC_FENCE has explicit barrier edges to the DS loads with DS latency attached. However, `S_WAIT_TENSORCNT` typically has **no DAG dependencies** on the DS loads - it depends on the tensor DMA operations, not the DS operations. Without the `getFenceStalls` check, `S_WAIT_TENSORCNT` would become DAG-ready much earlier than ATOMIC_FENCE and could be scheduled far ahead of it.

This would be problematic because:
1. The fence-related instructions form a logical group that should execute together
2. Scheduling `S_WAIT_TENSORCNT` early would break the intended pipeline structure
3. The kernel writer/frontend structured the IR with these instructions adjacent for a reason

By applying `getFenceStalls` to all Fence-flavor instructions, we ensure that:
- `S_WAIT_TENSORCNT` waits for DS latency even though it has no DS dependencies in the DAG
- All fence-related instructions become schedulable at approximately the same time
- `tryMemoryPipeline` promotes them together once they're all stall-free


## Example: BF16 Flash Attention Memory Pipeline

Consider the memory pipeline pattern from Gluon BF16 Flash Attention kernel. The software-pipelined loop body looks like:

```
Iteration N (as seen during scheduling):
  S_WAIT_TENSORCNT 2                    ; Wait for tensor DMA (from iteration N-1) to complete
                                        ; (count=2 means wait for all but 2 outstanding)
  
  ATOMIC_FENCE (release)                ; Memory fence with barrier edges to DS operations
                                        ; (removed in final assembly, but controls scheduling)
  
  S_BARRIER_SIGNAL_IMM                  ; Signal that this wave reached the barrier
  
  S_BARRIER_WAIT                        ; Wait for all waves (35 cycles after signal)
  
  ATOMIC_FENCE (acquire)                ; Memory fence after barrier
                                        ; (removed in final assembly)

  DS_LOAD_TR16_B128 operations          ; Transpose-load data from LDS into VGPRs
                                        ; (data was loaded in iteration N-1)
  
  TENSOR_LOAD_TO_LDS (iteration N+1)    ; Start loading next iteration's data to LDS
```

The key insight is that `ds_load_tr16_b128` (DS transpose load) operations are what ultimately **consume** the data loaded by `tensor_load_to_lds` from a previous iteration. The tensor DMA writes directly to LDS, and ds_load brings that data into VGPRs for WMMA operations. As a result, we want to be sure we complete ds_loads before scheduling the next tensor_load_to_lds (to prevent the tensor_load_to_lds from overwriting the LDS), and we want to be sure we have an appropriate wait_tensorcnt before the ds_loads (to be sure the desired data is in LDS).

### How tryMemoryPipeline Works in This Context

**Initial State**: The loop body has been pipelined by Triton

**Step 1: s_wait_tensorcnt (from previous iteration's tensor_load_to_lds)**
- The wait_tensorcnt is inserted by Triton
- In the optimized case, the wait_tensorcnt does not wait for tensor_loads in this iteration (count=2 means wait for all but 2 outstanding). We have code in AMDGPUBarrierLatencyDAGMutation to detect that the wait_tensorcnt does not depend on this iteration's tensor_load_to_lds, and thus we do not need latency for these edges.
- When scheduler sees there is no associated stall, `tryMemoryPipeline` promotes it

**Step 2: ATOMIC_FENCE (release)**
- Has barrier edges from previous DS loads, but since this is the beginning of the iteration, no previous DS loads
- `tryMemoryPipeline` promotes it
- This fence is removed in final assembly but controls scheduling

**Step 3: S_BARRIER_SIGNAL_IMM**
- Signals that this wave has reached the barrier point
- Depends only on the preceding ATOMIC_FENCE (release), but this dependency does not have latency
- `tryMemoryPipeline` promotes it immediately after ATOMIC_FENCE

**Step 4: S_BARRIER_WAIT**
- Waits for all waves to signal (synchronization across waves)
- Has synthetic latency from S_BARRIER_SIGNAL_IMM (35 cycles default via `amdgpu-barrier-signal-wait-latency`)
- `tryMemoryPipeline` promotes it once the latency is satisfied

**Step 5: ATOMIC_FENCE (acquire)**
- Depends on S_BARRIER_WAIT, but this dependency does not have latency
- `tryMemoryPipeline` promotes it immediately after barrier_wait

**Step 6: DS_LOAD_TR16_B128 operations (consuming previous iteration's data)**
- Load data from LDS into VGPRs
- Each DS load has 60-cycle latency (in Loaded mode)
- Future ATOMIC_FENCE (release) has barrier edges with this latency (not shown)

**Step 7: TENSOR_LOAD_TO_LDS operations**
- These mayAlias with the DS_LOAD, so there is a dependency between these instructions
- Once the DS_LOADs and address computations complete, TENSOR_LOAD_TO_LDS instructions become DAG-ready
- If `Effective == 0`, they are scheduled immediately
- These load data for the *next* iteration


### Worked Example

Consider this simplified scheduling scenario based on bf16_fa.ll with DS latency = 60 cycles and barrier_signal_wait_latency = 35 cycles. This example shows one iteration of a pipelined loop, using instruction names as they appear during scheduling:

```
Cycle 0:
  S_WAIT_TENSORCNT 2: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Waits for previous iteration's TENSOR_LOAD_TO_LDS to complete;
   count=2 means this iteration's tensor_loads are still in flight)

Cycle 1:
  ATOMIC_FENCE (release): Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Previous iteration's DS loads have completed their latency)

Cycle 2:
  S_BARRIER_SIGNAL_IMM: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Signal that this wave has reached the barrier)

Cycle 3-36:
  Independent SALU, VALU and WMMA

Cycle 37:
  S_BARRIER_WAIT: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (35 cycles after S_BARRIER_SIGNAL_IMM - latency satisfied)

Cycle 38:
  ATOMIC_FENCE (acquire): Fence, Effective=0 → PROMOTED by tryMemoryPipeline

Cycle 39-306:
  V_WMMA_F32_16X16X32_BF16 operations
  DS_LOAD operations interleaved

Cycle 307:
  DS_LOAD_TR16_B128 (picked by indepedent heuristic)
  (last ready DS_LOAD scheduled)

Cycle 309:
  TENSOR_LOAD_TO_LDS_d2: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (first in iteration TENSOR_LOAD_TO_LDS_d2)

--- Next sub-iteration begins ---

Cycle 366:
  S_WAIT_TENSORCNT 2: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Previous DS loads have completed their latency)

Cycle 367:
  ATOMIC_FENCE (release): Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Previous DS loads have completed their latency)

Cycle 368:
  S_BARRIER_SIGNAL_IMM: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (Signal that this wave has reached the barrier)

Cycle 369-402:
  Independent SALU, VALU and WMMA

Cycle 403:
  S_BARRIER_WAIT: Fence, Effective=0 → PROMOTED by tryMemoryPipeline
  (35 cycles after S_BARRIER_SIGNAL_IMM - latency satisfied)

Cycle 404:
  ATOMIC_FENCE (acquire): Fence, Effective=0 → PROMOTED by tryMemoryPipeline


...

```

**Key observations**: 
1. The S_BARRIER_WAIT at cycle 37 demonstrates the 35-cycle `amdgpu-barrier-signal-wait-latency` between signal and wait
2. The S_WAIT_TENSORCNT at cycle 366 demonstrates the effect of LDS latency pushing back all operations that participate in the fence


## Why This Design Works

### 1. Minimal Stall = Schedule Immediately
By requiring `Effective == 0`, we ensure memory pipeline instructions are only promoted when they won't introduce stalls. This prevents degrading the memory pipeline.

### 2. Structure Preservation
By promoting memory pipeline instructions as soon as they're stall-free (rather than delaying them for other heuristics), we maintain the original program order structure that was carefully designed for latency hiding.

### 3. Cooperation with Other Heuristics
- `tryEffectiveStall` runs first, ensuring basic stall minimization
- `tryMemoryPipeline` then adds priority for memory pipeline instructions

### 4. User Controls
The `amdgpu-barrier-signal-wait-latency` attribute gives kernel writers control over how much work should be scheduled between barrier_signal and barrier_wait, directly controlling the pipeline depth.
The `amdgpu-ds-latency-mode` flag gives kernel writes control over how much latency ds instructions have, which directly impacts the spacing out of fences inside the scheduling region.


## Summary

`tryMemoryPipeline` is a structure-preserving heuristic that:

1. Identifies memory pipeline instructions (DMA and Fence flavors)
2. Checks if they can be scheduled without stalls
3. Promotes stall-free memory pipeline instructions immediately
4. Works with `amdgpu-barrier-signal-wait-latency` and `amdgpu-ds-latency-mode` to control pipeline
5. Cooperates with `tryEffectiveStall` to avoid introducing new stalls

The key principle is: **schedule memory pipeline operations as early as possible while maintaining zero stalls**, trusting that the upstream compiler has structured the pipeline for optimal cross-iteration latency hiding.
