/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_av1_decoder.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
#include <formats/image_yuv_blit.h>
#include <stdlib.h>

struct vaapi_av1_decoder
{
   vaapi_hevc_export_t pending;
   vaapi_hevc_export_ops_t exports;
   vaapi_av1_pool_t *pool;
   vaapi_frame_pool_t *frames;
   rav1_sequence_t sequence;
   rav1_grain_t grain[32];
   video_color_metadata_t color;
   unsigned widths[32], heights[32];
   bool closed, failed;
};

static bool vaapi_av1_decoder_merge(unsigned *value, unsigned coded)
{
   if (coded == 2)
      return true;
   if (!coded || coded > 255 || (*value && *value != 2 && *value != coded))
      return false;
   *value = coded;
   return true;
}

static bool vaapi_av1_decoder_color(const rav1_sequence_t *sequence,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   unsigned range;
   if (!vaapi_av1_sequence_valid(sequence))
      return false;
   memset(out, 0, sizeof(*out));
   if (container)
      *out = *container;
   range = sequence->color_range ? 2 : 1;
   if (out->range > 2 || (out->range && out->range != range)
         || !vaapi_av1_decoder_merge(&out->primaries, sequence->color_primaries)
         || !vaapi_av1_decoder_merge(&out->transfer, sequence->transfer_characteristics)
         || !vaapi_av1_decoder_merge(&out->matrix, sequence->matrix_coefficients))
      return false;
   if (out->matrix && out->matrix != 2 && out->matrix != 1
         && out->matrix != 5 && out->matrix != 6 && out->matrix != 9)
      return false;
   out->range = range;
   return true;
}

int vaapi_av1_decoder_poll(vaapi_av1_decoder_t *decoder)
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
            int status = vaapi_av1_pool_poll(decoder->pool, i);
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

static int vaapi_av1_decoder_decode(void *opaque,
      const uint8_t *packet, size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   vaapi_av1_decoder_t *decoder = (vaapi_av1_decoder_t *)opaque;
   int result, physical;
   if (!decoder || decoder->closed || !picture
         || !sequence || memcmp(sequence, &decoder->sequence, sizeof(*sequence))
         || vaapi_av1_decoder_poll(decoder) < 0)
      return -1;
   result = vaapi_av1_pool_decode(decoder->pool, sequence, picture, packet, size);
   if (result < 0)
      decoder->failed = true;
   else if (!result)
   {
      physical = decoder->pool->mapping[picture->slot];
      /* Parser storage is reused before repeat output. */
      decoder->grain[physical] = picture->grain.params;
      physical = (int)decoder->pool->outputs[physical];
      decoder->widths[physical] = picture->setup.size.upscaled_width;
      decoder->heights[physical] = picture->setup.size.height;
   }
   return result;
}

static bool vaapi_av1_decoder_acquire(void *owner, unsigned slot, uint64_t *ticket)
{
   return vaapi_av1_pool_acquire((vaapi_av1_pool_t *)owner, slot, ticket);
}
static bool vaapi_av1_decoder_current(void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_av1_pool_current((vaapi_av1_pool_t *)owner, slot, ticket);
}
static bool vaapi_av1_decoder_release(void *owner, unsigned slot, uint64_t ticket)
{
   return vaapi_av1_pool_release((vaapi_av1_pool_t *)owner, slot, ticket);
}
static const vaapi_export_owner_ops_t vaapi_av1_decoder_ownership = {
      vaapi_av1_decoder_acquire, vaapi_av1_decoder_current,
      vaapi_av1_decoder_release};

static bool vaapi_av1_decoder_grain(const rav1_grain_t *a,
      const rav1_grain_t *b)
{
   unsigned plane, count;
   if (a->apply != b->apply)
      return false;
   if (!a->apply)
      return true;
   if (!vaapi_av1_grain_valid(a) || a->seed != b->seed ||
       a->chroma_scaling_from_luma != b->chroma_scaling_from_luma ||
       a->scaling_shift != b->scaling_shift || a->ar_coeff_lag != b->ar_coeff_lag ||
       a->ar_coeff_shift != b->ar_coeff_shift || a->scale_shift != b->scale_shift ||
       a->overlap != b->overlap ||
       a->clip_to_restricted_range != b->clip_to_restricted_range ||
       memcmp(a->num_points, b->num_points, sizeof(a->num_points)))
      return false;
   for (plane = 0; plane < 3; plane++)
   {
      if (memcmp(a->point_value[plane], b->point_value[plane], a->num_points[plane]) ||
          memcmp(a->point_scaling[plane], b->point_scaling[plane], a->num_points[plane]))
         return false;
      count = 2 * a->ar_coeff_lag * (a->ar_coeff_lag + 1) +
         (plane && a->num_points[0]);
      if ((a->num_points[plane] || (plane && a->chroma_scaling_from_luma)) &&
          memcmp(a->ar_coeffs[plane], b->ar_coeffs[plane], count))
         return false;
      if (plane && a->num_points[plane] &&
          (a->chroma_mult[plane - 1] != b->chroma_mult[plane - 1] ||
           a->chroma_luma_mult[plane - 1] != b->chroma_luma_mult[plane - 1] ||
           a->chroma_offset[plane - 1] != b->chroma_offset[plane - 1]))
         return false;
   }
   return true;
}

