/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_gpu_queue.h"
#ifdef HAVE_THREADS
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <retro_miscellaneous.h>
#include <time.h>
#endif
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n", (unsigned)__LINE__, #x); exit(1); } } while (0)

static void pool(unsigned slots)
{
   record_gpu_queue_t *q = record_gpu_queue_new(slots);
   record_gpu_frame_t frame = {0}, got;
   unsigned i, slot = 99;
   CHECK(q != NULL);
   CHECK(record_gpu_queue_take(q, &got) == RECORD_GPU_QUEUE_BUSY);
   CHECK(!record_gpu_queue_submit(q, &frame));
   CHECK(!record_gpu_queue_cancel(q));
   CHECK(!record_gpu_queue_release(q, 0));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_OK && slot == 0);
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_ERROR);
   CHECK(!record_gpu_queue_finish(q));
   CHECK(!record_gpu_queue_free(q));
   frame.slot = slots;
   CHECK(!record_gpu_queue_submit(q, &frame));
   CHECK(record_gpu_queue_cancel(q));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_OK && slot == 0);
   CHECK(record_gpu_queue_cancel(q));
   for (i = 0; i < slots; i++)
   {
      CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_OK && slot == i);
      frame.slot = slot; frame.timestamp = i; frame.duration = 2;
      frame.ready_value = i + 10; frame.keyframe = i == 0;
      CHECK(record_gpu_queue_submit(q, &frame));
      CHECK(!record_gpu_queue_submit(q, &frame));
   }
   CHECK(!record_gpu_queue_free(q));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
   CHECK(record_gpu_queue_finish(q));
   CHECK(record_gpu_queue_finish(q));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_END);
   for (i = 0; i < slots; i++)
   {
      CHECK(record_gpu_queue_take(q, &got) == RECORD_GPU_QUEUE_OK);
      CHECK(got.slot == i && got.timestamp == i && got.duration == 2);
      CHECK(got.ready_value == i + 10 && got.keyframe == (i == 0));
      CHECK(!record_gpu_queue_free(q));
   }
   /* EOS must reach the worker before outstanding GPU outputs drain. */
   CHECK(record_gpu_queue_take(q, &got) == RECORD_GPU_QUEUE_END);
   CHECK(!record_gpu_queue_release(q, slots));
   for (i = slots; i > 0; i--)
   {
      CHECK(record_gpu_queue_release(q, i - 1));
      CHECK(!record_gpu_queue_release(q, i - 1));
   }
   CHECK(record_gpu_queue_free(q));
}

static void recycle(void)
{
   record_gpu_queue_t *q = record_gpu_queue_new(3);
   record_gpu_frame_t frame = {0};
   unsigned i;
   CHECK(q != NULL);
   for (i = 0; i < 3; i++)
   {
      CHECK(record_gpu_queue_acquire(q, &frame.slot) == RECORD_GPU_QUEUE_OK);
      CHECK(record_gpu_queue_submit(q, &frame));
   }
   for (i = 0; i < 3; i++)
      CHECK(record_gpu_queue_take(q, &frame) == RECORD_GPU_QUEUE_OK);
   CHECK(record_gpu_queue_release(q, 1));
   CHECK(record_gpu_queue_acquire(q, &frame.slot) == RECORD_GPU_QUEUE_OK && frame.slot == 1);
   frame.timestamp = 99;
   CHECK(record_gpu_queue_submit(q, &frame));
   CHECK(record_gpu_queue_take(q, &frame) == RECORD_GPU_QUEUE_OK && frame.timestamp == 99);
   CHECK(record_gpu_queue_finish(q));
   CHECK(record_gpu_queue_take(q, &frame) == RECORD_GPU_QUEUE_END);
   CHECK(!record_gpu_queue_free(q));
   CHECK(record_gpu_queue_release(q, 2));
   CHECK(record_gpu_queue_release(q, 1));
   CHECK(record_gpu_queue_release(q, 0));
   CHECK(record_gpu_queue_free(q));
}

