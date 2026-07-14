# RUN: not llvm-mc -triple=riscv32 --mattr=+experimental-zilx %s 2>&1 \
# RUN:        | FileCheck %s --check-prefix=CHECK-ERROR
# RUN: not llvm-mc -triple=riscv64 --mattr=+experimental-zilx %s 2>&1 \
# RUN:        | FileCheck %s --check-prefix=CHECK-ERROR

# CHECK-ERROR: :[[@LINE+1]]:9: error: expected '(' or optional integer offset
lxh a0, a1, a2

# CHECK-ERROR: :[[@LINE+1]]:12: error: expected '(' or optional integer offset
lxh.uw a0, a1, a2

# CHECK-ERROR: :[[@LINE+1]]:11: error: expected '(' or optional integer offset
lxh.s a0, a1, a2

# CHECK-ERROR: :[[@LINE+1]]:14: error: expected '(' or optional integer offset
lxh.s.uw a0, a1, a2
