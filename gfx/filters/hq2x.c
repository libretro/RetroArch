/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

// Compile: gcc -o hq2x.so -shared hq2x.c -std=c99 -O3 -Wall -pedantic -fPIC

#include "softfilter.h"
#include <stdlib.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation hq2x_get_implementation
#endif

#define HQ2X_SCALE 2

static unsigned hq2x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565;
}

static unsigned hq2x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned hq2x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

static void *hq2x_generic_create(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd)
{
   (void)simd;

   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   filt->workers = (struct softfilter_thread_data*)calloc(threads, sizeof(struct softfilter_thread_data));
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
   if (!filt->workers)
   {
      free(filt);
      return NULL;
   }
   return filt;
}

static void hq2x_generic_output(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width = width * HQ2X_SCALE;
   *out_height = height * HQ2X_SCALE;
}

static void hq2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   free(filt->workers);
   free(filt);
}

#define DIFF_OFFSET ((0x440 << 21) + (0x207 << 11) + 0x407)
#define DIFF_MASK   ((0x380 << 21) + (0x1f0 << 11) + 0x3f0)

#define HQ2X_565_MASK  (0x7e0f81f)
#define HQ2X_4444_MASK (0xf0f0f0f)
#define HQ2X_565_SHIFT (16)
#define HQ2X_4444_SHIFT (16)

static uint32_t *yuvTable;
static uint8_t rotate[256];

const static uint8_t hqTable[256] = {
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
  4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 12, 12, 5,  3,  1, 12,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 17, 13, 5,  3, 16, 14,
  4, 4, 6, 18, 4, 4, 6, 18, 5,  3, 16, 12, 5,  3,  1, 14,
  4, 4, 6,  2, 4, 4, 6,  2, 5, 19, 12, 12, 5, 19, 16, 12,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 12,
  4, 4, 6,  2, 4, 4, 6,  2, 5, 19,  1, 12, 5, 19,  1, 14,
  4, 4, 6,  2, 4, 4, 6, 18, 5,  3, 16, 12, 5, 19,  1, 14,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 15, 12, 5,  3, 17, 13,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 12,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 17, 13, 5,  3, 16, 14,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 13, 5,  3,  1, 14,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3, 16, 13,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3,  1, 12,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3, 16, 12, 5,  3,  1, 14,
  4, 4, 6,  2, 4, 4, 6,  2, 5,  3,  1, 12, 5,  3,  1, 14,
};

static void initialize(void)
{
   unsigned i, n;
   static int initialized = 0;

   if (initialized)
      return;

   if (posix_memalign((void**)&yuvTable, 16, 32768 * sizeof(uint32_t)))
      return;

   for (i = 0; i < 32768; ++i)
   {
      const uint8_t R = (i >>  0) & 31;
      const uint8_t G = (i >>  5) & 31;
      const uint8_t B = (i >> 10) & 31;

      /* bgr555->bgr888 */
      const double r = (R << 3) | (R >> 2);
      const double g = (G << 3) | (G >> 2);
      const double b = (B << 3) | (B >> 2);

      /* bgr888->yuv888 */
      const double y = (r + g + b) * (0.25f * (63.5f / 48.0f));
      const double u = ((r - b) * 0.25f + 128.0f) * (7.5f / 7.0f);
      const double v = ((g * 2.0f - r - b) * 0.125f + 128.0f) * (7.5f / 6.0f);

      yuvTable[i] = ((unsigned)y << 21) + ((unsigned)u << 11) + ((unsigned)v);
   }

   for (n = 0; n < 256; ++n)
   {
      rotate[n] = ((n >> 2) & 0x11) | ((n << 2) & 0x88)
         | ((n & 0x01) << 5) | ((n & 0x08) << 3)
         | ((n & 0x10) >> 3) | ((n & 0x80) >> 5);
   }

   initialized = 1;
}

static inline uint8_t same(uint16_t x, uint16_t y)
{
   return !((yuvTable[x] - yuvTable[y] + DIFF_OFFSET) & DIFF_MASK);
}

