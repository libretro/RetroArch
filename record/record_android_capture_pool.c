/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_pool.h"
#include "record_android_capture_device.h"
#include <stdlib.h>
#include "../gfx/video_defines.h"

struct record_android_capture_pool
{
   record_android_capture_device_t device_owner;
   record_android_capture_images_t *images;
   record_android_capture_submit_t *entries[RECORD_GPU_QUEUE_SLOTS];
   record_gpu_dispatch_mediacodec_t *gpu;
   record_android_capture_pipe_t *pipe;
   int64_t end_us;
   uint64_t renderer_last_offer, renderer_end;
   unsigned count, dims, mode, renderer_duration, renderer_leases;
   bool closed, preparing, prepared, destroyed, fail_on_drop;
   bool renderer_bound, renderer_offer, renderer_recorded, renderer_stopping;
};

static bool record_acpool_terminal(const record_android_capture_pool_t *p)
{
   enum record_dispatch_mediacodec_state state;
   if (!p->gpu)
      return true;
   state = record_gpu_dispatch_mediacodec_status(p->gpu, NULL, NULL);
   return state == RECORD_MC_DISPATCH_DONE
         || state == RECORD_MC_DISPATCH_FAILED;
}

bool record_android_capture_pool_free(record_android_capture_pool_t *p)
{
   unsigned i;
   if (!p)
      return true;
   if (!p->closed
         || (p->preparing && (!p->destroyed || !record_acpool_terminal(p))))
      return false;
   if (p->pipe)
   {
      if (record_android_capture_pipe_step(p->pipe) != 1
            || !record_android_capture_pipe_free(p->pipe))
         return false;
      p->pipe = NULL;
   }
   if (!record_acpool_terminal(p))
      return false;
   if (p->gpu)
   {
      if (!record_gpu_dispatch_mediacodec_free(p->gpu))
         return false;
      p->gpu = NULL;
   }
   for (i = 0; i < p->count; i++)
   {
      if (!record_android_capture_submit_free(p->entries[i]))
         return false;
      p->entries[i] = NULL;
   }
   if (p->renderer_leases)
      return false;
   record_android_capture_images_free(p->images);
   record_android_capture_device_detach(&p->device_owner);
   free(p);
   return true;
}

bool record_android_capture_pool_closed(
      const record_android_capture_pool_t *p)
{
   return p && p->closed;
}

bool record_android_capture_pool_finish(
      record_android_capture_pool_t *p, int64_t end_us)
{
   if (!p || end_us < 0)
      return false;
   if (p->closed)
      return p->end_us == end_us;
   if (p->pipe)
   {
      if (!record_android_capture_pipe_finish(p->pipe, end_us))
         return false;
   }
   else if (p->gpu && !record_gpu_dispatch_mediacodec_finish(p->gpu, end_us))
      return false;
   p->end_us = end_us;
   p->closed = true;
   return true;
}

void record_android_capture_pool_cancel(record_android_capture_pool_t *p)
{
   if (!p)
      return;
   p->renderer_stopping = true;
   if (p->pipe)
      record_android_capture_pipe_cancel(p->pipe);
   else if (p->gpu)
      record_gpu_dispatch_mediacodec_cancel(p->gpu);
}

static bool record_acpool_device_prepare(void *context, bool queue_quiesced)
{
   record_android_capture_pool_t *p = (record_android_capture_pool_t *)context;
   p->preparing = true;
   record_android_capture_pool_cancel(p);
   if (!p->closed && !record_android_capture_pool_finish(p,
            (int64_t)p->renderer_end))
      return false;
   return record_android_capture_pool_prepare_device_destroy(p, queue_quiesced);
}

static void record_acpool_device_destroyed(void *context)
{
   record_android_capture_pool_after_device_destroy(
         (record_android_capture_pool_t *)context);
}

