//===-- LinxV5ISelLowering.cpp - LinxV5 DAG Lowering Implementation -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that LinxV5 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "LinxV5ISelLowering.h"
#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5Subtarget.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-lower"

STATISTIC(NumTailCalls, "Number of tail calls");

static SDValue lowerV5GMOV(SDValue Op, SelectionDAG &DAG);
static SDValue lowerV5SharedS2L(SDValue Op, SelectionDAG &DAG,
                                unsigned Function);
static SDValue lowerV5SharedL2S(SDValue Op, SelectionDAG &DAG,
                                unsigned Function);

static cl::opt<unsigned>
    MaxDepthForSearchSext("max-depth-for-linxv5-search-sext",
                          cl::desc("Max recursion depth for search sext"),
                          cl::init(4), cl::Hidden);

cl::opt<bool> EnableAllVectorAsTilereg(
    "enable-all-vector-as-tilereg",
    cl::desc("Lowering any vector type as tile register."), cl::init(false),
    cl::Hidden);

LinxV5TargetLowering::LinxV5TargetLowering(const TargetMachine &TM,
                                           const LinxV5Subtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  LinxV5ABI::ABI ABI = Subtarget.getTargetABI();

  assert(ABI == LinxV5ABI::ABI_LP64 && "LinxV5 only support abi=lp64!");

  MVT XLenVT = Subtarget.getXLenVT();

  if (Subtarget.isSIMT()) {
    addRegisterClass(MVT::i8, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i32, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i64, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::bf16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f32, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f64, &LinxV5::SIMTCGVRegClass);
  } else if (!Subtarget.enableLegacyISel()) {
    auto *RC = STI.getRegisterInfo()->getSTDRC();
    addRegisterClass(MVT::i64, RC);
    if (STI.hasFloat()) {
      addRegisterClass(MVT::f64, RC);
      addRegisterClass(MVT::f32, RC);
      addRegisterClass(MVT::f16, RC);
    }
  } else {
    addRegisterClass(MVT::i64, &LinxV5::VBXTRRegClass);
  }

  if (EnableAllVectorAsTilereg) {
    const TargetRegisterClass *RC = Subtarget.isSIMT()
                                        ? &LinxV5::SIMTCGVRegClass
                                        : &LinxV5::Tile_ABS_CGRegClass;
    for (MVT VT : MVT::integer_fixedlen_vector_valuetypes())
      addRegisterClass(VT, RC);
    for (MVT VT : MVT::fp_fixedlen_vector_valuetypes())
      addRegisterClass(VT, RC);
  }

  // Compute derived properties from the register classes.
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(LinxV5::R1);

  for (auto N : {ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD})
    setLoadExtAction(N, XLenVT, MVT::i1, Promote);

  // TODO: add all necessary setOperationAction calls.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, XLenVT, Expand);

  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC, XLenVT, Expand);
  setOperationAction(ISD::SELECT_CC, XLenVT, Expand);
  if (Subtarget.isSIMT() || !Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::BR_CC,
                       {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64},
                       Expand);
    setOperationAction(ISD::SELECT_CC,
                       {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64},
                       Expand);

    setOperationAction(ISD::SETCC, MVT::i1, Custom);

    static const MVT::SimpleValueType FloatVTs[] = {MVT::f16, MVT::f32,
                                                    MVT::f64};

    // Convert an integer comparison to a floating point comparison
    static const ISD::CondCode CCToExpand[] = {
        ISD::SETUEQ, ISD::SETUGT, ISD::SETUGE, ISD::SETULT,
        ISD::SETULE, ISD::SETO,   ISD::SETUO,  ISD::SETUNE};
    const auto SetCommonFCompareActions = [&](MVT VT) {
      setCondCodeAction(CCToExpand, VT, Expand);
    };

    for (MVT VT : FloatVTs) {
      SetCommonFCompareActions(VT);
    }
  }

  if (Subtarget.isScalar() && !Subtarget.enableLegacyISel()) {
    static const MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16};
    setOperationAction(ISD::FREM, {MVT::f64, MVT::f32}, Expand);
    setOperationAction(ISD::FREM, MVT::f16, Promote);
    setOperationAction(ISD::FPOW, FPVTs, Expand);
    setOperationAction(ISD::FCOPYSIGN, FPVTs, Expand);
    setOperationAction(ISD::FABS, FPVTs, Expand);
    setOperationAction(ISD::FSQRT, FPVTs, Legal);
    setOperationAction(ISD::FEXP, FPVTs, LibCall);
    setOperationAction(ISD::FLOG, FPVTs, LibCall);
    setOperationAction(ISD::FSIN, FPVTs, LibCall);
    setOperationAction(ISD::FCOS, FPVTs, LibCall);
  }

  if (Subtarget.isSIMT()) {
    static const MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16};
    setOperationAction(ISD::FABS, FPVTs, Legal);
    setOperationAction(ISD::FSQRT, FPVTs, Legal);
    setOperationAction(ISD::FEXP, FPVTs, Legal);
    setOperationAction(ISD::FLOG2, FPVTs, Legal);
    setOperationAction(ISD::FSIN, FPVTs, Legal);
    setOperationAction(ISD::FCOS, FPVTs, Legal);
  }

  for (MVT VT : MVT::fixedlen_vector_valuetypes()) {
    setOperationAction(ISD::LOAD, VT, Custom);
    setOperationAction(ISD::STORE, VT, Custom);
  }
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  if (Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  }

  if (Subtarget.isScalar()) {
    setOperationAction(ISD::ADD, MVT::i32, Custom);
    setOperationAction(ISD::SUB, MVT::i32, Custom);
    setOperationAction(ISD::SHL, MVT::i32, Custom);
    setOperationAction(ISD::SRA, MVT::i32, Custom);
    setOperationAction(ISD::SRL, MVT::i32, Custom);
  }

  setLibcallName(RTLIB::MUL_I128, nullptr);
  setOperationAction(ISD::UMULO, MVT::i64, Custom);
  setOperationAction(ISD::SMULO, MVT::i64, Custom);

  setOperationAction(ISD::MULHS, XLenVT, Expand);
  setOperationAction(ISD::MULHU, XLenVT, Expand);

  if (Subtarget.isScalar()) {
    setOperationAction(ISD::MUL, MVT::i32, Custom);
    if (Subtarget.isWireless()) {
      MVT IntVTs[] = {MVT::i8, MVT::i16, MVT::i32, MVT::i64};
      setOperationAction(ISD::SDIV, IntVTs, LibCall);
      setOperationAction(ISD::UDIV, IntVTs, LibCall);
      setOperationAction(ISD::SREM, IntVTs, LibCall);
      setOperationAction(ISD::UREM, IntVTs, LibCall);
    } else {
      assert(Subtarget.isGeneric());
      MVT IntVTs[] = {MVT::i8, MVT::i16, MVT::i32};
      setOperationAction(ISD::SDIV, IntVTs, Custom);
      setOperationAction(ISD::UDIV, IntVTs, Custom);
      setOperationAction(ISD::UREM, IntVTs, Custom);
    }
  }

  setOperationAction(ISD::SDIVREM, XLenVT, Expand);
  setOperationAction(ISD::UDIVREM, XLenVT, Expand);
  setOperationAction(ISD::SMUL_LOHI, XLenVT, Expand);
  setOperationAction(ISD::UMUL_LOHI, XLenVT, Expand);

  setOperationAction(ISD::SHL_PARTS, XLenVT, Custom);
  setOperationAction(ISD::SRL_PARTS, XLenVT, Custom);
  setOperationAction(ISD::SRA_PARTS, XLenVT, Custom);

  setOperationAction(ISD::ROTL, {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64}, Expand);
  setOperationAction(ISD::ROTR, {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64}, Expand);

  setOperationAction(ISD::BSWAP, XLenVT, Expand);

  // PTO ISA has scalar ctz/clz/bcnt (ctz/clz/bcnt SrcL, M, N, ->Rd). Lower
  // the count-trailing/leading/popcount ops to those instructions with the
  // full XLEN-wide field (M=0, N=XLEN) instead of Expand→SWAR.
  // Note: the ISA ctz returns N for an all-zero field, which matches both
  // ISD::CTTZ (must return XLEN) and CTTZ_ZERO_UNDEF (any value is legal).
  setOperationAction(ISD::CTTZ, XLenVT, Legal);
  setOperationAction(ISD::CTLZ, XLenVT, Legal);
  setOperationAction(ISD::CTPOP, XLenVT, Legal);

  if (!Subtarget.isSIMT() &&
      (Subtarget.enableLegacyISel() || !Subtarget.hasCSel()))
    setOperationAction(ISD::SELECT, XLenVT, Custom);

  if (Subtarget.isScalar() && Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::FP_TO_SINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i32, Custom);
  } else if (Subtarget.isScalar() && !Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::BITCAST, {MVT::i32, MVT::i16}, Custom);
    setOperationAction(ISD::FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::FP_TO_SINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i32, Custom);
    if (Subtarget.hasFloat()) {
      setOperationAction(ISD::FP_TO_UINT, MVT::i16, Custom);
      setOperationAction(ISD::FP_TO_SINT, MVT::i16, Custom);
      setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i16, Custom);
      setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i16, Custom);
    }
  }

  if (Subtarget.isSIMT() || !Subtarget.enableLegacyISel()) {
    // Linxv4 simt block do not have extload or trunc store for floating-point.
    // Expand them.
    // Fill illegal fields of ActionTable is OK. It is unreachable.
    for (auto ValVT : MVT::fp_valuetypes()) {
      for (auto MemVT : MVT::fp_valuetypes()) {
        setLoadExtAction(ISD::EXTLOAD, ValVT, MemVT, Expand);
        setTruncStoreAction(ValVT, MemVT, Expand);
      }
    }

    MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16, MVT::bf16};

    setOperationAction(ISD::STRICT_FMA, FPVTs, Legal);
    setOperationAction(ISD::FMA, FPVTs, Legal);
  }

  setOperationAction(ISD::GlobalAddress, XLenVT, Custom);
  setOperationAction(ISD::BlockAddress, XLenVT, Custom);
  setOperationAction(ISD::ConstantPool, XLenVT, Custom);
  setOperationAction(ISD::JumpTable, XLenVT, Custom);

  setOperationAction(ISD::GlobalTLSAddress, XLenVT, Custom);

  setOperationAction(ISD::TRAP, MVT::Other, Legal);
  setOperationAction(ISD::DEBUGTRAP, MVT::Other, Legal);

  setMaxAtomicSizeInBitsSupported(0);

  setBooleanContents(ZeroOrOneBooleanContent);
  MVT INTVTs[] = {MVT::Other, MVT::i1, MVT::i8,  MVT::i16, MVT::i32, MVT::i64,
                  MVT::f16,   MVT::f32, MVT::f64, MVT::bf16};
  setOperationAction(ISD::INTRINSIC_W_CHAIN, INTVTs, Custom);

  setOperationAction(ISD::INTRINSIC_WO_CHAIN, INTVTs, Custom);

  setOperationAction(ISD::INTRINSIC_VOID, INTVTs, Custom);

  // Function alignments.
  const Align FunctionAlignment(2);
  setMinFunctionAlignment(FunctionAlignment);
  setPrefFunctionAlignment(FunctionAlignment);

  setMinimumJumpTableEntries(5);

  // Jumps are expensive, compared to logic
  setJumpIsExpensive();

  // We can use any register for comparisons
  setHasMultipleConditionRegisters();

  setTargetDAGCombine(ISD::SETCC);

  setTargetDAGCombine(ISD::ZERO_EXTEND);

  if (!Subtarget.enableLegacyISel())
    setTargetDAGCombine(ISD::GlobalAddress);
}

const char *LinxV5TargetLowering::getTargetNodeName(unsigned Opcode) const {
#define MAKE_CASE(V)                                                           \
  case V:                                                                      \
    return #V;
  switch ((LinxV5ISD::NodeType)Opcode) {
  case LinxV5ISD::FIRST_NUMBER:
    break;
    MAKE_CASE(LinxV5ISD::RET_FLAG)
    MAKE_CASE(LinxV5ISD::URET_FLAG)
    MAKE_CASE(LinxV5ISD::SRET_FLAG)
    MAKE_CASE(LinxV5ISD::MRET_FLAG)
    MAKE_CASE(LinxV5ISD::CALL)
    MAKE_CASE(LinxV5ISD::SELECT_CC)
    MAKE_CASE(LinxV5ISD::BuildPairF64)
    MAKE_CASE(LinxV5ISD::TAIL)
    MAKE_CASE(LinxV5ISD::SLLW)
    MAKE_CASE(LinxV5ISD::SRAW)
    MAKE_CASE(LinxV5ISD::SRLW)
    MAKE_CASE(LinxV5ISD::DIVW)
    MAKE_CASE(LinxV5ISD::DIVUW)
    MAKE_CASE(LinxV5ISD::REMUW)
    MAKE_CASE(LinxV5ISD::FCVTU)
    MAKE_CASE(LinxV5ISD::FCVTS)
    MAKE_CASE(LinxV5ISD::BITCAST)
    MAKE_CASE(LinxV5ISD::FunctionBlock)
    MAKE_CASE(LinxV5ISD::ADDlo)
    MAKE_CASE(LinxV5ISD::LOOPSET)
    MAKE_CASE(LinxV5ISD::RDADD)
    MAKE_CASE(LinxV5ISD::RDOR)
    MAKE_CASE(LinxV5ISD::RDMAX)
    MAKE_CASE(LinxV5ISD::RDMIN)
    MAKE_CASE(LinxV5ISD::SHFLUP)
    MAKE_CASE(LinxV5ISD::MAX)
    MAKE_CASE(LinxV5ISD::MIN)
    MAKE_CASE(LinxV5ISD::SHFLDOWN)
    MAKE_CASE(LinxV5ISD::SHFLIDX)
    MAKE_CASE(LinxV5ISD::SHFLXOR)
    MAKE_CASE(LinxV5ISD::VCALL)
    MAKE_CASE(LinxV5ISD::MCALL)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMX)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMXB)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMX_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMXB_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL_SHARED)
    MAKE_CASE(LinxV5ISD::BLK_TLOAD)
    MAKE_CASE(LinxV5ISD::BLK_TSTORE)
    MAKE_CASE(LinxV5ISD::BLK_ACCCVT)
    MAKE_CASE(LinxV5ISD::V5_GMOV)
    MAKE_CASE(LinxV5ISD::V5_SHARED_L2S)
    MAKE_CASE(LinxV5ISD::V5_SHARED_S2L)
    MAKE_CASE(LinxV5ISD::FABS)
    MAKE_CASE(LinxV5ISD::FSQRT)
    MAKE_CASE(LinxV5ISD::FEXP)
    MAKE_CASE(LinxV5ISD::MERGE_PREDICATION)
    MAKE_CASE(LinxV5ISD::CopyP)
    MAKE_CASE(LinxV5ISD::Copy2P)
    MAKE_CASE(LinxV5ISD::Copy2PTerm)
    MAKE_CASE(LinxV5ISD::MERGE_CF)
    MAKE_CASE(LinxV5ISD::IMPLICIT_DEF)
  }
#undef MAKE_CASE
  return nullptr;
}

EVT LinxV5TargetLowering::getSetCCResultType(const DataLayout &DL,
                                             LLVMContext &, EVT VT) const {
  if (!VT.isVector())
    return getPointerTy(DL);
  return VT.changeVectorElementTypeToInteger();
}

bool LinxV5TargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                 const AddrMode &AM, Type *Ty,
                                                 unsigned AS,
                                                 Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // Require a 12-bit signed offset.
  if (!isInt<12>(AM.BaseOffs))
    return false;

  // Unscaled reg + imm12 case.
  if (AM.Scale == 0)
    return true;

  // Scaled reg + imm12 case.
  if (AM.Scale == 1 && !AM.HasBaseReg)
    return true;

  if (AM.BaseOffs != 0)
    return false;
  uint64_t NumBytes = 0;
  if (Ty->isSized()) {
    uint64_t NumBits = DL.getTypeSizeInBits(Ty);
    NumBytes = NumBits / 8;
    if (!isPowerOf2_64(NumBits))
      NumBytes = 0;
  }
  return static_cast<uint64_t>(AM.Scale) == NumBytes && AM.HasBaseReg;
}

bool LinxV5TargetLowering::isLegalICmpImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

bool LinxV5TargetLowering::isLegalAddImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

// The backend supports 16, 32 and 64 bit floating point immediates.
bool LinxV5TargetLowering::isFPImmLegal(const APFloat &Imm, EVT VT,
                                        bool ForCodeSize) const {
  if (Subtarget.isSIMT()) {
    EVT ScalarVT = VT.getScalarType();
    assert(
        ScalarVT == MVT::f32 || ScalarVT == MVT::f64 || ScalarVT == MVT::f16 ||
        ScalarVT == MVT::bf16 && "Unsupported FP immediate type in SIMT mode");
    return true;
  }
  return false;
}

bool LinxV5TargetLowering::isZExtFree(SDValue Val, EVT VT2) const {
  // Zexts are free if they can be combined with a load.
  if (auto *LD = dyn_cast<LoadSDNode>(Val)) {
    EVT MemVT = LD->getMemoryVT();
    if ((MemVT == MVT::i8 || MemVT == MVT::i16 || MemVT == MVT::i32) &&
        (LD->getExtensionType() == ISD::NON_EXTLOAD ||
         LD->getExtensionType() == ISD::ZEXTLOAD))
      return true;
  }

  return TargetLowering::isZExtFree(Val, VT2);
}

bool LinxV5TargetLowering::isSExtFree(SDValue Val) const {
  // Sexts are free if they are done on the results of the following operators:
  // ADD/SUB/AND/OR/XOR/SRL/SRA/SHL
  SDNode *N = Val.getNode();
  unsigned Opc = N->getOpcode();
  switch (Opc) {
  case ISD::ADD:
  case ISD::SUB:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
  case ISD::SRL:
  case ISD::SRA:
  case ISD::SHL:
    return true;
  default:
    return false;
  }
}

bool LinxV5TargetLowering::isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const {
  return SrcVT == MVT::i32 && DstVT == MVT::i64;
}

bool LinxV5TargetLowering::isIntDivCheap(EVT VT, AttributeList Attr) const {
  return false;
}

bool LinxV5TargetLowering::signExtendConstant(const ConstantInt *CI) const {
  return CI->getType()->isIntegerTy(32);
}

bool LinxV5TargetLowering::isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                                      EVT VT) const {
  if (!VT.isSimple())
    return false;

  if (Subtarget.isScalar() && Subtarget.enableLegacyISel())
    // VBXISel do not support fma
    return false;

  switch (VT.getSimpleVT().SimpleTy) {
  case MVT::f64:
  case MVT::f32:
  case MVT::f16:
  case MVT::bf16:
    return Subtarget.hasFloat();
  default:
    return false;
  }
}

