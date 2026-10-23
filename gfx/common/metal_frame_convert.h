/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_METAL_FRAME_CONVERT_H
#define RARCH_METAL_FRAME_CONVERT_H
#include <string.h>
#include "metal_frame.h"

typedef struct metal_frame_sdr_params
{
   uint32_t dims[4];
   float levels[4];
   float coefficients[4];
   uint32_t mode[4];
} metal_frame_sdr_params_t;

static INLINE bool metal_frame_yuv_params(const metal_frame_view_t *view,
      metal_frame_sdr_params_t *output)
{
   metal_frame_sdr_params_t p;
   float sample, y_range, c_range, center, black;
   bool ten, full;
   if (!view || !output || (view->pixfmt != IMAGE_PIXFMT_NV12
            && view->pixfmt != IMAGE_PIXFMT_P010)
         || (view->color.range != 1 && view->color.range != 2)
         || !view->width || !view->height
         || !VIDEO_SCALE_FITS(view->width, view->height)
         || (view->crop_x & 1u) || (view->crop_y & 1u)
         || view->chroma_position > 2)
      return false;
   ten     = view->pixfmt == IMAGE_PIXFMT_P010;
   full    = view->color.range == 2;
   sample  = ten ? 65535.0f / 64.0f : 255.0f;
   center  = ten ? 512.0f : 128.0f;
   black   = full ? 0.0f : ten ? 64.0f : 16.0f;
   y_range = full ? (ten ? 1023.0f : 255.0f) : ten ? 876.0f : 219.0f;
   c_range = full ? (ten ? 1023.0f : 255.0f) : ten ? 896.0f : 224.0f;
   memset(&p, 0, sizeof(p));
   p.dims[0] = view->width; p.dims[1] = view->height;
   p.dims[2] = view->crop_x; p.dims[3] = view->crop_y;
   p.levels[0] = sample / y_range; p.levels[1] = -black / y_range;
   p.levels[2] = sample / c_range; p.levels[3] = -center / c_range;
   p.coefficients[0] = 1.5748f; p.coefficients[1] = -0.18732427f;
   p.coefficients[2] = -0.46812427f; p.coefficients[3] = 1.8556f;
   p.mode[0] = view->color.transfer;
   p.mode[1] = view->chroma_position;
   *output = p;
   return true;
}
/* SDR is encoded sRGB; HDR is linear scRGB (1.0 = 80 nits). */
static INLINE bool metal_frame_sdr_params(const metal_frame_view_t *view,
      metal_frame_sdr_params_t *output)
{
   return view && view->color.primaries == 1 && view->color.matrix == 1
      && (view->color.transfer == 1 || view->color.transfer == 13)
      && metal_frame_yuv_params(view, output);
}
static INLINE bool metal_frame_hdr_params(const metal_frame_view_t *view,
      metal_frame_sdr_params_t *output)
{
   metal_frame_sdr_params_t p;
   if (!output || !view || view->color.primaries != 9 || view->color.matrix != 9
         || (view->color.transfer != 16 && view->color.transfer != 18)
         || !metal_frame_yuv_params(view, &p))
      return false;
   p.coefficients[0] = 1.4746f; p.coefficients[1] = -0.164553127f;
   p.coefficients[2] = -0.571353127f; p.coefficients[3] = 1.8814f;
   *output = p;
   return true;
}

#endif
