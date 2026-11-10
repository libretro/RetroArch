/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_output_d3d12.h"
#include "record_file_win32.h"
#include <stdlib.h>
#include <string.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#include <formats/rac3.h>
#if defined(HAVE_D3D12) && defined(HAVE_MF_AUDIO) && defined(HAVE_RTMP) && defined(HAVE_RFLV)
#define RECORD_OUTPUT_STREAM
#include "record_stream_d3d12.h"
#endif

struct record_gpu_output_d3d12
{
   retro_spsc_t audio;
   uint64_t audio_pts;
#ifdef RECORD_OUTPUT_STREAM
   record_stream_d3d12_t *stream;
   record_stream_wait_t stream_wait;
#endif
   record_gpu_worker_d3d12_t *worker;
   record_mux_mp4_t *mux;
   record_file_win32_t *file;
   rac3_encoder_t *encoder;
   int16_t *samples;
   float *pcm;
   uint8_t *packet;
   size_t frame_bytes, fifo_bytes;
   unsigned rate, channels, staged;
   retro_atomic_int_t audio_closed, failed;
   bool writing, video_end, audio_end, audio_turn, ended;
};

static int record_gpu_output_fail(record_gpu_output_d3d12_t *p,
      enum record_gpu_output_error error)
{
   if (!retro_atomic_load_acquire_int(&p->failed))
      retro_atomic_store_release_int(&p->failed, (int)error);
   return RECORD_GPU_OUTPUT_ERROR;
}

static void record_gpu_output_destroy(record_gpu_output_d3d12_t *p)
{
   record_mux_mp4_free(p->mux);
   rac3_encoder_free(p->encoder);
   retro_spsc_free(&p->audio);
   free(p->packet);
   free(p->samples);
   free(p->pcm);
   free(p);
}

