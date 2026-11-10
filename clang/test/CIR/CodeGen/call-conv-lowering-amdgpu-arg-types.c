// REQUIRES: amdgpu-registered-target
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -fclangir -fclangir-call-conv-lowering -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -fclangir -fclangir-call-conv-lowering -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t-cir.ll %s
// RUN: %clang_cc1 -triple amdgpu-amd-amdhsa -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

// Checks that CallConvLowering classifies arguments of ordinary AMDGPU
// functions, which use the default convention rather than the kernel one.

// TODO(cir): Add the argument cases still NYI in CallConvLowering:
// - Structs with a flexible array member. The bridge does not set that
//   record flag yet.
// - Reading arguments with va_arg in a variadic definition.
// - _Complex wider than 64 bits. Classic splits it into its two parts, while
//   the bridge only splits records and passes it whole.
// - _BitInt wider than 128 bits, passed indirectly in addrspace(5). The bridge
//   cannot express an indirect address space yet.

typedef struct {} empty_t;
typedef struct { char c; } i8_wrapper_t;
typedef struct { int i; } i32_wrapper_t;
typedef struct { float f; } f32_wrapper_t;
typedef struct { int *p; } ptr_wrapper_t;
typedef struct { char a, b; } char2_t;
typedef struct { char a, b, c; } small_t;
typedef union { int i; float f; } int_float_t;
typedef union {
  int *ip;
  float *fp;
} __attribute__((transparent_union)) transparent_t;
typedef struct { int a, b; } pair_t;
typedef struct { int a, b, c; } triple_t;
typedef struct { int data[17]; } big_t;
typedef struct { int a : 3; int b : 5; } bits_t;
typedef short short4 __attribute__((ext_vector_type(4)));
typedef float float3 __attribute__((ext_vector_type(3)));

void arg_void(void) {}

// CIR: cir.func {{.*}}@arg_void()
// LLVM: define {{.*}}void @arg_void()

// Sub-word integers and bool carry signext or zeroext per their signedness.
void arg_bool(_Bool b) {}

// CIR: cir.func {{.*}}@arg_bool(%arg0: !cir.bool {{.*}}llvm.zeroext
// LLVM: define {{.*}}void @arg_bool(i1 noundef zeroext %{{.*}})

void arg_char(char c) {}

// CIR: cir.func {{.*}}@arg_char(%arg0: !s8i {{.*}}llvm.signext
// LLVM: define {{.*}}void @arg_char(i8 noundef signext %{{.*}})

void arg_uchar(unsigned char c) {}

// CIR: cir.func {{.*}}@arg_uchar(%arg0: !u8i {{.*}}llvm.zeroext
// LLVM: define {{.*}}void @arg_uchar(i8 noundef zeroext %{{.*}})

void arg_short(short s) {}

// CIR: cir.func {{.*}}@arg_short(%arg0: !s16i {{.*}}llvm.signext
// LLVM: define {{.*}}void @arg_short(i16 noundef signext %{{.*}})

// Word-sized and wider scalars pass through unchanged.
void arg_int(int i) {}

// CIR: cir.func {{.*}}@arg_int(%arg0: !s32i {{.*}})
// LLVM: define {{.*}}void @arg_int(i32 noundef %{{.*}})

void arg_long(long l) {}

// CIR: cir.func {{.*}}@arg_long(%arg0: !s64i {{.*}})
// LLVM: define {{.*}}void @arg_long(i64 noundef %{{.*}})

void arg_float(float f) {}

// CIR: cir.func {{.*}}@arg_float(%arg0: !cir.float {{.*}})
// LLVM: define {{.*}}void @arg_float(float noundef %{{.*}})

void arg_double(double d) {}

// CIR: cir.func {{.*}}@arg_double(%arg0: !cir.double {{.*}})
// LLVM: define {{.*}}void @arg_double(double noundef %{{.*}})

void arg_half(_Float16 h) {}

// CIR: cir.func {{.*}}@arg_half(%arg0: !cir.f16 {{.*}})
// LLVM: define {{.*}}void @arg_half(half noundef %{{.*}})

// Outside a HIP kernel a generic pointer stays generic.
void arg_ptr(int *p) {}

// CIR: cir.func {{.*}}@arg_ptr(%arg0: !cir.ptr<!s32i> {{.*}})
// LLVM: define {{.*}}void @arg_ptr(ptr noundef %{{.*}})

