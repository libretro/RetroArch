/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_gpu_dispatch_mediacodec.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <limits.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && (defined(__ANDROID__)                                                 \
            || defined(RECORD_MEDIACODEC_GPU_DISPATCH_TEST))
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
struct record_gpu_dispatch_mediacodec
{
   record_gpu_mediacodec_config_t config;
   uint64_t last_pts, last_end;
   int64_t end_us;
   record_dispatch_mediacodec_t *io;
   record_gpu_queue_t *queue;
   record_mediacodec_surface_t *surface;
   record_android_capture_gl_t *capture;
   record_android_capture_draw_t *draw;
   record_mediacodec_gpu_worker_t *worker;
   pthread_t thread;
   retro_atomic_int_t state, done, epoch, closed, cancelled, error,
         system_error;
   int wake;
   bool joinable, initialized, have_frame, reads_retired;
};
static void record_mgd_fail(record_gpu_dispatch_mediacodec_t *j, int error)
{
   retro_atomic_cas_int(&j->error, 0, error);
   retro_atomic_store_release_int(&j->cancelled, 1);
   retro_atomic_store_release_int(&j->state, RECORD_MC_DISPATCH_DRAINING);
   record_dispatch_mediacodec_cancel(j->io);
}
static void record_mgd_system(record_gpu_dispatch_mediacodec_t *j, int error)
{
   retro_atomic_cas_int(&j->system_error, 0, error ? error : EIO);
   retro_atomic_store_release_int(&j->cancelled, 1);
}
static void record_mgd_notify(record_gpu_dispatch_mediacodec_t *j)
{
   uint64_t one = 1;
   if ((retro_atomic_fetch_add_seq_cst_int(&j->epoch, 2) & 1)
         && write(j->wake, &one, sizeof(one)) != (ssize_t)sizeof(one)
         && errno != EAGAIN)
      record_mgd_system(j, errno);
}
static void record_mgd_close_fd(void *context, int fd)
{
   record_gpu_dispatch_mediacodec_t *j
         = (record_gpu_dispatch_mediacodec_t *)context;
   if (close(fd) && errno != EINTR)
      record_mgd_system(j, errno);
}
static bool record_mgd_start(record_gpu_dispatch_mediacodec_t *j)
{
   void *window = record_dispatch_mediacodec_surface(j->io);
   record_gpu_mediacodec_config_t *c = &j->config;
   bool ok = false;
   if (!window
         || !record_mediacodec_surface_new(&c->surface, c->display, c->config,
               c->share, (EGLNativeWindowType)window, c->width, c->height,
               c->mode != 0, &j->surface))
      return false;
   if (!record_mediacodec_surface_begin(j->surface))
      return false;
   if (record_android_capture_gl_new(
             c->display, &c->capture, c->buffers, c->count, &j->capture))
      j->draw = record_android_capture_draw_new(
            &c->draw, c->width, c->height, c->mode, c->flip_y);
   if (j->draw)
      j->worker = record_mediacodec_gpu_worker_new(j->queue, j->surface,
            j->capture, j->draw, c->count, record_mgd_close_fd, j);
   ok = j->worker != NULL;
   return record_mediacodec_surface_leave(j->surface) && ok;
}
static int record_mgd_cleanup(record_gpu_dispatch_mediacodec_t *j)
{
   if (j->worker)
   {
      if (!record_mediacodec_gpu_worker_free(j->worker))
         return 0;
      j->worker = NULL;
   }
   if (j->capture || j->draw)
   {
      if (!record_mediacodec_surface_begin_retirement(j->surface))
         return 0;
      record_android_capture_draw_free(j->draw, false);
      j->draw = NULL;
      if (!record_android_capture_gl_free(j->capture))
      {
         record_mediacodec_surface_leave(j->surface);
         return 0;
      }
      j->capture = NULL;
   }
   if (j->surface)
   {
      if (!record_mediacodec_surface_leave(j->surface)
            || !record_mediacodec_surface_free(j->surface))
         return 0;
      j->surface = NULL;
   }
   if (!record_dispatch_mediacodec_gpu_released(j->io))
      return 0;
   return 2;
}
static int record_mgd_service(record_gpu_dispatch_mediacodec_t *j)
{
   enum record_dispatch_mediacodec_state io_state;
   int result;
   if (retro_atomic_load_acquire_int(&j->system_error)
         || record_dispatch_mediacodec_cancelled(j->io)
         || retro_atomic_load_acquire_int(&j->cancelled))
      record_mgd_fail(j, RECORD_MC_SESSION_CANCELLED);
   if (j->reads_retired)
      return record_mgd_cleanup(j);
   if (!j->initialized && !retro_atomic_load_acquire_int(&j->cancelled))
   {
      io_state = record_dispatch_mediacodec_status(j->io, NULL, NULL, NULL);
      if (io_state == RECORD_MC_DISPATCH_STARTING)
         return 0;
      j->initialized = true;
      if (io_state != RECORD_MC_DISPATCH_READY || !record_mgd_start(j))
         record_mgd_fail(j, RECORD_MC_SESSION_OUTPUT);
      else
         retro_atomic_store_release_int(&j->state, RECORD_MC_DISPATCH_READY);
   }
   if (j->worker)
   {
      result = retro_atomic_load_acquire_int(&j->cancelled)
            ? record_mediacodec_gpu_worker_retire_step(j->worker)
            : record_mediacodec_gpu_worker_step(j->worker);
      record_dispatch_mediacodec_notify(j->io);
      if (result < 0)
      {
         record_mgd_fail(j, RECORD_MC_SESSION_OUTPUT);
         return 0;
      }
      if (result != 2)
         return result;
   }
   else if (!retro_atomic_load_acquire_int(&j->cancelled))
      return 0;
   if (!retro_atomic_load_acquire_int(&j->closed))
      return 0;
   retro_atomic_store_release_int(&j->state, RECORD_MC_DISPATCH_DRAINING);
   if (!record_dispatch_mediacodec_gpu_retired(j->io, j->end_us))
   {
      record_mgd_fail(j, RECORD_MC_SESSION_OUTPUT);
      return 0;
   }
   j->reads_retired = true;
   return record_mgd_cleanup(j);
}
static void *record_mgd_run(void *context)
{
   record_gpu_dispatch_mediacodec_t *j
         = (record_gpu_dispatch_mediacodec_t *)context;
   struct pollfd descriptor;
   uint64_t value;
   int result, waited;
   descriptor.fd = j->wake;
   descriptor.events = POLLIN;
   for (;;)
   {
      result = record_mgd_service(j);
      if (result == 2)
         break;
      if (result == 1)
         continue;
      retro_atomic_fetch_add_seq_cst_int(&j->epoch, 1);
      result = record_mgd_service(j);
      if (result)
      {
         retro_atomic_fetch_and_int(&j->epoch, ~1);
         if (result == 2)
            break;
         continue;
      }
      descriptor.revents = 0;
      if (retro_atomic_load_acquire_int(&j->system_error))
         descriptor.fd = -1;
      waited = poll(&descriptor, 1, 1);
      retro_atomic_fetch_and_int(&j->epoch, ~1);
      if (waited < 0 && errno != EINTR)
         record_mgd_system(j, errno);
      else if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
         record_mgd_system(j, EIO);
      else if (descriptor.revents & POLLIN)
      {
         if (read(j->wake, &value, sizeof(value)) != (ssize_t)sizeof(value)
               && errno != EAGAIN && errno != EINTR)
            record_mgd_system(j, errno);
      }
   }
   retro_atomic_store_release_int(&j->state,
         retro_atomic_load_acquire_int(&j->error)
                     || retro_atomic_load_acquire_int(&j->system_error)
               ? RECORD_MC_DISPATCH_FAILED
               : RECORD_MC_DISPATCH_DONE);
   retro_atomic_store_release_int(&j->done, 1);
   return NULL;
}
record_gpu_dispatch_mediacodec_t *record_gpu_dispatch_mediacodec_new(
      record_dispatch_mediacodec_t *io, const record_gpu_mediacodec_config_t *c)
{
   record_gpu_dispatch_mediacodec_t *j;
   unsigned width, height, color, i;
   if (!io || !c || !c->count || c->count > RECORD_GPU_QUEUE_SLOTS
         || c->mode > 2 || c->display == EGL_NO_DISPLAY || !c->config
         || !record_dispatch_mediacodec_video_format(
               io, &width, &height, &color)
         || width != c->width || height != c->height
         || color != (c->mode ? 1u : 0u))
      return NULL;
   for (i = 0; i < c->count; i++)
      if (!c->buffers[i])
         return NULL;
   if (!record_dispatch_mediacodec_gpu_claim(io))
      return NULL;
   j = (record_gpu_dispatch_mediacodec_t *)calloc(1, sizeof(*j));
   if (!j)
   {
      record_dispatch_mediacodec_gpu_release_claim(io);
      return NULL;
   }
   j->wake = -1;
   j->config = *c;
   j->io = io;
   j->queue = record_gpu_queue_new(c->count);
   if (!j->queue)
      goto failed;
   retro_atomic_int_init(&j->state, RECORD_MC_DISPATCH_STARTING);
   retro_atomic_int_init(&j->done, 0);
   retro_atomic_int_init(&j->epoch, 0);
   retro_atomic_int_init(&j->closed, 0);
   retro_atomic_int_init(&j->cancelled, 0);
   retro_atomic_int_init(&j->error, 0);
   retro_atomic_int_init(&j->system_error, 0);
   j->wake = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
   if (j->wake < 0 || pthread_create(&j->thread, NULL, record_mgd_run, j))
      goto failed;
   j->joinable = pthread_detach(j->thread) != 0;
   return j;
failed:
   if (j->wake >= 0)
      close(j->wake);
   record_gpu_queue_free(j->queue);
   record_dispatch_mediacodec_gpu_release_claim(io);
   free(j);
   return NULL;
}
int record_gpu_dispatch_mediacodec_acquire(
      record_gpu_dispatch_mediacodec_t *j, unsigned *slot)
{
   int state;
   if (!j || retro_atomic_load_acquire_int(&j->closed)
         || retro_atomic_load_acquire_int(&j->cancelled)
         || record_dispatch_mediacodec_cancelled(j->io))
      return RECORD_GPU_QUEUE_ERROR;
   state = retro_atomic_load_acquire_int(&j->state);
   if (state != RECORD_MC_DISPATCH_READY)
      return state == RECORD_MC_DISPATCH_STARTING ? RECORD_GPU_QUEUE_BUSY
                                                  : RECORD_GPU_QUEUE_ERROR;
   return record_gpu_queue_acquire(j->queue, slot);
}
bool record_gpu_dispatch_mediacodec_submit(
      record_gpu_dispatch_mediacodec_t *j, const record_gpu_frame_t *frame)
{
   uint64_t maximum = (((uint64_t)1 << 63) - 1) / 1000;
   if (!j || !frame || frame->keyframe || !frame->duration
         || frame->timestamp > maximum
         || frame->duration > maximum - frame->timestamp
         || frame->ready_value > (uint64_t)INT_MAX + 1
         || (j->have_frame && frame->timestamp <= j->last_pts)
         || retro_atomic_load_acquire_int(&j->closed)
         || retro_atomic_load_acquire_int(&j->cancelled)
         || record_dispatch_mediacodec_cancelled(j->io)
         || retro_atomic_load_acquire_int(&j->state) != RECORD_MC_DISPATCH_READY
         || !record_gpu_queue_submit(j->queue, frame))
      return false;
   j->last_pts = frame->timestamp;
   j->last_end = frame->timestamp + frame->duration;
   j->have_frame = true;
   record_mgd_notify(j);
   return true;
}
bool record_gpu_dispatch_mediacodec_cancel_slot(
      record_gpu_dispatch_mediacodec_t *j)
{
   return j && record_gpu_queue_cancel(j->queue);
}
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *j, int64_t end_us)
{
   if (!j || end_us < 0 || (uint64_t)end_us < j->last_end)
      return false;
   if (retro_atomic_load_acquire_int(&j->closed))
      return j->end_us == end_us;
   if (!record_gpu_queue_finish(j->queue))
      return false;
   j->end_us = end_us;
   retro_atomic_store_release_int(&j->closed, 1);
   record_mgd_notify(j);
   return true;
}
void record_gpu_dispatch_mediacodec_cancel(record_gpu_dispatch_mediacodec_t *j)
{
   if (j)
   {
      retro_atomic_store_release_int(&j->cancelled, 1);
      record_dispatch_mediacodec_cancel(j->io);
      record_mgd_notify(j);
   }
}
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *j, int *error, int *system_error)
{
   if (error)
      *error = j ? retro_atomic_load_acquire_int(&j->error)
                 : RECORD_MC_SESSION_OUTPUT;
   if (system_error)
      *system_error = j ? retro_atomic_load_acquire_int(&j->system_error) : 0;
   return j
         ? (enum record_dispatch_mediacodec_state)retro_atomic_load_acquire_int(
                 &j->state)
         : RECORD_MC_DISPATCH_FAILED;
}
bool record_gpu_dispatch_mediacodec_free(record_gpu_dispatch_mediacodec_t *j)
{
   if (!j)
      return true;
   if (!retro_atomic_load_acquire_int(&j->done))
      return false;
   if (j->joinable)
   {
      if (pthread_join(j->thread, NULL))
         return false;
      j->joinable = false;
   }
   if (!record_gpu_queue_free(j->queue))
      return false;
   record_dispatch_mediacodec_gpu_release_claim(j->io);
   close(j->wake);
   free(j);
   return true;
}
#else
record_gpu_dispatch_mediacodec_t *record_gpu_dispatch_mediacodec_new(
      record_dispatch_mediacodec_t *io, const record_gpu_mediacodec_config_t *c)
{
   (void)io;
   (void)c;
   return NULL;
}
int record_gpu_dispatch_mediacodec_acquire(
      record_gpu_dispatch_mediacodec_t *j, unsigned *slot)
{
   (void)j;
   (void)slot;
   return RECORD_GPU_QUEUE_ERROR;
}
bool record_gpu_dispatch_mediacodec_submit(
      record_gpu_dispatch_mediacodec_t *j, const record_gpu_frame_t *frame)
{
   (void)j;
   (void)frame;
   return false;
}
bool record_gpu_dispatch_mediacodec_cancel_slot(
      record_gpu_dispatch_mediacodec_t *j)
{
   (void)j;
   return false;
}
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *j, int64_t end)
{
   (void)j;
   (void)end;
   return false;
}
void record_gpu_dispatch_mediacodec_cancel(record_gpu_dispatch_mediacodec_t *j)
{
   (void)j;
}
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *j, int *error, int *system_error)
{
   (void)j;
   if (error)
      *error = RECORD_MC_SESSION_OUTPUT;
   if (system_error)
      *system_error = 0;
   return RECORD_MC_DISPATCH_FAILED;
}
bool record_gpu_dispatch_mediacodec_free(record_gpu_dispatch_mediacodec_t *j)
{
   return j == NULL;
}
#endif
