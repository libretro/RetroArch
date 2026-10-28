/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mix.c).
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

#include <retro_environment.h>

#include <audio/audio_mix.h>

#if defined(AUDIO_MIX_HAVE_SSE2)
#include <emmintrin.h>
#elif defined(AUDIO_MIX_HAVE_NEON)
#include <arm_neon.h>
#endif

void audio_mix_volume_C(float *s, const float *in, float vol, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
      s[i] += in[i] * vol;
}

/* The vector versions do the C loop's multiply and add per sample, so
 * they give the same sums (but for denormals, which 32-bit ARM's NEON
 * takes as zero). */
#if defined(AUDIO_MIX_HAVE_SSE2)
void audio_mix_volume_SSE2(float *s, const float *in, float vol, size_t len)
{
   size_t i       = 0;
   __m128 volume  = _mm_set1_ps(vol);

   for (; i + 8 <= len; i += 8)
   {
      __m128 a = _mm_add_ps(_mm_loadu_ps(s + i),
            _mm_mul_ps(_mm_loadu_ps(in + i),     volume));
      __m128 b = _mm_add_ps(_mm_loadu_ps(s + i + 4),
            _mm_mul_ps(_mm_loadu_ps(in + i + 4), volume));
      _mm_storeu_ps(s + i,     a);
      _mm_storeu_ps(s + i + 4, b);
   }
   if (i + 4 <= len)
   {
      _mm_storeu_ps(s + i, _mm_add_ps(_mm_loadu_ps(s + i),
            _mm_mul_ps(_mm_loadu_ps(in + i), volume)));
      i += 4;
   }
   for (; i < len; i++)
      s[i] += in[i] * vol;
}
#elif defined(AUDIO_MIX_HAVE_NEON)
void audio_mix_volume_NEON(float *s, const float *in, float vol, size_t len)
{
   size_t i            = 0;
   float32x4_t volume  = vdupq_n_f32(vol);

   for (; i + 8 <= len; i += 8)
   {
      float32x4_t a = vaddq_f32(vld1q_f32(s + i),
            vmulq_f32(vld1q_f32(in + i),     volume));
      float32x4_t b = vaddq_f32(vld1q_f32(s + i + 4),
            vmulq_f32(vld1q_f32(in + i + 4), volume));
      vst1q_f32(s + i,     a);
      vst1q_f32(s + i + 4, b);
   }
   if (i + 4 <= len)
   {
      vst1q_f32(s + i, vaddq_f32(vld1q_f32(s + i),
            vmulq_f32(vld1q_f32(in + i), volume)));
      i += 4;
   }
   for (; i < len; i++)
      s[i] += in[i] * vol;
}
#endif
