; RUN: not llc -global-isel=0 -mtriple=amdgpu12.00-- -filetype=null %s 2>&1 | FileCheck %s
; RUN: not llc -global-isel=1 -mtriple=amdgpu12.00-- -filetype=null %s 2>&1 | FileCheck %s

; Accesses of the VGPR "as memory" address space (13) that are still not
; implemented must be rejected with a clean diagnostic on both SelectionDAG and
; GlobalISel, rather than failing with "cannot select" / "unable to legalize".
; Whole-dword and 8-/16-bit accesses are implemented; see as-vgpr-basic.ll and
; as-vgpr-bits.ll.

; A sub-dword load extended into a value wider than a dword.
; CHECK: error: {{.*}}unsupported access of VGPR 'as memory' address space (13); only whole-dword and 8-/16-bit loads and stores are implemented
define i64 @load_i8_zext_i64(ptr addrspace(13) inreg %p) {
  %x = load i8, ptr addrspace(13) %p
  %y = zext i8 %x to i64
  ret i64 %y
}

; A memory size that is neither a whole dword nor 8/16 bits.
; CHECK: error: {{.*}}unsupported access of VGPR 'as memory' address space (13); only whole-dword and 8-/16-bit loads and stores are implemented
define i1 @load_i1(ptr addrspace(13) inreg %p) {
  %x = load i1, ptr addrspace(13) %p
  ret i1 %x
}

; CHECK: error: {{.*}}unsupported access of VGPR 'as memory' address space (13); only whole-dword and 8-/16-bit loads and stores are implemented
define void @store_i1(ptr addrspace(13) inreg %p, i1 %v) {
  store i1 %v, ptr addrspace(13) %p
  ret void
}

; A whole-dword size with no corresponding V_LOAD_IDX/V_STORE_IDX pseudo.
; CHECK: error: {{.*}}unsupported access of VGPR 'as memory' address space (13); only whole-dword and 8-/16-bit loads and stores are implemented
define <14 x i32> @load_v14i32(ptr addrspace(13) inreg %p) {
  %x = load <14 x i32>, ptr addrspace(13) %p
  ret <14 x i32> %x
}
