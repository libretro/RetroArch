/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_CAPTURE_HELPERS_ONLY
#include "hevc_native_capture_test.c"
#include "../../../record/record_gpu_dispatch_vulkan_hevc.h"
#ifdef _WIN32
#include <windows.h>
#define NATIVE_DISPATCH_YIELD() Sleep(1)
#else
#include <time.h>
static void native_dispatch_yield(void)
{
   struct timespec pause;
   pause.tv_sec = 0;
   pause.tv_nsec = 1000000;
   nanosleep(&pause, NULL);
}
#define NATIVE_DISPATCH_YIELD() native_dispatch_yield()
#endif
struct native_hevc_recording
{
   struct native_hevc_capture capture;
   int16_t pcm[3200];
};
int main(int argc, char **argv)
{
   struct native_hevc_recording *n = (struct native_hevc_recording*)calloc(1, sizeof(*n));
   struct native_hevc_capture *c;
   struct native_encoder *gpu;
   record_gpu_dispatch_vulkan_t *job;
   VkFence produced[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
   VkFenceCreateInfo fence;
   enum record_gpu_dispatch_vulkan_state state;
   record_gpu_output_vulkan_config_t output;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_hevc_t *probe;
   record_gpu_frame_t frame;
   PFN_vkWaitForFences wait;
   unsigned depth, index, i, slot;
   bool direct, audio, cancel;
   int result = 77;
   assert(n && (argc == 5 || (argc == 6 && !strcmp(argv[5], "--cancel"))));
   (void)upload_frame;
   (void)capture_new;
   (void)capture_free;
   c = &n->capture;
   gpu = &c->adapter.gpu.base;
   depth = (unsigned)atoi(argv[1]);
   direct = atoi(argv[3]) != 0;
   audio = atoi(argv[4]) != 0;
   cancel = argc == 6;
   probe = native_adapter_new_mode(&c->adapter, depth, true, direct, false);
   if (!probe)
   {
      puts("UNAVAILABLE native HEVC capture/session device");
      goto done;
   }
   if (direct && !vulkan_encode_hevc_pool(probe)->storage_images)
   {
      assert(vulkan_encode_hevc_free(probe));
      puts("UNAVAILABLE direct HEVC storage planes");
      goto done;
   }
   assert(vulkan_encode_hevc_free(probe));
   memset(&output, 0, sizeof(output));
   output.video.max_packet_size = 1048576;
   output.audio_follow_video = true;
   if (audio)
   {
      output.audio_rate = 48000;
      output.audio_layout = 3;
      output.audio_kbps = 192;
      output.audio_fifo_frames = 4096;
   }
   load_capture(c);
   c->adapter.config.session.video.ctb_size = 0;
   job = record_gpu_dispatch_vulkan_new_hevc(&c->adapter.config,
         &c->adapter.ops, &output, argv[2], &c->capture_ops);
   assert(job);
   for (i = 0; i < 100000; i++)
   {
      state = record_gpu_dispatch_vulkan_status(job, NULL, NULL);
      if (state != RECORD_VKD_STARTING)
         break;
      NATIVE_DISPATCH_YIELD();
   }
   if (i == 100000 || state != RECORD_VKD_READY)
   {
      enum record_gpu_session_vulkan_error error;
      int file_error;
      record_gpu_dispatch_vulkan_status(job, &error, &file_error);
      fprintf(stderr, "dispatcher startup polls=%u state=%d error=%d file=%d\n",
            i, (int)state, (int)error, file_error);
   }
   assert(i < 100000 && state == RECORD_VKD_READY);
   pool = record_gpu_dispatch_vulkan_pool(job);
   assert(pool && pool->storage_images == direct);
   c->capture = record_gpu_dispatch_vulkan_capture(job);
   assert(c->capture);
   capture_new_source(c, pool);
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence.pNext = NULL;
   fence.flags = 0;
   for (i = 0; i < 2; i++)
      assert(c->adapter.ops.pool.create_fence(gpu->device, &fence, NULL,
               &produced[i]) == VK_SUCCESS);
   wait = (PFN_vkWaitForFences)gpu->device_proc(gpu->device, "vkWaitForFences");
   assert(wait);
   for (index = 0; index < 24; index++)
   {
      for (i = 0; i < 100000; i++)
      {
         if (record_gpu_dispatch_vulkan_acquire(job, &slot) == RECORD_GPU_QUEUE_OK)
            break;
         assert(record_gpu_dispatch_vulkan_status(job, NULL, NULL) == RECORD_VKD_READY);
         NATIVE_DISPATCH_YIELD();
      }
      assert(i < 100000 && slot == index % 2);
      assert(c->adapter.ops.submit.reset_fence(gpu->device, 1, &produced[slot]) == VK_SUCCESS);
      c->capture_command.fence = produced[slot];
      capture_frame(c, pool, index);
      frame.slot = slot;
      frame.timestamp = index;
      frame.duration = 1;
      frame.ready_value = index + 1;
      frame.keyframe = index == 7;
      assert(record_gpu_dispatch_vulkan_submit(job, &frame));
      if (audio)
      {
         for (i = 0; i < 3200; i++)
            n->pcm[i] = (int16_t)(((index * 1600 + i / 2) % 96 < 48 ? 6000 : -6000)
                  * (i % 2 ? -1 : 1));
         for (i = 0; i < 100000; i++)
         {
            result = record_gpu_dispatch_vulkan_audio(job, n->pcm, 1600);
            if (result == RECORD_VULKAN_OUTPUT_PROGRESS)
               break;
            assert(result == RECORD_VULKAN_OUTPUT_BUSY);
            assert(record_gpu_dispatch_vulkan_status(job, NULL, NULL) == RECORD_VKD_READY);
         NATIVE_DISPATCH_YIELD();
         }
         assert(i < 100000);
      }
      if (cancel && index == 11)
      {
         record_gpu_dispatch_vulkan_cancel(job);
         break;
      }
   }
   /* Producer completion only; the worker owns encode/output retirement. */
   assert(wait(gpu->device, 2, produced, VK_TRUE, 2000000000) == VK_SUCCESS);
   capture_free_source(c);
   for (i = 0; i < 2; i++)
      c->adapter.ops.pool.destroy_fence(gpu->device, produced[i], NULL);
   assert(record_gpu_dispatch_vulkan_video_finish(job));
   assert(record_gpu_dispatch_vulkan_audio_finish(job));
   for (i = 0; i < 100000; i++)
   {
      state = record_gpu_dispatch_vulkan_status(job, NULL, NULL);
      if (state == RECORD_VKD_DONE || state == RECORD_VKD_FAILED)
         break;
      NATIVE_DISPATCH_YIELD();
   }
   assert(i < 100000 && state == (cancel ? RECORD_VKD_FAILED : RECORD_VKD_DONE));
   if (cancel)
   {
      enum record_gpu_session_vulkan_error error;
      record_gpu_dispatch_vulkan_status(job, &error, NULL);
      assert(error == RECORD_VULKAN_SESSION_CANCELLED);
   }
   {
      uint64_t frames;
      assert(record_gpu_dispatch_vulkan_audio_frames(job, &frames));
      if (cancel)
         assert(frames <= (audio ? 19200u : 0u));
      else
         assert(frames == (audio ? 38400u : 0u));
   }
   for (i = 0; i < 100000; i++)
   {
      if (record_gpu_dispatch_vulkan_free(job))
         break;
      NATIVE_DISPATCH_YIELD();
   }
   assert(i < 100000);
   printf("device=%s HEVC dispatcher depth=%u direct=%u audio=%u cancel=%u pictures=%u validation_errors=%u\n",
         gpu->properties.deviceName, depth, direct ? 1u : 0u, audio ? 1u : 0u, cancel ? 1u : 0u, cancel ? 12u : 24u, gpu->validation_errors);
   result = gpu->validation_errors ? 1 : 0;
done:
   native_adapter_context_free(&c->adapter);
   return result;
}