bool LinxV5TargetLowering::isSDNodeAlwaysUniform(const SDNode *N) const {
  switch (N->getOpcode()) {
  case ISD::EntryToken:
  case ISD::TokenFactor:
    return true;
  case ISD::INTRINSIC_W_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(1));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch(IntID) {
    case Intrinsic::blkv_if:
    case Intrinsic::blkv_flow:
    case Intrinsic::blkv_if_break:
    case Intrinsic::blkv_loop:
    case Intrinsic::blkv_end_cf:
    case Intrinsic::linx_blkv_rdmax:
    case Intrinsic::linx_blkv_rdmin:
    case Intrinsic::linx_blkv_rdadd:
    case Intrinsic::linx_blkv_rdor:
      return true;
    }
    return false;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch (IntID) {
    case Intrinsic::blkv_get_tile_ptr:
      return true;
    }
    return false;
  }
  case LinxV5ISD::MERGE_PREDICATION:
  case LinxV5ISD::Copy2P:
  case LinxV5ISD::Copy2PTerm:
  case LinxV5ISD::CopyP:
  case LinxV5ISD::IMPLICIT_DEF:
  case LinxV5ISD::RDADD:
  case LinxV5ISD::RDOR:
  case LinxV5ISD::RDMAX:
  case LinxV5ISD::RDMIN:
    return true;
  }
  if (N->isMachineOpcode()) {
    switch (N->getMachineOpcode()) {
      case LinxV5::LinxV5PseudoMergePred:
        return true;
    }
  }
  return false;
}

const TargetRegisterClass *
LinxV5TargetLowering::getRegClassFor(MVT VT, bool isDivergent) const {
  if (!Subtarget.isSIMT())
    return TargetLowering::getRegClassFor(VT, isDivergent);
  // TODO: Need return simtcgv or simtcgs by isDivergent after all uniform
  // instruction is supported.
  if (!isDivergent)
      return &LinxV5::SIMTCGSRegClass;
  return &LinxV5::SIMTCGVRegClass;
}

bool LinxV5TargetLowering::isSDNodeSourceOfDivergence(
    const SDNode *N, FunctionLoweringInfo *FLI,
    LegacyDivergenceAnalysis *KDA) const {
  return isSDNodeSourceOfDivergenceImpl(N, FLI, KDA, false);
}

bool LinxV5TargetLowering::isSDNodeSourceOfDivergenceImpl(
    const SDNode *N, FunctionLoweringInfo *FLI, LegacyDivergenceAnalysis *KDA,
    bool XDivergence) const {
  switch (N->getOpcode()) {
  case ISD::CopyFromReg: {
    const RegisterSDNode *R = cast<RegisterSDNode>(N->getOperand(1));
    const MachineRegisterInfo &MRI = FLI->MF->getRegInfo();
    const LinxV5RegisterInfo *TRI = Subtarget.getRegisterInfo();
    Register Reg = R->getReg();
    bool TreatLC12AsUniform = XDivergence || Subtarget.enableContinuousMemOpt();

    // LiveIns of block C function must be uniform
    if (MRI.isLiveIn(Reg))
      return false;

    if (Reg.isPhysical())
      return !TRI->isUniformReg(MRI, Reg, TreatLC12AsUniform);

    if (const Value *V = FLI->getValueFromVirtualReg(R->getReg()))
      return KDA->isDivergent(V);

    // assert(Reg == FLI->DemoteRegister || isCopyFromRegOfInlineAsm(N));
    return !TRI->isUniformReg(MRI, Reg, TreatLC12AsUniform);
  }
  case ISD::LOAD: {
    return true;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch(IntID) {
      case Intrinsic::blkv_get_index_x:
      return true;
      case Intrinsic::blkv_get_index_y:
      case Intrinsic::blkv_get_index_z:
      return !XDivergence;
      default:
        return false;
    }
    return false;
  }
  case LinxV5ISD::MERGE_CF:
    // MERGE_CF lowers to SIMT_PSEL, which selects SrcL for mask-active lanes and
    // SrcR for inactive lanes. The result is divergent per-lane even when both
    // operands are uniform (e.g., the i1 merge_cf used to compute a loop exit
    // condition in blkv_if_break). Without this, an any_extend of the i8 PSEL
    // result would match UniformUnaryOp and emit a scalar SIMT_ICVT_*_SCAR,
    // creating an invalid VecReg→ScalarReg copy. Marking MERGE_CF as a source
    // of divergence forces the vector (divergent) ICVT path.
    return true;
  default:
    return false;
  }
}

// Changes the condition code and swaps operands if necessary, so the SetCC
// operation matches one of the comparisons supported directly in the LinxV5
// ISA.
static void normaliseSetCC(SDValue &LHS, SDValue &RHS, ISD::CondCode &CC) {
  switch (CC) {
  default:
    break;
  case ISD::SETGT:
  case ISD::SETLE:
  case ISD::SETUGT:
  case ISD::SETULE:
    CC = ISD::getSetCCSwappedOperands(CC);
    std::swap(LHS, RHS);
    break;
  }
}

// Return the LinxV5 branch opcode that matches the given DAG integer
// condition code. The CondCode must be one of those supported by the LinxV5
// ISA (see normaliseSetCC).
static unsigned getVBXBranchOpcodeForIntCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return LinxV5::PseudoVBXBEQ;
  case ISD::SETNE:
    return LinxV5::PseudoVBXBNE;
  case ISD::SETLT:
    return LinxV5::PseudoVBXBLT;
  case ISD::SETGE:
    return LinxV5::PseudoVBXBGE;
  case ISD::SETULT:
    return LinxV5::PseudoVBXBLTU;
  case ISD::SETUGE:
    return LinxV5::PseudoVBXBGEU;
  default:
    llvm_unreachable("Unsupported CondCode");
  }
}

static unsigned getBranchOpcodeForIntCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return LinxV5::SETC_EQ_BR;
  case ISD::SETNE:
    return LinxV5::SETC_NE_BR;
  case ISD::SETLT:
    return LinxV5::SETC_LT_BR;
  case ISD::SETGE:
    return LinxV5::SETC_GE_BR;
  case ISD::SETULT:
    return LinxV5::SETC_LTU_BR;
  case ISD::SETUGE:
    return LinxV5::SETC_GEU_BR;
  default:
    llvm_unreachable("Unsupported CondCode");
  }
}

static void expandMUL128(SelectionDAG &DAG, SDValue LL, SDValue LH, SDValue RL,
                         SDValue RH, SDLoc &dl, EVT NVT, SDValue &Lo,
                         SDValue &Hi) {
  unsigned Bits = NVT.getSizeInBits();
  unsigned HalfBits = Bits >> 1;
  SDValue Mask = DAG.getConstant(APInt::getLowBitsSet(Bits, HalfBits), dl, NVT);
  SDValue LLL = DAG.getNode(ISD::AND, dl, NVT, LL, Mask);
  SDValue RLL = DAG.getNode(ISD::AND, dl, NVT, RL, Mask);

  SDValue T = DAG.getNode(ISD::MUL, dl, NVT, LLL, RLL);
  SDValue TL = DAG.getNode(ISD::AND, dl, NVT, T, Mask);

  SDValue Shift = DAG.getShiftAmountConstant(HalfBits, NVT, dl);
  SDValue TH = DAG.getNode(ISD::SRL, dl, NVT, T, Shift);
  SDValue LLH = DAG.getNode(ISD::SRL, dl, NVT, LL, Shift);
  SDValue RLH = DAG.getNode(ISD::SRL, dl, NVT, RL, Shift);

  SDValue U = DAG.getNode(ISD::ADD, dl, NVT,
                          DAG.getNode(ISD::MUL, dl, NVT, LLH, RLL), TH);
  SDValue UL = DAG.getNode(ISD::AND, dl, NVT, U, Mask);
  SDValue UH = DAG.getNode(ISD::SRL, dl, NVT, U, Shift);

  SDValue V = DAG.getNode(ISD::ADD, dl, NVT,
                          DAG.getNode(ISD::MUL, dl, NVT, LLL, RLH), UL);
  SDValue VH = DAG.getNode(ISD::SRL, dl, NVT, V, Shift);

  SDValue W =
      DAG.getNode(ISD::ADD, dl, NVT, DAG.getNode(ISD::MUL, dl, NVT, LLH, RLH),
                  DAG.getNode(ISD::ADD, dl, NVT, UH, VH));
  Lo = DAG.getNode(ISD::ADD, dl, NVT, TL,
                   DAG.getNode(ISD::SHL, dl, NVT, V, Shift));

  Hi = DAG.getNode(ISD::ADD, dl, NVT, W,
                   DAG.getNode(ISD::ADD, dl, NVT,
                               DAG.getNode(ISD::MUL, dl, NVT, RH, LL),
                               DAG.getNode(ISD::MUL, dl, NVT, RL, LH)));
}

SDValue LinxV5TargetLowering::lowerXMULO(SDValue Op, SelectionDAG &DAG) const {
  unsigned opcode = Op.getOpcode();
  assert((opcode == ISD::UMULO || opcode == ISD::SMULO) && "Invalid Opcode.");

  bool isSigned = (opcode == ISD::SMULO);
  EVT VT = MVT::i64;
  SDLoc dl(Op);
  SDValue LHS = Op.getOperand(0);

  if (LHS.getValueType() != VT)
    return Op;

  SDValue ShiftAmt = DAG.getConstant(63, dl, VT);

  SDValue RHS = Op.getOperand(1);
  SDValue HiLHS, HiRHS;
  if (isSigned) {
    HiLHS = DAG.getNode(ISD::SRA, dl, VT, LHS, ShiftAmt);
    HiRHS = DAG.getNode(ISD::SRA, dl, MVT::i64, RHS, ShiftAmt);
  } else {
    HiLHS = DAG.getConstant(0, dl, VT);
    HiRHS = DAG.getConstant(0, dl, MVT::i64);
  }

  SDValue LO;
  SDValue HI;
  expandMUL128(DAG, LHS, HiLHS, RHS, HiRHS, dl, VT, LO, HI);
  if (isSigned) {
    SDValue Tmp1 = DAG.getNode(ISD::SRA, dl, VT, LO, ShiftAmt);
    HI = DAG.getSetCC(dl, MVT::i64, HI, Tmp1, ISD::SETNE);
  } else {
    HI = DAG.getSetCC(dl, MVT::i64, HI, DAG.getConstant(0, dl, VT), ISD::SETNE);
  }
  SDValue Ops[2] = {LO, HI};
  SDValue Res = DAG.getMergeValues(Ops, dl);
  return Res;
}

/// When intrinsic with varargs, it is not treat as common ISD intrinsic node.
TargetLowering::LegalizeAction
LinxV5TargetLowering::getCustomOperationAction(SDNode &Op) const {
  // TODO: Check for linx kernel launch
  return Legal;
}

static bool isSIMT(const SelectionDAG &DAG) {
  return DAG.getSubtarget<LinxV5Subtarget>().isSIMT();
}

SDValue LinxV5TargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::SMULO:
  case ISD::UMULO:
    return lowerXMULO(Op, DAG);
  case ISD::GlobalAddress:
    return lowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return lowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return lowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return lowerJumpTable(Op, DAG);
  case ISD::GlobalTLSAddress:
    return lowerGlobalTLSAddress(Op, DAG);
  case ISD::SELECT:
    return lowerSELECT(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::FRAMEADDR:
    return lowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return lowerRETURNADDR(Op, DAG);
  case ISD::SHL_PARTS:
    return lowerShiftLeftParts(Op, DAG);
  case ISD::SRA_PARTS:
    return lowerShiftRightParts(Op, DAG, true);
  case ISD::SRL_PARTS:
    return lowerShiftRightParts(Op, DAG, false);
  case ISD::LOAD:
    return lowerLOAD(Op, DAG);
  case ISD::STORE:
    return lowerSTORE(Op, DAG);
  case ISD::BITCAST: {
    if (!Subtarget.enableLegacyISel()) {
      SDLoc DL(Op);
      EVT VT = Op.getValueType();
      if (VT.isVector())
        return SDValue();
      SDValue Op0 = Op.getOperand(0);
      EVT Op0VT = Op0.getValueType();
      assert(Op0VT.isInteger() && "unexpected ValueType!");
      if (Op0VT.getSimpleVT() != MVT::i64) {
        Op0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, Op0);
      }
      return DAG.getNode(LinxV5ISD::BITCAST, DL, VT, Op0);
    }
    return SDValue();
  }
  case ISD::INTRINSIC_W_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(Op->getOperand(1));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(Op);
    auto calcNumTileDefs = [](SDValue Op) {
      unsigned dNum = 0;
      for (auto &V : Op->values()) {
        if (V.isVector())
          ++dNum;
      }
      return dNum;
    };
    auto calcNumTileUses = [](SDValue Op) {
      unsigned uNum = 0;
      for (auto &O : Op->ops()) {
        if (O.getValueType().isVector())
          ++uNum;
      }
      return uNum;
    };
    if (Intrinsic::getBaseName(IntID).startswith("llvm.linx.vcall.par")) {
      unsigned dNum = calcNumTileDefs(Op);
      unsigned uNum = calcNumTileUses(Op);
      return lowerTileOpWithBody(DL, Op, dNum, uNum, DAG, LinxV5ISD::VCALL);
    } else if (Intrinsic::getBaseName(IntID).startswith(
                   "llvm.linx.mcall.par")) {
      unsigned dNum = calcNumTileDefs(Op);
      unsigned uNum = calcNumTileUses(Op);
      return lowerTileOpWithBody(DL, Op, dNum, uNum, DAG, LinxV5ISD::MCALL);
    }
    switch (IntID) {
    default: {
      Op->print(errs(), &DAG);
      report_fatal_error("unimplemented Intrinsic operand");
    }
    case Intrinsic::linx_get_thread_id:
    case Intrinsic::linx_get_thread_idx:
      return lowerGetThreadIdx(Op, DAG);
    case Intrinsic::linx_v5_gmov:
      return lowerV5GMOV(Op, DAG);
    case Intrinsic::linx_v5_shared_l2s_insert:
      return lowerV5SharedL2S(Op, DAG,
                              LinxV5Op::TileOPTMA::TMOV_L2S_INSERT);
    case Intrinsic::linx_v5_shared_l2s_publish:
      return lowerV5SharedL2S(Op, DAG,
                              LinxV5Op::TileOPTMA::TMOV_L2S_PUBLISH);
    case Intrinsic::linx_v5_shared_s2l_broadcast:
      return lowerV5SharedS2L(
          Op, DAG, LinxV5Op::TileOPTMA::TMOV_S2L_BROADCAST);
    case Intrinsic::linx_v5_shared_s2l_extract:
      return lowerV5SharedS2L(Op, DAG,
                              LinxV5Op::TileOPTMA::TMOV_S2L_EXTRACT);
    case Intrinsic::linx_get_simt_ret:
      return lowerGetSIMTRet(Op, DAG);
    case Intrinsic::linx_get_sysreg: {
      return lowerSysGet(Op, DAG);
    }
    case Intrinsic::linx_shuffle_up:
      return lowerShuffle(LinxV5ISD::SHFLUP, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_down:
      return lowerShuffle(LinxV5ISD::SHFLDOWN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_idx:
      return lowerShuffle(LinxV5ISD::SHFLIDX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_bfly:
      return lowerShuffle(LinxV5ISD::SHFLXOR, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blk_matmul:
      return lowerTemplateBLK(LinxV5ISD::BLK_MATMUL, DL, Op, 2, DAG);
    case Intrinsic::linx_blk_matmul_shared:
      return lowerTemplateBLKShared(DL, Op, DAG);
    case Intrinsic::linx_blk_matmul_ac:
      return lowerTemplateBLK(LinxV5ISD::BLK_MATMUL_AC, DL, Op, 3, DAG);
    case Intrinsic::linx_blk_matmulmx:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMX, DL, Op, 4, DAG);
    case Intrinsic::linx_blk_matmulmxb:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMXB, DL, Op, 3, DAG);
    case Intrinsic::linx_blk_matmulmx_ac:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMX_AC, DL, Op, 5, DAG);
    case Intrinsic::linx_blk_matmulmxb_ac:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMXB_AC, DL, Op, 4, DAG);
    case Intrinsic::linx_blk_tload:
      return lowerTLoad(LinxV5ISD::BLK_TLOAD, DL, Op, DAG);
    case Intrinsic::linx_blk_acccvt:
      return lowerACCCVT(DL, Op, DAG);
    case Intrinsic::linx_blkv_fabs:
      return lowerFPArith(LinxV5ISD::FABS, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_fsqrt:
      return lowerFPArith(LinxV5ISD::FSQRT, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_fexp:
      return lowerFPArith(LinxV5ISD::FEXP, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_max:
      return lowerTwoSrcFloat(LinxV5ISD::MAX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_min:
      return lowerTwoSrcFloat(LinxV5ISD::MIN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdmax:
      return lowerReduce(LinxV5ISD::RDMAX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdmin:
      return lowerReduce(LinxV5ISD::RDMIN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdadd:
      return lowerReduce(LinxV5ISD::RDADD, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdor:
      return lowerReduce(LinxV5ISD::RDOR, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::blkv_merge_cf:
      return lowerMergeCF(DL, Op, DAG);
    case Intrinsic::blkv_if_break: {
      SDValue Chain = Op.getOperand(0);
      SDValue ExitCond = DAG.getZExtOrTrunc(Op.getOperand(2), DL, MVT::i64);
      SDValue Broken = DAG.getZExtOrTrunc(Op.getOperand(3), DL, MVT::i64);
      SDValue OldMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    Chain);
      SDValue MergedExit =
          DAG.getNode(LinxV5ISD::MERGE_PREDICATION, DL, MVT::Other,
                      OldMask.getValue(1), ExitCond);
      SDValue NewMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    MergedExit);
      SDValue NextExit = DAG.getNode(ISD::OR, DL, MVT::i64, NewMask, Broken);
      SDValue RestoreMask =
          DAG.getNode(LinxV5ISD::Copy2P, DL, MVT::Other, NewMask.getValue(1),
                      OldMask);
      return DAG.getMergeValues({NextExit, RestoreMask}, DL);
    }
    }
    return SDValue();
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    // Intrinsic without side effect.
    ConstantSDNode *CN = cast<ConstantSDNode>(Op->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(Op);
    switch (IntID) {
    default: {
      Op->print(errs(), &DAG);
      report_fatal_error("unimplemented Intrinsic operand");
    }
    case Intrinsic::blkv_get_tile_ptr: {
      Register Reg =
          cast<RegisterSDNode>(Op.getOperand(1).getOperand(1))->getReg();
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, Op.getValueType());
    }
    case Intrinsic::blkv_get_index_x:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC0,
                                MVT::i16);
    case Intrinsic::blkv_get_index_y:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC1,
                                MVT::i16);
    case Intrinsic::blkv_get_index_z:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC2,
                                MVT::i16);
    case Intrinsic::linx_get_thread_id:
    case Intrinsic::linx_get_thread_idx:
      return lowerGetThreadIdx(Op, DAG);
    }
  }
  case ISD::INTRINSIC_VOID: {
    return LowerINTRINSIC_VOID(Op, DAG);
  }
  case ISD::SETCC: {
    if (!Op.getNode()->isDivergent()) {
      SDLoc DL(Op);
      SDValue In1 = DAG.getSExtOrTrunc(Op.getOperand(0), DL, MVT::i64);
      SDValue In2 = DAG.getSExtOrTrunc(Op.getOperand(1), DL, MVT::i64);
      return DAG.getNode(ISD::SETCC, DL, Op.getValueType(), In1, In2, Op.getOperand(2));
    }
    return SDValue();
  }
  default: {
    Op->print(errs(), &DAG);
    report_fatal_error("unimplemented operand");
  }
  }
}

static SDValue getTargetNode(GlobalAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  if (!DAG.getSubtarget<LinxV5Subtarget>().enableLegacyISel())
    return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, N->getOffset(),
                                      Flags);
  return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                   Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

static bool canCallSIMT(const SelectionDAG &DAG) { return !isSIMT(DAG); }

// operands = [chain, intrinsic_id, src]
SDValue LinxV5TargetLowering::lowerReduce(unsigned Opcode, SDLoc &DL,
                                          SDValue Chain, SDValue Op,
                                          SelectionDAG &DAG) const {
  SDValue Src = Op.getOperand(2);
  SDValue Result = DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                               {Chain, Src});
  return Result;
}

