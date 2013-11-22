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

#include "pixconv.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef SCALER_NO_SIMD
#undef __SSE2__
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(__SSE2_)
void conv_rgb565_0rgb1555(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint16_t *output = (uint16_t*)output_;

   int max_width = width - 7;

   const __m128i hi_mask   = _mm_set1_epi16(0x7fe0);
   const __m128i lo_mask   = _mm_set1_epi16(0x1f);

   for (h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 1)
   {
      for (w = 0; w < max_width; w += 8)
      {
         const __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
         __m128i hi = _mm_and_si128(_mm_slli_epi16(in, 1), hi_mask);
         __m128i lo = _mm_and_si128(in, lo_mask);
         _mm_storeu_si128((__m128i*)(output + w), _mm_or_si128(hi, lo));
      }

      for (; w < width; w++)
      {
         uint16_t col = input[w];
         uint16_t hi = (col >> 1) & 0x7fe0;
         uint16_t lo = col & 0x1f;
         output[w] = hi | lo;
      }
   }
}
#else
void conv_rgb565_0rgb1555(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint16_t *output = (uint16_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 1)
   {
      for (w = 0; w < width; w++)
      {
         uint16_t col = input[w];
         uint16_t hi = (col >> 1) & 0x7fe0;
         uint16_t lo = col & 0x1f;
         output[w] = hi | lo;
      }
   }
}

#endif

#if defined(__SSE2__)
void conv_0rgb1555_rgb565(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint16_t *output = (uint16_t*)output_;

   int max_width = width - 7;

   const __m128i hi_mask   = _mm_set1_epi16((int16_t)((0x1f << 11) | (0x1f << 6)));
   const __m128i lo_mask   = _mm_set1_epi16(0x1f);
   const __m128i glow_mask = _mm_set1_epi16(1 << 5);

   for (h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 1)
   {
      for (w = 0; w < max_width; w += 8)
      {
         const __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
         __m128i rg   = _mm_and_si128(_mm_slli_epi16(in, 1), hi_mask);
         __m128i b    = _mm_and_si128(in, lo_mask);
         __m128i glow = _mm_and_si128(_mm_srli_epi16(in, 4), glow_mask);
         _mm_storeu_si128((__m128i*)(output + w), _mm_or_si128(rg, _mm_or_si128(b, glow)));
      }

      for (; w < width; w++)
      {
         uint16_t col = input[w];
         uint16_t rg = (col << 1) & ((0x1f << 11) | (0x1f << 6));
         uint16_t b = col & 0x1f;
         uint16_t glow = (col >> 4) & (1 << 5);
         output[w] = rg | b | glow;
      }
   }
}
#else
void conv_0rgb1555_rgb565(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint16_t *output = (uint16_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 1)
   {
      for (w = 0; w < width; w++)
      {
         uint16_t col = input[w];
         uint16_t rg = (col << 1) & ((0x1f << 11) | (0x1f << 6));
         uint16_t b = col & 0x1f;
         uint16_t glow = (col >> 4) & (1 << 5);
         output[w] = rg | b | glow;
      }
   }
}
#endif

#if defined(__SSE2__)
void conv_0rgb1555_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   const __m128i pix_mask_r  = _mm_set1_epi16(0x1f << 10);
   const __m128i pix_mask_gb = _mm_set1_epi16(0x1f <<  5);
   const __m128i mul15_mid   = _mm_set1_epi16(0x4200);
   const __m128i mul15_hi    = _mm_set1_epi16(0x0210);
   const __m128i a           = _mm_set1_epi16(0x00ff);

   int max_width = width - 7;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (w = 0; w < max_width; w += 8)
      {
         const __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
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
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 10) & 0x1f;
         uint32_t g = (col >>  5) & 0x1f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 3) | (g >> 2);
         b = (b << 3) | (b >> 2);

         output[w] = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}
#endif

