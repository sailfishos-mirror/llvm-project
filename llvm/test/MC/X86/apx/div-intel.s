# RUN: llvm-mc -triple x86_64 -show-encoding -x86-asm-syntax=intel -output-asm-variant=1 %s | FileCheck %s

# CHECK: {evex}	div	bl
# CHECK: encoding: [0x62,0xf4,0x7c,0x08,0xf6,0xf3]
         {evex}	div	bl
# CHECK: {nf}	div	bl
# CHECK: encoding: [0x62,0xf4,0x7c,0x0c,0xf6,0xf3]
         {nf}	div	bl
# CHECK: {evex}	div	dx
# CHECK: encoding: [0x62,0xf4,0x7d,0x08,0xf7,0xf2]
         {evex}	div	dx
# CHECK: {nf}	div	dx
# CHECK: encoding: [0x62,0xf4,0x7d,0x0c,0xf7,0xf2]
         {nf}	div	dx
# CHECK: {evex}	div	ecx
# CHECK: encoding: [0x62,0xf4,0x7c,0x08,0xf7,0xf1]
         {evex}	div	ecx
# CHECK: {nf}	div	ecx
# CHECK: encoding: [0x62,0xf4,0x7c,0x0c,0xf7,0xf1]
         {nf}	div	ecx
# CHECK: {evex}	div	r9
# CHECK: encoding: [0x62,0xd4,0xfc,0x08,0xf7,0xf1]
         {evex}	div	r9
# CHECK: {nf}	div	r9
# CHECK: encoding: [0x62,0xd4,0xfc,0x0c,0xf7,0xf1]
         {nf}	div	r9
# CHECK: {evex}	div	byte ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7c,0x08,0xf6,0xb4,0x80,0x23,0x01,0x00,0x00]
         {evex}	div	byte ptr [r8 + 4*rax + 291]
# CHECK: {nf}	div	byte ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7c,0x0c,0xf6,0xb4,0x80,0x23,0x01,0x00,0x00]
         {nf}	div	byte ptr [r8 + 4*rax + 291]
# CHECK: {evex}	div	word ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7d,0x08,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {evex}	div	word ptr [r8 + 4*rax + 291]
# CHECK: {nf}	div	word ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7d,0x0c,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {nf}	div	word ptr [r8 + 4*rax + 291]
# CHECK: {evex}	div	dword ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7c,0x08,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {evex}	div	dword ptr [r8 + 4*rax + 291]
# CHECK: {nf}	div	dword ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0x7c,0x0c,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {nf}	div	dword ptr [r8 + 4*rax + 291]
# CHECK: {evex}	div	qword ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0xfc,0x08,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {evex}	div	qword ptr [r8 + 4*rax + 291]
# CHECK: {nf}	div	qword ptr [r8 + 4*rax + 291]
# CHECK: encoding: [0x62,0xd4,0xfc,0x0c,0xf7,0xb4,0x80,0x23,0x01,0x00,0x00]
         {nf}	div	qword ptr [r8 + 4*rax + 291]
