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

/* Convoluted Cosine Resampler */

#include <stdint.h>
#include <stdlib.h>

#ifdef __SSE__
#include <xmmintrin.h>
#endif

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <memalign.h>
#include <math/float_minmax.h>

#include <audio/audio_resampler.h>

/* Since SSE and NEON don't provide support for trigonometric functions
 * we approximate those with polynoms
 *
 * CC_RESAMPLER_PRECISION defines how accurate the approximation is
 * a setting of 5 or more means full precison.
 * setting 0 doesn't use a polynom
 * setting 1 uses P(X) = X - (3/4)*X^3 + (1/4)*X^5
 *
 * only 0 and 1 are implemented for SSE and NEON currently
 *
 * the MIPS_ARCH_ALLEGREX target doesnt require this setting since it has
 * native support for the required functions so it will always use full precision.
 */

#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif

typedef struct rarch_CC_resampler
{
   audio_frame_float_t buffer[4];

   float distance;
   void (*process)(void *re, struct resampler_data *data);
} rarch_CC_resampler_t;

#ifdef _MIPS_ARCH_ALLEGREX
static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   float ratio, fraction;
   audio_frame_float_t     *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)
      (inp + data->input_frames);
   audio_frame_float_t    *outp = (audio_frame_float_t*)data->data_out;

   (void)re_;

   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "mtv       %2,   s700              \n"   /* 700 = data->ratio = b */
         /*    "vsat0.s   s700, s700              \n" */
         "vrcp.s    s701, s700              \n"   /* 701 = 1.0 / b */
         "vadd.s    s702, s700, s700        \n"   /* 702 = 2 * b */
         "vmul.s    s703, s700, s710        \n"   /* 703 = b * pi */

         "mfv       %0,   s701              \n"
         "mfv       %1,   s730              \n"

         ".set      pop\n"
         : "=r"(ratio), "=r"(fraction)
         : "r"((float)data->ratio)
   );

   for (;;)
   {
      while (fraction < ratio)
      {
         if (inp == inp_max)
            goto done;
         __asm__ (
               ".set      push               \n"
               ".set      noreorder          \n"

               "lv.s    s620, 0(%1)             \n"
               "lv.s    s621, 4(%1)             \n"

               "vsub.s  s731, s701, s730     \n"

               "vadd.q  c600, c730[-X,Y,-X,Y], c730[1/2,1/2,-1/2,-1/2]\n"

               "vmul.q  c610, c600, c700[Z,Z,Z,Z]  \n"   /* *2*b */
               "vmul.q  c600, c600, c700[W,W,W,W]  \n"   /* *b*pi */
               "vsin.q  c610, c610                 \n"
               "vadd.q  c600, c600, c610           \n"

               "vmul.q  c600[-1:1,-1:1,-1:1,-1:1], c600, c710[Y,Y,Y,Y]	\n"

               "vsub.p  c600, c600, c602           \n"

               "vmul.q  c620, c620[X,Y,X,Y], c600[X,X,Y,Y]  \n"

               "vadd.q  c720, c720, c620           \n"

               "vadd.s  s730, s730, s730[1]  \n"
               "mfv     %0,   s730           \n"

               ".set      pop         \n"
               : "=r"(fraction)
               : "r"(inp));

         inp++;
      }
      __asm__ (
            ".set    push                       \n"
            ".set    noreorder                  \n"

            "vmul.p  c720, c720, c720[1/2,1/2]  \n"
            "sv.s    s720, 0(%1)                \n"
            "sv.s    s721, 4(%1)                \n"
            "vmov.q  c720, c720[Z,W,0,0]        \n"
            "vsub.s  s730, s730, s701           \n"
            "mfv     %0,   s730                 \n"

            ".set    pop                        \n"
            : "=r"(fraction)
            : "r"(outp));

      outp++;
   }

   /* The VFPU state is assumed to remain intact
    * in-between calls to resampler_CC_process. */

done:
   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void *resampler_CC_init(const struct resampler_config *config,
      double bandwidth_mod,
      enum resampler_quality quality,
      resampler_simd_mask_t mask)
{
   (void)mask;
   (void)bandwidth_mod;
   (void)config;

   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "vcst.s    s710, VFPU_PI           \n"   /* 710 = pi */
         "vcst.s    s711, VFPU_1_PI         \n"   /* 711 = 1.0 / (pi) */

         "vzero.q   c720                    \n"
         "vzero.q   c730                    \n"

         ".set      pop\n");

   return (void*)-1;
}
#else

