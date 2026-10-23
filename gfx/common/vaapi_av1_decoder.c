/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_av1_decoder.h"
#include <formats/image_yuv_blit.h>
#include <stdlib.h>
#include <string.h>
struct vaapi_av1_decoder
{
   vaapi_av1_export_t pending;
   vaapi_av1_export_ops_t exports;
   vaapi_av1_pool_t *pool;
   vaapi_frame_pool_t *frames;
   rav1_hw_picture_t picture;
   video_color_metadata_t container, colors[32];
   VAFilmGrainStructAV1 grains[32];
   int references[32], displays[32];
   unsigned widths[32], heights[32], yuv[32];
   bool closed, failed;
};
static bool vaapi_av1_color(const rav1_sequence_t *s,
      const video_color_metadata_t *container, video_color_metadata_t *out)
{
   video_color_metadata_t color;
   unsigned coded[3], *fields[3], i, range;
   if (!s || !out || s->color_range > 1 || !s->color_primaries
         || !s->transfer_characteristics || !s->matrix_coefficients
         || s->color_primaries > 255 || s->transfer_characteristics > 255
         || s->matrix_coefficients > 255)
      return false;
   memset(&color, 0, sizeof(color));
   if (container)
      color = *container;
   range = s->color_range ? 2 : 1;
   if (color.range > 2 || (color.range && color.range != range))
      return false;
   coded[0] = s->color_primaries;
   coded[1] = s->transfer_characteristics;
   coded[2] = s->matrix_coefficients;
   fields[0] = &color.primaries;
   fields[1] = &color.transfer;
   fields[2] = &color.matrix;
   for (i = 0; i < 3; i++)
      if (coded[i] != 2)
      {
         if (*fields[i] && *fields[i] != 2 && *fields[i] != coded[i])
            return false;
         *fields[i] = coded[i];
      }
   color.range = range;
   *out = color;
   return true;
}
int vaapi_av1_decoder_poll(vaapi_av1_decoder_t *d)
{
   unsigned i;
   int result = 1;
   if (!d)
      return -1;
   vaapi_frame_collect(d->frames);
   if (d->pending.active && !d->pending.ready
         && !vaapi_av1_export_close(&d->pending))
      result = -1;
   if (d->pool->active)
      for (i = 0; i < d->pool->session->slots; i++)
         if (d->pool->transactions[i].native.active)
         {
            int status = vaapi_av1_pool_poll(d->pool, i);
            if (status < 0)
            {
               d->failed = true;
               result = -1;
            }
            else if (status > 0 && result > 0)
               result = 0;
         }
   return d->failed ? -1 : result;
}
static int vaapi_av1_free_surface(
      uint32_t occupied, unsigned slots, int preferred)
{
   unsigned i;
   if (preferred >= 0 && (unsigned)preferred < slots
         && !(occupied & (UINT32_C(1) << preferred)))
      return preferred;
   for (i = 0; i < slots; i++)
      if (!(occupied & (UINT32_C(1) << i)))
         return (int)i;
   return -1;
}
static int vaapi_av1_decoder_decode(void *opaque, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence, const rav1_hw_picture_t *p)
{
   vaapi_av1_decoder_t *d = (vaapi_av1_decoder_t *)opaque;
   video_color_metadata_t color;
   uint32_t occupied, writes;
   unsigned i, slots, logical;
   int reference, display, result;
   if (!d || d->closed || !sequence || !p || p->header.show_existing
         || vaapi_av1_decoder_poll(d) < 0 || !d->pool->active
         || !vaapi_av1_color(sequence, &d->container, &color))
      return -1;
   slots = d->pool->session->slots;
   if (p->slot < 0 || (unsigned)p->slot >= slots
         || sequence->chroma_sample_position
         || sequence->bit_depth != d->pool->session->depth
         || sequence->max_width > d->pool->session->width
         || sequence->max_height > d->pool->session->height
         || (sequence->film_grain_params_present && slots < 18))
      return -1;
   logical = (unsigned)p->slot;
   d->picture = *p;
   occupied = vaapi_av1_pool_held(d->pool);
   for (i = 0; i < 8; i++)
   {
      int slot = p->reference_slots[i], physical, grained;
      if (slot == -1)
         continue;
      if (slot < 0 || (unsigned)slot >= slots || slot == p->slot)
         return -1;
      physical = d->references[slot];
      grained = d->displays[slot];
      if (physical < 0 || grained < 0)
         return -1;
      d->picture.reference_slots[i] = physical;
      occupied |= (UINT32_C(1) << physical) | (UINT32_C(1) << grained);
   }
   reference = vaapi_av1_free_surface(occupied, slots, d->references[logical]);
   if (reference < 0)
      return 1;
   occupied |= UINT32_C(1) << reference;
   display = p->grain.params.apply
         ? vaapi_av1_free_surface(occupied, slots, d->displays[logical])
         : reference;
   if (display < 0)
      return 1;
   d->picture.slot = reference;
   result = vaapi_av1_pool_decode(
         d->pool, packet, size, sequence, &d->picture, (unsigned)display);
   if (!result)
   {
      writes = (UINT32_C(1) << reference) | (UINT32_C(1) << display);
      for (i = 0; i < slots; i++)
         if ((d->references[i] >= 0
                   && (writes & (UINT32_C(1) << d->references[i])))
               || (d->displays[i] >= 0
                     && (writes & (UINT32_C(1) << d->displays[i]))))
            d->references[i] = d->displays[i] = -1;
      d->references[logical] = reference;
      d->displays[logical] = display;
      d->widths[logical] = p->setup.size.upscaled_width;
      d->heights[logical] = p->setup.size.height;
      d->colors[logical] = color;
      d->grains[logical] = d->pool->work.film_grain_info;
      d->yuv[logical] = (color.range == 2 ? IMAGE_YUV_FLAG_FULL_RANGE : 0)
            | (color.matrix == 1 ? IMAGE_YUV_FLAG_BT709 : 0);
   }
   else if (result < 0)
      d->failed = true;
   return result;
}
static int vaapi_av1_decoder_output(void *opaque, unsigned logical,
      const rav1_grain_t *grain, video_hw_frame_t *out)
{
   vaapi_av1_decoder_t *d = (vaapi_av1_decoder_t *)opaque;
   VAFilmGrainStructAV1 native;
   unsigned reference, display;
   if (!d || d->closed || !out || !grain || !d->pool->active
         || logical >= d->pool->session->slots || vaapi_av1_decoder_poll(d) < 0
         || d->references[logical] < 0 || d->displays[logical] < 0
         || !vaapi_av1_grain(&native, grain)
         || memcmp(&native, &d->grains[logical], sizeof(native)))
      return -1;
   reference = (unsigned)d->references[logical];
   display = (unsigned)d->displays[logical];
   if (d->pending.active && d->pending.slot != display)
      return 1;
   if (!d->pending.active)
   {
      if (!(d->pool->ready & (UINT32_C(1) << display)))
         return d->pool->transactions[reference].native.active ? 1 : -1;
      if (d->pool->exports & (UINT32_C(1) << display))
         return 1;
      if (!vaapi_av1_export_open(&d->pending, d->pool, display, &d->exports))
      {
         d->failed = true;
         return -1;
      }
   }
   return vaapi_frame_export(d->frames, &d->pending, 0, 0, d->widths[logical],
         d->heights[logical], d->yuv[logical], &d->colors[logical], out);
}
bool vaapi_av1_decoder_open(vaapi_av1_pool_t *pool,
      const rav1_sequence_t *params, const video_color_metadata_t *color,
      const vaapi_av1_export_ops_t *exports, rav1_hw_ops_t *ops)
{
   vaapi_av1_decoder_t *d;
   video_color_metadata_t resolved;
   unsigned i;
   if (!pool || !pool->active || pool->broken || !params || !ops || ops->decoder
         || ops->decode || ops->output || !exports || !exports->surface
         || !exports->cloexec || !exports->close || !exports->identity
         || !pool->session->ready || pool->session->closing || params->profile
         || params->monochrome || params->chroma_sample_position
         || params->subsampling_x != 1 || params->subsampling_y != 1
         || params->bit_depth != pool->session->depth || !params->max_width
         || !params->max_height || params->max_width > pool->session->width
         || params->max_height > pool->session->height
         || params->operating_point_count != 1
         || params->operating_points[0].idc
         || (params->film_grain_params_present && pool->session->slots < 18)
         || !vaapi_av1_color(params, color, &resolved))
      return false;
   d = (vaapi_av1_decoder_t *)calloc(1, sizeof(*d));
   if (!d)
      return false;
   d->frames = vaapi_frame_pool_new(pool->session->slots);
   if (!d->frames)
   {
      free(d);
      return false;
   }
   d->pool = pool;
   d->exports = *exports;
   if (color)
      d->container = *color;
   for (i = 0; i < 32; i++)
      d->references[i] = d->displays[i] = -1;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = d;
   ops->decode = vaapi_av1_decoder_decode;
   ops->output = vaapi_av1_decoder_output;
   return true;
}
vaapi_av1_decoder_t *vaapi_av1_decoder_detach(rav1_hw_ops_t *ops)
{
   vaapi_av1_decoder_t *d;
   if (!ops || !ops->decoder || ops->decode != vaapi_av1_decoder_decode
         || ops->output != vaapi_av1_decoder_output)
      return NULL;
   d = (vaapi_av1_decoder_t *)ops->decoder;
   d->closed = true;
   vaapi_frame_pool_close(d->frames);
   memset(ops, 0, sizeof(*ops));
   return d;
}
bool vaapi_av1_decoder_retire(vaapi_av1_decoder_t *d)
{
   if (!d)
      return true;
   if (!d->closed)
      return false;
   vaapi_av1_decoder_poll(d);
   if (d->pending.active && !vaapi_av1_export_close(&d->pending))
      return false;
   if (!vaapi_frame_pool_free(d->frames))
      return false;
   free(d);
   return true;
}
