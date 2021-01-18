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

/* Compile: gcc -o scale2x.so -shared scale2x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation scale2x_get_implementation
#define softfilter_thread_data scale2x_softfilter_thread_data
#define filter_data scale2x_filter_data
#endif

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

static unsigned scale2x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned scale2x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned scale2x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *scale2x_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   (void)config;
   (void)userdata;

   if (!filt) {
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data));
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   if (!filt->workers) {
      free(filt);
      return NULL;
   }
   return filt;
}

static void scale2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width << 1;
   *out_height = height << 1;
}

static void scale2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void scale2x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output0                  = (uint32_t*)thr->out_data;
   uint32_t *output1                  = (uint32_t*)thr->out_data + out_stride;
   uint16_t x, y;

   for (y = 0; y < thr->height; y++)
   {
      /* Determine offsets of previous/next source lines */
      uint32_t line_prev = (y == 0)               ? 0 : in_stride;
      uint32_t line_next = (y == thr->height - 1) ? 0 : in_stride;

      for (x = 0; x < thr->width; x++)
      {
         /* Get sample points */
         uint32_t A = *(input - line_prev);
         uint32_t B = (x > 0) ? *(input - 1) : *input;
         uint32_t C = *input;
         uint32_t D = (x < thr->width - 1) ? *(input + 1) : *input;
         uint32_t E = *(input++ + line_next);

         /* Apply pixel expansion algorithm */
         if (A != E && B != D)
         {
            *output0++ = (A == B ? A : C);
            *output0++ = (A == D ? A : C);
            *output1++ = (E == B ? E : C);
            *output1++ = (E == D ? E : C);
         }
         else
         {
            *output0++ = C;
            *output0++ = C;
            *output1++ = C;
            *output1++ = C;
         }
      }

      input   += in_stride - thr->width;
      output0 += (out_stride << 1) - (thr->width << 1);
      output1 += (out_stride << 1) - (thr->width << 1);
   }
}

static void scale2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 1);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 1);
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output0                  = (uint16_t*)thr->out_data;
   uint16_t *output1                  = (uint16_t*)thr->out_data + out_stride;
   uint16_t x, y;

   for (y = 0; y < thr->height; y++)
   {
      /* Determine offsets of previous/next source lines */
      uint32_t line_prev = (y == 0)               ? 0 : in_stride;
      uint32_t line_next = (y == thr->height - 1) ? 0 : in_stride;

      for (x = 0; x < thr->width; x++)
      {
         /* Get sample points */
         uint16_t A = *(input - line_prev);
         uint16_t B = (x > 0) ? *(input - 1) : *input;
         uint16_t C = *input;
         uint16_t D = (x < thr->width - 1) ? *(input + 1) : *input;
         uint16_t E = *(input++ + line_next);

         /* Apply pixel expansion algorithm */
         if (A != E && B != D)
         {
            *output0++ = (A == B ? A : C);
            *output0++ = (A == D ? A : C);
            *output1++ = (E == B ? E : C);
            *output1++ = (E == D ? E : C);
         }
         else
         {
            *output0++ = C;
            *output0++ = C;
            *output1++ = C;
            *output1++ = C;
         }
      }

      input   += in_stride - thr->width;
      output0 += (out_stride << 1) - (thr->width << 1);
      output1 += (out_stride << 1) - (thr->width << 1);
   }
}

static void scale2x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code. This only
    * makes the tiniest performance difference, but
    * every little helps when running on an o3DS... */
   struct filter_data *filt = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data = (uint8_t*)output;
   thr->in_data = (const uint8_t*)input;
   thr->out_pitch = output_stride;
   thr->in_pitch = input_stride;
   thr->width = width;
   thr->height = height;

   if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888) {
      packets[0].work = scale2x_work_cb_xrgb8888;
   } else if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = scale2x_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation scale2x_generic = {
   scale2x_generic_input_fmts,
   scale2x_generic_output_fmts,

   scale2x_generic_create,
   scale2x_generic_destroy,

   scale2x_generic_threads,
   scale2x_generic_output,
   scale2x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Scale2x",
   "scale2x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &scale2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
