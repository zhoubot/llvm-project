//===-- LinxV5TileOpExpand.cpp - LinxV5 Assembler Backend
//------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5TileOpExpand.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5TileOpReader.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

namespace llvm {

static cl::opt<bool> EnableDimOpt("linxv5-enable-dim-opt",
                                  cl::desc("B.DIM Coding Optimization"),
                                  cl::init(true), cl::Hidden);

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, llvm::SmallVector<unsigned> inVec) {
  // The load local address pseudo-instruction "PS.B.IOR" is used in PC-relative
  // addressing of local symbols:
  //   PS.B.IOR [$reglistIn], [$reglistOut]
  // expands to
  //   B.IOR [$reglistIn], [$reglistOut]
  //   B.IOR [$reglistIn], [$reglistOut]
  //   ....
  llvm::SmallVector<MCInst> McVec;

  unsigned int InstNums =
      inVec.size() % 3 == 0 ? (inVec.size() / 3) : (inVec.size() / 3) + 1;
  unsigned int inID = 0;
  for (int i = 0; i < InstNums; ++i) {
    unsigned int RegSrc0 = LinxV5::R0;
    unsigned int RegSrc1 = LinxV5::R0;
    unsigned int RegSrc2 = LinxV5::R0;
    if (inID < inVec.size())
      RegSrc0 = inVec[inID++];
    if (inID < inVec.size())
      RegSrc1 = inVec[inID++];
    if (inID < inVec.size())
      RegSrc2 = inVec[inID++];
    McVec.push_back(MCInstBuilder(LinxV5::B_IO)
                        .addOperand(MCOperand::createReg(LinxV5::R0))
                        .addOperand(MCOperand::createReg(RegSrc0))
                        .addOperand(MCOperand::createReg(RegSrc1))
                        .addOperand(MCOperand::createReg(RegSrc2)));
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIORFromInstByIndex(MCInst Inst, int inIndex) {
  llvm::SmallVector<unsigned> inVec;
  for (unsigned i = 0; (inIndex + i) < Inst.getNumOperands(); i++) {
    unsigned Reg = Inst.getOperand(inIndex + i).getReg();
    if (Reg <= LinxV5::R23 && Reg >= LinxV5::R0)
      inVec.push_back(Reg);
  }
  return getBIORFromInst(Inst, inVec);
}

llvm::SmallVector<MCInst> getBARGFromInst(const MCInst &Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> MCVec;
  switch (Inst.getOpcode()) {
  default:
    break;
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(8))
            .addOperand(Inst.getOperand(9))
            .addOperand(Inst.getOperand(6))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTMOV_SizeI:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(11))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(9))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(10))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(Inst.getOperand(11))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(8))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
    MCVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(Inst.getOperand(9))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(
                MCOperand::createImm(LinxV5Op::DataType::EMPTY_DataType))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Null))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    break;
  }
  return MCVec;
}