SDValue LinxV5TargetLowering::lowerShuffle(unsigned Opcode, SDLoc &DL,
                                           SDValue Chain, SDValue Op,
                                           SelectionDAG &DAG) const {
  if (Opcode == LinxV5ISD::SHFLXOR) {
    SDValue Shfl =
        DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                    Chain, Op.getOperand(2), Op.getOperand(3));
    return Shfl;
  }
  SDValue Shfl =
      DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                  Chain, Op.getOperand(2), Op.getOperand(3), Op.getOperand(4));
  return Shfl;
}

// operands = [chain, intrinsic_id, lhs, rhs]
SDValue LinxV5TargetLowering::lowerTwoSrcFloat(unsigned Opcode, SDLoc &DL,
                                               SDValue Chain, SDValue Op,
                                               SelectionDAG &DAG) const {

  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);

  SDValue Result =
      DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                  {Chain, LHS, RHS});
  return Result;
}

// operands = [chain, intrinsic_id, lhs, rhs]
SDValue LinxV5TargetLowering::lowerMergeCF(SDLoc &DL, SDValue Op,
                                               SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);

  assert(LHS.getValueType() == RHS.getValueType() && "must merge data with the same type");

  SDValue Result;
  if (LHS.getValueType() == MVT::i1) {
    SDValue LHSPromote = DAG.getSExtOrTrunc(LHS, DL, MVT::i8);
    SDValue RHSPromote = DAG.getSExtOrTrunc(RHS, DL, MVT::i8);
    SDValue Merge = DAG.getNode(LinxV5ISD::MERGE_CF, DL,
                         DAG.getVTList(MVT::i8, MVT::Other),
                         Chain, LHSPromote, RHSPromote);
    Result = DAG.getSExtOrTrunc(Merge, DL, MVT::i1);
  } else {
    Result = DAG.getNode(LinxV5ISD::MERGE_CF, DL,
                          DAG.getVTList(Op.getValueType(), MVT::Other),
                          Chain, LHS, RHS);
  }

  return Result;
}

static unsigned calculateVCallSizeMask(EVT Type, unsigned MaxCode = 12) {
  // The tile register type's size is the per-PE tile size and encodes directly
  // (no division by 4): SizeCode = Log2(SizeBytes)-6 (128 B -> 1). Local B.IOT
  // Local B.IOT and Shared B.IOS destinations both allow SizeCode 1..12
  // (128 B..256 KB per the active PTO-ISA contract).
  uint64_t SizeBytes = Type.getFixedSizeInBits() / 8;
  if (!isPowerOf2_64(SizeBytes) || SizeBytes < 128)
    report_fatal_error(
        "LinxV5 Tile size must be a power of two from 128 B upward");
  unsigned Code = Log2_64(SizeBytes) - 6; // 128 -> 1
  if (Code > MaxCode)
    report_fatal_error("LinxV5 Tile size exceeds the per-PE capacity for its "
                       "destination role");
  return Code;
}

static uint64_t getV5ConstantOperand(SDValue Op, StringRef Name,
                                     uint64_t MaxValue, bool AllowZero = true) {
  const auto *Constant = dyn_cast<ConstantSDNode>(Op);
  if (!Constant)
    report_fatal_error(Twine("LinxV5 ") + Name +
                       " operand must be a compile-time constant");
  uint64_t Value = Constant->getZExtValue();
  if (Value > MaxValue || (!AllowZero && Value == 0))
    report_fatal_error(Twine("invalid LinxV5 ") + Name + " operand");
  return Value;
}

static SDValue lowerV5GMOV(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(2), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(3), "PE mask", 15, false);
  if (const auto *PeerTID = dyn_cast<ConstantSDNode>(Op.getOperand(4)))
    if (PeerTID->getZExtValue() > 3)
      report_fatal_error("invalid LinxV5 GMOV peer thread ID");
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64),
      Op.getOperand(4), Op.getOperand(5)};
  return DAG.getNode(LinxV5ISD::V5_GMOV, DL, Op->getVTList(), Ops);
}

static SDValue lowerV5SharedS2L(SDValue Op, SelectionDAG &DAG,
                                unsigned Function) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(3), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(4), "PE mask", 15, false);
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(Function, DL, MVT::i64),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      Op.getOperand(2),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64)};
  return DAG.getNode(LinxV5ISD::V5_SHARED_S2L, DL, Op->getVTList(), Ops);
}

static SDValue lowerV5SharedL2S(SDValue Op, SelectionDAG &DAG,
                                unsigned Function) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(2), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(3), "PE mask", 15, false);
  SDValue SrcTile = Op.getOperand(4);
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(Function, DL, MVT::i64),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(SrcTile.getValueType()), DL,
                            MVT::i64),
      SrcTile};
  return DAG.getNode(LinxV5ISD::V5_SHARED_L2S, DL,
                     DAG.getVTList(MVT::i64, MVT::Other), Ops);
}

/// Intrinsic Ops: (0)Chain; (1)IntNo; (2)FuncPtr; (3,4,5): Dimensions; (6):
/// Tile, ...
SDValue LinxV5TargetLowering::lowerTileOpWithBody(SDLoc &DL, SDValue Op,
                                                  unsigned VDefNum,
                                                  unsigned VUseNum,
                                                  SelectionDAG &DAG,
                                                  unsigned Opcode) const {
  MachineFunction &MF = DAG.getMachineFunction();
  Module *M = MF.getFunction().getParent();

  SmallVector<SDValue> Ops;

  // Chain and Glue
  SDValue Chain = Op.getOperand(0);
  unsigned GetNum = Op.getNumOperands() - 6 - VUseNum;
  SmallVector<SDValue> GPRUses;  // SDValue
  for (unsigned i = 0; i < GetNum; ++i) {
    SDValue Src = Op.getOperand(i + 6 + VUseNum);
    if (Src.getValueType().isInteger() && Src.getValueType() != MVT::i64) {
      Src = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, Src);
    }
    GPRUses.push_back(Src);
  }
  Ops.push_back(Chain);

  SDValue FuncPtr = Op.getOperand(2);
  const GlobalValue *GV = dyn_cast<GlobalAddressSDNode>(FuncPtr)->getGlobal();
  FuncPtr = DAG.getTargetGlobalAddress(GV, DL, MVT::i64);
  Ops.push_back(FuncPtr);

  Ops.push_back(Op.getOperand(3)); // Dim-X
  Ops.push_back(Op.getOperand(4)); // Dim-Y
  Ops.push_back(Op.getOperand(5)); // Dim-Z

  SDNode *N = Op.getNode();
  for (int i = 0; i < VDefNum; i++) {
    unsigned TileSize = calculateVCallSizeMask(SDValue(N, i).getValueType());
    SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
    Ops.push_back(SizeValue); // Tile Size
  }

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 6);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  if (const Function *F = dyn_cast<Function>(GV)) {
    SmallString<32> StackSizeName(F->getName());
    StackSizeName += "_stack_size";
    GlobalVariable *SSGV = M->getGlobalVariable(StackSizeName.str());
    if (!SSGV) {
      Type *Ty = Type::getInt32Ty(M->getContext());
      SSGV = new GlobalVariable(*M, Ty, false, GlobalValue::ExternalLinkage, nullptr, StackSizeName.str());
    }
    // TODO: support specifying stack size with register
    SDValue StackSizeSym = DAG.getTargetGlobalAddress(SSGV, DL, MVT::i64);
    Ops.push_back(StackSizeSym);
  }

  for (SDValue Reg : GPRUses) {
    Ops.push_back(Reg);
  }

  SDValue Call;
  Call = DAG.getNode(Opcode, DL, Op->getVTList(), Ops);
  return Call;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6~): Tile Input
SDValue LinxV5TargetLowering::lowerTemplateBLK(unsigned Opcode, SDLoc &DL,
                                               SDValue Op, unsigned VUseNum,
                                               SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementTypeA = Op.getOperand(5);
  unsigned TypeEnumA = cast<ConstantSDNode>(TileElementTypeA)->getZExtValue();
  SDValue TypeEnumValueA = DAG.getTargetConstant(TypeEnumA, DL, MVT::i64);
  Ops.push_back(TypeEnumValueA);

  SDValue TileElementTypeB = Op.getOperand(6);
  unsigned TypeEnumB = cast<ConstantSDNode>(TileElementTypeB)->getZExtValue();
  SDValue TypeEnumValueB = DAG.getTargetConstant(TypeEnumB, DL, MVT::i64);
  Ops.push_back(TypeEnumValueB);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 7);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops for blk_matmul_shared:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type A; (6): Tile Element Type B;
/// (7): Local Tile Input A; (8): Shared SSA handle.
/// The Shared Right (B) is NOT a tile operand here — it is bound by C.B.IOS
/// at MC expansion. Node result is the implicit ACC tile plus Other.
SDValue LinxV5TargetLowering::lowerTemplateBLKShared(SDLoc &DL, SDValue Op,
                                                      SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementTypeA = Op.getOperand(5);
  unsigned TypeEnumA = cast<ConstantSDNode>(TileElementTypeA)->getZExtValue();
  Ops.push_back(DAG.getTargetConstant(TypeEnumA, DL, MVT::i64));

  SDValue TileElementTypeB = Op.getOperand(6);
  unsigned TypeEnumB = cast<ConstantSDNode>(TileElementTypeB)->getZExtValue();
  Ops.push_back(DAG.getTargetConstant(TypeEnumB, DL, MVT::i64));

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  Ops.push_back(DAG.getTargetConstant(TileSize, DL, MVT::i64));

  SDValue TileUseA = Op.getOperand(7);
  if (TileUseA.isUndef()) {
    Op->print(errs(), &DAG);
    report_fatal_error("\nPlease initialize tile register before use!");
  }
  Ops.push_back(TileUseA); // Local A tile (B is Shared, bound by C.B.IOS)

  Ops.push_back(Op.getOperand(8));

  SDValue VCall = DAG.getNode(
      LinxV5ISD::BLK_MATMUL_SHARED, DL,
      DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0) Chain; (1) IntNo; (2,3,4): Dimensions;
/// (5,6): AType, BType; (7~): Tile Input
SDValue LinxV5TargetLowering::lowerTemplateBLKMX(unsigned Opcode, SDLoc &DL,
                                                 SDValue Op, unsigned VUseNum,
                                                 SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-M
  Ops.push_back(Op.getOperand(3)); // Dim-N
  Ops.push_back(Op.getOperand(4)); // Dim-K

  SDValue AType = Op.getOperand(5);
  unsigned ATypeEnum = cast<ConstantSDNode>(AType)->getZExtValue();
  SDValue ATypeValue = DAG.getTargetConstant(ATypeEnum, DL, MVT::i64);
  Ops.push_back(ATypeValue);

  SDValue BType = Op.getOperand(6);
  unsigned BTypeEnum = cast<ConstantSDNode>(BType)->getZExtValue();
  SDValue BTypeValue = DAG.getTargetConstant(BTypeEnum, DL, MVT::i64);
  Ops.push_back(BTypeValue);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 7);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6): PadValue; (7): Layout
/// (8): DType, (9): Stride
SDValue LinxV5TargetLowering::lowerTLoad(unsigned Opcode, SDLoc &DL, SDValue Op,
                                         SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementType = Op.getOperand(5);
  unsigned TypeEnum = cast<ConstantSDNode>(TileElementType)->getZExtValue();
  SDValue TypeEnumValue = DAG.getTargetConstant(TypeEnum, DL, MVT::i64);
  Ops.push_back(TypeEnumValue);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  SDValue PadValueType = Op.getOperand(6);
  SDValue LayoutType = Op.getOperand(7);
  unsigned PadValueTypeEnum =
      cast<ConstantSDNode>(PadValueType)->getSExtValue();
  unsigned LayoutTypeEnum = cast<ConstantSDNode>(LayoutType)->getSExtValue();
  Ops.push_back(DAG.getTargetConstant(LayoutTypeEnum, DL, MVT::i64)); // Layout
  Ops.push_back(
      DAG.getTargetConstant(PadValueTypeEnum, DL, MVT::i64)); // PadValue

  Ops.push_back(Op.getOperand(8)); // DTypePtr
  Ops.push_back(Op.getOperand(9)); // Stride

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6): Layout
/// (7): DType, (8): Stride, (9): Tile Input
SDValue LinxV5TargetLowering::lowerTStore(unsigned Opcode, SDLoc &DL,
                                          SDValue Op, SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementType = Op.getOperand(5);
  unsigned TypeEnum = cast<ConstantSDNode>(TileElementType)->getZExtValue();
  SDValue TypeEnumValue = DAG.getTargetConstant(TypeEnum, DL, MVT::i64);
  Ops.push_back(TypeEnumValue);

  SDValue LayoutType = Op.getOperand(6);
  unsigned LayoutTypeEnum = cast<ConstantSDNode>(LayoutType)->getSExtValue();
  Ops.push_back(DAG.getTargetConstant(LayoutTypeEnum, DL, MVT::i64)); // Layout

  Ops.push_back(Op.getOperand(7)); // DTypePtr
  Ops.push_back(Op.getOperand(8)); // Stride

  SDValue TileUse = Op.getOperand(9);
  Ops.push_back(TileUse); // Tile Use
  if (TileUse.isUndef()) {
    Op->print(errs(), &DAG);
    report_fatal_error("\nPlease initialize tile register before use!");
  }

  SDValue VCall = DAG.getNode(Opcode, DL, DAG.getVTList(MVT::Other), Ops);
  return VCall;
}

SDValue LinxV5TargetLowering::lowerACCCVT(SDLoc &DL, SDValue Op,
                                          SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0)); // chain
  Ops.push_back(Op.getOperand(2)); // M
  Ops.push_back(Op.getOperand(3)); // N
  Ops.push_back(Op.getOperand(4)); // SrcType
  Ops.push_back(Op.getOperand(5)); // DstType
  Ops.push_back(DAG.getConstant(calculateVCallSizeMask(Op.getValueType()), DL,
                                MVT::i64)); // TileSize
  Ops.push_back(Op.getOperand(6));          // Layout
  Ops.push_back(Op.getOperand(7));          // canon
  Ops.push_back(Op.getOperand(8));          // ACC tile
  SDValue Res = DAG.getNode(LinxV5ISD::BLK_ACCCVT, DL,
                            {Op.getValueType(), MVT::Other}, Ops);
  return Res;
}

SDValue LinxV5TargetLowering::lowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *LD = cast<LoadSDNode>(Op);
  EVT VT = LD->getValueType(0);

  if (VT.isFixedLengthVector()) {
    SmallVector<SDValue> Ops;
    Ops.push_back(LD->getChain());
    unsigned ColValid = VT.getFixedSizeInBits() / 64;
    Ops.push_back(DAG.getTargetConstant(ColValid, DL, MVT::i64)); // Dim-X
    Ops.push_back(DAG.getTargetConstant(1, DL, MVT::i64)); // Dim-Y

    SDValue ColValue = DAG.getTargetConstant(ColValid, DL, MVT::i64);
    Ops.push_back(ColValue); // Dim-Z

    SDValue DataTypeValue =
        DAG.getTargetConstant(LinxV5Op::DataType::S64, DL, MVT::i64);
    Ops.push_back(DataTypeValue); // DataType
    unsigned TileSize = calculateVCallSizeMask(VT);
    SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
    Ops.push_back(SizeValue);

    Ops.push_back(
        DAG.getTargetConstant(LinxV5Op::ArgFormat::NORM, DL, MVT::i64));
    Ops.push_back(
        DAG.getTargetConstant(LinxV5Op::PadValue::Zero, DL, MVT::i64));

    Ops.push_back(LD->getBasePtr());
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    SDValue VCall =
        DAG.getNode(LinxV5ISD::BLK_TLOAD, DL,
                    DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
    return VCall;
  }
  return SDValue();
}

SDValue LinxV5TargetLowering::lowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *ST = cast<StoreSDNode>(Op);
  SDLoc DL(Op);

  EVT ValueVT = ST->getValue().getValueType();
  EVT MemVT = ST->getMemoryVT();
  if (ValueVT.isFixedLengthVector()) {
    SmallVector<SDValue> Ops;
    Ops.push_back(ST->getChain());
    unsigned ColValid = MemVT.getFixedSizeInBits() / 64;
    Ops.push_back(DAG.getTargetConstant(ColValid, DL, MVT::i64)); // Dim-X
    Ops.push_back(DAG.getTargetConstant(1, DL, MVT::i64)); // Dim-Y

    SDValue ColValue = DAG.getTargetConstant(ColValid, DL, MVT::i64);
    Ops.push_back(ColValue); // Dim-Z

    SDValue DataTypeValue =
        DAG.getTargetConstant(LinxV5Op::DataType::S64, DL, MVT::i64);
    Ops.push_back(DataTypeValue); // DataType

    Ops.push_back(
        DAG.getTargetConstant(LinxV5Op::ArgFormat::NORM, DL, MVT::i64));

    Ops.push_back(ST->getBasePtr());
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(ST->getValue());
    SDValue VCall =
        DAG.getNode(LinxV5ISD::BLK_TSTORE, DL, DAG.getVTList(MVT::Other), Ops);
    return VCall;
  }
  return SDValue();
}

SDValue LinxV5TargetLowering::LowerINTRINSIC_VOID(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDLoc DL(Op);
  unsigned IntNo = cast<ConstantSDNode>(Op.getOperand(1))->getZExtValue();
  auto calcNumTileUses = [](SDValue Op) {
    unsigned uNum = 0;
    for (auto &O : Op->ops()) {
      if (O.getValueType().isVector())
        ++uNum;
    }
    return uNum;
  };
  if (canCallSIMT(DAG) && IntNo == Intrinsic::linx_set_loop_iterations) {
    return lowerSetLoopIterations(Op, DAG);
  } else if (Intrinsic::getBaseName(IntNo).startswith("llvm.linx.vcall.par")) {
    unsigned uNum = calcNumTileUses(Op);
    return lowerTileOpWithBody(DL, Op, 0, uNum, DAG, LinxV5ISD::VCALL);
  } else if (Intrinsic::getBaseName(IntNo).startswith("llvm.linx.mcall.par")) {
    unsigned uNum = calcNumTileUses(Op);
    return lowerTileOpWithBody(DL, Op, 0, uNum, DAG, LinxV5ISD::MCALL);
  } else if (IntNo == Intrinsic::linx_blk_tstore) {
    return lowerTStore(LinxV5ISD::BLK_TSTORE, DL, Op, DAG);
  } else if (IntNo == Intrinsic::blkv_end_cf) {
    SDValue OldMask = Op->getOperand(2);
    SDValue RestoreMask =
        DAG.getNode(LinxV5ISD::Copy2PTerm, DL, MVT::Other, OldMask.getValue(1),
                     OldMask);
    return RestoreMask;
  } else {
    Op->print(errs(), &DAG);
    llvm_unreachable("Unexpected Intrinsic Void Opcode!");
  }
  return SDValue();
}

