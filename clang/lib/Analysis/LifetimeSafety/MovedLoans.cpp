
#include "clang/Analysis/Analyses/LifetimeSafety/MovedLoans.h"
#include "Dataflow.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Facts.h"
#include "clang/Analysis/Analyses/LifetimeSafety/LoanPropagation.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Loans.h"
#include "clang/Analysis/Analyses/LifetimeSafety/Utils.h"

namespace clang::lifetimes::internal {
namespace {
struct Lattice {
  MovedLoansMap MovedLoans = MovedLoansMap(nullptr);

  explicit Lattice(MovedLoansMap MovedLoans) : MovedLoans(MovedLoans) {}

  Lattice() = default;

  bool operator==(const Lattice &Other) const {
    return MovedLoans == Other.MovedLoans;
  }
  bool operator!=(const Lattice &Other) const { return !(*this == Other); }
};

class AnalysisImpl
    : public DataflowAnalysis<AnalysisImpl, Lattice, Direction::Forward> {
public:
  AnalysisImpl(const CFG &C, AnalysisDeclContext &AC, FactManager &F,
               const LoanPropagationAnalysis &LoanPropagation,
               const LoanManager &LoanMgr,
               MovedLoansMap::Factory &MovedLoansMapFactory)
      : DataflowAnalysis(C, AC, F), LoanPropagation(LoanPropagation),
        LoanMgr(LoanMgr), MovedLoansMapFactory(MovedLoansMapFactory) {}

  using Base::transfer;

  StringRef getAnalysisName() const { return "MovedLoans"; }

  Lattice getInitialState() { return Lattice{}; }

  // TODO: Doc.
  Lattice join(Lattice A, Lattice B) {
    MovedLoansMap MovedLoans = utils::join(
        A.MovedLoans, B.MovedLoans, MovedLoansMapFactory,
        [](const Expr *const *MoveA, const Expr *const *MoveB) -> const Expr * {
          assert(MoveA || MoveB);
          if (!MoveA)
            return *MoveB;
          if (!MoveB)
            return *MoveA;
          return (*MoveA)->getExprLoc() < (*MoveB)->getExprLoc() ? *MoveA
                                                                 : *MoveB;
        },
        utils::JoinKind::Asymmetric);
    return Lattice(MovedLoans);
  }

  Lattice transfer(Lattice In, const MovedOriginFact &F) {
    MovedLoansMap MovedLoans = In.MovedLoans;
    OriginID MovedOrigin = F.getMovedOrigin();
    LoanSet ImmediatelyMovedLoans = LoanPropagation.getLoans(MovedOrigin, &F);
    FactMgr.forAllPredecessors(&F, [&](const Fact *PF) {
      auto *IF = PF->getAs<IssueFact>();
      if (!IF)
        return;
      for (LoanID LID : ImmediatelyMovedLoans) {
        const Loan *MovedLoan = LoanMgr.getLoan(LID);
        auto *PL = dyn_cast<PathLoan>(MovedLoan);
        if (!PL)
          continue;
        LoanID ReachableLID = IF->getLoanID();
        const Loan *ReachableLoan = LoanMgr.getLoan(ReachableLID);
        if (auto *RL = dyn_cast<PathLoan>(ReachableLoan))
          if (RL->getAccessPath() == PL->getAccessPath())
            MovedLoans = MovedLoansMapFactory.add(MovedLoans, ReachableLID,
                                                  F.getMoveExpr());
      }
    });
    return Lattice(MovedLoans);
  }

  MovedLoansMap getMovedLoans(ProgramPoint P) { return getState(P).MovedLoans; }

private:
  const LoanPropagationAnalysis &LoanPropagation;
  const LoanManager &LoanMgr;
  MovedLoansMap::Factory &MovedLoansMapFactory;
};
} // namespace

class MovedLoansAnalysis::Impl final : public AnalysisImpl {
  using AnalysisImpl::AnalysisImpl;
};

MovedLoansAnalysis::MovedLoansAnalysis(
    const CFG &C, AnalysisDeclContext &AC, FactManager &F,
    const LoanPropagationAnalysis &LoanPropagation, const LoanManager &LoanMgr,
    MovedLoansMap::Factory &MovedLoansMapFactory)
    : PImpl(std::make_unique<Impl>(C, AC, F, LoanPropagation, LoanMgr,
                                   MovedLoansMapFactory)) {
  PImpl->run();
}

MovedLoansAnalysis::~MovedLoansAnalysis() = default;

MovedLoansMap MovedLoansAnalysis::getMovedLoans(ProgramPoint P) const {
  return PImpl->getMovedLoans(P);
}
} // namespace clang::lifetimes::internal
