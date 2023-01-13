/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

/* Compile: gcc -o picoscale_256x_320x240.so -shared picoscale_256x_320x240.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation picoscale_256x_320x240_get_implementation
#define softfilter_thread_data picoscale_256x_320x240_softfilter_thread_data
#define filter_data picoscale_256x_320x240_filter_data
#endif

#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define PICOSCALE_restrict __restrict
#elif defined(_MSC_VER) && _MSC_VER >= 1400
#define PICOSCALE_restrict __restrict
#else
#define PICOSCALE_restrict
#endif

typedef struct
{
   void (*upscale_256_320x192_240)(
         uint16_t *PICOSCALE_restrict di, uint16_t ds,
         const uint16_t *PICOSCALE_restrict si, uint16_t ss);
   void (*upscale_256_320x224_240)(
         uint16_t *PICOSCALE_restrict di, uint16_t ds,
         const uint16_t *PICOSCALE_restrict si, uint16_t ss);
   void (*upscale_256_320x___)(
         uint16_t *PICOSCALE_restrict di, uint16_t ds,
         const uint16_t *PICOSCALE_restrict si, uint16_t ss,
         uint16_t height);
} picoscale_functions_t;

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   picoscale_functions_t functions;
};

/*******************************************************************
 * Image scaling algorithms from picodrive standalone
 *
 * Scaler types:
 * - snn:  "smoothed" nearest neighbour (see below)
 * - bln:  n-level-bilinear with n quantized weights
 *         quantization: 0: a<1/2*n, 1/n: 1/2*n<=a<3/2*n, etc
 *         currently n=2, n=4 are implemented
 *
 * "smoothed" nearest neighbour: uses the average of the source pixels if no
 * source pixel covers more than 65% of the result pixel. It definitely
 * looks better than nearest neighbour and is still quite fast. It creates
 * a sharper look than a bilinear filter, at the price of some visible jags
 * on diagonal edges.
 *
 * Copyright (C) 2021 kub <derkub@gmail.com>
 *******************************************************************/

/* RGB565 pixel mixing, see https://www.compuphase.com/graphic/scale3.htm and
                            http://blargg.8bitalley.com/info/rgb_mixing.html */

/* 2-level mixing */
#define PICOSCALE_P_05(d,p1,p2)  d=(((p1)&(p2)) + ((((p1)^(p2))&~0x0821)>>1)) /* round up */
/* 4-level mixing, 2 times slower
 * > 1/4*p1 + 3/4*p2 = 1/2*(1/2*(p1+p2) + p2) */
#define PICOSCALE_P_025(d,p1,p2) PICOSCALE_P_05(t, p1, p2); PICOSCALE_P_05( d, t, p2)
#define PICOSCALE_P_075(d,p1,p2) PICOSCALE_P_025(d,p2,p1)

/* pixel transforms */
#define PICOSCALE_F_NOP(v) (v) /* source already in dest format (CLUT/RGB) */

/*
scalers h:
*/

/* scale 4:5 */
#define PICOSCALE_H_UPSCALE_SNN_4_5(di,ds,si,ss,w,f) do {    \
   uint16_t i;                                               \
   for (i = w/4; i > 0; i--, si += 4, di += 5) {             \
      di[0] = f(si[0]);                                      \
      di[1] = f(si[1]);                                      \
      PICOSCALE_P_05(di[2], f(si[1]),f(si[2]));              \
      di[3] = f(si[2]);                                      \
      di[4] = f(si[3]);                                      \
   }                                                         \
   di += ds - w/4*5;                                         \
   si += ss - w;                                             \
} while (0)

#define PICOSCALE_H_UPSCALE_BL2_4_5(di,ds,si,ss,w,f) do {    \
   uint16_t i;                                               \
   for (i = w/4; i > 0; i--, si += 4, di += 5) {             \
      di[0] = f(si[0]);                                      \
      PICOSCALE_P_05(di[1], f(si[0]),f(si[1]));              \
      PICOSCALE_P_05(di[2], f(si[1]),f(si[2]));              \
      di[3] = f(si[2]);                                      \
      di[4] = f(si[3]);                                      \
   }                                                         \
   di += ds - w/4*5;                                         \
   si += ss - w;                                             \
} while (0)

