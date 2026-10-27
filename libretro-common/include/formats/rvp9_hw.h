/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RVP9_HW_H__
#define __LIBRETRO_SDK_FORMAT_RVP9_HW_H__

#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include <formats/rvp9.h>
#include <formats/rvp9_config.h>
#include <formats/image.h>
#include <formats/video_hw_frame.h>

typedef struct rvp9_hw_picture
{
   rvp9_hdr header; /* effective loop-filter deltas and segmentation features */
   int slot, reference_slots[RVP9_REF_FRAMES];
   int named_slots[RVP9_REFS_PER_FRAME];
   int previous_slot, use_prev_frame_mvs;
} rvp9_hw_picture_t;

typedef struct rvp9_hw_ops
{
   void *decoder;
   /* 0 accepted, 1 busy without submission, negative failure.
    * Acceptance ends borrowed CPU reads; asynchronous work owns its input.
    * Reusing a DPB slot must not invalidate previously exported leases.
    * Packet decoders receive show_existing_frame with its referenced slot,
    * inherited geometry and show_frame set; no reference state is committed. */
   int (*decode)(void *decoder, const uint8_t *packet, size_t size,
         const rvp9_hw_picture_t *picture);
   /* 0 independent NV12/P010 GPU lease, 1 busy without lease, negative failure. */
   int (*output)(void *decoder, unsigned slot, video_hw_frame_t *frame);
} rvp9_hw_ops_t;

typedef struct rvp9_hw_backend
{
   void *user;
   bool (*open)(void *user, const rvp9_hdr *header, unsigned slots,
         rvp9_hw_ops_t *ops);
   void (*close)(rvp9_hw_ops_t *ops);
   /* Opaque bitstream decoders also consume show_existing_frame packets. */
   bool packet_decoder;
   /* New callbacks follow the published user/open/close/mode prefix. */
   /* Optional admission with container metadata, borrowed only during open. */
   bool (*open_color)(void *user, const rvp9_hdr *header,
         const video_color_metadata_t *color, unsigned slots,
         rvp9_hw_ops_t *ops);
   /* Packet decoders only. Terminal input: 0 complete, 1 pending, negative failure.
    * discard drops pending display output. Exported leases remain valid. */
   int (*drain)(void *decoder, bool discard);
   /* Append-only extension; config may be NULL and arguments are borrowed. */
   bool (*open_config)(void *user, const rvp9_hdr *header,
         const video_color_metadata_t *color, const rvp9_config_t *config,
         unsigned slots, rvp9_hw_ops_t *ops);
   /* Same-format key-frame restart: 0 ready, 1 pending, negative failure.
    * Discard old work without invalidating exported leases. */
   int (*restart)(void *decoder);
} rvp9_hw_backend_t;

RETRO_BEGIN_DECLS
/* Publish immutable backend storage before opening streams; NULL withdraws it.
 * The backend/user must outlive its open decoders. Existing sessions keep close. */
void rvp9_set_hw_backend(const rvp9_hw_backend_t *backend);
/* Withdraw only this offer; leave a replacement backend registered. */
int rvp9_unset_hw_backend(const rvp9_hw_backend_t *backend);
bool rvp9_hw_backend_open(const rvp9_hdr *header, unsigned slots,
      rvp9_hw_ops_t *ops, rvp9_hw_backend_t *backend);
bool rvp9_hw_backend_open_config(const rvp9_hdr *header,
      const video_color_metadata_t *color, const rvp9_config_t *config,
      unsigned slots, rvp9_hw_ops_t *ops, rvp9_hw_backend_t *backend);
bool rvp9_hw_backend_open_color(const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots,
      rvp9_hw_ops_t *ops, rvp9_hw_backend_t *backend);
RETRO_END_DECLS

/* Owner-thread state, independent of backend GPU completion and frame leases.
 * Ten DPB slots cover eight references, previous picture and new destination.
 * Fixed profile/geometry; reconfigure by retiring the session and reopening.
 * Packets are individual coded frames, with superframe indices removed. */
typedef struct rvp9_hw_session
{
   rvp9_hw_picture_t pending;
   rvp9_hdr header;
   rvp9_hw_ops_t ops;
   const uint8_t *packet;
   size_t packet_size;
   int reference_slots[RVP9_REF_FRAMES];
   int previous_slot, output_slot;
   unsigned slots;
   int started, phase, broken, packet_decoder;
} rvp9_hw_session_t;

/* Caller owns storage/backend. Reset only after pending calls are cancelled;
 * exported leases remain the backend's responsibility, including after close. */
