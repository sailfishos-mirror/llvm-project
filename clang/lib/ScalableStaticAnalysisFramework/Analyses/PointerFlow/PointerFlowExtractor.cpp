//===- PointerFlowExtractor.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SSAFAnalysesCommon.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/TypeBase.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerFlow/PointerFlow.h"
#include "clang/ScalableStaticAnalysisFramework/Core/ASTEntityMapping.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityName.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/ExtractorRegistry.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryBuilder.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryExtractor.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"
#include <functional>
#include <memory>
#include <optional>

namespace {
using namespace clang;
using namespace ssaf;

class PointerAssignmentMatcher {
  ASTContext &Ctx;
  std::function<EntityId(const EntityName &)> AddEntity;

  // Convert a Expr/NamedDecl to an EntityPointerLevel(Set):
  Expected<EntityPointerLevelSet> toEPL(const NamedDecl *N,
                                        bool IsRet = false) {
    auto Ret = createEntityPointerLevel(N, Ctx, AddEntity, IsRet);

    if (Ret)
      return EntityPointerLevelSet{*Ret};
    return Ret.takeError();
  }

  Expected<EntityPointerLevelSet> toEPL(const Expr *N, bool IsRet = false) {
    return translateEntityPointerLevel(N, Ctx, AddEntity);
  }

  template <typename T1, typename T2>
  bool addEdges(const T1 *LHS, const T2 *RHS) {
    return addEdges(toEPL(LHS), RHS, false);
  }

  template <typename T>
  bool addEdges(Expected<EntityPointerLevelSet> &&LHS, const T *RHS,
                bool IsRHSRet = false) {
    auto Rs = toEPL(RHS, IsRHSRet);
    bool Error = false;

    if (!Rs) {
      addError(Rs.takeError());
      Error = true;
    }
    if (!LHS) {
      addError(LHS.takeError());
      Error = true;
    }
    if (Error)
      return false;
    for (auto L : *LHS)
      Results[L].insert(Rs->begin(), Rs->end());
    return true;
  }

  template <typename ParmsProvider, typename ArgsProvider>
  bool matchArgsWithParams(unsigned ArgIdxStart, ParmsProvider *PP,
                           ArgsProvider *AP) {
    bool Matched = false;
    unsigned ArgIdx = ArgIdxStart;

    for (unsigned ParmIdx = 0; ParmIdx < PP->getNumParams();
         ++ArgIdx, ++ParmIdx) {
      if (const ParmVarDecl *PD = PP->getParamDecl(ParmIdx);
          PD && hasPtrOrArrType(*PD)) {
        addEdges(PD, AP->getArg(ArgIdx));
        Matched = true;
      }
    }
    return Matched;
  }

  void addError(llvm::Error &&E) {
    Error = llvm::joinErrors(std::move(Error), std::move(E));
  }

  // Match initializer lists of the form 'Var = {a, b, c, ...}':
  //
  //   If 'Var' is a struct/union:
  //     'Var = {a, b, c, ...}' => 'Var.field_1 = a'
  //                               'Var.field_2 = b'
  //                               ...
  //   If 'Var' is an array:
  //     'Var = {a, b, c, ...}' => '*Var = a'
  //                               '*Var = b'
  //                               ...
  //
  // The process is recursive: 'a', 'b', 'c', etc. may themselves be
  // initializer lists.  We therefore use `ArrayIndirectLevel` to keep track.
  bool matchInitializerList(const ValueDecl *Base, const Expr *InitExpr,
                            unsigned ArrayElementIndirectLevel = 0) {
    const InitListExpr *ILE = dyn_cast<InitListExpr>(InitExpr);

    if (!ILE) {
      if (!hasPtrOrArrType(*InitExpr))
        return false;

      auto Expected = toEPL(Base);

      if (!Expected) {
        addError(Expected.takeError());
        return false;
      }

      auto R = llvm::map_range(*Expected, [&ArrayElementIndirectLevel](
                                              const EntityPointerLevel &EPL) {
        EntityPointerLevel Result = EPL;
        for (unsigned I = 0; I < ArrayElementIndirectLevel; ++I)
          Result = incrementPointerLevel(Result);
        return Result;
      });
      return addEdges(EntityPointerLevelSet{R.begin(), R.end()}, InitExpr);
    }
    // Note that `Base`'s type is NOT the real LHS type when
    // ArrayElementIndirectLevel > 0:
    QualType Type = InitExpr->getType();

    if (auto *RD = Type->getAsRecordDecl())
      return matchInitializerListForRecordDecl(RD, ILE);
    if (Type->isArrayType())
      return matchInitializerListForArray(Base, ILE, ArrayElementIndirectLevel);
    // Must be the case of using a initializer-list for a scalar:
    return matchInitializerList(Base, ILE->getInit(0));
  }

