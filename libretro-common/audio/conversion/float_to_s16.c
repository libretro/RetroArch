/* Copyright  (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (float_to_s16.c).
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
#include <stdint.h>
#include <stddef.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#include <features/features_cpu.h>
#include <audio/conversion/float_to_s16.h>

#if (defined(__ARM_NEON__) || defined(HAVE_NEON))
static bool float_to_s16_neon_enabled = false;
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
void convert_float_s16_asm(int16_t *s, const float *in, size_t len);
#else
#include <arm_neon.h>
#endif

void convert_float_to_s16(int16_t *s, const float *in, size_t len)
{
   size_t i           = 0;
   if (float_to_s16_neon_enabled)
   {
      float        gf = (1<<15);
      float32x4_t vgf = {gf, gf, gf, gf};
      while (len >= 8)
      {
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
         size_t aligned_samples = len & ~7;
         if (aligned_samples)
            convert_float_s16_asm(s, in, aligned_samples);

         s        += aligned_samples;
         in       += aligned_samples;
         samples  -= aligned_samples;
         i         = 0;
#else
         int16x4x2_t oreg;
         int32x4x2_t creg;
         float32x4x2_t inreg = vld2q_f32(in);
         creg.val[0]         = vcvtq_s32_f32(vmulq_f32(inreg.val[0], vgf));
         creg.val[1]         = vcvtq_s32_f32(vmulq_f32(inreg.val[1], vgf));
         oreg.val[0]         = vqmovn_s32(creg.val[0]);
         oreg.val[1]         = vqmovn_s32(creg.val[1]);
         vst2_s16(s, oreg);
         in      += 8;
         s       += 8;
         len     -= 8;
#endif
      }
   }

   for (; i < len; i++)
   {
      int32_t val = (int32_t)(in[i] * 0x8000);
      s[i]        = (val > 0x7FFF) ? 0x7FFF :
         (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

void convert_float_to_s16_init_simd(void)
{
   uint64_t cpu = cpu_features_get();

   if (cpu & RETRO_SIMD_NEON)
      float_to_s16_neon_enabled = true;
}
#else
void convert_float_to_s16(int16_t *s, const float *in, size_t len)
{
   size_t i          = 0;
#if defined(__SSE2__)
   __m128 factor     = _mm_set1_ps((float)0x8000);
   /* Initialize a 4D vector with 32768.0 for its elements */

   for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
   { /* Skip forward 8 samples at a time... */
      __m128 input_a = _mm_loadu_ps(in + 0); /* Create a 4-float vector from the next four samples... */
      __m128 input_b = _mm_loadu_ps(in + 4); /* ...and another from the *next* next four. */
      __m128 res_a   = _mm_mul_ps(input_a, factor);
      __m128 res_b   = _mm_mul_ps(input_b, factor); /* Multiply these samples by 32768 */
      __m128i ints_a = _mm_cvtps_epi32(res_a);
      __m128i ints_b = _mm_cvtps_epi32(res_b); /* Convert the samples to 32-bit integers */
      __m128i packed = _mm_packs_epi32(ints_a, ints_b); /* Then convert them to 16-bit ints, clamping to [-32768, 32767] */

      _mm_storeu_si128((__m128i *)s, packed); /* Then put the result in the output array */
   }

   len               = len - i;
   i                 = 0;
   /* If there are any stray samples at the end, we need to convert them
    * (maybe the original array didn't contain a multiple of 8 samples) */
#elif defined(__ALTIVEC__)
   int samples_in    = len;

   /* Unaligned loads/store is a bit expensive,
    * so we optimize for the good path (very likely). */
   if (((uintptr_t)s & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
      {
         vector float       input0 = vec_ld( 0, in);
         vector float       input1 = vec_ld(16, in);
         vector signed int result0 = vec_cts(input0, 15);
         vector signed int result1 = vec_cts(input1, 15);
         vec_st(vec_packs(result0, result1), 0, s);
      }

      samples_in    -= i;
   }

   len               = samples_in;
   i                 = 0;
#elif defined(_MIPS_ARCH_ALLEGREX)
#ifdef DEBUG
   /* Make sure the buffers are 16 byte aligned, this should be
    * the default behaviour of malloc in the PSPSDK.
    * Assume alignment. */
   retro_assert(((uintptr_t)in  & 0xf) == 0);
   retro_assert(((uintptr_t)s & 0xf) == 0);
#endif

   for (i = 0; i + 8 <= len; i += 8)
   {
      __asm__ (
            ".set    push                 \n"
            ".set    noreorder            \n"

            "lv.q    c100,  0(%0)         \n"
            "lv.q    c110,  16(%0)        \n"

            "vf2in.q c100, c100, 31       \n"
            "vf2in.q c110, c110, 31       \n"
            "vi2s.q  c100, c100           \n"
            "vi2s.q  c102, c110           \n"

            "sv.q    c100,  0(%1)         \n"

            ".set    pop                  \n"
            :: "r"(in + i), "r"(s + i));
   }
#endif

   /* This loop converts stray samples to the right format,
    * but it's also a fallback in case no SIMD instructions are available. */
   for (; i < len; i++)
   {
      int32_t val    = (int32_t)(in[i] * 0x8000);
      s[i]           = (val > 0x7FFF)
         ? 0x7FFF
         : (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

void convert_float_to_s16_init_simd(void) { }
#endif
