//===------- LinxV5InstrInfo.td - LinxV5 instructions ---*- tablegen -*------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------------===//

def SDT_LinxV5Call     : SDTypeProfile<0, -1, [SDTCisVT<0, XLenVT>]>;

def linxv5_call      : SDNode<"LinxV5ISD::CALL", SDT_LinxV5Call,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;

def linxv5_tail      : SDNode<"LinxV5ISD::TAIL", SDT_LinxV5Call,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;

def linxv5_fblock  : SDNode<"LinxV5ISD::FunctionBlock", SDT_LinxV5Call,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;

// dst = (0)<optional>TileOut0 (1)<optional>TileOut1
// src = (1)Label, (2)Dim.X, (3)Dim.Y, (4)Dim.Z, (5)<optional>Tile Output Size0, (6)<optional>Tile Output Size1
//       (7)<optional>TileSrc0, (8)<optional>TileSrc1, (9)<optional>TileSrc2
//       (10)GetMask,
//       (10+...)Implicit GPR Uses...
def linxv5_vcall   : SDNode<"LinxV5ISD::VCALL", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def linxv5_mcall   : SDNode<"LinxV5ISD::MCALL", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

// Format of Template Block Instruction:
// dst = (0)TileOut
// src = (1)Dim.X, (2)Dim.Y, (3)Dim.Z,
//       (4)Tile Element Type-A, (5)Tile Element Type-B, (6)Tile Output Size
//       (7)TileSrc0, (8)TileSrc1, (9)....
def linxv5_blk_matmul   : SDNode<"LinxV5ISD::BLK_MATMUL", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def linxv5_blk_matmul_ac   : SDNode<"LinxV5ISD::BLK_MATMUL_AC", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

// Format of Template Block MX Instruction:
// dst = (0)TileOut
// src = (1)Dim.M, (2)Dim.N, (3)Dim.K,
//       (4)AType, (5)BType, (6)Tile Output Size,
//       (7)TileSrc0, (8)TileSrc1, (9)....
def linxv5_blk_matmulmx  : SDNode<"LinxV5ISD::BLK_MATMULMX", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def linxv5_blk_matmulmxb  : SDNode<"LinxV5ISD::BLK_MATMULMXB", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def linxv5_blk_matmulmx_ac  : SDNode<"LinxV5ISD::BLK_MATMULMX_AC", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def linxv5_blk_matmulmxb_ac  : SDNode<"LinxV5ISD::BLK_MATMULMXB_AC", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def linxv5_blk_tload : SDNode<"LinxV5ISD::BLK_TLOAD", SDT_LinxV5Call, [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def linxv5_blk_acccvt : SDNode<"LinxV5ISD::BLK_ACCCVT", SDT_LinxV5Call, [SDNPHasChain]>;

def DstRWithArrowAsmOperand : AsmOperandClass {
  let Name = "DstRWithArrow";
  let ParserMethod = "parseDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def DstRWithArrowNoRAAsmOperand : AsmOperandClass {
  let Name = "DstRWithArrowNoRA";
  let ParserMethod = "parseDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def DstRWithArrow : RegisterOperand<MCDst> {
  let ParserMatchClass = DstRWithArrowAsmOperand;
  let PrintMethod = "printDstRWithArrow";
}

def DstRWithArrowNoRA : RegisterOperand<MCDstNoRA> {
  let ParserMatchClass = DstRWithArrowNoRAAsmOperand;
  let PrintMethod = "printDstRWithArrow";
}

def RegSrcAsmOperand : AsmOperandClass {
  let Name = "GPRSrc";
  let ParserMethod = "parseGPRSrc";
  let RenderMethod = "addRegOperands";
}

def GPRSrc : RegisterOperand<MCSrc> {
  let ParserMatchClass = RegSrcAsmOperand;
}

def RegSrcNoR0R1AsmOperand : AsmOperandClass {
  let Name = "GPRSrcNoR0R1";
  let ParserMethod = "parseGPRSrc";
  let RenderMethod = "addRegOperands";
}

def GPRSrcNoR0R1 : RegisterOperand<MCSrc> {
  let ParserMatchClass = RegSrcNoR0R1AsmOperand;
}

def RegSrcNoR0AsmOperand : AsmOperandClass {
  let Name = "GPRSrcNoR0";
  let ParserMethod = "parseGPRSrc";
  let RenderMethod = "addRegOperands";
}

def GPRSrcNoR0 : RegisterOperand<MCSrc> {
  let ParserMatchClass = RegSrcNoR0AsmOperand;
}

def GPRDstWithOptionalArrowAsmOperand : AsmOperandClass {
  let Name = "GPRDstWithOptionalArrow";
  let ParserMethod = "parseGPRDstWithOptionalArrow";
  let RenderMethod = "addRegOperands";
}

def GPRDstWithOptionalArrow : RegisterOperand<MCSrc> {
  let ParserMatchClass = GPRDstWithOptionalArrowAsmOperand;
}

def RegDepSrcAsmOperand : AsmOperandClass {
  let Name = "RegDepSrc";
  let ParserMethod = "parseRegDepSrc";
  let RenderMethod = "addRegOperands";
}

def RegDepSrc : RegisterOperand<Dep_SRC> {
  let ParserMatchClass = RegDepSrcAsmOperand;
  let PrintMethod = "printDepSrcReg";
}

def LoopRegDst : RegisterOperand<LoopReg> {
  let ParserMatchClass = DstRWithArrowAsmOperand;
  let PrintMethod = "printDstRWithArrow";
}

def DstRWithArrowLoopBAsmOperand : AsmOperandClass {
  let Name = "DstRWithArrowLoopB";
  let ParserMethod = "parseLoopBDstRWithArrow";
  let RenderMethod = "addImmOperands";
}

def LoopBRegDst : Operand<i64>, ImmLeaf<i64, [{return isUInt<2>(Imm);}]> {
  let ParserMatchClass = DstRWithArrowLoopBAsmOperand;
  let PrintMethod = "printLoopBRegDstRWithArrow";
}

def SDT_LinxV5LoopSet : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>]>;

def linxv5_loop_set  : SDNode<"LinxV5ISD::LOOPSET", SDT_LinxV5LoopSet, [SDNPHasChain]>; // LinxV5ISD::LOOPSET <dst>, <src>

class LoadADDRrr<string SrcRType, int prior> :
    ComplexPattern<iPTR, 3, "SelectLoadADDRrr<LinxV5Op::"#SrcRType#">", [], [], prior>;

class StoreADDRrr_unscaled<string SrcRType, int prior> :
    ComplexPattern<iPTR, 2, "SelectStoreADDRrr_unscaled<LinxV5Op::"#SrcRType#">", [], [], prior>;
class StoreADDRrr_scaled<string SrcRType, int shift, int prior> :
    ComplexPattern<iPTR, 2, "SelectStoreADDRrr_scaled<LinxV5Op::"#SrcRType#", "#shift#">", [], [], prior>;

def LoadADDRrrD  : LoadADDRrr<"NONE", 1>;
def LoadADDRrrUW : LoadADDRrr<"UW", 2>;
def LoadADDRrrSW : LoadADDRrr<"SW", 2>;

def StoreADDRrrD_unscaled  : StoreADDRrr_unscaled<"NONE", 1>;
def StoreADDRrrUW_unscaled : StoreADDRrr_unscaled<"UW", 2>;
def StoreADDRrrSW_unscaled : StoreADDRrr_unscaled<"SW", 2>;
class StoreADDRrrD_scaled<int shift> : StoreADDRrr_scaled<"NONE", shift, 3>;
class StoreADDRrrUW_scaled<int shift> : StoreADDRrr_scaled<"UW", shift, 4>;
class StoreADDRrrSW_scaled<int shift> : StoreADDRrr_scaled<"SW", shift, 4>;

def ADDRri_unscaled :
    ComplexPattern<iPTR, 2, "SelectADDRri_unscaled", [], [], 10>;
class ADDRri_scaled<int shift> :
    ComplexPattern<iPTR, 2, "SelectADDRri_scaled<"#shift#">", [], [], 11>;

class RegWithSrcRTypeAsmOperand<string TypeName> : AsmOperandClass {
  let Name = "RegWithSrcRType"#TypeName;
  let PredicateMethod = "isRegWithSrcRType<LinxV5Op::"#TypeName#"Traits>";
  let ParserMethod = "tryParseRegWithSrcRType<LinxV5Op::"#TypeName#"Traits>";
  let RenderMethod = "addRegWithSrcRTypeOperands";
}

class RegWithSrcRType<string TypeName, bits<2> type> : RegisterOperand<MCSrc> {
  bits<2> Type;
  let Type{1-0} = type;
  let ParserMatchClass = RegWithSrcRTypeAsmOperand<TypeName>;
  let PrintMethod = "printRegWithSrcRType<LinxV5Op::"#TypeName#"Traits>";
}

def SrcRNONE : RegWithSrcRType<"NONE", 0b00>;
def SrcRSW : RegWithSrcRType<"SW", 0b01>;
def SrcRUW : RegWithSrcRType<"UW", 0b10>;
def SrcRNEG : RegWithSrcRType<"NEG", 0b11>;
def SrcRNOT : RegWithSrcRType<"NOT", 0b11>;

def ShamtImmAsmOperand : AsmOperandClass {
  let Name = "ShamtImm";
  let ParserMethod = "parseShamtImm";
  let RenderMethod = "addImmOperands";
}

def ShamtImm : Operand<i64>, ImmLeaf<i64, [{return isUInt<5>(Imm);}]> {
  let ParserMatchClass = ShamtImmAsmOperand;
  let PrintMethod = "printShamtImm";
  let DecoderMethod = "decodeUImmOperand<5>";
}

class ALUrr_Base<bits<3> Func, LinxV5InstOpcode opc, LinxV5InstType type, RegWithSrcRType SrcROp, string opcstr> :
    LinxV5Inst_B<SrcROp.Type, Func, type, opc, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr>;

class ALUrrZ5_Base<bits<3> Func, LinxV5InstOpcode opc, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type> :
LinxV5Inst_B_Z5<SrcROp.Type, Func, type, opc, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR), opcstr>;

class ALUri_Base<bits<3> Func, LinxV5InstOpcode opc, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E<type, opc, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm12), opcstr>;

multiclass ALUarith<bits<3> Func, string opcstr, LinxV5InstOpcode opc, LinxV5InstType type> {
  def ""   : ALUrr_Base<Func, opc, type, SrcRNONE, opcstr>;
  def _SW  : ALUrr_Base<Func, opc, type, SrcRSW, opcstr>;
  def _UW  : ALUrr_Base<Func, opc, type, SrcRUW, opcstr>;
  def _NEG : ALUrr_Base<Func, opc, type, SrcRNEG, opcstr>;
}

defm ADD : ALUarith<0b000, "add", OPC_Arithmetic_Operation_64, Type_010>;
defm SUB : ALUarith<0b001, "sub", OPC_Arithmetic_Operation_64, Type_010>;

defm ADDW : ALUarith<0b000, "addw", OPC_Arithmetic_Operation_32, Type_010>;
defm SUBW : ALUarith<0b001, "subw", OPC_Arithmetic_Operation_32, Type_010>;

multiclass ALUarithI<bits<3> Func, string opcstr, LinxV5InstType type> {
  def ""  : ALUri_Base<Func, OPC_Arithmetic_Operation_64_I, uimm12, opcstr, type>;
  def W   : ALUri_Base<Func, OPC_Arithmetic_Operation_32_I, uimm12, opcstr#"w", type>;
}

let isReMaterializable = 1, isAsCheapAsAMove = 1 in {
  def ADDI : ALUri_Base<0b000, OPC_Arithmetic_Operation_64_I, uimm12, "addi", Type_010> {
    let Format = InstFormat_ADDI;
  }
}

def ADDIW : ALUri_Base<0b000, OPC_Arithmetic_Operation_32_I, uimm12, "addiw", Type_010>;
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
defm SUBI : ALUarithI<0b001, "subi", Type_010>;

multiclass ALUlogic<bits<3> Func, string opcstr, LinxV5InstOpcode opc, LinxV5InstType type> {
  def ""  :  ALUrr_Base<Func, opc, type, SrcRNONE, opcstr>;
  def _SW  : ALUrr_Base<Func, opc, type, SrcRSW, opcstr>;
  def _UW  : ALUrr_Base<Func, opc, type, SrcRUW, opcstr>;
  def _NOT : ALUrr_Base<Func, opc, type, SrcRNOT, opcstr>;
}

defm AND : ALUlogic<0b010, "and", OPC_Arithmetic_Operation_64, Type_010>;
defm OR  : ALUlogic<0b011, "or",  OPC_Arithmetic_Operation_64, Type_010>;
defm XOR : ALUlogic<0b100, "xor", OPC_Arithmetic_Operation_64, Type_010>;

defm ANDW : ALUlogic<0b010, "andw", OPC_Arithmetic_Operation_32, Type_010>;
defm ORW  : ALUlogic<0b011, "orw",  OPC_Arithmetic_Operation_32, Type_010>;
defm XORW : ALUlogic<0b100, "xorw", OPC_Arithmetic_Operation_32, Type_010>;

multiclass ALUlogicI<bits<3> Func, string opcstr, LinxV5InstType type> {
  def ""  : ALUri_Base<Func, OPC_Arithmetic_Operation_64_I, simm12, opcstr, type>;
  def W   : ALUri_Base<Func, OPC_Arithmetic_Operation_32_I, simm12, opcstr#"w", type>;
}

defm ANDI : ALUlogicI<0b010, "andi", Type_010>;

let isReMaterializable = 1, isAsCheapAsAMove = 1 in {
defm ORI  : ALUlogicI<0b011, "ori", Type_010>;
defm XORI : ALUlogicI<0b100, "xori", Type_010>;
}

class ALUriZ6_Base<bits<3> Func, LinxV5InstOpcode opc, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E_Z6<type, opc, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm6), opcstr>;

class ALUriZ5_Base<bits<3> Func, LinxV5InstOpcode opc, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E_Z5<type, opc, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm5), opcstr>;

class PTOShiftRR<bits<3> Func, bits<7> OpcodeBits, string opcstr> :
    LinxV5Inst<(outs DstRWithArrow:$DstR),
               (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr, ""> {
  bits<5> SrcR;
  bits<5> SrcL;
  bits<5> DstR;
  let Inst{31-25} = 0;
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Opcode = OpcodeBits{6-4};
  let Inst{3-0} = OpcodeBits{3-0};
  let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

multiclass ALUshift<bits<3> Func, string opcstr, LinxV5InstType type> {
  def ""  : PTOShiftRR<Func, 0b0000101, opcstr>;
  def I   : ALUriZ6_Base<Func, OPC_Arithmetic_Operation_64_I, uimm6, opcstr#"i", type>;
  def W   : PTOShiftRR<Func, 0b0100101, opcstr#"w">;
  def IW  : ALUriZ5_Base<Func, OPC_Arithmetic_Operation_32_I, uimm5, opcstr#"iw", type>;
}

defm SRL : ALUshift<0b101, "srl", Type_010>;
defm SRA : ALUshift<0b110, "sra", Type_010>;
defm SLL : ALUshift<0b111, "sll", Type_010>;

//===---------------------------------- Start Linx v0.40 Bit Operation Instructions ---------------------------===//

// TODO: need a special print & decode method
class BIT_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E_MN<type, OPC_Bit_Operation, Func, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), opcstr>;

// REV reverses byte order within an N-bit field: `rev SrcL, M, N, ->Rd`.
// Like ctz/clz/bcnt, lsb26 holds the raw field offset M and lsb20 holds the
// width minus one (imml = N-1), so the operand classes match BIT_Base
// (uimm6 = raw M at Inst{31-26}; uimm6_plus1 = N-1 at Inst{25-20}).
class REV_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E_MN<type, OPC_Bit_Operation, Func, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), opcstr>;

def BXS : BIT_Base<0b000, "bxs", Type_011>;
def BXU : BIT_Base<0b001, "bxu", Type_011>;
def BIC : BIT_Base<0b010, "bic", Type_011>;
def BIS : BIT_Base<0b011, "bis", Type_011>;
def CTZ : BIT_Base<0b100, "ctz", Type_011>;
def CLZ : BIT_Base<0b101, "clz", Type_011>;
def BCNT : BIT_Base<0b110, "bcnt", Type_011>;
def REV : REV_Base<0b111, "rev", Type_011>;

//===---------------------------------- End Linx v0.40 Bit Operation Instructions ---------------------------===//

//===---------------------------------- Linx v0.40 Multi-Cycle ALU Instructions ---------------------------===//
class MUL_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_A_Z7<Func, type, OPC_MultiCycle_MUL_Instruction, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr>;

class MAD_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_A<0b00, Func, type, OPC_MultiCycle_MUL_Instruction, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, GPRSrc:$SrcD), opcstr> {
    let AsmString = opcstr # "\t$SrcL, $SrcR, $SrcD, $DstR";
}

// TODO: MULH/MULHU should support double output in the future
def MUL   : MUL_Base<0b000, "mul", Type_011>;
def MULU  : MUL_Base<0b001, "mulu", Type_011>;
def MULW  : MUL_Base<0b010, "mulw", Type_011>;
def MULUW : MUL_Base<0b011, "muluw", Type_011>;
def MADD  : MAD_Base<0b110, "madd", Type_011>;
def MADDW : MAD_Base<0b111, "maddw", Type_011>;


let PE = PET_FP in {
class MAX_Base<bits<3> Func, bits<5> flag, string opcstr> :
    LinxV5Inst_A<0b00, Func, Type_101, OPC_MAX, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
    let SrcD = flag;
    let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

class FMAX_Base<bits<3> Func, bits<2> SrcType, string opcstr> :
    LinxV5Inst_A<SrcType, Func, Type_101, OPC_MAX, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
    let SrcD = 0b00000;
    let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

multiclass FMAX_RType<bits<3> Func, string opcstr> {
  def _FD : FMAX_Base<Func, 0b00, opcstr # ".fd">;
  def _FS : FMAX_Base<Func, 0b01, opcstr # ".fs">;
  def _FH : FMAX_Base<Func, 0b10, opcstr # ".fh">;
  def _FB : FMAX_Base<Func, 0b11, opcstr # ".fb">;
}
}

def MAX   : MAX_Base<0b100, 0b00000, "max">;
def MAXU  : MAX_Base<0b100, 0b00001, "maxu">;
defm FMAX : FMAX_RType<0b110, "fmax">;

def MIN   : MAX_Base<0b101, 0b00000, "min">;
def MINU  : MAX_Base<0b101, 0b00001, "minu">;
defm FMIN  : FMAX_RType<0b111, "fmin">;

class DIV_Base<bits<3> Func, string opcstr, LinxV5InstType Type> :
    LinxV5Inst_C<Func, Type, OPC_MultiCycle_DIV_Instruction,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
    let Inst{31-25} = 0b0000000;
}

def DIV   : DIV_Base<0b000, "div", Type_011>;
def DIVU  : DIV_Base<0b001, "divu", Type_011>;
def DIVW  : DIV_Base<0b010, "divw", Type_011>;
def DIVUW : DIV_Base<0b011, "divuw", Type_011>;
def REM   : DIV_Base<0b100, "rem", Type_011>;
def REMU  : DIV_Base<0b101, "remu", Type_011>;
def REMW  : DIV_Base<0b110, "remw", Type_011>;
def REMUW : DIV_Base<0b111, "remuw", Type_011>;

//===---------------------------------- Linx v0.40 Compare Instructions -----------------------------------===//
class CMPrr_Base<bits<3> Func, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type> :
    LinxV5Inst_B_Z5<SrcROp.Type, Func, type, OPC_Compare_Instruction, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR), opcstr>;

class CMPri_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E<type, OPC_Compare_Instruction_I, Func, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm12), opcstr>;

multiclass CMP_CMP<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  def ""  : CMPrr_Base<Func, SrcRNONE, opcstr, type>;
  def _SW : CMPrr_Base<Func, SrcRSW, opcstr, type>;
  def _UW : CMPrr_Base<Func, SrcRUW, opcstr, type>;
  def I   : CMPri_Base<Func, Imm, opcstr#"i", type>;
}

multiclass CMP_AU<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    CMP_CMP<Func, Imm, opcstr, type> {
  def _NOT : CMPrr_Base<Func, SrcRNOT, opcstr, type>;
}

defm CMP_EQ : CMP_CMP<0b000, simm12, "cmp.eq", Type_010>;
defm CMP_NE : CMP_CMP<0b001, simm12, "cmp.ne", Type_010>;
defm CMP_LT : CMP_CMP<0b100, simm12,"cmp.lt", Type_010>;
defm CMP_GE : CMP_CMP<0b101, simm12, "cmp.ge", Type_010>;
defm CMP_LTU : CMP_CMP<0b110, uimm12, "cmp.ltu", Type_010>;
defm CMP_GEU : CMP_CMP<0b111, uimm12, "cmp.geu", Type_010>;
defm CMP_AND : CMP_AU<0b010, simm12, "cmp.and", Type_010>;
defm CMP_OR  : CMP_AU<0b011, simm12, "cmp.or", Type_010>;


let Defs = [CARG] in {
class SETCCond_rr_Base<bits<3> Func, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type>:
    LinxV5Inst_B_Z5_ZDst<SrcROp.Type, Func, type, OPC_SetCommit, (outs), (ins GPRSrc:$SrcL, SrcROp:$SrcR), opcstr>;

class SETCCond_ri_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_F<type, OPC_SetCommit_I, Func, (outs), (ins GPRSrc:$SrcL, Imm:$Imm17), opcstr> {
  bits<17> Imm17;

  let Imm12 = Imm17{16-5};
  let Imm5 = Imm17{4-0};
  let AsmString = opcstr # "\t$SrcL, $Imm17";
}
}

multiclass SETCCond_rr_Base<bits<3> Func, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type> {
  def "" : SETCCond_rr_Base<Func, SrcROp, opcstr, type>, P2I<NAME, 1>;
  let isBranch = 1, isTerminator = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
  def _BR : Pseudo<(outs), (ins GPRSrc:$rs1, SrcROp:$rs2, simm26_lsb0_bnext:$Target), []>, P2I<NAME, 0>;
}

multiclass SETCCond_ri_Base<bits<3> Func, Operand Imm , string opcstr, LinxV5InstType type> {
  def "" : SETCCond_ri_Base<Func, Imm, opcstr, type>, P2I<NAME, 1>;
  let isBranch = 1, isTerminator = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
  def _BR : Pseudo<(outs), (ins GPRSrc:$rs1, Imm:$Imm12, simm26_lsb0_bnext:$Target), []>, P2I<NAME, 0>;
}

multiclass SETCCmp<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  defm ""   : SETCCond_rr_Base<Func, SrcRNONE, opcstr, type>;
  defm _SW  : SETCCond_rr_Base<Func, SrcRSW, opcstr, type>;
  defm _UW  : SETCCond_rr_Base<Func, SrcRUW, opcstr, type>;
  defm I    : SETCCond_ri_Base<Func, Imm, opcstr # "i", type>;
}

multiclass SETCArith<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  defm ""   : SETCCond_rr_Base<Func, SrcRNONE, opcstr, type>;
  defm _SW  : SETCCond_rr_Base<Func, SrcRSW, opcstr, type>;
  defm _UW  : SETCCond_rr_Base<Func, SrcRUW, opcstr, type>;
  defm _NOT : SETCCond_rr_Base<Func, SrcRNOT, opcstr, type>;
  defm I    : SETCCond_ri_Base<Func, Imm, opcstr # "i", type>;
}

defm SETC_EQ  : SETCCmp<0b000, simm12_shln, "setc.eq", Type_010>;
defm SETC_NE  : SETCCmp<0b001, simm12_shln, "setc.ne", Type_010>;
defm SETC_AND : SETCArith<0b010, simm12_shln, "setc.and", Type_010>;
defm SETC_OR  : SETCArith<0b011, simm12_shln, "setc.or", Type_010>;
defm SETC_LT  : SETCCmp<0b100, simm12_shln, "setc.lt", Type_010>;
defm SETC_GE  : SETCCmp<0b101, simm12_shln, "setc.ge", Type_010>;
defm SETC_LTU : SETCCmp<0b110, uimm12_shln, "setc.ltu", Type_010>;
defm SETC_GEU : SETCCmp<0b111, uimm12_shln, "setc.geu", Type_010>;

//===------------------------------------ Linx v0.50 LSU Instructions -------------------------------------===//

class Load_rrr<bits<3> Func, LinxV5InstOpcode Opcode, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type> :
    LinxV5Inst_B<SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr> {
  let AsmString = opcstr # "\t[$SrcL, $SrcR" # "$Imm5], $DstR";
  let mayLoad = 1;
}

class Load_rri<bits<3> Func, LinxV5InstOpcode Opcode, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_E<type, Opcode, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm12), opcstr> {
  let AsmString = opcstr # "\t[$SrcL, $Imm12], $DstR";
  let mayLoad = 1;
}

multiclass Load_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  def ""     : Load_rrr<Func, OPC_Load_Register_Offset, SrcRNONE, opcstr, type>;
  def "_SW"  : Load_rrr<Func, OPC_Load_Register_Offset, SrcRSW, opcstr, type>;
  def "_UW"  : Load_rrr<Func, OPC_Load_Register_Offset, SrcRUW, opcstr, type>;
  def "I"    : Load_rri<Func, OPC_Load_Immediate_Offset, Imm, opcstr#"i", type>;
}

multiclass Load_Base_With_UnScaled<bits<3> Func, Operand ScaleImm, string opcstr, LinxV5InstType type> {
  defm ""    : Load_Base<Func, ScaleImm, opcstr, type>;
  def "I_U"  : Load_rri<Func, OPC_Load_Unscaled, simm12, opcstr#"i.u", type> {
    let Format = InstFormat_LoadI_UnScaled;
  }
}

def LB : Load_rrr<0b000, OPC_Load_Register_Offset, SrcRNONE, "lb", Type_100>;
def LB_SW : Load_rrr<0b000, OPC_Load_Register_Offset, SrcRSW, "lb", Type_100>;
def LB_UW : Load_rrr<0b000, OPC_Load_Register_Offset, SrcRUW, "lb", Type_100>;
def LBI : Load_rri<0b000, OPC_Load_Immediate_Offset, simm12, "lbi", Type_100> {
  let Format = InstFormat_LoadI_UnScaled;
}
def LBU : Load_rrr<0b100, OPC_Load_Register_Offset, SrcRNONE, "lbu", Type_100>;
def LBU_SW : Load_rrr<0b100, OPC_Load_Register_Offset, SrcRSW, "lbu", Type_100>;
def LBU_UW : Load_rrr<0b100, OPC_Load_Register_Offset, SrcRUW, "lbu", Type_100>;
def LBUI : Load_rri<0b100, OPC_Load_Immediate_Offset, simm12, "lbui", Type_100> {
  let Format = InstFormat_LoadI_UnScaled;
}
defm LH : Load_Base_With_UnScaled<0b001,  simm_pshl<12, 1>, "lh", Type_100>;
defm LW : Load_Base_With_UnScaled<0b010,  simm_pshl<12, 2>, "lw", Type_100>;
defm LD : Load_Base_With_UnScaled<0b011,  simm_pshl<12, 3>, "ld", Type_100>;
defm LHU : Load_Base_With_UnScaled<0b101, simm_pshl<12, 1>, "lhu", Type_100>;
defm LWU : Load_Base_With_UnScaled<0b110, simm_pshl<12, 2>, "lwu", Type_100>;

