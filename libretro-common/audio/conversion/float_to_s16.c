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
      float        gf    = (1<<15);
      float32x4_t vgf    = {gf, gf, gf, gf};
      float32x4_t vhalf  = vdupq_n_f32(0.5f);
      uint32x4_t  vsign  = vdupq_n_u32(0x80000000u);
      while (len >= 8)
      {
#ifdef HAVE_ARM_NEON_ASM_OPTIMIZATIONS
         size_t aligned_samples = len & ~7;
         if (aligned_samples)
            convert_float_s16_asm(s, in, aligned_samples);

         s        += aligned_samples;
         in       += aligned_samples;
         len      -= aligned_samples;
         i         = 0;
#else
         int16x4x2_t oreg;
         int32x4x2_t creg;
         float32x4x2_t inreg = vld2q_f32(in);
         float32x4_t   sc0   = vmulq_f32(inreg.val[0], vgf);
         float32x4_t   sc1   = vmulq_f32(inreg.val[1], vgf);
         float32x4_t   b0    = vreinterpretq_f32_u32(vorrq_u32(
               vandq_u32(vreinterpretq_u32_f32(sc0), vsign),
               vreinterpretq_u32_f32(vhalf)));
         float32x4_t   b1    = vreinterpretq_f32_u32(vorrq_u32(
               vandq_u32(vreinterpretq_u32_f32(sc1), vsign),
               vreinterpretq_u32_f32(vhalf)));
         creg.val[0]         = vcvtq_s32_f32(vaddq_f32(sc0, b0));
         creg.val[1]         = vcvtq_s32_f32(vaddq_f32(sc1, b1));
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
      float   scaled = in[i] * 0x8000;
      int32_t val    = (int32_t)(scaled +
            (scaled >= 0.0f ? 0.5f : -0.5f));
      s[i]           = (val > 0x7FFF) ? 0x7FFF :
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
   __m128 half       = _mm_set1_ps(0.5f);
   __m128 signmask   = _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000));
   /* Round half away from zero: add copysign(0.5, x) then truncate.
    * This matches the scalar fallback and every other SIMD variant
    * bit-for-bit, so the same float buffer yields identical s16 output
    * on all targets (netplay / rewind / regression fixtures). */

   for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
   { /* Skip forward 8 samples at a time... */
      __m128 res_a   = _mm_mul_ps(_mm_loadu_ps(in + 0), factor); /* next four samples * 32768 */
      __m128 res_b   = _mm_mul_ps(_mm_loadu_ps(in + 4), factor); /* the *next* next four   */
      __m128 bias_a  = _mm_or_ps(_mm_and_ps(res_a, signmask), half); /* copysign(0.5, res) */
      __m128 bias_b  = _mm_or_ps(_mm_and_ps(res_b, signmask), half);
      __m128i ints_a = _mm_cvttps_epi32(_mm_add_ps(res_a, bias_a)); /* rounded, truncating cvt */
      __m128i ints_b = _mm_cvttps_epi32(_mm_add_ps(res_b, bias_b));
      __m128i packed = _mm_packs_epi32(ints_a, ints_b); /* Then to 16-bit, clamping to [-32768, 32767] */

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
      const vector float        vscale =
         (vector float){32768.0f, 32768.0f, 32768.0f, 32768.0f};
      const vector float        vzero  =
         (vector float){0.0f, 0.0f, 0.0f, 0.0f};
      const vector float        vhalf  =
         (vector float){0.5f, 0.5f, 0.5f, 0.5f};
      const vector unsigned int vsign  =
         (vector unsigned int){0x80000000u, 0x80000000u,
                               0x80000000u, 0x80000000u};
      /* Round half away from zero, matching the scalar/SSE2/NEON paths. */
      for (i = 0; i + 8 <= len; i += 8, in += 8, s += 8)
      {
         vector float       input0  = vec_ld( 0, in);
         vector float       input1  = vec_ld(16, in);
         vector float       sc0     = vec_madd(input0, vscale, vzero);
         vector float       sc1     = vec_madd(input1, vscale, vzero);
         vector float       b0      = (vector float)vec_or(
               vec_and((vector unsigned int)sc0, vsign),
               (vector unsigned int)vhalf);
         vector float       b1      = (vector float)vec_or(
               vec_and((vector unsigned int)sc1, vsign),
               (vector unsigned int)vhalf);
         vector signed int  result0 = vec_cts(vec_add(sc0, b0), 0);
         vector signed int  result1 = vec_cts(vec_add(sc1, b1), 0);
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
      float   scaled = in[i] * 0x8000;
      int32_t val    = (int32_t)(scaled +
            (scaled >= 0.0f ? 0.5f : -0.5f));
      s[i]           = (val > 0x7FFF)
         ? 0x7FFF
         : (val < -0x8000 ? -0x8000 : (int16_t)val);
   }
}

void convert_float_to_s16_init_simd(void) { }
#endif
