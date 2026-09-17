/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fastjmp.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <fastjmp.h>

/* Three register-save sequences, one per ABI, as file-scope assembly.
 * Each function reads its argument from the ABI's first register and
 * stores or restores exactly the callee-saved set; fastjmp_set records
 * the caller's stack pointer as it was before the call, so a later jump
 * returns into the caller with the stack as fastjmp_set left it.
 *
 * MSVC has no file-scope assembly on x64: fastjmp_msvc_x64.asm beside
 * this file is the Win64 sequence for ml64, and under _MSC_VER this file
 * compiles to nothing. */

#if defined(FASTJMP_NATIVE) && !defined(_MSC_VER)

#if defined(__APPLE__)
#define FASTJMP_SYM(n) "_" n
#else
#define FASTJMP_SYM(n) n
#endif

#if defined(FASTJMP_X86_64) && !defined(_WIN32)

/* x86-64 System V: argument in rdi, value in esi; callee-saved rbx,
 * rbp, r12-r15. Layout: rip, rbx, rsp, rbp, r12, r13, r14, r15. */
__asm__(
   ".text\n"
   ".globl " FASTJMP_SYM("fastjmp_set") "\n"
   ".globl " FASTJMP_SYM("fastjmp_jmp") "\n"
#if !defined(__APPLE__)
   ".type fastjmp_set, @function\n"
   ".type fastjmp_jmp, @function\n"
#endif
   FASTJMP_SYM("fastjmp_set") ":\n"
   "   movq 0(%rsp), %rax\n"
   "   movq %rsp, %rdx\n"
   "   addq $8, %rdx\n"
   "   movq %rax, 0(%rdi)\n"
   "   movq %rbx, 8(%rdi)\n"
   "   movq %rdx, 16(%rdi)\n"
   "   movq %rbp, 24(%rdi)\n"
   "   movq %r12, 32(%rdi)\n"
   "   movq %r13, 40(%rdi)\n"
   "   movq %r14, 48(%rdi)\n"
   "   movq %r15, 56(%rdi)\n"
   "   xorl %eax, %eax\n"
   "   ret\n"
   FASTJMP_SYM("fastjmp_jmp") ":\n"
   "   movl %esi, %eax\n"
   "   movq 0(%rdi), %rdx\n"
   "   movq 8(%rdi), %rbx\n"
   "   movq 16(%rdi), %rsp\n"
   "   movq 24(%rdi), %rbp\n"
   "   movq 32(%rdi), %r12\n"
   "   movq 40(%rdi), %r13\n"
   "   movq 48(%rdi), %r14\n"
   "   movq 56(%rdi), %r15\n"
   "   jmp *%rdx\n"
);

#elif defined(FASTJMP_X86_64) && defined(_WIN32)

/* x86-64 Win64 (MinGW): argument in rcx, value in edx; callee-saved
 * rbx, rbp, rsi, rdi, r12-r15 and xmm6-xmm15. Layout: rip, rbx, rsp,
 * rbp, rsi, rdi, r12-r15, then the ten xmm at 16-byte slots from 80.
 * The buffer is 16-aligned by its type; rcx is bumped by 112 midway so
 * every movaps displacement stays within a byte. */