void getPseudoCallBIOTBySrcDstNum(llvm::SmallVector<MCInst> &McVec,
                                  MCInst Inst, const MCInstrInfo &MII) {
  TileCallReader CallReader(Inst, MII);
  unsigned TileDstNum = CallReader.getTileDstNum();
  unsigned TileSrcNum = CallReader.getTileSrcNum();

  if (TileDstNum == 0 && TileSrcNum == 0) {
    if (CallReader.HasS())
      // v5 B_IOT_NoSrc_Dst: (outs DstTile), (ins PE_MASK, TSize, Last)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                          .addOperand(CallReader.getS())              // DstTile (OUT)
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK=all
                          .addOperand(CallReader.getStackSize())      // TSize
                          .addOperand(MCOperand::createImm(1)));     // Last
    return;
  }

  auto getOpcode = [&](unsigned numSrc, bool hasDst) -> unsigned {
    switch (numSrc) {
    case 0:
      return LinxV5::B_IOT_NoSrc_Dst;
    case 1:
      return hasDst ? LinxV5::B_IOT_OneSrc_Dst : LinxV5::B_IOT_OneSrc_NoDst;
    case 2:
      return hasDst ? LinxV5::B_IOT_TwoSrc_Dst : LinxV5::B_IOT_TwoSrc_NoDst;
    default:
      llvm_unreachable("Invalid b.io src count");
    }
  };

  unsigned srcIdx = 0;
  unsigned dstIdx = 0;

  if (CallReader.HasS())
    // v5 B_IOT_NoSrc_Dst: (outs DstTile), (ins PE_MASK, TSize, Last)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(CallReader.getS())              // DstTile (OUT)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK=all
                        .addOperand(CallReader.getStackSize())      // TSize
                        .addOperand(MCOperand::createImm(0)));     // Last (not last)

  // B.IO only can take at most 2 src, 1 dst
  while (srcIdx < TileSrcNum || dstIdx < TileDstNum) {
    unsigned remainingSrc = TileSrcNum - srcIdx;
    unsigned remainingDst = TileDstNum - dstIdx;
    unsigned thisSrcCount = std::min(2u, remainingSrc);
    bool hasDst = (remainingDst > 0);
    bool isLast = (remainingSrc <= 2 && remainingDst <= 1);

    MCInstBuilder builder(getOpcode(thisSrcCount, hasDst));

    // MC operands list all defs before uses.
    if (hasDst)
      builder.addOperand(CallReader.getTileDst(dstIdx));

    // PE_MASK initialized to all-1 (1111) for all PEs.
    builder.addOperand(MCOperand::createImm(0b1111));  // PE_MASK=all

    if (hasDst) {
      builder.addOperand(CallReader.getTileSize(dstIdx));  // TSize
    }
    // NoDst forms have no TSize operand in v5.

    builder.addOperand(MCOperand::createImm(isLast ? 1 : 0));  // Last

    for (unsigned i = 0; i < thisSrcCount; i++)
      builder.addOperand(CallReader.getTileSrc(srcIdx++));

    if (hasDst)
      dstIdx++;

    McVec.push_back(builder);
  }
}

llvm::SmallVector<MCInst> getBATTRFromInst(MCInst Inst,
                                           const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;

  // MX matmul family:
  // If DataTypeA != DataTypeB, emit:
  //   B.ATTR DataTypeB
  // If DataTypeA == DataTypeB, no B.ATTR is needed.
  if (isActiveMatrixPseudo(Inst.getOpcode())) {
    constexpr unsigned DataTypeAOpNo = 7;
    constexpr unsigned DataTypeBOpNo = 8;

    assert(Inst.getNumOperands() > DataTypeBOpNo &&
           "Unexpected operand layout for MX matmul pseudo");

    const MCOperand &DataTypeA = Inst.getOperand(DataTypeAOpNo);
    const MCOperand &DataTypeB = Inst.getOperand(DataTypeBOpNo);

    assert(DataTypeA.isImm() && DataTypeB.isImm() &&
           "DataType operands of MX matmul pseudo must be immediates");

    if (DataTypeA.getImm() == DataTypeB.getImm())
      return McVec;

    McVec.push_back(
        MCInstBuilder(LinxV5::BDATR)
            .addOperand(MCOperand::createImm(0))
            .addOperand(MCOperand::createImm(LinxV5Op::Canon::NORMAL_CANON))
            .addOperand(MCOperand::createImm(DataTypeB.getImm()))
            .addOperand(MCOperand::createImm(LinxV5Op::PadValue::Zero))
            .addOperand(MCOperand::createImm(LinxV5Op::CmpMode::EQ))
            .addOperand(MCOperand::createImm(LinxV5Op::RMode::RNONE))
            .addOperand(MCOperand::createImm(LinxV5Op::Sat::NOSAT))
            .addOperand(MCOperand::createImm(LinxV5Op::ByteID::BYTE0)));
    return McVec;
  }

  TileCallReader CallReader(Inst, MII);
  if (CallReader.HasDR() && CallReader.getDR().getImm() == LinxV5Op::DREnum::DR)
    McVec.push_back(
        MCInstBuilder(LinxV5::BCATR)
            .addOperand(MCOperand::createImm(0))
            .addOperand(MCOperand::createImm(LinxV5Op::DREnum::DR)));
  return McVec;
}

