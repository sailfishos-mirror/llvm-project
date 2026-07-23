! REQUIRES: openmp_runtime
! RUN: %flang_fc1 -emit-hlfir %openmp_flags -fopenmp-version=51 -o - %s 2>&1 | FileCheck %s --check-prefix=HLFIR
! RUN: %flang_fc1 -emit-llvm %openmp_flags -fopenmp-version=51 -o - %s 2>&1 | FileCheck %s --check-prefix=LLVM

subroutine allocator_omitted(x, y)
  integer :: x, y
  !$omp parallel private(x, y) allocate(x)
    x = 1
    y = 2
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_omitted
! HLFIR: %[[NULL_ALLOC:.*]] = arith.constant 0 : i32
! HLFIR: omp.parallel allocate(%[[NULL_ALLOC]] : i32 -> %[[X:.*]]#0 : !fir.ref<i32>)
! HLFIR-SAME: private({{.*}} %[[X]]#0 -> %[[X_PRIVATE:.*]], {{.*}} -> %[[Y_PRIVATE:.*]] : !fir.ref<i32>, !fir.ref<i32>) {
! HLFIR: } {allocate_private_indices = array<i64: 0>}

! LLVM-LABEL: define internal void @allocator_omitted_..omp_par
! LLVM: %[[X_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: %[[Y_ALLOCA:.*]] = alloca i32, align 4
! LLVM: store i32 1, ptr %[[X_ALLOC]], align 4
! LLVM: store i32 2, ptr %[[Y_ALLOCA]], align 4
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[X_ALLOC]], ptr null)

subroutine allocator_explicit(x)
  use omp_lib
  integer :: x

  !$omp parallel private(x) allocate(allocator(omp_null_allocator): x)
    x = 1
  !$omp end parallel

  !$omp parallel private(x) allocate(allocator(omp_default_mem_alloc): x)
    x = 2
  !$omp end parallel
end subroutine

! LLVM-LABEL: define internal void @allocator_explicit_..omp_par.1
! LLVM: %[[DEFAULT_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr inttoptr (i64 1 to ptr))
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[DEFAULT_ALLOC]], ptr inttoptr (i64 1 to ptr))
! LLVM-LABEL: define internal void @allocator_explicit_..omp_par
! LLVM: %[[NULL_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[NULL_ALLOC]], ptr null)

subroutine allocator_dynamic(x, allocator)
  use omp_lib, only : omp_allocator_handle_kind
  integer :: x
  integer(kind=omp_allocator_handle_kind), intent(in) :: allocator
  !$omp parallel firstprivate(x) allocate(allocator(allocator): x)
    x = x + 1
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_dynamic
! HLFIR: %[[ALLOCATOR:.*]] = fir.load
! HLFIR: omp.parallel allocate(%[[ALLOCATOR]] : i64 -> %[[X:.*]]#0 : !fir.ref<i32>)
! HLFIR-SAME: private({{.*}} %[[X]]#0 -> %[[X_PRIVATE:.*]] : !fir.ref<i32>) {
! HLFIR: } {allocate_private_indices = array<i64: 0>}

! LLVM-LABEL: define void @allocator_dynamic_(
! LLVM: %[[ALLOCATOR_VALUE:.*]] = load i64, ptr %{{.*}}, align 8
! LLVM-COUNT-1: %[[ALLOCATOR_PTR:.*]] = inttoptr i64 %[[ALLOCATOR_VALUE]] to ptr
! LLVM: store ptr %[[ALLOCATOR_PTR]], ptr
! LLVM-LABEL: define internal void @allocator_dynamic_..omp_par
! LLVM: %[[CAPTURED_ALLOCATOR:.*]] = load ptr, ptr %{{.*}}, align 8
! LLVM: %[[DYNAMIC_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr %[[CAPTURED_ALLOCATOR]])
! LLVM: %[[ORIGINAL_VALUE:.*]] = load i32, ptr %{{.*}}, align 4
! LLVM: store i32 %[[ORIGINAL_VALUE]], ptr %[[DYNAMIC_ALLOC]], align 4
! LLVM: %[[PRIVATE_VALUE:.*]] = load i32, ptr %[[DYNAMIC_ALLOC]], align 4
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[DYNAMIC_ALLOC]], ptr %[[CAPTURED_ALLOCATOR]])

function allocator_value() result(allocator)
  use omp_lib, only : omp_allocator_handle_kind, omp_default_mem_alloc
  integer(kind=omp_allocator_handle_kind) :: allocator
  allocator = omp_default_mem_alloc
end function

subroutine allocator_expression(x, y)
  use omp_lib, only : omp_allocator_handle_kind
  integer :: x, y
  integer(kind=omp_allocator_handle_kind), external :: allocator_value
  !$omp parallel private(x, y) allocate(allocator(allocator_value()): x, y)
    x = 1
    y = 2
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_expression
! HLFIR-COUNT-1: fir.call @_QPallocator_value()
! HLFIR: omp.parallel allocate(%[[ALLOCATOR:.*]] : i64 -> %{{.*}}#0 : !fir.ref<i32>, %[[ALLOCATOR]] : i64 -> %{{.*}}#0 : !fir.ref<i32>)
! HLFIR: } {allocate_private_indices = array<i64: 0, 1>}

! LLVM-LABEL: define void @allocator_expression_(
! LLVM-COUNT-1: %[[ALLOCATOR_VALUE:.*]] = call i64 @allocator_value_()
! LLVM-COUNT-1: %[[ALLOCATOR_PTR:.*]] = inttoptr i64 %[[ALLOCATOR_VALUE]] to ptr
! LLVM: store ptr %[[ALLOCATOR_PTR]], ptr
! LLVM-LABEL: define internal void @allocator_expression_..omp_par
! LLVM: %[[CAPTURED_ALLOCATOR:.*]] = load ptr, ptr %{{.*}}, align 8
! LLVM: call ptr @__kmpc_alloc({{.*}}, i64 4, ptr %[[CAPTURED_ALLOCATOR]])
! LLVM: call ptr @__kmpc_alloc({{.*}}, i64 4, ptr %[[CAPTURED_ALLOCATOR]])

subroutine allocator_host_associated()
  integer :: x
contains
  subroutine inner()
    !$omp parallel private(x) allocate(x)
      x = 1
    !$omp end parallel
  end subroutine
end subroutine

! HLFIR-LABEL: func.func private @_QFallocator_host_associatedPinner
! HLFIR: omp.parallel allocate({{.*}} -> %[[X:.*]]#0 : !fir.ref<i32>)
! HLFIR-SAME: private({{.*}} %[[X]]#0 -> {{.*}} : !fir.ref<i32>) {
! HLFIR: } {allocate_private_indices = array<i64: 0>}

subroutine allocator_order(x, y, z)
  use omp_lib
  integer :: x, y, z
  !$omp parallel private(x, y, z) &
  !$omp& allocate(allocator(omp_high_bw_mem_alloc): y, x) &
  !$omp& allocate(allocator(omp_low_lat_mem_alloc): z)
    x = 1
    y = 2
    z = 3
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_order
! HLFIR: omp.parallel allocate(
! HLFIR-SAME: private(
! HLFIR: } {allocate_private_indices = array<i64: 1, 0, 2>}

! LLVM-LABEL: define internal void @allocator_order_..omp_par
! LLVM: %[[Y_ALLOC:.*]] = call ptr @__kmpc_alloc
! LLVM: %[[X_ALLOC:.*]] = call ptr @__kmpc_alloc
! LLVM: %[[Z_ALLOC:.*]] = call ptr @__kmpc_alloc
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[Z_ALLOC]],
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[X_ALLOC]],
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[Y_ALLOC]],

subroutine allocator_scalar_types(r, c, l, s)
  real :: r
  complex :: c
  logical :: l
  character(4) :: s
  !$omp parallel private(r, c, l, s) allocate(r, c, l, s)
    r = 1.0
    c = (1.0, 2.0)
    l = .true.
    s = "test"
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_scalar_types
! HLFIR: } {allocate_private_indices = array<i64: 0, 1, 2, 3>}