// A _BitInt wider than 64 bits and vectors, including 16-bit and 3-element
// ones, pass whole.
void arg_bitint65(_BitInt(65) i) {}

// CIR: cir.func {{.*}}@arg_bitint65(%arg0: !cir.int<s, 65, bitint> {{.*}})
// LLVM: define {{.*}}void @arg_bitint65(i65 noundef %{{.*}})

void arg_short4(short4 v) {}

// CIR: cir.func {{.*}}@arg_short4(%arg0: !cir.vector<4 x !s16i> {{.*}})
// LLVM: define {{.*}}void @arg_short4(<4 x i16> noundef %{{.*}})

void arg_float3(float3 v) {}

// CIR: cir.func {{.*}}@arg_float3(%arg0: !cir.vector<3 x !cir.float> {{.*}})
// LLVM: define {{.*}}void @arg_float3(<3 x float> noundef %{{.*}})

// An empty struct is dropped from the signature.
void arg_empty(empty_t e) {}

// CIR: cir.func {{.*}}@arg_empty()
// LLVM: define {{.*}}void @arg_empty()

// Single-element structs pass as their element. The callee rebuilds the
// struct in a private (addrspace 5) slot.
void arg_i8_wrapper(i8_wrapper_t w) {}

// CIR: cir.func {{.*}}@arg_i8_wrapper(%arg0: !s8i{{.*}})
// CIR:   cir.alloca "coerce" {{.*}} : !cir.ptr<{{.*}}, target_address_space(5)>
// LLVM: define {{.*}}void @arg_i8_wrapper(i8 %{{.*}})

void arg_i32_wrapper(i32_wrapper_t w) {}

// CIR: cir.func {{.*}}@arg_i32_wrapper(%arg0: !s32i{{.*}})
// LLVM: define {{.*}}void @arg_i32_wrapper(i32 %{{.*}})

void arg_f32_wrapper(f32_wrapper_t w) {}

// CIR: cir.func {{.*}}@arg_f32_wrapper(%arg0: !cir.float{{.*}})
// LLVM: define {{.*}}void @arg_f32_wrapper(float %{{.*}})

void arg_ptr_wrapper(ptr_wrapper_t w) {}

// CIR: cir.func {{.*}}@arg_ptr_wrapper(%arg0: !cir.ptr<{{[^,]*}}>{{.*}})
// LLVM: define {{.*}}void @arg_ptr_wrapper(ptr %{{.*}})

// Aggregates up to 32 bits are packed into one integer register.
void arg_char2(char2_t s) {}

// CIR: cir.func {{.*}}@arg_char2(%arg0: !u16i{{.*}})
// LLVM: define {{.*}}void @arg_char2(i16 %{{.*}})

void arg_small(small_t s) {}

// CIR: cir.func {{.*}}@arg_small(%arg0: !u32i{{.*}})
// LLVM: define {{.*}}void @arg_small(i32 %{{.*}})

void arg_union(int_float_t u) {}

// CIR: cir.func {{.*}}@arg_union(%arg0: !u32i{{.*}})
// LLVM: define {{.*}}void @arg_union(i32 %{{.*}})

// A transparent union is passed as its first member.
void arg_transparent(transparent_t u) {}

// CIR: cir.func {{.*}}@arg_transparent(%[[U:[^:]+]]: !cir.ptr<!s32i>{{.*}})
// CIR-NEXT:   %[[U_COERCE:.*]] = cir.alloca "coerce" align(8) : !cir.ptr<!cir.ptr<!s32i>, target_address_space(5)>
// CIR-NEXT:   cir.store %[[U]], %[[U_COERCE]] : !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>, target_address_space(5)>
// LLVM: define {{.*}}void @arg_transparent(ptr %[[U:[^)]+]])
// LLVM:   store ptr %[[U]], ptr addrspace(5) %{{.*}}, align 8

// A caller can pass any member type directly.
void call_transparent(int *p) { arg_transparent(p); }

// CIR: cir.func {{.*}}@call_transparent(
// CIR:   %[[U_PTR:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_transparent_t> -> !cir.ptr<!cir.ptr<!s32i>>
// CIR-NEXT:   %[[ARG:.*]] = cir.load align(8) %[[U_PTR]] : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// CIR-NEXT:   cir.call @arg_transparent(%[[ARG]]) {{.*}}: (!cir.ptr<!s32i>) -> ()
// LLVM: define {{.*}}void @call_transparent(ptr noundef %{{.*}})
// LLVM:   call void @arg_transparent(ptr %{{.*}})

