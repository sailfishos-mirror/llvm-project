//===- LoadStoreVec.h -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A pass that vectorizes short store-load chains.
// Unlike generic bundle vectorization, this pass can vectorize instructions
// of different types.
//

#ifndef LLVM_TRANSFORMS_VECTORIZE_SANDBOXVECTORIZER_PASSES_LOADSTOREVEC_H
#define LLVM_TRANSFORMS_VECTORIZE_SANDBOXVECTORIZER_PASSES_LOADSTOREVEC_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/SandboxIR/Pass.h"

namespace llvm {

class DataLayout;

namespace sandboxir {

class Context;
class Value;
class Instruction;
class Scheduler;
class Type;

class LLVM_ABI LoadStoreVec final : public RegionPass {
  const DataLayout *DL = nullptr;
  /// Checks legality of vectorization and \returns the vector type on success,
  /// nullopt otherwise.
  std::optional<Type *> canVectorize(ArrayRef<Instruction *> Bndl,
                                     Scheduler &Sched);

  /// Erases each instruction in \p Bndl -- the store or load bundle just
  /// replaced by a new vector value -- that has no remaining uses, plus each
  /// load in \p MaybeDeadOperands that likewise has no remaining uses (an
  /// operand isn't necessarily single-use, e.g. a packed operand, see
  /// packOperands()). A store in \p Bndl is always dead, since it never
  /// produced a value in the first place; a load in \p Bndl (a top-level
  /// seed load) is only erased once it truly has no uses left, which
  /// vectorizeLoads() guarantees by unpacking every use first. Also cleans
  /// up any pointer-operand GEP that becomes dead as a result.
  void eraseDeadAfterVectorize(ArrayRef<Instruction *> Bndl,
                               ArrayRef<Value *> MaybeDeadOperands);

  /// Builds a single vector load out of the loads in \p Operands. \Returns
  /// the new load, or nullptr if \p Operands are not a vectorizable load
  /// chain. Used only by vectorizeLoads(), for a load-kind seed slice.
  Value *createVectorLoad(ArrayRef<Value *> Operands, Scheduler &Sched,
                          const Analyses &A, Context &Ctx);

  /// Tries to vectorize the store chain \p Bndl into a single vector store.
  /// \Returns whether it succeeded.
  bool vectorizeStores(ArrayRef<Instruction *> Bndl, Region &Rgn,
                       Scheduler &Sched, const Analyses &A);

  /// Tries to vectorize the load chain \p Bndl into a single vector load,
  /// replacing each original load's uses with an extract from it.
  /// \Returns whether it succeeded.
  bool vectorizeLoads(ArrayRef<Instruction *> Bndl, Region &Rgn,
                      Scheduler &Sched, const Analyses &A);

public:
  LoadStoreVec(StringRef AuxArg) : RegionPass("load-store-vec") {
    assert(AuxArg.empty() && "This pass ignores aux arg!");
  }
  bool runOnRegion(Region &Rgn, const Analyses &A) final;
};

} // namespace sandboxir

} // namespace llvm

#endif // LLVM_TRANSFORMS_VECTORIZE_SANDBOXVECTORIZER_PASSES_STRUCTINITVEC_H