// operands = [chain, intrinsic_id, val]
SDValue LinxV5TargetLowering::lowerFPArith(unsigned Opcode, SDLoc &DL,
                                           SDValue Chain, SDValue Op,
                                           SelectionDAG &DAG) const {

  SDValue VAL = Op.getOperand(2);

  SDValue Result = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), {Chain, VAL});
  return Result;
}

template <class NodeTy>
SDValue LinxV5TargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                      bool IsLocal) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());

  if (!DAG.getSubtarget<LinxV5Subtarget>().enableLegacyISel()) {
    SDValue HiAddr = getTargetNode(N, DL, Ty, DAG, LinxV5II::MO_TPCREL_HI);
    MCSymbol *LocSymbol =
        DAG.getMachineFunction().getContext().createTempSymbol();
    SDValue Label = DAG.getMCSymbol(LocSymbol, Ty);
    SDNode *Hi =
        DAG.getMachineNode(LinxV5::PseudoADDTPC_HI, DL, Ty, Label, HiAddr);
    SDValue LoAddr = DAG.getMCSymbol(LocSymbol, Ty, LinxV5II::MO_TPCREL_LO);
    return DAG.getNode(LinxV5ISD::ADDlo, DL, Ty, SDValue(Hi, 0), LoAddr);
  } else {
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    return SDValue(DAG.getMachineNode(LinxV5::PseudoVBXCONST, DL, Ty, Addr), 0);
  }
}

SDValue LinxV5TargetLowering::lowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  int64_t Offset = N->getOffset();
  MVT XLenVT = Subtarget.getXLenVT();

  const GlobalValue *GV = N->getGlobal();
  bool IsLocal = getTargetMachine().shouldAssumeDSOLocal(*GV->getParent(), GV);

  // When enableLegacyISel is disabled, the offset processing of Addr is
  // performed in the getTargetNode() function in the getAddr() function. When
  // this function is enabled, the getTargetNode() function sets offset to 0 to
  // enable optimization after specific patterns (addtpc+add) are identified.
  // Instead,  we manually add the following 'ADD' command to complete the
  // information.
  SDValue Addr = getAddr(N, DAG, IsLocal);

  // In order to maximise the opportunity for common subexpression elimination,
  // emit a separate ADD node for the global address offset instead of folding
  // it in the global address node. Later peephole optimisations may choose to
  // fold it back in when profitable.
  if (Offset != 0 && DAG.getSubtarget<LinxV5Subtarget>().enableLegacyISel())
    return DAG.getNode(ISD::ADD, DL, Ty, Addr,
                       DAG.getConstant(Offset, DL, XLenVT));
  return Addr;
}

SDValue LinxV5TargetLowering::lowerBlockAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  BlockAddressSDNode *N = cast<BlockAddressSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue LinxV5TargetLowering::lowerConstantPool(SDValue Op,
                                                SelectionDAG &DAG) const {
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue LinxV5TargetLowering::lowerJumpTable(SDValue Op,
                                             SelectionDAG &DAG) const {
  JumpTableSDNode *N = cast<JumpTableSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue LinxV5TargetLowering::getStaticTLSAddr(GlobalAddressSDNode *N,
                                               SelectionDAG &DAG,
                                               bool UseGOT) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  const GlobalValue *GV = N->getGlobal();

  if (UseGOT) {
    report_fatal_error("not support IE yet");
  }

  if (Subtarget.enableLegacyISel()) {
    // generate tprel-const. Here isn't the final ISel of Linx. Let VBX to
    // lowering constant TLS value.
    SDValue Addr =
        DAG.getTargetGlobalAddress(GV, DL, Ty, 0, LinxV5II::MO_TPREL);

    SDValue MN =
        SDValue(DAG.getMachineNode(LinxV5::PseudoVBXCONST, DL, Ty, Addr), 0);

    SDValue GetTP =
        SDValue(DAG.getMachineNode(LinxV5::PseudoVBXSYSGET, DL, Ty,
                                   DAG.getTargetConstant(0, DL, MVT::i64)),
                0);
    SDValue MNAdd = DAG.getNode(ISD::ADD, DL, Ty, MN, GetTP);
    return MNAdd;
  } else {
    SDValue TP =
        SDValue(DAG.getMachineNode(LinxV5::SSR_GET, DL, Ty,
                                   DAG.getTargetConstant(0, DL, MVT::i64)),
                0);
    SDValue HiOff =
        DAG.getTargetGlobalAddress(GV, DL, Ty, 0, LinxV5II::MO_TPREL_HI);
    SDValue LoOff =
        DAG.getTargetGlobalAddress(GV, DL, Ty, 0, LinxV5II::MO_TPREL_LO);
    SDValue TPOff = SDValue(DAG.getMachineNode(LinxV5::LUI, DL, Ty, HiOff), 0);
    TPOff = DAG.getNode(LinxV5ISD::ADDlo, DL, Ty, TPOff, LoOff);
    return DAG.getNode(ISD::ADD, DL, Ty, TP, TPOff);
  }
}

SDValue LinxV5TargetLowering::getDynamicTLSAddr(GlobalAddressSDNode *N,
                                                SelectionDAG &DAG) const {
  report_fatal_error("not support Dynamic TLS yet!");
}

SDValue LinxV5TargetLowering::lowerGlobalTLSAddress(SDValue Op,
                                                    SelectionDAG &DAG) const {
  SDLoc DL(Op);
  EVT Ty = Op.getValueType();
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  int64_t Offset = N->getOffset();
  MVT XLenVT = Subtarget.getXLenVT();

  TLSModel::Model Model = getTargetMachine().getTLSModel(N->getGlobal());

  SDValue Addr;
  switch (Model) {
  case TLSModel::LocalExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/false);
    break;
  case TLSModel::InitialExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/true);
    break;
  case TLSModel::LocalDynamic:
  case TLSModel::GeneralDynamic:
    Addr = getDynamicTLSAddr(N, DAG);
    break;
  }

  // In order to maximise the opportunity for common subexpression elimination,
  // emit a separate ADD node for the global address offset instead of folding
  // it in the global address node. Later peephole optimisations may choose to
  // fold it back in when profitable.
  if (Offset != 0)
    return DAG.getNode(ISD::ADD, DL, Ty, Addr,
                       DAG.getConstant(Offset, DL, XLenVT));
  return Addr;
}

SDValue LinxV5TargetLowering::lowerSELECT(SDValue Op, SelectionDAG &DAG) const {
  SDValue CondV = Op.getOperand(0);
  SDValue TrueV = Op.getOperand(1);
  SDValue FalseV = Op.getOperand(2);
  SDLoc DL(Op);
  MVT XLenVT = Subtarget.getXLenVT();
  // If the result type is XLenVT and CondV is the output of a SETCC node
  // which also operated on XLenVT inputs, then merge the SETCC node into the
  // lowered LinxV5ISD::SELECT_CC to take advantage of the integer
  // compare+branch instructions. i.e.:
  // (select (setcc lhs, rhs, cc), truev, falsev)
  // -> (linxisd::select_cc lhs, rhs, cc, truev, falsev)
  if (Op.getSimpleValueType() == XLenVT && CondV.getOpcode() == ISD::SETCC &&
      CondV.getOperand(0).getSimpleValueType() == XLenVT) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    auto CC = cast<CondCodeSDNode>(CondV.getOperand(2));
    ISD::CondCode CCVal = CC->get();

    normaliseSetCC(LHS, RHS, CCVal);

    SDValue TargetCC = DAG.getConstant(CCVal, DL, XLenVT);
    SDValue Ops[] = {LHS, RHS, TargetCC, TrueV, FalseV};
    return DAG.getNode(LinxV5ISD::SELECT_CC, DL, Op.getValueType(), Ops);
  }

  // Otherwise:
  // (select condv, truev, falsev)
  // -> (linxisd::select_cc condv, zero, setne, truev, falsev)
  SDValue Zero = DAG.getConstant(0, DL, XLenVT);
  SDValue SetNE = DAG.getConstant(ISD::SETNE, DL, XLenVT);

  SDValue Ops[] = {CondV, Zero, SetNE, TrueV, FalseV};

  return DAG.getNode(LinxV5ISD::SELECT_CC, DL, Op.getValueType(), Ops);
}

SDValue LinxV5TargetLowering::lowerVASTART(SDValue Op,
                                           SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  LinxV5MachineFunctionInfo *FuncInfo = MF.getInfo<LinxV5MachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(MF.getDataLayout()));

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue LinxV5TargetLowering::lowerFRAMEADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  const LinxV5RegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setFrameAddressIsTaken(true);
  Register FrameReg = RI.getFrameRegister(MF);
  int XLenInBytes = static_cast<int>(Subtarget.getXLen() / 8);

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), DL, FrameReg, VT);
  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  while (Depth--) {
    int Offset = -(XLenInBytes * 2);
    SDValue Ptr = DAG.getNode(ISD::ADD, DL, VT, FrameAddr,
                              DAG.getIntPtrConstant(Offset, DL));
    FrameAddr =
        DAG.getLoad(VT, DL, DAG.getEntryNode(), Ptr, MachinePointerInfo());
  }
  return FrameAddr;
}

SDValue LinxV5TargetLowering::lowerRETURNADDR(SDValue Op,
                                              SelectionDAG &DAG) const {
  const LinxV5RegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setReturnAddressIsTaken(true);
  MVT XLenVT = Subtarget.getXLenVT();
  int XLenInBytes = static_cast<int>(Subtarget.getXLen() / 8);

  if (verifyReturnAddressArgumentIsConstant(Op, DAG))
    return SDValue();

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  if (Depth) {
    int Off = -XLenInBytes;
    SDValue FrameAddr = lowerFRAMEADDR(Op, DAG);
    SDValue Offset = DAG.getConstant(Off, DL, VT);
    return DAG.getLoad(VT, DL, DAG.getEntryNode(),
                       DAG.getNode(ISD::ADD, DL, VT, FrameAddr, Offset),
                       MachinePointerInfo());
  }

  // Return the value of the return address register, marking it an implicit
  // live-in.
  Register Reg = MF.addLiveIn(RI.getRARegister(), getRegClassFor(XLenVT));
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, XLenVT);
}

SDValue LinxV5TargetLowering::lowerShiftLeftParts(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // if Shamt-XLEN < 0: // Shamt < XLEN
  //   Lo = Lo << Shamt
  //   Hi = (Hi << Shamt) | ((Lo >>u 1) >>u (XLEN-1 - Shamt))
  // else:
  //   Lo = 0
  //   Hi = Lo << (Shamt-XLEN)

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::SUB, DL, VT, XLenMinus1, Shamt);

  SDValue LoTrue = DAG.getNode(ISD::SHL, DL, VT, Lo, Shamt);
  SDValue ShiftRight1Lo = DAG.getNode(ISD::SRL, DL, VT, Lo, One);
  SDValue ShiftRightLo =
      DAG.getNode(ISD::SRL, DL, VT, ShiftRight1Lo, XLenMinus1Shamt);
  SDValue ShiftLeftHi = DAG.getNode(ISD::SHL, DL, VT, Hi, Shamt);
  SDValue HiTrue = DAG.getNode(ISD::OR, DL, VT, ShiftLeftHi, ShiftRightLo);
  SDValue HiFalse = DAG.getNode(ISD::SHL, DL, VT, Lo, ShamtMinusXLen);

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, Zero);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue LinxV5TargetLowering::lowerGetThreadIdx(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue SSRId = DAG.getTargetConstant(0x0802, DL, MVT::i64);
  return SDValue(DAG.getMachineNode(LinxV5::SSR_GET, DL, Op.getValueType(),
                                    SSRId),
                 0);
}

SDValue LinxV5TargetLowering::lowerGetSIMTRet(SDValue Op,
                                              SelectionDAG &DAG) const {
  assert(!DAG.getSubtarget<LinxV5Subtarget>().isSIMT());
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  unsigned RetNo = cast<ConstantSDNode>(Op->getOperand(2))->getZExtValue();
  return DAG.getCopyFromReg(Chain, DL, LinxV5::getReduceDst(RetNo),
                            Op.getValueType());
}

SDValue LinxV5TargetLowering::lowerSysGet(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  auto ConstNode = cast<ConstantSDNode>(Op->getOperand(2));
  uint64_t Imm = ConstNode->getZExtValue();
  SDValue ConstID = DAG.getTargetConstant(Imm, DL, MVT::i64);

  SmallVector<SDValue, 4> Ops;
  Ops.push_back(ConstID);
  Ops.push_back(Op->getOperand(0));

  if (Subtarget.enableLegacyISel()) {
    return SDValue(DAG.getMachineNode(LinxV5::PseudoVBXSYSGET, DL,
                                      Op.getValueType(), MVT::Other, Ops),
                   0);
  } else {
    return SDValue(DAG.getMachineNode(LinxV5::SSR_GET, DL, Op.getValueType(),
                                      MVT::Other, Ops),
                   0);
  }
}

SDValue LinxV5TargetLowering::lowerSetLoopIterations(SDValue Op,
                                                     SelectionDAG &DAG) const {
  assert(!DAG.getSubtarget<LinxV5Subtarget>().isSIMT());
  SDLoc DL(Op);
  unsigned Layper = cast<ConstantSDNode>(Op->getOperand(2))->getZExtValue();
  SDValue Chain = Op.getOperand(0);
  SDValue Src = Op.getOperand(3);
  return DAG.getNode(LinxV5ISD::LOOPSET, DL, MVT::Other, Chain,
                     DAG.getRegister(LinxV5::getLoopBoundary(Layper), MVT::i64),
                     Src);
}

SDValue LinxV5TargetLowering::lowerShiftRightParts(SDValue Op,
                                                   SelectionDAG &DAG,
                                                   bool IsSRA) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // SRA expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (XLEN-1 - Shamt))
  //     Hi = Hi >>s Shamt
  //   else:
  //     Lo = Hi >>s (Shamt-XLEN);
  //     Hi = Hi >>s (XLEN-1)
  //
  // SRL expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (XLEN-1 - Shamt))
  //     Hi = Hi >>u Shamt
  //   else:
  //     Lo = Hi >>u (Shamt-XLEN);
  //     Hi = 0;

  unsigned ShiftRightOp = IsSRA ? ISD::SRA : ISD::SRL;

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::SUB, DL, VT, XLenMinus1, Shamt);

  SDValue ShiftRightLo = DAG.getNode(ISD::SRL, DL, VT, Lo, Shamt);
  SDValue ShiftLeftHi1 = DAG.getNode(ISD::SHL, DL, VT, Hi, One);
  SDValue ShiftLeftHi =
      DAG.getNode(ISD::SHL, DL, VT, ShiftLeftHi1, XLenMinus1Shamt);
  SDValue LoTrue = DAG.getNode(ISD::OR, DL, VT, ShiftRightLo, ShiftLeftHi);
  SDValue HiTrue = DAG.getNode(ShiftRightOp, DL, VT, Hi, Shamt);
  SDValue LoFalse = DAG.getNode(ShiftRightOp, DL, VT, Hi, ShamtMinusXLen);
  SDValue HiFalse =
      IsSRA ? DAG.getNode(ISD::SRA, DL, VT, Hi, XLenMinus1) : Zero;

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, LoFalse);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

// Returns the opcode of the target-specific SDNode that implements the 32-bit
// form of the given Opcode.
static LinxV5ISD::NodeType getLinxV5WOpcode(unsigned Opcode) {
  switch (Opcode) {
  case ISD::SHL:
    return LinxV5ISD::SLLW;
  case ISD::SRA:
    return LinxV5ISD::SRAW;
  case ISD::SRL:
    return LinxV5ISD::SRLW;
  case ISD::SDIV:
    return LinxV5ISD::DIVW;
  case ISD::UDIV:
    return LinxV5ISD::DIVUW;
  case ISD::UREM:
    return LinxV5ISD::REMUW;
  default:
    llvm_unreachable("Unexpected opcode");
  }
}

// Converts the given 32-bit operation to a target-specific SelectionDAG node.
// Because i32 isn't a legal type for RV64, these operations would otherwise
// be promoted to i64, making it difficult to select the SLLW/DIVUW/.../*W
// later one because the fact the operation was originally of type i32 is
// lost.
static SDValue customLegalizeToWOp(SDNode *N, SelectionDAG &DAG,
                                   unsigned ExtOpc = ISD::ANY_EXTEND) {
  SDLoc DL(N);
  LinxV5ISD::NodeType WOpcode = getLinxV5WOpcode(N->getOpcode());
  SDValue NewOp0 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(1));
  SDValue NewRes = DAG.getNode(WOpcode, DL, MVT::i64, NewOp0, NewOp1);
  // ReplaceNodeResults requires we maintain the same type for the return value.
  return DAG.getNode(ISD::TRUNCATE, DL, N->getValueType(0), NewRes);
}

// Converts the given 32-bit operation to a i64 operation with signed extension
// semantic to reduce the signed extension instructions.
static SDValue customLegalizeToWOpWithSExt(SDNode *N, SelectionDAG &DAG) {
  SDLoc DL(N);
  SDValue NewOp0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(1));
  SDValue NewWOp = DAG.getNode(N->getOpcode(), DL, MVT::i64, NewOp0, NewOp1);
  SDValue NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, NewWOp,
                               DAG.getValueType(MVT::i32));
  return DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes);
}