  // Helper function for matchInitializerList that handles record:
  bool matchInitializerListForRecordDecl(const RecordDecl *RecordTy,
                                         const InitListExpr *ILE) {
    if (auto *CXXRD = dyn_cast<CXXRecordDecl>(RecordTy))
      if (CXXRD->getNumBases() != 0) {
        // FIXME: support this:
        addError(makeErrAtNode(
            Ctx, *ILE,
            "attempt to create pointer assignment edges between "
            "CXXRecordDecls with base classes and initializer-lists"));
        return false;
      }
    // Handle union:
    if (RecordTy->isUnion()) {
      auto *InitField = ILE->getInitializedFieldInUnion();

      if (!InitField)
        return false;
      assert(!ILE->inits().empty());
      return matchInitializerList(InitField, ILE->getInit(0));
    }
    // Handle struct/class:
    ILE = ILE->getSemanticForm() ? ILE->getSemanticForm() : ILE;

    auto FieldIter = RecordTy->field_begin();
    bool Matched = false;

    assert(RecordTy->getNumFields() >= ILE->getNumInits());
    for (auto *Init : ILE->inits()) {
      if (matchInitializerList(*(FieldIter++), Init))
        Matched = true;
    }
    return Matched;
  }

  // Helper function for matchInitializerList that handles array:
  bool matchInitializerListForArray(const ValueDecl *Array,
                                    const InitListExpr *ILE,
                                    unsigned ArrayIndirectLevel = 0) {
    bool Matched = false;

    for (auto *E : ILE->inits())
      if (matchInitializerList(Array, E, ArrayIndirectLevel + 1))
        Matched = true;
    return Matched;
  }

public:
  llvm::Error Error;
  EdgeSet Results;

  PointerAssignmentMatcher(
      ASTContext &Ctx, std::function<EntityId(const EntityName &)> AddEntity)
      : Ctx(Ctx), AddEntity(AddEntity), Error(llvm::Error::success()) {}

