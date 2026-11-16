/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_stream_d3d12.h"
#include "../../../record/record_stream_vulkan.h"
/* Existing packet fixtures supply a simulated encoder. One active stream at
 * a time, exactly as in contract_test.c; the proxy outlives its consumer. */
struct record_gpu_worker_vulkan
{
   record_gpu_worker_d3d12_t *source;
};
static record_gpu_worker_vulkan_t proxy;
int record_gpu_worker_vulkan_step(record_gpu_worker_vulkan_t *worker,
      vulkan_encode_packet_t *out)
{
   nvenc_d3d12_packet_t packet;
   int result = record_gpu_worker_d3d12_step(worker->source, &packet);
   switch (result)
   {
      case RECORD_GPU_WORKER_BUSY: return RECORD_GPU_VULKAN_BUSY;
      case RECORD_GPU_WORKER_PROGRESS: return RECORD_GPU_VULKAN_PROGRESS;
      case RECORD_GPU_WORKER_END: return RECORD_GPU_VULKAN_END;
      case RECORD_GPU_WORKER_PACKET:
         out->data = packet.data;
         out->size = packet.size;
         out->timestamp = packet.timestamp;
         out->duration = packet.duration;
         out->slot = packet.slot;
         out->keyframe = packet.keyframe;
         return RECORD_GPU_VULKAN_PACKET;
      default: return RECORD_GPU_VULKAN_ERROR;
   }
}
bool record_gpu_worker_vulkan_release(record_gpu_worker_vulkan_t *worker)
{
   return record_gpu_worker_d3d12_release(worker->source);
}
bool record_gpu_worker_vulkan_retry_needed(const record_gpu_worker_vulkan_t *worker)
{
   return record_gpu_worker_d3d12_retry_needed(worker->source);
}
static record_stream_gpu_t *vulkan_contract_new(record_gpu_worker_d3d12_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client)
{
   proxy.source = worker;
   return record_stream_vulkan_new(worker ? &proxy : NULL, config, client);
}
#define record_stream_d3d12_new vulkan_contract_new
#define record_stream_d3d12_step record_stream_gpu_step
#define record_stream_d3d12_audio record_stream_gpu_audio
#define record_stream_d3d12_audio_finish record_stream_gpu_audio_finish
#define record_stream_d3d12_abort record_stream_gpu_abort
#define record_stream_d3d12_error record_stream_gpu_error
#define record_stream_d3d12_free record_stream_gpu_free
#include "contract_test.c"
