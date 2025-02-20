/* Copyright  (C) 2010-2020 The RetroArch team
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
#if defined(__ARM_NEON__) && defined(HAVE_ARM_NEON_ASM_OPTIMIZATIONS)

#if defined(__thumb__)
#define DECL_ARMMODE(x) "  .align 2\n" "  .global " x "\n" "  .thumb\n" "  .thumb_func\n" "  .type " x ", %function\n" x ":\n"
#else
#define DECL_ARMMODE(x) "  .align 4\n" "  .global " x "\n" "  .arm\n" x ":\n"
#endif

asm(
    DECL_ARMMODE("convert_s16_float_asm")
    DECL_ARMMODE("_convert_s16_float_asm")
    "# convert_s16_float_asm(float *s, const int16_t *in, size_t len, const float *gain)\n"
    "   # Hacky way to get a constant of 2^-15.\n"
    "   # Might be faster to just load a constant from memory.\n"
    "   # It's just done once however ...\n"
    "   vmov.f32 q8, #0.25\n"
    "   vmul.f32 q8, q8, q8\n"
    "   vmul.f32 q8, q8, q8\n"
    "   vmul.f32 q8, q8, q8\n"
    "   vadd.f32 q8, q8, q8\n"
    "\n"
    "   # Apply gain\n"
    "   vld1.f32 {d6[0]}, [r3]\n"
    "   vmul.f32 q8, q8, d6[0]\n"
    "\n"
    "1:\n"
    "   # Preload here?\n"
    "   vld1.s16 {q0}, [r1]!\n"
    "\n"
    "   # Widen to 32-bit\n"
    "   vmovl.s16 q1, d0\n"
    "   vmovl.s16 q2, d1\n"
    "\n"
    "   # Convert to float\n"
    "   vcvt.f32.s32 q1, q1\n"
    "   vcvt.f32.s32 q2, q2\n"
    "\n"
    "   vmul.f32 q1, q1, q8\n"
    "   vmul.f32 q2, q2, q8\n"
    "\n"
    "   vst1.f32 {q1-q2}, [r0]!\n"
    "\n"
    "   # Guaranteed to get samples in multiples of 8.\n"
    "   subs r2, r2, #8\n"
    "   bne 1b\n"
    "\n"
    "   bx lr\n"
    "\n"
    );
#endif