  // Match and extract assignments.
  // The extraction function 'XF' can be described by the following rules:
  //
  // XF(l = r)               => '(toEPL(l), toEPL(r))'
  // XF(foo(a, b, ...))      => XF(Param_1 = a), XF(Param_2 = b), ...
  // XF(return e;)           => XF(Fun = e), where 'Fun' is the enclosing
  //                                         function
  // XF(ctor(a, ...) : x1(y1), ... {...})
  //                         => XF(Param_1 = a), ...,
  //                            XF(x1 = y1), ...,
  //                            ctor's body will be visited later.
  // XF(T var = e)           => XF(Var = e)
  // XF(T var = init-list)   => see `matchInitializerList`
  bool matches(const DynTypedNode &DynNode, ASTContext &Ctx,
               const NamedDecl *RootDecl) {
    if (const Stmt *S = DynNode.get<Stmt>()) {
      // Match 'p = q' whenever it has pointer or array type:
      if (const auto *BO = dyn_cast<BinaryOperator>(S);
          BO && BO->getOpcode() == BO_Assign && hasPtrOrArrType(*BO)) {
        return addEdges(BO->getLHS(), BO->getRHS());
      }

      // Match arg-to-param passing (in CallExpr) for any pointer type argument:
      if (const auto *CE = dyn_cast<CallExpr>(S)) {
        const FunctionDecl *FD = CE->getDirectCallee();

        if (!FD)
          return false;

        unsigned ArgIdx = 0;

        if (isa<CXXOperatorCallExpr>(CE))
          if (auto *MD = dyn_cast<CXXMethodDecl>(FD);
              MD && !MD->isExplicitObjectMemberFunction())
            ArgIdx = 1;
        return matchArgsWithParams(ArgIdx, FD, CE);
      }
      // Match arg-to-param passing (in CXXConstructExpr) for any pointer type
      // argument:
      if (const auto *CCE = dyn_cast<CXXConstructExpr>(S)) {
        return matchArgsWithParams(/*ArgIdxStart=*/0, CCE->getConstructor(),
                                   CCE);
      }
      if (const auto *RS = dyn_cast<ReturnStmt>(S)) {
        const Expr *RetExpr = RS->getRetValue();
        if (!hasPtrOrArrType(*RetExpr))
          return false;
        return addEdges(toEPL(RootDecl, true), RetExpr, false);
      }
    }

    if (const Decl *D = DynNode.get<Decl>()) {
      const Expr *InitExpr = nullptr;

      if (const auto *VD = dyn_cast<ValueDecl>(D)) {
        if (const auto *Var = dyn_cast<VarDecl>(VD))
          InitExpr = Var->getInit();
        if (const auto *Fd = dyn_cast<FieldDecl>(VD))
          InitExpr = Fd->getInClassInitializer();

        // Match initializer-list:
        if (auto *InitLst = dyn_cast_or_null<InitListExpr>(InitExpr))
          return matchInitializerList(VD, InitLst);

        // Match initializers to variables/fields of a pointer type:
        if (InitExpr && hasPtrOrArrType(*VD))
          return addEdges(VD, InitExpr);
      }

      // Match C++ constructor member-initializers:
      if (const auto *CtorD = dyn_cast<CXXConstructorDecl>(D)) {
        bool Matched = false;

        for (auto *E : CtorD->inits()) {
          if (E->isDelegatingInitializer())
            return matches(DynTypedNode::create(*E->getInit()), Ctx, RootDecl);
          if (const FieldDecl *FD = E->getMember();
              FD && hasPtrOrArrType(*FD)) {
            addEdges(E->getMember(), E->getInit());
            Matched = true;
          }
        }
        return Matched;
      }
    }
    return false;
  }
};
} // namespace

namespace clang::ssaf {
class PointerFlowTUSummaryExtractor : public TUSummaryExtractor {
public:
  PointerFlowTUSummaryExtractor(TUSummaryBuilder &Builder)
      : TUSummaryExtractor(Builder) {}

  EntityId addEntity(const EntityName &EN) {
    return SummaryBuilder.addEntity(EN);
  }

  Expected<std::unique_ptr<PointerFlowEntitySummary>>
  extractEntitySummary(const NamedDecl *Contributor, ASTContext &Ctx) {
    PointerAssignmentMatcher Matcher(
        Ctx, [this](const EntityName &EN) { return addEntity(EN); });
    ContributorFactFinder<PointerAssignmentMatcher> Finder(Ctx, Matcher);

    Finder.findMatches(const_cast<NamedDecl *>(Contributor));
    if (Matcher.Error)
      return std::move(Matcher.Error);
    return std::make_unique<PointerFlowEntitySummary>(
        PointerFlowEntitySummary(std::move(Matcher.Results)));
  }

  void HandleTranslationUnit(ASTContext &Ctx) override {
    ContributorFinder ContributorFinder;

    ContributorFinder.TraverseAST(Ctx);
    for (auto *CD : ContributorFinder.Contributors) {
      auto EntitySummary = extractEntitySummary(CD, Ctx);

      if (!EntitySummary)
        llvm::reportFatalInternalError(EntitySummary.takeError());
      assert(*EntitySummary);
      if ((*EntitySummary)->empty())
        continue;

      auto ContributorName = getEntityName(CD);

      if (!ContributorName)
        llvm::reportFatalInternalError(makeEntityNameErr(Ctx, *CD));

      auto [Ignored, InsertionSucceeded] = SummaryBuilder.addSummary(
          addEntity(*ContributorName), std::move(*EntitySummary));

      assert(InsertionSucceeded && "duplicated contributor extraction");
    }
  }
};
} // namespace clang::ssaf

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int PointerFlowTUSummaryExtractorAnchorSource = 0;
static TUSummaryExtractorRegistry::Add<PointerFlowTUSummaryExtractor>
    RegisterExtractor(PointerFlowEntitySummary::Name,
                      "The TUSummaryExtractor for pointer assignments");
