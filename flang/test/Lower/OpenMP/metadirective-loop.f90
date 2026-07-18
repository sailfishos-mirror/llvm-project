! Test lowering of metadirectives with ordinary loop-associated variants.

! RUN: %flang_fc1 -fopenmp -emit-hlfir -fopenmp-version=52 %s -o - | FileCheck %s

! CHECK: #loop_unroll = #llvm.loop_unroll<disable = false, count = 4 : i64>
! CHECK: #loop_annotation = #llvm.loop_annotation<unroll = #loop_unroll>

! CHECK-LABEL: func.func @_QPtest_do(
! CHECK-NOT:     omp.parallel
! CHECK:         omp.wsloop
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:             omp.yield
! CHECK:         return
subroutine test_do(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_simd(
! CHECK-NOT:     omp.wsloop
! CHECK:         omp.simd linear(
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:             omp.yield
! CHECK-NOT:     fir.do_loop
! CHECK:         fir.load
! CHECK:         hlfir.assign
! CHECK:         return
subroutine test_simd(n, a, after)
  integer :: n, a(n), after, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: simd) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  after = i
end subroutine

! CHECK-LABEL: func.func @_QPtest_do_simd(
! CHECK-NOT:     omp.parallel
! CHECK:         omp.wsloop
! CHECK:           omp.simd
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:               omp.yield
! CHECK:         return
subroutine test_do_simd(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do simd) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_begin_do(
! CHECK-NOT:     omp.parallel
! CHECK:         omp.wsloop
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:             omp.yield
! CHECK:         return
subroutine test_begin_do(n, a)
  integer :: n, a(n), i
  !$omp begin metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  !$omp end metadirective
end subroutine

! Evaluations after the associated loop are lowered once after selection.
! CHECK-LABEL: func.func @_QPtest_begin_do_continuation(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK-NOT:       arith.constant 99
! CHECK:         } else {
! CHECK:           fir.do_loop
! CHECK-NOT:       arith.constant 99
! CHECK:         }
! CHECK:         %[[C99:.*]] = arith.constant 99 : i32
! CHECK:         hlfir.assign %[[C99]]
! CHECK-NOT:     arith.constant 99
! CHECK:         return
subroutine test_begin_do_continuation(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  !$omp begin metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  a(1) = 99
  !$omp end metadirective
end subroutine

! The continuation must remain available when the PFT is reused for ENTRY.
! CHECK-LABEL: func.func @_QPtest_entry_continuation(
! CHECK:         fir.if
! CHECK:         %[[ENTRY_C99:.*]] = arith.constant 99 : i32
! CHECK:         hlfir.assign %[[ENTRY_C99]]
! CHECK:         return
! CHECK-LABEL: func.func @_QPtest_alt_entry_continuation(
! CHECK:         fir.if
! CHECK:         %[[ALT_ENTRY_C99:.*]] = arith.constant 99 : i32
! CHECK:         hlfir.assign %[[ALT_ENTRY_C99]]
! CHECK:         return
subroutine test_entry_continuation(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  entry test_alt_entry_continuation(flag, n, a)
  !$omp begin metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  a(1) = 99
  !$omp end metadirective
end subroutine

! Standalone association must also remain available when the PFT is reused.
! CHECK-LABEL: func.func @_QPtest_standalone_entry_no_directive(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:         } else {
! CHECK:           fir.do_loop
! CHECK:         }
! CHECK-NOT:     fir.do_loop
! CHECK:         return
! CHECK-LABEL: func.func @_QPtest_alt_standalone_entry_no_directive(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:         } else {
! CHECK:           fir.do_loop
! CHECK:         }
! CHECK-NOT:     fir.do_loop
! CHECK:         return
! CHECK-LABEL: func.func @_QPtest_after_standalone_entry_no_directive(
! CHECK-NOT:     fir.if
! CHECK-NOT:     omp.
! CHECK-NOT:     fir.do_loop
! CHECK:         %[[AFTER_ENTRY_C77:.*]] = arith.constant 77 : i32
! CHECK:         hlfir.assign %[[AFTER_ENTRY_C77]]
! CHECK:         return
subroutine test_standalone_entry_no_directive(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  entry test_alt_standalone_entry_no_directive(flag, n, a)
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  entry test_after_standalone_entry_no_directive(n, a)
  a(1) = 77
end subroutine

! Intervening compiler directives have the same ownership across ENTRY.
! CHECK-LABEL: func.func @_QPtest_standalone_entry(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:         } else {
! CHECK:           fir.do_loop {{.*}} attributes {loopAnnotation = #loop_annotation}
! CHECK:         }
! CHECK-NOT:     fir.do_loop
! CHECK:         return
! CHECK-LABEL: func.func @_QPtest_alt_standalone_entry(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:         } else {
! CHECK:           fir.do_loop {{.*}} attributes {loopAnnotation = #loop_annotation}
! CHECK:         }
! CHECK-NOT:     fir.do_loop
! CHECK:         return
subroutine test_standalone_entry(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  entry test_alt_standalone_entry(flag, n, a)
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  !dir$ unroll 4
  do i = 1, n
    a(i) = i
  end do
end subroutine

! A statically inapplicable loop variant must leave the following loop for the
! selected standalone variant's continuation.
! CHECK-LABEL: func.func @_QPtest_static_standalone_fallback(
! CHECK-NOT:     omp.wsloop
! CHECK-NOT:     omp.loop_nest
! CHECK:         omp.barrier
! CHECK:         fir.do_loop
! CHECK:           hlfir.assign
! CHECK-NOT:     fir.do_loop
! CHECK:         return
subroutine test_static_standalone_fallback(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor("unknown")}: do) &
  !$omp & otherwise(barrier)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_dynamic_loop(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:         } else {
! CHECK:           omp.simd
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_loop(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(simd)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! A runtime standalone fallback must receive a sequential copy of the loop.
! CHECK-LABEL: func.func @_QPtest_dynamic_standalone_fallback(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:         } else {
! CHECK:           omp.barrier
! CHECK:           fir.do_loop
! CHECK:             hlfir.assign
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_standalone_fallback(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(barrier)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! An explicit NOTHING fallback takes the same associated loop through normal
! sequential lowering.
! CHECK-LABEL: func.func @_QPtest_dynamic_nothing_fallback(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:         } else {
! CHECK-NOT:       omp.
! CHECK:           fir.do_loop
! CHECK:             hlfir.assign
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_nothing_fallback(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! Compiler directives preceding the associated loop are processed before it.
! CHECK-LABEL: func.func @_QPtest_dynamic_unroll_fallback(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.wsloop
! CHECK:         } else {
! CHECK:           fir.do_loop {{.*}} attributes {loopAnnotation = #loop_annotation}
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_unroll_fallback(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  !dir$ unroll 4
  do i = 1, n
    a(i) = i
  end do
end subroutine

! Each runtime arm must compute its own affected depth and restore temporary
! loop-index attributes before lowering the next arm.
! CHECK-LABEL: func.func @_QPtest_dynamic_collapse(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.simd {{.*}}private({{.*}}Ei_private_i32{{.*}}Ej_private_i32
! CHECK:             omp.loop_nest ({{.*}}, {{.*}}) : i32 {{.*}} collapse(2)
! CHECK:               hlfir.assign
! CHECK:         } else {
! CHECK:           omp.simd linear(
! CHECK:             omp.loop_nest ({{.*}}) : i32
! CHECK:               fir.do_loop
! CHECK:                 hlfir.assign
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_collapse(flag, n, a)
  logical, intent(in) :: flag
  integer :: n, a(n, n), i, j
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: simd collapse(2)) &
  !$omp & otherwise(simd)
  do i = 1, n
    do j = 1, n
      a(j, i) = i + j
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_ordered_depth(
! CHECK-NOT:     omp.parallel
! CHECK:         omp.wsloop {{.*}}private({{.*}}Ei_private_i32{{.*}}Ej_private_i32
! CHECK:           omp.loop_nest ({{.*}}) : i32
! CHECK:             fir.do_loop
! CHECK:               hlfir.assign
! CHECK:         return
subroutine test_ordered_depth(n, a)
  integer :: n, a(n, n), i, j
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do ordered(2)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do j = 1, n
      a(j, i) = i + j
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_schedule(
! CHECK:         omp.wsloop schedule(static)
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:         return
subroutine test_schedule(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do schedule(static)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_collapse(
! CHECK:         omp.wsloop
! CHECK:           omp.loop_nest ({{.*}}, {{.*}}) : i32 {{.*}} collapse(2)
! CHECK:             hlfir.assign
! CHECK:         return
subroutine test_collapse(n, a)
  integer :: n, a(n, n), i, j
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do collapse(2)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do j = 1, n
      a(j, i) = i + j
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_safelen(
! CHECK:         omp.simd {{.*}}safelen(4)
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:         return
subroutine test_safelen(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: simd safelen(4)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! SIMD collapse makes every affected index lastprivate in OpenMP 5.2. Check
! that lowering copies both private values back to their source bindings.
! CHECK-LABEL: func.func @_QPtest_simd_collapse_lastprivate(
! CHECK:         %[[I:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_simd_collapse_lastprivateEi"}
! CHECK:         %[[J:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_simd_collapse_lastprivateEj"}
! CHECK:         omp.simd {{.*}}private({{.*}}Ei_private_i32{{.*}}Ej_private_i32
! CHECK:           omp.loop_nest ({{.*}}, {{.*}}) : i32 {{.*}} collapse(2)
! CHECK:             fir.if
! CHECK:               hlfir.assign {{.*}} to %[[I]]#0
! CHECK:               hlfir.assign {{.*}} to %[[J]]#0
! CHECK:             omp.yield
! CHECK:         return
subroutine test_simd_collapse_lastprivate(n, a)
  integer :: n, a(n, n), i, j
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: simd collapse(2)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do j = 1, n
      a(j, i) = i + j
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_block_nested_do(
! CHECK-NOT:     omp.parallel
! CHECK:         omp.wsloop {{.*}}private({{.*}}Ei_private_i32
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:         return
subroutine test_block_nested_do(n, a)
  integer :: n, a(n), i
  block
    !$omp metadirective &
    !$omp & when(implementation={vendor(llvm)}: do) &
    !$omp & otherwise(nothing)
    do i = 1, n
      a(i) = i
    end do
  end block
end subroutine
