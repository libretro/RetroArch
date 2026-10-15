/* SPDX-License-Identifier: MIT */
#ifndef __LIBRETRO_SDK_FORMAT_RAV1_REFERENCE_STATE_H__
#define __LIBRETRO_SDK_FORMAT_RAV1_REFERENCE_STATE_H__

#include <formats/rav1_reference.h>

typedef struct rav1_reference_state
{
   uint64_t epoch, picture_id[8];
   rav1_reference_t refs[8];
   unsigned type[8], showable[8];
   unsigned have_previous, previous_frame_id;
} rav1_reference_state_t;

typedef struct rav1_reference_transaction
{
   uint64_t epoch, picture_id;
   const rav1_reference_state_t *owner;
   rav1_frame_prefix_t frame;
   rav1_reference_t current;
   unsigned type, showable, valid_mask, refresh_mask, ready;
} rav1_reference_transaction_t;

/* Zero-initialize once. Reset on seek/new sequence, preserving ticket identity.
 * State is confined to the decoder thread. No GPU handles or pixels are owned.
 */
static INLINE bool rav1_reference_state_reset(rav1_reference_state_t *state)
{
   uint64_t epoch;
   if (!state || state->epoch == UINT64_MAX)
      return false;
   epoch = state->epoch + 1;
   memset(state, 0, sizeof(*state));
   state->epoch = epoch;
   return true;
}

/* Prefix/sequence must be validated. Preparation never changes committed state.
 * Tickets are tied to the state epoch; coded pictures get distinct local IDs.
 * Reset/commit invalidates pending tickets. A key-frame replay consumes every
 * alias of that picture, including slots outside its original refresh mask. */
static INLINE bool rav1_reference_prepare(rav1_reference_transaction_t *out,
                                          const rav1_reference_state_t *state,
                                          const rav1_sequence_t *seq,
                                          const rav1_frame_prefix_t *frame)
{
   unsigned i, index, modulus, distance;
   if (!out || frame == &out->frame)
      return false;
   memset(out, 0, sizeof(*out));
   if (!state || !seq || !frame || state->epoch == UINT64_MAX ||
       frame->refresh_frame_flags > 255 ||
       (seq->frame_id_numbers_present &&
        (!seq->frame_id_length || seq->frame_id_length > 16)))
      return false;
   out->epoch = state->epoch;
   out->owner = state;
   out->frame = *frame;
   for (i = 0; i < 8; i++)
      if (state->refs[i].valid)
         out->valid_mask |= 1u << i;
   if (frame->show_existing)
   {
      index = frame->frame_to_show;
      if (index > 7 || !state->refs[index].valid || !state->showable[index] ||
          !state->picture_id[index] || state->type[index] > RAV1_FRAME_SWITCH ||
          (seq->frame_id_numbers_present &&
           frame->display_frame_id != state->refs[index].frame_id))
         goto fail;
      out->picture_id = state->picture_id[index];
      out->type = state->type[index];
      out->showable = out->type != RAV1_FRAME_KEY;
      out->current = state->refs[index];
      out->refresh_mask = out->type == RAV1_FRAME_KEY ? 255 : 0;
      out->ready = 1;
      return true;
   }
   if (frame->frame_type > RAV1_FRAME_SWITCH ||
       (frame->frame_type == RAV1_FRAME_INTRA_ONLY &&
        frame->refresh_frame_flags == 255))
      goto fail;
   if (seq->frame_id_numbers_present)
   {
      modulus = 1u << seq->frame_id_length;
      if (frame->current_frame_id >= modulus)
         goto fail;
      if (state->have_previous &&
          !(frame->frame_type == RAV1_FRAME_KEY && frame->show_frame))
      {
         distance = (frame->current_frame_id - state->previous_frame_id) &
                    (modulus - 1);
         if (!distance || distance >= modulus / 2)
            goto fail;
      }
   }
   out->picture_id = state->epoch + 1;
   out->type = frame->frame_type;
   out->showable = frame->showable_frame;
   out->current.order_hint = frame->order_hint;
   out->current.frame_id = frame->current_frame_id;
   out->refresh_mask = frame->refresh_frame_flags;
   return true;
fail:
   memset(out, 0, sizeof(*out));
   return false;
}

/* Stage validated setup metadata. Every selected reference must meet
 * AV1's frame_size_with_refs scaling bounds. Full headers/tiles and decode must
 * still succeed before commit. Per-block state stays with the decoder. */
static INLINE bool rav1_reference_stage(rav1_reference_transaction_t *ticket,
                                        const rav1_reference_state_t *state,
                                        const rav1_frame_setup_t *setup)
{
   unsigned i, index;
   if (!ticket)
      return false;
   ticket->ready = 0;
   if (!state || !setup || ticket->owner != state ||
       ticket->epoch != state->epoch || ticket->frame.show_existing)
      return false;
   if (setup->valid_mask > 255 || !setup->size.width ||
       setup->size.width > 65536 || !setup->size.height ||
       setup->size.height > 65536 || !setup->size.upscaled_width ||
       setup->size.upscaled_width > 65536)
      return false;
   if (!ticket->frame.frame_is_intra)
   {
      for (i = 0; i < 7; i++)
      {
         const rav1_reference_t *ref;
         index = setup->ref_frame_idx[i];
         if (index > 7 || !(setup->valid_mask & (1u << index)) ||
             !state->refs[index].valid)
            return false;
         ref = &state->refs[index];
         if (!ref->size.upscaled_width || ref->size.upscaled_width > 65536 ||
             !ref->size.height || ref->size.height > 65536 ||
             2 * setup->size.width < ref->size.upscaled_width ||
             2 * setup->size.height < ref->size.height ||
             setup->size.width > 16 * ref->size.upscaled_width ||
             setup->size.height > 16 * ref->size.height)
            return false;
      }
   }
   ticket->current.size = setup->size;
   ticket->current.valid = 1;
   ticket->valid_mask = setup->valid_mask;
   ticket->ready = 1;
   return true;
}

/* Commit after decode/submission acceptance, or accepting a show-existing
 * frame. Refresh external GPU/picture/grain metadata with this same refresh
 * mask and picture ID. GPU completion/leases stay independent; no wait is
 * required. Abandoning a ticket needs no call. Failure leaves state unchanged.
 */
static INLINE bool
rav1_reference_commit(rav1_reference_state_t *state,
                      const rav1_reference_transaction_t *ticket)
{
   unsigned i;
   if (!state || !ticket || ticket->owner != state || !ticket->ready ||
       ticket->epoch != state->epoch || state->epoch == UINT64_MAX ||
       ticket->valid_mask > 255 || ticket->refresh_mask > 255)
      return false;
   for (i = 0; i < 8; i++)
   {
      state->refs[i].valid = (ticket->valid_mask >> i) & 1;
      if (ticket->frame.show_existing && ticket->type == RAV1_FRAME_KEY &&
          state->picture_id[i] == ticket->picture_id)
         state->showable[i] = 0;
      if ((ticket->refresh_mask >> i) & 1)
      {
         state->refs[i] = ticket->current;
         state->picture_id[i] = ticket->picture_id;
         state->type[i] = ticket->type;
         state->showable[i] = ticket->showable;
      }
   }
   if (!ticket->frame.show_existing || ticket->type == RAV1_FRAME_KEY)
   {
      state->previous_frame_id = ticket->current.frame_id;
      state->have_previous = 1;
   }
   state->epoch++;
   return true;
}

#endif