void LinxV5TargetLowering::ReplaceNodeResults(SDNode *N,
                                              SmallVectorImpl<SDValue> &Results,
                                              SelectionDAG &DAG) const {
  SDLoc DL(N);
  switch (N->getOpcode()) {
  case ISD::ADD:
  case ISD::SUB:
  case ISD::MUL:
    assert(N->getValueType(0) == MVT::i32 && "Unexpected custom legalisation");
    if (N->getOperand(1).getOpcode() == ISD::Constant)
      return;
    Results.push_back(customLegalizeToWOpWithSExt(N, DAG));
    break;
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
    assert(N->getValueType(0) == MVT::i32 && "Unexpected custom legalisation");
    if (N->getOperand(1).getOpcode() == ISD::Constant)
      return;
    Results.push_back(customLegalizeToWOp(N, DAG));
    break;
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::UREM: {
    MVT VT = N->getSimpleValueType(0);
    assert((VT == MVT::i8 || VT == MVT::i16 || VT == MVT::i32) &&
           Subtarget.isScalar() && "Unexpected custom legalisation");
    if (N->getOperand(0).getOpcode() == ISD::Constant ||
        N->getOperand(1).getOpcode() == ISD::Constant)
      return;

    // If the input is i32, use ANY_EXTEND since the W instructions don't read
    // the upper 32 bits. For other types we need to sign or zero extend
    // based on the opcode.
    unsigned ExtOpc = ISD::ANY_EXTEND;
    if (VT != MVT::i32)
      ExtOpc =
          N->getOpcode() == ISD::SDIV ? ISD::SIGN_EXTEND : ISD::ZERO_EXTEND;

    Results.push_back(customLegalizeToWOp(N, DAG, ExtOpc));
    break;
  }
  case ISD::BITCAST: {
    if (Subtarget.enableLegacyISel() || !Subtarget.hasFloat())
      break;
    EVT VT = N->getValueType(0);
    assert(VT.isInteger() && "Unexpected VT!");
    SDValue Op0 = N->getOperand(0);
    EVT Op0VT = Op0.getValueType();
    if (Op0VT.isVector())
      break;
    MVT XLenVT = Subtarget.getXLenVT();
    SDValue Res = DAG.getNode(LinxV5ISD::BITCAST, DL, MVT::i64, Op0);
    if (VT.getSimpleVT() != MVT::i64)
      Res = DAG.getNode(ISD::TRUNCATE, DL, VT, Res);
    Results.push_back(Res);
    break;
  }
  case ISD::STRICT_FP_TO_SINT:
  case ISD::STRICT_FP_TO_UINT:
  case ISD::FP_TO_SINT:
  case ISD::FP_TO_UINT: {
    bool IsStrict = N->isStrictFPOpcode();
    if (Subtarget.enableLegacyISel())
      assert(N->getValueType(0) == MVT::i32 &&
             "Unexpected custom legalisation");
    else
      assert(
          (N->getValueType(0) == MVT::i32 || N->getValueType(0) == MVT::i16) &&
          "Unexpected custom legalisation");
    SDValue Op0 = IsStrict ? N->getOperand(1) : N->getOperand(0);
    // If the FP type needs to be softened, emit a library call using the 'si'
    // version. If we left it to default legalization we'd end up with 'di'. If
    // the FP type doesn't need to be softened just let generic type
    // legalization promote the result type.
    if (getTypeAction(*DAG.getContext(), Op0.getValueType()) ==
        TargetLowering::TypeSoftenFloat) {
      RTLIB::Libcall LC;
      if (N->getOpcode() == ISD::FP_TO_SINT ||
          N->getOpcode() == ISD::STRICT_FP_TO_SINT)
        LC = RTLIB::getFPTOSINT(Op0.getValueType(), N->getValueType(0));
      else
        LC = RTLIB::getFPTOUINT(Op0.getValueType(), N->getValueType(0));
      if (LC == RTLIB::UNKNOWN_LIBCALL)
        break;
      MakeLibCallOptions CallOptions;
      EVT OpVT = Op0.getValueType();
      CallOptions.setTypeListBeforeSoften(OpVT, N->getValueType(0), true);
      SDValue Chain = IsStrict ? N->getOperand(0) : SDValue();
      SDValue Result;
      std::tie(Result, Chain) =
          makeLibCall(DAG, LC, N->getValueType(0), Op0, CallOptions, DL, Chain);
      Results.push_back(Result);
      if (IsStrict)
        Results.push_back(Chain);
      break;
    }
    if (Subtarget.enableLegacyISel() || !Subtarget.hasFloat())
      break;
    EVT VT = N->getValueType(0);
    assert(VT.isInteger() && "Unexpected custom legalisation");
    bool IsSigned = N->getOpcode() == ISD::FP_TO_SINT ||
                    N->getOpcode() == ISD::STRICT_FP_TO_SINT;
    if (!isTypeLegal(Op0.getValueType()))
      break; // libcall
    unsigned Opc = IsSigned ? LinxV5ISD::FCVTS : LinxV5ISD::FCVTU;
    SDValue Res = DAG.getNode(Opc, DL, MVT::i64, Op0);
    if (VT.getSimpleVT() != MVT::i64)
      Res = DAG.getNode(ISD::TRUNCATE, DL, VT, Res);
    Results.push_back(Res);
    break;
  }
  case ISD::INTRINSIC_W_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(1));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(N);
    switch (IntID) {
    default:
      report_fatal_error("unimplemented Intrinsic operand");
    case Intrinsic::linx_get_simt_ret: {
      unsigned RetNo = cast<ConstantSDNode>(N->getOperand(2))->getZExtValue();
      SDValue Val = DAG.getCopyFromReg(N->getOperand(0), DL,
                                       LinxV5::getReduceDst(RetNo), MVT::i64);
      SDValue Chain = Val.getValue(1);
      EVT VT = N->getValueType(0);
      if (VT.bitsLT(MVT::i64)) {
        Val = DAG.getAnyExtOrTrunc(Val, DL,
                                   MVT::getIntegerVT(VT.getSizeInBits()));
      }
      if (!VT.isInteger()) {
        Val = DAG.getBitcast(VT, Val);
      }
      Results.push_back(Val);
      Results.push_back(Chain);
      return;
    }
    case Intrinsic::blkv_if: {
      SDValue Chain = N->getOperand(0);
      SDValue PredFlag = N->getOperand(2);
      SDValue RegCopy = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other), Chain);
      SDValue PredFlagExt = DAG.getZExtOrTrunc(PredFlag, DL, MVT::i64);
      SDValue MergePred = DAG.getNode(LinxV5ISD::MERGE_PREDICATION, DL,
                                      MVT::Other, RegCopy.getValue(1), PredFlagExt);
      SDValue RegCopyMerge =
          DAG.getNode(LinxV5ISD::CopyP, DL,
                      DAG.getVTList(MVT::i64, MVT::Other),
                      MergePred);
      SDValue CMP = DAG.getNode(ISD::SETCC, DL, MVT::i64, RegCopyMerge,
                                DAG.getConstant(0, DL, MVT::i64),
                                DAG.getCondCode(ISD::SETNE));
      SDValue CMP_TRUNC = DAG.getSExtOrTrunc(CMP, DL, MVT::i1);
      Results.push_back(CMP_TRUNC);
      Results.push_back(RegCopy);
      Results.push_back(RegCopyMerge.getValue(1));
      return;
    }
    case Intrinsic::blkv_flow: {
      SDValue Chain = N->getOperand(0);
      SDValue OldMask = N->getOperand(2);
      SDValue CurMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other), Chain);
      SDValue NewMask = DAG.getNode(ISD::XOR, DL, MVT::i64, CurMask, OldMask);
      SDValue SetMask =
          DAG.getNode(LinxV5ISD::Copy2PTerm, DL,
                      MVT::Other,
                      CurMask.getValue(1),
                      NewMask);
      SDValue J2Else = DAG.getNode(ISD::SETCC, DL, MVT::i64, NewMask,
                                   DAG.getConstant(0, DL, MVT::i64),
                                   DAG.getCondCode(ISD::SETNE));
      SDValue J2Else_TRUNC = DAG.getZExtOrTrunc(J2Else, DL, MVT::i1);
      Results.push_back(J2Else_TRUNC);
      Results.push_back(OldMask);
      Results.push_back(SetMask);
      return;
    }
    case Intrinsic::blkv_loop: {
      SDValue Chain = N->getOperand(0);
      SDValue LegalCond = DAG.getZExtOrTrunc(N->getOperand(2), DL, MVT::i64);
      SDValue NotBroken = DAG.getNode(ISD::XOR, DL, MVT::i64, LegalCond,
                                      DAG.getConstant(-1, DL, MVT::i64));
      SDValue OldMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other), Chain);
      SDValue NewMask = DAG.getNode(ISD::AND, DL, MVT::i64, NotBroken, OldMask);
      SDValue WriteMask =
          DAG.getNode(LinxV5ISD::Copy2PTerm, DL,
                      MVT::Other,
                      OldMask.getValue(1),
                      NewMask);
      SDValue AllBroken = DAG.getNode(ISD::SETCC, DL, MVT::i64, NewMask,
                                      DAG.getConstant(0, DL, MVT::i64),
                                      DAG.getCondCode(ISD::SETEQ));
      SDValue AllBrokenTrunc = DAG.getZExtOrTrunc(AllBroken, DL, MVT::i1);
      Results.push_back(AllBrokenTrunc);
      Results.push_back(WriteMask);
      return;
    }
    case Intrinsic::blkv_merge_cf: {
      SDValue Chain = N->getOperand(0);
      SDValue LHS = N->getOperand(2);
      SDValue RHS = N->getOperand(3);
      if (LHS.getValueType() == MVT::i1) {
        // For i1, lowerMergeCF promotes operands to i8, creates MERGE_CF{i8,
        // Other}, then truncates back to i1. The truncation node only produces
        // {i1} — it has no chain output. The original code pushed
        // merge.getValue(1) on the truncation node, which asserts because
        // truncation only has result #0. Fix: extract the chain from the
        // MERGE_CF{i8} node directly, not from the truncation node.
        SDValue LHSPromote = DAG.getSExtOrTrunc(LHS, DL, MVT::i8);
        SDValue RHSPromote = DAG.getSExtOrTrunc(RHS, DL, MVT::i8);
        SDValue Merge = DAG.getNode(LinxV5ISD::MERGE_CF, DL,
                             DAG.getVTList(MVT::i8, MVT::Other),
                             Chain, LHSPromote, RHSPromote);
        SDValue Result = DAG.getSExtOrTrunc(Merge.getValue(0), DL, MVT::i1);
        Results.push_back(Result);              // i1 truncated result
        Results.push_back(Merge.getValue(1));   // chain from MERGE_CF node
        return;
      }
      SDValue merge = lowerMergeCF(DL, SDValue(N, 0), DAG);
      Results.push_back(merge.getValue(0));
      Results.push_back(merge.getValue(1));
      return;
    }
    case Intrinsic::blkv_if_break: {
      SDValue Chain = N->getOperand(0);
      SDValue OldMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    Chain);
      SDValue ExitCond = DAG.getZExtOrTrunc(N->getOperand(2), DL, MVT::i64);
      SDValue Broken = DAG.getZExtOrTrunc(N->getOperand(3), DL, MVT::i64);
      SDValue MergedExit =
          DAG.getNode(LinxV5ISD::MERGE_PREDICATION, DL, MVT::Other,
                      OldMask.getValue(1), ExitCond);
      SDValue NewMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    MergedExit);
      SDValue NextExit = DAG.getNode(ISD::OR, DL, MVT::i64, NewMask, Broken);
      SDValue RestoreMask =
          DAG.getNode(LinxV5ISD::Copy2P, DL, MVT::Other,
                      NewMask.getValue(1), OldMask);
      Results.push_back(NextExit);
      Results.push_back(RestoreMask);
      return;
    }
    }
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    // Intrinsic without side effect.
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(N);
    switch (IntID) {
    default:
      report_fatal_error("unimplemented Intrinsic operand");
    }
  }
  case ISD::SETCC: {
    if (!N->isDivergent()) {
      SDValue SetCC = DAG.getNode(N->getOpcode(), DL, MVT::i64, N->getOperand(0),
                          N->getOperand(1), N->getOperand(2));
      SDValue SetCC_Trunc = DAG.getZExtOrTrunc(SetCC, DL, MVT::i1);
      Results.push_back(SetCC_Trunc);
    }
    return;
  }
  default:
    llvm_unreachable("Don't know how to custom type legalize this operation!");
  }
}

static bool isTrunkFromSext(SDNode *N, EVT SrcVt, unsigned Depth) {
  // Limit search depth
  if (Depth > MaxDepthForSearchSext)
    return false;

  unsigned Opc = N->getOpcode();
  if (Opc == ISD::AND || Opc == ISD::OR || Opc == ISD::XOR) {
    if (isTrunkFromSext(N->getOperand(0).getNode(), SrcVt, Depth + 1)) {
      return true;
    }

    if (isTrunkFromSext(N->getOperand(1).getNode(), SrcVt, Depth + 1)) {
      return true;
    }
  } else if (Opc == ISD::TRUNCATE && N->getValueType(0) == SrcVt) {
    return isTrunkFromSext(N->getOperand(0).getNode(), SrcVt, Depth + 1);
  } else if (Opc == ISD::AssertSext && N->getValueType(0) == MVT::i64) {
    VTSDNode *TypeNode = cast<VTSDNode>(N->getOperand(1));
    if (TypeNode->getVT() == SrcVt) {
      return true;
    }
  }

  return false;
}

static bool isProfitToTransZextToSext(SDNode *N, SelectionDAG &DAG) {
  // If the zext operand's sign bit is known to be zero, don't convert to sext.
  if (DAG.SignBitIsZero(N->getOperand(0))) {
    return false;
  }

  if (!N->hasOneUse() || N->getValueType(0) != MVT::i64) {
    return false;
  }

  EVT SrcVt = N->getOperand(0).getValueType();
  if (SrcVt != MVT::i8 && SrcVt != MVT::i16 && SrcVt != MVT::i32) {
    return false;
  }

  SDNode::use_iterator UI = N->use_begin();
  SDNode *User = *UI;
  if ((User->getOpcode() == ISD::SHL || User->getOpcode() == ISD::SRA ||
       User->getOpcode() == ISD::SRL) &&
      User->getOperand(1).getNode() == N) {
    if (isTrunkFromSext(N->getOperand(0).getNode(), SrcVt, 0)) {
      return true;
    }
  }

  return false;
}

static SDValue combineZeroExtend(SDNode *N, SelectionDAG &DAG,
                                 const LinxV5Subtarget &Subtarget,
                                 TargetLowering::DAGCombinerInfo &DCI) {
  if (isProfitToTransZextToSext(N, DAG)) {
    SDLoc DL(N);
    SDValue Op0 = N->getOperand(0);
    SDValue NewNode = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, Op0);
    return DCI.CombineTo(N, NewNode);
  }

  return SDValue();
}

// If all users of the globaladdr are of the form (globaladdr + constant), find
// the smallest constant, fold it into the globaladdr's offset and rewrite the
// globaladdr as (globaladdr + constant) - constant.
static SDValue performGlobalAddressCombine(SDNode *N, SelectionDAG &DAG,
                                           const LinxV5Subtarget &Subtarget,
                                           const TargetMachine &TM) {
  auto *GN = cast<GlobalAddressSDNode>(N);

  uint64_t MinOffset = -1ull;
  for (SDNode *N : GN->uses()) {
    if (N->getOpcode() != ISD::ADD)
      return SDValue();
    auto *C = dyn_cast<ConstantSDNode>(N->getOperand(0));
    if (!C)
      C = dyn_cast<ConstantSDNode>(N->getOperand(1));
    if (!C)
      return SDValue();
    MinOffset = std::min(MinOffset, C->getZExtValue());
  }
  uint64_t Offset = MinOffset + GN->getOffset();

  // Require that the new offset is larger than the existing one. Otherwise, we
  // can end up oscillating between two possible DAGs, for example,
  // (add (add globaladdr + 10, -1), 1) and (add globaladdr + 9, 1).
  if (Offset <= uint64_t(GN->getOffset()))
    return SDValue();

  // Check whether folding this offset is legal. It must not go out of bounds of
  // the referenced object to avoid violating the code model, and must be
  // smaller than 2^31 because this is the largest offset expressible in all
  // object formats.
  //
  // This check also prevents us from folding negative offsets, which will end
  // up being treated in the same way as large positive ones. They could also
  // cause code model violations, and aren't really common enough to matter.
  if (Offset >= (1 << 31))
    return SDValue();

  const GlobalValue *GV = GN->getGlobal();
  Type *T = GV->getValueType();
  if (!T->isSized() ||
      Offset > GV->getParent()->getDataLayout().getTypeAllocSize(T))
    return SDValue();

  SDLoc DL(GN);
  SDValue Result = DAG.getGlobalAddress(GV, DL, MVT::i64, Offset);
  return DAG.getNode(ISD::SUB, DL, MVT::i64, Result,
                     DAG.getConstant(MinOffset, DL, MVT::i64));
}

SDValue LinxV5TargetLowering::PerformDAGCombine(SDNode *N,
                                                DAGCombinerInfo &DCI) const {
  SelectionDAG &DAG = DCI.DAG;

  switch (N->getOpcode()) {
  default:
    break;
  case LinxV5ISD::SLLW:
  case LinxV5ISD::SRAW:
  case LinxV5ISD::SRLW: {
    // Only the lower 32 bits of LHS and lower 5 bits of RHS are read.
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    APInt LHSMask = APInt::getLowBitsSet(LHS.getValueSizeInBits(), 32);
    APInt RHSMask = APInt::getLowBitsSet(RHS.getValueSizeInBits(), 5);
    if (SimplifyDemandedBits(N->getOperand(0), LHSMask, DCI) ||
        SimplifyDemandedBits(N->getOperand(1), RHSMask, DCI)) {
      if (N->getOpcode() != ISD::DELETED_NODE)
        DCI.AddToWorklist(N);
      return SDValue(N, 0);
    }
    break;
  }
  case LinxV5ISD::SELECT_CC: {
    // Transform
    // (select_cc (xor X, 1), 0, setne, trueV, falseV) ->
    // (select_cc X, 0, seteq, trueV, falseV) if we can prove X is 0/1.
    // This can occur when legalizing some floating point comparisons.
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    auto CCVal = static_cast<ISD::CondCode>(N->getConstantOperandVal(2));
    APInt Mask = APInt::getBitsSetFrom(LHS.getValueSizeInBits(), 1);
    if (ISD::isIntEqualitySetCC(CCVal) && isNullConstant(RHS) &&
        LHS.getOpcode() == ISD::XOR && isOneConstant(LHS.getOperand(1)) &&
        DAG.MaskedValueIsZero(LHS.getOperand(0), Mask)) {
      SDLoc DL(N);
      CCVal = ISD::getSetCCInverse(CCVal, LHS.getValueType());
      SDValue TargetCC = DAG.getConstant(CCVal, DL, Subtarget.getXLenVT());
      return DAG.getNode(LinxV5ISD::SELECT_CC, DL, N->getValueType(0),
                         {LHS.getOperand(0), RHS, TargetCC, N->getOperand(3),
                          N->getOperand(4)});
    }
    break;
  }
  case ISD::SETCC: {
    // (setcc X, 1, setne) -> (setcc X, 0, seteq) if we can prove X is 0/1.
    // Comparing with 0 may allow us to fold into bnez/beqz.
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    if (LHS.getValueType().isScalableVector())
      break;
    auto CC = cast<CondCodeSDNode>(N->getOperand(2))->get();
    APInt Mask = APInt::getBitsSetFrom(LHS.getValueSizeInBits(), 1);
    if (isOneConstant(RHS) && ISD::isIntEqualitySetCC(CC) &&
        DAG.MaskedValueIsZero(LHS, Mask)) {
      SDLoc DL(N);
      SDValue Zero = DAG.getConstant(0, DL, LHS.getValueType());
      CC = ISD::getSetCCInverse(CC, LHS.getValueType());
      return DAG.getSetCC(DL, N->getValueType(0), LHS, Zero, CC);
    }
    break;
  }
  case ISD::ZERO_EXTEND:
    return combineZeroExtend(N, DAG, Subtarget, DCI);
  case ISD::GlobalAddress:
    return performGlobalAddressCombine(N, DAG, Subtarget, getTargetMachine());
  }

  return SDValue();
}

bool LinxV5TargetLowering::targetShrinkDemandedConstant(
    SDValue Op, const APInt &DemandedBits, const APInt &DemandedElts,
    TargetLoweringOpt &TLO) const {
  // Delay this optimization as late as possible.
  if (!TLO.LegalOps)
    return false;

  EVT VT = Op.getValueType();
  if (VT.isVector())
    return false;

  // Only handle AND for now.
  if (Op.getOpcode() != ISD::AND)
    return false;

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op.getOperand(1));
  if (!C)
    return false;

  const APInt &Mask = C->getAPIntValue();

  // Clear all non-demanded bits initially.
  APInt ShrunkMask = Mask & DemandedBits;

  // If the shrunk mask fits in sign extended 12 bits, let the target
  // independent code apply it.
  if (ShrunkMask.isSignedIntN(12))
    return false;

  // Try to make a smaller immediate by setting undemanded bits.

  // We need to be able to make a negative number through a combination of mask
  // and undemanded bits.
  APInt ExpandedMask = Mask | ~DemandedBits;
  if (!ExpandedMask.isNegative())
    return false;

  // What is the fewest number of bits we need to represent the negative number.
  unsigned MinSignedBits = ExpandedMask.getMinSignedBits();

  // Try to make a 12 bit negative immediate. If that fails try to make a 32
  // bit negative immediate unless the shrunk immediate already fits in 32 bits.
  APInt NewMask = ShrunkMask;
  if (MinSignedBits <= 12)
    NewMask.setBitsFrom(11);
  else if (MinSignedBits <= 32 && !ShrunkMask.isSignedIntN(32))
    NewMask.setBitsFrom(31);
  else
    return false;

  // Sanity check that our new mask is a subset of the demanded mask.
  assert(NewMask.isSubsetOf(ExpandedMask));

  // If we aren't changing the mask, just return true to keep it and prevent
  // the caller from optimizing.
  if (NewMask == Mask)
    return true;

  // Replace the constant with the new mask.
  SDLoc DL(Op);
  SDValue NewC = TLO.DAG.getConstant(NewMask, DL, VT);
  SDValue NewOp = TLO.DAG.getNode(ISD::AND, DL, VT, Op.getOperand(0), NewC);
  return TLO.CombineTo(Op, NewOp);
}

