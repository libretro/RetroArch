/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_android_capture_request.h"
#include "record_android_capture_egl.h"
#ifdef HAVE_THREADS
#include "../gfx/video_driver.h"
#include "../gfx/video_thread_wrapper.h"
#include <rthreads/rthreads.h>
#include <stdlib.h>
#include <string.h>

struct record_android_capture_request
{
   record_android_capture_pool_config_t config;
   VkPhysicalDeviceMemoryProperties memory;
   record_android_capture_renderer_t binding;
   video_thread_async_load_t node;
   video_thread_async_job_t job;
   record_dispatch_mediacodec_t *io;
   record_android_capture_pool_t *pool;
   record_android_capture_egl_t *egl;
   int64_t end_us, close_end;
   enum record_android_capture_operation operation;
   unsigned duration;
   int result;
   bool busy, ready, closing, failed, had_pool, menu_source;
};

static bool record_acrequest_in_flight(record_android_capture_request_t *r)
{
   video_driver_state_t *video = video_state_get_ptr();
   return video_driver_thread_wrapper_active()
         && video_thread_record_gpu_in_flight(video->data, &r->binding);
}

static int record_acrequest_create(record_android_capture_request_t *r)
{
   enum record_dispatch_mediacodec_state state;
   if (r->failed)
      return -1;
   if (!r->pool)
   {
      state = record_dispatch_mediacodec_status(r->io, NULL, NULL, NULL);
      if (state == RECORD_MC_DISPATCH_STARTING)
         return 0;
      if (state != RECORD_MC_DISPATCH_READY)
         return -1;
      if (!record_android_capture_egl_new(r->config.gpu.mode, &r->egl)
            || !record_android_capture_egl_configure(r->egl, &r->config.gpu))
      {
         r->failed = true;
         return -1;
      }
      if (!record_android_capture_pool_new(r->io, &r->config, &r->pool))
      {
         r->had_pool = r->pool != NULL;
         r->failed = true;
         return -1;
      }
      r->had_pool = true;
      if (!record_android_capture_pool_bind_renderer(
                r->pool, r->duration, &r->binding))
      {
         r->failed = true;
         return -1;
      }
      if (r->menu_source)
         r->binding.flags |= RECORD_ANDROID_CAPTURE_MENU;
   }
   state = record_android_capture_pool_status(r->pool, NULL, NULL);
   return state == RECORD_MC_DISPATCH_READY     ? 1
         : state == RECORD_MC_DISPATCH_STARTING ? 0
                                                : -1;
}

/* Cold render owner. Every native queue operation uses the current driver's
 * existing serialization. No borrowed lock is retained in the request. */
static uintptr_t record_acrequest_run(
      void *driver_data, const video_poke_interface_t *poke, void *user)
{
   record_android_capture_request_t *r
         = (record_android_capture_request_t *)user;
   video_device_info_t device;
   slock_t *lock = NULL;
   bool current, destroyed = false;
   int result = 0;
   memset(&device, 0, sizeof(device));
   current = driver_data && poke && poke->get_device
         && poke->get_device(driver_data, &device)
         && device.api == VIDEO_DEVICE_VULKAN
         && device.device == (void *)r->config.device
         && device.physical_device == (void *)r->config.physical
         && device.queue == (void *)r->config.queue
         && device.queue_family == r->config.family && device.queue_lock;
   if (r->operation == RECORD_ANDROID_CAPTURE_CREATE)
      result = current ? record_acrequest_create(r) : -1;
   else
   {
      if (r->pool)
      {
         record_android_capture_pool_device_state(r->pool, &destroyed);
         /* A stale device/lock is never queried. Common teardown must first
          * publish actual device death; unproven owners remain retained. */
         if (!current && !destroyed)
            goto complete;
         if (current && device.queue_lock)
         {
            if (!slock_try_lock((slock_t *)device.queue_lock))
               goto complete;
            lock = (slock_t *)device.queue_lock;
         }
         if (r->operation == RECORD_ANDROID_CAPTURE_CANCEL
               || r->operation == RECORD_ANDROID_CAPTURE_PREPARE_DEVICE
               || r->operation == RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE)
            record_android_capture_pool_cancel(r->pool);
         if (r->operation == RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE)
         {
            /* Every pool is registered before native allocation. The common
             * teardown must reach its existing drain for ambiguous work; a
             * failed nonblocking child proof must not deadlock that drain.
             * Cancellation stops publication, but proves no GPU completion. */
            record_android_capture_pool_finish(r->pool, r->end_us);
            result = 1;
            goto unlock;
         }
         if (r->operation >= RECORD_ANDROID_CAPTURE_FINISH
               && !record_android_capture_pool_finish(r->pool, r->end_us)
               && !(record_dispatch_mediacodec_cancelled(r->io)
                     && record_android_capture_pool_closed(r->pool)))
         {
            record_android_capture_pool_step(r->pool);
            goto unlock;
         }
         if (r->operation == RECORD_ANDROID_CAPTURE_PREPARE_DEVICE)
            result = record_android_capture_pool_prepare_device_destroy(
                           r->pool, false)
                  ? 1
                  : 0;
         else
         {
            result = record_android_capture_pool_step(r->pool);
            if (r->operation == RECORD_ANDROID_CAPTURE_COLLECT && result == 1)
            {
               if (record_android_capture_pool_free(r->pool))
               {
                  r->pool = NULL;
                  memset(&r->binding, 0, sizeof(r->binding));
               }
               else
                  result = 0;
            }
         }
      unlock:
         if (lock)
            slock_unlock(lock);
      }
      else
         result = 1;
      if (r->operation == RECORD_ANDROID_CAPTURE_COLLECT && !r->pool && r->egl)
      {
         if (record_android_capture_egl_free(r->egl))
            r->egl = NULL;
         else
            result = -1;
      }
   }
complete:
   r->result = result;
   /* Nonzero distinguishes executed work from the wrapper's teardown reply;
    * operation success/pending/failure is stored separately. */
   return 1;
}

