//===- PointerFlowFormat.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// JSON serialization helpers for EdgeSet (PointerFlow edge maps).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOWFORMAT_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOWFORMAT_H

#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerFlow/PointerFlow.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"
#include "llvm/ADT/iterator_range.h"

namespace clang::ssaf {

/// Serialize an EdgeSet as an array of arrays of EntityPointerLevels:
///   [ [lhs, rhs, rhs, ...], [lhs, rhs, rhs, ...], ... ]
llvm::json::Array
edgeSetToJSON(const llvm::iterator_range<EdgeSet::const_iterator> &Edges,
              JSONFormat::EntityIdToJSONFn IdToJSON);

/// Deserialize an EdgeSet from the array format produced by `edgeSetToJSON`.
llvm::Expected<EdgeSet>
edgeSetFromJSON(const llvm::json::Array &EdgesData,
                JSONFormat::EntityIdFromJSONFn IdFromJSON);

} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOWFORMAT_H
