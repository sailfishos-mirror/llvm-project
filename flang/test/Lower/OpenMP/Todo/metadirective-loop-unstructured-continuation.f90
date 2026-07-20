! A begin/end continuation with unstructured control flow owns function-region
! PFT blocks that cannot be detached and lowered after variant selection.

! RUN: %not_todo_cmd %flang_fc1 -emit-hlfir -fopenmp -fopenmp-version=52 -o - %s 2>&1 | FileCheck %s

! CHECK: not yet implemented: unstructured continuation in loop-associated METADIRECTIVE

subroutine test_continuation(n, a, selector)
  integer :: n, a(n), selector, i
  !$omp begin metadirective &
  !$omp & when(implementation={vendor(llvm)}: do) &
  !$omp & otherwise(nothing)
  do i = 1, n
    a(i) = i
  end do
  go to (10, 20), selector
10 a(1) = 1
  go to 30
20 a(1) = 2
30 continue
  !$omp end metadirective
end subroutine
