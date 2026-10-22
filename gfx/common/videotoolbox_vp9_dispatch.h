/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_DISPATCH_H
#define RETROARCH_VIDEOTOOLBOX_VP9_DISPATCH_H
#include "videotoolbox_vp9_decoder.h"
#ifdef VIDEOTOOLBOX_VP9_OWNER
#if defined(__APPLE__) && !defined(__cplusplus) && !defined(inline)            \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)
#define inline __inline__
#define VIDEOTOOLBOX_VP9_DISPATCH_INLINE
#endif
#include <dispatch/dispatch.h>
#ifdef VIDEOTOOLBOX_VP9_DISPATCH_INLINE
#undef inline
#undef VIDEOTOOLBOX_VP9_DISPATCH_INLINE
#endif
#if defined(__has_feature)
#if __has_feature(objc_arc)
#define VIDEOTOOLBOX_VP9_DISPATCH_ARC 1
#endif
#endif

typedef struct videotoolbox_vp9_dispatch
{
   dispatch_queue_t queue;
   dispatch_source_t signal, timer;
   videotoolbox_vp9_decoder_t *decoder;
   retro_atomic_int_t closing;
   unsigned cancellations;
} videotoolbox_vp9_dispatch_t;

static void videotoolbox_vp9_dispatch_dispose(void *opaque)
{
   videotoolbox_vp9_dispatch_t *e = (videotoolbox_vp9_dispatch_t *)opaque;
   if (--e->cancellations)
      return;
#ifdef VIDEOTOOLBOX_VP9_DISPATCH_ARC
   e->signal = NULL;
   e->timer  = NULL;
   e->queue  = NULL;
#else
   if (e->signal)
      dispatch_release(e->signal);
   if (e->timer)
      dispatch_release(e->timer);
   dispatch_release(e->queue);
#endif
   free(e);
}

static void videotoolbox_vp9_dispatch_cancel(videotoolbox_vp9_dispatch_t *e)
{
   dispatch_source_t signal = e->signal, timer = e->timer;
#ifndef VIDEOTOOLBOX_VP9_DISPATCH_ARC
   if (signal)
      dispatch_retain(signal);
   if (timer)
      dispatch_retain(timer);
#endif
   if (signal)
      dispatch_source_cancel(signal);
   if (timer)
      dispatch_source_cancel(timer);
#ifndef VIDEOTOOLBOX_VP9_DISPATCH_ARC
   if (signal)
      dispatch_release(signal);
   if (timer)
      dispatch_release(timer);
#endif
}

static void videotoolbox_vp9_dispatch_service(void *opaque)
{
   videotoolbox_vp9_dispatch_t *e = (videotoolbox_vp9_dispatch_t *)opaque;
   videotoolbox_vp9_decoder_t *d  = e->decoder;
   if (!d)
      return;
   if (retro_atomic_load_acquire_int(&e->closing))
   {
      if (videotoolbox_vp9_decoder_retire_poll(&e->decoder))
         videotoolbox_vp9_dispatch_cancel(e);
      else
         dispatch_source_set_timer(e->timer,
               dispatch_time(DISPATCH_TIME_NOW, 16 * NSEC_PER_MSEC),
               DISPATCH_TIME_FOREVER, 0);
      return;
   }
   videotoolbox_vp9_owner_worker(d->owner);
   if (retro_spsc_read_avail(&d->owner->queue) >= sizeof(unsigned))
      dispatch_source_merge_data(e->signal, 1);
}

static void videotoolbox_vp9_dispatch_wake(void *opaque)
{
   videotoolbox_vp9_dispatch_t *e = (videotoolbox_vp9_dispatch_t *)opaque;
   dispatch_source_merge_data(e->signal, 1);
}

/* Retain the source before publishing close: service may finish immediately.
 * After publishing, the executor context is no longer accessed here. */
static void videotoolbox_vp9_dispatch_retire(
      void *opaque, videotoolbox_vp9_decoder_t *d)
{
   videotoolbox_vp9_dispatch_t *e = (videotoolbox_vp9_dispatch_t *)opaque;
   dispatch_source_t signal       = e->signal;
   (void)d;
#ifndef VIDEOTOOLBOX_VP9_DISPATCH_ARC
   dispatch_retain(signal);
#endif
   retro_atomic_store_release_int(&e->closing, 1);
   dispatch_source_merge_data(signal, 1);
#ifndef VIDEOTOOLBOX_VP9_DISPATCH_ARC
   dispatch_release(signal);
#endif
}

/* Cold worker setup; successful construction takes pool ownership. */
static INLINE bool videotoolbox_vp9_dispatch_open(const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma, unsigned count,
      size_t capacity, metal_frame_cv_pool_t *pool,
      videotoolbox_vp9_register_t registration, rvp9_hw_ops_t *ops)
{
   videotoolbox_vp9_dispatch_t *e;
   dispatch_source_t sources[2];
   unsigned i;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   e = (videotoolbox_vp9_dispatch_t *)calloc(1, sizeof(*e));
   if (!e)
      return false;
   retro_atomic_int_init(&e->closing, 0);
   e->queue =
         dispatch_queue_create("org.libretro.metal.vp9", DISPATCH_QUEUE_SERIAL);
   if (!e->queue)
   {
      free(e);
      return false;
   }
   e->signal =
         dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, e->queue);
   e->timer =
         dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, e->queue);
   sources[0] = e->signal;
   sources[1] = e->timer;
   for (i = 0; i < 2; i++)
      if (sources[i])
      {
         e->cancellations++;
         dispatch_set_context(sources[i], e);
         dispatch_source_set_event_handler_f(
               sources[i], videotoolbox_vp9_dispatch_service);
         dispatch_source_set_cancel_handler_f(
               sources[i], videotoolbox_vp9_dispatch_dispose);
         if (i == 1)
            dispatch_source_set_timer(
                  sources[i], DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
         dispatch_resume(sources[i]);
      }
   if (!e->cancellations)
   {
#ifdef VIDEOTOOLBOX_VP9_DISPATCH_ARC
      e->queue = NULL;
#else
      dispatch_release(e->queue);
#endif
      free(e);
      return false;
   }
   if (!e->signal || !e->timer
         || !videotoolbox_vp9_decoder_open(header, color, chroma, count,
               capacity, pool, registration, videotoolbox_vp9_dispatch_wake,
               videotoolbox_vp9_dispatch_retire, e, ops))
   {
      videotoolbox_vp9_dispatch_cancel(e);
      return false;
   }
   e->decoder = (videotoolbox_vp9_decoder_t *)ops->decoder;
   return true;
}
#endif
#endif
