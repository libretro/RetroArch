/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/mediacodec_hevc_backend.h"
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <assert.h>
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <retro_timers.h>
#include <pthread.h>
#include <errno.h>
struct mediacodec_hevc_decoder
{
   mediacodec_hevc_import_t importer;
};
static retro_atomic_int_t retire_gate, polls, retired, worker_hold, open_hold,
      open_entered;
typedef struct backend_test_start
{
   void *(*run)(void *);
   void *user;
} backend_test_start_t;
static void *backend_test_start(void *opaque)
{
   backend_test_start_t start = *(backend_test_start_t *)opaque;
   free(opaque);
   while (retro_atomic_load_acquire_int(&worker_hold))
      retro_sleep(1);
   return start.run(start.user);
}
static uintptr_t caller_thread;
static int native_fail, env_fail, native_opens, image_token, deleted,
      thread_fail, detach_fail, attr_fail, event_fail, notify_race;
static int backend_test_pthread_create(pthread_t *thread,
      const pthread_attr_t *attr, void *(*run)(void *), void *user)
{
   backend_test_start_t *start;
   int state, result;
   assert(attr && !pthread_attr_getdetachstate(attr, &state)
         && state == PTHREAD_CREATE_DETACHED);
   if (thread_fail)
      return EAGAIN;
   start = (backend_test_start_t *)malloc(sizeof(*start));
   assert(start);
   start->run = run;
   start->user = user;
   result = pthread_create(thread, attr, backend_test_start, start);
   if (result)
      free(start);
   return result;
}
static int backend_test_pthread_attr_init(pthread_attr_t *attr)
{
   return attr_fail ? ENOMEM : pthread_attr_init(attr);
}
static int backend_test_pthread_attr_setdetachstate(
      pthread_attr_t *attr, int state)
{
   return detach_fail ? EINVAL : pthread_attr_setdetachstate(attr, state);
}
static bool backend_test_eventcount_init(retro_eventcount_t *ec)
{
   return event_fail ? false : retro_eventcount_init(ec);
}
static void backend_test_notify(retro_eventcount_t *ec);
static int image_acquire(void *reader, void **image, int *fd)
{
   assert(reader == &image_token);
   *image = &image_token;
   *fd = -1;
   return 1;
}
static bool image_describe(void *image, android_image_view_t *view)
{
   assert(image == &image_token);
   view->buffer = image;
   view->width = view->right = view->buffer_width = 64;
   view->height = view->bottom = view->buffer_height = 32;
   return true;
}
static void image_delete(void *image)
{
   assert(image == &image_token);
   deleted++;
}
static void reader_delete(void *reader)
{
   assert(reader == &image_token);
}
static JNIEnv *environment(void *user)
{
   assert(user == &native_opens);
   return env_fail ? NULL : (JNIEnv *)&native_opens;
}
bool mediacodec_hevc_decoder_open(JNIEnv *env, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, unsigned max_input,
      const mediacodec_hevc_import_t *importer, rh265_hw_ops_t *ops)
{
   struct mediacodec_hevc_decoder *decoder;
   assert(env && source && params && slots == 2 && max_input == 1048576
         && importer);
   native_opens++;
   if (native_fail)
      return false;
   if (retro_atomic_load_acquire_int(&open_hold))
   {
      retro_atomic_store_release_int(&open_entered, 1);
      while (retro_atomic_load_acquire_int(&open_hold))
         retro_sleep(1);
   }
   decoder = (struct mediacodec_hevc_decoder *)calloc(1, sizeof(*decoder));
   assert(decoder);
   decoder->importer = *importer;
   ops->decoder = decoder;
   return true;
}
void mediacodec_hevc_decoder_close(rh265_hw_ops_t *ops)
{
   struct mediacodec_hevc_decoder *decoder
         = (struct mediacodec_hevc_decoder *)ops->decoder;
   if (!decoder)
      return;
   memset(ops, 0, sizeof(*ops));
   decoder->importer.enqueue(decoder->importer.context, decoder);
}
int mediacodec_hevc_decoder_retire_fd(const mediacodec_hevc_decoder_t *decoder)
{
   (void)decoder;
   return -1;
}
bool mediacodec_hevc_decoder_retire_poll(mediacodec_hevc_decoder_t *decoder)
{
   assert(sthread_get_current_thread_id() != caller_thread);
   retro_atomic_fetch_add_int(&polls, 1);
   if (!retro_atomic_load_acquire_int(&retire_gate))
      return false;
   if (!decoder->importer.release(decoder->importer.context))
      return false;
   free(decoder);
   retro_atomic_fetch_add_int(&retired, 1);
   return true;
}
#define pthread_create backend_test_pthread_create
#define pthread_attr_init backend_test_pthread_attr_init
#define pthread_attr_setdetachstate backend_test_pthread_attr_setdetachstate
#define retro_eventcount_init backend_test_eventcount_init
#define retro_eventcount_notify backend_test_notify
#include "../../../gfx/common/mediacodec_hevc_backend.c"
#undef pthread_create
#undef pthread_attr_init
#undef pthread_attr_setdetachstate
#undef retro_eventcount_init
#undef retro_eventcount_notify
static void backend_test_notify(retro_eventcount_t *ec)
{
   mediacodec_hevc_backend_context_t *c;
   unsigned i;
   retro_eventcount_notify(ec);
   if (!notify_race)
      return;
   c = (mediacodec_hevc_backend_context_t *)((char *)ec
         - offsetof(mediacodec_hevc_backend_context_t, wake));
   for (i = 0; i < 2000 && retro_atomic_load_acquire_int(&c->refs) != 1; i++)
      retro_sleep(1);
   assert(retro_atomic_load_acquire_int(&c->refs) == 1);
}
static void wait_idle(mediacodec_hevc_backend_t *backend)
{
   unsigned i;
   for (i = 0; i < 2000 && mediacodec_hevc_backend_active(backend); i++)
      retro_sleep(1);
   assert(!mediacodec_hevc_backend_active(backend));
}
#include "registry_test.h"
int main(void)
{
   static mediacodec_hevc_backend_t backend;
   static rh265_video_params_t params;
   rh265_hw_ops_t a, b, rejected;
   const rh265_video *source = (const rh265_video *)&native_opens;
   unsigned i;
   int opens;
   android_image_ops_t image_ops;
   android_image_queue_t *images;
   android_image_view_t image;
   mediacodec_output_format_t format;
   rh265_video_color_t color;
   video_hw_frame_t frame;
   void *release_a, *release_b;
   struct mediacodec_hevc_decoder *decoder;
   caller_thread = sthread_get_current_thread_id();
   retro_atomic_int_init(&retire_gate, 0);
   retro_atomic_int_init(&polls, 0);
   retro_atomic_int_init(&retired, 0);
   retro_atomic_int_init(&worker_hold, 0);
   retro_atomic_int_init(&open_hold, 0);
   retro_atomic_int_init(&open_entered, 0);
   assert(!mediacodec_hevc_backend_init(
         &backend, environment, &native_opens, 1048576, 9));
   assert(mediacodec_hevc_backend_init(
         &backend, environment, &native_opens, 1048576, 2));
   memset(&params, 0, sizeof(params));
   params.chroma_format_idc = 1;
   params.bit_depth = 8;
   env_fail = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a)
         && !mediacodec_hevc_backend_active(&backend));
   env_fail = 0;
   thread_fail = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a)
         && !mediacodec_hevc_backend_active(&backend));
   thread_fail = 0;
   event_fail = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a)
         && !mediacodec_hevc_backend_active(&backend));
   event_fail = 0;
   attr_fail = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a)
         && !mediacodec_hevc_backend_active(&backend));
   attr_fail = 0;
   retro_atomic_store_release_int(&retire_gate, 1);
   detach_fail = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a)
         && !a.decoder && !mediacodec_hevc_backend_active(&backend));
   detach_fail = 0;
   retro_atomic_store_release_int(&retire_gate, 0);
   retro_atomic_store_release_int(&retired, 0);
   native_fail = 1;
   retro_atomic_store_release_int(&worker_hold, 1);
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a));
   assert(mediacodec_hevc_backend_active(&backend) == 1);
   retro_atomic_store_release_int(&worker_hold, 0);
   wait_idle(&backend);
   notify_race = 1;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &a));
   wait_idle(&backend);
   notify_race = 0;
   native_fail = 0;
   assert(backend.offer.open(backend.offer.user, source, &params, 2, &a));
   params.bit_depth = 10;
   assert(backend.offer.open(backend.offer.user, source, &params, 2, &b));
   assert(mediacodec_hevc_backend_active(&backend) == 2);
   opens = native_opens;
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &rejected)
         && native_opens == opens);
   release_a = ((struct mediacodec_hevc_decoder *)a.decoder)->importer.context;
   release_b = ((struct mediacodec_hevc_decoder *)b.decoder)->importer.context;
   mediacodec_hevc_backend_retain(release_a);
   mediacodec_hevc_backend_retain(release_b);
   backend.offer.close(&a);
   backend.offer.close(&b);
   assert(!a.decoder && !b.decoder);
   for (i = 0; i < 1000 && retro_atomic_load_acquire_int(&polls) < 2; i++)
      retro_sleep(1);
   assert(retro_atomic_load_acquire_int(&polls) >= 2
         && !retro_atomic_load_acquire_int(&retired));
   assert(!backend.offer.open(backend.offer.user, source, &params, 2, &rejected)
         && native_opens == opens);
   retro_atomic_store_release_int(&retire_gate, 1);
   mediacodec_hevc_backend_notify_unref(release_a);
   mediacodec_hevc_backend_notify_unref(release_b);
   wait_idle(&backend);
   assert(retro_atomic_load_acquire_int(&retired) == 2);
   /* Actual exported frame-pool lease holds a closed epoch alive. No native
    * decoder mock or exporter release is permitted to consume a live image. */
   assert(backend.offer.open(backend.offer.user, source, &params, 2, &a));
   decoder = (struct mediacodec_hevc_decoder *)a.decoder;
   memset(&image_ops, 0, sizeof(image_ops));
   image_ops.acquire = image_acquire;
   image_ops.describe = image_describe;
   image_ops.delete_image = image_delete;
   image_ops.delete_reader = reader_delete;
   images = android_image_queue_new(&image_token, 1, &image_ops);
   assert(images);
   assert(android_image_queue_acquire(images, &image) == 1);
   memset(&format, 0, sizeof(format));
   format.serial = 1;
   memset(&color, 0, sizeof(color));
   color.description_present = color.video_signal_present = 1;
   color.primaries = color.transfer = color.matrix = 1;
   assert(!decoder->importer.export_image(
         decoder->importer.context, &image, &format, &color, &frame));
   assert(frame.pixfmt == IMAGE_PIXFMT_P010);
   opens = retro_atomic_load_acquire_int(&polls);
   backend.offer.close(&a);
   for (i = 0; i < 2000 && retro_atomic_load_acquire_int(&polls) == opens; i++)
      retro_sleep(1);
   assert(retro_atomic_load_acquire_int(&polls) > opens);
   opens = retro_atomic_load_acquire_int(&polls);
   retro_sleep(50);
   assert(retro_atomic_load_acquire_int(&polls) == opens);
   assert(mediacodec_hevc_backend_active(&backend) == 1
         && retro_atomic_load_acquire_int(&retired) == 2);
   notify_race = 1;
   video_hw_frame_abandon(&frame);
   wait_idle(&backend);
   notify_race = 0;
   assert(android_image_queue_poll(images) == 1 && deleted == 1);
   android_image_queue_close(images);
   assert(android_image_queue_free(images));
   notify_race = 1;
   for (i = 0; i < 40; i++)
   {
      assert(backend.offer.open(backend.offer.user, source, &params, 2, &a));
      backend.offer.close(&a);
      wait_idle(&backend);
   }
   assert(retro_atomic_load_acquire_int(&retired) == 43);
   notify_race = 0;
   test_registry();
   puts("PASS: HEVC backend detached startup, eventcount retirement, notifier "
        "lifetime, bounded admission and repeated seek epochs");
   return 0;
}