class Store_rrr<bits<3> Func, LinxV5InstOpcode Opcode, RegWithSrcRType SrcROp, string opcstr, string explicit_lsl, LinxV5InstType type> :
    LinxV5Inst_A<SrcROp.Type, Func, type, Opcode, (outs ), (ins GPRSrc:$SrcD, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr> {
  let DstR = 0b00000;
  let AsmString = opcstr # "\t$SrcD, [$SrcL, ${SrcR}"#explicit_lsl#"]";
  let mayStore = 1;
}

multiclass Store_Base<bits<3> Func, LinxV5InstOpcode Opcode, string opcstr, string explicit_lsl, LinxV5InstType type> {
  def ""      : Store_rrr<Func, Opcode, SrcRNONE, opcstr, explicit_lsl, type>;
  def "_SW"   : Store_rrr<Func, Opcode, SrcRSW, opcstr, explicit_lsl, type>;
  def "_UW"   : Store_rrr<Func, Opcode, SrcRUW, opcstr, explicit_lsl, type>;
}

class Load_Symbol<bits<3> Func, string opcstr>
    : LinxV5Inst<(outs DstRWithArrow:$DstR), (ins simm17:$Imm17), opcstr, ""> {
  bits<5> DstR;
  bits<17> Imm17;
  let Format = InstFormat_Load_Symbol;
  let Inst{31-15} = Imm17;
  let Inst{14-12} = Func;
  let Inst{11-7}  = DstR;
  let Inst{6-4}   = OPC_load_symbol.Value;
  let Inst{3-1}   = Type_100.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t[$Imm17], $DstR";
}

def LB_PCR : Load_Symbol<0b000, "lb.pcr">;
def LH_PCR : Load_Symbol<0b001, "lh.pcr">;
def LW_PCR : Load_Symbol<0b010, "lw.pcr">;
def LD_PCR : Load_Symbol<0b011, "ld.pcr">;
def LBU_PCR : Load_Symbol<0b100, "lbu.pcr">;
def LHU_PCR : Load_Symbol<0b101, "lhu.pcr">;
def LWU_PCR : Load_Symbol<0b110, "lwu.pcr">;


class Store_Symbol<bits<3> Func, string opcstr>
    : LinxV5Inst<(outs ), (ins GPRSrc:$SrcL, simm17:$Imm17), opcstr, ""> {
  bits<5> SrcL;
  bits<17> Imm17;
  let Format = InstFormat_Store_Symbol;
  let Inst{31-20} = Imm17{11-0};
  let Inst{19-15} = SrcL;
  let Inst{14-12}  = Func;
  let Inst{11-7} = Imm17{16-12};
  let Inst{6-4}   = OPC_store_symbol.Value;
  let Inst{3-1}   = Type_100.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, [$Imm17]";
}

def SB_PCR : Store_Symbol<0b000, "sb.pcr">;
def SH_PCR : Store_Symbol<0b001, "sh.pcr">;
def SW_PCR : Store_Symbol<0b010, "sw.pcr">;
def SD_PCR : Store_Symbol<0b011, "sd.pcr">;



defm SB : Store_Base<0b000, OPC_Store, "sb", "", Type_100>;
defm SH : Store_Base<0b001, OPC_Store, "sh", "<<1", Type_100>;
defm SW : Store_Base<0b010, OPC_Store, "sw", "<<2", Type_100>;
defm SD : Store_Base<0b011, OPC_Store, "sd", "<<3", Type_100>;
defm SH_U : Store_Base<0b101, OPC_Store, "sh.u", "", Type_100>;
defm SW_U : Store_Base<0b110, OPC_Store, "sw.u", "", Type_100>;
defm SD_U : Store_Base<0b111, OPC_Store, "sd.u", "", Type_100>;

class Store_rri<bits<3> Func, Operand Imm, string opcstr, InstFormat format, LinxV5InstType type> :
    LinxV5Inst_D<Func, type, OPC_Store_I, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, Imm:$Imm12), opcstr> {
  let AsmString = opcstr # "\t$SrcL, [$SrcR, $Imm12]";
  let mayStore = 1;
  let Format = format;
}

def SHI : Store_rri<0b001,   simm_pshl<12, 1>, "shi", InstFormatDefault, Type_100>;
def SWI : Store_rri<0b010,   simm_pshl<12, 2>, "swi", InstFormatDefault, Type_100>;
def SDI : Store_rri<0b011,   simm_pshl<12, 3>, "sdi", InstFormatDefault, Type_100>;
def SBI : Store_rri<0b000,   simm12,           "sbi", InstFormat_StoreI_UnScaled, Type_100>;
def SHI_U : Store_rri<0b101, simm12,           "shi.u", InstFormat_StoreI_UnScaled, Type_100>;
def SWI_U : Store_rri<0b110, simm12,           "swi.u", InstFormat_StoreI_UnScaled, Type_100>;
def SDI_U : Store_rri<0b111, simm12,           "sdi.u", InstFormat_StoreI_UnScaled, Type_100>;

def simm20_lui : SImm20Operand {
  let ParserMatchClass = SImmAsmOperand<20, "LUI">;
}
def simm20_addtpc : SImm20Operand {
  let ParserMatchClass = SImmAsmOperand<20, "ADDTPC">;
}

def simm32_addtpc : SImm32Operand {
  let ParserMatchClass = SImmAsmOperand<32, "HLADDTPC">;
}

def ADDTPC : LinxV5Inst_I<Type_011,0b000,(outs DstRWithArrowNoRA:$DstR),(ins simm20_addtpc:$imm20), "addtpc">;
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def LUI : LinxV5Inst_I<Type_011, 0b001,(outs DstRWithArrow:$DstR),(ins simm20_lui:$imm20), "lui">;

//===------------------------------------- Start Linx040 Three Source Integer Instructions Define -------------------------------------===//

class 3SrcInt_Base<bits<3> Func, string opcstr, RegWithSrcRType SrcROp, LinxV5InstType type> :
    LinxV5Inst_A<SrcROp.Type, Func, type, OPC_3SrcInt_Operation, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR, GPRSrc:$SrcP), opcstr> {
    bits<5> SrcP;
    let Inst{31-27} = SrcP;
    let AsmString = opcstr # "\t$SrcP, $SrcL, $SrcR, $DstR";
}

multiclass CSEL_3Src<bits<3> Func, string opcstr, LinxV5InstType type>  {
    def ""   : 3SrcInt_Base<Func, opcstr, SrcRNONE, type>;
    def _NEG : 3SrcInt_Base<Func, opcstr, SrcRNEG, type>;
}

defm CSEL : CSEL_3Src<0b000, "csel", Type_011>;

//===------------------------------------- Start LinxV5 Compress Instruction Define -------------------------------------===//

def MOVR : LinxV5CInst_D<0b011, 0b00, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL), "c.movr">;

def MOVI : LinxV5CInst_E<0b011, 0b01,(outs DstRWithArrowNoRA:$DstR), (ins simm5:$Imm), "c.movi">;

class LinxV5CInst_A_ALU<bits<3> Opcode, bits<2> flag, dag outs, dag ins, string opcodestr>
    : LinxV5CInst_A<Opcode, flag, outs, ins, opcodestr> {
  let AsmString = opcodestr # "\t$SrcL, $SrcR, ->t";
}

let Defs = [T] in {
def C_ADD : LinxV5CInst_A_ALU<0b100, 0b00, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.add">;
def C_SUB : LinxV5CInst_A_ALU<0b100, 0b01, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.sub">;
def C_AND : LinxV5CInst_A_ALU<0b100, 0b10, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.and">;
def C_OR  : LinxV5CInst_A_ALU<0b100, 0b11, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.or">;
} // end let Defs = [T]

def C_SETC_EQ : LinxV5CInst_A<0b011, 0b10, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.setc.eq">;
def C_SETC_NE : LinxV5CInst_A<0b011, 0b11, (outs ), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), "c.setc.ne">;

class LinxV5CInst_LOADi<bits<3> Opcode, bits<2> flag, Operand ScaleImm, string opcstr>
    : LinxV5CInst_B<Opcode, flag, (outs ), (ins GPRSrc:$SrcL, ScaleImm:$Imm), opcstr> {
  let Defs = [T];
  let Uses = [TOS1];
  let AsmString = opcstr # "\t[$SrcL, $Imm], ->t";
}

class LinxV5CInst_STOREi<bits<3> Opcode, bits<2> flag, Operand ScaleImm, string opcstr>
    : LinxV5CInst_B<Opcode, flag, (outs ), (ins GPRSrc:$SrcL, ScaleImm:$Imm), opcstr> {
  let Uses = [TOS1];
  let AsmString = opcstr # "\tt#1, [$SrcL, $Imm]";
}

let mayLoad = 1 in {
def C_LWI : LinxV5CInst_LOADi<0b101, 0b00, simm_pshl<5, 2>, "c.lwi">;
def C_LDI : LinxV5CInst_LOADi<0b101, 0b01, simm_pshl<5, 3>, "c.ldi">;
}
let mayStore = 1 in {
def C_SWI : LinxV5CInst_STOREi<0b101, 0b10, simm_pshl<5, 2>, "c.swi">;
def C_SDI : LinxV5CInst_STOREi<0b101, 0b11, simm_pshl<5, 3>, "c.sdi">;
}

def C_ADDI : LinxV5CInst_B<0b110, 0b00, (outs ), (ins GPRSrc:$SrcL, simm5:$Imm), "c.addi"> {
  let Defs = [T];
  let AsmString = "c.addi\t$SrcL, $Imm, ->t";
}

let hasSideEffects = 0, Defs = [CARG] in
def C_SETC_TGT : LinxV5CInst_C<0b110, 0b00000, (outs ), (ins GPRSrc:$SrcL), "c.setc.tgt">;

class LinxV5CInst_EXT<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst_C<Opcode, Func, outs, ins, opcodestr> {
  let Defs = [T];
  let AsmString = opcodestr # "\t$SrcL, ->t";
}

def SEXT_B : LinxV5CInst_EXT<0b110, 0b01000, (outs ), (ins GPRSrc:$SrcL), "c.sext.b">;
def SEXT_H : LinxV5CInst_EXT<0b110, 0b01001, (outs ), (ins GPRSrc:$SrcL), "c.sext.h">;
def SEXT_W : LinxV5CInst_EXT<0b110, 0b01010, (outs ), (ins GPRSrc:$SrcL), "c.sext.w">;
def ZEXT_B : LinxV5CInst_EXT<0b110, 0b01011, (outs ), (ins GPRSrc:$SrcL), "c.zext.b">;
def ZEXT_H : LinxV5CInst_EXT<0b110, 0b01100, (outs ), (ins GPRSrc:$SrcL), "c.zext.h">;
def ZEXT_W : LinxV5CInst_EXT<0b110, 0b01101, (outs ), (ins GPRSrc:$SrcL), "c.zext.w">;

class LinxV5CInst_CMPI<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst_F<Opcode, Func, outs, ins, opcodestr> {
  let Defs = [T];
  let Uses = [TOS1];
  let AsmString = opcodestr # "\tt#1, $Imm, ->t";
}

class LinxV5CInst_SHIFTI<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst_F<Opcode, Func, outs, ins, opcodestr> {
  let Defs = [T];
  let Uses = [TOS1];
  let AsmString = opcodestr # "\tt#1, $Imm, ->t";
}

def C_CMP_EQI : LinxV5CInst_CMPI<0b110, 0b00000, (outs ), (ins simm5:$Imm), "c.cmp.eqi">;
def C_CMP_NEI : LinxV5CInst_CMPI<0b110, 0b00001, (outs ), (ins simm5:$Imm), "c.cmp.nei">;
def C_SLLI : LinxV5CInst_SHIFTI<0b110, 0b00010, (outs ), (ins uimm5:$Imm), "c.slli">;
def C_SRLI : LinxV5CInst_SHIFTI<0b110, 0b00011, (outs ), (ins uimm5:$Imm), "c.srli">;

class LinxV5CInst_SSRGET<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst_F<Opcode, Func, outs, ins, opcodestr> {
  let Defs = [T];
  let AsmString = opcodestr # "\t$Imm, ->t";
}

def C_SSRGET : LinxV5CInst_SSRGET<0b110, 0b10000, (outs ), (ins uimm5:$Imm), "c.ssrget">;

def C_EBREAK : LinxV5CInst_F<0b110, 0b11000, (outs ), (ins uimm5:$Imm), "C.EBREAK"> {
  let AsmString = "C.EBREAK" # "\t$Imm";
}

//===------------------------------------- End LinxV5 Compress Instruction Define -------------------------------------===//

//===------------------------------------- Start Linx040 Floating-point Operation Define -----------------------------===//
let Format = InstFormatFP in
class Float_3Src_Base<bits<3> Func, bits<2> SrcType, string opcstr> :
     LinxV5Inst_A<SrcType, Func, Type_101, OPC_3Src_Floating_Point, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcA, GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
      PEType PE = PET_FP;
      bits<5> SrcA;
      let Inst{31-27} = SrcA;
     let AsmString = opcstr # "\t$SrcL, $SrcR, $SrcA, $DstR";
}

let Format = InstFormatFP in
class Float_3Src_Two<bits<3> Func, bits<2> SrcType, string opcstr> :
     LinxV5Inst_A<SrcType, Func, Type_101, OPC_3Src_Floating_Point, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
     PEType PE = PET_FP;
     let SrcD = 0b00000;
     let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

def FADD_FD : Float_3Src_Two<0b000, 0b00, "fadd.fd">;
def FADD_FS : Float_3Src_Two<0b000, 0b01, "fadd.fs">;
def FADD_FH : Float_3Src_Two<0b000, 0b10, "fadd.fh">;
def FADD_FB : Float_3Src_Two<0b000, 0b11, "fadd.fb">;
def FSUB_FD : Float_3Src_Two<0b001, 0b00, "fsub.fd">;
def FSUB_FS : Float_3Src_Two<0b001, 0b01, "fsub.fs">;
def FSUB_FH : Float_3Src_Two<0b001, 0b10, "fsub.fh">;
def FSUB_FB : Float_3Src_Two<0b001, 0b11, "fsub.fb">;
def FMUL_FD : Float_3Src_Two<0b010, 0b00, "fmul.fd">;
def FMUL_FS : Float_3Src_Two<0b010, 0b01, "fmul.fs">;
def FMUL_FH : Float_3Src_Two<0b010, 0b10, "fmul.fh">;
def FMUL_FB : Float_3Src_Two<0b010, 0b11, "fmul.fb">;
def FDIV_FD : Float_3Src_Two<0b011, 0b00, "fdiv.fd">;
def FDIV_FS : Float_3Src_Two<0b011, 0b01, "fdiv.fs">;
def FDIV_FH : Float_3Src_Two<0b011, 0b10, "fdiv.fh">;
def FDIV_FB : Float_3Src_Two<0b011, 0b11, "fdiv.fb">;

def FMADD_FD : Float_3Src_Base<0b100, 0b00, "fmadd.fd">;
def FMADD_FS : Float_3Src_Base<0b100, 0b01, "fmadd.fs">;
def FMADD_FH : Float_3Src_Base<0b100, 0b10, "fmadd.fh">;
def FMADD_FB : Float_3Src_Base<0b100, 0b11, "fmadd.fb">;
def FMSUB_FD : Float_3Src_Base<0b101, 0b00, "fmsub.fd">;
def FMSUB_FS : Float_3Src_Base<0b101, 0b01, "fmsub.fs">;
def FMSUB_FH : Float_3Src_Base<0b101, 0b10, "fmsub.fh">;
def FMSUB_FB : Float_3Src_Base<0b101, 0b11, "fmsub.fb">;
def FNMADD_FD: Float_3Src_Base<0b110, 0b00, "fnmadd.fd">;
def FNMADD_FS: Float_3Src_Base<0b110, 0b01, "fnmadd.fs">;
def FNMADD_FH: Float_3Src_Base<0b110, 0b10, "fnmadd.fh">;
def FNMADD_FB: Float_3Src_Base<0b110, 0b11, "fnmadd.fb">;
def FNMSUB_FD: Float_3Src_Base<0b111, 0b00, "fnmsub.fd">;
def FNMSUB_FS: Float_3Src_Base<0b111, 0b01, "fnmsub.fs">;
def FNMSUB_FH: Float_3Src_Base<0b111, 0b10, "fnmsub.fh">;
def FNMSUB_FB: Float_3Src_Base<0b111, 0b11, "fnmsub.fb">;

let Format = InstFormatFP in
class Float_2Src_Compare<bits<3> Func, bits<2> SrcType, string opcstr> :
     LinxV5Inst_A<SrcType, Func, Type_101, OPC_2Src_Floating_Point, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
     PEType PE = PET_FP;
     let SrcD = 0b00000;
     let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

let Format = InstFormatFP in
class Float_2Src_Compare_S<bits<3> Func, bits<2> SrcType, string opcstr> :
     LinxV5Inst_A<SrcType, Func, Type_101, OPC_2Src_Floating_Point, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
     PEType PE = PET_FP;
     let SrcD = 0b00001;
     let AsmString = opcstr # "\t$SrcL, $SrcR, $DstR";
}

def FEQ_FD: Float_2Src_Compare<0b000, 0b00, "feq.fd">;
def FEQ_FS: Float_2Src_Compare<0b000, 0b01, "feq.fs">;
def FEQ_FH: Float_2Src_Compare<0b000, 0b10, "feq.fh">;
def FEQ_FB: Float_2Src_Compare<0b000, 0b11, "feq.fb">;
def FNE_FD: Float_2Src_Compare<0b001, 0b00, "fne.fd">;
def FNE_FS: Float_2Src_Compare<0b001, 0b01, "fne.fs">;
def FNE_FH: Float_2Src_Compare<0b001, 0b10, "fne.fh">;
def FNE_FB: Float_2Src_Compare<0b001, 0b11, "fne.fb">;
def FLT_FD: Float_2Src_Compare<0b010, 0b00, "flt.fd">;
def FLT_FS: Float_2Src_Compare<0b010, 0b01, "flt.fs">;
def FLT_FH: Float_2Src_Compare<0b010, 0b10, "flt.fh">;
def FLT_FB: Float_2Src_Compare<0b010, 0b11, "flt.fb">;
def FGE_FD: Float_2Src_Compare<0b011, 0b00, "fge.fd">;
def FGE_FS: Float_2Src_Compare<0b011, 0b01, "fge.fs">;
def FGE_FH: Float_2Src_Compare<0b011, 0b10, "fge.fh">;
def FGE_FB: Float_2Src_Compare<0b011, 0b11, "fge.fb">;

def FEQS_FD: Float_2Src_Compare_S<0b000, 0b00, "feqs.fd">;
def FEQS_FS: Float_2Src_Compare_S<0b000, 0b01, "feqs.fs">;
def FEQS_FH: Float_2Src_Compare_S<0b000, 0b10, "feqs.fh">;
def FEQS_FB: Float_2Src_Compare_S<0b000, 0b11, "feqs.fb">;
def FNES_FD: Float_2Src_Compare_S<0b001, 0b00, "fnes.fd">;
def FNES_FS: Float_2Src_Compare_S<0b001, 0b01, "fnes.fs">;
def FNES_FH: Float_2Src_Compare_S<0b001, 0b10, "fnes.fh">;
def FNES_FB: Float_2Src_Compare_S<0b001, 0b11, "fnes.fb">;
def FLTS_FD: Float_2Src_Compare_S<0b010, 0b00, "flts.fd">;
def FLTS_FS: Float_2Src_Compare_S<0b010, 0b01, "flts.fs">;
def FLTS_FH: Float_2Src_Compare_S<0b010, 0b10, "flts.fh">;
def FLTS_FB: Float_2Src_Compare_S<0b010, 0b11, "flts.fb">;
def FGES_FD: Float_2Src_Compare_S<0b011, 0b00, "fges.fd">;
def FGES_FS: Float_2Src_Compare_S<0b011, 0b01, "fges.fs">;
def FGES_FH: Float_2Src_Compare_S<0b011, 0b10, "fges.fh">;
def FGES_FB: Float_2Src_Compare_S<0b011, 0b11, "fges.fb">;

let Format = InstFormatFP in
class fALGO_Base<bits<3> Func, bits<2> SrcType, string opcstr>
    : LinxV5Inst_A<SrcType, Func, Type_101, OPC_FloatAlgorithm, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL), ""> {
  let SrcD = 0b00000;
  let SrcR = 0b00000;
  let AsmString = opcstr # "\t$SrcL, $DstR";
}

multiclass fALGO<bits<3> Func, string opcstr> {
def _FD : fALGO_Base<Func, 0b00, opcstr # ".fd">;
def _FS : fALGO_Base<Func, 0b01, opcstr # ".fs">;
def _FH : fALGO_Base<Func, 0b10, opcstr # ".fh">;
def _FB : fALGO_Base<Func, 0b11, opcstr # ".fb">;
}

defm FABS   : fALGO<0b000, "fabs">;
defm FSQRT  : fALGO<0b001, "fsqrt">;
defm FRECIP : fALGO<0b010, "frecip">;
defm FEXP : fALGO<0b011, "fexp">;

//===------------------------------------- End Linx040 Floating-point Operation Define -----------------------------===//

//===------------------------------------- Start Linx040 Convert Operation Define -----------------------------===//

let Format = InstFormatFP in
class Convert_Base<bits<2> SrcType, bits<5> DstType, bits<3> Func, string opcstr> :
     LinxV5Inst_F_Convert<SrcType, DstType, Func, Type_101, OPC_Convert, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL), opcstr> {
     let AsmString = opcstr # "\t$SrcL, $DstR";
}

multiclass Convert_DST_Int<bits<2> SrcType, bits<3> Func, string opcstr> {
  def "2UD" : Convert_Base<SrcType, 0b00000, Func, opcstr # "2ud">;
  def "2UW" : Convert_Base<SrcType, 0b00001, Func, opcstr # "2uw">;
  def "2UH" : Convert_Base<SrcType, 0b00010, Func, opcstr # "2uh">;
  def "2UB" : Convert_Base<SrcType, 0b00011, Func, opcstr # "2ub">;
  def "2SD" : Convert_Base<SrcType, 0b00100, Func, opcstr # "2sd">;
  def "2SW" : Convert_Base<SrcType, 0b00101, Func, opcstr # "2sw">;
  def "2SH" : Convert_Base<SrcType, 0b00110, Func, opcstr # "2sh">;
  def "2SB" : Convert_Base<SrcType, 0b00111, Func, opcstr # "2sb">;
}

multiclass Convert_DST_Float<bits<2> SrcType, bits<3> Func, string opcstr> {
  def "2FD" : Convert_Base<SrcType, 0b00000, Func, opcstr # "2fd">;
  def "2FS" : Convert_Base<SrcType, 0b00001, Func, opcstr # "2fs">;
  def "2FH" : Convert_Base<SrcType, 0b00010, Func, opcstr # "2fh">;
  def "2FB" : Convert_Base<SrcType, 0b00011, Func, opcstr # "2fb">;
}

multiclass Convert_F_2_F<bits<3> Func, string opcstr> {
  defm "_FD" : Convert_DST_Float<0b00, Func, opcstr # ".fd">;
  defm "_FS" : Convert_DST_Float<0b01, Func, opcstr # ".fs">;
  defm "_FH" : Convert_DST_Float<0b10, Func, opcstr # ".fh">;
  defm "_FB" : Convert_DST_Float<0b11, Func, opcstr # ".fb">;
}

multiclass Convert_F_2_I<bits<3> Func, string opcstr> {
  defm "_FD" : Convert_DST_Int<0b00, Func, opcstr # ".fd">;
  defm "_FS" : Convert_DST_Int<0b01, Func, opcstr # ".fs">;
  defm "_FH" : Convert_DST_Int<0b10, Func, opcstr # ".fh">;
  defm "_FB" : Convert_DST_Int<0b11, Func, opcstr # ".fb">;
}

multiclass Convert_S_2_F<string opcstr> {
  defm "_SD" : Convert_DST_Float<0b00, 0b110, opcstr # ".sd">;
  defm "_SW" : Convert_DST_Float<0b01, 0b110, opcstr # ".sw">;
  defm "_SH" : Convert_DST_Float<0b10, 0b110, opcstr # ".sh">;
  defm "_SB" : Convert_DST_Float<0b11, 0b110, opcstr # ".sb">;
}

multiclass Convert_U_2_F<string opcstr> {
  defm "_UD" : Convert_DST_Float<0b00, 0b111, opcstr # ".ud">;
  defm "_UW" : Convert_DST_Float<0b01, 0b111, opcstr # ".uw">;
  defm "_UH" : Convert_DST_Float<0b10, 0b111, opcstr # ".uh">;
  defm "_UB" : Convert_DST_Float<0b11, 0b111, opcstr # ".ub">;
}

defm FCVT : Convert_F_2_F<0b000, "fcvt">;
defm FCVTA : Convert_F_2_I<0b001, "fcvta">;
defm FCVTM : Convert_F_2_I<0b010, "fcvtm">;
defm FCVTN : Convert_F_2_I<0b011, "fcvtn">;
defm FCVTP : Convert_F_2_I<0b100, "fcvtp">;
defm FCVTZ : Convert_F_2_I<0b101, "fcvtz">;
defm SCVTF : Convert_S_2_F<"scvtf">;
defm UCVTF : Convert_U_2_F<"ucvtf">;

//===------------------------------------- End Linx040 Convert Operation Define -----------------------------===//

//===------------------------------------- Start Linx040 Pseudo Instructions Define -------------------------------------===//

// Pseudo instructions all have their own print method
// Thus don't need to convert into real v0.30 instructions
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
def PseudoADDTPC_HI : Pseudo<(outs DstRWithArrowNoRA:$DstR), (ins linxbaresymbol:$label, linxbaresymbol:$addrhi), []> {
  let AsmString = "$label" # ":\n " # "addtpc\t$addrhi, $DstR";
  let isNotDuplicable = 1;
}
}

// define pseudo terminators
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isTerminator = 1 in {
// define pseudo branches
let Uses = [CARG], isBranch = 1 in {
def PseudoBRCondCARG : Pseudo<(outs), (ins simm26_lsb0_bnext:$Target), []>;

let isBarrier = 1, isIndirectBranch = 1 in
def PseudoBRIndCARG : Pseudo<(outs), (ins), []>;
}

// define pseudo indirect calls
let isCall = 1, isBarrier = 1, isReturn = 1 in {
let Uses = [CARG, R1] in
def PseudoTAILIndCARG : Pseudo<(outs), (ins), []>;

let Uses = [R1] in
def PseudoTAILInd : Pseudo<(outs), (ins GPRSrc:$rs1), []>;
}

let isCall = 1 in {
let Uses = [CARG], Defs = [R10] in
def PseudoCALLIndCARG : Pseudo<(outs), (ins), []>;

let Defs = [R10] in
def PseudoCALLInd : Pseudo<(outs), (ins GPRSrc:$rs1), []>;
}

let isCall = 1, Uses = [SIMT_LB0, SIMT_LB1, SIMT_LB2] in {
def PseudoFunctionBlock : Pseudo<(outs), (ins call_symbol:$func), []>;
}

// define pseudo ret
let isBarrier = 1, isReturn = 1, Uses = [CARG] in
def PseudoRETCARG : Pseudo<(outs), (ins), []>;
}

def : Pat<(linxv5_call tglobaladdr:$func), (PseudoCALL tglobaladdr:$func)>;
def : Pat<(linxv5_call texternalsym:$func), (PseudoCALL texternalsym:$func)>;
def : Pat<(linxv5_call i64:$rs1), (PseudoCALLInd i64:$rs1)>;
def : Pat<(linxv5_fblock tglobaladdr:$func), (PseudoFunctionBlock tglobaladdr:$func)>;

def : Pat<(linxv5_tail (iPTR tglobaladdr:$dst)), (PseudoTAIL texternalsym:$dst)>;
def : Pat<(linxv5_tail (iPTR texternalsym:$dst)), (PseudoTAIL texternalsym:$dst)>;
def : Pat<(linxv5_tail i64:$rs1), (PseudoTAILInd i64:$rs1)>;

//===------------------------------------- End Linx040 Pseudo Instructions Define -------------------------------------===//

//===------------------------------------- Linx040 Header Inst Define -------------------------------------===//

def BStartWithoutTargetBrTypeAsmOperand : AsmOperandClass {
  let Name = "BStartWithoutTargetBrType";
  let ParserMethod = "parseBStartWithoutTargetBrType";
  let RenderMethod = "addImmOperands";
}

def BStartWithoutTargetBrType : Operand<OtherVT> {
  let ParserMatchClass = BStartWithoutTargetBrTypeAsmOperand;
  let PrintMethod = "printBStartWithoutTargetBrType";
  let DecoderMethod = "decodeUImmOperand<3>";
}

def BAttrTypeAsmOperand : AsmOperandClass {
  let Name = "BAttrType";
  let ParserMethod = "parseBAttrType";
  let RenderMethod = "addBAttrType";
}

def BAttrType : Operand<OtherVT> {
  let ParserMatchClass = BAttrTypeAsmOperand;
  let PrintMethod = "printBAttrType";
  let DecoderMethod = "decodeUImmOperand<8>";
}

def BArgFormatAsmOperand : AsmOperandClass {
  let Name = "BArgFormat";
  let ParserMethod = "parseBArgFormat";
  let RenderMethod = "addImmOperands";
}

