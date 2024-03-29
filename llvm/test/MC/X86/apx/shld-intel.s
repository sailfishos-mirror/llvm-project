# RUN: llvm-mc -triple x86_64 -show-encoding -x86-asm-syntax=intel -output-asm-variant=1 %s | FileCheck %s

# CHECK: {evex}	shld	dx, dx, 123
# CHECK: encoding: [0x62,0xf4,0x7d,0x08,0x24,0xd2,0x7b]
         {evex}	shld	dx, dx, 123
# CHECK: {nf}	shld	dx, dx, 123
# CHECK: encoding: [0x62,0xf4,0x7d,0x0c,0x24,0xd2,0x7b]
         {nf}	shld	dx, dx, 123
# CHECK: shld	dx, dx, dx, 123
# CHECK: encoding: [0x62,0xf4,0x6d,0x18,0x24,0xd2,0x7b]
         shld	dx, dx, dx, 123
# CHECK: {nf}	shld	dx, dx, dx, 123
# CHECK: encoding: [0x62,0xf4,0x6d,0x1c,0x24,0xd2,0x7b]
         {nf}	shld	dx, dx, dx, 123
# CHECK: {evex}	shld	word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: encoding: [0x62,0xd4,0x7d,0x08,0x24,0x94,0x80,0x23,0x01,0x00,0x00,0x7b]
         {evex}	shld	word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: {nf}	shld	word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: encoding: [0x62,0xd4,0x7d,0x0c,0x24,0x94,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: shld	dx, word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: encoding: [0x62,0xd4,0x6d,0x18,0x24,0x94,0x80,0x23,0x01,0x00,0x00,0x7b]
         shld	dx, word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: {nf}	shld	dx, word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: encoding: [0x62,0xd4,0x6d,0x1c,0x24,0x94,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	dx, word ptr [r8 + 4*rax + 291], dx, 123
# CHECK: {evex}	shld	ecx, ecx, 123
# CHECK: encoding: [0x62,0xf4,0x7c,0x08,0x24,0xc9,0x7b]
         {evex}	shld	ecx, ecx, 123
# CHECK: {nf}	shld	ecx, ecx, 123
# CHECK: encoding: [0x62,0xf4,0x7c,0x0c,0x24,0xc9,0x7b]
         {nf}	shld	ecx, ecx, 123
# CHECK: shld	ecx, ecx, ecx, 123
# CHECK: encoding: [0x62,0xf4,0x74,0x18,0x24,0xc9,0x7b]
         shld	ecx, ecx, ecx, 123
# CHECK: {nf}	shld	ecx, ecx, ecx, 123
# CHECK: encoding: [0x62,0xf4,0x74,0x1c,0x24,0xc9,0x7b]
         {nf}	shld	ecx, ecx, ecx, 123
# CHECK: {evex}	shld	dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: encoding: [0x62,0xd4,0x7c,0x08,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {evex}	shld	dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: {nf}	shld	dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: encoding: [0x62,0xd4,0x7c,0x0c,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: encoding: [0x62,0xd4,0x74,0x18,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: {nf}	shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: encoding: [0x62,0xd4,0x74,0x1c,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, 123
# CHECK: {evex}	shld	r9, r9, 123
# CHECK: encoding: [0x62,0x54,0xfc,0x08,0x24,0xc9,0x7b]
         {evex}	shld	r9, r9, 123
# CHECK: {nf}	shld	r9, r9, 123
# CHECK: encoding: [0x62,0x54,0xfc,0x0c,0x24,0xc9,0x7b]
         {nf}	shld	r9, r9, 123
# CHECK: shld	r9, r9, r9, 123
# CHECK: encoding: [0x62,0x54,0xb4,0x18,0x24,0xc9,0x7b]
         shld	r9, r9, r9, 123
# CHECK: {nf}	shld	r9, r9, r9, 123
# CHECK: encoding: [0x62,0x54,0xb4,0x1c,0x24,0xc9,0x7b]
         {nf}	shld	r9, r9, r9, 123
# CHECK: {evex}	shld	qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: encoding: [0x62,0x54,0xfc,0x08,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {evex}	shld	qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: {nf}	shld	qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: encoding: [0x62,0x54,0xfc,0x0c,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: shld	r9, qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: encoding: [0x62,0x54,0xb4,0x18,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         shld	r9, qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: {nf}	shld	r9, qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: encoding: [0x62,0x54,0xb4,0x1c,0x24,0x8c,0x80,0x23,0x01,0x00,0x00,0x7b]
         {nf}	shld	r9, qword ptr [r8 + 4*rax + 291], r9, 123
# CHECK: {evex}	shld	dx, dx, cl
# CHECK: encoding: [0x62,0xf4,0x7d,0x08,0xa5,0xd2]
         {evex}	shld	dx, dx, cl
# CHECK: {nf}	shld	dx, dx, cl
# CHECK: encoding: [0x62,0xf4,0x7d,0x0c,0xa5,0xd2]
         {nf}	shld	dx, dx, cl
# CHECK: shld	dx, dx, dx, cl
# CHECK: encoding: [0x62,0xf4,0x6d,0x18,0xa5,0xd2]
         shld	dx, dx, dx, cl
# CHECK: {nf}	shld	dx, dx, dx, cl
# CHECK: encoding: [0x62,0xf4,0x6d,0x1c,0xa5,0xd2]
         {nf}	shld	dx, dx, dx, cl
# CHECK: {evex}	shld	word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: encoding: [0x62,0xd4,0x7d,0x08,0xa5,0x94,0x80,0x23,0x01,0x00,0x00]
         {evex}	shld	word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: {nf}	shld	word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: encoding: [0x62,0xd4,0x7d,0x0c,0xa5,0x94,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: shld	dx, word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: encoding: [0x62,0xd4,0x6d,0x18,0xa5,0x94,0x80,0x23,0x01,0x00,0x00]
         shld	dx, word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: {nf}	shld	dx, word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: encoding: [0x62,0xd4,0x6d,0x1c,0xa5,0x94,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	dx, word ptr [r8 + 4*rax + 291], dx, cl
# CHECK: {evex}	shld	ecx, ecx, cl
# CHECK: encoding: [0x62,0xf4,0x7c,0x08,0xa5,0xc9]
         {evex}	shld	ecx, ecx, cl
# CHECK: {nf}	shld	ecx, ecx, cl
# CHECK: encoding: [0x62,0xf4,0x7c,0x0c,0xa5,0xc9]
         {nf}	shld	ecx, ecx, cl
# CHECK: shld	ecx, ecx, ecx, cl
# CHECK: encoding: [0x62,0xf4,0x74,0x18,0xa5,0xc9]
         shld	ecx, ecx, ecx, cl
# CHECK: {nf}	shld	ecx, ecx, ecx, cl
# CHECK: encoding: [0x62,0xf4,0x74,0x1c,0xa5,0xc9]
         {nf}	shld	ecx, ecx, ecx, cl
# CHECK: {evex}	shld	dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: encoding: [0x62,0xd4,0x7c,0x08,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {evex}	shld	dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: {nf}	shld	dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: encoding: [0x62,0xd4,0x7c,0x0c,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: encoding: [0x62,0xd4,0x74,0x18,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: {nf}	shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: encoding: [0x62,0xd4,0x74,0x1c,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	ecx, dword ptr [r8 + 4*rax + 291], ecx, cl
# CHECK: {evex}	shld	r9, r9, cl
# CHECK: encoding: [0x62,0x54,0xfc,0x08,0xa5,0xc9]
         {evex}	shld	r9, r9, cl
# CHECK: {nf}	shld	r9, r9, cl
# CHECK: encoding: [0x62,0x54,0xfc,0x0c,0xa5,0xc9]
         {nf}	shld	r9, r9, cl
# CHECK: shld	r9, r9, r9, cl
# CHECK: encoding: [0x62,0x54,0xb4,0x18,0xa5,0xc9]
         shld	r9, r9, r9, cl
# CHECK: {nf}	shld	r9, r9, r9, cl
# CHECK: encoding: [0x62,0x54,0xb4,0x1c,0xa5,0xc9]
         {nf}	shld	r9, r9, r9, cl
# CHECK: {evex}	shld	qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: encoding: [0x62,0x54,0xfc,0x08,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {evex}	shld	qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: {nf}	shld	qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: encoding: [0x62,0x54,0xfc,0x0c,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: shld	r9, qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: encoding: [0x62,0x54,0xb4,0x18,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         shld	r9, qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: {nf}	shld	r9, qword ptr [r8 + 4*rax + 291], r9, cl
# CHECK: encoding: [0x62,0x54,0xb4,0x1c,0xa5,0x8c,0x80,0x23,0x01,0x00,0x00]
         {nf}	shld	r9, qword ptr [r8 + 4*rax + 291], r9, cl