llvm::SmallVector<MCInst> getBIOTFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    getPseudoCallBIOTBySrcDstNum(McVec, Inst, MII);
    return McVec;
  }

  // Active local Matrix pseudos share one stable operand layout:
  //   Op0 destination, Op1..6 dimensions, Op7..8 data types,
  //   Op9 destination SizeCode, Op10..N relative Local sources.
  // Bind sources in their encoded order and place the destination only on
  // the terminating packet. The source register spellings (T#n/U#n/M#n/N#n)
  // are architectural relative-generation selectors, not physical indices.
  if (isActiveMatrixPseudo(Inst.getOpcode()) &&
      Inst.getOpcode() != LinxV5::PseudoMAMULB_SharedRight_SizeI) {
    constexpr unsigned DstOpNo = 0;
    constexpr unsigned TileSizeOpNo = 9;
    constexpr unsigned FirstSrcOpNo = 10;
    assert(Inst.getNumOperands() > FirstSrcOpNo &&
           "active Matrix pseudo must carry at least one source");

    unsigned SrcOpNo = FirstSrcOpNo;
    while (SrcOpNo < Inst.getNumOperands()) {
      unsigned Remaining = Inst.getNumOperands() - SrcOpNo;
      unsigned ThisCount = std::min(2u, Remaining);
      bool IsLast = Remaining <= 2;
      unsigned Opcode = ThisCount == 1
                            ? (IsLast ? LinxV5::B_IOT_OneSrc_Dst
                                      : LinxV5::B_IOT_OneSrc_NoDst)
                            : (IsLast ? LinxV5::B_IOT_TwoSrc_Dst
                                      : LinxV5::B_IOT_TwoSrc_NoDst);
      MCInstBuilder Builder(Opcode);
      if (IsLast)
        Builder.addOperand(Inst.getOperand(DstOpNo));
      Builder.addOperand(MCOperand::createImm(0b1111));
      if (IsLast)
        Builder.addOperand(Inst.getOperand(TileSizeOpNo));
      Builder.addOperand(MCOperand::createImm(IsLast ? 1 : 0));
      for (unsigned I = 0; I < ThisCount; ++I)
        Builder.addOperand(Inst.getOperand(SrcOpNo++));
      McVec.push_back(Builder);
    }
    return McVec;
  }
  switch (Inst.getOpcode()) {
  default:
    llvm_unreachable("Can not find BIO From Pseudo Inst!");
  // v5 basic TMATMUL: D = A * B. Unified ordinary-Tile result model:
  //   Op0=DstTile(def), Op1..6=dims, Op7=DataTypeA, Op8=DataTypeB,
  //   Op9=TileSize, Op10=SrcTile0(A), Op11=SrcTile1(B).
  // Emit one B.IOT TwoSrc_Dst carrying both sources plus the ordinary dst.
  case LinxV5::PseudoMAMULB_SizeI:
  case LinxV5::PseudoTGEMV_SizeI:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(0))             // DstTile
                        .addOperand(MCOperand::createImm(0b1111))   // PE_MASK
                        .addOperand(Inst.getOperand(9))             // TSize
                        .addOperand(MCOperand::createImm(1))        // Last=1
                        .addOperand(Inst.getOperand(10))            // SrcTile0 (A)
                        .addOperand(Inst.getOperand(11)));          // SrcTile1 (B)
    break;
  case LinxV5::PseudoMAMULB_SharedRight_SizeI:
    // v5 Shared Right: B is bound by C.B.IOS, not in B.IOT. Only A remains in
    // the source stream; the ordinary dst still carries last. The C.B.IOS
    // binder itself is emitted by expandPseudoCCall from the SharedTID operand.
    //   Op0=DstTile(def), Op9=TileSize, Op10=SrcTile0(A), Op11=SharedTID.
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))             // DstTile
                        .addOperand(MCOperand::createImm(0b1111))   // PE_MASK (fixed)
                        .addOperand(Inst.getOperand(9))             // TSize
                        .addOperand(MCOperand::createImm(1))        // Last=1
                        .addOperand(Inst.getOperand(10)));          // SrcTile0 (A)
    break;
  case LinxV5::PseudoMAMULBAC_SizeI:
  case LinxV5::PseudoMAMULBACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBAC_SizeI) {
      // v5: B_IOT_TwoSrc_Dst(PE_MASK, TSize, Last, SrcTile0, SrcTile1, DstTile)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                          .addOperand(Inst.getOperand(11))  // DstTile
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(Inst.getOperand(0))            // TSize
                          .addOperand(MCOperand::createImm(0))       // Last=0
                          .addOperand(Inst.getOperand(9))            // SrcTile0
                          .addOperand(Inst.getOperand(10)));  // SrcTile1
      // v5: B_IOT_OneSrc_NoDst(PE_MASK, Last, SrcTile0)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(MCOperand::createImm(1))       // Last=1
                          .addOperand(Inst.getOperand(12)));         // SrcTile0
      break;
    }
    // v5: B_IOT_TwoSrc_Dst(PE_MASK, TSize, Last, SrcTile0, SrcTile1, DstTile)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(11))  // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(0))            // TSize
                        .addOperand(MCOperand::createImm(1))       // Last=1
                        .addOperand(Inst.getOperand(9))            // SrcTile0
                        .addOperand(Inst.getOperand(10)));  // SrcTile1
    break;
  case LinxV5::PseudoTGEMV_BIAS_SizeI:
  case LinxV5::PseudoTGEMV_ACC_SizeI:
    // TGEMV BIAS/ACC layout: Dst, dimensions/types, size, then three
    // sources. Keep the destination on the first packet and Last on the
    // final source packet.
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(12)));
    break;
  case LinxV5::PseudoMAMULBMX_SizeI:
  case LinxV5::PseudoMAMULBMXAC_SizeI:
  case LinxV5::PseudoMAMULBMXACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXAC_SizeI) {
      // v5: B_IOT_TwoSrc_NoDst(PE_MASK, Last, SrcTile0, SrcTile1)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(MCOperand::createImm(0))       // Last=0
                          .addOperand(Inst.getOperand(10))          // SrcTile0
                          .addOperand(Inst.getOperand(11)));        // SrcTile1
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(MCOperand::createImm(0))       // Last=0
                          .addOperand(Inst.getOperand(12))          // SrcTile0
                          .addOperand(Inst.getOperand(13)));        // SrcTile1
      // v5: B_IOT_OneSrc_Dst(PE_MASK, TSize, Last, SrcTile0, DstTile)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                          .addOperand(Inst.getOperand(14))  // DstTile
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(Inst.getOperand(0))            // TSize
                          .addOperand(MCOperand::createImm(1))       // Last=1
                          .addOperand(Inst.getOperand(9)));  // SrcTile0
      break;
    }
    // v5: B_IOT_TwoSrc_NoDst(PE_MASK, Last, SrcTile0, SrcTile1)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(MCOperand::createImm(0))       // Last=0
                        .addOperand(Inst.getOperand(10))          // SrcTile0
                        .addOperand(Inst.getOperand(11)));        // SrcTile1
    // v5: B_IOT_TwoSrc_Dst(PE_MASK, TSize, Last, SrcTile0, SrcTile1, DstTile)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(13))  // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(0))            // TSize
                        .addOperand(MCOperand::createImm(1))       // Last=1
                        .addOperand(Inst.getOperand(9))            // SrcTile0
                        .addOperand(Inst.getOperand(12)));  // SrcTile1
    break;
  case LinxV5::PseudoTGEMVMX_SizeI:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(12))
                        .addOperand(Inst.getOperand(13)));
    break;
  case LinxV5::PseudoTGEMVMX_BIAS_SizeI:
  case LinxV5::PseudoTGEMVMX_ACC_SizeI:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(10))
                        .addOperand(Inst.getOperand(11)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(12))
                        .addOperand(Inst.getOperand(13)));
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))
                        .addOperand(MCOperand::createImm(0b1111))
                        .addOperand(Inst.getOperand(9))
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(14)));
    break;
  case LinxV5::PseudoMAMULBMXB_SizeI:
  case LinxV5::PseudoMAMULBMXBAC_SizeI:
  case LinxV5::PseudoMAMULBMXBACC_SizeI:
    if (Inst.getOpcode() == LinxV5::PseudoMAMULBMXBAC_SizeI) {
      // v5: B_IOT_TwoSrc_NoDst(PE_MASK, Last, SrcTile0, SrcTile1)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(MCOperand::createImm(0))       // Last=0
                          .addOperand(Inst.getOperand(10))          // SrcTile0
                          .addOperand(Inst.getOperand(11)));        // SrcTile1
      // v5: B_IOT_TwoSrc_Dst(PE_MASK, TSize, Last, SrcTile0, SrcTile1, DstTile)
      McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_Dst)
                          .addOperand(Inst.getOperand(13))  // DstTile
                          .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                          .addOperand(Inst.getOperand(0))            // TSize
                          .addOperand(MCOperand::createImm(1))       // Last=1
                          .addOperand(Inst.getOperand(9))            // SrcTile0
                          .addOperand(Inst.getOperand(12)));  // SrcTile1
      break;
    }
    // v5: B_IOT_TwoSrc_NoDst(PE_MASK, Last, SrcTile0, SrcTile1)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_TwoSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(MCOperand::createImm(0))       // Last=0
                        .addOperand(Inst.getOperand(10))          // SrcTile0
                        .addOperand(Inst.getOperand(11)));        // SrcTile1
    // v5: B_IOT_OneSrc_Dst(PE_MASK, TSize, Last, SrcTile0, DstTile)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(12))  // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(0))            // TSize
                        .addOperand(MCOperand::createImm(1))       // Last=1
                        .addOperand(Inst.getOperand(9)));  // SrcTile0
    break;
    // v5: NoSrc_Dst(PE_MASK, TSize, Last)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(7))             // TSize
                        .addOperand(MCOperand::createImm(1)));     // Last
    break;
  case LinxV5::PseudoESAVE:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))            // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(1))            // TSize(StackSize)
                        .addOperand(MCOperand::createImm(1)));     // Last
    break;
  case LinxV5::PseudoERCOV:
    // v5: OneSrc_NoDst(PE_MASK, Last, SrcTile0)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(MCOperand::createImm(1))       // Last
                        .addOperand(Inst.getOperand(0)));          // SrcTile0
    break;
  case LinxV5::PseudoEmptyTile:
  case LinxV5::PseudoEmptyTileASM:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))            // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(MCOperand::createImm(0))       // TSize=0
                        .addOperand(MCOperand::createImm(1)));     // Last
    break;
  case LinxV5::PseudoTMOV_SizeI:
    // v5: OneSrc_Dst(PE_MASK, TSize, Last, SrcTile0, DstTile)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(9))  // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(0))            // TSize
                        .addOperand(MCOperand::createImm(1))       // Last
                        .addOperand(Inst.getOperand(8)));  // SrcTile0
    break;
  case LinxV5::PseudoTCOPY: {
    // v5: OneSrc_Dst(PE_MASK, TSize, Last, SrcTile0, DstTile)
    // PseudoTCOPY operands: [0]=DstTile(def), [1]=TileSize(imm), [2]=SrcTile.
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_Dst)
                        .addOperand(Inst.getOperand(0))  // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(1))            // TSize
                        .addOperand(MCOperand::createImm(1))       // Last
                        .addOperand(Inst.getOperand(2)));  // SrcTile0
    break;
  }
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
    // v5: OneSrc_NoDst(PE_MASK, Last, SrcTile0)
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_OneSrc_NoDst)
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(MCOperand::createImm(1))       // Last
                        .addOperand(Inst.getOperand(0)));          // SrcTile0
    break;
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOT_NoSrc_Dst)
                        .addOperand(Inst.getOperand(0))            // DstTile
                        .addOperand(MCOperand::createImm(0b1111))  // PE_MASK
                        .addOperand(Inst.getOperand(8))            // TSize
                        .addOperand(MCOperand::createImm(1)));     // Last
    break;
  }
  return McVec;
}

