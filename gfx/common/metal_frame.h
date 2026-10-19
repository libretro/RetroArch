/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_METAL_FRAME_H
#define RARCH_METAL_FRAME_H

#include <boolean.h>
#include <formats/image.h>
#include <formats/video_hw_frame.h>
#include "../video_defines.h"

RETRO_BEGIN_DECLS

/* All source references are borrowed. Backing objects are CFTypeRefs.
 * Reuse a listener created outside the frame path. Readiness must advance
 * even on producer failure, after the last write to these textures. */
typedef struct metal_frame_source
{
   uint64_t ready_value, done_value;
   void *device;
   void *planes[2];
   void *ready;
   void *done;
   void *listener;
   const void *backing[3];
   video_color_metadata_t color;
   unsigned pixfmt, width, height, crop_x, crop_y, yuv;
} metal_frame_source_t;

typedef struct metal_frame_view
{
   void *planes[2];
   video_color_metadata_t color;
   unsigned pixfmt, width, height, crop_x, crop_y, yuv;
} metal_frame_view_t;

struct video_frame_import;

/* Success exports one lease; failure leaves output and source untouched.
 * Done values belong to this image slot and must not be advanced early. */
bool metal_frame_create(const metal_frame_source_t *source,
      video_hw_frame_t *output);

/* Success transfers the lease to command-buffer completion. The callback
 * ends its encoders before returning; the caller commits the buffer.
 * Prepare fallible resources first. Views are borrowed for this call only.
 * Failure leaves the lease with the caller. No CPU or GPU round trip. */
bool metal_frame_encode(const struct video_frame_import *frame,
      void *device, void *command_buffer,
      void (*encode)(void *, const metal_frame_view_t *, void *), void *user);

static INLINE bool metal_frame_geometry_valid(unsigned pixfmt,
      unsigned width, unsigned height, unsigned x, unsigned y,
      unsigned plane_width, unsigned plane_height,
      unsigned uv_width, unsigned uv_height)
{
   bool planar = pixfmt == IMAGE_PIXFMT_NV12 || pixfmt == IMAGE_PIXFMT_P010;
   if ((!planar && pixfmt != IMAGE_PIXFMT_8888 && pixfmt != IMAGE_PIXFMT_FP16)
         || !width || !height || !VIDEO_SCALE_FITS(width, height)
         || x > plane_width || y > plane_height
         || width > plane_width - x || height > plane_height - y)
      return false;
   if (!planar)
      return !uv_width && !uv_height;
   return !(x & 1u) && !(y & 1u)
      && uv_width == plane_width / 2u + (plane_width & 1u)
      && uv_height == plane_height / 2u + (plane_height & 1u);
}

RETRO_END_DECLS
#endif
