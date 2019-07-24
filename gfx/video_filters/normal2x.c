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

/* Compile: gcc -o normal2x.so -shared normal2x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation normal2x_get_implementation
#define softfilter_thread_data normal2x_softfilter_thread_data
#define filter_data normal2x_filter_data
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

static unsigned normal2x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned normal2x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned normal2x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *normal2x_generic_create(const struct softfilter_config *config,
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

static void normal2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width << 1;
   *out_height = height << 1;
}

static void normal2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void normal2x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input = (const uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned in_stride = (unsigned)(thr->in_pitch >> 2);
   unsigned out_stride = (unsigned)(thr->out_pitch >> 2);
   unsigned x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint32_t *out_ptr = output;
      for (x = 0; x < thr->width; ++x)
      {
         uint32_t color = *(input + x);
         uint32_t color_buf[2];

         color_buf[0] = color;
         color_buf[1] = color;

         memcpy(out_ptr,              color_buf, sizeof(color_buf));
         memcpy(out_ptr + out_stride, color_buf, sizeof(color_buf));

         out_ptr += 2;
      }

      input  += in_stride;
      output += out_stride << 1;
   }
}

static void normal2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input = (const uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned in_stride = (unsigned)(thr->in_pitch >> 1);
   unsigned out_stride = (unsigned)(thr->out_pitch >> 1);
   unsigned x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint16_t *out_ptr = output;
      for (x = 0; x < thr->width; ++x)
      {
         uint16_t color = *(input + x);
         uint16_t color_buf[2];

         color_buf[0] = color;
         color_buf[1] = color;

         memcpy(out_ptr,              color_buf, sizeof(color_buf));
         memcpy(out_ptr + out_stride, color_buf, sizeof(color_buf));

         out_ptr += 2;
      }

      input  += in_stride;
      output += out_stride << 1;
   }
}

static void normal2x_generic_packets(void *data,
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
      packets[0].work = normal2x_work_cb_xrgb8888;
   } else if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = normal2x_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation normal2x_generic = {
   normal2x_generic_input_fmts,
   normal2x_generic_output_fmts,

   normal2x_generic_create,
   normal2x_generic_destroy,

   normal2x_generic_threads,
   normal2x_generic_output,
   normal2x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Normal2x",
   "normal2x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &normal2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
