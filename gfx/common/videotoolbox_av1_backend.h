/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_AV1_BACKEND_H
#define RETROARCH_VIDEOTOOLBOX_AV1_BACKEND_H
#include "videotoolbox_av1_dispatch.h"
#include "metal_frame_convert.h"
#include "metal_decoder_snapshot.h"
#ifdef VIDEOTOOLBOX_AV1_OWNER

typedef metal_decoder_snapshot_view_t videotoolbox_av1_device_t;

typedef struct videotoolbox_av1_backend
{
   rav1_hw_backend_t offer;
   void *user;
   bool (*acquire)(void *, videotoolbox_av1_device_t *);
   void (*release)(void *);
   bool (*current)(void *, uintptr_t);
   bool (*enabled)(void *);
   videotoolbox_av1_register_t registration;
   size_t capacity;
} videotoolbox_av1_backend_t;

static bool videotoolbox_av1_backend_open_config(void *opaque,
      const rav1_sequence_t *header, const rav1_obu_t *sequence,
      const video_color_metadata_t *color, unsigned count, rav1_hw_ops_t *ops)
{
   videotoolbox_av1_backend_t *b = (videotoolbox_av1_backend_t *)opaque;
   videotoolbox_av1_device_t snapshot;
   videotoolbox_av1_config_t config;
   metal_frame_view_t view;
   metal_frame_sdr_params_t conversion;
   metal_frame_cv_pool_t *pool;
   bool opened;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!b || !b->acquire || !b->release || !b->current || count < 9
         || count > 32 || b->capacity < 3 || b->capacity > 64u * 1024 * 1024
         || (b->enabled && !b->enabled(b->user)))
      return false;
   if (count < 10)
      count = 10;
   if (b->capacity > 256u * 1024 * 1024 / count
         || !videotoolbox_av1_configure(header, sequence, color, &config))
      return false;
   memset(&snapshot, 0, sizeof(snapshot));
   if (!b->acquire(b->user, &snapshot) || !snapshot.device)
   {
      if (snapshot.device)
         b->release(snapshot.device);
      return false;
   }
   memset(&view, 0, sizeof(view));
   view.color = config.color;
   view.width = config.width;
   view.height = config.height;
   view.pixfmt = config.pixfmt;
   view.chroma_position = config.chroma_position;
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
   opened = videotoolbox_av1_dispatch_open(header, color, sequence, count,
         b->capacity, pool, b->registration, ops);
   if (!opened)
      metal_frame_cv_pool_free(pool);
   else if (!b->current(b->user, snapshot.generation))
   {
      videotoolbox_av1_decoder_close(ops);
      return false;
   }
   return opened;
}

static INLINE bool videotoolbox_av1_backend_init(videotoolbox_av1_backend_t *b,
      void *user, bool (*acquire)(void *, videotoolbox_av1_device_t *),
      void (*release)(void *), bool (*current)(void *, uintptr_t),
      bool (*enabled)(void *), videotoolbox_av1_register_t registration,
      size_t capacity)
{
   if (!b)
      return false;
   memset(b, 0, sizeof(*b));
   if (!acquire || !release || !current || capacity < 3
         || capacity > 64u * 1024 * 1024)
      return false;
   b->user = user;
   b->acquire = acquire;
   b->release = release;
   b->current = current;
   b->enabled = enabled;
   b->registration = registration;
   b->capacity = capacity;
   b->offer.user = b;
   b->offer.close = videotoolbox_av1_decoder_close;
   b->offer.drain = videotoolbox_av1_decoder_finish;
   b->offer.open_config = videotoolbox_av1_backend_open_config;
   b->offer.packet_decoder = true;
   return true;
}
#endif
#endif
