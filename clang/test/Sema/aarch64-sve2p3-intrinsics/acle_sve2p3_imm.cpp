// REQUIRES: aarch64-registered-target

// RUN: %clang_cc1 -triple aarch64-none-linux-gnu -target-feature +sve -target-feature +sve2p3 -fsyntax-only -verify %s
// RUN: %clang_cc1 -DSVE_OVERLOADED_FORMS -triple aarch64-none-linux-gnu -target-feature +sve -target-feature +sve2p3 -fsyntax-only -verify %s

#ifdef SVE_OVERLOADED_FORMS
#define SVE_ACLE_FUNC(A1, A2_UNUSED) A1
#else
#define SVE_ACLE_FUNC(A1, A2) A1##A2
#endif

#include <arm_sve.h>

void test_range_0_1() {
  // expected-error-re@+1 {{argument value {{[0-9]+}} is outside the valid range [0, 1]}}
  SVE_ACLE_FUNC(svluti6_lane, _s16_x2)(svcreate2_s16(svundef_s16(), svundef_s16()),
                                        svundef_u8(), -1);
  // expected-error-re@+1 {{argument value {{[0-9]+}} is outside the valid range [0, 1]}}
  SVE_ACLE_FUNC(svluti6_lane, _u16_x2)(svcreate2_u16(svundef_u16(), svundef_u16()),
                                        svundef_u8(), 2);
  // expected-error-re@+1 {{argument value {{[0-9]+}} is outside the valid range [0, 1]}}
  SVE_ACLE_FUNC(svluti6_lane, _f16_x2)(svcreate2_f16(svundef_f16(), svundef_f16()),
                                        svundef_u8(), -1);
}
