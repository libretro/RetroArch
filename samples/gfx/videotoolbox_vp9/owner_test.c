/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/videotoolbox_vp9_owner.h"
#ifdef VIDEOTOOLBOX_VP9_OWNER
static int owner_delay, owner_waits;
static void *owner_delayed_cookie;
static VTDecompressionSessionRef owner_delayed_session;
#define VT_VP9_OWNER_TEST
#define main setup_contract_main
#include "setup_test.c"
#undef main

static unsigned wakes;

static void wake(void *user)
{
   assert(user == &wakes);
   wakes++;
}

static videotoolbox_vp9_owner_t *open_owner(void)
{
   videotoolbox_vp9_owner_t *o;
   OSStatus status;
   memset(&header, 0, sizeof(header));
   memset(&color, 0, sizeof(color));
   header.w             = 1920;
   header.h             = 1080;
   header.bit_depth     = 8;
   header.subsampling_x = header.subsampling_y = 1;
   header.color_space                          = 2;
   color.transfer                              = 1;
   expected_pixel = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
   assert(!videotoolbox_vp9_owner_new(
         &header, &color, 1, 9, 64, NULL, wake, &wakes, &status));
   assert(status == kCMFormatDescriptionError_InvalidParameter);
   assert(!videotoolbox_vp9_owner_new(&header, &color, 1, 32, 64u * 1024 * 1024,
         NULL, wake, &wakes, &status));
   o = videotoolbox_vp9_owner_new(
         &header, &color, 1, 10, 64, NULL, wake, &wakes, &status);
   assert(o && !status);
   return o;
}

static void close_owner(videotoolbox_vp9_owner_t **o)
{
   assert(!videotoolbox_vp9_owner_retire_poll(o));
   videotoolbox_vp9_owner_close(*o);
   assert(videotoolbox_vp9_owner_retire_poll(o) && !*o);
}

static videotoolbox_vp9_owner_t *thread_owner;
static retro_atomic_int_t worker_stop;

static void run_worker(void *user)
{
   (void)user;
   while (!retro_atomic_load_acquire_int(&worker_stop))
      if (!videotoolbox_vp9_owner_worker(thread_owner))
         sthread_yield();
}

static void test_owner_threads(void)
{
   videotoolbox_vp9_result_t result;
   sthread_t *worker;
   unsigned i, retry;
   int status;
   thread_owner = open_owner();
   retro_atomic_int_init(&worker_stop, 0);
   worker = sthread_create(run_worker, NULL);
   assert(worker);
   for (i = 0; i < 2000; i++)
   {
      assert(!videotoolbox_vp9_owner_append(
            thread_owner, (const uint8_t *)"vp9", 3, true, 0));
      status = 1;
      for (retry = 0; retry < 1000000 && status == 1; retry++)
      {
         status = videotoolbox_vp9_owner_take(thread_owner, 0, &result);
         if (status == 1)
            sthread_yield();
      }
      assert(!status && result.image);
      CVPixelBufferRelease(result.image);
   }
   retro_atomic_store_release_int(&worker_stop, 1);
   sthread_join(worker);
   assert(!videotoolbox_vp9_owner_finish(thread_owner, false));
   close_owner(&thread_owner);
   assert(!retro_atomic_load_acquire_int(&objects));
}

