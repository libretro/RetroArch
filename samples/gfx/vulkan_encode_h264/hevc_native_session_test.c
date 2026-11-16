/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_CAPTURE_HELPERS_ONLY
#include "hevc_native_capture_test.c"
#include "../../../record/record_gpu_session_vulkan_hevc.h"
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
   record_gpu_session_vulkan_t *session;
   record_gpu_output_vulkan_config_t output;
   const vulkan_encode_pool_t *pool;
   record_gpu_queue_t *queue;
   vulkan_encode_hevc_t *probe;
   record_gpu_frame_t frame;
   PFN_vkWaitForFences wait;
   unsigned depth, index, i, slot;
   bool direct, audio;
   int result = 77;
   assert(n && argc == 5);
   (void)upload_frame;
   c = &n->capture;
   gpu = &c->adapter.gpu.base;
   depth = (unsigned)atoi(argv[1]);
   direct = atoi(argv[3]) != 0;
   audio = atoi(argv[4]) != 0;
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
   session = record_gpu_session_vulkan_new_hevc(&c->adapter.config,
         &c->adapter.ops, &output, argv[2]);
   assert(session && record_gpu_session_vulkan_error(session, NULL) == RECORD_VULKAN_SESSION_NONE);
   pool = record_gpu_session_vulkan_pool(session);
   queue = record_gpu_session_vulkan_queue(session);
   assert(pool && queue && pool->storage_images == direct);
   load_capture(c);
   capture_new(c, pool);
   wait = (PFN_vkWaitForFences)gpu->device_proc(gpu->device, "vkWaitForFences");
   assert(wait);
   for (index = 0; index < 24; index++)
   {
      for (i = 0; i < 100000; i++)
      {
         if (record_gpu_queue_acquire(queue, &slot) == RECORD_GPU_QUEUE_OK)
            break;
         assert(record_gpu_session_vulkan_step(session) != RECORD_VULKAN_OUTPUT_ERROR);
      }
      assert(i < 100000 && slot == index % 2);
      capture_frame(c, pool, index);
      frame.slot = slot;
      frame.timestamp = index;
      frame.duration = 1;
      frame.ready_value = index + 1;
      frame.keyframe = index == 7;
      assert(record_gpu_queue_submit(queue, &frame));
      if (audio)
      {
         for (i = 0; i < 3200; i++)
            n->pcm[i] = (int16_t)(((index * 1600 + i / 2) % 96 < 48 ? 6000 : -6000)
                  * (i % 2 ? -1 : 1));
         for (i = 0; i < 100000; i++)
         {
            result = record_gpu_session_vulkan_audio(session, n->pcm, 1600);
            if (result == RECORD_VULKAN_OUTPUT_PROGRESS)
               break;
            assert(result == RECORD_VULKAN_OUTPUT_BUSY);
            assert(record_gpu_session_vulkan_step(session) != RECORD_VULKAN_OUTPUT_ERROR);
         }
         assert(i < 100000);
      }
      /* Fixture-only completion; production session servicing never waits. */
      for (i = 0; i < 100000; i++)
      {
         result = record_gpu_session_vulkan_step(session);
         assert(result != RECORD_VULKAN_OUTPUT_ERROR);
         if (result == RECORD_VULKAN_OUTPUT_BUSY
               && !record_gpu_session_vulkan_retry_needed(session))
            break;
      }
      assert(i < 100000);
      assert(wait(gpu->device, 1, &pool->slot[slot].fence,
               VK_TRUE, 2000000000) == VK_SUCCESS);
   }
   capture_free(c);
   assert(record_gpu_session_vulkan_video_finish(session));
   assert(record_gpu_session_vulkan_audio_finish(session));
   for (i = 0; i < 100000; i++)
   {
      result = record_gpu_session_vulkan_step(session);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
   }
   assert(i < 100000 && result == RECORD_VULKAN_OUTPUT_END);
   assert(record_gpu_session_vulkan_audio_frames(session) == (audio ? 38400u : 0u));
   assert(record_gpu_session_vulkan_free(session));
   printf("device=%s HEVC session depth=%u direct=%u audio=%u pictures=24 validation_errors=%u\n",
         gpu->properties.deviceName, depth, direct ? 1u : 0u, audio ? 1u : 0u, gpu->validation_errors);
   result = gpu->validation_errors ? 1 : 0;
done:
   native_adapter_context_free(&c->adapter);
   return result;
}
