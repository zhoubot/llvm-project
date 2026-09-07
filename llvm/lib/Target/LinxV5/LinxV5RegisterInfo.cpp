//===-- LinxV5RegisterInfo.cpp - LinxV5 Register Information ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "LinxV5RegisterInfo.h"
#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5Subtarget.h"
#include "LinxV5InstrInfo.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#define GET_REGINFO_TARGET_DESC
#include "LinxV5GenRegisterInfo.inc"

using namespace llvm;

#define DEBUG_TYPE "linx-register-info"

static cl::opt<bool> EnableSIMTSpillSaveRestoreP(
    "linxv5-simt-spill-save-restore-p", cl::Hidden,
    cl::desc("Wrap SIMT spill/reload with save/full-mask/restore of SIMT_P"),
    cl::init(false));

static_assert(LinxV5::R23 == LinxV5::R0 + 23, "Register list not consecutive");
static_assert(LinxV5::T4 == LinxV5::T1 + 3, "Register list not consecutive");
static_assert(LinxV5::U4 == LinxV5::U1 + 3, "Register list not consecutive");
static_assert(LinxV5::TOS4 == LinxV5::TOS1 + 3,
              "Register list not consecutive");
static_assert(LinxV5::UOS4 == LinxV5::UOS1 + 3,
              "Register list not consecutive");

static Register getSIMTSpillMaskScratchReg() { return LinxV5::T4; }

Register LinxV5::getSPReg() { return LinxV5::R1; }
Register LinxV5::getRAReg() { return LinxV5::R10; }
Register LinxV5::getFPReg() { return LinxV5::R11; } // s0
Register LinxV5::getBPReg() { return LinxV5::R12; } // s1
Register LinxV5::getUSPReg() { return LinxV5::U4; }
Register LinxV5::getVSPReg() { return LinxV5::SIMT_TO1; }

MCRegister LinxV5::getLoopCounter(unsigned Depth) {
  static const MCRegister Regs[3] = {LinxV5::SIMT_LC0, LinxV5::SIMT_LC1,
                                     LinxV5::SIMT_LC2};
  if (Depth >= 3)
    report_fatal_error("unexpected simt loop depth!");
  return Regs[Depth];
}

MCRegister LinxV5::getLoopBoundary(unsigned Depth) {
  static const MCRegister Regs[3] = {LinxV5::SIMT_LB0, LinxV5::SIMT_LB1,
                                     LinxV5::SIMT_LB2};
  if (Depth >= 3)
    report_fatal_error("unexpected simt loop depth!");
  return Regs[Depth];
}

MCRegister LinxV5::getReduceDst(unsigned RetNo) {
  static const MCRegister Regs[8] = {LinxV5::R2, LinxV5::R3, LinxV5::R4,
                                     LinxV5::R5, LinxV5::R6, LinxV5::R7,
                                     LinxV5::R8, LinxV5::R9};
  if (RetNo >= 4)
    report_fatal_error("reduce number exceeded 3?");
  return Regs[RetNo];
}

LinxV5RegisterInfo::LinxV5RegisterInfo(unsigned HwMode,
                                       const LinxV5Subtarget &ST)
    : LinxV5GenRegisterInfo(LinxV5::R10, /*DwarfFlavour*/ 0, /*EHFlavor*/ 0,
                            /*PC*/ 0, HwMode),
      STI(ST) {}

const TargetRegisterClass *LinxV5RegisterInfo::getSTDRC() const {
  return STI.enableLegacyISel() ? &LinxV5::VBXTRRegClass
                                : &LinxV5::MixedGPRRegClass;
}

unsigned LinxV5RegisterInfo::getRegPressureLimit(const TargetRegisterClass *RC,
                                                 MachineFunction &MF) const {
  switch (RC->getID()) {
  default:
    return LinxV5GenRegisterInfo::getRegPressureLimit(RC, MF);
  case LinxV5::MixedGPRRegClassID:
    return 24;
  }
}

unsigned LinxV5RegisterInfo::getRegPressureSetLimit(const MachineFunction &MF,
                                                    unsigned Idx) const {
  if (Idx == LinxV5::RegisterPressureSets::MixedGPR)
    return getRegPressureLimit(&LinxV5::MixedGPRRegClass,
                               const_cast<MachineFunction &>(MF));
  return LinxV5GenRegisterInfo::getRegPressureSetLimit(MF, Idx);
}

