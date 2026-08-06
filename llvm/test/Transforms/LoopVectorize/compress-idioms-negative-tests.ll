; RUN: opt < %s -lv-monotonic-patterns=true -force-target-supports-masked-memory-ops -force-vector-width=4 -passes=loop-vectorize -disable-output -pass-remarks-analysis=".*" 2>&1 | FileCheck %s

; CHECK: loop not vectorized: cannot identify array bounds

; Negative test: Conditional pointer (rather than index) increments are not supported yet (needs LAA support).
define void @test_compress_store_with_pointer(ptr writeonly noalias %init.dst, ptr readonly %src, i32 %c, i64 %n) {
entry:
  br label %for.body

for.body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %for.inc ]
  %dst = phi ptr [ %init.dst, %entry ], [ %dst.1, %for.inc ]
  %src.ptr = getelementptr inbounds i32, ptr %src, i64 %iv
  %load.src = load i32, ptr %src.ptr, align 4
  %cmp = icmp slt i32 %load.src, %c
  br i1 %cmp, label %if.then, label %for.inc

if.then:
  %dst.inc = getelementptr inbounds i8, ptr %dst, i64 4
  store i32 %load.src, ptr %dst, align 4
  br label %for.inc

for.inc:
  %dst.1 = phi ptr [ %dst.inc, %if.then ], [ %dst, %for.body ]
  %iv.next = add nuw nsw i64 %iv, 1
  %exitcond.not = icmp eq i64 %iv.next, %n
  br i1 %exitcond.not, label %exit, label %for.body

exit:
  ret void
}

; CHECK: Recipe with invalid costs prevented vectorization at VF=(2, 4): phi

; Negative test: Storing the conditionally incremented phi is invalid (as all uses must be uniform).
define void @test_store_conditionally_incremented_value(ptr writeonly noalias %dst, ptr readonly %src, i32 %c, i64 %n) {
entry:
  br label %for.body

for.body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %for.inc ]
  %idx = phi i32 [ 0, %entry ], [ %idx.1, %for.inc ]
  %src.ptr = getelementptr inbounds i32, ptr %src, i64 %iv
  %load.src = load i32, ptr %src.ptr, align 4
  %cmp = icmp slt i32 %load.src, %c
  br i1 %cmp, label %if.then, label %for.inc

if.then:
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %iv
  store i32 %idx, ptr %dst.ptr, align 4
  %idx.next = add nsw i32 %idx, 1
  br label %for.inc

for.inc:
  %idx.1 = phi i32 [ %idx.next, %if.then ], [ %idx, %for.body ]
  %iv.next = add nuw nsw i64 %iv, 1
  %exitcond.not = icmp eq i64 %iv.next, %n
  br i1 %exitcond.not, label %exit, label %for.body

exit:
  ret void
}

; CHECK: loop not vectorized: value that could not be identified as reduction is used outside the loop

; Pre-increment is currently not matched as we require one use of the step instruction.
define i32 @test_pre_increment_compress_store(ptr writeonly noalias %dst, ptr readonly %src, i32 %c, i64 %n) {
entry:
  br label %for.body

for.body:
  %iv = phi i64 [ 0, %entry ], [ %iv.next, %for.inc ]
  %idx = phi i32 [ 0, %entry ], [ %idx.1, %for.inc ]
  %src.ptr = getelementptr inbounds i32, ptr %src, i64 %iv
  %load.src = load i32, ptr %src.ptr, align 4
  %cmp = icmp slt i32 %load.src, %c
  br i1 %cmp, label %if.then, label %for.inc

if.then:
  %idx.next = add nsw i32 %idx, 1
  %dst.idx = sext i32 %idx.next to i64
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %dst.idx
  store i32 %load.src, ptr %dst.ptr, align 4
  br label %for.inc

for.inc:
  %idx.1 = phi i32 [ %idx.next, %if.then ], [ %idx, %for.body ]
  %iv.next = add nuw nsw i64 %iv, 1
  %exitcond.not = icmp eq i64 %iv.next, %n
  br i1 %exitcond.not, label %exit, label %for.body

exit:
  ret i32 %idx.1
}
