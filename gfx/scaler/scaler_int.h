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

#ifndef SCALER_INT_H__
#define SCALER_INT_H__

#include "scaler.h"

void scaler_argb8888_vert(const struct scaler_ctx *ctx, void *output, int stride);
void scaler_argb8888_horiz(const struct scaler_ctx *ctx, const void *input, int stride);

void scaler_argb8888_point_special(const struct scaler_ctx *ctx,
      void *output, const void *input,
      int out_width, int out_height,
      int in_width, int in_height,
      int out_stride, int in_stride);

#endif

