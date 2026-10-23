/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error Dispatcher ownership acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_dispatch_mediacodec.h"
#ifndef _WIN32
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <retro_atomic.h>
static retro_atomic_int_t fault, env_fault, startup_fault, output_fault;
static retro_atomic_int_t inputs, live, descriptors;
static retro_atomic_int_t hold_start;
static void pause_test(void);
struct record_session_mediacodec
{
   pthread_t owner;
   bool audio_closed, gpu_done, gpu_released, cancelled;
};
static int test_eventfd(unsigned int initial, int flags)
{
   int fd;
   if (retro_atomic_load_acquire_int(&fault) == 1)
      return -1;
   fd = eventfd(initial, flags);
   if (fd >= 0)
      retro_atomic_fetch_add_int(&descriptors, 1);
   return fd;
}
static int test_close(int fd)
{
   retro_atomic_fetch_add_int(&descriptors, -1);
   return close(fd);
}
static int test_create(pthread_t *thread, const pthread_attr_t *attr,
      void *(*run)(void *), void *context)
{
   if (retro_atomic_load_acquire_int(&fault) == 2)
      return EAGAIN;
   return pthread_create(thread, attr, run, context);
}
static int test_detach(pthread_t thread)
{
   return retro_atomic_load_acquire_int(&fault) == 3 ? EINVAL
                                                     : pthread_detach(thread);
}
static ssize_t test_write(int fd, const void *data, size_t bytes)
{
   if (retro_atomic_load_acquire_int(&fault) == 4)
   {
      retro_atomic_store_release_int(&fault, 0);
      errno = EIO;
      return -1;
   }
   return write(fd, data, bytes);
}
static int test_poll(struct pollfd *fds, nfds_t count, int timeout)
{
   if (retro_atomic_load_acquire_int(&fault) == 5)
   {
      retro_atomic_store_release_int(&fault, 0);
      errno = EIO;
      return -1;
   }
   return poll(fds, count, timeout);
}
#define eventfd test_eventfd
#define close test_close
#define pthread_create test_create
#define pthread_detach test_detach
#define write test_write
#define poll test_poll
#include "../../../record/record_dispatch_mediacodec.c"
#undef eventfd
#undef close
#undef pthread_create
#undef pthread_detach
#undef write
#undef poll

