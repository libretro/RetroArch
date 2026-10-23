/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <rthreads/rthreads.h>
#include "../../../gfx/common/videotoolbox_av1_owner.h"
#ifdef VIDEOTOOLBOX_AV1_OWNER
static int owner_delay, owner_waits;
static void *owner_delayed_cookie;
static VTDecompressionSessionRef owner_delayed_session;
#define VT_AV1_OWNER_TEST
#define main setup_contract_main
#include "setup_test.c"
#undef main

CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef image)
{
   assert(image->kind == 8);
   retain(image);
   return image;
}
void CVPixelBufferRelease(CVPixelBufferRef image)
{
   assert(image->kind == 8);
   CFRelease(image);
}
static int block_error, sample_error, decode_error;
static CMSampleBufferRef delayed_sample;
OSStatus CMBlockBufferCreateWithMemoryBlock(void *allocator, void *data,
      size_t size, void *block_allocator,
      const CMBlockBufferCustomBlockSource *source, size_t offset,
      size_t length, unsigned flags, CMBlockBufferRef *out)
{
   assert(!allocator && block_allocator == kCFAllocatorNull && !offset
         && !flags);
   assert(source->version == 0 && !source->AllocateBlock && source->FreeBlock);
   assert(data && size == length);
   *out = new_object(6);
   if (*out)
   {
      (*out)->source = *source;
      (*out)->data = data;
      (*out)->size = size;
   }
   return *out ? block_error : -1001;
}

OSStatus CMSampleBufferCreateReady(void *allocator, CMBlockBufferRef block,
      CMVideoFormatDescriptionRef format, long samples, long times,
      const CMSampleTimingInfo *timing, long sizes, const size_t *size,
      CMSampleBufferRef *out)
{
   assert(!allocator && format && samples == 1 && sizes == 1);
   assert(*size == block->size);
   assert(times == 0 || (times == 1 && timing));
   *out = new_object(7);
   if (*out)
   {
      (*out)->format = block;
      retain(block);
      if (times)
         (*out)->timing = *timing;
   }
   return *out ? sample_error : -1001;
}

OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef session,
      CMSampleBufferRef sample, VTDecodeFrameFlags flags, void *ref,
      VTDecodeInfoFlags *info)
{
   CVPixelBufferRef image = NULL;
   assert(session && ref
         && (flags & kVTDecodeFrame_EnableAsynchronousDecompression));
   if (info)
      *info = 0;
   if (decode_error)
      return decode_error;
   if (owner_delay)
   {
      assert(!delayed_sample);
      retain(sample);
      delayed_sample = sample;
      owner_delayed_cookie = ref;
      owner_delayed_session = session;
      return 0;
   }
   if (!(flags & kVTDecodeFrame_DoNotOutputFrame))
      image = new_object(8);
   session->callback.decompressionOutputCallback(
         session->callback.decompressionOutputRefCon, ref, 0, 0, image,
         kCMTimeInvalid, kCMTimeInvalid);
   if (image)
      CVPixelBufferRelease(image);
   return 0;
}
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(
      VTDecompressionSessionRef session)
{
   assert(session && session->kind == 5);
   owner_waits++;
   return 0;
}

static unsigned wakes;

static void wake(void *user)
{
   assert(user == &wakes);
   wakes++;
}

static videotoolbox_av1_owner_t *open_owner(void)
{
   videotoolbox_av1_owner_t *o;
   OSStatus status;
   static uint8_t bytes[128];
   rav1_obu_t sequence;
   size_t offset = 0;
   bytes[0] = 0x0a;
   bytes[1] = (uint8_t)sizeof(av1_sequence_8_normal);
   memcpy(bytes + 2, av1_sequence_8_normal, sizeof(av1_sequence_8_normal));
   assert(rav1_obu_next(&sequence, bytes, sizeof(av1_sequence_8_normal) + 2,
                &offset, false)
         == 1);
   assert(
         rav1_sequence_parse(&header, sequence.payload, sequence.payload_size));
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   expected_pixel = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
   assert(!videotoolbox_av1_owner_new(
         &header, &color, &sequence, 9, 64, NULL, wake, &wakes, &status));
   assert(status == kCMFormatDescriptionError_InvalidParameter);
   assert(!videotoolbox_av1_owner_new(&header, &color, &sequence, 32,
         64u * 1024 * 1024, NULL, wake, &wakes, &status));
   o = videotoolbox_av1_owner_new(
         &header, &color, &sequence, 10, 64, NULL, wake, &wakes, &status);
   assert(o && !status);
   return o;
}

