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

/* Compile: gcc -o upscale_1_66x_fast.so -shared upscale_1_66x_fast.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation upscale_1_66x_fast_get_implementation
#define softfilter_thread_data upscale_1_66x_fast_softfilter_thread_data
#define filter_data upscale_1_66x_fast_filter_data
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

static unsigned upscale_1_66x_fast_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned upscale_1_66x_fast_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned upscale_1_66x_fast_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *upscale_1_66x_fast_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   if (!(filt->workers = (struct softfilter_thread_data*)calloc(1, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->threads = 1;
   filt->in_fmt  = in_fmt;
   return filt;
}

static void upscale_1_66x_fast_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
      *out_width  = ((int)(width/3))*5;
      *out_height = ((int)(height/3))*5;
}

static void upscale_1_66x_fast_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt)
      return;
   free(filt->workers);
   free(filt);
}

/*
 * Approximately bilinear scalers
 *
 * Copyright (C) 2019 hi-ban, Nebuleon <nebuleon.fumika@gmail.com>
 *
 * This function and all auxiliary functions are free software; you can
 * redistribute them and/or modify them under the terms of the GNU Lesser
 * General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * These functions are distributed in the hope that they will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight1_1(A, B)  ((((cR(A) + cR(B)) >> 1) & 0x1f) << 11 | (((cG(A) + cG(B)) >> 1) & 0x3f) << 5 | (((cB(A) + cB(B)) >> 1) & 0x1f))
#define Weight1_2(A, B)  ((((cR(A) + (cR(B) << 1)) / 3) & 0x1f) << 11 | (((cG(A) + (cG(B) << 1)) / 3) & 0x3f) << 5 | (((cB(A) + (cB(B) << 1)) / 3) & 0x1f))
#define Weight2_1(A, B)  ((((cR(B) + (cR(A) << 1)) / 3) & 0x1f) << 11 | (((cG(B) + (cG(A) << 1)) / 3) & 0x3f) << 5 | (((cB(B) + (cB(A) << 1)) / 3) & 0x1f))
#define Weight1_3(A, B)  ((((cR(A) + (cR(B) * 3)) >> 2) & 0x1f) << 11 | (((cG(A) + (cG(B) * 3)) >> 2) & 0x3f) << 5 | (((cB(A) + (cB(B) * 3)) >> 2) & 0x1f))
#define Weight3_1(A, B)  ((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight1_4(A, B)  ((((cR(A) + (cR(B) << 2)) / 5) & 0x1f) << 11 | (((cG(A) + (cG(B) << 2)) / 5) & 0x3f) << 5 | (((cB(A) + (cB(B) << 2)) / 5) & 0x1f))
#define Weight4_1(A, B)  ((((cR(B) + (cR(A) << 2)) / 5) & 0x1f) << 11 | (((cG(B) + (cG(A) << 2)) / 5) & 0x3f) << 5 | (((cB(B) + (cB(A) << 2)) / 5) & 0x1f))
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))
#define Weight1_1_1_1(A, B, C, D)  ((((cR(A) + cR(B) + cR(C) + cR(D)) >> 2) & 0x1f) << 11 | (((cG(A) + cG(B) + cG(C) + cG(D)) >> 2) & 0x3f) << 5 | (((cB(A) + cB(B) + cB(C) + cB(D)) >> 2) & 0x1f))


static void upscale_1_66x_fast_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   uint16_t in_stride                 = (uint16_t)(thr->in_pitch >> 1);
   uint16_t out_stride                = (uint16_t)(thr->out_pitch >> 1);
   uint16_t x, y;

   uint16_t _1;
   uint16_t _2;
   uint16_t _3;
   uint16_t _4;
   uint16_t _5;
   uint16_t _6;
   uint16_t _7;
   uint16_t _8;
   uint16_t _9;

   /* Before:
    *    a b c
    *    d e f
    *    g h i
    *
    * After (parenthesis = average):
    *    a        (aab)       b        (bcc)       c
    *    (aad)    (...)       (bbe)    (...)       (ccf)
    *    d        (dde)       e        (eff)       f
    *    (dgg)    (...)       (ehh)    (...)       (fii)
    *    g        (ggh)       h        (hii)       i
    */


   for (y = 0; y < thr->height / 3; y++)
   {
      uint16_t *out_ptr = output;
      for (x = 0; x < thr->width / 3; x++)
      {
         const uint16_t *in_line_ptr = input + x*3;
         uint16_t *out_line_ptr      = out_ptr;

         _1      = *in_line_ptr;
         _2      = *(in_line_ptr + 1);
         _3      = *(in_line_ptr + 2);
         in_line_ptr += in_stride;

         _4      = *in_line_ptr;
         _5      = *(in_line_ptr + 1);
         _6      = *(in_line_ptr + 2);
         in_line_ptr += in_stride;

         _7      = *in_line_ptr;
         _8      = *(in_line_ptr + 1);
         _9      = *(in_line_ptr + 2);

         /* Row 1 */
         *out_line_ptr       = _1;
         *(out_line_ptr + 1) = Weight2_1( _1,  _2);
         *(out_line_ptr + 2) = _2;
         *(out_line_ptr + 3) = Weight1_2( _2,  _3);
         *(out_line_ptr + 4) = _3;
         out_line_ptr       += out_stride;

         /* Row 2 */
         *out_line_ptr       = Weight2_1( _1,  _4);
         *(out_line_ptr + 1) = Weight2_1(Weight2_1( _1,  _2), Weight2_1( _4,  _5));
         *(out_line_ptr + 2) = Weight2_1( _2,  _5);
         *(out_line_ptr + 3) = Weight2_1(Weight1_2( _2,  _3), Weight1_2( _5,  _6));
         *(out_line_ptr + 4) = Weight2_1( _3,  _6);
         out_line_ptr       += out_stride;

         /* Row 3 */
         *out_line_ptr       = _4;
         *(out_line_ptr + 1) = Weight2_1( _4,  _5);
         *(out_line_ptr + 2) = _5;
         *(out_line_ptr + 3) = Weight1_2( _5,  _6);
         *(out_line_ptr + 4) = _6;
         out_line_ptr       += out_stride;

         /* Row 4 */
         *out_line_ptr       = Weight1_2( _4,  _7);
         *(out_line_ptr + 1) = Weight1_2(Weight2_1( _4,  _5), Weight2_1( _7,  _8));
         *(out_line_ptr + 2) = Weight1_2( _5,  _8);
         *(out_line_ptr + 3) = Weight1_2(Weight1_2( _5,  _6), Weight1_2( _8,  _9));
         *(out_line_ptr + 4) = Weight1_2( _6,  _9);
         out_line_ptr       += out_stride;

         /* Row 5 */
         *out_line_ptr       = _7;
         *(out_line_ptr + 1) = Weight2_1( _7,  _8);
         *(out_line_ptr + 2) = _8;
         *(out_line_ptr + 3) = Weight1_2( _8,  _9);
         *(out_line_ptr + 4) = _9;

         out_ptr += 5;
      }

      input  += in_stride * 3;
      output += out_stride * 5;
   }
}

static void upscale_1_66x_fast_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   /* We are guaranteed single threaded operation
    * (filt->threads = 1) so we don't need to loop
    * over threads and can cull some code. This only
    * makes the tiniest performance difference, but
    * every little helps when running on an o3DS... */
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[0];

   thr->out_data                      = (uint8_t*)output;
   thr->in_data                       = (const uint8_t*)input;
   thr->out_pitch                     = output_stride;
   thr->in_pitch                      = input_stride;
   thr->width                         = width;
   thr->height                        = height;

   if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
      packets[0].work                 = upscale_1_66x_fast_work_cb_rgb565;
   packets[0].thread_data             = thr;
}

static const struct softfilter_implementation upscale_1_66x_fast_generic = {
   upscale_1_66x_fast_generic_input_fmts,
   upscale_1_66x_fast_generic_output_fmts,

   upscale_1_66x_fast_generic_create,
   upscale_1_66x_fast_generic_destroy,

   upscale_1_66x_fast_generic_threads,
   upscale_1_66x_fast_generic_output,
   upscale_1_66x_fast_generic_packets,

   SOFTFILTER_API_VERSION,
   "Upscale1.66x_fast",
   "upscale_1_66x_fast",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   return &upscale_1_66x_fast_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif

