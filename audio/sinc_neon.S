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
   vld1.f32 {q4-q5},   [r2]!
   vld1.f32 {q6-q7},   [r2]!
   # Coeff
   vld1.f32 {q8-q9},   [r3, :128]!
   vld1.f32 {q10-q11}, [r3, :128]!

   # Left
   vmul.f32 q0, q0, q8
   vmul.f32 q1, q1, q9
   vmla.f32 q0, q2, q10
   vmla.f32 q1, q3, q11

   # Right
   vmul.f32 q4, q4, q8
   vmul.f32 q5, q5, q9
   vmla.f32 q4, q6, q10
   vmla.f32 q5, q7, q11

   # Add everything together
   vadd.f32 q0, q0, q1
   vadd.f32 q4, q4, q5
   vadd.f32 d0, d0, d1
   vadd.f32 d8, d8, d9
   vpadd.f32 d0, d0, d8
   vst1.f32 d0, [r0]
   
   bx lr
