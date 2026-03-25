//===-- Unittests for wctype perfect hash maps ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/wctype/lower_to_upper.h"
#include "src/__support/wctype/upper_to_lower.h"
#include "test/UnitTest/Test.h"

namespace LIBC_NAMESPACE_DECL {

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpper) {
  for (const auto entry : LOWER_TO_UPPER_DATA) {
    const auto &lower = entry[0];
    const auto &upper = entry[1];
    EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(lower)) << lower << "\n";
    EXPECT_EQ(upper, LOWER_TO_UPPER_MAP.find(lower).value())
        << lower << " -> " << upper << "\n";
  }
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLower) {
  for (const auto entry : UPPER_TO_LOWER_DATA) {
    const auto &upper = entry[0];
    const auto &lower = entry[1];
    EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(upper)) << upper << "\n";
    EXPECT_EQ(lower, UPPER_TO_LOWER_MAP.find(upper).value())
        << upper << " -> " << lower << "\n";
  }
}

#if WINT_MAX > 65535

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpper4BytesWidth) {
  for (const auto entry : LOWER_TO_UPPER_DATA_32) {
    const auto &lower = entry[0];
    const auto &upper = entry[1];
    EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(lower)) << lower << "\n";
    EXPECT_EQ(upper, LOWER_TO_UPPER_MAP_32.find(lower).value())
        << lower << " -> " << upper << "\n";
  }
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLower4BytesWidth) {
  for (const auto entry : UPPER_TO_LOWER_DATA_32) {
    const auto &upper = entry[0];
    const auto &lower = entry[1];
    EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(upper)) << upper << "\n";
    EXPECT_EQ(lower, UPPER_TO_LOWER_MAP_32.find(upper).value())
        << upper << " -> " << lower << "\n";
  }
}

#endif // WINT_MAX > 65535

} // namespace LIBC_NAMESPACE_DECL
