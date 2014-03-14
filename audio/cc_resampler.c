/*
 * Convoluted Cosine Resampler
 * Copyright (C) 2014 - Ali Bouhlel ( aliaspider@gmail.com )
 *
 * licence: GPLv3
 */

#include "resampler.h"
#include "../libretro.h"
#include "../performance.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "../general.h"

typedef struct rarch_CC_resampler
{
   int dummy;
}rarch_CC_resampler_t;

typedef struct audio_frame_float
{
   float l;
   float r;
}audio_frame_float_t;

typedef struct audio_frame_int16
{
   int16_t l;
   int16_t r;
}audio_frame_int16_t;


#ifdef _MIPS_ARCH_ALLEGREX
static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   (void)re_;
//   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;


   float ratio,fraction;


   audio_frame_float_t *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = inp + data->input_frames;
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
    :"=r"(ratio),"=r"(fraction): "r"((float)data->ratio)
   );

   while(true)
   {
      while ((fraction < ratio))
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
         :"=r"(fraction): "r"(inp)
         );
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
      :"=r"(fraction): "r"(outp)
      );
      outp++;
   }

done:
   data->output_frames = (outp - (audio_frame_float_t*)data->data_out);
}
#else
#error "platform not supported"
#endif

static void resampler_CC_free(void *re_)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   if (re)
      free(re);
}

static void *resampler_CC_init(double bandwidth_mod)
{
   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)calloc(1, sizeof(rarch_CC_resampler_t));
   if (!re)
      return NULL;

   __asm__ (
    ".set      push\n"
    ".set      noreorder\n"

    "vcst.s    s710, VFPU_PI           \n"   // 710 = pi
    "vcst.s    s711, VFPU_1_PI         \n"   // 711 = 1.0 / (pi)

    "vzero.q   c720                    \n"
    "vzero.q   c730                    \n"

    ".set      pop\n"
   );

   return re;
}

const rarch_resampler_t CC_resampler = {
   resampler_CC_init,
   resampler_CC_process,
   resampler_CC_free,
   "CC",
};



