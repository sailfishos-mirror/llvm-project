! RUN: %python %S/../test_errors.py %s %flang -fopenmp -fopenmp-version=51

! Nesting restrictions on selected replacements.

subroutine selected_do_in_do(n, a)
  integer :: n, a(n, n), i, j
  !$omp do
  do i = 1, n
    !ERROR: A worksharing region may not be closely nested inside a worksharing, explicit task, taskloop, critical, ordered, atomic, or master region
    !$omp metadirective when(implementation={vendor(llvm)}: do) default(nothing)
    do j = 1, n
      a(j, i) = i
    end do
  end do
end subroutine

subroutine selected_barrier_in_do_simd(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: do simd) &
  !$omp& default(nothing)
  do i = 1, n
    !ERROR: `BARRIER` region may not be closely nested inside of `WORKSHARING`, `LOOP`, `TASK`, `TASKLOOP`, `CRITICAL`, `ORDERED`, `ATOMIC` or `MASTER` region.
    !ERROR: The only OpenMP constructs that can be encountered during execution of a 'SIMD' region are the `ATOMIC` construct, the `LOOP` construct, the `SIMD` construct, the `SCAN` construct and the `ORDERED` construct with the `SIMD` clause.
    !$omp metadirective when(implementation={vendor(llvm)}: barrier) default(nothing)
    a(i) = i
  end do
end subroutine

! Nesting restrictions on constructs inside selected replacements.

subroutine construct_in_selected_simd(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: simd) default(nothing)
  do i = 1, n
    !ERROR: The only OpenMP constructs that can be encountered during execution of a 'SIMD' region are the `ATOMIC` construct, the `LOOP` construct, the `SIMD` construct, the `SCAN` construct and the `ORDERED` construct with the `SIMD` clause.
    !$omp barrier
    a(i) = i
  end do
end subroutine

subroutine construct_in_runtime_selected_do(flag, n, a)
  logical :: flag
  integer :: n, a(n), i
  !$omp metadirective when(user={condition(flag)}: do) default(nothing)
  do i = 1, n
    !ERROR: `BARRIER` region may not be closely nested inside of `WORKSHARING`, `LOOP`, `TASK`, `TASKLOOP`,`CRITICAL`, `ORDERED`, `ATOMIC` or `MASTER` region.
    !$omp barrier
    a(i) = i
  end do
end subroutine

subroutine master_in_selected_do(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: do) default(nothing)
  do i = 1, n
    !ERROR: `MASTER` region may not be closely nested inside of `WORKSHARING`, `LOOP`, `TASK`, `TASKLOOP`, or `ATOMIC` region.
    !$omp master
    a(i) = i
    !$omp end master
  end do
end subroutine

! SCAN nesting in both directions.

subroutine orphaned_selected_scan
  !ERROR: Orphaned SCAN directives are prohibited; perhaps you forgot to enclose the directive in to a WORKSHARING LOOP, a WORKSHARING LOOP SIMD or a SIMD directive.
  !$omp metadirective when(implementation={vendor(llvm)}: scan) default(nothing)
end subroutine

subroutine scan_in_selected_simd(n)
  integer :: n, i, x
  !$omp metadirective &
  !$omp& when(implementation={vendor(llvm)}: simd reduction(inscan, +: x)) &
  !$omp& default(nothing)
  do i = 1, n
    !$omp scan inclusive(x)
  end do
end subroutine

subroutine selected_scan_in_selected_simd(n)
  integer :: n, i, x
  !$omp metadirective &
  !$omp& when(implementation={vendor(llvm)}: simd reduction(inscan, +: x)) &
  !$omp& default(nothing)
  do i = 1, n
    !$omp metadirective &
    !$omp& when(implementation={vendor(llvm)}: scan inclusive(x)) &
    !$omp& default(nothing)
  end do
end subroutine

! Clause-sensitive ORDERED nesting in both directions.

subroutine ordered_in_selected_do(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: do ordered(1)) &
  !$omp& default(nothing)
  do i = 1, n
    !$omp ordered depend(sink: i - 1)
    a(i) = i
  end do
end subroutine

