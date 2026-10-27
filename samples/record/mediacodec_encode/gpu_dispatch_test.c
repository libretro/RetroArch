/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#ifdef NDEBUG
#error GPU dispatcher acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_gpu_dispatch_mediacodec.h"
#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <retro_atomic.h>
struct record_dispatch_mediacodec
{
   retro_atomic_int_t cancelled, retired, released, gpu_owner;
   unsigned color;
   int64_t end;
};
struct record_mediacodec_surface
{
   bool active;
};
struct record_android_capture_gl
{
   int live;
};
struct record_android_capture_draw
{
   int live;
};
struct record_mediacodec_gpu_worker
{
   record_gpu_queue_t *queue;
   void (*close_fd)(void *, int);
   void *context;
   unsigned slot;
   bool held, ended;
};
static struct record_dispatch_mediacodec io;
static struct record_mediacodec_surface surface;
static struct record_android_capture_gl capture;
static struct record_android_capture_draw draw;
static struct record_mediacodec_gpu_worker worker;
static pthread_t gpu_thread;
static retro_atomic_int_t resources, posted, hold_read, read_error;
static int fault, mode, surface_live, worker_live;
static bool temporary_failure;
static int test_eventfd(unsigned int initial, int flags)
{
   return fault == 10 ? -1 : eventfd(initial, flags);
}
static int test_create(pthread_t *thread, const pthread_attr_t *attr,
      void *(*run)(void *), void *context)
{
   return fault == 11 ? EAGAIN : pthread_create(thread, attr, run, context);
}
static int test_detach(pthread_t thread)
{
   return fault == 12 ? EINVAL : pthread_detach(thread);
}
#define eventfd test_eventfd
#define pthread_create test_create
#define pthread_detach test_detach
#include "../../../record/record_gpu_dispatch_mediacodec.c"
#undef eventfd
#undef pthread_create
#undef pthread_detach
static void owner(void)
{
   assert(pthread_equal(pthread_self(), gpu_thread));
}
bool record_dispatch_mediacodec_video_format(
      const record_dispatch_mediacodec_t *j, unsigned *width, unsigned *height,
      unsigned *color)
{
   assert(j == &io);
   *width = fault == 13 ? 65 : 64;
   *height = fault == 14 ? 49 : 48;
   *color = fault == 15 ? !io.color : io.color;
   return true;
}
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *j)
{
   assert(j == &io);
   return retro_atomic_cas_int(&io.gpu_owner, 0, 1) != 0;
}
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *j)
{
   assert(j == &io && retro_atomic_load_acquire_int(&io.gpu_owner));
   retro_atomic_store_release_int(&io.gpu_owner, 0);
}
enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *j, int *error, int *file, int *system)
{
   assert(j == &io && !error && !file && !system);
   return RECORD_MC_DISPATCH_READY;
}
bool record_dispatch_mediacodec_cancelled(const record_dispatch_mediacodec_t *j)
{
   assert(j == &io);
   return retro_atomic_load_acquire_int(&io.cancelled) != 0;
}
void record_dispatch_mediacodec_cancel(record_dispatch_mediacodec_t *j)
{
   assert(j == &io);
   retro_atomic_store_release_int(&io.cancelled, 1);
}
void record_dispatch_mediacodec_notify(record_dispatch_mediacodec_t *j)
{
   assert(j == &io);
}
void *record_dispatch_mediacodec_surface(record_dispatch_mediacodec_t *j)
{
   assert(j == &io);
   return &io;
}
bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *j, int64_t end)
{
   assert(j == &io && !worker.held);
   io.end = end;
   retro_atomic_store_release_int(&io.retired, 1);
   return true;
}
bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *j)
{
   assert(j == &io && retro_atomic_load_acquire_int(&io.retired));
   assert(!surface_live && !capture.live && !draw.live && !worker_live);
   retro_atomic_store_release_int(&io.released, 1);
   return true;
}
bool record_mediacodec_surface_new(const record_mediacodec_surface_ops_t *ops,
      EGLDisplay display, EGLConfig config, EGLContext share,
      EGLNativeWindowType window, unsigned width, unsigned height, bool pq,
      record_mediacodec_surface_t **output)
{
   (void)ops;
   (void)share;
   assert(display && config && window && width == 64 && height == 48);
   assert(pq == (mode != 0) && !surface_live);
   gpu_thread = pthread_self();
   surface.active = false;
   surface_live = 1;
   retro_atomic_fetch_add_int(&resources, 1);
   *output = &surface;
   return fault != 1;
}
bool record_mediacodec_surface_begin(record_mediacodec_surface_t *s)
{
   owner();
   assert(s == &surface && !s->active);
   if (fault == 2)
      return false;
   s->active = true;
   return true;
}
bool record_mediacodec_surface_begin_retirement(record_mediacodec_surface_t *s)
{
   owner();
   assert(s == &surface);
   s->active = true;
   return true;
}
bool record_mediacodec_surface_leave(record_mediacodec_surface_t *s)
{
   owner();
   assert(s == &surface);
   if (fault == 6 && !temporary_failure)
   {
      temporary_failure = true;
      return false;
   }
   s->active = false;
   return true;
}
bool record_mediacodec_surface_free(record_mediacodec_surface_t *s)
{
   owner();
   assert(s == &surface && !s->active && !capture.live && !draw.live);
   if (fault == 7 && !temporary_failure)
   {
      temporary_failure = true;
      return false;
   }
   surface_live = 0;
   retro_atomic_fetch_add_int(&resources, -1);
   return true;
}
bool record_android_capture_gl_new(EGLDisplay display,
      const record_android_capture_gl_ops_t *ops,
      struct AHardwareBuffer *const *buffers, unsigned count,
      record_android_capture_gl_t **output)
{
   (void)ops;
   owner();
   assert(display && surface.active && buffers && count == 1);
   capture.live = 1;
   retro_atomic_fetch_add_int(&resources, 1);
   *output = &capture;
   return fault != 3;
}
bool record_android_capture_gl_free(record_android_capture_gl_t *c)
{
   owner();
   assert(surface.active && !worker.held);
   if (!c)
      return true;
   assert(c == &capture && c->live);
   if (fault == 8 && !temporary_failure)
   {
      temporary_failure = true;
      return false;
   }
   c->live = 0;
   retro_atomic_fetch_add_int(&resources, -1);
   return true;
}
record_android_capture_draw_t *record_android_capture_draw_new(
      const record_android_capture_draw_ops_t *ops, unsigned width,
      unsigned height, unsigned input_mode, bool flip)
{
   (void)ops;
   owner();
   assert(surface.active && capture.live && width == 64 && height == 48);
   assert(input_mode == (unsigned)mode && flip);
   if (fault == 4)
      return NULL;
   draw.live = 1;
   retro_atomic_fetch_add_int(&resources, 1);
   return &draw;
}
void record_android_capture_draw_free(
      record_android_capture_draw_t *d, bool abandon)
{
   owner();
   assert(surface.active && !abandon && !worker.held);
   if (d)
   {
      assert(d == &draw && d->live);
      d->live = 0;
      retro_atomic_fetch_add_int(&resources, -1);
   }
}
record_mediacodec_gpu_worker_t *record_mediacodec_gpu_worker_new(
      record_gpu_queue_t *q, record_mediacodec_surface_t *s,
      record_android_capture_gl_t *c, record_android_capture_draw_t *d,
      unsigned count, void (*close_fd)(void *, int), void *context)
{
   owner();
   assert(q && s == &surface && c == &capture && d == &draw && count == 1);
   if (fault == 5)
      return NULL;
   memset(&worker, 0, sizeof(worker));
   worker.queue = q;
   worker.close_fd = close_fd;
   worker.context = context;
   worker_live = 1;
   return &worker;
}
static int worker_step(record_mediacodec_gpu_worker_t *w, bool retire)
{
   record_gpu_frame_t frame;
   int result;
   owner();
   assert(w == &worker && worker_live);
   if (w->held)
   {
      if (retro_atomic_load_acquire_int(&read_error))
         return -1;
      if (retro_atomic_load_acquire_int(&hold_read))
         return 0;
      assert(record_gpu_queue_release(w->queue, w->slot));
      w->held = false;
   }
   result = record_gpu_queue_take(w->queue, &frame);
   if (result == RECORD_GPU_QUEUE_BUSY)
      return 0;
   if (result == RECORD_GPU_QUEUE_END)
   {
      w->ended = true;
      return 2;
   }
   assert(result == RECORD_GPU_QUEUE_OK);
   if (frame.ready_value)
      w->close_fd(w->context, (int)(frame.ready_value - 1));
   if (retire)
   {
      assert(record_gpu_queue_release(w->queue, frame.slot));
      return 1;
   }
   w->slot = frame.slot;
   w->held = true;
   retro_atomic_fetch_add_int(&posted, 1);
   return 1;
}
int record_mediacodec_gpu_worker_step(record_mediacodec_gpu_worker_t *w)
{
   return worker_step(w, false);
}
int record_mediacodec_gpu_worker_retire_step(record_mediacodec_gpu_worker_t *w)
{
   return worker_step(w, true);
}
bool record_mediacodec_gpu_worker_free(record_mediacodec_gpu_worker_t *w)
{
   owner();
   assert(w == &worker && w->ended && !w->held);
   worker_live = 0;
   return true;
}
static void pause_test(void)
{
   struct timespec t;
   t.tv_sec = 0;
   t.tv_nsec = 1000000;
   nanosleep(&t, NULL);
}
static void wait_state(record_gpu_dispatch_mediacodec_t *j,
      enum record_dispatch_mediacodec_state state)
{
   unsigned i;
   for (i = 0; i < 10000; i++)
   {
      if (record_gpu_dispatch_mediacodec_status(j, NULL, NULL) == state)
         return;
      pause_test();
   }
   assert(0);
}
static void collect(record_gpu_dispatch_mediacodec_t *j)
{
   unsigned i;
   for (i = 0; i < 10000; i++)
   {
      if (record_gpu_dispatch_mediacodec_free(j))
         break;
      pause_test();
   }
   assert(i < 10000 && !retro_atomic_load_acquire_int(&resources));
   assert(retro_atomic_load_acquire_int(&io.released));
   assert(!retro_atomic_load_acquire_int(&io.gpu_owner));
}
static record_gpu_dispatch_mediacodec_t *start(void)
{
   record_gpu_mediacodec_config_t c;
   static char buffer;
   memset(&c, 0, sizeof(c));
   c.width = 64;
   c.height = 48;
   c.count = 1;
   c.mode = (unsigned)mode;
   c.flip_y = true;
   c.buffers[0] = (struct AHardwareBuffer *)&buffer;
   c.display = (EGLDisplay)&io;
   c.config = (EGLConfig)&io;
   io.color = mode ? 1 : 0;
   io.end = 0;
   retro_atomic_int_init(&io.cancelled, 0);
   retro_atomic_int_init(&io.retired, 0);
   retro_atomic_int_init(&io.released, 0);
   retro_atomic_int_init(&io.gpu_owner, 0);
   temporary_failure = false;
   return record_gpu_dispatch_mediacodec_new(&io, &c);
}
int main(void)
{
   record_gpu_dispatch_mediacodec_t *j;
   record_gpu_frame_t frame;
   unsigned slot, i;
   int before, fd;
   retro_atomic_int_init(&resources, 0);
   retro_atomic_int_init(&posted, 0);
   retro_atomic_int_init(&hold_read, 0);
   retro_atomic_int_init(&read_error, 0);
   for (mode = 0; mode < 3; mode++)
   {
      fault = mode == 0 ? 12 : 0;
      j = start();
      assert(j);
      wait_state(j, RECORD_MC_DISPATCH_READY);
      assert(!record_gpu_dispatch_mediacodec_new(&io, &j->config));
      memset(&frame, 0, sizeof(frame));
      frame.duration = 1000;
      for (i = 0; i < 1000; i++)
      {
         unsigned retry;
         for (retry = 0; retry < 10000; retry++)
         {
            if (record_gpu_dispatch_mediacodec_acquire(j, &slot)
                  == RECORD_GPU_QUEUE_OK)
               break;
            pause_test();
         }
         assert(retry < 10000);
         frame.slot = slot;
         if (i)
         {
            frame.timestamp = (i - 1) * 1000;
            assert(!record_gpu_dispatch_mediacodec_submit(j, &frame));
         }
         frame.timestamp = i * 1000;
         assert(record_gpu_dispatch_mediacodec_submit(j, &frame));
      }
      assert(!record_gpu_dispatch_mediacodec_finish(j, 999999));
      assert(record_gpu_dispatch_mediacodec_finish(j, 1000000));
      assert(record_gpu_dispatch_mediacodec_finish(j, 1000000));
      assert(!record_gpu_dispatch_mediacodec_finish(j, 1000001));
      wait_state(j, RECORD_MC_DISPATCH_DONE);
      collect(j);
      assert(io.end == 1000000);
   }
   assert(retro_atomic_load_acquire_int(&posted) == 3000);
   mode = 0;
   for (fault = 1; fault <= 8; fault++)
   {
      j = start();
      assert(j);
      wait_state(j,
            fault <= 6 ? RECORD_MC_DISPATCH_DRAINING
                       : RECORD_MC_DISPATCH_READY);
      if (fault <= 6)
         assert(retro_atomic_load_acquire_int(&io.cancelled));
      assert(record_gpu_dispatch_mediacodec_finish(j, 0));
      wait_state(j,
            fault <= 6 ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE);
      collect(j);
   }
   for (fault = 10; fault <= 11; fault++)
   {
      assert(!start());
      assert(!retro_atomic_load_acquire_int(&resources));
   }
   for (fault = 13; fault <= 15; fault++)
      assert(!start());
   fault = 0;
   j = start();
   assert(j);
   wait_state(j, RECORD_MC_DISPATCH_READY);
   assert(record_gpu_dispatch_mediacodec_acquire(j, &slot)
         == RECORD_GPU_QUEUE_OK);
   assert(!record_gpu_dispatch_mediacodec_finish(j, 0));
   assert(record_gpu_dispatch_mediacodec_cancel_slot(j));
   assert(record_gpu_dispatch_mediacodec_acquire(j, &slot)
         == RECORD_GPU_QUEUE_OK);
   memset(&frame, 0, sizeof(frame));
   frame.slot = slot;
   frame.duration = 1000;
   fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
   assert(fd >= 0);
   frame.ready_value = (uint64_t)fd + 1;
   retro_atomic_store_release_int(&hold_read, 1);
   before = retro_atomic_load_acquire_int(&posted);
   assert(record_gpu_dispatch_mediacodec_submit(j, &frame));
   for (i = 0; i < 10000 && retro_atomic_load_acquire_int(&posted) == before;
         i++)
      pause_test();
   assert(i < 10000);
   retro_atomic_store_release_int(&read_error, 1);
   wait_state(j, RECORD_MC_DISPATCH_DRAINING);
   assert(record_gpu_dispatch_mediacodec_finish(j, 1000));
   assert(!record_gpu_dispatch_mediacodec_free(j));
   assert(!retro_atomic_load_acquire_int(&io.retired)
         && !retro_atomic_load_acquire_int(&io.released));
   assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
   retro_atomic_store_release_int(&hold_read, 0);
   retro_atomic_store_release_int(&read_error, 0);
   wait_state(j, RECORD_MC_DISPATCH_FAILED);
   collect(j);
   puts("PASS: real GLES executor thread and slot queue, 3000 posts, source "
        "ends, "
        "partial-startup cleanup and retained reads on cancellation; native "
        "GPU/I/O owners mocked");
   return 0;
}
#else
#include "../../../record/record_gpu_dispatch_mediacodec.c"
int main(void)
{
   assert(!record_gpu_dispatch_mediacodec_new(NULL, NULL));
   assert(record_gpu_dispatch_mediacodec_free(NULL));
   puts("PASS: unsupported GPU dispatcher stubs");
   return 0;
}
#endif
