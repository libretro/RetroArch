/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_vp9_decoder.h"
#include <formats/rvp9_color.h>
#include <formats/image_yuv_blit.h>
#include <stdlib.h>

struct vaapi_vp9_decoder
{
   vaapi_hevc_export_t pending;
   vaapi_hevc_export_ops_t exports;
   vaapi_vp9_pool_t *pool;
   vaapi_frame_pool_t *frames;
   video_color_metadata_t container, colors[32];
   unsigned width, height, chroma_position;
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
         && !vaapi_hevc_export_close(&decoder->pending))
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
   return decoder->failed ? -1 : result;
}

static int vaapi_vp9_decoder_decode(void *opaque,
      const uint8_t *packet, size_t size, const rvp9_hw_picture_t *picture)
{
   vaapi_vp9_decoder_t *decoder = (vaapi_vp9_decoder_t *)opaque;
   video_color_metadata_t color;
   int result, physical;
   if (!decoder || decoder->closed || !picture
         || !rvp9_color_resolve(&picture->header, &decoder->container, &color)
         || vaapi_vp9_decoder_poll(decoder) < 0)
      return -1;
   result = vaapi_vp9_pool_decode(decoder->pool, picture, packet, size);
   if (result < 0)
      decoder->failed = true;
   else if (!result)
   {
      physical = decoder->pool->mapping[picture->slot];
      decoder->colors[physical] = color;
   }
   return result;
}

static bool vaapi_vp9_decoder_acquire(void *owner, unsigned slot, uint64_t *ticket)
{
   return vaapi_vp9_pool_acquire((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static bool vaapi_vp9_decoder_current(void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_vp9_pool_current((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static bool vaapi_vp9_decoder_release(void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_vp9_pool_release((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static const vaapi_export_owner_ops_t vaapi_vp9_decoder_ownership = {
      vaapi_vp9_decoder_acquire, vaapi_vp9_decoder_current,
      vaapi_vp9_decoder_release};

static int vaapi_vp9_decoder_output(void *opaque, unsigned slot,
      video_hw_frame_t *out)
{
   vaapi_vp9_decoder_t *decoder = (vaapi_vp9_decoder_t *)opaque;
   const video_color_metadata_t *color;
   unsigned physical, yuv = 0;
   int mapped;
   if (!decoder || decoder->closed || !out || !decoder->pool->active
         || slot >= decoder->pool->logical_slots
         || vaapi_vp9_decoder_poll(decoder) < 0)
      return -1;
   mapped = decoder->pool->mapping[slot];
   if (mapped < 0)
      return -1;
   physical = (unsigned)mapped;
   if (decoder->pending.active && decoder->pending.slot != physical)
      return 1;
   if (!decoder->pending.active)
   {
      if (!(decoder->pool->ready & (UINT32_C(1) << physical)))
         return decoder->pool->transactions[physical].active ? 1 : -1;
      if (!vaapi_export_open(&decoder->pending, decoder->pool,
                decoder->pool->session, &vaapi_vp9_decoder_ownership,
                physical, &decoder->exports))
      {
         decoder->failed = true;
         return -1;
      }
   }
   color = &decoder->colors[physical];
   if (color->matrix == 1)
      yuv |= IMAGE_YUV_FLAG_BT709;
   if (color->range == 2)
      yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   return vaapi_frame_export_position(decoder->frames, &decoder->pending,
         0, 0, decoder->width, decoder->height, yuv,
         decoder->chroma_position, color, out);
}

bool vaapi_vp9_decoder_open_position(vaapi_vp9_pool_t *pool,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      unsigned chroma_position, const vaapi_hevc_export_ops_t *exports,
      rvp9_hw_ops_t *ops)
{
   vaapi_vp9_decoder_t *decoder;
   video_color_metadata_t resolved;
   const vaapi_vp9_session_t *session;
   if (!pool || !pool->active || pool->broken || !ops || ops->decoder
         || chroma_position < 1 || chroma_position > 2
         || !exports || !exports->surface || !exports->cloexec
         || !exports->close || !exports->identity
         || !rvp9_color_resolve(header, color, &resolved))
      return false;
   session = pool->session;
   if (!session || !session->ready || session->closing || header->w <= 0
         || header->h <= 0 || (unsigned)header->w != session->width
         || (unsigned)header->h != session->height
         || (unsigned)header->bit_depth != session->depth
         || ((header->w | header->h) & 1))
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
   decoder->exports = *exports;
   if (color)
      decoder->container = *color;
   decoder->width = session->width;
   decoder->height = session->height;
   decoder->chroma_position = chroma_position;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = decoder;
   ops->decode = vaapi_vp9_decoder_decode;
   ops->output = vaapi_vp9_decoder_output;
   return true;
}

bool vaapi_vp9_decoder_open(vaapi_vp9_pool_t *pool,
      const rvp9_hdr *header, const video_color_metadata_t *color,
      const vaapi_hevc_export_ops_t *exports, rvp9_hw_ops_t *ops)
{
   return vaapi_vp9_decoder_open_position(pool, header, color, 2, exports, ops);
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
   if (decoder->pending.active && !vaapi_hevc_export_close(&decoder->pending))
      return false;
   if (!vaapi_frame_pool_free(decoder->frames))
      return false;
   free(decoder);
   return true;
}