static inline uint8_t diff(uint32_t x, uint16_t y)
{
   return ((x - yuvTable[y]) & DIFF_MASK);
}

static inline void grow(uint32_t *n, unsigned shift, uint32_t mask)
{
   *n |= (*n << shift);
   *n &= mask;
}

static inline uint16_t pack(uint32_t n, uint32_t mask, unsigned shift)
{
   n &= mask;
   return (n | (n >> shift));
}

static uint16_t blend1(uint32_t A, uint32_t B, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   A = (A * 3 + B) >> 2;
   return pack(A, mask, shift);
}

static uint16_t blend2(uint32_t A, uint32_t B, uint32_t C, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   grow(&C, shift, mask);
   return pack((A * 2 + B + C) >> 2, mask, shift);
}

static uint16_t blend3(uint32_t A, uint32_t B, uint32_t C, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   grow(&C, shift, mask);
   return pack((A * 5 + B * 2 + C) >> 3, mask, shift);
}

static uint16_t blend4(uint32_t A, uint32_t B, uint32_t C, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   grow(&C, shift, mask);
   return pack((A * 6 + B + C) >> 3, mask, shift);
}

static uint16_t blend5(uint32_t A, uint32_t B, uint32_t C, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   grow(&C, shift, mask);
   return pack((A * 2 + (B + C) * 3) >> 3, mask, shift);
}

static uint16_t blend6(uint32_t A, uint32_t B, uint32_t C, unsigned shift, uint32_t mask)
{
   grow(&A, shift, mask);
   grow(&B, shift, mask);
   grow(&C, shift, mask);
   return pack((A * 14 + B + C) >> 4, mask, shift);
}

typedef uint16_t (*blend_16bit)(unsigned rule, uint16_t E, uint16_t A,
  uint16_t B, uint16_t D, uint16_t F, uint16_t H);

static uint16_t blend(unsigned colfmt, unsigned rule, uint16_t E, uint16_t A,
  uint16_t B, uint16_t D, uint16_t F, uint16_t H)
{
   unsigned shift;
   uint32_t mask;
   switch (colfmt)
   {
      case SOFTFILTER_FMT_RGB565:
         mask = HQ2X_565_MASK;
         shift = HQ2X_565_SHIFT;
         break;
#if 0
      case SOFTFILTER_FMT_RGB4444:
         mask = HQ2X_4444_MASK;
         shift = HQ2X_4444_SHIFT;
         break;
#endif
   }

   switch (rule)
   {
      default:
      case  0:
         return E;
      case  1:
         return blend1(E, A, shift, mask);
      case  2:
         return blend1(E, D, shift, mask);
      case  3:
         return blend1(E, B, shift, mask);
      case  4:
         return blend2(E, D, B, shift, mask);
      case  5:
         return blend2(E, A, B, shift, mask);
      case  6:
         return blend2(E, A, D, shift ,mask);
      case  7:
         return blend3(E, B, D, shift, mask);
      case  8:
         return blend3(E, D, B, shift, mask);
      case  9:
         return blend4(E, D, B, shift, mask);
      case 10:
         return blend5(E, D, B, shift, mask);
      case 11:
         return blend6(E, D, B, shift, mask);
      case 12:
         return same(B, D) ? blend2(E, D, B, shift, mask) : E;
      case 13:
         return same(B, D) ? blend5(E, D, B, shift, mask) : E;
      case 14:
         return same(B, D) ? blend6(E, D, B, shift, mask) : E;
      case 15:
         return same(B, D) ? blend2(E, D, B, shift, mask) : blend1(E, A, shift, mask);
      case 16:
         return same(B, D) ? blend4(E, D, B, shift, mask) : blend1(E, A, shift, mask);
      case 17:
         return same(B, D) ? blend5(E, D, B, shift, mask) : blend1(E, A, shift, mask);
      case 18:
         return same(B, F) ? blend3(E, B, D, shift, mask) : blend1(E, D, shift, mask);
      case 19:
         return same(D, H) ? blend3(E, D, B, shift, mask) : blend1(E, B, shift, mask);
   }
}


