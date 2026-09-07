//===----------------------- LinxV5TileFixup.cpp -------------------------===//
//
// Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// Move TileReg COPYs and spills to the block boundary. To avoid break scalar-tr
// live-ranges.
//   ......
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "linxv5-tile-fixup"
#define PASS_NAME "LinxV5 Tile Fixup"

namespace {

class LinxV5TileFixup : public MachineFunctionPass {
public:
  static char ID;

  LinxV5TileFixup() : MachineFunctionPass(ID) {
    initializeLinxV5TileFixupPass(*PassRegistry::getPassRegistry());
  }
  virtual ~LinxV5TileFixup() = default;

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool Hoist(MachineBasicBlock &MBB);

  bool AnnotateTileSizes(MachineFunction &MF);

  bool isTileReg(Register Reg) {
    return LinxV5::Tile_ABSRegClass.contains(Reg);
  }

  bool DependTileRegs(const MachineInstr &MI) {
    if (LinxV5::isTileOp(MI))
      return true;
    if (MI.isImplicitDef() && isTileReg(MI.getOperand(0).getReg()))
      return true;
    if (MI.isKill() && isTileReg(MI.getOperand(0).getReg()))
      return true;
    return false;
  }

private:
  MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
};
char LinxV5TileFixup::ID = 0;

bool LinxV5TileFixup::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  MRI = &MF.getRegInfo();
  TII = MF.getSubtarget().getInstrInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= Hoist(MBB);
  }
  Changed |= AnnotateTileSizes(MF);

  return Changed;
}

bool LinxV5TileFixup::Hoist(MachineBasicBlock &MBB) {
  bool Changed = false;
  auto Boundary = MBB.begin();
  for (auto MBBI = MBB.begin(), MBBE = MBB.end(); MBBI != MBBE;) {
    MachineInstr *MI = &*MBBI++;
    if ((MI->isCopy() && isTileReg(MI->getOperand(0).getReg())) ||
        MI->getOpcode() == LinxV5::PseudoTSpill ||
        MI->getOpcode() == LinxV5::PseudoTReload) {
      if (MI->getIterator() != Boundary) {
        MBB.splice(Boundary, &MBB, MI);
        Changed = true;
      }
      Boundary = std::next(MI->getIterator());
    } else if (DependTileRegs(*MI)) {
      Boundary = std::next(MI->getIterator());
    }
  }
  return Changed;
}

// Adjust the stack slot size of the Tile register
bool LinxV5TileFixup::AnnotateTileSizes(MachineFunction &MF) {
  bool Changed = false;
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == LinxV5::PseudoTSpill) {
        Register TileReg = MI.getOperand(0).getReg();

        unsigned RegSizeCode = LinxV5::getTileRegSize(MBB, MI, TileReg, true);
        if (RegSizeCode != -1u) {
          unsigned RegSize = 1 << (RegSizeCode + 6);
          int FI = MI.getOperand(1).getIndex();
          MFI.setObjectSize(FI, RegSize);
          MFI.setObjectAlignment(FI, Align(256));
          Changed = true;
        }
      }
    }
  }
  return Changed;
}

} // namespace

INITIALIZE_PASS(LinxV5TileFixup, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {

FunctionPass *createLinxV5TileFixupPass() { return new LinxV5TileFixup(); }

} // namespace llvm
