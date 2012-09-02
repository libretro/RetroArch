/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "pixconv.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(__SSE2__)
void conv_0rgb1555_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   __m128i pix_mask_r  = _mm_set1_epi16(0x1f << 10);
   __m128i pix_mask_gb = _mm_set1_epi16(0x1f <<  5);
   __m128i mul15_mid   = _mm_set1_epi16(0x4200);
   __m128i mul15_hi    = _mm_set1_epi16(0x0210);
   __m128i a           = _mm_set1_epi16(0x00ff);

   int max_width = width - 7;

   for (int h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      int w;
      for (w = 0; w < max_width; w += 8)
      {
         __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
         __m128i r = _mm_and_si128(in, pix_mask_r);
         __m128i g = _mm_and_si128(in, pix_mask_gb);
         __m128i b = _mm_and_si128(_mm_slli_epi16(in, 5), pix_mask_gb);

         r = _mm_mulhi_epi16(r, mul15_hi);
         g = _mm_mulhi_epi16(g, mul15_mid);
         b = _mm_mulhi_epi16(b, mul15_mid);

         __m128i res_lo_bg = _mm_unpacklo_epi8(b, g);
         __m128i res_hi_bg = _mm_unpackhi_epi8(b, g);
         __m128i res_lo_ra = _mm_unpacklo_epi8(r, a);
         __m128i res_hi_ra = _mm_unpackhi_epi8(r, a);

         __m128i res_lo = _mm_or_si128(res_lo_bg, _mm_slli_si128(res_lo_ra, 2));
         __m128i res_hi = _mm_or_si128(res_hi_bg, _mm_slli_si128(res_hi_ra, 2));

         _mm_storeu_si128((__m128i*)(output + w + 0), res_lo);
         _mm_storeu_si128((__m128i*)(output + w + 4), res_hi);
      }

      for (; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 10) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 3) | (g >> 2);
         b = (b << 3) | (b >> 2);

         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}
#else
void conv_0rgb1555_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 10) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 3) | (g >> 2);
         b = (b << 3) | (b >> 2);

         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}
#endif

void conv_0rgb1555_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t b = (col >>  0) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t r = (col >> 10) & 0x1f;
         b = (b << 3) | (b >> 2);
         g = (g << 3) | (g >> 2);
         r = (r << 3) | (r >> 2);

         *out++ = b;
         *out++ = g;
         *out++ = r;
      }
   }
}

void conv_bgr24_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint8_t *input = (const uint8_t*)input_;
   uint32_t *output     = (uint32_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 2, input += in_stride)
   {
      const uint8_t *inp = input;
      for (int w = 0; w < width; w++)
      {
         uint32_t b = *inp++;
         uint32_t g = *inp++;
         uint32_t r = *inp++;
         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

void conv_argb8888_0rgb1555(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output      = (uint16_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 2)
   {
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint16_t r = (col >> 19) & 0x1f;
         uint16_t g = (col >> 11) & 0x1f;
         uint16_t b = (col >>  3) & 0x1f;
         output[w] = (r << 10) | (g << 5) | (b << 0);
      }
   }
}

void conv_argb8888_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride >> 2)
   {
      uint8_t *out = output;
      for (int w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         *out++ = (uint8_t)(col >>  0);
         *out++ = (uint8_t)(col >>  8);
         *out++ = (uint8_t)(col >> 16);
      }
   }
}

void conv_copy(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int copy_len = abs(out_stride);
   if (abs(in_stride) < copy_len)
      copy_len = abs(in_stride);

   const uint8_t *input = (const uint8_t*)input_;
   uint8_t *output      = (uint8_t*)output_;

   for (int h = 0; h < height; h++, output += out_stride, input += in_stride)
      memcpy(output, input, copy_len);
}

