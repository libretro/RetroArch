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

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
#define CC_HAVE_NEON 1
#include <arm_neon.h>
#else
#define CC_HAVE_NEON 0
#endif

#include <math.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <retro_miscellaneous.h>
#include <memalign.h>
#include <math/float_minmax.h>

#include <audio/audio_resampler.h>

/* Since SSE and NEON don't provide support for trigonometric functions
 * we approximate those with polynoms
 *
 * CC_RESAMPLER_PRECISION defines how accurate the approximation is
 * a setting of 5 or more means full precision.
 * setting 0 doesn't use a polynom
 * setting 1 uses P(X) = X - (3/4)*X^3 + (1/4)*X^5
 *
 * Only 0 and 1 have a vector kernel, so above 4 the reference is the
 * only arm built: otherwise one binary would resample differently
 * depending on which arm the mask picked. It costs about 21 dB of
 * THD+N at 100 Hz and nothing above 2 kHz, measured against the
 * polynomial at 32 to 48 kHz.
 *
 * the MIPS_ARCH_ALLEGREX target doesnt require this setting since it has
 * native support for the required functions so it will always use full precision.
 */

#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif

/* The vector arms implement the polynomial kernel only. */
#if (CC_RESAMPLER_PRECISION > 4)
#define CC_VECTOR_ARMS 0
#else
#define CC_VECTOR_ARMS 1
#endif

typedef struct rarch_CC_resampler
{
   /* The stream's state leads the struct so the SIMD arms can load and
    * store it aligned, and so it sits at the offsets the NEON kernels
    * address. */
   audio_frame_float_t buffer[4];
   float distance;
   /* The pair the mask picked at init, and whichever of them the
    * current ratio calls for. */
   void (*process)(void *re, struct resampler_data *data);
   void (*upsample)(void *re, struct resampler_data *data);
   void (*downsample)(void *re, struct resampler_data *data);
} rarch_CC_resampler_t;

/* struct resampler_data carries no output capacity, so the arms that
 * emit more than one frame per input frame stop at what the ratio asks
 * for. A ratio no pair of sample rates can name resamples nothing. */
#define CC_RESAMPLER_RATIO_MAX 65536.0

#define CC_RATIO_USABLE(r) ((r) > 0.0 && (r) <= CC_RESAMPLER_RATIO_MAX)

static INLINE size_t resampler_CC_out_max(const struct resampler_data *data)
{
   if (!CC_RATIO_USABLE(data->ratio))
      return 0;
   return (size_t)((double)data->input_frames * data->ratio) + 2;
}

#ifdef _MIPS_ARCH_ALLEGREX
/* The VFPU is per-thread context, and the frontend creates a resampler
 * on the main thread but runs it on the audio worker, so nothing in
 * those registers survives from init() to process(). One register file
 * also cannot hold several instances at once. The constants are issued
 * per call and the state travels in the handle. */
typedef struct rarch_CC_resampler_psp
{
   /* c720, the output pair, then c730, the position within it. Quad
    * loads and stores, so 16-byte aligned by the allocation. */
   float state[8];
} rarch_CC_resampler_psp_t;

static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   float ratio, fraction;
   rarch_CC_resampler_psp_t *re = (rarch_CC_resampler_psp_t*)re_;
   audio_frame_float_t     *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)
      (inp + data->input_frames);
   audio_frame_float_t    *outp = (audio_frame_float_t*)data->data_out;
   audio_frame_float_t *outp_max = outp + resampler_CC_out_max(data);

   if (!re)
   {
      data->output_frames = 0;
      return;
   }

   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "mtv       %2,   s700              \n"   /* 700 = data->ratio = b */
         /*    "vsat0.s   s700, s700              \n" */
         "lv.q      c720,  0(%3)            \n"   /* the output pair */
         "lv.q      c730, 16(%3)            \n"   /* the position within it */
         "vcst.s    s710, VFPU_PI           \n"   /* 710 = pi */
         "vcst.s    s711, VFPU_1_PI         \n"   /* 711 = 1.0 / (pi) */
         "vrcp.s    s701, s700              \n"   /* 701 = 1.0 / b */
         "vadd.s    s702, s700, s700        \n"   /* 702 = 2 * b */
         "vmul.s    s703, s700, s710        \n"   /* 703 = b * pi */

         "mfv       %0,   s701              \n"
         "mfv       %1,   s730              \n"

         ".set      pop\n"
         : "=r"(ratio), "=r"(fraction)
         : "r"((float)data->ratio), "r"(re->state)
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
      if (outp == outp_max)
         goto done;
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

