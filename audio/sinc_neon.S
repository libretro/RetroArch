/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
# void process_sinc_neon(float *out, const float *left, const float *right, const float *coeff)
# Hardcoded to 16 taps.
process_sinc_neon_asm:
   # Left
   vld1.f32 {q0-q1},   [r1]!
   vld1.f32 {q2-q3},   [r1]!
   # Right
   vld1.f32 {q8-q9},   [r2]!
   vld1.f32 {q10-q11}, [r2]!
   # Coeff
   vld1.f32 {q12-q13}, [r3, :128]!
   vld1.f32 {q14-q15}, [r3, :128]!

   # Left
   vmul.f32 q0, q0, q12
   vmul.f32 q1, q1, q13
   vmla.f32 q0, q2, q14
   vmla.f32 q1, q3, q15

   # Right
   vmul.f32 q8, q8, q12
   vmul.f32 q9, q9, q13
   vmla.f32 q8, q10, q14
   vmla.f32 q9, q11, q15

   # Add everything together
   vadd.f32 q0, q0, q1
   vadd.f32 q8, q8, q9
   vadd.f32 d0, d0, d1
   vadd.f32 d16, d16, d17
   vpadd.f32 d0, d0, d16
   vst1.f32 d0, [r0]
   
   bx lr
