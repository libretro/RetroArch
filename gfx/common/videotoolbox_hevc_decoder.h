/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_DECODER_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_DECODER_H

#include <formats/rh265_hw.h>
#include "videotoolbox_hevc_owner.h"
#include "metal_frame.h"

#ifdef VIDEOTOOLBOX_HEVC_SLOTS
typedef struct videotoolbox_hevc_decoder
{
   videotoolbox_hevc_owner_t *owner;
   metal_frame_cv_pool_t *pool;
   rh265_video_params_t params;
   metal_frame_view_t view;
   CVPixelBufferRef image[32];
} videotoolbox_hevc_decoder_t;

static int videotoolbox_hevc_decoder_decode(void *opaque,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, const rh265_hw_picture_t *picture)
{
   videotoolbox_hevc_decoder_t *decoder = (videotoolbox_hevc_decoder_t*)opaque;
   videotoolbox_hevc_result_t stale;
   videotoolbox_hevc_slot_t *slot;
   OSStatus status;
   VTDecodeInfoFlags info;
   if (!decoder || !params || !au || !slices || !picture
         || picture->slot >= decoder->owner->slots->count
         || memcmp(params, &decoder->params, sizeof(*params))) return -1;
   slot = &decoder->owner->slots->slot[picture->slot];
   if (retro_atomic_load_acquire_int(&slot->state) == 1) return 1;
   /* A reference slot can be reused after a prior output was discarded. */
   if (videotoolbox_hevc_slots_take(decoder->owner->slots, picture->slot, &stale) == 1
         && stale.image) CVPixelBufferRelease(stale.image);
   if (decoder->image[picture->slot])
   {
      CVPixelBufferRelease(decoder->image[picture->slot]);
      decoder->image[picture->slot] = NULL;
   }
   status = videotoolbox_hevc_slots_submit(decoder->owner->slots, picture->slot,
         decoder->owner->session, decoder->owner->format, au, slices, au->num_slices, &info);
   if (!status && (info & kVTDecodeInfo_FrameDropped)) return -1;
   return status == 1 ? 1 : status ? -1 : 0;
}

static int videotoolbox_hevc_decoder_output(void *opaque,
      unsigned index, video_hw_frame_t *out)
{
   videotoolbox_hevc_decoder_t *decoder = (videotoolbox_hevc_decoder_t*)opaque;
   videotoolbox_hevc_result_t result;
   int status;
   if (!decoder || !out || index >= decoder->owner->slots->count) return -1;
   if (!decoder->image[index])
   {
      status = videotoolbox_hevc_slots_take(decoder->owner->slots, index, &result);
      if (!status) return 1;
      if (status < 0) return -1;
      if (result.status || !result.image || (result.flags & kVTDecodeInfo_FrameDropped))
      {
         if (result.image) CVPixelBufferRelease(result.image);
         return -1;
      }
      decoder->image[index] = result.image;
   }
   status = metal_frame_cv_pool_export(decoder->pool, &decoder->view,
         decoder->image[index], out);
   if (!status)
   {
      CVPixelBufferRelease(decoder->image[index]);
      decoder->image[index] = NULL;
   }
   return status;
}

static INLINE void videotoolbox_hevc_decoder_close(rh265_hw_ops_t *ops)
{
   videotoolbox_hevc_decoder_t *decoder;
   unsigned i;
   if (!ops || !(decoder = (videotoolbox_hevc_decoder_t*)ops->decoder)) return;
   memset(ops, 0, sizeof(*ops));
   videotoolbox_hevc_owner_retire(&decoder->owner);
   for (i = 0; i < 32; i++)
      if (decoder->image[i]) CVPixelBufferRelease(decoder->image[i]);
   metal_frame_cv_pool_free(decoder->pool);
   free(decoder);
}

/* Setup only. Success takes pool ownership; failure leaves it with caller.
 * Parsed params must come from the source query (including zeroed padding).
 * Frontend supplies a nonblocking retirement executor and a completed-image
 * Metal pool. Registration and driver lifetime are handled by the caller. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE bool videotoolbox_hevc_decoder_open(const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, metal_frame_cv_pool_t *pool,
      videotoolbox_hevc_enqueue_t enqueue, void *user, rh265_hw_ops_t *ops)
{
   videotoolbox_hevc_decoder_t *decoder;
   rh265_video_color_t color;
   OSStatus status;
   if (!ops) return false;
   memset(ops, 0, sizeof(*ops));
   if (!source || !params || !pool || !enqueue || params->pps_id < 0
         || !slots || slots > 32 || params->width <= 0 || params->height <= 0
         || params->crop_left < 0 || params->crop_top < 0
         || params->crop_right < 0 || params->crop_bottom < 0
         || params->crop_left >= params->width
         || params->crop_right >= params->width - params->crop_left
         || params->crop_top >= params->height
         || params->crop_bottom >= params->height - params->crop_top
         || rh265_video_get_color(source, (unsigned)params->pps_id, &color)) return false;
   decoder = (videotoolbox_hevc_decoder_t*)calloc(1, sizeof(*decoder));
   if (!decoder) return false;
   if (rh265_video_get_params(source, (unsigned)params->pps_id, &decoder->params)
         || memcmp(params, &decoder->params, sizeof(*params)))
   { free(decoder); return false; }
   decoder->owner = videotoolbox_hevc_owner_new(source, (unsigned)params->pps_id,
         slots, enqueue, user, &status);
   if (!decoder->owner) { free(decoder); return false; }
   decoder->pool = pool;
   decoder->view.width = (unsigned)(params->width - params->crop_left - params->crop_right);
   decoder->view.height = (unsigned)(params->height - params->crop_top - params->crop_bottom);
   decoder->view.crop_x = (unsigned)params->crop_left;
   decoder->view.crop_y = (unsigned)params->crop_top;
   decoder->view.pixfmt = params->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   decoder->view.color.primaries = color.primaries;
   decoder->view.color.transfer = color.transfer;
   decoder->view.color.matrix = color.matrix;
   decoder->view.color.range = color.full_range ? 2 : 1;
   ops->decoder = decoder; ops->decode = videotoolbox_hevc_decoder_decode;
   ops->output = videotoolbox_hevc_decoder_output;
   return true;
}
#endif
#endif
