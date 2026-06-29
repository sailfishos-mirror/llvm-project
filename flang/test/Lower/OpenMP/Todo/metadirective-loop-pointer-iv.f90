! POINTER loop iteration variables require construct-scoped name resolution.

! RUN: %not_todo_cmd %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %s 2>&1 | FileCheck %s

! CHECK: not yet implemented: POINTER or ALLOCATABLE loop iteration variable in loop-associated METADIRECTIVE variant

subroutine test_pointer_iv(n, a)
  integer :: n, a(n)
  integer, target :: target
  integer, pointer :: i
  i => target
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
end subroutine
