//===- llvm/unittests/Frontend/EnumSetTest.cpp - EnumSet unit tests -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Frontend/OpenMP/OMP.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::omp;

namespace {
namespace detail {
template <typename Elem, Elem...> struct is_one_of {
  constexpr bool operator()(Elem) const { return false; }
};

template <typename Elem, Elem Value, Elem... Values>
struct is_one_of<Elem, Value, Values...> {
  constexpr bool operator()(Elem V) const {
    return V == Value || is_one_of<Elem, Values...>{}(V);
  }
};

template <typename Elem, Elem... Values, typename Range>
constexpr bool ElementsAre(Range &&R) {
  size_t Count = 0;
  // This also serves as an EnumSetIterator test.
  for (auto It = R.begin(), End = R.end(); It != End; ++It) {
    if (!is_one_of<Elem, Values...>{}(*It))
      return false;
    ++Count;
  }
  if (Count != sizeof...(Values))
    return false;
  return true;
}
} // namespace detail

using ClauseSet = EnumSet<Clause, Clause_enumSize>;

TEST(EnumSetTest, DefaultInitialization) {
  constexpr ClauseSet S;
  EXPECT_THAT(S, testing::IsEmpty());
  EXPECT_EQ(S.size(), static_cast<size_t>(0));

  static_assert(S.empty());
  static_assert(S.size() == 0);
}

TEST(EnumSetTest, ListInitialization) {
  constexpr ClauseSet S{Clause::OMPC_private, Clause::OMPC_shared};
  EXPECT_THAT(S, testing::ElementsAre(OMPC_private, OMPC_shared));

  static_assert(
      detail::ElementsAre<Clause, Clause::OMPC_private, Clause::OMPC_shared>(
          S));
}

TEST(EnumSetTest, CopyInitialization) {
  constexpr ClauseSet S(ClauseSet{Clause::OMPC_private, Clause::OMPC_shared});
  EXPECT_THAT(S, testing::ElementsAre(OMPC_private, OMPC_shared));

  static_assert(
      detail::ElementsAre<Clause, Clause::OMPC_private, Clause::OMPC_shared>(
          S));
}

TEST(EnumSetTest, Set) {
  ClauseSet S;
  S.set(Clause::OMPC_private);
  EXPECT_THAT(S, testing::ElementsAre(OMPC_private));

  static_assert(detail::ElementsAre<Clause, Clause::OMPC_private>(
      ClauseSet{}.set(Clause::OMPC_private)));
}

TEST(EnumSetTest, Reset) {
  ClauseSet S{Clause::OMPC_private, Clause::OMPC_shared};
  S.reset(Clause::OMPC_private);
  EXPECT_THAT(S, testing::ElementsAre(OMPC_shared));

  static_assert(detail::ElementsAre<Clause, Clause::OMPC_shared>(
      ClauseSet{Clause::OMPC_private, Clause::OMPC_shared}.reset(
          Clause::OMPC_private)));
}

TEST(EnumSetTest, Flip) {
  ClauseSet S{Clause::OMPC_private};
  S.flip(Clause::OMPC_private);
  S.flip(Clause::OMPC_shared);
  EXPECT_THAT(S, testing::ElementsAre(OMPC_shared));

  static_assert(detail::ElementsAre<Clause, Clause::OMPC_shared>(
      ClauseSet{Clause::OMPC_private}
          .flip(Clause::OMPC_private)
          .flip(Clause::OMPC_shared)));
}

TEST(EnumSetTest, Test) {
  constexpr ClauseSet S{Clause::OMPC_private};
  ASSERT_TRUE(S.test(Clause::OMPC_private));
  ASSERT_FALSE(S.test(Clause::OMPC_shared));

  static_assert(S.test(Clause::OMPC_private));
  static_assert(!S.test(Clause::OMPC_shared));
}

TEST(EnumSetTest, SquareBracket) {
  constexpr ClauseSet S{Clause::OMPC_private};
  ASSERT_TRUE(S[Clause::OMPC_private]);
  ASSERT_FALSE(S[Clause::OMPC_shared]);

  static_assert(S[Clause::OMPC_private]);
  static_assert(!S[Clause::OMPC_shared]);
}

TEST(EnumSetTest, Union) {
  constexpr ClauseSet A{Clause::OMPC_private, OMPC_shared};
  constexpr ClauseSet B{Clause::OMPC_nowait};
  constexpr ClauseSet S = A | B;
  EXPECT_THAT(S, testing::ElementsAre(Clause::OMPC_nowait, Clause::OMPC_private,
                                      OMPC_shared));

  static_assert(detail::ElementsAre<Clause, Clause::OMPC_nowait,
                                    Clause::OMPC_private, OMPC_shared>(S));
}

TEST(EnumSetTest, Intersection) {
  constexpr ClauseSet A{Clause::OMPC_private, OMPC_shared};
  constexpr ClauseSet B{Clause::OMPC_nowait, OMPC_shared};
  constexpr ClauseSet S = A & B;
  EXPECT_THAT(S, testing::ElementsAre(OMPC_shared));

  static_assert(detail::ElementsAre<Clause, OMPC_shared>(S));
}
} // namespace
