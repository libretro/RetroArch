/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_vp9_owner.h"
#include "vaapi_vp9_runtime.h"
#include <formats/rvp9_color.h>
#include <stdlib.h>
#include <string.h>

struct vaapi_vp9_owner
{
   vaapi_vp9_runtime_t runtime;
   vaapi_vp9_pool_t pool;
   rvp9_hw_ops_t decoder;
   vaapi_vp9_decoder_t *retired;
   VADisplay terminated_display;
   bool published, closing, terminating;
};

static int vaapi_vp9_owner_decode(void *opaque, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   vaapi_vp9_owner_t *owner = (vaapi_vp9_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.decode)
      return -1;
   return owner->decoder.decode(owner->decoder.decoder, packet, size, picture);
}

static int vaapi_vp9_owner_output(
      void *opaque, unsigned slot, video_hw_frame_t *frame)
{
   vaapi_vp9_owner_t *owner = (vaapi_vp9_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.output)
      return -1;
   return owner->decoder.output(owner->decoder.decoder, slot, frame);
}

bool vaapi_vp9_owner_open_with_io(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots,
      const vaapi_vp9_runtime_io_t *io, const vaapi_vp9_export_ops_t *exports,
      rvp9_hw_ops_t *ops)
{
   vaapi_vp9_owner_t *owner;
   vaapi_vp9_export_ops_t native_exports;
   video_color_metadata_t resolved;
   bool opened;
   if (!out || *out || !ops || ops->decoder || !path || !*path || !header
         || slots < 10 || slots > 32 || header->w <= 0 || header->h <= 0
         || header->w > 65535 || header->h > 65535
         || ((header->w | header->h) & 1) || header->frame_type
         || header->show_existing_frame
         || !rvp9_color_resolve(header, color, &resolved))
      return false;
   owner = (vaapi_vp9_owner_t *)calloc(1, sizeof(*owner));
   if (!owner)
      return false;
   *out = owner;
   if (io)
      opened = vaapi_vp9_runtime_open_with_io(&owner->runtime, path, major_id,
            minor_id, (unsigned)header->w, (unsigned)header->h,
            (unsigned)header->bit_depth, slots, io);
   else
      opened = vaapi_vp9_runtime_open(&owner->runtime, path, major_id, minor_id,
            (unsigned)header->w, (unsigned)header->h,
            (unsigned)header->bit_depth, slots);
   if (!opened
         || !vaapi_vp9_pool_init(
               &owner->pool, &owner->runtime.session, &owner->runtime.submit))
      return false;
   if (exports)
      native_exports = *exports;
   else if (!vaapi_hevc_export_bind(&native_exports, &owner->runtime))
      return false;
   if (!vaapi_vp9_decoder_open(
             &owner->pool, header, &resolved, &native_exports, &owner->decoder))
      return false;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = owner;
   ops->decode = vaapi_vp9_owner_decode;
   ops->output = vaapi_vp9_owner_output;
   owner->published = true;
   return true;
}

bool vaapi_vp9_owner_open(vaapi_vp9_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops)
{
   return vaapi_vp9_owner_open_with_io(
         out, path, major_id, minor_id, header, color, slots, NULL, NULL, ops);
}

vaapi_vp9_owner_t *vaapi_vp9_owner_detach(rvp9_hw_ops_t *ops)
{
   vaapi_vp9_owner_t *owner;
   if (!ops || ops->decode != vaapi_vp9_owner_decode
         || ops->output != vaapi_vp9_owner_output || !ops->decoder)
      return NULL;
   owner = (vaapi_vp9_owner_t *)ops->decoder;
   owner->closing = true;
   owner->retired = vaapi_vp9_decoder_detach(&owner->decoder);
   memset(ops, 0, sizeof(*ops));
   return owner;
}

bool vaapi_vp9_owner_retire(vaapi_vp9_owner_t *owner)
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
         owner->retired = vaapi_vp9_decoder_detach(&owner->decoder);
      if (owner->retired)
      {
         if (!vaapi_vp9_decoder_retire(owner->retired))
            return false;
         owner->retired = NULL;
         polled = true;
      }
      if (owner->pool.exports)
         return false;
      if (owner->pool.active && !owner->pool.broken)
      {
         bool pending = false;
         for (i = 0; i < owner->pool.session->slots; i++)
            if (owner->pool.transactions[i].active)
            {
               int status = polled ? 1 : vaapi_vp9_pool_poll(&owner->pool, i);
               if (status > 0)
                  pending = true;
               else if (status < 0)
                  owner->pool.broken = true;
            }
         if (!owner->pool.broken
               && (pending || !vaapi_vp9_pool_close(&owner->pool)))
            return false;
      }
      owner->terminated_display = owner->runtime.display;
      owner->terminating = true;
   }
   if (!vaapi_vp9_runtime_close(&owner->runtime))
      return false;
   if (owner->pool.active
         && !vaapi_vp9_pool_terminated(&owner->pool, owner->terminated_display))
      return false;
   free(owner);
   return true;
}