#define PICOSCALE_H_UPSCALE_BL4_4_5(di,ds,si,ss,w,f) do {    \
   uint16_t i, t; uint16_t p = f(si[0]);                     \
   for (i = w/4; i > 0; i--, si += 4, di += 5) {             \
      PICOSCALE_P_025(di[0], p,       f(si[0]));             \
      PICOSCALE_P_05 (di[1], f(si[0]),f(si[1]));             \
      PICOSCALE_P_05 (di[2], f(si[1]),f(si[2]));             \
      PICOSCALE_P_075(di[3], f(si[2]),f(si[3]));             \
      di[4] = p = f(si[3]);                                  \
   }                                                         \
   di += ds - w/4*5;                                         \
   si += ss - w;                                             \
} while (0)

/*
scalers v:
*/

#define PICOSCALE_V_MIX(di,li,ri,w,p_mix,f) do {    \
   uint16_t i, t, u; (void)t, (void)u;              \
   for (i = 0; i < w; i += 4) {                     \
      p_mix((di)[i  ], f((li)[i  ]),f((ri)[i  ]));  \
      p_mix((di)[i+1], f((li)[i+1]),f((ri)[i+1]));  \
      p_mix((di)[i+2], f((li)[i+2]),f((ri)[i+2]));  \
      p_mix((di)[i+3], f((li)[i+3]),f((ri)[i+3]));  \
   }                                                \
} while (0)

/* 256x___ -> 320x___, H32/mode 4, PAR 5:4, for PAL DAR 4:3 (wrong for NTSC) */
void picoscale_upscale_rgb_snn_256_320x___(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss, uint16_t height)
{
   uint16_t y;

   for (y = 0; y < height; y++)
   {
      PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
   }
}

void picoscale_upscale_rgb_bl2_256_320x___(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss, uint16_t height)
{
   uint16_t y;

   for (y = 0; y < height; y++)
   {
      PICOSCALE_H_UPSCALE_BL2_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
   }
}

void picoscale_upscale_rgb_bl4_256_320x___(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss, uint16_t height)
{
   uint16_t y;

   for (y = 0; y < height; y++)
   {
      PICOSCALE_H_UPSCALE_BL4_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
   }
}

