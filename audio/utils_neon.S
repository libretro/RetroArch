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
#if defined(__ARM_NEON__) || defined(HAVE_NEON)

.arm

.align 4
.globl audio_convert_s16_float_asm
.globl _audio_convert_s16_float_asm
# audio_convert_s16_float_asm(float *out, const int16_t *in, size_t samples, const float *gain)
audio_convert_s16_float_asm:
_audio_convert_s16_float_asm:
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

.align 4
.globl audio_convert_float_s16_asm
.globl _audio_convert_float_s16_asm
# audio_convert_float_s16_asm(int16_t *out, const float *in, size_t samples)
audio_convert_float_s16_asm:
_audio_convert_float_s16_asm:
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
