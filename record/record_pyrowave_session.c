/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_session.h"
#include <stdlib.h>
#include <string.h>

struct record_pyrowave_session
{
   rpyrowave_stream_t *stream;
   record_pyrowave_video_t backend;
   record_pyrowave_pcm_t *pcm;
   record_pyrowave_session_status_t status;
   record_gpu_frame_t video;
   record_pyrowave_pcm_view_t audio;
   void *incoming[2];
   bool incoming_active[2];
   unsigned local_reason;
   bool sender, video_held, audio_held, video_offered, audio_offered;
   bool video_end, audio_end, end_sent, failed;
};

static void record_pyrowave_session_fail(record_pyrowave_session_t *s, unsigned reason)
{
   if (!s->failed) s->local_reason = reason;
   s->failed = true;
}

static bool record_pyrowave_session_video_valid(const record_pyrowave_video_t *v, bool sender)
{
   return v && v->user && (sender ? v->take && v->packet && v->release
         : v->acquire && v->submit && v->cancel && v->finish);
}

static bool record_pyrowave_session_pcm_valid(const rpyrowave_stream_config_t *cfg, record_pyrowave_pcm_t *pcm)
{
   return cfg->audio_rate ? record_pyrowave_pcm_compatible(pcm, cfg->audio_channels, cfg->maximum_audio_frames) : !pcm;
}

static int record_pyrowave_session_acquire(void *user, const rpyrowave_stream_record_t *r, void **data, size_t *capacity)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   unsigned index = r->kind == RPYROWAVE_STREAM_VIDEO ? 0 : 1;
   int result;
   if (s->incoming_active[index] || s->failed) return -1;
   result = !index ? s->backend.acquire(s->backend.user, data, capacity)
      : record_pyrowave_pcm_acquire(s->pcm, data, capacity);
   if (result == RECORD_GPU_QUEUE_BUSY) return 0;
   if (result != RECORD_GPU_QUEUE_OK) return -1;
   s->incoming[index] = *data; s->incoming_active[index] = true;
   if (!*data || *capacity < r->bytes)
   {
      bool cancelled = !index ? s->backend.cancel(s->backend.user) : record_pyrowave_pcm_cancel(s->pcm);
      if (cancelled)
      {
         s->incoming[index] = NULL; s->incoming_active[index] = false;
      }
      else record_pyrowave_session_fail(s, index ? RECORD_PYROWAVE_PCM_ERROR : RECORD_PYROWAVE_VIDEO_ERROR);
      return -1;
   }
   return 1;
}

static int record_pyrowave_session_commit(void *user, const rpyrowave_stream_record_t *r, void *data)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   unsigned index = r->kind == RPYROWAVE_STREAM_VIDEO ? 0 : 1;
   bool result;
   if (!s->incoming_active[index] || data != s->incoming[index] || !data) return 0;
   result = !index ? s->backend.submit(s->backend.user, r->bytes, r->timestamp, r->duration) == RECORD_GPU_QUEUE_OK
      : record_pyrowave_pcm_submit(s->pcm, r->serial, r->bytes / (s->status.config.audio_channels * 2));
   if (!result) return 0;
   s->incoming[index] = NULL; s->incoming_active[index] = false;
   if (!index) s->status.video_received++;
   else s->status.audio_received++;
   return 1;
}

static void record_pyrowave_session_cancel(void *user, const rpyrowave_stream_record_t *r, void *data)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   unsigned index = r->kind == RPYROWAVE_STREAM_VIDEO ? 0 : 1;
   bool result;
   (void)data;
   if (!s->incoming_active[index]) return;
   result = !index ? s->backend.cancel(s->backend.user) : record_pyrowave_pcm_cancel(s->pcm);
   if (result)
   {
      s->incoming[index] = NULL; s->incoming_active[index] = false;
   }
   else record_pyrowave_session_fail(s, index ? RECORD_PYROWAVE_PCM_ERROR : RECORD_PYROWAVE_VIDEO_ERROR);
}

static void record_pyrowave_session_dropped(void *user, const rpyrowave_stream_record_t *r)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   if (r->kind == RPYROWAVE_STREAM_VIDEO) s->status.video_dropped++;
   else s->status.audio_dropped++;
}

