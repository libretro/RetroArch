/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "softfilter.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation epx_get_implementation
#define softfilter_thread_data epx_softfilter_thread_data
#define filter_data epx_filter_data
#endif

#define EPX_SCALE 2

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
};

static unsigned epx_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned epx_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned epx_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *epx_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   (void)config;
   (void)userdata;
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)
      calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}

static void epx_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * EPX_SCALE;
   *out_height = height * EPX_SCALE;
}

static void epx_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   free(filt->workers);
   free(filt);
}

static void epx_generic_rgb565 (unsigned width, unsigned height,
      int first, int lsat, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   uint16_t colorX, colorA, colorB, colorC, colorD;
   uint16_t *sP, *uP, *lP;
   uint32_t*dP1, *dP2;
   int w;

   for (; height; height--)
   {
      sP  = (uint16_t *) src;
      uP  = (uint16_t *) (src - src_stride);
      lP  = (uint16_t *) (src + src_stride);
      dP1 = (uint32_t *) dst;
      dP2 = (uint32_t *) (dst + dst_stride);

      /* left edge */

      colorX = *sP;
      colorC = *++sP;
      colorB = *lP++;
      colorD = *uP++;

      if ((colorX != colorC) && (colorB != colorD))
      {
         #ifdef MSB_FIRST
         *dP1 = (colorX << 16) + ((colorC == colorD) ? colorC : colorX);
         *dP2 = (colorX << 16) + ((colorB == colorC) ? colorB : colorX);
         #else
         *dP1 = colorX + (((colorC == colorD) ? colorC : colorX) << 16);
         *dP2 = colorX + (((colorB == colorC) ? colorB : colorX) << 16);
         #endif
      }
      else
         *dP1 = *dP2 = (colorX << 16) + colorX;

      dP1++;
      dP2++;

      for (w = width - 2; w; w--)
      {
         colorA = colorX;
         colorX = colorC;
         colorC = *++sP;
         colorB = *lP++;
         colorD = *uP++;

         if ((colorA != colorC) && (colorB != colorD))
         {
#ifdef MSB_FIRST
           *dP1 = (((colorD == colorA) ? colorD : colorX) << 16) + ((colorC == colorD) ? colorC : colorX);
           *dP2 = (((colorA == colorB) ? colorA : colorX) << 16) + ((colorB == colorC) ? colorB : colorX);
#else
           *dP1 = ((colorD == colorA) ? colorD : colorX) + (((colorC == colorD) ? colorC : colorX) << 16);
           *dP2 = ((colorA == colorB) ? colorA : colorX) + (((colorB == colorC) ? colorB : colorX) << 16);
#endif
         }
         else
            *dP1 = *dP2 = (colorX << 16) + colorX;

         dP1++;
         dP2++;
      }

      /* right edge */

      colorA = colorX;
      colorX = colorC;
      colorB = *lP;
      colorD = *uP;

      if ((colorA != colorX) && (colorB != colorD))
      {
#ifdef MSB_FIRST
         *dP1 = (((colorD == colorA) ? colorD : colorX) << 16) + colorX;
         *dP2 = (((colorA == colorB) ? colorA : colorX) << 16) + colorX;
#else
         *dP1 = ((colorD == colorA) ? colorD : colorX) + (colorX << 16);
         *dP2 = ((colorA == colorB) ? colorA : colorX) + (colorX << 16);
#endif
      }
      else
         *dP1 = *dP2 = (colorX << 16) + colorX;

      src += src_stride;
      dst += dst_stride << 1;
   }
}

static void epx_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   epx_generic_rgb565(width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
}

static void epx_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)data;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * EPX_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      /* Workers need to know if they can
       * access pixels outside their given buffer. */
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = epx_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation epx_generic = {
   epx_generic_input_fmts,
   epx_generic_output_fmts,

   epx_generic_create,
   epx_generic_destroy,

   epx_generic_threads,
   epx_generic_output,
   epx_generic_packets,
   SOFTFILTER_API_VERSION,
   "EPX",
   "epx",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &epx_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
