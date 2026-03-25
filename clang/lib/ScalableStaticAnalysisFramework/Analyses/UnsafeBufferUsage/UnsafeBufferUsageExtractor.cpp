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
#include "clang/Analysis/Analyses/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/EntityPointerLevel.h"
#include "clang/ScalableStaticAnalysisFramework/Analyses/UnsafeBufferUsage.h"
#include "clang/ScalableStaticAnalysisFramework/Core/ASTEntityMapping.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityName.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/ExtractorRegistry.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryBuilder.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryExtractor.h"
#include "clang/ScalableStaticAnalysisFramework/SSAFForceLinker.h" // IWYU pragma: keep
#include <memory>

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
