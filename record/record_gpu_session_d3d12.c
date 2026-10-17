/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_session_d3d12.h"
#include <retro_atomic.h>
#include <stdlib.h>

enum record_gpu_session_stage
{
   RGS_RUNNING, RGS_OUTPUT, RGS_VIDEO, RGS_ENCODER, RGS_ABANDON, RGS_RETIRED
};
struct record_gpu_session_d3d12
{
   record_gpu_capture_d3d12_t *capture;
   nvenc_d3d12_t *encoder;
   record_gpu_worker_d3d12_t *worker;
   record_gpu_output_d3d12_t *output;
   retro_atomic_int_t video_closed, audio_closed, cancelled, error, file_error;
   enum record_gpu_session_stage stage;
};

static void record_gpu_session_fail(record_gpu_session_d3d12_t *s,
      enum record_gpu_session_error error)
{
   if (!retro_atomic_load_acquire_int(&s->error))
      retro_atomic_store_release_int(&s->error, (int)error);
}

record_gpu_session_d3d12_t *record_gpu_session_d3d12_new(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path)
{
   record_gpu_session_d3d12_t *s;
   nvenc_d3d12_config_t config;
   record_gpu_output_config_t mux;
   void *headers;
   size_t size = 0;
   if (!capture || !encoder || !output || !path || !*path
         || output->video.headers || output->video.headers_size
         || output->video.ac3_header || output->video.ac3_header_size
         || encoder->codec < NVENC_D3D12_H264 || encoder->codec > NVENC_D3D12_AV1)
      return NULL;
   config = *encoder;
   if (!record_gpu_capture_d3d12_bind(capture, &config))
      return NULL;
   s = (record_gpu_session_d3d12_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->capture = capture;
   retro_atomic_int_init(&s->video_closed, 0);
   retro_atomic_int_init(&s->audio_closed, output->audio_rate ? 0 : 1);
   retro_atomic_int_init(&s->cancelled, 0);
   retro_atomic_int_init(&s->error, 0);
   retro_atomic_int_init(&s->file_error, 0);
   s->stage = RGS_OUTPUT;
   headers = malloc(65536);
   if (headers)
   {
      s->encoder = nvenc_d3d12_open(&config);
      if (s->encoder && nvenc_d3d12_headers(s->encoder, headers, 65536, &size))
      {
         s->worker = record_gpu_worker_d3d12_new(
               record_gpu_capture_d3d12_queue(capture), s->encoder,
               record_gpu_capture_d3d12_fence(capture));
         mux = *output;
         mux.video.headers = headers;
         mux.video.headers_size = size;
         mux.video.width = config.width;
         mux.video.height = config.height;
         mux.video.bit_depth = config.format == NVENC_D3D12_P010 ? 10 : 8;
         switch (config.codec)
         {
            case NVENC_D3D12_H264: mux.video.codec = RECORD_MP4_H264; break;
            case NVENC_D3D12_HEVC: mux.video.codec = RECORD_MP4_HEVC; break;
            default: mux.video.codec = RECORD_MP4_AV1; break;
         }
         if (s->worker)
            s->output = record_gpu_output_d3d12_new(s->worker, &mux, path);
      }
      free(headers);
   }
   if (s->output)
      s->stage = RGS_RUNNING;
   else
      record_gpu_session_fail(s, RECORD_GPU_SESSION_ERROR_STARTUP);
   return s;
}

int record_gpu_session_d3d12_audio(record_gpu_session_d3d12_t *s,
      const int16_t *samples, size_t frames)
{
   if (!s || retro_atomic_load_acquire_int(&s->audio_closed)
         || retro_atomic_load_acquire_int(&s->cancelled)
         || retro_atomic_load_acquire_int(&s->error))
      return RECORD_GPU_OUTPUT_ERROR;
   return record_gpu_output_d3d12_audio(s->output, samples, frames);
}

bool record_gpu_session_d3d12_video_finish(record_gpu_session_d3d12_t *s)
{
   if (!s) return false;
   if (retro_atomic_load_acquire_int(&s->video_closed)) return true;
   if (!record_gpu_capture_d3d12_finish(s->capture)) return false;
   retro_atomic_store_release_int(&s->video_closed, 1);
   return true;
}

bool record_gpu_session_d3d12_audio_finish(record_gpu_session_d3d12_t *s)
{
   if (!s) return false;
   retro_atomic_store_release_int(&s->audio_closed, 1);
   return true;
}

void record_gpu_session_d3d12_cancel(record_gpu_session_d3d12_t *s)
{
   if (s) retro_atomic_store_release_int(&s->cancelled, 1);
}

int record_gpu_session_d3d12_step(record_gpu_session_d3d12_t *s)
{
   nvenc_d3d12_packet_t packet;
   int result;
   bool closed;
   if (!s) return RECORD_GPU_OUTPUT_ERROR;
   if (s->stage == RGS_RETIRED)
      return retro_atomic_load_acquire_int(&s->error)
         ? RECORD_GPU_OUTPUT_ERROR : RECORD_GPU_OUTPUT_END;
   closed = retro_atomic_load_acquire_int(&s->audio_closed)
      && retro_atomic_load_acquire_int(&s->video_closed);
   if (s->output && retro_atomic_load_acquire_int(&s->audio_closed))
      record_gpu_output_d3d12_audio_finish(s->output);
   if (retro_atomic_load_acquire_int(&s->cancelled))
   {
      record_gpu_session_fail(s, RECORD_GPU_SESSION_ERROR_CANCELLED);
      if (s->stage == RGS_RUNNING) s->stage = RGS_OUTPUT;
   }
   switch (s->stage)
   {
      case RGS_RUNNING:
         result = record_gpu_output_d3d12_step(s->output);
         if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR)
         {
            if (result == RECORD_GPU_OUTPUT_ERROR)
               record_gpu_session_fail(s, RECORD_GPU_SESSION_ERROR_OUTPUT);
            s->stage = RGS_OUTPUT;
            return RECORD_GPU_OUTPUT_PROGRESS;
         }
         return result;
      case RGS_OUTPUT:
         if (s->output)
         {
            if (retro_atomic_load_acquire_int(&s->error))
               record_gpu_output_d3d12_abort(s->output);
            result = record_gpu_output_d3d12_step(s->output);
            if (result == RECORD_GPU_OUTPUT_BUSY)
               return RECORD_GPU_OUTPUT_BUSY;
            retro_atomic_store_release_int(&s->file_error,
                  (int)record_gpu_output_d3d12_file_error(s->output));
            if (!closed || !record_gpu_output_d3d12_free(s->output))
               return RECORD_GPU_OUTPUT_BUSY;
            s->output = NULL;
         }
         if (!closed) return RECORD_GPU_OUTPUT_BUSY;
         s->stage = RGS_VIDEO;
         return RECORD_GPU_OUTPUT_PROGRESS;
      case RGS_VIDEO:
         if (s->worker)
         {
            result = record_gpu_worker_d3d12_step(s->worker, &packet);
            if (result == RECORD_GPU_WORKER_PACKET)
            {
               if (record_gpu_worker_d3d12_release(s->worker))
                  return RECORD_GPU_OUTPUT_PROGRESS;
               result = RECORD_GPU_WORKER_ERROR;
            }
            if (result == RECORD_GPU_WORKER_ERROR)
            {
               record_gpu_session_fail(s, RECORD_GPU_SESSION_ERROR_VIDEO);
               if (!nvenc_d3d12_free(s->encoder)) return RECORD_GPU_OUTPUT_BUSY;
               s->encoder = NULL;
               s->stage = RGS_ABANDON;
               return RECORD_GPU_OUTPUT_PROGRESS;
            }
            if (result != RECORD_GPU_WORKER_END)
               return result == RECORD_GPU_WORKER_PROGRESS
                  ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
            if (!record_gpu_worker_d3d12_free(s->worker))
               return RECORD_GPU_OUTPUT_BUSY;
            s->worker = NULL;
         }
         s->stage = RGS_ENCODER;
         return RECORD_GPU_OUTPUT_PROGRESS;
      case RGS_ENCODER:
         if (!nvenc_d3d12_free(s->encoder)) return RECORD_GPU_OUTPUT_BUSY;
         s->encoder = NULL;
         s->stage = RGS_RETIRED;
         return retro_atomic_load_acquire_int(&s->error)
            ? RECORD_GPU_OUTPUT_ERROR : RECORD_GPU_OUTPUT_END;
      case RGS_ABANDON:
         if (!record_gpu_worker_d3d12_abandon(s->worker))
            return RECORD_GPU_OUTPUT_BUSY;
         s->worker = NULL;
         s->stage = RGS_RETIRED;
         return RECORD_GPU_OUTPUT_ERROR;
      default: break;
   }
   return RECORD_GPU_OUTPUT_ERROR;
}