static void record_pyrowave_session_sent(void *user, const rpyrowave_stream_record_t *r,
      const void *data, void *token, int sent)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   bool result;
   (void)data;
   if (r->kind == RPYROWAVE_STREAM_VIDEO && token == &s->video && s->video_offered)
   {
      s->video_offered = false;
      result = s->backend.release(s->backend.user, s->video.slot);
      if (result) s->video_held = false;
      else record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
      if (sent) s->status.video_sent++;
   }
   else if (r->kind == RPYROWAVE_STREAM_AUDIO && token == &s->audio && s->audio_offered)
   {
      s->audio_offered = false;
      result = record_pyrowave_pcm_release(s->pcm, s->audio.slot);
      if (result) s->audio_held = false;
      else record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
      if (sent) s->status.audio_sent++;
   }
   else record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
}

static void record_pyrowave_session_event(void *user, unsigned event,
      const rpyrowave_stream_config_t *cfg, uint64_t generation, unsigned reason)
{
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)user;
   if (event == RPYROWAVE_STREAM_CONFIGURE)
   {
      s->status.config = *cfg; s->status.generation = generation;
      s->status.state = RECORD_PYROWAVE_CONFIGURING;
   }
   else if (event == RPYROWAVE_STREAM_STARTED) s->status.state = RECORD_PYROWAVE_ACTIVE;
   else
   {
      if (event == RPYROWAVE_STREAM_ENDED) s->status.state = RECORD_PYROWAVE_ENDED;
      else if (event == RPYROWAVE_STREAM_CANCELLED) s->status.state = RECORD_PYROWAVE_CANCELLED;
      else if (event == RPYROWAVE_STREAM_REJECTED) s->status.state = RECORD_PYROWAVE_REJECTED;
      else s->status.state = RECORD_PYROWAVE_FAILED;
      s->status.reason = reason;
      if (!s->sender && s->backend.user)
      {
         if (!s->backend.finish(s->backend.user)) record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
         if (s->pcm && !record_pyrowave_pcm_finish(s->pcm)) record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
      }
   }
}

static record_pyrowave_session_t *record_pyrowave_session_new(const rpyrowave_stream_io_t *io,
      const rpyrowave_stream_config_t *cfg, uint64_t generation)
{
   rpyrowave_stream_callbacks_t callbacks;
   record_pyrowave_session_t *s = (record_pyrowave_session_t*)calloc(1, sizeof(*s));
   if (!s) return NULL;
   memset(&callbacks, 0, sizeof(callbacks)); callbacks.user = s;
   callbacks.acquire = record_pyrowave_session_acquire; callbacks.commit = record_pyrowave_session_commit;
   callbacks.cancel = record_pyrowave_session_cancel; callbacks.dropped = record_pyrowave_session_dropped;
   callbacks.sent = record_pyrowave_session_sent; callbacks.event = record_pyrowave_session_event;
   s->sender = cfg != NULL;
   s->stream = cfg ? rpyrowave_stream_sender_new(cfg, generation, io, &callbacks)
      : rpyrowave_stream_receiver_new(io, &callbacks);
   if (!s->stream)
   {
      free(s);
      return NULL;
   }
   if (cfg) s->status.config = *cfg;
   s->status.generation = generation;
   s->audio_end = cfg && !cfg->audio_rate;
   return s;
}

record_pyrowave_session_t *record_pyrowave_session_sender_new(
      const rpyrowave_stream_config_t *cfg, uint64_t generation, const rpyrowave_stream_io_t *io,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   record_pyrowave_session_t *s;
   if (!cfg || rpyrowave_stream_config_valid(cfg) || !record_pyrowave_session_video_valid(video, true)
         || !record_pyrowave_session_pcm_valid(cfg, pcm)) return NULL;
   s = record_pyrowave_session_new(io, cfg, generation);
   if (s)
   {
      s->backend = *video; s->pcm = pcm;
   }
   return s;
}

record_pyrowave_session_t *record_pyrowave_session_receiver_new(const rpyrowave_stream_io_t *io)
{
   return record_pyrowave_session_new(io, NULL, 0);
}