#if defined(__SSE2__)
void conv_rgb565_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   const __m128i pix_mask_r = _mm_set1_epi16(0x1f << 10);
   const __m128i pix_mask_g = _mm_set1_epi16(0x3f <<  5);
   const __m128i pix_mask_b = _mm_set1_epi16(0x1f <<  5);
   const __m128i mul16_r    = _mm_set1_epi16(0x0210);
   const __m128i mul16_g    = _mm_set1_epi16(0x2080);
   const __m128i mul16_b    = _mm_set1_epi16(0x4200);
   const __m128i a          = _mm_set1_epi16(0x00ff);

   int max_width = width - 7;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (w = 0; w < max_width; w += 8)
      {
         const __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
         __m128i r = _mm_and_si128(_mm_srli_epi16(in, 1), pix_mask_r);
         __m128i g = _mm_and_si128(in, pix_mask_g);
         __m128i b = _mm_and_si128(_mm_slli_epi16(in, 5), pix_mask_b);

         r = _mm_mulhi_epi16(r, mul16_r);
         g = _mm_mulhi_epi16(g, mul16_g);
         b = _mm_mulhi_epi16(b, mul16_b);

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
         uint32_t r = (col >> 11) & 0x1f;
         uint32_t g = (col >>  5) & 0x3f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 2) | (g >> 4);
         b = (b << 3) | (b >> 2);

         output[w] = (0xff << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}
#else
void conv_rgb565_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 1)
   {
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 11) & 0x1f;
         uint32_t g = (col >>  5) & 0x3f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 2) | (g >> 4);
         b = (b << 3) | (b >> 2);

         output[w] = (0xffu << 24) | (b << 16) | (g << 8) | (r << 0);
      }
   }
}
#endif

#if defined(__SSE2__)
// :( TODO: Make this saner.
static inline void store_bgr24_sse2(void *output, __m128i a, __m128i b, __m128i c, __m128i d)
{
   const __m128i mask_0 = _mm_set_epi32(0, 0, 0, 0x00ffffff);
   const __m128i mask_1 = _mm_set_epi32(0, 0, 0x00ffffff, 0);
   const __m128i mask_2 = _mm_set_epi32(0, 0x00ffffff, 0, 0);
   const __m128i mask_3 = _mm_set_epi32(0x00ffffff, 0, 0, 0);

   __m128i a0 = _mm_and_si128(a, mask_0);
   __m128i a1 = _mm_srli_si128(_mm_and_si128(a, mask_1),  1);
   __m128i a2 = _mm_srli_si128(_mm_and_si128(a, mask_2),  2);
   __m128i a3 = _mm_srli_si128(_mm_and_si128(a, mask_3),  3);
   __m128i a4 = _mm_slli_si128(_mm_and_si128(b, mask_0), 12);
   __m128i a5 = _mm_slli_si128(_mm_and_si128(b, mask_1), 11);

   __m128i b0 = _mm_srli_si128(_mm_and_si128(b, mask_1), 5);
   __m128i b1 = _mm_srli_si128(_mm_and_si128(b, mask_2), 6);
   __m128i b2 = _mm_srli_si128(_mm_and_si128(b, mask_3), 7);
   __m128i b3 = _mm_slli_si128(_mm_and_si128(c, mask_0), 8);
   __m128i b4 = _mm_slli_si128(_mm_and_si128(c, mask_1), 7);
   __m128i b5 = _mm_slli_si128(_mm_and_si128(c, mask_2), 6);

   __m128i c0 = _mm_srli_si128(_mm_and_si128(c, mask_2), 10);
   __m128i c1 = _mm_srli_si128(_mm_and_si128(c, mask_3), 11);
   __m128i c2 = _mm_slli_si128(_mm_and_si128(d, mask_0),  4);
   __m128i c3 = _mm_slli_si128(_mm_and_si128(d, mask_1),  3);
   __m128i c4 = _mm_slli_si128(_mm_and_si128(d, mask_2),  2);
   __m128i c5 = _mm_slli_si128(_mm_and_si128(d, mask_3),  1);

   __m128i *out = (__m128i*)output;

   _mm_storeu_si128(out + 0,
         _mm_or_si128(a0, _mm_or_si128(a1, _mm_or_si128(a2, _mm_or_si128(a3, _mm_or_si128(a4, a5))))));

   _mm_storeu_si128(out + 1,
         _mm_or_si128(b0, _mm_or_si128(b1, _mm_or_si128(b2, _mm_or_si128(b3, _mm_or_si128(b4, b5))))));

   _mm_storeu_si128(out + 2,
         _mm_or_si128(c0, _mm_or_si128(c1, _mm_or_si128(c2, _mm_or_si128(c3, _mm_or_si128(c4, c5))))));
}

