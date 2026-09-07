//===-- LinxV5InstrInfo.cpp - LinxV5 Instruction Information ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "LinxV5InstrInfo.h"
#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5Subtarget.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRangeEdit.h"
#include "llvm/CodeGen/LiveRegMatrix.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

#include <queue>

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRMAP_INFO
#include "LinxV5GenInstrInfo.inc"

#define DEBUG_TYPE "linxv5-instr-info"

static cl::opt<bool> DisableConstCheap(
    "disable-linxv5-const-cheap", cl::Hidden, cl::init(false),
    cl::desc("Do not treate an immediate/symbol as cheap as a move"));

static cl::opt<bool> EnableBFIOpt(
    "linxv5-enable-bfi-opt", cl::Hidden, cl::init(true),
    cl::desc("Optimize symmetric 64-bit const inst sequence with BFI."));

STATISTIC(NumStoreReg, "Number of store regs to stack slots");
STATISTIC(NumTileSpill, "Number of jcore-tile reg spills");
STATISTIC(NumTileReload, "Number of jcore-tile reg reloads");

static inline MachineOperand *getAsmOperand(MachineInstr *MI, unsigned Val) {
  unsigned OpNo = InlineAsm::MIOp_FirstOperand;

  // Scan to find the machine operand number for the operand.
  for (; Val; --Val) {
    if (OpNo >= MI->getNumOperands())
      break;
    unsigned OpFlags = MI->getOperand(OpNo).getImm();
    OpNo += InlineAsm::getNumOperandRegisters(OpFlags) + 1;
  }

  // We may have a location metadata attached to the end of the
  // instruction, and at no point should see metadata at any
  // other point while processing. It's an error if so.
  if (OpNo >= MI->getNumOperands() || MI->getOperand(OpNo).isMetadata()) {
    return nullptr;
  } else {
    unsigned OpFlags = MI->getOperand(OpNo).getImm();
    ++OpNo; // Skip over the ID number.
    return &MI->getOperand(OpNo);
  }
}

static inline const char *parseAsmOperand(MachineInstr *MI, const char *p,
                                          MachineOperand *&MO) {
  if (*p != '$')
    return nullptr;
  ++p;
  bool hasBraces = false;
  if (*p == '{') {
    hasBraces = true;
    ++p;
  }
  unsigned NumDigits = 0;
  while (isDigit(p[NumDigits]))
    ++NumDigits;
  unsigned Val = 0;
  if (StringRef(p, NumDigits).getAsInteger(10, Val))
    report_fatal_error("Invalid inline asm operand number!");
  MO = getAsmOperand(MI, Val);

  p += NumDigits;

  if (hasBraces) {
    while (*p++ != '}')
      ;
  }
  return p;
}

// parse '${1}<${2}>'
static inline const char *
parseTileDstWithSize(LinxV5::SingleAsm &SA, MachineInstr *MI, const char *p) {
  MachineOperand *Dst = nullptr;
  p = parseAsmOperand(MI, p, Dst);

  if (!p || !Dst)
    return nullptr;

  // v5: "->$N" without <size> — size is now TSize=%c[N] earlier in the
  // string, not embedded in the dst operand. Record def with size=0
  // (implicit/unknown); callers handle size=0 by falling back to other
  // size sources (e.g. TileDType).
  if (*p != '<') {
    // v5 form: ->dst without <size>
    SA.Defs.push_back(Dst);
    SA.Sizes.push_back(0);
    return p;
  }

  // v4 form: ->dst<size>
  MachineOperand *Size = nullptr;
  p = parseAsmOperand(MI, p + 1, Size);

  if (!p)
    return nullptr;
  if (*p != '>')
    return nullptr;
  SA.Defs.push_back(Dst);
  SA.Sizes.push_back(Size->getImm());
  return p + 1;
}

static inline const char *skipSeparator(const char *p) {
  while (*p == ',' || *p == ' ' || *p == '\t')
    ++p;
  return p;
}

LinxV5::SingleAsm LinxV5::parseSingleAsm(MachineInstr *MI) {
  LinxV5::SingleAsm SA;
  assert(MI->isInlineAsm() && "parseSingleAsm only works on inline asms");

  // Count the number of register definitions to find the asm string.
  unsigned NumDefs = 0;
  for (; MI->getOperand(NumDefs).isReg() && MI->getOperand(NumDefs).isDef();
       ++NumDefs)
    assert(NumDefs != MI->getNumOperands() - 2 && "No asm string?");

  assert(MI->getOperand(NumDefs).isSymbol() && "No asm string?");

  // Disassemble the AsmStr, printing out the literal pieces, the operands, etc.
  const char *AsmStr = MI->getOperand(NumDefs).getSymbolName();
  const char *p = AsmStr;
  while (*p) {
    if (StringRef(p).startswith("->")) {
      p += 2;
      const char *n;
      do {
        n = parseTileDstWithSize(SA, MI, p);
        if (!n)
          break;
        n = skipSeparator(n);
        p = n;
      } while (true);
    }
    ++p;
  }
  return SA;
}