bool record_pyrowave_session_accept(record_pyrowave_session_t *s,
      const record_pyrowave_video_t *video, record_pyrowave_pcm_t *pcm)
{
   if (!s || s->sender || s->status.state != RECORD_PYROWAVE_CONFIGURING
         || !record_pyrowave_session_video_valid(video, false) || !record_pyrowave_session_pcm_valid(&s->status.config, pcm)) return false;
   s->backend = *video; s->pcm = pcm;
   if (rpyrowave_stream_accept(s->stream, 0) != RPYROWAVE_STREAM_PROGRESS)
   {
      memset(&s->backend, 0, sizeof(s->backend)); s->pcm = NULL;
      return false;
   }
   s->status.state = RECORD_PYROWAVE_NEGOTIATING;
   return true;
}

bool record_pyrowave_session_reject(record_pyrowave_session_t *s, unsigned reason)
{
   if (!s || s->sender || s->status.state != RECORD_PYROWAVE_CONFIGURING || !reason
         || rpyrowave_stream_accept(s->stream, reason) != RPYROWAVE_STREAM_PROGRESS) return false;
   s->status.state = RECORD_PYROWAVE_NEGOTIATING;
   return true;
}

void record_pyrowave_session_status(const record_pyrowave_session_t *s, record_pyrowave_session_status_t *status)
{
   if (s && status) *status = s->status;
}

static void record_pyrowave_session_retire(record_pyrowave_session_t *s)
{
   if (s->video_held && !s->video_offered)
   {
      record_pyrowave_packet_t packet;
      if (s->backend.packet(s->backend.user, s->video.slot, &packet) != RECORD_GPU_QUEUE_BUSY)
      {
         if (s->backend.release(s->backend.user, s->video.slot)) s->video_held = false;
         else record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
      }
   }
   if (s->audio_held && !s->audio_offered)
   {
      if (record_pyrowave_pcm_release(s->pcm, s->audio.slot)) s->audio_held = false;
      else record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
   }
}

static void record_pyrowave_session_audio(record_pyrowave_session_t *s)
{
   rpyrowave_stream_record_t record;
   uint64_t end;
   int result;
   if (s->audio_offered || s->audio_end || s->failed) return;
   if (!s->audio_held)
   {
      result = record_pyrowave_pcm_take(s->pcm, &s->audio);
      if (result == RECORD_GPU_QUEUE_END) s->audio_end = true;
      else if (result == RECORD_GPU_QUEUE_ERROR) record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
      if (result != RECORD_GPU_QUEUE_OK) return;
      s->audio_held = true;
   }
   memset(&record, 0, sizeof(record));
   record.kind = RPYROWAVE_STREAM_AUDIO; record.generation = s->status.generation;
   record.serial = s->audio.first_sample;
   if (s->audio.frames > s->status.config.maximum_audio_frames
         || record.serial > (uint64_t)-1 - s->audio.frames
         || rpyrowave_stream_time(record.serial, s->status.config.audio_rate, 1, &record.timestamp)
         || rpyrowave_stream_time(record.serial + s->audio.frames, s->status.config.audio_rate, 1, &end))
   {
      record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
      return;
   }
   record.duration = end - record.timestamp;
   record.bytes = (uint32_t)(s->audio.frames * s->status.config.audio_channels * 2);
   result = rpyrowave_stream_offer(s->stream, &record, s->audio.data, &s->audio);
   if (result == RPYROWAVE_STREAM_PROGRESS) s->audio_offered = true;
   else if (result != RPYROWAVE_STREAM_BUSY) record_pyrowave_session_fail(s, RECORD_PYROWAVE_PCM_ERROR);
}

