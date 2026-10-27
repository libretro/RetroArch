/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_vp9_backend.h"
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
#include "mediacodec_frame_export.h"
#include "mediacodec_retire.h"
#include <formats/image.h>
#include <rthreads/retro_eventcount.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct mediacodec_vp9_backend_context
{
   mediacodec_vp9_backend_t *backend;
   mediacodec_frame_exporter_t *exporter;
   mediacodec_vp9_decoder_t *retired;
   retro_eventcount_t wake;
   retro_atomic_int_t state, refs;
} mediacodec_vp9_backend_context_t;

static void mediacodec_vp9_backend_context_free(
      mediacodec_vp9_backend_context_t *c)
{
   mediacodec_vp9_backend_t *backend = c->backend;
   if (c->exporter)
      mediacodec_frame_exporter_free(c->exporter);
   retro_eventcount_free(&c->wake);
   free(c);
   retro_atomic_fetch_sub_int(&backend->active, 1);
}

static void mediacodec_vp9_backend_context_unref(
      mediacodec_vp9_backend_context_t *c)
{
   if (retro_atomic_fetch_sub_int(&c->refs, 1) == 1)
      mediacodec_vp9_backend_context_free(c);
}

static void mediacodec_vp9_backend_retain(void *opaque)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   retro_atomic_fetch_add_int(&c->refs, 1);
}
static void mediacodec_vp9_backend_notify_unref(void *opaque)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   if (retro_atomic_load_acquire_int(&c->state))
      retro_eventcount_notify(&c->wake);
   mediacodec_vp9_backend_context_unref(c);
}
static void mediacodec_vp9_backend_publish(
      mediacodec_vp9_backend_context_t *c, int state)
{
   retro_atomic_store_release_int(&c->state, state);
   retro_eventcount_notify(&c->wake);
   /* The publisher keeps its reference until notification has returned. */
   mediacodec_vp9_backend_context_unref(c);
}

static bool mediacodec_vp9_backend_release(void *opaque)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   if (c->exporter && !mediacodec_frame_exporter_free(c->exporter))
      return false;
   c->exporter = NULL;
   return true;
}

static int mediacodec_vp9_backend_export(void *opaque,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rvp9_hdr *header,
      const video_color_metadata_t *color, video_hw_frame_t *frame)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   return mediacodec_vp9_frame_export(
         c->exporter, image, format, header, color, frame);
}

static void mediacodec_vp9_backend_enqueue(
      void *opaque, mediacodec_vp9_decoder_t *decoder)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   c->retired = decoder;
   mediacodec_vp9_backend_publish(c, 1);
}

static void *mediacodec_vp9_backend_retire(void *opaque)
{
   mediacodec_vp9_backend_context_t *c
         = (mediacodec_vp9_backend_context_t *)opaque;
   mediacodec_vp9_decoder_t *decoder;
   int key;
   while (!retro_atomic_load_acquire_int(&c->state))
   {
      key = retro_eventcount_prepare_wait(&c->wake);
      if (retro_atomic_load_acquire_int(&c->state))
      {
         retro_eventcount_cancel_wait(&c->wake);
         break;
      }
      retro_eventcount_commit_wait(&c->wake, key);
   }
   decoder = c->retired;
   if (decoder)
      for (;;)
      {
         int fd;
         key = retro_eventcount_prepare_wait(&c->wake);
         if (mediacodec_vp9_decoder_retire_poll(decoder))
         {
            retro_eventcount_cancel_wait(&c->wake);
            break;
         }
         fd = mediacodec_vp9_decoder_retire_fd(decoder);
         if (fd >= 0)
         {
            retro_eventcount_cancel_wait(&c->wake);
            /* An unprovable fence retains the bounded owner. */
            if (!mediacodec_retire_fence(fd))
               return NULL;
         }
         else
            retro_eventcount_commit_wait(&c->wake, key);
      }
   mediacodec_vp9_backend_context_unref(c);
   return NULL;
}

