//===-- Definition of cpp::monostate ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_MONOSTATE_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_MONOSTATE_H

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

namespace cpp {

struct monostate {};

LIBC_INLINE constexpr bool operator==(monostate, monostate) { return true; }
LIBC_INLINE constexpr bool operator!=(monostate, monostate) { return false; }
LIBC_INLINE constexpr bool operator<(monostate, monostate) { return false; }
LIBC_INLINE constexpr bool operator>(monostate, monostate) { return false; }
LIBC_INLINE constexpr bool operator<=(monostate, monostate) { return true; }
LIBC_INLINE constexpr bool operator>=(monostate, monostate) { return true; }

} // namespace cpp

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_SRC___SUPPORT_CPP_MONOSTATE_H
