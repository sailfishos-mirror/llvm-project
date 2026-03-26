//===---------------- PointerAssignments.h ----------------------*- C++ -*-===//
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
#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERASSIGNMENTS_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERASSIGNMENTS_H

#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel.h"

namespace clang::ssaf {

/// Maps LHSs to their RHS sets:
using EdgeSet = std::map<EntityPointerLevel, EntityPointerLevelSet>;

class PointerAssignmentsEntitySummary final : public EntitySummary {
  EdgeSet Edges;

  friend class PointerAssignmentsTUSummaryExtractor;

  PointerAssignmentsEntitySummary(EdgeSet Edges)
      : EntitySummary(), Edges(std::move(Edges)) {}

public:
  SummaryName getSummaryName() const override { return summaryName(); };

  bool operator==(const EdgeSet &Other) const { return Edges == Other; }

  bool operator==(const PointerAssignmentsEntitySummary &Other) const {
    return Edges == Other.Edges;
  }

  bool empty() const { return Edges.empty() && Edges.empty(); }

  static llvm::json::Object
  summaryToJSON(const EntitySummary &ES,
                JSONFormat::EntityIdToJSONFn EntityId2JSON);

  static llvm::Expected<std::unique_ptr<EntitySummary>>
  summaryFromJSON(const llvm::json::Object &Data, EntityIdTable &,
                  JSONFormat::EntityIdFromJSONFn EntityIdFromJSON);

  static SummaryName summaryName() { return SummaryName{"PointerAssignments"}; }
};

struct PointerAssignmentsJSONFormatInfo : JSONFormat::FormatInfo {
  PointerAssignmentsJSONFormatInfo()
      : JSONFormat::FormatInfo(
            PointerAssignmentsEntitySummary::summaryName(),
            PointerAssignmentsEntitySummary::summaryToJSON,
            PointerAssignmentsEntitySummary::summaryFromJSON) {}
};

} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_POINTERASSIGNMENTS_H