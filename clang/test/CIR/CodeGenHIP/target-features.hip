#include "../CodeGenCUDA/Inputs/cuda.h"

// REQUIRES: amdgpu-registered-target

// RUN: %clang_cc1 -triple=amdgcn-amd-amdhsa -x hip -fclangir \
// RUN:            -fcuda-is-device -target-cpu gfx900 -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR %s --input-file=%t.cir

// RUN: %clang_cc1 -triple=amdgcn-amd-amdhsa -x hip -fclangir \
// RUN:            -fcuda-is-device -target-cpu gfx900 -emit-llvm %s -o %t.cir.ll
// RUN: FileCheck --check-prefix=LLVM %s --input-file=%t.cir.ll

// RUN: %clang_cc1 -triple=amdgcn-amd-amdhsa -x hip \
// RUN:            -fcuda-is-device -target-cpu gfx900 -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM %s --input-file=%t.ll

// AMDGPU with gfx900: target-cpu is set, target-features is absent.

// CIR: cir.func{{.*}} @_Z6kernelv()
// CIR-SAME: "cir.target-cpu" = "gfx900"
// CIR-NOT: cir.target-features
__global__ void kernel() {}

// CIR: cir.func{{.*}} @_Z9device_fnv()
// CIR-SAME: "cir.target-cpu" = "gfx900"
// CIR-NOT: cir.target-features
__device__ void device_fn() {}

// LLVM: define{{.*}} void @_Z6kernelv(){{.*}} #[[K_ATTR:[0-9]+]]
// LLVM: define{{.*}} void @_Z9device_fnv(){{.*}} #[[D_ATTR:[0-9]+]]
// LLVM-DAG: attributes #[[K_ATTR]] = {{.*}}"target-cpu"="gfx900"
// LLVM-DAG: attributes #[[D_ATTR]] = {{.*}}"target-cpu"="gfx900"