const uint32_t *
LinxV5RegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                         CallingConv::ID CC) const {
  auto &Subtarget = MF.getSubtarget<LinxV5Subtarget>();

  switch (Subtarget.getTargetABI()) {
  case LinxV5ABI::ABI_LP64:
    return CSR_ILP32_LP64_RegMask;
  default:
    llvm_unreachable("Unrecognized ABI");
  }
}

const MCPhysReg *
LinxV5RegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  auto &Subtarget = MF->getSubtarget<LinxV5Subtarget>();
  if (MF->getFunction().hasFnAttribute("interrupt"))
    assert(0 && "Unsupport interrupt!");

  switch (Subtarget.getTargetABI()) {
  case LinxV5ABI::ABI_LP64:
    return CSR_ILP32_LP64_SaveList;
  default:
    llvm_unreachable("Unrecognized ABI");
  }
}

BitVector LinxV5RegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  const LinxV5FrameLowering *TFI = STI.getFrameLowering();
  BitVector Reserved(getNumRegs());

  // Mark any registers requested to be reserved as such
  for (size_t Reg = 0; Reg < getNumRegs(); Reg++) {
    if (MF.getSubtarget<LinxV5Subtarget>().isRegisterReservedByUser(Reg))
      markSuperRegs(Reserved, Reg);
  }

  markSuperRegs(Reserved, LinxV5::CARG);

  // Use markSuperRegs to ensure any register aliases are also reserved
  // LinxV5's reserved registers
  markSuperRegs(Reserved, LinxV5::R0);
  markSuperRegs(Reserved, LinxV5::getSPReg()); // sp
  markSuperRegs(Reserved, LinxV5::getRAReg());

  // all loop regs are reserved
  for (MCRegister Reg : LinxV5::LoopRegRegClass) {
    markSuperRegs(Reserved, Reg);
  }

  if (MF.getInfo<LinxV5MachineFunctionInfo>()->isClockhandsAllocation()) {
    // Reserve all global gpr so we can let RegisterCoalescer to reduce
    // COPYs like:
    //   %1:mixedgpr = $a0
    // and
    //   $a0 = %1:mixedgpr
    // Generally, the RegisterCoalescer won't optimize for phys register
    // not reserved to avoid extends phys liverange effect RA. But LinxV5
    // split RA as two stage by two different regclass. Reserve all regs
    // of the first stage make no effects for the second stage.
    for (MCRegister Reg : LinxV5::GRRegClass) {
      markSuperRegs(Reserved, Reg);
    }
  }

  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT()) {
    for (MCRegister Reg : LinxV5::GRRegClass)
      markSuperRegs(Reserved, Reg);
    // Reverse Register when rolling ClockHand liveouts.
    // If we do not reserve one register, that hand will not have wiggle-room
    // for liveouts rolling.
    // TODO: Only reserve when hand used for value between blocks.
    markSuperRegs(Reserved, LinxV5::T4);
    markSuperRegs(Reserved, LinxV5::U4);
    markSuperRegs(Reserved, LinxV5::SIMT_VT4);
    markSuperRegs(Reserved, LinxV5::SIMT_VU4);
    markSuperRegs(Reserved, LinxV5::SIMT_VM4);
    markSuperRegs(Reserved, LinxV5::SIMT_VN4);
    markSuperRegs(Reserved, LinxV5::SIMT_P);
    for (MCRegister Reg : LinxV5::SIMT_TileBaseRegClass)
      markSuperRegs(Reserved, Reg);
    for (MCRegister Reg : LinxV5::SIMT_RIORegClass)
      markSuperRegs(Reserved, Reg);
  }

  // Temporarily reserve the x3 register as a temporary GPR required for
  // tilerig spilling.
  markSuperRegs(Reserved, LinxV5::R23);

  markSuperRegs(Reserved, LinxV5::Tile_T8);
  markSuperRegs(Reserved, LinxV5::Tile_U8);
  markSuperRegs(Reserved, LinxV5::Tile_M8);
  markSuperRegs(Reserved, LinxV5::Tile_N8);
  markSuperRegs(Reserved, LinxV5::Tile_S);

  if (TFI->hasFP(MF)) {
    markSuperRegs(Reserved, LinxV5::getFPReg()); // LinxV5's fp
  }
  // Reserve the base register if we need to realign the stack and allocate
  // variable-sized objects at runtime.
  if (TFI->hasBP(MF)) {
    markSuperRegs(Reserved, LinxV5ABI::getBPReg()); // bp
  }

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool LinxV5RegisterInfo::isConstantPhysReg(MCRegister Phys) const {
  return Phys == LinxV5::R0;
}

