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
#include <stdlib.h>
#include <string.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation twoxsai_get_implementation
#define softfilter_thread_data twoxsai_softfilter_thread_data
#define filter_data twoxsai_filter_data
#endif

#define TWOXSAI_SCALE 2

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

static unsigned twoxsai_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned twoxsai_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned twoxsai_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *twoxsai_generic_create(const struct softfilter_config *config,
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

static void twoxsai_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * TWOXSAI_SCALE;
   *out_height = height * TWOXSAI_SCALE;
}

static void twoxsai_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   free(filt->workers);
   free(filt);
}

#define twoxsai_interpolate_xrgb8888(A, B) ((((A) & 0xFEFEFEFE) >> 1) + (((B) & 0xFEFEFEFE) >> 1) + ((A) & (B) & 0x01010101))

#define twoxsai_interpolate2_xrgb8888(A, B, C, D) ((((A) & 0xFCFCFCFC) >> 2) + (((B) & 0xFCFCFCFC) >> 2) + (((C) & 0xFCFCFCFC) >> 2) + (((D) & 0xFCFCFCFC) >> 2) + (((((A) & 0x03030303) + ((B) & 0x03030303) + ((C) & 0x03030303) + ((D) & 0x03030303)) >> 2) & 0x03030303))

#define twoxsai_interpolate_rgb565(A, B) ((((A) & 0xF7DE) >> 1) + (((B) & 0xF7DE) >> 1) + ((A) & (B) & 0x0821))

#define twoxsai_interpolate2_rgb565(A, B, C, D) ((((A) & 0xE79C) >> 2) + (((B) & 0xE79C) >> 2) + (((C) & 0xE79C) >> 2) + (((D) & 0xE79C) >> 2)  + (((((A) & 0x1863) + ((B) & 0x1863) + ((C) & 0x1863) + ((D) & 0x1863)) >> 2) & 0x1863))

#define twoxsai_interpolate_4444(A, B) (((A & 0xEEEE) >> 1) + ((B & 0xEEEE) >> 1) + (A & B & 0x1111))
#define twoxsai_interpolate2_4444(A, B, C, D) (((A & 0xCCCC) >> 2) + ((B & 0xCCCC) >> 2) + ((C & 0xCCCC) >> 2) + ((D & 0xCCCC) >> 2) + ((((A & 0x3333) + (B & 0x3333) + (C & 0x3333) + (D & 0x3333)) >> 2) & 0x3333))

#define twoxsai_result(A, B, C, D) (((A) != (C) || (A) != (D)) - ((B) != (C) || (B) != (D)));

#define twoxsai_declare_variables(typename_t, in, nextline) \
         typename_t product, product1, product2; \
         typename_t colorI = *(in - nextline - 1); \
         typename_t colorE = *(in - nextline + 0); \
         typename_t colorF = *(in - nextline + 1); \
         typename_t colorJ = *(in - nextline + 2); \
         typename_t colorG = *(in - 1); \
         typename_t colorA = *(in + 0); \
         typename_t colorB = *(in + 1); \
         typename_t colorK = *(in + 2); \
         typename_t colorH = *(in + nextline - 1); \
         typename_t colorC = *(in + nextline + 0); \
         typename_t colorD = *(in + nextline + 1); \
         typename_t colorL = *(in + nextline + 2); \
         typename_t colorM = *(in + nextline + nextline - 1); \
         typename_t colorN = *(in + nextline + nextline + 0); \
         typename_t colorO = *(in + nextline + nextline + 1);

