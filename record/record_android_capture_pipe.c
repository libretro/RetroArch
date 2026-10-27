/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_pipe.h"
#include <stdlib.h>
#include <limits.h>

struct record_android_capture_pipe
{
   record_gpu_dispatch_mediacodec_t *gpu;
   record_android_capture_submit_t *entries[RECORD_GPU_QUEUE_SLOTS];
   void (*close_fd)(void *, int);
   void *context;
   record_gpu_frame_t frame;
   uint64_t last_pts, last_end;
   int64_t end_us;
   unsigned count, slot, cursor;
   uint32_t active, copied;
   int fd;
   bool reserved, pending, have_frame, cancelled, closed, retired, recorded;
};

record_android_capture_pipe_t *record_android_capture_pipe_new(
      record_gpu_dispatch_mediacodec_t *gpu,
      record_android_capture_submit_t *const *entries, unsigned count,
      void (*close_fd)(void *, int), void *context)
{
   record_android_capture_pipe_t *p;
   unsigned i, j;
   if (!gpu || !entries || !count || count > RECORD_GPU_QUEUE_SLOTS
         || !close_fd)
      return NULL;
   for (i = 0; i < count; i++)
   {
      if (!entries[i])
         return NULL;
      for (j = 0; j < i; j++)
         if (entries[i] == entries[j])
            return NULL;
   }
   p = (record_android_capture_pipe_t *)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->gpu = gpu;
   p->count = count;
   p->close_fd = close_fd;
   p->context = context;
   p->fd = -1;
   for (i = 0; i < count; i++)
      p->entries[i] = entries[i];
   return p;
}

void record_android_capture_pipe_cancel(record_android_capture_pipe_t *p)
{
   if (!p)
      return;
   p->cancelled = true;
   record_gpu_dispatch_mediacodec_cancel(p->gpu);
   if (p->fd >= 0)
      p->close_fd(p->context, p->fd);
   p->fd = -1;
   p->pending = false;
   if (p->reserved && record_gpu_dispatch_mediacodec_cancel_slot(p->gpu))
      p->reserved = false;
}

static int record_acpipe_push(record_android_capture_pipe_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command)
{
   uint64_t maximum = (((uint64_t)1 << 63) - 1) / 1000;
   int result;
   if (!p || !source || p->cancelled || p->closed || !duration || pts > maximum
         || duration > maximum - pts || (p->have_frame && pts <= p->last_pts))
      return -1;
   if (p->pending || p->recorded)
      return 0;
   if (!p->reserved)
   {
      result = record_gpu_dispatch_mediacodec_acquire(p->gpu, &p->slot);
      if (result != RECORD_GPU_QUEUE_OK)
         return result == RECORD_GPU_QUEUE_BUSY ? 0 : -1;
      p->reserved = true;
      if (p->slot >= p->count)
      {
         record_android_capture_pipe_cancel(p);
         return -1;
      }
   }
   if (p->active & ((uint32_t)1 << p->slot))
   {
      result = record_android_capture_submit_poll(p->entries[p->slot]);
      if (result == 0)
         return 0;
      if (result < 0
            || !record_android_capture_submit_release(p->entries[p->slot]))
      {
         record_android_capture_pipe_cancel(p);
         return -1;
      }
      p->active &= ~((uint32_t)1 << p->slot);
      p->copied &= ~((uint32_t)1 << p->slot);
   }
   result = command
         ? record_android_capture_submit_record(
                 p->entries[p->slot], source, command)
         : record_android_capture_submit_push(p->entries[p->slot], source);
   if (result == 1 || result == -2)
   {
      p->active |= (uint32_t)1 << p->slot;
      p->last_pts = pts;
      p->last_end = pts + duration;
      p->have_frame = true;
      p->frame.timestamp = pts;
      p->frame.duration = duration;
      p->frame.slot = p->slot;
      p->pending = true;
      p->recorded = command != VK_NULL_HANDLE;
   }
   if (result < 0)
      record_android_capture_pipe_cancel(p);
   return result;
}

int record_android_capture_pipe_push(record_android_capture_pipe_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration)
{
   return record_acpipe_push(p, source, pts, duration, VK_NULL_HANDLE);
}

int record_android_capture_pipe_record(record_android_capture_pipe_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command)
{
   if (!command)
      return -1;
   return record_acpipe_push(p, source, pts, duration, command);
}

