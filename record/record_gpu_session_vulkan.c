/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_session_vulkan_backend.h"
#include <retro_atomic.h>
#include <stdlib.h>

enum record_vulkan_session_stage
{
   RVKS_RUNNING, RVKS_OUTPUT, RVKS_DRAIN, RVKS_ABANDON, RVKS_RETIRED
};
struct record_gpu_session_vulkan
{
   void *encoder;
   const record_gpu_session_vulkan_backend_t *backend;
   const vulkan_encode_pool_t *pool;
   record_gpu_queue_t *queue;
   record_gpu_worker_vulkan_t *worker;
   record_gpu_output_vulkan_t *output;
   uint64_t audio_frames;
   retro_atomic_int_t video_closed, audio_closed, cancelled, error, file_error;
   enum record_vulkan_session_stage stage;
};

static void record_vulkan_session_fail(record_gpu_session_vulkan_t *s,
      enum record_gpu_session_vulkan_error error)
{
   retro_atomic_cas_int(&s->error, 0, (int)error);
}

static bool record_vulkan_h264_drain(void *encoder)
{
   return vulkan_encode_h264_drain((vulkan_encode_h264_t*)encoder);
}

static bool record_vulkan_h264_free(void *encoder)
{
   return vulkan_encode_h264_free((vulkan_encode_h264_t*)encoder);
}

