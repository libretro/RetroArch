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
   unsigned pixfmt, width, height, crop_x, crop_y, yuv, chroma_position;
} metal_frame_source_t;

typedef struct metal_frame_view
{
   void *planes[2];
   video_color_metadata_t color;
   unsigned pixfmt, width, height, crop_x, crop_y, yuv, chroma_position;
} metal_frame_view_t;

struct video_frame_import;

/* Success exports one lease; failure leaves output and source untouched.
 * Done values belong to this image slot and must not be advanced early. */
bool metal_frame_create(const metal_frame_source_t *source,
      video_hw_frame_t *output);

/* CoreVideo NV12/P010 plane views. Cache and pixel buffer are borrowed;
 * source planes/backing must be empty. Retains the buffer and both views.
 * Pixel format supplies range; conflicting explicit range is refused.
 * Known CoreVideo color tags fill absent/unspecified source fields.
 * Conflicting or unknown attached tags refuse without changing output. */
bool metal_frame_create_cv(const metal_frame_source_t *source,
      void *pixel_buffer, void *texture_cache, video_hw_frame_t *output);

typedef struct metal_frame_cv_pool metal_frame_cv_pool_t;

/* Setup for already-decoded CoreVideo images on this device. Creates the
 * cache, ready event, listener and private completion events outside export.
 * NULL means setup/capability refusal; caller retains device ownership. */
metal_frame_cv_pool_t *metal_frame_cv_pool_create(void *device, unsigned count);

/* Setup only. Borrowed cache, device, listener and events are retained.
 * Ready must already be signaled to at least 1: export only decoded images.
 * Supply 1..32 distinct, initially zero done events, separate from ready.
 * Done events are exclusive to this pool and its leases; do not signal
 * them externally or share them with another pool.
 * All operations on a pool belong to one producer thread. */
metal_frame_cv_pool_t *metal_frame_cv_pool_new(void *device, void *cache,
      void *ready, void *listener, void *const *done, unsigned count);
void metal_frame_cv_pool_free(metal_frame_cv_pool_t *pool);

/* 0 exported lease, 1 all slots occupied/exhausted, -1 invalid/refused.
 * Metadata planes must be empty. Image is borrowed; out is untouched on
 * busy/error. One bounded slot scan, no wait. Exported leases survive free. */
int metal_frame_cv_pool_export(metal_frame_cv_pool_t *pool,
      const metal_frame_view_t *metadata, void *image, video_hw_frame_t *out);

/* Success transfers the lease to command-buffer completion. The callback
 * starts with no active encoder and ends its encoders before returning; the caller commits the buffer.
 * False from the callback must leave no commands using the source planes.
 * Refusal may leave a harmless readiness wait on the command buffer.
 * Prepare fallible resources first. Views are borrowed for this call only.
 * Failure leaves the lease with the caller. No CPU or GPU round trip. */
bool metal_frame_encode(const struct video_frame_import *frame,
      void *device, void *command_buffer,
      bool (*encode)(void *, const metal_frame_view_t *, void *), void *user);

/* Same-format packed copy into independent, single-level storage. The
 * command buffer must retain references. No transfer/color conversion. */
bool metal_frame_copy(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *destination);

/* Shared SDR/HDR planar pipeline. Create once outside frame submission;
 * returns an owned CFTypeRef. */
void *metal_frame_sdr_pipeline_create(void *device);

/* NV12/P010 BT.709 primaries/matrix, explicit limited/full range and
 * BT.709 or sRGB transfer, to independent shader-writable RGBA8 sRGB.
 * Use the factory pipeline and nearest 2x2 chroma reconstruction.
 * The retained-reference command buffer owns resources after acceptance. */
bool metal_frame_convert_sdr(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *pipeline, void *destination);

/* NV12/P010 BT.2020 nonconstant-luminance PQ/HLG, explicit range, to
 * shader-writable RGBA16Float linear scRGB. PQ preserves absolute nits;
 * HLG uses the existing 1000-nit, gamma-1.2 display reference. Negative
 * out-of-gamut components survive. Same single-lease contract as SDR. */
bool metal_frame_convert_hdr(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *pipeline, void *destination);

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
