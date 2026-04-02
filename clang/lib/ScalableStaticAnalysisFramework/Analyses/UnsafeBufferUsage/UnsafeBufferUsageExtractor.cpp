//===- UnsafeBufferUsageExtractor.cpp -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SSAFAnalysesCommon.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/Analysis/Analyses/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Core/ASTEntityMapping.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityName.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"

namespace {
using namespace clang;
struct UnsafePointerMatcher {
  std::set<const Expr *> UnsafePointers;

  bool matches(const DynTypedNode &N, ASTContext &Ctx,
               const NamedDecl *Contributor) {
    return matchUnsafePointers(N, Ctx, UnsafePointers);
  }
};

void findFactsInContributor(const NamedDecl *Contributor, ASTContext &Ctx,
                            std::set<const Expr *> &UnsafePointers) {
  UnsafePointerMatcher Matcher;
  ContributorFactFinder<UnsafePointerMatcher> Finder(Ctx, Matcher);

  Finder.findMatches(Contributor);
  UnsafePointers.merge(Matcher.UnsafePointers);
}

static llvm::Error makeCreateEntityNameError(const NamedDecl *FailedDecl,
                                             ASTContext &Ctx) {
  std::string LocStr = FailedDecl->getSourceRange().getBegin().printToString(
      Ctx.getSourceManager());
  return llvm::createStringError(
      "failed to create entity name for %s declared at %s",
      FailedDecl->getNameAsString().c_str(), LocStr.c_str());
}

static llvm::Error makeAddEntitySummaryError(const NamedDecl *FailedContributor,
                                             ASTContext &Ctx) {
  std::string LocStr =
      FailedContributor->getSourceRange().getBegin().printToString(
          Ctx.getSourceManager());
  return llvm::createStringError(
      "failed to add entity summary for contributor %s declared at %s",
      FailedContributor->getNameAsString().c_str(), LocStr.c_str());
}

Expected<EntityPointerLevelSet>
buildEntityPointerLevels(std::set<const Expr *> &&UnsafePointers,
                         UnsafeBufferUsageTUSummaryExtractor &Extractor,
                         ASTContext &Ctx,
                         std::function<EntityId(EntityName)> AddEntity) {
  EntityPointerLevelSet Result{};
  llvm::Error AllErrors = llvm::ErrorSuccess();

  for (const Expr *Ptr : UnsafePointers) {
    Expected<EntityPointerLevelSet> Translation =
        translateEntityPointerLevel(Ptr, Ctx, AddEntity);

    if (Translation) {
      // Filter out those temporary invalid EntityPointerLevels associated with
      // `&E` pointers:
      auto FilteredTranslation = llvm::make_filter_range(
          *Translation, [](const EntityPointerLevel &E) -> bool {
            return E.getPointerLevel() > 0;
          });
      Result.insert(FilteredTranslation.begin(), FilteredTranslation.end());
      continue;
    }
    AllErrors = llvm::joinErrors(std::move(AllErrors), Translation.takeError());
  }
  if (AllErrors)
    return AllErrors;
  return Result;
}
} // namespace

namespace clang::ssaf {

class UnsafeBufferUsageTUSummaryExtractor : public TUSummaryExtractor {
public:
  UnsafeBufferUsageTUSummaryExtractor(TUSummaryBuilder &Builder)
      : TUSummaryExtractor(Builder) {}

  EntityId addEntity(EntityName EN) { return SummaryBuilder.addEntity(EN); }

  Expected<std::unique_ptr<UnsafeBufferUsageEntitySummary>>
  extractEntitySummary(const NamedDecl *Contributor, ASTContext &Ctx) {
    std::set<const Expr *> UnsafePointers;
    EntityPointerLevelSet Results;

    findFactsInContributor(Contributor, Ctx, UnsafePointers);
    for (const Expr *Ptr : UnsafePointers) {
      Expected<EntityPointerLevelSet> Translation =
          translateEntityPointerLevel(Ptr, Ctx, [this](const EntityName &EN) {
            return SummaryBuilder.addEntity(EN);
          });

      if (Translation) {
        // Filter out those temporary invalid EntityPointerLevels associated
        // with
        // `&E` pointers. They need no transformation of entities:
        auto FilteredTranslation = llvm::make_filter_range(
            *Translation, [](const EntityPointerLevel &E) -> bool {
              return E.getPointerLevel() > 0;
            });
        Results.insert(FilteredTranslation.begin(), FilteredTranslation.end());
        continue;
      }
      return Translation.takeError();
    }
    return std::make_unique<UnsafeBufferUsageEntitySummary>(
        UnsafeBufferUsageEntitySummary(std::move(Results)));
  }

  void HandleTranslationUnit(ASTContext &Ctx) override {
    llvm::Error Errors = llvm::ErrorSuccess();
    auto addError = [&Errors](llvm::Error Err) {
      Errors = llvm::joinErrors(std::move(Errors), std::move(Err));
    };
    ContributorFinder ContributorFinder;

    ContributorFinder.VisitTranslationUnitDecl(Ctx.getTranslationUnitDecl());
    for (auto *CD : ContributorFinder.Contributors) {
      auto EntitySummary = extractEntitySummary(CD, Ctx);

      if (!EntitySummary) {
        addError(EntitySummary.takeError());
        continue;
      }
      assert(*EntitySummary &&
             "std::unique_ptr<EntitySummary> should not be null");
      if ((*EntitySummary)->empty())
        continue;

      auto ContributorName = getEntityName(CD);

      if (!ContributorName) {
        addError(entityNameErrFor(Ctx, *CD));
        continue;
      }

      auto [EntitySummaryPtr, Success] = SummaryBuilder.addSummary(
          addEntity(*ContributorName), std::move(*EntitySummary));

      if (!Success)
        addError(failedToAddEntitySummaryFor(Ctx, CD));
    }
    // FIXME: handle errors!
    llvm::consumeError(std::move(Errors));
  }
};

// Proxy functions for unit tests:
extern Expected<std::unique_ptr<UnsafeBufferUsageEntitySummary>>
extractEntitySummary(UnsafeBufferUsageTUSummaryExtractor &Extractor,
                     const NamedDecl *Contributor, ASTContext &Ctx) {
  return Extractor.extractEntitySummary(Contributor, Ctx);
}

extern UnsafeBufferUsageTUSummaryExtractor *
createUnsafeBufferUsageTUSummaryExtractor(TUSummaryBuilder &Builder) {
  return new UnsafeBufferUsageTUSummaryExtractor(Builder);
}

extern void destroyUnsafeBufferUsageTUSummaryExtractor(
    UnsafeBufferUsageTUSummaryExtractor *Extractor) {
  delete Extractor;
}

extern EntityId addEntity(UnsafeBufferUsageTUSummaryExtractor &Extractor,
                          const EntityName &EN) {
  return Extractor.addEntity(EN);
}
} // namespace clang::ssaf

volatile int UnsafeBufferUsageTUSummaryExtractorAnchorSource = 0;
static clang::ssaf::TUSummaryExtractorRegistry::Add<
    ssaf::UnsafeBufferUsageTUSummaryExtractor>
    RegisterExtractor("UnsafeBufferUsageTUSummaryExtractor",
                      "The TUSummaryExtractor for unsafe buffer pointers");
