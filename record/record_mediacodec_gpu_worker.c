/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_mediacodec_gpu_worker.h"
#include <limits.h>
#include <stdlib.h>

struct record_mediacodec_gpu_worker
{
   record_gpu_queue_t *queue;
   record_mediacodec_surface_t *surface;
   record_android_capture_gl_t *capture;
   record_android_capture_draw_t *draw;
   void (*close_fd)(void *context, int fd);
   void *context;
   uint32_t pending;
   unsigned count, next;
   bool failed, closed, ended, started, context_retired;
};
record_mediacodec_gpu_worker_t *record_mediacodec_gpu_worker_new(
      record_gpu_queue_t *queue, record_mediacodec_surface_t *surface,
      record_android_capture_gl_t *capture, record_android_capture_draw_t *draw,
      unsigned count, void (*close_fd)(void *context, int fd), void *context)
{
   record_mediacodec_gpu_worker_t *w;
   if (!queue || !surface || !capture || !draw || !count || count > 16
         || !close_fd)
      return NULL;
   w = (record_mediacodec_gpu_worker_t *)calloc(1, sizeof(*w));
   if (!w)
      return NULL;
   w->queue = queue;
   w->surface = surface;
   w->capture = capture;
   w->draw = draw;
   w->count = count;
   w->close_fd = close_fd;
   w->context = context;
   return w;
}
static bool record_mcg_release(record_mediacodec_gpu_worker_t *w, unsigned slot)
{
   if (!record_gpu_queue_release(w->queue, slot))
      return false;
   w->pending &= ~((uint32_t)1 << slot);
   return true;
}
static int record_mcg_poll(record_mediacodec_gpu_worker_t *w)
{
   unsigned i, slot;
   int result;
   for (i = 0; i < w->count; i++)
   {
      slot = (w->next + i) % w->count;
      if (!(w->pending & ((uint32_t)1 << slot)))
         continue;
      w->next = (slot + 1) % w->count;
      if (w->context_retired)
         return record_mcg_release(w, slot) ? 1 : -1;
      if (!(w->failed ? record_mediacodec_surface_begin_retirement(w->surface)
                      : record_mediacodec_surface_begin(w->surface)))
         return -1;
      result = w->failed ? record_android_capture_gl_recover_step(w->capture)
                         : record_android_capture_gl_poll(w->capture, slot);
      if (!record_mediacodec_surface_leave(w->surface))
         return -1;
      if (result == 1)
         return record_mcg_release(w, slot) ? 1 : -1;
      return result;
   }
   return 0;
}
static void record_mcg_close(record_mediacodec_gpu_worker_t *w, uint64_t value)
{
   if (value && value - 1 <= INT_MAX)
      w->close_fd(w->context, (int)(value - 1));
}
static int record_mcg_service(record_mediacodec_gpu_worker_t *w, bool retire)
{
   record_gpu_frame_t frame;
   GLuint texture = 0;
   bool drew, fenced;
   int result, fd, progress;
   if (!w || (w->failed && !retire))
      return -1;
   if (w->ended)
      return 2;
   w->started = true;
   if (retire)
   {
      w->failed = true;
      if (!w->context_retired && !record_mediacodec_surface_leave(w->surface))
         return -1;
   }
   progress = record_mcg_poll(w);
   if (progress < 0)
      goto failed;
   result = w->closed ? RECORD_GPU_QUEUE_END
                      : record_gpu_queue_take(w->queue, &frame);
   if (result == RECORD_GPU_QUEUE_END)
   {
      w->closed = true;
      if (!w->pending)
      {
         w->ended = true;
         return 2;
      }
      return progress;
   }
   if (result == RECORD_GPU_QUEUE_BUSY)
      return progress;
   if (result != RECORD_GPU_QUEUE_OK)
      goto failed;
   if (frame.slot < 16)
      w->pending |= (uint32_t)1 << frame.slot;
   if (retire)
   {
      record_mcg_close(w, frame.ready_value);
      if (!record_mcg_release(w, frame.slot))
         goto failed;
      return 1;
   }
   if (frame.slot >= w->count || frame.keyframe
         || frame.ready_value > (uint64_t)INT_MAX + 1
         || frame.timestamp > (((uint64_t)1 << 63) - 1) / 1000)
      goto refused;
   fd = frame.ready_value ? (int)(frame.ready_value - 1) : -1;
   if (!record_mediacodec_surface_begin(w->surface))
      goto refused;
   if (!record_android_capture_gl_begin(w->capture, frame.slot, &fd, &texture))
   {
      if (fd >= 0)
         w->close_fd(w->context, fd);
      record_mcg_release(w, frame.slot);
      record_mediacodec_surface_leave(w->surface);
      goto failed;
   }
   w->pending |= (uint32_t)1 << frame.slot;
   drew = record_android_capture_draw_frame(w->draw, texture);
   fenced = record_android_capture_gl_end(w->capture, frame.slot);
   if (!drew || !fenced)
   {
      record_mediacodec_surface_leave(w->surface);
      goto failed;
   }
   if (!record_mediacodec_surface_present(w->surface, (int64_t)frame.timestamp))
      goto failed;
   return 1;
refused:
   record_mcg_close(w, frame.ready_value);
   record_mcg_release(w, frame.slot);
failed:
   w->failed = true;
   return -1;
}
int record_mediacodec_gpu_worker_step(record_mediacodec_gpu_worker_t *w)
{
   return record_mcg_service(w, false);
}
int record_mediacodec_gpu_worker_retire_step(record_mediacodec_gpu_worker_t *w)
{
   return record_mcg_service(w, true);
}
void record_mediacodec_gpu_worker_after_context_retired(
      record_mediacodec_gpu_worker_t *w)
{
   if (!w)
      return;
   w->failed = w->context_retired = true;
   record_android_capture_gl_after_context_retired(w->capture);
}
bool record_mediacodec_gpu_worker_free(record_mediacodec_gpu_worker_t *w)
{
   if (!w)
      return true;
   if (w->pending || (w->started && !w->ended))
      return false;
   free(w);
   return true;
}