record_gpu_output_d3d12_t *record_gpu_output_d3d12_new(
      record_gpu_worker_d3d12_t *worker, const record_gpu_output_config_t *config,
      const char *path)
{
   record_gpu_output_d3d12_t *p;
   record_mp4_config_t mux;
   if (!worker || !config || !path || !*path
         || config->video.ac3_header || config->video.ac3_header_size)
      return NULL;
#ifndef RECORD_OUTPUT_STREAM
   if (config->stream_client)
      return NULL;
#endif
   p = (record_gpu_output_d3d12_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   retro_atomic_int_init(&p->audio_closed, config->audio_rate ? 0 : 1);
   retro_atomic_int_init(&p->failed, 0);
   p->worker = worker;
   p->rate = config->audio_rate;
#ifdef RECORD_OUTPUT_STREAM
   if (config->stream_client)
   {
      p->stream = record_stream_d3d12_new(worker, config, config->stream_client);
      if (!p->stream)
         goto error;
      p->stream_wait.network.fd = -1;
      return p;
   }
#endif
   mux = config->video;
   if (p->rate)
   {
      rac3_frame_info_t info;
      size_t size;
      if ((p->rate != 32000 && p->rate != 44100 && p->rate != 48000)
            || config->audio_fifo_frames < 1536
            || config->audio_fifo_frames > p->rate * 2u)
         goto error;
      p->encoder = rac3_encoder_new(p->rate, config->audio_layout,
            config->audio_kbps);
      if (!p->encoder || rac3_encoder_frame_bytes(p->encoder) > mux.max_packet_size)
         goto error;
      p->packet = (uint8_t*)malloc(RAC3_MAX_FRAME_BYTES);
      p->pcm = (float*)calloc(1536 * 6, sizeof(*p->pcm));
      p->samples = (int16_t*)malloc(1536 * 6 * sizeof(*p->samples));
      if (!p->encoder || !p->packet || !p->pcm || !p->samples)
         goto error;
      size = rac3_encode_frame(p->encoder, p->pcm, p->packet,
            RAC3_MAX_FRAME_BYTES);
      if (!size || rac3_parse_frame_info(p->packet, size, &info) != RAC3_OK
            || !info.channels || info.channels > 6)
         goto error;
      p->channels = info.channels;
      p->frame_bytes = sizeof(*p->samples) * p->channels;
      p->fifo_bytes = config->audio_fifo_frames * p->frame_bytes;
      if (!retro_spsc_init(&p->audio, p->fifo_bytes))
         goto error;
      mux.ac3_header = p->packet;
      mux.ac3_header_size = size;
      /* Header generation must not advance the live encoder's history. */
      rac3_encoder_free(p->encoder);
      p->encoder = rac3_encoder_new(p->rate, config->audio_layout,
            config->audio_kbps);
      if (!p->encoder)
         goto error;
   }
   else
   {
      if (config->audio_layout || config->audio_kbps || config->audio_fifo_frames)
         goto error;
      p->audio_end = true;
   }
   p->mux = record_mux_mp4_new(&mux);
   if (!p->mux)
      goto error;
   p->file = record_file_win32_new(path);
   if (!p->file)
      goto error;
   return p;
error:
   record_gpu_output_destroy(p);
   return NULL;
}

int record_gpu_output_d3d12_audio(record_gpu_output_d3d12_t *p,
      const int16_t *samples, size_t frames)
{
   size_t bytes, space, used;
#ifdef RECORD_OUTPUT_STREAM
   if (p && p->stream)
      return record_stream_d3d12_audio(p->stream, samples, frames);
#endif
   if (!p || !p->rate || !samples || !frames
         || frames > p->fifo_bytes / p->frame_bytes
         || retro_atomic_load_acquire_int(&p->audio_closed)
         || retro_atomic_load_acquire_int(&p->failed))
      return RECORD_GPU_OUTPUT_ERROR;
   bytes = frames * p->frame_bytes;
   space = retro_spsc_write_avail(&p->audio);
   used = p->audio.capacity - space;
   if (used > p->fifo_bytes || bytes > p->fifo_bytes - used)
      return RECORD_GPU_OUTPUT_BUSY;
   return retro_spsc_write(&p->audio, samples, bytes) == bytes
      ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_ERROR;
}

bool record_gpu_output_d3d12_audio_finish(record_gpu_output_d3d12_t *p)
{
   if (!p)
      return false;
   retro_atomic_store_release_int(&p->audio_closed, 1);
#ifdef RECORD_OUTPUT_STREAM
   if (p->stream)
      return record_stream_d3d12_audio_finish(p->stream);
#endif
   return true;
}

static int record_gpu_output_audio_step(record_gpu_output_d3d12_t *p)
{
   size_t bytes, frames, size, i;
   int closed;
   if (p->audio_end)
      return RECORD_GPU_OUTPUT_BUSY;
   closed = retro_atomic_load_acquire_int(&p->audio_closed);
   bytes = retro_spsc_read_avail(&p->audio);
   frames = bytes / p->frame_bytes;
   if (frames > 1536 - p->staged)
      frames = 1536 - p->staged;
   if (frames)
   {
      bytes = frames * p->frame_bytes;
      if (retro_spsc_read(&p->audio, p->samples + p->staged * p->channels,
               bytes) != bytes)
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
      p->staged += (unsigned)frames;
   }
   if (p->staged < 1536 && !closed)
   {
      p->audio_turn = false;
      return frames ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
   }
   if (!p->staged)
   {
      p->audio_end = true;
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (p->audio_pts > (uint64_t)-1 - 1536)
      return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
   memset(p->pcm, 0, 1536 * p->channels * sizeof(*p->pcm));
   for (i = 0; i < p->staged * p->channels; i++)
      p->pcm[i] = p->samples[i] * (1.0f / 32768.0f);
   size = rac3_encode_frame(p->encoder, p->pcm, p->packet, RAC3_MAX_FRAME_BYTES);
   if (!size)
      return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
   if (record_mux_mp4_push(p->mux, 2, p->packet, size, p->audio_pts,
            1536, true) != RECORD_MP4_OK)
      return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
   p->audio_pts += 1536;
   p->staged = 0;
   p->audio_turn = false;
   return RECORD_GPU_OUTPUT_PROGRESS;
}

int record_gpu_output_d3d12_step(record_gpu_output_d3d12_t *p)
{
   const void *data;
   size_t size;
   nvenc_d3d12_packet_t packet;
   int result;
   if (!p)
      return RECORD_GPU_OUTPUT_ERROR;
#ifdef RECORD_OUTPUT_STREAM
   if (p->stream)
      return record_stream_d3d12_step(p->stream, 0, &p->stream_wait);
#endif
   if (p->writing)
   {
      result = record_file_win32_step(p->file);
      if (result == RECORD_FILE_BUSY)
         return RECORD_GPU_OUTPUT_BUSY;
      p->writing = false;
      if (result == RECORD_FILE_ERROR)
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_FILE);
      if (!record_mux_mp4_release(p->mux))
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
      if (!retro_atomic_load_acquire_int(&p->failed))
         return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (retro_atomic_load_acquire_int(&p->failed))
      return RECORD_GPU_OUTPUT_ERROR;
   if (p->ended)
      return RECORD_GPU_OUTPUT_END;
   if (record_mux_mp4_get(p->mux, &data, &size))
   {
      if (record_file_win32_push(p->file, data, size) != RECORD_FILE_OK)
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_FILE);
      p->writing = true;
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (p->video_end && p->audio_end)
   {
      if (!record_mux_mp4_finish(p->mux))
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
      if (!record_file_win32_finish(p->file))
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_FILE);
      p->ended = true;
      return RECORD_GPU_OUTPUT_END;
   }
   if (p->audio_turn)
   {
      result = record_gpu_output_audio_step(p);
      if (result != RECORD_GPU_OUTPUT_BUSY)
         return result;
   }
   if (!p->video_end)
   {
      result = record_gpu_worker_d3d12_step(p->worker, &packet);
      if (result == RECORD_GPU_WORKER_ERROR)
         return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_VIDEO);
      if (result == RECORD_GPU_WORKER_PACKET)
      {
         int mux_result = RECORD_MP4_ERROR;
         if (packet.duration && packet.duration <= 0xffffffffu)
            mux_result = record_mux_mp4_push(p->mux, 1, packet.data, packet.size,
                  packet.timestamp, (uint32_t)packet.duration, packet.keyframe);
         if (!record_gpu_worker_d3d12_release(p->worker))
            return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_VIDEO);
         if (mux_result != RECORD_MP4_OK)
            return record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
         p->audio_turn = true;
         return RECORD_GPU_OUTPUT_PROGRESS;
      }
      if (result == RECORD_GPU_WORKER_END)
         p->video_end = true;
      if (result == RECORD_GPU_WORKER_END || result == RECORD_GPU_WORKER_PROGRESS)
      {
         p->audio_turn = true;
         return RECORD_GPU_OUTPUT_PROGRESS;
      }
   }
   return record_gpu_output_audio_step(p);
}

