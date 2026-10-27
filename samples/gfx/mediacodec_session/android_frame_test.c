/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <formats/image.h>
#include "../../../gfx/common/android_frame.h"
#include "../../../gfx/common/mediacodec_retire.h"
static int native, deleted;
static int acquire(void *reader, void **image, int *fd)
{
   assert(reader == &native);
   *image = &native;
   *fd = -1;
   return 1;
}
static bool describe(void *image, android_image_view_t *view)
{
   assert(image == &native);
   view->buffer = &native;
   view->width = view->right = view->buffer_width = 64;
   view->height = view->bottom = view->buffer_height = 32;
   return true;
}
static void delete_image(void *image)
{
   assert(image == &native);
   deleted++;
}
static void delete_reader(void *reader)
{
   assert(reader == &native);
}
static android_image_queue_t *queue;
static android_frame_pool_t *pool;
static video_hw_frame_t shared_frame;
static retro_atomic_int_t ready;
static void render(void *unused)
{
   unsigned i;
   android_frame_view_t view;
   (void)unused;
   for (i = 0; i < 10000; i++)
   {
      while (!retro_atomic_load_acquire_int(&ready))
         sthread_yield();
      assert(android_frame_claim(shared_frame.owner, shared_frame.done,
            shared_frame.done_value, shared_frame.image, &view));
      assert(view.image.buffer == &native);
      assert(android_frame_finish(shared_frame.owner, shared_frame.done,
            shared_frame.done_value, -1));
      retro_atomic_store_release_int(&ready, 0);
   }
   return;
}
static unsigned notifier_refs, notifications;
static void notifier_retain(void *user)
{
   assert(user == &notifications && !notifier_refs);
   notifier_refs++;
}
static void notifier_complete(void *user)
{
   assert(user == &notifications && notifier_refs == 1);
   assert(android_frame_pool_free(pool));
   pool = NULL;
   notifications++;
   notifier_refs--;
}
#ifndef _WIN32
static int retirement_fd;
static retro_atomic_int_t fence_entered, fence_completed;
static void fence_executor(void *unused)
{
   (void)unused;
   retro_atomic_store_release_int(&fence_entered, 1);
   assert(mediacodec_retire_fence(retirement_fd));
   retro_atomic_store_release_int(&fence_completed, 1);
}
#endif
static void retirement_events(const android_image_ops_t *ops)
{
   static android_frame_view_t view, claimed;
   static video_hw_frame_t frame;
#ifndef _WIN32
   int done[2];
   sthread_t *thread;
#endif
   queue = android_image_queue_new(&native, 1, ops);
   pool = android_frame_pool_new(1);
   assert(queue && pool);
   assert(!android_frame_pool_set_retire_notifier(pool, NULL,
         notifier_retain, notifier_complete));
   assert(android_frame_pool_set_retire_notifier(pool, &notifications,
         notifier_retain, notifier_complete));
   assert(!android_frame_pool_set_retire_notifier(pool, &notifications,
         notifier_retain, notifier_complete));
   memset(&view, 0, sizeof(view));
   view.format.serial = 1;
   view.pixfmt = IMAGE_PIXFMT_NV12;
   assert(android_image_queue_acquire(queue, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   assert(android_frame_claim(frame.owner, frame.done, frame.done_value,
         frame.image, &claimed));
   android_frame_pool_close(pool);
   android_image_queue_close(queue);
   assert(!android_frame_finish(frame.owner, frame.done, frame.done_value + 1, -1));
   assert(!notifier_refs && !notifications);
#ifdef _WIN32
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(android_image_queue_retire_fd(queue) == -1);
#else
   assert(pipe(done) == 0);
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, done[0]));
   retirement_fd = android_image_queue_retire_fd(queue);
   assert(retirement_fd == done[0] && !android_image_queue_poll(queue));
   assert(!mediacodec_retire_fence(-1));
   retro_atomic_int_init(&fence_entered, 0);
   retro_atomic_int_init(&fence_completed, 0);
   thread = sthread_create(fence_executor, NULL);
   assert(thread);
   while (!retro_atomic_load_acquire_int(&fence_entered))
      sthread_yield();
   assert(!retro_atomic_load_acquire_int(&fence_completed));
   assert(write(done[1], "x", 1) == 1);
   sthread_join(thread);
   assert(retro_atomic_load_acquire_int(&fence_completed));
   close(done[1]);
#endif
   assert(!pool && !notifier_refs && notifications == 1);
   assert(android_image_queue_poll(queue) == 1);
   assert(android_image_queue_retire_fd(queue) == -1);
   assert(android_image_queue_free(queue));
}
int main(void)
{
   android_image_ops_t ops;
   android_frame_view_t view, taken;
   video_hw_frame_t frame, old, untouched;
#ifndef _WIN32
   int done[2];
#endif
   unsigned i;
   sthread_t *thread;
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   queue = android_image_queue_new(&native, 1, &ops);
   assert(queue);
   pool = android_frame_pool_new(1);
   assert(pool);
   memset(&view, 0, sizeof(view));
   view.format.serial = 1;
   view.pixfmt = IMAGE_PIXFMT_P010;
   assert(android_image_queue_acquire(queue, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   old = frame;
   assert(android_frame_callback(frame.abandon));
   memset(&untouched, 0x55, sizeof(untouched));
   taken = view;
   assert(android_frame_export(pool, &view, &untouched) == 1
         && untouched.width == 0x55555555);
   assert(!android_frame_claim(
         frame.owner, frame.done, frame.done_value + 1, frame.image, &taken));
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &taken));
   assert(
         !android_frame_unclaim(frame.owner, frame.done, frame.done_value + 1));
   assert(android_frame_unclaim(frame.owner, frame.done, frame.done_value));
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &taken));
   video_hw_frame_abandon(
         &old); /* Already claimed: cannot release GPU-held input. */
   assert(!android_image_queue_poll(queue));
   old = frame;
