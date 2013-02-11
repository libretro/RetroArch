/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

.arm
.align 4
.global process_sinc_neon_asm
# void process_sinc_neon(float *out, const float *left, const float *right, const float *coeff, unsigned taps)
# Assumes taps is >= 8, and a multiple of 8.
process_sinc_neon_asm:

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

