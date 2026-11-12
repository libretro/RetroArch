/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_vt.h"
#include "record_at_audio.h"
#include "record_vt_packet.h"
#include <formats/rflv_mux.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>

enum vt_stream_producer
{
   VT_STREAM_NONE, VT_STREAM_VIDEO, VT_STREAM_AUDIO
};

struct record_stream_vt
{
   retro_spsc_t audio;
   uint64_t audio_pts, video_epoch;
   record_gpu_worker_vt_t *worker;
   net_rtmp_client_t *client;
   net_rtmp_session_t *session;
   record_at_audio_t *aac;
   record_audio_queue_t *source;
   const int16_t *source_data;
   rflv_mux_t *mux;
   record_vt_packet_t *view;
   int16_t *input;
   size_t input_frames, staged, frame_bytes, fifo_bytes;
   size_t source_frames, source_at;
   retro_atomic_int_t audio_closed, failed;
   enum vt_stream_producer producer;
   unsigned audio_epoch;
   bool prepared, sending, video_end, audio_end, audio_turn;
   bool audio_finishing, finishing, ended, retired;
   bool view_live;
};

static int vt_stream_fail(record_stream_vt_t *p, enum record_stream_vt_error error)
{
   retro_atomic_cas_int(&p->failed, 0, (int)error);
   return RECORD_STREAM_VT_ERROR;
}