static void close_owner(videotoolbox_av1_owner_t **o)
{
   assert(!videotoolbox_av1_owner_retire_poll(o));
   videotoolbox_av1_owner_close(*o);
   assert(videotoolbox_av1_owner_retire_poll(o) && !*o);
}

static videotoolbox_av1_owner_t *thread_owner;
static retro_atomic_int_t worker_stop;

static void run_worker(void *user)
{
   (void)user;
   while (!retro_atomic_load_acquire_int(&worker_stop))
      if (!videotoolbox_av1_owner_worker(thread_owner))
         sthread_yield();
}

static void test_owner_threads(void)
{
   videotoolbox_av1_result_t result;
   sthread_t *worker;
   unsigned i, retry;
   int status;
   thread_owner = open_owner();
   retro_atomic_int_init(&worker_stop, 0);
   worker = sthread_create(run_worker, NULL);
   assert(worker);
   for (i = 0; i < 2000; i++)
   {
      assert(!videotoolbox_av1_owner_append(
            thread_owner, (const uint8_t *)"\x32\x01\x80", 3, true, 0));
      status = 1;
      for (retry = 0; retry < 1000000 && status == 1; retry++)
      {
         status = videotoolbox_av1_owner_take(thread_owner, 0, &result);
         if (status == 1)
            sthread_yield();
      }
      assert(!status && result.image);
      CVPixelBufferRelease(result.image);
   }
   retro_atomic_store_release_int(&worker_stop, 1);
   sthread_join(worker);
   assert(!videotoolbox_av1_owner_finish(thread_owner, false));
   close_owner(&thread_owner);
   assert(!retro_atomic_load_acquire_int(&objects));
}

static videotoolbox_av1_slots_t *race_slots;
static retro_atomic_ptr_t race_cookie;
static retro_atomic_int_t race_stop;

static void race_callback(void *user)
{
   (void)user;
   while (!retro_atomic_load_acquire_int(&race_stop))
   {
      void *cookie = retro_atomic_load_acquire_ptr(&race_cookie);
      if (cookie)
         videotoolbox_av1_slots_output(race_slots, cookie,
               (OSStatus)((uintptr_t)cookie >> 7), 0, NULL, kCMTimeInvalid,
               kCMTimeInvalid);
      sthread_yield();
   }
}

