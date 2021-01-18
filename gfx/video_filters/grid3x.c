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

/* Compile: gcc -o grid3x.so -shared grid3x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation grid3x_get_implementation
#define softfilter_thread_data grid3x_softfilter_thread_data
#define filter_data grid3x_filter_data
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

static unsigned grid3x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned grid3x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned grid3x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *grid3x_generic_create(const struct softfilter_config *config,
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

static void grid3x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width  * 3;
   *out_height = height * 3;
}

static void grid3x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void grid3x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   uint32_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint32_t *out_ptr = output;
      for (x = 0; x < thr->width; ++x)
      {
         uint32_t *out_line_ptr  = out_ptr;
         uint32_t color          = *(input + x);

         /* Scanline colour is color * 0.75
          * > First pass: 50:50 mix of color:0 */
         uint32_t scanline_color = (color + (color & 0x1010101)) >> 1;
         /* > Second pass: 50:50 mix of color:(color:0)
          *   => Gives ((1 + 0.5) / 2) = 0.75 */
         scanline_color = (color + scanline_color + ((color ^ scanline_color) & 0x1010101)) >> 1;

         /* c.f "Mixing Packed RGB Pixels Efficiently"
          * http://blargg.8bitalley.com/info/rgb_mixing.html */

         /* Row 1: <colour><colour><scanline> */
         *out_line_ptr       = color;
         *(out_line_ptr + 1) = color;
         *(out_line_ptr + 2) = scanline_color;
         out_line_ptr       += out_stride;

         /* Row 2: <colour><colour><scanline> */
         *out_line_ptr       = color;
         *(out_line_ptr + 1) = color;
         *(out_line_ptr + 2) = scanline_color;
         out_line_ptr       += out_stride;

         /* Row 3: <scanline><scanline><scanline> */
         *out_line_ptr       = scanline_color;
         *(out_line_ptr + 1) = scanline_color;
         *(out_line_ptr + 2) = scanline_color;

         out_ptr += 3;
      }

      input  += in_stride;
      output += out_stride * 3;
   }
}

static void grid3x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t x, y;

   for (y = 0; y < thr->height; ++y)
   {
      uint16_t *out_ptr = output;
      for (x = 0; x < thr->width; ++x)
      {
         uint16_t *out_line_ptr  = out_ptr;
         uint16_t color          = *(input + x);

         /* Scanline colour is color * 0.75
          * > First pass: 50:50 mix of color:0 */
         uint16_t scanline_color = (color + (color & 0x821)) >> 1;
         /* > Second pass: 50:50 mix of color:(color:0)
          *   => Gives ((1 + 0.5) / 2) = 0.75 */
         scanline_color = (color + scanline_color + ((color ^ scanline_color) & 0x821)) >> 1;

         /* c.f "Mixing Packed RGB Pixels Efficiently"
          * http://blargg.8bitalley.com/info/rgb_mixing.html */

         /* Row 1: <colour><colour><scanline> */
         *out_line_ptr       = color;
         *(out_line_ptr + 1) = color;
         *(out_line_ptr + 2) = scanline_color;
         out_line_ptr       += out_stride;

         /* Row 2: <colour><colour><scanline> */
         *out_line_ptr       = color;
         *(out_line_ptr + 1) = color;
         *(out_line_ptr + 2) = scanline_color;
         out_line_ptr       += out_stride;

         /* Row 3: <scanline><scanline><scanline> */
         *out_line_ptr       = scanline_color;
         *(out_line_ptr + 1) = scanline_color;
         *(out_line_ptr + 2) = scanline_color;

         out_ptr += 3;
      }

      input  += in_stride;
      output += out_stride * 3;
   }
}

static void grid3x_generic_packets(void *data,
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
      packets[0].work = grid3x_work_cb_xrgb8888;
   } else if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = grid3x_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation grid3x_generic = {
   grid3x_generic_input_fmts,
   grid3x_generic_output_fmts,

   grid3x_generic_create,
   grid3x_generic_destroy,

   grid3x_generic_threads,
   grid3x_generic_output,
   grid3x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Grid3x",
   "grid3x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &grid3x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