bool LinxV5RegisterInfo::isAsmClobberable(const MachineFunction &MF,
                                          MCRegister PhysReg) const {
  return !MF.getSubtarget<LinxV5Subtarget>().isRegisterReservedByUser(PhysReg);
}

const uint32_t *LinxV5RegisterInfo::getNoPreservedMask() const {
  return CSR_NoRegs_RegMask;
}

bool LinxV5RegisterInfo::shouldDoFullStageRAFromSpill(
    Register New, Register Spill, const MachineRegisterInfo *MRI) const {
  return MRI->getRegClass(Spill) == &LinxV5::GRRegClass &&
         MRI->getRegClass(New) != &LinxV5::GRRegClass;
}

static unsigned getScaleShift(unsigned Opc, int64_t Offset) {
  switch (Opc) {
  case LinxV5::LDI:
  case LinxV5::SDI:
  case LinxV5::SD:
  case LinxV5::SIMT_SCAR_SDI:
  case LinxV5::SIMT_SCAR_SDI_GLOBAL:
  case LinxV5::SIMT_LDI_SCAR:
  case LinxV5::SIMT_LDI_SCAR_GLOBAL:
  case LinxV5::SIMT_SDI:
  case LinxV5::SIMT_SDI_GLOBAL:
  case LinxV5::SIMT_LDI:
  case LinxV5::SIMT_LDI_GLOBAL:
    return 3;
  case LinxV5::LWI:
  case LinxV5::LWUI:
  case LinxV5::SWI:
  case LinxV5::SW:
  case LinxV5::SIMT_SCAR_SWI:
  case LinxV5::SIMT_SCAR_SWI_GLOBAL:
  case LinxV5::SIMT_LWI_SCAR:
  case LinxV5::SIMT_LWI_SCAR_GLOBAL:
  case LinxV5::SIMT_SWI:
  case LinxV5::SIMT_SWI_GLOBAL:
  case LinxV5::SIMT_LWI:
  case LinxV5::SIMT_LWI_GLOBAL:
    return 2;
  case LinxV5::LHI:
  case LinxV5::LHUI:
  case LinxV5::SHI:
  case LinxV5::SH:
  case LinxV5::SIMT_SCAR_SHI:
  case LinxV5::SIMT_SCAR_SHI_GLOBAL:
  case LinxV5::SIMT_LHI_SCAR:
  case LinxV5::SIMT_LHI_SCAR_GLOBAL:
  case LinxV5::SIMT_SHI:
  case LinxV5::SIMT_SHI_GLOBAL:
  case LinxV5::SIMT_LHI:
  case LinxV5::SIMT_LHI_GLOBAL:
    return 1;
  case LinxV5::LD:
  case LinxV5::LW:
  case LinxV5::LH:
  case LinxV5::LB: {
    unsigned Shift = countTrailingZeros((uint64_t)Offset);
    return Shift < 32 ? Shift : 31;
  }
  default:
    return 0;
  }
}

