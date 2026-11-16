/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define STREAM_CONTRACT_HELPERS_ONLY
#include "vulkan_contract_test.c"
#include "../gpu_av_output/vulkan_mock_file.h"
#include "../../../record/record_gpu_output_vulkan.h"

int main(void)
{
   struct net_rtmp_client client;
   record_gpu_worker_d3d12_t worker;
   record_gpu_output_config_t common;
   record_gpu_output_vulkan_config_t config;
   record_gpu_output_vulkan_t *output_stream;
   unsigned i;
   int result, fd;
   bool read, write, retry_seen = false;
   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i += 2)
   {
      pcm[i] = 1234;
      pcm[i + 1] = -5678;
   }
   make_config(&client, &worker, &common);
   memset(&config, 0, sizeof(config));
   config.video = common.video;
   config.audio_rate = common.audio_rate;
   config.audio_kbps = common.audio_kbps;
   config.audio_layout = common.audio_layout;
   config.audio_fifo_frames = common.audio_fifo_frames;
   proxy.source = &worker;
   config.stream_client = &client;
   worker.frames = 0;
   file_open_fail = true;
   output_stream = record_gpu_output_vulkan_new(&proxy, &config, "must-not-open.mp4");
   assert(output_stream && !record_gpu_output_vulkan_free(output_stream));
   assert(!record_gpu_output_vulkan_network(output_stream, &fd, &read, &write));
   assert(fd == -1 && !read && !write);
   client.descriptor = 23;
   assert(record_gpu_output_vulkan_audio(output_stream, pcm, 1025) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_output_vulkan_audio_finish(output_stream));
   assert(record_gpu_output_vulkan_step(output_stream) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_output_vulkan_network(output_stream, &fd, &read, &write));
   assert(fd == 23 && read && !write);
   submit_busy = 1;
   result = RECORD_VULKAN_OUTPUT_BUSY;
   for (i = 0; i < 10000 && result != RECORD_VULKAN_OUTPUT_END; i++)
   {
      result = record_gpu_output_vulkan_step(output_stream);
      assert(result != RECORD_VULKAN_OUTPUT_ERROR);
      if (result == RECORD_VULKAN_OUTPUT_BUSY)
         retry_seen = record_gpu_output_vulkan_retry_needed(output_stream) || retry_seen;
   }
   assert(result == RECORD_VULKAN_OUTPUT_END && submitted == 1025 && returned == 2 && retry_seen);
   assert(!record_gpu_output_vulkan_network(output_stream, &fd, &read, &write));
   assert(fd == -1 && !read && !write);
   assert(record_gpu_output_vulkan_audio_frames(output_stream) == 1025);
   assert(record_gpu_output_vulkan_file_error(output_stream) == 0);
   assert(record_gpu_output_vulkan_error(output_stream) == RECORD_VULKAN_OUTPUT_ERROR_NONE);
   assert(record_gpu_output_vulkan_free(output_stream));
   assert(net_rtmp_session_free(client.session));
   make_config(&client, &worker, &common);
   memset(&config, 0, sizeof(config));
   config.video = common.video;
   config.audio_rate = common.audio_rate;
   config.audio_kbps = common.audio_kbps;
   config.audio_layout = common.audio_layout;
   config.audio_fifo_frames = common.audio_fifo_frames;
   proxy.source = &worker;
   config.stream_client = &client;
   output_stream = record_gpu_output_vulkan_new(&proxy, &config, "must-not-open.mp4");
   assert(output_stream && record_gpu_output_vulkan_abort(output_stream));
   assert(record_gpu_output_vulkan_error(output_stream) == RECORD_VULKAN_OUTPUT_ERROR_ABORTED);
   assert(record_gpu_output_vulkan_step(output_stream) == RECORD_VULKAN_OUTPUT_ERROR);
   assert(!record_gpu_output_vulkan_network(output_stream, &fd, &read, &write));
   assert(fd == -1 && !read && !write);
   assert(!record_gpu_output_vulkan_free(output_stream));
   assert(record_gpu_output_vulkan_audio_finish(output_stream));
   assert(record_gpu_output_vulkan_free(output_stream) && net_rtmp_session_free(client.session));
   puts("PASS Vulkan output selects streaming, opens no file and preserves drain/retry ownership");
   return 0;
}