static void record_pyrowave_session_video(record_pyrowave_session_t *s)
{
   record_pyrowave_packet_t packet;
   rpyrowave_stream_record_t record;
   int result;
   if (s->video_offered || s->video_end || s->failed) return;
   if (!s->video_held)
   {
      result = s->backend.take(s->backend.user, &s->video);
      if (result == RECORD_GPU_QUEUE_END) s->video_end = true;
      else if (result == RECORD_GPU_QUEUE_ERROR) record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
      if (result != RECORD_GPU_QUEUE_OK) return;
      s->video_held = true;
   }
   result = s->backend.packet(s->backend.user, s->video.slot, &packet);
   if (result == RECORD_GPU_QUEUE_BUSY) return;
   if (result != RECORD_GPU_QUEUE_OK)
   {
      if (s->backend.release(s->backend.user, s->video.slot))
      {
         s->video_held = false; s->status.video_dropped++;
      }
      else record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
      return;
   }
   memset(&record, 0, sizeof(record));
   record.generation = s->status.generation; record.serial = s->video.ready_value;
   record.timestamp = s->video.timestamp; record.duration = s->video.duration;
   record.kind = RPYROWAVE_STREAM_VIDEO;
   if (!packet.data || packet.bytes > s->status.config.maximum_frame_bytes)
   {
      record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
      return;
   }
   record.bytes = (uint32_t)packet.bytes;
   result = rpyrowave_stream_offer(s->stream, &record, packet.data, &s->video);
   if (result == RPYROWAVE_STREAM_PROGRESS) s->video_offered = true;
   else if (result != RPYROWAVE_STREAM_BUSY) record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
}

static void record_pyrowave_session_offer_pending(record_pyrowave_session_t *s)
{
   if (s->sender && (s->status.state == RECORD_PYROWAVE_ACTIVE || s->status.state == RECORD_PYROWAVE_DRAINING))
   {
      record_pyrowave_session_audio(s); record_pyrowave_session_video(s);
      if (s->status.state == RECORD_PYROWAVE_DRAINING && s->video_end && s->audio_end
            && !s->video_held && !s->audio_held && !s->end_sent)
      {
         if (rpyrowave_stream_finish(s->stream) != RPYROWAVE_STREAM_PROGRESS)
            record_pyrowave_session_fail(s, RECORD_PYROWAVE_VIDEO_ERROR);
         else s->end_sent = true;
      }
   }
}

int record_pyrowave_session_pump(record_pyrowave_session_t *s)
{
   int result;
   if (!s) return RPYROWAVE_STREAM_ERROR;
   if (s->status.state >= RECORD_PYROWAVE_ENDED)
   {
      record_pyrowave_session_retire(s);
      if (s->failed)
      {
         s->status.state = RECORD_PYROWAVE_FAILED; s->status.reason = s->local_reason;
      }
      return s->video_held || s->audio_held ? RPYROWAVE_STREAM_BUSY : RPYROWAVE_STREAM_FINISHED;
   }
   record_pyrowave_session_offer_pending(s);
   result = rpyrowave_stream_pump(s->stream);
   record_pyrowave_session_offer_pending(s);
   if (s->failed)
   {
      unsigned reason = s->local_reason;
      rpyrowave_stream_abort(s->stream);
      s->status.state = RECORD_PYROWAVE_FAILED; s->status.reason = reason;
      record_pyrowave_session_retire(s);
      return RPYROWAVE_STREAM_ERROR;
   }
   return result;
}

bool record_pyrowave_session_finish(record_pyrowave_session_t *s)
{
   if (!s || !s->sender || (s->status.state != RECORD_PYROWAVE_ACTIVE && s->status.state != RECORD_PYROWAVE_DRAINING)) return false;
   s->status.state = RECORD_PYROWAVE_DRAINING;
   return true;
}

bool record_pyrowave_session_wants_write(const record_pyrowave_session_t *s)
{
   return s && s->status.state < RECORD_PYROWAVE_ENDED && rpyrowave_stream_wants_write(s->stream);
}

void record_pyrowave_session_abort(record_pyrowave_session_t *s)
{
   if (!s) return;
   rpyrowave_stream_abort(s->stream);
   record_pyrowave_session_retire(s);
   if (s->failed)
   {
      s->status.state = RECORD_PYROWAVE_FAILED; s->status.reason = s->local_reason;
   }
}

bool record_pyrowave_session_abandon_video(record_pyrowave_session_t *s)
{
   if (!s || s->status.state < RECORD_PYROWAVE_ENDED || s->video_offered) return false;
   s->video_held = false;
   s->incoming[0] = NULL;
   s->incoming_active[0] = false;
   memset(&s->backend, 0, sizeof(s->backend));
   return true;
}

bool record_pyrowave_session_free(record_pyrowave_session_t *s)
{
   if (!s) return true;
   record_pyrowave_session_abort(s);
   if (s->video_held || s->audio_held || s->incoming_active[0] || s->incoming_active[1]) return false;
   rpyrowave_stream_free(s->stream);
   free(s);
   return true;
}
