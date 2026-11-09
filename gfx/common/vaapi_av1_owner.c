/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vaapi_av1_owner.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
#include <stdlib.h>

struct vaapi_av1_owner
{
   vaapi_av1_runtime_t runtime;
   vaapi_av1_pool_t pool;
   rav1_hw_ops_t decoder;
   vaapi_av1_decoder_t *retired;
   VADisplay terminated_display;
   size_t max_input;
   bool published, closing, terminating;
};

static int vaapi_av1_owner_decode(void *opaque, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   vaapi_av1_owner_t *owner = (vaapi_av1_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.decode
         || size > owner->max_input)
      return -1;
   return owner->decoder.decode(owner->decoder.decoder, packet, size, sequence, picture);
}

static int vaapi_av1_owner_output(void *opaque, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *frame)
{
   vaapi_av1_owner_t *owner = (vaapi_av1_owner_t *)opaque;
   if (!owner || owner->closing || !owner->decoder.output)
      return -1;
   return owner->decoder.output(owner->decoder.decoder, slot, grain, frame);
}

bool vaapi_av1_owner_open_with_io(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color,
      unsigned slots, size_t max_input,
      const vaapi_av1_runtime_io_t *io, const vaapi_hevc_export_ops_t *exports,
      rav1_hw_ops_t *ops)
{
   vaapi_av1_owner_t *owner;
   vaapi_hevc_export_ops_t native_exports;
   unsigned physical;
   bool opened;
   if (!out || *out || !ops || ops->decoder || !path || !*path
         || slots < 9 || slots > 32 || max_input < 128
         || max_input > 64u * 1024u * 1024u
         || !vaapi_av1_sequence_valid(sequence)
         || sequence->max_width > 65535 || sequence->max_height > 65535
         || ((sequence->max_width | sequence->max_height) & 1))
      return false;
   owner = (vaapi_av1_owner_t *)calloc(1, sizeof(*owner));
   if (!owner)
      return false;
   *out = owner;
   owner->max_input = max_input;
   physical = slots > 29 ? 32 : slots + 3;
   /* Eight reference pairs, a target pair and three retained displays. */
   if (sequence->film_grain_params_present && physical < 21)
      physical = 21;
   if (io)
      opened = vaapi_av1_runtime_open_with_io(&owner->runtime, path, major_id,
            minor_id, sequence->max_width, sequence->max_height,
            sequence->bit_depth, physical, io);
   else
      opened = vaapi_av1_runtime_open(&owner->runtime, path, major_id, minor_id,
            sequence->max_width, sequence->max_height,
            sequence->bit_depth, physical);
   if (!opened || !(sequence->film_grain_params_present
         ? vaapi_av1_pool_init_grain(&owner->pool, &owner->runtime.session,
               &owner->runtime.submit, slots)
         : vaapi_av1_pool_init(&owner->pool, &owner->runtime.session,
               &owner->runtime.submit, slots)))
      return false;
   if (exports)
      native_exports = *exports;
   else if (!vaapi_hevc_export_bind(&native_exports, &owner->runtime))
      return false;
   if (!vaapi_av1_decoder_open(&owner->pool, sequence, color,
             &native_exports, &owner->decoder))
      return false;
   memset(ops, 0, sizeof(*ops));
   ops->decoder = owner;
   ops->decode = vaapi_av1_owner_decode;
   ops->output = vaapi_av1_owner_output;
   owner->published = true;
   return true;
}

bool vaapi_av1_owner_open(vaapi_av1_owner_t **out, const char *path,
      unsigned major_id, unsigned minor_id, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color,
      unsigned slots, size_t max_input,
      rav1_hw_ops_t *ops)
{
   return vaapi_av1_owner_open_with_io(out, path, major_id, minor_id, sequence,
         color, slots, max_input, NULL, NULL, ops);
}

vaapi_av1_owner_t *vaapi_av1_owner_detach(rav1_hw_ops_t *ops)
{
   vaapi_av1_owner_t *owner;
   if (!ops || ops->decode != vaapi_av1_owner_decode
         || ops->output != vaapi_av1_owner_output || !ops->decoder)
      return NULL;
   owner = (vaapi_av1_owner_t *)ops->decoder;
   owner->closing = true;
   owner->retired = vaapi_av1_decoder_detach(&owner->decoder);
   memset(ops, 0, sizeof(*ops));
   return owner;
}

bool vaapi_av1_owner_retire(vaapi_av1_owner_t *owner)
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
         owner->retired = vaapi_av1_decoder_detach(&owner->decoder);
      if (owner->retired)
      {
         if (!vaapi_av1_decoder_retire(owner->retired))
            return false;
         owner->retired = NULL;
         polled = true;
      }
      if (owner->pool.active)
      {
         bool pending = false;
         for (i = 0; i < owner->pool.session->slots; i++)
         {
            if (owner->pool.exports[i])
               return false;
            if (!owner->pool.broken && owner->pool.transactions[i].active)
            {
               int status = polled ? 1 : vaapi_av1_pool_poll(&owner->pool, i);
               if (status > 0)
                  pending = true;
               else if (status < 0)
                  owner->pool.broken = true;
            }
         }
         if (!owner->pool.broken
               && (pending || !vaapi_av1_pool_close(&owner->pool)))
            return false;
      }
      owner->terminated_display = owner->runtime.display;
      owner->terminating = true;
   }
   if (!vaapi_av1_runtime_close(&owner->runtime))
      return false;
   if (owner->pool.active && !vaapi_av1_pool_terminated(
             &owner->pool, owner->terminated_display))
      return false;
   free(owner);
   return true;
}

#endif