bool LinxV5::isTileOp(const MachineInstr &MI) {
  if (MI.isInlineAsm()) {
    const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
    for (auto &MO : MI.operands()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (Reg.isPhysical() && LinxV5::Tile_ABSRegClass.contains(Reg))
        return true;
      if (Reg.isVirtual() &&
          LinxV5::Tile_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg)))
        return true;
    }
    return false;
  }
  uint64_t TSFlags = MI.getDesc().TSFlags;
  return LinxV5II::isTileOp(TSFlags);
}

unsigned LinxV5::getTileOpRegSize(MachineInstr &MI, Register Reg) {
  if (MI.getOpcode() == LinxV5::PseudoTCOPY) {
    return MI.getOperand(1).getImm();
  } else if (MI.isInlineAsm()) {
    LinxV5::SingleAsm SA = parseSingleAsm(&MI);
    for (int i = 0; i < SA.Defs.size(); ++i) {
      if (SA.Defs[i]->getReg() == Reg)
        return SA.Sizes[i];
    }
    report_fatal_error("Can not find tile definition in inline asm");
  } else {
    // FIXME: this is a magic num
    unsigned di = 0;
    for (; di < MI.getNumExplicitDefs(); ++di) {
      if (MI.getOperand(di).getReg() == Reg)
        break;
    }
    if (di == MI.getNumExplicitDefs())
      report_fatal_error("Can not find tile definition in machine instruction");
    unsigned Idx = MI.getNumExplicitDefs() + 7 + di;
    return MI.getOperand(Idx).getImm();
  }
}

bool LinxV5::isIsolateInstr(MachineInstr &MI) {
  uint64_t TSFlags = MI.getDesc().TSFlags;
  const LinxV5Subtarget &STI =
      MI.getParent()->getParent()->getSubtarget<LinxV5Subtarget>();
  return LinxV5II::isTileOp(TSFlags) || LinxV5II::isHeaderOnly(TSFlags) ||
         (MI.isInlineAsm() && !STI.isSIMT());
}

bool LinxV5::isRAInstrOfInlineASMBlock(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case TargetOpcode::COPY:
  case LinxV5::LDI:
  case LinxV5::SDI:
  case LinxV5::ADDI:
  case LinxV5::SUBI:
  case LinxV5::ORI:
  case LinxV5::XORI:
  case LinxV5::LUI:
    return true;
  default:
    return false;
  }
}

bool LinxV5::isPhysScratchRegAvailable(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator At,
                                       Register Reg) {
  for (auto &MI : make_range(At, MBB.end())) {
    // meet use first, busy if not undef.
    if (auto *MO = MI.findRegisterUseOperand(Reg)) {
      if (MO->isUndef())
        continue;
      return false;
    }

    // meet def first, idle.
    if (MI.findRegisterDefOperand(Reg))
      return true;
  }
  // didn't meet any use or def, idle.
  return true;
}

unsigned LinxV5::getSizeFromSIMTType(unsigned DstTypeEnum) {
  switch (DstTypeEnum & 3) {
  case 0:
    return SIMTRegSize::SIMT_REG_SIZE_D; // D type
  case 1:
    return SIMTRegSize::SIMT_REG_SIZE_W; // W type
  case 2:
    return SIMTRegSize::SIMT_REG_SIZE_H; // H type
  case 3:
    return SIMTRegSize::SIMT_REG_SIZE_B; // B type
  default:
    report_fatal_error("unexpected SIMT Reg Type!");
  }
}

unsigned LinxV5::getSIMTDstTypeFromBits(unsigned Bits) {
  switch (Bits) {
  case 64:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D; // D type
  case 32:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W; // W type
  case 16:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H; // H type
  case 8:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B; // B type
  default:
    report_fatal_error("unexpected reg bits!");
  }
}

unsigned LinxV5::getSIMTDstTypeFromSize(unsigned RegSize) {
  switch (RegSize) {
  case SIMTRegSize::SIMT_REG_SIZE_D:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D; // D type
  case SIMTRegSize::SIMT_REG_SIZE_W:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W; // W type
  case SIMTRegSize::SIMT_REG_SIZE_H:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H; // H type
  case SIMTRegSize::SIMT_REG_SIZE_B:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B; // B type
  default:
    report_fatal_error("unexpected reg size!");
  }
}

unsigned LinxV5::getSIMTSrcTypeFromSize(unsigned RegSize) {
  switch (RegSize) {
  case SIMTRegSize::SIMT_REG_SIZE_D:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD; // D type
  case SIMTRegSize::SIMT_REG_SIZE_W:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW; // W type
  case SIMTRegSize::SIMT_REG_SIZE_H:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH; // H type
  case SIMTRegSize::SIMT_REG_SIZE_B:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB; // B type
  default:
    report_fatal_error("unexpected reg size!");
  }
}

