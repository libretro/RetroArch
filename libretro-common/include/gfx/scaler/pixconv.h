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

#include <clamping.h>

#include <retro_common_api.h>

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