#if defined(__SSE__)
#define CC_RESAMPLER_IDENT "SSE"

static void resampler_CC_downsample(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float ratio                  = 1.0 / data->ratio;
   float b                      = data->ratio; /* cutoff frequency. */

   __m128 vec_previous          = _mm_loadu_ps((float*)&re->buffer[0]);
   __m128 vec_current           = _mm_loadu_ps((float*)&re->buffer[2]);

   while (inp != inp_max)
   {
      __m128 vec_ww1, vec_ww2;
      __m128 vec_w_previous;
      __m128 vec_w_current;
      __m128 vec_in;
      __m128 vec_ratio =
         _mm_mul_ps(_mm_set_ps1(ratio), _mm_set_ps(3.0, 2.0, 1.0, 0.0));
      __m128 vec_w     = _mm_sub_ps(_mm_set_ps1(re->distance), vec_ratio);

      __m128 vec_w1    = _mm_add_ps(vec_w , _mm_set_ps1(0.5));
      __m128 vec_w2    = _mm_sub_ps(vec_w , _mm_set_ps1(0.5));

      __m128 vec_b     = _mm_set_ps1(b);

      vec_w1           = _mm_mul_ps(vec_w1, vec_b);
      vec_w2           = _mm_mul_ps(vec_w2, vec_b);

      (void)vec_ww1;
      (void)vec_ww2;

#if (CC_RESAMPLER_PRECISION > 0)
      vec_ww1 = _mm_mul_ps(vec_w1, vec_w1);
      vec_ww2 = _mm_mul_ps(vec_w2, vec_w2);

      vec_ww1 = _mm_mul_ps(vec_ww1, _mm_sub_ps(_mm_set_ps1(3.0),vec_ww1));
      vec_ww2 = _mm_mul_ps(vec_ww2, _mm_sub_ps(_mm_set_ps1(3.0),vec_ww2));

      vec_ww1 = _mm_mul_ps(_mm_set_ps1(1.0/4.0), vec_ww1);
      vec_ww2 = _mm_mul_ps(_mm_set_ps1(1.0/4.0), vec_ww2);

      vec_w1  = _mm_mul_ps(vec_w1, _mm_sub_ps(_mm_set_ps1(1.0), vec_ww1));
      vec_w2  = _mm_mul_ps(vec_w2, _mm_sub_ps(_mm_set_ps1(1.0), vec_ww2));
#endif

      vec_w1  = _mm_min_ps(vec_w1, _mm_set_ps1( 0.5));
      vec_w2  = _mm_min_ps(vec_w2, _mm_set_ps1( 0.5));
      vec_w1  = _mm_max_ps(vec_w1, _mm_set_ps1(-0.5));
      vec_w2  = _mm_max_ps(vec_w2, _mm_set_ps1(-0.5));
      vec_w   = _mm_sub_ps(vec_w1, vec_w2);

      vec_w_previous =
         _mm_shuffle_ps(vec_w,vec_w,_MM_SHUFFLE(1, 1, 0, 0));
      vec_w_current  =
         _mm_shuffle_ps(vec_w,vec_w,_MM_SHUFFLE(3, 3, 2, 2));

      vec_in = _mm_loadl_pi(_mm_setzero_ps(),(__m64*)inp);
      vec_in = _mm_shuffle_ps(vec_in,vec_in,_MM_SHUFFLE(1, 0, 1, 0));

      vec_previous =
         _mm_add_ps(vec_previous, _mm_mul_ps(vec_in, vec_w_previous));
      vec_current  =
         _mm_add_ps(vec_current, _mm_mul_ps(vec_in, vec_w_current));

      re->distance++;
      inp++;

      if (re->distance > (ratio + 0.5))
      {
         _mm_storel_pi((__m64*)outp, vec_previous);
         vec_previous =
            _mm_shuffle_ps(vec_previous,vec_current,_MM_SHUFFLE(1, 0, 3, 2));
         vec_current  =
            _mm_shuffle_ps(vec_current,_mm_setzero_ps(),_MM_SHUFFLE(1, 0, 3, 2));

         re->distance -= ratio;
         outp++;
      }
   }

   _mm_storeu_ps((float*)&re->buffer[0], vec_previous);
   _mm_storeu_ps((float*)&re->buffer[2],  vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void resampler_CC_upsample(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float b                      = float_min(data->ratio, 1.00); /* cutoff frequency. */
   float ratio                  = 1.0 / data->ratio;
   __m128 vec_previous          = _mm_loadu_ps((float*)&re->buffer[0]);
   __m128 vec_current           = _mm_loadu_ps((float*)&re->buffer[2]);

   while (inp != inp_max)
   {
      __m128 vec_in = _mm_loadl_pi(_mm_setzero_ps(),(__m64*)inp);
      vec_previous =
         _mm_shuffle_ps(vec_previous,vec_current,_MM_SHUFFLE(1, 0, 3, 2));
      vec_current  =
         _mm_shuffle_ps(vec_current,vec_in,_MM_SHUFFLE(1, 0, 3, 2));

      while (re->distance < 1.0)
      {
         __m128 vec_w_previous, vec_w_current, vec_out;
#if (CC_RESAMPLER_PRECISION > 0)
         __m128 vec_ww1, vec_ww2;
#endif
         __m128 vec_w =
            _mm_add_ps(_mm_set_ps1(re->distance), _mm_set_ps(-2.0, -1.0, 0.0, 1.0));

         __m128 vec_w1 = _mm_add_ps(vec_w , _mm_set_ps1(0.5));
         __m128 vec_w2 = _mm_sub_ps(vec_w , _mm_set_ps1(0.5));

         __m128 vec_b = _mm_set_ps1(b);
         vec_w1 = _mm_mul_ps(vec_w1, vec_b);
         vec_w2 = _mm_mul_ps(vec_w2, vec_b);

#if (CC_RESAMPLER_PRECISION > 0)
         vec_ww1 = _mm_mul_ps(vec_w1, vec_w1);
         vec_ww2 = _mm_mul_ps(vec_w2, vec_w2);

         vec_ww1 = _mm_mul_ps(vec_ww1,_mm_sub_ps(_mm_set_ps1(3.0),vec_ww1));
         vec_ww2 = _mm_mul_ps(vec_ww2,_mm_sub_ps(_mm_set_ps1(3.0),vec_ww2));

         vec_ww1 = _mm_mul_ps(_mm_set_ps1(1.0 / 4.0), vec_ww1);
         vec_ww2 = _mm_mul_ps(_mm_set_ps1(1.0 / 4.0), vec_ww2);

         vec_w1  = _mm_mul_ps(vec_w1, _mm_sub_ps(_mm_set_ps1(1.0), vec_ww1));
         vec_w2  = _mm_mul_ps(vec_w2, _mm_sub_ps(_mm_set_ps1(1.0), vec_ww2));
#endif

         vec_w1  = _mm_min_ps(vec_w1, _mm_set_ps1( 0.5));
         vec_w2  = _mm_min_ps(vec_w2, _mm_set_ps1( 0.5));
         vec_w1  = _mm_max_ps(vec_w1, _mm_set_ps1(-0.5));
         vec_w2  = _mm_max_ps(vec_w2, _mm_set_ps1(-0.5));

         vec_w   = _mm_sub_ps(vec_w1, vec_w2);

         vec_w_previous = _mm_shuffle_ps(vec_w,vec_w,_MM_SHUFFLE(1, 1, 0, 0));
         vec_w_current  = _mm_shuffle_ps(vec_w,vec_w,_MM_SHUFFLE(3, 3, 2, 2));

         vec_out =  _mm_mul_ps(vec_previous, vec_w_previous);
         vec_out = _mm_add_ps(vec_out, _mm_mul_ps(vec_current, vec_w_current));
         vec_out =
            _mm_add_ps(vec_out, _mm_shuffle_ps(vec_out,vec_out,_MM_SHUFFLE(3, 2, 3, 2)));

         _mm_storel_pi((__m64*)outp,vec_out);

         re->distance += ratio;
         outp++;
      }

      re->distance -= 1.0;
      inp++;
   }

   _mm_storeu_ps((float*)&re->buffer[0], vec_previous);
   _mm_storeu_ps((float*)&re->buffer[2],  vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

#elif defined (__ARM_NEON__) && !defined(DONT_WANT_ARM_OPTIMIZATIONS)

#define CC_RESAMPLER_IDENT "NEON"

size_t resampler_CC_downsample_neon(float *outp, const float *inp,
      rarch_CC_resampler_t* re_, size_t input_frames, float ratio);
size_t resampler_CC_upsample_neon  (float *outp, const float *inp,
      rarch_CC_resampler_t* re_, size_t input_frames, float ratio);

static void resampler_CC_downsample(void *re_, struct resampler_data *data)
{
   data->output_frames = resampler_CC_downsample_neon(
         data->data_out, data->data_in, re_, data->input_frames, data->ratio);
}

static void resampler_CC_upsample(void *re_, struct resampler_data *data)
{
   data->output_frames = resampler_CC_upsample_neon(
         data->data_out, data->data_in, re_, data->input_frames, data->ratio);
}

#else

/* C reference version. Not optimized. */

#define CC_RESAMPLER_IDENT "C"

#if (CC_RESAMPLER_PRECISION > 4)
static INLINE float cc_int(float x, float b)
{
   float val = x * b * M_PI + sinf(x * b * M_PI);
   return (val > M_PI) ? M_PI : (val < -M_PI) ? -M_PI : val;
}

#define cc_kernel(x, b)    ((cc_int((x) + 0.5, (b)) - cc_int((x) - 0.5, (b))) / (2.0 * M_PI))
#else
static INLINE float cc_int(float x, float b)
{
   float val = x * b;
#if (CC_RESAMPLER_PRECISION > 0)
   val = val*(1 - 0.25 * val * val * (3.0 - val * val));
#endif
   return (val > 0.5) ? 0.5 : (val < -0.5) ? -0.5 : val;
}

#define cc_kernel(x, b)    ((cc_int((x) + 0.5, (b)) - cc_int((x) - 0.5, (b))))
#endif

static INLINE void add_to(const audio_frame_float_t *source,
      audio_frame_float_t *target, float ratio)
{
   target->l += source->l * ratio;
   target->r += source->r * ratio;
}

static void resampler_CC_downsample(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)
      (inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float                  ratio = 1.0 / data->ratio;
   float                      b = data->ratio; /* cutoff frequency. */

   while (inp != inp_max)
   {
      add_to(inp, re->buffer + 0, cc_kernel(re->distance, b));
      add_to(inp, re->buffer + 1, cc_kernel(re->distance - ratio, b));
      add_to(inp, re->buffer + 2, cc_kernel(re->distance - ratio - ratio, b));

      re->distance++;
      inp++;

      if (re->distance > (ratio + 0.5))
      {
         *outp = re->buffer[0];

         re->buffer[0] = re->buffer[1];
         re->buffer[1] = re->buffer[2];

         re->buffer[2].l = 0.0;
         re->buffer[2].r = 0.0;

         re->distance -= ratio;
         outp++;
      }
   }

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void resampler_CC_upsample(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)
      (inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float                      b = float_min(data->ratio, 1.00); /* cutoff frequency. */
   float                  ratio = 1.0 / data->ratio;

   while (inp != inp_max)
   {
      re->buffer[0] = re->buffer[1];
      re->buffer[1] = re->buffer[2];
      re->buffer[2] = re->buffer[3];
      re->buffer[3] = *inp;

      while (re->distance < 1.0)
      {
         int i;

         outp->l = 0.0;
         outp->r = 0.0;

         for (i = 0; i < 4; i++)
         {
            float temp = cc_kernel(re->distance + 1.0 - i, b);
            outp->l   += re->buffer[i].l * temp;
            outp->r   += re->buffer[i].r * temp;
         }

         re->distance += ratio;
         outp++;
      }

      re->distance -= 1.0;
      inp++;
   }

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}
#endif

static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   if (re)
      re->process(re_, data);
}

static void *resampler_CC_init(const struct resampler_config *config,
      double bandwidth_mod,
      enum resampler_quality quality,
      resampler_simd_mask_t mask)
{
   int i;
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)
      memalign_alloc(32, sizeof(rarch_CC_resampler_t));

   /* TODO: lookup if NEON support can be detected at
    * runtime and a funcptr set at runtime for either
    * C codepath or NEON codepath. This will help out
    * Android. */
   (void)mask;
   (void)config;
   if (!re)
      return NULL;

   for (i = 0; i < 4; i++)
   {
      re->buffer[i].l = 0.0;
      re->buffer[i].r = 0.0;
   }

   /* Variations of data->ratio around 0.75 are safer
    * than around 1.0 for both up/downsampler. */
   if (bandwidth_mod < 0.75)
   {
      re->process = resampler_CC_downsample;
      re->distance = 0.0;
   }
   else
   {
      re->process = resampler_CC_upsample;
      re->distance = 2.0;
   }

   return re;
}
#endif

static void resampler_CC_free(void *re_)
{
#ifndef _MIPS_ARCH_ALLEGREX
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   if (re)
      memalign_free(re);
#endif
   (void)re_;
}

retro_resampler_t CC_resampler = {
   resampler_CC_init,
   resampler_CC_process,
   resampler_CC_free,
   RESAMPLER_API_VERSION,
   "CC",
   "cc"
};
