/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Assertions are required
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../record/record_android_capture_pipe.h"
struct record_android_capture_submit
{
   int push, poll, fd, taken, active, released, discarded, queries;
};
struct record_gpu_dispatch_mediacodec
{
   enum record_dispatch_mediacodec_state state;
   int reserved, owned, fail_submit, cancels, posts;
   unsigned slot;
};
static int closed_fd, closed_count;
static void close_fd(void *context, int fd)
{
   assert(context == &closed_count);
   closed_fd = fd;
   closed_count++;
}
int record_android_capture_submit_push(record_android_capture_submit_t *e,
      const record_android_capture_source_t *s)
{
   (void)s;
   assert(!e->active);
   if (e->push == 1 || e->push == -2)
      e->active = 1;
   return e->push;
}
int record_android_capture_submit_record(record_android_capture_submit_t *e,
      const record_android_capture_source_t *source, VkCommandBuffer command)
{
   assert(command);
   return record_android_capture_submit_push(e, source);
}
int record_android_capture_submit_commit(record_android_capture_submit_t *e)
{
   assert(e->active);
   return 1;
}
bool record_android_capture_submit_abort_recorded(
      record_android_capture_submit_t *e)
{
   assert(e->active);
   e->active = 0;
   return true;
}
int record_android_capture_submit_poll(record_android_capture_submit_t *e)
{
   e->queries++;
   return e->poll;
}
int record_android_capture_submit_take_ready(
      record_android_capture_submit_t *e, int *fd)
{
   if (!e->active || e->taken || e->poll == 0)
      return 0;
   if (e->poll < 0)
      return -1;
   *fd = e->fd;
   e->taken = 1;
   return 1;
}
bool record_android_capture_submit_release(record_android_capture_submit_t *e)
{
   assert(e->active && e->taken && e->poll == 1);
   e->active = e->taken = 0;
   e->released++;
   return true;
}
int record_android_capture_submit_discard(record_android_capture_submit_t *e)
{
   e->queries++;
   if (e->poll != 1)
      return e->poll;
   e->active = 0;
   e->discarded++;
   return 1;
}
int record_gpu_dispatch_mediacodec_acquire(
      record_gpu_dispatch_mediacodec_t *g, unsigned *slot)
{
   assert(!g->reserved);
   if (g->owned)
      return RECORD_GPU_QUEUE_BUSY;
   *slot = g->slot;
   g->reserved = 1;
   return RECORD_GPU_QUEUE_OK;
}
bool record_gpu_dispatch_mediacodec_submit(
      record_gpu_dispatch_mediacodec_t *g, const record_gpu_frame_t *f)
{
   assert(g->reserved && f->slot == g->slot && !f->keyframe);
   if (g->fail_submit)
      return false;
   g->reserved = 0;
   g->owned = 1;
   g->posts++;
   return true;
}
bool record_gpu_dispatch_mediacodec_cancel_slot(
      record_gpu_dispatch_mediacodec_t *g)
{
   assert(g->reserved);
   g->reserved = 0;
   return true;
}
void record_gpu_dispatch_mediacodec_cancel(record_gpu_dispatch_mediacodec_t *g)
{
   g->cancels++;
}
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *g, int64_t end)
{
   (void)end;
   assert(!g->reserved);
   return true;
}
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *g, int *error, int *system_error)
{
   (void)error;
   (void)system_error;
   return g->state;
}
#include "../../../record/record_android_capture_pipe.c"
static void lane(int fd, int deferred, int failure)
{
   record_android_capture_submit_t entry = {0};
   record_android_capture_submit_t *entries[2];
   record_gpu_dispatch_mediacodec_t gpu = {0};
   record_android_capture_source_t source = {0};
   record_android_capture_pipe_t *p;
   int queries, closes = closed_count;
   entries[0] = entries[1] = &entry;
   gpu.state = RECORD_MC_DISPATCH_READY;
   entry.push = failure == 2 ? -2 : 1;
   entry.poll = deferred ? 0 : 1;
   entry.fd = fd;
   assert(!record_android_capture_pipe_new(
         &gpu, entries, 2, close_fd, &closed_count));
   p = record_android_capture_pipe_new(
         &gpu, entries, 1, close_fd, &closed_count);
   assert(p && !record_android_capture_pipe_free(p));
   assert(record_android_capture_pipe_push(p, &source, 0, 10)
         == (failure == 2 ? -2 : 1));
   assert(record_android_capture_pipe_push(p, &source, 0, 10) == -1);
   if (failure != 2)
   {
      assert(!record_android_capture_pipe_finish(p, 10));
      if (deferred)
      {
         queries = entry.queries;
         assert(record_android_capture_pipe_step(p) == 0);
         assert(entry.queries == queries + 1 && gpu.posts == 0);
         entry.poll = 1;
      }
      gpu.fail_submit = failure == 1;
      assert(record_android_capture_pipe_step(p) == (failure == 1 ? -1 : 0));
      if (!failure)
      {
         assert(gpu.posts == 1);
         assert(record_android_capture_pipe_push(p, &source, 20, 10) == 0);
         gpu.owned = 0;
         entry.poll = 0;
         assert(record_android_capture_pipe_push(p, &source, 20, 10) == 0);
         assert(entry.released == 0 && gpu.reserved);
         assert(record_android_capture_pipe_finish(p, 10));
         assert(!gpu.reserved);
      }
   }
   if (failure)
   {
      assert(record_android_capture_pipe_finish(p, 10));
      assert(gpu.cancels && !gpu.reserved);
      assert(closed_count == closes + (failure == 1 && fd >= 0));
      if (failure == 1 && fd >= 0)
         assert(closed_fd == fd);
   }
   assert(!record_android_capture_pipe_finish(p, 9));
   assert(!record_android_capture_pipe_finish(p, 11));
   assert(record_android_capture_pipe_step(p) == 0);
   gpu.state = failure ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE;
   entry.poll = -1;
   assert(record_android_capture_pipe_step(p) == -1);
   assert(!record_android_capture_pipe_free(p));
   entry.poll = 0;
   assert(record_android_capture_pipe_step(p) == 0);
   entry.poll = 1;
   assert(record_android_capture_pipe_step(p) == 1);
   assert(entry.discarded == 1);
   assert(record_android_capture_pipe_step(p) == 1);
   assert(record_android_capture_pipe_free(p));
}
static void multiple(void)
{
   record_android_capture_submit_t first = {0}, second = {0};
   record_android_capture_submit_t *entries[2];
   record_gpu_dispatch_mediacodec_t gpu = {0};
   record_android_capture_source_t source = {0};
   record_android_capture_pipe_t *p;
   int queries;
   entries[0] = &first;
   entries[1] = &second;
   first.push = first.poll = second.push = second.poll = 1;
   first.fd = second.fd = -1;
   gpu.state = RECORD_MC_DISPATCH_READY;
   p = record_android_capture_pipe_new(
         &gpu, entries, 2, close_fd, &closed_count);
   assert(p);
   assert(record_android_capture_pipe_push(p, &source, 0, 10) == 1);
   assert(record_android_capture_pipe_step(p) == 0);
   gpu.owned = 0;
   gpu.slot = 1;
   assert(record_android_capture_pipe_push(p, &source, 20, 10) == 1);
   assert(record_android_capture_pipe_step(p) == 0);
   assert(!record_android_capture_pipe_sources_retired(p));
   assert(record_android_capture_pipe_step(p) == 0);
   assert(!record_android_capture_pipe_sources_retired(p));
   assert(record_android_capture_pipe_step(p) == 0);
   assert(record_android_capture_pipe_sources_retired(p));
   queries = first.queries + second.queries;
   assert(record_android_capture_pipe_step(p) == 0);
   assert(first.queries + second.queries == queries);
   assert(record_android_capture_pipe_finish(p, 30));
   gpu.owned = 0;
   gpu.state = RECORD_MC_DISPATCH_DONE;
   queries = first.queries + second.queries;
   assert(record_android_capture_pipe_step(p) == 0);
   assert(first.queries + second.queries == queries + 1);
   assert(!record_android_capture_pipe_free(p));
   assert(record_android_capture_pipe_step(p) == 1);
   assert(first.discarded == 1 && second.discarded == 1);
   assert(record_android_capture_pipe_free(p));
}
static void reuse(void)
{
   record_android_capture_submit_t entry = {0};
   record_android_capture_submit_t *entries[1];
   record_gpu_dispatch_mediacodec_t gpu = {0};
   record_android_capture_source_t source = {0};
   record_android_capture_pipe_t *p;
   entries[0] = &entry;
   gpu.state = RECORD_MC_DISPATCH_READY;
   entry.push = entry.poll = 1;
   entry.fd = -1;
   p = record_android_capture_pipe_new(
         &gpu, entries, 1, close_fd, &closed_count);
   assert(p);
   assert(record_android_capture_pipe_push(p, &source, 0, 0) == -1);
   assert(record_android_capture_pipe_push(p, &source, (uint64_t)-1, 1) == -1);
   assert(!gpu.reserved && !entry.active);
   assert(record_android_capture_pipe_push(p, &source, 0, 10) == 1);
   assert(record_android_capture_pipe_step(p) == 0);
   gpu.owned = 0;
   assert(record_android_capture_pipe_push(p, &source, 20, 10) == 1);
   assert(entry.released == 1);
   assert(!record_android_capture_pipe_sources_retired(p));
   assert(record_android_capture_pipe_step(p) == 0);
   assert(gpu.posts == 2);
   assert(record_android_capture_pipe_finish(p, 30));
   gpu.state = RECORD_MC_DISPATCH_DRAINING;
   assert(record_android_capture_pipe_step(p) == 0);
   assert(!gpu.cancels);
   gpu.owned = 0;
   gpu.state = RECORD_MC_DISPATCH_DONE;
   assert(record_android_capture_pipe_step(p) == 1);
   assert(record_android_capture_pipe_free(p));
}
int main(void)
{
   reuse();
   multiple();
   lane(-1, 0, 0);
   lane(4, 1, 0);
   lane(5, 1, 1);
   lane(-1, 0, 1);
   lane(6, 1, 2);
   puts("PASS: capture relay deferred submission, dual retirement and "
        "cancellation");
   return 0;
}
