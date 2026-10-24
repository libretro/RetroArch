/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_HW_STREAM_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_HW_STREAM_H__

#include <formats/rav1_hw.h>
#include <formats/rav1_hdr.h>
#include <stdlib.h>

enum rav1_hw_stream_result
{
   RAV1_HW_STREAM_END = 0,
   RAV1_HW_STREAM_FRAME = 1,
   RAV1_HW_STREAM_PROGRESS = 2,
   RAV1_HW_STREAM_BUSY = 3,
   RAV1_HW_STREAM_DISCARDED = 4,
   /* Visible input accepted; retain its presentation info until FRAME. */
   RAV1_HW_STREAM_PENDING = 5
};

/* Persistent caller-owned storage, confined to the decoder thread. */
typedef struct rav1_hw_stream
{
   rav1_hw_session_t session;
   rav1_hw_backend_t backend;
   /* Cold startup admission only; the callback borrows the sequence view.
    * Retain owned configuration for later seeks, or fail the admission. */
   bool (*retain_sequence)(void *user, const rav1_sequence_t *sequence,
         const rav1_obu_t *obu);
   void *sequence_user;
   rav1_obu_t obu, sequence_obu;
   const uint8_t *packet;
   size_t size, offset, next;
   rav1_sequence_t sequence;
   rav1_hdr_metadata_t hdr, output_hdr;
   video_color_metadata_t color;
   int opened, submitted, started, error, ending;
} rav1_hw_stream_t;

/* Cold configuration storage. Do not retain packet views after admission. */
static INLINE bool rav1_hw_stream_retain_config(uint8_t **bytes,
      size_t *size, const rav1_obu_t *obu)
{
   uint8_t *copy;
   if (!bytes || !size || !obu || obu->type != 1 || !obu->data || !obu->size
         || obu->size > 1024u * 1024u || obu->temporal_id || obu->spatial_id)
      return false;
   if (*bytes && *size == obu->size && !memcmp(*bytes, obu->data, *size))
      return true;
   copy = (uint8_t*)malloc(obu->size);
   if (!copy)
      return false;
   memcpy(copy, obu->data, obu->size);
   free(*bytes);
   *bytes = copy;
   *size = obu->size;
   return true;
}

static INLINE bool rav1_hw_stream_open_sequence(rav1_hw_stream_t *s)
{
   rav1_hw_ops_t ops;
   if (s->opened)
   {
      const rav1_sequence_t *old = &s->session.sequence;
      const rav1_sequence_t *seq = &s->sequence;
      if (!memcmp(seq, old, sizeof(*seq)))
      {
         memset(&s->sequence_obu, 0, sizeof(s->sequence_obu));
         return true;
      }
      /* av1C is provisional until the first frame, but its output
       * geometry and color have already been admitted by the caller. */
      if (s->started
          || seq->operating_points[0].level != old->operating_points[0].level
          || seq->operating_points[0].tier != old->operating_points[0].tier
          || seq->max_width != old->max_width ||
          seq->max_height != old->max_height || seq->profile != old->profile ||
          seq->bit_depth != old->bit_depth || seq->monochrome != old->monochrome ||
          seq->subsampling_x != old->subsampling_x ||
          seq->subsampling_y != old->subsampling_y ||
          seq->chroma_sample_position != old->chroma_sample_position ||
          seq->color_range != old->color_range ||
          seq->color_primaries != old->color_primaries ||
          seq->transfer_characteristics != old->transfer_characteristics ||
          seq->matrix_coefficients != old->matrix_coefficients)
         return false;
      s->backend.close(&s->session.ops);
      s->opened = 0;
   }
   if (!rav1_hw_backend_open_config(&s->sequence,
            s->sequence_obu.data ? &s->sequence_obu : NULL, &s->color,
            9, &ops, &s->backend))
      return false;
   if (!rav1_hw_session_init_mode(&s->session, &s->sequence, &ops, 9,
            s->backend.packet_decoder))
   {
      s->backend.close(&ops);
      memset(&s->backend, 0, sizeof(s->backend));
      return false;
   }
   if (s->retain_sequence
         && !s->retain_sequence(s->sequence_user, &s->sequence, &s->sequence_obu))
   {
      s->backend.close(&s->session.ops);
      memset(&s->backend, 0, sizeof(s->backend));
      memset(&s->sequence_obu, 0, sizeof(s->sequence_obu));
      return false;
   }
   memset(&s->sequence_obu, 0, sizeof(s->sequence_obu));
   s->opened = 1;
   return true;
}

