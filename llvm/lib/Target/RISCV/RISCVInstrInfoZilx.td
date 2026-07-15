//===-- RISCVInstrInfoZilx.td - 'Zilx' instructions --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file describes the RISC-V instructions for 'Zilx' (Index Load for 
/// Integer).
///
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
class IndexedLoad<bits<3> type, bits<2> width, bit isUnsigned,
                  string opcodestr>
    : RVInstI<0b111, OPC_LOAD,
              (outs GPR:$rd),
              (ins GPRMemZeroOffset:$rs1, GPR:$rs2),
              opcodestr, "$rd, $rs1, $rs2"> {
  bits<5> rs2;

  let Inst{31-30} = width;
  let Inst{29} = isUnsigned;
  let Inst{28} = 0b0;
  let Inst{27-25} = type;
  let Inst{24-20} = rs2;
}

let Predicates = [HasStdExtZilx] in {
def LXB  : IndexedLoad<0b010, 0b00, 0, "lxb">,
           Sched<[WriteLDB, ReadMemBase, ReadMemBase]>;
def LXBU : IndexedLoad<0b010, 0b00, 1, "lxbu">,
           Sched<[WriteLDB, ReadMemBase, ReadMemBase]>;
def LXH  : IndexedLoad<0b010, 0b01, 0, "lxh">,
           Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXHU : IndexedLoad<0b010, 0b01, 1, "lxhu">,
           Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXW  : IndexedLoad<0b010, 0b10, 0, "lxw">,
           Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;

def LXH_S  : IndexedLoad<0b100, 0b01, 0,  "lxh.s">,
             Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXHU_S : IndexedLoad<0b100, 0b01, 1,  "lxhu.s">,
             Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXW_S  : IndexedLoad<0b100, 0b10, 0,  "lxw.s">,
             Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;
} // Predicates = [HasStdExtZilx]

let Predicates = [HasStdExtZilx, IsRV64] in {
def LXB_UW  : IndexedLoad<0b011, 0b00, 0, "lxb.uw">,
              Sched<[WriteLDB, ReadMemBase, ReadMemBase]>;
def LXBU_UW : IndexedLoad<0b011, 0b00, 1, "lxbu.uw">,
              Sched<[WriteLDB, ReadMemBase, ReadMemBase]>;
def LXH_UW  : IndexedLoad<0b011, 0b01, 0, "lxh.uw">,
              Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXHU_UW : IndexedLoad<0b011, 0b01, 1, "lxhu.uw">,
              Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXW_UW  : IndexedLoad<0b011, 0b10, 0, "lxw.uw">,
              Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;

def LXWU    : IndexedLoad<0b010, 0b10, 1, "lxwu">,
              Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;
def LXWU_UW : IndexedLoad<0b011, 0b10, 1, "lxwu.uw">,
              Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;
def LXD     : IndexedLoad<0b010, 0b11, 0, "lxd">,
              Sched<[WriteLDD, ReadMemBase, ReadMemBase]>;
def LXD_UW  : IndexedLoad<0b011, 0b11, 0, "lxd.uw">,
              Sched<[WriteLDD, ReadMemBase, ReadMemBase]>;

def LXH_S_UW  : IndexedLoad<0b101, 0b01, 0,  "lxh.s.uw">,
                Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXHU_S_UW : IndexedLoad<0b101, 0b01, 1,  "lxhu.s.uw">,
                Sched<[WriteLDH, ReadMemBase, ReadMemBase]>;
def LXW_S_UW  : IndexedLoad<0b101, 0b10, 0,  "lxw.s.uw">,
                Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;

def LXWU_S    : IndexedLoad<0b100, 0b10, 1, "lxwu.s">,
                Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;
def LXWU_S_UW : IndexedLoad<0b101, 0b10, 1, "lxwu.s.uw">,
                Sched<[WriteLDW, ReadMemBase, ReadMemBase]>;
def LXD_S     : IndexedLoad<0b100, 0b11, 0, "lxd.s">,
                Sched<[WriteLDD, ReadMemBase, ReadMemBase]>;
def LXD_S_UW  : IndexedLoad<0b101, 0b11, 0, "lxd.s.uw">,
                Sched<[WriteLDD, ReadMemBase, ReadMemBase]>;
} // Predicates = [HasStdExtZilx, IsRV64]

class AddrRegReg<int N>
    : ComplexPattern<iPTR, 2, "SelectAddrRegRegFixedScale<"#N#">",
                     [], [], !add(10, N)>;
class AddrRegZextReg<int N>
    : ComplexPattern<i64, 2, "SelectAddrRegZextRegFixedScale<"#N#", 32>",
                     [], [], !add(20, N)>;

def AddrRegReg0 : AddrRegReg<0>;
def AddrRegReg1 : AddrRegReg<1>;
def AddrRegReg2 : AddrRegReg<2>;
def AddrRegReg3 : AddrRegReg<3>;

def AddrRegZextReg0 : AddrRegZextReg<0>;
def AddrRegZextReg1 : AddrRegZextReg<1>;
def AddrRegZextReg2 : AddrRegZextReg<2>;
def AddrRegZextReg3 : AddrRegZextReg<3>;

