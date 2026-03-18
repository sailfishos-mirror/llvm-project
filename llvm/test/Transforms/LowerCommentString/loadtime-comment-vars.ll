; RUN: opt -passes=lower-comment-string -S < %s | FileCheck %s

target triple = "powerpc64-ibm-aix"

@sccsid = internal global ptr @.str, align 8, !copyright.variable !0
@.str = private unnamed_addr constant [24 x i8] c"@(#) sccsid Version 1.0\00", align 1
@version = internal global [22 x i8] c"Copyright Version 2.0\00", align 1, !copyright.variable !1

; CHECK: define void @foo() !implicit.ref ![[REF1:[0-9]+]] !implicit.ref ![[REF2:[0-9]+]] {
define void @foo() {
entry:
  ret void
}

; CHECK: define void @bar() !implicit.ref ![[REF1]] !implicit.ref ![[REF2]] {
define void @bar() {
entry:
  ret void
}

!0 = !{!"sccsid"}
!1 = !{!"version"}

; Verify that the generated implicit.ref metadata nodes point to the correct global variables.
; CHECK: ![[REF1]] = !{ptr @sccsid}
; CHECK: ![[REF2]] = !{ptr @version}