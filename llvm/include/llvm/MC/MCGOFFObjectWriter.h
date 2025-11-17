//===- MCGOFFObjectWriter.h - GOFF Object Writer ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCGOFFOBJECTWRITER_H
#define LLVM_MC_MCGOFFOBJECTWRITER_H

#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"
#include <memory>
#include <vector>

namespace llvm {
class MCObjectWriter;
class MCSectionGOFF;
class MCSymbolGOFF;
class raw_pwrite_stream;

class MCGOFFObjectTargetWriter : public MCObjectTargetWriter {
protected:
  MCGOFFObjectTargetWriter() = default;

public:
  enum RLDRelocationType {
    Reloc_Type_ACon = 0x1,   // General address.
    Reloc_Type_RelImm = 0x2, // Relative-immediate address.
    Reloc_Type_QCon = 0x3,   // Offset of symbol in class.
    Reloc_Type_VCon = 0x4,   // Address of external symbol.
    Reloc_Type_RCon = 0x5,   // PSECT of symbol.
  };

  ~MCGOFFObjectTargetWriter() override = default;

  virtual unsigned getRelocType(const MCValue &Target,
                                const MCFixup &Fixup) const = 0;

  Triple::ObjectFormatType getFormat() const override { return Triple::GOFF; }

  static bool classof(const MCObjectTargetWriter *W) {
    return W->getFormat() == Triple::GOFF;
  }
};

struct GOFFSavedRelocationEntry {
  const MCSectionGOFF *Section;
  const MCSymbolGOFF *SymA;
  const MCSymbolGOFF *SymB;
  unsigned RelocType;
  uint64_t FixupOffset;
  uint32_t Length;
  uint64_t FixedValue; // Info only.

  GOFFSavedRelocationEntry(const MCSectionGOFF *Section,
                           const MCSymbolGOFF *SymA, const MCSymbolGOFF *SymB,
                           unsigned RelocType, uint64_t FixupOffset,
                           uint32_t Length, uint64_t FixedValue)
      : Section(Section), SymA(SymA), SymB(SymB), RelocType(RelocType),
        FixupOffset(FixupOffset), Length(Length), FixedValue(FixedValue) {}
};

class GOFFObjectWriter : public MCObjectWriter {
  // The target specific GOFF writer instance.
  std::unique_ptr<MCGOFFObjectTargetWriter> TargetObjectWriter;

  // The stream used to write the GOFF records.
  raw_pwrite_stream &OS;

  // The RootSD section.
  MCSectionGOFF *RootSD = nullptr;

  // Saved relocation data.
  std::vector<GOFFSavedRelocationEntry> SavedRelocs;

public:
  GOFFObjectWriter(std::unique_ptr<MCGOFFObjectTargetWriter> MOTW,
                   raw_pwrite_stream &OS);
  ~GOFFObjectWriter() override;

  void setRootSD(MCSectionGOFF *RootSD) { this->RootSD = RootSD; }

  // Implementation of the MCObjectWriter interface.
  void recordRelocation(const MCFragment &F, const MCFixup &Fixup,
                        MCValue Target, uint64_t &FixedValue) override;

  uint64_t writeObject() override;
};

/// \brief Construct a new GOFF writer instance.
///
/// \param MOTW - The target-specific GOFF writer subclass.
/// \param OS - The stream to write to.
/// \returns The constructed object writer.
std::unique_ptr<MCObjectWriter>
createGOFFObjectWriter(std::unique_ptr<MCGOFFObjectTargetWriter> MOTW,
                       raw_pwrite_stream &OS);
} // namespace llvm

#endif
