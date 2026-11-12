/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_d3d12.h"
#include "record_mf_audio.h"
#include <formats/rflv_mux.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#include <stdlib.h>

enum stream_producer
{
   STREAM_NONE, STREAM_VIDEO, STREAM_AUDIO
};

struct record_stream_d3d12
{
   retro_spsc_t audio;
   uint64_t audio_pts;
   record_gpu_worker_d3d12_t *worker;
   net_rtmp_client_t *client;
   net_rtmp_session_t *session;
   record_mf_audio_t *aac;
   rflv_mux_t *mux;
   int16_t *input;
   size_t input_frames, staged, frame_bytes, fifo_bytes;
   retro_atomic_int_t audio_closed, failed;
   enum stream_producer producer;
   bool prepared, sending, video_end, audio_end, audio_turn;
   bool audio_finishing, finishing, ended, retired;
   bool av1;
};

static int stream_fail(record_stream_d3d12_t *p, enum record_gpu_output_error error)
{
   retro_atomic_cas_int(&p->failed, 0, (int)error);
   return RECORD_GPU_OUTPUT_ERROR;
}

record_stream_d3d12_t *record_stream_d3d12_new(record_gpu_worker_d3d12_t *worker,
      const record_gpu_output_config_t *config, net_rtmp_client_t *client)
{
   record_stream_d3d12_t *p;
   net_rtmp_client_view_t view;
   rflv_config_t mux;
   unsigned channels;
   if (!worker || !client || !config
         || (config->video.codec != RECORD_MP4_H264
            && config->video.codec != RECORD_MP4_HEVC && config->video.codec != RECORD_MP4_AV1)
         || (config->video.bit_depth != 8
            && (config->video.codec == RECORD_MP4_H264 || config->video.bit_depth != 10))
         || !config->video.timescale
         || config->video.ac3_header || config->video.ac3_header_size
         || net_rtmp_client_step(client, 0, &view) != NET_RTMP_CLIENT_READY)
      return NULL;
   p = (record_stream_d3d12_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->worker = worker;
   p->av1 = config->video.codec == RECORD_MP4_AV1;
   p->client = client;
   p->session = view.session;
   retro_atomic_int_init(&p->audio_closed, config->audio_rate ? 0 : 1);
   retro_atomic_int_init(&p->failed, 0);
   mux.avcc = config->video.headers;
   mux.avcc_size = config->video.headers_size;
   mux.asc = NULL;
   mux.asc_size = 0;
   mux.audio_timescale = 0;
   mux.video_timescale = config->video.timescale;
   mux.max_packet_size = config->video.max_packet_size;
   if (config->audio_rate)
   {
      channels = config->audio_layout == 4 ? 1 : config->audio_layout == 3 ? 2 : 0;
      if (!channels || (config->audio_rate != 44100 && config->audio_rate != 48000)
            || config->audio_fifo_frames < 1024
            || config->audio_fifo_frames > config->audio_rate * 2u
            || config->audio_kbps > 192)
         goto failed;
      p->aac = record_mf_audio_new(config->audio_rate, channels, config->audio_kbps * 1000);
      if (!p->aac || !record_mf_audio_config(p->aac, &mux.asc, &mux.asc_size))
         goto failed;
      mux.audio_timescale = 10000000;
      p->frame_bytes = channels * sizeof(int16_t);
      p->fifo_bytes = config->audio_fifo_frames * p->frame_bytes;
      if (!retro_spsc_init(&p->audio, p->fifo_bytes))
         goto failed;
   }
   else
   {
      if (config->audio_layout || config->audio_kbps || config->audio_fifo_frames)
         goto failed;
      p->audio_end = true;
   }
   p->mux = config->video.codec == RECORD_MP4_AV1 ? rflv_mux_new_av1_obu(&mux)
      : config->video.codec == RECORD_MP4_HEVC
      ? rflv_mux_new_hevc_annexb(&mux) : rflv_mux_new_annexb(&mux);
   if (!p->mux || (config->video.codec != RECORD_MP4_H264
            && !rflv_mux_set_color(p->mux, &config->stream_color, config->video.bit_depth)))
      goto failed;
   return p;
failed:
   rflv_mux_free(p->mux);
   if (p->aac)
   {
      record_mf_audio_cancel(p->aac);
      record_mf_audio_free(p->aac);
   }
   retro_spsc_free(&p->audio);
   free(p);
   return NULL;
}

int record_stream_d3d12_audio(record_stream_d3d12_t *p, const int16_t *samples, size_t frames)
{
   size_t bytes, used;
   if (!p || !p->aac || !samples || !frames
         || frames > p->fifo_bytes / p->frame_bytes
         || retro_atomic_load_acquire_int(&p->audio_closed)
         || retro_atomic_load_acquire_int(&p->failed))
      return RECORD_GPU_OUTPUT_ERROR;
   bytes = frames * p->frame_bytes;
   used = p->audio.capacity - retro_spsc_write_avail(&p->audio);
   if (used > p->fifo_bytes || bytes > p->fifo_bytes - used)
      return RECORD_GPU_OUTPUT_BUSY;
   return retro_spsc_write(&p->audio, samples, bytes) == bytes
      ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_ERROR;
}

bool record_stream_d3d12_audio_finish(record_stream_d3d12_t *p)
{
   if (!p)
      return false;
   retro_atomic_store_release_int(&p->audio_closed, 1);
   return true;
}

static int stream_audio(record_stream_d3d12_t *p)
{
   record_mf_audio_packet_t packet;
   size_t frames, bytes;
   int result, closed;
   if (p->audio_end)
      return RECORD_GPU_OUTPUT_BUSY;
   result = record_mf_audio_pump(p->aac, &packet);
   if (result == RECORD_MF_AUDIO_PACKET)
   {
      p->producer = STREAM_AUDIO;
      if (packet.timestamp < 0 || packet.duration <= 0
            || rflv_mux_push(p->mux, 2, packet.data, packet.size,
               (uint64_t)packet.timestamp, (uint64_t)packet.timestamp, false) != RFLV_PROGRESS)
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (result == RECORD_MF_AUDIO_END)
   {
      p->audio_end = true;
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (result == RECORD_MF_AUDIO_ERROR)
      return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
   if (result == RECORD_MF_AUDIO_PROGRESS)
      return RECORD_GPU_OUTPUT_PROGRESS;
   if (p->audio_finishing)
      return RECORD_GPU_OUTPUT_BUSY;
   closed = retro_atomic_load_acquire_int(&p->audio_closed);
   if (!p->prepared)
   {
      frames = retro_spsc_read_avail(&p->audio) / p->frame_bytes;
      if (!p->input && !frames)
      {
         if (!closed)
            return RECORD_GPU_OUTPUT_BUSY;
         if (record_mf_audio_finish(p->aac) != RECORD_MF_AUDIO_PROGRESS)
            return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
         p->audio_finishing = true;
         return RECORD_GPU_OUTPUT_PROGRESS;
      }
      if (!p->input)
      {
         result = record_mf_audio_input(p->aac, &p->input, &p->input_frames);
         if (result == RECORD_MF_AUDIO_BUSY)
            return RECORD_GPU_OUTPUT_BUSY;
         if (result != RECORD_MF_AUDIO_PROGRESS || !p->input_frames || p->input_frames > 1024)
            return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
      }
      if (frames > p->input_frames - p->staged)
         frames = p->input_frames - p->staged;
      bytes = frames * p->frame_bytes;
      if (bytes && retro_spsc_read(&p->audio,
               (uint8_t*)p->input + p->staged * p->frame_bytes, bytes) != bytes)
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
      p->staged += frames;
      if (p->staged < p->input_frames && !closed)
         return frames ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
   }
   if (!p->staged || p->audio_pts > (uint64_t)-1 - p->staged)
      return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
   result = record_mf_audio_submit(p->aac, p->staged, p->audio_pts);
   p->prepared = true;
   if (result == RECORD_MF_AUDIO_BUSY)
      return RECORD_GPU_OUTPUT_BUSY;
   if (result != RECORD_MF_AUDIO_PROGRESS)
      return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
   p->audio_pts += p->staged;
   p->input = NULL;
   p->staged = 0;
   p->prepared = false;
   return RECORD_GPU_OUTPUT_PROGRESS;
}

static int stream_return_producer(record_stream_d3d12_t *p)
{
   if (p->producer == STREAM_VIDEO)
   {
      if (!record_gpu_worker_d3d12_release(p->worker))
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_VIDEO);
      p->audio_turn = true;
   }
   else if (p->producer == STREAM_AUDIO)
   {
      bool released = record_mf_audio_release(p->aac);
      p->producer = STREAM_NONE;
      if (!released)
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_AUDIO);
      p->audio_turn = false;
   }
   p->producer = STREAM_NONE;
   return RECORD_GPU_OUTPUT_PROGRESS;
}

static int stream_cancel(record_stream_d3d12_t *p, record_stream_wait_t *wait)
{
   net_rtmp_client_cancel(p->client);
   if (p->sending)
   {
      if (!net_rtmp_session_media_release(p->session))
      {
         wait->encoder_retry = true;
         return RECORD_GPU_OUTPUT_BUSY;
      }
      p->sending = false;
   }
   rflv_mux_free(p->mux);
   p->mux = NULL;
   if (stream_return_producer(p) == RECORD_GPU_OUTPUT_ERROR && p->producer != STREAM_NONE)
   {
      wait->encoder_retry = true;
      return RECORD_GPU_OUTPUT_BUSY;
   }
   if (p->aac)
      record_mf_audio_cancel(p->aac);
   p->input = NULL;
   p->prepared = false;
   p->retired = true;
   return RECORD_GPU_OUTPUT_ERROR;
}

int record_stream_d3d12_step(record_stream_d3d12_t *p,
      uint32_t receive_time, record_stream_wait_t *wait)
{
   net_rtmp_span_t spans[2];
   nvenc_d3d12_packet_t packet;
   rflv_tag_t tag;
   enum net_rtmp_client_state state;
   int result;
   bool audio_attempted = false;
   if (!p || !wait)
      return RECORD_GPU_OUTPUT_ERROR;
   wait->network.session = p->session;
   wait->network.fd = -1;
   wait->network.read = wait->network.write = wait->network.again = false;
   wait->encoder_retry = false;
   if (retro_atomic_load_acquire_int(&p->failed))
      return stream_cancel(p, wait);
   if (p->ended)
      return RECORD_GPU_OUTPUT_END;
   state = net_rtmp_client_step(p->client, receive_time, &wait->network);
   if (state == NET_RTMP_CLIENT_FAILED)
      return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_NETWORK);
   if (state == NET_RTMP_CLIENT_FINISHED)
   {
      if (!p->finishing)
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_NETWORK);
      p->ended = true;
      return RECORD_GPU_OUTPUT_END;
   }
   if (p->sending)
   {
      if (!net_rtmp_session_media_release(p->session))
         return wait->network.again ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
      p->sending = false;
      if (!rflv_mux_release(p->mux))
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
      return stream_return_producer(p);
   }
   result = rflv_mux_step(p->mux);
   if (result == RFLV_ERROR)
      return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
   if (result == RFLV_PROGRESS)
      return RECORD_GPU_OUTPUT_PROGRESS;
   if (result == RFLV_PACKET)
   {
      if (!rflv_mux_get_tag(p->mux, &tag))
         return rflv_mux_release(p->mux) ? RECORD_GPU_OUTPUT_PROGRESS
            : stream_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
      spans[0].data = tag.spans[0].data;
      spans[0].size = tag.spans[0].size;
      spans[1].data = tag.spans[1].data;
      spans[1].size = tag.spans[1].size;
      if (net_rtmp_session_media(p->session, tag.type, tag.timestamp, spans, 2))
      {
         p->sending = true;
         return RECORD_GPU_OUTPUT_PROGRESS;
      }
      return wait->network.again ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
   }
   if (p->finishing)
      return wait->network.again ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
   if (p->video_end && p->audio_end)
   {
      if (!rflv_mux_finish(p->mux))
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
      if (!net_rtmp_session_finish(p->session))
         return wait->network.again ? RECORD_GPU_OUTPUT_PROGRESS : RECORD_GPU_OUTPUT_BUSY;
      p->finishing = true;
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   if (p->audio_turn)
   {
      audio_attempted = true;
      result = stream_audio(p);
      if (result != RECORD_GPU_OUTPUT_BUSY)
         return result;
   }
   if (!p->video_end)
   {
      result = record_gpu_worker_d3d12_step(p->worker, &packet);
      if (result == RECORD_GPU_WORKER_ERROR)
         return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_VIDEO);
      if (result == RECORD_GPU_WORKER_PACKET)
      {
         p->producer = STREAM_VIDEO;
         if (!packet.duration || (p->av1
               ? rflv_mux_push(p->mux, 1, packet.data, packet.size,
                  packet.timestamp, packet.timestamp, packet.keyframe)
               : rflv_mux_push_annexb(p->mux, packet.data, packet.size,
                  packet.timestamp, packet.timestamp, packet.keyframe)) != RFLV_PROGRESS)
            return stream_fail(p, RECORD_GPU_OUTPUT_ERROR_MUX);
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
   if (!audio_attempted)
      result = stream_audio(p);
   if (result == RECORD_GPU_OUTPUT_BUSY)
   {
      wait->encoder_retry = p->prepared || (p->audio_finishing && !p->audio_end)
         || (!p->video_end && record_gpu_worker_d3d12_retry_needed(p->worker));
      if (wait->network.again)
         return RECORD_GPU_OUTPUT_PROGRESS;
   }
   return result;
}

void record_stream_d3d12_abort(record_stream_d3d12_t *p)
{
   if (p)
      stream_fail(p, RECORD_GPU_OUTPUT_ERROR_ABORTED);
}

enum record_gpu_output_error record_stream_d3d12_error(const record_stream_d3d12_t *p)
{
   return p ? (enum record_gpu_output_error)retro_atomic_load_acquire_int(&p->failed)
      : RECORD_GPU_OUTPUT_ERROR_NETWORK;
}

bool record_stream_d3d12_free(record_stream_d3d12_t *p)
{
   if (!p)
      return true;
   if (!retro_atomic_load_acquire_int(&p->audio_closed) || p->sending || p->input
         || p->producer != STREAM_NONE
         || (retro_atomic_load_acquire_int(&p->failed) && !p->retired)
         || (!p->ended && !retro_atomic_load_acquire_int(&p->failed)))
      return false;
   if (!record_mf_audio_free(p->aac))
      return false;
   rflv_mux_free(p->mux);
   retro_spsc_free(&p->audio);
   free(p);
   return true;
}
