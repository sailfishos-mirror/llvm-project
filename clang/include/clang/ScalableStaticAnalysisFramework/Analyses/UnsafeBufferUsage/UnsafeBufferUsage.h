//===- UnsafeBufferUsage.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H

#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/SummaryName.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/EntitySummary.h"

namespace clang::ssaf {
/// An UnsafeBufferUsageEntitySummary contains a set of EntityPointerLevels
/// extracted from unsafe buffer pointers contributed by an entity.
class UnsafeBufferUsageEntitySummary final : public EntitySummary {
  const EntityPointerLevelSet UnsafeBuffers;

  friend class UnsafeBufferUsageTUSummaryExtractor;

  UnsafeBufferUsageEntitySummary(EntityPointerLevelSet UnsafeBuffers)
      : EntitySummary(), UnsafeBuffers(std::move(UnsafeBuffers)) {}

public:
  SummaryName getSummaryName() const override { return summaryName(); };

  bool operator==(const EntityPointerLevelSet &Other) const {
    return UnsafeBuffers == Other;
  }

  bool operator==(const UnsafeBufferUsageEntitySummary &Other) const {
    return UnsafeBuffers == Other.UnsafeBuffers;
  }

  bool empty() const { return UnsafeBuffers.empty(); }

  static llvm::json::Object
  jsonSerializeFn(const EntitySummary &ES,
                  JSONFormat::EntityIdToJSONFn EntityId2JSON);

  static llvm::Expected<std::unique_ptr<EntitySummary>>
  jsonDeserializeFn(const llvm::json::Object &Data, EntityIdTable &,
                    JSONFormat::EntityIdFromJSONFn EntityIdFromJSON);

  static SummaryName summaryName() { return SummaryName{"UnsafeBufferUsage"}; }
};

struct UnsafeBufferUsageJSONFormatInfo : JSONFormat::FormatInfo {
  UnsafeBufferUsageJSONFormatInfo()
      : JSONFormat::FormatInfo(
            UnsafeBufferUsageEntitySummary::summaryName(),
            UnsafeBufferUsageEntitySummary::jsonSerializeFn,
            UnsafeBufferUsageEntitySummary::jsonDeserializeFn) {}
};
} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H
