; Copyright  (C) 2010-2024 The RetroArch team
;
; ---------------------------------------------------------------------------------------
; The following license statement only applies to this file (fastjmp_msvc_x64.asm).
; ---------------------------------------------------------------------------------------
;
; Permission is hereby granted, free of charge,
; to any person obtaining a copy of this software and associated documentation files (the "Software"),
; to deal in the Software without restriction, including without limitation the rights to
; use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
; and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
; INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
; IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
; WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
; OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

; fastjmp for MSVC x64, which has no file-scope inline assembly. The
; Win64 sequence of fastjmp.c, for ml64: argument in rcx, value in edx;
; callee-saved rbx, rbp, rsi, rdi, r12-r15 and xmm6-xmm15. Layout: rip,
; rbx, rsp, rbp, rsi, rdi, r12-r15, then the ten xmm at 16-byte slots
; from 80. Assemble with ml64 /c and link the object beside fastjmp.c.

_TEXT SEGMENT

PUBLIC fastjmp_set
PUBLIC fastjmp_jmp

fastjmp_set PROC
	mov	rax, qword ptr [rsp]
	mov	rdx, rsp
	add	rdx, 8
	mov	qword ptr [rcx + 0], rax
	mov	qword ptr [rcx + 8], rbx
	mov	qword ptr [rcx + 16], rdx
	mov	qword ptr [rcx + 24], rbp
	mov	qword ptr [rcx + 32], rsi
	mov	qword ptr [rcx + 40], rdi
	mov	qword ptr [rcx + 48], r12
	mov	qword ptr [rcx + 56], r13
	mov	qword ptr [rcx + 64], r14
	mov	qword ptr [rcx + 72], r15
	movaps	xmmword ptr [rcx + 80], xmm6
	movaps	xmmword ptr [rcx + 96], xmm7
	movaps	xmmword ptr [rcx + 112], xmm8
	add	rcx, 112
	movaps	xmmword ptr [rcx + 16], xmm9
	movaps	xmmword ptr [rcx + 32], xmm10
	movaps	xmmword ptr [rcx + 48], xmm11
	movaps	xmmword ptr [rcx + 64], xmm12
	movaps	xmmword ptr [rcx + 80], xmm13
	movaps	xmmword ptr [rcx + 96], xmm14
	movaps	xmmword ptr [rcx + 112], xmm15
	xor	eax, eax
	ret
fastjmp_set ENDP

fastjmp_jmp PROC
	mov	eax, edx
	mov	rdx, qword ptr [rcx + 0]
	mov	rbx, qword ptr [rcx + 8]
	mov	rsp, qword ptr [rcx + 16]
	mov	rbp, qword ptr [rcx + 24]
	mov	rsi, qword ptr [rcx + 32]
	mov	rdi, qword ptr [rcx + 40]
	mov	r12, qword ptr [rcx + 48]
	mov	r13, qword ptr [rcx + 56]
	mov	r14, qword ptr [rcx + 64]
	mov	r15, qword ptr [rcx + 72]
	movaps	xmm6, xmmword ptr [rcx + 80]
	movaps	xmm7, xmmword ptr [rcx + 96]
	movaps	xmm8, xmmword ptr [rcx + 112]
	add	rcx, 112
	movaps	xmm9, xmmword ptr [rcx + 16]
	movaps	xmm10, xmmword ptr [rcx + 32]
	movaps	xmm11, xmmword ptr [rcx + 48]
	movaps	xmm12, xmmword ptr [rcx + 64]
	movaps	xmm13, xmmword ptr [rcx + 80]
	movaps	xmm14, xmmword ptr [rcx + 96]
	movaps	xmm15, xmmword ptr [rcx + 112]
	jmp	rdx
fastjmp_jmp ENDP

_TEXT ENDS

END
