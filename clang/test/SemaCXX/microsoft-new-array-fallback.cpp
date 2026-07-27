// RUN: %clang_cc1 -fms-compatibility -fsyntax-only -verify -std=c++11 %s

typedef __SIZE_TYPE__ size_t;

void *operator new(size_t); // #new_decl

struct Tag {};

void f() {
  int *p = new (Tag{}) int[4]; // #new_expr
  // expected-error@#new_expr {{no matching function for call to 'operator new[]'}}
  // expected-note@#new_decl {{candidate function not viable: requires 1 argument, but 2 were provided}}
  // expected-note@#new_expr {{MSVC compatibility fall back to 'operator new' failed}}
  (void)p;
}
