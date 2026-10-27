/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error GPU retirement acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_mediacodec_gpu_worker.h"
#define SURFACE ((record_mediacodec_surface_t *)1)
#define CAPTURE ((record_android_capture_gl_t *)2)
#define DRAW ((record_android_capture_draw_t *)3)
static bool active, terminal, complete, reading[16], fenced[16];
static unsigned posts, draws, imports, closes, retire_scopes, begins;
static int failure, restore_fail;
static bool recovery_fails;
static int64_t pts;
bool record_mediacodec_surface_begin(record_mediacodec_surface_t *s)
{
   assert(s == SURFACE && !active && !terminal);
   begins++;
   if (failure == 1)
      return false;
   active = true;
   return true;
}
bool record_mediacodec_surface_begin_retirement(record_mediacodec_surface_t *s)
{
   assert(s == SURFACE);
   active = terminal = true;
   retire_scopes++;
   return true;
}
bool record_mediacodec_surface_leave(record_mediacodec_surface_t *s)
{
   assert(s == SURFACE);
   if (active && restore_fail)
   {
      restore_fail--;
      return false;
   }
   active = false;
   return true;
}
bool record_mediacodec_surface_present(
      record_mediacodec_surface_t *s, int64_t at)
{
   assert(s == SURFACE && active && !terminal && at >= 0);
   pts = at;
   posts++;
   if (failure == 7)
      restore_fail = 1;
   return record_mediacodec_surface_leave(s) && failure != 6;
}
bool record_android_capture_gl_begin(
      record_android_capture_gl_t *c, unsigned slot, int *fd, GLuint *texture)
{
   assert(c == CAPTURE && active && slot < 16 && !reading[slot] && *fd >= -1);
   if (failure == 2)
      return false;
   if (*fd >= 0)
      imports++;
   *fd = -2;
   if (failure == 3)
      return false;
   reading[slot] = true;
   *texture = slot + 1;
   return true;
}
bool record_android_capture_draw_frame(
      record_android_capture_draw_t *d, GLuint t)
{
   assert(d == DRAW && active && t && reading[t - 1]);
   draws++;
   return failure != 4;
}
bool record_android_capture_gl_end(
      record_android_capture_gl_t *c, unsigned slot)
{
   assert(c == CAPTURE && active && reading[slot]);
   fenced[slot] = failure != 5;
   return fenced[slot];
}
int record_android_capture_gl_poll(
      record_android_capture_gl_t *c, unsigned slot)
{
   assert(c == CAPTURE && active && slot < 16);
   if (!reading[slot])
      return 1;
   if (!fenced[slot])
      return -1;
   if (!complete)
      return 0;
   reading[slot] = fenced[slot] = false;
   return 1;
}
int record_android_capture_gl_recover_step(record_android_capture_gl_t *c)
{
   assert(c == CAPTURE && active);
   if (recovery_fails)
      return -1;
   if (!complete)
      return 0;
   memset(reading, 0, sizeof(reading));
   memset(fenced, 0, sizeof(fenced));
   return 1;
}
void record_android_capture_gl_after_context_retired(
      record_android_capture_gl_t *c)
{
   assert(c == CAPTURE && !active);
   memset(reading, 0, sizeof(reading));
   memset(fenced, 0, sizeof(fenced));
}
static void close_fd(void *ctx, int fd)
{
   assert(ctx == &closes && fd == 7);
   closes++;
}
static void reset(void)
{
   unsigned i;
   assert(!active);
   for (i = 0; i < 16; i++)
      assert(!reading[i] && !fenced[i]);
   terminal = complete = false;
   recovery_fails = false;
   posts = draws = imports = closes = retire_scopes = begins = 0;
   failure = restore_fail = 0;
   pts = -1;
}
static record_mediacodec_gpu_worker_t *create(record_gpu_queue_t *q, unsigned n)
{
   return record_mediacodec_gpu_worker_new(
         q, SURFACE, CAPTURE, DRAW, n, close_fd, &closes);
}
static void submit(record_gpu_queue_t *q, uint64_t at, uint64_t ready, bool key)
{
   record_gpu_frame_t frame;
   memset(&frame, 0, sizeof(frame));
   assert(record_gpu_queue_acquire(q, &frame.slot) == RECORD_GPU_QUEUE_OK);
   frame.timestamp = at;
   frame.ready_value = ready;
   frame.keyframe = key;
   assert(record_gpu_queue_submit(q, &frame));
}
int main(void)
{
   record_gpu_queue_t *q;
   record_mediacodec_gpu_worker_t *w;
   unsigned i, slot;
   for (i = 1; i <= 16; i++)
   {
      unsigned j;
      reset();
      q = record_gpu_queue_new(i);
      w = create(q, i);
      assert(q && w);
      for (j = 0; j < i; j++)
      {
         submit(q, j, 8, false);
         assert(record_mediacodec_gpu_worker_step(w) == 1 && pts == (int64_t)j);
      }
      assert(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
      assert(record_gpu_queue_finish(q));
      assert(record_mediacodec_gpu_worker_step(w) == 0);
      assert(!record_mediacodec_gpu_worker_free(w));
      assert(!record_gpu_queue_free(q));
      complete = true;
      for (j = 0; j < i; j++)
         assert(record_mediacodec_gpu_worker_step(w) == (j + 1 == i ? 2 : 1));
      assert(posts == i && draws == i && imports == i && !closes);
      assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   }
   reset();
   q = record_gpu_queue_new(1);
   w = create(q, 1);
   assert(w);
   for (i = 0; i < 10000; i++)
   {
      submit(q, i, i & 1 ? 0 : 8, false);
      complete = false;
      assert(record_mediacodec_gpu_worker_step(w) == 1);
      assert(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
      assert(record_mediacodec_gpu_worker_step(w) == 0);
      complete = true;
      assert(record_mediacodec_gpu_worker_step(w) == 1);
   }
   assert(record_gpu_queue_finish(q));
   assert(record_mediacodec_gpu_worker_step(w) == 2);
   assert(posts == 10000 && draws == 10000 && imports == 5000 && !closes);
   assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   for (i = 1; i <= 7; i++)
   {
      reset();
      q = record_gpu_queue_new(1);
      w = create(q, 1);
      assert(w);
      submit(q, 0, 8, false);
      failure = (int)i;
      assert(record_mediacodec_gpu_worker_step(w) == -1);
      assert(record_mediacodec_gpu_worker_step(w) == -1);
      assert(record_gpu_queue_finish(q));
      failure = 0;
      recovery_fails = i == 5;
      if (i >= 4)
      {
         assert(record_mediacodec_gpu_worker_retire_step(w)
               == (i == 5 ? -1 : 0));
         assert(!record_mediacodec_gpu_worker_free(w));
      }
      if (i == 5)
         record_mediacodec_gpu_worker_after_context_retired(w);
      complete = true;
      assert(record_mediacodec_gpu_worker_retire_step(w) == 2);
      assert(closes == (i <= 2 ? 1u : 0u));
      assert(posts == (i >= 6 ? 1u : 0u));
      assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   }
   reset();
   q = record_gpu_queue_new(1);
   w = create(q, 1);
   assert(w);
   submit(q, 0, 0, false);
   failure = 5;
   assert(record_mediacodec_gpu_worker_step(w) == -1);
   failure = 0;
   assert(record_gpu_queue_finish(q));
   assert(record_mediacodec_gpu_worker_retire_step(w) == 0);
   assert(!record_mediacodec_gpu_worker_free(w));
   complete = true;
   assert(record_mediacodec_gpu_worker_retire_step(w) == 2);
   assert(draws == 1 && !posts);
   assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   reset();
   q = record_gpu_queue_new(3);
   w = create(q, 3);
   assert(w);
   submit(q, 0, 8, false);
   submit(q, 1, 0, false);
   submit(q, 2, 8, false);
   assert(record_gpu_queue_finish(q));
   for (i = 0; i < 3; i++)
      assert(record_mediacodec_gpu_worker_retire_step(w) == 1);
   assert(record_mediacodec_gpu_worker_retire_step(w) == 2);
   assert(closes == 2 && !begins && !posts && !draws);
   assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   reset();
   q = record_gpu_queue_new(1);
   w = create(q, 1);
   assert(w);
   submit(q, 0, 8, true);
   assert(record_mediacodec_gpu_worker_step(w) == -1 && closes == 1 && !begins);
   assert(record_gpu_queue_finish(q));
   assert(record_mediacodec_gpu_worker_retire_step(w) == 2);
   assert(record_mediacodec_gpu_worker_free(w) && record_gpu_queue_free(q));
   puts("PASS: real capture queue, 10000 Surface posts, GPU retirement, EOS "
        "and failure drain");
   return 0;
}
