// RUN: mlir-opt %s --convert-math-to-apfloat -split-input-file -verify-diagnostics | FileCheck %s

func.func @full_example() {
  %neg14fp8 = arith.constant -1.4 : f8E4M3FN
  %neg14fp32 = arith.constant 1.4 : f32
  %c2 = math.absf %neg14fp8 : f8E4M3FN
  vector.print %c2 : f8E4M3FN
  %c3 = math.absf %neg14fp32 : f32
  // see llvm/unittests/ADT/APFloatTest::TEST(APFloatTest, Float8E8M0FNUFMA)
  %twof8E8M0FNU = arith.constant 2.0 : f8E8M0FNU
  %fourf8E8M0FNU = arith.constant 4.0 : f8E8M0FNU
  %eightf8E8M0FNU = arith.constant 8.0 : f8E8M0FNU
  %c4 = math.fma %fourf8E8M0FNU, %twof8E8M0FNU, %eightf8E8M0FNU : f8E8M0FNU
  return
}

// CHECK-LABEL:   func.func private @_mlir_apfloat_fused_multiply_add(i32, i64, i64, i64) -> i64
// CHECK:         func.func private @_mlir_apfloat_abs(i32, i64) -> i64

// CHECK-LABEL:   func.func @full_example() {
// CHECK:           %[[CONSTANT_0:.*]] = arith.constant -1.375000e+00 : f8E4M3FN
// CHECK:           %[[CONSTANT_1:.*]] = arith.constant 1.400000e+00 : f32
// CHECK:           %[[BITCAST_0:.*]] = arith.bitcast %[[CONSTANT_0]] : f8E4M3FN to i8
// CHECK:           %[[EXTUI_0:.*]] = arith.extui %[[BITCAST_0]] : i8 to i64
// CHECK:           %[[CONSTANT_2:.*]] = arith.constant 10 : i32
// CHECK:           %[[VAL_0:.*]] = call @_mlir_apfloat_abs(%[[CONSTANT_2]], %[[EXTUI_0]]) : (i32, i64) -> i64
// CHECK:           %[[TRUNCI_0:.*]] = arith.trunci %[[VAL_0]] : i64 to i8
// CHECK:           %[[BITCAST_1:.*]] = arith.bitcast %[[TRUNCI_0]] : i8 to f8E4M3FN
// CHECK:           vector.print %[[BITCAST_1]] : f8E4M3FN
// CHECK:           %[[BITCAST_2:.*]] = arith.bitcast %[[CONSTANT_1]] : f32 to i32
// CHECK:           %[[EXTUI_1:.*]] = arith.extui %[[BITCAST_2]] : i32 to i64
// CHECK:           %[[CONSTANT_3:.*]] = arith.constant 2 : i32
// CHECK:           %[[VAL_1:.*]] = call @_mlir_apfloat_abs(%[[CONSTANT_3]], %[[EXTUI_1]]) : (i32, i64) -> i64
// CHECK:           %[[TRUNCI_1:.*]] = arith.trunci %[[VAL_1]] : i64 to i32
// CHECK:           %[[BITCAST_3:.*]] = arith.bitcast %[[TRUNCI_1]] : i32 to f32
// CHECK:           %[[CONSTANT_4:.*]] = arith.constant 2.000000e+00 : f8E8M0FNU
// CHECK:           %[[CONSTANT_5:.*]] = arith.constant 4.000000e+00 : f8E8M0FNU
// CHECK:           %[[CONSTANT_6:.*]] = arith.constant 8.000000e+00 : f8E8M0FNU
// CHECK:           %[[BITCAST_4:.*]] = arith.bitcast %[[CONSTANT_5]] : f8E8M0FNU to i8
// CHECK:           %[[EXTUI_2:.*]] = arith.extui %[[BITCAST_4]] : i8 to i64
// CHECK:           %[[BITCAST_5:.*]] = arith.bitcast %[[CONSTANT_4]] : f8E8M0FNU to i8
// CHECK:           %[[EXTUI_3:.*]] = arith.extui %[[BITCAST_5]] : i8 to i64
// CHECK:           %[[BITCAST_6:.*]] = arith.bitcast %[[CONSTANT_6]] : f8E8M0FNU to i8
// CHECK:           %[[EXTUI_4:.*]] = arith.extui %[[BITCAST_6]] : i8 to i64
// CHECK:           %[[CONSTANT_7:.*]] = arith.constant 15 : i32
// CHECK:           %[[VAL_2:.*]] = call @_mlir_apfloat_fused_multiply_add(%[[CONSTANT_7]], %[[EXTUI_2]], %[[EXTUI_3]], %[[EXTUI_4]]) : (i32, i64, i64, i64) -> i64
// CHECK:           %[[TRUNCI_2:.*]] = arith.trunci %[[VAL_2]] : i64 to i8
// CHECK:           %[[BITCAST_7:.*]] = arith.bitcast %[[TRUNCI_2]] : i8 to f8E8M0FNU
// CHECK:           return
// CHECK:         }
