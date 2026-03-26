//===-- AMDGPUCoExecInfo.h - Co-execution info ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Shared types for co-execution modeling used by GCNHazardRecognizer
/// and AMDGPUStaticSimulator, and the schedulers.
///
/// Multi-cycle instructions (WMMA, TRANS, etc.) have execution windows where
/// other instruction types can co-execute. For WMMA, slot patterns depend on
/// the variant:
///
///   E0 (Issue): Control instructions only (s_delay_alu, s_set_vgpr_msb)
///   E (External): Memory and SALU can co-execute, no VALU
///   I (Internal): VALU, TRANS, memory, and SALU can all co-execute
///   V (Vacant): Memory/SALU/next-WMMA ok, NO VALU/TRANS
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECINFO_H
#define LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECINFO_H

#include "SIDefines.h"
#include "SIInstrInfo.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <optional>

namespace llvm {

namespace AMDGPU {

//===----------------------------------------------------------------------===//
// Co-execution Bitmasks
//===----------------------------------------------------------------------===//

/// Bitmask for instruction types allowed to co-execute at a stage.
namespace CoExecMask {
constexpr uint8_t None = 0;
constexpr uint8_t CTRL = 1 << 0;  // Control: s_delay_alu, s_set_vgpr_msb
constexpr uint8_t VALU = 1 << 1;  // Vector ALU
constexpr uint8_t TRANS = 1 << 2; // Transcendentals (V_EXP etc)
constexpr uint8_t SALU = 1 << 3;  // Scalar ALU
constexpr uint8_t DS = 1 << 4;    // LDS read/write
constexpr uint8_t VMEM = 1 << 5;  // Global memory
constexpr uint8_t SMEM = 1 << 6;  // Scalar memory
constexpr uint8_t WMMA = 1 << 7;  // Next WMMA (V stages only)
constexpr uint8_t All = 0xFF;

constexpr uint8_t MEM = DS | VMEM | SMEM;
constexpr uint8_t StageE0 = CTRL;             // Issue: control only
constexpr uint8_t StageE = CTRL | SALU | MEM; // External: mem/salu
constexpr uint8_t StageI =
    CTRL | SALU | MEM | VALU | TRANS;                // Internal: all ALU
constexpr uint8_t StageV = CTRL | SALU | MEM | WMMA; // Vacant: no valu/trans
} // namespace CoExecMask

//===----------------------------------------------------------------------===//
// Co-execution Stage Type
//===----------------------------------------------------------------------===//

/// Stage type for co-execution (for annotation/display).
enum class CoExecStageType : uint8_t {
  NONE = 0, // Not in co-exec window
  E0,       // Issue cycle - control only
  E,        // External - MEM/SALU allowed
  I,        // Internal - MEM/SALU/VALU allowed
  V         // Vacant - MEM/SALU/WMMA allowed, no VALU
};

/// Max stages: INT8 16x16x64 = 17 cycles, round up for safety.
constexpr unsigned MaxCoExecStages = 20;

//===----------------------------------------------------------------------===//
// Co-execution Info
//===----------------------------------------------------------------------===//

/// Co-execution characteristics for a multi-cycle instruction.
/// Used by hazard recognizer and static simulator.
struct CoExecInfo {
  /// Cycles until unit is free (next instruction of same type can issue).
  unsigned Occupancy = 0;
  /// Total co-execution window size including tail.
  unsigned TotalWindow = 0;
  /// Per-stage allowed instruction mask.
  uint8_t StageMask[MaxCoExecStages] = {};
  /// Last I-stage index (for LD_SCALE rule).
  unsigned LastIStage = 0;
  /// True for FP8/FP6/FP4 scaled variants.
  bool HasScaling = false;
  /// Pattern string for display (e.g., "0EIIEEIIV").
  StringRef Pattern;

  /// Default constructor - initialize to safe defaults.
  CoExecInfo() {
    for (unsigned I = 0; I < MaxCoExecStages; ++I)
      StageMask[I] = CoExecMask::All; // Default: permissive
  }

  /// Get capability mask for a stage.
  uint8_t getMask(unsigned Stage) const {
    return Stage < MaxCoExecStages ? StageMask[Stage] : CoExecMask::All;
  }

  /// Check if instruction class mask can co-execute at a given stage.
  bool canCoExec(uint8_t InstMask, unsigned Stage) const {
    if (Stage >= TotalWindow)
      return true;
    return (StageMask[Stage] & InstMask) != 0;
  }

  /// Find next stage where instruction class is allowed.
  std::optional<unsigned> findNextAllowedStage(uint8_t InstMask,
                                               unsigned FromStage) const {
    for (unsigned I = FromStage; I < TotalWindow; ++I) {
      if ((StageMask[I] & InstMask) != 0)
        return I;
    }
    return std::nullopt;
  }

  /// Get stage type from mask for display.
  static CoExecStageType getStageType(uint8_t Mask) {
    using namespace CoExecMask;
    if (Mask == StageE0)
      return CoExecStageType::E0;
    if (Mask == StageE)
      return CoExecStageType::E;
    if (Mask == StageI)
      return CoExecStageType::I;
    if (Mask == StageV)
      return CoExecStageType::V;
    // For 'All' or unknown, return based on what's allowed
    if (Mask & VALU)
      return CoExecStageType::I; // If VALU allowed, it's I-like
    if (Mask & CoExecMask::WMMA)
      return CoExecStageType::V; // If WMMA allowed (not VALU), V-like
    return CoExecStageType::E;   // Default to E
  }