__asm__(
   ".text\n"
   ".globl fastjmp_set\n"
   ".globl fastjmp_jmp\n"
   ".def fastjmp_set; .scl 2; .type 32; .endef\n"
   ".def fastjmp_jmp; .scl 2; .type 32; .endef\n"
   ".p2align 4\n"
   "fastjmp_set:\n"
   "   movq 0(%rsp), %rax\n"
   "   movq %rsp, %rdx\n"
   "   addq $8, %rdx\n"
   "   movq %rax, 0(%rcx)\n"
   "   movq %rbx, 8(%rcx)\n"
   "   movq %rdx, 16(%rcx)\n"
   "   movq %rbp, 24(%rcx)\n"
   "   movq %rsi, 32(%rcx)\n"
   "   movq %rdi, 40(%rcx)\n"
   "   movq %r12, 48(%rcx)\n"
   "   movq %r13, 56(%rcx)\n"
   "   movq %r14, 64(%rcx)\n"
   "   movq %r15, 72(%rcx)\n"
   "   movaps %xmm6, 80(%rcx)\n"
   "   movaps %xmm7, 96(%rcx)\n"
   "   movaps %xmm8, 112(%rcx)\n"
   "   addq $112, %rcx\n"
   "   movaps %xmm9, 16(%rcx)\n"
   "   movaps %xmm10, 32(%rcx)\n"
   "   movaps %xmm11, 48(%rcx)\n"
   "   movaps %xmm12, 64(%rcx)\n"
   "   movaps %xmm13, 80(%rcx)\n"
   "   movaps %xmm14, 96(%rcx)\n"
   "   movaps %xmm15, 112(%rcx)\n"
   "   xorl %eax, %eax\n"
   "   ret\n"
   ".p2align 4\n"
   "fastjmp_jmp:\n"
   "   movl %edx, %eax\n"
   "   movq 0(%rcx), %rdx\n"
   "   movq 8(%rcx), %rbx\n"
   "   movq 16(%rcx), %rsp\n"
   "   movq 24(%rcx), %rbp\n"
   "   movq 32(%rcx), %rsi\n"
   "   movq 40(%rcx), %rdi\n"
   "   movq 48(%rcx), %r12\n"
   "   movq 56(%rcx), %r13\n"
   "   movq 64(%rcx), %r14\n"
   "   movq 72(%rcx), %r15\n"
   "   movaps 80(%rcx), %xmm6\n"
   "   movaps 96(%rcx), %xmm7\n"
   "   movaps 112(%rcx), %xmm8\n"
   "   addq $112, %rcx\n"
   "   movaps 16(%rcx), %xmm9\n"
   "   movaps 32(%rcx), %xmm10\n"
   "   movaps 48(%rcx), %xmm11\n"
   "   movaps 64(%rcx), %xmm12\n"
   "   movaps 80(%rcx), %xmm13\n"
   "   movaps 96(%rcx), %xmm14\n"
   "   movaps 112(%rcx), %xmm15\n"
   "   jmp *%rdx\n"
);

#elif defined(FASTJMP_AARCH64)

/* AArch64, every OS: argument in x0, value in w1; callee-saved x19-x28,
 * x29, the link register, and d8-d15. */
__asm__(
   ".text\n"
   ".globl " FASTJMP_SYM("fastjmp_set") "\n"
   ".globl " FASTJMP_SYM("fastjmp_jmp") "\n"
   ".p2align 4\n"
   FASTJMP_SYM("fastjmp_set") ":\n"
   "   mov x16, sp\n"
   "   stp x16, x30, [x0]\n"
   "   stp x19, x20, [x0, #16]\n"
   "   stp x21, x22, [x0, #32]\n"
   "   stp x23, x24, [x0, #48]\n"
   "   stp x25, x26, [x0, #64]\n"
   "   stp x27, x28, [x0, #80]\n"
   "   str x29, [x0, #96]\n"
   "   stp d8, d9, [x0, #112]\n"
   "   stp d10, d11, [x0, #128]\n"
   "   stp d12, d13, [x0, #144]\n"
   "   stp d14, d15, [x0, #160]\n"
   "   mov w0, wzr\n"
   "   br x30\n"
   ".p2align 4\n"
   FASTJMP_SYM("fastjmp_jmp") ":\n"
   "   ldp x16, x30, [x0]\n"
   "   mov sp, x16\n"
   "   ldp x19, x20, [x0, #16]\n"
   "   ldp x21, x22, [x0, #32]\n"
   "   ldp x23, x24, [x0, #48]\n"
   "   ldp x25, x26, [x0, #64]\n"
   "   ldp x27, x28, [x0, #80]\n"
   "   ldr x29, [x0, #96]\n"
   "   ldp d8, d9, [x0, #112]\n"
   "   ldp d10, d11, [x0, #128]\n"
   "   ldp d12, d13, [x0, #144]\n"
   "   ldp d14, d15, [x0, #160]\n"
   "   mov w0, w1\n"
   "   br x30\n"
);

#endif

#endif /* FASTJMP_NATIVE && !_MSC_VER */

/* A TU with nothing in it is an error in strict C89; this is not. */
typedef int fastjmp_c_is_not_empty;
