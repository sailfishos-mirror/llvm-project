; RUN: opt < %s -passes=asan -S | FileCheck %s
; REQUIRES: aarch64-registered-target

target triple = "aarch64-unknown-linux-gnu"

!llvm.module.flags = !{!0, !1, !2, !3}

!0 = !{i32 7, !"ptrauth-returns", i32 1}
!1 = !{i32 7, !"ptrauth-auth-traps", i32 1}
!2 = !{i32 7, !"ptrauth-indirect-gotos", i32 1}
!3 = !{i32 7, !"aarch64-jump-table-hardening", i32 1}

; CHECK: define internal void @asan.module_ctor() #[[#ATTR:]]
; CHECK: attributes #[[#ATTR]] = { nounwind "aarch64-jump-table-hardening" "ptrauth-auth-traps" "ptrauth-indirect-gotos" "ptrauth-returns" }
