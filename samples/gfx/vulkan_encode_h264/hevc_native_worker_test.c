/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_ADAPTER_HELPERS_ONLY
#include "hevc_native_adapter_test.c"
#include "../../../record/record_gpu_worker_vulkan_hevc.h"

static void native_worker_publish(struct native_hevc_adapter *a,
      const vulkan_encode_pool_t *pool, record_gpu_queue_t *queue, unsigned frame)
{
   record_gpu_frame_t descriptor;
   assert(record_gpu_queue_acquire(queue, &descriptor.slot) == RECORD_GPU_QUEUE_OK);
   assert(descriptor.slot == frame % 2);
   upload_frame(&a->gpu, pool, frame);
   descriptor.timestamp = frame;
   descriptor.duration = 1;
   descriptor.ready_value = frame + 1;
   descriptor.keyframe = frame == 7;
   assert(record_gpu_queue_submit(queue, &descriptor));
}

int main(int argc, char **argv)
{
   struct native_hevc_adapter *a = (struct native_hevc_adapter*)calloc(1, sizeof(*a));
   struct native_encoder *c;
   vulkan_encode_hevc_t *encoder;
   const vulkan_encode_pool_t *pool;
   record_gpu_queue_t *queue;
   record_gpu_worker_vulkan_t *worker;
   vulkan_encode_packet_t packet, again;
   PFN_vkWaitForFences wait;
   unsigned char *headers = (unsigned char*)malloc(65536);
   unsigned frame, slot;
   size_t size;
   FILE *file;
   bool cancel;
   int result = 77;
   assert(a && headers && (argc == 3 || argc == 4));
   c = &a->gpu.base;
   cancel = argc == 4;
   encoder = native_adapter_new(a, (unsigned)atoi(argv[1]));
   if (!encoder)
   {
      puts("UNAVAILABLE native HEVC encode device or validation layer");
      goto done;
   }
   pool = vulkan_encode_hevc_pool(encoder);
   queue = record_gpu_queue_new(2);
   worker = record_gpu_worker_vulkan_new_hevc(queue, encoder);
   assert(pool && queue && worker);
   wait = (PFN_vkWaitForFences)c->device_proc(c->device, "vkWaitForFences");
   assert(wait);
   file = fopen(argv[2], "wb");
   assert(file && vulkan_encode_hevc_headers(encoder, headers, 65536, &size));
   assert(fwrite(headers, 1, size, file) == size);
   for (frame = 0; frame < 24; frame++)
   {
      native_worker_publish(a, pool, queue, frame);
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_PROGRESS);
      /* Native fixture only; service itself contains no wait. */
      assert(wait(c->device, 1, &pool->slot[frame % 2].fence,
               VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_PACKET);
      assert(packet.timestamp == frame && packet.duration == 1 && packet.slot == frame % 2);
      assert(record_gpu_worker_vulkan_step(worker, &again) == RECORD_GPU_VULKAN_PACKET);
      assert(packet.data == again.data && packet.size == again.size && packet.size);
      assert(!record_gpu_worker_vulkan_free(worker) && !vulkan_encode_hevc_free(encoder));
      assert(fwrite(packet.data, 1, packet.size, file) == packet.size);
      assert(record_gpu_worker_vulkan_release(worker));
   }
   if (cancel)
   {
      native_worker_publish(a, pool, queue, 24);
      native_worker_publish(a, pool, queue, 25);
      assert(record_gpu_queue_acquire(queue, &slot) == RECORD_GPU_QUEUE_BUSY);
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_PROGRESS);
      assert(wait(c->device, 1, &pool->slot[0].fence, VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_PACKET);
      assert(record_gpu_queue_finish(queue));
      assert(!vulkan_encode_hevc_drain(encoder) && !vulkan_encode_hevc_free(encoder));
      assert(record_gpu_worker_vulkan_release(worker));
      assert(vulkan_encode_hevc_drain(encoder) && vulkan_encode_hevc_free(encoder));
      assert(record_gpu_worker_vulkan_abandon(worker));
   }
   else
   {
      assert(record_gpu_queue_finish(queue));
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_PROGRESS);
      assert(record_gpu_worker_vulkan_step(worker, &packet) == RECORD_GPU_VULKAN_END);
      assert(record_gpu_worker_vulkan_free(worker) && vulkan_encode_hevc_free(encoder));
   }
   assert(record_gpu_queue_free(queue) && fclose(file) == 0);
   printf("device=%s depth=%u worker_pictures=24 cancel=%u validation_errors=%u\n",
         c->properties.deviceName, a->config.session.video.bit_depth,
         cancel ? 1u : 0u, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   free(headers);
   native_adapter_context_free(a);
   return result;
}
