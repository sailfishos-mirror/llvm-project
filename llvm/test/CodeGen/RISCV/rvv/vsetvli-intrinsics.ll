; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: sed 's/iXLen/i32/g' %s | llc -mtriple=riscv32 -mattr=+v \
; RUN:   -verify-machineinstrs | FileCheck %s --check-prefixes=CHECK,VLENUNKNOWN
; RUN: sed 's/iXLen/i64/g' %s | llc -mtriple=riscv64 -mattr=+v \
; RUN:   -verify-machineinstrs | FileCheck %s --check-prefixes=CHECK,VLENUNKNOWN
; RUN: sed 's/iXLen/i32/g' %s | llc -mtriple=riscv32 -mattr=+v \
; RUN:   -riscv-v-vector-bits-max=128 -verify-machineinstrs \
; RUN:   | FileCheck %s --check-prefixes=CHECK,VLEN128
; RUN: sed 's/iXLen/i64/g' %s | llc -mtriple=riscv64 -mattr=+v \
; RUN:   -riscv-v-vector-bits-max=128 -verify-machineinstrs \
; RUN:   | FileCheck %s --check-prefixes=CHECK,VLEN128

declare iXLen @llvm.riscv.vsetvli.iXLen(iXLen, iXLen, iXLen)
declare iXLen @llvm.riscv.vsetvlimax.iXLen(iXLen, iXLen)

define iXLen @test_vsetvli_e8m1(iXLen %avl) nounwind {
; CHECK-LABEL: test_vsetvli_e8m1:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, a0, e8, m1, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 0, iXLen 0)
  ret iXLen %vl
}

define iXLen @test_vsetvli_e16mf4(iXLen %avl) nounwind {
; CHECK-LABEL: test_vsetvli_e16mf4:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, a0, e16, mf4, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 1, iXLen 6)
  ret iXLen %vl
}

define iXLen @test_vsetvli_e64mf8(iXLen %avl) nounwind {
; CHECK-LABEL: test_vsetvli_e64mf8:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, a0, e64, mf8, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 3, iXLen 5)
  ret iXLen %vl
}

define iXLen @test_vsetvli_e8mf2_zero_avl() nounwind {
; CHECK-LABEL: test_vsetvli_e8mf2_zero_avl:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetivli a0, 0, e8, mf2, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen 0, iXLen 0, iXLen 7)
  ret iXLen %vl
}

define iXLen @test_vsetvli_e32mf8_zero_avl() nounwind {
; CHECK-LABEL: test_vsetvli_e32mf8_zero_avl:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetivli a0, 0, e16, mf4, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen 0, iXLen 1, iXLen 6)
  ret iXLen %vl
}

define iXLen @test_vsetvlimax_e32m2() nounwind {
; CHECK-LABEL: test_vsetvlimax_e32m2:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, zero, e32, m2, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvlimax.iXLen(iXLen 2, iXLen 1)
  ret iXLen %vl
}

define iXLen @test_vsetvlimax_e64m4() nounwind {
; CHECK-LABEL: test_vsetvlimax_e64m4:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, zero, e64, m4, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvlimax.iXLen(iXLen 3, iXLen 2)
  ret iXLen %vl
}

define iXLen @test_vsetvlimax_e64m8() nounwind {
; CHECK-LABEL: test_vsetvlimax_e64m8:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, zero, e64, m8, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvlimax.iXLen(iXLen 3, iXLen 3)
  ret iXLen %vl
}

; Check that we remove the intrinsic if it's unused.
define void @test_vsetvli_e8m1_nouse(iXLen %avl) nounwind {
; CHECK-LABEL: test_vsetvli_e8m1_nouse:
; CHECK:       # %bb.0:
; CHECK-NEXT:    ret
  call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 0, iXLen 0)
  ret void
}

define void @test_vsetvlimax_e32m2_nouse() nounwind {
; CHECK-LABEL: test_vsetvlimax_e32m2_nouse:
; CHECK:       # %bb.0:
; CHECK-NEXT:    ret
  call iXLen @llvm.riscv.vsetvlimax.iXLen(iXLen 2, iXLen 1)
  ret void
}

declare <vscale x 4 x i32> @llvm.riscv.vle.nxv4i32.iXLen(<vscale x 4 x i32>, ptr, iXLen)

; Check that we remove the redundant vsetvli when followed by another operation
define <vscale x 4 x i32> @redundant_vsetvli(iXLen %avl, ptr %ptr) nounwind {
; CHECK-LABEL: redundant_vsetvli:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli zero, a0, e32, m2, ta, ma
; CHECK-NEXT:    vle32.v v8, (a1)
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 2, iXLen 1)
  %x = call <vscale x 4 x i32> @llvm.riscv.vle.nxv4i32.iXLen(<vscale x 4 x i32> undef, ptr %ptr, iXLen %vl)
  ret <vscale x 4 x i32> %x
}

; Check that we remove the repeated/redundant vsetvli when followed by another
; operation
; FIXME: We don't catch the second vsetvli because it has a use of its output.
; We could replace it with the output of the first vsetvli.
define <vscale x 4 x i32> @repeated_vsetvli(iXLen %avl, ptr %ptr) nounwind {
; CHECK-LABEL: repeated_vsetvli:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, a0, e32, m2, ta, ma
; CHECK-NEXT:    vsetvli zero, a0, e32, m2, ta, ma
; CHECK-NEXT:    vle32.v v8, (a1)
; CHECK-NEXT:    ret
  %vl0 = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %avl, iXLen 2, iXLen 1)
  %vl1 = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen %vl0, iXLen 2, iXLen 1)
  %x = call <vscale x 4 x i32> @llvm.riscv.vle.nxv4i32.iXLen(<vscale x 4 x i32> undef, ptr %ptr, iXLen %vl1)
  ret <vscale x 4 x i32> %x
}

define iXLen @test_vsetvli_negone_e8m1(iXLen %avl) nounwind {
; CHECK-LABEL: test_vsetvli_negone_e8m1:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vsetvli a0, zero, e8, m1, ta, ma
; CHECK-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen -1, iXLen 0, iXLen 0)
  ret iXLen %vl
}

define iXLen @test_vsetvli_eqvlmax_e8m8(iXLen %avl) nounwind {
; VLENUNKNOWN-LABEL: test_vsetvli_eqvlmax_e8m8:
; VLENUNKNOWN:       # %bb.0:
; VLENUNKNOWN-NEXT:    li a0, 128
; VLENUNKNOWN-NEXT:    vsetvli a0, a0, e8, m8, ta, ma
; VLENUNKNOWN-NEXT:    ret
;
; VLEN128-LABEL: test_vsetvli_eqvlmax_e8m8:
; VLEN128:       # %bb.0:
; VLEN128-NEXT:    vsetvli a0, zero, e8, m8, ta, ma
; VLEN128-NEXT:    ret
  %vl = call iXLen @llvm.riscv.vsetvli.iXLen(iXLen 128, iXLen 0, iXLen 3)
  ret iXLen %vl
}
