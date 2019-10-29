/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (s16_to_float.c).
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
#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

#include <boolean.h>
#include <features/features_cpu.h>
#include <audio/conversion/s16_to_float.h>

#if defined(__ARM_NEON__) && !defined(DONT_WANT_ARM_OPTIMIZATIONS)
static bool s16_to_float_neon_enabled = false;

/* Avoid potential hard-float/soft-float ABI issues. */
void convert_s16_float_asm(float *out, const int16_t *in,
      size_t samples, const float *gain);
#endif

/**
 * convert_s16_to_float:
 * @out               : output buffer
 * @in                : input buffer
 * @samples           : size of samples to be converted
 * @gain              : gain applied (.e.g. audio volume)
 *
 * Converts from signed integer 16-bit
 * to floating point.
 **/
void convert_s16_to_float(float *out,
      const int16_t *in, size_t samples, float gain)
{
   size_t i      = 0;

#if defined(__SSE2__)
   float fgain   = gain / UINT32_C(0x80000000);
   __m128 factor = _mm_set1_ps(fgain);

   for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
   {
      __m128i input    = _mm_loadu_si128((const __m128i *)in);
      __m128i regs_l   = _mm_unpacklo_epi16(_mm_setzero_si128(), input);
      __m128i regs_r   = _mm_unpackhi_epi16(_mm_setzero_si128(), input);
      __m128 output_l  = _mm_mul_ps(_mm_cvtepi32_ps(regs_l), factor);
      __m128 output_r  = _mm_mul_ps(_mm_cvtepi32_ps(regs_r), factor);

      _mm_storeu_ps(out + 0, output_l);
      _mm_storeu_ps(out + 4, output_r);
   }

   samples = samples - i;
   i       = 0;
#elif defined(__ALTIVEC__)
   size_t samples_in = samples;

   /* Unaligned loads/store is a bit expensive, so we
    * optimize for the good path (very likely). */
   if (((uintptr_t)out & 15) + ((uintptr_t)in & 15) == 0)
   {
      size_t i;
      const vector float gain_vec = { gain, gain , gain, gain };
      const vector float zero_vec = { 0.0f, 0.0f, 0.0f, 0.0f};

      for (i = 0; i + 8 <= samples; i += 8, in += 8, out += 8)
      {
         vector signed short input = vec_ld(0, in);
         vector signed int hi      = vec_unpackh(input);
         vector signed int lo      = vec_unpackl(input);
         vector float out_hi       = vec_madd(vec_ctf(hi, 15), gain_vec, zero_vec);
         vector float out_lo       = vec_madd(vec_ctf(lo, 15), gain_vec, zero_vec);

         vec_st(out_hi,  0, out);
         vec_st(out_lo, 16, out);
      }

      samples_in -= i;
   }

   samples = samples_in;
   i       = 0;

#elif defined(__ARM_NEON__) && !defined(DONT_WANT_ARM_OPTIMIZATIONS)
   if (s16_to_float_neon_enabled)
   {
      size_t aligned_samples = samples & ~7;
      if (aligned_samples)
         convert_s16_float_asm(out, in, aligned_samples, &gain);

      /* Could do all conversion in ASM, but keep it simple for now. */
      out     = out + aligned_samples;
      in      = in  + aligned_samples;
      samples = samples - aligned_samples;
      i       = 0;
   }

#endif

   gain = gain / 0x8000;

#if defined(_MIPS_ARCH_ALLEGREX)
#ifdef DEBUG
   /* Make sure the buffer is 16 byte aligned, this should be the
    * default behaviour of malloc in the PSPSDK.
    * Only the output buffer can be assumed to be 16-byte aligned. */
   retro_assert(((uintptr_t)out & 0xf) == 0);
#endif

   __asm__ (
         ".set    push                    \n"
         ".set    noreorder               \n"
         "mtv     %0, s200                \n"
         ".set    pop                     \n"
         ::"r"(gain));

   for (i = 0; i + 16 <= samples; i += 16)
   {
      __asm__ (
            ".set    push                 \n"
            ".set    noreorder            \n"

            "lv.s    s100,  0(%0)         \n"
            "lv.s    s101,  4(%0)         \n"
            "lv.s    s110,  8(%0)         \n"
            "lv.s    s111, 12(%0)         \n"
            "lv.s    s120, 16(%0)         \n"
            "lv.s    s121, 20(%0)         \n"
            "lv.s    s130, 24(%0)         \n"
            "lv.s    s131, 28(%0)         \n"

            "vs2i.p  c100, c100           \n"
            "vs2i.p  c110, c110           \n"
            "vs2i.p  c120, c120           \n"
            "vs2i.p  c130, c130           \n"

            "vi2f.q  c100, c100, 16       \n"
            "vi2f.q  c110, c110, 16       \n"
            "vi2f.q  c120, c120, 16       \n"
            "vi2f.q  c130, c130, 16       \n"

            "vmscl.q e100, e100, s200     \n"

            "sv.q    c100,  0(%1)         \n"
            "sv.q    c110, 16(%1)         \n"
            "sv.q    c120, 32(%1)         \n"
            "sv.q    c130, 48(%1)         \n"

            ".set    pop                  \n"
            :: "r"(in + i), "r"(out + i));
   }

#endif

   for (; i < samples; i++)
      out[i] = (float)in[i] * gain;
}

/**
 * convert_s16_to_float_init_simd:
 *
 * Sets up function pointers for conversion
 * functions based on CPU features.
 **/
void convert_s16_to_float_init_simd(void)
{
#if defined(__ARM_NEON__) && !defined(DONT_WANT_ARM_OPTIMIZATIONS)
   unsigned cpu = cpu_features_get();

   if (cpu & RETRO_SIMD_NEON)
      s16_to_float_neon_enabled = true;
#endif
}
