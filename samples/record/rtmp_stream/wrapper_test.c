/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define STREAM_CONTRACT_HELPERS_ONLY
#include "contract_test.c"
#include "../gpu_av_output/mock_file.h"

int main(void)
{
   struct net_rtmp_client client;
   record_gpu_worker_d3d12_t worker;
   record_gpu_output_config_t config;
   record_gpu_output_d3d12_t *output_stream;
   unsigned i;
   int result, fd;
   bool read, write, retry_seen = false;
   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i += 2)
   {
      pcm[i] = 1234;
      pcm[i + 1] = -5678;
   }
   make_config(&client, &worker, &config);
   config.stream_client = &client;
   worker.frames = 0;
   file_open_fail = true;
   output_stream = record_gpu_output_d3d12_new(&worker, &config, "must-not-open.mp4");
   assert(output_stream && !record_gpu_output_d3d12_free(output_stream));
   assert(!record_gpu_output_d3d12_network(output_stream, &fd, &read, &write));
   assert(fd == -1 && !read && !write);
   assert(record_gpu_output_d3d12_audio(output_stream, pcm, 1025) == RECORD_GPU_OUTPUT_PROGRESS);
   assert(record_gpu_output_d3d12_audio_finish(output_stream));
   submit_busy = 1;
   result = RECORD_GPU_OUTPUT_BUSY;
   for (i = 0; i < 10000 && result != RECORD_GPU_OUTPUT_END; i++)
   {
      result = record_gpu_output_d3d12_step(output_stream);
      assert(result != RECORD_GPU_OUTPUT_ERROR);
      if (result == RECORD_GPU_OUTPUT_BUSY)
         retry_seen = record_gpu_output_d3d12_retry_needed(output_stream) || retry_seen;
   }
   assert(result == RECORD_GPU_OUTPUT_END && submitted == 1025 && returned == 2 && retry_seen);
   assert(record_gpu_output_d3d12_file_error(output_stream) == 0);
   assert(record_gpu_output_d3d12_error(output_stream) == RECORD_GPU_OUTPUT_ERROR_NONE);
   assert(record_gpu_output_d3d12_free(output_stream));
   assert(net_rtmp_session_free(client.session));
   make_config(&client, &worker, &config);
   config.stream_client = &client;
   output_stream = record_gpu_output_d3d12_new(&worker, &config, "must-not-open.mp4");
   assert(output_stream && record_gpu_output_d3d12_abort(output_stream));
   assert(record_gpu_output_d3d12_error(output_stream) == RECORD_GPU_OUTPUT_ERROR_ABORTED);
   assert(record_gpu_output_d3d12_step(output_stream) == RECORD_GPU_OUTPUT_ERROR);
   assert(!record_gpu_output_d3d12_free(output_stream));
   assert(record_gpu_output_d3d12_audio_finish(output_stream));
   assert(record_gpu_output_d3d12_free(output_stream) && net_rtmp_session_free(client.session));
   puts("PASS recording output selects streaming, opens no file and preserves drain/retry ownership");
   return 0;
}
