//===- UnsafeBufferUsage.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysisFramework/SSAFBuiltinForceLinker.h" // IWYU pragma: keep
#include "llvm/Support/Error.h"

using namespace clang;
using namespace ssaf;

namespace {
struct JSONSerializationAPI {
  using Object = llvm::json::Object;
  using Array = llvm::json::Array;
  using Value = llvm::json::Value;

  JSONFormat::EntityIdToJSONFn EntityIdToFormat;
  JSONFormat::EntityIdFromJSONFn EntityIdFromFormat;
};
} // namespace

// Adapter functions to the current API:
static llvm::json::Object jsonSerializeFn(const EntitySummary &Sum,
                                          JSONFormat::EntityIdToJSONFn Fn) {
  JSONSerializationAPI SA{Fn, {}};
  return UnsafeBufferUsageEntitySummary::serialize(
      SA, *static_cast<const UnsafeBufferUsageEntitySummary *>(&Sum));
}

static llvm::Expected<std::unique_ptr<EntitySummary>>
jsonDeserializeFn(const llvm::json::Object &Data, EntityIdTable &,
                  JSONFormat::EntityIdFromJSONFn Fn) {
  JSONSerializationAPI SA{{}, Fn};
  return UnsafeBufferUsageEntitySummary::deserialize<JSONSerializationAPI>(
      SA, Data);
}

struct UnsafeBufferUsageJSONFormatInfo : JSONFormat::FormatInfo {
  UnsafeBufferUsageJSONFormatInfo()
      : JSONFormat::FormatInfo(UnsafeBufferUsageEntitySummary::summaryName(),
                               jsonSerializeFn, jsonDeserializeFn) {}
};

static llvm::Registry<JSONFormat::FormatInfo>::Add<
    UnsafeBufferUsageJSONFormatInfo>
    RegisterUnsafeBufferUsageJSONFormatInfo(
        "UnsafeBufferUsage",
        "JSON Format info for UnsafeBufferUsageEntitySummary");

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int UnsafeBufferUsageSSAFJSONFormatAnchorSource = 0;
