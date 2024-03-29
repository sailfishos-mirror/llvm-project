; RUN: llc -verify-machineinstrs -mcpu=pwr7 -mattr=-altivec -mtriple powerpc64-ibm-aix-xcoff \
; RUN:     -xcoff-traceback-table=false --code-model=large -filetype=obj -o %t.o < %s
; RUN: llvm-readobj --relocs --expand-relocs %t.o | FileCheck -D#NFA=2 --check-prefix=RELOC %s
; RUN: llvm-readobj --syms %t.o | FileCheck -D#NFA=2 --check-prefix=SYM %s
; RUN: llvm-objdump -D -r --symbol-description %t.o | FileCheck -D#NFA=2 --check-prefix=DIS %s

@ThreadLocalVarInit = thread_local(localexec) global i64 1, align 8
@VarInit = global i64 87, align 8
@IThreadLocalVarUninit = internal thread_local(localexec) global i64 0, align 8
@IThreadLocalVarUninit2 = internal thread_local(localexec) global i64 0, align 8
declare nonnull ptr @llvm.threadlocal.address.p0(ptr nonnull)

define void @storeITLUninit(i64 noundef %x) {
entry:
  %0 = tail call align 8 ptr @llvm.threadlocal.address.p0(ptr align 8 @IThreadLocalVarUninit)
  store i64 %x, ptr %0, align 8
  ret void
}

define i64 @loadTLInit() {
entry:
  %0 = tail call align 8 ptr @llvm.threadlocal.address.p0(ptr align 8 @ThreadLocalVarInit)
  %1 = load i64, ptr %0, align 8
  %2 = load i64, ptr @VarInit, align 8
  %add = add nsw i64 %2, %1
  ret i64 %add
}

define signext i64 @loadTLUninit() {
entry:
  %0 = tail call align 8 ptr @llvm.threadlocal.address.p0(ptr align 8 @IThreadLocalVarUninit)
  store i64 1, ptr %0, align 8
  %1 = tail call align 8 ptr @llvm.threadlocal.address.p0(ptr align 8 @IThreadLocalVarUninit2)
  %2 = load i64, ptr %1, align 8
  %add = add nsw i64 %2, 1
  ret i64 %add
}