bool LinxV5TargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  if (!Subtarget.enableLegacyISel())
    // Offsets are folded in the DAG combine rather than here so that we can
    // intelligently choose an offset based on the uses.
    return false;
}

static bool isSelectPseudo(MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    return false;
  case LinxV5::Select_GPR_Using_CC_GPR:
    return true;
  }
}

static MachineBasicBlock *emitSelectPseudo(MachineInstr &MI,
                                           MachineBasicBlock *BB) {
  // To "insert" Select_* instructions, we actually have to insert the triangle
  // control-flow pattern.  The incoming instructions know the destination vreg
  // to set, the condition code register to branch on, the true/false values to
  // select between, and the condcode to use to select the appropriate branch.
  //
  // We produce the following control flow:
  //     HeadMBB
  //     |  \
  //     |  IfFalseMBB
  //     | /
  //    TailMBB
  //
  // When we find a sequence of selects we attempt to optimize their emission
  // by sharing the control flow. Currently we only handle cases where we have
  // multiple selects with the exact same condition (same LHS, RHS and CC).
  // The selects may be interleaved with other instructions if the other
  // instructions meet some requirements we deem safe:
  // - They are debug instructions. Otherwise,
  // - They do not have side-effects, do not access memory and their inputs do
  //   not depend on the results of the select pseudo-instructions.
  // The TrueV/FalseV operands of the selects cannot depend on the result of
  // previous selects in the sequence.
  // These conditions could be further relaxed. See the X86 target for a
  // related approach and more information.
  Register LHS = MI.getOperand(1).getReg();
  Register RHS = MI.getOperand(2).getReg();
  auto CC = static_cast<ISD::CondCode>(MI.getOperand(3).getImm());

  SmallVector<MachineInstr *, 4> SelectDebugValues;
  SmallSet<Register, 4> SelectDests;
  SelectDests.insert(MI.getOperand(0).getReg());

  MachineInstr *LastSelectPseudo = &MI;

  for (auto E = BB->end(), SequenceMBBI = MachineBasicBlock::iterator(MI);
       SequenceMBBI != E; ++SequenceMBBI) {
    if (SequenceMBBI->isDebugInstr())
      continue;
    else if (isSelectPseudo(*SequenceMBBI)) {
      if (SequenceMBBI->getOperand(1).getReg() != LHS ||
          SequenceMBBI->getOperand(2).getReg() != RHS ||
          SequenceMBBI->getOperand(3).getImm() != CC ||
          SelectDests.count(SequenceMBBI->getOperand(4).getReg()) ||
          SelectDests.count(SequenceMBBI->getOperand(5).getReg()))
        break;
      LastSelectPseudo = &*SequenceMBBI;
      SequenceMBBI->collectDebugValues(SelectDebugValues);
      SelectDests.insert(SequenceMBBI->getOperand(0).getReg());
    } else {
      if (SequenceMBBI->hasUnmodeledSideEffects() ||
          SequenceMBBI->mayLoadOrStore())
        break;
      if (llvm::any_of(SequenceMBBI->operands(), [&](MachineOperand &MO) {
            return MO.isReg() && MO.isUse() && SelectDests.count(MO.getReg());
          }))
        break;
    }
  }

  const LinxV5Subtarget &STI = BB->getParent()->getSubtarget<LinxV5Subtarget>();
  const TargetInstrInfo &TII = *BB->getParent()->getSubtarget().getInstrInfo();
  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction::iterator I = ++BB->getIterator();

  MachineBasicBlock *HeadMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *TailMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *IfFalseMBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(I, IfFalseMBB);
  F->insert(I, TailMBB);

  // Transfer debug instructions associated with the selects to TailMBB.
  for (MachineInstr *DebugInstr : SelectDebugValues) {
    TailMBB->push_back(DebugInstr->removeFromParent());
  }

  // Move all instructions after the sequence to TailMBB.
  TailMBB->splice(TailMBB->end(), HeadMBB,
                  std::next(LastSelectPseudo->getIterator()), HeadMBB->end());
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi nodes for the selects.
  TailMBB->transferSuccessorsAndUpdatePHIs(HeadMBB);
  // Set the successors for HeadMBB.
  HeadMBB->addSuccessor(IfFalseMBB);
  HeadMBB->addSuccessor(TailMBB);

  // Insert appropriate branch.
  unsigned Opcode = STI.enableLegacyISel()
                        ? getVBXBranchOpcodeForIntCondCode(CC)
                        : getBranchOpcodeForIntCondCode(CC);

  BuildMI(HeadMBB, DL, TII.get(Opcode)).addReg(LHS).addReg(RHS).addMBB(TailMBB);

  // IfFalseMBB just falls through to TailMBB.
  IfFalseMBB->addSuccessor(TailMBB);

  // Create PHIs for all of the select pseudo-instructions.
  auto SelectMBBI = MI.getIterator();
  auto SelectEnd = std::next(LastSelectPseudo->getIterator());
  auto InsertionPoint = TailMBB->begin();
  while (SelectMBBI != SelectEnd) {
    auto Next = std::next(SelectMBBI);
    if (isSelectPseudo(*SelectMBBI)) {
      // %Result = phi [ %TrueValue, HeadMBB ], [ %FalseValue, IfFalseMBB ]
      BuildMI(*TailMBB, InsertionPoint, SelectMBBI->getDebugLoc(),
              TII.get(LinxV5::PHI), SelectMBBI->getOperand(0).getReg())
          .addReg(SelectMBBI->getOperand(4).getReg())
          .addMBB(HeadMBB)
          .addReg(SelectMBBI->getOperand(5).getReg())
          .addMBB(IfFalseMBB);
      SelectMBBI->eraseFromParent();
    }
    SelectMBBI = Next;
  }

  F->getProperties().reset(MachineFunctionProperties::Property::NoPHIs);
  return TailMBB;
}

static MachineBasicBlock *emitLoweredLoopSet(MachineInstr &MI,
                                             MachineBasicBlock *BB) {
  MachineFunction *MF = BB->getParent();
  const auto *TII = MF->getSubtarget().getInstrInfo();
  BuildMI(*BB, &MI, MI.getDebugLoc(), TII->get(LinxV5::LOOP_SET),
          MI.getOperand(0).getReg())
      .add(MI.getOperand(1))
      .add(MI.getOperand(2));
  MI.eraseFromParent();
  return BB;
}

static MachineBasicBlock *emitLowerMergePred(MachineInstr &MI,
                                             MachineBasicBlock *BB) {
  MachineFunction *MF = BB->getParent();
  const auto *TII = MF->getSubtarget().getInstrInfo();
  BuildMI(*BB, &MI, MI.getDebugLoc(), TII->get(LinxV5::SIMT_CMP_NEI_P))
      .add(MI.getOperand(0))
      .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD)
      .addImm(0);
  MI.eraseFromParent();
  return BB;
}

static bool isMergeCFPseudo(const MachineInstr &MI) {
  return MI.getOpcode() == LinxV5::LinxV5PseudoMergeCF;
}

static bool isCopy2P(const MachineInstr &MI) {
  return MI.getOpcode() == LinxV5::LinxV5PseudoCopy2P ||
         MI.getOpcode() == LinxV5::LinxV5PseudoCopy2PTerm;
}

static void expandMergeCFPseudo(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator InsertPt,
                                MachineInstr &MI, Register SavedP,
                                const TargetInstrInfo &TII) {
  BuildMI(MBB, InsertPt, MI.getDebugLoc(), TII.get(LinxV5::SIMT_PSEL),
          MI.getOperand(0).getReg())
      .add(MI.getOperand(1))
      .addReg(SavedP)
      .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD)
      .add(MI.getOperand(2))
      .add(MI.getOperand(3))
      .add(MI.getOperand(4))
      .add(MI.getOperand(5))
      .add(MI.getOperand(6));
}

static MachineBasicBlock *emitLowerMergeCF(MachineInstr &MI,
                                           MachineBasicBlock *BB) {
  MachineFunction *MF = BB->getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  const auto *TII = MF->getSubtarget().getInstrInfo();

  SmallVector<MachineInstr *, 4> Merges;
  auto RestoreIt = BB->end();
  auto It = MachineBasicBlock::iterator(MI);
  auto End = BB->end();
  for (; It != End; ++It) {
    if (It->isDebugInstr())
      continue;
    if (isMergeCFPseudo(*It)) {
      Merges.push_back(&*It);
      continue;
    }
    if (isCopy2P(*It)) {
      RestoreIt = It;
      break;
    }
    if (It->modifiesRegister(LinxV5::SIMT_P,
                             MF->getSubtarget<LinxV5Subtarget>()
                                 .getRegisterInfo()))
      break;
  }

  Register SavedP = MRI.createVirtualRegister(&LinxV5::SIMTCGSRegClass);
  BuildMI(*BB, MI.getIterator(), MI.getDebugLoc(),
          TII->get(LinxV5::LinxV5PseudoCopyFromP), SavedP);

  if (RestoreIt != BB->end())
    BB->splice(MI.getIterator(), BB, RestoreIt);

  for (MachineInstr *Merge : Merges) {
    expandMergeCFPseudo(*BB, Merge->getIterator(), *Merge, SavedP, *TII);
    Merge->eraseFromParent();
  }

  if (Merges.empty()) {
    expandMergeCFPseudo(*BB, MI.getIterator(), MI, SavedP, *TII);
    MI.eraseFromParent();
  }

  return BB;
}


static MachineBasicBlock *
emitLoweredReduce(MachineInstr &MI, MachineBasicBlock *BB, unsigned Opc) {
  MachineFunction *MF = BB->getParent();
  const auto *TII = MF->getSubtarget().getInstrInfo();
  BuildMI(*BB, &MI, MI.getDebugLoc(), TII->get(Opc), MI.getOperand(0).getReg())
      .add(MI.getOperand(1))
      .add(MI.getOperand(2))
      .add(MI.getOperand(3));
  MI.eraseFromParent();
  return BB;
}

MachineBasicBlock *
LinxV5TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                  MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  case LinxV5::Select_GPR_Using_CC_GPR:
    return emitSelectPseudo(MI, BB);
  case LinxV5::PseudoLoopSet:
    return emitLoweredLoopSet(MI, BB);
  case LinxV5::LinxV5PseudoMergePred:
    return emitLowerMergePred(MI, BB);
  case LinxV5::LinxV5PseudoMergeCF:
    return emitLowerMergeCF(MI, BB);
  default:; // fall-thru
  }

  if (LinxV5II::getFormat(MI.getDesc().TSFlags) ==
      LinxV5II::InstFormat_REDUCE) {
    return emitLoweredReduce(MI, BB, LinxV5::getPseudoMap(MI.getOpcode()));
  }
  llvm_unreachable("unhandled custom inserter!");
}

// Calling Convention Implementation.
// The expectations for frontend ABI lowering vary from target to target.
// Ideally, an LLVM frontend would be able to avoid worrying about many ABI
// details, but this is a longer term goal. For now, we simply try to keep the
// role of the frontend as simple and well-defined as possible. The rules can
// be summarised as:
// * Never split up large scalar arguments. We handle them here.
// * If a hardfloat calling convention is being used, and the struct may be
// passed in a pair of registers (fp+fp, int+fp), and both registers are
// available, then pass as two separate arguments. If either the GPRs or FPRs
// are exhausted, then pass according to the rule below.
// * If a struct could never be passed in registers or directly in a stack
// slot (as it is larger than 2*XLEN and the floating point rules don't
// apply), then pass it using a pointer with the byval attribute.
// * If a struct is less than 2*XLEN, then coerce to either a two-element
// word-sized array or a 2*XLEN scalar (depending on alignment).
// * The frontend can determine whether a struct is returned by reference or
// not based on its size and fields. If it will be returned by reference, the
// frontend must modify the prototype so a pointer with the sret annotation is
// passed as the first argument. This is not necessary for large scalar
// returns.
// * Struct return values and varargs should be coerced to structs containing
// register-size fields in the same situations they would be for fixed
// arguments.

static const MCPhysReg ArgGPRs[] = {LinxV5::R2, LinxV5::R3, LinxV5::R4,
                                    LinxV5::R5, LinxV5::R6, LinxV5::R7,
                                    LinxV5::R8, LinxV5::R9};

static const MCPhysReg SIMTArgGPRs[] = {
    // a0 ~ a7
    LinxV5::R2, LinxV5::R3, LinxV5::R4, LinxV5::R5, LinxV5::R6, LinxV5::R7,
    LinxV5::R8, LinxV5::R9,
    // x0 ~ x3
    LinxV5::R20, LinxV5::R21, LinxV5::R22, LinxV5::R23,
    // s1 ~ s8
    LinxV5::R12, LinxV5::R13, LinxV5::R14, LinxV5::R15, LinxV5::R16,
    LinxV5::R17, LinxV5::R18, LinxV5::R19};

static const MCPhysReg SIMTTileInList[] = {
    LinxV5::SIMT_TA, LinxV5::SIMT_TB, LinxV5::SIMT_TC, LinxV5::SIMT_TD,
    LinxV5::SIMT_TE, LinxV5::SIMT_TF, LinxV5::SIMT_TG, LinxV5::SIMT_TH};

static const MCPhysReg SIMTTileOutNoSpillList[] = {
    LinxV5::SIMT_TO, LinxV5::SIMT_TO1, LinxV5::SIMT_TO2, LinxV5::SIMT_TO3};

static const MCPhysReg SIMTTileOutList[] = {LinxV5::SIMT_TO, LinxV5::SIMT_TO2,
                                            LinxV5::SIMT_TO3};

static const MCPhysReg SIMTGPRInList[] = {
    LinxV5::SIMT_RI0, LinxV5::SIMT_RI1, LinxV5::SIMT_RI2,  LinxV5::SIMT_RI3,
    LinxV5::SIMT_RI4, LinxV5::SIMT_RI5, LinxV5::SIMT_RI6,  LinxV5::SIMT_RI7,
    LinxV5::SIMT_RI8, LinxV5::SIMT_RI9, LinxV5::SIMT_RI10, LinxV5::SIMT_RI11};

static const MCPhysReg SIMTGPROutList[] = {LinxV5::SIMT_RO0, LinxV5::SIMT_RO1,
                                           LinxV5::SIMT_RO2, LinxV5::SIMT_RO3};

// TODO: delete in the furture
static const MCPhysReg SIMTGPRInListCaller[] = {
    LinxV5::R2, LinxV5::R3, LinxV5::R4, LinxV5::R5,
    LinxV5::R6, LinxV5::R7, LinxV5::R8, LinxV5::R9};

static const MCPhysReg SIMTGPROutListCaller[] = {LinxV5::R2};

// Pass a 2*XLEN argument that has been split into two XLEN values through
// registers or the stack as necessary.
static bool CC_LinxV5Assign2XLen(unsigned XLen, CCState &State, CCValAssign VA1,
                                 ISD::ArgFlagsTy ArgFlags1, unsigned ValNo2,
                                 MVT ValVT2, MVT LocVT2,
                                 ISD::ArgFlagsTy ArgFlags2) {
  unsigned XLenInBytes = XLen / 8;
  if (Register Reg = State.AllocateReg(ArgGPRs)) {
    // At least one half can be passed via register.
    State.addLoc(CCValAssign::getReg(VA1.getValNo(), VA1.getValVT(), Reg,
                                     VA1.getLocVT(), CCValAssign::Full));
  } else {
    // Both halves must be passed on the stack, with proper alignment.
    Align StackAlign =
        std::max(Align(XLenInBytes), ArgFlags1.getNonZeroOrigAlign());
    State.addLoc(
        CCValAssign::getMem(VA1.getValNo(), VA1.getValVT(),
                            State.AllocateStack(XLenInBytes, StackAlign),
                            VA1.getLocVT(), CCValAssign::Full));
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
    return false;
  }

  if (Register Reg = State.AllocateReg(ArgGPRs)) {
    // The second half can also be passed via register.
    State.addLoc(
        CCValAssign::getReg(ValNo2, ValVT2, Reg, LocVT2, CCValAssign::Full));
  } else {
    // The second half is passed via the stack, without additional alignment.
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
  }

  return false;
}

// Implements the LinxV5 SIMT calling convention. Returns true upon failure.
static bool
CC_LinxV5_SIMT(const DataLayout &DL, LinxV5ABI::ABI ABI, unsigned ValNo,
               MVT ValVT, MVT LocVT, CCValAssign::LocInfo LocInfo,
               ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsFixed,
               bool IsRet, Type *OrigTy, const LinxV5TargetLowering &TLI,
               Optional<unsigned> FirstMaskArgument, llvm::Function &F) {
  MVT XLenVT = MVT::i64;
  assert(!IsRet && "TODO: Support SIMT Ret GPR");

  Argument *Arg = F.getArg(ValNo);
  Register Reg = LinxV5::NoRegister;
  if (Arg->getType()->isVectorTy()) {
    if (Arg->hasAttribute(llvm::Attribute::LinxBLKFuncOut)) {
      if (F.hasFnAttribute("blkv-no-spill"))
        Reg = State.AllocateReg(SIMTTileOutNoSpillList);
      else
        Reg = State.AllocateReg(SIMTTileOutList);
    } else
      Reg = State.AllocateReg(SIMTTileInList);
  } else {
    if (TLI.getSubtarget().isSIMT())
      Reg = State.AllocateReg(SIMTGPRInList);
    else
      Reg = State.AllocateReg(SIMTGPRInListCaller);
  }

  assert(Reg != 0 && "LinxV5 CallingConv Fail!");
  State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
  return false;
}

