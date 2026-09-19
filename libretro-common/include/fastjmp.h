/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (fastjmp.h).
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

/* fastjmp: a setjmp/longjmp that saves the callee-saved registers and
 * nothing else, for leaving generated code.
 *
 * A recompiler that must unwind out of a block it emitted cannot use the
 * C library's longjmp everywhere: on Windows x64 that longjmp walks the
 * unwind tables, and a JIT block has none, so it faults; on POSIX the
 * plain setjmp also saves and restores the signal mask, a system call on
 * a path the interpreter takes every state change. fastjmp_set stores
 * the return address, the stack pointer and the callee-saved registers
 * of the ABI, fastjmp_jmp restores them and returns from fastjmp_set a
 * second time with the given value; nothing is unwound and no mask is
 * touched.
 *
 * Real implementations: x86-64 System V, x86-64 Win64 (which also has
 * to preserve xmm6-xmm15), and AArch64 on every OS. On MSVC x64 the
 * assembler cannot be inline, and the same code is fastjmp_msvc_x64.asm
 * beside fastjmp.c, to be assembled with ml64 into the build. Elsewhere
 * the two names are the C library's setjmp and longjmp -- correct, only
 * without the two properties above -- so a core compiles everywhere and
 * gets the fast form where it exists.
 *
 * The usual setjmp rules hold: fastjmp_jmp must target a buffer whose
 * fastjmp_set frame is still live, and locals changed between the set
 * and the jump are only reliable if volatile. ret is what fastjmp_set
 * returns the second time and must not be 0.
 */

#ifndef __LIBRETRO_SDK_FASTJMP_H__
#define __LIBRETRO_SDK_FASTJMP_H__

#include <stddef.h>
#include <retro_common_api.h>

#if defined(_M_X64) || defined(_M_AMD64) || defined(__amd64__) || defined(__x86_64__) || defined(__x86_64)
#define FASTJMP_X86_64 1
#endif
#if defined(_M_ARM64) || defined(__aarch64__)
#define FASTJMP_AARCH64 1
#endif

/* FASTJMP_FORCE_FALLBACK, defined by a build, selects the C library's
 * setjmp everywhere: for checking that a core behaves the same on both,
 * and for the fallback test on a host that has the native form. */
#if (defined(FASTJMP_X86_64) || defined(FASTJMP_AARCH64)) && !defined(FASTJMP_FORCE_FALLBACK)

#define FASTJMP_NATIVE 1

RETRO_BEGIN_DECLS

/* Sized for the widest of the three layouts: Win64's rip, rsp, eight
 * GPRs and ten xmm registers.
 *
 * Win64 stores xmm6-xmm15 with movaps, which faults on an address that
 * is not a multiple of 16, so the type has to carry that alignment
 * itself: a byte array asks for none, and a fastjmp_buf that is a
 * member after a char, or a static the linker packs, lands wherever it
 * fits. Both compilers that build the native Win64 form have a
 * spelling for it that is valid in C89. */
#if defined(_MSC_VER)
#define FASTJMP_ALIGN16 __declspec(align(16))
#elif defined(__GNUC__) || defined(__clang__)
#define FASTJMP_ALIGN16 __attribute__((aligned(16)))
#else
#define FASTJMP_ALIGN16
#endif

typedef struct fastjmp_buf
{
#if defined(FASTJMP_X86_64) && defined(_WIN32)
   FASTJMP_ALIGN16 unsigned char buf[240];
#elif defined(FASTJMP_AARCH64)
   unsigned char buf[176];
#else
   unsigned char buf[64];
#endif
} fastjmp_buf;

/* 0 the first time; ret from a later fastjmp_jmp. */
int  fastjmp_set(fastjmp_buf *buf);
void fastjmp_jmp(const fastjmp_buf *buf, int ret);

RETRO_END_DECLS

#else

#include <setjmp.h>
typedef jmp_buf fastjmp_buf;
#define fastjmp_set(b)      setjmp(*(b))
#define fastjmp_jmp(b, r)   longjmp(*(fastjmp_buf*)(b), (r))

#endif

#endif
