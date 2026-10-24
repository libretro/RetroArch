/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_HW_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_HW_H__

#include <formats/rav1_reference_state.h>
#include <formats/rav1_picture.h>
#include <formats/rav1_motion.h>
#include <formats/rav1_grain.h>
#include <formats/image.h>
#include <formats/video_hw_frame.h>

typedef struct rav1_hw_picture
{
   rav1_tile_group_t tile_group;
   rav1_frame_prefix_t header;
   rav1_frame_setup_t setup;
   rav1_tile_info_t tiles;
   rav1_picture_t picture;
   rav1_motion_t motion;
   rav1_grain_header_t grain;
   rav1_reference_t reference_info[8];
   unsigned reference_types[8];
   int slot, reference_slots[8];
   uint8_t order_hints[8], reference_order_hints[8][8];
} rav1_hw_picture_t;

typedef struct rav1_hw_ops
{
   void *decoder;
   /* 0 accepted, 1 BUSY without submission, negative failure.
    * Acceptance ends borrowed CPU reads; asynchronous work owns its input.
    * Packet decoders also receive show-existing headers with the inherited
    * size, grain and referenced slot; acceptance precedes reference refresh.
    * Snapshot metadata before returning. DPB reuse must preserve exported
    * leases. Decline unsupported grain before acceptance. */
   int (*decode)(void *decoder, const uint8_t *packet, size_t size,
                 const rav1_sequence_t *sequence,
                 const rav1_hw_picture_t *picture);
   /* 0 independent GPU lease, 1 BUSY, negative failure; no lease on BUSY/error.
    * Apply requested grain in output, keeping reference pixels grain-free.
    * Snapshot grain before returning; export ready/done ownership through
    * video_hw_frame_t. */
   int (*output)(void *decoder, unsigned slot, const rav1_grain_t *grain,
                 video_hw_frame_t *frame);
} rav1_hw_ops_t;

typedef struct rav1_hw_backend
{
   void *user;
   bool (*open)(void *user, const rav1_sequence_t *sequence, unsigned slots,
         rav1_hw_ops_t *ops);
   void (*close)(rav1_hw_ops_t *ops);
   /* Opaque bitstream decoders also consume show-existing header OBUs. */
   bool packet_decoder;
   /* Optional admission; all views are borrowed only during open. */
   bool (*open_config)(void *user, const rav1_sequence_t *sequence,
         const rav1_obu_t *sequence_obu, const video_color_metadata_t *color,
         unsigned slots, rav1_hw_ops_t *ops);
   /* Packet decoders: 0 terminal completion, 1 pending, negative failure. */
   int (*drain)(void *decoder, bool discard);
} rav1_hw_backend_t;

RETRO_BEGIN_DECLS
/* Publish immutable storage before opening streams; NULL withdraws it.
 * Backend/user storage must outlive open decoders, which retain close. */
void rav1_set_hw_backend(const rav1_hw_backend_t *backend);
/* Withdraw only this offer; leave a replacement backend registered. */
int rav1_unset_hw_backend(const rav1_hw_backend_t *backend);
bool rav1_hw_backend_open(const rav1_sequence_t *sequence, unsigned slots,
      rav1_hw_ops_t *ops, rav1_hw_backend_t *backend);
bool rav1_hw_backend_open_config(const rav1_sequence_t *sequence,
      const rav1_obu_t *sequence_obu, const video_color_metadata_t *color,
      unsigned slots, rav1_hw_ops_t *ops, rav1_hw_backend_t *backend);
RETRO_END_DECLS

typedef struct rav1_hw_reference
{
   rav1_picture_reference_t picture;
   rav1_global_motion_t global;
   rav1_grain_t grain;
   uint8_t order_hints[8];
} rav1_hw_reference_t;

/* Caller-owned persistent storage, confined to the decoder thread. Individual
 * FRAME OBUs, or show-existing FRAME_HEADER OBUs, are submitted after splitting
 * temporal units. An unsized OBU must fill the supplied bounded span.
 * Profile 0, 8/10-bit 4:2:0; retire/reopen on sequence changes.
 * Backend owns GPU completion/leases, including after the session is retired.
 */
