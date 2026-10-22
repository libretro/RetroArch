/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_vp9_decoder.h"
#include <formats/image_yuv_blit.h>
#include <formats/rvp9_color.h>
#include <stdlib.h>
#include <string.h>

struct vaapi_vp9_decoder
{
   vaapi_vp9_export_t pending;
   vaapi_vp9_export_ops_t exports;
   vaapi_vp9_pool_t *pool;
   vaapi_frame_pool_t *frames;
   rvp9_hdr params;
   video_color_metadata_t color;
   unsigned width, height, yuv;
   bool closed, failed;
};

int vaapi_vp9_decoder_poll(vaapi_vp9_decoder_t *decoder)
{
   unsigned i;
   int result = 1;
   if (!decoder)
      return -1;
   vaapi_frame_collect(decoder->frames);
   if (decoder->pending.active && !decoder->pending.ready
         && !vaapi_vp9_export_close(&decoder->pending))
      result = -1;
   if (decoder->pool->active)
      for (i = 0; i < decoder->pool->session->slots; i++)
         if (decoder->pool->transactions[i].active)
         {
            int status = vaapi_vp9_pool_poll(decoder->pool, i);
            if (status < 0)
            {
               decoder->failed = true;
               result = -1;
            }
            else if (status > 0 && result > 0)
               result = 0;
         }
   if (decoder->failed)
      return -1;
   return result;
}

static int vaapi_vp9_decoder_decode(void *opaque, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   vaapi_vp9_decoder_t *decoder = (vaapi_vp9_decoder_t *)opaque;
   const rvp9_hdr *header;
   int result;
   if (!decoder || decoder->closed || !picture
         || vaapi_vp9_decoder_poll(decoder) < 0)
      return -1;
   header = &picture->header;
   if (header->w != decoder->params.w || header->h != decoder->params.h
         || header->profile != decoder->params.profile
         || header->bit_depth != decoder->params.bit_depth
         || header->subsampling_x != 1 || header->subsampling_y != 1
         || header->color_space != decoder->params.color_space
         || header->color_range != decoder->params.color_range)
      return -1;
   result = vaapi_vp9_pool_decode(decoder->pool, packet, size, picture);
   if (result < 0)
      decoder->failed = true;
   return result;
}

static int vaapi_vp9_decoder_output(
      void *opaque, unsigned slot, video_hw_frame_t *out)
{
   vaapi_vp9_decoder_t *decoder = (vaapi_vp9_decoder_t *)opaque;
   if (!decoder || decoder->closed || !out || !decoder->pool->active
         || slot >= decoder->pool->session->slots
         || vaapi_vp9_decoder_poll(decoder) < 0)
      return -1;
   if (decoder->pending.active && decoder->pending.slot != slot)
      return 1;
   if (!decoder->pending.active)
   {
      if (!(decoder->pool->ready & (UINT32_C(1) << slot)))
         return decoder->pool->transactions[slot].active ? 1 : -1;
      if (decoder->pool->exports & (UINT32_C(1) << slot))
         return 1;
      if (!vaapi_vp9_export_open(
                &decoder->pending, decoder->pool, slot, &decoder->exports))
      {
         decoder->failed = true;
         return -1;
      }
   }
   return vaapi_frame_export(decoder->frames, &decoder->pending, 0, 0,
         decoder->width, decoder->height, decoder->yuv, &decoder->color, out);
}

bool vaapi_vp9_decoder_open(vaapi_vp9_pool_t *pool, const rvp9_hdr *params,
      const video_color_metadata_t *color,
      const vaapi_vp9_export_ops_t *exports, rvp9_hw_ops_t *ops)
{
   vaapi_vp9_decoder_t *decoder;
   const vaapi_vp9_session_t *session;
   video_color_metadata_t resolved;
   if (!pool || !pool->active || pool->broken || !params || !ops || ops->decoder
         || !exports || !exports->surface || !exports->cloexec
         || !exports->close || !exports->identity
         || !rvp9_color_resolve(params, color, &resolved))
      return false;
   session = pool->session;
   if (!session || !session->ready || session->closing || session->slots < 10
         || session->slots > 32 || params->w <= 0 || params->h <= 0
         || (unsigned)params->w != session->width
         || (unsigned)params->h != session->height
         || ((params->w | params->h) & 1)
         || (unsigned)params->bit_depth != session->depth
         || params->profile != (params->bit_depth == 10 ? 2 : 0)
         || params->subsampling_x != 1 || params->subsampling_y != 1
         || params->frame_type || params->show_existing_frame)
      return false;
   decoder = (vaapi_vp9_decoder_t *)calloc(1, sizeof(*decoder));
   if (!decoder)
      return false;
   decoder->frames = vaapi_frame_pool_new(session->slots);
   if (!decoder->frames)
   {
      free(decoder);
      return false;
   }
   decoder->pool = pool;
   decoder->params = *params;
   decoder->color = resolved;
   decoder->yuv = (resolved.range == 2 ? IMAGE_YUV_FLAG_FULL_RANGE : 0)
         | (resolved.matrix == 1 ? IMAGE_YUV_FLAG_BT709 : 0);
   decoder->exports = *exports;
   decoder->width = (unsigned)params->w;
   decoder->height = (unsigned)params->h;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = decoder;
   ops->decode = vaapi_vp9_decoder_decode;
   ops->output = vaapi_vp9_decoder_output;
   return true;
}

vaapi_vp9_decoder_t *vaapi_vp9_decoder_detach(rvp9_hw_ops_t *ops)
{
   vaapi_vp9_decoder_t *decoder;
   if (!ops || ops->decode != vaapi_vp9_decoder_decode
         || ops->output != vaapi_vp9_decoder_output || !ops->decoder)
      return NULL;
   decoder = (vaapi_vp9_decoder_t *)ops->decoder;
   decoder->closed = true;
   vaapi_frame_pool_close(decoder->frames);
   memset(ops, 0, sizeof(*ops));
   return decoder;
}

bool vaapi_vp9_decoder_retire(vaapi_vp9_decoder_t *decoder)
{
   if (!decoder)
      return true;
   if (!decoder->closed)
      return false;
   vaapi_vp9_decoder_poll(decoder);
   if (decoder->pending.active && !vaapi_vp9_export_close(&decoder->pending))
      return false;
   if (!vaapi_frame_pool_free(decoder->frames))
      return false;
   free(decoder);
   return true;
}
