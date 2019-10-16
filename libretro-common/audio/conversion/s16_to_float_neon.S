/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (s16_to_float_neon.S).
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
#if defined(__ARM_NEON__) && !defined(DONT_WANT_ARM_OPTIMIZATIONS)

#ifndef __MACH__
.arm
#endif

.align 4
.globl convert_s16_float_asm
#ifndef __MACH__
.type convert_s16_float_asm, %function
#endif
.globl _convert_s16_float_asm
#ifndef __MACH__
.type _convert_s16_float_asm, %function
#endif
# convert_s16_float_asm(float *out, const int16_t *in, size_t samples, const float *gain)
convert_s16_float_asm:
_convert_s16_float_asm:
   # Hacky way to get a constant of 2^-15.
   # Might be faster to just load a constant from memory.
   # It's just done once however ...
   vmov.f32 q8, #0.25
   vmul.f32 q8, q8, q8
   vmul.f32 q8, q8, q8
   vmul.f32 q8, q8, q8
   vadd.f32 q8, q8, q8

   # Apply gain
   vld1.f32 {d6[0]}, [r3]
   vmul.f32 q8, q8, d6[0]

1:
   # Preload here?
   vld1.s16 {q0}, [r1]!

   # Widen to 32-bit
   vmovl.s16 q1, d0
   vmovl.s16 q2, d1

   # Convert to float
   vcvt.f32.s32 q1, q1
   vcvt.f32.s32 q2, q2

   vmul.f32 q1, q1, q8
   vmul.f32 q2, q2, q8

   vst1.f32 {q1-q2}, [r0]!

   # Guaranteed to get samples in multiples of 8.
   subs r2, r2, #8
   bne 1b

   bx lr

#endif
