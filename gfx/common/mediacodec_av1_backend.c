/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_av1_backend.h"
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && defined(RETRO_ATOMIC_HAS_PTR)
#include "mediacodec_frame_export.h"
#include "mediacodec_retire.h"
#include <formats/image.h>
#include <rthreads/retro_eventcount.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct mediacodec_av1_backend_context
{
   mediacodec_av1_backend_t *backend;
   mediacodec_frame_exporter_t *exporter;
   mediacodec_av1_decoder_t *retired;
   retro_eventcount_t wake;
   retro_atomic_int_t state, refs;
} mediacodec_av1_backend_context_t;

static void mediacodec_av1_backend_context_free(
      mediacodec_av1_backend_context_t *c)
{
   mediacodec_av1_backend_t *backend = c->backend;
   if (c->exporter)
      mediacodec_frame_exporter_free(c->exporter);
   retro_eventcount_free(&c->wake);
   free(c);
   retro_atomic_fetch_sub_int(&backend->active, 1);
}

static void mediacodec_av1_backend_context_unref(
      mediacodec_av1_backend_context_t *c)
{
   if (retro_atomic_fetch_sub_int(&c->refs, 1) == 1)
      mediacodec_av1_backend_context_free(c);
}

static void mediacodec_av1_backend_retain(void *opaque)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   retro_atomic_fetch_add_int(&c->refs, 1);
}
static void mediacodec_av1_backend_notify_unref(void *opaque)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   if (retro_atomic_load_acquire_int(&c->state))
      retro_eventcount_notify(&c->wake);
   mediacodec_av1_backend_context_unref(c);
}
static void mediacodec_av1_backend_publish(
      mediacodec_av1_backend_context_t *c, int state)
{
   retro_atomic_store_release_int(&c->state, state);
   retro_eventcount_notify(&c->wake);
   /* The publisher keeps its reference until notification has returned. */
   mediacodec_av1_backend_context_unref(c);
}

static bool mediacodec_av1_backend_release(void *opaque)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   if (c->exporter && !mediacodec_frame_exporter_free(c->exporter))
      return false;
   c->exporter = NULL;
   return true;
}

static int mediacodec_av1_backend_export(void *opaque,
      const android_image_view_t *image,
      const mediacodec_output_format_t *format, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color, video_hw_frame_t *frame)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   return mediacodec_av1_frame_export(
         c->exporter, image, format, sequence, color, frame);
}

static void mediacodec_av1_backend_enqueue(
      void *opaque, mediacodec_av1_decoder_t *decoder)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   c->retired = decoder;
   mediacodec_av1_backend_publish(c, 1);
}

static void *mediacodec_av1_backend_retire(void *opaque)
{
   mediacodec_av1_backend_context_t *c
         = (mediacodec_av1_backend_context_t *)opaque;
   mediacodec_av1_decoder_t *decoder;
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
         if (mediacodec_av1_decoder_retire_poll(decoder))
         {
            retro_eventcount_cancel_wait(&c->wake);
            break;
         }
         fd = mediacodec_av1_decoder_retire_fd(decoder);
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
   mediacodec_av1_backend_context_unref(c);
   return NULL;
}

