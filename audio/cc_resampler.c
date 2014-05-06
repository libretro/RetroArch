/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014 - Ali Bouhlel ( aliaspider@gmail.com )
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

// Convoluted Cosine Resampler

#include "resampler.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef RESAMPLER_TEST
#include "../general.h"
#else
#define RARCH_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif

typedef struct audio_frame_float
{
   float l;
   float r;
} audio_frame_float_t;

typedef struct audio_frame_int16
{
   int16_t l;
   int16_t r;
} audio_frame_int16_t;

#ifdef _MIPS_ARCH_ALLEGREX1
static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   (void)re_;
   float ratio, fraction;

   audio_frame_float_t *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp = (audio_frame_float_t*)data->data_out;

   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "mtv       %2,   s700              \n"   // 700 = data->ratio = b
         //    "vsat0.s   s700, s700              \n"
         "vrcp.s    s701, s700              \n"   // 701 = 1.0 / b
         "vadd.s    s702, s700, s700        \n"   // 702 = 2 * b
         "vmul.s    s703, s700, s710        \n"   // 703 = b * pi

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
         __asm__ (
               ".set      push               \n"
               ".set      noreorder          \n"

               "lv.s    s620, 0(%1)             \n"
               "lv.s    s621, 4(%1)             \n"

               "vsub.s  s731, s701, s730     \n"

               "vadd.q  c600, c730[-X,Y,-X,Y], c730[1/2,1/2,-1/2,-1/2]\n"

               "vmul.q  c610, c600, c700[Z,Z,Z,Z]  \n"   //*2*b
               "vmul.q  c600, c600, c700[W,W,W,W]  \n"   //*b*pi
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
         if (inp == inp_max)
            goto done;
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

   // The VFPU state is assumed to remain intact in-between calls to resampler_CC_process.

done:
   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}


static void resampler_CC_free(void *re_)
{
   (void)re_;
}

static void *resampler_CC_init(double bandwidth_mod)
{
   __asm__ (
         ".set      push\n"
         ".set      noreorder\n"

         "vcst.s    s710, VFPU_PI           \n"   // 710 = pi
         "vcst.s    s711, VFPU_1_PI         \n"   // 711 = 1.0 / (pi)

         "vzero.q   c720                    \n"
         "vzero.q   c730                    \n"

         ".set      pop\n");

   RARCH_LOG("\nConvoluted Cosine resampler (VFPU): \n");
   return (void*)-1;
}
#else

// C reference version. Not optimized.
typedef struct rarch_CC_resampler
{
   audio_frame_float_t buffer[4];
   float distance;
   void (*process)(void *re, struct resampler_data *data);
} rarch_CC_resampler_t;

static inline float cc_int(float x, float b)
{
   float val = x * b * M_PI + sinf(x * b * M_PI);
   return (val > M_PI) ? M_PI : (val < -M_PI) ? -M_PI : val;
}

static inline float cc_kernel(float x, float b)
{
   return (cc_int(x + 0.5, b) - cc_int(x - 0.5, b)) / (2.0 * M_PI);
}

static inline void add_to(const audio_frame_float_t *source, audio_frame_float_t *target, float ratio)
{
   target->l += source->l * ratio;
   target->r += source->r * ratio;
}

static void resampler_CC_downsample(void *re_, struct resampler_data *data)
{
   float ratio, b;
   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = (audio_frame_float_t*)(inp + data->input_frames);
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;

   ratio = 1.0 / data->ratio;
   b = data->ratio; // cutoff frequency

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

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

static void resampler_CC_upsample(void *re_, struct resampler_data *data)
{
   float b, ratio;
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = inp + data->input_frames;
   audio_frame_float_t *outp = (audio_frame_float_t*)data->data_out;

   b = min(data->ratio, 1.00); // cutoff frequency
   ratio = 1.0 / data->ratio;

   while (inp != inp_max)
   {
      re->buffer[0] = re->buffer[1];
      re->buffer[1] = re->buffer[2];
      re->buffer[2] = re->buffer[3];
      re->buffer[3] = *inp;

      while (re->distance < 1.0)
      {
         int i;
         float temp;
         outp->l = 0.0;
         outp->r = 0.0;

         for (i = 0; i < 4; i++)
         {
            temp = cc_kernel(re->distance + 1.0 - i, b);
            outp->l += re->buffer[i].l * temp;
            outp->r += re->buffer[i].r * temp;
         }

         re->distance += ratio;
         outp++;
      }

      re->distance -= 1.0;
      inp++;
   }

   data->output_frames = outp - (audio_frame_float_t*)data->data_out;
}

static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   re->process(re_, data);
}

static void resampler_CC_free(void *re_)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   if (re)
      free(re);
}

static void *resampler_CC_init(double bandwidth_mod)
{
   int i;
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)calloc(1, sizeof(rarch_CC_resampler_t));
   if (!re)
      return NULL;

   for (i = 0; i < 4; i++)
   {
      re->buffer[i].l = 0.0;
      re->buffer[i].r = 0.0;
   }

   RARCH_LOG("Convoluted Cosine resampler (C) : ");

   if (bandwidth_mod < 0.75) // variations of data->ratio around 0.75 are safer than around 1.0 for both up/downsampler.
   {
      RARCH_LOG("CC_downsample @%f \n", bandwidth_mod);
      re->process = resampler_CC_downsample;
      re->distance = 0.0;
   }
   else
   {
      RARCH_LOG("CC_upsample @%f \n", bandwidth_mod);
      re->process = resampler_CC_upsample;
      re->distance = 2.0;
   }

   return re;
}
#endif

const rarch_resampler_t CC_resampler = {
   resampler_CC_init,
   resampler_CC_process,
   resampler_CC_free,
   "CC",
};