bool isZeroRegAndOneImm(MCOperand &RegOp, MCOperand &ImmOp) {
  if (!EnableDimOpt)
    return false;

  // Determine reg zero.
  bool isZeroReg = RegOp.isReg() && RegOp.getReg() == LinxV5::R0;

  // Determine imm 1.
  bool isZeroImm = ImmOp.isImm() && ImmOp.getImm() == 1;

  return (isZeroReg && isZeroImm);
}

bool isZeroRegAndOneImm(const MCInst &Inst, unsigned i) {
  if (i + 1 >= Inst.getNumOperands())
    return false;

  const MCOperand &RegOp = Inst.getOperand(i);
  const MCOperand &ImmOp = Inst.getOperand(i + 1);

  // Determine reg zero.
  bool isZeroReg = RegOp.isReg() && RegOp.getReg() == LinxV5::R0;

  // Determine imm 0.
  bool isZeroImm = ImmOp.isImm() && ImmOp.getImm() == 0;

  return (isZeroReg && isZeroImm);
}

llvm::SmallVector<MCInst> getBDIMFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  using namespace LinxV5;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    auto RegM = CallReader.getDimMReg();
    auto ImmM = CallReader.getDimMImm();

    auto RegN = CallReader.getDimNReg();
    auto ImmN = CallReader.getDimNImm();

    auto RegK = CallReader.getDimKReg();
    auto ImmK = CallReader.getDimKImm();

    // ->lb0
    if (!isZeroRegAndOneImm(RegM, ImmM))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(RegM)
                          .addOperand(ImmM));

    // ->lb1
    if (!isZeroRegAndOneImm(RegN, ImmN))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(RegN)
                          .addOperand(ImmN));

    // ->lb2
    if (!isZeroRegAndOneImm(RegK, ImmK))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(2))
                          .addOperand(RegK)
                          .addOperand(ImmK));
    return McVec;
  }
  switch (Inst.getOpcode()) {
  case PseudoTMOV_SizeI: {
    // ->lb0
    McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                        .addOperand(MCOperand::createImm(0))
                        .addOperand(Inst.getOperand(1))
                        .addOperand(Inst.getOperand(2)));

    // ->lb1
    McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                        .addOperand(MCOperand::createImm(1))
                        .addOperand(Inst.getOperand(3))
                        .addOperand(Inst.getOperand(4)));
    break;
  }
  case PseudoTLOAD_noDsrc_noDdst:
  case PseudoTSTORE_noDsrc_noDdst:
  case PseudoTLOAD_noDsrc_Ddst:
  case PseudoTSTORE_noDsrc_Ddst:
  case PseudoTLOAD_Dsrc_noDdst:
  case PseudoTSTORE_Dsrc_noDdst:
  case PseudoTLOAD_Dsrc_Ddst:
  case PseudoTSTORE_Dsrc_Ddst:
  case PseudoMAMULB_SizeI:
  case PseudoMAMULBAC_SizeI:
  case PseudoMAMULBACC_SizeI:
  case PseudoMAMULBMX_SizeI:
  case PseudoMAMULBMXB_SizeI:
  case PseudoMAMULBMXAC_SizeI:
  case PseudoMAMULBMXBAC_SizeI:
  case PseudoMAMULBMXACC_SizeI:
  case PseudoMAMULBMXBACC_SizeI:
  case PseudoTGEMV_SizeI:
  case PseudoTGEMV_BIAS_SizeI:
  case PseudoTGEMV_ACC_SizeI:
  case PseudoTGEMVMX_SizeI:
  case PseudoTGEMVMX_BIAS_SizeI:
  case PseudoTGEMVMX_ACC_SizeI: {
    // ->lb0
    if (!isZeroRegAndOneImm(Inst, 1))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(0))
                          .addOperand(Inst.getOperand(1))
                          .addOperand(Inst.getOperand(2)));

    // ->lb1
    if (!isZeroRegAndOneImm(Inst, 3))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(1))
                          .addOperand(Inst.getOperand(3))
                          .addOperand(Inst.getOperand(4)));
    // ->lb2
    if (!isZeroRegAndOneImm(Inst, 5))
      McVec.push_back(MCInstBuilder(LinxV5::B_DIM)
                          .addOperand(MCOperand::createImm(2))
                          .addOperand(Inst.getOperand(5))
                          .addOperand(Inst.getOperand(6)));
    break;
  }
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIORFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    if (CallReader.HasRegInList()) {
      llvm::SmallVector<unsigned> GPRInList = CallReader.getGPRInList();
      McVec = getBIORFromInst(Inst, GPRInList);
    }
    return McVec;
  }
  switch (Inst.getOpcode()) {
  default:
    break;
  case LinxV5::PseudoTLOAD_Dsrc_noDdst:
  case LinxV5::PseudoTLOAD_Dsrc_Ddst:
  case LinxV5::PseudoTLOAD_noDsrc_noDdst:
  case LinxV5::PseudoTLOAD_noDsrc_Ddst:
  case LinxV5::PseudoTSTORE_Dsrc_noDdst:
  case LinxV5::PseudoTSTORE_Dsrc_Ddst:
  case LinxV5::PseudoTSTORE_noDsrc_noDdst:
  case LinxV5::PseudoTSTORE_noDsrc_Ddst:
    llvm::SmallVector<unsigned> GPRInList;
    for (int i = Inst.getNumOperands() - 1; i >= 0; i--) {
      auto CurMO = Inst.getOperand(i);
      if (!CurMO.isReg() ||
          !LinxV5MCRegisterClasses[LinxV5::GRRegClassID].contains(
              CurMO.getReg()))
        break;
      GPRInList.emplace_back(CurMO.getReg());
    }
    // need reverse
    std::reverse(GPRInList.begin(), GPRInList.end());
    McVec = getBIORFromInst(Inst, GPRInList);
    break;
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBTEXTTFromInst(MCInst Inst, const MCInstrInfo &MII) {
  using namespace LinxV5;
  llvm::SmallVector<MCInst> McVec;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    McVec.push_back(
        MCInstBuilder(LinxV5::BTEXT).addOperand(CallReader.getCallee()));
  }
  return McVec;
}

