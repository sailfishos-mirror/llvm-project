//===- PointerFlow.cpp ---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerFlow/PointerFlow.h"
#include "SSAFAnalysesCommon.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel/EntityPointerLevelFormat.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerFlow/PointerFlowFormat.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"

using namespace clang;
using namespace ssaf;
using Object = llvm::json::Object;
using Array = llvm::json::Array;
using Value = llvm::json::Value;

namespace {
constexpr const char *const PointerFlowKey = "PointerFlow";
} // namespace

ssaf::PointerFlowEntitySummary
ssaf::buildPointerFlowEntitySummary(EdgeSet Edges) {
  return PointerFlowEntitySummary(std::move(Edges));
}

llvm::iterator_range<EdgeSet::const_iterator>
ssaf::getEdges(const PointerFlowEntitySummary &Sum) {
  return Sum.Edges;
}

Array ssaf::edgeSetToJSON(
    const llvm::iterator_range<EdgeSet::const_iterator> &Edges,
    JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  Array EdgesData;

  for (const auto &[LHS, RHSSet] : Edges) {
    Array EdgeEntry;
    EdgeEntry.push_back(entityPointerLevelToJSON(LHS, EntityId2JSON));
    for (const auto &RHS : RHSSet)
      EdgeEntry.push_back(entityPointerLevelToJSON(RHS, EntityId2JSON));
    EdgesData.push_back(Value(std::move(EdgeEntry)));
  }
  return EdgesData;
}

llvm::Expected<EdgeSet>
ssaf::edgeSetFromJSON(const Array &EdgesData,
                      JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  EdgeSet Edges;

  for (const auto &EdgesEntryData : EdgesData) {
    const auto *EPLArray = EdgesEntryData.getAsArray();

    if (!EPLArray || EPLArray->size() <= 1)
      return makeSawButExpectedError(
          EdgesEntryData, "a JSON array of EntityPointerLevels with a size "
                          "greater than 1: [lhs, rhs, rhs, ...]");

    auto SrcEPL =
        entityPointerLevelFromJSON(*EPLArray->begin(), EntityIdFromJSON);

    if (!SrcEPL)
      return SrcEPL.takeError();
    for (const auto &EPLData :
         llvm::make_range(EPLArray->begin() + 1, EPLArray->end())) {
      auto EPL = entityPointerLevelFromJSON(EPLData, EntityIdFromJSON);
      if (!EPL)
        return EPL.takeError();
      Edges[*SrcEPL].insert(*EPL);
    }
  }
  return Edges;
}

// Writes the 'Edges' map as an array of array of EntityPointerLevels:
// Array [
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    ...
// ]
static llvm::json::Object
summaryToJSON(const EntitySummary &ES,
              JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  Object Data;
  Data[PointerFlowKey] = Value(
      edgeSetToJSON(getEdges(static_cast<const PointerFlowEntitySummary &>(ES)),
                    EntityId2JSON));
  return Data;
}

static llvm::Expected<std::unique_ptr<EntitySummary>>
summaryFromJSON(const Object &Data, EntityIdTable &,
                JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  const Value *EdgesData = Data.get(PointerFlowKey);

  if (!EdgesData)
    return makeSawButExpectedError(Data, "a JSON object with the key: %s",
                                   PointerFlowKey);

  const auto *EdgesDataAsArr = EdgesData->getAsArray();

  if (!EdgesDataAsArr)
    return makeSawButExpectedError(
        *EdgesData, "a JSON array of array of EntityPointerLevels");

  auto Edges = edgeSetFromJSON(*EdgesDataAsArr, EntityIdFromJSON);

  if (!Edges)
    return Edges.takeError();
  return std::make_unique<PointerFlowEntitySummary>(
      buildPointerFlowEntitySummary(std::move(*Edges)));
}

struct PointerFlowJSONFormatInfo : JSONFormat::FormatInfo {
  PointerFlowJSONFormatInfo()
      : JSONFormat::FormatInfo(PointerFlowEntitySummary::summaryName(),
                               summaryToJSON, summaryFromJSON) {}
};

static llvm::Registry<JSONFormat::FormatInfo>::Add<PointerFlowJSONFormatInfo>
    RegisterPointerFlowJSONFormatInfo(
        "PointerFlow", "JSON Format info for PointerFlowEntitySummary");

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int PointerFlowSSAFJSONFormatAnchorSource = 0;
