/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_s16_neon.S).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#if defined(__ARM_NEON__) && defined(HAVE_ARM_NEON_ASM_OPTIMIZATIONS)

#ifndef __MACH__
.arm
#endif

.align 4
.globl convert_float_s16_asm
#ifndef __MACH__
.type convert_float_s16_asm, %function
#endif
.globl _convert_float_s16_asm
#ifndef __MACH__
.type _convert_float_s16_asm, %function
#endif
# convert_float_s16_asm(int16_t *out, const float *in, size_t samples)
convert_float_s16_asm:
_convert_float_s16_asm:
   # Hacky way to get a constant of 2^15.
   # ((2^4)^2)^2 * 0.5 = 2^15
   vmov.f32 q8, #16.0
   vmov.f32 q9, #0.5
   vmul.f32 q8, q8, q8
   vmul.f32 q8, q8, q8
   vmul.f32 q8, q8, q9

1:
   # Preload here?
   vld1.f32 {q0-q1}, [r1]!

   vmul.f32 q0, q0, q8
   vmul.f32 q1, q1, q8

   vcvt.s32.f32 q0, q0
   vcvt.s32.f32 q1, q1

   vqmovn.s32 d4, q0
   vqmovn.s32 d5, q1

   vst1.f32 {d4-d5}, [r0]!

   # Guaranteed to get samples in multiples of 8.
   subs r2, r2, #8
   bne 1b

   bx lr

#endif
