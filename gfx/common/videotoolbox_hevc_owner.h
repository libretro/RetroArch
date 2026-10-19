/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_OWNER_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_OWNER_H

#include "videotoolbox_hevc_slots.h"

#ifdef VIDEOTOOLBOX_HEVC_SLOTS

/* Nonblocking enqueue; run once off the caller thread. User/queue must survive retirement. */
typedef void (*videotoolbox_hevc_enqueue_t)(void *user, void (*run)(void *), void *work);

typedef struct videotoolbox_hevc_owner
{
   CMVideoFormatDescriptionRef format;
   VTDecompressionSessionRef session;
   videotoolbox_hevc_slots_t *slots;
   videotoolbox_hevc_enqueue_t enqueue;
   void *user;
} videotoolbox_hevc_owner_t;

static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE videotoolbox_hevc_owner_t *videotoolbox_hevc_owner_new(
      const rh265_video *source, unsigned pps, unsigned count,
      videotoolbox_hevc_enqueue_t enqueue, void *user, OSStatus *status)
{
   videotoolbox_hevc_owner_t *owner;
   VTDecompressionOutputCallbackRecord callback;
   OSStatus result;
   if (status) *status = kCMFormatDescriptionError_InvalidParameter;
   if (!source || !enqueue || !count || count > 32) return NULL;
   if (status) *status = kCMFormatDescriptionError_AllocationFailed;
   owner = (videotoolbox_hevc_owner_t*)calloc(1, sizeof(*owner));
   if (!owner) return NULL;
   owner->slots = videotoolbox_hevc_slots_new(count);
   if (!owner->slots) { free(owner); return NULL; }
   owner->enqueue = enqueue; owner->user = user;
   callback.decompressionOutputCallback = videotoolbox_hevc_slots_output;
   callback.decompressionOutputRefCon = owner->slots;
   result = videotoolbox_hevc_session_create(source, pps, &callback,
         &owner->format, &owner->session);
   if (status) *status = result;
   if (result)
   {
      videotoolbox_hevc_slots_release(owner->slots);
      free(owner);
      return NULL;
   }
   return owner;
}

/* Retirement executor only. Drain delayed callbacks before releasing their storage.
 * This blocking framework operation must never run on the playback thread. */
static void videotoolbox_hevc_owner_destroy(void *work)
{
   videotoolbox_hevc_owner_t *owner = (videotoolbox_hevc_owner_t*)work;
   VTDecompressionSessionWaitForAsynchronousFrames(owner->session);
   VTDecompressionSessionInvalidate(owner->session);
   CFRelease(owner->session);
   CFRelease(owner->format);
   videotoolbox_hevc_slots_quiesce(owner->slots);
   videotoolbox_hevc_slots_release(owner->slots);
   free(owner);
}

/* Owner thread only; admission closes before ownership reaches the executor. */
static INLINE void videotoolbox_hevc_owner_retire(videotoolbox_hevc_owner_t **handle)
{
   videotoolbox_hevc_owner_t *owner;
   if (!handle || !(owner = *handle)) return;
   *handle = NULL;
   videotoolbox_hevc_slots_close(owner->slots);
   owner->enqueue(owner->user, videotoolbox_hevc_owner_destroy, owner);
}

#endif
#endif
