//===---------- UnsafeBufferUsage.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/SSAFForceLinker.h" // IWYU pragma: keep

namespace {
constexpr const char *const UnsafeBuffersKey = "UnsafeBuffers";
template <typename... Ts>
static inline llvm::Error makeErrorSawButExpected(const llvm::json::Value &Saw,
                                                  llvm::StringRef Expected,
                                                  const Ts &...ExpectedArgs) {
  return llvm::createStringError(
      ("saw %s but expected " + Expected).str().c_str(),
      llvm::formatv("{0:2}", Saw).str().data(), Expected.data(),
      ExpectedArgs...);
}
} // namespace

namespace clang::ssaf {
using Object = llvm::json::Object;
using Array = llvm::json::Array;
using Value = llvm::json::Value;

// Writes the summary into an array of EntityPointerLevels:
llvm::json::Object UnsafeBufferUsageEntitySummary::jsonSerializeFn(
    const EntitySummary &ES, JSONFormat::EntityIdToJSONFn EntityId2JSON) {
  Array UnsafeBuffersData;

  for (const auto &EPL :
       static_cast<const UnsafeBufferUsageEntitySummary &>(ES).UnsafeBuffers)
    UnsafeBuffersData.push_back(entityPointerLevelToJSON(EPL, EntityId2JSON));

  Object Data;

  Data[UnsafeBuffersKey] = Value(std::move(UnsafeBuffersData));
  return Data;
}

llvm::Expected<std::unique_ptr<EntitySummary>>
UnsafeBufferUsageEntitySummary::jsonDeserializeFn(
    const llvm::json::Object &Data, EntityIdTable &,
    JSONFormat::EntityIdFromJSONFn EntityIdFromJSON) {
  const Value *UnsafeBuffersData = Data.get(UnsafeBuffersKey);

  if (!UnsafeBuffersData)
    return makeErrorSawButExpected(
        Object(Data), "a JSON object with the key: %s", UnsafeBuffersKey);

  const auto *AsArr = UnsafeBuffersData->getAsArray();

  if (!AsArr)
    return makeErrorSawButExpected(*UnsafeBuffersData,
                                   "a JSON array of EntityPointerLevels");

  EntityPointerLevelSet UnsafeBuffers;

  for (auto &UnsafeBufferData : *AsArr) {
    auto EPL = entityPointerLevelFromJSON(UnsafeBufferData, EntityIdFromJSON);

    if (!EPL)
      return EPL.takeError();
    UnsafeBuffers.insert(*EPL);
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