void conv_0rgb1555_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   const __m128i pix_mask_r  = _mm_set1_epi16(0x1f << 10);
   const __m128i pix_mask_gb = _mm_set1_epi16(0x1f <<  5);
   const __m128i mul15_mid   = _mm_set1_epi16(0x4200);
   const __m128i mul15_hi    = _mm_set1_epi16(0x0210);
   const __m128i a           = _mm_set1_epi16(0x00ff);

   int max_width = width - 15;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;

      for (w = 0; w < max_width; w += 16, out += 48)
      {
         const __m128i in0 = _mm_loadu_si128((const __m128i*)(input + w + 0));
         const __m128i in1 = _mm_loadu_si128((const __m128i*)(input + w + 8));
         __m128i r0 = _mm_and_si128(in0, pix_mask_r);
         __m128i r1 = _mm_and_si128(in1, pix_mask_r);
         __m128i g0 = _mm_and_si128(in0, pix_mask_gb);
         __m128i g1 = _mm_and_si128(in1, pix_mask_gb);
         __m128i b0 = _mm_and_si128(_mm_slli_epi16(in0, 5), pix_mask_gb);
         __m128i b1 = _mm_and_si128(_mm_slli_epi16(in1, 5), pix_mask_gb);

         r0 = _mm_mulhi_epi16(r0, mul15_hi);
         r1 = _mm_mulhi_epi16(r1, mul15_hi);
         g0 = _mm_mulhi_epi16(g0, mul15_mid);
         g1 = _mm_mulhi_epi16(g1, mul15_mid);
         b0 = _mm_mulhi_epi16(b0, mul15_mid);
         b1 = _mm_mulhi_epi16(b1, mul15_mid);

         __m128i res_lo_bg0 = _mm_unpacklo_epi8(b0, g0);
         __m128i res_lo_bg1 = _mm_unpacklo_epi8(b1, g1);
         __m128i res_hi_bg0 = _mm_unpackhi_epi8(b0, g0);
         __m128i res_hi_bg1 = _mm_unpackhi_epi8(b1, g1);
         __m128i res_lo_ra0 = _mm_unpacklo_epi8(r0, a);
         __m128i res_lo_ra1 = _mm_unpacklo_epi8(r1, a);
         __m128i res_hi_ra0 = _mm_unpackhi_epi8(r0, a);
         __m128i res_hi_ra1 = _mm_unpackhi_epi8(r1, a);

         __m128i res_lo0 = _mm_or_si128(res_lo_bg0, _mm_slli_si128(res_lo_ra0, 2));
         __m128i res_lo1 = _mm_or_si128(res_lo_bg1, _mm_slli_si128(res_lo_ra1, 2));
         __m128i res_hi0 = _mm_or_si128(res_hi_bg0, _mm_slli_si128(res_hi_ra0, 2));
         __m128i res_hi1 = _mm_or_si128(res_hi_bg1, _mm_slli_si128(res_hi_ra1, 2));

         // Non-POT pixel sizes ftl :(
         store_bgr24_sse2(out, res_lo0, res_hi0, res_lo1, res_hi1);
      }

      for (; w < width; w++)
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