#ifdef _WIN32
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(android_image_queue_poll(queue) == 1);
#else
   assert(pipe(done) == 0);
   assert(android_frame_finish(
         frame.owner, frame.done, frame.done_value, done[0]));
   assert(!android_image_queue_poll(queue));
   assert(write(done[1], "x", 1) == 1);
   assert(android_image_queue_poll(queue) == 1);
   close(done[1]);
#endif
   assert(android_image_queue_acquire(queue, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   video_hw_frame_abandon(&old);
   assert(!android_image_queue_poll(queue));
   android_frame_pool_close(pool);
   assert(!android_frame_pool_free(pool));
   video_hw_frame_abandon(&frame);
   assert(android_image_queue_poll(queue) == 1);
   assert(android_frame_pool_free(pool));
   pool = android_frame_pool_new(1);
   assert(pool);
   retro_atomic_int_init(&ready, 0);
   thread = sthread_create(render, NULL);
   assert(thread);
   for (i = 0; i < 10000; i++)
   {
      assert(android_image_queue_acquire(queue, &view.image) == 1);
      assert(!android_frame_export(pool, &view, &shared_frame));
      retro_atomic_store_release_int(&ready, 1);
      while (retro_atomic_load_acquire_int(&ready))
         sthread_yield();
      assert(android_image_queue_poll(queue) == 1);
   }
   sthread_join(thread);
   android_frame_pool_close(pool);
   assert(android_frame_pool_free(pool));
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue));
   assert(deleted == 10002);
   retirement_events(&ops);
#ifdef _WIN32
   puts("PASS: opaque Android leases and 10000 threaded handoffs; native fence "
        "not executed on Windows");
#else
   puts("PASS: opaque Android frame leases, stale/claimed abandonment, delayed "
        "GPU fence and 10000 threaded handoffs");
#endif
   return 0;
}