static record_stream_vt_t *vt_stream_new(record_gpu_worker_vt_t *worker,
      record_at_audio_t *audio, net_rtmp_client_t *client,
      const record_stream_vt_config_t *config, record_audio_queue_t *source)
{
   record_stream_vt_t *p;
   net_rtmp_client_view_t view;
   rflv_config_t mux;
   const uint8_t *asc;
   unsigned leading, trailing, index;
   uint64_t frames, epoch;
   bool hevc;
   if (!worker || !client || !config || !config->format || (source && !audio)
         || !config->video_timescale || config->video_timescale > 0x7fffffffU
         || (config->bit_depth != 8 && config->bit_depth != 10)
         || net_rtmp_client_step(client, 0, &view) != NET_RTMP_CLIENT_READY || !view.session)
      return NULL;
   p = (record_stream_vt_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->worker = worker;
   p->aac = audio;
   p->source = source;
   p->client = client;
   p->session = view.session;
   p->video_epoch = config->video_epoch_ticks;
   p->audio_epoch = config->audio_epoch_frames;
   retro_atomic_int_init(&p->audio_closed, audio ? 0 : 1);
   retro_atomic_int_init(&p->failed, 0);
   p->view = record_vt_packet_new(config->format, config->video_timescale, config->max_packet_size);
   if (!p->view || !record_vt_packet_config(p->view, &mux.avcc, &mux.avcc_size, &hevc)
         || (!hevc && config->bit_depth != 8))
      goto failed;
   mux.video_timescale = config->video_timescale;
   mux.max_packet_size = config->max_packet_size;
   mux.asc = NULL;
   mux.asc_size = 0;
   mux.audio_timescale = 0;
   if (audio)
   {
      if ((config->audio_rate != 44100 && config->audio_rate != 48000)
            || !config->channels || config->channels > 2
            || (source ? config->audio_fifo_frames
               || record_audio_queue_channels(source) != config->channels
               : config->audio_fifo_frames < 1024 || config->audio_fifo_frames > config->audio_rate * 2u)
            || !record_at_audio_config(audio, &mux.asc, &mux.asc_size) || mux.asc_size != 2
            || !record_at_audio_timing(audio, &leading, &trailing, &frames)
            || frames || config->audio_epoch_frames < leading)
         goto failed;
      index = config->audio_rate == 44100 ? 4 : 3;
      asc = (const uint8_t*)mux.asc;
      if (asc[0] != (uint8_t)(16 | (index >> 1))
            || asc[1] != (uint8_t)((index << 7) | (config->channels << 3)))
         goto failed;
      epoch = (uint64_t)config->audio_epoch_frames * config->video_timescale;
      if (config->video_epoch_ticks != epoch / config->audio_rate + (epoch % config->audio_rate != 0))
         goto failed;
      mux.audio_timescale = config->audio_rate;
      p->frame_bytes = config->channels * sizeof(int16_t);
      p->fifo_bytes = config->audio_fifo_frames * p->frame_bytes;
      if (!source && !retro_spsc_init(&p->audio, p->fifo_bytes))
         goto failed;
   }
   else
   {
      if (config->audio_rate || config->channels || config->audio_fifo_frames || config->audio_epoch_frames)
         goto failed;
      p->audio_end = true;
   }
   p->mux = hevc ? rflv_mux_new_hevc(&mux) : rflv_mux_new(&mux);
   if (!p->mux || (hevc && !rflv_mux_set_color(p->mux, &config->color, config->bit_depth)))
      goto failed;
   return p;
failed:
   record_vt_packet_free(p->view);
   rflv_mux_free(p->mux);
   retro_spsc_free(&p->audio);
   free(p);
   return NULL;
}

record_stream_vt_t *record_stream_vt_new(record_gpu_worker_vt_t *worker,
      record_at_audio_t *audio, net_rtmp_client_t *client, const record_stream_vt_config_t *config)
{
   return vt_stream_new(worker, audio, client, config, NULL);
}

record_stream_vt_t *record_stream_vt_new_queued(record_gpu_worker_vt_t *worker,
      record_at_audio_t *audio, net_rtmp_client_t *client,
      const record_stream_vt_config_t *config, record_audio_queue_t *source)
{
   return source ? vt_stream_new(worker, audio, client, config, source) : NULL;
}

int record_stream_vt_audio(record_stream_vt_t *p, const int16_t *samples, size_t frames)
{
   size_t bytes, used;
   if (!p || !p->aac || p->source || !samples || !frames
         || frames > p->fifo_bytes / p->frame_bytes
         || retro_atomic_load_acquire_int(&p->audio_closed)
         || retro_atomic_load_acquire_int(&p->failed))
      return RECORD_STREAM_VT_ERROR;
   bytes = frames * p->frame_bytes;
   used = p->audio.capacity - retro_spsc_write_avail(&p->audio);
   if (used > p->fifo_bytes || bytes > p->fifo_bytes - used)
      return RECORD_STREAM_VT_BUSY;
   return retro_spsc_write(&p->audio, samples, bytes) == bytes
      ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_ERROR;
}

bool record_stream_vt_audio_finish(record_stream_vt_t *p)
{
   if (!p || p->source)
      return false;
   retro_atomic_store_release_int(&p->audio_closed, 1);
   return true;
}

static int vt_stream_source(record_stream_vt_t *p, size_t *frames, int *closed)
{
   uint64_t timestamp;
   int result;
   *frames = 0;
   *closed = 0;
   if (!p->source_data)
   {
      result = record_audio_queue_peek_lend(p->source, &p->source_data,
            &p->source_frames, &timestamp);
      if (result == RECORD_AUDIO_END)
         *closed = 1;
      else if (result == RECORD_AUDIO_ERROR)
         return RECORD_STREAM_VT_ERROR;
      else if (result == RECORD_AUDIO_OK)
      {
         p->source_at = 0;
         if (p->audio_pts > (uint64_t)-1 - p->staged || timestamp != p->audio_pts + p->staged)
            return RECORD_STREAM_VT_ERROR;
      }
   }
   if (p->source_data)
      *frames = p->source_frames - p->source_at;
   return RECORD_STREAM_VT_PROGRESS;
}

static int vt_stream_audio(record_stream_vt_t *p)
{
   record_at_audio_packet_t packet;
   size_t frames, bytes;
   int result, closed;
   if (p->audio_end)
      return RECORD_STREAM_VT_BUSY;
   result = record_at_audio_pump(p->aac, &packet);
   if (result == RECORD_AT_AUDIO_PACKET)
   {
      p->producer = VT_STREAM_AUDIO;
      if (packet.timestamp < -(int64_t)p->audio_epoch
            || packet.timestamp > (int64_t)((~(uint64_t)0) >> 1) - p->audio_epoch || packet.frames != 1024
            || rflv_mux_push(p->mux, 2, packet.data, packet.size,
               (uint64_t)(packet.timestamp + p->audio_epoch), (uint64_t)(packet.timestamp + p->audio_epoch), false) != RFLV_PROGRESS)
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
      return RECORD_STREAM_VT_PROGRESS;
   }
   if (result == RECORD_AT_AUDIO_END)
   {
      p->audio_end = true;
      return RECORD_STREAM_VT_PROGRESS;
   }
   if (result == RECORD_AT_AUDIO_ERROR)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
   if (result == RECORD_AT_AUDIO_PROGRESS)
      return RECORD_STREAM_VT_PROGRESS;
   if (result != RECORD_AT_AUDIO_BUSY)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
   if (p->audio_finishing)
      return RECORD_STREAM_VT_BUSY;
   closed = 0;
   if (!p->prepared)
   {
      if (p->source)
      {
         if (vt_stream_source(p, &frames, &closed) == RECORD_STREAM_VT_ERROR)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
      }
      else
      {
         closed = retro_atomic_load_acquire_int(&p->audio_closed);
         frames = retro_spsc_read_avail(&p->audio) / p->frame_bytes;
      }
      if (!p->input && !frames)
      {
         if (!closed)
            return RECORD_STREAM_VT_BUSY;
         if (record_at_audio_finish(p->aac) != RECORD_AT_AUDIO_PROGRESS)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
         p->audio_finishing = true;
         return RECORD_STREAM_VT_PROGRESS;
      }
      if (!p->input)
      {
         result = record_at_audio_input(p->aac, &p->input, &p->input_frames);
         if (result == RECORD_AT_AUDIO_BUSY)
            return RECORD_STREAM_VT_BUSY;
         if (result != RECORD_AT_AUDIO_PROGRESS || !p->input_frames || p->input_frames > 1024)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
      }
      if (frames > p->input_frames - p->staged)
         frames = p->input_frames - p->staged;
      bytes = frames * p->frame_bytes;
      if (bytes)
      {
         if (p->source)
         {
            memcpy((uint8_t*)p->input + p->staged * p->frame_bytes,
                  (const uint8_t*)p->source_data + p->source_at * p->frame_bytes, bytes);
            p->source_at += frames;
            if (p->source_at == p->source_frames)
            {
               if (!record_audio_queue_ack(p->source))
                  return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
               p->source_data = NULL;
            }
         }
         else if (retro_spsc_read(&p->audio,
                  (uint8_t*)p->input + p->staged * p->frame_bytes, bytes) != bytes)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
      }
      p->staged += frames;
      if (p->staged < p->input_frames && !closed)
         return frames ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_BUSY;
   }
   if (!p->staged || p->audio_pts > (uint64_t)-1 - p->staged)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
   result = record_at_audio_submit(p->aac, p->staged, p->audio_pts);
   p->prepared = true;
   if (result == RECORD_AT_AUDIO_BUSY)
      return RECORD_STREAM_VT_BUSY;
   if (result != RECORD_AT_AUDIO_PROGRESS)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
   p->audio_pts += p->staged;
   p->input = NULL;
   p->staged = 0;
   p->prepared = false;
   return RECORD_STREAM_VT_PROGRESS;
}

static int vt_stream_return_producer(record_stream_vt_t *p)
{
   if (p->producer == VT_STREAM_VIDEO)
   {
      if (p->view_live)
      {
         if (!record_vt_packet_release(p->view))
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_VIDEO);
         p->view_live = false;
      }
      if (!record_gpu_worker_vt_release(p->worker))
      {
         p->producer = VT_STREAM_NONE;
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_VIDEO);
      }
      p->audio_turn = true;
   }
   else if (p->producer == VT_STREAM_AUDIO)
   {
      bool released = record_at_audio_release(p->aac);
      p->producer = VT_STREAM_NONE;
      if (!released)
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_AUDIO);
      p->audio_turn = false;
   }
   p->producer = VT_STREAM_NONE;
   return RECORD_STREAM_VT_PROGRESS;
}

