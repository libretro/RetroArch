/* Copyright  (C) 2010-2017 The RetroArch team
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

#include <audio/audio_mix.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ALTIVEC__)
#include <altivec.h>
#endif

void audio_mix_volume_C(float *out, const float *in, float vol, size_t samples)
{
   size_t i;
   for (i = 0; i < samples; i++)
      out[i] += in[i] * vol;
}

#ifdef __SSE2__
void audio_mix_volume_SSE2(float *out, const float *in, float vol, size_t samples)
{
   size_t i;
   __m128 volume = _mm_set1_ps(vol);

   for (i = 0; i + 16 <= samples; i += 16, out += 16, in += 16)
   {
      unsigned j;
      __m128 input[4];
      __m128 additive[4];
      
      input[0]    = _mm_loadu_ps(out +  0);
      input[1]    = _mm_loadu_ps(out +  4);
      input[2]    = _mm_loadu_ps(out +  8);
      input[3]    = _mm_loadu_ps(out + 12);

      additive[0] = _mm_mul_ps(volume, _mm_loadu_ps(in +  0));
      additive[1] = _mm_mul_ps(volume, _mm_loadu_ps(in +  4));
      additive[2] = _mm_mul_ps(volume, _mm_loadu_ps(in +  8));
      additive[3] = _mm_mul_ps(volume, _mm_loadu_ps(in + 12));

      for (j = 0; j < 4; j++)
         _mm_storeu_ps(out + 4 * j, _mm_add_ps(input[j], additive[j]));
   }

   audio_mix_volume_C(out, in, vol, samples - i);
}
#endif