static INLINE bool rvp9_hw_session_init_mode(rvp9_hw_session_t *s,
      const rvp9_hw_ops_t *ops, unsigned slots, bool packet_decoder)
{
   unsigned i;
   if (!s) return false;
   memset(s, 0, sizeof(*s));
   s->previous_slot = s->output_slot = -1;
   for (i = 0; i < RVP9_REF_FRAMES; i++) s->reference_slots[i] = -1;
   if (!ops || !ops->decode || !ops->output || slots < 10 || slots > 32)
      return false;
   s->ops = *ops;
   s->slots = slots;
   s->packet_decoder = packet_decoder;
   return true;
}

static INLINE bool rvp9_hw_session_init(rvp9_hw_session_t *s,
      const rvp9_hw_ops_t *ops, unsigned slots)
{
   return rvp9_hw_session_init_mode(s, ops, slots, false);
}

static INLINE bool rvp9_hw_session_restart(rvp9_hw_session_t *s)
{
   rvp9_hw_ops_t ops;
   unsigned slots;
   bool packet_decoder;
   if (!s)
      return false;
   ops = s->ops;
   slots = s->slots;
   packet_decoder = s->packet_decoder;
   return rvp9_hw_session_init_mode(s, &ops, slots, packet_decoder);
}

static INLINE void rvp9_hw_session_clear_packet(rvp9_hw_session_t *s)
{
   s->packet = NULL; s->packet_size = 0; s->phase = 0;
}

/* Cancel only an unaccepted packet; references/output and leases are intact. */
static INLINE bool rvp9_hw_session_cancel(rvp9_hw_session_t *s)
{
   if (!s || !s->phase) return false;
   rvp9_hw_session_clear_packet(s);
   memset(&s->pending, 0, sizeof(s->pending));
   return true;
}

/* Drop display without exporting/copying a surface; decode references survive. */
static INLINE bool rvp9_hw_session_discard_output(rvp9_hw_session_t *s)
{
   if (!s || s->output_slot < 0) return false;
   s->output_slot = -1;
   return true;
}

static INLINE void rvp9_hw_inherit(rvp9_hdr *h, const rvp9_hdr *prev)
{
   int i, reset = !h->frame_type || h->intra_only || h->error_resilient;
   for (i = 0; i < 4; i++)
      if (!h->lf_ref_delta_upd[i])
         h->lf_ref_deltas[i] = reset ? (i == 0 ? 1 : i >= 2 ? -1 : 0)
               : prev->lf_ref_deltas[i];
   for (i = 0; i < 2; i++)
      if (!h->lf_mode_delta_upd[i])
         h->lf_mode_deltas[i] = reset ? 0 : prev->lf_mode_deltas[i];
   if (!h->seg_update_data)
   {
      if (reset)
      {
         memset(h->seg_feature_enabled, 0, sizeof(h->seg_feature_enabled));
         memset(h->seg_feature_data, 0, sizeof(h->seg_feature_data));
         h->seg_abs_delta = 0;
      }
      else
      {
         memcpy(h->seg_feature_enabled, prev->seg_feature_enabled,
               sizeof(h->seg_feature_enabled));
         memcpy(h->seg_feature_data, prev->seg_feature_data,
               sizeof(h->seg_feature_data));
         h->seg_abs_delta = prev->seg_abs_delta;
      }
   }
}

/* 0 consumed, 1 busy, negative invalid/backend failure (-2 unsupported).
 * Packet decoders may consume hidden frames behind pending display output.
 * Drain pending display output before another visible packet. On backend
 * busy retain immutable packet storage and retry the same pointer and size.
 * One decode callback per call; no allocation, pixel copies or waiting. */