def BArgFormat : Operand<i64> {
  let ParserMatchClass = BArgFormatAsmOperand;
  let PrintMethod = "printBArgFormat";
  let DecoderMethod = "decodeBArgFormat";
}

// B.SUBVIEW SubviewSizeCode: 4-bit, legal 1..12 (128 B..256 KiB per PE);
// 0 and 13..15 are reserved (`BundleRangeSubviewRawLegal`).
def SubviewSizeCodeAsmOperand : AsmOperandClass {
  let Name = "SubviewSizeCode";
  let RenderMethod = "addImmOperands";
}
def SubviewSizeCode : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm) && Imm >= 1 && Imm <= 12;}]> {
  let ParserMatchClass = SubviewSizeCodeAsmOperand;
  let PrintMethod = "printSubviewSizeCode";
  let DecoderMethod = "decodeSUBVIEWSizeCode";
}

// B.ASSEMBLE ParentSizeCode: 4-bit, legal 0..12; 13..15 reserved. Size
// code 0 is only legal on a non-INIT (MIDDLE/LAST) modifier and never on
// INIT (checked by the assembler predicate and the decoder).
def ParentSizeCodeAsmOperand : AsmOperandClass {
  let Name = "ParentSizeCode";
  let RenderMethod = "addImmOperands";
}
def ParentSizeCode : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm) && Imm <= 12;}]> {
  let ParserMatchClass = ParentSizeCodeAsmOperand;
  let PrintMethod = "printParentSizeCode";
  let DecoderMethod = "decodeParentSizeCode";
}

def RModeAsmOperand : AsmOperandClass {
  let Name = "RMode";
  let ParserMethod = "parseRMode";
  let RenderMethod = "addImmOperands";
}

def RMode : Operand<i64> {
  let ParserMatchClass = RModeAsmOperand;
  let PrintMethod = "printRMode";
  let DecoderMethod = "decodeUImmOperand<3>";
}

def CanonAsmOperand : AsmOperandClass {
  let Name = "Canon";
  let ParserMethod = "parseCanon";
  let RenderMethod = "addCanonType";
}

def Canon : Operand<OtherVT> {
  let ParserMatchClass = CanonAsmOperand;
  let PrintMethod = "printCanon";
  let DecoderMethod = "decodeUImmOperand<1>";
}

def SatAsmOperand : AsmOperandClass {
  let Name = "Sat";
  let ParserMethod = "parseSat";
  let RenderMethod = "addSatType";
}

def Sat : Operand<i64> {
  let ParserMatchClass = SatAsmOperand;
  let PrintMethod = "printSat";
  let DecoderMethod = "decodeUImmOperand<1>";
}

def ByteIDAsmOperand : AsmOperandClass {
  let Name = "ByteID";
  let ParserMethod = "parseByteID";
  let RenderMethod = "addByteIDType";
}

def ByteID : Operand<i64> {
  let ParserMatchClass = ByteIDAsmOperand;
  let PrintMethod = "printByteID";
  let DecoderMethod = "decodeUImmOperand<2>";
}

def DRImmAsmOperand : AsmOperandClass {
  let Name = "DRImm";
  let ParserMethod = "parseDRImm";
  let RenderMethod = "addDRImmType";
}

def DRImm : Operand<OtherVT> {
  let ParserMatchClass = DRImmAsmOperand;
  let PrintMethod = "printDRImm";
  let DecoderMethod = "decodeUImmOperand<1>";
}

def PadValueAsmOperand : AsmOperandClass {
  let Name = "PadValue";
  let ParserMethod = "parsePadValue";
  let RenderMethod = "addPadValueType";
}

def PadValue : Operand<i64> {
  let ParserMatchClass = PadValueAsmOperand;
  let PrintMethod = "printPadValue";
  let DecoderMethod = "decodeUImmOperand<2>";
}

def CmpModeAsmOperand : AsmOperandClass {
  let Name = "CmpMode";
  let ParserMethod = "parseCmpMode";
  let RenderMethod = "addCmpModeType";
}

def CmpMode : Operand<i64> {
  let ParserMatchClass = CmpModeAsmOperand;
  let PrintMethod = "printCmpMode";
  let DecoderMethod = "decodeUImmOperand<3>";
}

def BstartDataTypeAsmOperand : AsmOperandClass {
  let Name = "BstartDataType";
  let ParserMethod = "parseBstartDataType";
  let RenderMethod = "addImmOperands";
}

def BstartDataType : Operand<OtherVT> {
  let ParserMatchClass = BstartDataTypeAsmOperand;
  let PrintMethod = "printBstartDataType";
  let DecoderMethod = "decodeUImmOperand<5>";
}

def TileOPTMAAsmOperand : AsmOperandClass {
  let Name = "TileOPTMA";
  let ParserMethod = "parseTileOPTMA";
  let RenderMethod = "addImmOperands";
}

def TileOPTMA : Operand<OtherVT> {
  let ParserMatchClass = TileOPTMAAsmOperand;
  let PrintMethod = "printTileOPTMA";
  let DecoderMethod = "decodeUImmOperand<5>";
}

def TileOPCUBEAsmOperand : AsmOperandClass {
  let Name = "TileOPCUBE";
  let ParserMethod = "parseTileOPCUBE";
  let RenderMethod = "addImmOperands";
}

def TileOPCUBE : Operand<OtherVT> {
  let ParserMatchClass = TileOPCUBEAsmOperand;
  let PrintMethod = "printTileOPCUBE";
  let DecoderMethod = "decodeUImmOperand<5>";
}

def TileOPTEPLAsmOperand : AsmOperandClass {
  let Name = "TileOPTEPL";
  let ParserMethod = "parseTileOPTEPL";
  let RenderMethod = "addImmOperands";
}

def TileOPTEPL : Operand<OtherVT> {
  let ParserMatchClass = TileOPTEPLAsmOperand;
  let PrintMethod = "printTileOPTEPL";
  let DecoderMethod = "decodeUImmOperand<7>";
}

def TileOPModeAsmOperand : AsmOperandClass {
  let Name = "TileOPMode";
  let ParserMethod = "parseTileOPMode";
  let RenderMethod = "addImmOperands";
}

def TileOPMode : Operand<OtherVT> {
  let ParserMatchClass = TileOPModeAsmOperand;
  let PrintMethod = "printTileOPMode";
  let DecoderMethod = "decodeUImmOperand<2>";
}

def TEPLModeAsmOperand : AsmOperandClass {
  let Name = "TEPLMode";
  let ParserMethod = "parseTEPLMode";
  let RenderMethod = "addImmOperands";
}

def TEPLMode : Operand<OtherVT> {
  let ParserMatchClass = TEPLModeAsmOperand;
  let PrintMethod = "printTEPLMode";
  let DecoderMethod = "decodeUImmOperand<2>";
}

def GPRBitMapAsmOperand : AsmOperandClass {
  let Name = "GPRBitMap";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseGPRBitMap";
}

def GroupOpAsmOperand : AsmOperandClass {
  let Name = "GroupOp";
  let ParserMethod = "parseGroupOp";
  let RenderMethod = "addGroupOp";
}

def GroupOp : Operand<OtherVT> {
  let ParserMatchClass = GroupOpAsmOperand;
  let PrintMethod = "printGroupOp";
  let DecoderMethod = "decodeUImmOperand<1>";
}

def GPRBitMap : Operand<OtherVT> {
  let ParserMatchClass = GPRBitMapAsmOperand;
  let PrintMethod = "printGPRBitMap";
  let DecoderMethod = "decodeUImmOperand<24>";
}

def KILLInst : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<24> RegMask;

  let OutOperandList = (outs );
  let InOperandList = (ins GPRBitMap:$RegMask);

  let Inst{31} = 0b0;
  let Inst{30-7} = RegMask;
  let Inst{6-3} = 0b1111;
  let Inst{2-1} = 0b10;
  let Inst{0} = 0b1;

  let AsmString = "kill" # "\t" # "[$RegMask]";
  let PE = PET_ALL;
}

class C_BSTART<bits<5> Kind, string opcstr> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_16;
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;

  bits<3> BrType;

  let Inst{15-14} = 0b00;
  let Inst{13-11} = BrType;
  let Inst{10-6} = Kind;
  let Inst{5-4} = 0b00;
  let Inst{3-0} = 0b0000;

  dag OutOperandList = (outs );
  dag InOperandList = (ins BStartWithoutTargetBrType:$BrType);

  let AsmString =  opcstr # "\t$BrType";
}

class C_BSTART_PAR<bits<2> Kind, string opcstr> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_16;
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;

  let Inst{15-14} = Kind;
  let Inst{13-11} = 0b001;
  let Inst{10-6} = 0b00011;
  let Inst{5-4} = 0b00;
  let Inst{3-0} = 0b0000;

  dag OutOperandList = (outs );
  dag InOperandList = (ins );

  let AsmString =  opcstr;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

def BSTART_STD_WITHOUT_TARGET_16 : C_BSTART<0b00000, "C.BSTART.STD">;  // 16bits c.bstart.std <fall/icall/ind/ret>
def BSTART_AUX_WITHOUT_TARGET_16 : C_BSTART<0b00001, "C.BSTART.AUX">;
def BSTART_FP_WITHOUT_TARGET_16 : C_BSTART<0b00010, "C.BSTART.FP">;

def C_BSTART_MPAR : C_BSTART_PAR<0b00, "C.BSTART.MPAR">;
def C_BSTART_MSEQ : C_BSTART_PAR<0b01, "C.BSTART.MSEQ">;
def C_BSTART_VPAR : C_BSTART_PAR<0b10, "C.BSTART.VPAR">;
def C_BSTART_VSEQ : C_BSTART_PAR<0b11, "C.BSTART.VSEQ">;

class BSTART_WITHOUT_TARGET_32<bits<3> Func, bits<5> BlockType, string HeadType, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_32;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins );

  let Inst{31-15} = 0b00000000000000000;
  let Inst{14-12} = Func;
  let Inst{11-7} = BlockType;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t" # BrType;
}

def EBREAK : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<4> imm4;

  let Inst{31-28} = 0b0000;
  let Inst{27-24} = imm4;
  let Inst{23-20} = 0b0001;
  let Inst{19-15} = 0b00000;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00000;
  let Inst{6-4} = 0b010;
  let Inst{3-1} = 0b101;
  let Inst{0} = 0b1;

  dag OutOperandList = (outs );
  dag InOperandList = (ins uimm4:$imm4);

  let AsmString = "EBREAK\t$imm4";
}

def BSTART_STD_WITHOUT_TARGET_32_FALL : BSTART_WITHOUT_TARGET_32<0b001, 0b00000, ".STD ", "FALL">;
def BSTART_STD_WITHOUT_TARGET_32_RET : BSTART_WITHOUT_TARGET_32<0b111, 0b00000, ".STD ", "RET">;
def BSTART_STD_WITHOUT_TARGET_32_IND : BSTART_WITHOUT_TARGET_32<0b101, 0b00000, ".STD ", "IND">;
def BSTART_STD_WITHOUT_TARGET_32_ICALL : BSTART_WITHOUT_TARGET_32<0b110, 0b00000, ".STD ", "ICALL">;
def BSTART_AUX_WITHOUT_TARGET_32_FALL : BSTART_WITHOUT_TARGET_32<0b001, 0b00001, ".AUX ", "FALL">;
def BSTART_AUX_WITHOUT_TARGET_32_RET : BSTART_WITHOUT_TARGET_32<0b111, 0b00001, ".AUX ", "RET">;
def BSTART_AUX_WITHOUT_TARGET_32_IND : BSTART_WITHOUT_TARGET_32<0b101, 0b00001, ".AUX ", "IND">;
def BSTART_AUX_WITHOUT_TARGET_32_ICALL : BSTART_WITHOUT_TARGET_32<0b110, 0b00001, ".AUX ", "ICALL">;
def BSTART_FP_WITHOUT_TARGET_32_FALL : BSTART_WITHOUT_TARGET_32<0b001, 0b00010, ".FP ", "FALL">;
def BSTART_FP_WITHOUT_TARGET_32_RET : BSTART_WITHOUT_TARGET_32<0b111, 0b00010, ".FP ", "RET">;
def BSTART_FP_WITHOUT_TARGET_32_IND : BSTART_WITHOUT_TARGET_32<0b101, 0b00010, ".FP ", "IND">;
def BSTART_FP_WITHOUT_TARGET_32_ICALL : BSTART_WITHOUT_TARGET_32<0b110, 0b00010, ".FP ", "ICALL">;

class BSTART_WITH_TARGET_25<bits<3> Opc, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITH_TARGET_25;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<25> Src;

  let OutOperandList = (outs );
  let InOperandList = (ins simm26_lsb0_bnext:$Src);

  let Inst{31-7} = Src;
  let Inst{6-4} = Opc;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART.STD\t" # BrType # ", $Src";
}

def BSTART_STD_WITH_TARGET_25_DIRECT : BSTART_WITH_TARGET_25<0b001, "DIRECT">;
def BSTART_STD_WITH_TARGET_25_COND : BSTART_WITH_TARGET_25<0b010, "COND">;

class BSTART_WITH_TARGET_17<bits<3> Func, bits<5> BlockType, string HeadType, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITH_TARGET_17;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<17> Src;

  let OutOperandList = (outs );
  let InOperandList = (ins simm18_lsb0_bnext:$Src);

  let Inst{31-15} = Src;
  let Inst{14-12} = Func;
  let Inst{11-7} = BlockType;
  let Inst{6-3} = 0b0000;
  let Inst{2-1} = 0b00;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t" # BrType # ", $Src";
}

def BSTART_STD_WITH_FIXUP_17_FALL : BSTART_WITH_TARGET_17<0b001, 0b00000, ".STD ", "FALL">;
def BSTART_STD_WITH_TARGET_17_DIRECT : BSTART_WITH_TARGET_17<0b010, 0b00000, ".STD", "DIRECT">;
def BSTART_STD_WITH_TARGET_17_COND : BSTART_WITH_TARGET_17<0b011, 0b00000, ".STD", "COND">;
def BSTART_STD_WITH_TARGET_17_CALL : BSTART_WITH_TARGET_17<0b100, 0b00000, ".STD", "CALL">;
def BSTART_AUX_WITH_FIXUP_17_FALL : BSTART_WITH_TARGET_17<0b001, 0b00001, ".AUX ", "FALL">;
def BSTART_AUX_WITH_TARGET_17_DIRECT : BSTART_WITH_TARGET_17<0b010, 0b00001, ".AUX ", "DIRECT">;
def BSTART_AUX_WITH_TARGET_17_COND : BSTART_WITH_TARGET_17<0b011, 0b00001, ".AUX ", "COND">;
def BSTART_AUX_WITH_TARGET_17_CALL : BSTART_WITH_TARGET_17<0b100, 0b00001, ".AUX ", "CALL">;
def BSTART_FP_WITH_FIXUP_17_FALL : BSTART_WITH_TARGET_17<0b001, 0b00010, ".FP ", "FALL">;
def BSTART_FP_WITH_TARGET_17_DIRECT : BSTART_WITH_TARGET_17<0b010, 0b00010, ".FP", "DIRECT">;
def BSTART_FP_WITH_TARGET_17_COND : BSTART_WITH_TARGET_17<0b011, 0b00010, ".FP", "COND">;
def BSTART_FP_WITH_TARGET_17_CALL : BSTART_WITH_TARGET_17<0b100, 0b00010, ".FP", "CALL">;

class BSTART_STD_WITH_TARGET_16<bits<3> Opc, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITH_TARGET_12;
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;

  bits<12> Src;

  let OutOperandList = (outs );
  let InOperandList = (ins simm13_lsb0:$Src);

  let Inst{15-4} = Src;
  let Inst{3-1} = Opc;
  let Inst{0} = 0b0;

  let AsmString = "C.BSTART.STD\t" # BrType # ", $Src";
}

def BSTART_STD_WITH_TARGET_16_DIRECT : BSTART_STD_WITH_TARGET_16<0b001, "DIRECT">;
def BSTART_STD_WITH_TARGET_16_COND : BSTART_STD_WITH_TARGET_16<0b010, "COND">;

class BSTART_WITH_TARGET_48<bits<3> Func, bits<5> BlockType, string HeadType, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITH_TARGET_29;
  field bits<48> SoftFail = 0;
  field bits<48> Inst;
  let Size = 6;

  bits<29> Src;

  let OutOperandList = (outs );
  let InOperandList = (ins simm30_lsb0_bnext:$Src);

  let Inst{47-31} = Src{16-0};
  let Inst{30-28} = Func;
  let Inst{27-23} = BlockType;
  let Inst{19-17} = 0b000;
  let Inst{16} = 1;
  let Inst{15-4} = Src{28-17};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = "HL.BSTART" # HeadType # "\t" # BrType # ", $Src";
}

def BSTART_STD_WITH_FIXUP_48_FALL : BSTART_WITH_TARGET_48<0b001, 0b00000, ".STD ", "FALL">;
def BSTART_STD_WITH_TARGET_48_DIRECT : BSTART_WITH_TARGET_48<0b010, 0b00000, ".STD", "DIRECT">;
def BSTART_STD_WITH_TARGET_48_COND : BSTART_WITH_TARGET_48<0b011, 0b00000, ".STD", "COND">;
def BSTART_STD_WITH_TARGET_48_CALL : BSTART_WITH_TARGET_48<0b100, 0b00000, ".STD", "CALL">;
def BSTART_AUX_WITH_FIXUP_48_FALL : BSTART_WITH_TARGET_48<0b001, 0b00001, ".AUX ", "FALL">;
def BSTART_AUX_WITH_TARGET_48_DIRECT : BSTART_WITH_TARGET_48<0b010, 0b00001, ".AUX ", "DIRECT">;
def BSTART_AUX_WITH_TARGET_48_COND : BSTART_WITH_TARGET_48<0b011, 0b00001, ".AUX ", "COND">;
def BSTART_AUX_WITH_TARGET_48_CALL : BSTART_WITH_TARGET_48<0b100, 0b00001, ".AUX ", "CALL">;
def BSTART_FP_WITH_FIXUP_48_FALL : BSTART_WITH_TARGET_48<0b001, 0b00010, ".FP ", "FALL">;
def BSTART_FP_WITH_TARGET_48_DIRECT : BSTART_WITH_TARGET_48<0b010, 0b00010, ".FP", "DIRECT">;
def BSTART_FP_WITH_TARGET_48_COND : BSTART_WITH_TARGET_48<0b011, 0b00010, ".FP", "COND">;
def BSTART_FP_WITH_TARGET_48_CALL : BSTART_WITH_TARGET_48<0b100, 0b00010, ".FP", "CALL">;

class BSTART_WITH_TARGET_64<bits<3> Func, bits<5> BlockType, string HeadType, string BrType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITH_TARGET_42;
  field bits<64> SoftFail = 0;
  field bits<64> Inst;
  let Size = 8;

  bits<42> Src;

  let OutOperandList = (outs );
  let InOperandList = (ins simm43_lsb0_bnext:$Src);

  let Inst{63-47} = Src{16-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = BlockType;
  let Inst{38-36} = 0b000;
  let Inst{35-33} = 0b000;
  let Inst{32} = 0b1;
  let Inst{31-7} = Src{41-17};
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = "L.BSTART" # HeadType # "\t" # BrType # ", $Src";
}

def BSTART_STD_WITH_FIXUP_64_FALL : BSTART_WITH_TARGET_64<0b001, 0b00000, ".STD ", "FALL">;
def BSTART_STD_WITH_TARGET_64_DIRECT : BSTART_WITH_TARGET_64<0b010, 0b00000, ".STD", "DIRECT">;
def BSTART_STD_WITH_TARGET_64_COND : BSTART_WITH_TARGET_64<0b011, 0b00000, ".STD", "COND">;
def BSTART_STD_WITH_TARGET_64_CALL : BSTART_WITH_TARGET_64<0b100, 0b00000, ".STD", "CALL">;
def BSTART_AUX_WITH_FIXUP_64_FALL : BSTART_WITH_TARGET_64<0b001, 0b00001, ".AUX ", "FALL">;
def BSTART_AUX_WITH_TARGET_64_DIRECT : BSTART_WITH_TARGET_64<0b010, 0b00001, ".AUX ", "DIRECT">;
def BSTART_AUX_WITH_TARGET_64_COND : BSTART_WITH_TARGET_64<0b011, 0b00001, ".AUX ", "COND">;
def BSTART_AUX_WITH_TARGET_64_CALL : BSTART_WITH_TARGET_64<0b100, 0b00001, ".AUX ", "CALL">;
def BSTART_FP_WITH_FIXUP_64_FALL : BSTART_WITH_TARGET_64<0b001, 0b00010, ".FP ", "FALL">;
def BSTART_FP_WITH_TARGET_64_DIRECT : BSTART_WITH_TARGET_64<0b010, 0b00010, ".FP", "DIRECT">;
def BSTART_FP_WITH_TARGET_64_COND : BSTART_WITH_TARGET_64<0b011, 0b00010, ".FP", "COND">;
def BSTART_FP_WITH_TARGET_64_CALL : BSTART_WITH_TARGET_64<0b100, 0b00010, ".FP", "CALL">;

class Tile_DataType<bits<5> Opcode, string HeadType, dag ins> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_32;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<5> DataType;
  bits<5> TileOP;
  bits<2> Mode;

  let OutOperandList = (outs );
  let InOperandList = ins;

  let Inst{31-27} = DataType;
  let Inst{26-25} = Mode;
  let Inst{24-20} = TileOP;
  let Inst{19-15} = Opcode;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00011;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t$TileOP, $DataType, $Mode";
}

class Tile_DataType_TEPL<bits<5> Opcode, string HeadType, dag ins> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_32;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<5> DataType;
  bits<7> TileOP;

  let OutOperandList = (outs );
  let InOperandList = ins;

  let Inst{31-27} = DataType;
  let Inst{26-20} = TileOP;
  let Inst{19-15} = Opcode;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00011;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t$TileOP, $DataType";
}


class Tile_DataType_NoMode<bits<5> Opcode, string HeadType, dag ins>
        : Tile_DataType<Opcode, HeadType, ins> {
  let Mode = 0;
  let AsmString = "BSTART" # HeadType # "\t$TileOP, $DataType";
}

class Tile_DataType_FixedOp<bits<5> Opcode, bits<5> FixedTileOP,
                            string HeadType, dag ins>
        : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_32;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<5> DataType;

  let OutOperandList = (outs );
  let InOperandList = ins;

  let Inst{31-27} = DataType;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = FixedTileOP;
  let Inst{19-15} = Opcode;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00011;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t$DataType";
}

def BSTART_TMA : Tile_DataType_NoMode<0b00010, ".TLSU", (ins BstartDataType:$DataType, TileOPTMA:$TileOP)>;
def BSTART_CUBE: Tile_DataType_NoMode<0b00110, ".CUBE", (ins BstartDataType:$DataType, TileOPCUBE:$TileOP)>;
def BSTART_TEPL_NoMode: Tile_DataType_TEPL<0b00011, ".TEPL", (ins BstartDataType:$DataType, TileOPTEPL:$TileOP)>;
def BSTART_GMOV : Tile_DataType_FixedOp<0b00010, 0b01101, ".GMOV",
                                       (ins BstartDataType:$DataType)>;
//def BSTART_TEPL: Tile_DataType<0b00011, ".TEPL", (ins BstartDataType:$DataType, TileOPTEPL:$TileOP, TEPLMode:$Mode)>;

class Tile_Mode<bits<5> Opcode, string HeadType> : LinxV5InstBase {
  let Format = InstFormat_BSTART_WITHOUT_TARGET_32;
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<2> Mode;

  let OutOperandList = (outs );
  let InOperandList = (ins TileOPMode:$Mode);

  let Inst{31-27} = 0;
  let Inst{26-25} = Mode;
  let Inst{24-20} = 0;
  let Inst{19-15} = Opcode;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00011;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "BSTART" # HeadType # "\t$Mode";
}

def BSTART_MPAR: Tile_Mode<0b00000, ".MPAR">;
def BSTART_MSEQ: Tile_Mode<0b00001, ".MSEQ">;
def BSTART_VPAR: Tile_Mode<0b00100, ".VPAR">;
def BSTART_VSEQ: Tile_Mode<0b00101, ".VSEQ">;

class BlockModifierBase : LinxV5InstBase {
  let Format = InstFormatBlockModifier;
}

def BTEXT : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins simm26_lsb0_bnext:$Src);

  bits<25> Src;
  let Inst{31-7} = Src;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.TEXT\t$Src";
}

def BCATR : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins BAttrType:$Type, DRImm:$DRImm);

  bits<5> Type;
  bits<1> DRImm;
  let Inst{31-27} = 0b00000;
  let Inst{26} = DRImm;
  let Inst{25-20} = 0b000000;
  let Inst{19-15} = Type;
  let Inst{14-12} = 0b000;
  let Inst{11-7} = 0b00000;
  let Inst{6-4} = 0b010;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.CATR\t$Type, $DRImm";
}

def BDATR : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins BArgFormat:$DataLayout, Canon:$canon, BstartDataType:$SrcType, PadValue:$PadValue, CmpMode:$CmpMode, RMode:$RMode, Sat:$sat, ByteID: $ByteId);

  bits<5> DataLayout;
  bits<3> CmpMode;
  bits<2> PadValue;
  bits<1> canon;
  bits<5> SrcType;
  bits<3> RMode;
  bits<1> sat;
  bits<2> ByteId;
  let Inst{31-29} = CmpMode;
  let Inst{28-27} = PadValue;
  let Inst{26} = sat;
  let Inst{25} = canon;
  let Inst{24-20} = SrcType;
  let Inst{19-18} = ByteId;
  let Inst{17-15} = RMode;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = DataLayout;
  let Inst{6-4} = 0b010;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.DATR\t$DataLayout$canon, $SrcType, $PadValue, $ByteId, $CmpMode, $RMode, #sat";
}

// B.SUBVIEW / B.ASSEMBLE: PTO-ISA 0.58.4 range modifiers (ADR-0098,
// pto-spec 564ac2d). Both are 32-bit block commands that attach to the
// immediately preceding continuous B.IOT/B.IOS binder group.
//   B.SUBVIEW  match=0x00000053 mask=0x0000787f
//   B.ASSEMBLE match=0x00001053 mask=0x0000707f
// Derived offset = GPR[RegSrc] + ZeroExtend(uimm11) modulo XLEN.
//   SubviewSizeCode 1..12  (0 and 13..15 reserved)
//   ParentSizeCode  0..12  (13..15 reserved; 0 legal only on non-INIT)
def B_SUBVIEW : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm1:$SrcSelect, GPRSrc:$RegSrc, uimm11:$uimm11,
                           SubviewSizeCode:$SubviewSizeCode);

  bits<1> SrcSelect;
  bits<11> uimm11;
  bits<5> RegSrc;
  bits<4> SubviewSizeCode;
  let Inst{31} = SrcSelect;
  let Inst{30-20} = uimm11;
  let Inst{19-15} = RegSrc;
  let Inst{14-11} = 0b0000;
  let Inst{10-7} = SubviewSizeCode;
  let Inst{6-4} = 0b101;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.SUBVIEW\t$SrcSelect, $RegSrc, $uimm11, $SubviewSizeCode";
}

def B_ASSEMBLE : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm1:$INIT, uimm1:$LAST, GPRSrc:$RegSrc,
                           uimm11:$uimm11, ParentSizeCode:$ParentSizeCode);

  bits<1> INIT;
  bits<1> LAST;
  bits<11> uimm11;
  bits<5> RegSrc;
  bits<4> ParentSizeCode;
  let Inst{31} = INIT;
  let Inst{30-20} = uimm11;
  let Inst{19-15} = RegSrc;
  let Inst{14-12} = 0b001;
  let Inst{11} = LAST;
  let Inst{10-7} = ParentSizeCode;
  let Inst{6-4} = 0b101;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.ASSEMBLE\t$INIT, $LAST, $RegSrc, $uimm11, $ParentSizeCode";
}

