/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_AV1_DECODER_H
#define RETROARCH_VIDEOTOOLBOX_AV1_DECODER_H
#include <formats/rav1_hw.h>
#include "videotoolbox_av1_owner.h"
#include "metal_frame.h"
#ifdef VIDEOTOOLBOX_AV1_OWNER

typedef struct videotoolbox_av1_decoder videotoolbox_av1_decoder_t;

struct videotoolbox_av1_decoder
{
   rav1_sequence_t header;
   rav1_grain_t grain;
   metal_frame_view_t view;
   videotoolbox_av1_owner_t *owner;
   metal_frame_cv_pool_t *pool;
   CVPixelBufferRef image;
   void (*retire)(void *, videotoolbox_av1_decoder_t *);
   void *user;
   unsigned output_slot;
};

static int videotoolbox_av1_decoder_decode(void *opaque, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   videotoolbox_av1_decoder_t *d = (videotoolbox_av1_decoder_t *)opaque;
   bool show;
   int status;
   if (!d || !sequence || !picture || picture->slot < 0
         || (unsigned)picture->slot >= d->owner->count
         || memcmp(sequence, &d->header, sizeof(*sequence))
         || !picture->setup.size.upscaled_width || !picture->setup.size.height
         || picture->setup.size.upscaled_width > sequence->max_width
         || picture->setup.size.height > sequence->max_height)
      return -1;
   show = picture->header.show_frame || picture->header.show_existing;
   status = videotoolbox_av1_owner_append(
         d->owner, packet, size, show, picture->slot);
   if (!status && show)
   {
      if (d->image)
         CVPixelBufferRelease(d->image);
      d->image = NULL;
      d->output_slot = (unsigned)picture->slot;
      d->view.width = picture->setup.size.upscaled_width;
      d->view.height = picture->setup.size.height;
      d->grain = picture->grain.params;
      videotoolbox_av1_owner_discard_previous(d->owner);
   }
   return status;
}

static int videotoolbox_av1_decoder_output(void *opaque, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *out)
{
   videotoolbox_av1_decoder_t *d = (videotoolbox_av1_decoder_t *)opaque;
   videotoolbox_av1_result_t result;
   int status;
   if (!d || !out || !grain || slot != d->output_slot
         || memcmp(grain, &d->grain, sizeof(*grain)))
      return -1;
   if (!d->image)
   {
      status = videotoolbox_av1_owner_take(d->owner, slot, &result);
      if (status)
         return status;
      if (result.status || !result.image
            || (result.flags & kVTDecodeInfo_FrameDropped))
      {
         if (result.image)
            CVPixelBufferRelease(result.image);
         return -1;
      }
      d->image = result.image;
   }
   status = metal_frame_cv_pool_export(d->pool, &d->view, d->image, out);
   if (!status)
   {
      CVPixelBufferRelease(d->image);
      d->image = NULL;
   }
   return status;
}

static INLINE int videotoolbox_av1_decoder_finish(void *opaque, bool discard)
{
   videotoolbox_av1_decoder_t *d = (videotoolbox_av1_decoder_t *)opaque;
   if (!d)
      return -1;
   if (d->image)
   {
      if (!discard)
         return 1;
      CVPixelBufferRelease(d->image);
      d->image = NULL;
   }
   return videotoolbox_av1_owner_finish(d->owner, discard);
}

static INLINE int videotoolbox_av1_decoder_restart(void *opaque)
{
   videotoolbox_av1_decoder_t *d = (videotoolbox_av1_decoder_t *)opaque;
   int result;
   if (!d)
      return -1;
   if (d->image)
      CVPixelBufferRelease(d->image);
   d->image = NULL;
   result = videotoolbox_av1_owner_restart(d->owner);
   if (!result)
      d->output_slot = (unsigned)-1;
   return result;
}

static INLINE bool videotoolbox_av1_decoder_retire_poll(
      videotoolbox_av1_decoder_t **handle)
{
   videotoolbox_av1_decoder_t *d;
   if (!handle || !(d = *handle))
      return true;
   if (!videotoolbox_av1_owner_retire_poll(&d->owner))
      return false;
   free(d);
   *handle = NULL;
   return true;
}

static INLINE void videotoolbox_av1_decoder_close(rav1_hw_ops_t *ops)
{
   videotoolbox_av1_decoder_t *d;
   if (!ops || !(d = (videotoolbox_av1_decoder_t *)ops->decoder))
      return;
   memset(ops, 0, sizeof(*ops));
   videotoolbox_av1_owner_close(d->owner);
   if (d->image)
      CVPixelBufferRelease(d->image);
   d->image = NULL;
   metal_frame_cv_pool_free(d->pool);
   d->pool = NULL;
   d->retire(d->user, d);
}

/* Cold worker setup. Success takes pool ownership. wake/retire use a stable
 * external executor; neither may run service/retirement inline on playback. */
static INLINE bool videotoolbox_av1_decoder_open(const rav1_sequence_t *header,
      const video_color_metadata_t *color, const rav1_obu_t *sequence,
      unsigned count, size_t capacity, metal_frame_cv_pool_t *pool,
      videotoolbox_av1_register_t registration, void (*wake)(void *),
      void (*retire)(void *, videotoolbox_av1_decoder_t *), void *user,
      rav1_hw_ops_t *ops)
{
   videotoolbox_av1_decoder_t *d;
   videotoolbox_av1_config_t config;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!pool || !retire || !header
         || !videotoolbox_av1_configure(header, sequence, color, &config))
      return false;
   d = (videotoolbox_av1_decoder_t *)calloc(1, sizeof(*d));
   if (!d)
      return false;
   d->owner = videotoolbox_av1_owner_new(header, color, sequence, count,
         capacity, registration, wake, user, NULL);
   if (!d->owner)
   {
      free(d);
      return false;
   }
   d->header = *header;
   d->view.color = config.color;
   d->view.width = config.width;
   d->view.height = config.height;
   d->view.pixfmt = config.pixfmt;
   d->pool = pool;
   d->retire = retire;
   d->user = user;
   d->output_slot = count;
   ops->decoder = d;
   ops->decode = videotoolbox_av1_decoder_decode;
   ops->output = videotoolbox_av1_decoder_output;
   return true;
}
#endif
#endif
