/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (pixconv.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_SCALER_PIXCONV_H__
#define __LIBRETRO_SDK_SCALER_PIXCONV_H__

#include <stdint.h>

#include <retro_inline.h>
#include <clamping.h>

#include <retro_common_api.h>

/* Single-pixel expanders from 16-bit formats to 32-bit xRGB/xBGR.
 *
 * Each channel is widened by replicating its top bits into the new
 * low bits, which maps 0 to 0 and full scale to 0xff exactly.  The
 * channels are moved into their byte lanes first and replicated
 * together with one shift and one mask, so red and blue (and green
 * too for the 5:5:5 formats) cost a single operation instead of one
 * each.  The byte above the colour channels is left zero; callers
 * OR in the alpha they need.
 *
 * 4-bit channels are spread one per byte, low nibble, and multiplied
 * by 0x11, which duplicates every nibble into its byte's high half
 * without carries.
 */
static INLINE uint32_t pixconv_rgb565_to_xrgb8888(uint32_t c)
{
   uint32_t rb = ((c & 0xf800) << 8) | ((c & 0x001f) << 3);
   uint32_t g  =  (c & 0x07e0) << 5;
   rb |= (rb >> 5) & 0x070007;
   g  |= (g  >> 6) & 0x000300;
   return rb | g;
}

static INLINE uint32_t pixconv_rgb565_to_xbgr8888(uint32_t c)
{
   uint32_t br = ((c & 0x001f) << 19) | ((c & 0xf800) >> 8);
   uint32_t g  =  (c & 0x07e0) << 5;
   br |= (br >> 5) & 0x070007;
   g  |= (g  >> 6) & 0x000300;
   return br | g;
}

static INLINE uint32_t pixconv_0rgb1555_to_xrgb8888(uint32_t c)
{
   uint32_t x = ((c & 0x7c00) << 9) | ((c & 0x03e0) << 6)
              | ((c & 0x001f) << 3);
   return x | ((x >> 5) & 0x070707);
}

static INLINE uint32_t pixconv_0rgb1555_to_xbgr8888(uint32_t c)
{
   uint32_t x = ((c & 0x001f) << 19) | ((c & 0x03e0) << 6)
              | ((c & 0x7c00) >> 7);
   return x | ((x >> 5) & 0x070707);
}

/* 16-bit RGBA4444 (R in the top nibble) to ARGB8888. */
static INLINE uint32_t pixconv_rgba4444_to_argb8888(uint32_t c)
{
   uint32_t x = ((c & 0x000f) << 24) | ((c & 0xf000) <<  4)
              |  (c & 0x0f00)        | ((c & 0x00f0) >>  4);
   return x * 0x11;
}

/* 16-bit ARGB4444 (A in the top nibble) to ARGB8888. */
static INLINE uint32_t pixconv_argb4444_to_argb8888(uint32_t c)
{
   uint32_t x = ((c & 0xf000) << 12) | ((c & 0x0f00) <<  8)
              | ((c & 0x00f0) <<  4) |  (c & 0x000f);
   return x * 0x11;
}

/* 16-bit ARGB4444 (A in the top nibble) to ABGR8888. */
static INLINE uint32_t pixconv_argb4444_to_abgr8888(uint32_t c)
{
   uint32_t x = ((c & 0xf000) << 12) | ((c & 0x000f) << 16)
              | ((c & 0x00f0) <<  4) | ((c & 0x0f00) >>  8);
   return x * 0x11;
}

RETRO_BEGIN_DECLS

void conv_0rgb1555_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_0rgb1555_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_0rgb1555(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_abgr8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgba4444_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgba4444_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_bgr24_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_bgr24_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_0rgb1555(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_rgba4444(void *output_, const void *input_,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_abgr8888_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_abgr8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_0rgb1555_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_bgr24(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_yuyv_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_copy(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

RETRO_END_DECLS

#endif
