//===- AutomapToTargetData.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Optimizer/Builder/DirectivesCommon.h"
#include "flang/Optimizer/Builder/FIRBuilder.h"
#include "flang/Optimizer/Builder/HLFIRTools.h"
#include "flang/Optimizer/Dialect/FIROps.h"
#include "flang/Optimizer/Dialect/FIRType.h"
#include "flang/Optimizer/Dialect/Support/KindMapping.h"
#include "flang/Optimizer/HLFIR/HLFIROps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/Pass/Pass.h"
#include "llvm/Frontend/OpenMP/OMPConstants.h"
#include <mlir/Dialect/OpenMP/OpenMPInterfaces.h>
#include <mlir/IR/Operation.h>

namespace flangomp {
#define GEN_PASS_DEF_AUTOMAPTOTARGETDATAPASS
#include "flang/Optimizer/OpenMP/Passes.h.inc"
} // namespace flangomp

using namespace mlir;

namespace {
class AutomapToTargetDataPass
    : public flangomp::impl::AutomapToTargetDataPassBase<
          AutomapToTargetDataPass> {
  // Returns true if the variable has a dynamic size and therefore requires
  // bounds operations to describe its extents.
  bool needsBoundsOps(Value var) {
    assert(isa<omp::PointerLikeType>(var.getType()) &&
           "only pointer like types expected");
    Type t = fir::unwrapRefType(var.getType());
    if (Type inner = fir::dyn_cast_ptrOrBoxEleTy(t))
      return fir::hasDynamicSize(inner);
    return fir::hasDynamicSize(t);
  }

  // Generate MapBoundsOp operations for the variable if required.
  void genBoundsOps(fir::FirOpBuilder &builder, Value var,
                    SmallVectorImpl<Value> &boundsOps) {
    Location loc = var.getLoc();
    fir::factory::AddrAndBoundsInfo info =
        fir::factory::getDataOperandBaseAddr(builder, var,
                                             /*isOptional=*/false, loc);
    fir::ExtendedValue exv =
        hlfir::translateToExtendedValue(loc, builder, hlfir::Entity{info.addr},
                                        /*contiguousHint=*/true)
            .first;
    SmallVector<Value> tmp =
        fir::factory::genImplicitBoundsOps<mlir::omp::MapBoundsOp,
                                           mlir::omp::MapBoundsType>(
            builder, info, exv, /*dataExvIsAssumedSize=*/false, loc);
    llvm::append_range(boundsOps, tmp);
  }

  void findRelatedAllocmemFreemem(fir::AddrOfOp addressOfOp,
                                  llvm::SmallVector<fir::StoreOp> &allocmems,
                                  llvm::SmallVector<fir::LoadOp> &freemems) {
    assert(addressOfOp->hasOneUse() && "op must have single use");

    auto declaredRef =
        cast<hlfir::DeclareOp>(*addressOfOp->getUsers().begin())->getResult(0);

    for (Operation *refUser : declaredRef.getUsers()) {
      if (auto storeOp = dyn_cast<fir::StoreOp>(refUser))
        if (auto emboxOp = storeOp.getValue().getDefiningOp<fir::EmboxOp>())
          if (auto allocmemOp =
                  emboxOp.getOperand(0).getDefiningOp<fir::AllocMemOp>())
            allocmems.push_back(storeOp);

      if (auto loadOp = dyn_cast<fir::LoadOp>(refUser))
        for (Operation *loadUser : loadOp.getResult().getUsers())
          if (auto boxAddrOp = dyn_cast<fir::BoxAddrOp>(loadUser))
            for (Operation *boxAddrUser : boxAddrOp.getResult().getUsers())
              if (auto freememOp = dyn_cast<fir::FreeMemOp>(boxAddrUser))
                freemems.push_back(loadOp);
    }
  }

  void runOnOperation() override {
    ModuleOp module = getOperation()->getParentOfType<ModuleOp>();
    if (!module)
      module = dyn_cast<ModuleOp>(getOperation());
    if (!module)
      return;

    // Build FIR builder for helper utilities.
    fir::KindMapping kindMap = fir::getKindMapping(module);
    fir::FirOpBuilder builder{module, std::move(kindMap)};

    // Collect global variables with AUTOMAP flag.
    llvm::DenseSet<fir::GlobalOp> automapGlobals;
    module.walk([&](fir::GlobalOp globalOp) {
      if (auto iface =
              dyn_cast<omp::DeclareTargetInterface>(globalOp.getOperation()))
        if (iface.isDeclareTarget() && iface.getDeclareTargetAutomap())
          automapGlobals.insert(globalOp);
    });

    for (fir::GlobalOp globalOp : automapGlobals)
      if (auto uses = globalOp.getSymbolUses(module.getOperation()))
        for (auto &x : *uses)
          if (auto addrOp = dyn_cast<fir::AddrOfOp>(x.getUser())) {
            llvm::SmallVector<fir::StoreOp> allocstores;
            llvm::SmallVector<fir::LoadOp> freememloads;
            findRelatedAllocmemFreemem(addrOp, allocstores, freememloads);

            for (auto storeOp : allocstores) {
              builder.setInsertionPointAfter(storeOp);
              SmallVector<Value> bounds;
              if (needsBoundsOps(storeOp.getMemref()))
                genBoundsOps(builder, storeOp.getMemref(), bounds);

              omp::TargetEnterExitUpdateDataOperands clauses;
              mlir::omp::MapInfoOp mapInfo = mlir::omp::MapInfoOp::create(
                  builder, storeOp.getLoc(), storeOp.getMemref().getType(),
                  storeOp.getMemref(),
                  TypeAttr::get(
                      fir::unwrapRefType(storeOp.getMemref().getType())),
                  builder.getIntegerAttr(
                      builder.getIntegerType(64, false),
                      static_cast<unsigned>(
                          llvm::omp::OpenMPOffloadMappingFlags::OMP_MAP_TO)),
                  builder.getAttr<omp::VariableCaptureKindAttr>(
                      omp::VariableCaptureKind::ByRef),
                  /*var_ptr_ptr=*/mlir::Value{},
                  /*members=*/SmallVector<Value>{},
                  /*members_index=*/ArrayAttr{}, bounds,
                  /*mapperId=*/mlir::FlatSymbolRefAttr(),
                  globalOp.getSymNameAttr(), builder.getBoolAttr(false));
              clauses.mapVars.push_back(mapInfo);
              builder.create<omp::TargetEnterDataOp>(storeOp.getLoc(), clauses);
            }

            for (auto loadOp : freememloads) {
              builder.setInsertionPoint(loadOp);
              SmallVector<Value> bounds;
              if (needsBoundsOps(loadOp.getMemref()))
                genBoundsOps(builder, loadOp.getMemref(), bounds);

              omp::TargetEnterExitUpdateDataOperands clauses;
              mlir::omp::MapInfoOp mapInfo = mlir::omp::MapInfoOp::create(
                  builder, loadOp.getLoc(), loadOp.getMemref().getType(),
                  loadOp.getMemref(),
                  TypeAttr::get(
                      fir::unwrapRefType(loadOp.getMemref().getType())),
                  builder.getIntegerAttr(
                      builder.getIntegerType(64, false),
                      static_cast<unsigned>(
                          llvm::omp::OpenMPOffloadMappingFlags::
                              OMP_MAP_DELETE)),
                  builder.getAttr<omp::VariableCaptureKindAttr>(
                      omp::VariableCaptureKind::ByRef),
                  /*var_ptr_ptr=*/mlir::Value{},
                  /*members=*/SmallVector<Value>{},
                  /*members_index=*/ArrayAttr{}, bounds,
                  /*mapperId=*/mlir::FlatSymbolRefAttr(),
                  globalOp.getSymNameAttr(), builder.getBoolAttr(false));
              clauses.mapVars.push_back(mapInfo);
              builder.create<omp::TargetExitDataOp>(loadOp.getLoc(), clauses);
            }
          }
  }
};
} // namespace