/* Initialize fresh storage; NULL sequence defers opening until an in-band
 * sequence header. Supplied sequences must have passed rav1_sequence_parse. */
static INLINE bool rav1_hw_stream_init(rav1_hw_stream_t *s,
                                       const rav1_sequence_t *sequence)
{
   if (!s || sequence == &s->sequence || sequence == &s->session.sequence)
      return false;
   memset(s, 0, sizeof(*s));
   if (!sequence)
      return true;
   s->sequence = *sequence;
   if (rav1_hw_stream_open_sequence(s))
      return true;
   s->error = -2;
   return false;
}

/* Exported leases remain owned by their consumer after stream retirement. */
static INLINE void rav1_hw_stream_close(rav1_hw_stream_t *s)
{
   if (!s)
      return;
   if (s->opened)
   {
      rav1_hw_session_cancel(&s->session);
      s->backend.close(&s->session.ops);
   }
   memset(s, 0, sizeof(*s));
}

/* Raw backends accept a new reference chain without retiring GPU leases.
 * Opaque packet decoders require a platform flush instead. */
static INLINE bool rav1_hw_stream_restart(rav1_hw_stream_t *s)
{
   rav1_hw_ops_t ops;
   unsigned slots;
   if (!s || !s->opened || s->error || s->session.broken
         || s->backend.packet_decoder)
      return false;
   ops = s->session.ops;
   slots = s->session.slots;
   rav1_hw_session_cancel(&s->session);
   if (!rav1_hw_session_init_mode(&s->session, &s->sequence, &ops, slots, false))
   {
      s->session.ops = ops;
      s->error = -1;
      return false;
   }
   s->packet = NULL;
   s->size = s->offset = s->next = 0;
   s->submitted = s->started = s->ending = 0;
   memset(&s->obu, 0, sizeof(s->obu));
   memset(&s->hdr, 0, sizeof(s->hdr));
   memset(&s->output_hdr, 0, sizeof(s->output_hdr));
   return true;
}

/* 0 borrowed, 1 previous packet active, negative invalid/unsupported.
 * Keep input immutable/resident until END, error or close. No packet copy. */
static INLINE int rav1_hw_stream_begin(rav1_hw_stream_t *s,
                                       const uint8_t *packet, size_t size)
{
   if (!s || s->error || s->ending || !packet || !size)
      return s && s->error ? s->error : -1;
   if (s->packet)
      return 1;
   s->packet = packet;
   s->size = size;
   s->offset = s->next = 0;
   s->submitted = 0;
   memset(&s->obu, 0, sizeof(s->obu));
   return 0;
}

/* Export only the retained display; accepted input can already be retired. */
static INLINE int rav1_hw_stream_output(rav1_hw_stream_t *s,
      video_hw_frame_t *out, bool discard)
{
   int ret;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->error)
      return s ? s->error : -1;
   if (s->session.output_slot < 0)
      return RAV1_HW_STREAM_END;
   if (discard)
   {
      rav1_hw_session_discard_output(&s->session);
      return RAV1_HW_STREAM_DISCARDED;
   }
   ret = rav1_hw_session_output(&s->session, out);
   if (ret > 0)
      return RAV1_HW_STREAM_BUSY;
   if (ret < 0)
   {
      rav1_hw_session_cancel(&s->session);
      s->packet = NULL;
      memset(&s->obu, 0, sizeof(s->obu));
      s->error = ret;
      return ret;
   }
   rav1_hdr_metadata_apply(&out->color, &s->output_hdr);
   return RAV1_HW_STREAM_FRAME;
}

/* Terminal input only, after packet END. One drain and at most one export;
 * pending native delivery returns BUSY without waiting. */
