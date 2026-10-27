/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_SLOTS_H
#define RETROARCH_VIDEOTOOLBOX_VP9_SLOTS_H
#include <stdlib.h>
#include "videotoolbox_vp9_sample.h"

#if defined(VIDEOTOOLBOX_VP9_SAMPLE) && defined(RETRO_ATOMIC_HAS_CAS)          \
      && defined(RETRO_ATOMIC_HAS_PTR)
#define VIDEOTOOLBOX_VP9_SLOTS 1

typedef struct videotoolbox_vp9_result
{
   CMTime pts, duration;
   CVPixelBufferRef image;
   OSStatus status;
   VTDecodeInfoFlags flags;
} videotoolbox_vp9_result_t;

typedef struct videotoolbox_vp9_slot
{
   videotoolbox_vp9_result_t result;
   retro_atomic_ptr_t state;
} videotoolbox_vp9_slot_t;

typedef struct videotoolbox_vp9_slots
{
   videotoolbox_vp9_slot_t slot[32];
   uintptr_t serial;
   unsigned count, closed;
} videotoolbox_vp9_slots_t;

static INLINE videotoolbox_vp9_slots_t *videotoolbox_vp9_slots_new(
      unsigned count)
{
   videotoolbox_vp9_slots_t *slots;
   unsigned i;
   if (!count || count > 32)
      return NULL;
   slots = (videotoolbox_vp9_slots_t *)calloc(1, sizeof(*slots));
   if (!slots)
      return NULL;
   slots->count = count;
   for (i = 0; i < count; i++)
      retro_atomic_ptr_init(&slots->slot[i].state, NULL);
   return slots;
}

/* Owner only. Cookies are opaque values, never dereferenced or reused. */
static INLINE void *videotoolbox_vp9_slots_begin(
      videotoolbox_vp9_slots_t *slots, unsigned index)
{
   videotoolbox_vp9_slot_t *slot;
   uintptr_t cookie;
   if (!slots || slots->closed || index >= slots->count
         || slots->serial == ((uintptr_t)-1 >> 7))
      return NULL;
   slot = &slots->slot[index];
   if (retro_atomic_load_acquire_ptr(&slot->state))
      return NULL;
   cookie = (++slots->serial << 7) | ((uintptr_t)index << 2);
   memset(&slot->result, 0, sizeof(slot->result));
   slot->result.pts = slot->result.duration = kCMTimeInvalid;
   retro_atomic_store_release_ptr(&slot->state, (void *)cookie);
   return (void *)cookie;
}

/* A cookie and its state change atomically: stale callbacks cannot claim a
 * recycled slot. The pool survives until worker/session retirement quiesces. */
static void videotoolbox_vp9_slots_output(void *ref, void *frame,
      OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image,
      CMTime pts, CMTime duration)
{
   videotoolbox_vp9_slots_t *slots = (videotoolbox_vp9_slots_t *)ref;
   videotoolbox_vp9_slot_t *slot;
   uintptr_t cookie = (uintptr_t)frame;
   unsigned index   = (unsigned)((cookie >> 2) & 31);
   if (!slots || !cookie || (cookie & 3) || index >= slots->count
         || (!status && !image && !(flags & kVTDecodeInfo_FrameDropped)))
      return;
   slot = &slots->slot[index];
   if (!retro_atomic_cas_ptr(&slot->state, frame, (void *)(cookie | 1)))
      return;
   slot->result.image =
         !status && image ? CVPixelBufferRetain((CVPixelBufferRef)image) : NULL;
   slot->result.status   = status;
   slot->result.flags    = flags;
   slot->result.pts      = pts;
   slot->result.duration = duration;
   retro_atomic_store_release_ptr(&slot->state, (void *)(cookie | 2));
}

/* Worker rejection or successful hidden-only submission; no display lease.
 * The compressed-storage pin is independent of this completion state. */
static INLINE void videotoolbox_vp9_slots_complete(
      videotoolbox_vp9_slots_t *slots, void *frame, OSStatus status)
{
   videotoolbox_vp9_slots_output(slots, frame, status,
         kVTDecodeInfo_FrameDropped, NULL, kCMTimeInvalid, kCMTimeInvalid);
}

/* Owner only. Transfers an image reference: 1 complete, 0 pending, -1 invalid.
 */
static INLINE int videotoolbox_vp9_slots_take(videotoolbox_vp9_slots_t *slots,
      unsigned index, videotoolbox_vp9_result_t *out)
{
   videotoolbox_vp9_slot_t *slot;
   uintptr_t state;
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   out->pts = out->duration = kCMTimeInvalid;
   if (!slots || index >= slots->count)
      return -1;
   slot  = &slots->slot[index];
   state = (uintptr_t)retro_atomic_load_acquire_ptr(&slot->state);
   if ((state & 3) != 2)
      return 0;
   *out               = slot->result;
   slot->result.image = NULL;
   retro_atomic_store_release_ptr(&slot->state, NULL);
   return 1;
}

/* Retirement executor only, after all worker/native callbacks have stopped. */
static INLINE void videotoolbox_vp9_slots_free_quiesced(
      videotoolbox_vp9_slots_t *slots)
{
   unsigned i;
   if (!slots)
      return;
   for (i = 0; i < slots->count; i++)
      if (slots->slot[i].result.image)
         CVPixelBufferRelease(slots->slot[i].result.image);
   free(slots);
}
#endif
#endif