llvm::SmallVector<MCInst> getBIODFromInst(MCInst Inst, const MCInstrInfo &MII) {
  llvm::SmallVector<MCInst> McVec;
  using namespace LinxV5;
  const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  // VCALL/MCALL
  if (LinxV5II::isTileOp(TSFlags) && !LinxV5II::isHeaderOnly(TSFlags)) {
    TileCallReader CallReader(Inst, MII);
    bool HasDepSrc = CallReader.getDepSrcNum() > 0;
    bool HasDepDst = CallReader.HasDepOutput();
    if (HasDepSrc && HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(CallReader.getDepSrc(0)));
      return McVec;
    }

    if (HasDepSrc && !HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Nodst)
                        .addOperand(CallReader.getDepSrc(0)));
      return McVec;
    }

    if (!HasDepSrc && HasDepDst) {
      McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(MCOperand::createReg(0)));
      return McVec;
    }

    return McVec;
  }

  switch (Inst.getOpcode()) {
  case PseudoTLOAD_noDsrc_Ddst:
  case PseudoTSTORE_noDsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(MCOperand::createReg(0)));

    break;
  case PseudoTSTORE_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(Inst.getOperand(8)));
    break;
  case PseudoTLOAD_Dsrc_Ddst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Dst)
                        .addOperand(Inst.getOperand(9)));
    break;

  case PseudoTSTORE_Dsrc_noDdst:
    McVec.push_back(MCInstBuilder(LinxV5::B_IOD_Nodst)
                        .addOperand(Inst.getOperand(8)));
    break;
  case PseudoTLOAD_Dsrc_noDdst:
    McVec.push_back(
        MCInstBuilder(LinxV5::B_IOD_Nodst).addOperand(Inst.getOperand(9)));

    break;
  }
  return McVec;
}

