//===------------------ SSAFAnalysesCommon.h --------------------*- C++ -*-===//
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

#include "clang/AST/Decl.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/TypeBase.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/JSON.h"

namespace {
using namespace clang;

template <typename NodeTy, typename... Ts>
static inline llvm::Error strErrAtNode(ASTContext &Ctx, const NodeTy &N,
                                       StringRef Fmt, const Ts &...Args) {
  std::string LocStr = N.getBeginLoc().printToString(Ctx.getSourceManager());
  llvm::SmallVector<char> FmtData;

  (Fmt + " at %s").toStringRef(FmtData);
  return llvm::createStringError(FmtData.data(), Args..., LocStr.c_str());
}

static inline llvm::Error entityNameErrFor(ASTContext &Ctx,
                                           const NamedDecl &D) {
  return strErrAtNode(Ctx, D, "failed to create entity name for %s",
                      D.getNameAsString().data());
}

static inline llvm::Error failedToAddEntitySummaryFor(ASTContext &Ctx,
                                                      const NamedDecl *D) {
  std::string LocStr = D->getBeginLoc().printToString(Ctx.getSourceManager());

  return llvm::createStringError("failed to add entity summary for %s at %s",
                                 D->getNameAsString().data(), LocStr.c_str());
}

template <typename... Ts>
static inline llvm::Error makeErrorSawButExpected(const llvm::json::Value &Saw,
                                                  llvm::StringRef Expected,
                                                  const Ts &...ExpectedArgs) {
  return llvm::createStringError(
      ("saw %s but expected " + Expected).str().c_str(),
      llvm::formatv("{0:2}", Saw).str().data(), Expected.data(),
      ExpectedArgs...);
}

template <typename TypedObj> static bool hasPtrOrArrType(const TypedObj &E) {
  return llvm::isa<PointerType>(E.getType().getCanonicalType()) ||
         llvm::isa<ArrayType>(E.getType().getCanonicalType());
}

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
    return false;
  }
};

/// An AST visitor that skips callable decl and record decl strict-descendant
/// because those are separate contributors.
///
/// The visitor calls
/// `MatcherTy::matchFact(DynTypedNode &, ASTContext &, const NamedDecl
/// *Contributor)` on each visited Decl or Stmt node to collect facts in the
/// `Contributor`.
template <typename MatcherTy>
class ContributorFactFinder : public DynamicRecursiveASTVisitor {
  ASTContext &Ctx;
  MatcherTy &Matcher;
  const NamedDecl *RootDecl;

  template <typename T> bool match(const T &Node) {
    return Matcher.matches(DynTypedNode::create(Node), Ctx, RootDecl);
  }

public:
  ContributorFactFinder(ASTContext &Ctx, MatcherTy &Matcher)
      : Ctx(Ctx), Matcher(Matcher) {
    ShouldVisitTemplateInstantiations = true;
    ShouldVisitImplicitCode = false;
  }

  /// The entry point:
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
    return true; // skip lambda as it is a callable
  }
};
} // namespace
#endif // LLVM_CLANG_SCALABLESTATICANALYSISFRAMEWORK_ANALYSES_SSAFANALYSESCOMMON_H

