/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_pool.h"
#include <stdlib.h>

struct record_android_capture_pool
{
   record_android_capture_images_t *images;
   record_android_capture_submit_t *entries[RECORD_GPU_QUEUE_SLOTS];
   record_gpu_dispatch_mediacodec_t *gpu;
   record_android_capture_pipe_t *pipe;
   int64_t end_us;
   unsigned count;
   bool closed, preparing, prepared, destroyed;
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
   record_android_capture_images_free(p->images);
   free(p);
   return true;
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
   if (p->pipe)
      record_android_capture_pipe_cancel(p->pipe);
   else if (p->gpu)
      record_gpu_dispatch_mediacodec_cancel(p->gpu);
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
   p->count = c->gpu.count;
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
   if (!p || p->closed || !p->pipe)
      return -1;
   return record_android_capture_pipe_push(p->pipe, source, pts, duration);
}

int record_android_capture_pool_record(record_android_capture_pool_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command)
{
   if (!p || p->closed || !p->pipe)
      return -1;
   return record_android_capture_pipe_record(
         p->pipe, source, pts, duration, command);
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
