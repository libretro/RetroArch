/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_output_vulkan.h"
#include "record_file_posix.h"
#include <stdlib.h>
#include <string.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#include <formats/rac3.h>
#if defined(HAVE_MF_AUDIO) && defined(HAVE_RTMP) && defined(HAVE_RFLV)
#define RECORD_VULKAN_OUTPUT_STREAM
#include "record_stream_vulkan.h"
#endif


struct record_gpu_output_vulkan
{
   retro_spsc_t audio;
   uint64_t audio_pts, audio_frames, audio_limit;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   record_stream_gpu_t *stream;
   record_stream_wait_t stream_wait;
#endif
   record_gpu_worker_vulkan_t *worker;
   record_mux_mp4_t *mux;
   record_file_posix_t *file;
   rac3_encoder_t *encoder;
   float *pcm;
   uint8_t *packet;
   size_t frame_bytes, fifo_bytes;
   unsigned rate, channels, staged, timescale;
   retro_atomic_int_t audio_closed, failed;
   bool writing, video_end, audio_end, audio_turn, ended, audio_follow_video;
};

#ifdef RECORD_VULKAN_OUTPUT_STREAM
static int record_vulkan_stream_result(int result)
{
   switch (result)
   {
      case RECORD_GPU_OUTPUT_BUSY: return RECORD_VULKAN_OUTPUT_BUSY;
      case RECORD_GPU_OUTPUT_PROGRESS: return RECORD_VULKAN_OUTPUT_PROGRESS;
      case RECORD_GPU_OUTPUT_END: return RECORD_VULKAN_OUTPUT_END;
      default: return RECORD_VULKAN_OUTPUT_ERROR;
   }
}
#endif

static int record_gpu_output_vulkan_fail(record_gpu_output_vulkan_t *p,
                                  enum record_gpu_output_vulkan_error error)
{
   if (!retro_atomic_load_acquire_int(&p->failed))
      retro_atomic_store_release_int(&p->failed, (int)error);
   return RECORD_VULKAN_OUTPUT_ERROR;
}

static void record_gpu_output_vulkan_destroy(record_gpu_output_vulkan_t *p)
{
   record_mux_mp4_free(p->mux);
   rac3_encoder_free(p->encoder);
   retro_spsc_free(&p->audio);
   free(p->packet);
   free(p->pcm);
   free(p);
}

