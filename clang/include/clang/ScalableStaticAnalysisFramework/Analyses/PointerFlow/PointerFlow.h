//===- PointerFlow.h -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines an analysis that builds directed graphs where nodes
//  are pointers and edges are assignment operations, each of which bridges two
//  nodes.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOW_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOW_H

#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/EntitySummary.h"

namespace clang::ssaf {

/// Maps LHSs to their RHS sets:
using EdgeSet = std::map<EntityPointerLevel, EntityPointerLevelSet>;

class PointerFlowEntitySummary final : public EntitySummary {
  EdgeSet Edges;

  friend class PointerFlowTUSummaryExtractor;
  friend PointerFlowEntitySummary buildPointerFlowEntitySummary(EdgeSet Edges);
  friend llvm::iterator_range<EdgeSet::const_iterator>
  getEdges(const PointerFlowEntitySummary &);

  PointerFlowEntitySummary(EdgeSet Edges)
      : EntitySummary(), Edges(std::move(Edges)) {}

public:
  SummaryName getSummaryName() const override { return summaryName(); };

  bool operator==(const EdgeSet &Other) const { return Edges == Other; }

  bool operator==(const PointerFlowEntitySummary &Other) const {
    return Edges == Other.Edges;
  }

  bool empty() const { return Edges.empty() && Edges.empty(); }

  static SummaryName summaryName() { return SummaryName{"PointerFlow"}; }
};
} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERFLOW_POINTERFLOW_H
