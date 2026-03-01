// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REQUIRES: target={{(aarch64|s390x|x86_64|arm64e)-.+}}
// UNSUPPORTED: target={{.*-windows.*}}

// *SAN does not like our clearly nonsense personality and handler functions
// which is the correct response for them, but alas we have to allow it for JITs
// because they tend to use a shared handler rather than having the handler
// within the function bounds.
// UNSUPPORTED: asan
// UNSUPPORTED: msan

#include <libunwind.h>
#include <stdint.h>
#include <stdio.h>
#include <unwind.h>

extern "C" void exit(int);

struct alignas(16) AlignmentTester {
  __attribute__((noinline)) void testAlignment() {
    uintptr_t thisValue = (uintptr_t)this;
    if (thisValue % 16 != 0) {
      fprintf(stderr, "Test bug: unaligned handler frame\n");
      __builtin_trap();
    }
  }
};

#if defined(__x86_64__)
// Use an intermediary function on x86_64 as the handlers are dispatched with
// the expectation that they will perform some stack correction, most notably
// planting down a restored base pointer
extern "C" int
randomUnrelatedFunctionImpl() __asm__("_randomUnrelatedFunctionImpl");
__attribute__((noinline)) __attribute__((naked)) int randomUnrelatedFunction() {
  asm("pushq %rbp \n\t"
      "movq %rsp, %rbp\n\t"
      "jmp _randomUnrelatedFunctionImpl\n\t");
}
extern "C" __attribute__((noinline)) int randomUnrelatedFunctionImpl() {
#else
static int randomUnrelatedFunction() {
#endif
  AlignmentTester tester;
  tester.testAlignment();
  fprintf(stderr,
          "Successfully dispatched to handler unrelated to actual function\n");
  exit(0);
  return 0;
}

extern "C" int __gxx_personality_v0(int, _Unwind_Action actions, uint64_t,
                                    _Unwind_Exception *,
                                    _Unwind_Context *context) {

  if (actions & 1) // Search
    return _URC_HANDLER_FOUND;

  // Having claimed to have found a handler, we just set an unrelated
  // the PC to an unrelated function
#if defined(__PTRAUTH__) || __has_feature(ptrauth_calls)
  uintptr_t sp = _Unwind_GetCFA(context);
  uintptr_t handler = (uintptr_t)ptrauth_auth_and_resign(
      (void *)&randomUnrelatedFunction, ptrauth_key_function_pointer, 0,
      ptrauth_key_return_address, (void *)sp);
#else
  uintptr_t handler = (uintptr_t)&randomUnrelatedFunction;
#endif
  _Unwind_SetIP(context, handler);
  return _URC_INSTALL_CONTEXT;
}

__attribute__((noinline)) static int throwAThing() { throw 1; }

int main(int, const char **) {
  try {
    throwAThing();
  } catch (int) {
    fprintf(stderr, "Failed: did not call the overridden personality\n");
    return 1;
  }
  fprintf(stderr, "Failed: did not trigger exception handler\n");
  return 1;
}