#ifdef HAVE_THREADS
#ifndef TEST_FRAMES
#define TEST_FRAMES 200000
#endif
struct stress
{
   record_gpu_queue_t *queue;
   retro_atomic_int_t failed;
   uint64_t pixels[RECORD_GPU_QUEUE_SLOTS];
   unsigned made;
};
/* 30 s without progress: a stall fails, a loaded machine does not */
static int stalled(unsigned count, unsigned *seen, time_t *deadline)
{
   time_t now = time(NULL);
   if (count != *seen)
   {
      *seen     = count;
      *deadline = now + 30;
      return 0;
   }
   return now >= *deadline;
}
static void producer(void *data)
{
   struct stress *s = (struct stress*)data;
   record_gpu_frame_t frame = {0};
   unsigned i = 0, spins = 0, seen = 0;
   time_t deadline = time(NULL) + 30;
   while (i < TEST_FRAMES
         && ((spins++ & 1023) || !stalled(i, &seen, &deadline)))
   {
      int result = record_gpu_queue_acquire(s->queue, &frame.slot);
      if (result == RECORD_GPU_QUEUE_BUSY)
         continue;
      if (result != RECORD_GPU_QUEUE_OK)
         break;
      s->pixels[frame.slot] = i;
      frame.timestamp = i; frame.duration = i + 1; frame.ready_value = i + 2;
      frame.keyframe = (i % 12) == 0;
      if (!record_gpu_queue_submit(s->queue, &frame))
         break;
      i++;
   }
   if (i != TEST_FRAMES)
   {
      retro_atomic_store_release_int(&s->failed, 1);
      record_gpu_queue_cancel(s->queue);
   }
   s->made = i;
   record_gpu_queue_finish(s->queue);
}
static void threaded(unsigned slots)
{
   struct stress *s = (struct stress*)calloc(1, sizeof(*s));
   sthread_t *thread;
   record_gpu_frame_t frame;
   unsigned n = 0, spins = 0, seen = 0;
   int result = RECORD_GPU_QUEUE_BUSY;
   time_t deadline = time(NULL) + 30;
   CHECK(s != NULL);
   retro_atomic_int_init(&s->failed, 0);
   s->queue = record_gpu_queue_new(slots);
   CHECK(s->queue != NULL);
   thread = sthread_create(producer, s);
   CHECK(thread != NULL);
   while ((spins++ & 1023) || !stalled(n, &seen, &deadline))
   {
      result = record_gpu_queue_take(s->queue, &frame);
      if (result == RECORD_GPU_QUEUE_END)
         break;
      if (result == RECORD_GPU_QUEUE_BUSY)
         continue;
      CHECK(result == RECORD_GPU_QUEUE_OK);
      CHECK(frame.slot < slots && s->pixels[frame.slot] == n);
      CHECK(frame.timestamp == n && frame.duration == n + 1 && frame.ready_value == n + 2);
      CHECK(frame.keyframe == ((n % 12) == 0));
      CHECK(record_gpu_queue_release(s->queue, frame.slot));
      n++;
   }
   sthread_join(thread);
   printf("slots %u: produced %u, consumed %u, result %d\n", slots, s->made, n, result);
   CHECK(!retro_atomic_load_acquire_int(&s->failed));
   CHECK(result == RECORD_GPU_QUEUE_END && n == TEST_FRAMES);
   CHECK(record_gpu_queue_free(s->queue));
   free(s);
}
#endif
int main(void)
{
   unsigned i;
   CHECK(record_gpu_queue_new(0) == NULL);
   CHECK(record_gpu_queue_new(RECORD_GPU_QUEUE_SLOTS + 1) == NULL);
   CHECK(record_gpu_queue_free(NULL));
   recycle();
   for (i = 1; i <= RECORD_GPU_QUEUE_SLOTS; i++)
      pool(i);
#ifdef HAVE_THREADS
   threaded(1); threaded(3); threaded(RECORD_GPU_QUEUE_SLOTS);
#endif
   puts("PASS GPU slot ownership, cancellation, backpressure and EOS");
   return 0;
}
