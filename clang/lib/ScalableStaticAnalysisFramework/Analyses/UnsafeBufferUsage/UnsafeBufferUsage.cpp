//===---------- UnsafeBufferUsage.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/SSAFForceLinker.h" // IWYU pragma: keep

namespace {
constexpr const char *const UnsafeBuffersKey = "UnsafeBuffers";
} // namespace

namespace clang::ssaf {
using Object = llvm::json::Object;
using Array = llvm::json::Array;
using Value = llvm::json::Value;

llvm::json::Object UnsafeBufferUsageEntitySummary::jsonSerializeFn(
    const EntitySummary &ES, JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  // Writes a EntityPointerLevel as
  // Array {
  //   Object {
  //     "@" : [entity-id]
  //   },
  //   [pointer-level]
  // }
  Array UnsafeBuffersData;

  for (const auto &EPL :
       static_cast<const UnsafeBufferUsageEntitySummary &>(ES).UnsafeBuffers)
    UnsafeBuffersData.push_back(
        Value(Array{// EntityId:
                    Value(EntityId2JSON(EPL.getEntity())),
                    // PointerLevel:
                    Value(EPL.getPointerLevel())}));

  Object Data;

  Data[UnsafeBuffersKey] = Value(std::move(UnsafeBuffersData));
  return Data;
}

llvm::Expected<std::unique_ptr<EntitySummary>>
UnsafeBufferUsageEntitySummary::jsonDeserializeFn(
    const llvm::json::Object &Data, EntityIdTable &,
    JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  const Array *UnsafeBuffersData = Data.getArray(UnsafeBuffersKey);
  constexpr const char *const ErrMsg = "unrecognized UnsafeBufferUsageEntitySummary data";

  if (!UnsafeBuffersData)
    return llvm::createStringError(ErrMsg);

  EntityPointerLevelSet UnsafeBuffers;

  for (auto &EltData : *UnsafeBuffersData) {
    const Array *EltDataAsArr = EltData.getAsArray();

    if (!EltDataAsArr || EltDataAsArr->size() != 2)
      return llvm::createStringError(ErrMsg);

    const Object *IdData = (*EltDataAsArr)[0].getAsObject();
    std::optional<uint64_t> PtrLvData = (*EltDataAsArr)[1].getAsInteger();

    if (!IdData || !PtrLvData)
      return llvm::createStringError(ErrMsg);

    llvm::Expected<EntityId> Id = EntityIdFromJSON(*IdData);

    if (!Id)
      return Id.takeError();
    UnsafeBuffers.insert(EntityPointerLevel(Id.get(), *PtrLvData));
  }
  return std::make_unique<UnsafeBufferUsageEntitySummary>(
      UnsafeBufferUsageEntitySummary(std::move(UnsafeBuffers)));
}

static llvm::Registry<JSONFormat::FormatInfo>::Add<
    UnsafeBufferUsageJSONFormatInfo>
    RegisterUnsafeBufferUsageJSONFormatInfo(
        "UnsafeBufferUsage",
        "JSON Format info for UnsafeBufferUsageEntitySummary");

} // namespace clang::ssaf

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int UnsafeBufferUsageSSAFJSONFormatAnchorSource = 0;
