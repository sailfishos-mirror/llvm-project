//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation for freelist.
///
//===----------------------------------------------------------------------===//

#include "freelist.h"

namespace LIBC_NAMESPACE_DECL {

void FreeList::push(Node *node) {
  if (begin_) {
    // Since the list is circular, insert the node immediately before begin_.
    node->prev_ = begin_->prev_;
    node->next_ = begin_;
    begin_->prev_->next_ = node;
    begin_->prev_ = node;
  } else {
    begin_ = node->prev_ = node->next_ = node;
  }
}

void FreeList::remove(Node *node) {
  LIBC_ASSERT(begin_ && "cannot remove from empty list");
  Node *next = node->next_;
  if (node == next) {
    LIBC_ASSERT(node == begin_ &&
                "a self-referential node must be the only element");
    begin_ = nullptr;
  } else {
    Node *prev = node->prev_;
    prev->next_ = next;
    next->prev_ = prev;
    if (begin_ == node)
      begin_ = next;
  }
}

} // namespace LIBC_NAMESPACE_DECL