static void hq2x_16bit_generic(unsigned width, unsigned height,
      int first, int last,
  uint16_t *src, unsigned src_stride,
  uint16_t *dst, unsigned dst_stride,
  unsigned colfmt)
{
   initialize();

   for (unsigned y = 0; y < height; ++y)
   {
      const uint16_t *in = (const uint16_t*)(src + y * src_stride);
      uint16_t *out0 = (uint16_t*)(dst + y * dst_stride * 2);
      uint16_t *out1 = (uint16_t*)(dst + y * dst_stride * 2 + dst_stride);

      const int prevline = (((y == 0) && first) ? 0 : src_stride);
      const int nextline = (((y == height - 1) && last) ? 0 : src_stride);

      in++;
      *out0++ = 0; *out0++ = 0;
      *out1++ = 0; *out1++ = 0;

      for (unsigned x = 1; x < width - 1; ++x)
      {
         const uint16_t A = *(in - prevline - 1);
         const uint16_t B = *(in - prevline + 0);
         const uint16_t C = *(in - prevline + 1);
         const uint16_t D = *(in - 1);
         const uint16_t E = *(in + 0);
         const uint16_t F = *(in + 1);
         const uint16_t G = *(in + nextline - 1);
         const uint16_t H = *(in + nextline + 0);
         const uint16_t I = *(in + nextline + 1);
         const uint32_t e = yuvTable[E] + DIFF_OFFSET;

         uint8_t pattern;
         pattern  = diff(e, A) << 0;
         pattern |= diff(e, B) << 1;
         pattern |= diff(e, C) << 2;
         pattern |= diff(e, D) << 3;
         pattern |= diff(e, F) << 4;
         pattern |= diff(e, G) << 5;
         pattern |= diff(e, H) << 6;
         pattern |= diff(e, I) << 7;

         *(out0 + 0) = blend(colfmt, hqTable[pattern], E, A, B, D, F, H);
         pattern = rotate[pattern];
         *(out0 + 1) = blend(colfmt, hqTable[pattern], E, C, F, B, H, D);
         pattern = rotate[pattern];
         *(out1 + 1) = blend(colfmt, hqTable[pattern], E, I, H, F, D, B);
         pattern = rotate[pattern];
         *(out1 + 0) = blend(colfmt, hqTable[pattern], E, G, D, H, B, F);

         in++;
         out0 += 2;
         out1 += 2;
      }

      in++;
      *out0++ = 0; *out0++ = 0;
      *out1++ = 0; *out1++ = 0;
   }
}

static void hq2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   uint16_t *input = (uint16_t*)thr->in_data;
   uint16_t *output = (uint16_t*)thr->out_data;
   unsigned width = thr->width;
   unsigned height = thr->height;

   hq2x_16bit_generic(width, height,
         thr->first, thr->last, input, thr->in_pitch / SOFTFILTER_BPP_RGB565, output, thr->out_pitch / SOFTFILTER_BPP_RGB565,
         thr->colfmt);
}

static void hq2x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   struct filter_data *filt = (struct filter_data*)data;
   unsigned i;
   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end = (height * (i + 1)) / filt->threads;
      thr->out_data = (uint8_t*)output + y_start * HQ2X_SCALE * output_stride;
      thr->in_data = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch = input_stride;
      thr->width = width;
      thr->height = y_end - y_start;

      // Workers need to know if they can access pixels outside their given buffer.
      thr->first = y_start;
      thr->last = y_end == height;
      thr->colfmt = SOFTFILTER_FMT_RGB565;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = hq2x_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation hq2x_generic = {
   hq2x_generic_input_fmts,
   hq2x_generic_output_fmts,

   hq2x_generic_create,
   hq2x_generic_destroy,

   hq2x_generic_threads,
   hq2x_generic_output,
   hq2x_generic_packets,
   "HQ2x",
   SOFTFILTER_API_VERSION,
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   (void)simd;
   return &hq2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#endif