// DavinciOO v5 FIXPIPE attributes. Required exactly once by every supported
// Matrix CUBE family. PTO 0.58.4 assigns TransA, TransB and CScaleEn to the
// previously reserved bits 7, 8 and 9; bit 10 remains reserved and zero.
def B_FPATR : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;
  let isDisassembleOnly = 0;

  let OutOperandList = (outs);
  let InOperandList = (ins uimm6:$PreQuantMode, uimm3:$ReluMode,
                           uimm4:$GroupNCode, uimm1:$RowMaxEn,
                           uimm1:$GroupMaxEn, uimm1:$RowMaxInit,
                           uimm1:$MaxAbsEn, uimm1:$TransA, uimm1:$TransB,
                           uimm1:$CScaleEn);

  bits<6> PreQuantMode;
  bits<3> ReluMode;
  bits<4> GroupNCode;
  bits<1> RowMaxEn;
  bits<1> GroupMaxEn;
  bits<1> RowMaxInit;
  bits<1> MaxAbsEn;
  bits<1> TransA;
  bits<1> TransB;
  bits<1> CScaleEn;

  let Inst{31-26} = PreQuantMode;
  let Inst{25-23} = ReluMode;
  let Inst{22-19} = GroupNCode;
  let Inst{18} = RowMaxEn;
  let Inst{17} = GroupMaxEn;
  let Inst{16} = RowMaxInit;
  let Inst{15} = MaxAbsEn;
  let Inst{14-12} = 0b010;
  let Inst{11} = 0b0;
  let Inst{10} = 0b0;
  let Inst{9} = CScaleEn;
  let Inst{8} = TransB;
  let Inst{7} = TransA;
  let Inst{6-4} = 0b010;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;

  let AsmString = "B.FPATR	$PreQuantMode, $ReluMode, $GroupNCode, "
                  "$RowMaxEn, $GroupMaxEn, $RowMaxInit, $MaxAbsEn, "
                  "$TransA, $TransB, $CScaleEn";
}

def B_IO : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;
  let isDisassembleOnly = 0;
  let OutOperandList = (outs GPRDstWithOptionalArrow:$RegDst);
  let InOperandList = (ins GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, GPRSrcNoR0:$RegSrc2);

  bits<5> RegSrc0;
  bits<5> RegSrc1;
  bits<5> RegSrc2;
  bits<5> RegDst;
  let Inst{31-27} = RegSrc2;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = RegSrc1;
  let Inst{19-15} = RegSrc0;
  let Inst{14-12} = 0b000;
  let Inst{11-7} = RegDst;
  let Inst{6-4} = 0b001;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.IOR  [$RegSrc0,$RegSrc1,$RegSrc2],[$RegDst]";
}

def B_IOD_Dst : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;
  let isDisassembleOnly = 0;
  let OutOperandList = (outs );
  let InOperandList = (ins RegDepSrc:$DepSrc0);

  bits<5> DepSrc0;
  let Inst{31-27} = 0b00000;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = 0b00000;
  let Inst{19-15} = DepSrc0;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00001;
  let Inst{6-4} = 0b001;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.IOD ${DepSrc0}, ->d";
}

def B_IOD_Nodst : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;
  let isDisassembleOnly = 0;
  let isCodeGenOnly = 1;  // B.IOD retired (handoff 5.0.1); drop from decoder so
                          // B.IOS with SharedTID=0 decodes unambiguously.
  let OutOperandList = (outs );
  let InOperandList = (ins RegDepSrc:$DepSrc0);

  bits<5> DepSrc0;
  let Inst{31-27} = 0b00000;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = 0b00000;
  let Inst{19-15} = DepSrc0;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0b00000;
  let Inst{6-4} = 0b001;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
  let AsmString = "B.IOD $DepSrc0";
}

def BSTOP_C : LinxV5InstBase {
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;

  let Inst{15-0} = 0x0000;

  let OutOperandList = (outs );
  let InOperandList = (ins );

  let AsmString = "C.BSTOP";
}

def BSTOP : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let Inst{31-0} = 0x1;

  let OutOperandList = (outs );
  let InOperandList = (ins );

  let AsmString = "BSTOP";
}

def XB : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<7> CrossBID;
  bits<10> AcrID;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm7:$CrossBID, uimm10:$AcrID);

  let Inst{31-25} = CrossBID;
  let Inst{24-15} = AcrID;
  let Inst{14-12} = 0b110;
  let Inst{11-7} = 0b11111;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b000;
  let Inst{0} = 0b1;

  let AsmString = "XB $AcrID, $CrossBID";
}


class LinxV5Inst_B_CACR<bits<5> BlockType, string opcstr> : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<4> AllowACRs;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm4:$AllowACRs);

  let Inst{31-19} = 0;
  let Inst{18-15} = AllowACRs;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = BlockType;
  let Inst{6-4} = 0b010;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;

  let AsmString = opcstr # " $AllowACRs";
}

multiclass LinxV5Inst_B_CACR_TYPE<string opcstr> {
  def "_STD" : LinxV5Inst_B_CACR<0b00000, opcstr # ".STD">;
  def "_AUX" : LinxV5Inst_B_CACR<0b00001, opcstr # ".AUX">;
  def "_FP" : LinxV5Inst_B_CACR<0b00010, opcstr # ".FP">;
  def "_SIMT" : LinxV5Inst_B_CACR<0b00011, opcstr # ".SIMT">;
}

defm B_CACR : LinxV5Inst_B_CACR_TYPE<"B.CACR">;


class LinxV5Inst_B_HINT<bits<2> Temp, string TempStr, bits<2> Likely, string LikelyStr, string opcstr> : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<12> Imm12;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm12:$Imm12);

  let Inst{31-20} = Imm12;
  let Inst{19} = 0;
  let Inst{18-17} = Temp;
  let Inst{16-15} = Likely;
  let Inst{14-12} = 0b000;
  let Inst{11-7} = 0;
  let Inst{6-4} = 0b011;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;

  let AsmString = opcstr # LikelyStr # TempStr # ", $Imm12";
}

multiclass LinxV5Inst_B_HINT_Likely<bits<2> Temp, string TempStr, string opcstr> {
  def ""          : LinxV5Inst_B_HINT<Temp, TempStr, 0b00, "", opcstr>;
  def "_likely"   : LinxV5Inst_B_HINT<Temp, TempStr, 0b11, " BR.likely,", opcstr>;
  def "_unlikely" : LinxV5Inst_B_HINT<Temp, TempStr, 0b01, " BR.unlikely,", opcstr>;
}

multiclass LinxV5Inst_B_HINT_Temp<string opcstr> {
  defm "_NONE" : LinxV5Inst_B_HINT_Likely<0b00, " TEMP.none", opcstr>;
  defm "_COOL" : LinxV5Inst_B_HINT_Likely<0b01, " TEMP.cool", opcstr>;
  defm "_WARM" : LinxV5Inst_B_HINT_Likely<0b10, " TEMP.warm", opcstr>;
  defm "_HOT"  : LinxV5Inst_B_HINT_Likely<0b11, " TEMP.hot", opcstr>;
}

defm B_HINT : LinxV5Inst_B_HINT_Temp<"B.HINT">;

class LinxV5Inst_B_HINT_TRACE<bits<1> inst, string traceStr, string opcstr> : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  let OutOperandList = (outs );
  let InOperandList = (ins );

  let Inst{31-16} = 0;
  let Inst{15} = inst;
  let Inst{14-12} = 0b001;
  let Inst{11-7} = 0;
  let Inst{6-4} = 0b011;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;

  let AsmString = opcstr # traceStr;
}

def B_HINT_TRACE_BEGIN : LinxV5Inst_B_HINT_TRACE<0b0, " TRACE.begin", "B.HINT">;
def B_HINT_TRACE_END : LinxV5Inst_B_HINT_TRACE<0b1, " TRACE.end", "B.HINT">;

def C_ADDPC : LinxV5InstBase {
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;

  bits<5> imm6;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm6_lsb0:$imm6);

  let Inst{15-11} = 0b01010;
  let Inst{10-6} = imm6;
  let Inst{5-4} = 0b01;
  let Inst{3-1} = 0b011;
  let Inst{0} = 0b0;
  let AsmString = "c.setret\t$imm6, ->ra";
}

def ADDPC : LinxV5InstBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let Size = 4;

  bits<20> imm20;

  let OutOperandList = (outs );
  let InOperandList = (ins uimm21_lsb0:$imm20);

  let Inst{31-12} = imm20;
  let Inst{11-7} = 0b01010;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b011;
  let Inst{0} = 0b1;

  let AsmString = "setret\t$imm20, ->ra";
}

} // end hasSideEffects = 0, mayLoad = 0, mayStore = 0

def : InstAlias<"B.DATR $PadValue", (BDATR 0, 0, 31, PadValue:$PadValue, 0, 0, 0, 0), 1>;
def : InstAlias<"B.DATR $PadValue, $CmpMode", (BDATR 0, 0, 31, PadValue:$PadValue, CmpMode:$CmpMode, 0, 0, 0), 1>;
def : InstAlias<"B.DATR\t$srctype, $ByteId, $PadValue", (BDATR 0, 0, BstartDataType:$srctype, PadValue:$PadValue, 0, 0, 0, ByteID:$ByteId), 1>;
def : InstAlias<"B.CATR $DRImm", (BCATR 0, DRImm:$DRImm), 2>;
def : InstAlias<"B.CATR $Type", (BCATR BAttrType:$Type, 0), 1>;

def : InstAlias<"B.DATR $DataLayout$canon", (BDATR BArgFormat:$DataLayout, Canon:$canon, 31, 3, 0, 0, 0, 0), 1>;
def : InstAlias<"B.DATR\t$srctype, $RMode, $sat", (BDATR 0, 0, BstartDataType:$srctype, 3, 0, RMode:$RMode, Sat:$sat, 0), 1>;
def : InstAlias<"B.DATR\t$srctype, $PadValue, $RMode, $sat",
    (BDATR 0, 0, BstartDataType:$srctype, PadValue:$PadValue, 0,
     RMode:$RMode, Sat:$sat, 0), 1>;
def : InstAlias<"B.DATR\t$srctype, $RMode", (BDATR 0, 0, BstartDataType:$srctype, 3, 0, RMode:$RMode, 0, 0), 1>;
//def : InstAlias<"B.DATR\t$srctype, $datalayout", (BDATR BArgFormat:$datalayout, 0,  BstartDataType:$srctype, 3, 0, 0, 0, 0), 1>;
//def : InstAlias<"B.DATR $DataLayout", (BDATR BArgFormat:$DataLayout, 0, 31, 3, 0, 0, 0, 0), 3>;
def : InstAlias<"B.DATR $DataLayout$canon, $PadValue", (BDATR BArgFormat:$DataLayout, Canon:$canon, 31, PadValue:$PadValue, 0, 0, 0, 0), 2>;
def : InstAlias<"B.DATR $DataLayout$canon, $SrcType, $PadValue", (BDATR BArgFormat:$DataLayout, Canon:$canon, BstartDataType:$SrcType, PadValue:$PadValue, 0, 0, 0, 0), 1>;
def : InstAlias<"B.DATR $DataLayout$canon, $PadValue, $CmpMode", (BDATR BArgFormat:$DataLayout, Canon:$canon, 31, PadValue:$PadValue, CmpMode:$CmpMode, 0, 0, 0), 2>;
def : InstAlias<"B.DATR $DataLayout$canon, $SrcType, $PadValue, $CmpMode", (BDATR BArgFormat:$DataLayout, Canon:$canon, BstartDataType:$SrcType, PadValue:$PadValue, CmpMode:$CmpMode, 0, 0, 0), 1>;

// PTO-ISA must-zero B.DATR forms (TMATMUL/TGEMV/TMOV datr_contract:
// pad_union = must-zero). The pre-existing aliases hardcode
// PadValue = 3 (Null) whenever RMode is present, which a conforming
// legality check (BundleTMATMULDataAttributesLegal, pad_value ==
// Zeros{2}) rejects. These aliases expose PadValue as an operand so
// callers can spell `B.DATR <type>, <byteid>, Zero, <rmode>, <sat>`.
def : InstAlias<"B.DATR\t$srctype, $ByteId, $PadValue, $RMode, $sat", (BDATR 0, 0, BstartDataType:$srctype, PadValue:$PadValue, 0, RMode:$RMode, Sat:$sat, ByteID:$ByteId), 2>;
def : InstAlias<"B.DATR\t$srctype, $PadValue, $RMode, $sat", (BDATR 0, 0, BstartDataType:$srctype, PadValue:$PadValue, 0, RMode:$RMode, Sat:$sat, 0), 2>;

//def : InstAlias<"BSTART.TEPL $TileOP, $Mode", (BSTART_TEPL 31, TileOPTEPL:$TileOP, TEPLMode:$Mode), 1>;

