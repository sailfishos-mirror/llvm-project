#include "clang/Analysis/Analyses/LifetimeAnnotations.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"

namespace clang {
namespace lifetimes {

const FunctionDecl *
getDeclWithMergedLifetimeBoundAttrs(const FunctionDecl *FD) {
  return FD != nullptr ? FD->getMostRecentDecl() : nullptr;
}

const CXXMethodDecl *
getDeclWithMergedLifetimeBoundAttrs(const CXXMethodDecl *CMD) {
  const FunctionDecl *FD = CMD;
  return cast_if_present<CXXMethodDecl>(
      getDeclWithMergedLifetimeBoundAttrs(FD));
}

// Return true if this is an "normal" assignment operator.
// We assume that a normal assignment operator always returns *this, that is,
// an lvalue reference that is the same type as the implicit object parameter
// (or the LHS for a non-member operator$=).
bool isNormalAssignmentOperator(const FunctionDecl *FD) {
  OverloadedOperatorKind OO = FD->getDeclName().getCXXOverloadedOperator();
  if (OO == OO_Equal || isCompoundAssignmentOperator(OO)) {
    QualType RetT = FD->getReturnType();
    if (RetT->isLValueReferenceType()) {
      ASTContext &Ctx = FD->getASTContext();
      QualType LHST;
      auto *MD = dyn_cast<CXXMethodDecl>(FD);
      if (MD && MD->isCXXInstanceMember())
        LHST = Ctx.getLValueReferenceType(MD->getFunctionObjectParameterType());
      else
        LHST = FD->getParamDecl(0)->getType();
      if (Ctx.hasSameType(RetT, LHST))
        return true;
    }
  }
  return false;
}

bool isAssignmentOperatorLifetimeBound(const CXXMethodDecl *CMD) {
  CMD = lifetimes::getDeclWithMergedLifetimeBoundAttrs(CMD);
  return CMD && isNormalAssignmentOperator(CMD) && CMD->param_size() == 1 &&
         CMD->getParamDecl(0)->hasAttr<clang::LifetimeBoundAttr>();
}

bool implicitObjectParamIsLifetimeBound(const FunctionDecl *FD) {
  FD = getDeclWithMergedLifetimeBoundAttrs(FD);
  const TypeSourceInfo *TSI = FD->getTypeSourceInfo();
  if (!TSI)
    return false;
  // Don't declare this variable in the second operand of the for-statement;
  // GCC miscompiles that by ending its lifetime before evaluating the
  // third operand. See gcc.gnu.org/PR86769.
  AttributedTypeLoc ATL;
  for (TypeLoc TL = TSI->getTypeLoc();
       (ATL = TL.getAsAdjusted<AttributedTypeLoc>());
       TL = ATL.getModifiedLoc()) {
    if (ATL.getAttrAs<clang::LifetimeBoundAttr>())
      return true;
  }

  return isNormalAssignmentOperator(FD);
}

} // namespace lifetimes
} // namespace clang