static int vt_stream_cancel(record_stream_vt_t *p, record_stream_vt_wait_t *wait)
{
   net_rtmp_client_cancel(p->client);
   if (p->sending)
   {
      if (!net_rtmp_session_media_release(p->session))
      {
         wait->encoder_retry = true;
         return RECORD_STREAM_VT_BUSY;
      }
      p->sending = false;
   }
   rflv_mux_free(p->mux);
   p->mux = NULL;
   if (vt_stream_return_producer(p) == RECORD_STREAM_VT_ERROR && p->producer != VT_STREAM_NONE)
   {
      wait->encoder_retry = true;
      return RECORD_STREAM_VT_BUSY;
   }
   p->input = NULL;
   p->prepared = false;
   if (p->source_data)
   {
      if (!record_audio_queue_ack(p->source))
      {
         wait->encoder_retry = true;
         return RECORD_STREAM_VT_BUSY;
      }
      p->source_data = NULL;
   }
   p->retired = true;
   return RECORD_STREAM_VT_ERROR;
}

int record_stream_vt_step(record_stream_vt_t *p,
      uint32_t receive_time, record_stream_vt_wait_t *wait)
{
   net_rtmp_span_t spans[2];
   record_vt_packet_view_t packet;
   void *sample = NULL;
   rflv_tag_t tag;
   enum net_rtmp_client_state state;
   int result;
   bool audio_attempted = false;
   if (!p || !wait)
      return RECORD_STREAM_VT_ERROR;
   wait->network.session = p->session;
   wait->network.fd = -1;
   wait->network.read = wait->network.write = wait->network.again = false;
   wait->encoder_retry = false;
   if (retro_atomic_load_acquire_int(&p->failed))
      return vt_stream_cancel(p, wait);
   if (p->ended)
      return RECORD_STREAM_VT_END;
   state = net_rtmp_client_step(p->client, receive_time, &wait->network);
   if (state == NET_RTMP_CLIENT_FAILED)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_NETWORK);
   if (state == NET_RTMP_CLIENT_FINISHED)
   {
      if (!p->finishing)
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_NETWORK);
      p->ended = true;
      return RECORD_STREAM_VT_END;
   }
   if (p->sending)
   {
      if (!net_rtmp_session_media_release(p->session))
         return wait->network.again ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_BUSY;
      p->sending = false;
      if (!rflv_mux_release(p->mux))
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_MUX);
      return vt_stream_return_producer(p);
   }
   result = rflv_mux_step(p->mux);
   if (result == RFLV_ERROR)
      return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_MUX);
   if (result == RFLV_PROGRESS)
      return RECORD_STREAM_VT_PROGRESS;
   if (result == RFLV_PACKET)
   {
      if (!rflv_mux_get_tag(p->mux, &tag))
         return rflv_mux_release(p->mux) ? RECORD_STREAM_VT_PROGRESS
            : vt_stream_fail(p, RECORD_STREAM_VT_ERROR_MUX);
      spans[0].data = tag.spans[0].data;
      spans[0].size = tag.spans[0].size;
      spans[1].data = tag.spans[1].data;
      spans[1].size = tag.spans[1].size;
      if (net_rtmp_session_media(p->session, tag.type, tag.timestamp, spans, 2))
      {
         p->sending = true;
         return RECORD_STREAM_VT_PROGRESS;
      }
      return wait->network.again ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_BUSY;
   }
   if (p->finishing)
      return wait->network.again ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_BUSY;
   if (p->video_end && p->audio_end)
   {
      if (!rflv_mux_finish(p->mux))
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_MUX);
      if (!net_rtmp_session_finish(p->session))
         return wait->network.again ? RECORD_STREAM_VT_PROGRESS : RECORD_STREAM_VT_BUSY;
      p->finishing = true;
      return RECORD_STREAM_VT_PROGRESS;
   }
   if (p->audio_turn)
   {
      audio_attempted = true;
      result = vt_stream_audio(p);
      if (result != RECORD_STREAM_VT_BUSY)
         return result;
   }
   if (!p->video_end)
   {
      result = record_gpu_worker_vt_step(p->worker, &sample);
      if (result == RECORD_GPU_VT_ERROR)
         return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_VIDEO);
      if (result == RECORD_GPU_VT_PACKET)
      {
         p->producer = VT_STREAM_VIDEO;
         if (record_vt_packet_read(p->view, sample, &packet) != 1)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_VIDEO);
         p->view_live = true;
         if (packet.dts > ~(uint64_t)0 - p->video_epoch || packet.pts > ~(uint64_t)0 - p->video_epoch
               || rflv_mux_push(p->mux, 1, packet.data, packet.size,
                  packet.dts + p->video_epoch, packet.pts + p->video_epoch, packet.keyframe) != RFLV_PROGRESS)
            return vt_stream_fail(p, RECORD_STREAM_VT_ERROR_MUX);
         return RECORD_STREAM_VT_PROGRESS;
      }
      if (result == RECORD_GPU_VT_END)
         p->video_end = true;
      if (result == RECORD_GPU_VT_END || result == RECORD_GPU_VT_PROGRESS)
      {
         p->audio_turn = true;
         return RECORD_STREAM_VT_PROGRESS;
      }
   }
   if (!audio_attempted)
      result = vt_stream_audio(p);
   if (result == RECORD_STREAM_VT_BUSY)
   {
      wait->encoder_retry = p->prepared || (p->audio_finishing && !p->audio_end)
         || (!p->video_end && record_gpu_worker_vt_retry_needed(p->worker));
      if (wait->network.again)
         return RECORD_STREAM_VT_PROGRESS;
   }
   return result;
}

void record_stream_vt_abort(record_stream_vt_t *p)
{
   if (p)
      vt_stream_fail(p, RECORD_STREAM_VT_ERROR_ABORTED);
}

enum record_stream_vt_error record_stream_vt_error(const record_stream_vt_t *p)
{
   return p ? (enum record_stream_vt_error)retro_atomic_load_acquire_int(&p->failed)
      : RECORD_STREAM_VT_ERROR_NETWORK;
}

bool record_stream_vt_free(record_stream_vt_t *p)
{
   if (!p)
      return true;
   if ((!p->source && !retro_atomic_load_acquire_int(&p->audio_closed))
         || p->sending || p->input || p->source_data
         || p->producer != VT_STREAM_NONE
         || (retro_atomic_load_acquire_int(&p->failed) && !p->retired)
         || (!p->ended && !retro_atomic_load_acquire_int(&p->failed)))
      return false;
   if (!record_vt_packet_free(p->view))
      return false;
   rflv_mux_free(p->mux);
   retro_spsc_free(&p->audio);
   free(p);
   return true;
}
