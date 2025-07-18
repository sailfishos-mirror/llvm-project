//===- InitUndef.cpp - Initialize undef value to pseudo ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a function pass that initializes undef value to
// temporary pseudo instruction to prevent register allocation resulting in a
// constraint violated result for the particular instruction. It also rewrites
// the NoReg tied operand back to an IMPLICIT_DEF.
//
// Certain instructions have register overlapping constraints, and
// will cause illegal instruction trap if violated, we use early clobber to
// model this constraint, but it can't prevent register allocator allocating
// same or overlapped if the input register is undef value, so convert
// IMPLICIT_DEF to temporary pseudo instruction and remove it later could
// prevent that happen, it's not best way to resolve this, and it might
// change the order of program or increase the register pressure, so ideally we
// should model the constraint right, but before we model the constraint right,
// it's the only way to prevent that happen.
//
// When we enable the subregister liveness option, it will also trigger the same
// issue due to the partial of register is undef. If we pseudoinit the whole
// register, then it will generate redundant COPY instruction. Currently, it
// will generate INSERT_SUBREG to make sure the whole register is occupied
// when program encounter operation that has early-clobber constraint.
//
//
// See also: https://github.com/llvm/llvm-project/issues/50157
//
// Additionally, this pass rewrites tied operands of instructions
// from NoReg to IMPLICIT_DEF.  (Not that this is a non-overlapping set of
// operands to the above.)  We use NoReg to side step a MachineCSE
// optimization quality problem but need to convert back before
// TwoAddressInstruction.  See pr64282 for context.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/InitUndef.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/DetectDeadLanes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "init-undef"
#define INIT_UNDEF_NAME "Init Undef Pass"

namespace {

class InitUndefLegacy : public MachineFunctionPass {
public:
  static char ID;

  InitUndefLegacy() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return INIT_UNDEF_NAME; }
};

class InitUndef {
  const TargetInstrInfo *TII;
  MachineRegisterInfo *MRI;
  const TargetSubtargetInfo *ST;
  const TargetRegisterInfo *TRI;

  // Newly added vregs, assumed to be fully rewritten
  SmallSet<Register, 8> NewRegs;
  SmallVector<MachineInstr *, 8> DeadInsts;

public:
  bool run(MachineFunction &MF);

private:
  bool processBasicBlock(MachineFunction &MF, MachineBasicBlock &MBB,
                         const DeadLaneDetector *DLD);
  bool handleSubReg(MachineFunction &MF, MachineInstr &MI,
                    const DeadLaneDetector &DLD);
  bool fixupIllOperand(MachineInstr *MI, MachineOperand &MO);
  bool handleReg(MachineInstr *MI);
};

} // end anonymous namespace

char InitUndefLegacy::ID = 0;
INITIALIZE_PASS(InitUndefLegacy, DEBUG_TYPE, INIT_UNDEF_NAME, false, false)
char &llvm::InitUndefID = InitUndefLegacy::ID;

static bool isEarlyClobberMI(MachineInstr &MI) {
  return llvm::any_of(MI.all_defs(), [](const MachineOperand &DefMO) {
    return DefMO.isReg() && DefMO.isEarlyClobber();
  });
}

static bool findImplictDefMIFromReg(Register Reg, MachineRegisterInfo *MRI) {
  for (auto &DefMI : MRI->def_instructions(Reg)) {
    if (DefMI.getOpcode() == TargetOpcode::IMPLICIT_DEF)
      return true;
  }
  return false;
}

bool InitUndef::handleReg(MachineInstr *MI) {
  bool Changed = false;
  for (auto &UseMO : MI->uses()) {
    if (!UseMO.isReg())
      continue;
    if (UseMO.isTied())
      continue;
    if (!UseMO.getReg().isVirtual())
      continue;

    if (UseMO.isUndef() || findImplictDefMIFromReg(UseMO.getReg(), MRI))
      Changed |= fixupIllOperand(MI, UseMO);
  }
  return Changed;
}