def : InstAlias<"setret\t$imm20", (ADDPC uimm21_lsb0:$imm20), 0>;
def : InstAlias<"c.setret\t$imm6", (C_ADDPC uimm6_lsb0:$imm6), 0>;
def : InstAlias<"C.BSTART $BrType", (BSTART_STD_WITHOUT_TARGET_16 BStartWithoutTargetBrType:$BrType), 0>;
def : InstAlias<"BSTART DIRECT, $Src", (BSTART_STD_WITH_TARGET_25_DIRECT simm26_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART CALL, $Src", (BSTART_STD_WITH_TARGET_25_DIRECT simm26_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART COND, $Src", (BSTART_STD_WITH_TARGET_25_COND simm26_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART FALL, $Src", (BSTART_STD_WITH_FIXUP_17_FALL simm18_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART DIRECT, $Src", (BSTART_STD_WITH_TARGET_17_DIRECT simm18_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART COND, $Src", (BSTART_STD_WITH_TARGET_17_COND simm18_lsb0_bnext:$Src), 0>;
def : InstAlias<"BSTART CALL, $Src", (BSTART_STD_WITH_TARGET_17_CALL simm18_lsb0_bnext:$Src), 0>;

def : InstAlias<"C.BSTART DIRECT, $Src", (BSTART_STD_WITH_TARGET_16_DIRECT simm13_lsb0:$Src), 0>;
def : InstAlias<"C.BSTART COND, $Src", (BSTART_STD_WITH_TARGET_16_COND simm13_lsb0:$Src), 0>;
def : InstAlias<"invalid", (BSTOP_C), 0>;
def : InstAlias<"nop", (ORI R0, R0, 0), 1>;
def : InstAlias<"BSTART", (BSTART_STD_WITHOUT_TARGET_32_FALL), 0>;
def : InstAlias<"BSTART.AUX", (BSTART_AUX_WITHOUT_TARGET_32_FALL), 0>;
def : InstAlias<"BSTART.FP", (BSTART_FP_WITHOUT_TARGET_32_FALL), 0>;
def : InstAlias<"BSTART IND", (BSTART_STD_WITHOUT_TARGET_32_IND), 0>;
def : InstAlias<"BSTART ICALL", (BSTART_STD_WITHOUT_TARGET_32_ICALL), 0>;
def : InstAlias<"BSTART RET", (BSTART_STD_WITHOUT_TARGET_32_RET), 0>;

// Canonical form uses a comma-space between GPRs and an arrow for the
// destination.  Keep the historical bracketed aliases below as input-only
// compatibility spellings; disassembly always goes through printB_IOR.
def : InstAlias<"B.IOR [], $RegDst", (B_IO GPRDstWithOptionalArrow:$RegDst, R0, R0, R0), 1>;
def : InstAlias<"B.IOR [$RegSrc0], $RegDst", (B_IO GPRDstWithOptionalArrow:$RegDst, GPRSrcNoR0:$RegSrc0, R0, R0), 1>;
def : InstAlias<"B.IOR [$RegSrc0, $RegSrc1], $RegDst", (B_IO GPRDstWithOptionalArrow:$RegDst, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, R0), 1>;
def : InstAlias<"B.IOR [$RegSrc0, $RegSrc1, $RegSrc2], $RegDst", (B_IO GPRDstWithOptionalArrow:$RegDst, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, GPRSrcNoR0:$RegSrc2), 1>;
def : InstAlias<"B.IOR [], []", (B_IO R0, R0, R0, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0], []", (B_IO R0, GPRSrc:$RegSrc0, R0, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0, $RegSrc1], []", (B_IO R0, GPRSrc:$RegSrc0, GPRSrcNoR0:$RegSrc1, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0, $RegSrc1, $RegSrc2], []", (B_IO R0, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, GPRSrcNoR0:$RegSrc2), 1>;
def : InstAlias<"B.IOR [],[$RegDst]", (B_IO GPRSrcNoR0:$RegDst, R0, R0, R0), 1>;
def : InstAlias<"B.IOR [$RegSrc0],[$RegDst]", (B_IO GPRSrcNoR0:$RegDst, GPRSrcNoR0:$RegSrc0, R0, R0), 1>;
def : InstAlias<"B.IOR [$RegSrc0,$RegSrc1],[$RegDst]", (B_IO GPRSrcNoR0:$RegDst, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, R0), 1>;
def : InstAlias<"B.IOR [],[]", (B_IO R0, R0, R0, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0],[]", (B_IO R0, GPRSrcNoR0:$RegSrc0, R0, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0,$RegSrc1],[]", (B_IO R0, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, R0), 2>;
def : InstAlias<"B.IOR [$RegSrc0,$RegSrc1,$RegSrc2],[]", (B_IO R0, GPRSrcNoR0:$RegSrc0, GPRSrcNoR0:$RegSrc1, GPRSrcNoR0:$RegSrc2), 1>;

//to support bstart.SYS
//c.bstart.sys
def : InstAlias<"C.BSTART.SYS $BrType", (BSTART_AUX_WITHOUT_TARGET_16 BStartWithoutTargetBrType:$BrType), 0>;

//bstart.sys
def : InstAlias<"BSTART.SYS", (BSTART_AUX_WITHOUT_TARGET_32_FALL), 0>;
def : InstAlias<"BSTART.SYS FALL", (BSTART_AUX_WITHOUT_TARGET_32_FALL), 0>;
def : InstAlias<"BSTART.SYS FALL, $Src", (BSTART_AUX_WITH_FIXUP_17_FALL simm18_lsb0_bnext:$Src), 0>;

//hl.bstart
def : InstAlias<"HL.BSTART.SYS FALL, $Src", (BSTART_AUX_WITH_FIXUP_48_FALL simm30_lsb0_bnext:$Src), 0>;

//l.bstart
def : InstAlias<"L.BSTART.SYS FALL, $Src", (BSTART_AUX_WITH_FIXUP_64_FALL simm43_lsb0_bnext:$Src), 0>;

class TMPL<bits<3> Func, string opcstr>:
    LinxV5Inst_TMPL<Func, (outs), (ins GPRSrcNoR0R1:$SrcB, GPRSrcNoR0R1:$SrcE, uimm15:$Imm15), opcstr>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, isCall = 0, isTerminator = 1 in {
def FENTRY:    TMPL<0b000, "FENTRY">;
def FEXIT:     TMPL<0b001, "FEXIT">;
def FRET_STK:  TMPL<0b011, "FRET.STK">;
def FRET_RA:   TMPL<0b010, "FRET.RA">;
}

class TMPL_MEM<bits<3> Func, string opcstr>:
    LinxV5Inst_TMPL_MEM<Func, (outs), (ins GPRSrc:$Src0, GPRSrc:$Src1, GPRSrc:$Src2), opcstr>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, isCall = 0, isTerminator = 0 in {
def MCOPY:    TMPL_MEM<0b000, "MCOPY">;
def MSET:     TMPL_MEM<0b001, "MSET">;
}

let hasSideEffects = 1 in {
def SSR_GET : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b000, (outs DstRWithArrow:$DstR), (ins uimm12:$Imm12), "ssrget"> {
  let SrcL = 0b00000;
  let AsmString = "ssrget\t$Imm12, $DstR";
}

def SSR_SET : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b001, (outs), (ins GPRSrc:$SrcL, uimm12:$Imm12), "ssrset"> {
  let DstR = 0b00000;
  let AsmString = "ssrset\t$SrcL, $Imm12";
}

def SSR_SWAP : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b010, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, uimm12:$Imm12), "ssrswap"> {
  let AsmString = "ssrswap $SrcL, $Imm12, $DstR";
}

def LSR_GET : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b011, (outs DstRWithArrow:$DstR), (ins uimm12:$Imm12), "lsrget"> {
  let SrcL = 0b00000;
  let AsmString = "lsrget $Imm12, $DstR";
}

def SETC_TGT : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b100, (outs), (ins GPRSrc:$SrcL), "setc.tgt"> {
  let Imm12 = 0;
  let DstR = 0b00000;
  let AsmString = "setc.tgt\t$SrcL";
}

class BCC_Base<bits<3> Func, string opcstr>
    : LinxV5Inst_D<Func, Type_011, OPC_Branch_Instruction, (outs), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, simm13_lsb0:$Imm12), opcstr> {
  let Format = InstFormat_BRANCH;
}

let isBranch = 1, isTerminator = 1 in {
def BEQ  : BCC_Base<0b000, "b.eq">;
def BNE  : BCC_Base<0b001, "b.ne">;
def BLT  : BCC_Base<0b010, "b.lt">;
def BGE  : BCC_Base<0b011, "b.ge">;
def BLTU : BCC_Base<0b100, "b.ltu">;
def BGEU : BCC_Base<0b101, "b.geu">;
}

def JR : LinxV5Inst_D<0b110, Type_011, OPC_Branch_Instruction, (outs), (ins GPRSrc:$SrcL, simm13_lsb0:$Imm12), ""> {
  let SrcR = R0.HWEncoding{4-0};
  let AsmString = "jr\t$SrcL, $Imm12";
  let isBarrier = 1;
  let isBranch = 1;
  let isIndirectBranch = 1;
  let isTerminator = 1;
}

class Branch_Base<bits<3> Func, string opcstr>
    : LinxV5Inst_IJ<Func, Type_011, 0b011, (outs), (ins simm23_lsb0:$Imm22), opcstr> {
  let Format = InstFormat_BRANCH_22;
}

let isBarrier = 1, isBranch = 1, isTerminator = 1 in
def J : Branch_Base<0b000, "j">;

let isBranch = 1, isTerminator = 1 in {
  def BZ : Branch_Base<0b001, "b.z">;
  def BNZ : Branch_Base<0b010, "b.nz">;
}
def LOOP_GET : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b110, (outs DstRWithArrow:$DstR), (ins LoopReg:$SrcL), "loop.get"> {
  let Imm12 = 0;
  let AsmString = "loop.get\t$SrcL, $DstR";
}

def LOOP_SET : LinxV5Inst_E<Type_101, OPC_SSR_Access, 0b111, (outs LoopRegDst:$DstR), (ins GPRSrc:$SrcL, uimm12:$Imm12), "loop.set">;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, usesCustomInserter = 1 in
def PseudoLoopSet : Pseudo<(outs), (ins LoopRegDst:$DstR, GPRSrc:$SrcL, uimm12:$Imm12), []>;

def : Pat<(linxv5_loop_set i64:$DstR, uimm12:$Imm12), (PseudoLoopSet i64:$DstR, (i64 R0), uimm12:$Imm12)>;
def : Pat<(linxv5_loop_set i64:$DstR, (add i64:$SrcL, uimm12:$Imm12)), (PseudoLoopSet i64:$DstR, i64:$SrcL, uimm12:$Imm12)>;
def : Pat<(linxv5_loop_set i64:$DstR, i64:$SrcL), (PseudoLoopSet i64:$DstR, i64:$SrcL, (i64 0))>;

// ===---------------------- PE_TYPE=SYS ------------------------===

class CTRL_BASE<bits<3> Func, bits<12> imm, dag ins, string opcstr, string argstr>
    : LinxV5Inst_F<Type_101, OPC_Execution_Control, Func, (outs), ins, opcstr> {
  let Imm12 = imm;
  let Imm5  = 0b00000;
  let AsmString = opcstr # "\t" # argstr;
}

class EXECTRL_BASE<bits<12> imm, string opcstr>
    : CTRL_BASE<0b000, imm, (ins GPRSrc:$SrcL), opcstr, "$SrcL">;

def BSE  : EXECTRL_BASE<0, "bse">;
def BWE  : EXECTRL_BASE<1, "bwe">;
def BWI  : EXECTRL_BASE<2, "bwi">;
let hasSideEffects = 1 in
def TRAP : EXECTRL_BASE<3, "trap">;

def : Pat<(trap), (TRAP (i64 R0))>;

class FENCE_BASE<dag ins, bit inst, string opcstr, string argstr>
    : LinxV5Inst_F<Type_101, OPC_Execution_Control, 0b010, (outs), ins, opcstr> {
  bits<4> PRED;
  bits<4> SUCC;

  let Imm12{11-9} = 0b000;
  let Imm12{8}    = inst;
  let Imm12{7-4}  = PRED;
  let Imm12{3-0}  = SUCC;

  let SrcL = R0.HWEncoding{4-0};
  let Imm5 = 0b00000;

  let AsmString = opcstr # "\t" # argstr;
}

class AC_BASE<dag ins, bit inst, string opcstr, string argstr>
    : LinxV5Inst_F<Type_101, OPC_Execution_Control, 0b011, (outs), ins, opcstr> {
  bits<4> rType;
  let Imm12{11-5} = 0;
  let Imm12{4} = inst;
  let Imm12{3-0} = rType;
  let Imm5 = 0;
  let SrcL = R0.HWEncoding{4-0};

  let AsmString = opcstr # argstr;
}

def FenceFlagAsmOperand : AsmOperandClass {
  let Name = "FenceFlag";
  let ParserMethod = "parseFenceFlag";
  let RenderMethod = "addImmOperands";
}

def FenceFlag : Operand<OtherVT> {
  let ParserMatchClass = FenceFlagAsmOperand;
  let PrintMethod = "printFenceFlag";
  let DecoderMethod = "decodeUImmOperand<4>";
}

let PE = PET_SYS in {
  def FENCE_D : FENCE_BASE<(ins FenceFlag:$PRED, FenceFlag:$SUCC), 0, "fence.d", "$PRED, $SUCC">;
  def FENCE_I : FENCE_BASE<(ins), 1, "fence.i", ""> {
    let PRED = 0b0000;
    let SUCC = 0b0000;
  }

  def ACRC : AC_BASE<(ins uimm4:$rType), 0, "acrc", " $rType">;
  def ACRE : AC_BASE<(ins uimm4:$rType), 1, "acre", " $rType">;
  def ASSERT : LinxV5Inst_F<Type_101, OPC_Execution_Control, 0b001, (outs), (ins GPRSrc:$SrcL), "assert"> {
  let Imm12 = 0;
  let Imm5 = 0;
  let AsmString = "assert $SrcL";
  }
}

multiclass CACHE_CTRL_BASE<bits<3> Func, string opcstr> {
  def _IVA  : CTRL_BASE<Func, 0, (ins GPRSrc:$SrcL), opcstr # ".iva", "\t$SrcL">;
  let SrcL = R0.HWEncoding{4-0} in
  def _IALL : CTRL_BASE<Func, 1, (ins), opcstr # ".iall", "">;
}

multiclass DCACHE_CTRL_BASE<bits<3> Func, string opcstr>
    : CACHE_CTRL_BASE<Func, opcstr> {
  def _CVA  : CTRL_BASE<Func, 2, (ins GPRSrc:$SrcL), opcstr # ".cva",  " $SrcL">;
  def _CIVA : CTRL_BASE<Func, 3, (ins GPRSrc:$SrcL), opcstr # ".civa", " $SrcL">;
  def _ISW  : CTRL_BASE<Func, 4, (ins GPRSrc:$SrcL), opcstr # ".isw",  " $SrcL">;
  def _CSW  : CTRL_BASE<Func, 5, (ins GPRSrc:$SrcL), opcstr # ".csw",  " $SrcL">;
  def _CISW : CTRL_BASE<Func, 6, (ins GPRSrc:$SrcL), opcstr # ".cisw", " $SrcL">;
  def _ZVA  : CTRL_BASE<Func, 7, (ins GPRSrc:$SrcL), opcstr # ".zva",  " $SrcL">;
}

multiclass DCACHE_CTRL_TLB<bits<3> Func, string opcstr> {
  def _IA  : CTRL_BASE<Func, 0, (ins GPRSrc:$SrcL), opcstr # ".ia",  " $SrcL">;
  def _IV : CTRL_BASE<Func, 1, (ins GPRSrc:$SrcL), opcstr # ".iv", " $SrcL">;
  def _IAV  : CTRL_BASE<Func, 2, (ins GPRSrc:$SrcL), opcstr # ".iav",  " $SrcL">;
  let SrcL = R0.HWEncoding{4-0} in
  def _IALL  : CTRL_BASE<Func, 3, (ins GPRSrc:$SrcL), opcstr # ".iall",  "">;
}

defm BC : CACHE_CTRL_BASE<0b100, "bc">;
defm IC : CACHE_CTRL_BASE<0b101, "ic">;
defm DC : DCACHE_CTRL_BASE<0b110, "dc">;
defm TLB : DCACHE_CTRL_TLB<0b111, "tlb">;

class LOAD_RESERVE_BASE<bits<4> width, bits<3> order, string opcstr>
    : LinxV5Inst_B_SYS<width, order, 0b000,
                       (outs DstRWithArrow:$DstR),
                       (ins GPRSrc:$SrcL), opcstr, " [$SrcL], $DstR"> {
  let SrcR = R0.HWEncoding{4-0};
}

multiclass LOAD_RESERVE_ORDER<bits<4> width, string opcstr> {
  def ""  : LOAD_RESERVE_BASE<width, 0b000, opcstr>;
  def _AQ : LOAD_RESERVE_BASE<width, 0b010, opcstr # ".aq">;
  def _RL : LOAD_RESERVE_BASE<width, 0b001, opcstr # ".rl">;
  def _F : LOAD_RESERVE_BASE<width, 0b100, opcstr # ".f">;
  def _AQRL : LOAD_RESERVE_BASE<width, 0b011, opcstr # ".aqrl">;
  def _AQF : LOAD_RESERVE_BASE<width, 0b110, opcstr # ".aqf">;
  def _RLF : LOAD_RESERVE_BASE<width, 0b101, opcstr # ".rlf">;
  def _AQRLF : LOAD_RESERVE_BASE<width, 0b111, opcstr # ".aqrlf">;
}

multiclass LOAD_RESERVE_WIDTH<string opcstr> {
  defm _B : LOAD_RESERVE_ORDER<0, opcstr # ".b">;
  defm _H : LOAD_RESERVE_ORDER<1, opcstr # ".h">;
  defm _W : LOAD_RESERVE_ORDER<2, opcstr # ".w">;
  defm _D : LOAD_RESERVE_ORDER<3, opcstr # ".d">;
}

defm LR : LOAD_RESERVE_WIDTH<"lr">;

class STORE_COND_BASE<bits<4> width, bits<3> order, string opcstr>
    : LinxV5Inst_B_SYS<width, order, 0b001,
                       (outs DstRWithArrow:$DstR),
                       (ins GPRSrc:$SrcL, GPRSrc:$SrcR),
                       opcstr, " $SrcL, [$SrcR], $DstR">;

multiclass STORE_COND_ORDER<bits<4> width, string opcstr> {
  def ""     : STORE_COND_BASE<width, 0b000, opcstr>;
  def _AQ    : STORE_COND_BASE<width, 0b010, opcstr # ".aq">;
  def _RL    : STORE_COND_BASE<width, 0b001, opcstr # ".rl">;
  def _F     : STORE_COND_BASE<width, 0b100, opcstr # ".f">;
  def _AQRL  : STORE_COND_BASE<width, 0b011, opcstr # ".aqrl">;
  def _AQF   : STORE_COND_BASE<width, 0b110, opcstr # ".aqf">;
  def _RLF   : STORE_COND_BASE<width, 0b101, opcstr # ".rlf">;
  def _AQRLF : STORE_COND_BASE<width, 0b111, opcstr # ".aqrlf">;
}

multiclass  STORE_COND_WIDTH<string opcstr> {
  defm _B : STORE_COND_ORDER<0, opcstr # ".b">;
  defm _H : STORE_COND_ORDER<1, opcstr # ".h">;
  defm _W : STORE_COND_ORDER<2, opcstr # ".w">;
  defm _D : STORE_COND_ORDER<3, opcstr # ".d">;
}

defm SC : STORE_COND_WIDTH<"sc">;

class LOAD_ATOMIC_BASE<bits<3> func, bits<4> method, bits<3> order, string opcstr>
    : LinxV5Inst_B_SYS<method, order, func,
                       (outs DstRWithArrow:$DstR),
                       (ins GPRSrc:$SrcL, GPRSrc:$SrcR),
                       opcstr, " [$SrcL], $SrcR, $DstR">;

multiclass LOAD_ATOMIC_ORDER<bits<3> func, bits<4> method, string opcstr> {
  def ""    : LOAD_ATOMIC_BASE<func, method, 0b000, opcstr>;
  def _AQ   : LOAD_ATOMIC_BASE<func, method, 0b010, opcstr # ".aq">;
  def _RL   : LOAD_ATOMIC_BASE<func, method, 0b001, opcstr # ".rl">;
  def _F   : LOAD_ATOMIC_BASE<func, method, 0b100, opcstr # ".f">;
  def _AQRL : LOAD_ATOMIC_BASE<func, method, 0b011, opcstr # ".aqrl">;
  def _AQF   : LOAD_ATOMIC_BASE<func, method, 0b110, opcstr # ".aqf">;
  def _RLF   : LOAD_ATOMIC_BASE<func, method, 0b101, opcstr # ".rlf">;
  def _AQRLF : LOAD_ATOMIC_BASE<func, method, 0b111, opcstr # ".aqrlf">;
}

multiclass LOAD_ATOMIC_METHOD<bits<3> func, string opcstr> {
  defm _ADD  : LOAD_ATOMIC_ORDER<func, 0, opcstr # ".add">;
  defm _AND  : LOAD_ATOMIC_ORDER<func, 1, opcstr # ".and">;
  defm _OR   : LOAD_ATOMIC_ORDER<func, 2, opcstr # ".or">;
  defm _XOR  : LOAD_ATOMIC_ORDER<func, 3, opcstr # ".xor">;
  defm _SMAX : LOAD_ATOMIC_ORDER<func, 4, opcstr # ".smax">;
  defm _SMIN : LOAD_ATOMIC_ORDER<func, 5, opcstr # ".smin">;
  defm _UMAX : LOAD_ATOMIC_ORDER<func, 6, opcstr # ".umax">;
  defm _UMIN : LOAD_ATOMIC_ORDER<func, 7, opcstr # ".umin">;
}

defm LW : LOAD_ATOMIC_METHOD<0b010, "lw">;
defm LD : LOAD_ATOMIC_METHOD<0b100, "ld">;

class STORE_ATOMIC_BASE<bits<3> func, bits<4> method, bits<3> order, string opcstr>
    : LinxV5Inst_B_SYS<method, order, func,
                       (outs),
                       (ins GPRSrc:$SrcL, GPRSrc:$SrcR),
                       opcstr, " [$SrcL], $SrcR"> {
  let DstR = 0b00000;
}

multiclass STORE_ATOMIC_ORDER<bits<3> func, bits<4> method, string opcstr> {
  def ""    : STORE_ATOMIC_BASE<func, method, 0b000, opcstr>;
  def _RL   : STORE_ATOMIC_BASE<func, method, 0b001, opcstr # ".rl">;
  def _F   : STORE_ATOMIC_BASE<func, method, 0b100, opcstr # ".f">;
  def _RLF   : STORE_ATOMIC_BASE<func, method, 0b101, opcstr # ".rlf">;
}

multiclass STORE_ATOMIC_METHOD<bits<3> func, string opcstr> {
  defm _ADD  : STORE_ATOMIC_ORDER<func, 0, opcstr # ".add">;
  defm _AND  : STORE_ATOMIC_ORDER<func, 1, opcstr # ".and">;
  defm _OR   : STORE_ATOMIC_ORDER<func, 2, opcstr # ".or">;
  defm _XOR  : STORE_ATOMIC_ORDER<func, 3, opcstr # ".xor">;
  defm _SMAX : STORE_ATOMIC_ORDER<func, 4, opcstr # ".smax">;
  defm _SMIN : STORE_ATOMIC_ORDER<func, 5, opcstr # ".smin">;
  defm _UMAX : STORE_ATOMIC_ORDER<func, 6, opcstr # ".umax">;
  defm _UMIN : STORE_ATOMIC_ORDER<func, 7, opcstr # ".umin">;
}

defm SW : STORE_ATOMIC_METHOD<0b011, "sw">;
defm SD : STORE_ATOMIC_METHOD<0b101, "sd">;

class SWAP_BASE<bits<3> func, bits<4> width, bits<3> order, string opcstr>
    : LinxV5Inst_B_SYS<width, order, func,
                       (outs DstRWithArrow:$DstR),
                       (ins GPRSrc:$SrcL, GPRSrc:$SrcR),
                       opcstr, " [$SrcL], $SrcR, $DstR">;

multiclass SWAP_ORDER<bits<3> func, bits<4> method, string opcstr> {
  def ""    : SWAP_BASE<func, method, 0b000, opcstr>;
  def _AQ   : SWAP_BASE<func, method, 0b010, opcstr # ".aq">;
  def _RL   : SWAP_BASE<func, method, 0b001, opcstr # ".rl">;
  def _F   : SWAP_BASE<func, method, 0b100, opcstr # ".f">;
  def _AQRL : SWAP_BASE<func, method, 0b011, opcstr # ".aqrl">;
  def _AQF   : SWAP_BASE<func, method, 0b110, opcstr # ".aqf">;
  def _RLF   : SWAP_BASE<func, method, 0b101, opcstr # ".rlf">;
  def _AQRLF : SWAP_BASE<func, method, 0b111, opcstr # ".aqrlf">;
}

multiclass  SWAP_WIDTH<bits<3> func, string opcstr> {
  defm B : SWAP_ORDER<func, 0, opcstr # "b">;
  defm H : SWAP_ORDER<func, 1, opcstr # "h">;
  defm W : SWAP_ORDER<func, 2, opcstr # "w">;
  defm D : SWAP_ORDER<func, 3, opcstr # "d">;
}

defm SWAP : SWAP_WIDTH<0b110, "swap">;

// ===---------------------- DAG ISel Patterns ------------------------===

// def SrcRNONEPat : PatFrag<(ops node:$in), (i64 node:$in)>;

def linxv5_addw : worth_biopw<add>;
def linxv5_subw : worth_biopw<sub>;
def linxv5_andw : worth_biopw<and>;
def linxv5_orw  : worth_biopw<or>;
def linxv5_xorw : worth_biopw<xor>;

class PatRR<SDPatternOperator N, Instruction I> :
    Pat<(N i64:$SrcL, i64:$SrcR), (I i64:$SrcL, i64:$SrcR)>;
class PatRX<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(N i64:$SrcL, (X i64:$SrcR)), (I i64:$SrcL, i64:$SrcR)>;
let AddedComplexity = 6 in
class PatRI<SDPatternOperator N, SDPatternOperator ImmOp, Instruction I> :
    Pat<(N i64:$SrcL, ImmOp:$Imm), (I i64:$SrcL, ImmOp:$Imm)>;
class PatRR0<SDPatternOperator N, Instruction I> :
    Pat<(N i64:$SrcL, i64:$SrcR), (I i64:$SrcL, i64:$SrcR, (i64 0))>;
class PatRX0<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(N i64:$SrcL, (X i64:$SrcR)), (I i64:$SrcL, i64:$SrcR, (i64 0))>;
class PatRRS<SDPatternOperator N, Instruction I> :
    Pat<(N i64:$SrcL, (worth_shlext i64:$SrcR, ShamtImm:$Shamt)), (I i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt)>;
class PatRXS<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(N i64:$SrcL, (worth_shlext (X i64:$SrcR), ShamtImm:$Shamt)), (I i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt)>;

multiclass ALUPatNONE<SDPatternOperator N, Instruction I> {
  def : PatRR0<N, I>;
  def : PatRRS<N, I>;
}

multiclass ALUPat<SDPatternOperator N, SDPatternOperator X, Instruction I> {
  def : PatRX0<N, X, I>;
  def : PatRXS<N, X, I>;
}

multiclass ExtPat<SDPatternOperator N, Instruction I> {
  def : Pat<(N i64:$Src), (I (i64 R0), i64:$Src, (i64 0))>;
}

multiclass AddSubPat<SDPatternOperator N, Instruction Irr, Instruction Iri, Instruction Irni> {
  defm : ALUPatNONE<N, Irr>;
  defm : ALUPat<N, uw_ext, !cast<Instruction>(Irr # "_UW")>;
  defm : ALUPat<N, worth_sw_ext, !cast<Instruction>(Irr # "_SW")>;
  defm : ALUPat<N, neg_ext, !cast<Instruction>(Irr # "_NEG")>;
  def : PatRI<N, uimm12, Iri>;
  def : PatRI<N, neg_uimm12, Irni>;
}

multiclass BitPat<SDPatternOperator N, Instruction Irr, Instruction Iri> {
  defm : ALUPatNONE<N, Irr>;
  defm : ALUPat<N, uw_ext, !cast<Instruction>(Irr # "_UW")>;
  defm : ALUPat<N, worth_sw_ext, !cast<Instruction>(Irr # "_SW")>;
  defm : ALUPat<N, not_ext, !cast<Instruction>(Irr # "_NOT")>;
  def : PatRI<N, simm12, Iri>;
}

multiclass ShiftPat<SDPatternOperator N, Instruction Irr, Instruction Iri> {
  def : PatRR<N, Irr>;
  def : PatRI<N, uimm6, Iri>;
}

let Predicates = [isScalar, useDAGISel] in {
defm : AddSubPat<add, ADD, ADDI, SUBI>;
defm : AddSubPat<sub, SUB, SUBI, ADDI>;
defm : AddSubPat<linxv5_addw, ADDW, ADDIW, SUBIW>;
defm : AddSubPat<linxv5_subw, SUBW, SUBIW, ADDIW>;

defm : BitPat<and, AND, ANDI>;
defm : BitPat<or, OR, ORI>;
defm : BitPat<xor, XOR, XORI>;
defm : BitPat<linxv5_andw, ANDW, ANDIW>;
defm : BitPat<linxv5_orw, ORW, ORIW>;
defm : BitPat<linxv5_xorw, XORW, XORIW>;

defm : ExtPat<uw_ext, OR_UW>;
defm : ExtPat<sw_ext, OR_SW>;
def : Pat<(sh_ext i64:$Src), (BXS i64:$Src, (i64 0), (i64 16))>;
def : Pat<(sb_ext i64:$Src), (BXS i64:$Src, (i64 0), (i64 8))>;
def : Pat<(uh_ext i64:$Src), (BXU i64:$Src, (i64 0), (i64 16))>;
def : Pat<(ub_ext i64:$Src), (BXU i64:$Src, (i64 0), (i64 8))>;

defm : ShiftPat<sra, SRA, SRAI>;
defm : ShiftPat<srl, SRL, SRLI>;
defm : ShiftPat<shl, SLL, SLLI>;
defm : ShiftPat<linxv5_sraw, SRAW, SRAIW>;
defm : ShiftPat<linxv5_srlw, SRLW, SRLIW>;
defm : ShiftPat<linxv5_sllw, SLLW, SLLIW>;
}

let Predicates = [isScalar, useDAGISel] in {
def : Pat<(linxv5_addlo i64:$hi, mcsym:$lo), (ADDI i64:$hi, mcsym:$lo)>;
def : Pat<(linxv5_addlo i64:$hi, tglobaltlsaddr:$lo), (ADDI i64:$hi, tglobaltlsaddr:$lo)>;
}

multiclass CMPPat<SDPatternOperator N, SDPatternOperator ImmOp, Instruction Irr, Instruction Iri> {
  def : PatRR<N, Irr>;
  def : PatRX<N, uw_ext, !cast<Instruction>(Irr # _UW)>;
  def : PatRX<N, worth_sw_ext, !cast<Instruction>(Irr # _SW)>;
  def : PatRI<N, ImmOp, Iri>;
}

class linxv5_setcc_swap<SDPatternOperator N> :
    PatFrag<(ops node:$lhs, node:$rhs), (N node:$rhs, node:$lhs)>;

class PatBccRR<SDPatternOperator N, Instruction I> :
    Pat<(brcond (i64 (N i64:$SrcL, i64:$SrcR)), bb:$target), (I i64:$SrcL, i64:$SrcR, bb:$target)>;
class PatBccRX<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(brcond (i64 (N i64:$SrcL, (X i64:$SrcR))), bb:$target), (I i64:$SrcL, i64:$SrcR, bb:$target)>;
let AddedComplexity = 6 in
class PatBccRI<SDPatternOperator N, SDPatternOperator ImmOp, Instruction I> :
    Pat<(brcond (i64 (N i64:$SrcL, ImmOp:$Imm)), bb:$target), (I i64:$SrcL, ImmOp:$Imm, bb:$target)>;

multiclass BccPat<SDPatternOperator N, SDPatternOperator ImmOp, Instruction Irr, Instruction Iri> {
  def : PatBccRR<N, !cast<Instruction>(Irr # _BR)>;
  def : PatBccRX<N, uw_ext, !cast<Instruction>(Irr # _UW_BR)>;
  def : PatBccRX<N, worth_sw_ext, !cast<Instruction>(Irr # _SW_BR)>;
  def : PatBccRI<N, ImmOp, !cast<Instruction>(Iri # _BR)>;
}

let Predicates = [isScalar, useDAGISel] in {
defm : CMPPat<seteq, simm12, CMP_EQ, CMP_EQI>;
defm : CMPPat<setne, simm12, CMP_NE, CMP_NEI>;
defm : CMPPat<setlt, simm12, CMP_LT, CMP_LTI>;
defm : CMPPat<setge, simm12, CMP_GE, CMP_GEI>;
defm : CMPPat<setult, uimm12, CMP_LTU, CMP_LTUI>;
defm : CMPPat<setuge, uimm12, CMP_GEU, CMP_GEUI>;
defm : CMPPat<linxv5_setcc_swap<setgt>, simm12, CMP_LT, CMP_LTI>;
defm : CMPPat<linxv5_setcc_swap<setle>, simm12, CMP_GE, CMP_GEI>;
defm : CMPPat<linxv5_setcc_swap<setugt>, uimm12, CMP_LTU, CMP_LTUI>;
defm : CMPPat<linxv5_setcc_swap<setule>, uimm12, CMP_GEU, CMP_GEUI>;

def : Pat<(brcond i64:$cond, bb:$block), (SETC_NE_BR i64:$cond, (i64 R0), bb:$block)>;
def : Pat<(brcond (i64 (xor i64:$cond, 1)), bb:$block),
          (SETC_EQ_BR i64:$cond, (i64 R0), bb:$block)>;
defm : BccPat<seteq, simm12, SETC_EQ, SETC_EQI>;
defm : BccPat<setne, simm12, SETC_NE, SETC_NEI>;
defm : BccPat<setlt, simm12, SETC_LT, SETC_LTI>;
defm : BccPat<setge, simm12, SETC_GE, SETC_GEI>;
defm : BccPat<setult, uimm12, SETC_LTU, SETC_LTUI>;
defm : BccPat<setuge, uimm12, SETC_GEU, SETC_GEUI>;
defm : BccPat<linxv5_setcc_swap<setgt>, simm12, SETC_LT, SETC_LTI>;
defm : BccPat<linxv5_setcc_swap<setle>, simm12, SETC_GE, SETC_GEI>;
defm : BccPat<linxv5_setcc_swap<setugt>, uimm12, SETC_LTU, SETC_LTUI>;
defm : BccPat<linxv5_setcc_swap<setule>, uimm12, SETC_GEU, SETC_GEUI>;

def : Pat<(brind i64:$rs1), (PseudoBRIND i64:$rs1)>;
}

let Predicates = [isScalar, useDAGISel] in {
def : PatRR<mul, MUL>;
def : PatRR<biopw<mul>, MULW>;
}

class PatLOADrr<ValueType VT, SDPatternOperator N, ComplexPattern A, Instruction I> :
    Pat<(VT (N (A i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt))), (I i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt)>;

class PatLOADri<ValueType VT, SDPatternOperator N, ComplexPattern A, Instruction I> :
    Pat<(VT (N (A i64:$SrcL, simm12i64:$Imm))), (I i64:$SrcL, simm12i64:$Imm)>;

multiclass LoadPat<ValueType VT, SDPatternOperator N, int shift, Instruction I> {
  def : PatLOADrr<VT, N, LoadADDRrrD, I>;
  def : PatLOADrr<VT, N, LoadADDRrrSW, !cast<Instruction>(I # _SW)>;
  def : PatLOADrr<VT, N, LoadADDRrrUW, !cast<Instruction>(I # _UW)>;
  def : PatLOADri<VT, N, ADDRri_scaled<shift>, !cast<Instruction>(I # "I")>;
  def : PatLOADri<VT, N, ADDRri_unscaled, !cast<Instruction>(I # "I_U")>;
}

multiclass LoadPat_unscaled<ValueType VT, SDPatternOperator N, Instruction I> {
  def : PatLOADrr<VT, N, LoadADDRrrD, I>;
  def : PatLOADrr<VT, N, LoadADDRrrSW, !cast<Instruction>(I # _SW)>;
  def : PatLOADrr<VT, N, LoadADDRrrUW, !cast<Instruction>(I # _UW)>;
  def : PatLOADri<VT, N, ADDRri_unscaled, !cast<Instruction>(I # "I")>;
}

let Predicates = [isScalar, useDAGISel] in {
defm : LoadPat<i64, sextloadi32, 2, LW>;
defm : LoadPat<i64, sextloadi16, 1, LH>;
defm : LoadPat_unscaled<i64, sextloadi8, LB>;
defm : LoadPat<i64, zextloadi32, 2, LWU>;
defm : LoadPat<i64, zextloadi16, 1, LHU>;
defm : LoadPat_unscaled<i64, zextloadi8,  LBU>;
defm : LoadPat<i64, extloadi32, 2, LW>;
defm : LoadPat<i64, extloadi16, 1, LH>;
defm : LoadPat_unscaled<i64, extloadi8,  LB>;
defm : LoadPat<i64, load, 3, LD>;
}

class PatSTORErr<ValueType VT, SDPatternOperator N, ComplexPattern A, Instruction I> :
    Pat<(N VT:$SrcD, (A i64:$SrcL, i64:$SrcR)), (I VT:$SrcD, i64:$SrcL, i64:$SrcR)>;

class PatSTOREri<ValueType VT, SDPatternOperator N, ComplexPattern A, Instruction I> :
    Pat<(N VT:$SrcD, (A i64:$SrcL, simm12i64:$Imm)), (I VT:$SrcD, i64:$SrcL, simm12i64:$Imm)>;

multiclass StorePat<ValueType VT, SDPatternOperator N, int shift, Instruction I> {
  def : PatSTORErr<VT, N, StoreADDRrrD_scaled<shift>, I>;
  def : PatSTORErr<VT, N, StoreADDRrrSW_scaled<shift>, !cast<Instruction>(I # _SW)>;
  def : PatSTORErr<VT, N, StoreADDRrrUW_scaled<shift>, !cast<Instruction>(I # _UW)>;
  def : PatSTORErr<VT, N, StoreADDRrrD_unscaled, !cast<Instruction>(I # _U)>;
  def : PatSTORErr<VT, N, StoreADDRrrSW_unscaled, !cast<Instruction>(I # _U_SW)>;
  def : PatSTORErr<VT, N, StoreADDRrrUW_unscaled, !cast<Instruction>(I # _U_UW)>;
  def : PatSTOREri<VT, N, ADDRri_scaled<shift>, !cast<Instruction>(I # "I")>;
  def : PatSTOREri<VT, N, ADDRri_unscaled, !cast<Instruction>(I # "I_U")>;
}

multiclass StorePat_unscale<ValueType VT, SDPatternOperator N, Instruction I> {
  def : PatSTORErr<VT, N, StoreADDRrrD_unscaled, !cast<Instruction>(I)>;
  def : PatSTORErr<VT, N, StoreADDRrrSW_unscaled, !cast<Instruction>(I # _SW)>;
  def : PatSTORErr<VT, N, StoreADDRrrUW_unscaled, !cast<Instruction>(I # _UW)>;
  def : PatSTOREri<VT, N, ADDRri_unscaled, !cast<Instruction>(I # "I")>;
}

let Predicates = [isScalar, useDAGISel] in {
defm : StorePat<i64, truncstorei32, 2, SW>;
defm : StorePat<i64, truncstorei16, 1, SH>;
defm : StorePat_unscale<i64, truncstorei8, SB>;
defm : StorePat<i64, store, 3, SD>;
}

let Predicates = [isScalar, useDAGISel, hasCSel] in {
// TODO: Fixup the defination of CSEL to follow the operand sequence of asm
def : Pat<(select i64:$SrcP, i64:$SrcL, i64:$SrcR), (CSEL i64:$SrcL, i64:$SrcR, i64:$SrcP)>;
}

let Predicates = [isScalar, useDAGISel] in {
def : PatRR<sdiv, DIV>;
def : PatRR<udiv, DIVU>;
def : PatRR<srem, REM>;
def : PatRR<urem, REMU>;

def : PatRR<linxv5_divw, DIVW>;
def : PatRR<linxv5_divuw, DIVUW>;
def : PatRR<linxv5_remuw, REMUW>;


// Handle the specific cases where using DIVU/REMU would be correct and result
// in fewer instructions than emitting DIVUW/REMUW then zero-extending the
// result.
def : Pat<(and (linxv5_divuw (assertzexti32 i64:$rs1),
                            (assertzexti32 i64:$rs2)), 0xffffffff),
          (DIVU i64:$rs1, i64:$rs2)>;
def : Pat<(and (linxv5_remuw (assertzexti32 i64:$rs1),
                            (assertzexti32 i64:$rs2)), 0xffffffff),
          (REMU i64:$rs1, i64:$rs2)>;

// Although the sexti32 operands may not have originated from an i32 srem,
// this pattern is safe as it is impossible for two sign extended inputs to
// produce a result where res[63:32]=0 and res[31]=1.
def : Pat<(srem (sexti32 i64:$rs1), (sexti32 i64:$rs2)),
          (REMW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (srem (sexti32 i64:$rs1),
                            (sexti32 i64:$rs2)), i32),
          (REMW i64:$rs1, i64:$rs2)>;
}

def PatBXU : ComplexPattern<i64, 3, "SelectBXU", [srl, and]>;
def PatBXS : ComplexPattern<i64, 3, "SelectBXFromShrShl<true>", [sra]>;
def PatBIC : ComplexPattern<i64, 3, "SelectBIC", [and]>;
def PatBIS : ComplexPattern<i64, 3, "SelectBIS", [or]>;

let Predicates = [isScalar, useDAGISel], AddedComplexity = 10 in {
def : Pat<(PatBXU i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), (BXU i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN)>;
def : Pat<(PatBXS i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), (BXS i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN)>;
def : Pat<(PatBIC i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), (BIC i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN)>;
def : Pat<(PatBIS i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN), (BIS i64:$SrcL, uimm6:$ImmM, uimm6_plus1:$ImmN)>;

// PTO ISA ctz/clz/bcnt SrcL, M, N, ->Rd over an N-bit field starting at M
// (wrapping). Lower the LLVM count ops to the full XLEN-wide field:
// M=0, N=XLEN, so the encode input for the N-1 field is 64 (uimm6_plus1
// encodes Imm-1). The ISA ctz/clz return N when the field is all-zero,
// satisfying both CTTZ (must be XLEN) and CTTZ_ZERO_UNDEF (any value).
def : Pat<(i64 (cttz i64:$SrcL)), (CTZ i64:$SrcL, 0, 64)>;
def : Pat<(i64 (ctlz i64:$SrcL)), (CLZ i64:$SrcL, 0, 64)>;
def : Pat<(i64 (ctpop i64:$SrcL)), (BCNT i64:$SrcL, 0, 64)>;
}

// ===---------------------- Float DAG ISel Patterns ------------------------===

class PatRRVT<ValueType VT, SDPatternOperator N, Instruction I> :
    Pat<(N VT:$SrcL, VT:$SrcR), (I VT:$SrcL, VT:$SrcR)>;

multiclass fPatRR<SDPatternOperator N, string I> {
  def : PatRRVT<f64, N, !cast<Instruction>(I # _FD)>;
  def : PatRRVT<f32, N, !cast<Instruction>(I # _FS)>;
  def : PatRRVT<f16, N, !cast<Instruction>(I # _FH)>;
}

class PatRVT<ValueType VT, SDPatternOperator N, Instruction I> :
    Pat<(N VT:$SrcL), (I VT:$SrcL)>;

multiclass fPatR<SDPatternOperator N, string I> {
  def : PatRVT<f64, N, !cast<Instruction>(I # _FD)>;
  def : PatRVT<f32, N, !cast<Instruction>(I # _FS)>;
  def : PatRVT<f16, N, !cast<Instruction>(I # _FH)>;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
defm : fPatRR<fadd, "FADD">;
defm : fPatRR<fsub, "FSUB">;
defm : fPatRR<fmul, "FMUL">;
defm : fPatRR<fdiv, "FDIV">;
defm : fPatR<fsqrt, "FSQRT">;
}

class fPatNEG<ValueType VT, Instruction I> :
    Pat<(VT (fneg VT:$Src)), (I (VT R0), VT:$Src)>;

let Predicates = [isScalar, hasFloat, useDAGISel] in {
def : fPatNEG<f64, FSUB_FD>;
def : fPatNEG<f32, FSUB_FS>;
def : fPatNEG<f16, FSUB_FH>;
}

// TODO: the operand sequence of FMADD_* is not same as fmadd.*
class fPatMACC<ValueType VT, SDPatternOperator N, Instruction I> :
    Pat<(N VT:$SrcL, VT:$SrcR, VT:$SrcA), (I VT:$SrcA, VT:$SrcL, VT:$SrcR)>;

multiclass fPatMACC<SDPatternOperator N, string I> {
  def : fPatMACC<f64, N, !cast<Instruction>(I # _FD)>;
  def : fPatMACC<f32, N, !cast<Instruction>(I # _FS)>;
  def : fPatMACC<f16, N, !cast<Instruction>(I # _FH)>;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
defm : fPatMACC<any_fma, "FMADD">;
// a * b + (- c)
defm : fPatMACC<TriOpFrag<(any_fma node:$LHS, node:$MHS, (fneg node:$RHS))>, "FMSUB">;
// -(a * b + c)
defm : fPatMACC<TriOpFrag<(fneg (any_fma node:$LHS, node:$MHS, node:$RHS))>, "FNMADD">;
// a * (-b) + c
defm : fPatMACC<TriOpFrag<(any_fma node:$LHS, (fneg node:$MHS), node:$RHS)>, "FNMSUB">;
// a * (-b) + (-c)
defm : fPatMACC<TriOpFrag<(any_fma node:$LHS, (fneg node:$MHS), (fneg node:$RHS))>, "FNMADD">;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
defm : fPatRR<setoeq, "FEQ">;
defm : fPatRR<seteq, "FEQ">;
defm : fPatRR<setone, "FNE">;
defm : fPatRR<setne, "FNE">;
defm : fPatRR<setolt, "FLT">;
defm : fPatRR<setlt, "FLT">;
defm : fPatRR<setoge, "FGE">;
defm : fPatRR<setge, "FGE">;

defm : fPatRR<linxv5_setcc_swap<setole>, "FGE">;
defm : fPatRR<linxv5_setcc_swap<setle>, "FGE">;
defm : fPatRR<linxv5_setcc_swap<setogt>, "FLT">;
defm : fPatRR<linxv5_setcc_swap<setgt>, "FLT">;
}

class PatICVT<ValueType VTDst, SDPatternOperator N, Instruction I> :
    Pat<(VTDst (N i64:$Src)), (VTDst (I i64:$Src))>;

class PatFCVTI<ValueType VTSrc, SDPatternOperator N, Instruction I> :
    Pat<(i64 (N VTSrc:$Src)), (I VTSrc:$Src)>;

class PatFCVTF<ValueType VTDst, ValueType VTSrc, SDPatternOperator N, Instruction I> :
    Pat<(VTDst (N VTSrc:$Src)), (I VTSrc:$Src)>;

// reserve
class PatFCVTIR<ValueType VTSrc, SDPatternOperator N, SDNode round, Instruction I> :
  Pat<(i64 (N (round VTSrc:$Src))), (I VTSrc:$Src)>;

multiclass PatIEXTCVT<SDPatternOperator N, SDPatternOperator X, string I> {
  def : Pat<(f64 (N (X i64:$Src))), (!cast<Instruction>(I # 2FD) i64:$Src)>;
  def : Pat<(f32 (N (X i64:$Src))), (!cast<Instruction>(I # 2FS) i64:$Src)>;
  def : Pat<(f16 (N (X i64:$Src))), (!cast<Instruction>(I # 2FH) i64:$Src)>;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
def  : PatICVT<f64, uint_to_fp, UCVTF_UD2FD>;
def  : PatICVT<f32, uint_to_fp, UCVTF_UD2FS>;
def  : PatICVT<f16, uint_to_fp, UCVTF_UD2FH>;
def  : PatICVT<f64, sint_to_fp, SCVTF_SD2FD>;
def  : PatICVT<f32, sint_to_fp, SCVTF_SD2FS>;
def  : PatICVT<f16, sint_to_fp, SCVTF_SD2FH>;
defm : PatIEXTCVT<uint_to_fp, uw_ext, "UCVTF_UW">;
defm : PatIEXTCVT<uint_to_fp, uh_ext, "UCVTF_UH">;
defm : PatIEXTCVT<uint_to_fp, ub_ext, "UCVTF_UB">;
defm : PatIEXTCVT<sint_to_fp, sw_ext, "SCVTF_SW">;
defm : PatIEXTCVT<sint_to_fp, sh_ext, "SCVTF_SH">;
defm : PatIEXTCVT<sint_to_fp, sb_ext, "SCVTF_SB">;

def : PatFCVTF<f64, f32, any_fpextend, FCVT_FS2FD>;
def : PatFCVTF<f64, f16, any_fpextend, FCVT_FH2FD>;
def : PatFCVTF<f32, f64, any_fpround,  FCVT_FD2FS>;
def : PatFCVTF<f32, f16, any_fpextend, FCVT_FH2FS>;
def : PatFCVTF<f16, f64, any_fpround,  FCVT_FD2FH>;
def : PatFCVTF<f16, f32, any_fpround,  FCVT_FS2FH>;

def : PatFCVTI<f64, fp_to_uint, FCVTZ_FD2UD>;
def : PatFCVTI<f32, fp_to_uint, FCVTZ_FS2UD>;
def : PatFCVTI<f16, fp_to_uint, FCVTZ_FH2UD>;
def : PatFCVTI<f64, linxv5_fcvtu, FCVTZ_FD2UD>;
def : PatFCVTI<f32, linxv5_fcvtu, FCVTZ_FS2UD>;
def : PatFCVTI<f16, linxv5_fcvtu, FCVTZ_FH2UD>;
def : PatFCVTI<f64, fp_to_sint, FCVTZ_FD2SD>;
def : PatFCVTI<f32, fp_to_sint, FCVTZ_FS2SD>;
def : PatFCVTI<f16, fp_to_sint, FCVTZ_FH2SD>;
def : PatFCVTI<f64, linxv5_fcvts, FCVTZ_FD2SD>;
def : PatFCVTI<f32, linxv5_fcvts, FCVTZ_FS2SD>;
def : PatFCVTI<f16, linxv5_fcvts, FCVTZ_FH2SD>;

def : Pat<(i64 (bitconvert f64:$SrcL)), (i64 (COPY f64:$SrcL))>;
def : Pat<(f64 (bitconvert i64:$SrcL)), (f64 (COPY i64:$SrcL))>;
def : Pat<(i64 (linxv5_bitcast f32:$SrcL)), (i64 (COPY f32:$SrcL))>;
def : Pat<(f32 (linxv5_bitcast i64:$SrcL)), (f32 (COPY i64:$SrcL))>;
def : Pat<(i64 (linxv5_bitcast f16:$SrcL)), (i64 (COPY f16:$SrcL))>;
def : Pat<(f16 (linxv5_bitcast i64:$SrcL)), (f16 (COPY i64:$SrcL))>;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
defm : LoadPat<f64, load, 3, LD>;
defm : LoadPat<f32, load, 2, LW>;
defm : LoadPat<f16, load, 1, LH>;

defm : StorePat<f64, store, 3, SD>;
defm : StorePat<f32, store, 2, SW>;
defm : StorePat<f16, store, 1, SH>;
}

let Predicates = [isScalar, hasFloat, useDAGISel] in {
def : Pat<(select i64:$SrcP, f64:$SrcL, f64:$SrcR), (CSEL f64:$SrcL, f64:$SrcR, i64:$SrcP)>;
def : Pat<(select i64:$SrcP, f32:$SrcL, f32:$SrcR), (CSEL f32:$SrcL, f32:$SrcR, i64:$SrcP)>;
def : Pat<(select i64:$SrcP, f16:$SrcL, f16:$SrcR), (CSEL f16:$SrcL, f16:$SrcR, i64:$SrcP)>;
}

def B_DIM : BlockModifierBase {
  field bits<32> SoftFail = 0;
  field bits<32> Inst;
  let isDisassembleOnly = 0;
  let Size = 4;

  bits<2> DstLoopReg;
  let OutOperandList = (outs LoopBRegDst:$DstLoopReg);
  let InOperandList = (ins GPRSrc:$Src, uimm17:$imm17);
  bits<17> imm17;
  bits<5> Src;


  let Inst{31-20} = imm17{11-0};
  let Inst{19-15} = Src;
  let Inst{14} = 0b0;
  let Inst{13-12} = DstLoopReg;
  let Inst{11-7} = imm17{16-12};
  let Inst{6-4} = 0b100;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;

  let AsmString = "B.DIM" # "\t$Src, $imm17, $DstLoopReg";
}

def C_B_DIMI : BlockModifierBase {
  field bits<16> SoftFail = 0;
  field bits<16> Inst;
  let Size = 2;
  let isDisassembleOnly = 0;

  bits<2> DstLoopReg;
  bits<8> imm8;
  let OutOperandList = (outs LoopBRegDst:$DstLoopReg);
  let InOperandList = (ins uimm8:$imm8);
  let Inst{15-14} = DstLoopReg;
  let Inst{13-6} = imm8;
  let Inst{5-4} = 0b11;
  let Inst{3-0} = 0b1100;

  let AsmString = "C.B.DIMI" # "\t$imm8, $DstLoopReg";
}

// v5: C.B.IOS replaces v4 C.B.DIM RegSrc form. It is a 16-bit compressed
// Shared operand binder carrying a 6-bit absolute Shared architectural ID;
// it cannot execute alone — the next compatible B.IOT/B.IOR consumes it at
// decode/rename. v4 C.B.DIMI (immediate) is kept; dynamic dimension uses
// 32-bit B.DIM. (header/C.B.IOS.md)
def SharedTIDAsmOperand : AsmOperandClass {
  let Name = "SharedTID";
  let ParserMethod = "parseSharedTID";
  let RenderMethod = "addImmOperands";
}
def SharedTIDOp : Operand<i64>, ImmLeaf<i64, [{return isUInt<6>(Imm);}]> {
  let ParserMatchClass = SharedTIDAsmOperand;
  let PrintMethod = "printSharedTID";
  let EncoderMethod = "getImmOpValueSharedTID";
}
// Destination Shared operand: "->S17" (arrow + absolute ID). Used by B.IOS
// destination form. The arrow is part of the operand (like TILE_DstWithArrow),
// so the AsmMatcher matches it as a single operand class.
def SharedTIDWithArrowAsmOperand : AsmOperandClass {
  let Name = "SharedTIDWithArrow";
  let ParserMethod = "parseSharedTIDWithArrow";
  let RenderMethod = "addImmOperands";
}
def SharedTIDWithArrowOp : Operand<i64>, ImmLeaf<i64, [{return isUInt<6>(Imm);}]> {
  let ParserMatchClass = SharedTIDWithArrowAsmOperand;
  let PrintMethod = "printSharedTIDWithArrow";
  let EncoderMethod = "getImmOpValueSharedTID";
}
def SharedRegOp : RegisterOperand<Shared_ABS>;
class B_IOT_Base<bits<3> Func, dag outs, dag ins, string opcodestr="", string argstr="">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
let Format = InstFormatBlockModifier;

  let Size = 4;
  let isDisassembleOnly = 0;
  // v5 encoding (replaces v4):
  //   [31:26] SrcTile1 (6b), [25:20] SrcTile0 (6b), [19] last,
  //   [18:15] SizeCode (4b), [14:12] Func (3b), [11:9] PEMode (3b),
  //   [8:7] DstTile (2b: T/U/M/N), [6:4]/[3:1] fixed, [0] fixed 1.
  // PTO-ISA ADR 0069 (commit 1e91bf9): [18:15] carries the 4-bit SizeCode
  // (0 = source-only, 1..12 = destination capacity 128B..256KB per PE; 13..15
  // reserved) and [11:9] carries the 3-bit PEMode expanded to a 4-PE mask.
  // The TSize operand drives the SizeCode field; the PE_MASK operand drives
  // the PEMode field (the PE_MASK encoder converts a 4-bit mask to PEMode).
  // v5 replaces v4 Group bit[19] with last, size imm4[18:15] with SizeCode,
  // and deletes source reuse (S0R/S1R bit{6}). acc is not encoded in B.IOT.
  // The GroupOp operand is reused for the last flag (its semantics: ", last").
  bits<4> TSize;      // SizeCode value 0..12 (13..15 reserved)
  bits<3> PE_MASK;    // PEMode value (encoder maps a 4-bit mask to the 3-bit mode)
  bits<6> SrcTile1;
  bits<6> SrcTile0;
  bits<2> DstTile;
  bits<1> Last;

  let Inst{31-26} = SrcTile1;
  let Inst{25-20} = SrcTile0;
  let Inst{19} = Last;
  let Inst{18-15} = TSize;    // SizeCode field
  let Inst{14-12} = Func;
  let Inst{11-9} = PE_MASK;   // PEMode field (encoder returns 3-bit mode)
  let Inst{8-7} = DstTile;
  let Inst{6-4} = 0b001;
  let Inst{3-1} = 0b001;
  let Inst{0} = 0b1;
}

def TileRegAsmOperand : AsmOperandClass {
  let Name = "TileReg";
  let ParserMethod = "parseTileReg";
  let RenderMethod = "addRegOperands";
}

def TileDstNoArrowAsmOperand : AsmOperandClass {
  let Name = "TileDstNoArrow";
  let ParserMethod = "parseTileReg";
  let RenderMethod = "addRegOperands";
}

def TileRegWithArrowAsmOperand : AsmOperandClass {
  let Name = "TileRegWithArrow";
  let ParserMethod = "parseTileRegWithArrow";
  let RenderMethod = "addRegOperands";
}

def GPRWithBracketAsmOperand : AsmOperandClass {
  let Name = "GPRWithBracket";
  let ParserMethod = "parseGPRWithBracket";
  let RenderMethod = "addRegOperands";
}

def TileSizeWithBracketAsmOperand : AsmOperandClass {
  let Name = "TileSizeWithBracket";
  let ParserMethod = "parseTileSizeWithBracket";
  let RenderMethod = "addImmOperands";
}

// v5: B.IOT destination-suffix SizeCode (e.g. "->t<8KB>"). Matches
// 1..12 (B.IOT destination capacity; 0 is source, 13..15 reserved).
def B_IOT_TileSizeAsmOperand : AsmOperandClass {
  let Name = "B_IOT_TileSize";
  let ParserMethod = "parseTileSizeWithBracket";
  let RenderMethod = "addImmOperands";
}

// v5: B.IOS SizeCode. Matches 0..12 (0 = source, 1..12 destination up to
// 256KB; 13..15 reserved).
def B_IOS_TileSizeAsmOperand : AsmOperandClass {
  let Name = "B_IOS_TileSize";
  let ParserMethod = "parseTileSizeWithBracket";
  let RenderMethod = "addImmOperands";
}

def TILE_Src_Reg : RegisterOperand<TILE_SRC> {
  let ParserMatchClass = TileRegAsmOperand;
  let PrintMethod = "printTileSrcReg";
}

def ACCTileRegAsmOperand : AsmOperandClass {
  let Name = "ACCTileReg";
  let ParserMethod = "parseTileReg";
  let RenderMethod = "addRegOperands";
}

def ACC_TILE_SRC : RegisterOperand<TILE_SRC_MMA> {
  let ParserMatchClass = ACCTileRegAsmOperand;
  let PrintMethod = "printTileSrcReg";
}

def TILE_DstWithArrow : RegisterOperand<TILE_DST> {
  let ParserMatchClass = TileRegWithArrowAsmOperand;
  let PrintMethod = "printTileDstReg";
}

def Tile_DstNoArrow : RegisterOperand<TILE_DST> {
  let ParserMatchClass = TileDstNoArrowAsmOperand;
  let PrintMethod = "printTileSrcReg";
}

def ACC_TileRegWithArrowAsmOperand : AsmOperandClass {
  let Name = "ACC_TileRegWithArrow";
  let ParserMethod = "parseTileRegWithArrow";
  let RenderMethod = "addRegOperands";
}

def ACC_TILE_DstWithArrow : RegisterOperand<ACC_DST> {
  let ParserMatchClass = ACC_TileRegWithArrowAsmOperand;
  let PrintMethod = "printTileDstReg";
}

def Stack_TileRegWithArrowAsmOperand : AsmOperandClass {
  let Name = "Stack_TileRegWithArrow";
  let ParserMethod = "parseTileRegWithArrow";
  let RenderMethod = "addRegOperands";
}

def Stack_DstWithArrow : RegisterOperand<TILE_DSTS> {
  let ParserMatchClass = Stack_TileRegWithArrowAsmOperand;
  let PrintMethod = "printTileDstReg";
}

def Stack_TileRegNoArrowAsmOperand : AsmOperandClass {
  let Name = "Stack_TileRegNoArrow";
  let ParserMethod = "parseTileReg";
  let RenderMethod = "addRegOperands";
}

def Stack_DstNoArrow : RegisterOperand<TILE_DSTS> {
  let ParserMatchClass = Stack_TileRegNoArrowAsmOperand;
  let PrintMethod = "printTileDstReg";
}

def GPRWithBracket : RegisterOperand<MCSrc> {
  let ParserMatchClass = GPRWithBracketAsmOperand;
  let PrintMethod = "printGPRWithBracket";
}

def TileSizeWithBracket : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm);}]> {
  let ParserMatchClass = TileSizeWithBracketAsmOperand;
  let PrintMethod = "printTileSizeWithBracket";
  let EncoderMethod = "getImmOpValueTileSize";
}

// v5: PE_MASK (4-bit, {PE0,PE1,PE2,PE3}, 1000=PE0..1111=all) and TSize
// (3-bit, 000=IMPLICIT, 001=128B..111=8KB). TSize is encoded at PE granularity;
// the hardware multiplies it by 4 to derive the full core size. These replace
// the v4 imm4 TileSize field. Custom AsmOperandClass + ParserMethod so the asm
// syntax "mask=N, TSize=N" is parsed as a whole token.
def PE_MASKAsmOperand : AsmOperandClass {
  let Name = "PE_MASK";
  let ParserMethod = "parsePE_MASK";
  let RenderMethod = "addImmOperands";
}
def PE_MASK_Op : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm);}]> {
  let ParserMatchClass = PE_MASKAsmOperand;
  let PrintMethod = "printPE_MASK";
  let EncoderMethod = "getImmOpValuePE_MASK";
  let DecoderMethod = "decodePEMode";
}
def TSizeAsmOperand : AsmOperandClass {
  let Name = "TSize";
  let ParserMethod = "parseTSize";
  let RenderMethod = "addImmOperands";
}
// TSize (legacy "TSize=N" spelling) now carries the 4-bit SizeCode value
// (0..12; 13..15 reserved and rejected by the parser/encoder). This stays
// wide for ISel-generated pseudo operands (PseudoV5GMOV/L2S/S2L), whose
// value is produced by calculateVCallSizeMask with the right role limit.
def TSize_Op : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm);}]> {
  let ParserMatchClass = TSizeAsmOperand;
  let PrintMethod = "printTSize";
  let EncoderMethod = "getImmOpValueTSize";
}

// B.IOT destination legacy "TSize=N" spelling must honor the Local SizeCode
// contract 1..12 (128B..256KB per PE). 0 is the source form (a destination
// spelling with TSize=0 is illegal) and 13..15 reserved. Used only by the
// B.IOT_*_Dst legacy aliases; PseudoV5* keep the wide TSize_Op above.
def B_IOT_TSizeAsmOperand : AsmOperandClass {
  let Name = "B_IOT_TSize";
  let ParserMethod = "parseTSize";
  let RenderMethod = "addImmOperands";
}
def B_IOT_TSize_Op : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm) && Imm >= 1 && Imm <= 12;}]> {
  let ParserMatchClass = B_IOT_TSizeAsmOperand;
  let PrintMethod = "printTSize";
  let EncoderMethod = "getImmOpValueTSize";
}

// B.IOS destination SizeCode: 0 selects the source role, 1..12 a Shared
// destination capacity (128B..256KB per PE); 13..15 reserved. The
// DecoderMethod lets the disassembler accept 0 (source) and 1..12.
def BIOS_Dst_TSize_Op : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm) && Imm <= 12;}]> {
  let ParserMatchClass = B_IOS_TileSizeAsmOperand;
  let PrintMethod = "printTileSizeWithBracket";
  let EncoderMethod = "getImmOpValueTSize";
  let DecoderMethod = "decodeBIOSDstTSize";
}

// v5: B.IOT destination-suffix SizeCode operand (1..12 for B.IOT destination,
// 128 B..256 KB per PE; 0 is only the source form and 13..15 reserved).
def B_IOT_TileSize_Op : Operand<i64>, ImmLeaf<i64, [{return isUInt<4>(Imm) && Imm >= 1 && Imm <= 12;}]> {
  let ParserMatchClass = B_IOT_TileSizeAsmOperand;
  let PrintMethod = "printTileSizeWithBracket";
  let EncoderMethod = "getImmOpValueTSize";
  let DecoderMethod = "decodeBIOTDstSizeCode";
}

// PTO v0.58.4: 32-bit B.IOS Shared operand binder, replacing the retired
// 16-bit C.B.IOS. Carries an absolute Shared ID (S0..S63), the per-PE size
// (SizeCode) and the participation mode (PEMode). SizeCode=0 selects a Shared
// source; 1..12 selects a Shared destination capacity (128B..256KB per PE).
//   source:      B.IOS S12, mask=1111
//   destination: B.IOS mask=0011, ->S12<128B>
// Encoding (ADR 0097): [31:28]=0, [27:26]=0, [25:20]=SharedTID, [19]=0,
//           [18:15]=SizeCode (4b), [14:12]=funct3=001, [11:9]=PEMode (3b),
//           [8:7]=00, [6:0]=0x13. match=0x00001013, mask=0xf00871ff.
class B_IOS_Base<bits<3> Role, dag outs, dag ins, string argstr="">
    : LinxV5Inst<outs, ins, "B.IOS", argstr> {
  let Format = InstFormatBlockModifier;
  let Size = 4;
  let isDisassembleOnly = 0;

  bits<6> SharedTID;
  bits<4> TSize;      // SizeCode value 0..12 (13..15 reserved); 0 = source
  bits<3> PE_MASK;    // PEMode value (encoder maps a 4-bit mask to the 3-bit mode)

  let Inst{31-28} = 0b0000;
  let Inst{27-26} = 0b00;
  let Inst{25-20} = SharedTID;
  let Inst{19} = 0b0;
  let Inst{18-15} = TSize;   // SizeCode field
  let Inst{14-12} = Role;
  let Inst{11-9} = PE_MASK;  // PEMode field (encoder returns 3-bit mode)
  let Inst{8-7} = 0b00;
  let Inst{6-0} = 0b0010011;
}

// PTO v0.58 reissue Shared binder. TSize=0 selects a source
// ("B.IOS S17, mask=1111"); a nonzero TSize selects a destination
// ("B.IOS mask=0011, ->S12<128B>"). A single B_IOS instruction avoids a
// decoder ambiguity between the two roles (they share funct3=001/opcode=0x13
// and differ only in TSize). printInst prints the source or destination form
// from TSize; the two InstAliases below accept both textual inputs.
def B_IOS : B_IOS_Base<0b001, (outs),
        (ins SharedTIDWithArrowOp:$SharedTID, PE_MASK_Op:$PE_MASK,
             BIOS_Dst_TSize_Op:$TSize)> {
  // printInst overrides the printed form based on TSize.
  let AsmString = "B.IOS" # "\t$PE_MASK, $SharedTID$TSize";
}
// Source input: "B.IOS S17, mask=1111" (TSize defaults to 0).
def : InstAlias<"B.IOS\t$SharedTID, $PE_MASK",
    (B_IOS SharedTIDWithArrowOp:$SharedTID, PE_MASK_Op:$PE_MASK, 0), 0>;
// Destination input: "B.IOS mask=0011, ->S12<128B>". Must use the B.IOS
// SizeCode operand (1..12); both Local B.IOT and Shared B.IOS use the active
// 0.58.4 destination range.
def : InstAlias<"B.IOS\t$PE_MASK, $SharedTID$TSize",
    (B_IOS SharedTIDWithArrowOp:$SharedTID, PE_MASK_Op:$PE_MASK,
     BIOS_Dst_TSize_Op:$TSize), 0>;

def B_IOT_NoSrc_Dst: B_IOT_Base<0b110, (outs TILE_DstWithArrow:$DstTile),
        (ins PE_MASK_Op:$PE_MASK, B_IOT_TileSize_Op:$TSize, GroupOp:$Last)> {
  let SrcTile0 = 0;
  let SrcTile1 = 0;
  let Format = InstFormat_STACK_SIZE;
  // v5: TileSize prints as a destination suffix "<8KB>" rather than "TSize=N".
  let AsmString = "B.IOT" # "\t${PE_MASK}${Last}, $DstTile$TSize";
}
def : InstAlias<"B.IOT\t$PE_MASK, $TSize, $DstTile",
    (B_IOT_NoSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                     B_IOT_TSize_Op:$TSize, 0), 0>;
def : InstAlias<"B.IOT\t$PE_MASK, $DstTile$TSize",
    (B_IOT_NoSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                     B_IOT_TileSize_Op:$TSize, 0), 0>;
// v5 legacy compatibility: "TSize=N, last, ->dst" still accepted on input.
def : InstAlias<"B.IOT\t$PE_MASK, $TSize, $Last, $DstTile",
    (B_IOT_NoSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                     B_IOT_TSize_Op:$TSize, GroupOp:$Last), 0>;

def B_IOT_OneSrc_Dst: B_IOT_Base<0b101, (outs TILE_DstWithArrow:$DstTile),
        (ins PE_MASK_Op:$PE_MASK, B_IOT_TileSize_Op:$TSize, GroupOp:$Last, TILE_Src_Reg:$SrcTile0)> {
  let SrcTile1 = 0;
  let Format = InstFormat_STACK_SIZE;
  let AsmString = "B.IOT" # "\t$SrcTile0, ${PE_MASK}${Last}, $DstTile$TSize";
}
def : InstAlias<"B.IOT\t$SrcTile0, $PE_MASK, $TSize, $DstTile",
    (B_IOT_OneSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                      B_IOT_TSize_Op:$TSize, 0, TILE_Src_Reg:$SrcTile0), 0>;
def : InstAlias<"B.IOT\t$SrcTile0, $PE_MASK, $DstTile$TSize",
    (B_IOT_OneSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                      B_IOT_TileSize_Op:$TSize, 0,
                      TILE_Src_Reg:$SrcTile0), 0>;

def B_IOT_TwoSrc_Dst: B_IOT_Base<0b100, (outs TILE_DstWithArrow:$DstTile),
        (ins PE_MASK_Op:$PE_MASK, B_IOT_TileSize_Op:$TSize, GroupOp:$Last, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1)> {
  let Format = InstFormat_STACK_SIZE;
  let AsmString = "B.IOT" # "\t$SrcTile0, $SrcTile1, ${PE_MASK}${Last}, $DstTile$TSize";
}
def : InstAlias<"B.IOT\t$SrcTile0, $SrcTile1, $PE_MASK, $TSize, $DstTile",
    (B_IOT_TwoSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                      B_IOT_TSize_Op:$TSize, 0, TILE_Src_Reg:$SrcTile0,
                      TILE_Src_Reg:$SrcTile1), 0>;
def : InstAlias<"B.IOT\t$SrcTile0, $SrcTile1, $PE_MASK, $DstTile$TSize",
    (B_IOT_TwoSrc_Dst TILE_DstWithArrow:$DstTile, PE_MASK_Op:$PE_MASK,
                      B_IOT_TileSize_Op:$TSize, 0,
                      TILE_Src_Reg:$SrcTile0,
                      TILE_Src_Reg:$SrcTile1), 0>;

def B_IOT_OneSrc_NoDst: B_IOT_Base<0b101, (outs), (ins PE_MASK_Op:$PE_MASK, GroupOp:$Last, TILE_Src_Reg:$SrcTile0)> {
  let SrcTile1 = 0;
  let DstTile = 0b00;
  let TSize = 0;
  let AsmString = "B.IOT" # "\t$SrcTile0, ${PE_MASK}${Last}";
}
def : InstAlias<"B.IOT\t$SrcTile0", (B_IOT_OneSrc_NoDst 0b1111, 0, TILE_Src_Reg:$SrcTile0), 0>;
def : InstAlias<"B.IOT\t$SrcTile0, $PE_MASK",
    (B_IOT_OneSrc_NoDst PE_MASK_Op:$PE_MASK, 0,
     TILE_Src_Reg:$SrcTile0), 1>;

// PTO v0.58 reissue: no mask-only Shared B.IOT companion exists (B.IOT binds
// Local Tiles only). The retired no-src/no-dst TSize-only form is removed;
// Shared size/mask are carried by B.IOS.

def B_IOT_TwoSrc_NoDst: B_IOT_Base<0b100, (outs), (ins PE_MASK_Op:$PE_MASK, GroupOp:$Last, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1)> {
  let DstTile = 0b00;
  let TSize = 0;
  let AsmString = "B.IOT" # "\t$SrcTile0, $SrcTile1, ${PE_MASK}${Last}";
}
def : InstAlias<"B.IOT\t$SrcTile0, $SrcTile1", (B_IOT_TwoSrc_NoDst 0b1111, 0, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1), 0>;
def : InstAlias<"B.IOT\t$SrcTile0, $SrcTile1, $PE_MASK",
    (B_IOT_TwoSrc_NoDst PE_MASK_Op:$PE_MASK, 0,
     TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1), 1>;

// ===-------------------------- Tile Op -----------------------------===
def GPRPlusImmAsmOperand : AsmOperandClass {
  let Name = "GPRPlusImm";
  let ParserMethod = "parseGPRPlusImm";
  let RenderMethod = "addRegOperands";
}

def PlusImm17AsmOperand : AsmOperandClass {
  let Name = "PlusImm17";
  let ParserMethod = "parsePlusImm17";
  let RenderMethod = "addImmOperands";
}

def GPRPlusImm : RegisterOperand<MCSrc> {
  let ParserMatchClass = GPRPlusImmAsmOperand;
  let PrintMethod = "printGPRPlusImm";
}

def PlusImm17 : Operand<i64>, ImmLeaf<i64, [{return isUInt<17>(Imm);}]> {
  let ParserMatchClass = PlusImm17AsmOperand;
  let PrintMethod = "printPlusImm17";
}

class PseudoTileOplImpl<dag outs, dag ins> : Pseudo<outs, ins> {
  let isBarrier = 0;
  let hasSideEffects = 1;
  let isTerminator = 0;
  let isCodeGenOnly = 0;
  let Namespace = "LinxV5";
}

class PseudoVCalllImpl<dag outs, dag ins> : PseudoTileOplImpl<outs, ins> {
  let TOP = TileOp_VEC;
  let isHeaderOnly = 0;
}

def GPRListOperand : AsmOperandClass {
  let Name = "GPRList";
  let ParserMethod = "parseGPRList";
  let RenderMethod = "addRegsOperands";
}

def GPRList : Operand<i64> {
  let PrintMethod = "printGPRList";
  let ParserMatchClass = GPRListOperand;
  let MIOperandInfo = (ops variable_ops);
}
class PseudoMCalllImpl<dag outs, dag ins> : PseudoTileOplImpl<outs, ins> {
  let TOP = TileOp_MTC;
  let isHeaderOnly = 0;
}

class PseudoTEPLImpl<dag outs, dag ins> : PseudoTileOplImpl<outs, ins> {
  let TOP = TileOp_TEPL;
  let isHeaderOnly = 1;
}

include "LinxV5TileCall.td"

let isHeaderOnly = 1 in
def PseudoTCOPY : PseudoMCalllImpl<(outs TILE_DstWithArrow:$DstTile),
(ins TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile)> {
  let AsmString = "TCOPY\t$SrcTile, $DstTile$TileSize";
}

let isCodeGenOnly = 1, hasSideEffects = 1 in {
def PseudoV5GMOV : Pseudo<(outs TILE_DstWithArrow:$DstTile),
    (ins BstartDataType:$DataType, PE_MASK_Op:$PE_MASK,
         TSize_Op:$TSize, GPRSrc:$PeerTID, TILE_Src_Reg:$SrcTile), []>;
def PseudoV5SharedL2S : Pseudo<(outs SharedRegOp:$SharedDst),
    (ins TileOPTMA:$Function, BstartDataType:$DataType,
         PE_MASK_Op:$PE_MASK, TSize_Op:$TSize, TILE_Src_Reg:$SrcTile), []>;
def PseudoV5SharedS2L : Pseudo<(outs TILE_DstWithArrow:$DstTile),
    (ins TileOPTMA:$Function, BstartDataType:$DataType,
         SharedRegOp:$SharedSrc, PE_MASK_Op:$PE_MASK,
         TSize_Op:$TSize), []>;
}

let isHeaderOnly = 1, isCodeGenOnly = 1 in
def PseudoEmptyTile : PseudoTEPLImpl<(outs TILE_DstWithArrow:$DstTile),
(ins TileSizeWithBracket:$TileSize)> {}

let isHeaderOnly = 1 in
def PseudoEmptyTileASM : PseudoTEPLImpl<(outs TILE_DstWithArrow:$DstTile),
(ins GPRWithBracket:$TileSize)> {
  let AsmString = "VPAR\t$DstTile$TileSize";
}

def PseudoESAVE : PseudoTEPLImpl<(outs TILE_DstWithArrow:$DstTile),
(ins TileSizeWithBracket:$TileSize, TEPLMode:$Mode)> {
  let AsmString = "ESAVE\t$Mode, $DstTile$TileSize";
}

def PseudoERCOV : PseudoTEPLImpl<(outs ),
(ins TILE_Src_Reg:$SrcTile, TEPLMode:$Mode)> {
  let AsmString = "ERCOV\t$Mode, $SrcTile";
}

def PseudoTSpill : PseudoMCalllImpl<(outs), (ins TILE_Src_Reg:$SrcTile, GPRSrc:$SrcR)> {
  let isCodeGenOnly = 1;
}

def PseudoTReload : PseudoMCalllImpl<(outs TILE_DstWithArrow:$DstTile), (ins GPRSrc:$SrcR)> {
  let isCodeGenOnly = 1;
}

class PseudoMTClImpl<dag outs, dag ins> : PseudoTileOplImpl<outs, ins> {
  let TOP = TileOp_MTC;
  let isHeaderOnly = 1;
}

def PseudoTLOAD_noDsrc_noDdst : PseudoMTClImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataType,
  TileSizeWithBracket:$TileSize, BArgFormat:$Layout, PadValue:$PadValue, GPRList:$List)> {
    let AsmString = "TLOAD.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}, ${PadValue}>" #"\t[$List], $DstTile$TileSize";
}

def PseudoTSTORE_noDsrc_noDdst : PseudoMTClImpl<(outs),
  (ins TILE_Src_Reg:$SrcTile, GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK,
  BstartDataType:$DataType, BArgFormat:$Layout, GPRList:$List)> {
    let AsmString = "TSTORE.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}>, $SrcTile," #"\t[$List]";
}

def PseudoTLOAD_noDsrc_Ddst : PseudoMTClImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataType,
  TileSizeWithBracket:$TileSize, BArgFormat:$Layout, PadValue:$PadValue, GPRList:$List)> {
    let AsmString = "TLOAD.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}, ${PadValue}>" #"\t[$List], $DstTile$TileSize, d";
}

def PseudoTSTORE_noDsrc_Ddst : PseudoMTClImpl<(outs),
  (ins TILE_Src_Reg:$SrcTile, GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK,
  BstartDataType:$DataType, BArgFormat:$Layout, GPRList:$List)> {
    let AsmString = "TSTORE.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}>, $SrcTile," #"\t[$List], ->d";
}

def PseudoTLOAD_Dsrc_noDdst : PseudoMTClImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataType,
  TileSizeWithBracket:$TileSize, RegDepSrc:$DepSrc, BArgFormat:$Layout, PadValue:$PadValue, GPRList:$List)> {
    let AsmString = "TLOAD.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}, ${PadValue}>" #"\t[$List], ${DepSrc}, $DstTile$TileSize";
}

def PseudoTSTORE_Dsrc_noDdst : PseudoMTClImpl<(outs),
  (ins TILE_Src_Reg:$SrcTile, GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK,
  BstartDataType:$DataType, RegDepSrc:$DepSrc, BArgFormat:$Layout, GPRList:$List)> {
    let AsmString = "TSTORE.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}>, $SrcTile," #"\t[$List], ${DepSrc}";
}

def PseudoTLOAD_Dsrc_Ddst : PseudoMTClImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataType,
  TileSizeWithBracket:$TileSize, RegDepSrc:$DepSrc, BArgFormat:$Layout, PadValue:$PadValue, GPRList:$List)> {
    let AsmString = "TLOAD.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}, ${PadValue}>" #"\t[$List], ${DepSrc}, $DstTile$TileSize, d";
}

def PseudoTSTORE_Dsrc_Ddst : PseudoMTClImpl<(outs),
  (ins TILE_Src_Reg:$SrcTile, GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK,
  BstartDataType:$DataType, RegDepSrc:$DepSrc, BArgFormat:$Layout, GPRList:$List)> {
    let AsmString = "TSTORE.$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}>, $SrcTile," #"\t[$List], ${DepSrc}, ->d";
}

// Cube Call
class PseudoCCallImpl<dag outs, dag ins> : PseudoTileOplImpl<outs, ins> {
  let TOP = TileOp_CUBE;
  let isHeaderOnly = 1;
}

// v5 unified MATMUL: every variant writes an ordinary Local Tile result
// (TILE_DstWithArrow), which participates in normal T/U/M/N clockhand
// allocation. There is no dedicated ACC storage/register class; ".ACC"
// variants carry an explicit ordinary-Tile accumulator source instead.
class PseudoCUBE_MAMULB_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $DstTile$TileSize";
}

def PseudoMAMULB_SizeI : PseudoCUBE_MAMULB_Base_I<"TMATMUL">;

// v5: TMATMUL with a Shared Right operand. Same CUBE op (BSTART.CUBE TMATMUL)
// but the Right tile is supplied by a B.IOS binder (SharedTID, S0..S63)
// instead of a B.IOT source stream. A stays a normal Local TILE_Src_Reg and
// the result is an ordinary Local Tile (TILE_DstWithArrow). PE_MASK is forced
// to 1111 at MC expansion. Internal mnemonic is CodeGen-only; the public API
// keeps the name TMATMUL.
class PseudoCUBE_MAMULB_SharedRight_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, SharedRegOp:$SharedSrc)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SharedSrc, $DstTile$TileSize";
}

def PseudoMAMULB_SharedRight_SizeI : PseudoCUBE_MAMULB_SharedRight_Base_I<"TMATMUL.SHARED">;

class PseudoCUBE_MAMULBAC_Higher_Base_I<string TileOp> :
  PseudoCCallImpl<(outs ACC_TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, ACC_TILE_SRC:$SrcTile2)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $SrcTile2, $DstTile$TileSize";
}

def PseudoMAMULBAC_Higher_SizeI : PseudoCUBE_MAMULBAC_Higher_Base_I<"MAMULBAC_Higher">;

class PseudoCUBE_MAMULBAC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $SrcTile2, $DstTile$TileSize";
}

def PseudoMAMULBAC_SizeI : PseudoCUBE_MAMULBAC_Base_I<"TMATMUL.BIAS">;

class PseudoCUBE_MAMULBACC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$ACC)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $ACC, $DstTile$TileSize";
}

def PseudoMAMULBACC_SizeI : PseudoCUBE_MAMULBACC_Base_I<"TMATMUL.ACC">;

class PseudoCUBE_MAMULBMX_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $DstTile$TileSize";
}

def PseudoMAMULBMX_SizeI : PseudoCUBE_MAMULBMX_Base_I<"TMATMULMX">;

class PseudoCUBE_MAMULBMXB_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $DstTile$TileSize";
}

def PseudoMAMULBMXB_SizeI : PseudoCUBE_MAMULBMXB_Base_I<"TMATMULMX">;

class PseudoCUBE_MAMULBMXAC_Higher_Base_I<string TileOp> :
PseudoCCallImpl<(outs ACC_TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
       TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, ACC_TILE_SRC:$SrcTile4)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $DstTile$TileSize";
}

def PseudoMAMULBMXAC_Higher_SizeI : PseudoCUBE_MAMULBMXAC_Higher_Base_I<"MAMULBMXAC_Higher">;

class PseudoCUBE_MAMULBMXAC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
       TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $DstTile$TileSize";
}

def PseudoMAMULBMXAC_SizeI : PseudoCUBE_MAMULBMXAC_Base_I<"TMATMULMX.BIAS">;

class PseudoCUBE_MAMULBMXBAC_Higher_Base_I<string TileOp> :
  PseudoCCallImpl<(outs ACC_TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, ACC_TILE_SRC:$SrcTile3)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $DstTile$TileSize";
}

def PseudoMAMULBMXBAC_Higher_SizeI : PseudoCUBE_MAMULBMXBAC_Higher_Base_I<"MAMULBMXAC_Higher">;

class PseudoCUBE_MAMULBMXBAC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $DstTile$TileSize";
}

def PseudoMAMULBMXBAC_SizeI : PseudoCUBE_MAMULBMXBAC_Base_I<"TMATMULMX.BIAS">;

class PseudoCUBE_MAMULBMXACC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$ACC)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $ACC, $DstTile$TileSize";
}

def PseudoMAMULBMXACC_SizeI : PseudoCUBE_MAMULBMXACC_Base_I<"TMATMULMX.ACC">;

class PseudoCUBE_MAMULBMXBACC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$ACC)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $ACC, $DstTile$TileSize";
}

def PseudoMAMULBMXBACC_SizeI : PseudoCUBE_MAMULBMXBACC_Base_I<"TMATMULMX.ACC">;

// v5 TGEMV (Function 16-18, 20-22): PE-local M=1 GEMV. Local-only operand
// stream; no SharedTile/C.B.IOS/Core4 cooperative form. Reuses the TMATMUL
// B.IOT source schema (same source arity per variant). The accumulator-input
// variants (TGEMV_ACC/TGEMVMX_ACC) read an ordinary Local C tile as a
// TILE_Src_Reg, never a physical ACC singleton. M=1 is not enforced at MC
// level (it is an IR/ISel/TileOP verifier concern; see work-package F).
class PseudoCUBE_TGEMV_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $DstTile$TileSize";
}
def PseudoTGEMV_SizeI : PseudoCUBE_TGEMV_Base_I<"TGEMV">;

class PseudoCUBE_TGEMV_BIAS_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $SrcTile2, $DstTile$TileSize";
}
def PseudoTGEMV_BIAS_SizeI : PseudoCUBE_TGEMV_BIAS_Base_I<"TGEMV.BIAS">;

class PseudoCUBE_TGEMV_ACC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" #"\t$SrcTile0, $SrcTile1, $SrcTile2, $DstTile$TileSize";
}
def PseudoTGEMV_ACC_SizeI : PseudoCUBE_TGEMV_ACC_Base_I<"TGEMV.ACC">;

class PseudoCUBE_TGEMVMX_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $DstTile$TileSize";
}
def PseudoTGEMVMX_SizeI : PseudoCUBE_TGEMVMX_Base_I<"TGEMVMX">;

class PseudoCUBE_TGEMVMX_BIAS_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $DstTile$TileSize";
}
def PseudoTGEMVMX_BIAS_SizeI : PseudoCUBE_TGEMVMX_BIAS_Base_I<"TGEMVMX.BIAS">;

class PseudoCUBE_TGEMVMX_ACC_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataTypeA, BstartDataType:$DataTypeB,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, TILE_Src_Reg:$SrcTile1, TILE_Src_Reg:$SrcTile2, TILE_Src_Reg:$SrcTile3, TILE_Src_Reg:$SrcTile4)> {
    let AsmString = TileOp # "\t<M: $RegM$immM, N: $RegN$immN, K: $RegK$immK, ${DataTypeA}, ${DataTypeB}>" # "\t$SrcTile0, $SrcTile1, $SrcTile2, $SrcTile3, $SrcTile4, $DstTile$TileSize";
}
def PseudoTGEMVMX_ACC_SizeI : PseudoCUBE_TGEMVMX_ACC_Base_I<"TGEMVMX.ACC">;

// v5: ACCCVT is removed (status: "removed" in DavinciOO ISA; replaced by
// TMATMUL_ACC). The PseudoACCCVT_SizeI/SizeR defs are deleted so they no
// longer generate mnemonic-match entries in MatchTable0. Their presence at
// the end of the sorted mnemonic table broke equal_range for nearby mnemonics
// (e.g. c.b.ios). The base classes and linxv5_blk_acccvt SDNode are kept for
// now (ISelLowering/ISelDAGToDAG still reference BLK_ACCCVT); full removal is
// a subsequent cleanup.

class PseudoCUBE_TMOV_Base_I<string TileOp> :
  PseudoCCallImpl<(outs TILE_DstWithArrow:$DstTile),
  (ins GPRPlusImm:$RegM, PlusImm17:$immM, GPRPlusImm:$RegN, PlusImm17:$immN, GPRPlusImm:$RegK, PlusImm17:$immK, BstartDataType:$DataType,
  TileSizeWithBracket:$TileSize, TILE_Src_Reg:$SrcTile0, BArgFormat:$Layout, PadValue:$PadValue, BstartDataType:$SrcType)> {
  let AsmString = TileOp # ".$Layout\t<LB0: $RegM$immM, LB1: $RegN$immN, LB2: $RegK$immK, ${DataType}, ${PadValue}>" # "\t$SrcTile0, $DstTile$TileSize";
}

def PseudoTMOV_SizeI : PseudoCUBE_TMOV_Base_I<"TMOV">;

//===--------------------------------------LinxV5 48bits Scalar inst-----------------------------------------------===
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def HL_LUI: LinxV5InstBase {
  field bits<48> SoftFail = 0;
  field bits<48> Inst;
  let Size = 6;

  bits<32> Imm32;
  bits<5> Dst;
  let OutOperandList = (outs DstRWithArrow:$Dst);
  let InOperandList = (ins uimm32:$Imm32);

  let Inst{47-28} = Imm32{19-0};
  let Inst{27-23} = Dst;
  let Inst{22-20} = 0b001;
  let Inst{19-17} = 0b011;
  let Inst{16} = 1;
  let Inst{15-4} = Imm32{31-20};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = "hl.lui" # "\t$Imm32, $Dst";
}
let hasSideEffects = 1 in {
  def HL_SSR_GET : V5Inst_48_SSR<0b000, (outs DstRWithArrow:$Dst), (ins uimm24:$Imm24), "ssrget", ""> {
    let SrcL = 0b00000;
    let AsmString = "hl.ssrget\t$Imm24, $Dst";
  }
}

def HL_SSR_SET : V5Inst_48_SSR<0b001, (outs ), (ins GPRSrc:$SrcL, uimm24:$Imm24), "ssrset"> {
  let Dst = 0b00000;
  let AsmString = "hl.ssrset\t$SrcL, $Imm24";
}

class ALUri_48_Base<bits<3> Func, LinxV5InstOpcode opc, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E<type, opc, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm24), opcstr>;

multiclass ALUarithI_48<bits<3> Func, string opcstr, LinxV5InstType type> {
  def ""  : ALUri_48_Base<Func, OPC_Arithmetic_Operation_64_I, uimm24, opcstr, type>;
  def W   : ALUri_48_Base<Func, OPC_Arithmetic_Operation_32_I, uimm24, opcstr#"w", type>;
}

def HL_ADDI : ALUri_48_Base<0b000, OPC_Arithmetic_Operation_64_I, uimm24, "hl.addi", Type_010>;

def HL_ADDIW : ALUri_48_Base<0b000, OPC_Arithmetic_Operation_32_I, uimm24, "hl.addiw", Type_010>;
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
defm HL_SUBI : ALUarithI_48<0b001, "hl.subi", Type_010>;

multiclass ALUlogicI_48<bits<3> Func, string opcstr, LinxV5InstType type> {
  def ""  : ALUri_48_Base<Func, OPC_Arithmetic_Operation_64_I, simm_pshl<24, 0>, opcstr, type>;
  def W   : ALUri_48_Base<Func, OPC_Arithmetic_Operation_32_I, simm_pshl<24, 0>, opcstr#"w", type>;
}

defm HL_ANDI : ALUlogicI_48<0b010, "hl.andi", Type_010>;

let isReMaterializable = 1, isAsCheapAsAMove = 1 in {
  defm HL_ORI  : ALUlogicI_48<0b011, "hl.ori", Type_010>;
  defm HL_XORI : ALUlogicI_48<0b100, "hl.xori", Type_010>;
}

class CMPri_48_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E<type, OPC_Compare_Instruction_I, Func, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm24), opcstr>;

def HL_CMP_EQI : CMPri_48_Base<0b000, simm_pshl<24, 0>, "hl.cmp.eqi", Type_010>;
def HL_CMP_NEI : CMPri_48_Base<0b001, simm_pshl<24, 0>, "hl.cmp.nei", Type_010>;
def HL_CMP_LTI : CMPri_48_Base<0b100, simm_pshl<24, 0>, "hl.cmp.lti", Type_010>;
def HL_CMP_GEI : CMPri_48_Base<0b101, simm_pshl<24, 0>, "hl.cmp.gei", Type_010>;
def HL_CMP_LTUI : CMPri_48_Base<0b110, uimm24, "hl.cmp.ltui", Type_010>;
def HL_CMP_GEUI : CMPri_48_Base<0b111, uimm24, "hl.cmp.geui", Type_010>;
def HL_CMP_ANDI : CMPri_48_Base<0b010, simm_pshl<24, 0>, "hl.cmp.andi", Type_010>;
def HL_CMP_ORI  : CMPri_48_Base<0b011, simm_pshl<24, 0>, "hl.cmp.ori", Type_010>;


class SETCCond_ri_48_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_F<type, OPC_SetCommit_I, Func, (outs), (ins GPRSrc:$SrcL, Imm:$Imm29), opcstr> {
  bits<29> Imm29;

  let Imm24 = Imm29{28-5};
  let Imm5 = Imm29{4-0};
  let AsmString = opcstr # "\t$SrcL, $Imm29";
}

multiclass SETCCond_ri_48_Base<bits<3> Func, Operand Imm , string opcstr, LinxV5InstType type> {
  def "" : SETCCond_ri_48_Base<Func, Imm, opcstr, type>, P2I<NAME, 1>;
  let isBranch = 1, isTerminator = 1, hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
  def _BR : Pseudo<(outs), (ins GPRSrc:$rs1, Imm:$Imm12, simm26_lsb0_bnext:$Target), []>, P2I<NAME, 0>;
}

defm HL_SETC_EQI  : SETCCond_ri_48_Base<0b000, simm24_shln, "hl.setc.eqi", Type_010>;
defm HL_SETC_NEI  : SETCCond_ri_48_Base<0b001, simm24_shln, "hl.setc.nei", Type_010>;
defm HL_SETC_ANDI : SETCCond_ri_48_Base<0b010, simm24_shln, "hl.setc.andi", Type_010>;
defm HL_SETC_ORI  : SETCCond_ri_48_Base<0b011, simm24_shln, "hl.setc.ori", Type_010>;
defm HL_SETC_LTI  : SETCCond_ri_48_Base<0b100, simm24_shln, "hl.setc.lti", Type_010>;
defm HL_SETC_GEI  : SETCCond_ri_48_Base<0b101, simm24_shln, "hl.setc.gei", Type_010>;
defm HL_SETC_LTUI : SETCCond_ri_48_Base<0b110, uimm24_shln, "hl.setc.ltui", Type_010>;
defm HL_SETC_GEUI : SETCCond_ri_48_Base<0b111, uimm24_shln, "hl.setc.geui", Type_010>;

def HL_ADDTPC : LinxV5Inst_48_I<Type_011,0b000,(outs DstRWithArrowNoRA:$DstR),(ins simm32_addtpc:$imm32), "hl.addtpc">;

def HL_SETRET : LinxV5Inst_48_I<Type_011,0b000,(outs ),(ins uimm33_lsb0:$imm32), "hl.addtpc"> {
  let DstR = 0b01010;
  let AsmString = "hl.setret\t$imm32, ->ra";
}

class MUL_48_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_A_Z7<Func, type, OPC_MultiCycle_MUL_Instruction, (outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr>;

class MAD_48_Base<bits<3> Func, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_A<0b00, Func, type, OPC_MultiCycle_MUL_Instruction, (outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, GPRSrc:$SrcD), opcstr> {
    let AsmString = opcstr # "\t$SrcL, $SrcR, $SrcD, $DstR, $DstR1";
}

def HL_MUL   : MUL_48_Base<0b000, "hl.mul", Type_011>;
def HL_MULU  : MUL_48_Base<0b001, "hl.mulu", Type_011>;
def HL_MADD  : MAD_48_Base<0b110, "hl.madd", Type_011>;
def HL_MADDW : MAD_48_Base<0b111, "hl.maddw", Type_011>;

class DIV_48_Base<bits<3> Func, string opcstr, LinxV5InstType Type> :
    LinxV5Inst_48_C<Func, Type, OPC_MultiCycle_DIV_Instruction,(outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
    let Imm7 = 0b0000000;
}

def HL_DIV   : DIV_48_Base<0b000, "hl.div", Type_011>;
def HL_DIVU  : DIV_48_Base<0b001, "hl.divu", Type_011>;
def HL_DIVW  : DIV_48_Base<0b010, "hl.divw", Type_011>;
def HL_DIVUW : DIV_48_Base<0b011, "hl.divuw", Type_011>;
def HL_REM   : DIV_48_Base<0b100, "hl.rem", Type_011>;
def HL_REMU  : DIV_48_Base<0b101, "hl.remu", Type_011>;
def HL_REMW  : DIV_48_Base<0b110, "hl.remw", Type_011>;
def HL_REMUW : DIV_48_Base<0b111, "hl.remuw", Type_011>;

class Load_48_rri<bits<3> Func, LinxV5InstOpcode Opcode, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E2<type, Opcode, Func,(outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, Imm:$Imm22), opcstr> {
  let AsmString = opcstr # "\t[$SrcL, $Imm22], $DstR";
  let mayLoad = 1;
}

let mayLoad = 1 in
class Load_48_rrip<bits<3> Func, LinxV5InstOpcode Opcode, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E3<0b01, type, Opcode, Func,(outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, Imm:$Imm17), opcstr>;

let mayLoad = 1 in
class Load_48_rripr<bits<3> Func, LinxV5InstOpcode Opcode, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E3<0b10, type, Opcode, Func,(outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, Imm:$Imm17), opcstr>;

let mayLoad = 1 in
class Load_48_rripo<bits<3> Func, LinxV5InstOpcode Opcode, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_E3<0b11, type, Opcode, Func,(outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, Imm:$Imm17), opcstr>;

let mayLoad = 1 in
multiclass Load_48_rrr<bits<3> Func, LinxV5InstOpcode Opcode, RegWithSrcRType SrcROp, string opcstr, LinxV5InstType type>{
  def "_P"  : LinxV5Inst_48_B<0b01, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr#"p">;
  def "_PR" : LinxV5Inst_48_B<0b10, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr#".pr">;
  def "_PO" : LinxV5Inst_48_B<0b11, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR, GPRSrc:$DstR1), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr#".po">;
}

multiclass Load_48_Base<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  defm ""     : Load_48_rrr<Func, OPC_Load_Register_Offset, SrcRNONE, opcstr, type>;
  defm "_SW"  : Load_48_rrr<Func, OPC_Load_Register_Offset, SrcRSW, opcstr, type>;
  defm "_UW"  : Load_48_rrr<Func, OPC_Load_Register_Offset, SrcRUW, opcstr, type>;
  def "I"     : Load_48_rri<Func, OPC_Load_Immediate_Offset, Imm, opcstr#"i", type>;
  def "IP"    : Load_48_rrip<Func, OPC_Load_Immediate_Offset, Imm, opcstr#"ip", type>;
  def "I_PR"   : Load_48_rripr<Func, OPC_Load_Immediate_Offset, Imm, opcstr#"i.pr", type>;
  def "I_PO"   : Load_48_rripo<Func, OPC_Load_Immediate_Offset, Imm, opcstr#"i.po", type>;
}