static bool mediacodec_vp9_backend_open(void *opaque, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, rvp9_hw_ops_t *ops)
{
   mediacodec_vp9_backend_t *backend = (mediacodec_vp9_backend_t *)opaque;
   mediacodec_vp9_backend_context_t *c;
   mediacodec_vp9_import_t importer;
   pthread_t thread;
   pthread_attr_t attr;
   JNIEnv *env;
   int active;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!backend || !backend->environment || !header || slots < 10 || slots > 32
         || header->subsampling_x != 1 || header->subsampling_y != 1
         || (header->profile != 0 && header->profile != 2)
         || header->bit_depth != (header->profile == 2 ? 10 : 8))
      return false;
   for (;;)
   {
      active = retro_atomic_load_acquire_int(&backend->active);
      if (active < 0 || (unsigned)active >= backend->max_sessions)
         return false;
      if (retro_atomic_cas_int(&backend->active, active, active + 1))
         break;
   }
   env = backend->environment(backend->user);
   if (!env)
   {
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   c = (mediacodec_vp9_backend_context_t *)calloc(1, sizeof(*c));
   if (!c)
   {
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   c->backend = backend;
   if (!retro_eventcount_init(&c->wake))
   {
      free(c);
      retro_atomic_fetch_sub_int(&backend->active, 1);
      return false;
   }
   retro_atomic_int_init(&c->state, 0);
   retro_atomic_int_init(&c->refs, 2);
   c->exporter = mediacodec_frame_exporter_new(slots,
         header->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12);
   if (!c->exporter || !mediacodec_frame_exporter_set_retire_notifier(
             c->exporter, c, mediacodec_vp9_backend_retain,
             mediacodec_vp9_backend_notify_unref))
   {
      mediacodec_vp9_backend_context_free(c);
      return false;
   }
   if (pthread_attr_init(&attr))
   {
      mediacodec_vp9_backend_context_free(c);
      return false;
   }
   if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
   {
      pthread_attr_destroy(&attr);
      mediacodec_vp9_backend_context_free(c);
      return false;
   }
   active = pthread_create(&thread, &attr, mediacodec_vp9_backend_retire, c);
   pthread_attr_destroy(&attr);
   if (active)
   {
      mediacodec_vp9_backend_context_free(c);
      return false;
   }
   memset(&importer, 0, sizeof(importer));
   importer.context = c;
   importer.export_image = mediacodec_vp9_backend_export;
   importer.release = mediacodec_vp9_backend_release;
   importer.enqueue = mediacodec_vp9_backend_enqueue;
   if (!mediacodec_vp9_decoder_open_color(
             env, header, color, slots, backend->max_input, &importer, ops))
   {
      mediacodec_vp9_backend_publish(c, 2);
      return false;
   }
   return true;
}

#endif

bool mediacodec_vp9_backend_init(mediacodec_vp9_backend_t *backend,
      JNIEnv *(*environment)(void *), void *user, unsigned max_input,
      unsigned max_sessions)
{
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
   if (!backend || !environment || !max_input || max_input > 64u * 1024u * 1024u
         || !max_sessions || max_sessions > 8)
      return false;
   memset(backend, 0, sizeof(*backend));
   backend->user = user;
   backend->environment = environment;
   backend->max_input = max_input;
   backend->max_sessions = max_sessions;
   retro_atomic_int_init(&backend->active, 0);
   backend->offer.user = backend;
   backend->offer.open_color = mediacodec_vp9_backend_open;
   backend->offer.drain = mediacodec_vp9_decoder_finish;
   backend->offer.packet_decoder = true;
   backend->offer.close = mediacodec_vp9_decoder_close;
   return true;
#else
   (void)backend;
   (void)environment;
   (void)user;
   (void)max_input;
   (void)max_sessions;
   return false;
#endif
}

unsigned mediacodec_vp9_backend_active(mediacodec_vp9_backend_t *backend)
{
   return backend ? (unsigned)retro_atomic_load_acquire_int(&backend->active)
                  : 0;
}
