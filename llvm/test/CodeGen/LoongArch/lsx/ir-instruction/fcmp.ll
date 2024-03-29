; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc --mtriple=loongarch64 --mattr=+lsx < %s | FileCheck %s

;; TREU
define void @v4f32_fcmp_true(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_true:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vrepli.b $vr0, -1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp true <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

;; FALSE
define void @v2f64_fcmp_false(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_false:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vrepli.b $vr0, 0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp false <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETOEQ
define void @v4f32_fcmp_oeq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_oeq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.ceq.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp oeq <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_oeq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_oeq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.ceq.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp oeq <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETUEQ
define void @v4f32_fcmp_ueq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ueq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cueq.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ueq <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ueq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ueq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cueq.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ueq <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETEQ
define void @v4f32_fcmp_eq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_eq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.ceq.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast oeq <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_eq(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_eq:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.ceq.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast ueq <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETOLE
define void @v4f32_fcmp_ole(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ole:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ole <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ole(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ole:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ole <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETULE
define void @v4f32_fcmp_ule(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ule:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cule.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ule <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ule(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ule:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cule.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ule <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETLE
define void @v4f32_fcmp_le(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_le:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast ole <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_le(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_le:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast ule <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETOLT
define void @v4f32_fcmp_olt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_olt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp olt <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_olt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_olt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp olt <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETULT
define void @v4f32_fcmp_ult(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ult:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cult.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ult <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ult(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ult:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cult.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ult <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETLT
define void @v4f32_fcmp_lt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_lt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast olt <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_lt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_lt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast ult <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETONE
define void @v4f32_fcmp_one(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_one:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cne.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp one <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_one(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_one:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cne.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp one <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETUNE
define void @v4f32_fcmp_une(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_une:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cune.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp une <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_une(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_une:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cune.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp une <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETNE
define void @v4f32_fcmp_ne(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ne:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cne.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast one <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ne(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ne:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cne.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast une <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETO
define void @v4f32_fcmp_ord(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ord:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cor.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ord <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ord(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ord:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cor.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ord <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; SETUO
define void @v4f32_fcmp_uno(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_uno:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cun.s $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp uno <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_uno(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_uno:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cun.d $vr0, $vr0, $vr1
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp uno <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETOGT
define void @v4f32_fcmp_ogt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ogt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ogt <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ogt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ogt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ogt <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETUGT
define void @v4f32_fcmp_ugt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ugt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cult.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp ugt <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ugt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ugt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cult.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp ugt <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETGT
define void @v4f32_fcmp_gt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_gt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast ogt <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_gt(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_gt:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.clt.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast ugt <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETOGE
define void @v4f32_fcmp_oge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_oge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp oge <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_oge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_oge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp oge <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETUGE
define void @v4f32_fcmp_uge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_uge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cule.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp uge <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_uge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_uge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cule.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp uge <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}

;; Expand SETGE
define void @v4f32_fcmp_ge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v4f32_fcmp_ge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.s $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <4 x float>, ptr %a0
  %v1 = load <4 x float>, ptr %a1
  %cmp = fcmp fast oge <4 x float> %v0, %v1
  %ext = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %ext, ptr %res
  ret void
}

define void @v2f64_fcmp_ge(ptr %res, ptr %a0, ptr %a1) nounwind {
; CHECK-LABEL: v2f64_fcmp_ge:
; CHECK:       # %bb.0:
; CHECK-NEXT:    vld $vr0, $a1, 0
; CHECK-NEXT:    vld $vr1, $a2, 0
; CHECK-NEXT:    vfcmp.cle.d $vr0, $vr1, $vr0
; CHECK-NEXT:    vst $vr0, $a0, 0
; CHECK-NEXT:    ret
  %v0 = load <2 x double>, ptr %a0
  %v1 = load <2 x double>, ptr %a1
  %cmp = fcmp fast uge <2 x double> %v0, %v1
  %ext = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %ext, ptr %res
  ret void
}
