//===-- include/flang/Support/OpenMP-features.h -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef FORTRAN_SUPPORT_OPENMP_FEATURES_H_
#define FORTRAN_SUPPORT_OPENMP_FEATURES_H_

namespace Fortran::common {

/// Set _OPENMP macro according to given version number
template <typename FortranPredefinitions>
void setOpenMPMacro(int version, FortranPredefinitions &predefinitions) {
  switch (version) {
  case 31:
  default:
    predefinitions.emplace_back("_OPENMP", "201107");
    break;
  case 40:
    predefinitions.emplace_back("_OPENMP", "201307");
    break;
  case 45:
    predefinitions.emplace_back("_OPENMP", "201511");
    break;
  case 50:
    predefinitions.emplace_back("_OPENMP", "201811");
    break;
  case 51:
    predefinitions.emplace_back("_OPENMP", "202011");
    break;
  case 52:
    predefinitions.emplace_back("_OPENMP", "202111");
    break;
  case 60:
    predefinitions.emplace_back("_OPENMP", "202411");
    break;
  }
}
} // namespace Fortran::common
#endif // FORTRAN_SUPPORT_OPENMP_FEATURES_H_