void conv_rgb565_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output      = (uint8_t*)output_;

   const __m128i pix_mask_r = _mm_set1_epi16(0x1f << 10);
   const __m128i pix_mask_g = _mm_set1_epi16(0x3f <<  5);
   const __m128i pix_mask_b = _mm_set1_epi16(0x1f <<  5);
   const __m128i mul16_r    = _mm_set1_epi16(0x0210);
   const __m128i mul16_g    = _mm_set1_epi16(0x2080);
   const __m128i mul16_b    = _mm_set1_epi16(0x4200);
   const __m128i a          = _mm_set1_epi16(0x00ff);

   int max_width = width - 15;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;

      for (w = 0; w < max_width; w += 16, out += 48)
      {
         const __m128i in0 = _mm_loadu_si128((const __m128i*)(input + w));
         const __m128i in1 = _mm_loadu_si128((const __m128i*)(input + w + 8));
         __m128i r0 = _mm_and_si128(_mm_srli_epi16(in0, 1), pix_mask_r);
         __m128i g0 = _mm_and_si128(in0, pix_mask_g);
         __m128i b0 = _mm_and_si128(_mm_slli_epi16(in0, 5), pix_mask_b);
         __m128i r1 = _mm_and_si128(_mm_srli_epi16(in1, 1), pix_mask_r);
         __m128i g1 = _mm_and_si128(in1, pix_mask_g);
         __m128i b1 = _mm_and_si128(_mm_slli_epi16(in1, 5), pix_mask_b);

         r0 = _mm_mulhi_epi16(r0, mul16_r);
         g0 = _mm_mulhi_epi16(g0, mul16_g);
         b0 = _mm_mulhi_epi16(b0, mul16_b);
         r1 = _mm_mulhi_epi16(r1, mul16_r);
         g1 = _mm_mulhi_epi16(g1, mul16_g);
         b1 = _mm_mulhi_epi16(b1, mul16_b);

         __m128i res_lo_bg0 = _mm_unpacklo_epi8(b0, g0);
         __m128i res_hi_bg0 = _mm_unpackhi_epi8(b0, g0);
         __m128i res_lo_ra0 = _mm_unpacklo_epi8(r0, a);
         __m128i res_hi_ra0 = _mm_unpackhi_epi8(r0, a);
         __m128i res_lo_bg1 = _mm_unpacklo_epi8(b1, g1);
         __m128i res_hi_bg1 = _mm_unpackhi_epi8(b1, g1);
         __m128i res_lo_ra1 = _mm_unpacklo_epi8(r1, a);
         __m128i res_hi_ra1 = _mm_unpackhi_epi8(r1, a);

         __m128i res_lo0 = _mm_or_si128(res_lo_bg0, _mm_slli_si128(res_lo_ra0, 2));
         __m128i res_hi0 = _mm_or_si128(res_hi_bg0, _mm_slli_si128(res_hi_ra0, 2));
         __m128i res_lo1 = _mm_or_si128(res_lo_bg1, _mm_slli_si128(res_lo_ra1, 2));
         __m128i res_hi1 = _mm_or_si128(res_hi_bg1, _mm_slli_si128(res_hi_ra1, 2));

         store_bgr24_sse2(out, res_lo0, res_hi0, res_lo1, res_hi1);
      }

      for (; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t r = (col >> 11) & 0x1f;
         uint32_t g = (col >>  5) & 0x3f;
         uint32_t b = (col >>  0) & 0x1f;
         r = (r << 3) | (r >> 2);
         g = (g << 2) | (g >> 4);
         b = (b << 3) | (b >> 2);

         *out++ = b;
         *out++ = g;
         *out++ = r;
      }
   }
}
#else
void conv_0rgb1555_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;
      for (w = 0; w < width; w++)
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

void conv_rgb565_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint16_t *input = (const uint16_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 1)
   {
      uint8_t *out = output;
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint32_t b = (col >>  0) & 0x1f;
         uint32_t g = (col >>  5) & 0x3f;
         uint32_t r = (col >> 11) & 0x1f;
         b = (b << 3) | (b >> 2);
         g = (g << 2) | (g >> 4);
         r = (r << 3) | (r >> 2);

         *out++ = r;
         *out++ = g;
         *out++ = b;
      }
   }
}
#endif