static bool isSIMTMemInstr(unsigned Opc) {
  switch (Opc) {
  case LinxV5::SIMT_SCAR_SDI:
  case LinxV5::SIMT_SCAR_SDI_GLOBAL:
  case LinxV5::SIMT_LDI_SCAR:
  case LinxV5::SIMT_LDI_SCAR_GLOBAL:
  case LinxV5::SIMT_SDI:
  case LinxV5::SIMT_SDI_GLOBAL:
  case LinxV5::SIMT_LDI:
  case LinxV5::SIMT_LDI_GLOBAL:
  case LinxV5::SIMT_SCAR_SWI:
  case LinxV5::SIMT_SCAR_SWI_GLOBAL:
  case LinxV5::SIMT_LWI_SCAR:
  case LinxV5::SIMT_LWI_SCAR_GLOBAL:
  case LinxV5::SIMT_SWI:
  case LinxV5::SIMT_SWI_GLOBAL:
  case LinxV5::SIMT_LWI:
  case LinxV5::SIMT_LWI_GLOBAL:
  case LinxV5::SIMT_SCAR_SHI:
  case LinxV5::SIMT_SCAR_SHI_GLOBAL:
  case LinxV5::SIMT_LHI_SCAR:
  case LinxV5::SIMT_LHI_SCAR_GLOBAL:
  case LinxV5::SIMT_SHI:
  case LinxV5::SIMT_SHI_GLOBAL:
  case LinxV5::SIMT_LHI:
  case LinxV5::SIMT_LHI_GLOBAL:
    return true;
  default:
    return false;
  }
}

static unsigned getMemInstr_rr(MachineInstr &MI) {
#define MOP(RI, RR)                                                            \
  case LinxV5::RI:                                                             \
    return LinxV5::RR;

  switch (MI.getOpcode()) {
    // scale load
    MOP(LBI, LB)
    MOP(LHI, LH)
    MOP(LWI, LW)
    MOP(LDI, LD)

    // scale load and uext
    MOP(LBUI, LBU)
    MOP(LHUI, LHU)
    MOP(LWUI, LWU)

    // unscale load
    MOP(LHI_U, LH)
    MOP(LWI_U, LW)
    MOP(LDI_U, LD)

    // unscale load and uext
    MOP(LHUI_U, LHU)
    MOP(LWUI_U, LWU)

    // scale store
    MOP(SBI, SB)
    MOP(SHI, SH)
    MOP(SWI, SW)
    MOP(SDI, SD)

    // unscale store
    MOP(SHI_U, SH_U)
    MOP(SWI_U, SW_U)
    MOP(SDI_U, SD_U)

  default:
    llvm_unreachable("unrecognized ri mem instr!");
  }
#undef MOP
}

