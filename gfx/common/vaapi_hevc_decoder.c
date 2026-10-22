/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_decoder.h"
#include <formats/image_yuv_blit.h>
#include <stdlib.h>
#include <string.h>

struct vaapi_hevc_decoder
{
   vaapi_hevc_export_t pending;
   vaapi_hevc_export_ops_t exports;
   vaapi_hevc_pool_t *pool;
   vaapi_frame_pool_t *frames;
   rh265_video_params_t params;
   video_color_metadata_t color;
   unsigned width, height, yuv;
   bool closed, failed;
};

int vaapi_hevc_decoder_poll(vaapi_hevc_decoder_t *decoder)
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
            int status = vaapi_hevc_pool_poll(decoder->pool, i);
            if (status < 0)
            {
               decoder->failed = true;
               result          = -1;
            }
            else if (status > 0 && result > 0)
               result = 0;
         }
   if (decoder->failed)
      return -1;
   return result;
}

static int vaapi_hevc_decoder_decode(void *opaque,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, const rh265_hw_picture_t *picture)
{
   vaapi_hevc_decoder_t *decoder = (vaapi_hevc_decoder_t *)opaque;
   int result;
   if (!decoder || decoder->closed || !params
         || memcmp(params, &decoder->params, sizeof(*params))
         || vaapi_hevc_decoder_poll(decoder) < 0)
      return -1;
   result = vaapi_hevc_pool_decode(decoder->pool, params, au, picture, slices);
   if (result < 0)
      decoder->failed = true;
   return result;
}

static int vaapi_hevc_decoder_output(
      void *opaque, unsigned slot, video_hw_frame_t *out)
{
   vaapi_hevc_decoder_t *decoder = (vaapi_hevc_decoder_t *)opaque;
   if (!decoder || decoder->closed || !out || !decoder->pool->active
         || slot >= decoder->pool->session->slots
         || vaapi_hevc_decoder_poll(decoder) < 0)
      return -1;
   if (decoder->pending.active && decoder->pending.slot != slot)
      return 1;
   if (!decoder->pending.active)
   {
      if (!(decoder->pool->ready & (UINT32_C(1) << slot)))
         return decoder->pool->transactions[slot].active ? 1 : -1;
      if (decoder->pool->exports & (UINT32_C(1) << slot))
         return 1;
      if (!vaapi_hevc_export_open(
                &decoder->pending, decoder->pool, slot, &decoder->exports))
      {
         decoder->failed = true;
         return -1;
      }
   }
   return vaapi_frame_export(decoder->frames, &decoder->pending,
         (unsigned)decoder->params.crop_left,
         (unsigned)decoder->params.crop_top, decoder->width, decoder->height,
         decoder->yuv, &decoder->color, out);
}

static int vaapi_hevc_decoder_drain(void *opaque)
{
   vaapi_hevc_decoder_t *decoder = (vaapi_hevc_decoder_t *)opaque;
   if (!decoder || decoder->closed)
      return -1;
   return vaapi_hevc_decoder_poll(decoder) < 0 ? -1 : 0;
}

bool vaapi_hevc_decoder_open(vaapi_hevc_pool_t *pool,
      const rh265_video_params_t *params, const video_color_metadata_t *color,
      unsigned yuv, const vaapi_hevc_export_ops_t *exports, rh265_hw_ops_t *ops)
{
   vaapi_hevc_decoder_t *decoder;
   const vaapi_hevc_session_t *session;
   if (!pool || !pool->active || pool->broken || !params || !color || !ops
         || ops->decoder || !exports || !exports->surface || !exports->cloexec
         || !exports->close || !exports->identity
         || (yuv & ~(IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709)))
      return false;
   session = pool->session;
   if (!session || !session->ready || session->closing
         || (session->slice_mode != VA_DEC_SLICE_MODE_BASE
               && (session->slice_mode != VA_DEC_SLICE_MODE_NORMAL
                     || !pool->source || !pool->long_work))
         || !session->slots || session->slots > 32 || params->width <= 0
         || params->height <= 0 || (unsigned)params->width != session->width
         || (unsigned)params->height != session->height
         || (unsigned)params->bit_depth != session->depth
         || (params->bit_depth != 8 && params->bit_depth != 10)
         || params->chroma_format_idc != 1 || params->pcm_enabled
         || params->tiles_enabled || params->dependent_slice_segments_enabled
         || params->crop_left < 0 || params->crop_right < 0
         || params->crop_top < 0 || params->crop_bottom < 0
         || ((params->crop_left | params->crop_top) & 1)
         || params->crop_left >= params->width
         || params->crop_right >= params->width - params->crop_left
         || params->crop_top >= params->height
         || params->crop_bottom >= params->height - params->crop_top)
      return false;
   decoder = (vaapi_hevc_decoder_t *)calloc(1, sizeof(*decoder));
   if (!decoder)
      return false;
   decoder->frames = vaapi_frame_pool_new(session->slots);
   if (!decoder->frames)
   {
      free(decoder);
      return false;
   }
   decoder->pool    = pool;
   decoder->params  = *params;
   decoder->color   = *color;
   decoder->yuv     = yuv;
   decoder->exports = *exports;
   decoder->width =
         (unsigned)(params->width - params->crop_left - params->crop_right);
   decoder->height =
         (unsigned)(params->height - params->crop_top - params->crop_bottom);
   memset(ops, 0, sizeof(*ops));
   ops->decoder = decoder;
   ops->decode  = vaapi_hevc_decoder_decode;
   ops->output  = vaapi_hevc_decoder_output;
   ops->drain   = vaapi_hevc_decoder_drain;
   return true;
}

vaapi_hevc_decoder_t *vaapi_hevc_decoder_detach(rh265_hw_ops_t *ops)
{
   vaapi_hevc_decoder_t *decoder;
   if (!ops || ops->decode != vaapi_hevc_decoder_decode
         || ops->output != vaapi_hevc_decoder_output || !ops->decoder)
      return NULL;
   decoder         = (vaapi_hevc_decoder_t *)ops->decoder;
   decoder->closed = true;
   decoder->pool->source = NULL;
   vaapi_frame_pool_close(decoder->frames);
   memset(ops, 0, sizeof(*ops));
   return decoder;
}

bool vaapi_hevc_decoder_retire(vaapi_hevc_decoder_t *decoder)
{
   if (!decoder)
      return true;
   if (!decoder->closed)
      return false;
   vaapi_hevc_decoder_poll(decoder);
   if (decoder->pending.active && !vaapi_hevc_export_close(&decoder->pending))
      return false;
   if (!vaapi_frame_pool_free(decoder->frames))
      return false;
   free(decoder);
   return true;
}