bool record_android_capture_pool_new(record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *c,
      record_android_capture_pool_t **out)
{
   record_android_capture_pool_t *p;
   record_gpu_mediacodec_config_t gpu;
   VkFormat format;
   unsigned i;
   if (!out)
      return false;
   *out = NULL;
   if (!io || !c || !c->physical || !c->device || !c->queue || !c->memory
         || !c->submit.close_fd || !c->gpu.count
         || c->gpu.count > RECORD_GPU_QUEUE_SLOTS || c->gpu.mode > 2)
      return false;
   p = (record_android_capture_pool_t *)calloc(1, sizeof(*p));
   if (!p)
      return false;
   p->device_owner.context = p;
   p->device_owner.device = c->device;
   p->device_owner.prepare = record_acpool_device_prepare;
   p->device_owner.destroyed = record_acpool_device_destroyed;
   if (!record_android_capture_device_attach(&p->device_owner))
   {
      free(p);
      return false;
   }
   p->count = c->gpu.count;
   p->fail_on_drop = c->fail_on_drop;
   p->dims = VIDEO_SCALE_PACK(c->gpu.width, c->gpu.height);
   p->mode = c->gpu.mode;
   format = c->gpu.mode == 2 ? VK_FORMAT_R16G16B16A16_SFLOAT
         : c->gpu.mode == 1  ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                             : VK_FORMAT_R8G8B8A8_UNORM;
   p->images = record_android_capture_images_new(c->physical, c->device,
         &c->images, c->memory, p->count, c->gpu.width, c->gpu.height, format);
   if (!p->images)
      goto failed;
   gpu = c->gpu;
   for (i = 0; i < p->count; i++)
   {
      gpu.buffers[i] = record_android_capture_images_buffer(p->images, i);
      p->entries[i] = record_android_capture_submit_new(c->device, c->queue,
            c->family, record_android_capture_images_image(p->images, i),
            format, gpu.width, gpu.height, &c->submit);
      if (!gpu.buffers[i] || !p->entries[i])
         goto failed;
   }
   p->gpu = record_gpu_dispatch_mediacodec_new(io, &gpu);
   if (!p->gpu)
      goto failed;
   p->pipe = record_android_capture_pipe_new(
         p->gpu, p->entries, p->count, c->submit.close_fd, c->submit.context);
   if (!p->pipe)
      goto failed;
   *out = p;
   return true;
failed:
   record_android_capture_pool_cancel(p);
   record_android_capture_pool_finish(p, 0);
   if (!record_android_capture_pool_free(p))
      *out = p;
   return false;
}

int record_android_capture_pool_push(record_android_capture_pool_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration)
{
   int result;
   if (!p || p->closed || p->preparing || !p->pipe)
      return -1;
   result = record_android_capture_pipe_push(p->pipe, source, pts, duration);
   if (result == 1 || result == -2)
      p->renderer_end = pts + duration;
   return result;
}

int record_android_capture_pool_record(record_android_capture_pool_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command)
{
   int result;
   if (!p || p->closed || p->preparing || !p->pipe)
      return -1;
   result = record_android_capture_pipe_record(
         p->pipe, source, pts, duration, command);
   if (result == 1 || result == -2)
      p->renderer_end = pts + duration;
   return result;
}

int record_android_capture_pool_commit(record_android_capture_pool_t *p)
{
   if (!p || p->preparing || !p->pipe)
      return -1;
   return record_android_capture_pipe_commit(p->pipe);
}

bool record_android_capture_pool_abort_recorded(
      record_android_capture_pool_t *p)
{
   return p && !p->preparing && p->pipe
         && record_android_capture_pipe_abort_recorded(p->pipe);
}

int record_android_capture_pool_step(record_android_capture_pool_t *p)
{
   if (!p)
      return -1;
   if (p->preparing && (!p->destroyed || !record_acpool_terminal(p)))
      return 0;
   return p->pipe                    ? record_android_capture_pipe_step(p->pipe)
         : record_acpool_terminal(p) ? 1
                                     : 0;
}

enum record_dispatch_mediacodec_state record_android_capture_pool_status(
      const record_android_capture_pool_t *p, int *error, int *system_error)
{
   return record_gpu_dispatch_mediacodec_status(
         p ? p->gpu : NULL, error, system_error);
}

bool record_android_capture_pool_sources_retired(
      const record_android_capture_pool_t *p)
{
   return !p || record_android_capture_pipe_sources_retired(p->pipe);
}