JNIEnv *rjni_env(void)
{
   static JNIEnv dummy;
   return retro_atomic_load_acquire_int(&env_fault) ? NULL : &dummy;
}
static void owner(record_session_mediacodec_t *s)
{
   assert(s && pthread_equal(pthread_self(), s->owner));
}
record_session_mediacodec_t *record_session_mediacodec_new(JNIEnv *env,
      const mediacodec_encoder_request_t *v, unsigned interval, unsigned rate,
      unsigned channels, unsigned bitrate, size_t max_frames, size_t capacity,
      const char *path)
{
   record_session_mediacodec_t *s;
   assert(env && !strcmp(v->mime, "video/hevc") && !strcmp(path, "record.mp4"));
   assert(v->width == 320 && v->height == 192 && interval == 2 && rate == 48000
         && channels == 2 && bitrate == 128000 && max_frames == 1024
         && capacity == 65536);
   if (retro_atomic_load_acquire_int(&startup_fault))
      return NULL;
   while (retro_atomic_load_acquire_int(&hold_start))
      pause_test();
   s = (record_session_mediacodec_t *)calloc(1, sizeof(*s));
   assert(s);
   s->owner = pthread_self();
   retro_atomic_fetch_add_int(&live, 1);
   return s;
}
void *record_session_mediacodec_surface(record_session_mediacodec_t *s)
{
   owner(s);
   return s;
}
int record_session_mediacodec_audio(record_session_mediacodec_t *s,
      const int16_t *samples, size_t frames, uint64_t timestamp)
{
   assert(s && !s->audio_closed && samples && frames == 2 && timestamp < 2000);
   retro_atomic_fetch_add_int(&inputs, 1);
   return RECORD_AUDIO_OK;
}
bool record_session_mediacodec_audio_finish(
      record_session_mediacodec_t *s, uint64_t end)
{
   owner(s);
   assert(end == 2000 || !end);
   s->audio_closed = true;
   return true;
}
bool record_session_mediacodec_gpu_retired(
      record_session_mediacodec_t *s, int64_t end)
{
   owner(s);
   assert(end == 100000 || !end);
   s->gpu_done = true;
   return true;
}
bool record_session_mediacodec_gpu_released(record_session_mediacodec_t *s)
{
   owner(s);
   assert(s->gpu_done);
   s->gpu_released = true;
   return true;
}
void record_session_mediacodec_cancel(record_session_mediacodec_t *s)
{
   owner(s);
   s->cancelled = true;
}
int record_session_mediacodec_step(record_session_mediacodec_t *s)
{
   owner(s);
   if (!s->audio_closed || !s->gpu_done || !s->gpu_released)
      return RECORD_MC_WORKER_BUSY;
   return s->cancelled || retro_atomic_load_acquire_int(&output_fault)
         ? RECORD_MC_WORKER_ERROR
         : RECORD_MC_WORKER_END;
}
enum record_session_mediacodec_error record_session_mediacodec_error(
      const record_session_mediacodec_t *s, int *error)
{
   *error = retro_atomic_load_acquire_int(&output_fault) ? 5 : 0;
   return *error        ? RECORD_MC_SESSION_FILE
         : s->cancelled ? RECORD_MC_SESSION_CANCELLED
                        : RECORD_MC_SESSION_NONE;
}
bool record_session_mediacodec_free(record_session_mediacodec_t *s)
{
   owner(s);
   assert(s->audio_closed && s->gpu_done && s->gpu_released);
   retro_atomic_fetch_add_int(&live, -1);
   free(s);
   return true;
}
static void pause_test(void)
{
   struct timespec duration;
   duration.tv_sec = 0;
   duration.tv_nsec = 1000000;
   nanosleep(&duration, NULL);
}
static void wait_state(record_dispatch_mediacodec_t *j,
      enum record_dispatch_mediacodec_state expected)
{
   unsigned at;
   for (at = 0; at < 10000; at++)
   {
      if (record_dispatch_mediacodec_status(j, NULL, NULL, NULL) == expected)
         return;
      pause_test();
   }
   assert(0);
}
static void collect(record_dispatch_mediacodec_t *j)
{
   unsigned at;
   for (at = 0; at < 10000; at++)
   {
      if (record_dispatch_mediacodec_free(j))
         break;
      pause_test();
   }
   assert(at < 10000 && !retro_atomic_load_acquire_int(&live)
         && !retro_atomic_load_acquire_int(&descriptors));
}
static record_dispatch_mediacodec_t *start(void)
{
   mediacodec_encoder_request_t v = {0};
   record_dispatch_mediacodec_t *j;
   char mime[] = "video/hevc", path[] = "record.mp4";
   v.mime = mime;
   v.width = 320;
   v.height = 192;
   j = record_dispatch_mediacodec_new(
         &v, 2, 48000, 2, 128000, 1024, 65536, path);
   memset(mime, 0, sizeof(mime));
   memset(path, 0, sizeof(path));
   return j;
}
static void *pcm_producer(void *data)
{
   record_dispatch_mediacodec_t *j = (record_dispatch_mediacodec_t *)data;
   static const int16_t pcm[4] = {1, 2, 3, 4};
   unsigned i;
   for (i = 0; i < 1000; i++)
      assert(record_dispatch_mediacodec_audio(j, pcm, 2, i * 2)
            == RECORD_AUDIO_OK);
   assert(record_dispatch_mediacodec_audio_finish(j, 2000));
   return NULL;
}
static void *gpu_executor(void *data)
{
   record_dispatch_mediacodec_t *j = (record_dispatch_mediacodec_t *)data;
   assert(record_dispatch_mediacodec_surface(j));
   assert(record_dispatch_mediacodec_gpu_retired(j, 100000));
   assert(!record_dispatch_mediacodec_gpu_retired(j, 100001));
   assert(record_dispatch_mediacodec_gpu_released(j));
   return NULL;
}
int main(void)
{
   record_dispatch_mediacodec_t *j;
   pthread_t producer, gpu;
   int i, error, detail, system;
   unsigned width, height, color;
   retro_atomic_int_init(&fault, 0);
   retro_atomic_int_init(&env_fault, 0);
   retro_atomic_int_init(&startup_fault, 0);
   retro_atomic_int_init(&output_fault, 0);
   retro_atomic_int_init(&inputs, 0);
   retro_atomic_int_init(&live, 0);
   retro_atomic_int_init(&descriptors, 0);
   retro_atomic_int_init(&hold_start, 0);
   for (i = 1; i <= 2; i++)
   {
      retro_atomic_store_release_int(&fault, i);
      assert(!start() && !retro_atomic_load_acquire_int(&descriptors));
   }
   retro_atomic_store_release_int(&fault, 0);
   retro_atomic_store_release_int(&hold_start, 1);
   j = start();
   assert(j
         && record_dispatch_mediacodec_audio(j, NULL, 2, 0)
               == RECORD_AUDIO_BUSY);
   assert(!record_dispatch_mediacodec_surface(j));
   assert(record_dispatch_mediacodec_audio_finish(j, 0));
   assert(record_dispatch_mediacodec_gpu_retired(j, 0));
   assert(record_dispatch_mediacodec_gpu_released(j));
   record_dispatch_mediacodec_cancel(j);
   retro_atomic_store_release_int(&hold_start, 0);
   wait_state(j, RECORD_MC_DISPATCH_FAILED);
   collect(j);
   for (i = 0; i < 64; i++)
   {
      retro_atomic_store_release_int(&fault, i == 0 ? 3 : 0);
      j = start();
      assert(j);
      wait_state(j, RECORD_MC_DISPATCH_READY);
      assert(record_dispatch_mediacodec_video_format(j, &width, &height, &color)
            && width == 320 && height == 192 && !color);
      assert(
            !record_dispatch_mediacodec_video_format(j, NULL, &height, &color));
      assert(!record_dispatch_mediacodec_free(j));
      assert(record_dispatch_mediacodec_gpu_claim(j));
      assert(!record_dispatch_mediacodec_gpu_claim(j));
      assert(!record_dispatch_mediacodec_gpu_released(j));
      assert(!pthread_create(&producer, NULL, pcm_producer, j));
      assert(!pthread_create(&gpu, NULL, gpu_executor, j));
      assert(!pthread_join(producer, NULL) && !pthread_join(gpu, NULL));
      wait_state(j, RECORD_MC_DISPATCH_DONE);
      assert(!record_dispatch_mediacodec_free(j));
      record_dispatch_mediacodec_gpu_release_claim(j);
      assert(!record_dispatch_mediacodec_gpu_claim(j));
      record_dispatch_mediacodec_status(j, &error, &detail, &system);
      assert(!error && !detail && !system);
      collect(j);
   }
   assert(retro_atomic_load_acquire_int(&inputs) == 64000);
   for (i = 0; i < 6; i++)
   {
      unsigned at;
      retro_atomic_store_release_int(&fault, i >= 4 ? i : 0);
      retro_atomic_store_release_int(&env_fault, i == 1);
      retro_atomic_store_release_int(&startup_fault, i == 2);
      retro_atomic_store_release_int(&output_fault, i == 3);
      j = start();
      assert(j);
      if (!i)
         record_dispatch_mediacodec_cancel(j);
      if (i == 4)
      {
         for (at = 0; at < 10000; at++)
         {
            if (retro_atomic_load_acquire_int(&j->epoch) & 1)
               break;
            pause_test();
         }
         assert(at < 10000);
         record_dispatch_mediacodec_notify(j);
      }
      wait_state(j, RECORD_MC_DISPATCH_DRAINING);
      assert(record_dispatch_mediacodec_cancelled(j));
      assert(!record_dispatch_mediacodec_free(j));
      assert(record_dispatch_mediacodec_audio_finish(j, 0));
      assert(record_dispatch_mediacodec_gpu_retired(j, 0));
      pause_test();
      assert(!record_dispatch_mediacodec_free(j));
      assert(record_dispatch_mediacodec_gpu_released(j));
      wait_state(j, RECORD_MC_DISPATCH_FAILED);
      record_dispatch_mediacodec_status(j, &error, &detail, &system);
      assert(error && system == (i >= 4 ? EIO : 0)
            && detail == (i == 3 ? 5 : 0));
      collect(j);
   }
   puts("PASS: independent I/O thread, 64000 notifications, JNI thread "
        "ownership, "
        "deep copies, startup/detach faults and deferred GPU/PCM closure; "
        "session mocked");
   return 0;
}
#else
#include "../../../record/record_dispatch_mediacodec.c"
int main(void)
{
   assert(!record_dispatch_mediacodec_new(NULL, 0, 0, 0, 0, 0, 0, NULL));
   assert(record_dispatch_mediacodec_free(NULL));
   puts("PASS: unsupported dispatcher stubs");
   return 0;
}
#endif