done:
   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "sv.q      c720,  0(%0)            \n"
         "sv.q      c730, 16(%0)            \n"

         ".set      pop\n"
         :: "r"(re->state));

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void *resampler_CC_init(const struct resampler_config *config,
      double bandwidth_mod,
      enum resampler_quality quality,
      resampler_simd_mask_t mask)
{
   int i;
   rarch_CC_resampler_psp_t *re;

   (void)mask;
   (void)config;

   if (!CC_RATIO_USABLE(bandwidth_mod))
      return NULL;
   if (!(re = (rarch_CC_resampler_psp_t*)
            memalign_alloc(16, sizeof(rarch_CC_resampler_psp_t))))
      return NULL;

   for (i = 0; i < 8; i++)
      re->state[i] = 0.0f;

   return re;
}
#else


/* The scalar reference is always built: it is the fallback when the
 * mask names nothing this build has a kernel for. */
/* C reference version. Not optimized. */


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

static void resampler_CC_downsample_c(void *re_, struct resampler_data *data)
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

static void resampler_CC_upsample_c(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)
      (inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   audio_frame_float_t *outp_max = outp + resampler_CC_out_max(data);
   float                      b = float_min(data->ratio, 1.00); /* cutoff frequency. */
   float                  ratio = 1.0 / data->ratio;

   while (inp != inp_max)
   {
      re->buffer[0] = re->buffer[1];
      re->buffer[1] = re->buffer[2];
      re->buffer[2] = re->buffer[3];
      re->buffer[3] = *inp;

      while (re->distance < 1.0 && outp != outp_max)
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

#if defined(__SSE__) && CC_VECTOR_ARMS
static void resampler_CC_downsample_sse(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float ratio                  = 1.0 / data->ratio;
   float b                      = data->ratio; /* cutoff frequency. */

   __m128 vec_previous          = _mm_load_ps((float*)&re->buffer[0]);
   __m128 vec_current           = _mm_load_ps((float*)&re->buffer[2]);

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

   _mm_store_ps((float*)&re->buffer[0], vec_previous);
   _mm_store_ps((float*)&re->buffer[2],  vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void resampler_CC_upsample_sse(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   audio_frame_float_t *outp_max = outp + resampler_CC_out_max(data);
   float b                      = float_min(data->ratio, 1.00); /* cutoff frequency. */
   float ratio                  = 1.0 / data->ratio;
   __m128 vec_previous          = _mm_load_ps((float*)&re->buffer[0]);
   __m128 vec_current           = _mm_load_ps((float*)&re->buffer[2]);

   while (inp != inp_max)
   {
      __m128 vec_in = _mm_loadl_pi(_mm_setzero_ps(),(__m64*)inp);
      vec_previous =
         _mm_shuffle_ps(vec_previous,vec_current,_MM_SHUFFLE(1, 0, 3, 2));
      vec_current  =
         _mm_shuffle_ps(vec_current,vec_in,_MM_SHUFFLE(1, 0, 3, 2));

      while (re->distance < 1.0 && outp != outp_max)
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

   _mm_store_ps((float*)&re->buffer[0], vec_previous);
   _mm_store_ps((float*)&re->buffer[2],  vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}
#endif


#if CC_HAVE_NEON && CC_VECTOR_ARMS
/* The SSE kernels' lanes, in NEON: vzipq gives the {0,0,1,1} and
 * {2,2,3,3} spreads _mm_shuffle_ps built, and vcombine the half
 * splices. */
static INLINE float32x4_t cc_neon_window(float32x4_t vec_w, float32x4_t vec_b)
{
   float32x4_t vec_w1 = vmulq_f32(vaddq_f32(vec_w, vdupq_n_f32(0.5f)), vec_b);
   float32x4_t vec_w2 = vmulq_f32(vsubq_f32(vec_w, vdupq_n_f32(0.5f)), vec_b);
#if (CC_RESAMPLER_PRECISION > 0)
   float32x4_t vec_ww1 = vmulq_f32(vec_w1, vec_w1);
   float32x4_t vec_ww2 = vmulq_f32(vec_w2, vec_w2);

   vec_ww1 = vmulq_f32(vec_ww1, vsubq_f32(vdupq_n_f32(3.0f), vec_ww1));
   vec_ww2 = vmulq_f32(vec_ww2, vsubq_f32(vdupq_n_f32(3.0f), vec_ww2));

   vec_ww1 = vmulq_f32(vdupq_n_f32(0.25f), vec_ww1);
   vec_ww2 = vmulq_f32(vdupq_n_f32(0.25f), vec_ww2);

   vec_w1  = vmulq_f32(vec_w1, vsubq_f32(vdupq_n_f32(1.0f), vec_ww1));
   vec_w2  = vmulq_f32(vec_w2, vsubq_f32(vdupq_n_f32(1.0f), vec_ww2));
#endif
   vec_w1  = vminq_f32(vec_w1, vdupq_n_f32( 0.5f));
   vec_w2  = vminq_f32(vec_w2, vdupq_n_f32( 0.5f));
   vec_w1  = vmaxq_f32(vec_w1, vdupq_n_f32(-0.5f));
   vec_w2  = vmaxq_f32(vec_w2, vdupq_n_f32(-0.5f));
   return vsubq_f32(vec_w1, vec_w2);
}

static void resampler_CC_downsample_neon(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;
   float ratio                  = 1.0 / data->ratio;
   float b                      = data->ratio; /* cutoff frequency. */
   const float32x4_t vec_step   = { 0.0f, 1.0f, 2.0f, 3.0f };
   float32x4_t vec_b            = vdupq_n_f32(b);
   float32x4_t vec_previous     = vld1q_f32((const float*)&re->buffer[0]);
   float32x4_t vec_current      = vld1q_f32((const float*)&re->buffer[2]);

   while (inp != inp_max)
   {
      float32x4x2_t spread;
      float32x2_t   in2   = vld1_f32((const float*)inp);
      float32x4_t   vec_in;
      float32x4_t   vec_w = cc_neon_window(
            vsubq_f32(vdupq_n_f32(re->distance),
                      vmulq_f32(vdupq_n_f32(ratio), vec_step)), vec_b);

      spread       = vzipq_f32(vec_w, vec_w);
      vec_in       = vcombine_f32(in2, in2);

      vec_previous = vaddq_f32(vec_previous, vmulq_f32(vec_in, spread.val[0]));
      vec_current  = vaddq_f32(vec_current,  vmulq_f32(vec_in, spread.val[1]));

      re->distance++;
      inp++;

      if (re->distance > (ratio + 0.5))
      {
         vst1_f32((float*)outp, vget_low_f32(vec_previous));
         vec_previous = vcombine_f32(vget_high_f32(vec_previous),
                                     vget_low_f32(vec_current));
         vec_current  = vcombine_f32(vget_high_f32(vec_current),
                                     vdup_n_f32(0.0f));
         re->distance -= ratio;
         outp++;
      }
   }

   vst1q_f32((float*)&re->buffer[0], vec_previous);
   vst1q_f32((float*)&re->buffer[2], vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void resampler_CC_upsample_neon(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re      = (rarch_CC_resampler_t*)re_;
   audio_frame_float_t *inp      = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max  = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp     = (audio_frame_float_t*)data->data_out;
   audio_frame_float_t *outp_max = outp + resampler_CC_out_max(data);
   float b                       = float_min(data->ratio, 1.00); /* cutoff frequency. */
   float ratio                   = 1.0 / data->ratio;
   const float32x4_t vec_taps    = { 1.0f, 0.0f, -1.0f, -2.0f };
   float32x4_t vec_b             = vdupq_n_f32(b);
   float32x4_t vec_previous      = vld1q_f32((const float*)&re->buffer[0]);
   float32x4_t vec_current       = vld1q_f32((const float*)&re->buffer[2]);

   while (inp != inp_max)
   {
      float32x2_t in2 = vld1_f32((const float*)inp);

      vec_previous = vcombine_f32(vget_high_f32(vec_previous),
                                  vget_low_f32(vec_current));
      vec_current  = vcombine_f32(vget_high_f32(vec_current), in2);

      while (re->distance < 1.0 && outp != outp_max)
      {
         float32x4x2_t spread;
         float32x4_t   vec_out;
         float32x4_t   vec_w = cc_neon_window(
               vaddq_f32(vdupq_n_f32(re->distance), vec_taps), vec_b);

         spread  = vzipq_f32(vec_w, vec_w);

         vec_out = vmulq_f32(vec_previous, spread.val[0]);
         vec_out = vaddq_f32(vec_out, vmulq_f32(vec_current, spread.val[1]));
         vec_out = vaddq_f32(vec_out,
               vcombine_f32(vget_high_f32(vec_out), vget_high_f32(vec_out)));

         vst1_f32((float*)outp, vget_low_f32(vec_out));

         re->distance += ratio;
         outp++;
      }

      re->distance -= 1.0;
      inp++;
   }

   vst1q_f32((float*)&re->buffer[0], vec_previous);
   vst1q_f32((float*)&re->buffer[2], vec_current);

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}
#endif


static void resampler_CC_reset(void *re_);

static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   if (!re)
      return;
   /* init picks the direction from the nominal ratio, but the ratio
    * moves at runtime - slow motion multiplies it. The downsampler
    * emits at most one frame per input frame, so it cannot serve a
    * ratio above one. The upsampler serves either, to the same output
    * below one, so the stream moves there and stays; the two carry
    * distance differently, so the move restarts it. */
   if (re->process == re->downsample && data->ratio > 1.0)
   {
      re->process = re->upsample;
      resampler_CC_reset(re);
   }
   re->process(re_, data);
}

static void *resampler_CC_init(const struct resampler_config *config,
      double bandwidth_mod,
      enum resampler_quality quality,
      resampler_simd_mask_t mask)
{
   int i;
   rarch_CC_resampler_t *re;

   (void)config;
   if (!CC_RATIO_USABLE(bandwidth_mod))
      return NULL;
   if (!(re = (rarch_CC_resampler_t*)
            memalign_alloc(32, sizeof(rarch_CC_resampler_t))))
      return NULL;

   re->upsample   = resampler_CC_upsample_c;
   re->downsample = resampler_CC_downsample_c;
#if defined(__SSE__) && CC_VECTOR_ARMS
   if (mask & RESAMPLER_SIMD_SSE)
   {
      re->upsample   = resampler_CC_upsample_sse;
      re->downsample = resampler_CC_downsample_sse;
   }
#endif
#if CC_HAVE_NEON && CC_VECTOR_ARMS
   if (mask & RESAMPLER_SIMD_NEON)
   {
      re->upsample   = resampler_CC_upsample_neon;
      re->downsample = resampler_CC_downsample_neon;
   }
#endif

   for (i = 0; i < 4; i++)
   {
      re->buffer[i].l = 0.0;
      re->buffer[i].r = 0.0;
   }

   /* Variations of data->ratio around 0.75 are safer
    * than around 1.0 for both up/downsampler. */
   if (bandwidth_mod < 0.75)
   {
      re->process = re->downsample;
      re->distance = 0.0;
   }
   else
   {
      re->process = re->upsample;
      re->distance = 2.0;
   }

   return re;
}
#endif

static void resampler_CC_free(void *re_)
{
   if (re_)
      memalign_free(re_);
}

#ifdef _MIPS_ARCH_ALLEGREX
static void resampler_CC_reset(void *re_)
{
   rarch_CC_resampler_psp_t *re = (rarch_CC_resampler_psp_t*)re_;
   int i;
   if (!re)
      return;
   for (i = 0; i < 8; i++)
      re->state[i] = 0.0f;
}
#else
static void resampler_CC_reset(void *re_)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   int i;
   if (!re)
      return;
   for (i = 0; i < 4; i++)
   {
      re->buffer[i].l = 0.0;
      re->buffer[i].r = 0.0;
   }
   /* The starting distance init chose for the direction. */
   re->distance = (re->process == re->upsample) ? 2.0 : 0.0;
}
#endif

retro_resampler_t CC_resampler = {
   resampler_CC_init,
   resampler_CC_process,
   resampler_CC_free,
   RESAMPLER_API_VERSION,
   "CC",
   "cc",
   resampler_CC_reset,
   0
};