record_gpu_session_vulkan_t *record_gpu_session_vulkan_allocate(
      unsigned slots, const record_gpu_output_vulkan_config_t *output,
      const char *path)
{
   record_gpu_session_vulkan_t *s;
   if (!output || !path || !*path || !slots || slots > RECORD_GPU_QUEUE_SLOTS
         || output->video.headers || output->video.headers_size
         || output->video.ac3_header || output->video.ac3_header_size
         || output->video.timescale || output->video.color_primaries
         || output->video.transfer_characteristics
         || output->video.matrix_coefficients || output->video.full_range)
      return NULL;
   s = (record_gpu_session_vulkan_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->queue = record_gpu_queue_new(slots);
   if (!s->queue)
   {
      free(s);
      return NULL;
   }
   retro_atomic_int_init(&s->video_closed, 0);
   retro_atomic_int_init(&s->audio_closed, output->audio_rate ? 0 : 1);
   retro_atomic_int_init(&s->cancelled, 0);
   retro_atomic_int_init(&s->error, 0);
   retro_atomic_int_init(&s->file_error, 0);
   s->stage = RVKS_OUTPUT;
   return s;
}

void record_gpu_session_vulkan_start(record_gpu_session_vulkan_t *s,
      void *encoder, const record_gpu_session_vulkan_backend_t *backend,
      const vulkan_encode_pool_t *pool, record_gpu_worker_vulkan_t *worker,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   s->encoder = encoder;
   s->backend = backend;
   s->pool = pool;
   s->worker = worker;
   if (worker)
      s->output = record_gpu_output_vulkan_new(worker, output, path);
   if (s->output)
      s->stage = RVKS_RUNNING;
   else
      record_vulkan_session_fail(s, RECORD_VULKAN_SESSION_STARTUP);
}

record_gpu_session_vulkan_t *record_gpu_session_vulkan_new(
      const vulkan_encode_h264_open_t *config,
      const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   static const record_gpu_session_vulkan_backend_t backend = {
      record_vulkan_h264_drain, record_vulkan_h264_free
   };
   record_gpu_session_vulkan_t *s;
   vulkan_encode_h264_t *encoder = NULL;
   record_gpu_worker_vulkan_t *worker = NULL;
   record_gpu_output_vulkan_config_t mux;
   void *headers;
   size_t size = 0;
   if (!config || !ops)
      return NULL;
   s = record_gpu_session_vulkan_allocate(config->slots, output, path);
   if (!s)
      return NULL;
   mux = *output;
   headers = malloc(65536);
   if (headers)
   {
      encoder = vulkan_encode_h264_open(config, ops);
      if (encoder && vulkan_encode_h264_headers(encoder, headers, 65536, &size))
      {
         worker = record_gpu_worker_vulkan_new(s->queue, encoder);
         mux.video.headers = headers;
         mux.video.headers_size = size;
         mux.video.codec = RECORD_MP4_H264;
         mux.video.bit_depth = 8;
         mux.video.width = config->session.video.width;
         mux.video.height = config->session.video.height;
         mux.video.timescale = config->session.video.fps_num;
         mux.video.color_primaries = 1;
         mux.video.transfer_characteristics = 1;
         mux.video.matrix_coefficients = 1;
         mux.video.full_range = config->session.video.full_range;
      }
   }
   record_gpu_session_vulkan_start(s, encoder, &backend,
         encoder ? vulkan_encode_h264_pool(encoder) : NULL,
         worker, &mux, path);
   free(headers);
   return s;
}

const vulkan_encode_pool_t *record_gpu_session_vulkan_pool(
      const record_gpu_session_vulkan_t *s)
{
   return s && !retro_atomic_load_acquire_int(&s->video_closed)
      && !retro_atomic_load_acquire_int(&s->cancelled)
      && !retro_atomic_load_acquire_int(&s->error)
      ? s->pool : NULL;
}

record_gpu_queue_t *record_gpu_session_vulkan_queue(const record_gpu_session_vulkan_t *s)
{
   return s && !retro_atomic_load_acquire_int(&s->video_closed)
      && !retro_atomic_load_acquire_int(&s->cancelled)
      && !retro_atomic_load_acquire_int(&s->error) ? s->queue : NULL;
}

int record_gpu_session_vulkan_audio(record_gpu_session_vulkan_t *s,
      const int16_t *samples, size_t frames)
{
   if (!s || retro_atomic_load_acquire_int(&s->audio_closed)
         || retro_atomic_load_acquire_int(&s->cancelled)
         || retro_atomic_load_acquire_int(&s->error))
      return RECORD_VULKAN_OUTPUT_ERROR;
   return record_gpu_output_vulkan_audio(s->output, samples, frames);
}

bool record_gpu_session_vulkan_video_finish(record_gpu_session_vulkan_t *s)
{
   if (!s)
      return false;
   if (retro_atomic_load_acquire_int(&s->video_closed))
      return true;
   if (!record_gpu_queue_finish(s->queue))
      return false;
   retro_atomic_store_release_int(&s->video_closed, 1);
   return true;
}

bool record_gpu_session_vulkan_audio_finish(record_gpu_session_vulkan_t *s)
{
   if (!s)
      return false;
   retro_atomic_store_release_int(&s->audio_closed, 1);
   return true;
}

void record_gpu_session_vulkan_cancel(record_gpu_session_vulkan_t *s)
{
   if (s)
      retro_atomic_store_release_int(&s->cancelled, 1);
}

int record_gpu_session_vulkan_step(record_gpu_session_vulkan_t *s)
{
   int result;
   bool closed;
   if (!s)
      return RECORD_VULKAN_OUTPUT_ERROR;
   if (s->stage == RVKS_RETIRED)
      return retro_atomic_load_acquire_int(&s->error)
         ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END;
   closed = retro_atomic_load_acquire_int(&s->video_closed)
      && retro_atomic_load_acquire_int(&s->audio_closed);
   if (s->output && retro_atomic_load_acquire_int(&s->audio_closed))
      record_gpu_output_vulkan_audio_finish(s->output);
   if (retro_atomic_load_acquire_int(&s->cancelled))
   {
      record_vulkan_session_fail(s, RECORD_VULKAN_SESSION_CANCELLED);
      if (s->stage == RVKS_RUNNING)
         s->stage = RVKS_OUTPUT;
   }
   switch (s->stage)
   {
      case RVKS_RUNNING:
         result = record_gpu_output_vulkan_step(s->output);
         if (result == RECORD_VULKAN_OUTPUT_ERROR || result == RECORD_VULKAN_OUTPUT_END)
         {
            if (result == RECORD_VULKAN_OUTPUT_ERROR)
               record_vulkan_session_fail(s, RECORD_VULKAN_SESSION_OUTPUT);
            retro_atomic_cas_int(&s->file_error, 0,
                  record_gpu_output_vulkan_file_error(s->output));
            s->stage = RVKS_OUTPUT;
            return RECORD_VULKAN_OUTPUT_PROGRESS;
         }
         return result;
      case RVKS_OUTPUT:
         if (s->output)
         {
            if (retro_atomic_load_acquire_int(&s->error))
               record_gpu_output_vulkan_abort(s->output);
            result = record_gpu_output_vulkan_step(s->output);
            if (result == RECORD_VULKAN_OUTPUT_BUSY)
               return RECORD_VULKAN_OUTPUT_BUSY;
            retro_atomic_cas_int(&s->file_error, 0,
                  record_gpu_output_vulkan_file_error(s->output));
            s->audio_frames = record_gpu_output_vulkan_audio_frames(s->output);
            if (!closed || !record_gpu_output_vulkan_free(s->output))
               return RECORD_VULKAN_OUTPUT_BUSY;
            s->output = NULL;
         }
         if (!closed)
            return RECORD_VULKAN_OUTPUT_BUSY;
         s->stage = RVKS_DRAIN;
         return RECORD_VULKAN_OUTPUT_PROGRESS;
      case RVKS_DRAIN:
         if (s->worker)
            record_gpu_worker_vulkan_release(s->worker);
         if (!s->backend->drain(s->encoder)
               || !s->backend->free(s->encoder))
            return RECORD_VULKAN_OUTPUT_BUSY;
         s->encoder = NULL;
         s->pool = NULL;
         s->stage = RVKS_ABANDON;
         return RECORD_VULKAN_OUTPUT_PROGRESS;
      case RVKS_ABANDON:
         if (!record_gpu_worker_vulkan_abandon(s->worker))
            return RECORD_VULKAN_OUTPUT_BUSY;
         s->worker = NULL;
         if (!record_gpu_queue_free(s->queue))
            return RECORD_VULKAN_OUTPUT_BUSY;
         s->queue = NULL;
         s->stage = RVKS_RETIRED;
         return retro_atomic_load_acquire_int(&s->error)
            ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END;
      default:
         break;
   }
   return RECORD_VULKAN_OUTPUT_ERROR;
}

bool record_gpu_session_vulkan_retry_needed(const record_gpu_session_vulkan_t *s)
{
   if (!s || s->stage == RVKS_RETIRED)
      return false;
   if (s->stage == RVKS_RUNNING)
      return record_gpu_output_vulkan_retry_needed(s->output);
   return (s->output && record_gpu_output_vulkan_retry_needed(s->output))
      || (retro_atomic_load_acquire_int(&s->video_closed)
            && retro_atomic_load_acquire_int(&s->audio_closed));
}

bool record_gpu_session_vulkan_wait(const record_gpu_session_vulkan_t *s,
      uint64_t timeout_ns)
{
   return s && s->stage != RVKS_RETIRED && s->output
      && record_gpu_output_vulkan_wait(s->output, timeout_ns);
}

enum record_gpu_session_vulkan_error record_gpu_session_vulkan_error(
      const record_gpu_session_vulkan_t *s, int *file_error)
{
   if (file_error)
      *file_error = s ? retro_atomic_load_acquire_int(&s->file_error) : 0;
   return s ? (enum record_gpu_session_vulkan_error)retro_atomic_load_acquire_int(&s->error)
      : RECORD_VULKAN_SESSION_STARTUP;
}

uint64_t record_gpu_session_vulkan_audio_frames(const record_gpu_session_vulkan_t *s)
{
   return s ? s->audio_frames : 0;
}

bool record_gpu_session_vulkan_free(record_gpu_session_vulkan_t *s)
{
   if (!s)
      return true;
   if (s->stage != RVKS_RETIRED)
      return false;
   free(s);
   return true;
}

bool record_gpu_session_vulkan_network(const record_gpu_session_vulkan_t *s,
      int *fd, bool *read, bool *write)
{
   return record_gpu_output_vulkan_network(s ? s->output : NULL, fd, read, write);
}