subroutine selected_ordered_in_do(n, a)
  integer :: n, a(n), i
  !$omp do ordered(1)
  do i = 1, n
    !$omp metadirective &
    !$omp& when(implementation={vendor(llvm)}: ordered depend(sink: i - 1)) &
    !$omp& default(nothing)
    a(i) = i
  end do
end subroutine

subroutine bad_selected_ordered_vector(n, a)
  integer :: n, a(n, n), i, j
  !$omp do ordered(2)
  do i = 1, n
    do j = 1, n
      !$omp metadirective &
      !ERROR: The number of variables in the SINK iteration vector does not match the parameter specified in ORDERED clause
      !$omp& when(implementation={vendor(llvm)}: ordered depend(sink: i - 1)) &
      !$omp& default(nothing)
      a(j, i) = i
    end do
  end do
end subroutine

subroutine ordered_block_in_selected_do(n, a)
  integer :: n, a(n), i
  !$omp begin metadirective &
  !$omp& when(implementation={vendor(llvm)}: do ordered) default(nothing)
  do i = 1, n
    !$omp ordered
      a(i) = i
    !$omp end ordered
  end do
  !$omp end metadirective
end subroutine

subroutine bad_ordered_block_in_selected_do(n, a)
  integer :: n, a(n), i
  !$omp begin metadirective &
  !$omp& when(implementation={vendor(llvm)}: do ordered(1)) default(nothing)
  do i = 1, n
    !ERROR: An ORDERED directive without the DEPEND clause must be closely nested in a worksharing-loop (or worksharing-loop SIMD) region with ORDERED clause without the parameter
    !$omp ordered
      a(i) = i
    !$omp end ordered
  end do
  !$omp end metadirective
end subroutine

! Cancellation nesting in both directions.

subroutine selected_cancel_in_do(n, a)
  integer :: n, a(n), i
  !$omp do
  do i = 1, n
    !$omp metadirective when(implementation={vendor(llvm)}: cancel do) &
    !$omp& default(nothing)
    a(i) = i
  end do
end subroutine

subroutine orphaned_selected_cancel
  !ERROR: CANCEL DO directive is not closely nested inside the construct that matches the DO clause type
  !$omp metadirective when(implementation={vendor(llvm)}: cancel do) &
  !$omp& default(nothing)
end subroutine

subroutine cancel_in_selected_do(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: do) default(nothing)
  do i = 1, n
    !$omp cancel do
    a(i) = i
  end do
end subroutine

subroutine cancel_in_selected_ordered_do(n, a)
  integer :: n, a(n), i
  !$omp metadirective when(implementation={vendor(llvm)}: do ordered) &
  !$omp& default(nothing)
  do i = 1, n
    !ERROR: The CANCEL construct cannot be nested inside of a worksharing construct with the ORDERED clause
    !$omp cancel do
    a(i) = i
  end do
end subroutine

subroutine selected_taskgroup_cancel_in_parallel
  !$omp parallel
    !$omp metadirective &
    !ERROR: With TASKGROUP clause, CANCEL construct must be closely nested inside TASK or TASKLOOP construct and CANCEL region must be closely nested inside TASKGROUP region
    !$omp& when(implementation={vendor(llvm)}: cancel taskgroup) &
    !$omp& default(nothing)
  !$omp end parallel
end subroutine

subroutine taskgroup_cancel_in_selected_task
  !$omp parallel
    !$omp begin metadirective &
    !$omp& when(implementation={vendor(llvm)}: task) default(nothing)
      !ERROR: With TASKGROUP clause, CANCEL construct must be closely nested inside TASK or TASKLOOP construct and CANCEL region must be closely nested inside TASKGROUP region
      !$omp cancel taskgroup
    !$omp end metadirective
  !$omp end parallel
end subroutine

! Unreachable replacements do not contribute nesting restrictions.

subroutine dead_invalid_replacement(n, a)
  integer :: n, a(n), i
  !$omp do
  do i = 1, n
    !$omp metadirective when(device={kind(nohost)}: barrier) default(nothing)
    a(i) = i
  end do
end subroutine