static INLINE int rvp9_hw_session_submit(
      rvp9_hw_session_t *s, const uint8_t *packet, size_t size)
{
   rvp9_hdr *h;
   uint32_t occupied;
   unsigned i;
   int ret, dims[RVP9_REF_FRAMES * 2];
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
      for (i = 0; i < RVP9_REF_FRAMES; i++)
      {
         dims[i * 2]     = s->reference_slots[i] >= 0 ? s->header.w : 0;
         dims[i * 2 + 1] = s->reference_slots[i] >= 0 ? s->header.h : 0;
      }
      memset(&s->pending, 0, sizeof(s->pending));
      h   = &s->pending.header;
      ret = rvp9_parse_frame_header(packet, size, &s->header, dims, h);
      if (ret < 0)
         return ret;
      if (s->output_slot >= 0 && (ret == 1 || h->show_frame))
         return 1;
      if (ret == 1)
      {
         if (!s->started || h->profile != s->header.profile
               || s->reference_slots[h->frame_to_show] < 0)
            return -1;
         if (s->packet_decoder)
         {
            h->w                     = s->header.w;
            h->h                     = s->header.h;
            h->show_frame            = 1;
            s->pending.slot          = s->reference_slots[h->frame_to_show];
            s->pending.previous_slot = s->previous_slot;
            for (i = 0; i < RVP9_REF_FRAMES; i++)
               s->pending.reference_slots[i] = s->reference_slots[i];
            for (i = 0; i < RVP9_REFS_PER_FRAME; i++)
               s->pending.named_slots[i] = -1;
            s->packet      = packet;
            s->packet_size = size;
            s->phase       = 2;
            goto submit;
         }
         s->output_slot = s->reference_slots[h->frame_to_show];
         return 0;
      }
      if ((h->profile != 0 && h->profile != 2)
            || (h->bit_depth != 8 && h->bit_depth != 10)
            || h->subsampling_x != 1 || h->subsampling_y != 1
            || (!s->started && h->frame_type)
            || (s->started
                  && (h->profile != s->header.profile
                        || h->bit_depth != s->header.bit_depth
                        || h->w != s->header.w || h->h != s->header.h)))
         return -2;
      occupied = 0;
      for (i = 0; i < RVP9_REF_FRAMES; i++)
      {
         int slot                      = s->reference_slots[i];
         s->pending.reference_slots[i] = slot;
         if (slot >= 0)
            occupied |= UINT32_C(1) << slot;
      }
      for (i = 0; i < RVP9_REFS_PER_FRAME; i++)
      {
         int slot = h->frame_type && !h->intra_only
                          ? s->reference_slots[h->ref_idx[i]]
                          : -1;
         if (h->frame_type && !h->intra_only && slot < 0)
            return -1;
         s->pending.named_slots[i] = slot;
      }
      if (s->previous_slot >= 0)
         occupied |= UINT32_C(1) << s->previous_slot;
      for (i = 0; i < s->slots; i++)
         if (!(occupied & (UINT32_C(1) << i)))
            break;
      if (i == s->slots)
         return -1;
      s->pending.slot          = (int)i;
      s->pending.previous_slot = s->previous_slot;
      s->pending.use_prev_frame_mvs =
            s->started && !h->error_resilient && h->frame_type && !h->intra_only
            && s->header.show_frame && s->header.frame_type
            && !s->header.intra_only;
      rvp9_hw_inherit(h, &s->header);
      s->packet      = packet;
      s->packet_size = size;
      s->phase       = 1;
   }
submit:
   ret = s->ops.decode(s->ops.decoder, s->packet, s->packet_size, &s->pending);
   if (ret > 0)
      return 1;
   if (ret < 0)
   {
      s->broken = 1;
      rvp9_hw_session_clear_packet(s);
      return -1;
   }
   if (s->phase == 2)
   {
      s->output_slot = s->pending.slot;
      rvp9_hw_session_clear_packet(s);
      return 0;
   }
   h = &s->pending.header;
   for (i = 0; i < RVP9_REF_FRAMES; i++)
      if (h->refresh_frame_flags & (1 << i))
         s->reference_slots[i] = s->pending.slot;
   s->previous_slot = s->pending.slot;
   if (h->show_frame)
      s->output_slot = s->pending.slot;
   s->header  = *h;
   s->started = 1;
   rvp9_hw_session_clear_packet(s);
   return 0;
}

/* 0 caller-owned GPU frame, 1 unavailable/busy, negative failure.
 * Release/abandon the previous out before calling. No lease on busy/error.
 * Surface backends repeat output alone; packet decoders first consume the
 * show_existing_frame packet without changing reference-slot state. */
static INLINE int rvp9_hw_session_output(rvp9_hw_session_t *s,
      video_hw_frame_t *out)
{
   video_hw_frame_t frame;
   int ret;
   if (!out) return -1;
   memset(out, 0, sizeof(*out));
   if (!s || s->broken || !s->ops.output) return -1;
   if (s->output_slot < 0) return 1;
   memset(&frame, 0, sizeof(frame));
   ret = s->ops.output(s->ops.decoder, (unsigned)s->output_slot, &frame);
   if (ret > 0) return 1;
   if (ret < 0 || !frame.image || frame.width != (unsigned)s->header.w
         || frame.height != (unsigned)s->header.h
         || (s->header.bit_depth == 10
               ? frame.pixfmt != IMAGE_PIXFMT_P010 && frame.pixfmt != IMAGE_PIXFMT_YUV10_FP16
               : frame.pixfmt && frame.pixfmt != IMAGE_PIXFMT_NV12
                 && frame.pixfmt != IMAGE_PIXFMT_YUV8_FP16))
   {
      if (!ret) video_hw_frame_abandon(&frame);
      s->broken = 1;
      return -1;
   }
   s->output_slot = -1;
   *out = frame;
   return 0;
}
#endif
