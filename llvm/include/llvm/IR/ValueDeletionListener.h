//===- llvm/IR/ValueDeletionListener.h - Callback on Value deletion -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares ValueDeletionListener, an interface that allows analyses
// to be notified when any Value in a given LLVMContext is deleted. Think of
// an analysis that uses this as "a value handle to many values" — a single
// registration replaces one handle per tracked value.
//
// Registration and deregistration happen automatically via RAII: the
// constructor registers with an LLVMContext, and the destructor deregisters.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_VALUEDELETIONLISTENER_H
#define LLVM_IR_VALUEDELETIONLISTENER_H

#include "llvm/Support/Compiler.h"

namespace llvm {

class LLVMContext;
class Value;

/// A listener that is notified whenever a Value is deleted in its LLVMContext.
///
/// Subclasses implement the callback by passing a static function to the
/// constructor, which static_casts the listener back to the derived type.
/// This avoids virtual dispatch overhead while preserving type safety at
/// the point of registration.
///
/// Lifetime is managed via RAII: the constructor registers with the
/// LLVMContext, and the destructor deregisters.
class ValueDeletionListener {
public:
  using CallbackT = void (*)(ValueDeletionListener *, const Value *);

private:
  LLVMContext &Ctx;
  CallbackT Callback;

public:
  LLVM_ABI ValueDeletionListener(LLVMContext &C, CallbackT CB);
  LLVM_ABI ~ValueDeletionListener();

  ValueDeletionListener(const ValueDeletionListener &) = delete;
  ValueDeletionListener &operator=(const ValueDeletionListener &) = delete;

  void valueDeleted(const Value *V) { Callback(this, V); }
};

} // namespace llvm

#endif // LLVM_IR_VALUEDELETIONLISTENER_H