// Aggregates of 33 to 64 bits are packed into a pair of i32 registers.
void arg_pair(pair_t p) {}

// CIR: cir.func {{.*}}@arg_pair(%[[P:[^:]+]]: !cir.array<!u32i x 2>{{.*}})
// CIR-NEXT:   %[[P_COERCE:.*]] = cir.alloca "coerce" align(4) : !cir.ptr<!cir.array<!u32i x 2>, target_address_space(5)>
// CIR-NEXT:   cir.store %[[P]], %[[P_COERCE]] : !cir.array<!u32i x 2>, !cir.ptr<!cir.array<!u32i x 2>, target_address_space(5)>
// LLVM: define {{.*}}void @arg_pair([2 x i32] %[[P:[^)]+]])
// LLVM:   store [2 x i32] %[[P]], ptr addrspace(5) %{{.*}}, align 4

// Larger aggregates that fit the register budget pass one field per register.
void arg_triple(triple_t t) {}

// CIR: cir.func {{.*}}@arg_triple(%[[T0:[^:]+]]: !s32i{{.*}}, %[[T1:[^:]+]]: !s32i{{.*}}, %[[T2:[^:]+]]: !s32i{{.*}})
// CIR-NEXT:   %[[T_COERCE:.*]] = cir.alloca "coerce" align(4) : !cir.ptr<!rec_triple_t, target_address_space(5)>
// CIR-NEXT:   %[[F0:.*]] = cir.get_member %[[T_COERCE]][0]
// CIR-NEXT:   cir.store %[[T0]], %[[F0]]
// CIR-NEXT:   %[[F1:.*]] = cir.get_member %[[T_COERCE]][1]
// CIR-NEXT:   cir.store %[[T1]], %[[F1]]
// CIR-NEXT:   %[[F2:.*]] = cir.get_member %[[T_COERCE]][2]
// CIR-NEXT:   cir.store %[[T2]], %[[F2]]
// LLVM: define {{.*}}void @arg_triple(i32 %[[T0:[^,]+]], i32 %[[T1:[^,]+]], i32 %[[T2:[^)]+]])
// LLVM:   store i32 %[[T0]], ptr addrspace(5) %{{.*}}, align 4
// LLVM:   store i32 %[[T1]], ptr addrspace(5) %{{.*}}, align 4
// LLVM:   store i32 %[[T2]], ptr addrspace(5) %{{.*}}, align 4

// The call site is coerced the same way as the callee.
void call_small(small_t s) { arg_small(s); }

// CIR: cir.func {{.*}}@call_small(%arg0: !u32i{{.*}})
// CIR:   cir.call @arg_small(%{{.*}}){{.*}}: (!u32i{{.*}}) -> ()
// LLVM: define {{.*}}void @call_small(i32 %{{.*}})
// LLVM:   call void @arg_small(i32 %{{.*}})

void call_pair(pair_t p) { arg_pair(p); }

// CIR: cir.func {{.*}}@call_pair(
// CIR:   %[[ARG:.*]] = cir.load align(4) %{{.*}} : !cir.ptr<!cir.array<!u32i x 2>>, !cir.array<!u32i x 2>
// CIR:   cir.call @arg_pair(%[[ARG]]) {{.*}}: (!cir.array<!u32i x 2>) -> ()
// LLVM: define {{.*}}void @call_pair(
// LLVM:   %[[ARG:.*]] = load [2 x i32], ptr %{{.*}}, align 4
// LLVM:   call void @arg_pair([2 x i32] %[[ARG]])

void call_triple(triple_t t) { arg_triple(t); }

// CIR: cir.func {{.*}}@call_triple(
// CIR:   cir.call @arg_triple(%{{.*}}, %{{.*}}, %{{.*}}) {{.*}}: (!s32i, !s32i, !s32i) -> ()
// LLVM: define {{.*}}void @call_triple(
// LLVM:   call void @arg_triple(i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}})

// The call site carries the same extension as the callee.
void call_char(char c) { arg_char(c); }

// LLVM: define {{.*}}void @call_char(i8 noundef signext %{{.*}})
// LLVM:   call void @arg_char(i8 noundef signext %{{.*}})

void take_var(int n, ...);

// Promoted scalars pass through an ellipsis unchanged.
void call_var_scalars(char c, double d) { take_var(2, c, d); }

