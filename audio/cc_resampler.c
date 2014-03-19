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

#ifndef RESAMPLER_TEST
#include "../general.h"
#else
#define RARCH_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif


#ifdef _MIPS_ARCH_ALLEGREX1
typedef struct rarch_CC_resampler
{
   int dummy;
}rarch_CC_resampler_t;

static void resampler_CC_process(void *re_, struct resampler_data *data)
{
   (void)re_;
//   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;
   float ratio,fraction;

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

   RARCH_LOG("\nConvoluted Cosine resampler (VFPU): \n");
   return re;
}
#else


//#define HAVE_SSE_MATHFUN_H
#if defined(__SSE2__) && defined(HAVE_SSE_MATHFUN_H)
#define USE_SSE2
#include "sse_mathfun.h"

static inline float _mm_sin(float x)
{
   static float temp;
   __m128 vector =  _mm_set1_ps(x);
   vector = sin_ps(vector);
   _mm_store1_ps(&temp,vector);
   return temp;
}
static inline float _mm_cos(float x)
{
   static float temp;
   __m128 vector =  _mm_set1_ps(x);
   vector = cos_ps(vector);
   _mm_store1_ps(&temp,vector);
   return temp;
}

#define sin(x) _mm_sin(x)
#define cos(x) _mm_cos(x)
#endif


typedef struct audio_frame_float
{
   float l;
   float r;
}audio_frame_float_t;

typedef struct rarch_CC_resampler
{
   audio_frame_float_t buffer[4];
   float distance;
   void (*process)(void *re, struct resampler_data *data);

}rarch_CC_resampler_t;



static inline float cc_int(float x, float b){
   float val = x * b * M_PI + sin(x * b * M_PI);
   return (val > M_PI)? M_PI : (val < -M_PI)? -M_PI : val;
}

static inline float cc_kernel(float x, float b){
   return (cc_int(x + 0.5, b) - cc_int(x - 0.5, b)) / (2.0 * M_PI);
}

static inline void add_to(const audio_frame_float_t* source,audio_frame_float_t* target, float ratio){
   target->l += source->l * ratio;
   target->r += source->r * ratio;
}

static void resampler_CC_downsample(void *re_, struct resampler_data *data)
{

   rarch_CC_resampler_t *re     = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp     = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = inp + data->input_frames;
   audio_frame_float_t *outp    = (audio_frame_float_t*)data->data_out;

   float ratio = 1.0 / data->ratio;

   float b = data->ratio;  // cutoff frequency

   while(inp != inp_max)
   {
      add_to(inp, re->buffer + 0, cc_kernel(re->distance, b));
      add_to(inp, re->buffer + 1, cc_kernel(re->distance - ratio, b));
      add_to(inp, re->buffer + 2, cc_kernel(re->distance - ratio - ratio, b));

      re->distance++;
      inp++;

      if (re->distance > (ratio + 0.5))
      {
         *outp=re->buffer[0];

         re->buffer[0] = re->buffer[1];
         re->buffer[1] = re->buffer[2];

         re->buffer[2].l = 0.0;
         re->buffer[2].r = 0.0;

         re->distance-=ratio;
         outp++;
      }
   }

   data->output_frames = (outp - (audio_frame_float_t*)data->data_out);
}

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

static void resampler_CC_upsample(void *re_, struct resampler_data *data)
{

   rarch_CC_resampler_t *re = (rarch_CC_resampler_t*)re_;

   audio_frame_float_t *inp = (audio_frame_float_t*)data->data_in;
   audio_frame_float_t *inp_max = inp + data->input_frames;
   audio_frame_float_t *outp = (audio_frame_float_t*)data->data_out;

   float b = min(data->ratio, 1.00);  // cutoff frequency
   float ratio = 1.0 / data->ratio;

   while(inp != inp_max)
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

         for (i=0; i!=4; i++)
         {
            temp = cc_kernel(re->distance + 1.0 - i, b);
            outp->l += re->buffer[i].l * temp;
            outp->r += re->buffer[i].r * temp;
         }

         re->distance += ratio;
         outp++;
      }

      re->distance-= 1.0;
      inp++;
   }

   data->output_frames = (outp - (audio_frame_float_t*)data->data_out);

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

   for (i=0; i!=4 ; i++)
   {
      re->buffer[i].l=0.0;
      re->buffer[i].r=0.0;
   }

   RARCH_LOG("Convoluted Cosine resampler (C) : ");

   if (bandwidth_mod < 0.75)  // variations of data->ratio around 0.75 are safer than around 1.0 for both up/downsampler.
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