unsigned getPseudoTILEOpcode(unsigned Opcode) {
  static const llvm::DenseMap<unsigned, unsigned> PseudoToOpc = {
      {LinxV5::PseudoMAMULB_SizeI, LinxV5Op::TileOPCUBE::MAMULB},
      {LinxV5::PseudoMAMULB_SharedRight_SizeI, LinxV5Op::TileOPCUBE::MAMULB},
      {LinxV5::PseudoMAMULBAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBAC},
      {LinxV5::PseudoMAMULBMX_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX},
      {LinxV5::PseudoMAMULBMXB_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX},
      {LinxV5::PseudoMAMULBMXAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMXAC},
      {LinxV5::PseudoMAMULBMXBAC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMXAC},
      {LinxV5::PseudoMAMULBMXACC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX_ACC},
      {LinxV5::PseudoMAMULBMXBACC_SizeI, LinxV5Op::TileOPCUBE::MAMULBMX_ACC},
      {LinxV5::PseudoTLOAD_noDsrc_noDdst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_noDsrc_noDdst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_noDsrc_Ddst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_noDsrc_Ddst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_Dsrc_noDdst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_Dsrc_noDdst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoTLOAD_Dsrc_Ddst, LinxV5Op::TileOPTMA::TLOAD},
      {LinxV5::PseudoTSTORE_Dsrc_Ddst, LinxV5Op::TileOPTMA::TSTORE},
      {LinxV5::PseudoMAMULBACC_SizeI, LinxV5Op::TileOPCUBE::MAMULB_ACC},
      {LinxV5::PseudoTGEMV_SizeI, LinxV5Op::TileOPCUBE::TGEMV},
      {LinxV5::PseudoTGEMV_BIAS_SizeI, LinxV5Op::TileOPCUBE::TGEMV_BIAS},
      {LinxV5::PseudoTGEMV_ACC_SizeI, LinxV5Op::TileOPCUBE::TGEMV_ACC},
      {LinxV5::PseudoTGEMVMX_SizeI, LinxV5Op::TileOPCUBE::TGEMVMX},
      {LinxV5::PseudoTGEMVMX_BIAS_SizeI, LinxV5Op::TileOPCUBE::TGEMVMX_BIAS},
      {LinxV5::PseudoTGEMVMX_ACC_SizeI, LinxV5Op::TileOPCUBE::TGEMVMX_ACC},
      {LinxV5::PseudoESAVE, LinxV5Op::TileOPTEPL::ESAVE},
      {LinxV5::PseudoERCOV, LinxV5Op::TileOPTEPL::ERCOV}};

  return PseudoToOpc.lookup(Opcode);
}