record_gpu_output_vulkan_t *
record_gpu_output_vulkan_new(record_gpu_worker_vulkan_t *worker,
                             const record_gpu_output_vulkan_config_t *config,
                             const char *path)
{
   record_gpu_output_vulkan_t *p;
   record_mp4_config_t mux;
   if (!worker || !config || !path || !*path || config->video.ac3_header ||
       config->video.ac3_header_size)
      return NULL;
#ifndef RECORD_VULKAN_OUTPUT_STREAM
   if (config->stream_client)
      return NULL;
#endif
   p = (record_gpu_output_vulkan_t *)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   retro_atomic_int_init(&p->audio_closed, config->audio_rate ? 0 : 1);
   retro_atomic_int_init(&p->failed, 0);
   p->worker = worker;
   p->rate = config->audio_rate;
   p->timescale = config->video.timescale;
   p->audio_follow_video = config->audio_follow_video;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (config->stream_client)
   {
      record_gpu_output_config_t stream_config;
      memset(&stream_config, 0, sizeof(stream_config));
      stream_config.video = config->video;
      stream_config.stream_color.primaries = config->video.color_primaries;
      stream_config.stream_color.transfer = config->video.transfer_characteristics;
      stream_config.stream_color.matrix = config->video.matrix_coefficients;
      stream_config.stream_color.range = (config->video.color_primaries
            || config->video.transfer_characteristics || config->video.matrix_coefficients
            || config->video.full_range) ? (config->video.full_range ? 2 : 1) : 0;
      stream_config.stream_color.max_cll = config->video.max_cll;
      stream_config.stream_color.max_fall = config->video.max_fall;
      stream_config.stream_color.mastering = config->video.mastering;
      stream_config.audio_rate = config->audio_rate;
      stream_config.audio_kbps = config->audio_kbps;
      stream_config.audio_layout = config->audio_layout;
      stream_config.audio_fifo_frames = config->audio_fifo_frames;
      stream_config.stream_audio_follow_video = config->audio_follow_video;
      p->stream = record_stream_vulkan_new(worker, &stream_config, config->stream_client);
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
      if ((p->rate != 32000 && p->rate != 44100 && p->rate != 48000) ||
          config->audio_fifo_frames < 1536 || config->audio_fifo_frames > p->rate * 2u)
         goto error;
      p->encoder = rac3_encoder_new(p->rate, config->audio_layout, config->audio_kbps);
      if (!p->encoder || rac3_encoder_frame_bytes(p->encoder) > mux.max_packet_size)
         goto error;
      p->packet = (uint8_t *)malloc(RAC3_MAX_FRAME_BYTES);
      p->pcm = (float *)calloc(1536 * 6, sizeof(*p->pcm));
      if (!p->encoder || !p->packet || !p->pcm)
         goto error;
      size = rac3_encode_frame(p->encoder, p->pcm, p->packet, RAC3_MAX_FRAME_BYTES);
      if (!size || rac3_parse_frame_info(p->packet, size, &info) != RAC3_OK ||
          !info.channels || info.channels > 6)
         goto error;
      p->channels = info.channels;
      p->frame_bytes = sizeof(int16_t) * p->channels;
      p->fifo_bytes = config->audio_fifo_frames * p->frame_bytes;
      if (!retro_spsc_init(&p->audio, p->fifo_bytes))
         goto error;
      mux.ac3_header = p->packet;
      mux.ac3_header_size = size;
      /* Header generation must not advance the live encoder's history. */
      rac3_encoder_free(p->encoder);
      p->encoder = rac3_encoder_new(p->rate, config->audio_layout, config->audio_kbps);
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
   p->file = record_file_posix_new(path);
   if (!p->file)
      goto error;
   return p;
error:
   record_gpu_output_vulkan_destroy(p);
   return NULL;
}

int record_gpu_output_vulkan_audio(record_gpu_output_vulkan_t *p,
                                   const int16_t *samples, size_t frames)
{
   size_t bytes, space, used;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p && p->stream)
      return record_vulkan_stream_result(record_stream_gpu_audio(p->stream, samples, frames));
#endif
   if (!p || !p->rate || !samples || !frames ||
       frames > p->fifo_bytes / p->frame_bytes ||
       retro_atomic_load_acquire_int(&p->audio_closed) ||
       retro_atomic_load_acquire_int(&p->failed))
      return RECORD_VULKAN_OUTPUT_ERROR;
   bytes = frames * p->frame_bytes;
   space = retro_spsc_write_avail(&p->audio);
   used = p->audio.capacity - space;
   if (used > p->fifo_bytes || bytes > p->fifo_bytes - used)
      return RECORD_VULKAN_OUTPUT_BUSY;
   return retro_spsc_write(&p->audio, samples, bytes) == bytes
              ? RECORD_VULKAN_OUTPUT_PROGRESS
              : RECORD_VULKAN_OUTPUT_ERROR;
}

bool record_gpu_output_vulkan_audio_finish(record_gpu_output_vulkan_t *p)
{
   if (!p)
      return false;
   retro_atomic_store_release_int(&p->audio_closed, 1);
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p->stream)
      return record_stream_gpu_audio_finish(p->stream);
#endif
   return true;
}

static int record_gpu_output_vulkan_audio_step(record_gpu_output_vulkan_t *p)
{
   size_t bytes, frames, size, i;
   int closed;
   if (p->audio_end)
      return RECORD_VULKAN_OUTPUT_BUSY;
   closed = retro_atomic_load_acquire_int(&p->audio_closed);
   bytes = retro_spsc_read_avail(&p->audio);
   frames = bytes / p->frame_bytes;
   if (frames > 1536 - p->staged)
      frames = 1536 - p->staged;
   if (p->audio_follow_video)
   {
      uint64_t available;
      if (p->audio_limit < p->audio_frames)
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
      available = p->audio_limit - p->audio_frames;
      if (frames > available)
         frames = (size_t)available;
      /* Producer closure alone cannot flush a partial block: more submitted
       * pictures may still be queued on the GPU and extend the audio limit. */
      closed = closed && p->video_end;
   }
   if (frames > (uint64_t)-1 - p->audio_frames)
      return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
   if (frames)
   {
      size_t converted = 0;
      unsigned span_index;
      bytes = frames * p->frame_bytes;
      for (span_index = 0; span_index < 2 && converted < bytes; span_index++)
      {
         const void *borrow;
         const int16_t *samples;
         size_t span = retro_spsc_read_begin(&p->audio, &borrow);
         if (span > bytes - converted)
            span = bytes - converted;
         if (!span || span % sizeof(int16_t))
         {
            retro_spsc_read_end(&p->audio, 0);
            return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
         }
         samples = (const int16_t*)borrow;
         for (i = 0; i < span / sizeof(*samples); i++)
            p->pcm[p->staged * p->channels + converted / sizeof(*samples) + i] =
               samples[i] * (1.0f / 32768.0f);
         retro_spsc_read_end(&p->audio, span);
         converted += span;
      }
      if (converted != bytes)
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
      p->staged += (unsigned)frames;
      p->audio_frames += frames;
   }
   if (p->staged < 1536 && !closed)
   {
      p->audio_turn = false;
      return frames ? RECORD_VULKAN_OUTPUT_PROGRESS : RECORD_VULKAN_OUTPUT_BUSY;
   }
   if (!p->staged)
   {
      p->audio_end = true;
      return RECORD_VULKAN_OUTPUT_PROGRESS;
   }
   if (p->audio_pts > (uint64_t)-1 - 1536)
      return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
   if (p->staged < 1536)
      memset(p->pcm + p->staged * p->channels, 0,
            (1536 - p->staged) * p->channels * sizeof(*p->pcm));
   size = rac3_encode_frame(p->encoder, p->pcm, p->packet, RAC3_MAX_FRAME_BYTES);
   if (!size)
      return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
   if (record_mux_mp4_push(p->mux, 2, p->packet, size, p->audio_pts, 1536, true) !=
       RECORD_MP4_OK)
      return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_MUX);
   p->audio_pts += 1536;
   p->staged = 0;
   p->audio_turn = false;
   return RECORD_VULKAN_OUTPUT_PROGRESS;
}

