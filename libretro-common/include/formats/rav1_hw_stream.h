/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_HW_STREAM_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_HW_STREAM_H__

#include <formats/rav1_hw.h>

enum rav1_hw_stream_result
{
   RAV1_HW_STREAM_END = 0,
   RAV1_HW_STREAM_FRAME = 1,
   RAV1_HW_STREAM_PROGRESS = 2,
   RAV1_HW_STREAM_BUSY = 3,
   RAV1_HW_STREAM_DISCARDED = 4
};

/* Persistent caller-owned storage, confined to the decoder thread. */
typedef struct rav1_hw_stream
{
   rav1_hw_session_t session;
   rav1_hw_backend_t backend;
   rav1_obu_t obu;
   const uint8_t *packet;
   size_t size, offset, next;
   rav1_sequence_t sequence;
   int opened, submitted, started, error;
} rav1_hw_stream_t;

static INLINE bool rav1_hw_stream_open_sequence(rav1_hw_stream_t *s)
{
   rav1_hw_ops_t ops;
   if (s->opened)
   {
      const rav1_sequence_t *old = &s->session.sequence;
      const rav1_sequence_t *seq = &s->sequence;
      if (!memcmp(seq, old, sizeof(*seq)))
         return true;
      /* av1C is provisional until the first frame, but its output
       * geometry and color have already been admitted by the caller. */
      if (s->started || seq->max_width != old->max_width ||
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
   if (!rav1_hw_backend_open(&s->sequence, 9, &ops, &s->backend))
      return false;
   if (!rav1_hw_session_init(&s->session, &s->sequence, &ops, 9))
   {
      s->backend.close(&ops);
      memset(&s->backend, 0, sizeof(s->backend));
      return false;
   }
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

/* 0 borrowed, 1 previous packet active, negative invalid/unsupported.
 * Keep input immutable/resident until END, error or close. No packet copy. */
static INLINE int rav1_hw_stream_begin(rav1_hw_stream_t *s,
                                       const uint8_t *packet, size_t size)
{
   if (!s || s->error || !packet || !size)
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

/* Release the previous out before stepping again. One OBU per call; BUSY
 * retries submission/export without advancing input.
 * FRAME transfers a GPU lease; DISCARDED consumes display without export.
 * Sequence changes after frame acceptance and split FRAME_HEADER/TILE_GROUP
 * coding are unsupported.
 * Decode errors are terminal until close/reinit; accepted input is owned
 * by the backend and unaccepted borrowed input is cancelled. */
static INLINE int rav1_hw_stream_step(rav1_hw_stream_t *s,
                                      video_hw_frame_t *out, bool discard)
{
   int ret, result = RAV1_HW_STREAM_PROGRESS;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->error)
      return s ? s->error : -1;
   if (!s->packet)
      return RAV1_HW_STREAM_END;
   if (s->offset == s->size)
   {
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
   case 15:
      break;
   case 3:
   case 6:
      if (!s->opened)
      {
         ret = -2;
         goto fail;
      }
      if (!s->submitted)
      {
         ret = rav1_hw_session_submit(&s->session, s->obu.data, s->obu.size);
         if (ret > 0)
            return RAV1_HW_STREAM_BUSY;
         if (ret < 0)
            goto fail;
         s->submitted = s->started = 1;
      }
      if (s->session.output_slot >= 0)
      {
         if (discard)
         {
            rav1_hw_session_discard_output(&s->session);
            result = RAV1_HW_STREAM_DISCARDED;
         }
         else
         {
            ret = rav1_hw_session_output(&s->session, out);
            if (ret > 0)
               return RAV1_HW_STREAM_BUSY;
            if (ret < 0)
               goto fail;
            result = RAV1_HW_STREAM_FRAME;
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
