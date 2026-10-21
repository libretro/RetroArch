/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_ENCODE_SLOTS_H
#define RETROARCH_VIDEOTOOLBOX_ENCODE_SLOTS_H
#include <stdlib.h>
#include <retro_atomic.h>
#ifndef VIDEOTOOLBOX_ENCODE_TEST_TYPES
#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) && !defined(inline)
#define inline __inline__
#define VIDEOTOOLBOX_ENCODE_UNDEF_INLINE
#endif
#include <VideoToolbox/VideoToolbox.h>
#ifdef VIDEOTOOLBOX_ENCODE_UNDEF_INLINE
#undef inline
#undef VIDEOTOOLBOX_ENCODE_UNDEF_INLINE
#endif
#endif

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
#define VIDEOTOOLBOX_ENCODE_SLOTS 16
typedef struct videotoolbox_encode_slots videotoolbox_encode_slots_t;
typedef struct videotoolbox_encode_slot
{
   videotoolbox_encode_slots_t *owner;
   CVPixelBufferRef input;
   CMSampleBufferRef sample;
   OSStatus status;
   VTEncodeInfoFlags flags;
   retro_atomic_int_t state; /* 0 free, 1 callback pending, 2 borrowed output */
} videotoolbox_encode_slot_t;
struct videotoolbox_encode_slots
{
   videotoolbox_encode_slot_t slot[VIDEOTOOLBOX_ENCODE_SLOTS];
   retro_atomic_int_t refs;
   unsigned count, closed;
};

static INLINE videotoolbox_encode_slots_t *videotoolbox_encode_slots_new(unsigned count)
{
   videotoolbox_encode_slots_t *pool;
   unsigned i;
   if (!count || count > VIDEOTOOLBOX_ENCODE_SLOTS) return NULL;
   pool = (videotoolbox_encode_slots_t*)calloc(1, sizeof(*pool));
   if (!pool) return NULL;
   pool->count = count;
   retro_atomic_int_init(&pool->refs, 1);
   for (i = 0; i < count; i++)
   {
      pool->slot[i].owner = pool;
      retro_atomic_int_init(&pool->slot[i].state, 0);
   }
   return pool;
}

/* One owner reference plus one reference for each outstanding callback.
 * Closing/releasing the owner does not invalidate accepted callback tickets. */
static INLINE void videotoolbox_encode_slots_release(videotoolbox_encode_slots_t *pool)
{
   unsigned i;
   if (!pool || retro_atomic_fetch_sub_int(&pool->refs, 1) != 1) return;
   for (i = 0; i < pool->count; i++)
   {
      if (pool->slot[i].input) CVPixelBufferRelease(pool->slot[i].input);
      if (pool->slot[i].sample) CFRelease(pool->slot[i].sample);
   }
   free(pool);
}

static INLINE void videotoolbox_encode_slots_close(videotoolbox_encode_slots_t *pool)
{
   if (pool) pool->closed = 1;
}

/* Worker only, before EncodeFrame (which may call back synchronously).
 * The fixed input must already be GPU-complete, and stays immutable until ack. */
static INLINE videotoolbox_encode_slot_t *videotoolbox_encode_slots_begin(
      videotoolbox_encode_slots_t *pool, unsigned index, CVPixelBufferRef input)
{
   videotoolbox_encode_slot_t *slot;
   if (!pool || pool->closed || !input || index >= pool->count) return NULL;
   slot = &pool->slot[index];
   if (retro_atomic_load_acquire_int(&slot->state)) return NULL;
   slot->input = CVPixelBufferRetain(input);
   slot->sample = NULL; slot->status = 0; slot->flags = 0;
   retro_atomic_fetch_add_int(&pool->refs, 1);
   retro_atomic_store_release_int(&slot->state, 1);
   return slot;
}

/* A rejected EncodeFrame for which no callback is or will be delivered.
 * Also refuses a result already published by a synchronous callback. */
static INLINE bool videotoolbox_encode_slots_reject(videotoolbox_encode_slot_t *slot)
{
   if (!slot || retro_atomic_load_acquire_int(&slot->state) != 1) return false;
   CVPixelBufferRelease(slot->input); slot->input = NULL;
   retro_atomic_store_release_int(&slot->state, 0);
   videotoolbox_encode_slots_release(slot->owner);
   return true;
}

/* VideoToolbox callback. A null sample/drop is published, not silently lost.
 * No allocation, encoder call, wait or queue release occurs here. */
static INLINE void videotoolbox_encode_slots_output(void *ref, void *frame,
      OSStatus status, VTEncodeInfoFlags flags, CMSampleBufferRef sample)
{
   videotoolbox_encode_slot_t *slot = (videotoolbox_encode_slot_t*)frame;
   (void)ref;
   if (!slot) return;
   slot->status = status; slot->flags = flags;
   slot->sample = !status && sample ? (CMSampleBufferRef)CFRetain(sample) : NULL;
   retro_atomic_store_release_int(&slot->state, 2);
   videotoolbox_encode_slots_release(slot->owner);
}

/* Worker borrows the result until ack; repeated peeks retain nothing extra.
 * The caller interprets framework status/drop flags and owns mux backpressure. */
static INLINE const videotoolbox_encode_slot_t *videotoolbox_encode_slots_peek(
      videotoolbox_encode_slots_t *pool, unsigned index)
{
   if (!pool || index >= pool->count
         || retro_atomic_load_acquire_int(&pool->slot[index].state) != 2) return NULL;
   return &pool->slot[index];
}

static INLINE bool videotoolbox_encode_slots_ack(
      videotoolbox_encode_slots_t *pool, unsigned index)
{
   videotoolbox_encode_slot_t *slot;
   if (!videotoolbox_encode_slots_peek(pool, index)) return false;
   slot = &pool->slot[index];
   if (slot->sample) CFRelease(slot->sample);
   CVPixelBufferRelease(slot->input);
   slot->sample = NULL; slot->input = NULL;
   retro_atomic_store_release_int(&slot->state, 0);
   return true;
}

/* Worker teardown only, AFTER the VT session cannot call/run callbacks.
 * Keep the owner reference until this returns. Ready packets remain borrowed. */
static INLINE void videotoolbox_encode_slots_quiesce(videotoolbox_encode_slots_t *pool)
{
   unsigned i;
   int retired = 0;
   if (!pool) return;
   for (i = 0; i < pool->count; i++)
      if (retro_atomic_load_acquire_int(&pool->slot[i].state) == 1)
      {
         CVPixelBufferRelease(pool->slot[i].input);
         pool->slot[i].input = NULL;
         retro_atomic_store_release_int(&pool->slot[i].state, 0);
         retired++;
      }
   if (retired) retro_atomic_fetch_sub_int(&pool->refs, retired);
}
#endif
#endif
