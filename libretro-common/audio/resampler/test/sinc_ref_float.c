/* Faithful float reference resampler mirroring RetroArch's
 * drivers/sinc_resampler.c (Kaiser table + subphase delta interpolation, and
 * Lanczos without interpolation).  Used only by the test harness to diff the
 * integer driver against the float driver.  Not for shipping. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sinc_ref_float.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { REF_WIN_LANCZOS = 0, REF_WIN_KAISER };

typedef struct
{
   float   *phase_table;
   float   *buffer_l;
   float   *buffer_r;
   unsigned phase_bits;
   unsigned subphase_bits;
   unsigned subphase_mask;
   unsigned taps;
   unsigned window;
   unsigned ptr;
   uint32_t time;
   float    subphase_mod;
   double   kaiser_beta;
} ref_sinc_t;

static double ref_sinc(double v)
{
   if (fabs(v) < 0.00000000001)
      return 1.0;
   return sin(v) / v;
}

static double ref_besseli0(double x)
{
   double sum = 1.0, term = 1.0, hs = (x * 0.5) * (x * 0.5);
   int k = 1;
   for (;;)
   {
      term *= hs / ((double)k * (double)k);
      sum  += term;
      if (term < 1.0e-21 * sum)
         break;
      k++;
   }
   return sum;
}

static void ref_table_kaiser(ref_sinc_t *re, double cutoff,
      float *t, int phases, int taps)
{
   int i, j, p;
   double beta = re->kaiser_beta;
   double wm   = ref_besseli0(beta);
   double sl   = taps / 2.0;
   int    st   = 2;
   for (i = 0; i < phases; i++)
      for (j = 0; j < taps; j++)
      {
         double sp, arg, wp;
         int n = j * phases + i;
         wp = (double)n / (phases * taps);
         wp = 2.0 * wp - 1.0;
         sp = sl * wp;
         arg = 1.0 - wp * wp;
         if (arg < 0.0) arg = 0.0;
         t[i * st * taps + j] = (float)(cutoff * ref_sinc(M_PI * sp * cutoff) *
               ref_besseli0(beta * sqrt(arg)) / wm);
      }
   for (p = 0; p < phases - 1; p++)
      for (j = 0; j < taps; j++)
         t[(p * st + 1) * taps + j] =
            t[(p + 1) * st * taps + j] - t[p * st * taps + j];
   for (j = 0; j < taps; j++)
   {
      double sp, arg, wp; float v;
      int n = j * phases + phases;
      wp = (double)n / (phases * taps);
      wp = 2.0 * wp - 1.0;
      sp = sl * wp;
      arg = 1.0 - wp * wp;
      if (arg < 0.0) arg = 0.0;
      v = (float)(cutoff * ref_sinc(M_PI * sp * cutoff) *
            ref_besseli0(beta * sqrt(arg)) / wm);
      t[((phases - 1) * st + 1) * taps + j] =
         v - t[(phases - 1) * st * taps + j];
   }
}

static void ref_table_lanczos(ref_sinc_t *re, double cutoff,
      float *t, int phases, int taps)
{
   int i, j;
   double sl = taps / 2.0;
   (void)re;
   for (i = 0; i < phases; i++)
      for (j = 0; j < taps; j++)
      {
         double sp, wp;
         int n = j * phases + i;
         wp = (double)n / (phases * taps);
         wp = 2.0 * wp - 1.0;
         sp = sl * wp;
         t[i * taps + j] = (float)(cutoff * ref_sinc(M_PI * sp * cutoff) *
               ref_sinc(M_PI * wp));
      }
}

#define REF_PUSH(re, in, taps, phases, frames) \
   do { \
      while ((frames) && (re)->time >= (phases)) { \
         if (!(re)->ptr) (re)->ptr = (taps); \
         (re)->ptr--; \
         (re)->buffer_l[(re)->ptr + (taps)] = (re)->buffer_l[(re)->ptr] = (in)[0]; \
         (re)->buffer_r[(re)->ptr + (taps)] = (re)->buffer_r[(re)->ptr] = (in)[1]; \
         (in) += 2; (re)->time -= (phases); (frames)--; \
      } \
   } while (0)

void sinc_ref_float_process(void *re_, struct resampler_data_float *data)
{
   ref_sinc_t *re    = (ref_sinc_t*)re_;
   unsigned phases   = 1u << (re->phase_bits + re->subphase_bits);
   uint32_t ratio    = (uint32_t)(phases / data->ratio);
   const float *in   = data->data_in;
   float *output     = data->data_out;
   size_t frames     = data->input_frames;
   size_t out_frames = 0;
   unsigned taps     = re->taps;
   unsigned taps2    = taps * 2;
   int kaiser        = (re->window == REF_WIN_KAISER);

   while (frames)
   {
      REF_PUSH(re, in, taps, phases, frames);
      {
         const float *bl = re->buffer_l + re->ptr;
         const float *br = re->buffer_r + re->ptr;
         while (re->time < phases)
         {
            unsigned i;
            float sl = 0.0f, sr = 0.0f;
            unsigned phase = re->time >> re->subphase_bits;
            if (kaiser)
            {
               const float *pt = re->phase_table + phase * taps2;
               const float *dt = pt + taps;
               float delta = (float)(re->time & re->subphase_mask) * re->subphase_mod;
               for (i = 0; i < taps; i++)
               {
                  float c = pt[i] + dt[i] * delta;
                  sl += bl[i] * c;
                  sr += br[i] * c;
               }
            }
            else
            {
               const float *pt = re->phase_table + phase * taps;
               for (i = 0; i < taps; i++)
               {
                  sl += bl[i] * pt[i];
                  sr += br[i] * pt[i];
               }
            }
            output[0] = sl;
            output[1] = sr;
            output += 2;
            out_frames++;
            re->time += ratio;
         }
      }
   }
   data->output_frames = out_frames;
}

void sinc_ref_float_free(void *re_)
{
   ref_sinc_t *re = (ref_sinc_t*)re_;
   if (re) { free(re->phase_table); free(re->buffer_l); }
   free(re);
}

void *sinc_ref_float_init(double bandwidth_mod, int quality)
{
   double cutoff = 0.0;
   unsigned sidelobes = 0;
   int window = REF_WIN_LANCZOS;
   int stride, phases;
   size_t phase_elems;
   ref_sinc_t *re = (ref_sinc_t*)calloc(1, sizeof(*re));
   if (!re) return NULL;

   /* quality: 0 LOWEST,1 LOWER,2 NORMAL,3 HIGHER,4 HIGHEST */
   switch (quality)
   {
      case 0: cutoff=0.98;  sidelobes=2;   re->phase_bits=12; re->subphase_bits=10; window=REF_WIN_LANCZOS; break;
      case 1: cutoff=0.98;  sidelobes=4;   re->phase_bits=12; re->subphase_bits=10; window=REF_WIN_LANCZOS; break;
      case 3: cutoff=0.90;  sidelobes=32;  re->phase_bits=10; re->subphase_bits=14; window=REF_WIN_KAISER; re->kaiser_beta=10.5; break;
      case 4: cutoff=0.962; sidelobes=128; re->phase_bits=10; re->subphase_bits=14; window=REF_WIN_KAISER; re->kaiser_beta=14.5; break;
      case 2:
      default: cutoff=0.825; sidelobes=8;  re->phase_bits=8;  re->subphase_bits=16; window=REF_WIN_KAISER; re->kaiser_beta=5.5; break;
   }

   re->window        = (unsigned)window;
   re->subphase_mask = (1u << re->subphase_bits) - 1u;
   re->subphase_mod  = 1.0f / (1u << re->subphase_bits);
   re->taps          = sidelobes * 2;
   if (bandwidth_mod < 1.0)
   {
      cutoff  *= bandwidth_mod;
      re->taps = (unsigned)ceil((double)re->taps / bandwidth_mod);
   }
   re->taps = (re->taps + 3u) & ~3u;

   stride      = (window == REF_WIN_KAISER) ? 2 : 1;
   phases      = 1 << re->phase_bits;
   phase_elems = (size_t)phases * re->taps * stride;

   re->phase_table = (float*)malloc(sizeof(float) * phase_elems);
   re->buffer_l    = (float*)calloc(4 * re->taps, sizeof(float));
   if (!re->phase_table || !re->buffer_l) { sinc_ref_float_free(re); return NULL; }
   re->buffer_r    = re->buffer_l + 2 * re->taps;

   if (window == REF_WIN_KAISER)
      ref_table_kaiser(re, cutoff, re->phase_table, phases, (int)re->taps);
   else
      ref_table_lanczos(re, cutoff, re->phase_table, phases, (int)re->taps);
   return re;
}