unsigned LinxV5::getUseRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI,
                                            MCRegister Reg, bool &FindDef) {
  for (MachineInstr &MI : make_range(MBBI, MBB.end())) {
    if (MI.readsRegister(Reg)) {
      if (MI.getOpcode() == TargetOpcode::COPY) {
        Register DstReg = MI.getOperand(0).getReg();
        return LinxV5::getUseRegSize(MBB, MI.getIterator(), DstReg);
      }
      int UseIdx = MI.findRegisterUseOperandIdx(Reg);
      if (!MI.getOperand(UseIdx + 1).isImm())
        return SIMTRegSize::SIMT_REG_SIZE_W;
      unsigned UseRegTypeEnum = MI.getOperand(UseIdx + 1).getImm();
      return LinxV5::getSizeFromSIMTType(UseRegTypeEnum);
    }
    if (MI.definesRegister(Reg)) {
      FindDef = true;
      return 0;
    }
  }
  return 0;
}

// Return 0 if no UseReg found (redefined before use)
unsigned LinxV5::getUseRegSize(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI,
                               MCRegister Reg) {
  bool FindDef = false;
  unsigned RegSize =
      getUseRegSizeAtSingleBlock(MBB, std::next(MBBI), Reg, FindDef);
  if (FindDef)
    return 0;

  if (RegSize)
    return RegSize;

  std::queue<MachineBasicBlock *> Queue;
  SmallDenseSet<MachineBasicBlock *> Candidates; // for loop
  Candidates.insert(&MBB);
  for (MachineBasicBlock *Succ : MBB.successors()) {
    if (!Candidates.count(Succ) || Succ == &MBB) {
      Queue.push(Succ);
      Candidates.insert(Succ);
    }
  }

  while (!Queue.empty()) {
    MachineBasicBlock *Top = Queue.front();
    Queue.pop();
    FindDef = false;
    RegSize = getUseRegSizeAtSingleBlock(*Top, Top->begin(), Reg, FindDef);
    if (RegSize)
      return RegSize;
    if (FindDef)
      continue;
    for (MachineBasicBlock *Succ : Top->successors()) {
      if (!Candidates.count(Succ)) {
        Queue.push(Succ);
        Candidates.insert(Succ);
      }
    }
  }

  return 0;
}

static unsigned getTileRegSizeImpl(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MCRegister Reg, bool isSpill);

static unsigned getTileRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI,
                                            MCRegister Reg, bool isSpill) {
  for (MachineInstr &MI : reverse(make_range(MBB.begin(), MBBI))) {
    for (int i = 0; i < MI.getNumOperands(); ++i) {
      auto &MO = MI.getOperand(i);
      if (!MO.isReg() || !MO.isDef())
        continue;
      if (MO.getReg() != Reg)
        continue;
      if (MI.getOpcode() == TargetOpcode::COPY) {
        Register SrcReg = MI.getOperand(1).getReg();
        return getTileRegSizeImpl(MBB, MI.getIterator(), SrcReg, isSpill);
      }
      if (MI.getOpcode() == LinxV5::PseudoTReload && isSpill)
        return 0;
      if (MI.getOpcode() == LinxV5::IMPLICIT_DEF)
        return 0;
      unsigned RegSizeMask = LinxV5::getTileOpRegSize(MI, Reg);
      return RegSizeMask + 1;
    }
  }
  return 0;
}

static unsigned getTileRegSizeImpl(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MCRegister Reg, bool isSpill) {
  unsigned RegSize =
      getTileRegSizeAtSingleBlock(MBB, std::next(MBBI), Reg, isSpill);
  if (RegSize)
    return RegSize;

  std::queue<MachineBasicBlock *> Queue;
  SmallDenseSet<MachineBasicBlock *> Candidates; // for loop
  Candidates.insert(&MBB);
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    if (!Candidates.count(Pred)) {
      Queue.push(Pred);
      Candidates.insert(Pred);
    }
  }

  while (!Queue.empty()) {
    MachineBasicBlock *Top = Queue.front();
    Queue.pop();
    RegSize = getTileRegSizeAtSingleBlock(*Top, Top->end(), Reg, isSpill);
    if (RegSize)
      return RegSize;
    for (MachineBasicBlock *Pred : Top->predecessors()) {
      if (!Candidates.count(Pred)) {
        Queue.push(Pred);
        Candidates.insert(Pred);
      }
    }
  }
  // No error is reported here, and `getTileRegSize` returns -1(0 - 1) to allow
  // the frame index correction to be determined.
  if (isSpill)
    return 0;

  report_fatal_error("Can not find Copy Use at SIMT Mode!");
}

unsigned LinxV5::getTileRegSize(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MCRegister Reg, bool isSpill) {
  return getTileRegSizeImpl(MBB, MBBI, Reg, isSpill) - 1;
}

bool LinxV5::enableBFIOpt() { return EnableBFIOpt; }