static void record_acrequest_done(void *user, uintptr_t executed)
{
   record_android_capture_request_t *r
         = (record_android_capture_request_t *)user;
   r->busy = false;
   if (!executed)
   {
      r->result = -1;
      r->ready = false;
      r->failed = true;
   }
   else if (r->operation == RECORD_ANDROID_CAPTURE_CREATE)
   {
      r->ready = r->result == 1;
      if (r->result < 0)
         r->failed = true;
   }
}

record_android_capture_request_t *record_android_capture_request_new(
      record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *config, unsigned duration,
      bool allow_drop, bool menu_source)
{
   record_android_capture_request_t *r;
   if (!io || !config || !config->device || !config->physical || !config->queue
         || !config->memory || !config->gpu.width || !config->gpu.height
         || !config->gpu.count || config->gpu.count > RECORD_GPU_QUEUE_SLOTS
         || config->gpu.mode > 2 || !duration || duration > 1000000)
      return NULL;
   r = (record_android_capture_request_t *)calloc(1, sizeof(*r));
   if (!r)
      return NULL;
   r->config = *config;
   r->config.fail_on_drop = !allow_drop;
   r->memory = *config->memory;
   r->config.memory = &r->memory;
   r->io = io;
   r->duration = duration;
   r->menu_source = menu_source;
   r->job.run = record_acrequest_run;
   r->job.user = r;
   r->node.kind = VIDEO_THREAD_ASYNC_JOB;
   r->node.user = r;
   r->node.done = record_acrequest_done;
   return r;
}

bool record_android_capture_request_post(record_android_capture_request_t *r,
      enum record_android_capture_operation operation, int64_t end_us)
{
   video_driver_state_t *video = video_state_get_ptr();
   if (!r || r->busy
         || (unsigned)operation > RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE
         || end_us < 0
         || (r->closing && operation == RECORD_ANDROID_CAPTURE_CREATE))
      return false;
   if (operation >= RECORD_ANDROID_CAPTURE_FINISH)
   {
      if (r->closing && r->close_end != end_us)
         return false;
      r->closing = true;
      r->close_end = end_us;
      r->ready = false;
      if (record_acrequest_in_flight(r))
         return false;
   }
   r->operation = operation;
   r->end_us = end_us;
   r->result = 0;
   r->node.img = &r->job;
   r->busy = true;
   if (video_driver_thread_wrapper_active())
   {
      if (video_thread_async_post(&r->node))
         return true;
      r->busy = false;
      r->ready = false;
      r->failed = true;
      r->result = -1;
      return false;
   }
   record_acrequest_done(r, record_acrequest_run(video->data, video->poke, r));
   return true;
}

int record_android_capture_request_result(
      const record_android_capture_request_t *r)
{
   return !r ? -1 : r->busy ? 0 : r->result;
}

const record_android_capture_renderer_t *record_android_capture_request_binding(
      const record_android_capture_request_t *r)
{
   return r && r->ready && !r->closing ? &r->binding : NULL;
}

bool record_android_capture_request_free(record_android_capture_request_t *r)
{
   if (!r)
      return true;
   if (r->busy || r->pool || r->egl || record_acrequest_in_flight(r))
      return false;
   if (!r->had_pool)
   {
      enum record_dispatch_mediacodec_state state
            = record_dispatch_mediacodec_status(r->io, NULL, NULL, NULL);
      if (state != RECORD_MC_DISPATCH_DONE
            && state != RECORD_MC_DISPATCH_FAILED)
      {
         bool closed;
         /* Do not close I/O's GPU side through an unrelated live claim.
          * A prior completed startup can instead finish on its I/O owner. */
         if (!record_dispatch_mediacodec_gpu_claim(r->io))
            return false;
         closed = record_dispatch_mediacodec_gpu_retired(r->io, 0)
               && record_dispatch_mediacodec_gpu_released(r->io);
         record_dispatch_mediacodec_gpu_release_claim(r->io);
         if (!closed)
            return false;
      }
   }
   free(r);
   return true;
}
#else
record_android_capture_request_t *record_android_capture_request_new(
      record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *config, unsigned duration,
      bool allow_drop, bool menu_source)
{
   (void)io;
   (void)config;
   (void)duration;
   (void)allow_drop;
   (void)menu_source;
   return NULL;
}
bool record_android_capture_request_post(record_android_capture_request_t *r,
      enum record_android_capture_operation operation, int64_t end_us)
{
   (void)r;
   (void)operation;
   (void)end_us;
   return false;
}
int record_android_capture_request_result(
      const record_android_capture_request_t *r)
{
   (void)r;
   return -1;
}
const record_android_capture_renderer_t *record_android_capture_request_binding(
      const record_android_capture_request_t *r)
{
   (void)r;
   return NULL;
}
bool record_android_capture_request_free(record_android_capture_request_t *r)
{
   return r == NULL;
}
#endif
