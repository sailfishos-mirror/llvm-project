// RUN: %clang_cc1 -fsyntax-only -std=c11 -verify %s

// Test that declaring __memory_scope conflicts with the builtin enum

// User declares their own enum first
enum __memory_scope {
  my_value = 0
};

// Trying to use builtin identifier will find the user's enum
// but the builtin enumerators won't be available
void test(void) {
  __memory_scope x = __memory_scope_system; // expected-error {{must use 'enum' tag}} expected-error {{use of undeclared identifier '__memory_scope_system'}}
}