// isActiveMatrixPseudo identifies every active Matrix CUBE operation
// (TMATMUL/TMATMULMX family). Per the DavinciOO v5 contract every supported
// CUBE bundle must carry exactly one B.FPATR, so this predicate drives
// B.FPATR emission in expandPseudoCCall. The SharedRight and Higher variants
// are members of the same family and are included. The deleted
// TMATMUL*_FIXP opcodes (Function 9-14) are intentionally absent; they are
// reserved/illegal. TGEMV pseudos are added here once defined in
// LinxV5InstrInfo.td (Function 16/17/18/20/21/22).
bool isActiveMatrixPseudo(unsigned Opcode) {
  switch (Opcode) {
  case LinxV5::PseudoMAMULB_SizeI:
  case LinxV5::PseudoMAMULB_SharedRight_SizeI:
  case LinxV5::PseudoMAMULBAC_SizeI:
  case LinxV5::PseudoMAMULBACC_SizeI:
  case LinxV5::PseudoMAMULBMX_SizeI:
  case LinxV5::PseudoMAMULBMXB_SizeI:
  case LinxV5::PseudoMAMULBMXAC_SizeI:
  case LinxV5::PseudoMAMULBMXBAC_SizeI:
  case LinxV5::PseudoMAMULBMXACC_SizeI:
  case LinxV5::PseudoMAMULBMXBACC_SizeI:
  case LinxV5::PseudoTGEMV_SizeI:
  case LinxV5::PseudoTGEMV_BIAS_SizeI:
  case LinxV5::PseudoTGEMV_ACC_SizeI:
  case LinxV5::PseudoTGEMVMX_SizeI:
  case LinxV5::PseudoTGEMVMX_BIAS_SizeI:
  case LinxV5::PseudoTGEMVMX_ACC_SizeI:
    return true;
  default:
    return false;
  }
}

} // namespace llvm