#ifndef OWNER_MAIN_NAME
#define OWNER_MAIN_NAME main
#endif
int OWNER_MAIN_NAME(void)
{
   videotoolbox_vp9_owner_t *o = open_owner();
   videotoolbox_vp9_result_t result;
   CVPixelBufferRef image;
   unsigned i;
   uint8_t input[3] = {1, 2, 3};
   uint8_t big[20];
   for (i = 0; i < 10; i++)
      assert(!videotoolbox_vp9_owner_append(
            o, input, sizeof(input), true, (int)i));
   memset(input, 0xff, sizeof(input));
   assert(wakes == 10 && !owner_waits);
   assert(videotoolbox_vp9_owner_append(o, input, sizeof(input), true, 0) == 1);
   for (i = 0; i < 10; i++)
   {
      assert(o->work[i].packet.data[0] == 1);
      assert(videotoolbox_vp9_owner_take(o, i, &result) == 1);
      assert(videotoolbox_vp9_owner_worker(o) == 1);
      assert(!videotoolbox_vp9_owner_take(o, i, &result));
      assert(result.image);
      CVPixelBufferRelease(result.image);
   }
   assert(!videotoolbox_vp9_owner_worker(o));
   assert(!videotoolbox_vp9_owner_finish(o, false));
   close_owner(&o);

   o = open_owner();
   assert(!videotoolbox_vp9_owner_append(o, input, 3, false, -1));
   assert(videotoolbox_vp9_owner_finish(o, false) == 1);
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(!videotoolbox_vp9_owner_finish(o, false));
   assert(videotoolbox_vp9_owner_append(o, input, 3, false, -1) < 0);
   close_owner(&o);

   o = open_owner();
   assert(!videotoolbox_vp9_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(videotoolbox_vp9_owner_finish(o, false) == 1);
   assert(!videotoolbox_vp9_owner_take(o, 0, &result));
   image = result.image;
   assert(!videotoolbox_vp9_owner_finish(o, false));
   close_owner(&o);
   assert(retro_atomic_load_acquire_int(&objects) == 1);
   CVPixelBufferRelease(image);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   for (i = 0; i < 9; i++)
      assert(!videotoolbox_vp9_owner_append(o, input, 3, false, -1));
   assert(videotoolbox_vp9_owner_finish(o, false) == 1);
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(!videotoolbox_vp9_owner_finish(o, false));
   close_owner(&o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   memset(big, 3, sizeof(big));
   assert(!videotoolbox_vp9_owner_append(o, big, sizeof(big), false, -1));
   assert(videotoolbox_vp9_owner_append(o, big, sizeof(big), true, 0) == 1);
   assert(!videotoolbox_vp9_owner_append(o, big, sizeof(big), true, 0));
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(!videotoolbox_vp9_owner_finish(o, true));
   close_owner(&o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   assert(!videotoolbox_vp9_owner_append(o, input, 3, true, 0));
   videotoolbox_vp9_owner_close(o);
   assert(videotoolbox_vp9_owner_append(o, input, 3, true, 0) < 0);
   assert(videotoolbox_vp9_owner_finish(o, true) < 0);
   assert(videotoolbox_vp9_owner_retire_poll(&o) && !o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o           = open_owner();
   owner_delay = 1;
   assert(!videotoolbox_vp9_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(videotoolbox_vp9_owner_take(o, 0, &result) == 1);
   image = new_object(8);
   owner_delayed_session->callback.decompressionOutputCallback(
         o, owner_delayed_cookie, 0, 0, image, kCMTimeInvalid, kCMTimeInvalid);
   CVPixelBufferRelease(image);
   assert(!videotoolbox_vp9_owner_take(o, 0, &result));
   CVPixelBufferRelease(result.image);
   assert(videotoolbox_vp9_owner_finish(o, false) == 1);
   assert(videotoolbox_vp9_owner_restart(o) == 1);
   videotoolbox_vp9_owner_close(o);
   assert(!videotoolbox_vp9_owner_retire_poll(&o) && o);
   CFRelease(delayed_sample);
   delayed_sample = NULL;
   owner_delay    = 0;
   assert(videotoolbox_vp9_owner_retire_poll(&o) && !o
          && !retro_atomic_load_acquire_int(&objects));

   o            = open_owner();
   decode_error = -303;
   assert(!videotoolbox_vp9_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_vp9_owner_worker(o) == 1);
   assert(videotoolbox_vp9_owner_take(o, 0, &result) < 0);
   assert(videotoolbox_vp9_owner_restart(o) < 0);
   assert(videotoolbox_vp9_owner_append(o, input, 3, false, -1) < 0);
   close_owner(&o);
   decode_error = 0;
   test_owner_threads();
   puts("PASS: VP9 bounded owner queue, backpressure, final output, hidden EOS, native errors and delayed-block retirement");
   return 0;
}
#else
int main(void)
{
   return 0;
}
#endif