// Implements the LinxV5 calling convention. Returns true upon failure.
static bool CC_LinxV5(const DataLayout &DL, LinxV5ABI::ABI ABI, unsigned ValNo,
                      MVT ValVT, MVT LocVT, CCValAssign::LocInfo LocInfo,
                      ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsFixed,
                      bool IsRet, Type *OrigTy, const LinxV5TargetLowering &TLI,
                      Optional<unsigned> FirstMaskArgument) {
  unsigned XLen = DL.getLargestLegalIntTypeSizeInBits();
  assert(XLen == 32 || XLen == 64);
  MVT XLenVT = XLen == 32 ? MVT::i32 : MVT::i64;

  // Any return value split in to more than two values can't be returned
  // directly.
  if (IsRet && ValNo > 1)
    return true;

  switch (ABI) {
  case LinxV5ABI::ABI_LP64:
    break;
  default:
    llvm_unreachable("Unexpected ABI");
  }

  // If this is a variadic argument, the LinxV5 calling convention requires
  // that it is assigned an 'even' or 'aligned' register if it has 8-byte
  // alignment (RV32) or 16-byte alignment (RV64). An aligned register should
  // be used regardless of whether the original argument was split during
  // legalisation or not. The argument will not be passed by registers if the
  // original type is larger than 2*XLEN, so the register alignment rule does
  // not apply.
  unsigned TwoXLenInBytes = (2 * XLen) / 8;
  if (!IsFixed && ArgFlags.getNonZeroOrigAlign() == TwoXLenInBytes &&
      DL.getTypeAllocSize(OrigTy) == TwoXLenInBytes) {
    unsigned RegIdx = State.getFirstUnallocated(ArgGPRs);
    // Skip 'odd' register if necessary.
    if (RegIdx != array_lengthof(ArgGPRs) && RegIdx % 2 == 1)
      State.AllocateReg(ArgGPRs);
  }

  SmallVectorImpl<CCValAssign> &PendingLocs = State.getPendingLocs();
  SmallVectorImpl<ISD::ArgFlagsTy> &PendingArgFlags =
      State.getPendingArgFlags();

  assert(PendingLocs.size() == PendingArgFlags.size() &&
         "PendingLocs and PendingArgFlags out of sync");

  // Split arguments might be passed indirectly, so keep track of the pending
  // values.
  if (ArgFlags.isSplit() || !PendingLocs.empty()) {
    LocVT = XLenVT;
    LocInfo = CCValAssign::Indirect;
    PendingLocs.push_back(
        CCValAssign::getPending(ValNo, ValVT, LocVT, LocInfo));
    PendingArgFlags.push_back(ArgFlags);
    if (!ArgFlags.isSplitEnd()) {
      return false;
    }
  }

  // If the split argument only had two elements, it should be passed directly
  // in registers or on the stack.
  if (ArgFlags.isSplitEnd() && PendingLocs.size() <= 2) {
    assert(PendingLocs.size() == 2 && "Unexpected PendingLocs.size()");
    // Apply the normal calling convention rules to the first half of the
    // split argument.
    CCValAssign VA = PendingLocs[0];
    ISD::ArgFlagsTy AF = PendingArgFlags[0];
    PendingLocs.clear();
    PendingArgFlags.clear();
    return CC_LinxV5Assign2XLen(XLen, State, VA, AF, ValNo, ValVT, LocVT,
                                ArgFlags);
  }

  // Allocate to a register if possible, or else a stack slot.
  Register Reg;
  if (ValVT.isScalableVector()) {
    assert(0 && "Support Scalable Vector!");
  } else
    Reg = State.AllocateReg(ArgGPRs);
  unsigned StackOffset =
      Reg ? 0 : State.AllocateStack(XLen / 8, Align(XLen / 8));

  // If we reach this point and PendingLocs is non-empty, we must be at the
  // end of a split argument that must be passed indirectly.
  if (!PendingLocs.empty()) {
    assert(ArgFlags.isSplitEnd() && "Expected ArgFlags.isSplitEnd()");
    assert(PendingLocs.size() > 2 && "Unexpected PendingLocs.size()");

    for (auto &It : PendingLocs) {
      if (Reg)
        It.convertToReg(Reg);
      else
        It.convertToMem(StackOffset);
      State.addLoc(It);
    }
    PendingLocs.clear();
    PendingArgFlags.clear();
    return false;
  }

  if (!TLI.getSubtarget().isSIMT()) {
    assert((LocVT == MVT::i64 || LocVT == MVT::f64 || LocVT == MVT::f32 ||
            LocVT == MVT::f16) &&
           "Expected an i64 or float-pointing value type at this stage");
  }

  if (Reg) {
    State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
    return false;
  }

  // When a floating-point value is passed on the stack, no bit-conversion is
  // needed.
  if (ValVT.isFloatingPoint()) {
    LocVT = ValVT;
    LocInfo = CCValAssign::Full;
  }
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, StackOffset, LocVT, LocInfo));
  return false;
}

template <typename ArgTy>
static Optional<unsigned> preAssignMask(const ArgTy &Args) {
  for (const auto &ArgIdx : enumerate(Args)) {
    MVT ArgVT = ArgIdx.value().VT;
    if (ArgVT.isScalableVector() &&
        ArgVT.getVectorElementType().SimpleTy == MVT::i1)
      return ArgIdx.index();
  }
  return None;
}

void LinxV5TargetLowering::analyzeInputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
    bool isFuncBlock) const {
  unsigned NumArgs = Ins.size();
  FunctionType *FType = MF.getFunction().getFunctionType();

  Optional<unsigned> FirstMaskArgument;

  for (unsigned i = 0; i != NumArgs; ++i) {
    MVT ArgVT = Ins[i].VT;
    ISD::ArgFlagsTy ArgFlags = Ins[i].Flags;

    Type *ArgTy = nullptr;
    if (IsRet)
      ArgTy = FType->getReturnType();
    else if (Ins[i].isOrigArg())
      ArgTy = FType->getParamType(Ins[i].getOrigArgIndex());

    LinxV5ABI::ABI ABI = MF.getSubtarget<LinxV5Subtarget>().getTargetABI();
    bool Result = false;
    if (isFuncBlock)
      Result = CC_LinxV5_SIMT(MF.getDataLayout(), ABI, i, ArgVT, ArgVT,
                              CCValAssign::Full, ArgFlags, CCInfo,
                              /*IsFixed=*/true, IsRet, ArgTy, *this,
                              FirstMaskArgument, MF.getFunction());
    else
      Result = CC_LinxV5(MF.getDataLayout(), ABI, i, ArgVT, ArgVT,
                         CCValAssign::Full, ArgFlags, CCInfo, /*IsFixed=*/true,
                         IsRet, ArgTy, *this, FirstMaskArgument);
    if (Result) {
      LLVM_DEBUG(dbgs() << "InputArg #" << i << " has unhandled type "
                        << EVT(ArgVT).getEVTString() << '\n');
      llvm_unreachable(nullptr);
    }
  }
}

void LinxV5TargetLowering::analyzeOutputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::OutputArg> &Outs, bool IsRet,
    CallLoweringInfo *CLI, bool isFuncBlock) const {
  unsigned NumArgs = Outs.size();

  Optional<unsigned> FirstMaskArgument;

  for (unsigned i = 0; i != NumArgs; i++) {
    MVT ArgVT = Outs[i].VT;
    ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
    Type *OrigTy = CLI ? CLI->getArgs()[Outs[i].OrigArgIndex].Ty : nullptr;

    LinxV5ABI::ABI ABI = MF.getSubtarget<LinxV5Subtarget>().getTargetABI();
    bool Result = false;
    if (isFuncBlock)
      Result = CC_LinxV5_SIMT(MF.getDataLayout(), ABI, i, ArgVT, ArgVT,
                              CCValAssign::Full, ArgFlags, CCInfo,
                              Outs[i].IsFixed, IsRet, OrigTy, *this,
                              FirstMaskArgument, MF.getFunction());
    else
      Result = CC_LinxV5(MF.getDataLayout(), ABI, i, ArgVT, ArgVT,
                         CCValAssign::Full, ArgFlags, CCInfo, Outs[i].IsFixed,
                         IsRet, OrigTy, *this, FirstMaskArgument);
    if (Result) {
      LLVM_DEBUG(dbgs() << "OutputArg #" << i << " has unhandled type "
                        << EVT(ArgVT).getEVTString() << "\n");
      llvm_unreachable(nullptr);
    }
  }
}

// Convert Val to a ValVT. Should not be called for CCValAssign::Indirect
// values.
static SDValue convertLocVTToValVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL) {
  switch (VA.getLocInfo()) {
  case CCValAssign::Full:
    break;
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  }
  return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromRegLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL,
                                const ISD::InputArg &In,
                                const LinxV5TargetLowering &TLI) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  EVT LocVT = VA.getLocVT();
  SDValue Val;
  const TargetRegisterClass *RC = TLI.getRegClassFor(LocVT.getSimpleVT());
  Register VReg = RegInfo.createVirtualRegister(RC);
  RegInfo.addLiveIn(VA.getLocReg(), VReg);
  Val = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);

  // If input is sign extended from 32 bits, note it for the SExtWRemoval pass.
  if (In.isOrigArg()) {
    Argument *OrigArg = MF.getFunction().getArg(In.getOrigArgIndex());
    if (OrigArg->getType()->isIntegerTy()) {
      unsigned BitWidth = OrigArg->getType()->getIntegerBitWidth();
      // An input zero extended from i31 can also be considered sign extended.
      if ((BitWidth <= 32 && In.Flags.isSExt()) ||
          (BitWidth < 32 && In.Flags.isZExt())) {
        LinxV5MachineFunctionInfo *LINXFI =
            MF.getInfo<LinxV5MachineFunctionInfo>();
        LINXFI->addSExt32Register(VReg);
      }
    }
  }

  if (VA.getLocInfo() == CCValAssign::Indirect)
    return Val;

  return convertLocVTToValVT(DAG, Val, VA, DL);
}

static SDValue convertValVTToLocVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL) {
  EVT LocVT = VA.getLocVT();

  switch (VA.getLocInfo()) {
  case CCValAssign::Full:
    break;
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  }
  return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromMemLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  EVT LocVT = VA.getLocVT();
  EVT ValVT = VA.getValVT();
  EVT PtrVT = MVT::getIntegerVT(DAG.getDataLayout().getPointerSizeInBits(0));
  int FI = MFI.CreateFixedObject(ValVT.getSizeInBits() / 8,
                                 VA.getLocMemOffset(), /*Immutable=*/true);
  SDValue FIN = DAG.getFrameIndex(FI, PtrVT);
  SDValue Val;

  ISD::LoadExtType ExtType;
  switch (VA.getLocInfo()) {
  case CCValAssign::Full:
  case CCValAssign::Indirect:
    ExtType = ISD::NON_EXTLOAD;
    break;
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  }
  Val = DAG.getExtLoad(
      ExtType, DL, LocVT, Chain, FIN,
      MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI), ValVT);
  return Val;
}

// FastCC has less than 1% performance improvement for some particular
// benchmark. But theoretically, it may has benenfit for some cases.
static bool CC_LinxV5_FastCC(unsigned ValNo, MVT ValVT, MVT LocVT,
                             CCValAssign::LocInfo LocInfo,
                             ISD::ArgFlagsTy ArgFlags, CCState &State) {
  if (LocVT == MVT::i32 || LocVT == MVT::i64) {
    // caller-saved registers.
    static const MCPhysReg GPRList[] = {LinxV5::R2,  LinxV5::R3,  LinxV5::R4,
                                        LinxV5::R5,  LinxV5::R6,  LinxV5::R7,
                                        LinxV5::R8,  LinxV5::R9,  LinxV5::R20,
                                        LinxV5::R21, LinxV5::R22, LinxV5::R23};
    if (unsigned Reg = State.AllocateReg(GPRList)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::i32 || LocVT == MVT::f32) {
    unsigned Offset4 = State.AllocateStack(4, Align(4));
    State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset4, LocVT, LocInfo));
    return false;
  }

  if (LocVT == MVT::i64 || LocVT == MVT::f64) {
    unsigned Offset5 = State.AllocateStack(8, Align(8));
    State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset5, LocVT, LocInfo));
    return false;
  }

  return true; // CC didn't match.
}

static void appendReduceDefs(MachineFunction &MF, MachineInstr &MI) {
  StringRef FuncName;
  if (MI.getOperand(0).isGlobal())
    FuncName = MI.getOperand(0).getGlobal()->getName();
  else if (MI.getOperand(0).isSymbol())
    FuncName = MI.getOperand(0).getSymbolName();
  else
    report_fatal_error("unexpected function-block descriptor!");
  Function *CalledFunc = MF.getMMI().getModule()->getFunction(FuncName);
  unsigned NumRets = 0;
  CalledFunc->getFnAttribute("reduction_num")
      .getValueAsString()
      .getAsInteger(10, NumRets);
  if (NumRets >= sizeof(SIMTArgGPRs) / sizeof(MCPhysReg))
    report_fatal_error("linx-simt exceeds the max reduce number!");
  for (unsigned i = 0; i < NumRets; ++i) {
    MI.addRegisterDefined(SIMTArgGPRs[i]);
  }
}

void LinxV5TargetLowering::finalizeLowering(MachineFunction &MF) const {
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (MI.getOpcode() == LinxV5::PseudoFunctionBlock) {
        appendReduceDefs(MF, MI);
      }
    }
  }
  TargetLowering::finalizeLowering(MF);
}

// Transform physical registers into virtual registers.
SDValue LinxV5TargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();

  switch (CallConv) {
  case CallingConv::C:
  case CallingConv::Fast:
    break;
  default:
    report_fatal_error("Unsupported calling convention");
  }

  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();
  unsigned XLenInBytes = Subtarget.getXLen() / 8;
  // Used with vargs to acumulate store chains.
  std::vector<SDValue> OutChains;

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  if (CallConv == CallingConv::Fast)
    CCInfo.AnalyzeFormalArguments(Ins, CC_LinxV5_FastCC);
  else
    analyzeInputArgs(MF, CCInfo, Ins, /*IsRet=*/false, Subtarget.isSIMT());

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue;
    if (VA.isRegLoc())
      ArgValue = unpackFromRegLoc(DAG, Chain, VA, DL, Ins[i], *this);
    else
      ArgValue = unpackFromMemLoc(DAG, Chain, VA, DL);

    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // If the original argument was split and passed by reference (e.g. i128
      // on RV32), we need to load all parts of it here (using the same
      // address).
      InVals.push_back(DAG.getLoad(VA.getValVT(), DL, Chain, ArgValue,
                                   MachinePointerInfo()));
      unsigned ArgIndex = Ins[i].OrigArgIndex;
      assert(Ins[i].PartOffset == 0);
      while (i + 1 != e && Ins[i + 1].OrigArgIndex == ArgIndex) {
        CCValAssign &PartVA = ArgLocs[i + 1];
        unsigned PartOffset = Ins[i + 1].PartOffset;
        SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, ArgValue,
                                      DAG.getIntPtrConstant(PartOffset, DL));
        InVals.push_back(DAG.getLoad(PartVA.getValVT(), DL, Chain, Address,
                                     MachinePointerInfo()));
        ++i;
      }
      continue;
    }
    InVals.push_back(ArgValue);
  }

  if (IsVarArg) {
    ArrayRef<MCPhysReg> ArgRegs = makeArrayRef(ArgGPRs);
    unsigned Idx = CCInfo.getFirstUnallocated(ArgRegs);
    const TargetRegisterClass *RC = Subtarget.getRegisterInfo()->getSTDRC();
    MachineFrameInfo &MFI = MF.getFrameInfo();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();
    LinxV5MachineFunctionInfo *RVFI = MF.getInfo<LinxV5MachineFunctionInfo>();

    // Offset of the first variable argument from stack pointer, and size of
    // the vararg save area. For now, the varargs save area is either zero or
    // large enough to hold a0-a7.
    int VaArgOffset, VarArgsSaveSize;

    // If all registers are allocated, then all varargs must be passed on the
    // stack and we don't need to save any argregs.
    if (ArgRegs.size() == Idx) {
      VaArgOffset = static_cast<int>(CCInfo.getNextStackOffset());
      VarArgsSaveSize = 0;
    } else {
      VarArgsSaveSize = static_cast<int>(XLenInBytes * (ArgRegs.size() - Idx));
      VaArgOffset = -VarArgsSaveSize;
    }

    // Record the frame index of the first variable argument
    // which is a value necessary to VASTART.
    int FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
    RVFI->setVarArgsFrameIndex(FI);

    // If saving an odd number of registers then create an extra stack slot to
    // ensure that the frame pointer is 2*XLEN-aligned, which in turn ensures
    // offsets to even-numbered registered remain 2*XLEN-aligned.
    if (Idx % 2) {
      MFI.CreateFixedObject(XLenInBytes, VaArgOffset - (int)XLenInBytes, true);
      VarArgsSaveSize += static_cast<int>(XLenInBytes);
    }

    // Copy the integer registers that may have been used for passing varargs
    // to the vararg save area.
    for (unsigned I = Idx; I < ArgRegs.size();
         ++I, VaArgOffset += static_cast<int>(XLenInBytes)) {
      const Register Reg = RegInfo.createVirtualRegister(RC);
      RegInfo.addLiveIn(ArgRegs[I], Reg);
      SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, XLenVT);
      FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
      SDValue PtrOff = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
      SDValue Store = DAG.getStore(Chain, DL, ArgValue, PtrOff,
                                   MachinePointerInfo::getFixedStack(MF, FI));
      cast<StoreSDNode>(Store.getNode())
          ->getMemOperand()
          ->setValue((Value *)nullptr);
      OutChains.push_back(Store);
    }
    RVFI->setVarArgsSaveSize(VarArgsSaveSize);
  }

  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens for vararg functions.
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
  }

  return Chain;
}

/// isEligibleForTailCallOptimization - Check whether the call is eligible
/// for tail call optimization.
/// Note: This is modelled after ARM's IsEligibleForTailCallOptimization.
bool LinxV5TargetLowering::isEligibleForTailCallOptimization(
    CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
    const SmallVector<CCValAssign, 16> &ArgLocs) const {
  auto &Callee = CLI.Callee;
  auto CalleeCC = CLI.CallConv;
  auto &Outs = CLI.Outs;
  auto &Caller = MF.getFunction();
  auto CallerCC = Caller.getCallingConv();

  // Exception-handling functions need a special set of instructions to
  // indicate a return to the hardware. Tail-calling another function would
  // probably break this.
  // TODO: The "interrupt" attribute isn't currently defined by LinxV5. This
  // should be expanded as new function attributes are introduced.
  if (Caller.hasFnAttribute("interrupt"))
    return false;

  // Do not tail call opt if the stack is used to pass parameters.
  if (CCInfo.getNextStackOffset() != 0)
    return false;

  // Do not tail call opt if any parameters need to be passed indirectly.
  // Since long doubles (fp128) and i128 are larger than 2*XLEN, they are
  // passed indirectly. So the address of the value will be passed in a
  // register, or if not available, then the address is put on the stack. In
  // order to pass indirectly, space on the stack often needs to be allocated
  // in order to store the value. In this case the CCInfo.getNextStackOffset()
  // != 0 check is not enough and we need to check if any CCValAssign ArgsLocs
  // are passed CCValAssign::Indirect.
  for (auto &VA : ArgLocs)
    if (VA.getLocInfo() == CCValAssign::Indirect)
      return false;

  // Do not tail call opt if either caller or callee uses struct return
  // semantics.
  auto IsCallerStructRet = Caller.hasStructRetAttr();
  auto IsCalleeStructRet = Outs.empty() ? false : Outs[0].Flags.isSRet();
  if (IsCallerStructRet || IsCalleeStructRet)
    return false;

  // Externally-defined functions with weak linkage should not be
  // tail-called. The behaviour of branch instructions in this situation (as
  // used for tail calls) is implementation-defined, so we cannot rely on the
  // linker replacing the tail call with a return.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = G->getGlobal();
    if (GV->hasExternalWeakLinkage())
      return false;
  }

  // The callee has to preserve all registers the caller needs to preserve.
  const LinxV5RegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *CallerPreserved = TRI->getCallPreservedMask(MF, CallerCC);
  if (CalleeCC != CallerCC) {
    const uint32_t *CalleePreserved = TRI->getCallPreservedMask(MF, CalleeCC);
    if (!TRI->regmaskSubsetEqual(CallerPreserved, CalleePreserved))
      return false;
  }

  // Byval parameters hand the function a pointer directly into the stack area
  // we want to reuse during a tail call. Working around this *is* possible
  // but less efficient and uglier in LowerCall.
  for (auto &Arg : Outs)
    if (Arg.Flags.isByVal())
      return false;

  return true;
}

