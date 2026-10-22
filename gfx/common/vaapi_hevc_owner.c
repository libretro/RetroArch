/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_owner.h"
#include <stdlib.h>
#include <string.h>

struct vaapi_hevc_owner
{
   vaapi_hevc_runtime_t runtime;
   vaapi_hevc_pool_t pool;
   rh265_hw_ops_t decoder;
   vaapi_hevc_decoder_t *retired;
   vaapi_hevc_short_work_t *work;
   VADisplay terminated_display;
   bool published, closing, terminating;
};

static int vaapi_hevc_owner_decode(void *opaque,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, const rh265_hw_picture_t *picture)
{
   vaapi_hevc_owner_t *owner = (vaapi_hevc_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.decode)
      return -1;
   return owner->decoder.decode(
         owner->decoder.decoder, params, au, slices, picture);
}

static int vaapi_hevc_owner_output(
      void *opaque, unsigned slot, video_hw_frame_t *frame)
{
   vaapi_hevc_owner_t *owner = (vaapi_hevc_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.output)
      return -1;
   return owner->decoder.output(owner->decoder.decoder, slot, frame);
}

static int vaapi_hevc_owner_drain(void *opaque)
{
   vaapi_hevc_owner_t *owner = (vaapi_hevc_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.drain)
      return -1;
   return owner->decoder.drain(owner->decoder.decoder);
}

bool vaapi_hevc_owner_open_with_io(vaapi_hevc_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, const vaapi_hevc_runtime_io_t *io,
      const vaapi_hevc_export_ops_t *exports, rh265_hw_ops_t *ops)
{
   vaapi_hevc_owner_t *owner;
   vaapi_hevc_export_ops_t native_exports;
   bool opened;
   if (!out || *out || !ops || ops->decoder || !path || !*path || !params
         || !color || yuv > 2 || slots < 2 || slots > 32 || max_input < 128
         || max_input > 64u * 1024u * 1024u || params->width < 8
         || params->height < 8 || params->width > 65535
         || params->height > 65535 || ((params->width | params->height) & 1)
         || (params->bit_depth != 8 && params->bit_depth != 10)
         || params->chroma_format_idc != 1 || params->pcm_enabled
         || params->tiles_enabled || params->dependent_slice_segments_enabled)
      return false;
   owner = (vaapi_hevc_owner_t *)calloc(1, sizeof(*owner));
   if (!owner)
      return false;
   *out        = owner;
   owner->work = (vaapi_hevc_short_work_t *)calloc(1, sizeof(*owner->work));
   if (!owner->work)
      return false;
   owner->work->data = (uint8_t *)malloc(max_input);
   if (!owner->work->data)
      return false;
   owner->work->capacity = max_input;
   if (io)
      opened = vaapi_hevc_runtime_open_with_io(&owner->runtime, path, major_id,
            minor_id, (unsigned)params->width, (unsigned)params->height,
            (unsigned)params->bit_depth, slots, VA_DEC_SLICE_MODE_BASE, io);
   else
      opened = vaapi_hevc_runtime_open(&owner->runtime, path, major_id,
            minor_id, (unsigned)params->width, (unsigned)params->height,
            (unsigned)params->bit_depth, slots, VA_DEC_SLICE_MODE_BASE);
   if (!opened)
      return false;
   if (!vaapi_hevc_pool_init(&owner->pool, &owner->runtime.session,
             &owner->runtime.submit, owner->work))
      return false;
   if (exports)
      native_exports = *exports;
   else if (!vaapi_hevc_export_bind(&native_exports, &owner->runtime))
      return false;
   if (!vaapi_hevc_decoder_open(&owner->pool, params, color, yuv,
             &native_exports, &owner->decoder))
      return false;
   memset(ops, 0, sizeof(*ops));
   ops->decoder     = owner;
   ops->decode      = vaapi_hevc_owner_decode;
   ops->output      = vaapi_hevc_owner_output;
   ops->drain       = vaapi_hevc_owner_drain;
   owner->published = true;
   return true;
}

bool vaapi_hevc_owner_open(vaapi_hevc_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rh265_video_params_t *params,
      const video_color_metadata_t *color, unsigned yuv, unsigned slots,
      size_t max_input, rh265_hw_ops_t *ops)
{
   return vaapi_hevc_owner_open_with_io(out, path, major_id, minor_id, params,
         color, yuv, slots, max_input, NULL, NULL, ops);
}

vaapi_hevc_owner_t *vaapi_hevc_owner_detach(rh265_hw_ops_t *ops)
{
   vaapi_hevc_owner_t *owner;
   if (!ops || ops->decode != vaapi_hevc_owner_decode
         || ops->output != vaapi_hevc_owner_output || !ops->decoder)
      return NULL;
   owner          = (vaapi_hevc_owner_t *)ops->decoder;
   owner->closing = true;
   owner->retired = vaapi_hevc_decoder_detach(&owner->decoder);
   memset(ops, 0, sizeof(*ops));
   return owner;
}

bool vaapi_hevc_owner_retire(vaapi_hevc_owner_t *owner)
{
   if (!owner)
      return true;
   if (owner->published && !owner->closing)
      return false;
   owner->closing = true;
   if (!owner->terminating)
   {
      unsigned i;
      bool polled = false;
      if (owner->decoder.decoder)
         owner->retired = vaapi_hevc_decoder_detach(&owner->decoder);
      if (owner->retired)
      {
         if (!vaapi_hevc_decoder_retire(owner->retired))
            return false;
         owner->retired = NULL;
         polled         = true;
      }
      if (owner->pool.exports)
         return false;
      if (owner->pool.active && !owner->pool.broken)
      {
         bool pending = false;
         for (i = 0; i < owner->pool.session->slots; i++)
            if (owner->pool.transactions[i].active)
            {
               int status = polled ? 1 : vaapi_hevc_pool_poll(&owner->pool, i);
               if (status > 0)
                  pending = true;
               else if (status < 0)
                  owner->pool.broken = true;
            }
         if (!owner->pool.broken
               && (pending || !vaapi_hevc_pool_close(&owner->pool)))
            return false;
      }
      owner->terminated_display = owner->runtime.display;
      owner->terminating        = true;
   }
   if (!vaapi_hevc_runtime_close(&owner->runtime))
      return false;
   if (owner->pool.active
         && !vaapi_hevc_pool_terminated(
               &owner->pool, owner->terminated_display))
      return false;
   if (owner->work)
   {
      free(owner->work->data);
      free(owner->work);
   }
   free(owner);
   return true;
}
