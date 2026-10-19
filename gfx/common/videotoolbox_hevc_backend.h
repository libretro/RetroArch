/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_BACKEND_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_BACKEND_H
#include "videotoolbox_hevc_decoder.h"
#include "metal_frame_convert.h"

#ifdef VIDEOTOOLBOX_HEVC_SLOTS
typedef struct videotoolbox_hevc_backend
{
   rh265_hw_backend_t offer;
   void *user, *enqueue_user;
   /* Setup callbacks only: acquire returns an owned, stable device snapshot.
    * Platform registry withdrawal must synchronize with acquisition. */
   void *(*acquire)(void *user);
   void (*release)(void *device);
   bool (*enabled)(void *user);
   videotoolbox_hevc_enqueue_t enqueue;
   bool validate_color;
   bool (*hdr_enabled)(void *user);
} videotoolbox_hevc_backend_t;

static VIDEOTOOLBOX_HEVC_AVAILABLE bool videotoolbox_hevc_backend_open(void *opaque, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   videotoolbox_hevc_backend_t *backend = (videotoolbox_hevc_backend_t*)opaque;
   metal_frame_cv_pool_t *pool;
   void *device;
   bool opened;
   if (!ops) return false;
   memset(ops, 0, sizeof(*ops));
   if (!backend || !source || !params || !slots || slots > 32
         || !backend->acquire || !backend->release || !backend->enqueue
         || (backend->enabled && !backend->enabled(backend->user))) return false;
   if (backend->validate_color)
   {
      rh265_video_color_t color;
      metal_frame_view_t view;
      metal_frame_sdr_params_t conversion;
      if (params->pps_id < 0 || params->width <= 0 || params->height <= 0
            || params->crop_left < 0 || params->crop_top < 0
            || params->crop_right < 0 || params->crop_bottom < 0
            || params->crop_left >= params->width || params->crop_top >= params->height
            || params->crop_right >= params->width - params->crop_left
            || params->crop_bottom >= params->height - params->crop_top
            || rh265_video_get_color(source, (unsigned)params->pps_id, &color)) return false;
      memset(&view, 0, sizeof(view));
      view.width = (unsigned)(params->width - params->crop_left - params->crop_right);
      view.height = (unsigned)(params->height - params->crop_top - params->crop_bottom);
      view.crop_x = (unsigned)params->crop_left; view.crop_y = (unsigned)params->crop_top;
      view.pixfmt = params->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
      view.color.primaries = color.primaries; view.color.transfer = color.transfer;
      view.color.matrix = color.matrix; view.color.range = color.full_range ? 2 : 1;
      if (!metal_frame_sdr_params(&view, &conversion)
            && !(backend->hdr_enabled && backend->hdr_enabled(backend->user)
               && metal_frame_hdr_params(&view, &conversion))) return false;
   }
   device = backend->acquire(backend->user);
   if (!device) return false;
   pool = metal_frame_cv_pool_create(device, 32);
   backend->release(device);
   if (!pool) return false;
   opened = videotoolbox_hevc_decoder_open(source, params, slots, pool,
         backend->enqueue, backend->enqueue_user, ops);
   if (!opened) metal_frame_cv_pool_free(pool);
   return opened;
}

/* Caller owns stable storage until withdrawal and setup calls finish.
 * Existing codec sessions copy their close operation and own all resources. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE void videotoolbox_hevc_backend_init(videotoolbox_hevc_backend_t *backend,
      void *user, void *(*acquire)(void*), void (*release)(void*),
      bool (*enabled)(void*), videotoolbox_hevc_enqueue_t enqueue,
      void *enqueue_user, bool validate_color)
{
   memset(backend, 0, sizeof(*backend));
   backend->offer.user = backend; backend->offer.open = videotoolbox_hevc_backend_open;
   backend->offer.close = videotoolbox_hevc_decoder_close;
   backend->user = user; backend->acquire = acquire; backend->release = release;
   backend->enabled = enabled; backend->enqueue = enqueue;
   backend->enqueue_user = enqueue_user; backend->validate_color = validate_color;
}
#endif
#endif
