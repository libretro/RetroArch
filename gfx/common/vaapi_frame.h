/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_FRAME_H
#define RARCH_VAAPI_FRAME_H
#include <boolean.h>
#include <formats/video_hw_frame.h>
RETRO_BEGIN_DECLS
struct vaapi_hevc_export;
typedef struct vaapi_frame_pool vaapi_frame_pool_t;
typedef struct vaapi_frame_view
{
   const struct vaapi_hevc_export *source;
   video_color_metadata_t color;
   unsigned x, y, width, height, yuv, pixfmt;
} vaapi_frame_view_t;
/* Cold decoder-worker setup. One producer, one renderer owner per lease.
 * Refuses non-lock-free atomics. Close/free/collect are producer-only. */
vaapi_frame_pool_t *vaapi_frame_pool_new(unsigned count);
void vaapi_frame_pool_close(vaapi_frame_pool_t *pool);
bool vaapi_frame_pool_free(vaapi_frame_pool_t *pool);
/* Producer moves a completed export into an opaque internal Vulkan frame.
 * 0 consumes source, 1 BUSY/-1 refusal consume nothing; out unchanged on error.
 * Session/runtime/pool backing source must outlive the returned lease. */
int vaapi_frame_export(vaapi_frame_pool_t *pool, struct vaapi_hevc_export *source,
      unsigned x, unsigned y, unsigned width, unsigned height, unsigned yuv,
      const video_color_metadata_t *color, video_hw_frame_t *out);
bool vaapi_frame_callback(void (*abandon)(void*, uint64_t, uint64_t));
/* Claim publishes a read-only source pointer valid ONLY while claimed and
 * until take. Cold imports may borrow FDs without consuming this export. */
bool vaapi_frame_claim(void *owner, uint64_t index, uint64_t generation,
      uint64_t image, vaapi_frame_view_t *view);
/* Moves the claimed export into empty source for the imported frame owner. */
bool vaapi_frame_take(void *owner, uint64_t index, uint64_t generation, struct vaapi_hevc_export *source);
/* Before GPU use: restore pending. Pass the taken export back, or NULL if
 * source was only borrowed. False consumes nothing. */
bool vaapi_frame_unclaim(void *owner, uint64_t index, uint64_t generation, struct vaapi_hevc_export *source);
/* After GPU/resource retirement: publish return. Same ownership rule as
 * unclaim; never closes FDs or calls the decoder pool on the renderer. */
bool vaapi_frame_finish(void *owner, uint64_t index, uint64_t generation, struct vaapi_hevc_export *source);
/* One bounded producer pass. Close returned exports and release VA tickets on
 * the correct owner. Refused native ticket returns remain held for retry. */
unsigned vaapi_frame_collect(vaapi_frame_pool_t *pool);
RETRO_END_DECLS
#endif