void conv_bgr24_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint8_t *input = (const uint8_t*)input_;
   uint32_t *output     = (uint32_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride)
   {
      const uint8_t *inp = input;
      for (w = 0; w < width; w++)
      {
         uint32_t b = *inp++;
         uint32_t g = *inp++;
         uint32_t r = *inp++;
         output[w] = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

void conv_argb8888_0rgb1555(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint32_t *input = (const uint32_t*)input_;
   uint16_t *output      = (uint16_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 1, input += in_stride >> 2)
   {
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         uint16_t r = (col >> 19) & 0x1f;
         uint16_t g = (col >> 11) & 0x1f;
         uint16_t b = (col >>  3) & 0x1f;
         output[w] = (r << 10) | (g << 5) | (b << 0);
      }
   }
}

#if defined(__SSE2__)
void conv_argb8888_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   int max_width = width - 15;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 2)
   {
      uint8_t *out = output;

      for (w = 0; w < max_width; w += 16, out += 48)
      {
         store_bgr24_sse2(out,
               _mm_loadu_si128((const __m128i*)(input + w +  0)),
               _mm_loadu_si128((const __m128i*)(input + w +  4)),
               _mm_loadu_si128((const __m128i*)(input + w +  8)),
               _mm_loadu_si128((const __m128i*)(input + w + 12)));
      }

      for (; w < width; w++)
      {
         uint32_t col = input[w];
         *out++ = (uint8_t)(col >>  0);
         *out++ = (uint8_t)(col >>  8);
         *out++ = (uint8_t)(col >> 16);
      }
   }
}
#else
void conv_argb8888_bgr24(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint32_t *input = (const uint32_t*)input_;
   uint8_t *output       = (uint8_t*)output_;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride >> 2)
   {
      uint8_t *out = output;
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         *out++ = (uint8_t)(col >>  0);
         *out++ = (uint8_t)(col >>  8);
         *out++ = (uint8_t)(col >> 16);
      }
   }
}
#endif

void conv_argb8888_abgr8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint32_t *input = (const uint32_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 2)
   {
      for (w = 0; w < width; w++)
      {
         uint32_t col = input[w];
         output[w] = ((col << 16) & 0xff0000) | ((col >> 16) & 0xff) | (col & 0xff00ff00);
      }
   }
}

