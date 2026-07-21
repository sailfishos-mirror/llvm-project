! Test lowering of metadirectives with ordinary loop-associated variants.

! RUN: %flang_fc1 -fopenmp -emit-hlfir -fopenmp-version=52 %s -o - | FileCheck %s
! RUN: %flang_fc1 -fopenmp -emit-hlfir -fopenmp-version=52 %s -o - | FileCheck %s --check-prefix=COPY

! Check copy-in direction independently of function order: privatizer
! declarations are emitted at module scope in reverse source order.
! COPY-LABEL: omp.private {type = firstprivate} @_QFtest_firstprivate_allocatableEx_firstprivate_box_heap_i32
! COPY:       } copy {
! COPY-NEXT:  ^bb0(%[[ALLOC_HOST:.*]]: !fir.ref<!fir.box<!fir.heap<i32>>>, %[[ALLOC_PRIV:.*]]: !fir.ref<!fir.box<!fir.heap<i32>>>):
! COPY-NEXT:    %[[ALLOC_BOX:.*]] = fir.load %[[ALLOC_HOST]] : !fir.ref<!fir.box<!fir.heap<i32>>>
! COPY-NEXT:    %[[ALLOC_ADDR:.*]] = fir.box_addr %[[ALLOC_BOX]] : (!fir.box<!fir.heap<i32>>) -> !fir.heap<i32>
! COPY-NEXT:    %[[ALLOC_VALUE:.*]] = fir.load %[[ALLOC_ADDR]] : !fir.heap<i32>
! COPY-NEXT:    hlfir.assign %[[ALLOC_VALUE]] to %[[ALLOC_PRIV]] realloc

! COPY-LABEL: omp.private {type = firstprivate} @_QFtest_parallel_do_default_firstprivateEx_firstprivate_i32
! COPY-NEXT:  ^bb0(%[[DEFAULT_HOST:.*]]: !fir.ref<i32>, %[[DEFAULT_PRIV:.*]]: !fir.ref<i32>):
! COPY-NEXT:    %[[DEFAULT_VALUE:.*]] = fir.load %[[DEFAULT_HOST]] : !fir.ref<i32>
! COPY-NEXT:    hlfir.assign %[[DEFAULT_VALUE]] to %[[DEFAULT_PRIV]]

! COPY-LABEL: omp.private {type = firstprivate} @_QFtest_taskloop_implicit_firstprivateEa_firstprivate_box_Uxi32
! COPY:       } copy {
! COPY-NEXT:  ^bb0(%[[ARRAY_HOST:.*]]: !fir.ref<!fir.box<!fir.array<?xi32>>>, %[[ARRAY_PRIV:.*]]: !fir.ref<!fir.box<!fir.array<?xi32>>>):
! COPY-NEXT:    %[[ARRAY_VALUE:.*]] = fir.load %[[ARRAY_HOST]] : !fir.ref<!fir.box<!fir.array<?xi32>>>
! COPY-NEXT:    hlfir.assign %[[ARRAY_VALUE]] to %[[ARRAY_PRIV]]

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

! The following loop must remain available when the PFT is reused for ENTRY.
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

! A statically inapplicable loop variant leaves the following loop after the
! selected standalone variant, so it is lowered sequentially.
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

! A statically inapplicable loop variant nested in a parallel region leaves the
! following loop sequential.
! CHECK-LABEL: func.func @_QPtest_inapplicable_do_in_parallel(
! CHECK:         omp.parallel
! CHECK-NOT:       omp.wsloop
! CHECK-NOT:       omp.loop_nest
! CHECK:           fir.do_loop
! CHECK:             hlfir.assign
! CHECK-NOT:       fir.do_loop
! CHECK:           omp.terminator
! CHECK:         return
subroutine test_inapplicable_do_in_parallel(n, a, after)
  integer :: n, a(n), after, i
  !$omp parallel num_threads(1) shared(n, a, after)
  !$omp metadirective &
  !$omp & when(implementation={vendor("unknown")}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  after = i
  !$omp end parallel
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

! When the standalone fallback is selected at runtime, the following loop is
! lowered sequentially in that arm.
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

! When NOTHING is selected, the following loop is lowered normally.
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

! Data sharing from an ordinary enclosing construct remains available to the
! selected worksharing loop.
! CHECK-LABEL: func.func @_QPtest_do_in_parallel(
! CHECK:         omp.parallel
! CHECK:           omp.wsloop {{.*}}private({{.*}}Ei_private_i32
! CHECK:             omp.loop_nest
! CHECK:               hlfir.assign
! CHECK:           omp.terminator
! CHECK:         return
subroutine test_do_in_parallel(n, a)
  integer :: n, a(n), i
  !$omp parallel shared(n, a)
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  !$omp end parallel
end subroutine

!===----------------------------------------------------------------------===!
! Selected variants that create data environments
!===----------------------------------------------------------------------===!

! CHECK-LABEL: func.func @_QPtest_parallel_do()
! CHECK:         omp.parallel {
! CHECK:           omp.wsloop
! CHECK:             omp.loop_nest
! CHECK:             omp.yield
! CHECK:           omp.terminator
! CHECK:         return
subroutine test_parallel_do()
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, 100
  end do
end subroutine

!===----------------------------------------------------------------------===!
! Explicit clauses on the selected variant
!===----------------------------------------------------------------------===!

! CHECK-LABEL: func.func @_QPtest_reduction()
! CHECK:         omp.wsloop {{.*}} reduction(@add_reduction_i32
! CHECK:           omp.loop_nest
! CHECK:         return
subroutine test_reduction()
  integer :: i, s
  s = 0
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do reduction(+:s)) &
  !$omp & otherwise(nothing)
  do i = 1, 100
    s = s + i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_private()
! CHECK:         omp.wsloop private(
! CHECK:           omp.loop_nest
! CHECK:         return
subroutine test_private()
  integer :: i, x
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do private(x)) &
  !$omp & otherwise(nothing)
  do i = 1, 100
    x = i
  end do
end subroutine
! CHECK-LABEL: func.func @_QPtest_num_threads()
! CHECK:         omp.parallel num_threads({{.*}}) {
! CHECK:           omp.wsloop
! CHECK:             omp.loop_nest
! CHECK:         return
subroutine test_num_threads()
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do num_threads(4)) &
  !$omp & otherwise(nothing)
  do i = 1, 100
  end do
end subroutine

!===----------------------------------------------------------------------===!
! Metadirective nested inside a BLOCK construct
!===----------------------------------------------------------------------===!

! CHECK-LABEL: func.func @_QPtest_block_nested_parallel_do(
! CHECK:         omp.parallel {
! CHECK:           omp.wsloop private(@_QFtest_block_nested_parallel_doEi_private_i32 {{.*}}) {
! CHECK:             omp.loop_nest
subroutine test_block_nested_parallel_do(n, a)
  integer :: n
  integer :: a(n)
  integer :: i
  block
    !$omp metadirective &
    !$omp & when(implementation={vendor(llvm)}: parallel do) &
    !$omp & otherwise(nothing)
    do i = 1, n
      a(i) = i
    end do
  end block
end subroutine

!===----------------------------------------------------------------------===!
! Inner sequential loop induction-variable privatization
!===----------------------------------------------------------------------===!

! CHECK-LABEL: func.func @_QPtest_parallel_do_seq_inner(
! CHECK:         omp.parallel {
! CHECK:           omp.wsloop private({{.*}}_QFtest_parallel_do_seq_innerEi_private_i32{{.*}}_QFtest_parallel_do_seq_innerEk_private_i32{{.*}}) {
! CHECK:             omp.loop_nest
subroutine test_parallel_do_seq_inner(n, a)
  integer :: n
  integer :: a(n, n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do k = 1, n
      a(k, i) = k
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_do_seq_inner_shared(
! CHECK:         omp.wsloop private(
! CHECK-NOT:       _QFtest_do_seq_inner_sharedEk_private
! CHECK:           omp.loop_nest
subroutine test_do_seq_inner_shared(n, a)
  integer :: n
  integer :: a(n, n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do k = 1, n
      a(k, i) = k
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_critical_inner(
! CHECK:         omp.parallel {
! CHECK:           omp.wsloop private({{.*}}_QFtest_parallel_do_critical_innerEi_private_i32{{.*}}_QFtest_parallel_do_critical_innerEk_private_i32{{.*}}) {
! CHECK:             omp.loop_nest
! CHECK:               omp.critical
subroutine test_parallel_do_critical_inner(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp critical
    do k = 1, n
      a(k) = k
    end do
    !$omp end critical
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_inner_shared(
! CHECK:         omp.wsloop private(
! CHECK:           omp.loop_nest
! CHECK:             omp.parallel {
! CHECK-NOT:           _QFtest_parallel_do_inner_sharedEk_private
subroutine test_parallel_do_inner_shared(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp parallel shared(k)
    do k = 1, n
      a(k) = k
    end do
    !$omp end parallel
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_nested_metadirective_shared(
! CHECK-NOT:     _QFtest_parallel_do_nested_metadirective_sharedEk_private
! CHECK:         omp.wsloop private(@_QFtest_parallel_do_nested_metadirective_sharedEi_private_i32
! CHECK-NOT:       _QFtest_parallel_do_nested_metadirective_sharedEk_private
! CHECK:           omp.loop_nest
! CHECK:             omp.parallel {
! CHECK-NOT:           _QFtest_parallel_do_nested_metadirective_sharedEk_private
! CHECK:               fir.do_loop
subroutine test_parallel_do_nested_metadirective_shared(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp begin metadirective &
    !$omp & when(implementation={vendor(llvm)}: parallel shared(k)) &
    !$omp & otherwise(nothing)
    do k = 1, n
      a(k) = k
    end do
    !$omp end metadirective
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_reduction_iv(
! CHECK:         omp.wsloop private(@_QFtest_parallel_do_reduction_ivEi_private_i32 {{[^,]*}}) reduction(@add_reduction_i32 {{.*}}) {
subroutine test_parallel_do_reduction_iv(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do reduction(+:k)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do k = 1, n
      a(k) = k
    end do
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_enclosing_shared(
! CHECK:         omp.wsloop private({{.*}}_QFtest_parallel_do_enclosing_sharedEi_private_i32{{.*}}_QFtest_parallel_do_enclosing_sharedEk_private_i32{{.*}}) {
! CHECK:           omp.loop_nest
subroutine test_parallel_do_enclosing_shared(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp parallel shared(k)
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    do k = 1, n
      a(k) = k
    end do
  end do
  !$omp end parallel
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_target_inner(
! CHECK:         omp.wsloop private(@_QFtest_parallel_do_target_innerEi_private_i32 {{[^,]*}}) {
! CHECK:           omp.target
! CHECK-NOT:         _QFtest_parallel_do_target_innerEk_private_i32
subroutine test_parallel_do_target_inner(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, k
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp target map(tofrom: a)
    do k = 1, n
      a(k) = k
    end do
    !$omp end target
  end do
end subroutine

!===----------------------------------------------------------------------===!
! Data-sharing attributes of the selected variant
!===----------------------------------------------------------------------===!

! CHECK-LABEL: func.func @_QPtest_parallel_do_default_private(
! CHECK:         omp.parallel private(@_QFtest_parallel_do_default_privateEx_private_i32
! CHECK:           omp.wsloop private(@_QFtest_parallel_do_default_privateEi_private_i32
subroutine test_parallel_do_default_private(n, a)
  integer :: n
  integer :: a(n)
  integer :: i, x
  x = 0
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(private) shared(a, n)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    x = x + a(i)
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_default_private_block(
! CHECK:         omp.parallel {
! CHECK-NOT:       Elocal_private
! CHECK-NOT:       Esaved_private
! CHECK:           omp.wsloop
! CHECK-NOT:         Elocal_private
! CHECK-NOT:         Esaved_private
! CHECK:             fir.alloca i32 {bindc_name = "local"
subroutine test_parallel_do_default_private_block(n, a)
  integer :: n
  integer :: a(n)
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(private) shared(a, n)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    block
      integer :: local
      integer, save :: saved
      local = i
      saved = local
      a(i) = saved
    end block
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_default_private_task(
! CHECK:         omp.parallel private(@_QFtest_parallel_do_default_private_taskEx_private_i32
! CHECK:           omp.wsloop private(@_QFtest_parallel_do_default_private_taskEi_private_i32
! CHECK:             omp.task private(@_QFtest_parallel_do_default_private_taskEx_firstprivate_i32
! CHECK-SAME:          @_QFtest_parallel_do_default_private_taskEi_firstprivate_i32
subroutine test_parallel_do_default_private_task(n, a, x)
  integer :: n, x
  integer :: a(n)
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(private) shared(a, n)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp task
    x = i
    !$omp end task
    a(i) = x
  end do
end subroutine

! A PRIVATE clause on a nested task does not implicitly reference x in the
! selected parallel region, so default(private) must not create an outer copy.
! CHECK-LABEL: func.func @_QPtest_parallel_do_nested_task_private(
! CHECK:         omp.parallel {
! CHECK-NOT:       @_QFtest_parallel_do_nested_task_privateEx_private_i32
! CHECK:           omp.wsloop
! CHECK:             omp.task private(@_QFtest_parallel_do_nested_task_privateEx_private_i32
! CHECK:         return
subroutine test_parallel_do_nested_task_private(n, a, x)
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(private) shared(n, a)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    !$omp task private(x)
    x = i
    !$omp end task
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_firstprivate(
! CHECK:         omp.parallel {
! CHECK:           omp.wsloop private(@_QFtest_parallel_do_firstprivateEx_firstprivate_i32
subroutine test_parallel_do_firstprivate(n, a, x)
  integer :: n, x
  integer :: a(n)
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & firstprivate(x) shared(a, n)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = x
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_taskloop_implicit_firstprivate(
! CHECK:         omp.taskloop.context nogroup private(@_QFtest_taskloop_implicit_firstprivateEn_firstprivate_i32
! CHECK-SAME:      @_QFtest_taskloop_implicit_firstprivateEa_firstprivate_box_Uxi32
! CHECK-SAME:      @_QFtest_taskloop_implicit_firstprivateEi_private_i32
subroutine test_taskloop_implicit_firstprivate(n, a)
  integer :: n
  integer :: a(n)
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: taskloop nogroup) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = n
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_lastprivate_pointer(
! CHECK:         %[[P_ORIG:.*]]:2 = hlfir.declare {{.*}} {fortran_attrs = #fir.var_attrs<pointer>, uniq_name = "_QFtest_lastprivate_pointerEp"}
! CHECK:         omp.wsloop private(@_QFtest_lastprivate_pointerEp_private_box_ptr_i32
! CHECK:           fir.if
! CHECK:             %[[P_COPY:.*]] = fir.load %{{.*}} : !fir.ref<!fir.box<!fir.ptr<i32>>>
! CHECK:             fir.store %[[P_COPY]] to %[[P_ORIG]]#0 : !fir.ref<!fir.box<!fir.ptr<i32>>>
subroutine test_lastprivate_pointer(n, a, p)
  integer :: n
  integer, target :: a(n)
  integer, pointer :: p
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do lastprivate(p)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    p => a(i)
  end do
end subroutine

! Each dynamic arm must reconstruct and then restore its own DSA state. The
! parallel variant privatizes x; the worksharing fallback inherits the
! original x.
! CHECK-LABEL: func.func @_QPtest_dynamic_dsa_isolation(
! CHECK:         fir.if {{.*}} {
! CHECK:           omp.parallel {
! CHECK:             omp.wsloop private(@_QFtest_dynamic_dsa_isolationEx_private_i32
! CHECK:         } else {
! CHECK-NOT:       @_QFtest_dynamic_dsa_isolationEx_private_i32
! CHECK:           omp.wsloop private(@_QFtest_dynamic_dsa_isolationEi_private_i32
! CHECK-NOT:       @_QFtest_dynamic_dsa_isolationEx_private_i32
! CHECK:         }
! CHECK:         return
subroutine test_dynamic_dsa_isolation(flag, n, a, x)
  logical, intent(in) :: flag
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: parallel do private(x) shared(n, a)) &
  !$omp & otherwise(do)
  do i = 1, n
    x = i
    a(i) = x
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_default_firstprivate(
! CHECK:         omp.parallel private(@_QFtest_parallel_do_default_firstprivateEx_firstprivate_i32
! CHECK:           omp.wsloop
! CHECK:         return
subroutine test_parallel_do_default_firstprivate(n, a, x)
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(firstprivate) shared(n, a)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = x
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_parallel_do_default_shared(
! CHECK:         omp.parallel {
! CHECK-NOT:       @_QFtest_parallel_do_default_sharedEx_private_i32
! CHECK:           omp.wsloop private(@_QFtest_parallel_do_default_sharedEi_private_i32
! CHECK-NOT:         @_QFtest_parallel_do_default_sharedEx_private_i32
! CHECK:         return
subroutine test_parallel_do_default_shared(n, a, x)
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do default(shared)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    x = i
    a(i) = x
  end do
end subroutine

! DEFAULT(NONE) has already been validated by semantics. Lowering must retain
! the explicit shared bindings and only privatize the predetermined loop IV.
! CHECK-LABEL: func.func @_QPtest_parallel_do_default_none(
! CHECK:         omp.parallel {
! CHECK-NOT:       @_QFtest_parallel_do_default_noneEn_private
! CHECK-NOT:       @_QFtest_parallel_do_default_noneEa_private
! CHECK:           omp.wsloop private(@_QFtest_parallel_do_default_noneEi_private_i32
! CHECK:         return
subroutine test_parallel_do_default_none(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: parallel do &
  !$omp & default(none) shared(n, a)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_do_linear(
! CHECK:         %[[X:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_do_linearEx"}
! CHECK:         omp.wsloop linear(val(%[[X]]#0 : !fir.ref<i32> = %{{.*}} : i32))
! CHECK:           omp.loop_nest
! CHECK:         return
subroutine test_do_linear(n, a, x)
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do linear(x:2)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = x
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_firstprivate_allocatable(
! CHECK:         omp.wsloop private(@_QFtest_firstprivate_allocatableEx_firstprivate_box_heap_i32
! CHECK:           hlfir.declare {{.*}} {fortran_attrs = #fir.var_attrs<allocatable>, uniq_name = "_QFtest_firstprivate_allocatableEx"}
! CHECK:         return
subroutine test_firstprivate_allocatable(n, a, x)
  integer :: n, a(n), i
  integer, allocatable :: x
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do firstprivate(x)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = x
  end do
end subroutine

! CHECK-LABEL: func.func @_QPtest_lastprivate_allocatable(
! CHECK:         %[[A_ORIG:.*]]:2 = hlfir.declare {{.*}} {fortran_attrs = #fir.var_attrs<allocatable>, uniq_name = "_QFtest_lastprivate_allocatableEa"}
! CHECK:         omp.wsloop private(@_QFtest_lastprivate_allocatableEa_private_box_heap_i32
! CHECK:           fir.if
! CHECK:             %[[A_COPY:.*]] = fir.load %{{.*}} : !fir.ref<!fir.box<!fir.heap<i32>>>
! CHECK:             %[[A_VALUE:.*]] = fir.box_addr %[[A_COPY]] : (!fir.box<!fir.heap<i32>>) -> !fir.heap<i32>
! CHECK:             %[[A_LOAD:.*]] = fir.load %[[A_VALUE]] : !fir.heap<i32>
! CHECK:             hlfir.assign %[[A_LOAD]] to %[[A_ORIG]]#0 realloc
subroutine test_lastprivate_allocatable(n, a)
  integer :: n, i
  integer, allocatable :: a
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do lastprivate(a)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a = i
  end do
end subroutine
