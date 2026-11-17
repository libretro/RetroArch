/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_d3d12.h"

static int stream_d3d12_step(void *worker, record_stream_packet_t *out)
{
   nvenc_d3d12_packet_t packet;
   int result = record_gpu_worker_d3d12_step((record_gpu_worker_d3d12_t*)worker, &packet);
   switch (result)
   {
      case RECORD_GPU_WORKER_BUSY: return RECORD_STREAM_WORKER_BUSY;
      case RECORD_GPU_WORKER_PROGRESS: return RECORD_STREAM_WORKER_PROGRESS;
      case RECORD_GPU_WORKER_END: return RECORD_STREAM_WORKER_END;
      case RECORD_GPU_WORKER_PACKET:
         out->data = packet.data;
         out->size = packet.size;
         out->timestamp = packet.timestamp;
         out->duration = packet.duration;
         out->keyframe = packet.keyframe;
         return RECORD_STREAM_WORKER_PACKET;
      default: return RECORD_STREAM_WORKER_ERROR;
   }
}
static bool stream_d3d12_release(void *worker)
{
   return record_gpu_worker_d3d12_release((record_gpu_worker_d3d12_t*)worker);
}
static bool stream_d3d12_retry(const void *worker)
{
   return record_gpu_worker_d3d12_retry_needed((const record_gpu_worker_d3d12_t*)worker);
}
record_stream_d3d12_t *record_stream_d3d12_new(record_gpu_worker_d3d12_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client)
{
   static const record_stream_worker_ops_t ops = {
      stream_d3d12_step, stream_d3d12_release, stream_d3d12_retry
   };
   return record_stream_gpu_new(worker, &ops, config, client);
}

int record_stream_d3d12_audio(record_stream_d3d12_t *stream, const int16_t *samples, size_t frames)
{
   return record_stream_gpu_audio(stream, samples, frames);
}

bool record_stream_d3d12_audio_finish(record_stream_d3d12_t *stream)
{
   return record_stream_gpu_audio_finish(stream);
}

int record_stream_d3d12_step(record_stream_d3d12_t *stream, uint32_t receive_time, record_stream_wait_t *wait)
{
   return record_stream_gpu_step(stream, receive_time, wait);
}

void record_stream_d3d12_abort(record_stream_d3d12_t *stream)
{
   record_stream_gpu_abort(stream);
}

enum record_gpu_output_error record_stream_d3d12_error(const record_stream_d3d12_t *stream)
{
   return record_stream_gpu_error(stream);
}

bool record_stream_d3d12_free(record_stream_d3d12_t *stream)
{
   return record_stream_gpu_free(stream);
}