// Lower a call to a callseq_start + CALL + callseq_end chain, and add input
// and output parameter nodes.
SDValue
LinxV5TargetLowering::LowerCall(CallLoweringInfo &CLI,
                                SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  bool IsFunctionBlock = false;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();

  MachineFunction &MF = DAG.getMachineFunction();

  // Analyze the operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState ArgCCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  if (Subtarget.isSIMT())
    report_fatal_error("Call in simt!\n");

  if (CallConv == CallingConv::Fast)
    ArgCCInfo.AnalyzeCallOperands(Outs, CC_LinxV5_FastCC);
  else
    analyzeOutputArgs(MF, ArgCCInfo, Outs, /*IsRet=*/false, &CLI,
                      IsFunctionBlock);

  // Check if it's really possible to do a tail call.
  if (IsTailCall)
    IsTailCall = isEligibleForTailCallOptimization(ArgCCInfo, CLI, MF, ArgLocs);

  if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
    // maybe use simt callconv is better?
    // Note: Current program model ensure SIMT function must in current model.
    Function *CalledFunc = CLI.CB->getCalledFunction();
    if (CalledFunc && CalledFunc->getFnAttribute("__vec__").isValid()) {
      IsTailCall = false;
      IsFunctionBlock = true;
    }
  }

  if (IsTailCall)
    ++NumTailCalls;
  else if (CLI.CB && CLI.CB->isMustTailCall())
    report_fatal_error("failed to perform tail call elimination on a call "
                       "site marked musttail");

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = ArgCCInfo.getNextStackOffset();

  // Create local copies for byval args
  SmallVector<SDValue, 8> ByValArgs;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (!Flags.isByVal())
      continue;

    SDValue Arg = OutVals[i];
    unsigned Size = Flags.getByValSize();
    Align Alignment = Flags.getNonZeroByValAlign();

    int FI =
        MF.getFrameInfo().CreateStackObject(Size, Alignment, /*isSS=*/false);
    SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
    SDValue SizeNode = DAG.getConstant(Size, DL, XLenVT);

    Chain = DAG.getMemcpy(Chain, DL, FIPtr, Arg, SizeNode, Alignment,
                          /*IsVolatile=*/false,
                          /*AlwaysInline=*/false, IsTailCall,
                          MachinePointerInfo(), MachinePointerInfo());
    ByValArgs.push_back(FIPtr);
  }

  if (!IsTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, CLI.DL);

  // Copy argument values to their designated locations.
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;
  SDValue StackPtr;
  for (unsigned i = 0, j = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue = OutVals[i];
    ISD::ArgFlagsTy Flags = Outs[i].Flags;

    // Promote the value if needed.
    // For now, only handle fully promoted and indirect arguments.
    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // Store the argument in a stack slot and pass its address.
      SDValue SpillSlot = DAG.CreateStackTemporary(Outs[i].ArgVT);
      int FI = cast<FrameIndexSDNode>(SpillSlot)->getIndex();
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, SpillSlot,
                       MachinePointerInfo::getFixedStack(MF, FI)));
      // If the original argument was split (e.g. i128), we need
      // to store all parts of it here (and pass just one address).
      unsigned ArgIndex = Outs[i].OrigArgIndex;
      assert(Outs[i].PartOffset == 0);
      while (i + 1 != e && Outs[i + 1].OrigArgIndex == ArgIndex) {
        SDValue PartValue = OutVals[i + 1];
        unsigned PartOffset = Outs[i + 1].PartOffset;
        SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, SpillSlot,
                                      DAG.getIntPtrConstant(PartOffset, DL));
        MemOpChains.push_back(
            DAG.getStore(Chain, DL, PartValue, Address,
                         MachinePointerInfo::getFixedStack(MF, FI)));
        ++i;
      }
      ArgValue = SpillSlot;
    } else {
      ArgValue = convertValVTToLocVT(DAG, ArgValue, VA, DL);
    }

    // Use local copy if it is a byval arg.
    if (Flags.isByVal())
      ArgValue = ByValArgs[j++];

    if (VA.isRegLoc()) {
      // Queue up the argument copies and emit them at the end.
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), ArgValue));
    } else {
      assert(VA.isMemLoc() && "Argument not register or memory");
      assert(!IsTailCall && "Tail call not allowed if stack is used "
                            "for passing parameters");

      // Work out the address of the stack slot.
      if (!StackPtr.getNode())
        StackPtr = DAG.getCopyFromReg(Chain, DL, LinxV5::R1, PtrVT);
      SDValue Address =
          DAG.getNode(ISD::ADD, DL, PtrVT, StackPtr,
                      DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

      // Emit the store.
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, Address, MachinePointerInfo()));
    }
  }

  // Join the stores, which are independent of one another.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;

  // Build a sequence of copy-to-reg nodes, chained and glued together.
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
    Glue = Chain.getValue(1);
  }

  // Validate that none of the argument registers have been marked as
  // reserved, if so report an error. Do the same for the return address if this
  // is not a tailcall.
  validateCCReservedRegs(RegsToPass, MF);
  if (!IsTailCall &&
      MF.getSubtarget<LinxV5Subtarget>().isRegisterReservedByUser(LinxV5::R10))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Return address register required, but has been reserved."});

  // If the callee is a GlobalAddress/ExternalSymbol node, turn it into a
  // TargetGlobalAddress/TargetExternalSymbol node so that legalize won't
  // split it and then direct call can be matched by PseudoCALL.
  if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = S->getGlobal();

    unsigned OpFlags = LinxV5II::MO_CALL;

    Callee = DAG.getTargetGlobalAddress(GV, DL, PtrVT, 0, OpFlags);
  } else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    unsigned OpFlags = LinxV5II::MO_CALL;

    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), PtrVT, OpFlags);
  }

  // The first call operand is the chain and the second is the target address.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

  if (!IsTailCall) {
    // Add a register mask operand representing the call-preserved registers.
    const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
    const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
    assert(Mask && "Missing call preserved mask for calling convention");
    Ops.push_back(DAG.getRegisterMask(Mask));
  }

  // Glue the call to the argument copies, if any.
  if (Glue.getNode())
    Ops.push_back(Glue);

  // Emit the call.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);

  if (IsTailCall) {
    MF.getFrameInfo().setHasTailCall();
    return DAG.getNode(LinxV5ISD::TAIL, DL, NodeTys, Ops);
  }

  if (IsFunctionBlock)
    Chain = DAG.getNode(LinxV5ISD::FunctionBlock, DL, NodeTys, Ops);
  else
    Chain = DAG.getNode(LinxV5ISD::CALL, DL, NodeTys, Ops);
  DAG.addNoMergeSiteInfo(Chain.getNode(), CLI.NoMerge);
  Glue = Chain.getValue(1);

  // Mark the end of the call, which is glued to the call itself.
  Chain = DAG.getCALLSEQ_END(Chain, DAG.getConstant(NumBytes, DL, PtrVT, true),
                             DAG.getConstant(0, DL, PtrVT, true), Glue, DL);
  Glue = Chain.getValue(1);

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  analyzeInputArgs(MF, RetCCInfo, Ins, /*IsRet=*/true, IsFunctionBlock);

  // Copy all of the result registers out of their specified physreg.
  for (auto &VA : RVLocs) {
    // Copy the value out
    SDValue RetValue =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    // Glue the RetValue to the end of the call sequence
    Chain = RetValue.getValue(1);
    Glue = RetValue.getValue(2);

    if (VA.getLocVT() == MVT::i32 && VA.getValVT() == MVT::f64) {
      assert(VA.getLocReg() == ArgGPRs[0] && "Unexpected reg assignment");
      SDValue RetValue2 =
          DAG.getCopyFromReg(Chain, DL, ArgGPRs[1], MVT::i32, Glue);
      Chain = RetValue2.getValue(1);
      Glue = RetValue2.getValue(2);
      RetValue = DAG.getNode(LinxV5ISD::BuildPairF64, DL, MVT::f64, RetValue,
                             RetValue2);
    }

    RetValue = convertLocVTToValVT(DAG, RetValue, VA, DL);

    InVals.push_back(RetValue);
  }

  return Chain;
}

bool LinxV5TargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  Optional<unsigned> FirstMaskArgument;

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    MVT VT = Outs[i].VT;
    ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
    LinxV5ABI::ABI ABI = MF.getSubtarget<LinxV5Subtarget>().getTargetABI();
    bool Result = false;
    if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
      Result =
          CC_LinxV5_SIMT(MF.getDataLayout(), ABI, i, VT, VT, CCValAssign::Full,
                         ArgFlags, CCInfo, /*IsFixed=*/true, /*IsRet=*/true,
                         nullptr, *this, FirstMaskArgument, MF.getFunction());
    else
      Result = CC_LinxV5(MF.getDataLayout(), ABI, i, VT, VT, CCValAssign::Full,
                         ArgFlags, CCInfo, /*IsFixed=*/true, /*IsRet=*/true,
                         nullptr, *this, FirstMaskArgument);
    if (Result)
      return false;
  }
  return true;
}

SDValue
LinxV5TargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                  bool IsVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &DL, SelectionDAG &DAG) const {
  const MachineFunction &MF = DAG.getMachineFunction();
  const LinxV5Subtarget &STI = MF.getSubtarget<LinxV5Subtarget>();

  // Stores the assignment of the return value to a location.
  SmallVector<CCValAssign, 16> RVLocs;

  // Info about the registers and stack slot.
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  analyzeOutputArgs(DAG.getMachineFunction(), CCInfo, Outs, /*IsRet=*/true,
                    nullptr, Subtarget.isSIMT());

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (unsigned i = 0, e = RVLocs.size(); i < e; ++i) {
    SDValue Val = OutVals[i];
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    // Handle a 'normal' return.
    Val = convertValVTToLocVT(DAG, Val, VA, DL);
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);

    if (STI.isRegisterReservedByUser(VA.getLocReg()))
      MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
          MF.getFunction(),
          "Return value register required, but has been reserved."});

    // Guarantee that all emitted copies are stuck together.
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain; // Update chain.

  // Add the glue node if we have it.
  if (Glue.getNode()) {
    RetOps.push_back(Glue);
  }

  // Interrupt service routines use different return instructions.
  const Function &Func = DAG.getMachineFunction().getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    if (!Func.getReturnType()->isVoidTy())
      report_fatal_error(
          "Functions with the interrupt attribute must have void return type!");

    MachineFunction &MF = DAG.getMachineFunction();
    StringRef Kind =
        MF.getFunction().getFnAttribute("interrupt").getValueAsString();

    unsigned RetOpc;
    if (Kind == "user")
      RetOpc = LinxV5ISD::URET_FLAG;
    else if (Kind == "supervisor")
      RetOpc = LinxV5ISD::SRET_FLAG;
    else
      RetOpc = LinxV5ISD::MRET_FLAG;

    return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
  }

  return DAG.getNode(LinxV5ISD::RET_FLAG, DL, MVT::Other, RetOps);
}

void LinxV5TargetLowering::validateCCReservedRegs(
    const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
    MachineFunction &MF) const {
  const Function &F = MF.getFunction();
  const LinxV5Subtarget &STI = MF.getSubtarget<LinxV5Subtarget>();

  if (llvm::any_of(Regs, [&STI](auto Reg) {
        return STI.isRegisterReservedByUser(Reg.first);
      }))
    F.getContext().diagnose(DiagnosticInfoUnsupported{
        F, "Argument register required, but has been reserved."});
}

bool LinxV5TargetLowering::mayBeEmittedAsTailCall(const CallInst *CI) const {
  return CI->isTailCall();
}

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
LinxV5TargetLowering::ConstraintType
LinxV5TargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'f':
      return C_RegisterClass;
    case 'I':
    case 'J':
    case 'K':
      return C_Immediate;
    case 'A':
      return C_Memory;
    }
  }
  if (Constraint == "vr" || Constraint == "Tr" || Constraint == "Sr")
    return C_RegisterClass;
  return TargetLowering::getConstraintType(Constraint);
}

TargetLowering::ConstraintWeight
LinxV5TargetLowering::getSingleConstraintMatchWeight(
    AsmOperandInfo &info, const char *constraint) const {
  ConstraintWeight weight =
      TargetLowering::getSingleConstraintMatchWeight(info, constraint);
  switch (*constraint) {
  case 'S':
    if (constraint[1] == 'r')
      weight = CW_Register;
    break;
  case 'v':
  case 'T':
    if (constraint[1] == 'r')
      weight = CW_Register;
    break;
  default:
    break;
  }
  return weight;
}

std::pair<unsigned, const TargetRegisterClass *>
LinxV5TargetLowering::getRegForInlineAsmConstraint(
    const TargetRegisterInfo *TRI, StringRef Constraint, MVT VT) const {
  // First, see if this is a constraint that directly corresponds to a
  // LinxV5 register class.
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return std::make_pair(0U, &LinxV5::GRRegClass);
    default:
      break;
    }
  }

  if (Constraint == "vr")
    return std::make_pair(0U, &LinxV5::SIMTCGVRegClass);
  if (Constraint == "Tr")
    return std::make_pair(0U, &LinxV5::Tile_ABSRegClass);
  if (Constraint == "Sr")
    return std::make_pair(0U, &LinxV5::Shared_ABSRegClass);

  // Clang will correctly decode the usage of register name aliases into their
  // official names. However, other frontends like `rustc` do not. This allows
  // users of these frontends to use the ABI names for registers in LLVM-style
  // register constraints.
  unsigned XRegFromAlias = StringSwitch<unsigned>(Constraint.lower())
                               .Cases("{zero}", "{r0}", LinxV5::R0)
                               .Cases("{sp}", "{r1}", LinxV5::R1)
                               .Cases("{a0}", "{r2}", LinxV5::R2)
                               .Cases("{a1}", "{r3}", LinxV5::R3)
                               .Cases("{a2}", "{r4}", LinxV5::R4)
                               .Cases("{a3}", "{r5}", LinxV5::R5)
                               .Cases("{a4}", "{r6}", LinxV5::R6)
                               .Cases("{a5}", "{r7}", LinxV5::R7)
                               .Cases("{a6}", "{r8}", LinxV5::R8)
                               .Cases("{a7}", "{r9}", LinxV5::R9)
                               .Cases("{ra}", "{r10}", LinxV5::R10)
                               .Cases("{s0}", "{fp}", "{r11}", LinxV5::R11)
                               .Cases("{s1}", "{r12}", LinxV5::R12)
                               .Cases("{s2}", "{r13}", LinxV5::R13)
                               .Cases("{s3}", "{r14}", LinxV5::R14)
                               .Cases("{s4}", "{r15}", LinxV5::R15)
                               .Cases("{s5}", "{r16}", LinxV5::R16)
                               .Cases("{s6}", "{r17}", LinxV5::R17)
                               .Cases("{s7}", "{r18}", LinxV5::R18)
                               .Cases("{s8}", "{r19}", LinxV5::R19)
                               .Cases("{x0}", "{r20}", LinxV5::R20)
                               .Cases("{x1}", "{r21}", LinxV5::R21)
                               .Cases("{x2}", "{r22}", LinxV5::R22)
                               .Cases("{x3}", "{r23}", LinxV5::R23)
                               .Default(LinxV5::NoRegister);
  if (XRegFromAlias != LinxV5::NoRegister)
    return std::make_pair(XRegFromAlias, &LinxV5::GRRegClass);

  return std::make_pair(0, nullptr);
}

unsigned LinxV5TargetLowering::getInlineAsmMemConstraint(
    StringRef ConstraintCode) const {
  // Currently only support length 1 constraints.
  if (ConstraintCode.size() == 1) {
    switch (ConstraintCode[0]) {
    case 'A':
      return InlineAsm::Constraint_A;
    default:
      break;
    }
  }

  return TargetLowering::getInlineAsmMemConstraint(ConstraintCode);
}

void LinxV5TargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, std::string &Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  // Currently only support length 1 constraints.
  if (Constraint.length() == 1) {
    switch (Constraint[0]) {
    case 'I':
      // Validate & create a 12-bit signed immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = static_cast<uint64_t>(C->getSExtValue());
        if (isInt<12>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, SDLoc(Op), Subtarget.getXLenVT()));
      }
      return;
    case 'J':
      // Validate & create an integer zero operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op))
        if (C->getZExtValue() == 0)
          Ops.push_back(
              DAG.getTargetConstant(0, SDLoc(Op), Subtarget.getXLenVT()));
      return;
    case 'K':
      // Validate & create a 5-bit unsigned immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getZExtValue();
        if (isUInt<5>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, SDLoc(Op), Subtarget.getXLenVT()));
      }
      return;
    default:
      break;
    }
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

Register LinxV5TargetLowering::getExceptionPointerRegister(
    const Constant *PersonalityFn) const {
  return LinxV5::R2;
}

Register LinxV5TargetLowering::getExceptionSelectorRegister(
    const Constant *PersonalityFn) const {
  return LinxV5::R3;
}

bool LinxV5TargetLowering::shouldExtendTypeInLibCall(EVT Type) const {
  // Return false to suppress the unnecessary extensions if the LibCall
  // arguments or return value is f32 type for LP64 ABI.
  LinxV5ABI::ABI ABI = Subtarget.getTargetABI();
  if (ABI == LinxV5ABI::ABI_LP64 && (Type == MVT::f32))
    return false;

  return true;
}

bool LinxV5TargetLowering::shouldSignExtendTypeInLibCall(EVT Type,
                                                         bool IsSigned) const {
  if (Type == MVT::i32)
    return true;

  return IsSigned;
}

bool LinxV5TargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                                  SDValue C) const {
  // Check integral scalar types.
  if (VT.isScalarInteger()) {
    // Omit the optimization if the sub target has the M extension and the data
    // size exceeds XLen.
    if (VT.getSizeInBits() > Subtarget.getXLen())
      return false;
    if (auto *ConstNode = dyn_cast<ConstantSDNode>(C.getNode())) {
      // Break the MUL to a SLLI and an ADD/SUB.
      const APInt &Imm = ConstNode->getAPIntValue();
      if ((Imm + 1).isPowerOf2() || (Imm - 1).isPowerOf2() ||
          (1 - Imm).isPowerOf2() || (-1 - Imm).isPowerOf2())
        return true;
      // Omit the following optimization if the sub target has the M extension
      // and the data size >= XLen.
      if (VT.getSizeInBits() >= Subtarget.getXLen())
        return false;
      // Break the MUL to two SLLI instructions and an ADD/SUB, if Imm needs
      // a pair of LUI/ADDI.
      if (!Imm.isSignedIntN(12) && Imm.countTrailingZeros() < 12) {
        APInt ImmS = Imm.ashr(Imm.countTrailingZeros());
        if ((ImmS + 1).isPowerOf2() || (ImmS - 1).isPowerOf2() ||
            (1 - ImmS).isPowerOf2())
          return true;
      }
    }
  }

  return false;
}

bool LinxV5TargetLowering::isSkipCommonZextCombine(SDNode *N,
                                                   SelectionDAG &DAG) const {
  return isProfitToTransZextToSext(N, DAG);
}