multiclass Load_48_Base_With_UnScaled<bits<3> Func, Operand ScaleImm, string opcstr, LinxV5InstType type> {
  defm ""    : Load_48_Base<Func, ScaleImm, opcstr, type>;
  def "I_U"  : Load_48_rri<Func, OPC_Load_Unscaled, simm_pshl<22, 0>, opcstr#"i.u", type>;
  def "IP_U"    : Load_48_rrip<Func, OPC_Load_Unscaled, simm_pshl<17, 0>, opcstr#"ip.u", type>;
  def "I_UPR"    : Load_48_rripr<Func, OPC_Load_Unscaled, simm_pshl<17, 0>, opcstr#"i.upr", type>;
  def "I_UPO"    : Load_48_rripo<Func, OPC_Load_Unscaled, simm_pshl<17, 0>, opcstr#"i.upo", type>;
}

defm HL_LB : Load_48_rrr<0b000, OPC_Load_Register_Offset, SrcRNONE, "hl.lb", Type_100>;
defm HL_LB_SW : Load_48_rrr<0b000, OPC_Load_Register_Offset, SrcRSW, "hl.lb", Type_100>;
defm HL_LB_UW : Load_48_rrr<0b000, OPC_Load_Register_Offset, SrcRUW, "hl.lb", Type_100>;
def HL_LBI : Load_48_rri<0b000, OPC_Load_Immediate_Offset, simm_pshl<22, 0>, "hl.lbi", Type_100>;
def HL_LBIP : Load_48_rrip<0b000, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbip", Type_100>;
def HL_LBI_PR : Load_48_rripr<0b000, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbi.pr", Type_100>;
def HL_LBI_PO : Load_48_rripo<0b000, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbi.po", Type_100>;