// CIR: cir.func {{.*}}@call_var_scalars(
// CIR:   %[[C:.*]] = cir.load align(1) %{{.*}} : !cir.ptr<!s8i>, !s8i
// CIR-NEXT:   %[[C_INT:.*]] = cir.cast integral %[[C]] : !s8i -> !s32i
// CIR-NEXT:   %[[D:.*]] = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CIR-NEXT:   cir.call @take_var(%{{.*}}, %[[C_INT]], %[[D]]) {{.*}}: (!s32i {llvm.noundef}, !s32i {llvm.noundef}, !cir.double {llvm.noundef}) -> ()
// CIR: cir.func private @take_var(!s32i {llvm.noundef}, ...)
// LLVM: define {{.*}}void @call_var_scalars(
// LLVM:   %[[C_INT:.*]] = sext i8 %{{.*}} to i32
// LLVM-NEXT:   %[[D:.*]] = load double, ptr %{{.*}}, align 8
// LLVM-NEXT:   call void (i32, ...) @take_var(i32 noundef 2, i32 noundef %[[C_INT]], double noundef %[[D]])
// LLVM: declare void @take_var(i32 noundef, ...)

// Aggregates pass through an ellipsis whole, not packed into registers.
void call_var_aggs(pair_t p, small_t s) { take_var(2, p, s); }

// CIR: cir.func {{.*}}@call_var_aggs(
// CIR:   %[[P:.*]] = cir.load align(4) %{{.*}} : !cir.ptr<!rec_pair_t>, !rec_pair_t
// CIR-NEXT:   %[[S:.*]] = cir.load align(1) %{{.*}} : !cir.ptr<!rec_small_t>, !rec_small_t
// CIR-NEXT:   cir.call @take_var(%{{.*}}, %[[P]], %[[S]]) {{.*}}: (!s32i {llvm.noundef}, !rec_pair_t, !rec_small_t) -> ()
// LLVM: define {{.*}}void @call_var_aggs([2 x i32] %{{.*}}, i32 %{{.*}})
// LLVM:   %[[P:.*]] = load %struct.pair_t, ptr %{{.*}}, align 4
// LLVM-NEXT:   %[[S:.*]] = load %struct.small_t, ptr %{{.*}}, align 1
// LLVM-NEXT:   call void (i32, ...) @take_var(i32 noundef 2, %struct.pair_t %[[P]], %struct.small_t %[[S]])

// An aggregate past the register budget is passed byref in addrspace(5).
void arg_big(big_t b) {}

// CIR: cir.func {{.*}}@arg_big(%[[B_ARG:[^:]+]]: !cir.ptr<!rec_big_t, target_address_space(5)> {{.*}}llvm.byref = !rec_big_t
// CIR:   cir.copy %[[B_ARG]] align(4) to %{{.*}} align(4) : !cir.ptr<!rec_big_t, target_address_space(5)>, !cir.ptr<!rec_big_t>
// LLVM: define {{.*}}void @arg_big(ptr addrspace(5) noundef byref(%struct.big_t) align 4 %[[B_ARG:.*]])
// LLVM:   call void @llvm.memcpy.{{.*}}(ptr{{.*}} align 4 %{{.*}}, ptr addrspace(5) align 4 %[[B_ARG]], i64 68, i1 false)

// The caller passes its own local, not a copy of it.
void call_big(void) {
  big_t b = {0};
  arg_big(b);
}

// CIR: cir.func {{.*}}@call_big()
// CIR:   %[[B:.*]] = cir.alloca "b" {{.*}} : !cir.ptr<!rec_big_t, target_address_space(5)>
// CIR:   cir.call @arg_big(%[[B]]) {{.*}}: (!cir.ptr<!rec_big_t, target_address_space(5)> {{.*}}llvm.byref = !rec_big_t
// LLVM: define {{.*}}void @call_big()
// LLVM:   call void @arg_big(ptr addrspace(5) noundef byref(%struct.big_t) align 4 %{{.*}})

// Scalars use the same register budget, so a struct that would fit on its
// own is passed byref once they have used it up.
void arg_budget(int a0, int a1, int a2, int a3, int a4, int a5, int a6,
                int a7, int a8, int a9, int a10, int a11, int a12, int a13,
                triple_t t) {}