/* 256x224 -> 320x240, H32/mode 4, PAR 5:4, for NTSC DAR 4:3 (wrong for PAL) */
void picoscale_upscale_rgb_snn_256_320x224_240(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss)
{
   uint16_t y, j;

   for (y = 0; y < 224; y += 16)
   {
      for (j = 0; j < 8; j++)
      {
         PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }
      di +=  ds;
      for (j = 0; j < 8; j++)
      {
         PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }

      /* mix lines 6-8 */
      di -= 9*ds;
      PICOSCALE_V_MIX(&di[0], &di[-ds], &di[ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      PICOSCALE_V_MIX(&di[-ds], &di[-2*ds], &di[-ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      PICOSCALE_V_MIX(&di[ ds], &di[ ds], &di[ 2*ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      di += 9*ds;
   }

   /* The above scaling produces an output image 238 pixels high
    * > Last two rows must be zeroed out */
   memset(di,      0, sizeof(uint16_t) * ds);
   memset(di + ds, 0, sizeof(uint16_t) * ds);
}

/* 256x192 -> 320x240, Fuse (ZX Spectrum) snn upscaler added by andymcca */
void picoscale_upscale_rgb_snn_256_320x192_240(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss)
{
   uint16_t y;

   for (y = 0; y < 192; y += 4)  /* From 192 lines read 4 lines at a time, write 5 lines per loop, 5x48 = 240 */
   {
      /* First two lines */
      PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      /* Blank line */
      di +=  ds;
      /* Next two lines */
      PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      PICOSCALE_H_UPSCALE_SNN_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      
      /* mix lines 2-4 */

      di -= ds*3;
      PICOSCALE_V_MIX(&di[0], &di[-ds], &di[ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      PICOSCALE_V_MIX(&di[-ds], &di[-2*ds], &di[-ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      PICOSCALE_V_MIX(&di[ ds], &di[ ds], &di[ 2*ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      di += ds*3;
    }

}


void picoscale_upscale_rgb_bl2_256_320x224_240(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss)
{
   uint16_t y, j;

   for (y = 0; y < 224; y += 16)
   {
      for (j = 0; j < 4; j++)
      {
         PICOSCALE_H_UPSCALE_BL2_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }
      di +=  ds;
      for (j = 0; j < 12; j++)
      {
         PICOSCALE_H_UPSCALE_BL2_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }
      /* mix lines 3-10 */
      di -= 13*ds;
      PICOSCALE_V_MIX(&di[0], &di[-ds], &di[ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      for (j = 0; j < 7; j++)
      {
         di += ds;
         PICOSCALE_V_MIX(&di[0], &di[0], &di[ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
      }
      di += 6*ds;
   }

   /* The above scaling produces an output image 238 pixels high
    * > Last two rows must be zeroed out */
   memset(di,      0, sizeof(uint16_t) * ds);
   memset(di + ds, 0, sizeof(uint16_t) * ds);
}

void picoscale_upscale_rgb_bl4_256_320x224_240(uint16_t *PICOSCALE_restrict di, uint16_t ds,
      const uint16_t *PICOSCALE_restrict si, uint16_t ss)
{
   uint16_t y, j;

   for (y = 0; y < 224; y += 16)
   {
      for (j = 0; j < 2; j++)
      {
         PICOSCALE_H_UPSCALE_BL4_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }
      di += ds;
      for (j = 0; j < 14; j++)
      {
         PICOSCALE_H_UPSCALE_BL4_4_5(di, ds, si, ss, 256, PICOSCALE_F_NOP);
      }
      di -= 15*ds;
      /* mixing line 2: line 1 = -ds, line 2 = +ds */
      PICOSCALE_V_MIX(&di[0], &di[-ds], &di[ds], 320, PICOSCALE_P_025, PICOSCALE_F_NOP);
      di += ds;
      /* mixing lines 3-5: line n-1 = 0, line n = +ds */
      for (j = 0; j < 3; j++)
      {
         PICOSCALE_V_MIX(&di[0], &di[0], &di[ds], 320, PICOSCALE_P_025, PICOSCALE_F_NOP);
         di += ds;
      }
      /* mixing lines 6-9 */
      for (j = 0; j < 4; j++)
      {
         PICOSCALE_V_MIX(&di[0], &di[0], &di[ds], 320, PICOSCALE_P_05, PICOSCALE_F_NOP);
         di += ds;
      }
      /* mixing lines 10-13 */
      for (j = 0; j < 4; j++)
      {
         PICOSCALE_V_MIX(&di[0], &di[0], &di[ds], 320, PICOSCALE_P_075, PICOSCALE_F_NOP);
         di += ds;
      }
      /* lines 14-16, already in place */
      di += 3*ds;
   }

   /* The above scaling produces an output image 238 pixels high
    * > Last two rows must be zeroed out */
   memset(di,      0, sizeof(uint16_t) * ds);
   memset(di + ds, 0, sizeof(uint16_t) * ds);
}

/*******************************************************************
 *******************************************************************/

static unsigned picoscale_256x_320x240_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned picoscale_256x_320x240_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned picoscale_256x_320x240_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void picoscale_256x_320x240_initialize(struct filter_data *filt,
      const struct softfilter_config *config,
      void *userdata)
{
   char *filter_type = NULL;

   /* Assign default scaling functions */
   filt->functions.upscale_256_320x192_240 = picoscale_upscale_rgb_snn_256_320x192_240;
   filt->functions.upscale_256_320x224_240 = picoscale_upscale_rgb_snn_256_320x224_240;
   filt->functions.upscale_256_320x___     = picoscale_upscale_rgb_snn_256_320x___;

   /* Read set filter type */
   if (config->get_string(userdata, "filter_type", &filter_type, "snn"))
   {
      if (!strcmp(filter_type, "bl2"))
      {
         filt->functions.upscale_256_320x224_240 = picoscale_upscale_rgb_bl2_256_320x224_240;
         filt->functions.upscale_256_320x___     = picoscale_upscale_rgb_bl2_256_320x___;
      }
      else if (!strcmp(filter_type, "bl4"))
      {
         filt->functions.upscale_256_320x224_240 = picoscale_upscale_rgb_bl4_256_320x224_240;
         filt->functions.upscale_256_320x___     = picoscale_upscale_rgb_bl4_256_320x___;
      }
   }

   if (filter_type)
      free(filter_type);
}

static void *picoscale_256x_320x240_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }

   /* Assign scaling functions */
   picoscale_256x_320x240_initialize(filt, config, userdata);

   return filt;
}

static void picoscale_256x_320x240_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if ((width == 256) &&
       ((height == 224) || (height == 240) || (height == 192) || (height == 239)))
   {
      *out_width  = 320;
      *out_height = 240;
   }
   else
   {
      *out_width  = width;
      *out_height = height;
   }
}

static void picoscale_256x_320x240_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt)
      return;
   free(filt->workers);
   free(filt);
}

static void picoscale_256x_320x240_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;

   if (width == 256)
   {
      if (height == 224)
      {
         filt->functions.upscale_256_320x224_240(output, out_stride, input, in_stride);
         return;
      }
      else if (height == 192)
      {
         filt->functions.upscale_256_320x192_240(output, out_stride, input, in_stride);
         return;
      }

      else if (height == 240)
      {
         filt->functions.upscale_256_320x___(output, out_stride, input, in_stride, 240);
         return;
      }
      else if (height == 239)
      {
         filt->functions.upscale_256_320x___(output, out_stride, input, in_stride, 239);
         /* The above scaling function produces an output
          * image 239 pixels high
          * > Last row must be zeroed out */
         memset(output + (239 * out_stride), 0, sizeof(uint16_t) * out_stride);
         return;
      }
   }

   /* Input buffer is of dimensions that cannot be upscaled
    * > Simply copy input to output */

   /* If source and destination buffers have the
    * same pitch, perform fast copy of raw pixel data */
   if (in_stride == out_stride)
      memcpy(output, input, thr->out_pitch * height);
   else
   {
      /* Otherwise copy pixel data line-by-line */
      unsigned y;
      for (y = 0; y < height; y++)
      {
         memcpy(output, input, width * sizeof(uint16_t));
         input  += in_stride;
         output += out_stride;
      }
   }
}

static void picoscale_256x_320x240_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code. This only
    * makes the tiniest performance difference, but
    * every little helps when running on an o3DS... */
   struct filter_data           *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data  = (uint8_t*)output;
   thr->in_data   = (const uint8_t*)input;
   thr->out_pitch = output_stride;
   thr->in_pitch  = input_stride;
   thr->width     = width;
   thr->height    = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work     = picoscale_256x_320x240_work_cb_rgb565;
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation picoscale_256x_320x240_generic = {
   picoscale_256x_320x240_generic_input_fmts,
   picoscale_256x_320x240_generic_output_fmts,

   picoscale_256x_320x240_generic_create,
   picoscale_256x_320x240_generic_destroy,

   picoscale_256x_320x240_generic_threads,
   picoscale_256x_320x240_generic_output,
   picoscale_256x_320x240_generic_packets,

   SOFTFILTER_API_VERSION,
   "Picoscale_256x-320x240",
   "picoscale_256x_320x240",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &picoscale_256x_320x240_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