/* Normalize the admitted sequence OBU into Android av1C during cold setup. */
static uint8_t *mediacodec_av1_backend_config(
      const rav1_sequence_t *sequence, const rav1_obu_t *source, size_t *size)
{
   rav1_sequence_t parsed;
   rav1_obu_t obu;
   uint8_t *config;
   size_t offset = 0, length, used = 5;
   *size = 0;
   if (source->size > 1024u * 1024u
         || rav1_obu_next(&obu, source->data, source->size, &offset, true) != 1
         || offset != source->size || obu.type != 1 || obu.temporal_id
         || obu.spatial_id || obu.payload_size > 1024u * 1024u - 16
         || !rav1_sequence_parse(&parsed, obu.payload, obu.payload_size)
         || memcmp(&parsed, sequence, sizeof(parsed)))
      return NULL;
   config = (uint8_t *)malloc(obu.payload_size + 16);
   if (!config)
      return NULL;
   config[0] = 0x81;
   config[1] = (uint8_t)((sequence->profile << 5)
         | sequence->operating_points[0].level);
   config[2] = (uint8_t)((sequence->operating_points[0].tier << 7)
         | (sequence->bit_depth == 10 ? 0x40 : 0)
         | (sequence->subsampling_x << 3) | (sequence->subsampling_y << 2)
         | sequence->chroma_sample_position);
   /* Sample presentation delay cannot be inferred from frame display delay. */
   config[3] = 0;
   config[4] = 0x0a;
   length = obu.payload_size;
   do
   {
      config[used++] = (uint8_t)((length & 127) | (length > 127 ? 128 : 0));
      length >>= 7;
   } while (length);
   memcpy(config + used, obu.payload, obu.payload_size);
   *size = used + obu.payload_size;
   return config;
}

static bool mediacodec_av1_backend_open(void *opaque,
      const rav1_sequence_t *sequence, const rav1_obu_t *sequence_obu,
      const video_color_metadata_t *color, unsigned slots, rav1_hw_ops_t *ops)
{
   mediacodec_av1_backend_t *backend = (mediacodec_av1_backend_t *)opaque;
   mediacodec_av1_backend_context_t *c;
   mediacodec_av1_import_t importer;
   pthread_t thread;
   pthread_attr_t attr;
   JNIEnv *env;
   int active;
   uint8_t *config;
   size_t config_size;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!backend || !backend->environment || !sequence || !sequence_obu
         || slots < 9 || slots > 32 || sequence->profile || sequence->monochrome
         || sequence->subsampling_x != 1 || sequence->subsampling_y != 1
         || (sequence->bit_depth != 8 && sequence->bit_depth != 10))
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
   c = (mediacodec_av1_backend_context_t *)calloc(1, sizeof(*c));
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
         sequence->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12);
   if (!c->exporter || !mediacodec_frame_exporter_set_retire_notifier(
             c->exporter, c, mediacodec_av1_backend_retain,
             mediacodec_av1_backend_notify_unref))
   {
      mediacodec_av1_backend_context_free(c);
      return false;
   }
   if (pthread_attr_init(&attr))
   {
      mediacodec_av1_backend_context_free(c);
      return false;
   }
   if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
   {
      pthread_attr_destroy(&attr);
      mediacodec_av1_backend_context_free(c);
      return false;
   }
   active = pthread_create(&thread, &attr, mediacodec_av1_backend_retire, c);
   pthread_attr_destroy(&attr);
   if (active)
   {
      mediacodec_av1_backend_context_free(c);
      return false;
   }
   memset(&importer, 0, sizeof(importer));
   importer.context = c;
   importer.export_image = mediacodec_av1_backend_export;
   importer.release = mediacodec_av1_backend_release;
   importer.enqueue = mediacodec_av1_backend_enqueue;
   config = mediacodec_av1_backend_config(sequence, sequence_obu, &config_size);
   if (!config
         || !mediacodec_av1_decoder_open(env, config, config_size, color, slots,
               backend->max_input, &importer, ops))
   {
      free(config);
      mediacodec_av1_backend_publish(c, 2);
      return false;
   }
   free(config);
   return true;
}

#endif

bool mediacodec_av1_backend_init(mediacodec_av1_backend_t *backend,
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
   backend->offer.open_config = mediacodec_av1_backend_open;
   backend->offer.drain = mediacodec_av1_decoder_finish;
   backend->offer.packet_decoder = true;
   backend->offer.close = mediacodec_av1_decoder_close;
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

unsigned mediacodec_av1_backend_active(mediacodec_av1_backend_t *backend)
{
   return backend ? (unsigned)retro_atomic_load_acquire_int(&backend->active)
                  : 0;
}
