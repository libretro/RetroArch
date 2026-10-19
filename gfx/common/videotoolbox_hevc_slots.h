/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_SLOTS_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_SLOTS_H

#include <stdlib.h>
#include <retro_atomic.h>
#include "videotoolbox_hevc.h"

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
#define VIDEOTOOLBOX_HEVC_SLOTS 1

typedef struct videotoolbox_hevc_slots videotoolbox_hevc_slots_t;
typedef struct videotoolbox_hevc_slot
{
   CMTime pts, duration;
   videotoolbox_hevc_slots_t *context;
   CVPixelBufferRef image;
   OSStatus status;
   VTDecodeInfoFlags flags;
   retro_atomic_int_t state; /* 0 free, 1 pending, 2 ready */
} videotoolbox_hevc_slot_t;

struct videotoolbox_hevc_slots
{
   videotoolbox_hevc_slot_t slot[32];
   retro_atomic_int_t refs;
   unsigned count, closed;
};

typedef struct videotoolbox_hevc_result
{
   CMTime pts, duration;
   CVPixelBufferRef image;
   OSStatus status;
   VTDecodeInfoFlags flags;
} videotoolbox_hevc_result_t;

static INLINE videotoolbox_hevc_slots_t *videotoolbox_hevc_slots_new(unsigned count)
{
   videotoolbox_hevc_slots_t *context;
   unsigned i;
   if (!count || count > 32) return NULL;
   context = (videotoolbox_hevc_slots_t*)calloc(1, sizeof(*context));
   if (!context) return NULL;
   context->count = count;
   retro_atomic_int_init(&context->refs, 1);
   for (i = 0; i < count; i++)
   {
      context->slot[i].context = context;
      retro_atomic_int_init(&context->slot[i].state, 0);
   }
   return context;
}

/* Drop the owner reference once; accepted tickets keep the context alive. */
static INLINE void videotoolbox_hevc_slots_release(videotoolbox_hevc_slots_t *context)
{
   unsigned i;
   if (!context || retro_atomic_fetch_sub_int(&context->refs, 1) != 1) return;
   for (i = 0; i < context->count; i++)
      if (context->slot[i].image) CVPixelBufferRelease(context->slot[i].image);
   free(context);
}

/* Owner thread only. Existing tickets and completed results remain valid. */
static INLINE void videotoolbox_hevc_slots_close(videotoolbox_hevc_slots_t *context)
{
   if (context) context->closed = 1;
}

/* Owner thread only. Reserve before invoking a possibly synchronous callback. */
static INLINE videotoolbox_hevc_slot_t *videotoolbox_hevc_slots_begin(
      videotoolbox_hevc_slots_t *context, unsigned index)
{
   videotoolbox_hevc_slot_t *slot;
   if (!context || context->closed || index >= 32 || index >= context->count) return NULL;
   slot = &context->slot[index];
   if (retro_atomic_load_acquire_int(&slot->state)) return NULL;
   slot->image = NULL; slot->status = 0; slot->flags = 0;
   slot->pts = slot->duration = kCMTimeInvalid;
   retro_atomic_fetch_add_int(&context->refs, 1);
   retro_atomic_store_release_int(&slot->state, 1);
   return slot;
}

/* Only for a rejected submission for which no callback will be delivered. */
static INLINE void videotoolbox_hevc_slots_reject(videotoolbox_hevc_slot_t *slot)
{
   videotoolbox_hevc_slots_t *context = slot->context;
   retro_atomic_store_release_int(&slot->state, 0);
   videotoolbox_hevc_slots_release(context);
}

static INLINE void videotoolbox_hevc_slots_output(void *ref, void *frame,
      OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image,
      CMTime pts, CMTime duration)
{
   videotoolbox_hevc_slots_t *context = (videotoolbox_hevc_slots_t*)ref;
   videotoolbox_hevc_slot_t *slot = (videotoolbox_hevc_slot_t*)frame;
   if (!slot || slot->context != context) return;
   slot->image = !status && image ? CVPixelBufferRetain((CVPixelBufferRef)image) : NULL;
   slot->status = status; slot->flags = flags;
   slot->pts = pts; slot->duration = duration;
   retro_atomic_store_release_int(&slot->state, 2);
   videotoolbox_hevc_slots_release(context);
}

/* Owner thread only: 1 complete (including error/drop), 0 pending, -1 invalid.
 * A successful take transfers an owned image reference, if present,
 * and copies callback timing verbatim, including invalid times. */
static INLINE int videotoolbox_hevc_slots_take(videotoolbox_hevc_slots_t *context,
      unsigned index, videotoolbox_hevc_result_t *out)
{
   videotoolbox_hevc_slot_t *slot;
   if (!out) return -1;
   out->image = NULL; out->status = 0; out->flags = 0;
   out->pts = out->duration = kCMTimeInvalid;
   if (!context || index >= 32 || index >= context->count) return -1;
   slot = &context->slot[index];
   if (retro_atomic_load_acquire_int(&slot->state) != 2) return 0;
   out->image = slot->image; out->status = slot->status; out->flags = slot->flags;
   out->pts = slot->pts; out->duration = slot->duration;
   slot->image = NULL;
   retro_atomic_store_release_int(&slot->state, 0);
   return 1;
}

/* Teardown only, after the session can no longer call or run callbacks.
 * The caller must still hold its owner reference. This does not stop a session. */
static INLINE void videotoolbox_hevc_slots_quiesce(videotoolbox_hevc_slots_t *context)
{
   unsigned i;
   int retired = 0;
   if (!context) return;
   for (i = 0; i < context->count; i++)
      if (retro_atomic_load_acquire_int(&context->slot[i].state) == 1)
      {
         retro_atomic_store_release_int(&context->slot[i].state, 0);
         retired++;
      }
   if (retired) retro_atomic_fetch_sub_int(&context->refs, retired);
}

/* 0 accepted, 1 busy without submission, otherwise the framework error. */
static INLINE OSStatus videotoolbox_hevc_slots_submit_timed(
      videotoolbox_hevc_slots_t *context, unsigned index,
      VTDecompressionSessionRef session, CMVideoFormatDescriptionRef format,
      const rh265_access_unit_t *au, const rh265_nal_t *slices, unsigned capacity,
      const CMSampleTimingInfo *timing,
      VTDecodeInfoFlags *info)
{
   videotoolbox_hevc_slot_t *slot;
   OSStatus status;
   if (info) *info = 0;
   if (!context || context->closed || index >= 32 || index >= context->count)
      return kCMFormatDescriptionError_InvalidParameter;
   slot = videotoolbox_hevc_slots_begin(context, index);
   if (!slot) return 1;
   status = videotoolbox_hevc_decode_timed(session, format, au, slices, capacity, timing, slot, info);
   if (status) videotoolbox_hevc_slots_reject(slot);
   return status;
}

static INLINE OSStatus videotoolbox_hevc_slots_submit(
      videotoolbox_hevc_slots_t *context, unsigned index,
      VTDecompressionSessionRef session, CMVideoFormatDescriptionRef format,
      const rh265_access_unit_t *au, const rh265_nal_t *slices, unsigned capacity,
      VTDecodeInfoFlags *info)
{
   return videotoolbox_hevc_slots_submit_timed(context, index, session, format,
         au, slices, capacity, NULL, info);
}

#endif
#endif