defm HL_LBU : Load_48_rrr<0b100, OPC_Load_Register_Offset, SrcRNONE, "hl.lbu", Type_100>;
defm HL_LBU_SW : Load_48_rrr<0b100, OPC_Load_Register_Offset, SrcRSW, "hl.lbu", Type_100>;
defm HL_LBU_UW : Load_48_rrr<0b100, OPC_Load_Register_Offset, SrcRUW, "hl.lbu", Type_100>;
def HL_LBUI : Load_48_rri<0b100, OPC_Load_Immediate_Offset, simm_pshl<22, 0>, "hl.lbui", Type_100>;
def HL_LBUIP : Load_48_rrip<0b100, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbuip", Type_100>;
def HL_LBUI_PR : Load_48_rripr<0b100, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbui.pr", Type_100>;
def HL_LBUI_PO : Load_48_rripo<0b100, OPC_Load_Immediate_Offset, simm_pshl<17, 0>, "hl.lbui.po", Type_100>;
defm HL_LH : Load_48_Base_With_UnScaled<0b001,  simm_pshl<22, 1>, "hl.lh", Type_100>;
defm HL_LW : Load_48_Base_With_UnScaled<0b010,  simm_pshl<22, 2>, "hl.lw", Type_100>;
defm HL_LD : Load_48_Base_With_UnScaled<0b011,  simm_pshl<22, 3>, "hl.ld", Type_100>;
defm HL_LHU : Load_48_Base_With_UnScaled<0b101, simm_pshl<22, 1>, "hl.lhu", Type_100>;
defm HL_LWU : Load_48_Base_With_UnScaled<0b110, simm_pshl<22, 2>, "hl.lwu", Type_100>;

class Store_48_rri<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_D<Func, type, OPC_Store_I, (outs ), (ins GPRSrc:$SrcD, GPRSrc:$SrcR, Imm:$Imm22), opcstr> {
  let AsmString = opcstr # "\t$SrcD, [$SrcR, $Imm22]";
  let mayStore = 1;
}

let mayStore = 1 in {
  class Store_48_rrip<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_D2<0b01, Func, type, OPC_Store_I, (outs ), (ins GPRSrc:$SrcD, GPRSrc:$SrcD1, GPRSrc:$SrcR, Imm:$Imm17), opcstr>;
  class Store_48_rripr<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_D3<0b10, Func, type, OPC_Store_I, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcR, Imm:$Imm17), opcstr>;
  class Store_48_rripo<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> :
    LinxV5Inst_48_D3<0b11, Func, type, OPC_Store_I, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcR, Imm:$Imm17), opcstr>;
}

multiclass Store_48_rrip<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  def "_P"  : Store_48_rrip<Func, Imm, opcstr#"p", type>;
  def "_PR" : Store_48_rripr<Func, Imm, opcstr#".pr", type>;
  def "_PO"  : Store_48_rripo<Func, Imm, opcstr#".po", type>;
}

multiclass Store_48_rrip_u<bits<3> Func, Operand Imm, string opcstr, LinxV5InstType type> {
  def "_P_U"  : Store_48_rrip<Func, Imm, opcstr#"p.u", type>;
  def "_UPR" : Store_48_rripr<Func, Imm, opcstr#".upr", type>;
  def "_UPO"  : Store_48_rripo<Func, Imm, opcstr#".upo", type>;
}

def HL_SHI : Store_48_rri<0b001,   simm_pshl<22, 1>, "hl.shi", Type_100>;
def HL_SWI : Store_48_rri<0b010,   simm_pshl<22, 2>, "hl.swi", Type_100>;
def HL_SDI : Store_48_rri<0b011,   simm_pshl<22, 3>, "hl.sdi", Type_100>;
def HL_SBI : Store_48_rri<0b000,   simm_pshl<22, 0>, "hl.sbi", Type_100>;
def HL_SHI_U : Store_48_rri<0b101, simm_pshl<22, 0>, "hl.shi.u", Type_100>;
def HL_SWI_U : Store_48_rri<0b110, simm_pshl<22, 0>, "hl.swi.u", Type_100>;
def HL_SDI_U : Store_48_rri<0b111, simm_pshl<22, 0>, "hl.sdi.u", Type_100>;

defm HL_SHI : Store_48_rrip<0b001,   simm_pshl<17, 1>, "hl.shi", Type_100>;
defm HL_SWI : Store_48_rrip<0b010,   simm_pshl<17, 2>, "hl.swi", Type_100>;
defm HL_SDI : Store_48_rrip<0b011,   simm_pshl<17, 3>, "hl.sdi", Type_100>;
defm HL_SBI : Store_48_rrip<0b000,   simm_pshl<17, 0>, "hl.sbi", Type_100>;
defm HL_SHI : Store_48_rrip_u<0b101, simm_pshl<17, 0>, "hl.shi", Type_100>;
defm HL_SWI : Store_48_rrip_u<0b110, simm_pshl<17, 0>, "hl.swi", Type_100>;
defm HL_SDI : Store_48_rrip_u<0b111, simm_pshl<17, 0>, "hl.sdi", Type_100>;

let mayStore = 1 in
multiclass Store_48_rrr<bits<3> Func, LinxV5InstOpcode Opcode, RegWithSrcRType SrcROp, string opcstr, string explicit_lsl, LinxV5InstType type> {
  def "_P"  : LinxV5Inst_48_A2<0b01, SrcROp.Type, Func, type, Opcode, (outs ), (ins GPRSrc:$SrcD, GPRSrc:$SrcD1, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#"p", explicit_lsl>;
  def "_PR" : LinxV5Inst_48_A3<0b10, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#".pr", explicit_lsl>;
  def "_PO" : LinxV5Inst_48_A3<0b11, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#".po", explicit_lsl>;
}

let mayStore = 1 in
multiclass Store_48_rrru<bits<3> Func, LinxV5InstOpcode Opcode, RegWithSrcRType SrcROp, string opcstr, string explicit_lsl, LinxV5InstType type> {
  def "_P_U" : LinxV5Inst_48_A2<0b01, SrcROp.Type, Func, type, Opcode, (outs ), (ins GPRSrc:$SrcD, GPRSrc:$SrcD1, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#"p.u", explicit_lsl>;
  def "_UPR" : LinxV5Inst_48_A3<0b10, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#".upr", explicit_lsl>;
  def "_UPO" : LinxV5Inst_48_A3<0b11, SrcROp.Type, Func, type, Opcode, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcD, GPRSrc:$SrcL, SrcROp:$SrcR), opcstr#".upo", explicit_lsl>;
}

multiclass Store_48_Base<bits<3> Func, LinxV5InstOpcode Opcode, string opcstr, string explicit_lsl, LinxV5InstType type> {
  defm ""      : Store_48_rrr<Func, Opcode, SrcRNONE, opcstr, explicit_lsl, type>;
  defm "_SW"   : Store_48_rrr<Func, Opcode, SrcRSW, opcstr, explicit_lsl, type>;
  defm "_UW"   : Store_48_rrr<Func, Opcode, SrcRUW, opcstr, explicit_lsl, type>;
}

multiclass Store_48_Base_U<bits<3> Func, LinxV5InstOpcode Opcode, string opcstr, string explicit_lsl, LinxV5InstType type> {
  defm ""      : Store_48_rrru<Func, Opcode, SrcRNONE, opcstr, explicit_lsl, type>;
  defm "_SW"   : Store_48_rrru<Func, Opcode, SrcRSW, opcstr, explicit_lsl, type>;
  defm "_UW"   : Store_48_rrru<Func, Opcode, SrcRUW, opcstr, explicit_lsl, type>;
}

defm HL_SB : Store_48_Base<0b000, OPC_Store, "hl.sb", "", Type_100>;
defm HL_SH : Store_48_Base<0b001, OPC_Store, "hl.sh", "<<1", Type_100>;
defm HL_SW : Store_48_Base<0b010, OPC_Store, "hl.sw", "<<2", Type_100>;
defm HL_SD : Store_48_Base<0b011, OPC_Store, "hl.sd", "<<3", Type_100>;
defm HL_SH : Store_48_Base_U<0b101, OPC_Store, "hl.sh", "", Type_100>;
defm HL_SW : Store_48_Base_U<0b110, OPC_Store, "hl.sw", "", Type_100>;
defm HL_SD : Store_48_Base_U<0b111, OPC_Store, "hl.sd", "", Type_100>;

class LinxV5Inst_48_Load_Symbol<bits<3> Func, string opcstr>
    : V5Inst_48_base<(outs DstRWithArrow:$Dst), (ins simm29:$Imm29), opcstr, ""> {
    let Format = InstFormat_Load_Symbol_TARGET_29;
    bits<5> Dst;
     bits<29> Imm29;

  let Inst{47-31} = Imm29{16-0};
  let Inst{30-28} = Func;
  let Inst{27-23} = Dst;
  let Inst{22-20} = OPC_load_symbol.Value;
  let Inst{19-17} = Type_100.Value;
  let Inst{16}    = 0b1;
  let Inst{15-4}  = Imm29{28-17};
  let Inst{3-0}   = 0b1110;

  let AsmString = opcstr # "\t[$Imm29], " # "$Dst";
}

def HL_LB_PCR : LinxV5Inst_48_Load_Symbol<0b000, "hl.lb.pcr">;
def HL_LH_PCR : LinxV5Inst_48_Load_Symbol<0b001, "hl.lh.pcr">;
def HL_LW_PCR : LinxV5Inst_48_Load_Symbol<0b010, "hl.lw.pcr">;
def HL_LD_PCR : LinxV5Inst_48_Load_Symbol<0b011, "hl.ld.pcr">;
def HL_LBU_PCR : LinxV5Inst_48_Load_Symbol<0b100, "hl.lbu.pcr">;
def HL_LHU_PCR : LinxV5Inst_48_Load_Symbol<0b101, "hl.lhu.pcr">;
def HL_LWU_PCR : LinxV5Inst_48_Load_Symbol<0b110, "hl.lwu.pcr">;

class LinxV5Inst_48_Store_Symbol<bits<3> Func, string opcstr>
    : V5Inst_48_base<(outs ), (ins GPRSrc:$SrcL, simm29:$Imm29), opcstr, ""> {
  let Format = InstFormat_Store_Symbol_TARGET_29;
  bits<5> SrcL;
  bits<29> Imm29;

  let Inst{47-36} = Imm29{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = Imm29{16-12};
  let Inst{22-20} = OPC_store_symbol.Value;
  let Inst{19-17} = Type_100.Value;
  let Inst{16}    = 0b1;
  let Inst{15-4}  = Imm29{28-17};
  let Inst{3-0}   = 0b1110;

  let AsmString = opcstr # "\t$SrcL, [$Imm29]";
}

def HL_SB_PCR : LinxV5Inst_48_Store_Symbol<0b000, "hl.sb.pcr">;
def HL_SH_PCR : LinxV5Inst_48_Store_Symbol<0b001, "hl.sh.pcr">;
def HL_SW_PCR : LinxV5Inst_48_Store_Symbol<0b010, "hl.sw.pcr">;
def HL_SD_PCR : LinxV5Inst_48_Store_Symbol<0b011, "hl.sd.pcr">;

class LinxV5Inst_48_PRF_Base<bits<5> Model, RegWithSrcRType SrcROp, string opcstr> :
    LinxV5Inst_48_B_PRF<Model, SrcROp.Type, 0b00, (outs ), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr> {
    let DstR = 0b00000;
    let AsmString = opcstr # "\t[$SrcL, $SrcR" # "$Imm5]";
}

class LinxV5Inst_48_PRF_A_Base<bits<5> Model, RegWithSrcRType SrcROp, string opcstr> :
    LinxV5Inst_48_B_PRF<Model, SrcROp.Type, 0b01, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, SrcROp:$SrcR, ShamtImm:$Imm5), opcstr> {
    let AsmString = opcstr # "\t[$SrcL, $SrcR" # "$Imm5], $DstR";
}

multiclass LinxV5Inst_48_PRF_RType<string opcstr,bits<5> Model> {
  def ""   : LinxV5Inst_48_PRF_Base<Model, SrcRNONE, opcstr>;
  def _SW  : LinxV5Inst_48_PRF_Base<Model, SrcRSW, opcstr>;
  def _UW  : LinxV5Inst_48_PRF_Base<Model, SrcRUW, opcstr>;
}

multiclass LinxV5Inst_48_PRF_A_RType<string opcstr, bits<5> Model> {
  def ""   : LinxV5Inst_48_PRF_A_Base<Model, SrcRNONE, opcstr>;
  def _SW  : LinxV5Inst_48_PRF_A_Base<Model, SrcRSW, opcstr>;
  def _UW  : LinxV5Inst_48_PRF_A_Base<Model, SrcRUW, opcstr>;
}

defm HL_PRF_L1 : LinxV5Inst_48_PRF_RType<"hl.prf.l1", 0b00000>;
defm HL_PRF_L2 : LinxV5Inst_48_PRF_RType<"hl.prf.l2", 0b00001>;
defm HL_PRF_L3 : LinxV5Inst_48_PRF_RType<"hl.prf.l3", 0b00010>;

defm HL_PRF_A_L1 : LinxV5Inst_48_PRF_A_RType<"hl.prf.a.l1", 0b00000>;
defm HL_PRF_A_L2 : LinxV5Inst_48_PRF_A_RType<"hl.prf.a.l2", 0b00001>;
defm HL_PRF_A_L3 : LinxV5Inst_48_PRF_A_RType<"hl.prf.a.l3", 0b00010>;

class LinxV5Inst_48_PRFI_Base<bits<5> Model, string opcstr> :
    LinxV5Inst_48_E_PRFI<Model, 0b00, (outs ), (ins GPRSrc:$SrcL, simm17:$Imm17), opcstr> {
    let DstR = 0b00000;
    let AsmString = opcstr # "\t[$SrcL, $Imm17]";
}

class LinxV5Inst_48_PRFI_A_Base<bits<5> Model, string opcstr> :
    LinxV5Inst_48_E_PRFI<Model, 0b01, (outs DstRWithArrow:$DstR), (ins GPRSrc:$SrcL, simm17:$Imm17), opcstr> {
    let AsmString = opcstr # "\t[$SrcL, $Imm17], $DstR";
}

let PE = PET_SYS in {
def HL_PRFI_U_L1 : LinxV5Inst_48_PRFI_Base<0b00000, "hl.prfi.u.l1">;
def HL_PRFI_U_L2 : LinxV5Inst_48_PRFI_Base<0b00001, "hl.prfi.u.l2">;
def HL_PRFI_U_L3 : LinxV5Inst_48_PRFI_Base<0b00010, "hl.prfi.u.l3">;

def HL_PRFI_UA_L1 : LinxV5Inst_48_PRFI_A_Base<0b00000, "hl.prfi.ua.l1">;
def HL_PRFI_UA_L2 : LinxV5Inst_48_PRFI_A_Base<0b00001, "hl.prfi.ua.l2">;
def HL_PRFI_UA_L3 : LinxV5Inst_48_PRFI_A_Base<0b00010, "hl.prfi.ua.l3">;
}

class V5Inst_48_Atomic<bits<4> Flag, bits<3> Mask, string opstr>
    : V5Inst_48_A_D<Flag, Mask, (outs DstRWithArrow:$Dst),
                          (ins GPRSrc:$SrcL, GPRSrc:$SrcR, GPRSrc:$SrcD), opstr>;

multiclass V5Inst_48_Atomic_Mul<bits<4> Flag, string opcstr> {
  def ""  : V5Inst_48_Atomic<Flag, 0b000, opcstr>;
  def _AQ : V5Inst_48_Atomic<Flag, 0b010, opcstr # ".aq">;
  def _RL : V5Inst_48_Atomic<Flag, 0b001, opcstr # ".rl">;
  def _F : V5Inst_48_Atomic<Flag, 0b100, opcstr # ".f">;
  def _AQRL : V5Inst_48_Atomic<Flag, 0b011, opcstr # ".aqrl">;
  def _AQF : V5Inst_48_Atomic<Flag, 0b110, opcstr # ".aqf">;
  def _RLF : V5Inst_48_Atomic<Flag, 0b101, opcstr # ".rlf">;
  def _AQRLF : V5Inst_48_Atomic<Flag, 0b111, opcstr # ".aqrlf">;
}

let PE = PET_SYS in {
defm HL_CASB : V5Inst_48_Atomic_Mul<0b0000, "hl.casb">;
defm HL_CASH : V5Inst_48_Atomic_Mul<0b0001, "hl.cash">;
defm HL_CASW : V5Inst_48_Atomic_Mul<0b0010, "hl.casw">;
defm HL_CASD : V5Inst_48_Atomic_Mul<0b0011, "hl.casd">;
}

class V5Inst_48_Long_Imm<bits<3> Opcode, Operand Imm, string opcstr>
    : V5Inst_48_J<Opcode, (outs DstRWithArrow:$Dst), (ins Imm:$Imm32), opcstr>;

def HL_LIS : V5Inst_48_Long_Imm<0b000, simm32, "hl.lis">;
def HL_LIU : V5Inst_48_Long_Imm<0b001, uimm32, "hl.liu">;

class V5Inst_48_Mul_Arith<bits<3> Func, string opcstr>
    : V5Inst_48_M_A<Func, (outs DstRWithArrow:$Dst), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, uimm19:$Imm19), opcstr>;

let PE = PET_STD in {
def HL_MIADD : V5Inst_48_Mul_Arith<0b000, "hl.miadd">;
def HL_MISUB : V5Inst_48_Mul_Arith<0b001, "hl.misub">;
}

let PE = PET_STD in {
def HL_BFI :  V5Inst_48_M_A<0b010, (outs DstRWithArrow:$Dst), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, uimm6:$ImmM, uimm6_plus1:$ImmN), "hl.bfi"> {
  bits<6> ImmM;
  bits<6> ImmN;

  let Inst{47-41} = 0b0000000;
  let Inst{15-10} = ImmM;
  let Inst{9-4} = ImmN;
  let AsmString = "hl.bfi" # "\t$SrcL, $SrcR, $ImmM, $ImmN, $Dst";
}
}

class V5Inst_48_Contat<bits<3> Opcode, string opcstr>
    : V5Inst_48_M_A<Opcode, (outs DstRWithArrow:$Dst, GPRSrc:$Dst1), (ins GPRSrc:$SrcL, GPRSrc:$SrcR, uimm7:$Shamt), opcstr> {
      bits<7> Shamt;
      bits<5> Dst1;

      let Inst{47-41} = Shamt;
      let Inst{22-20} = 0b101;
      let Inst{15-11} = Dst1;
      let Inst{10-4} = 0b0000000;
      let AsmString =  opcstr # "\t$SrcL, $SrcR, $Shamt, $Dst, $Dst1";
    }
let PE = PET_STD in {
def HL_CCAT  : V5Inst_48_Contat<0b001, "hl.ccat">;
def HL_CCATW : V5Inst_48_Contat<0b010, "hl.ccatw">;
}

// The hl.qmt/hl.qpush instruction output register currently only accepts local registers(t/u).
class V5Inst_48_Queue<bits<4> Mask, bits<3> Opcode, string opcstr>
    : V5Inst_48_M_A<Opcode, (outs DstRWithArrow:$Dst), (ins GPRSrc:$SrcL, GPRSrc:$SrcR), opcstr> {
      let Inst{47-45} =0b000;
      let Inst{44-41} = Mask;
      let Inst{22-20} = 0b111;
      let Inst{15-11} = 0b00000;
      let Inst{10-4} = 0b0000000;
      let AsmString =  opcstr # "\t$SrcL, $SrcR, $Dst";
    }

let PE = PET_SYS in {
def HL_QMT     : V5Inst_48_Queue<0b0000, 0b000, "hl.qmt">;
def HL_QMT_I   : V5Inst_48_Queue<0b1000, 0b000, "hl.qmt.i">;
def HL_QMT_S   : V5Inst_48_Queue<0b0100, 0b000, "hl.qmt.s">;
def HL_QMT_R   : V5Inst_48_Queue<0b0010, 0b000, "hl.qmt.r">;
def HL_QMT_E   : V5Inst_48_Queue<0b0001, 0b000, "hl.qmt.e">;
def HL_QMT_IS  : V5Inst_48_Queue<0b1100, 0b000, "hl.qmt.is">;
def HL_QMT_IR  : V5Inst_48_Queue<0b1010, 0b000, "hl.qmt.ir">;
def HL_QMT_IE  : V5Inst_48_Queue<0b1001, 0b000, "hl.qmt.ie">;
def HL_QMT_ES  : V5Inst_48_Queue<0b0101, 0b000, "hl.qmt.es">;
def HL_QMT_ER  : V5Inst_48_Queue<0b0011, 0b000, "hl.qmt.er">;
def HL_QMT_IES : V5Inst_48_Queue<0b1101, 0b000, "hl.qmt.ies">;
def HL_QMT_IER : V5Inst_48_Queue<0b1011, 0b000, "hl.qmt.ier">;

def HL_QPUSH     : V5Inst_48_Queue<0b0000, 0b001, "hl.qpush">;
def HL_QPUSH_H   : V5Inst_48_Queue<0b0100, 0b001, "hl.qpush.h">;
def HL_QPUSH_R   : V5Inst_48_Queue<0b0010, 0b001, "hl.qpush.r">;
def HL_QPUSH_E   : V5Inst_48_Queue<0b0001, 0b001, "hl.qpush.e">;
def HL_QPUSH_HE  : V5Inst_48_Queue<0b0101, 0b001, "hl.qpush.he">;
def HL_QPUSH_HR  : V5Inst_48_Queue<0b0110, 0b001, "hl.qpush.hr">;
def HL_QPUSH_ER  : V5Inst_48_Queue<0b0011, 0b001, "hl.qpush.er">;
def HL_QPUSH_HER : V5Inst_48_Queue<0b0111, 0b001, "hl.qpush.her">;
}

class V5Inst_48_Queue_Qpop<bits<2> Mask, string opcstr>
    : V5Inst_48_M_A<0b010, (outs DstRWithArrow:$Dst, GPRSrc:$Dst1), (ins GPRSrc:$SrcL), opcstr> {
      bits<5> Dst1;

      let Inst{47-43} = 0b00000;
      let Inst{40-36} = 0b00000;
      let Inst{42-41} = Mask;
      let Inst{22-20} = 0b111;
      let Inst{15-11} = Dst1;
      let Inst{10-4} = 0b0000000;
      let AsmString =  opcstr # "\t$SrcL, $Dst, $Dst1";
    }

let PE = PET_SYS in {
def HL_QPOP    : V5Inst_48_Queue_Qpop<0b00, "hl.qpop">;
def HL_QPOP_R  : V5Inst_48_Queue_Qpop<0b10, "hl.qpop.r">;
def HL_QPOP_E  : V5Inst_48_Queue_Qpop<0b01, "hl.qpop.e">;
def HL_QPOP_ER : V5Inst_48_Queue_Qpop<0b11, "hl.qpop.er">;
}
//===--------------------------------------LinxV5 64bits Scalar inst-----------------------------------------------===

class V5_Long_IMM_64_Base<bits<3> Func, string opstr> :
  V5Inst_64_J_F<Func, (outs DstRWithArrow:$Dst), (ins GPRSrc:$SrcL, uimm32:$Imm32), opstr>;

let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def L_ADD_LI : V5_Long_IMM_64_Base<0b000, "l.addli">;

class V5_Load_Symbol_64_Base<bits<3> Func, string opstr> :
  V5Inst_64_K_H<Func, (outs DstRWithArrow:$Dst), (ins simm42:$Imm42), opstr> {
    let Format = InstFormat_Load_Symbol_TARGET_42;
}

def L_LB_PCR : V5_Load_Symbol_64_Base<0b000, "l.lb.pcr">;
def L_LH_PCR : V5_Load_Symbol_64_Base<0b001, "l.lh.pcr">;
def L_LW_PCR : V5_Load_Symbol_64_Base<0b010, "l.lw.pcr">;
def L_LD_PCR : V5_Load_Symbol_64_Base<0b011, "l.ld.pcr">;
def L_LBU_PCR : V5_Load_Symbol_64_Base<0b100, "l.lbu.pcr">;
def L_LHU_PCR : V5_Load_Symbol_64_Base<0b101, "l.lhu.pcr">;
def L_LWU_PCR : V5_Load_Symbol_64_Base<0b110, "l.lwu.pcr">;

class V5_Store_Symbol_64_Base<bits<3> Func, string opstr> :
  V5Inst_64_K_G<Func, (outs ), (ins GPRSrc:$SrcL, simm42:$Imm42), opstr> {
    let Format = InstFormat_Store_Symbol_TARGET_42;
}

def L_SB_PCR : V5_Store_Symbol_64_Base<0b000, "l.sb.pcr">;
def L_SH_PCR : V5_Store_Symbol_64_Base<0b001, "l.sh.pcr">;
def L_SW_PCR : V5_Store_Symbol_64_Base<0b010, "l.sw.pcr">;
def L_SD_PCR : V5_Store_Symbol_64_Base<0b011, "l.sd.pcr">;

class V5_Atomic_64_TwoDst<bits<4> Flag, bits<3> Mask, string opstr>
    : V5Inst_64_A_D<Flag, Mask, (outs DstRWithArrow:$Dst0, GPRSrc:$Dst1),
                          (ins GPRSrc:$SrcL, GPRSrc:$SrcR0, GPRSrc:$SrcR1, GPRSrc:$SrcD0, GPRSrc:$SrcD1), opstr>;

multiclass V5_Atomic_64_TwoDst_Mul<bits<4> Flag, string opcstr> {
  def ""  : V5_Atomic_64_TwoDst<Flag, 0b000, opcstr>;
  def _AQ : V5_Atomic_64_TwoDst<Flag, 0b010, opcstr # ".aq">;
  def _RL : V5_Atomic_64_TwoDst<Flag, 0b001, opcstr # ".rl">;
  def _F : V5_Atomic_64_TwoDst<Flag, 0b100, opcstr # ".f">;
  def _AQRL : V5_Atomic_64_TwoDst<Flag, 0b011, opcstr # ".aqrl">;
  def _AQF : V5_Atomic_64_TwoDst<Flag, 0b110, opcstr # ".aqf">;
  def _RLF : V5_Atomic_64_TwoDst<Flag, 0b101, opcstr # ".rlf">;
  def _AQRLF : V5_Atomic_64_TwoDst<Flag, 0b111, opcstr # ".aqrlf">;
}

defm L_CASBP : V5_Atomic_64_TwoDst_Mul<0b0000, "l.casbp">;
defm L_CASHP : V5_Atomic_64_TwoDst_Mul<0b0001, "l.cashp">;
defm L_CASWP : V5_Atomic_64_TwoDst_Mul<0b0010, "l.caswp">;
defm L_CASDP : V5_Atomic_64_TwoDst_Mul<0b0011, "l.casdp">;
