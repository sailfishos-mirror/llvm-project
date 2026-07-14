# RUN: llvm-mc -triple=riscv64 -show-encoding --mattr=+experimental-zilx %s \
# RUN:        | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: not llvm-mc -triple=riscv64 -show-encoding %s 2>&1 \
# RUN:        | FileCheck %s --check-prefix=CHECK-ERROR
# RUN: llvm-mc -triple=riscv64 -filetype=obj --mattr=+experimental-zilx %s \
# RUN:        | llvm-objdump -d --mattr=+experimental-zilx --no-print-imm-hex  - \
# RUN:        | FileCheck %s --check-prefix=CHECK-INST

lxwu a0, (a1), a2
# CHECK-INST: lxwu a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xa4]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxd a0, (a1), a2
# CHECK-INST: lxd a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xc4]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxb.uw a0, (a1), a2
# CHECK-INST: lxb.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x06]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxbu.uw a0, (a1), a2
# CHECK-INST: lxbu.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x26]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxh.uw a0, (a1), a2
# CHECK-INST: lxh.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x46]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxhu.uw a0, (a1), a2
# CHECK-INST: lxhu.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x66] 
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxw.uw a0, (a1), a2
# CHECK-INST: lxw.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x86]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxwu.uw a0, (a1), a2
# CHECK-INST: lxwu.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xa6]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxd.uw a0, (a1), a2
# CHECK-INST: lxd.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xc6]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxwu.s a0, (a1), a2
# CHECK-INST: lxwu.s a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xa8]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxd.s a0, (a1), a2
# CHECK-INST: lxd.s a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xc8]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxh.s.uw a0, (a1), a2
# CHECK-INST: lxh.s.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x4a]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxhu.s.uw a0, (a1), a2
# CHECK-INST: lxhu.s.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x6a]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxw.s.uw a0, (a1), a2
# CHECK-INST: lxw.s.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x8a]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxwu.s.uw a0, (a1), a2
# CHECK-INST: lxwu.s.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xaa]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}

lxd.s.uw a0, (a1), a2
# CHECK-INST: lxd.s.uw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0xca]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Index Load for Integer){{$}}
