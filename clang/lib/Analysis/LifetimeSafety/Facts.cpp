//===- Facts.cpp - Lifetime Analysis Facts Implementation -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/LifetimeSafety/Facts.h"
#include "clang/AST/Decl.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/FlowSensitive/DataflowWorklist.h"
#include "llvm/ADT/STLFunctionalExtras.h"

namespace clang::lifetimes::internal {

void Fact::dump(llvm::raw_ostream &OS, const LoanManager &,
                const OriginManager &) const {
  OS << "Fact (Kind: " << static_cast<int>(K) << ")\n";
}

void IssueFact::dump(llvm::raw_ostream &OS, const LoanManager &LM,
                     const OriginManager &OM) const {
  OS << "Issue (";
  LM.getLoan(getLoanID())->dump(OS);
  OS << ", ToOrigin: ";
  OM.dump(getOriginID(), OS);
  OS << ")\n";
}

void ExpireFact::dump(llvm::raw_ostream &OS, const LoanManager &LM,
                      const OriginManager &) const {
  OS << "Expire (";
  LM.getLoan(getLoanID())->dump(OS);
  OS << ")\n";
}

void OriginFlowFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                          const OriginManager &OM) const {
  OS << "OriginFlow: \n";
  OS << "\tDest: ";
  OM.dump(getDestOriginID(), OS);
  OS << "\n";
  OS << "\tSrc:  ";
  OM.dump(getSrcOriginID(), OS);
  OS << (getKillDest() ? "" : ", Merge");
  OS << "\n";
}

void MovedOriginFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                           const OriginManager &OM) const {
  OS << "MovedOrigins (";
  OM.dump(getMovedOrigin(), OS);
  OS << ")\n";
}

void ReturnEscapeFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                            const OriginManager &OM) const {
  OS << "OriginEscapes (";
  OM.dump(getEscapedOriginID(), OS);
  OS << ", via Return)\n";
}

void FieldEscapeFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                           const OriginManager &OM) const {
  OS << "OriginEscapes (";
  OM.dump(getEscapedOriginID(), OS);
  OS << ", via Field)\n";
}

void UseFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                   const OriginManager &OM) const {
  OS << "Use (";
  size_t NumUsedOrigins = getUsedOrigins()->getLength();
  size_t I = 0;
  for (const OriginList *Cur = getUsedOrigins(); Cur;
       Cur = Cur->peelOuterOrigin(), ++I) {
    OM.dump(Cur->getOuterOriginID(), OS);
    if (I < NumUsedOrigins - 1)
      OS << ", ";
  }
  OS << ", " << (isWritten() ? "Write" : "Read") << ")\n";
}

void TestPointFact::dump(llvm::raw_ostream &OS, const LoanManager &,
                         const OriginManager &) const {
  OS << "TestPoint (Annotation: \"" << getAnnotation() << "\")\n";
}

llvm::StringMap<ProgramPoint> FactManager::getTestPoints() const {
  llvm::StringMap<ProgramPoint> AnnotationToPointMap;
  for (const auto &BlockFacts : BlockToFacts) {
    for (const Fact *F : BlockFacts) {
      if (const auto *TPF = F->getAs<TestPointFact>()) {
        StringRef PointName = TPF->getAnnotation();
        assert(!AnnotationToPointMap.contains(PointName) &&
               "more than one test points with the same name");
        AnnotationToPointMap[PointName] = F;
      }
    }
  }
  return AnnotationToPointMap;
}

void FactManager::dump(const CFG &Cfg, AnalysisDeclContext &AC) const {
  llvm::dbgs() << "==========================================\n";
  llvm::dbgs() << "       Lifetime Analysis Facts:\n";
  llvm::dbgs() << "==========================================\n";
  if (const Decl *D = AC.getDecl())
    if (const auto *ND = dyn_cast<NamedDecl>(D))
      llvm::dbgs() << "Function: " << ND->getQualifiedNameAsString() << "\n";
  // Print blocks in the order as they appear in code for a stable ordering.
  for (const CFGBlock *B : *AC.getAnalysis<PostOrderCFGView>()) {
    llvm::dbgs() << "  Block B" << B->getBlockID() << ":\n";
    for (const Fact *F : getFacts(B)) {
      llvm::dbgs() << "    ";
      F->dump(llvm::dbgs(), LoanMgr, OriginMgr);
    }
    llvm::dbgs() << "  End of Block\n";
  }
}

llvm::ArrayRef<const Fact *>
FactManager::getBlockContaining(ProgramPoint P) const {
  if (const CFGBlock *B = getBlock(P))
    return getFacts(B);
  return {};
}

void FactManager::addBlockFacts(const CFGBlock *B,
                                llvm::ArrayRef<Fact *> NewFacts) {
  if (!NewFacts.empty()) {
    BlockToFacts[B->getBlockID()].assign(NewFacts.begin(), NewFacts.end());
    for (const Fact *F : NewFacts)
      FactToBlockMap[F] = B;
  }
}

const CFGBlock *FactManager::getBlock(const Fact *F) const {
  auto It = FactToBlockMap.find(F);
  if (It != FactToBlockMap.end())
    return It->second;
  return nullptr;
}

void FactManager::forAllPredecessors(const Fact *F,
                                     llvm::function_ref<void(const Fact *)> C) {
  const CFGBlock *StartBlock = getBlock(F);
  if (!StartBlock)
    return;

  // First process facts in the start block that precede F.
  llvm::ArrayRef<const Fact *> Facts = getFacts(StartBlock);
  auto It = llvm::find(Facts, F);
  // If found, iterate backwards from the fact before F.
  if (It != Facts.end())
    for (auto I = std::make_reverse_iterator(It); I != Facts.rend(); ++I)
      C(*I);

  // Use BackwardDataflowWorklist to traverse all ancestor blocks.
  BackwardDataflowWorklist Worklist(Cfg, AC);
  // We maintain a separate visited set to ensure we visit each ancestor block
  // exactly once.
  llvm::BitVector Visited(Cfg.getNumBlockIDs());

  auto EnqueuePredecessors = [&](const CFGBlock *B) {
    for (const CFGBlock *Pred : B->preds())
      if (Pred && !Visited.test(Pred->getBlockID())) {
        Visited.set(Pred->getBlockID());
        Worklist.enqueueBlock(Pred);
      }
  };

  EnqueuePredecessors(StartBlock);

  while (const CFGBlock *B = Worklist.dequeue()) {
    // Process all facts in the block in reverse order.
    llvm::ArrayRef<const Fact *> BlockFacts = getFacts(B);
    for (const Fact *Fact : llvm::reverse(BlockFacts))
      C(Fact);
    EnqueuePredecessors(B);
  }
}

} // namespace clang::lifetimes::internal