typedef struct rav1_hw_session
{
   rav1_hw_ops_t ops;
   const uint8_t *packet;
   size_t packet_size;
   rav1_reference_state_t state;
   rav1_reference_transaction_t transaction;
   rav1_hw_picture_t pending;
   rav1_sequence_t sequence;
   rav1_hw_reference_t references[8];
   rav1_grain_t output_grain;
   unsigned slots, output_width, output_height;
   int reference_slots[8], output_slot, phase, broken, packet_decoder;
} rav1_hw_session_t;

/* Validated sequence and ops must be distinct from session storage. Cancel any
 * unaccepted packet before retiring/reinitializing the backend and session. */
static INLINE bool rav1_hw_session_init_mode(rav1_hw_session_t *s,
      const rav1_sequence_t *sequence, const rav1_hw_ops_t *ops,
      unsigned slots, bool packet_decoder)
{
   unsigned i;
   if (!s || sequence == &s->sequence || ops == &s->ops)
      return false;
   memset(s, 0, sizeof(*s));
   s->output_slot = -1;
   for (i = 0; i < 8; i++)
      s->reference_slots[i] = -1;
   if (!sequence || !ops || !ops->decode || !ops->output || slots < 9 ||
       slots > 32 || sequence->profile ||
       (sequence->bit_depth != 8 && sequence->bit_depth != 10) ||
       sequence->monochrome || sequence->subsampling_x != 1 ||
       sequence->subsampling_y != 1)
      return false;
   s->sequence = *sequence;
   s->ops = *ops;
   s->slots = slots;
   s->packet_decoder = packet_decoder;
   return rav1_reference_state_reset(&s->state);
}

static INLINE bool rav1_hw_session_init(rav1_hw_session_t *s,
      const rav1_sequence_t *sequence, const rav1_hw_ops_t *ops, unsigned slots)
{
   return rav1_hw_session_init_mode(s, sequence, ops, slots, false);
}

static INLINE int rav1_hw_session_commit_repeat(rav1_hw_session_t *s)
{
   unsigned i, selected = s->pending.header.frame_to_show;
   int slot = s->reference_slots[selected];
   if (!rav1_reference_commit(&s->state, &s->transaction))
   {
      s->broken = 1;
      return -1;
   }
   s->output_grain = s->references[selected].grain;
   for (i = 0; i < 8; i++)
      if ((s->transaction.refresh_mask >> i) & 1)
      {
         s->reference_slots[i] = slot;
         s->references[i] = s->references[selected];
      }
   s->output_slot = slot;
   s->output_width = s->transaction.current.size.upscaled_width;
   s->output_height = s->transaction.current.size.height;
   return 0;
}

/* Cancel only an unaccepted packet. References and exported leases survive. */
static INLINE bool rav1_hw_session_cancel(rav1_hw_session_t *s)
{
   if (!s || !s->phase)
      return false;
   s->packet = NULL;
   s->packet_size = 0;
   s->phase = 0;
   memset(&s->pending, 0, sizeof(s->pending));
   return true;
}

/* 0 consumed, 1 BUSY, negative invalid/backend failure (-2 unsupported).
 * Packet decoders may submit hidden frames behind pending display output.
 * Drain/discard display before another visible packet. Retain input on BUSY
 * and retry the same pointer/size. One decode callback per call, no waiting. */
