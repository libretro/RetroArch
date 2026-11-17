/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#include "../../../record/record_stream_vulkan.c"
struct record_gpu_worker_vulkan
{
   vulkan_encode_packet_t packet;
   int result;
   bool released, retry;
};
struct record_stream_gpu
{
   int marker;
};
struct net_rtmp_client
{
   int marker;
};
static struct record_stream_gpu stream;
static record_stream_worker_ops_t dispatch;
static const record_gpu_output_config_t *received_config;
static net_rtmp_client_t *received_client;
record_stream_gpu_t *record_stream_gpu_new(void *worker,
      const record_stream_worker_ops_t *ops,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client)
{
   if (!worker) return NULL;
   assert(ops && ops->step && ops->release && ops->retry_needed);
   dispatch = *ops;
   received_config = config;
   received_client = client;
   return &stream;
}
int record_gpu_worker_vulkan_step(record_gpu_worker_vulkan_t *worker,
      vulkan_encode_packet_t *packet)
{
   *packet = worker->packet;
   return worker->result;
}
bool record_gpu_worker_vulkan_release(record_gpu_worker_vulkan_t *worker)
{
   return worker->released;
}
bool record_gpu_worker_vulkan_retry_needed(const record_gpu_worker_vulkan_t *worker)
{
   return worker->retry;
}
int main(void)
{
   static const int source[] = {RECORD_GPU_VULKAN_ERROR, RECORD_GPU_VULKAN_BUSY,
      RECORD_GPU_VULKAN_PROGRESS, RECORD_GPU_VULKAN_PACKET, RECORD_GPU_VULKAN_END, 99};
   static const int expected[] = {RECORD_STREAM_WORKER_ERROR, RECORD_STREAM_WORKER_BUSY,
      RECORD_STREAM_WORKER_PROGRESS, RECORD_STREAM_WORKER_PACKET, RECORD_STREAM_WORKER_END,
      RECORD_STREAM_WORKER_ERROR};
   static const unsigned char bytes[123] = {1, 2, 3};
   record_gpu_worker_vulkan_t worker;
   record_gpu_output_config_t config;
   net_rtmp_client_t client;
   record_stream_packet_t packet, untouched;
   unsigned i;
   memset(&worker, 0, sizeof(worker));
   memset(&config, 0, sizeof(config));
   assert(!record_stream_vulkan_new(NULL, &config, &client));
   assert(record_stream_vulkan_new(&worker, &config, &client) == &stream);
   assert(received_config == &config && received_client == &client);
   worker.packet.data = bytes;
   worker.packet.size = sizeof(bytes);
   worker.packet.timestamp = ((uint64_t)1 << 40) + 7;
   worker.packet.duration = ((uint64_t)1 << 36) + 3;
   worker.packet.keyframe = true;
   worker.packet.slot = 15;
   for (i = 0; i < sizeof(source) / sizeof(source[0]); i++)
   {
      worker.result = source[i];
      memset(&packet, 0xa5, sizeof(packet)); untouched = packet;
      assert(dispatch.step(&worker, &packet) == expected[i]);
      if (source[i] == RECORD_GPU_VULKAN_PACKET)
      {
         assert(packet.data == bytes && packet.size == sizeof(bytes));
         assert(packet.timestamp == worker.packet.timestamp);
         assert(packet.duration == worker.packet.duration && packet.keyframe);
      }
      else assert(!memcmp(&packet, &untouched, sizeof(packet)));
   }
   assert(!dispatch.release(&worker) && !dispatch.retry_needed(&worker));
   worker.released = worker.retry = true;
   assert(dispatch.release(&worker) && dispatch.retry_needed(&worker));
   return 0;
}
