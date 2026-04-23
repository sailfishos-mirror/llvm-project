//===- llvm/IR/InstructionDeletionListener.h - Instruction removal hook
//----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares InstructionDeletionListener, an interface that allows
// analyses to be notified when an Instruction is removed from a Function.
// Think of an analysis that uses this as "a value handle to many values" —
// a single per-function registration replaces one handle per tracked value.
//
// Registration and deregistration happen automatically via RAII: the
// constructor registers with a Function, and the destructor deregisters.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_INSTRUCTIONDELETIONLISTENER_H
#define LLVM_IR_INSTRUCTIONDELETIONLISTENER_H

#include "llvm/Support/Compiler.h"

namespace llvm {

class Function;
class Instruction;

/// A per-function listener notified when an Instruction is removed from its
/// parent BasicBlock.
///
/// Subclasses implement the callback by passing a static function to the
/// constructor, which static_casts the listener back to the derived type.
/// This avoids virtual dispatch overhead while preserving type safety.
///
/// Lifetime is managed via RAII: the constructor registers with the
/// Function, and the destructor deregisters.
class InstructionDeletionListener {
public:
  using CallbackT = void (*)(InstructionDeletionListener *, Instruction *);

private:
  Function &F;
  CallbackT Callback;

public:
  LLVM_ABI InstructionDeletionListener(Function &F, CallbackT CB);
  LLVM_ABI ~InstructionDeletionListener();

  InstructionDeletionListener(const InstructionDeletionListener &) = delete;
  InstructionDeletionListener &
  operator=(const InstructionDeletionListener &) = delete;

  void instructionRemoved(Instruction *I) { Callback(this, I); }
  Function &getFunction() const { return F; }
};

} // namespace llvm

#endif // LLVM_IR_INSTRUCTIONDELETIONLISTENER_H