static INLINE int rav1_hw_session_submit(rav1_hw_session_t *s,
                                         const uint8_t *packet, size_t size)
{
   const rav1_grain_t *grains[8];
   const rav1_picture_reference_t *primary = NULL;
   const rav1_global_motion_t *global = NULL;
   rav1_obu_t obu;
   rav1_bits_t bits;
   size_t offset = 0;
   uint32_t occupied = 0;
   unsigned i, selected, j;
   int ret, slot;
   if (!s || s->broken || !s->ops.decode || !packet || !size)
      return -1;
   if (s->phase)
   {
      if (packet != s->packet || size != s->packet_size)
         return -1;
   }
   else
   {
      if (s->output_slot >= 0 && !s->packet_decoder)
         return 1;
      if (rav1_obu_next(&obu, packet, size, &offset, true) != 1 ||
          offset != size)
         return -1;
      if ((obu.type != 6 && obu.type != 3) || obu.temporal_id || obu.spatial_id)
         return -2;
      memset(&s->pending, 0, sizeof(s->pending));
      bits.data = obu.payload;
      bits.size = obu.payload_size;
      bits.byte = 0;
      bits.bit = bits.failed = 0;
      if (!rav1_frame_prefix_parse(&s->pending.header, &bits, &s->sequence, 0,
                                   0, obu.type == 6))
         return -1;
      if (s->output_slot >= 0 && (s->pending.header.show_existing
               || s->pending.header.show_frame))
         return 1;
      if (!rav1_reference_prepare(&s->transaction, &s->state, &s->sequence,
               &s->pending.header))
         return -1;
      if (s->pending.header.show_existing)
      {
         selected = s->pending.header.frame_to_show;
         slot = s->reference_slots[selected];
         if (slot < 0 || slot >= (int)s->slots || !rav1_bits_read(&bits, 1) ||
             bits.failed)
            return -1;
         while (bits.byte < bits.size)
            if (rav1_bits_read(&bits, 1) || bits.failed)
               return -1;
         if (!s->packet_decoder)
            return rav1_hw_session_commit_repeat(s);
         s->pending.slot = slot;
         s->pending.setup.size = s->transaction.current.size;
         s->pending.grain.params = s->references[selected].grain;
         for (i = 0; i < 8; i++)
            s->pending.reference_slots[i] = s->reference_slots[i];
         s->packet = packet;
         s->packet_size = size;
         s->phase = 2;
         goto submit;
      }
      if (obu.type != 6 || (!s->state.have_previous &&
                            s->pending.header.frame_type != RAV1_FRAME_KEY))
         return -2;
      if (!rav1_frame_setup_parse(&s->pending.setup, &bits, &s->sequence,
                                  &s->pending.header, s->state.refs) ||
          !rav1_reference_stage(&s->transaction, &s->state,
                                &s->pending.setup) ||
          !rav1_tile_info_parse(&s->pending.tiles, &bits,
                                s->pending.setup.size.width,
                                s->pending.setup.size.height,
                                s->sequence.use_128x128_superblock != 0))
         return -1;
      if (s->pending.header.primary_ref_frame != 7)
      {
         selected = s->pending.setup
                        .ref_frame_idx[s->pending.header.primary_ref_frame];
         primary = &s->references[selected].picture;
         global = &s->references[selected].global;
      }
      for (i = 0; i < 8; i++)
         grains[i] = &s->references[i].grain;
      if (!rav1_picture_parse(&s->pending.picture, &bits, &s->sequence,
                              &s->pending.header, &s->pending.setup.size,
                              s->pending.setup.allow_intrabc != 0, primary) ||
          !rav1_motion_parse(&s->pending.motion, &bits, &s->sequence,
                             &s->pending.header, &s->pending.setup,
                             s->state.refs, global) ||
          !rav1_grain_parse(&s->pending.grain, &bits, &s->sequence,
                            &s->pending.header, &s->pending.setup, grains))
         return -1;
      while (bits.bit)
         if (rav1_bits_read(&bits, 1) || bits.failed)
            return -1;
      if (!rav1_tile_group_init(&s->pending.tile_group, obu.payload + bits.byte,
                                obu.payload_size - bits.byte,
                                &s->pending.tiles.layout, true))
         return -1;
      for (i = 0; i < 8; i++)
      {
         slot = (s->pending.setup.valid_mask >> i) & 1 ? s->reference_slots[i]
                                                       : -1;
         if (slot >= (int)s->slots ||
             (((s->pending.setup.valid_mask >> i) & 1) && slot < 0))
            return -1;
         s->pending.reference_info[i] = s->state.refs[i];
         s->pending.reference_info[i].valid = slot >= 0;
         s->pending.reference_types[i] = s->state.type[i];
         memcpy(s->pending.reference_order_hints[i],
                s->references[i].order_hints, 8);
         s->pending.reference_slots[i] = slot;
         if (slot >= 0)
            occupied |= UINT32_C(1) << slot;
      }
      if (!s->pending.header.frame_is_intra)
         for (i = 0; i < 7; i++)
            s->pending.order_hints[i + 1] =
                (uint8_t)s->state.refs[s->pending.setup.ref_frame_idx[i]]
                    .order_hint;
      for (i = 0; i < s->slots; i++)
         if (!(occupied & (UINT32_C(1) << i)))
            break;
      if (i == s->slots)
         return -1;
      s->pending.slot = (int)i;
      s->packet = packet;
      s->packet_size = size;
      s->phase = 1;
   }
submit:
   ret = s->ops.decode(s->ops.decoder, s->packet, s->packet_size, &s->sequence,
                       &s->pending);
   if (ret > 0)
      return 1;
   s->packet = NULL;
   s->packet_size = 0;
   s->phase = 0;
   if (!ret && s->pending.header.show_existing)
      return rav1_hw_session_commit_repeat(s);
   if (ret < 0 || !rav1_reference_commit(&s->state, &s->transaction))
   {
      s->broken = 1;
      return ret < 0 ? ret : -1;
   }
   for (i = 0; i < 8; i++)
   {
      if (!(s->transaction.valid_mask & (1u << i)))
         s->reference_slots[i] = -1;
      if ((s->transaction.refresh_mask >> i) & 1)
      {
         s->reference_slots[i] = s->pending.slot;
         s->references[i].picture = s->pending.picture.reference;
         s->references[i].global = s->pending.motion.global;
         s->references[i].grain = s->pending.grain.params;
         for (j = 0; j < 8; j++)
            s->references[i].order_hints[j] = s->pending.order_hints[j];
      }
   }
   if (s->pending.header.show_frame)
   {
      s->output_slot = s->pending.slot;
      s->output_width = s->pending.setup.size.upscaled_width;
      s->output_height = s->pending.setup.size.height;
      s->output_grain = s->pending.grain.params;
   }
   return 0;
}

