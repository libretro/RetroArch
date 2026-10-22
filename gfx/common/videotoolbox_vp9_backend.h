/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_BACKEND_H
#define RETROARCH_VIDEOTOOLBOX_VP9_BACKEND_H
#include "videotoolbox_vp9_dispatch.h"
#include "metal_frame_convert.h"
#include "metal_decoder_snapshot.h"
#ifdef VIDEOTOOLBOX_VP9_OWNER

typedef metal_decoder_snapshot_view_t videotoolbox_vp9_device_t;

typedef struct videotoolbox_vp9_backend
{
   rvp9_hw_backend_t offer;
   void *user;
   bool (*acquire)(void *, videotoolbox_vp9_device_t *);
   void (*release)(void *);
   bool (*current)(void *, uintptr_t);
   bool (*enabled)(void *);
   videotoolbox_vp9_register_t registration;
   size_t capacity;
   unsigned chroma;
} videotoolbox_vp9_backend_t;

static bool videotoolbox_vp9_backend_open_config(void *opaque,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      const rvp9_config_t *record, unsigned count, rvp9_hw_ops_t *ops)
{
   videotoolbox_vp9_backend_t *b = (videotoolbox_vp9_backend_t *)opaque;
   videotoolbox_vp9_device_t snapshot;
   videotoolbox_vp9_config_t config;
   metal_frame_view_t view;
   metal_frame_sdr_params_t conversion;
   metal_frame_cv_pool_t *pool;
   video_color_metadata_t resolved;
   unsigned chroma;
   bool opened;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!b || !b->acquire || !b->release || !b->current || count < 10
         || count > 32 || b->capacity <= 34
         || b->capacity > 256u * 1024 * 1024 / count
         || (b->enabled && !b->enabled(b->user)) || !header
         || header->show_existing_frame)
      return false;
   chroma = record ? record->chroma : b->chroma;
   if (record)
   {
      if (record->profile != header->profile
            || record->bit_depth != header->bit_depth
            || !rvp9_config_apply_color(record, color, &resolved))
         return false;
      color = &resolved;
   }
   if (!videotoolbox_vp9_configure(header, color, chroma, &config))
      return false;
   memset(&snapshot, 0, sizeof(snapshot));
   if (!b->acquire(b->user, &snapshot) || !snapshot.device)
   {
      if (snapshot.device)
         b->release(snapshot.device);
      return false;
   }
   memset(&view, 0, sizeof(view));
   view.color  = config.color;
   view.width  = config.width;
   view.height = config.height;
   view.pixfmt = config.pixfmt;
   if ((!metal_frame_sdr_params(&view, &conversion)
             && !(snapshot.hdr && metal_frame_hdr_params(&view, &conversion)))
         || !b->current(b->user, snapshot.generation))
   {
      b->release(snapshot.device);
      return false;
   }
   pool = metal_frame_cv_pool_create(snapshot.device, 32);
   b->release(snapshot.device);
   if (!pool)
      return false;
   opened = videotoolbox_vp9_dispatch_open(header, color, chroma, count,
         b->capacity, pool, b->registration, ops);
   if (!opened)
      metal_frame_cv_pool_free(pool);
   else if (!b->current(b->user, snapshot.generation))
   {
      videotoolbox_vp9_decoder_close(ops);
      return false;
   }
   return opened;
}

static bool videotoolbox_vp9_backend_open_color(void *opaque,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      unsigned count, rvp9_hw_ops_t *ops)
{
   return videotoolbox_vp9_backend_open_config(opaque, header, color,
         NULL, count, ops);
}

static bool videotoolbox_vp9_backend_open(
      void *opaque, const rvp9_hdr *header, unsigned count, rvp9_hw_ops_t *ops)
{
   return videotoolbox_vp9_backend_open_color(opaque, header, NULL, count, ops);
}

/* Initialize once before publication. acquire returns an owned device and
 * matching generation/HDR capability; current rejects retired snapshots. */
static INLINE bool videotoolbox_vp9_backend_init(videotoolbox_vp9_backend_t *b,
      void *user, bool (*acquire)(void *, videotoolbox_vp9_device_t *),
      void (*release)(void *), bool (*current)(void *, uintptr_t),
      bool (*enabled)(void *), videotoolbox_vp9_register_t registration,
      size_t capacity, unsigned chroma)
{
   if (!b)
      return false;
   memset(b, 0, sizeof(*b));
   if (!acquire || !release || !current || capacity <= 34
         || capacity > 64u * 1024 * 1024 || chroma > 1)
      return false;
   b->user                 = user;
   b->acquire              = acquire;
   b->release              = release;
   b->current              = current;
   b->enabled              = enabled;
   b->registration         = registration;
   b->capacity             = capacity;
   b->chroma               = chroma;
   b->offer.user           = b;
   b->offer.open           = videotoolbox_vp9_backend_open;
   b->offer.open_color     = videotoolbox_vp9_backend_open_color;
   b->offer.close          = videotoolbox_vp9_decoder_close;
   b->offer.drain          = videotoolbox_vp9_decoder_finish;
   b->offer.open_config    = videotoolbox_vp9_backend_open_config;
   b->offer.packet_decoder = true;
   b->offer.restart        = videotoolbox_vp9_decoder_restart;
   return true;
}
#endif
#endif