int record_gpu_output_vulkan_step(record_gpu_output_vulkan_t *p)
{
   const void *data;
   size_t size;
   vulkan_encode_packet_t packet;
   int result;
   if (!p)
      return RECORD_VULKAN_OUTPUT_ERROR;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p->stream)
      return record_vulkan_stream_result(record_stream_gpu_step(p->stream, 0, &p->stream_wait));
#endif
   if (p->writing)
   {
      uint64_t written = record_file_posix_bytes(p->file);
      result = record_file_posix_step(p->file);
      /* A step writes one bounded chunk: bytes moved are progress */
      if (result == RECORD_FILE_POSIX_BUSY)
         return record_file_posix_bytes(p->file) != written
            ? RECORD_VULKAN_OUTPUT_PROGRESS : RECORD_VULKAN_OUTPUT_BUSY;
      p->writing = false;
      if (result == RECORD_FILE_POSIX_ERROR)
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_FILE);
      if (!record_mux_mp4_release(p->mux))
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_MUX);
      if (!retro_atomic_load_acquire_int(&p->failed))
         return RECORD_VULKAN_OUTPUT_PROGRESS;
   }
   if (retro_atomic_load_acquire_int(&p->failed))
      return RECORD_VULKAN_OUTPUT_ERROR;
   if (p->ended)
      return RECORD_VULKAN_OUTPUT_END;
   if (record_mux_mp4_get(p->mux, &data, &size))
   {
      if (record_file_posix_push(p->file, data, size) != RECORD_FILE_POSIX_OK)
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_FILE);
      p->writing = true;
      return RECORD_VULKAN_OUTPUT_PROGRESS;
   }
   if (p->video_end && p->audio_end)
   {
      if (!record_mux_mp4_finish(p->mux))
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_MUX);
      if (!record_file_posix_finish(p->file))
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_FILE);
      p->ended = true;
      return RECORD_VULKAN_OUTPUT_END;
   }
   if (p->audio_turn)
   {
      result = record_gpu_output_vulkan_audio_step(p);
      if (result != RECORD_VULKAN_OUTPUT_BUSY)
         return result;
   }
   if (!p->video_end)
   {
      result = record_gpu_worker_vulkan_step(p->worker, &packet);
      if (result == RECORD_GPU_VULKAN_ERROR)
         return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_VIDEO);
      if (result == RECORD_GPU_VULKAN_PACKET)
      {
         int mux_result = RECORD_MP4_ERROR;
         if (packet.duration && packet.duration <= 0xffffffffu
               && packet.timestamp <= (uint64_t)-1 - packet.duration)
         {
            uint64_t end = packet.timestamp + packet.duration;
            if (p->audio_follow_video && p->rate)
            {
               uint64_t whole = end / p->timescale;
               uint64_t part = (end % p->timescale) * p->rate / p->timescale;
               if (whole > ((uint64_t)-1 - part) / p->rate)
               {
                  record_gpu_worker_vulkan_release(p->worker);
                  return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_AUDIO);
               }
               p->audio_limit = whole * p->rate + part;
            }
            mux_result = record_mux_mp4_push(
                p->mux, 1, packet.data, packet.size, packet.timestamp,
                (uint32_t)packet.duration, packet.keyframe);
         }
         if (!record_gpu_worker_vulkan_release(p->worker))
            return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_VIDEO);
         if (mux_result != RECORD_MP4_OK)
            return record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_MUX);
         p->audio_turn = true;
         return RECORD_VULKAN_OUTPUT_PROGRESS;
      }
      if (result == RECORD_GPU_VULKAN_END)
         p->video_end = true;
      if (result == RECORD_GPU_VULKAN_END || result == RECORD_GPU_VULKAN_PROGRESS)
      {
         p->audio_turn = true;
         return RECORD_VULKAN_OUTPUT_PROGRESS;
      }
   }
   return record_gpu_output_vulkan_audio_step(p);
}