LinxV5InstrInfo::LinxV5InstrInfo(LinxV5Subtarget &STI)
    : LinxV5GenInstrInfo(LinxV5::ADJCALLSTACKDOWN, LinxV5::ADJCALLSTACKUP),
      STI(STI) {}

bool LinxV5InstrInfo::isSchedulingBoundary(const MachineInstr &MI,
                                           const MachineBasicBlock *MBB,
                                           const MachineFunction &MF) const {
  if (TargetInstrInfo::isSchedulingBoundary(MI, MBB, MF))
    return true;

  if (LinxV5::isTileOp(MI))
    return true;

  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT() &&
      MI.modifiesRegister(LinxV5::SIMT_P, STI.getRegisterInfo()))
    return true;

  return false;
}

MachineBasicBlock::iterator
LinxV5InstrInfo::getFirstInsertionPoint(MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator I = TargetInstrInfo::getFirstInsertionPoint(MBB);
  if (!MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT())
    return I;

  while (I != MBB.end()) {
    unsigned Opc = I->getOpcode();
    if (Opc != LinxV5::LinxV5PseudoCopy2P &&
        Opc != LinxV5::LinxV5PseudoCopy2PTerm)
      break;
    ++I;
  }
  return I;
}

unsigned LinxV5InstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                              int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  // LinxV5's instructions.
  case LinxV5::LBI:
  case LinxV5::LHI:
  case LinxV5::LWI:
  case LinxV5::LDI:
  case LinxV5::LBUI:
  case LinxV5::LHUI:
  case LinxV5::LWUI:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

unsigned LinxV5InstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case LinxV5::SBI:
  case LinxV5::SHI:
  case LinxV5::SWI:
  case LinxV5::SDI:
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

void LinxV5InstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI,
                                  const DebugLoc &DL, MCRegister DstReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  if (SrcReg == LinxV5::Tile_ACC1 || DstReg == LinxV5::Tile_ACC1)
    report_fatal_error("could not copy phys `acc' reg, please manually copy it "
                       "to vec tile reg by blk_acccvt()/TCVT()");
  auto *TRI =
      MBB.getParent()->getSubtarget<LinxV5Subtarget>().getRegisterInfo();
  auto &MRI = MBB.getParent()->getRegInfo();
  if (MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT()) {
    assert(!LinxV5::GRRegClass.contains(DstReg) && "must reduce inst!");
    assert(!(LinxV5::SIMTCGSLRegClass.contains(DstReg) &&
             !TRI->isUniformReg(MRI, SrcReg, false)) &&
           "invalid copy VecReg to ScalarReg!");
    unsigned RegSize = 0;
    RegSize = LinxV5::getUseRegSize(MBB, MBBI, DstReg);
    if (RegSize && LinxV5::SIMTCGSLRegClass.contains(DstReg))
      BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_ORI_SCAR), DstReg)
          .addImm(LinxV5::getSIMTDstTypeFromSize(RegSize))
          .addReg(SrcReg, getKillRegState(KillSrc))
          .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSize))
          .addImm(0);
    else if (RegSize && LinxV5::SIMTCGVLRegClass.contains(DstReg))
      BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_MOV), DstReg)
          .addImm(LinxV5::getSIMTDstTypeFromSize(RegSize))
          .addReg(SrcReg, getKillRegState(KillSrc))
          .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSize));
    else {
      // dead copy, no user
      if (LinxV5::SIMTCGSLRegClass.contains(DstReg))
        BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_ORI_SCAR), DstReg)
            .addImm(LinxV5::getSIMTDstTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addImm(LinxV5::getSIMTSrcTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addImm(0);
      else if (LinxV5::SIMTCGVLRegClass.contains(DstReg))
        BuildMI(MBB, MBBI, DL, get(LinxV5::SIMT_MOV), DstReg)
            .addImm(LinxV5::getSIMTDstTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W))
            .addReg(SrcReg, getKillRegState(KillSrc))
            .addImm(LinxV5::getSIMTSrcTypeFromSize(
                LinxV5::SIMTRegSize::SIMT_REG_SIZE_W));
      else {
        errs() << *MBBI;
        report_fatal_error("unexpected copy");
      }
    }
  } else if (LinxV5::Tile_ABS_CGRegClass.contains(DstReg)) {
    unsigned RegSizeCode = LinxV5::getTileRegSize(MBB, MBBI, SrcReg, false);
    BuildMI(MBB, MBBI, DL, get(LinxV5::PseudoTCOPY), DstReg)
        .addImm(RegSizeCode)
        .addReg(SrcReg, getKillRegState(KillSrc));
  } else {
    BuildMI(MBB, MBBI, DL, get(LinxV5::ORI), DstReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addImm(0);
  }

  return;
}