int record_android_capture_pipe_commit(record_android_capture_pipe_t *p)
{
   int result;
   if (!p || !p->recorded)
      return -1;
   result = record_android_capture_submit_commit(p->entries[p->slot]);
   p->recorded = false;
   if (result < 0)
      record_android_capture_pipe_cancel(p);
   return result;
}

bool record_android_capture_pipe_abort_recorded(
      record_android_capture_pipe_t *p)
{
   if (!p || !p->recorded
         || !record_android_capture_submit_abort_recorded(p->entries[p->slot]))
      return false;
   p->active &= ~((uint32_t)1 << p->slot);
   p->copied &= ~((uint32_t)1 << p->slot);
   p->pending = p->recorded = false;
   return true;
}

int record_android_capture_pipe_step(record_android_capture_pipe_t *p)
{
   enum record_dispatch_mediacodec_state state;
   int result;
   unsigned slot;
   if (!p)
      return -1;
   if (p->retired)
      return 1;
   state = record_gpu_dispatch_mediacodec_status(p->gpu, NULL, NULL);
   if ((!p->closed && state == RECORD_MC_DISPATCH_DRAINING)
         || state == RECORD_MC_DISPATCH_FAILED)
      record_android_capture_pipe_cancel(p);
   if (p->pending && !p->cancelled)
   {
      if (p->recorded)
         return 0;
      if (p->fd < 0 && !p->frame.ready_value)
      {
         result = record_android_capture_submit_take_ready(
               p->entries[p->slot], &p->fd);
         if (result == 0)
         {
            result = record_android_capture_submit_poll(p->entries[p->slot]);
            if (result == 0)
               return 0;
            if (result > 0)
               result = record_android_capture_submit_take_ready(
                     p->entries[p->slot], &p->fd);
         }
         if (result != 1 || p->fd == INT_MAX)
         {
            record_android_capture_pipe_cancel(p);
            return -1;
         }
         p->frame.ready_value = (uint64_t)(p->fd + 1);
      }
      if (!record_gpu_dispatch_mediacodec_submit(p->gpu, &p->frame))
      {
         record_android_capture_pipe_cancel(p);
         return -1;
      }
      p->fd = -1;
      p->frame.ready_value = 0;
      p->pending = p->reserved = false;
      return 0;
   }
   if (!p->closed
         || (state != RECORD_MC_DISPATCH_DONE
               && state != RECORD_MC_DISPATCH_FAILED))
   {
      if (!p->cancelled)
         for (slot = 0; slot < p->count; slot++)
         {
            p->cursor = (p->cursor + 1) % p->count;
            if (!(p->active & ~p->copied & ((uint32_t)1 << p->cursor)))
               continue;
            result = record_android_capture_submit_poll(p->entries[p->cursor]);
            if (result < 0)
            {
               record_android_capture_pipe_cancel(p);
               return -1;
            }
            if (result == 1)
               p->copied |= (uint32_t)1 << p->cursor;
            break;
         }
      return 0;
   }
   for (slot = 0; slot < p->count; slot++)
   {
      p->cursor = (p->cursor + 1) % p->count;
      if (!(p->active & ((uint32_t)1 << p->cursor)))
         continue;
      result = record_android_capture_submit_discard(p->entries[p->cursor]);
      if (result != 1)
         return result;
      p->active &= ~((uint32_t)1 << p->cursor);
      if (p->slot == p->cursor)
         p->recorded = false;
      break;
   }
   p->retired = p->active == 0;
   return p->retired ? 1 : 0;
}

bool record_android_capture_pipe_finish(
      record_android_capture_pipe_t *p, int64_t end_us)
{
   if (!p || end_us < 0 || (uint64_t)end_us < p->last_end)
      return false;
   if (p->closed)
      return p->end_us == end_us;
   if (p->pending)
      return false;
   if (p->reserved)
   {
      if (!record_gpu_dispatch_mediacodec_cancel_slot(p->gpu))
         return false;
      p->reserved = false;
   }
   if (!record_gpu_dispatch_mediacodec_finish(p->gpu, end_us))
      return false;
   p->end_us = end_us;
   p->closed = true;
   return true;
}

bool record_android_capture_pipe_sources_retired(
      const record_android_capture_pipe_t *p)
{
   return !p || !(p->active & ~p->copied);
}

bool record_android_capture_pipe_free(record_android_capture_pipe_t *p)
{
   if (!p)
      return true;
   if (!p->retired || !p->closed || p->reserved || p->pending)
      return false;
   free(p);
   return true;
}
