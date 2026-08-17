! RUN: %python %S/../test_errors.py %s %flang -fopenacc -fopenmp -fopenmp-version=51
! RUN: %python %S/../test_errors.py %s %flang -fopenacc -fopenmp -fopenmp-version=52 -cpp -Ddefault=otherwise

! OpenACC declarative directives interrupt the association between a
! loop-associated OpenMP metadirective and a following DO construct, just as
! OpenMP declarative directives do.
subroutine no_loop_before_acc_declare(n, a)
  integer :: n, a(n), i
  integer, save :: x
  !$omp metadirective &
  !ERROR: This construct should contain a DO-loop or a loop-nest-generating construct
  !$omp& when(implementation={vendor(llvm)}: do) &
  !$omp& default(nothing)
  !$acc declare create(x)
  do i = 1, n
    a(i) = i
  end do
end subroutine

subroutine no_loop_before_acc_routine(n, a)
  integer :: n, a(n), i
  !$omp metadirective &
  !ERROR: This construct should contain a DO-loop or a loop-nest-generating construct
  !$omp& when(implementation={vendor(llvm)}: do) &
  !$omp& default(nothing)
  !$acc routine seq
  do i = 1, n
    a(i) = i
  end do
end subroutine
