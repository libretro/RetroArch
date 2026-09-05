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

#if defined(__thumb__)
#define DECL_ARMMODE(x) "  .align 2\n" "  .global " x "\n" "  .thumb\n" "  .thumb_func\n" "  .type " x ", %function\n" x ":\n"
#else
#define DECL_ARMMODE(x) "  .align 4\n" "  .global " x "\n" "  .arm\n" x ":\n"
#endif

asm(
    DECL_ARMMODE("convert_float_s16_asm")
    DECL_ARMMODE("_convert_float_s16_asm")
    "# convert_float_s16_asm(int16_t *s, const float *in, size_t len)\n"
    "   # Hacky way to get a constant of 2^15.\n"
    "   # ((2^4)^2)^2 * 0.5 = 2^15\n"
    "   vmov.f32 q8, #16.0\n"
    "   vmov.f32 q9, #0.5\n"
    "   vmul.f32 q8, q8, q8\n"
    "   vmul.f32 q8, q8, q8\n"
    "   vmul.f32 q8, q8, q9\n"
    "\n"
    "1:\n"
    "   # Preload here?\n"
    "   vld1.f32 {q0-q1}, [r1]!\n"
    "\n"
    "   vmul.f32 q0, q0, q8\n"
    "   vmul.f32 q1, q1, q8\n"
    "\n"
    "   vcvt.s32.f32 q0, q0\n"
    "   vcvt.s32.f32 q1, q1\n"
    "\n"
    "   vqmovn.s32 d4, q0\n"
    "   vqmovn.s32 d5, q1\n"
    "\n"
    "   vst1.f32 {d4-d5}, [r0]!\n"
    "\n"
    "   # Guaranteed to get samples in multiples of 8.\n"
    "   subs r2, r2, #8\n"
    "   bne 1b\n"
    "\n"
    "   bx lr\n"
    "\n"
    );
#endif
