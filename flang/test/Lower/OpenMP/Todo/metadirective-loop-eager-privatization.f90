! RUN: not %flang_fc1 -fopenmp -emit-hlfir -fopenmp-version=51 -mmlir --enable-delayed-privatization=false %s -o - 2>&1 | FileCheck %s

! CHECK: not yet implemented: loop-associated METADIRECTIVE without delayed privatization
subroutine test_metadirective_loop_eager_privatization(n, a)
  integer :: n
  integer :: a(n)
  integer :: i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & default(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine
