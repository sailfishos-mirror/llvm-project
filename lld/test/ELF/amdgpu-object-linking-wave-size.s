# REQUIRES: amdgpu

## Test that the linker rejects cross-TU calls between functions compiled with
## different wavefront sizes.

# RUN: split-file %s %t

# RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx1030 -filetype=obj %t/a.s -o %t/a.o
# RUN: llvm-mc -triple=amdgcn-amd-amdhsa -mcpu=gfx1030 -filetype=obj %t/b.s -o %t/b.o
# RUN: not ld.lld %t/a.o %t/b.o -o /dev/null 2>&1 | FileCheck %s

# CHECK: error: AMDGPU object linking: wave size mismatch in call from 'kernel' (wave32) to 'helper' (wave64)

#--- a.s
	.amdgcn_target "amdgcn-amd-amdhsa--gfx1030"
	.amdhsa_code_object_version 6
	.text
	.globl	kernel
	.p2align	8
	.type	kernel,@function
kernel:
	s_endpgm
.Lfunc_end0:
	.size	kernel, .Lfunc_end0-kernel
	.section	.rodata,"a",@progbits
	.p2align	6, 0x0
	.globl	kernel.kd
	.type	kernel.kd,@object
	.size	kernel.kd, 64
	.protected	kernel
kernel.kd:
	.long	0
	.long	0
	.long	264
	.long	0
	.long	0
	.quad	kernel@rel64-kernel.kd
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.long	0
	.long	11469063
	.long	5020
	.short	1063
	.short	0
	.long	0
	.text
	.amdgpu_info kernel
		.amdgpu_flags 3
		.amdgpu_num_vgpr 32
		.amdgpu_num_sgpr 33
		.amdgpu_private_segment_size 0
		.amdgpu_occupancy 4
		.amdgpu_wave_size 32
		.amdgpu_call helper
	.end_amdgpu_info

	.section	".note.GNU-stack","",@progbits
	.amdgpu_metadata
---
amdhsa.kernels:
  - .group_segment_fixed_size: 0
    .kernarg_segment_align: 8
    .kernarg_segment_size: 264
    .max_flat_workgroup_size: 1024
    .name:           kernel
    .private_segment_fixed_size: 0
    .sgpr_count:     39
    .symbol:         kernel.kd
    .uses_dynamic_stack: false
    .vgpr_count:     32
    .wavefront_size: 32
amdhsa.target:   amdgcn-amd-amdhsa--gfx1030
amdhsa.version:
  - 1
  - 2
...

	.end_amdgpu_metadata

#--- b.s
	.amdgcn_target "amdgcn-amd-amdhsa--gfx1030"
	.amdhsa_code_object_version 6
	.text
	.globl	helper
	.p2align	2
	.type	helper,@function
helper:
	s_setpc_b64 s[30:31]
.Lfunc_end1:
	.size	helper, .Lfunc_end1-helper

	.amdgpu_info helper
		.amdgpu_flags 0
		.amdgpu_num_vgpr 10
		.amdgpu_num_sgpr 8
		.amdgpu_private_segment_size 16
		.amdgpu_occupancy 8
		.amdgpu_wave_size 64
	.end_amdgpu_info

	.section	".note.GNU-stack","",@progbits
	.amdgpu_metadata
---
amdhsa.kernels: []
amdhsa.target:   amdgcn-amd-amdhsa--gfx1030
amdhsa.version:
  - 1
  - 2
...

	.end_amdgpu_metadata
