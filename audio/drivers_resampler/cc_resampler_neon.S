/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel ( aliaspider@gmail.com )
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

#if defined(__ARM_NEON__)

#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif

#ifndef __MACH__
.arm
#endif
.align 4
.globl resampler_CC_downsample_neon
#ifndef __MACH__
.type resampler_CC_downsample_neon, %function
#endif
.globl _resampler_CC_downsample_neon
#ifndef __MACH__
.type _resampler_CC_downsample_neon, %function
#endif

# size_t resampler_CC_downsample_neon(float *outp, const float *inp,
#       rarch_CC_resampler_t* re_, size_t input_frames, float ratio);

# r0: outp initial (and output_frames return value)
# r1: inp initial/current
# r2: re_ [r2+0] --> {q14,q15}=buffer , [r2+32] --> s5=distance
# r3: input_frames/inp_max
# r4: outp current
# r5:
# r6:
# r7:

# q0:  d0:  s0:  0.0             # q4:  d8:  s16: min(ratio, 1.0)
#           s1:  1.0             #           s17: min(ratio, 1.0)
#      d1:  s2:  2.0             #      d9:  s18: min(ratio, 1.0)
#           s3:  3.0             #           s19: min(ratio, 1.0)
# q1:  d2:  s4:  ratio           # q5:  d10: s20: 1.0
#           s5:  distance        #           s21: 1.0
#      d3:  s6:  (1.0/ratio)     #      d11: s22: 1.0
#           s7:  (1.0/ratio)+0.5 #           s23: 1.0
# q2:  d4:  s8:  0.5             # q6:  d12: s24: 3.0
#           s9:  0.5             #           s25: 3.0
#      d5:  s10: 0.5             #      d13: s26: 3.0
#           s11: 0.5             #           s27: 3.0
# q3:  d6:  s12: -0.5            # q7:  d14: s28: 0.25
#           s13: -0.5            #           s29: 0.25
#      d7:  s14: -0.5            #      d15: s30: 0.25
#           s15: -0.5            #           s31: 0.25

# q8:  d16: (temp)               # q12: d24: (temp)
#           (temp)               #           (temp)
#      d17: (temp)               #      d25: (temp)
#           (temp)               #           (temp)
# q9:  d18: (temp)               # q13: d26: (temp)
#           (temp)               #           (temp)
#      d19: (temp)               #      d27: (temp)
#           (temp)               #           (temp)
# q10: d20: (temp)               # q14: d28: buffer[0]
#           (temp)               #           buffer[1]
#      d21: (temp)               #      d29: buffer[2]
#           (temp)               #           buffer[3]
# q11: d22: (temp)               # q15: d30: buffer[4]
#           (temp)               #           buffer[5]
#      d23: (temp)               #      d31: buffer[6]
#           (temp)               #           buffer[7]

resampler_CC_downsample_neon:
_resampler_CC_downsample_neon:

vld1.f32 {q14-q15}, [r2, :256]
vldr s4, [sp]
vpush {q4,q5,q6,q7}
push {r4}

mov r4, r0

veor q0, q0, q0
vmov.f32 s1, #1.0
vmov.f32 s2, #2.0
vmov.f32 s3, #3.0

vmov.f32 q2, #0.5
vmov.f32 q3, #-0.5

vmov.f32 q5, #1.0
vmov.f32 q6, #3.0
vmov.f32 q7, #0.25

