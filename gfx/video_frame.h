/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef _VIDEO_FRAME_H
#define _VIDEO_FRAME_H

#include <stdint.h>

#include <gfx/scaler/scaler.h>

void video_frame_convert_rgb16_to_rgb32(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      int width, int height,
      int in_pitch);

void video_frame_scale(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      enum scaler_pix_fmt format,
      unsigned scaler_width,
      unsigned scaler_height,
      unsigned scaler_pitch,
      unsigned width,
      unsigned height,
      unsigned pitch);

void video_frame_record_scale(
      struct scaler_ctx *scaler,
      void *output,
      const void *input,
      unsigned scaler_width,
      unsigned scaler_height,
      unsigned scaler_pitch,
      unsigned width,
      unsigned height,
      unsigned pitch,
      bool bilinear);

void video_frame_convert_argb8888_to_abgr8888(
      struct scaler_ctx *scaler,
      void *output, const void *input,
      int width, int height, int in_pitch);

void video_frame_convert_to_bgr24(
      struct scaler_ctx *scaler,
      void *output, const void *input,
      int width, int height, int in_pitch,
      bool bgr24);

void video_frame_convert_rgba_to_bgr(
      const void *src_data,
      void *dst_data,
      unsigned width);

#endif
