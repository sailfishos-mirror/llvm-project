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
