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

/* Compile: gcc -o scale2x.so -shared scale2x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation scale2x_get_implementation
#define softfilter_thread_data scale2x_softfilter_thread_data
#define filter_data scale2x_filter_data
#endif

#define SCALE2X_SCALE 2

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

#define SCALE2X_GENERIC(typename_t, width, height, first, last, src, src_stride, dst, dst_stride, out0, out1) \
   for (y = 0; y < height; ++y) \
   { \
      const int prevline = ((y == 0) && first) ? 0 : src_stride; \
      const int nextline = ((y == height - 1) && last) ? 0 : src_stride; \
      \
      for (x = 0; x < width; ++x) \
      { \
         const typename_t A = *(src - prevline); \
         const typename_t B = (x > 0) ? *(src - 1) : *src; \
         const typename_t C = *src; \
         const typename_t D = (x < width - 1) ? *(src + 1) : *src; \
         const typename_t E = *(src++ + nextline); \
         \
         if (A != E && B != D) \
         { \
            *out0++ = (A == B ? A : C); \
            *out0++ = (A == D ? A : C); \
            *out1++ = (E == B ? E : C); \
            *out1++ = (E == D ? E : C); \
         } \
         else \
         { \
            *out0++ = C; \
            *out0++ = C; \
            *out1++ = C; \
            *out1++ = C; \
         } \
      } \
      \
      src += src_stride - width; \
      out0 += dst_stride + dst_stride - (width * SCALE2X_SCALE); \
      out1 += dst_stride + dst_stride - (width * SCALE2X_SCALE); \
   }

static void scale2x_generic_rgb565(unsigned width, unsigned height,
      int first, int last,
      const uint16_t *src, unsigned src_stride,
      uint16_t *dst, unsigned dst_stride)
{
   unsigned x, y;
   uint16_t *out0, *out1;
   out0 = (uint16_t*)dst;
   out1 = (uint16_t*)(dst + dst_stride);
   SCALE2X_GENERIC(uint16_t, width, height, first, last,
         src, src_stride, dst, dst_stride, out0, out1);
}

static void scale2x_generic_xrgb8888(unsigned width, unsigned height,
      int first, int last,
      const uint32_t *src, unsigned src_stride,
      uint32_t *dst, unsigned dst_stride)
{
   unsigned x, y;
   uint32_t *out0 = (uint32_t*)dst;
   uint32_t *out1 = (uint32_t*)(dst + dst_stride);

   SCALE2X_GENERIC(uint32_t, width, height, first, last,
         src, src_stride, dst, dst_stride, out0, out1);
}

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

static void scale2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * SCALE2X_SCALE;
   *out_height = height * SCALE2X_SCALE;
}

static void scale2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   free(filt->workers);
   free(filt);
}

static void scale2x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   const uint32_t *input = (const uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   scale2x_generic_xrgb8888(width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
}

static void scale2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   const uint16_t *input = (const uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   scale2x_generic_rgb565(width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
}

static void scale2x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start *
         SCALE2X_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      /* Workers need to know if they can access pixels
       * outside their given buffer. */
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = scale2x_work_cb_xrgb8888;
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = scale2x_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
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
