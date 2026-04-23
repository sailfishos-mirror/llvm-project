//===- InstructionDeletionListenerTest.cpp - per-Function listener tests --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/InstructionDeletionListener.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

class TrackingListener : public InstructionDeletionListener {
  DenseSet<const Value *> &UniformValuesRef;

  static void callback(InstructionDeletionListener *Self, Instruction *I) {
    static_cast<TrackingListener *>(Self)->UniformValuesRef.erase(I);
  }

public:
  TrackingListener(Function &F, DenseSet<const Value *> &UniformValues)
      : InstructionDeletionListener(F, &callback),
        UniformValuesRef(UniformValues) {}
};

static Function *createFunction(Module &M, const char *Name) {
  Type *I32Ty = Type::getInt32Ty(M.getContext());
  FunctionType *FTy = FunctionType::get(I32Ty, {I32Ty, I32Ty}, false);
  return Function::Create(FTy, GlobalValue::ExternalLinkage, Name, &M);
}

TEST(InstructionDeletionListenerTest, EraseFromParent) {
  LLVMContext C;
  Module M("test", C);
  Function *F = createFunction(M, "f");
  BasicBlock *BB = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Builder(BB);

  Instruction *Add =
      cast<Instruction>(Builder.CreateAdd(F->getArg(0), F->getArg(1)));
  Builder.CreateRet(F->getArg(0));

  DenseSet<const Value *> UniformValues;
  UniformValues.insert(Add);

  TrackingListener Listener(*F, UniformValues);

  EXPECT_TRUE(UniformValues.contains(Add));
  Add->eraseFromParent();
  EXPECT_FALSE(UniformValues.contains(Add));
}

TEST(InstructionDeletionListenerTest, RemoveAndDelete) {
  LLVMContext C;
  Module M("test", C);
  Function *F = createFunction(M, "f");
  BasicBlock *BB = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Builder(BB);

  Instruction *Add =
      cast<Instruction>(Builder.CreateAdd(F->getArg(0), F->getArg(1)));
  Builder.CreateRet(F->getArg(0));

  DenseSet<const Value *> UniformValues;
  UniformValues.insert(Add);

  TrackingListener Listener(*F, UniformValues);

  EXPECT_TRUE(UniformValues.contains(Add));
  Add->removeFromParent();
  EXPECT_FALSE(UniformValues.contains(Add));
  Add->deleteValue();
}

TEST(InstructionDeletionListenerTest, BasicBlockErasure) {
  LLVMContext C;
  Module M("test", C);
  Function *F = createFunction(M, "f");
  BasicBlock *Entry = BasicBlock::Create(C, "entry", F);
  BasicBlock *Dead = BasicBlock::Create(C, "dead", F);
  IRBuilder<> Builder(Entry);
  Builder.CreateRet(F->getArg(0));

  Builder.SetInsertPoint(Dead);
  Instruction *Add =
      cast<Instruction>(Builder.CreateAdd(F->getArg(0), F->getArg(1)));
  Builder.CreateUnreachable();

  DenseSet<const Value *> UniformValues;
  UniformValues.insert(Add);

  TrackingListener Listener(*F, UniformValues);

  EXPECT_TRUE(UniformValues.contains(Add));
  Dead->eraseFromParent();
  EXPECT_FALSE(UniformValues.contains(Add));
}

TEST(InstructionDeletionListenerTest, ListenerScopeRAII) {
  LLVMContext C;
  Module M("test", C);
  Function *F = createFunction(M, "f");
  BasicBlock *BB = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Builder(BB);
  Builder.CreateRet(F->getArg(0));

  EXPECT_FALSE(F->hasInstructionDeletionListeners());
  {
    DenseSet<const Value *> UniformValues;
    TrackingListener Listener(*F, UniformValues);
    EXPECT_TRUE(F->hasInstructionDeletionListeners());
  }
  EXPECT_FALSE(F->hasInstructionDeletionListeners());
}

TEST(InstructionDeletionListenerTest, MultipleListeners) {
  LLVMContext C;
  Module M("test", C);
  Function *F = createFunction(M, "f");
  BasicBlock *BB = BasicBlock::Create(C, "entry", F);
  IRBuilder<> Builder(BB);

  Instruction *Add =
      cast<Instruction>(Builder.CreateAdd(F->getArg(0), F->getArg(1)));
  Builder.CreateRet(F->getArg(0));

  DenseSet<const Value *> UniformValues1;
  DenseSet<const Value *> UniformValues2;
  UniformValues1.insert(Add);
  UniformValues2.insert(Add);

  TrackingListener L1(*F, UniformValues1);
  TrackingListener L2(*F, UniformValues2);

  EXPECT_TRUE(UniformValues1.contains(Add));
  EXPECT_TRUE(UniformValues2.contains(Add));
  Add->eraseFromParent();
  EXPECT_FALSE(UniformValues1.contains(Add));
  EXPECT_FALSE(UniformValues2.contains(Add));
}

TEST(InstructionDeletionListenerTest, PerFunctionIsolation) {
  LLVMContext C;
  Module M("test", C);
  Function *F1 = createFunction(M, "f1");
  Function *F2 = createFunction(M, "f2");

  BasicBlock *BB1 = BasicBlock::Create(C, "entry", F1);
  BasicBlock *BB2 = BasicBlock::Create(C, "entry", F2);

  IRBuilder<> B1(BB1);
  Instruction *Add1 =
      cast<Instruction>(B1.CreateAdd(F1->getArg(0), F1->getArg(1)));
  B1.CreateRet(F1->getArg(0));

  IRBuilder<> B2(BB2);
  Instruction *Add2 =
      cast<Instruction>(B2.CreateAdd(F2->getArg(0), F2->getArg(1)));
  B2.CreateRet(F2->getArg(0));

  DenseSet<const Value *> UniformValues1;
  DenseSet<const Value *> UniformValues2;
  UniformValues1.insert(Add1);
  UniformValues2.insert(Add2);

  TrackingListener L1(*F1, UniformValues1);
  TrackingListener L2(*F2, UniformValues2);

  // Erasing from F1 should not affect F2's listener.
  Add1->eraseFromParent();
  EXPECT_FALSE(UniformValues1.contains(Add1));
  EXPECT_TRUE(UniformValues2.contains(Add2));

  Add2->eraseFromParent();
  EXPECT_FALSE(UniformValues2.contains(Add2));
}

} // end anonymous namespace