class UnscaledIndexLoadPat<PatFrag LoadOp, RVInst Inst, ValueType vt = XLenVT>
    : Pat<(vt (LoadOp (AddrRegReg0 (XLenVT GPR:$rs1), (XLenVT GPR:$rs2)))),
          (Inst GPR:$rs1, GPR:$rs2)>;

class UnscaledZextIndexLoadPat<PatFrag LoadOp, RVInst Inst, ValueType vt = XLenVT>
    : Pat<(vt (LoadOp (AddrRegZextReg0 (XLenVT GPR:$rs1), (XLenVT GPR:$rs2)))),
          (Inst GPR:$rs1, GPR:$rs2)>;

class ScaledIndexLoadPat<PatFrag LoadOp, RVInst Inst, int Scale, ValueType vt = XLenVT>
    : Pat<(vt (LoadOp (!cast<AddrRegReg>("AddrRegReg" # Scale)
                      (XLenVT GPR:$rs1), (XLenVT GPR:$rs2)))),
          (Inst GPR:$rs1, GPR:$rs2)>;

class ScaledZextIndexLoadPat<PatFrag LoadOp, RVInst Inst, int Scale, ValueType vt = XLenVT>
    : Pat<(vt (LoadOp (!cast<AddrRegZextReg>("AddrRegZextReg" # Scale)
                      (XLenVT GPR:$rs1), (XLenVT GPR:$rs2)))),
          (Inst GPR:$rs1, GPR:$rs2)>;

// Patterns
let Predicates = [HasStdExtZilx] in {
def : UnscaledIndexLoadPat<extloadi8, LXB>;
def : UnscaledIndexLoadPat<sextloadi8, LXB>;
def : UnscaledIndexLoadPat<zextloadi8, LXBU>;

def : UnscaledIndexLoadPat<extloadi16, LXH>;
def : UnscaledIndexLoadPat<sextloadi16, LXH>;
def : UnscaledIndexLoadPat<zextloadi16, LXHU>;

def : UnscaledIndexLoadPat<load, LXW, i32>;

def : ScaledIndexLoadPat<extloadi16, LXH_S, 1>;
def : ScaledIndexLoadPat<sextloadi16, LXH_S, 1>;
def : ScaledIndexLoadPat<zextloadi16, LXHU_S, 1>;

def : ScaledIndexLoadPat<load, LXW_S, 2, i32>;
} // Predicates = [HasStdExtZilx]

let Predicates = [HasStdExtZilx, IsRV64] in {
def : UnscaledIndexLoadPat<extloadi32, LXW, i64>;
def : UnscaledIndexLoadPat<sextloadi32, LXW, i64>;
def : UnscaledIndexLoadPat<zextloadi32, LXWU, i64>;

def : UnscaledIndexLoadPat<load, LXD, i64>;

def : ScaledIndexLoadPat<extloadi32, LXW_S, 2, i64>;
def : ScaledIndexLoadPat<sextloadi32, LXW_S, 2, i64>;
def : ScaledIndexLoadPat<zextloadi32, LXWU_S, 2, i64>;

def : ScaledIndexLoadPat<load, LXD_S, 3, i64>;

def : UnscaledZextIndexLoadPat<extloadi8, LXB_UW>;
def : UnscaledZextIndexLoadPat<sextloadi8, LXB_UW>;
def : UnscaledZextIndexLoadPat<zextloadi8, LXBU_UW>;

def : UnscaledZextIndexLoadPat<extloadi16, LXH_UW>;
def : UnscaledZextIndexLoadPat<sextloadi16, LXH_UW>;
def : UnscaledZextIndexLoadPat<zextloadi16, LXHU_UW>;

def : UnscaledZextIndexLoadPat<load, LXW_UW, i32>;

def : UnscaledZextIndexLoadPat<extloadi32, LXW_UW>;
def : UnscaledZextIndexLoadPat<sextloadi32, LXW_UW>;
def : UnscaledZextIndexLoadPat<zextloadi32, LXWU_UW>;

def : UnscaledZextIndexLoadPat<load, LXD_UW>;

def : ScaledZextIndexLoadPat<extloadi16, LXH_S_UW, 1>;
def : ScaledZextIndexLoadPat<sextloadi16, LXH_S_UW, 1>;
def : ScaledZextIndexLoadPat<zextloadi16, LXHU_S_UW, 1>;

def : ScaledZextIndexLoadPat<load, LXW_S_UW, 2, i32>;

def : ScaledZextIndexLoadPat<extloadi32, LXW_S_UW, 2, i64>;
def : ScaledZextIndexLoadPat<sextloadi32, LXW_S_UW, 2, i64>;
def : ScaledZextIndexLoadPat<zextloadi32, LXWU_S_UW, 2, i64>;

def : ScaledZextIndexLoadPat<load, LXD_S_UW, 3, i64>;
} // Predicates = [HasStdExtZilx, IsRV64]
