/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "scaler_int.h"

#ifdef SCALER_NO_SIMD
#undef __SSE2__
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#ifdef _WIN32
#include <intrin.h>
#endif
#endif

static inline uint64_t build_argb64(uint16_t a, uint16_t r, uint16_t g, uint16_t b)
{
   return ((uint64_t)a << 48) | ((uint64_t)r << 32) | ((uint64_t)g << 16) | ((uint64_t)b << 0);
}

static inline uint8_t clamp_8bit(int16_t col)
{
   if (col > 255)
      return 255;
   else if (col < 0)
      return 0;
   else
      return (uint8_t)col;
}

// ARGB8888 scaler is split in two:
//
// First, horizontal scaler is applied.
// Here, all 8-bit channels are expanded to 16-bit. Values are then shifted 7 to left to occupy 15 bits.
// The sign bit is kept empty as we have to do signed multiplication for the filter.
// A mulhi [(a * b) >> 16] is applied which loses some precision, but is very efficient for SIMD.
// It is accurate enough for 8-bit purposes.
//
// The fixed point 1.0 for filter is (1 << 14). After horizontal scale, the output is kept
// with 16-bit channels, and will now have 13 bits of precision as [(a * (1 << 14)) >> 16] is effectively a right shift by 2.
//
// Vertical scaler takes the 13 bit channels, and performs the same mulhi steps.
// Another 2 bits of precision is lost, which ends up as 11 bits.
// Scaling is now complete. Channels are shifted right by 3, and saturated into 8-bit values.
//
// The C version of scalers perform the exact same operations as the SIMD code for testing purposes.

