! TEAMS still uses eager privatization, which requires semantic
! host-association symbols that a selected metadirective variant does not
! have.

! RUN: %not_todo_cmd %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %s 2>&1 | FileCheck %s

! CHECK: not yet implemented: TEAMS construct in loop-associated METADIRECTIVE variant

subroutine test_teams_distribute(n, a, x)
  integer :: n, a(n), x, i
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: teams distribute &
  !$omp & default(private) shared(n, a)) &
  !$omp & otherwise(nothing)
  do i = 1, n
    x = i
    a(i) = x
  end do
end subroutine