! LLVM-LABEL: define internal void @allocator_scalar_types_..omp_par
! LLVM: %[[REAL_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: %[[COMPLEX_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 8, ptr null)
! LLVM: %[[LOGICAL_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: %[[CHAR_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: store float 1.000000e+00, ptr %[[REAL_ALLOC]], align 4
! LLVM: store { float, float } {{.*}}, ptr %[[COMPLEX_ALLOC]], align 4
! LLVM: store i32 1, ptr %[[LOGICAL_ALLOC]], align 4
! LLVM: call void @llvm.memmove.p0.p0.i64(ptr %[[CHAR_ALLOC]],
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[CHAR_ALLOC]], ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[LOGICAL_ALLOC]], ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[COMPLEX_ALLOC]], ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[REAL_ALLOC]], ptr null)

subroutine allocator_alignment(x, y, z, w)
  use omp_lib
  integer :: x, y, z, w
  !$omp parallel private(x, y, z) firstprivate(w) &
  !$omp& allocate(x) &
  !$omp& allocate(align(64): y, z) &
  !$omp& allocate(allocator(omp_default_mem_alloc), align(128): w)
    x = 1
    y = 2
    z = 3
    w = w + 1
  !$omp end parallel
end subroutine

! HLFIR-LABEL: func.func @_QPallocator_alignment
! HLFIR: omp.parallel allocate(
! HLFIR-SAME: private(
! HLFIR: } {allocate_alignments = array<i64: 0, 64, 64, 128>, allocate_private_indices = array<i64: 0, 1, 2, 3>}

! LLVM-LABEL: define internal void @allocator_alignment_..omp_par
! LLVM: %[[X_ALLOC:.*]] = call ptr @__kmpc_alloc({{.*}}, i64 4, ptr null)
! LLVM: %[[Y_ALLOC:.*]] = call ptr @__kmpc_aligned_alloc({{.*}}, i64 64, i64 4, ptr null)
! LLVM: %[[Z_ALLOC:.*]] = call ptr @__kmpc_aligned_alloc({{.*}}, i64 64, i64 4, ptr null)
! LLVM: %[[W_ALLOC:.*]] = call ptr @__kmpc_aligned_alloc({{.*}}, i64 128, i64 4, ptr inttoptr (i64 1 to ptr))
! LLVM: store i32 1, ptr %[[X_ALLOC]], align 4
! LLVM: store i32 2, ptr %[[Y_ALLOC]], align 4
! LLVM: store i32 3, ptr %[[Z_ALLOC]], align 4
! LLVM: store i32 {{.*}}, ptr %[[W_ALLOC]], align 4
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[W_ALLOC]], ptr inttoptr (i64 1 to ptr))
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[Z_ALLOC]], ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[Y_ALLOC]], ptr null)
! LLVM: call void @__kmpc_free({{.*}}, ptr %[[X_ALLOC]], ptr null)