bool record_android_capture_pool_prepare_device_destroy(
      record_android_capture_pool_t *p, bool queue_quiesced)
{
   unsigned i;
   if (!p || p->prepared)
      return true;
   if (!p->closed)
      return false;
   p->preparing = true;
   for (i = 0; i < p->count; i++)
      if (!record_android_capture_submit_prepare_device_destroy(
                p->entries[i], queue_quiesced))
         return false;
   record_android_capture_images_prepare_device_destroy(p->images);
   p->prepared = true;
   return true;
}

bool record_android_capture_pool_after_device_destroy(
      record_android_capture_pool_t *p)
{
   unsigned i;
   if (!p)
      return true;
   if (!p->prepared)
      return false;
   for (i = 0; i < p->count; i++)
      if (!record_android_capture_submit_after_device_destroy(p->entries[i]))
         return false;
   p->destroyed = true;
   return true;
}

bool record_android_capture_pool_device_state(
      const record_android_capture_pool_t *p, bool *destroyed)
{
   if (destroyed)
      *destroyed = p && p->destroyed;
   return !p || p->prepared;
}

static void record_acpool_renderer_release(void *context, bool destroyed)
{
   record_android_capture_pool_t *p = (record_android_capture_pool_t *)context;
   (void)destroyed;
   p->renderer_leases--;
}

static void record_acpool_renderer_close(record_android_capture_pool_t *p)
{
   if (p->renderer_stopping && !p->closed)
      record_android_capture_pool_finish(p, (int64_t)p->renderer_end);
}

static int record_acpool_renderer_record(void *context,
      const record_android_capture_source_t *source, VkCommandBuffer command,
      uint64_t pts, unsigned dims, unsigned mode)
{
   record_android_capture_pool_t *p = (record_android_capture_pool_t *)context;
   int result;
   if (p->closed || p->preparing || p->renderer_stopping)
      return 0;
   if (!source || source->owner != p
         || source->release != record_acpool_renderer_release || dims != p->dims
         || mode != p->mode)
   {
      record_android_capture_pool_cancel(p);
      record_acpool_renderer_close(p);
      return -1;
   }
   if (p->renderer_offer && pts <= p->renderer_last_offer)
      return 0;
   p->renderer_offer = true;
   p->renderer_last_offer = pts;
   result = record_android_capture_pool_record(
         p, source, pts, p->renderer_duration, command);
   if (result == 1)
   {
      p->renderer_leases++;
      p->renderer_end = pts + p->renderer_duration;
      p->renderer_recorded = true;
   }
   else if (result < 0 || (result == 0 && p->fail_on_drop))
   {
      record_android_capture_pool_cancel(p);
      record_acpool_renderer_close(p);
      return -1;
   }
   return result;
}

static void record_acpool_renderer_service(void *context)
{
   record_android_capture_pool_t *p = (record_android_capture_pool_t *)context;
   record_acpool_renderer_close(p);
   if (record_android_capture_pool_step(p) < 0)
   {
      record_android_capture_pool_cancel(p);
      record_acpool_renderer_close(p);
   }
}

static void record_acpool_renderer_submitted(void *context, VkResult result)
{
   record_android_capture_pool_t *p = (record_android_capture_pool_t *)context;
   if (!p->renderer_recorded)
      return;
   p->renderer_recorded = false;
   if (result != VK_SUCCESS || record_android_capture_pool_commit(p) < 0)
   {
      record_android_capture_pool_cancel(p);
      record_acpool_renderer_close(p);
      return;
   }
   record_acpool_renderer_service(p);
}

bool record_android_capture_pool_bind_renderer(record_android_capture_pool_t *p,
      unsigned duration, record_android_capture_renderer_t *binding)
{
   if (!p || !binding || !duration || duration > 1000000 || !p->pipe
         || p->closed || p->preparing || p->renderer_bound)
      return false;
   p->renderer_duration = duration;
   p->renderer_bound = true;
   binding->context = p;
   binding->flags = 0;
   binding->release = record_acpool_renderer_release;
   binding->record = record_acpool_renderer_record;
   binding->submitted = record_acpool_renderer_submitted;
   binding->service = record_acpool_renderer_service;
   return true;
}