// CIR: cir.func {{.*}}@arg_budget(%arg0: !s32i {{.*}}%arg13: !s32i {{.*}}, %[[T_ARG:[^:]+]]: !cir.ptr<!rec_triple_t, target_address_space(5)> {{.*}}llvm.byref = !rec_triple_t
// CIR:   cir.copy %[[T_ARG]] align(4) to %{{.*}} align(4) : !cir.ptr<!rec_triple_t, target_address_space(5)>, !cir.ptr<!rec_triple_t>
// LLVM: define {{.*}}void @arg_budget(i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, i32 noundef %{{.*}}, ptr addrspace(5) noundef byref(%struct.triple_t) align 4 %[[T_ARG:.*]])
// LLVM:   call void @llvm.memcpy.{{.*}}(ptr{{.*}} align 4 %{{.*}}, ptr addrspace(5) align 4 %[[T_ARG]], i64 12, i1 false)

// Bit-fields are packed like any other small aggregate.
void arg_bits(bits_t b) {}

// CIR: cir.func {{.*}}@arg_bits(%[[B:[^:]+]]: !u32i{{.*}})
// CIR-NEXT:   %[[B_COERCE:.*]] = cir.alloca "coerce" align(4) : !cir.ptr<!u32i, target_address_space(5)>
// CIR-NEXT:   cir.store %[[B]], %[[B_COERCE]] : !u32i, !cir.ptr<!u32i, target_address_space(5)>
// LLVM: define {{.*}}void @arg_bits(i32 %[[B:[^)]+]])
// LLVM:   store i32 %[[B]], ptr addrspace(5) %{{.*}}, align 4

// A _Complex up to 64 bits is packed like a small aggregate.
void arg_cfloat(_Complex float c) {}

// CIR: cir.func {{.*}}@arg_cfloat(%[[C:[^:]+]]: !cir.array<!u32i x 2> {{.*}})
// CIR-NEXT:   %[[C_COERCE:.*]] = cir.alloca "coerce" align(4) : !cir.ptr<!cir.array<!u32i x 2>, target_address_space(5)>
// CIR-NEXT:   cir.store %[[C]], %[[C_COERCE]] : !cir.array<!u32i x 2>, !cir.ptr<!cir.array<!u32i x 2>, target_address_space(5)>
// CIR-NEXT:   cir.cast bitcast %[[C_COERCE]] : !cir.ptr<!cir.array<!u32i x 2>, target_address_space(5)> -> !cir.ptr<!cir.complex<!cir.float>, target_address_space(5)>
// LLVM: define {{.*}}void @arg_cfloat([2 x i32] noundef %[[C:[^)]+]])
// LLVM:   store [2 x i32] %[[C]], ptr addrspace(5) %{{.*}}, align 4

// 128-bit integers pass whole.
void arg_int128(__int128 i) {}

// CIR: cir.func {{.*}}@arg_int128(%[[I:[^:]+]]: !s128i {{.*}})
// CIR:   cir.store %[[I]], %{{.*}} : !s128i, !cir.ptr<!s128i>
// LLVM: define {{.*}}void @arg_int128(i128 noundef %[[I:[^)]+]])
// LLVM:   store i128 %[[I]], ptr %{{.*}}

void arg_bitint128(_BitInt(128) i) {}

// CIR: cir.func {{.*}}@arg_bitint128(%[[I:[^:]+]]: !s128i_bitint {{.*}})
// CIR:   cir.store %[[I]], %{{.*}} : !s128i_bitint, !cir.ptr<!s128i_bitint>
// LLVM: define {{.*}}void @arg_bitint128(i128 noundef %[[I:[^)]+]])
// LLVM:   store i128 %[[I]], ptr %{{.*}}, align 8

// A call through a function pointer is coerced like a direct call.
void call_fnptr(void (*fp)(triple_t), triple_t t) { fp(t); }

// CIR: cir.func {{.*}}@call_fnptr(%{{.*}}: !cir.ptr<!cir.func<(!rec_triple_t)>> {{.*}}, %{{.*}}: !s32i{{.*}}, %{{.*}}: !s32i{{.*}}, %{{.*}}: !s32i{{.*}})
// CIR:   %[[FN:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!cir.func<(!rec_triple_t)>> -> !cir.ptr<!cir.func<(!s32i, !s32i, !s32i)>>
// CIR-NEXT:   cir.call %[[FN]](%{{.*}}, %{{.*}}, %{{.*}}) {{.*}}: (!cir.ptr<!cir.func<(!s32i, !s32i, !s32i)>>, !s32i, !s32i, !s32i) -> ()
// LLVM: define {{.*}}void @call_fnptr(ptr noundef %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}})
// LLVM:   %[[FN:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:   call void %[[FN]](i32 %{{.*}}, i32 %{{.*}}, i32 %{{.*}})