uint64_t record_gpu_output_vulkan_audio_frames(const record_gpu_output_vulkan_t *p)
{
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p && p->stream)
      return record_stream_gpu_audio_frames(p->stream);
#endif
   return p ? p->audio_frames : 0;
}

bool record_gpu_output_vulkan_retry_needed(const record_gpu_output_vulkan_t *p)
{
   if (!p)
      return false;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p->stream)
      return p->stream_wait.encoder_retry || p->stream_wait.network.again;
#endif
   if (p->writing)
      return true;
   return !p->ended && !p->video_end && !retro_atomic_load_acquire_int(&p->failed) &&
          record_gpu_worker_vulkan_retry_needed(p->worker);
}

bool record_gpu_output_vulkan_wait(const record_gpu_output_vulkan_t *p,
      uint64_t timeout_ns)
{
   if (!p)
      return false;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p->stream)
      return false;
#endif
   return !p->writing && !p->ended && !p->video_end
      && !retro_atomic_load_acquire_int(&p->failed)
      && record_gpu_worker_vulkan_wait(p->worker, timeout_ns);
}

bool record_gpu_output_vulkan_abort(record_gpu_output_vulkan_t *p)
{
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p && p->stream)
   {
      record_stream_gpu_abort(p->stream);
      return true;
   }
#endif
   if (!p || p->ended)
      return false;
   record_gpu_output_vulkan_fail(p, RECORD_VULKAN_OUTPUT_ERROR_ABORTED);
   return !p->writing || record_file_posix_cancel(p->file);
}

enum record_gpu_output_vulkan_error
record_gpu_output_vulkan_error(const record_gpu_output_vulkan_t *p)
{
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p && p->stream)
      switch (record_stream_gpu_error(p->stream))
      {
         case RECORD_GPU_OUTPUT_ERROR_NONE: return RECORD_VULKAN_OUTPUT_ERROR_NONE;
         case RECORD_GPU_OUTPUT_ERROR_VIDEO: return RECORD_VULKAN_OUTPUT_ERROR_VIDEO;
         case RECORD_GPU_OUTPUT_ERROR_AUDIO: return RECORD_VULKAN_OUTPUT_ERROR_AUDIO;
         case RECORD_GPU_OUTPUT_ERROR_MUX: return RECORD_VULKAN_OUTPUT_ERROR_MUX;
         case RECORD_GPU_OUTPUT_ERROR_FILE: return RECORD_VULKAN_OUTPUT_ERROR_FILE;
         case RECORD_GPU_OUTPUT_ERROR_ABORTED: return RECORD_VULKAN_OUTPUT_ERROR_ABORTED;
         default: return RECORD_VULKAN_OUTPUT_ERROR_NETWORK;
      }
#endif
   return p ? (enum record_gpu_output_vulkan_error)retro_atomic_load_acquire_int(
                  &p->failed)
            : RECORD_VULKAN_OUTPUT_ERROR_VIDEO;
}

int record_gpu_output_vulkan_file_error(const record_gpu_output_vulkan_t *p)
{
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p && p->stream)
      return 0;
#endif
   return record_file_posix_error(p ? p->file : NULL);
}

bool record_gpu_output_vulkan_free(record_gpu_output_vulkan_t *p)
{
   if (!p)
      return true;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
   if (p->stream)
   {
      if (!retro_atomic_load_acquire_int(&p->audio_closed)
            || !record_stream_gpu_free(p->stream))
         return false;
      p->stream = NULL;
      record_gpu_output_vulkan_destroy(p);
      return true;
   }
#endif
   if (!retro_atomic_load_acquire_int(&p->audio_closed) || p->writing ||
       (!p->ended && !retro_atomic_load_acquire_int(&p->failed)))
      return false;
   if (!record_file_posix_free(p->file))
      return false;
   record_gpu_output_vulkan_destroy(p);
   return true;
}

bool record_gpu_output_vulkan_network(const record_gpu_output_vulkan_t *p,
      int *fd, bool *read, bool *write)
{
   if (!fd || !read || !write)
      return false;
   *fd = -1;
   *read = *write = false;
#ifdef RECORD_VULKAN_OUTPUT_STREAM
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