#ifndef twoxsai_function
#define twoxsai_function(result_cb, interpolate_cb, interpolate2_cb) \
         if (colorA == colorD && colorB != colorC) \
         { \
            if ((colorA == colorE && colorB == colorL) || (colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ)) \
               product = colorA; \
            else \
            { \
               product = interpolate_cb(colorA, colorB); \
            } \
            if ((colorA == colorG && colorC == colorO) || (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM)) \
               product1 = colorA; \
            else \
            { \
               product1 = interpolate_cb(colorA, colorC); \
            } \
            product2 = colorA; \
         } else if (colorB == colorC && colorA != colorD) \
         { \
            if ((colorB == colorF && colorA == colorH) || (colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI)) \
               product = colorB; \
            else \
            { \
               product = interpolate_cb(colorA, colorB); \
            } \
            if ((colorC == colorH && colorA == colorF) || (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI)) \
               product1 = colorC; \
            else \
            { \
               product1 = interpolate_cb(colorA, colorC); \
            } \
            product2 = colorB; \
         } \
         else if (colorA == colorD && colorB == colorC) \
         { \
            if (colorA == colorB) \
            { \
               product  = colorA; \
               product1 = colorA; \
               product2 = colorA; \
            } \
            else \
            { \
               int r = 0; \
               product1 = interpolate_cb(colorA, colorC); \
               product  = interpolate_cb(colorA, colorB); \
               r += result_cb(colorA, colorB, colorG, colorE); \
               r += result_cb(colorB, colorA, colorK, colorF); \
               r += result_cb(colorB, colorA, colorH, colorN); \
               r += result_cb(colorA, colorB, colorL, colorO); \
               if (r > 0) \
                  product2 = colorA; \
               else if (r < 0) \
                  product2 = colorB; \
               else \
               { \
                  product2 = interpolate2_cb(colorA, colorB, colorC, colorD); \
               } \
            } \
         } \
         else \
         { \
            product2 = interpolate2_cb(colorA, colorB, colorC, colorD); \
            if (colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ) \
               product = colorA; \
            else if (colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI) \
               product = colorB; \
            else \
            { \
               product = interpolate_cb(colorA, colorB); \
            } \
            if (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM) \
               product1 = colorA; \
            else if (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI) \
               product1 = colorC; \
            else \
            { \
               product1 = interpolate_cb(colorA, colorC); \
            } \
         } \
         out[0] = colorA; \
         out[1] = product; \
         out[dst_stride] = product1; \
         out[dst_stride + 1] = product2; \
         ++in; \
         out += 2
#endif

static void twoxsai_generic_xrgb8888(unsigned width, unsigned height,
      int first, int last, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned finish;
   unsigned nextline = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint32_t *in  = (uint32_t*)src;
      uint32_t *out = (uint32_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         twoxsai_declare_variables(uint32_t, in, nextline);

         /*
          * Map of the pixels:           I|E F|J
          *                              G|A B|K
          *                              H|C D|L
          *                              M|N O|P
          */

         twoxsai_function(twoxsai_result, twoxsai_interpolate_xrgb8888,
               twoxsai_interpolate2_xrgb8888);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxsai_generic_rgb565(unsigned width, unsigned height,
      int first, int last, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned finish;
   unsigned nextline = (last) ? 0 : src_stride;

   for (; height; height--)
   {
      uint16_t *in  = (uint16_t*)src;
      uint16_t *out = (uint16_t*)dst;

      for (finish = width; finish; finish -= 1)
      {
         twoxsai_declare_variables(uint16_t, in, nextline);

         /*
          * Map of the pixels:           I|E F|J
          *                              G|A B|K
          *                              H|C D|L
          *                              M|N O|P
          */

         twoxsai_function(twoxsai_result, twoxsai_interpolate_rgb565,
               twoxsai_interpolate2_rgb565);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxsai_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   twoxsai_generic_rgb565(width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
}

static void twoxsai_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint32_t *input = (uint32_t*)thr->in_data;
   uint32_t *output = (uint32_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   twoxsai_generic_xrgb8888(width, height,
         thr->first, thr->last, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
}

static void twoxsai_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width,
      unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)data;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start *
         TWOXSAI_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      /* Workers need to know if they can access pixels
       * outside their given buffer.
       */
      thr->first = y_start;
      thr->last = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = twoxsai_work_cb_rgb565;
#if 0
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB4444)
         packets[i].work = twoxsai_work_cb_rgb4444;
#endif
      else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = twoxsai_work_cb_xrgb8888;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation twoxsai_generic = {
   twoxsai_generic_input_fmts,
   twoxsai_generic_output_fmts,

   twoxsai_generic_create,
   twoxsai_generic_destroy,

   twoxsai_generic_threads,
   twoxsai_generic_output,
   twoxsai_generic_packets,
   SOFTFILTER_API_VERSION,
   "2xSaI",
   "2xsai",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   (void)simd;
   return &twoxsai_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
