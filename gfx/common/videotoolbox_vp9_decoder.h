/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_DECODER_H
#define RETROARCH_VIDEOTOOLBOX_VP9_DECODER_H
#include <formats/rvp9_hw.h>
#include "videotoolbox_vp9_owner.h"
#include "metal_frame.h"
#ifdef VIDEOTOOLBOX_VP9_OWNER

typedef struct videotoolbox_vp9_decoder videotoolbox_vp9_decoder_t;

struct videotoolbox_vp9_decoder
{
   rvp9_hdr header;
   metal_frame_view_t view;
   videotoolbox_vp9_owner_t *owner;
   metal_frame_cv_pool_t *pool;
   CVPixelBufferRef image;
   void (*retire)(void *, videotoolbox_vp9_decoder_t *);
   void *user;
   unsigned output_slot;
};

static int videotoolbox_vp9_decoder_decode(void *opaque, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   videotoolbox_vp9_decoder_t *d = (videotoolbox_vp9_decoder_t *)opaque;
   const rvp9_hdr *h;
   int status;
   if (!d || !picture || picture->slot < 0
         || (unsigned)picture->slot >= d->owner->count)
      return -1;
   h = &picture->header;
   if (h->w != d->header.w || h->h != d->header.h
         || h->profile != d->header.profile
         || h->bit_depth != d->header.bit_depth || h->subsampling_x != 1
         || h->subsampling_y != 1 || h->color_space != d->header.color_space
         || h->color_range != d->header.color_range)
      return -1;
   status = videotoolbox_vp9_owner_append(
         d->owner, packet, size, h->show_frame != 0, picture->slot);
   if (!status && h->show_frame)
   {
      if (d->image)
         CVPixelBufferRelease(d->image);
      d->image       = NULL;
      d->output_slot = (unsigned)picture->slot;
      videotoolbox_vp9_owner_discard_previous(d->owner);
   }
   return status;
}

static int videotoolbox_vp9_decoder_output(
      void *opaque, unsigned slot, video_hw_frame_t *out)
{
   videotoolbox_vp9_decoder_t *d = (videotoolbox_vp9_decoder_t *)opaque;
   videotoolbox_vp9_result_t result;
   int status;
   if (!d || !out || slot != d->output_slot)
      return -1;
   if (!d->image)
   {
      status = videotoolbox_vp9_owner_take(d->owner, slot, &result);
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

static INLINE int videotoolbox_vp9_decoder_finish(void *opaque, bool discard)
{
   videotoolbox_vp9_decoder_t *d = (videotoolbox_vp9_decoder_t *)opaque;
   if (!d)
      return -1;
   if (d->image)
   {
      if (!discard)
         return 1;
      CVPixelBufferRelease(d->image);
      d->image = NULL;
   }
   return videotoolbox_vp9_owner_finish(d->owner, discard);
}

static INLINE int videotoolbox_vp9_decoder_restart(void *opaque)
{
   videotoolbox_vp9_decoder_t *d = (videotoolbox_vp9_decoder_t *)opaque;
   int result;
   if (!d) return -1;
   if (d->image) CVPixelBufferRelease(d->image);
   d->image = NULL;
   result = videotoolbox_vp9_owner_restart(d->owner);
   if (!result) d->output_slot = (unsigned)-1;
   return result;
}

static INLINE bool videotoolbox_vp9_decoder_retire_poll(
      videotoolbox_vp9_decoder_t **handle)
{
   videotoolbox_vp9_decoder_t *d;
   if (!handle || !(d = *handle))
      return true;
   if (!videotoolbox_vp9_owner_retire_poll(&d->owner))
      return false;
   free(d);
   *handle = NULL;
   return true;
}

static INLINE void videotoolbox_vp9_decoder_close(rvp9_hw_ops_t *ops)
{
   videotoolbox_vp9_decoder_t *d;
   if (!ops || !(d = (videotoolbox_vp9_decoder_t *)ops->decoder))
      return;
   memset(ops, 0, sizeof(*ops));
   videotoolbox_vp9_owner_close(d->owner);
   if (d->image)
      CVPixelBufferRelease(d->image);
   d->image = NULL;
   metal_frame_cv_pool_free(d->pool);
   d->pool = NULL;
   d->retire(d->user, d);
}

/* Cold worker setup. Success takes pool ownership. wake/retire use a stable
 * external executor; neither may run service/retirement inline on playback. */
static INLINE bool videotoolbox_vp9_decoder_open(const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma, unsigned count,
      size_t capacity, metal_frame_cv_pool_t *pool,
      videotoolbox_vp9_register_t registration, void (*wake)(void *),
      void (*retire)(void *, videotoolbox_vp9_decoder_t *), void *user,
      rvp9_hw_ops_t *ops)
{
   videotoolbox_vp9_decoder_t *d;
   videotoolbox_vp9_config_t config;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!pool || !retire || !header || header->show_existing_frame
         || !videotoolbox_vp9_configure(header, color, chroma, &config))
      return false;
   d = (videotoolbox_vp9_decoder_t *)calloc(1, sizeof(*d));
   if (!d)
      return false;
   d->owner = videotoolbox_vp9_owner_new(header, color, chroma, count, capacity,
         registration, wake, user, NULL);
   if (!d->owner)
   {
      free(d);
      return false;
   }
   d->header      = *header;
   d->view.color  = config.color;
   d->view.width  = config.width;
   d->view.height = config.height;
   d->view.pixfmt = config.pixfmt;
   d->pool        = pool;
   d->retire      = retire;
   d->user        = user;
   d->output_slot = count;
   ops->decoder   = d;
   ops->decode    = videotoolbox_vp9_decoder_decode;
   ops->output    = videotoolbox_vp9_decoder_output;
   return true;
}
#endif
#endif
