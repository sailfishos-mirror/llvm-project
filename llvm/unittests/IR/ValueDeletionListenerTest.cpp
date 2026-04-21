//===- ValueDeletionListenerTest.cpp - Tests for ValueDeletionListener ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/ValueDeletionListener.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "gtest/gtest.h"
#include <memory>

using namespace llvm;

namespace {

class TrackingListener : public ValueDeletionListener {
  DenseSet<const Value *> &Tracked;

  static void callback(ValueDeletionListener *Self, const Value *V) {
    static_cast<TrackingListener *>(Self)->Tracked.erase(V);
  }

public:
  TrackingListener(LLVMContext &C, DenseSet<const Value *> &S)
      : ValueDeletionListener(C, &callback), Tracked(S) {}
};

TEST(ValueDeletionListenerTest, BasicDeletion) {
  LLVMContext C;
  Constant *ConstantV = ConstantInt::get(Type::getInt32Ty(C), 0);

  DenseSet<const Value *> Tracked;
  TrackingListener Listener(C, Tracked);

  std::unique_ptr<BitCastInst> V(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  Tracked.insert(V.get());
  EXPECT_TRUE(Tracked.contains(V.get()));

  // Deleting the value triggers the listener, removing it from the set.
  V.reset();
  EXPECT_TRUE(Tracked.empty());
}

TEST(ValueDeletionListenerTest, AddressReuse) {
  LLVMContext C;
  Constant *ConstantV = ConstantInt::get(Type::getInt32Ty(C), 0);

  DenseSet<const Value *> Tracked;
  TrackingListener Listener(C, Tracked);

  // Create, track, and destroy a value.
  const Value *OldAddr;
  {
    std::unique_ptr<BitCastInst> V1(
        new BitCastInst(ConstantV, Type::getInt32Ty(C)));
    OldAddr = V1.get();
    Tracked.insert(OldAddr);
    EXPECT_TRUE(Tracked.contains(OldAddr));
  } // V1 destroyed here — listener removes it from Tracked.
  EXPECT_FALSE(Tracked.contains(OldAddr));

  // Allocate a new instruction. Even if the allocator reuses the same
  // address, the set must not contain it.
  std::unique_ptr<BitCastInst> V2(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  EXPECT_FALSE(Tracked.contains(V2.get()));
}

TEST(ValueDeletionListenerTest, ListenerScopeRAII) {
  LLVMContext C;
  Constant *ConstantV = ConstantInt::get(Type::getInt32Ty(C), 0);

  DenseSet<const Value *> Tracked;

  std::unique_ptr<BitCastInst> V(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  Tracked.insert(V.get());

  {
    TrackingListener Listener(C, Tracked);
    EXPECT_TRUE(Tracked.contains(V.get()));
  }
  // Listener is destroyed (unregistered). Deleting the value now must not
  // crash, but the set won't be updated since no listener is active.
  const Value *Addr = V.get();
  V.reset();
  EXPECT_TRUE(Tracked.contains(Addr));
}

TEST(ValueDeletionListenerTest, MultipleListeners) {
  LLVMContext C;
  Constant *ConstantV = ConstantInt::get(Type::getInt32Ty(C), 0);

  DenseSet<const Value *> Set1, Set2;
  TrackingListener L1(C, Set1);
  TrackingListener L2(C, Set2);

  std::unique_ptr<BitCastInst> V(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  Set1.insert(V.get());
  Set2.insert(V.get());

  V.reset();
  EXPECT_TRUE(Set1.empty());
  EXPECT_TRUE(Set2.empty());
}

TEST(ValueDeletionListenerTest, UnrelatedValueNotAffected) {
  LLVMContext C;
  Constant *ConstantV = ConstantInt::get(Type::getInt32Ty(C), 0);

  DenseSet<const Value *> Tracked;
  TrackingListener Listener(C, Tracked);

  std::unique_ptr<BitCastInst> V1(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  std::unique_ptr<BitCastInst> V2(
      new BitCastInst(ConstantV, Type::getInt32Ty(C)));
  Tracked.insert(V1.get());

  // Deleting V2 (not tracked) should not affect V1 in the set.
  V2.reset();
  EXPECT_TRUE(Tracked.contains(V1.get()));
}

} // namespace
