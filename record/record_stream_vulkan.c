/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_vulkan.h"

static int stream_vulkan_step(void *worker, record_stream_packet_t *out)
{
   vulkan_encode_packet_t packet;
   int result = record_gpu_worker_vulkan_step((record_gpu_worker_vulkan_t*)worker, &packet);
   switch (result)
   {
      case RECORD_GPU_VULKAN_BUSY: return RECORD_STREAM_WORKER_BUSY;
      case RECORD_GPU_VULKAN_PROGRESS: return RECORD_STREAM_WORKER_PROGRESS;
      case RECORD_GPU_VULKAN_END: return RECORD_STREAM_WORKER_END;
      case RECORD_GPU_VULKAN_PACKET:
         out->data = packet.data;
         out->size = packet.size;
         out->timestamp = packet.timestamp;
         out->duration = packet.duration;
         out->keyframe = packet.keyframe;
         return RECORD_STREAM_WORKER_PACKET;
      default: return RECORD_STREAM_WORKER_ERROR;
   }
}
static bool stream_vulkan_release(void *worker)
{
   return record_gpu_worker_vulkan_release((record_gpu_worker_vulkan_t*)worker);
}
static bool stream_vulkan_retry(const void *worker)
{
   return record_gpu_worker_vulkan_retry_needed((const record_gpu_worker_vulkan_t*)worker);
}
record_stream_gpu_t *record_stream_vulkan_new(record_gpu_worker_vulkan_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client)
{
   static const record_stream_worker_ops_t ops = {
      stream_vulkan_step, stream_vulkan_release, stream_vulkan_retry
   };
   return record_stream_gpu_new(worker, &ops, config, client);
}
