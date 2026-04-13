//===-- SSAFAnalysesCommon.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Common code in SSAF analyses implementations
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_SSAFANALYSESCOMMON_H
#define LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_SSAFANALYSESCOMMON_H

#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/JSON.h"

using namespace clang;

template <typename NodeTy, typename... Ts>
llvm::Error makeErrAtNode(ASTContext &Ctx, const NodeTy &N, StringRef Fmt,
                          const Ts &...Args) {
  std::string LocStr = N.getBeginLoc().printToString(Ctx.getSourceManager());
  return llvm::createStringError((Fmt + " at %s").str().c_str(), Args...,
                                 LocStr.c_str());
}

llvm::Error makeEntityNameErr(ASTContext &Ctx, const NamedDecl &D) {
  return makeErrAtNode(Ctx, D, "failed to create entity name for %s",
                       D.getNameAsString().data());
}

static inline llvm::Error makeAddEntitySummaryErr(ASTContext &Ctx,
                                                  const NamedDecl *D) {
  std::string LocStr = D->getBeginLoc().printToString(Ctx.getSourceManager());

  return llvm::createStringError("failed to add entity summary for %s at %s",
                                 D->getNameAsString().data(), LocStr.c_str());
}

template <typename... Ts>
llvm::Error makeSawButExpectedError(const llvm::json::Value &Saw,
                                    llvm::StringRef Expected,
                                    const Ts &...ExpectedArgs) {
  std::string Fmt = ("saw %s but expected " + Expected).str();
  std::string SawStr = llvm::formatv("{0:2}", Saw).str();

  return llvm::createStringError(Fmt.c_str(), SawStr.c_str(), ExpectedArgs...);
}

template <typename DeclOrExpr> bool hasPtrOrArrType(const DeclOrExpr *E) {
  return llvm::isa<PointerType, ArrayType>(E->getType().getCanonicalType());
}

namespace clang::ssaf {
/// Traverses the AST and finds contributors:
class ContributorFinder : public DynamicRecursiveASTVisitor {
public:
  std::vector<const NamedDecl *> Contributors;

  bool VisitFunctionDecl(FunctionDecl *D) override {
    Contributors.push_back(D);
    return true;
  }

  bool VisitRecordDecl(RecordDecl *D) override {
    Contributors.push_back(D);
    return true;
  }

  bool VisitVarDecl(VarDecl *D) override {
    DeclContext *DC = D->getDeclContext();

    if (DC->isFileContext() || DC->isNamespace())
      Contributors.push_back(D);
    return true;
  }
};

// FIXME: move this to a .cpp file

/// An AST visitor that skips the root node's strict-descendants that are
/// callable Decls and record Decls, because those are separate contributors.
///
/// Clients need to implement their own `MatchAction`, which is a function that
/// takes a `DynTypedNode`, decides if it matches and performs any further
/// callback actions.
class ContributorFactFinder : public DynamicRecursiveASTVisitor {
  std::function<void(const DynTypedNode &)> MatchAction;
  const NamedDecl *RootDecl = nullptr;

  template <typename NodeTy> void match(const NodeTy &Node) {
    MatchAction(DynTypedNode::create(Node));
  }

public:
  ContributorFactFinder(std::function<void(const DynTypedNode &)> MatchAction)
      : MatchAction(MatchAction) {
    ShouldVisitTemplateInstantiations = true;
    ShouldVisitImplicitCode = false;
  }

  // The entry point:
  void findMatches(const NamedDecl *Contributor) {
    RootDecl = Contributor;
    TraverseDecl(const_cast<NamedDecl *>(Contributor));
  }

  bool TraverseDecl(Decl *Node) override {
    if (!Node)
      return true;
    // To skip callables:
    if (Node != RootDecl && isa<FunctionDecl, CXXConstructorDecl, BlockDecl,
                                ObjCMethodDecl, RecordDecl>(Node))
      return true;
    match(*Node);
    return DynamicRecursiveASTVisitor::TraverseDecl(Node);
  }

  bool TraverseStmt(Stmt *Node) override {
    if (!Node)
      return true;
    match(*Node);
    return DynamicRecursiveASTVisitor::TraverseStmt(Node);
  }

  bool TraverseLambdaExpr(LambdaExpr *L) override {
    // TODO: lambda captures of pointer variables (by copy or by reference)
    // are currently not tracked. Each capture initializes an implicit closure
    // field from the captured variable, which constitutes a pointer assignment
    // edge that should be recorded here.
    return true; // Skip lambda as it is a callable.
  }
};
} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_SSAFANALYSESCOMMON_H
