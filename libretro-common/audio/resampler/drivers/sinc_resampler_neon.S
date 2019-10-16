/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (sinc_resampler_neon.S).
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
.globl process_sinc_neon_asm
#ifndef __MACH__
.type process_sinc_neon_asm, %function
#endif
.globl _process_sinc_neon_asm
#ifndef __MACH__
.type _process_sinc_neon_asm, %function
#endif
# void process_sinc_neon(float *out, const float *left, const float *right, const float *coeff, unsigned taps)
# Assumes taps is >= 8, and a multiple of 8.
process_sinc_neon_asm:
_process_sinc_neon_asm:

   push {r4, lr}
   vmov.f32 q0, #0.0
   vmov.f32 q8, #0.0

   # Taps argument (r4) goes on stack in armeabi.
   ldr r4, [sp, #8]

1:
   # Left
   vld1.f32 {q2-q3}, [r1]!
   # Right
   vld1.f32 {q10-q11}, [r2]!
   # Coeff
   vld1.f32 {q12-q13}, [r3, :128]!

   # Left / Right
   vmla.f32 q0, q2, q12
   vmla.f32 q8, q10, q12
   vmla.f32 q0, q3, q13
   vmla.f32 q8, q11, q13

   subs r4, r4, #8
   bne 1b

   # Add everything together
   vadd.f32 d0, d0, d1
   vadd.f32 d16, d16, d17
   vpadd.f32 d0, d0, d16
   vst1.f32 d0, [r0]

   pop {r4, pc}

#endif
