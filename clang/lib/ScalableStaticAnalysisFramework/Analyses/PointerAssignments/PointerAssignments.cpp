//===---------- PointerAssignments.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerAssignments.h"
#include "SSAFAnalysesCommon.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/SSAFForceLinker.h" // IWYU pragma: keep
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"

namespace {
constexpr const char *const PointerAssignmentsKey = "PointerAssignments";
} // namespace

namespace clang::ssaf {
using Object = llvm::json::Object;
using Array = llvm::json::Array;
using Value = llvm::json::Value;

// Writes the 'Edges' map as an array of array of EntityPointerLevels:
// Array [
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    Array [ [lhs-node], [rhs-node], [rhs-node], ...]
//    ...
// ]
llvm::json::Object PointerAssignmentsEntitySummary::summaryToJSON(
    const EntitySummary &ES, JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  Array EdgesData;

  for (const auto &Entry :
       static_cast<const PointerAssignmentsEntitySummary &>(ES).Edges) {
    Array EdgesEntryData;
    EntityPointerLevel LHS = Entry.first;

    EdgesEntryData.push_back(entityPointerLevelToJSON(LHS, EntityId2JSON));
    // Add to nodes:
    for (const auto &RHS : Entry.second)
      EdgesEntryData.push_back(entityPointerLevelToJSON(RHS, EntityId2JSON));
    EdgesData.push_back(Value(std::move(EdgesEntryData)));
  }

  Object Data;

  Data[PointerAssignmentsKey] = Value(std::move(EdgesData));
  return Data;
}

llvm::Expected<std::unique_ptr<EntitySummary>>
PointerAssignmentsEntitySummary::summaryFromJSON(
    const Object &Data, EntityIdTable &,
    JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  const Value *EdgesData = Data.get(PointerAssignmentsKey);

  if (!EdgesData)
    return makeErrorSawButExpected(
        Object(Data), "a JSON object with the key: %s", PointerAssignmentsKey);

  EdgeSet Edges;
  const auto *EdgesDataAsArr = EdgesData->getAsArray();

  if (!EdgesDataAsArr)
    return makeErrorSawButExpected(
        *EdgesData, "a JSON array of arary of EntityPointerLevels");
  for (const auto &EdgesEntryData : *EdgesDataAsArr) {
    const auto *EPLArray = EdgesEntryData.getAsArray();

    if (!EPLArray || EPLArray->size() <= 1)
      return makeErrorSawButExpected(
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
  return std::make_unique<PointerAssignmentsEntitySummary>(
      PointerAssignmentsEntitySummary(std::move(Edges)));
}

static llvm::Registry<JSONFormat::FormatInfo>::Add<
    PointerAssignmentsJSONFormatInfo>
    RegisterPointerAssignmentsJSONFormatInfo(
        "PointerAssignments",
        "JSON Format info for PointerAssignmentsEntitySummary");

} // namespace clang::ssaf

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int PointerAssignmentsSSAFJSONFormatAnchorSource = 0;