void LinxV5InstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          Register SrcReg, bool IsKill, int FI,
                                          const TargetRegisterClass *RC,
                                          const TargetRegisterInfo *TRI) const {
  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  const MachineFrameInfo &MFI = MF->getFrameInfo();
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  ++NumStoreReg;

  bool IsSIMT = MF->getSubtarget<LinxV5Subtarget>().isSIMT();
  if (IsSIMT) {
    BuildMI(MBB, I, DL, get(LinxV5::PseudoVecSpill))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addImm(0) // RegSizeInBytes, 0 for scalar
        .addFrameIndex(FI)
        .addMemOperand(MMO);
    return;
  }

  if (LinxV5::Tile_ABSRegClass.hasSubClassEq(RC)) {
    ++NumTileSpill;
    BuildMI(MBB, I, DL, get(LinxV5::PseudoTSpill))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FI)
        .addMemOperand(MMO);
  } else {
    BuildMI(MBB, I, DL, get(LinxV5::SDI))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

void LinxV5InstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator I, Register DstReg,
    int FI, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI) const {

  DebugLoc DL;
  if (I != MBB.end())
    DL = I->getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  const MachineFrameInfo &MFI = MF->getFrameInfo();
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
      MFI.getObjectSize(FI), MFI.getObjectAlign(FI));

  if (!LinxV5::GRRegClass.hasSubClassEq(RC) &&
      !LinxV5::LTRRegClass.hasSubClassEq(RC) &&
      !LinxV5::LURRegClass.hasSubClassEq(RC) &&
      !LinxV5::MixedGPRRegClass.hasSubClassEq(RC) &&
      !LinxV5::MixedGPRNoRARegClass.hasSubClassEq(RC) &&
      !LinxV5::Tile_ABSRegClass.hasSubClassEq(RC) &&
      !LinxV5::SIMTCGVRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't load this register from stack slot");

  bool IsSIMT = MF->getSubtarget<LinxV5Subtarget>().isSIMT();
  if (IsSIMT) {
    BuildMI(MBB, I, DL, get(LinxV5::PseudoVecReload), DstReg)
        .addImm(0)
        .addFrameIndex(FI)
        .addMemOperand(MMO);
    return;
  }

  if (LinxV5::Tile_ABSRegClass.hasSubClassEq(RC)) {
    ++NumTileReload;
    BuildMI(MBB, I, DL, get(LinxV5::PseudoTReload), DstReg)
        .addFrameIndex(FI)
        .addMemOperand(MMO);
  } else {  // Scalar Regs
    BuildMI(MBB, I, DL, get(LinxV5::LDI), DstReg)
        .addFrameIndex(FI)
        .addImm(0)
        .addMemOperand(MMO);
  }
}

static void generateInt32InstSeq(int64_t Val, LinxV5MatInt::InstSeq &Res) {
  if (Val < 0 && isUInt<12>(-Val)) {
    Res.push_back(LinxV5MatInt::Inst(LinxV5::SUBI, -Val));
    return;
  }

  int64_t Lo12 = (uint64_t)Val & 0xfff;
  int64_t Hi20 = Val >> 12;
  assert(isInt<20>(Hi20) && "unexpected Int32!");

  if (Hi20)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::LUI, int64_t(Hi20)));

  if (Lo12 || Hi20 == 0) {
    Res.push_back(LinxV5MatInt::Inst(LinxV5::ADDI, Lo12));
  }
}

static void generateUInt32InstSeq(uint64_t Val, LinxV5MatInt::InstSeq &Res,
                                  bool ZeroExt) {
  int64_t SVal = (int64_t)((int32_t)Val);

  generateInt32InstSeq(SVal, Res);

  if ((uint64_t)SVal != Val && ZeroExt)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::OR_UW, 0));
}

static int getSymmetricWidth(uint64_t Val) {
  int Width = 32;
  uint64_t UMask = 0xFFFFFFFFFFFFFFFFU;
  uint64_t LMask = 0xFFFFFFFF;
  while ((((Val >> Width) ^ Val) & LMask) != 0) {
    Width--;
    LMask >>= 1;
    UMask >>= 2;
  }
  if ((Val & (~UMask)) == 0)
    return Width;
  else
    return 0;
}

