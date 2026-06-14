//===- VecUtils.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Vectorize/SandboxVectorizer/VecUtils.h"

#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/SandboxIR/Instruction.h"
#include "llvm/Transforms/Vectorize/SandboxVectorizer/InstrMaps.h"

namespace llvm::sandboxir {

SmallVector<BundleTy> VecUtils::getNextUserBundles(ArrayRef<Value *> Bndl,
                                                   const InstrMaps &IMaps) {
  SmallVector<BundleTy> Bundles;
  if (Bndl.empty())
    return Bundles;

  // Collect the operand indices at which \p U uses \p V. Operands are scanned
  // in ascending order, so the result is sorted.
  auto GetOpIdxVec = [](Value *V, User *U) -> SmallVector<unsigned, 2> {
    SmallVector<unsigned, 2> OpIdxVec;
    for (unsigned Idx : seq<unsigned>(U->getNumOperands()))
      if (U->getOperand(Idx) == V)
        OpIdxVec.push_back(Idx);
    return OpIdxVec;
  };

  Value *V0 = Bndl[0];
  DenseSet<User *> SeenUsers;
  // For each user U0 of lane 0, try to form a bundle of matching users across
  // all lanes.
  for (User *U0 : V0->users()) {
    if (!SeenUsers.insert(U0).second)
      continue;
    auto *UI0 = dyn_cast<Instruction>(U0);
    if (!UI0 || IMaps.isVectorized(UI0))
      continue;

    // The operand indices at which lane 0's user U0 uses lane 0's value V0.
    // Every other lane's user must use its lane value at the exact same operand
    // indices; otherwise the widened user's operands can't be grouped
    // consistently (each vector operand lane must come from the same position).
    SmallVector<unsigned, 2> OpIdxVec0 = GetOpIdxVec(V0, UI0);
    assert(!OpIdxVec0.empty() && "U0 does not use V0!");

    // Find a distinct matching user for each of the remaining lanes.
    BundleTy NextUserBndl;
    NextUserBndl.push_back(UI0);
    SmallPtrSet<Instruction *, 4> Claimed;
    Claimed.insert(UI0);
    for (Value *V : drop_begin(Bndl)) {
      Instruction *Match = nullptr;
      for (User *U : V->users()) {
        auto *UI = dyn_cast<Instruction>(U);
        if (!UI || IMaps.isVectorized(UI) || Claimed.contains(UI))
          continue;
        if (UI->getOpcode() != UI0->getOpcode() ||
            UI->getType() != UI0->getType())
          continue;
        if (UI->getParent() != UI0->getParent())
          continue;

        // Require the same operand-usage pattern as lane 0 (same indices, in
        // order). This rejects both operand-index mismatches and cases where V
        // is used a different number of times than V0 is in U0.
        if (GetOpIdxVec(V, UI) != OpIdxVec0)
          continue;

        Match = UI;
        break;
      }
      if (!Match) {
        NextUserBndl.clear();
        break;
      }
      Claimed.insert(Match);
      NextUserBndl.push_back(Match);
    }

    if (NextUserBndl.size() == Bndl.size())
      Bundles.emplace_back(std::move(NextUserBndl));
  }
  return Bundles;
}

unsigned VecUtils::getFloorPowerOf2(unsigned Num) {
  if (Num == 0)
    return Num;
  unsigned Mask = Num;
  Mask >>= 1;
  for (unsigned ShiftBy = 1; ShiftBy < sizeof(Num) * 8; ShiftBy <<= 1)
    Mask |= Mask >> ShiftBy;
  return Num & ~Mask;
}

#ifndef NDEBUG
template <typename T> static void dumpImpl(ArrayRef<T *> Bndl) {
  for (auto [Idx, V] : enumerate(Bndl))
    dbgs() << Idx << "." << *V << "\n";
}
void VecUtils::dump(ArrayRef<Value *> Bndl) { dumpImpl(Bndl); }
void VecUtils::dump(ArrayRef<Instruction *> Bndl) { dumpImpl(Bndl); }
#endif // NDEBUG

} // namespace llvm::sandboxir