bool InitUndef::handleSubReg(MachineFunction &MF, MachineInstr &MI,
                             const DeadLaneDetector &DLD) {
  bool Changed = false;

  for (MachineOperand &UseMO : MI.uses()) {
    if (!UseMO.isReg())
      continue;
    if (!UseMO.getReg().isVirtual())
      continue;
    if (UseMO.isTied())
      continue;

    Register Reg = UseMO.getReg();
    if (NewRegs.count(Reg))
      continue;
    DeadLaneDetector::VRegInfo Info = DLD.getVRegInfo(Reg.virtRegIndex());

    if (Info.UsedLanes == Info.DefinedLanes)
      continue;

    const TargetRegisterClass *TargetRegClass = MRI->getRegClass(Reg);

    LaneBitmask NeedDef = Info.UsedLanes & ~Info.DefinedLanes;

    LLVM_DEBUG({
      dbgs() << "Instruction has undef subregister.\n";
      dbgs() << printReg(Reg, nullptr)
             << " Used: " << PrintLaneMask(Info.UsedLanes)
             << " Def: " << PrintLaneMask(Info.DefinedLanes)
             << " Need Def: " << PrintLaneMask(NeedDef) << "\n";
    });

    SmallVector<unsigned> SubRegIndexNeedInsert;
    TRI->getCoveringSubRegIndexes(TargetRegClass, NeedDef,
                                  SubRegIndexNeedInsert);

    // It's not possible to create the INIT_UNDEF when there is no register
    // class associated for the subreg. This may happen for artificial subregs
    // that are not directly addressable.
    if (any_of(SubRegIndexNeedInsert, [&](unsigned Ind) -> bool {
          return !TRI->getSubRegisterClass(TargetRegClass, Ind);
        }))
      continue;

    Register LatestReg = Reg;
    for (auto ind : SubRegIndexNeedInsert) {
      Changed = true;
      const TargetRegisterClass *SubRegClass =
          TRI->getSubRegisterClass(TargetRegClass, ind);
      Register TmpInitSubReg = MRI->createVirtualRegister(SubRegClass);
      LLVM_DEBUG(dbgs() << "Register Class ID" << SubRegClass->getID() << "\n");
      BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
              TII->get(TargetOpcode::INIT_UNDEF), TmpInitSubReg);
      Register NewReg = MRI->createVirtualRegister(TargetRegClass);
      BuildMI(*MI.getParent(), &MI, MI.getDebugLoc(),
              TII->get(TargetOpcode::INSERT_SUBREG), NewReg)
          .addReg(LatestReg)
          .addReg(TmpInitSubReg)
          .addImm(ind);
      LatestReg = NewReg;
    }

    UseMO.setReg(LatestReg);
  }

  return Changed;
}

bool InitUndef::fixupIllOperand(MachineInstr *MI, MachineOperand &MO) {

  LLVM_DEBUG(
      dbgs() << "Emitting PseudoInitUndef Instruction for implicit register "
             << printReg(MO.getReg()) << '\n');

  const TargetRegisterClass *TargetRegClass = MRI->getRegClass(MO.getReg());
  LLVM_DEBUG(dbgs() << "Register Class ID" << TargetRegClass->getID() << "\n");
  Register NewReg = MRI->createVirtualRegister(TargetRegClass);
  BuildMI(*MI->getParent(), MI, MI->getDebugLoc(),
          TII->get(TargetOpcode::INIT_UNDEF), NewReg);
  MO.setReg(NewReg);
  if (MO.isUndef())
    MO.setIsUndef(false);
  return true;
}

bool InitUndef::processBasicBlock(MachineFunction &MF, MachineBasicBlock &MBB,
                                  const DeadLaneDetector *DLD) {
  bool Changed = false;
  for (MachineBasicBlock::iterator I = MBB.begin(); I != MBB.end(); ++I) {
    MachineInstr &MI = *I;

    // If we used NoReg to represent the passthru, switch this back to being
    // an IMPLICIT_DEF before TwoAddressInstructions.
    unsigned UseOpIdx;
    if (MI.getNumDefs() != 0 && MI.isRegTiedToUseOperand(0, &UseOpIdx)) {
      MachineOperand &UseMO = MI.getOperand(UseOpIdx);
      if (UseMO.getReg() == MCRegister::NoRegister) {
        const TargetRegisterClass *RC =
            TII->getRegClass(MI.getDesc(), UseOpIdx, TRI, MF);
        Register NewDest = MRI->createVirtualRegister(RC);
        // We don't have a way to update dead lanes, so keep track of the
        // new register so that we avoid querying it later.
        NewRegs.insert(NewDest);
        BuildMI(MBB, I, I->getDebugLoc(), TII->get(TargetOpcode::IMPLICIT_DEF),
                NewDest);
        UseMO.setReg(NewDest);
        Changed = true;
      }
    }

    if (isEarlyClobberMI(MI)) {
      if (MRI->subRegLivenessEnabled())
        Changed |= handleSubReg(MF, MI, *DLD);
      Changed |= handleReg(&MI);
    }
  }
  return Changed;
}

bool InitUndefLegacy::runOnMachineFunction(MachineFunction &MF) {
  return InitUndef().run(MF);
}

PreservedAnalyses InitUndefPass::run(MachineFunction &MF,
                                     MachineFunctionAnalysisManager &MFAM) {
  if (!InitUndef().run(MF))
    return PreservedAnalyses::all();
  auto PA = getMachineFunctionPassPreservedAnalyses();
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

bool InitUndef::run(MachineFunction &MF) {
  ST = &MF.getSubtarget();

  // The pass is only needed if early-clobber defs and undef ops cannot be
  // allocated to the same register.
  if (!ST->requiresDisjointEarlyClobberAndUndef())
    return false;

  MRI = &MF.getRegInfo();
  TII = ST->getInstrInfo();
  TRI = MRI->getTargetRegisterInfo();

  bool Changed = false;
  std::unique_ptr<DeadLaneDetector> DLD;
  if (MRI->subRegLivenessEnabled()) {
    DLD = std::make_unique<DeadLaneDetector>(MRI, TRI);
    DLD->computeSubRegisterLaneBitInfo();
  }

  for (MachineBasicBlock &BB : MF)
    Changed |= processBasicBlock(MF, BB, DLD.get());

  for (auto *DeadMI : DeadInsts)
    DeadMI->eraseFromParent();
  DeadInsts.clear();
  NewRegs.clear();

  return Changed;
}
