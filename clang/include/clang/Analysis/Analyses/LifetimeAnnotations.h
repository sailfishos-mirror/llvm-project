//===- LifetimeAnnotations.h -  -*--------------- C++--------------------*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Helper functions to inspect and infer lifetime annotations.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEANNOTATIONS_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEANNOTATIONS_H

#include "clang/AST/DeclCXX.h"

namespace clang {
namespace lifetimes {

const FunctionDecl *getDeclWithMergedLifetimeBoundAttrs(const FunctionDecl *FD);

const CXXMethodDecl *
getDeclWithMergedLifetimeBoundAttrs(const CXXMethodDecl *CMD);

// Return true if this is an "normal" assignment operator.
// We assume that a normal assignment operator always returns *this, that is,
// an lvalue reference that is the same type as the implicit object parameter
// (or the LHS for a non-member operator$=).
bool isNormalAssignmentOperator(const FunctionDecl *FD);

bool isAssignmentOperatorLifetimeBound(const CXXMethodDecl *CMD);

bool implicitObjectParamIsLifetimeBound(const FunctionDecl *FD);
} // namespace lifetimes
} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMEANNOTATIONS_H
