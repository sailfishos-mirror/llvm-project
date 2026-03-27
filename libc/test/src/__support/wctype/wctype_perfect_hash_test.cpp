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
namespace wctype_internal {

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpperKeyCheck) {
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0x61));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0x62));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0x63));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0xE7));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0x111));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(0x29E));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x0));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x1));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x2));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x3));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x33));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x332));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x20));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x35));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x23));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x400));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x401));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x47A));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x50));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x53));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x55));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x3FF));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0x3FE));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0xFFF));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(0xFFFF));
#if WINT_MAX > 0xFFFF
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(0x10428));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(0x104DD));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(0x1E940));
  EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(0x1E932));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x0));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x20));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x40));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x1F9E));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x1E91C));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x1E91F));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x118A1));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0x10D5A));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0xFFFFF));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0xFFFFFF));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0xFFFFFFF));
  EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(0xFFFFFFFF));
#endif
}

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpperConversion) {
  EXPECT_EQ(static_cast<wint_t>(0x41), LOWER_TO_UPPER_MAP.find(0x61).value());
  EXPECT_EQ(static_cast<wint_t>(0x42), LOWER_TO_UPPER_MAP.find(0x62).value());
  EXPECT_EQ(static_cast<wint_t>(0x43), LOWER_TO_UPPER_MAP.find(0x63).value());
  EXPECT_EQ(static_cast<wint_t>(0xC7), LOWER_TO_UPPER_MAP.find(0xE7).value());
  EXPECT_EQ(static_cast<wint_t>(0x110), LOWER_TO_UPPER_MAP.find(0x111).value());
  EXPECT_EQ(static_cast<wint_t>(0xA7B0),
            LOWER_TO_UPPER_MAP.find(0x29E).value());
#if WINT_MAX > 0xFFFF
  EXPECT_EQ(static_cast<wint_t>(0x10400),
            LOWER_TO_UPPER_MAP_32.find(0x10428).value());
  EXPECT_EQ(static_cast<wint_t>(0x104B5),
            LOWER_TO_UPPER_MAP_32.find(0x104DD).value());
  EXPECT_EQ(static_cast<wint_t>(0x1E91E),
            LOWER_TO_UPPER_MAP_32.find(0x1E940).value());
  EXPECT_EQ(static_cast<wint_t>(0x1E910),
            LOWER_TO_UPPER_MAP_32.find(0x1E932).value());
#endif
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLowerKeyCheck) {
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x400));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x401));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x47A));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x50));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x53));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x55));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x3FF));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(0x3FE));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x61));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x62));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x63));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0xE7));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x111));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x29E));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x0));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x1));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x2));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x3));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x33));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x332));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x20));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x35));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0x23));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0xFFF));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(0xFFFF));
#if WINT_MAX > 0xFFFF
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(0x1E91C));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(0x1E91F));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(0x118A1));
  EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(0x10D5A));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x10428));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x104DD));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x1E940));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x1E932));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x0));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x20));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x40));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0x1F9E));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0xFFFFF));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0xFFFFFF));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0xFFFFFFF));
  EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(0xFFFFFFFF));
#endif
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLowerConversion) {
  EXPECT_EQ(static_cast<wint_t>(0x61), UPPER_TO_LOWER_MAP.find(0x41).value());
  EXPECT_EQ(static_cast<wint_t>(0x62), UPPER_TO_LOWER_MAP.find(0x42).value());
  EXPECT_EQ(static_cast<wint_t>(0x63), UPPER_TO_LOWER_MAP.find(0x43).value());
  EXPECT_EQ(static_cast<wint_t>(0xE7), UPPER_TO_LOWER_MAP.find(0xC7).value());
  EXPECT_EQ(static_cast<wint_t>(0x111), UPPER_TO_LOWER_MAP.find(0x110).value());
  EXPECT_EQ(static_cast<wint_t>(0x29E),
            UPPER_TO_LOWER_MAP.find(0xA7B0).value());
#if WINT_MAX > 0xFFFF
  EXPECT_EQ(static_cast<wint_t>(0x10428),
            UPPER_TO_LOWER_MAP_32.find(0x10400).value());
  EXPECT_EQ(static_cast<wint_t>(0x104DD),
            UPPER_TO_LOWER_MAP_32.find(0x104B5).value());
  EXPECT_EQ(static_cast<wint_t>(0x1E940),
            UPPER_TO_LOWER_MAP_32.find(0x1E91E).value());
  EXPECT_EQ(static_cast<wint_t>(0x1E932),
            UPPER_TO_LOWER_MAP_32.find(0x1E910).value());
#endif
}

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpperFullCoverage) {
  for (const auto entry : LOWER_TO_UPPER_DATA) {
    const auto &lower = entry[0];
    const auto &upper = entry[1];
    EXPECT_EQ(true, LOWER_TO_UPPER_MAP.contains(lower)) << lower << "\n";
    EXPECT_EQ(false, LOWER_TO_UPPER_MAP.contains(upper)) << upper << "\n";
    EXPECT_EQ(upper, LOWER_TO_UPPER_MAP.find(lower).value())
        << lower << " -> " << upper << "\n";
  }
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLowerFullCoverage) {
  for (const auto entry : UPPER_TO_LOWER_DATA) {
    const auto &upper = entry[0];
    const auto &lower = entry[1];
    EXPECT_EQ(true, UPPER_TO_LOWER_MAP.contains(upper)) << upper << "\n";
    EXPECT_EQ(false, UPPER_TO_LOWER_MAP.contains(lower)) << lower << "\n";
    EXPECT_EQ(lower, UPPER_TO_LOWER_MAP.find(upper).value())
        << upper << " -> " << lower << "\n";
  }
}

#if WINT_MAX > 0xFFFF

TEST(LlvmLibcWctypePerfectHashTest, LowerToUpper4BytesWidth) {
  for (const auto entry : LOWER_TO_UPPER_DATA_32) {
    const auto &lower = entry[0];
    const auto &upper = entry[1];
    EXPECT_EQ(true, LOWER_TO_UPPER_MAP_32.contains(lower)) << lower << "\n";
    EXPECT_EQ(false, LOWER_TO_UPPER_MAP_32.contains(upper)) << upper << "\n";
    EXPECT_EQ(upper, LOWER_TO_UPPER_MAP_32.find(lower).value())
        << lower << " -> " << upper << "\n";
  }
}

TEST(LlvmLibcWctypePerfectHashTest, UpperToLower4BytesWidth) {
  for (const auto entry : UPPER_TO_LOWER_DATA_32) {
    const auto &upper = entry[0];
    const auto &lower = entry[1];
    EXPECT_EQ(true, UPPER_TO_LOWER_MAP_32.contains(upper)) << upper << "\n";
    EXPECT_EQ(false, UPPER_TO_LOWER_MAP_32.contains(lower)) << lower << "\n";
    EXPECT_EQ(lower, UPPER_TO_LOWER_MAP_32.find(upper).value())
        << upper << " -> " << lower << "\n";
  }
}

#endif // WINT_MAX > 0xFFFF

} // namespace wctype_internal
} // namespace LIBC_NAMESPACE_DECL
