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

#ifndef PIXCONV_H__
#define PIXCONV_H__

#include "scaler_common.h"

void conv_0rgb1555_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_0rgb1555_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_0rgb1555(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_rgb565_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_bgr24_argb8888(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_0rgb1555(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_rgb565(void *output, const void *input,
      int width, int height,
      int out_stride, int in_stride);

void conv_argb8888_bgr24(void *output, const void *input,
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

#endif

