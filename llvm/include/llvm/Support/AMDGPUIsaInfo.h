//===- AMDGPUIsaInfo.h - AMDGPU ISA info for cross-component use -*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Public subset of AMDGPU ISA information needed by the linker (and
// potentially other cross-component consumers) for subtarget queries and
// register encoding.
//
// These are re-declarations of functions whose implementations live in the
// AMDGPU target (AMDGPUBaseInfo.cpp). They are duplicated here so that
// consumers outside the AMDGPU target tree can call them without including
// target-private headers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_AMDGPUISAINFO_H
#define LLVM_SUPPORT_AMDGPUISAINFO_H

#include "llvm/Support/Compiler.h"
#include <cstdint>
#include <optional>

namespace llvm {

class MCSubtargetInfo;

namespace AMDGPU {

/// \returns true if the subtarget has GFX90A instructions (unified VGPR/AGPR).
LLVM_ABI bool isGFX90A(const MCSubtargetInfo &STI);

/// \returns true if the subtarget is GFX10 or later.
LLVM_ABI bool isGFX10Plus(const MCSubtargetInfo &STI);

/// \returns true if the subtarget is GFX1250 or later.
LLVM_ABI bool isGFX1250Plus(const MCSubtargetInfo &STI);

/// Compute total number of VGPRs from arch-VGPR and AGPR counts.
/// On GFX90A with AGPRs: alignTo(NumArchVGPR, 4) + NumAGPR.
/// Otherwise: max(NumArchVGPR, NumAGPR).
LLVM_ABI int getTotalNumVGPRs(bool Has90AInsts, int32_t ArgNumAGPR,
                              int32_t ArgNumVGPR);

namespace IsaInfo {

/// \returns Number of extra SGPRs implicitly required by the subtarget
/// when VCC, flat scratch, or XNACK special registers are used.
LLVM_ABI unsigned getNumExtraSGPRs(const MCSubtargetInfo &STI, bool VCCUsed,
                                   bool FlatScrUsed, bool XNACKUsed);

/// \returns Number of extra SGPRs implicitly required by the subtarget.
/// XNACK usage is inferred from the subtarget features.
LLVM_ABI unsigned getNumExtraSGPRs(const MCSubtargetInfo &STI, bool VCCUsed,
                                   bool FlatScrUsed);

/// \returns VGPR encoding granularity for the subtarget.
LLVM_ABI unsigned
getVGPREncodingGranule(const MCSubtargetInfo &STI,
                       std::optional<bool> EnableWavefrontSize32);

/// \returns SGPR encoding granularity for the subtarget.
LLVM_ABI unsigned getSGPREncodingGranule(const MCSubtargetInfo &STI);

/// \returns Encoded number of VGPR blocks (blocks - 1) for the subtarget.
LLVM_ABI unsigned
getEncodedNumVGPRBlocks(const MCSubtargetInfo &STI, unsigned NumVGPRs,
                        std::optional<bool> EnableWavefrontSize32);

/// \returns Number of SGPR blocks (blocks - 1) for the subtarget.
/// \p NumSGPRs should already include extra SGPR counts.
LLVM_ABI unsigned getNumSGPRBlocks(const MCSubtargetInfo &STI,
                                   unsigned NumSGPRs);

/// \returns Allocation granularity for architectural VGPRs (always 4).
LLVM_ABI unsigned getArchVGPRAllocGranule();

/// \returns Local memory (LDS) size in bytes for the subtarget.
LLVM_ABI unsigned getLocalMemorySize(const MCSubtargetInfo &STI);

/// \returns true if \p LDSBytes can satisfy \p Occupancy waves per EU using the
/// object-linking occupancy ABI's 1024-workitem workgroup model.
LLVM_ABI bool
isLocalMemorySizeCompatibleWithOccupancy(const MCSubtargetInfo &STI,
                                         uint64_t LDSBytes, unsigned Occupancy);

} // namespace IsaInfo
} // namespace AMDGPU
} // namespace llvm

#endif // LLVM_SUPPORT_AMDGPUISAINFO_H
