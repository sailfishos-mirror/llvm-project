; RUN: opt -passes="print<iv-users>" -disable-output %s 2>&1 | FileCheck %s

target datalayout = "n16"

define i16 @testing() {
; CHECK: IV Users for loop %loop with backedge-taken count 15:
; CHECK-NEXT:   %retval = {2,+,1}<nuw><nsw><%loop> (post-inc with loop %loop) in    ret i16 %retval
;
entry:
  br label %loop

loop:
  %iv = phi i16 [ 0, %entry ], [ %iv.next, %loop ]
  %iv.next = add i16 %iv, 1
  %cond = icmp ult i16 %iv.next, 16
  br i1 %cond, label %loop, label %exit

exit:
  %iv.exit = phi i16 [ %iv, %loop ]
  %retval = add i16 %iv.exit, 2
  ret i16 %retval
}