; RELOC:      File:
; RELOC-NEXT: Format: aix5coff64-rs6000
; RELOC-NEXT: Arch: powerpc64
; RELOC-NEXT: AddressSize: 64bit
; RELOC-NEXT: Relocations [
; RELOC:       Virtual Address: 0x2
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit ([[#NFA+19]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCU (0x30)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0x6
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit ([[#NFA+19]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCL (0x31)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0x12
; RELOC-NEXT:       Symbol: ThreadLocalVarInit ([[#NFA+21]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCU (0x30)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0x1A
; RELOC-NEXT:       Symbol: ThreadLocalVarInit ([[#NFA+21]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCL (0x31)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0x42
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit2 ([[#NFA+25]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCU (0x30)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0x46
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit2 ([[#NFA+25]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 16
; RELOC-NEXT:       Type: R_TOCL (0x31)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0xA8
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit ([[#NFA+29]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 64
; RELOC-NEXT:       Type: R_TLS_LE (0x23)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0xB0
; RELOC-NEXT:       Symbol: ThreadLocalVarInit ([[#NFA+27]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 64
; RELOC-NEXT:       Type: R_TLS_LE (0x23)
; RELOC-NEXT:     }
; RELOC:       Virtual Address: 0xC0
; RELOC-NEXT:       Symbol: IThreadLocalVarUninit2 ([[#NFA+31]])
; RELOC-NEXT:       IsSigned: No
; RELOC-NEXT:       FixupBitValue: 0
; RELOC-NEXT:       Length: 64
; RELOC-NEXT:       Type: R_TLS_LE (0x23)
; RELOC-NEXT:     }

; SYM:      File:
; SYM-NEXT: Format: aix5coff64-rs6000
; SYM-NEXT: Arch: powerpc64
; SYM-NEXT: AddressSize: 64bit
; SYM-NEXT: Symbols [
; SYM:     Index: [[#NFA+19]]
; SYM-NEXT:     Name: IThreadLocalVarUninit
; SYM-NEXT:     Value (RelocatableAddress): 0xA8
; SYM-NEXT:     Section: .data
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_HIDEXT (0x6B)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+20]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_SD (0x1)
; SYM-NEXT:       StorageMappingClass: XMC_TE (0x16)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM-NEXT:   }
; SYM:     Index: [[#NFA+21]]
; SYM-NEXT:     Name: ThreadLocalVarInit
; SYM-NEXT:     Value (RelocatableAddress): 0xB0
; SYM-NEXT:     Section: .data
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_HIDEXT (0x6B)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+22]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_SD (0x1)
; SYM-NEXT:       StorageMappingClass: XMC_TE (0x16)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM:     Index: [[#NFA+25]]
; SYM-NEXT:     Name: IThreadLocalVarUninit2
; SYM-NEXT:     Value (RelocatableAddress): 0xC0
; SYM-NEXT:     Section: .data
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_HIDEXT (0x6B)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+26]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_SD (0x1)
; SYM-NEXT:       StorageMappingClass: XMC_TE (0x16)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM-NEXT:   }
; SYM:     Index: [[#NFA+27]]
; SYM-NEXT:     Name: ThreadLocalVarInit
; SYM-NEXT:     Value (RelocatableAddress): 0x0
; SYM-NEXT:     Section: .tdata
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_EXT (0x2)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+28]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_SD (0x1)
; SYM-NEXT:       StorageMappingClass: XMC_TL (0x14)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM-NEXT:   }
; SYM:     Index: [[#NFA+29]]
; SYM-NEXT:     Name: IThreadLocalVarUninit
; SYM-NEXT:     Value (RelocatableAddress): 0x8
; SYM-NEXT:     Section: .tbss
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_HIDEXT (0x6B)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+30]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_CM (0x3)
; SYM-NEXT:       StorageMappingClass: XMC_UL (0x15)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM-NEXT:   }
; SYM:     Index: [[#NFA+31]]
; SYM-NEXT:     Name: IThreadLocalVarUninit2
; SYM-NEXT:     Value (RelocatableAddress): 0x10
; SYM-NEXT:     Section: .tbss
; SYM-NEXT:     Type: 0x0
; SYM-NEXT:     StorageClass: C_HIDEXT (0x6B)
; SYM-NEXT:     NumberOfAuxEntries: 1
; SYM-NEXT:     CSECT Auxiliary Entry {
; SYM-NEXT:       Index: [[#NFA+32]]
; SYM-NEXT:       SectionLen: 8
; SYM-NEXT:       ParameterHashIndex: 0x0
; SYM-NEXT:       TypeChkSectNum: 0x0
; SYM-NEXT:       SymbolAlignmentLog2: 3
; SYM-NEXT:       SymbolType: XTY_CM (0x3)
; SYM-NEXT:       StorageMappingClass: XMC_UL (0x15)
; SYM-NEXT:       Auxiliary Type: AUX_CSECT (0xFB)
; SYM-NEXT:     }
; SYM-NEXT:   }

; DIS:      file format aix5coff64-rs6000
; DIS:      Disassembly of section .text:
; DIS:      0000000000000000 (idx: [[#NFA+3]]) .storeITLUninit:
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addis 4, 2, 0
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCU	(idx: [[#NFA+19]]) IThreadLocalVarUninit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 4, 0(4)
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCL	(idx: [[#NFA+19]]) IThreadLocalVarUninit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                stdx 3, 13, 4
; DIS-NEXT:                                       blr
; DIS:      0000000000000010 (idx: [[#NFA+5]]) .loadTLInit:
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addis 3, 2, 0
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCU       (idx: [[#NFA+21]]) ThreadLocalVarInit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addis 4, 2, 0
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCU       (idx: [[#NFA+23]]) VarInit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 3, 8(3)
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCL       (idx: [[#NFA+21]]) ThreadLocalVarInit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 4, 16(4)
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCL       (idx: [[#NFA+23]]) VarInit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ldx 3, 13, 3
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 4, 0(4)
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                add 3, 4, 3
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                blr
; DIS:      0000000000000030 (idx: [[#NFA+7]]) .loadTLUninit:
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addis 3, 2, 0
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCU       (idx: [[#NFA+19]]) IThreadLocalVarUninit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                li 4, 1
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 3, 0(3)
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCL       (idx: [[#NFA+19]]) IThreadLocalVarUninit[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                stdx 4, 13, 3
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addis 3, 2, 0
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCU       (idx: [[#NFA+25]]) IThreadLocalVarUninit2[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ld 3, 24(3)
; DIS-NEXT: {{0*}}[[#ADDR + 2]]: R_TOCL       (idx: [[#NFA+25]]) IThreadLocalVarUninit2[TE]
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                ldx 3, 13, 3
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                addi 3, 3, 1
; DIS-NEXT: [[#%x, ADDR:]]: {{.*}}                blr

; DIS:      Disassembly of section .data:
; DIS:      0000000000000058 (idx: [[#NFA+9]]) VarInit[RW]:
; DIS-NEXT:       58: 00 00 00 00
; DIS-NEXT:       5c: 00 00 00 57
; DIS:      0000000000000060 (idx: [[#NFA+11]]) storeITLUninit[DS]:
; DIS-NEXT:       60: 00 00 00 00
; DIS-NEXT: 0000000000000060:  R_POS        (idx: [[#NFA+3]]) .storeITLUninit
; DIS-NEXT:       64: 00 00 00 00
; DIS-NEXT:       68: 00 00 00 00
; DIS-NEXT: 0000000000000068:  R_POS        (idx: [[#NFA+17]]) TOC[TC0]
; DIS-NEXT:       6c: 00 00 00 a8
; DIS:      0000000000000078 (idx: [[#NFA+13]]) loadTLInit[DS]:
; DIS-NEXT:       78: 00 00 00 00
; DIS-NEXT: 0000000000000078:  R_POS        (idx: [[#NFA+5]]) .loadTLInit
; DIS-NEXT:       7c: 00 00 00 10
; DIS-NEXT:       80: 00 00 00 00
; DIS-NEXT: 0000000000000080:  R_POS        (idx: [[#NFA+17]]) TOC[TC0]
; DIS-NEXT:       84: 00 00 00 a8
; DIS:      0000000000000090 (idx: [[#NFA+15]]) loadTLUninit[DS]:
; DIS-NEXT:       90: 00 00 00 00
; DIS-NEXT: 0000000000000090:  R_POS        (idx: [[#NFA+7]]) .loadTLUninit
; DIS-NEXT:       94: 00 00 00 30
; DIS-NEXT:       98: 00 00 00 00
; DIS-NEXT: 0000000000000098:  R_POS        (idx: [[#NFA+17]]) TOC[TC0]
; DIS-NEXT:       9c: 00 00 00 a8
; DIS:      00000000000000a8 (idx: [[#NFA+19]]) IThreadLocalVarUninit[TE]:
; DIS-NEXT:       a8: 00 00 00 00
; DIS-NEXT: 00000000000000a8:  R_TLS_LE     (idx: [[#NFA+29]]) IThreadLocalVarUninit[UL]
; DIS-NEXT:       ac: 00 00 00 08
; DIS:      00000000000000b0 (idx: [[#NFA+21]]) ThreadLocalVarInit[TE]:
; DIS-NEXT:       b0: 00 00 00 00
; DIS-NEXT: 00000000000000b0:  R_TLS_LE     (idx: [[#NFA+27]]) ThreadLocalVarInit[TL]
; DIS-NEXT:       b4: 00 00 00 00
; DIS:      00000000000000b8 (idx: [[#NFA+23]]) VarInit[TE]:
; DIS-NEXT:       b8: 00 00 00 00
; DIS-NEXT: 00000000000000b8:  R_POS        (idx: [[#NFA+9]]) VarInit[RW]
; DIS-NEXT:       bc: 00 00 00 58
; DIS:      00000000000000c0 (idx: [[#NFA+25]]) IThreadLocalVarUninit2[TE]:
; DIS-NEXT:       c0: 00 00 00 00
; DIS-NEXT: 00000000000000c0:  R_TLS_LE     (idx: [[#NFA+31]]) IThreadLocalVarUninit2[UL]
; DIS-NEXT:       c4: 00 00 00 10

; DIS:      Disassembly of section .tdata:
; DIS:      0000000000000000 (idx: [[#NFA+27]]) ThreadLocalVarInit[TL]:
; DIS-NEXT:        0: 00 00 00 00
; DIS-NEXT:        4: 00 00 00 01

; DIS:      Disassembly of section .tbss:
; DIS:      0000000000000008 (idx: [[#NFA+29]]) IThreadLocalVarUninit[UL]:
; DIS-NEXT: ...
; DIS:      0000000000000010 (idx: [[#NFA+31]]) IThreadLocalVarUninit2[UL]:
; DIS-NEXT: ...

