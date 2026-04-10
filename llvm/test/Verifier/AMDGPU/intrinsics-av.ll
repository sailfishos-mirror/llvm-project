; RUN: not llvm-as -disable-output < %s 2>&1 | FileCheck %s

; CHECK: global load/store intrinsics require that the last argument is a metadata string
; CHECK-NEXT: call <4 x i32> @llvm.amdgcn.av.global.load.b128({{.*}})
define <4 x i32> @av_global_load_b128_bad_metadata(ptr addrspace(1) %addr) {
  %data = call <4 x i32> @llvm.amdgcn.av.global.load.b128(ptr addrspace(1) %addr, metadata i32 0)
  ret <4 x i32> %data
}

; CHECK: global load/store intrinsics require that the last argument is a metadata string
; CHECK-NEXT: call void @llvm.amdgcn.av.global.store.b128({{.*}})
define void @av_global_store_b128_bad_metadata(ptr addrspace(1) %addr, <4 x i32> %data) {
  call void @llvm.amdgcn.av.global.store.b128(ptr addrspace(1) %addr, <4 x i32> %data, metadata i32 0)
  ret void
}
