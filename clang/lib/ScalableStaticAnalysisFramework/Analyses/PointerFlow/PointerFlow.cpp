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
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "llvm/ADT/STLExtras.h"
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

// Writes the 'Edges' map as an array of array of EntityPointerLevels:
// Array [
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    ...
// ]
static llvm::json::Object
summaryToJSON(const EntitySummary &ES,
              JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  Array EdgesData;

  for (const auto &Entry :
       getEdges(static_cast<const PointerFlowEntitySummary &>(ES))) {
    Array EdgesEntryData;
    EntityPointerLevel LHS = Entry.first;

    EdgesEntryData.push_back(entityPointerLevelToJSON(LHS, EntityId2JSON));
    // Add to nodes:
    for (const auto &RHS : Entry.second)
      EdgesEntryData.push_back(entityPointerLevelToJSON(RHS, EntityId2JSON));
    EdgesData.push_back(Value(std::move(EdgesEntryData)));
  }

  Object Data;

  Data[PointerFlowKey] = Value(std::move(EdgesData));
  return Data;
}

static llvm::Expected<std::unique_ptr<EntitySummary>>
summaryFromJSON(const Object &Data, EntityIdTable &,
                JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  const Value *EdgesData = Data.get(PointerFlowKey);

  if (!EdgesData)
    return makeSawButExpectedError(
        Object(Data), "a JSON object with the key: %s", PointerFlowKey);

  EdgeSet Edges;
  const auto *EdgesDataAsArr = EdgesData->getAsArray();

  if (!EdgesDataAsArr)
    return makeSawButExpectedError(
        *EdgesData, "a JSON array of arary of EntityPointerLevels");
  for (const auto &EdgesEntryData : *EdgesDataAsArr) {
    const auto *EPLArray = EdgesEntryData.getAsArray();

    if (!EPLArray || EPLArray->size() <= 1)
      return makeSawButExpectedError(
          EdgesEntryData, "a JSON array of EntityPointerLevels with a size "
                          "greater than 1: [lhs, rhs, rhs, ...]");

    llvm::Error Err = llvm::Error::success();
    auto EPLs = llvm::map_range(
        *EPLArray, [&Err, &EntityIdFromJSON](const auto &EPLData) {
          auto EPL = entityPointerLevelFromJSON(EPLData, EntityIdFromJSON);

          if (!EPL)
            Err = llvm::joinErrors(std::move(Err), EPL.takeError());
          return *EPL;
        });

    if (Err)
      return Err;
    Edges[*EPLs.begin()].insert(EPLs.begin() + 1, EPLs.end());
  }
  return std::make_unique<PointerFlowEntitySummary>(
      buildPointerFlowEntitySummary(std::move(Edges)));
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