/* Release/abandon the previous out first. BUSY retains pending display.
 * Frame export must preserve ready/done lifetime across later DPB reuse. */
static INLINE int rav1_hw_session_output(rav1_hw_session_t *s,
                                         video_hw_frame_t *out)
{
   video_hw_frame_t frame;
   int ret;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->broken || !s->ops.output)
      return -1;
   if (s->output_slot < 0)
      return 1;
   memset(&frame, 0, sizeof(frame));
   ret = s->ops.output(s->ops.decoder, (unsigned)s->output_slot,
                       &s->output_grain, &frame);
   if (ret > 0)
      return 1;
   if (ret < 0 || !frame.image || frame.width != s->output_width ||
       frame.height != s->output_height ||
       (s->sequence.bit_depth == 10
            ? frame.pixfmt != IMAGE_PIXFMT_P010 && frame.pixfmt != IMAGE_PIXFMT_YUV10_FP16
            : frame.pixfmt && frame.pixfmt != IMAGE_PIXFMT_NV12
                 && frame.pixfmt != IMAGE_PIXFMT_YUV8_FP16))
   {
      if (!ret)
         video_hw_frame_abandon(&frame);
      s->broken = 1;
      return ret < 0 ? ret : -1;
   }
   s->output_slot = -1;
   *out = frame;
   return 0;
}

/* Discard display without exporting a frame; reference pictures remain live. */
static INLINE bool rav1_hw_session_discard_output(rav1_hw_session_t *s)
{
   if (!s || s->output_slot < 0)
      return false;
   s->output_slot = -1;
   return true;
}

#endif
