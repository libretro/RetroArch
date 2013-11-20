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

#ifndef SCALER_H__
#define SCALER_H__

#include <stdint.h>
#include <stddef.h>
#include "../../boolean.h"
#include "scaler_common.h"

#define FILTER_UNITY (1 << 14)

enum scaler_pix_fmt
{
   SCALER_FMT_ARGB8888 = 0,
   SCALER_FMT_ABGR8888,
   SCALER_FMT_0RGB1555,
   SCALER_FMT_RGB565,
   SCALER_FMT_BGR24,
   SCALER_FMT_YUYV
};

enum scaler_type
{
   SCALER_TYPE_UNKNOWN = 0,
   SCALER_TYPE_POINT,
   SCALER_TYPE_BILINEAR,
   SCALER_TYPE_SINC
};

struct scaler_filter
{
   int16_t *filter;
   int filter_len;
   int filter_stride;
   int *filter_pos;
};

struct scaler_ctx
{
   int in_width;
   int in_height;
   int in_stride;

   int out_width;
   int out_height;
   int out_stride;

   enum scaler_pix_fmt in_fmt;
   enum scaler_pix_fmt out_fmt;
   enum scaler_type scaler_type;

   void (*scaler_horiz)(const struct scaler_ctx*,
         const void*, int);
   void (*scaler_vert)(const struct scaler_ctx*,
         void*, int);
   void (*scaler_special)(const struct scaler_ctx*,
         void*, const void*, int, int, int, int, int, int);

   void (*in_pixconv)(void*, const void*, int, int, int, int);
   void (*out_pixconv)(void*, const void*, int, int, int, int);
   void (*direct_pixconv)(void*, const void*, int, int, int, int);

   bool unscaled;
   struct scaler_filter horiz, vert;

   struct
   {
      uint32_t *frame;
      int stride;
   } input;

   struct
   {
      uint64_t *frame;
      int width;
      int height;
      int stride;
   } scaled;

   struct
   {
      uint32_t *frame;
      int stride;
   } output;
};

bool scaler_ctx_gen_filter(struct scaler_ctx *ctx);
void scaler_ctx_gen_reset(struct scaler_ctx *ctx);

void scaler_ctx_scale(struct scaler_ctx *ctx,
      void *output, const void *input);

void *scaler_alloc(size_t elem_size, size_t size);
void scaler_free(void *ptr);

#endif

