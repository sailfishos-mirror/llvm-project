! Test lowering an OpenMP LOOP replacement. A standalone LOOP with THREAD
! binding is represented by the existing SIMD lowering path.

! RUN: %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %s | FileCheck %s

! CHECK-LABEL: func.func @_QPtest_loop(
! CHECK:         omp.simd private(@_QFtest_loopEi_private_i32
! CHECK:           omp.loop_nest
! CHECK:             hlfir.assign
! CHECK:         return

subroutine test_loop(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: loop bind(thread)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine
