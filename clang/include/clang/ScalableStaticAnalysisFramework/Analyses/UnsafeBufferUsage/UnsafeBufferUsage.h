//===- UnsafeBufferUsage.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H

#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/SummaryName.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/EntitySummary.h"
#include "llvm/ADT/StringRef.h"
#include <set>

namespace clang::ssaf {

/// An EntityPointerLevel represents a level of the declared pointer/array
/// type of an entity.  In the fully-expanded spelling of the declared type, a
/// EntityPointerLevel is associated with a '*' (or a '[]`) in that declaration.
///
/// For example, for 'int *p[10];', there are two EntityPointerLevels. One
/// is associated with 'int *[10]' of 'p' and the other is associated with 'int
/// *' of 'p'.
///
/// An EntityPointerLevel can be identified by an EntityId and an unsigned
/// integer indicating the pointer level: '(EntityId, PointerLevel)'.
/// An EntityPointerLevel 'P' is valid iff 'P.EntityId' has a pointer type with
/// at least 'P.PointerLevel' levels (This implies 'P.PointerLevel > 0').
///
/// For the same example 'int *p[10];', the EntityPointerLevels below are valid:
/// - '(p, 2)' is associated with the 'int *' part of the declared type of 'p';
/// - '(p, 1)' is associated with the 'int *[10]' part of the declared type of
/// 'p'.
class EntityPointerLevel {
  EntityId Entity;
  unsigned PointerLevel;

  friend class UnsafeBufferUsageEntitySummary;
  friend class UnsafeBufferUsageTUSummaryExtractor;

  EntityPointerLevel(EntityId Entity, unsigned PointerLevel)
      : Entity(Entity), PointerLevel(PointerLevel) {}

public:
  EntityId getEntity() const { return Entity; }
  unsigned getPointerLevel() const { return PointerLevel; }

  bool operator==(const EntityPointerLevel &Other) const {
    return std::tie(Entity, PointerLevel) ==
           std::tie(Other.Entity, Other.PointerLevel);
  }

  bool operator!=(const EntityPointerLevel &Other) const {
    return !(*this == Other);
  }

  bool operator<(const EntityPointerLevel &Other) const {
    return std::tie(Entity, PointerLevel) <
           std::tie(Other.Entity, Other.PointerLevel);
  }

  /// Compares `EntityPointerLevel`s; additionally, partially compares
  /// `EntityPointerLevel` with `EntityId`.
  struct Comparator {
    using is_transparent = void;
    bool operator()(const EntityPointerLevel &L,
                    const EntityPointerLevel &R) const {
      return L < R;
    }
    bool operator()(const EntityId &L, const EntityPointerLevel &R) const {
      return L < R.getEntity();
    }
    bool operator()(const EntityPointerLevel &L, const EntityId &R) const {
      return L.getEntity() < R;
    }
  };
};

using EntityPointerLevelSet =
    std::set<EntityPointerLevel, EntityPointerLevel::Comparator>;

/// An UnsafeBufferUsageEntitySummary is an immutable set of unsafe buffers, in
/// the form of EntityPointerLevel.
class UnsafeBufferUsageEntitySummary final : public EntitySummary {
  const EntityPointerLevelSet UnsafeBuffers;

  friend class UnsafeBufferUsageTUSummaryExtractor;

  UnsafeBufferUsageEntitySummary(EntityPointerLevelSet UnsafeBuffers)
      : EntitySummary(), UnsafeBuffers(std::move(UnsafeBuffers)) {}

  static constexpr llvm::StringLiteral SummarySerializationKey =
      "UnsafeBuffers";

public:
  SummaryName getSummaryName() const override { return summaryName(); };

  bool operator==(const EntityPointerLevelSet &Other) const {
    return UnsafeBuffers == Other;
  }

  bool operator==(const UnsafeBufferUsageEntitySummary &Other) const {
    return UnsafeBuffers == Other.UnsafeBuffers;
  }

  bool empty() const { return UnsafeBuffers.empty(); }

  static SummaryName summaryName() { return SummaryName{"UnsafeBufferUsage"}; }

  // A SerializationAPI implementation for a specific Format needs to implement:
  //
  // 1) Abstract data types, Object (map), Array, Value, ..., etc. with
  //   supported operations, and
  // 2) EntityIdToFormatFn, and
  // 3) EntityIdFromFotmatFn.

  // Format-independent (de-)serialization functions:
  template <typename SerializationAPI>
  typename SerializationAPI::Object static serialize(
      SerializationAPI &SA, const UnsafeBufferUsageEntitySummary &S) {
    using Array = typename SerializationAPI::Array;
    using Object = typename SerializationAPI::Object;
    Array UnsafeBuffersData;

    UnsafeBuffersData.reserve(S.UnsafeBuffers.size());
    for (const auto &EPL : S.UnsafeBuffers)
      UnsafeBuffersData.push_back(
          Array{SA.EntityIdToFormat(EPL.getEntity()), EPL.getPointerLevel()});
    return Object{{SummarySerializationKey.data(), std::move(UnsafeBuffersData)}};
  }

  template <typename SerializationAPI>
  llvm::Expected<std::unique_ptr<EntitySummary>> static deserialize(
      SerializationAPI &SA, const typename SerializationAPI::Object &Data) {
    using Array = typename SerializationAPI::Array;
    using Object = typename SerializationAPI::Object;
    const Array *UnsafeBuffersData = Data.getArray(SummarySerializationKey.data());

    if (!UnsafeBuffersData)
      return llvm::createStringError("expected a json::Object with a key %s",
                                     SummarySerializationKey.data());

    EntityPointerLevelSet EPLs;

    for (auto &EltData : *UnsafeBuffersData) {
      const Array *EltDataAsArr = EltData.getAsArray();

      if (!EltDataAsArr || EltDataAsArr->size() != 2)
        return llvm::createStringError("expected a json::Array of size 2");

      const Object *IdData = (*EltDataAsArr)[0].getAsObject();
      std::optional<uint64_t> PtrLvData = (*EltDataAsArr)[1].getAsInteger();

      if (!IdData || !PtrLvData)
        return llvm::createStringError(
            "expected a json::Value of integer type");

      llvm::Expected<EntityId> Id = SA.EntityIdFromFormat(*IdData);

      if (!Id)
        return Id.takeError();
      EPLs.insert(EntityPointerLevel(Id.get(), *PtrLvData));
    }
    return std::make_unique<UnsafeBufferUsageEntitySummary>(
        UnsafeBufferUsageEntitySummary(std::move(EPLs)));
  }
};
} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_UNSAFEBUFFERUSAGE_UNSAFEBUFFERUSAGE_H
