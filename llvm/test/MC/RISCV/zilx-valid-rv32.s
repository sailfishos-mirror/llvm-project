# RUN: llvm-mc -triple=riscv32 -show-encoding --mattr=+experimental-zilx %s \
# RUN:        | FileCheck %s --check-prefixes=CHECK-ENCODING,CHECK-INST
# RUN: not llvm-mc -triple=riscv32 -show-encoding %s 2>&1 \
# RUN:        | FileCheck %s --check-prefix=CHECK-ERROR
# RUN: llvm-mc -triple=riscv32 -filetype=obj --mattr=+experimental-zilx %s \
# RUN:        | llvm-objdump -d --mattr=+experimental-zilx --no-print-imm-hex  - \
# RUN:        | FileCheck %s --check-prefix=CHECK-INST

lxb a0, (a1), a2
# CHECK-INST: lxb a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x04]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxbu a0, (a1), a2
# CHECK-INST: lxbu a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x24]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxh a0, (a1), a2
# CHECK-INST: lxh a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x44]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxhu a0, (a1), a2
# CHECK-INST: lxhu a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x64]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxw a0, (a1), a2
# CHECK-INST: lxw a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x84]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxh.s a0, (a1), a2
# CHECK-INST: lxh.s a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x48]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxhu.s a0, (a1), a2
# CHECK-INST: lxhu.s a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x68]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}

lxw.s a0, (a1), a2
# CHECK-INST: lxw.s a0, (a1), a2
# CHECK-ENCODING: [0x03,0xf5,0xc5,0x88]
# CHECK-ERROR: instruction requires the following: 'Zilx' (Indexed Integer Load Instructions){{$}}