void LinxV5::generateMatIntSeq(int64_t Val, LinxV5MatInt::InstSeq &Res,
                               bool HasFloat) {
  if (isInt<32>(Val)) {
    generateInt32InstSeq(Val, Res);
    return;
  } else if (isUInt<32>(Val)) {
    generateUInt32InstSeq(Val, Res, true);
    return;
  }

  uint64_t Hi32 = (static_cast<uint64_t>(Val) >> 32) & 0xFFFFFFFF;
  uint64_t Lo32 = static_cast<uint64_t>(Val) & 0xFFFFFFFF;
  if (enableBFIOpt() && !HasFloat) {
    // Optimize symmetric 64-bit long const with BFI
    int SymmWidth = getSymmetricWidth(static_cast<uint64_t>(Val));
    if (SymmWidth >= 16 && SymmWidth % 8 == 0) {
      uint64_t NewVal =
          static_cast<uint64_t>(Val) >> static_cast<unsigned>(SymmWidth);
      if (isUInt<32>(NewVal)) {
        generateUInt32InstSeq(NewVal, Res, false);
      } else {
        generateInt32InstSeq(NewVal, Res);
      }
      unsigned M = static_cast<unsigned>(SymmWidth) / 8;
      unsigned N = static_cast<unsigned>(SymmWidth);
      Res.push_back(LinxV5MatInt::Inst(LinxV5::HL_BFI, N + M));
      return;
    }
  }

  // General 64-bit long const
  int64_t Lo = SignExtend64<13>(Val);
  // addi + subi can only present [-4095, 4095], and present 0 twice.
  // but the full range of 13bit integer is [-4096, 4095].
  // so we pick Lo12 for -4096.
  if (Lo == (-4096)) {
    Lo = SignExtend64<12>(Val);
  }
  Val = (uint64_t)Val - (uint64_t)Lo;
  unsigned Shamt = 0;
  // 0xffffffff7fffffff - (-1) = 0xffffffff80000000 is int32
  if (!isInt<32>(Val)) {
    Shamt = findFirstSet(static_cast<uint64_t>(Val));
    assert(Shamt >= 12 && "unexpected trailing zero amount!");
    Val >>= Shamt;
    if (!isInt<12>(Val) && isInt<20>(Val)) {
      Val = (uint64_t)Val << 12;
      Shamt -= 12;
    }
  }

  generateMatIntSeq(Val, Res, HasFloat);

  if (Shamt)
    Res.push_back(LinxV5MatInt::Inst(LinxV5::SLLI, Shamt));

  if (Lo) {
    if (Lo >= 0)
      Res.push_back(LinxV5MatInt::Inst(LinxV5::ADDI, Lo));
    else
      Res.push_back(LinxV5MatInt::Inst(LinxV5::SUBI, -Lo));
  }
}

unsigned LinxV5::getSIMTDstType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_B;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_H;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_W;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_D;
  default:
    return LinxV5Op::SIMT_INT_DST_REG_TYPE_NONE;
  }
}

unsigned LinxV5::getSIMTSignedSrcType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SB;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SH;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SW;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD;
  default:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE;
  }
}

unsigned LinxV5::getSIMTUnsignedSrcType(const MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH;
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW;
  case MVT::i64:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD;
  default:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_NONE;
  }
}

void LinxV5::generateSIMTMatIntSeq(int64_t Val, LinxV5MatInt::SIMTInstSeq &Res,
                                   MVT VT) {
  LinxV5MatInt::InstSeq Seq;
  generateMatIntSeq(Val, Seq, true);
  unsigned DstType = getSIMTDstType(VT);
  unsigned SrcType = getSIMTSignedSrcType(VT);

  for (auto &I : Seq) {
    switch (I.Opc) {
    case LinxV5::LUI:
      Res.push_back(
          LinxV5MatInt::SIMTInst(LinxV5::LUI, DstType, SrcType, I.Imm));
      break;
    case LinxV5::ADDI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_ADDI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    case LinxV5::SUBI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_SUBI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    case LinxV5::OR_UW:
      if (VT == MVT::i64) {
        Res.back().DstType = getSIMTDstType(MVT::i32);
        Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_ICVT_U322U64, DstType,
                                             getSIMTUnsignedSrcType(MVT::i32),
                                             I.Imm));
      }
      break;
    case LinxV5::SLLI:
      Res.push_back(LinxV5MatInt::SIMTInst(LinxV5::SIMT_SLLI_SCAR, DstType,
                                           SrcType, I.Imm));
      break;
    default:
      report_fatal_error("unrecognized MatInt Opcode!");
    }
  }
}

void LinxV5InstrInfo::movImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             const DebugLoc &DL, Register DstReg, uint64_t Val,
                             MachineInstr::MIFlag Flag) const {
  int64_t Imm = (int64_t)Val;

  LinxV5MatInt::InstSeq Seq;
  Register SrcReg = LinxV5::R0;
  LinxV5::generateMatIntSeq(Imm, Seq, false);

  for (LinxV5MatInt::Inst &I : Seq) {
    if (I.Opc == LinxV5::LUI) {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg).addImm(I.Imm).setMIFlag(Flag);
    } else if (I.Opc == LinxV5::OR_UW) {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg)
          .addReg(LinxV5::R0)
          .addReg(SrcReg)
          .addImm(I.Imm)
          .setMIFlag(Flag);
    } else {
      BuildMI(MBB, MBBI, DL, get(I.Opc), DstReg)
          .addReg(SrcReg)
          .addImm(I.Imm)
          .setMIFlag(Flag);
    }

    SrcReg = DstReg;
  }
}