void *record_gpu_output_d3d12_wait_handle(const record_gpu_output_d3d12_t *p)
{
   if (!p) return NULL;
#ifdef RECORD_OUTPUT_STREAM
   if (p->stream)
      return p->stream_wait.encoder_retry
         ? record_gpu_worker_d3d12_wait_handle(p->worker) : NULL;
#endif
   if (p->writing) return record_file_win32_wait_handle(p->file);
   if (p->ended || p->video_end || retro_atomic_load_acquire_int(&p->failed))
      return NULL;
   return record_gpu_worker_d3d12_wait_handle(p->worker);
}

bool record_gpu_output_d3d12_retry_needed(const record_gpu_output_d3d12_t *p)
{
   if (!p) return false;
#ifdef RECORD_OUTPUT_STREAM
   if (p->stream)
      return p->stream_wait.encoder_retry || p->stream_wait.network.again;
#endif
   if (p->writing) return true;
   return !p->ended && !p->video_end
      && !retro_atomic_load_acquire_int(&p->failed)
      && record_gpu_worker_d3d12_retry_needed(p->worker);
}

bool record_gpu_output_d3d12_abort(record_gpu_output_d3d12_t *p)
{
#ifdef RECORD_OUTPUT_STREAM
   if (p && p->stream)
   {
      record_stream_d3d12_abort(p->stream);
      return true;
   }
#endif
   if (!p || p->ended)
      return false;
   record_gpu_output_fail(p, RECORD_GPU_OUTPUT_ERROR_ABORTED);
   return !p->writing || record_file_win32_cancel(p->file);
}

enum record_gpu_output_error record_gpu_output_d3d12_error(
      const record_gpu_output_d3d12_t *p)
{
#ifdef RECORD_OUTPUT_STREAM
   if (p && p->stream)
      return record_stream_d3d12_error(p->stream);
#endif
   return p ? (enum record_gpu_output_error)retro_atomic_load_acquire_int(&p->failed)
      : RECORD_GPU_OUTPUT_ERROR_VIDEO;
}

uint32_t record_gpu_output_d3d12_file_error(const record_gpu_output_d3d12_t *p)
{
#ifdef RECORD_OUTPUT_STREAM
   if (p && p->stream)
      return 0;
#endif
   return record_file_win32_error(p ? p->file : NULL);
}

bool record_gpu_output_d3d12_free(record_gpu_output_d3d12_t *p)
{
   if (!p)
      return true;
#ifdef RECORD_OUTPUT_STREAM
   if (p->stream)
   {
      if (!retro_atomic_load_acquire_int(&p->audio_closed)
            || !record_stream_d3d12_free(p->stream))
         return false;
      p->stream = NULL;
      record_gpu_output_destroy(p);
      return true;
   }
#endif
   if (!retro_atomic_load_acquire_int(&p->audio_closed) || p->writing
         || (!p->ended && !retro_atomic_load_acquire_int(&p->failed)))
      return false;
   if (!record_file_win32_free(p->file))
      return false;
   record_gpu_output_destroy(p);
   return true;
}

bool record_gpu_output_d3d12_network(const record_gpu_output_d3d12_t *p,
      int *fd, bool *read, bool *write)
{
   if (!fd || !read || !write)
      return false;
   *fd = -1;
   *read = *write = false;
#ifdef RECORD_OUTPUT_STREAM
   if (p && p->stream && p->stream_wait.network.fd >= 0)
   {
      *fd = p->stream_wait.network.fd;
      *read = p->stream_wait.network.read;
      *write = p->stream_wait.network.write;
      return true;
   }
#else
   (void)p;
#endif
   return false;
}
