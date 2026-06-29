! Loop-sequence association is handled separately from ordinary loop-nest
! association.

! RUN: %not_todo_cmd %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=60 -o - %s 2>&1 | FileCheck %s

! CHECK: not yet implemented: loop-sequence-associated METADIRECTIVE variant

subroutine test_fuse(n, a)
  integer :: n, a(n), i, j
  !$omp metadirective &
  !$omp & when(implementation={vendor(llvm)}: fuse) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  do j = 1, n
    a(j) = a(j) + j
  end do
end subroutine
