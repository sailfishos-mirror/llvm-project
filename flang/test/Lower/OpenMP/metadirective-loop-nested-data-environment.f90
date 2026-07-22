! Test variant-local loop induction variables when a metadirective is nested
! in an ordinary or selected OpenMP data environment.

! RUN: split-file %s %t
! RUN: %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %t/static.f90 | FileCheck %s --check-prefix=STATIC
! RUN: %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %t/dynamic.f90 | FileCheck %s --check-prefix=DYNAMIC
! RUN: %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %t/selected-metadirective.f90 | FileCheck %s --check-prefix=SELECTED

! STATIC-LABEL: func.func @_QPtest_static_do_in_parallel(
! STATIC:         %[[I:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_static_do_in_parallelEi"}
! STATIC:         omp.parallel
! STATIC:           omp.wsloop private(@_QFtest_static_do_in_parallelEi_private_i32 %[[I]]#0
! STATIC:             omp.loop_nest
! STATIC:           %[[AFTER:.*]] = fir.load %[[I]]#0 : !fir.ref<i32>
! STATIC:           hlfir.assign %[[AFTER]]

! DYNAMIC-LABEL: func.func @_QPtest_do_in_parallel(
! DYNAMIC:         %[[I:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_do_in_parallelEi"}
! DYNAMIC:         omp.parallel
! DYNAMIC:           fir.if
! DYNAMIC:             omp.wsloop private(@_QFtest_do_in_parallelEi_private_i32 %[[I]]#0
! DYNAMIC:               omp.loop_nest
! DYNAMIC:           } else {
! DYNAMIC-NOT:         omp.wsloop
! DYNAMIC:             fir.do_loop
! DYNAMIC:               fir.store {{.*}} to %[[I]]#0
! DYNAMIC:           %[[AFTER:.*]] = fir.load %[[I]]#0 : !fir.ref<i32>
! DYNAMIC:           hlfir.assign %[[AFTER]]

! SELECTED-LABEL: func.func @_QPtest_do_in_selected_parallel(
! SELECTED:         %[[I:.*]]:2 = hlfir.declare {{.*}} {uniq_name = "_QFtest_do_in_selected_parallelEi"}
! SELECTED:         omp.parallel
! SELECTED:           fir.if
! SELECTED:             omp.wsloop private(@_QFtest_do_in_selected_parallelEi_private_i32 %[[I]]#0
! SELECTED:               omp.loop_nest
! SELECTED:           } else {
! SELECTED-NOT:         omp.wsloop
! SELECTED:             fir.do_loop
! SELECTED:           %[[AFTER:.*]] = fir.load %[[I]]#0 : !fir.ref<i32>
! SELECTED:           hlfir.assign %[[AFTER]]

!--- static.f90
subroutine test_static_do_in_parallel(n, a, after)
  integer :: n, a(n), after, i
  i = 0
  !$omp parallel num_threads(1) shared(n, a, after, i)
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  after = i
  !$omp end parallel
end subroutine

!--- dynamic.f90
subroutine test_do_in_parallel(flag, n, a, after)
  logical, intent(in) :: flag
  integer :: n, a(n), after, i
  i = 0
  !$omp parallel num_threads(1) shared(flag, n, a, after, i)
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  after = i
  !$omp end parallel
end subroutine

!--- selected-metadirective.f90
subroutine test_do_in_selected_parallel(flag, n, a, after)
  logical, intent(in) :: flag
  integer :: n, a(n), after, i
  i = 0
  !$omp begin metadirective &
  !$omp & when(implementation={vendor(llvm)}: &
  !$omp &   parallel num_threads(1) shared(flag, n, a, after, i)) &
  !$omp & otherwise(nothing)
  !$omp metadirective &
  !$omp & when(user={condition(flag)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  after = i
  !$omp end metadirective
end subroutine