// The contents of values added to Cond are not examined outside of
// LinxV5InstrInfo, giving us flexibility in what to push to it. For LinxV5, we
// push BranchOpcode, Reg1, Reg2.
static void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  // Block ends with fall-through condbranch.
  assert(LastInst.getDesc().isConditionalBranch() &&
         "Unknown conditional branch");
  Target = LastInst.getOperand(2).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));
  Cond.push_back(LastInst.getOperand(0));
  Cond.push_back(LastInst.getOperand(1));
}

bool LinxV5InstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *&TBB,
                                    MachineBasicBlock *&FBB,
                                    SmallVectorImpl<MachineOperand> &Cond,
                                    bool AllowModify) const {
  // FIX ME: when simt ready here.
  if (STI.isSIMT())
    return true;

  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse();
       J != MBB.rend() && (J->isDebugInstr() || isUnpredicatedTerminator(*J));
       J++) {
    if (J->isDebugInstr())
      continue;
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // We can't handle blocks that end in an indirect branch.
  if (I->getDesc().isIndirectBranch())
    return true;

  // We can't handle blocks with more than 2 terminators.
  if (NumTerminators > 2)
    return true;

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
  return true;
}

unsigned LinxV5InstrInfo::removeBranch(MachineBasicBlock &MBB,
                                       int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->getDesc().isUnconditionalBranch() &&
      !I->getDesc().isConditionalBranch())
    return 0;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();

  I = MBB.end();
  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned LinxV5InstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 3 || Cond.size() == 0) &&
         "LinxV5 branch conditions have two components!");

  // Unconditional branch.
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(LinxV5::PseudoBR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  // Either a one or two-way conditional branch.
  unsigned Opc = static_cast<unsigned>(Cond[0].getImm());
  MachineInstr &CondMI =
      *BuildMI(&MBB, DL, get(Opc)).add(Cond[1]).add(Cond[2]).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);

  // One-way conditional branch.
  if (!FBB)
    return 1;

  // Two-way conditional branch.
  MachineInstr &MI = *BuildMI(&MBB, DL, get(LinxV5::PseudoBR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(MI);
  return 2;
}

bool LinxV5InstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() == 3) && "Invalid branch condition!");
  Cond[0].setImm(getOppositeOpcode(Cond[0].getImm()));
  return false;
}

MachineBasicBlock *
LinxV5InstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert(MI.getDesc().isBranch() && "Unexpected opcode!");
  // The branch target is always the last operand.
  unsigned NumOp = MI.getNumExplicitOperands();
  return MI.getOperand(NumOp - 1).getMBB();
}

unsigned LinxV5InstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  unsigned Opcode = MI.getOpcode();

  switch (Opcode) {
  default:
    return get(Opcode).getSize();
  case TargetOpcode::EH_LABEL:
  case TargetOpcode::IMPLICIT_DEF:
  case TargetOpcode::KILL:
  case TargetOpcode::DBG_VALUE:
    return 0;
  case TargetOpcode::INLINEASM:
  case TargetOpcode::INLINEASM_BR: {
    const MachineFunction &MF = *MI.getParent()->getParent();
    const auto &TM = static_cast<const LinxV5TargetMachine &>(MF.getTarget());
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *TM.getMCAsmInfo());
  }
  }
}

bool LinxV5InstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case LinxV5::PseudoVBXCONST: {
    if (DisableConstCheap) {
      break;
    } else {
      return true;
    }
  }
  case LinxV5::ADDI:
  case LinxV5::ORI:
  case LinxV5::XORI:
  case LinxV5::SUBI:
    return (MI.getOperand(1).isReg() &&
            MI.getOperand(1).getReg() == LinxV5::R0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}

// Remat instruction hoist is not performed in the MachineLICM phase.
bool LinxV5InstrInfo::isHoistRemat(const MachineInstr &MI) const {
  return false;
}

bool LinxV5InstrInfo::isReallyTriviallyReMaterializable(
    const MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  case LinxV5::SIMT_ADD:
  case LinxV5::L_ADD_LI:
  case LinxV5::SIMT_ADDI_SCAR:
  case LinxV5::LUI:
    return true;
  default:
    return false;
  }
}

Optional<DestSourcePair>
LinxV5InstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.isMoveReg())
    return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
  return None;
}

