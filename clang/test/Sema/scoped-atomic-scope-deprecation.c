// RUN: %clang_cc1 -x c -triple=amdgcn-amd-amdhsa -verify -fsyntax-only %s
// RUN: %clang_cc1 -x c -triple=x86_64-pc-linux-gnu -verify -fsyntax-only %s
// RUN: %clang_cc1 -x c -triple=spirv64-unknown-unknown -verify -fsyntax-only %s

// Test deprecation warning for integer types used as scope arguments

void test_integer_literal_scope(int *ptr) {
  int val;
  // Using integer literals should trigger deprecation warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, 0); // expected-warning {{synchronization scope should be of type __memory_scope}}
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, 1); // expected-warning {{synchronization scope should be of type __memory_scope}}
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, 2); // expected-warning {{synchronization scope should be of type __memory_scope}}
}

void test_integer_variable_scope(int *ptr) {
  int val;
  int scope = 0;
  // Using integer variables should trigger deprecation warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, scope); // expected-warning {{synchronization scope should be of type __memory_scope}}
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, scope); // expected-warning {{synchronization scope should be of type __memory_scope}}
}

void test_enum_scope_no_warning(int *ptr) {
  int val;
  // Using __memory_scope enum values should NOT trigger warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __memory_scope_system); // no warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __memory_scope_device); // no warning
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, __memory_scope_workgroup); // no warning
}

void test_macro_scope_no_warning(int *ptr) {
  int val;
  // Using __MEMORY_SCOPE_* macros (which expand to enum values) should NOT trigger warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __MEMORY_SCOPE_SYSTEM); // no warning
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __MEMORY_SCOPE_DEVICE); // no warning
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, __MEMORY_SCOPE_WRKGRP); // no warning
}

void test_various_scoped_atomics_with_integer(int *ptr) {
  // Test deprecation warning with various scoped atomic operations
  *ptr = __scoped_atomic_fetch_add(ptr, 1, __ATOMIC_RELAXED, 0); // expected-warning {{synchronization scope should be of type __memory_scope}}
  *ptr = __scoped_atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED, 1); // expected-warning {{synchronization scope should be of type __memory_scope}}
  *ptr = __scoped_atomic_fetch_sub(ptr, 1, __ATOMIC_RELAXED, 2); // expected-warning {{synchronization scope should be of type __memory_scope}}
  __scoped_atomic_store_n(ptr, 1, __ATOMIC_RELAXED, 3); // expected-warning {{synchronization scope should be of type __memory_scope}}
}

void test_various_scoped_atomics_with_enum(int *ptr) {
  // Test NO warning with various scoped atomic operations using enum
  *ptr = __scoped_atomic_fetch_add(ptr, 1, __ATOMIC_RELAXED, __memory_scope_device); // no warning
  *ptr = __scoped_atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED, __memory_scope_system); // no warning
  *ptr = __scoped_atomic_fetch_sub(ptr, 1, __ATOMIC_RELAXED, __memory_scope_workgroup); // no warning
  __scoped_atomic_store_n(ptr, 1, __ATOMIC_RELAXED, __memory_scope_wavefront); // no warning
}

void test_fence_with_integer() {
  // Test deprecation warning with __scoped_atomic_thread_fence using integer literals
  __scoped_atomic_thread_fence(__ATOMIC_SEQ_CST, 0); // expected-warning {{synchronization scope should be of type __memory_scope}}
  __scoped_atomic_thread_fence(__ATOMIC_ACQUIRE, 1); // expected-warning {{synchronization scope should be of type __memory_scope}}

  int scope_var = 2;
  __scoped_atomic_thread_fence(__ATOMIC_RELEASE, scope_var); // expected-warning {{synchronization scope should be of type __memory_scope}}
}

void test_fence_with_enum() {
  // Test NO warning with __scoped_atomic_thread_fence using enum values
  __scoped_atomic_thread_fence(__ATOMIC_SEQ_CST, __memory_scope_system); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_ACQUIRE, __memory_scope_device); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_RELEASE, __memory_scope_workgroup); // no warning
}

void test_fence_with_macro() {
  // Test NO warning with __scoped_atomic_thread_fence using macros
  __scoped_atomic_thread_fence(__ATOMIC_SEQ_CST, __MEMORY_SCOPE_SYSTEM); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_ACQUIRE, __MEMORY_SCOPE_DEVICE); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_RELEASE, __MEMORY_SCOPE_WRKGRP); // no warning
}

void helper_with_scope_param(__memory_scope scope, int *ptr, int val) {
  // Test that passing __memory_scope as function parameter works without warning
  __scoped_atomic_store_n(ptr, val, __ATOMIC_RELAXED, scope); // no warning
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, scope); // no warning
  *ptr = __scoped_atomic_fetch_add(ptr, 1, __ATOMIC_RELAXED, scope); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_SEQ_CST, scope); // no warning
}

void test_function_with_scope_parameter(int *ptr) {
  // Test calling function that takes __memory_scope parameter
  helper_with_scope_param(__memory_scope_device, ptr, 42); // no warning
  helper_with_scope_param(__memory_scope_system, ptr, 100); // no warning
  helper_with_scope_param(__MEMORY_SCOPE_WRKGRP, ptr, 200); // no warning
}

__memory_scope get_scope_from_function(void) {
  return __memory_scope_workgroup;
}

void test_scope_from_function_call(int *ptr) {
  // Test using __memory_scope returned from a function
  __memory_scope scope = get_scope_from_function();
  __scoped_atomic_store_n(ptr, 1, __ATOMIC_RELAXED, scope); // no warning
  *ptr = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, scope); // no warning
  __scoped_atomic_thread_fence(__ATOMIC_RELEASE, scope); // no warning

  // Test using function call directly as argument
  __scoped_atomic_store_n(ptr, 2, __ATOMIC_RELAXED, get_scope_from_function()); // no warning
}