  /// Get stage type for a specific stage.
  CoExecStageType getType(unsigned Stage) const {
    return getStageType(getMask(Stage));
  }

  /// Build a CoExecInfo from pattern string.
  static CoExecInfo build(unsigned Occupancy, unsigned TotalWindow,
                          const char *Pattern, unsigned LastIStage,
                          bool HasScaling);
};

//===----------------------------------------------------------------------===//
// Co-execution Info Construction
//===----------------------------------------------------------------------===//

/// Build CoExecInfo from a pattern string.
/// Pattern chars: '0'=E0, 'E'=External, 'I'=Internal, 'V'=Vacant, 'A'=Any
inline CoExecInfo CoExecInfo::build(unsigned Occupancy, unsigned TotalWindow,
                                    const char *Pattern, unsigned LastIStage,
                                    bool HasScaling) {
  CoExecInfo Info;
  Info.Occupancy = Occupancy;
  Info.TotalWindow = TotalWindow;
  Info.LastIStage = LastIStage;
  Info.HasScaling = HasScaling;
  Info.Pattern = Pattern;

  for (unsigned I = 0; I < TotalWindow && Pattern[I]; ++I) {
    switch (Pattern[I]) {
    case '0':
      Info.StageMask[I] = CoExecMask::StageE0;
      break;
    case 'E':
      Info.StageMask[I] = CoExecMask::StageE;
      break;
    case 'I':
      Info.StageMask[I] = CoExecMask::StageI;
      break;
    case 'V':
      Info.StageMask[I] = CoExecMask::StageV;
      break;
    case 'A':
    default:
      Info.StageMask[I] = CoExecMask::All;
      break;
    }
  }
  return Info;
}

/// Get co-execution info for a WMMA instruction based on opcode.
inline CoExecInfo getCoExecInfo(const MachineInstr &MI,
                                const SIInstrInfo &TII) {
  StringRef Name = TII.getName(MI.getOpcode());

  // Check for scaled variants (LD_SCALE rule applies)
  bool HasScaling = Name.contains_insensitive("scale");

  if (Name.contains_insensitive("16x16x64_iu8")) {
    return CoExecInfo::build(16, 17, "0EIIEEIIEEIIEEIIV", 15, HasScaling);
  }

  // F8F6F4 16x16x128: window size depends on operand formats
  if (Name.contains_insensitive("16x16x128_f8f6f4")) {
    // Check if both operands are FP4 (shorter window)
    bool BothF4 = false;
    if (const MachineOperand *FmtA =
            TII.getNamedOperand(MI, AMDGPU::OpName::matrix_a_fmt)) {
      if (const MachineOperand *FmtB =
              TII.getNamedOperand(MI, AMDGPU::OpName::matrix_b_fmt)) {
        BothF4 = (FmtA->getImm() == AMDGPU::WMMA::MATRIX_FMT_FP4 &&
                  FmtB->getImm() == AMDGPU::WMMA::MATRIX_FMT_FP4);
      }
    }

    if (BothF4) {
      // f4×f4: 4-cycle occupancy, 6-cycle window
      return CoExecInfo::build(4, 6, "0EEIVV", 3, HasScaling);
    }
    // f8×*, f6×*, or mixed: 8-cycle occupancy, 10-cycle window
    return CoExecInfo::build(8, 10, "0EEIEEIIVV", 7, HasScaling);
  }

  // FP8/BF8 16x16x64: 4-cycle occupancy, 6-cycle window
  if (Name.contains_insensitive("16x16x64_fp8") ||
      Name.contains_insensitive("16x16x64_bf8")) {
    return CoExecInfo::build(4, 6, "0EEIVV", 3, HasScaling);
  }

  // F16/BF16 16x16x32: 8-cycle occupancy, 9-cycle window
  if (Name.contains_insensitive("16x16x32_f16") ||
      Name.contains_insensitive("16x16x32_bf16")) {
    return CoExecInfo::build(8, 9, "0EIIEEIIV", 7, HasScaling);
  }

  // FP8/BF8 16x16x128: 8-cycle occupancy, 10-cycle window
  if (Name.contains_insensitive("16x16x128_fp8") ||
      Name.contains_insensitive("16x16x128_bf8")) {
    return CoExecInfo::build(8, 10, "0EEIEEIIVV", 7, HasScaling);
  }

  // 32x16x128 F4 variants
  if (Name.contains_insensitive("32x16x128_f4")) {
    return CoExecInfo::build(4, 6, "0EEIVV", 3, HasScaling);
  }

  // Default fallback: permissive 8-cycle pattern
  return CoExecInfo::build(8, 9, "AAAAAAAAA", 7, HasScaling);
}

} // namespace AMDGPU
} // namespace llvm

#endif // LLVM_LIB_TARGET_AMDGPU_AMDGPUCOEXECINFO_H
