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

/* Compile: gcc -o upscale_1_5x.so -shared upscale_1_5x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation upscale_1_5x_get_implementation
#define softfilter_thread_data upscale_1_5x_softfilter_thread_data
#define filter_data upscale_1_5x_filter_data
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

static unsigned upscale_1_5x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned upscale_1_5x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned upscale_1_5x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *upscale_1_5x_generic_create(const struct softfilter_config *config,
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

static void upscale_1_5x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width  + (width  >> 1);
   *out_height = height + (height >> 1);
}

static void upscale_1_5x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt) {
      return;
   }
   free(filt->workers);
   free(filt);
}

static void upscale_1_5x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   uint32_t in_stride                 = (uint32_t)(thr->in_pitch >> 2);
   uint32_t out_stride                = (uint32_t)(thr->out_pitch >> 2);
   uint32_t x, y;

   uint32_t color_a;
   uint32_t color_b;
   uint32_t color_c;
   uint32_t color_d;
   uint32_t color_ab;
   uint32_t color_cd;

   /* Before:
    *    a b
    *    c d
    *
    * After (parenthesis = average):
    *    a      (a,b)      b
    *    (a,c)  (a,b,c,d)  (b,d)
    *    c      (c,d)      d
    */

   for (y = 0; y < thr->height >> 1; y++)
   {
      uint32_t *out_ptr = output;
      for (x = 0; x < thr->width >> 1; x++)
      {
         const uint32_t *in_line_ptr = input + (x << 1);
         uint32_t *out_line_ptr      = out_ptr;

         color_a      = *in_line_ptr;
         color_b      = *(in_line_ptr + 1);
         in_line_ptr += in_stride;

         color_c      = *in_line_ptr;
         color_d      = *(in_line_ptr + 1);

         color_ab     = (color_a + color_b + ((color_a ^ color_b) & 0x1010101)) >> 1;
         color_cd     = (color_c + color_d + ((color_c ^ color_d) & 0x1010101)) >> 1;

         /* Row 1 */
         *out_line_ptr       = color_a;
         *(out_line_ptr + 1) = color_ab;
         *(out_line_ptr + 2) = color_b;
         out_line_ptr       += out_stride;

         /* Row 2 */
         *out_line_ptr       = (color_a  + color_c  + ((color_a  ^ color_c)  & 0x1010101)) >> 1;
         *(out_line_ptr + 1) = (color_ab + color_cd + ((color_ab ^ color_cd) & 0x1010101)) >> 1;
         *(out_line_ptr + 2) = (color_b  + color_d  + ((color_b  ^ color_d)  & 0x1010101)) >> 1;
         out_line_ptr       += out_stride;

         /* Row 3 */
         *out_line_ptr       = color_c;
         *(out_line_ptr + 1) = color_cd;
         *(out_line_ptr + 2) = color_d;

         out_ptr += 3;
      }

      input  += in_stride << 1;
      output += out_stride * 3;
   }
}

static void upscale_1_5x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t x, y;

   uint16_t color_a;
   uint16_t color_b;
   uint16_t color_c;
   uint16_t color_d;
   uint16_t color_ab;
   uint16_t color_cd;

   /* Before:
    *    a b
    *    c d
    *
    * After (parenthesis = average):
    *    a      (a,b)      b
    *    (a,c)  (a,b,c,d)  (b,d)
    *    c      (c,d)      d
    */

   for (y = 0; y < thr->height >> 1; y++)
   {
      uint16_t *out_ptr = output;
      for (x = 0; x < thr->width >> 1; x++)
      {
         const uint16_t *in_line_ptr = input + (x << 1);
         uint16_t *out_line_ptr      = out_ptr;

         color_a      = *in_line_ptr;
         color_b      = *(in_line_ptr + 1);
         in_line_ptr += in_stride;

         color_c      = *in_line_ptr;
         color_d      = *(in_line_ptr + 1);

         color_ab     = (color_a + color_b + ((color_a ^ color_b) & 0x821)) >> 1;
         color_cd     = (color_c + color_d + ((color_c ^ color_d) & 0x821)) >> 1;

         /* Row 1 */
         *out_line_ptr       = color_a;
         *(out_line_ptr + 1) = color_ab;
         *(out_line_ptr + 2) = color_b;
         out_line_ptr       += out_stride;

         /* Row 2 */
         *out_line_ptr       = (color_a  + color_c  + ((color_a  ^ color_c)  & 0x821)) >> 1;
         *(out_line_ptr + 1) = (color_ab + color_cd + ((color_ab ^ color_cd) & 0x821)) >> 1;
         *(out_line_ptr + 2) = (color_b  + color_d  + ((color_b  ^ color_d)  & 0x821)) >> 1;
         out_line_ptr       += out_stride;

         /* Row 3 */
         *out_line_ptr       = color_c;
         *(out_line_ptr + 1) = color_cd;
         *(out_line_ptr + 2) = color_d;

         out_ptr += 3;
      }

      input  += in_stride << 1;
      output += out_stride * 3;
   }
}

static void upscale_1_5x_generic_packets(void *data,
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
      packets[0].work = upscale_1_5x_work_cb_xrgb8888;
   } else if (filt->in_fmt == SOFTFILTER_FMT_RGB565) {
      packets[0].work = upscale_1_5x_work_cb_rgb565;
   }
   packets[0].thread_data = thr;
}

static const struct softfilter_implementation upscale_1_5x_generic = {
   upscale_1_5x_generic_input_fmts,
   upscale_1_5x_generic_output_fmts,

   upscale_1_5x_generic_create,
   upscale_1_5x_generic_destroy,

   upscale_1_5x_generic_threads,
   upscale_1_5x_generic_output,
   upscale_1_5x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Upscale1.5x",
   "upscale_1_5x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &upscale_1_5x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