vldr s5, [r2, #32]
vdiv.f32 s6, s20, s4
vadd.f32 s7, s6, s8
vdup.f32 q4, d2[0]
vmin.f32 q4, q4, q5

lsl r3, #3
add r3, r3, r1

cmp r3, r1
beq 3f
1:

vdup.f32 q8, d3[0]
vmul.f32 q8, q8, q0
vdup.f32 q9, d2[1]
vsub.f32 q8, q9, q8

vadd.f32 q10, q8, q2
vsub.f32 q11, q8, q2

vmul.f32 q10, q10, q4
vmul.f32 q11, q11, q4

#if (CC_RESAMPLER_PRECISION > 0)
vmul.f32 q8, q10, q10
vmul.f32 q9, q11, q11

vsub.f32 q12, q6, q8
vsub.f32 q13, q6, q9

vmul.f32 q12, q12, q8
vmul.f32 q13, q13, q9

vmul.f32 q12, q12, q7
vmul.f32 q13, q13, q7

vsub.f32 q12, q5, q12
vsub.f32 q13, q5, q13

vmul.f32 q10, q10, q12
vmul.f32 q11, q11, q13
#endif

vmin.f32 q10, q10, q2
vmin.f32 q11, q11, q2

vmax.f32 q10, q10, q3
vmax.f32 q11, q11, q3

vsub.f32 q10, q10, q11
vmov.f32 q11, q10

vzip.f32 q10, q11

vld1.f32 d16, [r1, :64]!
vmov.f32 d17, d16

vmul.f32 q10, q10, q8
vmul.f32 q11, q11, q8

vadd.f32 q14, q14, q10
vadd.f32 q15, q15, q11

# distance++
vadd.f32 s5, s5, s20

vcmpe.f32	s5, s7
vmrs	APSR_nzcv, fpscr
ble 2f

vst1.f32 d28, [r4, :64]!
vmov.f32 d28, d29
vmov.f32 d29, d30
vmov.f32 d30, d31
vmov.f32 d31, #0.0

vsub.f32 s5, s5, s6

2:
cmp r3, r1
bne 1b

3:
vst1.f32 {q14-q15}, [r2, :256]
vstr s5, [r2, #32]
sub r0, r4, r0
lsr r0, r0, #3

pop  {r4}
vpop {q4,q5,q6,q7}

bx lr

.align 4
.globl resampler_CC_upsample_neon
#ifndef __MACH__
.type resampler_CC_upsample_neon, %function
#endif
.globl _resampler_CC_upsample_neon
#ifndef __MACH__
.type _resampler_CC_upsample_neon, %function
#endif

# size_t resampler_CC_upsample_neon(float *outp, const float *inp,
#       rarch_CC_resampler_t* re_, size_t input_frames, float ratio);

# r0: outp initial (and output_frames return value)
# r1: inp initial/current
# r2: re_ [r2+0] --> {q14,q15}=buffer , [r2+32] --> s5=distance
# r3: input_frames/inp_max
# r4: outp current
# r5:
# r6:
# r7:

# q0:  d0:  s0:  1.0             # q4:  d8:  s16: min(ratio, 1.0)
#           s1:  0.0             #           s17: min(ratio, 1.0)
#      d1:  s2: -1.0             #      d9:  s18: min(ratio, 1.0)
#           s3: -2.0             #           s19: min(ratio, 1.0)
# q1:  d2:  s4:  ratio           # q5:  d10: s20: 1.0
#           s5:  distance        #           s21: 1.0
#      d3:  s6:  (1.0/ratio)     #      d11: s22: 1.0
#           s7:  (1.0/ratio)+0.5 #           s23: 1.0
# q2:  d4:  s8:  0.5             # q6:  d12: s24: 3.0
#           s9:  0.5             #           s25: 3.0
#      d5:  s10: 0.5             #      d13: s26: 3.0
#           s11: 0.5             #           s27: 3.0
# q3:  d6:  s12: -0.5            # q7:  d14: s28: 0.25
#           s13: -0.5            #           s29: 0.25
#      d7:  s14: -0.5            #      d15: s30: 0.25
#           s15: -0.5            #           s31: 0.25

# q8:  d16: (temp)               # q12: d24: (temp)
#           (temp)               #           (temp)
#      d17: (temp)               #      d25: (temp)
#           (temp)               #           (temp)
# q9:  d18: (temp)               # q13: d26: (temp)
#           (temp)               #           (temp)
#      d19: (temp)               #      d27: (temp)
#           (temp)               #           (temp)
# q10: d20: (temp)               # q14: d28: buffer[0]
#           (temp)               #           buffer[1]
#      d21: (temp)               #      d29: buffer[2]
#           (temp)               #           buffer[3]
# q11: d22: (temp)               # q15: d30: buffer[4]
#           (temp)               #           buffer[5]
#      d23: (temp)               #      d31: buffer[6]
#           (temp)               #           buffer[7]

resampler_CC_upsample_neon:
_resampler_CC_upsample_neon:

vld1.f32 {q14-q15}, [r2, :256]
vldr s4, [sp]
vpush {q4,q5,q6,q7}
push {r4}

mov r4, r0

veor q0, q0, q0
vmov.f32 s0, #1.0
vmov.f32 s2, #-1.0
vmov.f32 s3, #-2.0

vmov.f32 q2, #0.5
vmov.f32 q3, #-0.5

vmov.f32 q5, #1.0
vmov.f32 q6, #3.0
vmov.f32 q7, #0.25

vldr s5, [r2, #32]
vdiv.f32 s6, s20, s4
vadd.f32 s7, s6, s8
vdup.f32 q4, d2[0]
vmin.f32 q4, q4, q5

lsl r3, #3
add r3, r3, r1

cmp r3, r1
beq 4f
1:

vld1.f32 d16, [r1, :64]!
vmov.f32 d28, d29
vmov.f32 d29, d30
vmov.f32 d30, d31
vmov.f32 d31, d16

vcmpe.f32	s5, s20
vmrs	APSR_nzcv, fpscr
bge 3f
2:
vdup.f32 q8, d2[1]
vadd.f32 q8, q8, q0

vadd.f32 q10, q8, q2
vsub.f32 q11, q8, q2

vmul.f32 q10, q10, q4
vmul.f32 q11, q11, q4

#if (CC_RESAMPLER_PRECISION > 0)
vmul.f32 q8, q10, q10
vmul.f32 q9, q11, q11

vsub.f32 q12, q6, q8
vsub.f32 q13, q6, q9

vmul.f32 q12, q12, q8
vmul.f32 q13, q13, q9

vmul.f32 q12, q12, q7
vmul.f32 q13, q13, q7

vsub.f32 q12, q5, q12
vsub.f32 q13, q5, q13

vmul.f32 q10, q10, q12
vmul.f32 q11, q11, q13
#endif

vmin.f32 q10, q10, q2
vmin.f32 q11, q11, q2

vmax.f32 q10, q10, q3
vmax.f32 q11, q11, q3

vsub.f32 q10, q10, q11
vmov.f32 q11, q10

vzip.f32 q10, q11

vmul.f32 q10, q10, q14
vmul.f32 q11, q11, q15

vadd.f32 q10, q10, q11
vadd.f32 d20, d20, d21

vst1.f32 d20, [r4, :64]!

vadd.f32 s5, s5, s6

vcmpe.f32	s5, s20
vmrs	APSR_nzcv, fpscr
blt 2b

3:
# distance--
vsub.f32 s5, s5, s20

cmp r3, r1
bne 1b

4:
vst1.f32 {q14-q15}, [r2, :256]
vstr s5, [r2, #32]
sub r0, r4, r0
lsr r0, r0, #3

pop  {r4}
vpop {q4,q5,q6,q7}

bx lr

#endif
