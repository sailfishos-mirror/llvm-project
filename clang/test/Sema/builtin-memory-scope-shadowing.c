// RUN: %clang_cc1 -fsyntax-only -std=c11 -verify -triple=amdgcn-amd-amdhsa %s

// Test that user enumerators shadowing builtin __memory_scope_* names
// are caught by type checking when passed to scoped atomic operations.

// User declares an enumerator that shadows a builtin name
enum __foo {
  __memory_scope_system = 999
};

void test_shadowing_rejected(int *ptr) {
  int val;

  // Using the user's enumerator (which has type 'enum __foo' not '__memory_scope')
  // triggers a warning, plus an error for the invalid scope value
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __memory_scope_system); // expected-warning {{synchronization scope should be of type __memory_scope}} expected-error {{synchronization scope argument to atomic operation is invalid}}

  val = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, __memory_scope_system); // expected-warning {{synchronization scope should be of type __memory_scope}} expected-error {{synchronization scope argument to atomic operation is invalid}}

  __scoped_atomic_store_n(ptr, 42, __ATOMIC_RELAXED, __memory_scope_system); // expected-warning {{synchronization scope should be of type __memory_scope}} expected-error {{synchronization scope argument to atomic operation is invalid}}
}

void test_builtin_still_works(int *ptr) {
  int val;

  // The real builtin __memory_scope_device should still work
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, __memory_scope_device); // no warning - correct type

  val = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, __memory_scope_device); // no warning
}

void test_integer_literal_deprecated(int *ptr) {
  int val;

  // Integer literals should get deprecation warning (not error)
  __scoped_atomic_load(ptr, &val, __ATOMIC_RELAXED, 0); // expected-warning {{synchronization scope should be of type __memory_scope}}

  val = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, 1); // expected-warning {{synchronization scope should be of type __memory_scope}}

  // Integer variables should also get deprecation warning
  int scope = 2;
  __scoped_atomic_store_n(ptr, 42, __ATOMIC_RELAXED, scope); // expected-warning {{synchronization scope should be of type __memory_scope}}
}

// Test shadowing with anonymous enum
enum {
  __memory_scope_device = 888
};

void test_anonymous_enum_shadowing(int *ptr) {
  int val = __scoped_atomic_load_n(ptr, __ATOMIC_RELAXED, __memory_scope_device); // expected-warning {{synchronization scope should be of type __memory_scope}} expected-error {{synchronization scope argument to atomic operation is invalid}}
}
