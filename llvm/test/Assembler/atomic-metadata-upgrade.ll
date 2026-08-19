; RUN: llvm-as < %s | llvm-dis | FileCheck %s

; The AMDGPU-specific !amdgpu.ignore.denormal.mode was generalized into
; !atomic.ignore.denormal.mode. Textual IR using the old name is upgraded.

define void @upgraded(ptr %p, float %v) {
; CHECK-LABEL: @upgraded(
; CHECK: atomicrmw fadd ptr %p, float %v seq_cst, align 4, !atomic.ignore.denormal.mode !{{[0-9]+}}
  %r = atomicrmw fadd ptr %p, float %v seq_cst, !amdgpu.ignore.denormal.mode !0
  ret void
}

; Only atomicrmw ever carried the metadata, so an attachment of the same name
; anywhere else is left exactly as it was rather than being renamed wholesale.

define float @not_an_atomicrmw(ptr %p) {
; CHECK-LABEL: @not_an_atomicrmw(
; CHECK: load float, ptr %p, align 4, !amdgpu.ignore.denormal.mode !{{[0-9]+}}
  %v = load float, ptr %p, !amdgpu.ignore.denormal.mode !0
  ret float %v
}

; An atomicrmw already using the new name is unaffected.

define void @already_upgraded(ptr %p, float %v) {
; CHECK-LABEL: @already_upgraded(
; CHECK: atomicrmw fadd ptr %p, float %v seq_cst, align 4, !atomic.ignore.denormal.mode !{{[0-9]+}}
  %r = atomicrmw fadd ptr %p, float %v seq_cst, !atomic.ignore.denormal.mode !0
  ret void
}

; The upgrade applies to every floating-point atomicrmw, not just fadd.

define void @upgraded_fmax(ptr %p, float %v) {
; CHECK-LABEL: @upgraded_fmax(
; CHECK: atomicrmw fmax ptr %p, float %v seq_cst, align 4, !atomic.ignore.denormal.mode !{{[0-9]+}}
  %r = atomicrmw fmax ptr %p, float %v seq_cst, !amdgpu.ignore.denormal.mode !0
  ret void
}

!0 = !{}
