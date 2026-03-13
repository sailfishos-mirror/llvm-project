// RUN: %clang_cc1 -triple nvptx64-nvidia-cuda -x cuda \
// RUN:   -fcuda-is-device -fclangir -emit-cir \
// RUN:   -mmlir -mlir-print-ir-before=cir-target-lowering %s -o %t.cir 2> %t-pre.cir
// RUN: FileCheck --check-prefix=CIR-PRE --input-file=%t-pre.cir %s

// Verifies CIR emits correct address spaces for CUDA globals.

#include "Inputs/cuda.h"

// CIR-PRE: cir.global external  lang_address_space(offload_global) @i = #cir.int<0> : !s32i
__device__ int i;

// CIR-PRE: cir.global constant external  lang_address_space(offload_constant) @j = #cir.int<0> : !s32i
__constant__ int j;

// CIR-PRE: cir.global external  lang_address_space(offload_local) @k = #cir.poison : !s32i
__shared__ int k;

// CIR-PRE: cir.global external  lang_address_space(offload_local) @b = #cir.poison : !cir.float
__shared__ float b;

__device__ void foo() {
  // CIR-PRE: cir.get_global @i : !cir.ptr<!s32i, lang_address_space(offload_global)>
  i++;

  // CIR-PRE: cir.get_global @j : !cir.ptr<!s32i, lang_address_space(offload_constant)>
  j++;

  // CIR-PRE: cir.get_global @k : !cir.ptr<!s32i, lang_address_space(offload_local)>
  k++;
}