#define YUV_SHIFT 6
#define YUV_OFFSET (1 << (YUV_SHIFT - 1))
#define YUV_MAT_Y (1 << 6)
#define YUV_MAT_U_G (-22)
#define YUV_MAT_U_B (113)
#define YUV_MAT_V_R (90)
#define YUV_MAT_V_G (-46)
#if defined(__SSE2__)
void conv_yuyv_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint8_t *input = (const uint8_t*)input_;
   uint32_t *output     = (uint32_t*)output_;

   const __m128i mask_y = _mm_set1_epi16(0xffu);
   const __m128i mask_u = _mm_set1_epi32(0xffu << 8);
   const __m128i mask_v = _mm_set1_epi32(0xffu << 24);
   const __m128i chroma_offset = _mm_set1_epi16(128);
   const __m128i round_offset = _mm_set1_epi16(YUV_OFFSET);

   const __m128i yuv_mul = _mm_set1_epi16(YUV_MAT_Y);
   const __m128i u_g_mul = _mm_set1_epi16(YUV_MAT_U_G);
   const __m128i u_b_mul = _mm_set1_epi16(YUV_MAT_U_B);
   const __m128i v_r_mul = _mm_set1_epi16(YUV_MAT_V_R);
   const __m128i v_g_mul = _mm_set1_epi16(YUV_MAT_V_G);
   const __m128i a       = _mm_cmpeq_epi16(_mm_setzero_si128(), _mm_setzero_si128());

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride)
   {
      const uint8_t *src = input;
      uint32_t *dst = output;

      // Each loop processes 16 pixels.
      for (w = 0; w + 16 <= width; w += 16, src += 32, dst += 16)
      {
         __m128i yuv0 = _mm_loadu_si128((const __m128i*)(src +  0)); // [Y0, U0, Y1, V0, Y2, U1, Y3, V1, ...]
         __m128i yuv1 = _mm_loadu_si128((const __m128i*)(src + 16)); // [Y0, U0, Y1, V0, Y2, U1, Y3, V1, ...]

         __m128i y0 = _mm_and_si128(yuv0, mask_y); // [Y0, Y1, Y2, ...] (16-bit)
         __m128i u0 = _mm_and_si128(yuv0, mask_u); // [0, U0, 0, 0, 0, U1, 0, 0, ...]
         __m128i v0 = _mm_and_si128(yuv0, mask_v); // [0, 0, 0, V1, 0, , 0, V1, ...]
         __m128i y1 = _mm_and_si128(yuv1, mask_y); // [Y0, Y1, Y2, ...] (16-bit)
         __m128i u1 = _mm_and_si128(yuv1, mask_u); // [0, U0, 0, 0, 0, U1, 0, 0, ...]
         __m128i v1 = _mm_and_si128(yuv1, mask_v); // [0, 0, 0, V1, 0, , 0, V1, ...]

         // Juggle around to get U and V in the same 16-bit format as Y.
         u0 = _mm_srli_si128(u0, 1);
         v0 = _mm_srli_si128(v0, 3);
         u1 = _mm_srli_si128(u1, 1);
         v1 = _mm_srli_si128(v1, 3);
         __m128i u = _mm_packs_epi32(u0, u1);
         __m128i v = _mm_packs_epi32(v0, v1);

         // Apply YUV offsets (U, V) -= (-128, -128)
         u = _mm_sub_epi16(u, chroma_offset);
         v = _mm_sub_epi16(v, chroma_offset);

         // Upscale chroma horizontally (nearest)
         u0 = _mm_unpacklo_epi16(u, u);
         u1 = _mm_unpackhi_epi16(u, u);
         v0 = _mm_unpacklo_epi16(v, v);
         v1 = _mm_unpackhi_epi16(v, v);

         // Apply transformations
         y0 = _mm_mullo_epi16(y0, yuv_mul);
         y1 = _mm_mullo_epi16(y1, yuv_mul);
         __m128i u0_g   = _mm_mullo_epi16(u0, u_g_mul);
         __m128i u1_g   = _mm_mullo_epi16(u1, u_g_mul);
         __m128i u0_b   = _mm_mullo_epi16(u0, u_b_mul);
         __m128i u1_b   = _mm_mullo_epi16(u1, u_b_mul);
         __m128i v0_r   = _mm_mullo_epi16(v0, v_r_mul);
         __m128i v1_r   = _mm_mullo_epi16(v1, v_r_mul);
         __m128i v0_g   = _mm_mullo_epi16(v0, v_g_mul);
         __m128i v1_g   = _mm_mullo_epi16(v1, v_g_mul);

         // Add contibutions from the transformed components.
         __m128i r0 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(y0, v0_r), round_offset), YUV_SHIFT);
         __m128i g0 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(_mm_adds_epi16(y0, v0_g), u0_g), round_offset), YUV_SHIFT);
         __m128i b0 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(y0, u0_b), round_offset), YUV_SHIFT);

         __m128i r1 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(y1, v1_r), round_offset), YUV_SHIFT);
         __m128i g1 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(_mm_adds_epi16(y1, v1_g), u1_g), round_offset), YUV_SHIFT);
         __m128i b1 = _mm_srai_epi16(_mm_adds_epi16(_mm_adds_epi16(y1, u1_b), round_offset), YUV_SHIFT);

         // Saturate into 8-bit.
         r0 = _mm_packus_epi16(r0, r1);
         g0 = _mm_packus_epi16(g0, g1);
         b0 = _mm_packus_epi16(b0, b1);

         // Interleave into ARGB.
         __m128i res_lo_bg = _mm_unpacklo_epi8(b0, g0);
         __m128i res_hi_bg = _mm_unpackhi_epi8(b0, g0);
         __m128i res_lo_ra = _mm_unpacklo_epi8(r0, a);
         __m128i res_hi_ra = _mm_unpackhi_epi8(r0, a);
         __m128i res0 = _mm_unpacklo_epi16(res_lo_bg, res_lo_ra);
         __m128i res1 = _mm_unpackhi_epi16(res_lo_bg, res_lo_ra);
         __m128i res2 = _mm_unpacklo_epi16(res_hi_bg, res_hi_ra);
         __m128i res3 = _mm_unpackhi_epi16(res_hi_bg, res_hi_ra);

         _mm_storeu_si128((__m128i*)(dst +  0), res0);
         _mm_storeu_si128((__m128i*)(dst +  4), res1);
         _mm_storeu_si128((__m128i*)(dst +  8), res2);
         _mm_storeu_si128((__m128i*)(dst + 12), res3);
      }

      // Finish off the rest (if any) in C.
      for (; w < width; w += 2, src += 4, dst += 2)
      {
         int y0 = src[0];
         int  u = src[1] - 128;
         int y1 = src[2];
         int  v = src[3] - 128;

         uint8_t r0 = clamp_8bit((YUV_MAT_Y * y0 +                   YUV_MAT_V_R * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t g0 = clamp_8bit((YUV_MAT_Y * y0 + YUV_MAT_U_G * u + YUV_MAT_V_G * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t b0 = clamp_8bit((YUV_MAT_Y * y0 + YUV_MAT_U_B * u                   + YUV_OFFSET) >> YUV_SHIFT);

         uint8_t r1 = clamp_8bit((YUV_MAT_Y * y1 +                   YUV_MAT_V_R * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t g1 = clamp_8bit((YUV_MAT_Y * y1 + YUV_MAT_U_G * u + YUV_MAT_V_G * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t b1 = clamp_8bit((YUV_MAT_Y * y1 + YUV_MAT_U_B * u                   + YUV_OFFSET) >> YUV_SHIFT);

         dst[0] = 0xff000000u | (r0 << 16) | (g0 << 8) | (b0 << 0);
         dst[1] = 0xff000000u | (r1 << 16) | (g1 << 8) | (b1 << 0);
      }
   }
}
#else
void conv_yuyv_argb8888(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, w;
   const uint8_t *input = (const uint8_t*)input_;
   uint32_t *output     = (uint32_t*)output_;

   for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride)
   {
      const uint8_t *src = input;
      uint32_t *dst = output;

      for (w = 0; w < width; w += 2, src += 4, dst += 2)
      {
         int y0 = src[0];
         int  u = src[1] - 128;
         int y1 = src[2];
         int  v = src[3] - 128;

         uint8_t r0 = clamp_8bit((YUV_MAT_Y * y0 +                   YUV_MAT_V_R * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t g0 = clamp_8bit((YUV_MAT_Y * y0 + YUV_MAT_U_G * u + YUV_MAT_V_G * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t b0 = clamp_8bit((YUV_MAT_Y * y0 + YUV_MAT_U_B * u                   + YUV_OFFSET) >> YUV_SHIFT);

         uint8_t r1 = clamp_8bit((YUV_MAT_Y * y1 +                   YUV_MAT_V_R * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t g1 = clamp_8bit((YUV_MAT_Y * y1 + YUV_MAT_U_G * u + YUV_MAT_V_G * v + YUV_OFFSET) >> YUV_SHIFT);
         uint8_t b1 = clamp_8bit((YUV_MAT_Y * y1 + YUV_MAT_U_B * u                   + YUV_OFFSET) >> YUV_SHIFT);

         dst[0] = 0xff000000u | (r0 << 16) | (g0 << 8) | (b0 << 0);
         dst[1] = 0xff000000u | (r1 << 16) | (g1 << 8) | (b1 << 0);
      }
   }
}
#endif

void conv_copy(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h;
   int copy_len = abs(out_stride);
   if (abs(in_stride) < copy_len)
      copy_len = abs(in_stride);

   const uint8_t *input = (const uint8_t*)input_;
   uint8_t *output      = (uint8_t*)output_;

   for (h = 0; h < height; h++, output += out_stride, input += in_stride)
      memcpy(output, input, copy_len);
}