#if defined(__SSE2__)
void scaler_argb8888_vert(const struct scaler_ctx *ctx, void *output_, int stride)
{
   int h, w, y;
   const uint64_t *input = ctx->scaled.frame;
   uint32_t *output = (uint32_t*)output_;

   const int16_t *filter_vert = ctx->vert.filter;

   for (h = 0; h < ctx->out_height; h++, filter_vert += ctx->vert.filter_stride, output += stride >> 2)
   {
      const uint64_t *input_base = input + ctx->vert.filter_pos[h] * (ctx->scaled.stride >> 3);

      for (w = 0; w < ctx->out_width; w++)
      {
         __m128i res = _mm_setzero_si128();

         const uint64_t *input_base_y = input_base + w;

         for (y = 0; (y + 1) < ctx->vert.filter_len; y += 2, input_base_y += (ctx->scaled.stride >> 2))
         {
            __m128i coeff = _mm_set_epi64x(filter_vert[y + 1] * 0x0001000100010001ll, filter_vert[y + 0] * 0x0001000100010001ll);
            __m128i col   = _mm_set_epi64x(input_base_y[ctx->scaled.stride >> 3], input_base_y[0]);

            res = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         for (; y < ctx->vert.filter_len; y++, input_base_y += (ctx->scaled.stride >> 3))
         {
            __m128i coeff = _mm_set_epi64x(0, filter_vert[y] * 0x0001000100010001ll);
            __m128i col   = _mm_set_epi64x(0, input_base_y[0]);

            res = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         res = _mm_adds_epi16(_mm_srli_si128(res, 8), res);
         res = _mm_srai_epi16(res, (7 - 2 - 2));

         __m128i final = _mm_packus_epi16(res, res);

         output[w] = _mm_cvtsi128_si32(final);
      }
   }
}
#else
void scaler_argb8888_vert(const struct scaler_ctx *ctx, void *output_, int stride)
{
   int h, w, y;
   const uint64_t *input = ctx->scaled.frame;
   uint32_t *output = (uint32_t*)output_;

   const int16_t *filter_vert = ctx->vert.filter;

   for (h = 0; h < ctx->out_height; h++, filter_vert += ctx->vert.filter_stride, output += stride >> 2)
   {
      const uint64_t *input_base = input + ctx->vert.filter_pos[h] * (ctx->scaled.stride >> 3);

      for (w = 0; w < ctx->out_width; w++)
      {
         int16_t res_a = 0;
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         const uint64_t *input_base_y = input_base + w;
         for (y = 0; y < ctx->vert.filter_len; y++, input_base_y += (ctx->scaled.stride >> 3))
         {
            uint64_t col = *input_base_y;

            int16_t a = (col >> 48) & 0xffff;
            int16_t r = (col >> 32) & 0xffff;
            int16_t g = (col >> 16) & 0xffff;
            int16_t b = (col >>  0) & 0xffff;

            int16_t coeff = filter_vert[y];

            res_a += (a * coeff) >> 16;
            res_r += (r * coeff) >> 16;
            res_g += (g * coeff) >> 16;
            res_b += (b * coeff) >> 16;
         }

         res_a >>= (7 - 2 - 2);
         res_r >>= (7 - 2 - 2);
         res_g >>= (7 - 2 - 2);
         res_b >>= (7 - 2 - 2);

         output[w] = (clamp_8bit(res_a) << 24) | (clamp_8bit(res_r) << 16) | (clamp_8bit(res_g) << 8) | (clamp_8bit(res_b) << 0);
      }
   }
}
#endif

#if defined(__SSE2__)
void scaler_argb8888_horiz(const struct scaler_ctx *ctx, const void *input_, int stride)
{
   int h, w, x;
   const uint32_t *input = (const uint32_t*)input_;
   uint64_t *output      = ctx->scaled.frame;

   for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2, output += ctx->scaled.stride >> 3)
   {
      const int16_t *filter_horiz = ctx->horiz.filter;

      for (w = 0; w < ctx->scaled.width; w++, filter_horiz += ctx->horiz.filter_stride)
      {
         __m128i res = _mm_setzero_si128();

         const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];

         for (x = 0; (x + 1) < ctx->horiz.filter_len; x += 2)
         {
            __m128i coeff = _mm_set_epi64x(filter_horiz[x + 1] * 0x0001000100010001ll, filter_horiz[x + 0] * 0x0001000100010001ll);

            __m128i col = _mm_unpacklo_epi8(_mm_set_epi64x(0,
                     ((uint64_t)input_base_x[x + 1] << 32) | input_base_x[x + 0]), _mm_setzero_si128());

            col = _mm_slli_epi16(col, 7);
            res = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         for (; x < ctx->horiz.filter_len; x++)
         {
            __m128i coeff = _mm_set_epi64x(0, filter_horiz[x] * 0x0001000100010001ll);
            __m128i col   = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, 0, input_base_x[x]), _mm_setzero_si128());

            col = _mm_slli_epi16(col, 7);
            res = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         res       = _mm_adds_epi16(_mm_srli_si128(res, 8), res);

#ifdef __x86_64__
         output[w] = _mm_cvtsi128_si64(res);
#else // 32-bit doesn't have si64. Do it in two steps.
         union
         {
            uint32_t *u32;
            uint64_t *u64;
         } u;
         u.u64 = output + w;
         u.u32[0] = _mm_cvtsi128_si32(res);
         u.u32[1] = _mm_cvtsi128_si32(_mm_srli_si128(res, 4));
#endif
      }
   }
}
#else
void scaler_argb8888_horiz(const struct scaler_ctx *ctx, const void *input_, int stride)
{
   int h, w, x;
   const uint32_t *input = (uint32_t*)input_;
   uint64_t *output      = ctx->scaled.frame;

   for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2, output += ctx->scaled.stride >> 3)
   {
      const int16_t *filter_horiz = ctx->horiz.filter;

      for (w = 0; w < ctx->scaled.width; w++, filter_horiz += ctx->horiz.filter_stride)
      {
         const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];

         int16_t res_a = 0;
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         for (x = 0; x < ctx->horiz.filter_len; x++)
         {
            uint32_t col = input_base_x[x];

            int16_t a = (col >> (24 - 7)) & (0xff << 7);
            int16_t r = (col >> (16 - 7)) & (0xff << 7);
            int16_t g = (col >> ( 8 - 7)) & (0xff << 7);
            int16_t b = (col << ( 0 + 7)) & (0xff << 7);

            int16_t coeff = filter_horiz[x];

            res_a += (a * coeff) >> 16;
            res_r += (r * coeff) >> 16;
            res_g += (g * coeff) >> 16;
            res_b += (b * coeff) >> 16;
         }

         output[w] = build_argb64(res_a, res_r, res_g, res_b);
      }
   }
}
#endif

void scaler_argb8888_point_special(const struct scaler_ctx *ctx,
      void *output_, const void *input_,
      int out_width, int out_height,
      int in_width, int in_height,
      int out_stride, int in_stride)
{
   int h, w;
   (void)ctx;
   int x_pos  = (1 << 15) * in_width / out_width - (1 << 15);
   int x_step = (1 << 16) * in_width / out_width;
   int y_pos  = (1 << 15) * in_height / out_height - (1 << 15);
   int y_step = (1 << 16) * in_height / out_height;

   if (x_pos < 0)
      x_pos = 0;
   if (y_pos < 0)
      y_pos = 0;

   const uint32_t *input = (const uint32_t*)input_;
   uint32_t *output = (uint32_t*)output_;

   for (h = 0; h < out_height; h++, y_pos += y_step, output += out_stride >> 2)
   {
      int x = x_pos;
      const uint32_t *inp = input + (y_pos >> 16) * (in_stride >> 2);

      for (w = 0; w < out_width; w++, x += x_step)
         output[w] = inp[x >> 16];
   }
}