static void test_slot_races(void)
{
   sthread_t *one, *two;
   videotoolbox_av1_result_t result;
   unsigned iteration, retry;
   int status;
   void *cookie;
   race_slots = videotoolbox_av1_slots_new(1);
   assert(race_slots);
   retro_atomic_ptr_init(&race_cookie, NULL);
   retro_atomic_int_init(&race_stop, 0);
   one = sthread_create(race_callback, NULL);
   two = sthread_create(race_callback, NULL);
   assert(one && two);
   for (iteration = 0; iteration < 2000; iteration++)
   {
      cookie = videotoolbox_av1_slots_begin(race_slots, 0);
      assert(cookie);
      retro_atomic_store_release_ptr(&race_cookie, cookie);
      status = 0;
      for (retry = 0; retry < 1000000 && !status; retry++)
      {
         status = videotoolbox_av1_slots_take(race_slots, 0, &result);
         if (!status)
            sthread_yield();
      }
      assert(
            status == 1 && result.status == (OSStatus)((uintptr_t)cookie >> 7));
   }
   retro_atomic_store_release_int(&race_stop, 1);
   sthread_join(one);
   sthread_join(two);
   videotoolbox_av1_slots_free_quiesced(race_slots);
}
static void test_slots(void)
{
   videotoolbox_av1_slots_t *slots = videotoolbox_av1_slots_new(2);
   videotoolbox_av1_result_t result;
   CVPixelBufferRef image;
   CMTime pts = kCMTimeInvalid;
   void *first, *next;
   assert(slots && !videotoolbox_av1_slots_new(33));
   image = new_object(8);
   assert(image);
   pts.value = 99;
   pts.timescale = 24;
   pts.flags = 1;
   first = videotoolbox_av1_slots_begin(slots, 1);
   assert(first && !videotoolbox_av1_slots_begin(slots, 1));
   videotoolbox_av1_slots_output(slots, first, 0, 0, NULL, pts, pts);
   assert(!videotoolbox_av1_slots_take(slots, 1, &result));
   videotoolbox_av1_slots_output(slots, first, 0, 0, image, pts, pts);
   videotoolbox_av1_slots_output(slots, first, 0, 0, image, pts, pts);
   assert(videotoolbox_av1_slots_take(slots, 1, &result) == 1);
   assert(result.image == image && result.pts.value == 99
         && result.pts.timescale == 24);
   CVPixelBufferRelease(result.image);
   next = videotoolbox_av1_slots_begin(slots, 1);
   assert(next && next != first);
   videotoolbox_av1_slots_output(slots, first, -900, 0, image, pts, pts);
   assert(!videotoolbox_av1_slots_take(slots, 1, &result));
   videotoolbox_av1_slots_output(slots, next, -901, 0, image, pts, pts);
   assert(videotoolbox_av1_slots_take(slots, 1, &result) == 1);
   assert(!result.image && result.status == -901);
   next = videotoolbox_av1_slots_begin(slots, 0);
   videotoolbox_av1_slots_complete(slots, next, 0);
   assert(videotoolbox_av1_slots_take(slots, 0, &result) == 1 && !result.image);
   videotoolbox_av1_slots_output(slots, next, 0, 0, image, pts, pts);
   assert(!videotoolbox_av1_slots_take(slots, 0, &result));
   next = videotoolbox_av1_slots_begin(slots, 0);
   videotoolbox_av1_slots_output(slots, next, 0, 0, image, pts, pts);
   slots->closed = 1;
   assert(!videotoolbox_av1_slots_begin(slots, 1));
   slots->closed = 0;
   slots->serial = (uintptr_t)-1 >> 7;
   assert(!videotoolbox_av1_slots_begin(slots, 1));
   CVPixelBufferRelease(image);
   videotoolbox_av1_slots_free_quiesced(slots);
}
#ifndef OWNER_MAIN_NAME
#define OWNER_MAIN_NAME main
#endif
int OWNER_MAIN_NAME(void)
{
   videotoolbox_av1_owner_t *o = open_owner();
   videotoolbox_av1_result_t result;
   CVPixelBufferRef image;
   unsigned i;
   void *stale;
   uint8_t input[3] = {0x32, 1, 0x80};
   uint8_t big[40];
   for (i = 0; i < 10; i++)
      assert(!videotoolbox_av1_owner_append(
            o, input, sizeof(input), true, (int)i));
   input[2] = 0xff;
   assert(wakes == 10 && !owner_waits);
   stale = o->work[0].cookie;
   assert(videotoolbox_av1_owner_append(o, input, sizeof(input), true, 0) == 1);
   for (i = 0; i < 10; i++)
   {
      assert(o->work[i].packet.data[0] == 0x32);
      assert(videotoolbox_av1_owner_take(o, i, &result) == 1);
      assert(videotoolbox_av1_owner_worker(o) == 1);
      assert(!videotoolbox_av1_owner_take(o, i, &result));
      assert(result.image);
      CVPixelBufferRelease(result.image);
   }
   assert(!videotoolbox_av1_owner_worker(o));
   assert(!videotoolbox_av1_owner_finish(o, false));
   assert(!videotoolbox_av1_owner_restart(o));
   assert(!videotoolbox_av1_owner_append(o, input, sizeof(input), true, 0));
   videotoolbox_av1_owner_output(
         o, stale, -333, 0, NULL, kCMTimeInvalid, kCMTimeInvalid);
   assert(!retro_atomic_load_acquire_int(&o->broken));
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(!videotoolbox_av1_owner_take(o, 0, &result) && result.image);
   CVPixelBufferRelease(result.image);
   close_owner(&o);

   o = open_owner();
   assert(!videotoolbox_av1_owner_append(o, input, 3, false, -1));
   assert(videotoolbox_av1_owner_finish(o, false) == 1);
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(!videotoolbox_av1_owner_finish(o, false));
   assert(videotoolbox_av1_owner_append(o, input, 3, false, -1) < 0);
   close_owner(&o);

   o = open_owner();
   assert(!videotoolbox_av1_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(videotoolbox_av1_owner_finish(o, false) == 1);
   assert(!videotoolbox_av1_owner_take(o, 0, &result));
   image = result.image;
   assert(!videotoolbox_av1_owner_finish(o, false));
   close_owner(&o);
   assert(retro_atomic_load_acquire_int(&objects) == 1);
   CVPixelBufferRelease(image);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   for (i = 0; i < 9; i++)
      assert(!videotoolbox_av1_owner_append(o, input, 3, false, -1));
   assert(videotoolbox_av1_owner_finish(o, false) == 1);
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(!videotoolbox_av1_owner_finish(o, false));
   close_owner(&o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   memset(big, 3, sizeof(big));
   big[0] = 0x30;
   assert(!videotoolbox_av1_owner_append(o, big, sizeof(big), false, -1));
   assert(videotoolbox_av1_owner_append(o, big, sizeof(big), true, 0) == 1);
   assert(!videotoolbox_av1_owner_append(o, big, sizeof(big), true, 0));
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(!videotoolbox_av1_owner_finish(o, true));
   close_owner(&o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   assert(!videotoolbox_av1_owner_append(o, input, 3, true, 0));
   videotoolbox_av1_owner_close(o);
   assert(videotoolbox_av1_owner_append(o, input, 3, true, 0) < 0);
   assert(videotoolbox_av1_owner_finish(o, true) < 0);
   assert(videotoolbox_av1_owner_retire_poll(&o) && !o);
   assert(!retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   owner_delay = 1;
   assert(!videotoolbox_av1_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(videotoolbox_av1_owner_take(o, 0, &result) == 1);
   image = new_object(8);
   owner_delayed_session->callback.decompressionOutputCallback(
         o, owner_delayed_cookie, 0, 0, image, kCMTimeInvalid, kCMTimeInvalid);
   CVPixelBufferRelease(image);
   assert(!videotoolbox_av1_owner_take(o, 0, &result));
   CVPixelBufferRelease(result.image);
   assert(videotoolbox_av1_owner_finish(o, false) == 1);
   assert(videotoolbox_av1_owner_restart(o) == 1);
   videotoolbox_av1_owner_close(o);
   assert(!videotoolbox_av1_owner_retire_poll(&o) && o);
   CFRelease(delayed_sample);
   delayed_sample = NULL;
   owner_delay = 0;
   assert(videotoolbox_av1_owner_retire_poll(&o) && !o
         && !retro_atomic_load_acquire_int(&objects));

   o = open_owner();
   decode_error = -303;
   assert(!videotoolbox_av1_owner_append(o, input, 3, true, 0));
   assert(videotoolbox_av1_owner_worker(o) == 1);
   assert(videotoolbox_av1_owner_take(o, 0, &result) < 0);
   assert(videotoolbox_av1_owner_restart(o) < 0);
   assert(videotoolbox_av1_owner_append(o, input, 3, false, -1) < 0);
   close_owner(&o);
   decode_error = 0;
   test_owner_threads();
   test_slots();
   test_slot_races();
   puts("PASS: AV1 bounded owner queue, backpressure, final output, hidden "
        "EOS, native errors and delayed-block retirement");
   return 0;
}
#else
int main(void)
{
   return 0;
}
#endif