bool LinxV5InstrInfo::verifyInstruction(const MachineInstr &MI,
                                        StringRef &ErrInfo) const {
  const MCInstrInfo *MCII = STI.getInstrInfo();
  MCInstrDesc const &Desc = MCII->get(MI.getOpcode());

  for (auto &OI : enumerate(Desc.operands())) {
    unsigned OpType = OI.value().OperandType;
    if (OpType >= LinxV5Op::OPERAND_FIRST_LinxV5_IMM &&
        OpType <= LinxV5Op::OPERAND_LAST_LinxV5_IMM) {
      const MachineOperand &MO = MI.getOperand(OI.index());
      if (MO.isImm()) {
        int64_t Imm = MO.getImm();
        bool Ok;
        switch (OpType) {
        case LinxV5Op::OPERAND_UIMM2:
          Ok = isUInt<2>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM3:
          Ok = isUInt<3>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM4:
          Ok = isUInt<4>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM5:
          Ok = isUInt<5>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM5_32:
          Ok = isUInt<5>(Imm-1);
          break;
        case LinxV5Op::OPERAND_UIMM6:
          Ok = isUInt<6>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM7:
          Ok = isUInt<7>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM3:
          Ok = isInt<3>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM4:
          Ok = isInt<4>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM5:
          Ok = isInt<5>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM6:
          Ok = isInt<6>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM7:
          Ok = isInt<7>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM8:
          Ok = isInt<8>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM9:
          Ok = isInt<9>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM10:
          Ok = isInt<10>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM11:
          Ok = isInt<11>(Imm);
          break;
        case LinxV5Op::OPERAND_NOT_SIMM12:
          Ok = !isInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM12:
          Ok = isUInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM12:
          Ok = isInt<12>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM16:
          Ok = isUInt<16>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM17:
          Ok = isUInt<17>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM24:
          Ok = isUInt<24>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM64:
          Ok = isInt<64>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMM20:
          Ok = isUInt<20>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM20:
          Ok = isInt<20>(Imm);
          break;
        case LinxV5Op::OPERAND_SIMM32:
          Ok = isInt<32>(Imm);
          break;
        case LinxV5Op::OPERAND_UIMMLOG2XLEN:
          if (STI.getTargetTriple().isArch64Bit())
            Ok = isUInt<6>(Imm);
          else
            Ok = isUInt<5>(Imm);
          break;
        default:
          llvm_unreachable("Unexpected operand type");
        }
        if (!Ok) {
          ErrInfo = "Invalid immediate";
          return false;
        }
      }
    }
  }

  return true;
}

const TargetRegisterClass *
LinxV5InstrInfo::getRegClass(const MCInstrDesc &MCID, unsigned OpNum,
                             const TargetRegisterInfo *TRI,
                             const MachineFunction &MF) const {
  if (LinxV5II::isTileOp(MCID.TSFlags)) {
    if (OpNum >= MCID.getNumOperands()) {
      return &LinxV5::GRNoR0RegClass;
    }
  }
  return TargetInstrInfo::getRegClass(MCID, OpNum, TRI, MF);
}

bool LinxV5InstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  assert(MIa.mayLoadOrStore() && "MIa must be a load or store.");
  assert(MIb.mayLoadOrStore() && "MIb must be a load or store.");
  return false;
}

std::pair<unsigned, unsigned>
LinxV5InstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  const unsigned Mask = LinxV5II::MO_DIRECT_FLAG_MASK;
  return std::make_pair(TF & Mask, TF & ~Mask);
}

ArrayRef<std::pair<unsigned, const char *>>
LinxV5InstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace LinxV5II;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_CALL, "linx-call"},           {MO_TPCREL, "linx-tpcrel"},
      {MO_TPREL_LO, "linx-tprel-lo"},   {MO_TPREL_HI, "linx-tprel-hi"},
      {MO_TPCREL_LO, "linx-tpcrel-lo"}, {MO_TPCREL_HI, "linx-tpcrel-hi"},
      {MO_TPREL, "linx-tprel"}};
  return makeArrayRef(TargetFlags);
}

unsigned LinxV5InstrInfo::getExtendedOpcode(unsigned OpBefore,
                                            unsigned Ext) const {
  if (Ext == 0) return OpBefore;
  unsigned OpAfter = LinxV5InstrExtensionMap[OpBefore][Ext - 1];
  assert(OpAfter != LinxV5::INSTRUCTION_LIST_END &&
         "Can not get the extended opcode for instruction which has no "
         "extension type!");
  return OpAfter;
}

#define OPPOSITE(Cond, Oppo)                                                   \
  {LinxV5::SETC_##Cond##_BR, LinxV5::SETC_##Oppo##_BR},                        \
      {LinxV5::SETC_##Cond##_SW_BR, LinxV5::SETC_##Oppo##_SW_BR},              \
      {LinxV5::SETC_##Cond##_UW_BR, LinxV5::SETC_##Oppo##_UW_BR}, {            \
    LinxV5::SETC_##Cond##I_BR, LinxV5::SETC_##Oppo##I_BR                       \
  }

static DenseMap<unsigned, unsigned> OppositeConditionInstrMap = {
    OPPOSITE(EQ, NE), OPPOSITE(NE, EQ),   OPPOSITE(LT, GE),
    OPPOSITE(GE, LT), OPPOSITE(LTU, GEU), OPPOSITE(GEU, LTU)
};
#undef OPPOSITE

unsigned LinxV5InstrInfo::getOppositeOpcode(unsigned Op) const {
  auto I = OppositeConditionInstrMap.find(Op);
  assert(I != OppositeConditionInstrMap.end() && "unknown condition opcode!");
  return I->second;
}
