// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -x cuda \
// RUN:   -fcuda-is-device -fclangir -emit-cir \
// RUN:   -mmlir -mlir-print-ir-before=cir-target-lowering %s -o %t.cir 2> %t-pre.cir
// RUN: FileCheck --check-prefix=CIR-PRE --input-file=%t-pre.cir %s

// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -x cuda \
// RUN:   -fcuda-is-device -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR-POST --input-file=%t.cir %s

// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -x cuda \
// RUN:   -fcuda-is-device -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --check-prefix=CIR-LLVM --input-file=%t-cir.ll %s

// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -x cuda \
// RUN:   -fcuda-is-device -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=OGCG --input-file=%t.ll %s

// Verifies CIR emits correct address spaces for CUDA globals.

#include "Inputs/cuda.h"

// CIR-PRE: cir.global external  lang_address_space(offload_global) @i = #cir.int<0> : !s32i
// CIR-POST: cir.global external  target_address_space(1) @i = #cir.int<0> : !s32i
// CIR-LLVM-DAG: @i = addrspace(1) externally_initialized global i32 0, align 4
// OGCG-DAG: @i = addrspace(1) externally_initialized global i32 0, align 4
__device__ int i;

// CIR-PRE: cir.global constant external  lang_address_space(offload_constant) @j = #cir.int<0> : !s32i
// CIR-POST: cir.global constant external  target_address_space(4) @j = #cir.int<0> : !s32i
// CIR-LLVM-DAG: @j = addrspace(4) externally_initialized constant i32 0, align 4
// OGCG-DAG: @j = addrspace(4) externally_initialized constant i32 0, align 4
__constant__ int j;

// CIR-PRE: cir.global "private" internal dso_local  lang_address_space(offload_local) @k = #cir.poison : !s32i
// CIR-POST: cir.global "private" internal dso_local  target_address_space(3) @k = #cir.poison : !s32i
// CIR-LLVM-DAG: @k = internal addrspace(3) global i32 poison, align 4
// OGCG-DAG: @k = addrspace(3) global i32 undef, align 4
__shared__ int k;

// CIR-PRE: cir.global "private" internal dso_local  lang_address_space(offload_local) @b = #cir.poison : !cir.float
// CIR-POST: cir.global "private" internal dso_local  target_address_space(3) @b = #cir.poison : !cir.float
// CIR-LLVM-DAG: @b = internal addrspace(3) global float poison, align 4
// OGCG-DAG: @b = addrspace(3) global float undef, align 4
__shared__ float b;

__device__ void foo() {
  // CIR-PRE: cir.get_global @i : !cir.ptr<!s32i, lang_address_space(offload_global)>
  // CIR-POST: cir.get_global @i : !cir.ptr<!s32i, target_address_space(1)>
  // CIR-LLVM: load i32, ptr addrspace(1) @i
  // OGCG: load i32, ptr addrspacecast (ptr addrspace(1) @i to ptr)
  i++;

  // CIR-PRE: cir.get_global @j : !cir.ptr<!s32i, lang_address_space(offload_constant)>
  // CIR-POST: cir.get_global @j : !cir.ptr<!s32i, target_address_space(4)>
  // CIR-LLVM: load i32, ptr addrspace(4) @j
  // OGCG: load i32, ptr addrspacecast (ptr addrspace(4) @j to ptr)
  j++;

  // CIR-PRE: cir.get_global @k : !cir.ptr<!s32i, lang_address_space(offload_local)>
  // CIR-POST: cir.get_global @k : !cir.ptr<!s32i, target_address_space(3)>
  // CIR-LLVM: load i32, ptr addrspace(3) @k
  // OGCG: load i32, ptr addrspacecast (ptr addrspace(3) @k to ptr)
  k++;
}