static int vaapi_av1_decoder_output(void *opaque, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *out)
{
   vaapi_av1_decoder_t *decoder = (vaapi_av1_decoder_t *)opaque;
   const video_color_metadata_t *color;
   unsigned physical, yuv = 0;
   int mapped;
   if (!decoder || decoder->closed || !grain || !out
         || !decoder->pool->active
         || slot >= decoder->pool->logical_slots)
      return -1;
   mapped = decoder->pool->mapping[slot];
   if (mapped < 0 || !vaapi_av1_decoder_grain(grain, &decoder->grain[mapped])
         || vaapi_av1_decoder_poll(decoder) < 0)
      return -1;
   physical = decoder->pool->outputs[mapped];
   if (decoder->pending.active && decoder->pending.slot != physical)
      return 1;
   if (!decoder->pending.active)
   {
      if (!(decoder->pool->ready & (UINT32_C(1) << physical)))
         return decoder->pool->transactions[mapped].active ? 1 : -1;
      if (!vaapi_export_open(&decoder->pending, decoder->pool,
                decoder->pool->session, &vaapi_av1_decoder_ownership,
                physical, &decoder->exports))
      {
         decoder->failed = true;
         return -1;
      }
   }
   color = &decoder->color;
   if (color->matrix == 1)
      yuv |= IMAGE_YUV_FLAG_BT709;
   if (color->range == 2)
      yuv |= IMAGE_YUV_FLAG_FULL_RANGE;
   return vaapi_frame_export_position(decoder->frames, &decoder->pending,
         0, 0, decoder->widths[physical], decoder->heights[physical], yuv,
         decoder->sequence.chroma_sample_position, color, out);
}

bool vaapi_av1_decoder_open(vaapi_av1_pool_t *pool,
      const rav1_sequence_t *sequence, const video_color_metadata_t *color,
      const vaapi_hevc_export_ops_t *exports,
      rav1_hw_ops_t *ops)
{
   vaapi_av1_decoder_t *decoder;
   video_color_metadata_t resolved;
   const vaapi_av1_session_t *session;
   if (!pool || !pool->active || pool->broken || !ops || ops->decoder
         || (sequence && sequence->film_grain_params_present && !pool->grain)
         || !exports || !exports->surface || !exports->cloexec
         || !exports->close || !exports->identity
         || !vaapi_av1_decoder_color(sequence, color, &resolved))
      return false;
   session = pool->session;
   if (!session || !session->ready || session->closing
         || sequence->max_width != session->width
         || sequence->max_height != session->height
         || sequence->bit_depth != session->depth
         || ((session->width | session->height) & 1))
      return false;
   decoder = (vaapi_av1_decoder_t *)calloc(1, sizeof(*decoder));
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
   decoder->color = resolved;
   decoder->sequence = *sequence;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = decoder;
   ops->decode = vaapi_av1_decoder_decode;
   ops->output = vaapi_av1_decoder_output;
   return true;
}

vaapi_av1_decoder_t *vaapi_av1_decoder_detach(rav1_hw_ops_t *ops)
{
   vaapi_av1_decoder_t *decoder;
   if (!ops || ops->decode != vaapi_av1_decoder_decode
         || ops->output != vaapi_av1_decoder_output || !ops->decoder)
      return NULL;
   decoder = (vaapi_av1_decoder_t *)ops->decoder;
   decoder->closed = true;
   vaapi_frame_pool_close(decoder->frames);
   memset(ops, 0, sizeof(*ops));
   return decoder;
}

bool vaapi_av1_decoder_retire(vaapi_av1_decoder_t *decoder)
{
   if (!decoder)
      return true;
   if (!decoder->closed)
      return false;
   vaapi_av1_decoder_poll(decoder);
   if (decoder->pending.active && !vaapi_hevc_export_close(&decoder->pending))
      return false;
   if (!vaapi_frame_pool_free(decoder->frames))
      return false;
   free(decoder);
   return true;
}

#endif
