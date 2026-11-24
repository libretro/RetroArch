/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main session_fixture_unused_main
#include "vulkan_session_test.c"
#undef main
#include "../../../record/record_gpu_dispatch_vulkan.h"
#ifdef HAVE_THREADS
#ifdef _WIN32
static DWORD dispatch_caller;
#else
static pthread_t dispatch_caller;
#endif
static retro_atomic_int_t dispatch_gate, dispatch_entered;
static retro_atomic_int_t capture_created, capture_destroyed, capture_producer_retired;
static bool capture_fail;
struct vulkan_encode_capture { const vulkan_encode_pool_t *pool; };
static struct vulkan_encode_capture capture_fixture;
static void dispatch_not_caller(void)
{
#ifdef _WIN32
   assert(GetCurrentThreadId() != dispatch_caller);
#else
   assert(!pthread_equal(pthread_self(), dispatch_caller));
#endif
}
static VKAPI_ATTR void VKAPI_CALL capture_barrier(VkCommandBuffer command,
      const VkDependencyInfo *info)
{ (void)command; (void)info; }
vulkan_encode_capture_t *vulkan_encode_capture_new_compiled(
      const vulkan_encode_pool_t *pool, const vulkan_encode_capture_ops_t *ops,
      char *error, size_t capacity)
{
   dispatch_not_caller();
   assert(pool && ops && ops->barrier == capture_barrier && !error && !capacity);
   if (capture_fail)
      return NULL;
   capture_fixture.pool = pool;
   retro_atomic_store_release_int(&capture_created, 1);
   return &capture_fixture;
}
void vulkan_encode_capture_free(vulkan_encode_capture_t *capture)
{
   if (!capture)
      return;
   dispatch_not_caller();
   assert(capture == &capture_fixture && capture->pool->count == 2);
   assert(retro_atomic_load_acquire_int(&capture_producer_retired));
   assert(!retro_atomic_load_acquire_int(&capture_destroyed));
   retro_atomic_store_release_int(&capture_destroyed, 1);
}
static void dispatch_pool_retire(void)
{
   assert(retro_atomic_load_acquire_int(&capture_created)
         == retro_atomic_load_acquire_int(&capture_destroyed));
}
static void dispatch_startup_observe(void)
{
   unsigned retry;
#ifdef _WIN32
   assert(GetCurrentThreadId() != dispatch_caller);
#else
   assert(!pthread_equal(pthread_self(), dispatch_caller));
#endif
   retro_atomic_store_release_int(&dispatch_entered, 1);
   for (retry = 0; !retro_atomic_load_acquire_int(&dispatch_gate); retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
}
static record_gpu_dispatch_vulkan_t *dispatch_create(bool audio, bool gate, bool capture)
{
   vulkan_encode_h264_open_t config;
   record_gpu_output_vulkan_config_t output;
   static const vulkan_encode_h264_ops_t ops = {{0},{0},{0}};
   vulkan_encode_capture_ops_t capture_ops;
   reset();
   memset(&config, 0, sizeof(config));
   memset(&output, 0, sizeof(output));
   config.slots = 2;
   config.session.video.width = config.session.video.height = 64;
   config.session.video.fps_num = 30000;
   config.session.video.fps_den = 1001;
   output.video.max_packet_size = 65536;
   if (audio)
   {
      output.audio_rate = 48000;
      output.audio_layout = 3;
      output.audio_kbps = 192;
      output.audio_fifo_frames = 1536;
   }
#ifdef _WIN32
   dispatch_caller = GetCurrentThreadId();
#else
   dispatch_caller = pthread_self();
#endif
   retro_atomic_int_init(&dispatch_gate, gate ? 0 : 1);
   retro_atomic_int_init(&dispatch_entered, 0);
   retro_atomic_int_init(&capture_created, 0);
   retro_atomic_int_init(&capture_destroyed, 0);
   retro_atomic_int_init(&capture_producer_retired, 0);
   capture_fail = false;
   session_open_observer = dispatch_startup_observe;
   session_free_observer = dispatch_pool_retire;
   if (capture)
   {
      memset(&capture_ops, 0, sizeof(capture_ops));
      capture_ops.barrier = capture_barrier;
      return record_gpu_dispatch_vulkan_new_capture(&config, &ops, &output,
            "unused", &capture_ops);
   }
   return record_gpu_dispatch_vulkan_new(&config, &ops, &output, "unused");
}
static record_gpu_dispatch_vulkan_t *dispatch_new(bool audio, bool gate)
{
   return dispatch_create(audio, gate, false);
}
#ifndef RETRO_EVENTCOUNT_FORCE_SCOND
static void dispatch_ready(record_gpu_dispatch_vulkan_t *job)
{
   unsigned retry;
   enum record_gpu_dispatch_vulkan_state state;
   for (retry = 0; retry < 10000; retry++)
   {
      state = record_gpu_dispatch_vulkan_status(job, NULL, NULL);
      assert(state == RECORD_VKD_STARTING || state == RECORD_VKD_READY);
      if (state == RECORD_VKD_READY)
         return;
      SESSION_YIELD();
   }
   assert(!"dispatcher startup did not finish");
}
static void dispatch_collect(record_gpu_dispatch_vulkan_t *job,
      enum record_gpu_session_vulkan_error expected, uint64_t expected_audio)
{
   uint64_t audio_frames = (uint64_t)-1;
   unsigned retry;
   enum record_gpu_dispatch_vulkan_state state = RECORD_VKD_STARTING;
   enum record_gpu_session_vulkan_error error;
   bool freed = false;
   for (retry = 0; retry < 10000; retry++)
   {
      state = record_gpu_dispatch_vulkan_status(job, &error, NULL);
      if (state == RECORD_VKD_DONE || state == RECORD_VKD_FAILED)
         break;
      SESSION_YIELD();
   }
   assert(retry < 10000 && error == expected);
   assert(state == (expected ? RECORD_VKD_FAILED : RECORD_VKD_DONE));
   assert(record_gpu_dispatch_vulkan_audio_frames(job, &audio_frames));
   if (expected_audio != (uint64_t)-1)
      assert(audio_frames == expected_audio);
   for (retry = 0; retry < 10000; retry++)
   {
      if (record_gpu_dispatch_vulkan_free(job))
      {
         freed = true;
         break;
      }
      SESSION_YIELD();
   }
   assert(freed && opens == frees);
}
struct dispatch_audio
{
   record_gpu_dispatch_vulkan_t *job;
   unsigned made;
};
static SESSION_THREAD_RESULT dispatch_audio_produce(void *opaque)
{
   struct dispatch_audio *p = (struct dispatch_audio*)opaque;
   int16_t *pcm = (int16_t*)calloc(2048, sizeof(*pcm));
   unsigned idle = 0;
   assert(pcm);
   while (p->made < 40003 && idle < 10000)
   {
      size_t frames = 40003 - p->made;
      int result;
      if (frames > 1024)
         frames = 1024;
      result = record_gpu_dispatch_vulkan_audio(p->job, pcm, frames);
      if (result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
      if (result == RECORD_VULKAN_OUTPUT_BUSY)
      {
         idle++;
         SESSION_YIELD();
      }
      else
      {
         p->made += (unsigned)frames;
         idle = 0;
      }
   }
   free(pcm);
   assert(record_gpu_dispatch_vulkan_audio_finish(p->job));
   return 0;
}
static void dispatch_run(bool cancel)
{
   record_gpu_dispatch_vulkan_t *job = dispatch_new(true, false);
   struct dispatch_audio audio;
   record_gpu_frame_t frame;
   unsigned made = 0, idle = 0;
#ifdef _WIN32
   HANDLE thread;
#else
   pthread_t thread;
#endif
   assert(job && !record_gpu_dispatch_vulkan_free(job));
   {
      uint64_t count = 123;
      assert(!record_gpu_dispatch_vulkan_audio_frames(job, &count) && count == 123);
   }
   dispatch_ready(job);
   assert(record_gpu_dispatch_vulkan_pool(job)->count == 2);
   audio.job = job;
   audio.made = 0;
#ifdef _WIN32
   thread = CreateThread(NULL, 0, dispatch_audio_produce, &audio, 0, NULL);
   assert(thread);
#else
   assert(!pthread_create(&thread, NULL, dispatch_audio_produce, &audio));
#endif
   while (made < 64 && idle < 10000)
   {
      int result = record_gpu_dispatch_vulkan_acquire(job, &frame.slot);
      assert(result != RECORD_GPU_QUEUE_ERROR);
      if (result == RECORD_GPU_QUEUE_BUSY)
      {
         idle++;
         SESSION_YIELD();
         continue;
      }
      frame.timestamp = made * 1001;
      frame.duration = 1001;
      frame.ready_value = made + 1;
      frame.keyframe = !made;
      assert(record_gpu_dispatch_vulkan_submit(job, &frame));
      made++;
      idle = 0;
      if (cancel && made == 10)
      {
         record_gpu_dispatch_vulkan_cancel(job);
         break;
      }
   }
   assert(made == (cancel ? 10u : 64u));
   assert(record_gpu_dispatch_vulkan_video_finish(job));
#ifdef _WIN32
   assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0 && CloseHandle(thread));
#else
   assert(!pthread_join(thread, NULL));
#endif
   if (!cancel)
      assert(audio.made == 40003);
   dispatch_collect(job, cancel ? RECORD_VULKAN_SESSION_CANCELLED : RECORD_VULKAN_SESSION_NONE, cancel ? (uint64_t)-1 : 40003);
   if (!cancel)
      assert(submits == 64 && releases == 64);
}
/* A frame whose GPU work stays in flight: the worker blocks in the
 * encoder's wait until it completes, rather than polling the encoder */
static void dispatch_gpu_wait(void)
{
   record_gpu_dispatch_vulkan_t *job = dispatch_new(false, false);
   record_gpu_frame_t frame;
   unsigned retry;
   int polls;
   assert(job);
   dispatch_ready(job);
   retro_atomic_store_release_int(&gpu_busy, 1);
   assert(record_gpu_dispatch_vulkan_acquire(job, &frame.slot) == RECORD_GPU_QUEUE_OK);
   frame.timestamp = 0;
   frame.duration = 1001;
   frame.ready_value = 1;
   frame.keyframe = true;
   assert(record_gpu_dispatch_vulkan_submit(job, &frame));
   for (retry = 0; !retro_atomic_load_acquire_int(&gpu_waits); retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
   polls = retro_atomic_load_acquire_int(&gpu_polls);
   for (retry = 0; retry < 50; retry++)
      SESSION_YIELD();
   assert(retro_atomic_load_acquire_int(&gpu_polls) - polls <= 1);
   retro_atomic_store_release_int(&gpu_busy, 0);
   assert(record_gpu_dispatch_vulkan_video_finish(job));
   dispatch_collect(job, RECORD_VULKAN_SESSION_NONE, 0);
   assert(submits == 1 && releases == 1);
}
static void dispatch_startup_cancel(bool capture)
{
   record_gpu_dispatch_vulkan_t *job = dispatch_create(true, true, capture);
   unsigned retry, slot;
   assert(job);
   for (retry = 0; !retro_atomic_load_acquire_int(&dispatch_entered); retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
   assert(record_gpu_dispatch_vulkan_acquire(job, &slot) == RECORD_GPU_QUEUE_BUSY);
   assert(!record_gpu_dispatch_vulkan_pool(job));
   assert(!record_gpu_dispatch_vulkan_free(job));
   record_gpu_dispatch_vulkan_cancel(job);
   assert(record_gpu_dispatch_vulkan_video_finish(job));
   assert(record_gpu_dispatch_vulkan_audio_finish(job));
   retro_atomic_store_release_int(&dispatch_gate, 1);
   dispatch_collect(job, RECORD_VULKAN_SESSION_CANCELLED, (uint64_t)-1);
   assert(!retro_atomic_load_acquire_int(&capture_created));
}
static void dispatch_failure(bool startup)
{
   record_gpu_dispatch_vulkan_t *job = dispatch_new(true, true);
   unsigned retry;
   int error;
   assert(job);
   for (retry = 0; !retro_atomic_load_acquire_int(&dispatch_entered); retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
   file_open_fail = startup;
   file_push_fail = !startup;
   retro_atomic_store_release_int(&dispatch_gate, 1);
   for (retry = 0; record_gpu_dispatch_vulkan_status(job, NULL, NULL) != RECORD_VKD_DRAINING; retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
   assert(!record_gpu_dispatch_vulkan_free(job));
   record_gpu_dispatch_vulkan_status(job, NULL, &error);
   assert(startup || error == 112);
   assert(record_gpu_dispatch_vulkan_audio_finish(job));
   assert(record_gpu_dispatch_vulkan_video_finish(job));
   dispatch_collect(job, startup ? RECORD_VULKAN_SESSION_STARTUP : RECORD_VULKAN_SESSION_OUTPUT, (uint64_t)-1);
}
static void dispatch_capture_lifetime(bool cancel, bool fail)
{
   record_gpu_dispatch_vulkan_t *job = dispatch_create(true, true, true);
   unsigned retry;
   assert(job && !record_gpu_dispatch_vulkan_capture(job));
   for (retry = 0; !retro_atomic_load_acquire_int(&dispatch_entered); retry++)
   {
      assert(retry < 10000);
      SESSION_YIELD();
   }
   capture_fail = fail;
   retro_atomic_store_release_int(&dispatch_gate, 1);
   if (fail)
   {
      for (retry = 0; record_gpu_dispatch_vulkan_status(job, NULL, NULL) != RECORD_VKD_DRAINING; retry++)
      {
         assert(retry < 10000);
         SESSION_YIELD();
      }
      assert(!record_gpu_dispatch_vulkan_capture(job));
   }
   else
   {
      dispatch_ready(job);
      assert(record_gpu_dispatch_vulkan_capture(job) == &capture_fixture);
   }
   if (cancel)
   {
      record_gpu_dispatch_vulkan_cancel(job);
      assert(!record_gpu_dispatch_vulkan_capture(job));
   }
   assert(!retro_atomic_load_acquire_int(&capture_destroyed));
   retro_atomic_store_release_int(&capture_producer_retired, 1);
   assert(record_gpu_dispatch_vulkan_video_finish(job));
   if (!fail)
      for (retry = 0; !retro_atomic_load_acquire_int(&capture_destroyed); retry++)
      {
         assert(retry < 10000);
         SESSION_YIELD();
      }
   assert(!record_gpu_dispatch_vulkan_capture(job));
   assert(!record_gpu_dispatch_vulkan_free(job));
   assert(record_gpu_dispatch_vulkan_audio_finish(job));
   dispatch_collect(job, fail ? RECORD_VULKAN_SESSION_STARTUP
         : cancel ? RECORD_VULKAN_SESSION_CANCELLED : RECORD_VULKAN_SESSION_NONE, 0);
}
#endif
int main(void)
{
#ifdef RETRO_EVENTCOUNT_FORCE_SCOND
   assert(!dispatch_new(false, false) && opens == frees);
   puts("Vulkan dispatcher: locking wake fallback refused before worker startup PASS");
#else
   dispatch_run(false);
   dispatch_run(true);
   dispatch_gpu_wait();
   dispatch_startup_cancel(false);
   dispatch_startup_cancel(true);
   dispatch_failure(true);
   dispatch_failure(false);
   dispatch_capture_lifetime(false, false);
   dispatch_capture_lifetime(true, false);
   dispatch_capture_lifetime(false, true);
   puts("Vulkan dispatcher: worker startup, two producers, cancellation and retirement PASS");
#endif
   return 0;
}
#else
int main(void)
{
   assert(!record_gpu_dispatch_vulkan_new(NULL, NULL, NULL, "unused"));
   assert(!record_gpu_dispatch_vulkan_pool(NULL));
   assert(!record_gpu_dispatch_vulkan_new_capture(NULL, NULL, NULL, "unused", NULL));
   assert(!record_gpu_dispatch_vulkan_capture(NULL));
   assert(record_gpu_dispatch_vulkan_free(NULL));
   puts("Vulkan dispatcher: threadless unsupported admission PASS");
   return 0;
}
#endif