static INLINE int rav1_hw_stream_finish(rav1_hw_stream_t *s,
      video_hw_frame_t *out, bool discard)
{
   int ret, output;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->error || s->packet)
      return s && s->error ? s->error : -1;
   s->ending = 1;
   if (!s->opened || !s->backend.drain)
      return RAV1_HW_STREAM_END;
   ret = s->backend.drain(s->session.ops.decoder, discard);
   if (ret < 0)
   {
      rav1_hw_session_cancel(&s->session);
      s->packet = NULL;
      memset(&s->obu, 0, sizeof(s->obu));
      s->error = ret;
      return ret;
   }
   output = rav1_hw_stream_output(s, out, discard);
   if (output != RAV1_HW_STREAM_END)
      return output;
   return ret ? RAV1_HW_STREAM_BUSY : RAV1_HW_STREAM_END;
}

/* Release the previous out before stepping again. One OBU or retained display
 * per call; BUSY retries without advancing input. Packet/drain mode reports
 * PENDING after visible acceptance. END releases only the current input span;
 * finish is required at actual container EOF.
 * FRAME transfers a GPU lease; DISCARDED consumes display without export.
 * Sequence changes after frame acceptance and split FRAME_HEADER/TILE_GROUP
 * coding are unsupported.
 * Decode errors are terminal until close/reinit; accepted input is owned
 * by the backend and unaccepted borrowed input is cancelled. */
static INLINE int rav1_hw_stream_step(rav1_hw_stream_t *s,
                                      video_hw_frame_t *out, bool discard)
{
   int ret, previous, result = RAV1_HW_STREAM_PROGRESS;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->error)
      return s ? s->error : -1;
   if (!s->packet)
      return RAV1_HW_STREAM_END;
   if (s->offset == s->size)
   {
      if (s->backend.drain && s->session.output_slot >= 0)
      {
         ret = rav1_hw_stream_output(s, out, discard);
         if (ret != RAV1_HW_STREAM_BUSY)
            return ret;
      }
      s->packet = NULL;
      s->size = s->offset = s->next = 0;
      return RAV1_HW_STREAM_END;
   }
   if (!s->obu.data)
   {
      s->next = s->offset;
      ret = rav1_obu_next(&s->obu, s->packet, s->size, &s->next, true);
      if (ret != 1)
      {
         ret = -1;
         goto fail;
      }
      if (s->obu.temporal_id || s->obu.spatial_id)
      {
         ret = -2;
         goto fail;
      }
   }
   switch (s->obu.type)
   {
   case 1:
      if (!rav1_sequence_parse(&s->sequence, s->obu.payload,
                               s->obu.payload_size))
      {
         ret = -1;
         goto fail;
      }
      s->sequence_obu = s->obu;
      if (!rav1_hw_stream_open_sequence(s))
      {
         ret = -2;
         goto fail;
      }
      break;
   case 2:
      if (s->obu.payload_size)
      {
         ret = -1;
         goto fail;
      }
      break;
   case 5:
      if (rav1_hdr_metadata_obu(&s->hdr, &s->obu) < 0)
      {
         ret = -1;
         goto fail;
      }
      break;
   case 15:
      break;
   case 3:
   case 6:
      if (!s->opened)
      {
         ret = -2;
         goto fail;
      }
      previous = s->session.output_slot;
      if (!s->submitted)
      {
         ret = rav1_hw_session_submit(&s->session, s->obu.data, s->obu.size);
         if (ret > 0)
         {
            if (s->backend.drain && previous >= 0)
               return rav1_hw_stream_output(s, out, discard);
            return RAV1_HW_STREAM_BUSY;
         }
         if (ret < 0)
            goto fail;
         s->submitted = s->started = 1;
         if (previous < 0 && s->session.output_slot >= 0)
            s->output_hdr = s->hdr;
      }
      if (s->backend.drain)
      {
         if (previous < 0 && s->session.output_slot >= 0)
            result = discard ? rav1_hw_stream_output(s, out, true)
                             : RAV1_HW_STREAM_PENDING;
      }
      else if (s->session.output_slot >= 0)
      {
         result = rav1_hw_stream_output(s, out, discard);
         if (result == RAV1_HW_STREAM_BUSY)
            return result;
         if (result < 0)
         {
            ret = result;
            goto fail;
         }
      }
      break;
   default:
      ret = -2;
      goto fail;
   }
   s->offset = s->next;
   s->submitted = 0;
   memset(&s->obu, 0, sizeof(s->obu));
   return result;
fail:
   if (s->opened)
      rav1_hw_session_cancel(&s->session);
   s->packet = NULL;
   memset(&s->obu, 0, sizeof(s->obu));
   s->error = ret;
   return ret;
}

#endif
