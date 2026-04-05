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
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/ExtractorRegistry.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryBuilder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"

using namespace clang;
using namespace ssaf;
namespace {

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

class clang::ssaf::UnsafeBufferUsageTUSummaryExtractor
    : public TUSummaryExtractor {
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
    ContributorFinder ContributorFinder;

    ContributorFinder.TraverseAST(Ctx);
    for (auto *CD : ContributorFinder.Contributors) {
      auto EntitySummary = extractEntitySummary(CD, Ctx);

      if (!EntitySummary) {
        llvm::report_fatal_error(EntitySummary.takeError());
        continue;
      }
      assert(*EntitySummary);
      if ((*EntitySummary)->empty())
        continue;

      auto ContributorName = getEntityName(CD);

      if (!ContributorName) {
        llvm::report_fatal_error(makeEntityNameErr(Ctx, *CD));
        continue;
      }

      auto [Ignored, InsertionSucceeded] = SummaryBuilder.addSummary(
          addEntity(*ContributorName), std::move(*EntitySummary));

      assert(InsertionSucceeded && "duplicated contributor extraction");
    }
  }
};

// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int UnsafeBufferUsageTUSummaryExtractorAnchorSource = 0;

static clang::ssaf::TUSummaryExtractorRegistry::Add<
    ssaf::UnsafeBufferUsageTUSummaryExtractor>
    RegisterExtractor(UnsafeBufferUsageEntitySummary::Name,
                      "The TUSummaryExtractor for unsafe buffer pointers");