void LinxV5RegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                             int SPAdj, unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected non-zero SPAdj value");

  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const LinxV5InstrInfo *TII =
      MF.getSubtarget<LinxV5Subtarget>().getInstrInfo();
  DebugLoc DL = MI.getDebugLoc();
  MachineBasicBlock &MBB = *MI.getParent();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register FrameReg;

  int64_t Offset = (STI.getFrameLowering()
                        ->getFrameIndexReference(MF, FrameIndex, FrameReg)
                        .getFixed());

  // PseudoTSpill tilereg, %stack.fi
  // -->
  // %1:gpr = add sp, <off>
  // PseudoTSTORE <M, N, K, DataType>, tilereg, %1:gpr
  // --------------------------------------
  // tilereg = PseudoTReload %stack.fi
  // -->
  // %1:gpr = add sp, <off>
  // tilereg = PseudoTLOAD <M, N, K, DataType>, %1:gpr
  if (MI.getOpcode() == LinxV5::PseudoTSpill ||
      MI.getOpcode() == LinxV5::PseudoTReload) {
    bool IsSpill = MI.getOpcode() == LinxV5::PseudoTSpill;

    Register TileReg = MI.getOperand(0).getReg();
    int FI = MI.getOperand(1).getIndex();
    unsigned RegSize = MF.getFrameInfo().getObjectSize(FI);
    assert(RegSize >= 512 && RegSize <= 64 * 1024);
    unsigned RegSizeCode = llvm::Log2_32(RegSize) - 6;

    Register AddrReg = MRI.createVirtualRegister(&LinxV5::GRRegClass);
    if (isUInt<12>(Offset) || isUInt<12>(-Offset)) {
      // addi with small imm
      unsigned Opcode = Offset >= 0 ? LinxV5::ADDI : LinxV5::SUBI;
      int64_t Imm = Offset >= 0 ? Offset : -Offset;

      BuildMI(MBB, II, DL, TII->get(Opcode), AddrReg)
          .addReg(FrameReg)
          .addImm(Imm);
    } else {
      //  large imm. lower  -> LUI + ADDI + ADD
      Register Scratch = MRI.createVirtualRegister(&LinxV5::LTRRegClass);
      TII->movImm(MBB, II, DL, Scratch, Offset);

      BuildMI(MBB, II, DL, TII->get(LinxV5::ADD), AddrReg)
          .addReg(FrameReg)
          .addReg(Scratch, RegState::Kill)
          .addImm(0);
    }

    if (IsSpill)
      BuildMI(MBB, II, DL, TII->get(LinxV5::PseudoTSTORE_noDsrc_noDdst))
          .addReg(TileReg, RegState::Kill)
          .addReg(LinxV5::R0).addImm(RegSize / 8)   // M
          .addReg(LinxV5::R0).addImm(1)             // N
          .addReg(LinxV5::R0).addImm(RegSize / 8)   // K
          .addImm(LinxV5Op::DataType::S64)          // DataType
          .addImm(LinxV5Op::ArgFormat::NORM)        // LayOut
          .addReg(AddrReg, RegState::Kill);         // GPR
    else
      BuildMI(MBB, II, DL, TII->get(LinxV5::PseudoTLOAD_noDsrc_noDdst), TileReg)
          .addReg(LinxV5::R0).addImm(RegSize / 8)   // M
          .addReg(LinxV5::R0).addImm(1)             // N
          .addReg(LinxV5::R0).addImm(RegSize / 8)   // K
          .addImm(LinxV5Op::DataType::S64)          // DataType
          .addImm(RegSizeCode)                      // RegSizeCode
          .addImm(LinxV5Op::ArgFormat::NORM)        // LayOut
          .addImm(LinxV5Op::PadValue::Zero)         // PadValue
          .addReg(AddrReg, RegState::Kill);         // GPR

    MI.eraseFromParent();
    return;
  }

  auto getTargetOpcode = [&](MachineInstr &MI, bool IsRegType, bool IsVec) -> unsigned {
    static DenseMap<unsigned, SmallVector<unsigned>> VecOpcMap = {
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_B, {LinxV5::SIMT_Continuous_LBI,   LinxV5::SIMT_Continuous_SBI, LinxV5::SIMT_Continuous_LB, LinxV5::SIMT_Continuous_SB}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_H, {LinxV5::SIMT_Continuous_LHI_U, LinxV5::SIMT_Continuous_SHI_U, LinxV5::SIMT_Continuous_LH, LinxV5::SIMT_Continuous_SH_U}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_W, {LinxV5::SIMT_Continuous_LWI_U, LinxV5::SIMT_Continuous_SWI_U, LinxV5::SIMT_Continuous_LW, LinxV5::SIMT_Continuous_SW_U}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_D, {LinxV5::SIMT_Continuous_LDI_U, LinxV5::SIMT_Continuous_SDI_U, LinxV5::SIMT_Continuous_LD, LinxV5::SIMT_Continuous_SD_U}},
    };
    static DenseMap<unsigned, SmallVector<unsigned>> ScalarOpcMap = {
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_B, {LinxV5::SIMT_LBI,   LinxV5::SIMT_SBI, LinxV5::SIMT_LB, LinxV5::SIMT_SB}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_H, {LinxV5::SIMT_LHI_U, LinxV5::SIMT_SHI_U, LinxV5::SIMT_LH, LinxV5::SIMT_SH_U}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_W, {LinxV5::SIMT_LWI_U, LinxV5::SIMT_SWI_U, LinxV5::SIMT_LW, LinxV5::SIMT_SW_U}},
      {LinxV5::SIMTRegSize::SIMT_REG_SIZE_D, {LinxV5::SIMT_LDI_U, LinxV5::SIMT_SDI_U, LinxV5::SIMT_LD, LinxV5::SIMT_SD_U}},
    };
    unsigned RegSize = IsVec ? MI.getOperand(1).getImm() : LinxV5::SIMTRegSize::SIMT_REG_SIZE_D;
    unsigned IsSpill = MI.getOpcode() == LinxV5::PseudoVecSpill;
    unsigned RefIdx = (IsRegType << 1) | IsSpill;
    return IsVec ? VecOpcMap[RegSize][RefIdx] : ScalarOpcMap[RegSize][RefIdx];
  };

  if (MI.getOpcode() == LinxV5::PseudoVecSpill ||
      MI.getOpcode() == LinxV5::PseudoVecReload) {
    bool IsSpill = MI.getOpcode() == LinxV5::PseudoVecSpill;
    Register Reg = MI.getOperand(0).getReg();
    int FI = MI.getOperand(2).getIndex();
    unsigned RegSizeInBytes = MI.getOperand(1).getImm();
    bool IsVec = RegSizeInBytes != 0;
    if (!IsVec) RegSizeInBytes = LinxV5::SIMTRegSize::SIMT_REG_SIZE_D;
    errs() << "[expand-" << (IsSpill ? "spill" : "reload") << "] MF="
           << MBB.getParent()->getName()
           << " BB=" << MBB.getNumber()
           << " FI=" << FI
           << " Reg=" << printReg(Reg, this)
           << " WidthOp=" << MI.getOperand(1).getImm()
           << " IsVec=" << IsVec
           << " Offset=" << Offset << "\n";

    auto buildSaveRestoreMask = [&](bool Enable) {
      if (!Enable)
        return;

      Register SavedP = getSIMTSpillMaskScratchReg();
      BuildMI(MBB, II, DL, TII->get(LinxV5::LinxV5PseudoCopyFromP), SavedP);
      BuildMI(MBB, II, DL, TII->get(LinxV5::SIMT_ORI_SCAR), LinxV5::SIMT_P)
          .addImm(LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
          .addReg(LinxV5::R0)
          .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD)
          .addImm(-1);
    };

    auto buildRestoreMask = [&](bool Enable) {
      if (!Enable)
        return;

      BuildMI(MBB, II, DL, TII->get(LinxV5::LinxV5PseudoCopy2P))
          .addReg(getSIMTSpillMaskScratchReg());
    };

    bool WrapWithFullMask =
        EnableSIMTSpillSaveRestoreP &&
        MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT();
    buildSaveRestoreMask(WrapWithFullMask);
    if (isInt<12>(Offset)) {
      // addi with small imm
      unsigned Opcode = getTargetOpcode(MI, false, IsVec);
      errs() << "[expand-" << (IsSpill ? "spill" : "reload") << "-opc] MF="
             << MBB.getParent()->getName()
             << " FI=" << FI
             << " Opcode=" << TII->getName(Opcode) << "\n";
      BuildMI(MBB, II, DL, TII->get(Opcode))
        .addReg(Reg, IsSpill ? 0 : RegState::Define)
        .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSizeInBytes))
        .addReg(FrameReg)
        .addImm(LinxV5::getSIMTSrcTypeFromSize(LinxV5::SIMTRegSize::SIMT_REG_SIZE_D))
        .addImm(Offset);
    } else {
      //  large imm. lower  -> LUI + ADDI + ADD
      Register Scratch = MRI.createVirtualRegister(&LinxV5::LTRRegClass);
      TII->movImm(MBB, II, DL, Scratch, Offset);
      unsigned Opcode = getTargetOpcode(MI, true, IsVec);
      errs() << "[expand-" << (IsSpill ? "spill" : "reload") << "-opc] MF="
             << MBB.getParent()->getName()
             << " FI=" << FI
             << " Opcode=" << TII->getName(Opcode) << "\n";
      MachineInstrBuilder MIB =
          BuildMI(MBB, II, DL, TII->get(Opcode))
              .addReg(Reg, IsSpill ? 0 : RegState::Define)
              .addImm(LinxV5::getSIMTSrcTypeFromSize(RegSizeInBytes))
              .addReg(FrameReg)
              .addImm(LinxV5::getSIMTSrcTypeFromSize(
                  LinxV5::SIMTRegSize::SIMT_REG_SIZE_D))
              .addReg(Scratch, RegState::Kill)
              .addImm(LinxV5::getSIMTSrcTypeFromSize(
                  LinxV5::SIMTRegSize::SIMT_REG_SIZE_D))
              .addImm(0); // Shamt
    }
    buildRestoreMask(WrapWithFullMask);
    MI.eraseFromParent();
    return;
  }

  // unify add -> addi
  if (MI.getOpcode() == LinxV5::ADD) {
    MI.setDesc(TII->get(LinxV5::ADDI));
    MI.removeOperand(3);
    assert(MI.getOperand(2).getReg() == LinxV5::R0);
    MI.getOperand(2).ChangeToImmediate(0);
  }

  if (MI.getOpcode() == LinxV5::ADDI) {
    if (isUInt<12>(Offset) || isUInt<12>(-Offset)) {
      // 1. addi with small imm
      if (Offset < 0) {
        MI.setDesc(TII->get(LinxV5::SUBI));
        Offset = -Offset;
      }
      MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
      MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
      return;
    } else {
      // 2. addi with large imm. lower:
      //   %addr = addi %stack.i
      // to
      //   %scratch = lui xxx
      //   %scratch = addi %scratch, xxx
      //   %addr    = add $sp, %scratch
      Register Scratch = MRI.createVirtualRegister(&LinxV5::LTRRegClass);
      TII->movImm(MBB, II, DL, Scratch, Offset);
      BuildMI(MBB, II, DL, TII->get(LinxV5::ADD))
          .add(MI.getOperand(0))
          .addReg(FrameReg)
          .addReg(Scratch, RegState::Kill)
          .addImm(0);
      MI.eraseFromParent();
      return;
    }
  } else {
    unsigned ScaleShift = getScaleShift(MI.getOpcode(), Offset);
    int64_t NewOffset = Offset >> ScaleShift;
    assert((NewOffset << ScaleShift) == Offset && "invalid scale offset!");
    NewOffset += MI.getOperand(FIOperandNum + 1).getImm();
    if (isInt<12>(NewOffset) ||
        (isSIMTMemInstr(MI.getOpcode()) && isInt<24>(Offset))) {
      // 3. load/store with small imm
      MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
      MI.getOperand(FIOperandNum + 1).ChangeToImmediate(NewOffset);
      return;
    } else {
      // 4. load/store with large imm
      // lower:
      //   %load    = ldi %stack.i
      // to
      //   %scratch = LUI hi
      //   %scratch = ADDI %scratch, lo
      //   %load    = ld sp, %scratch << shamt
      Register Scratch = MRI.createVirtualRegister(&LinxV5::LTRRegClass);
      unsigned Opc = getMemInstr_rr(MI);
      Offset += MI.getOperand(FIOperandNum + 1).getImm() << ScaleShift;
      ScaleShift = getScaleShift(Opc, Offset);
      NewOffset = Offset >> ScaleShift;
      assert((NewOffset << ScaleShift) == Offset && "invalid scale offset!");
      TII->movImm(MBB, II, DL, Scratch, NewOffset);
      if (MI.mayStore()) {
        BuildMI(MBB, II, DL, TII->get(Opc))
            .add(MI.getOperand(0))
            .addReg(FrameReg)
            .addReg(Scratch, RegState::Kill);
      } else {
        BuildMI(MBB, II, DL, TII->get(Opc))
            .add(MI.getOperand(0))
            .addReg(FrameReg)
            .addReg(Scratch, RegState::Kill)
            .addImm(ScaleShift);
      }
      MI.getPrevNode()->cloneMemRefs(MF, MI);
      MI.eraseFromParent();
      return;
    }
  }
}

Register LinxV5RegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = STI.getFrameLowering();
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return LinxV5::getVSPReg();
  return TFI->hasFP(MF) ? LinxV5::getFPReg() : LinxV5::getSPReg();
}

bool LinxV5RegisterInfo::isUniformReg(const MachineRegisterInfo &MRI,
                                      Register Reg,
                                      bool TreatLC12AsUniform) const {

  // LC1/LC2 may feed continuous-address formation as x-uniform values. In DR
  // mode they are lane-dependent and must not select scalar l.* instructions.
  if (Reg == LinxV5::SIMT_LC1 || Reg == LinxV5::SIMT_LC2)
    return TreatLC12AsUniform;

  const TargetRegisterClass *RC;
  if (Reg.isVirtual())
    RC = MRI.getRegClass(Reg);
  else
    RC = getMinimalPhysRegClass(Reg);
  return LinxV5::UniformRegRegClass.hasSubClass(RC);
}