void *record_gpu_session_d3d12_wait_handle(const record_gpu_session_d3d12_t *s)
{
   if (!s) return NULL;
   if (s->output) return record_gpu_output_d3d12_wait_handle(s->output);
   if (s->stage == RGS_VIDEO)
      return record_gpu_worker_d3d12_wait_handle(s->worker);
   return NULL;
}

bool record_gpu_session_d3d12_retry_needed(const record_gpu_session_d3d12_t *s)
{
   if (!s || s->stage == RGS_RETIRED) return false;
   if (s->stage == RGS_RUNNING)
      return record_gpu_output_d3d12_retry_needed(s->output);
   if (s->output && record_gpu_output_d3d12_retry_needed(s->output)) return true;
   return retro_atomic_load_acquire_int(&s->audio_closed)
      && retro_atomic_load_acquire_int(&s->video_closed);
}

enum record_gpu_session_error record_gpu_session_d3d12_error(
      const record_gpu_session_d3d12_t *s, uint32_t *file_error)
{
   if (file_error)
      *file_error = s ? (uint32_t)retro_atomic_load_acquire_int(&s->file_error) : 0;
   return s ? (enum record_gpu_session_error)retro_atomic_load_acquire_int(&s->error)
      : RECORD_GPU_SESSION_ERROR_STARTUP;
}

bool record_gpu_session_d3d12_free(record_gpu_session_d3d12_t *s)
{
   if (!s) return true;
   if (s->stage != RGS_RETIRED) return false;
   free(s);
   return true;
}
