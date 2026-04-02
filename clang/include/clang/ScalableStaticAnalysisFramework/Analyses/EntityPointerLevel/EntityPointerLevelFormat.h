//===- EntityPointerLevelFormat.h -------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_ENTITYPOINTERLEVEL_ENTITYPOINTERLEVELFORMAT_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_ENTITYPOINTERLEVEL_ENTITYPOINTERLEVELFORMAT_H
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"

template <typename... Ts>
static inline llvm::Error makeSawButExpectedError(const llvm::json::Value &Saw,
                                                  llvm::StringRef Expected,
                                                  const Ts &...ExpectedArgs) {
  std::string Fmt = ("saw %s but expected " + Expected).str();
  std::string SawStr = llvm::formatv("{0:2}", Saw).str();

  return llvm::createStringError(Fmt.c_str(), SawStr.c_str(), ExpectedArgs...);
}

namespace clang::ssaf {
// Writes an EntityPointerLevel as
// Array [
//   Object { "@" : [entity-id]},
//   [pointer-level-integer]
// ]
static inline llvm::json::Value
entityPointerLevelToJSON(const EntityPointerLevel &EPL,
                         JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  return llvm::json::Array{EntityId2JSON(EPL.getEntity()),
                           llvm::json::Value(EPL.getPointerLevel())};
}

Expected<EntityPointerLevel> static inline entityPointerLevelFromJSON(
    const llvm::json::Value &EPLData,
    JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  auto *AsArr = EPLData.getAsArray();

  if (!AsArr || AsArr->size() != 2)
    return makeSawButExpectedError(
        EPLData, "an array with exactly two elements representing "
                 "EntityId and PointerLevel, respectively");

  auto *EntityIdObj = (*AsArr)[0].getAsObject();

  if (!EntityIdObj)
    return makeSawButExpectedError((*AsArr)[0],
                                   "an object representing EntityId");

  Expected<EntityId> Id = EntityIdFromJSON(*EntityIdObj);

  if (!Id)
    return Id.takeError();

  std::optional<uint64_t> PtrLv = (*AsArr)[1].getAsInteger();

  if (!PtrLv)
    return makeSawButExpectedError((*AsArr)[1],
                                   "an integer representing PointerLevel");

  return buildEntityPointerLevel(*Id, *PtrLv);
}
} // namespace clang::ssaf
#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_ENTITYPOINTERLEVEL_ENTITYPOINTERLEVELFORMAT_H