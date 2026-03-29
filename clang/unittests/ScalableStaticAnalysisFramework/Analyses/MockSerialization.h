//===- MockSerialization.h ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  A mock implementation of SerializationAPI for format-independent testing of
//  summary serialization
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_UNITTESTS_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_MOCKSERIALIZATION_H
#define LLVM_CLANG_UNITTESTS_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_MOCKSERIALIZATION_H

#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <initializer_list>
#include <map>
#include <optional>
#include <variant>
#include <vector>

using namespace llvm;
using namespace clang;
using namespace ssaf;

struct MockSerializationAPI {
  struct Value;
  using Array = std::vector<Value>;

  struct Object : public std::map<StringRef, Value> {
    Object(std::initializer_list<Object::value_type> IL)
        : std::map<StringRef, Value>(std::move(IL)) {};

    const Array *getArray(StringRef Key) const {
      auto I = find(Key);
      if (I == end())
        return nullptr;
      return I->second.getAsArray();
    }
  };

  struct Value {
  private:
    std::variant<Object, Array, uint64_t> Data;

  public:
    Value(Object Data) : Data(Data) {}
    Value(Array Data) : Data(Data) {}
    Value(uint64_t Data) : Data(Data) {}

    const Array *getAsArray() const { return std::get_if<Array>(&Data); };
    const Object *getAsObject() const { return std::get_if<Object>(&Data); };
    std::optional<uint64_t> getAsInteger() const {
      if (auto *Int = std::get_if<uint64_t>(&Data))
        return *Int;
      return std::nullopt;
    };

    bool operator==(const Value &Other) const {
      if (std::holds_alternative<Object>(Data))
        return Other.getAsObject() &&
               std::equal_to<std::map<StringRef, Value>>()(
                   *getAsObject(), *Other.getAsObject());
      if (std::holds_alternative<Array>(Data))
        return Other.getAsArray() && *getAsArray() == *Other.getAsArray();
      if (std::holds_alternative<uint64_t>(Data))
        return Other.getAsInteger() && *getAsInteger() == *Other.getAsInteger();
      return false;
    }
  };

  Object EntityIdToFormat(EntityId Id) {
    auto Iter = llvm::find(IDs, Id);

    if (Iter == IDs.end())
      Iter = IDs.insert(IDs.end(), Id);
    return Object{{"@", uint64_t(Iter - IDs.begin())}};
  }

  llvm::Expected<EntityId> EntityIdFromFormat(const Object &Data) {
    if (Data.count("@"))
      if (auto Idx = Data.at("@").getAsInteger())
        return IDs[*Idx];
    return llvm::createStringError("failed to convert Data to EntityId");
  }

private:
  std::vector<EntityId> IDs;
};
#endif // LLVM_CLANG_UNITTESTS_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_MOCKSERIALIZATION_H
