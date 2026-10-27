/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_mediacodec_worker.h"

struct record_mediacodec_encoder
{
   unsigned delay, outputs, pumps, releases, inputs, finish_calls;
   unsigned channels, frames;
   bool audio, format, eos, borrowed;
   uint8_t data;
};
struct record_mux_mediacodec
{
   unsigned writes[2], ends[2], calls;
   bool format[2], audio, started, finished;
};
static unsigned fault;

int record_mediacodec_encoder_pump(
      record_mediacodec_encoder_t *e, record_mediacodec_packet_t *p)
{
   e->pumps++;
   if (fault == 1)
      return RECORD_MC_ERROR;
   if (fault == 11)
      return RECORD_MC_END;
   if (fault == 12)
      return RECORD_MC_PACKET;
   if (e->delay)
   {
      e->delay--;
      return RECORD_MC_BUSY;
   }
   if (!e->format)
   {
      e->format = true;
      return RECORD_MC_FORMAT;
   }
   if (e->outputs < 2 && (!e->audio || e->frames))
   {
      e->borrowed = true;
      p->pts_us = e->outputs * 1000;
      p->data = &e->data;
      p->size = 1;
      p->flags = 1;
      return RECORD_MC_PACKET;
   }
   return e->eos ? RECORD_MC_END : RECORD_MC_BUSY;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *e)
{
   assert(e->format);
   return e;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *e,
      const void **data, size_t *bytes)
{
   (void)e;
   (void)data;
   (void)bytes;
   assert(0);
   return false;
}
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *e)
{
   assert(e->borrowed);
   if (fault == 2)
      return false;
   e->borrowed = false;
   e->outputs++;
   e->releases++;
   return true;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *e)
{
   assert(!e->audio && !e->eos);
   e->finish_calls++;
   e->eos = true;
   return fault != 3;
}
int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *e,
      const int16_t *pcm, size_t frames, uint64_t timestamp, size_t *accepted)
{
   size_t i, count = frames > 2 ? 2 : frames;
   assert(e->audio && !e->eos && timestamp == 100 + e->frames);
   *accepted = 0;
   e->inputs++;
   if (fault == 4)
      return -1;
   if (fault == 13)
   {
      *accepted = frames + 1;
      return 1;
   }
   if (e->inputs % 3 == 1)
      return 0;
   for (i = 0; i < count * e->channels; i++)
      assert(pcm[i] == (int16_t)(e->frames * e->channels + i));
   e->frames += (unsigned)count;
   *accepted = count;
   return 1;
}
int record_mediacodec_encoder_audio_finish(
      record_mediacodec_encoder_t *e, uint64_t end_sample)
{
   assert(e->audio && !e->eos && e->frames == 8 && end_sample == 960);
   e->finish_calls++;
   if (fault == 5)
      return -1;
   if (e->finish_calls == 1)
      return 0;
   if (e->finish_calls == 2)
      return 2;
   e->eos = true;
   return 1;
}
bool record_mux_mediacodec_track(
      record_mux_mediacodec_t *m, bool audio, void *format)
{
   record_mediacodec_encoder_t *e = (record_mediacodec_encoder_t *)format;
   assert(e->audio == audio && !m->format[audio] && !m->started);
   m->calls++;
   m->format[audio] = true;
   return fault != 6;
}
int record_mux_mediacodec_start(record_mux_mediacodec_t *m)
{
   assert(!m->started);
   m->calls++;
   if (fault == 7)
      return -1;
   if (!m->format[0] || (m->audio && !m->format[1]))
      return 0;
   m->started = true;
   return 1;
}
bool record_mux_mediacodec_write(record_mux_mediacodec_t *m, bool audio,
      const record_mediacodec_packet_t *p)
{
   assert(m->started && !m->ends[audio] && p->data && p->size == 1);
   assert(p->pts_us == m->writes[audio] * 1000);
   m->calls++;
   m->writes[audio]++;
   return fault != 8;
}
bool record_mux_mediacodec_end(
      record_mux_mediacodec_t *m, bool audio, int64_t end_us)
{
   assert(m->started && !m->ends[audio] && m->writes[audio] == 2);
   assert(end_us == (audio ? 20000 : 33333));
   m->calls++;
   m->ends[audio]++;
   return fault != 9;
}
int record_mux_mediacodec_finish(record_mux_mediacodec_t *m)
{
   assert(m->ends[0] && (!m->audio || m->ends[1]) && !m->finished);
   m->calls++;
   if (fault == 10)
      return -1;
   m->finished = true;
   return 1;
}

static void exercise(unsigned video_delay, unsigned audio_delay,
      unsigned channels, unsigned failure, bool late_close)
{
   record_mediacodec_encoder_t video, audio;
   record_mux_mediacodec_t mux;
   record_audio_queue_t *q = NULL;
   record_mediacodec_worker_t *w;
   int16_t pcm[16];
   const int16_t *borrow;
   size_t frames;
   uint64_t timestamp;
   unsigned i, calls;
   int result = 0;
   memset(&video, 0, sizeof(video));
   memset(&audio, 0, sizeof(audio));
   memset(&mux, 0, sizeof(mux));
   video.delay = video_delay;
   audio.delay = audio_delay;
   audio.audio = true;
   audio.channels = channels;
   mux.audio = channels != 0;
   fault = failure;
   if (channels)
   {
      q = record_audio_queue_new(channels, 8, 256);
      assert(q);
      for (i = 0; i < 8 * channels; i++)
         pcm[i] = (int16_t)i;
      assert(record_audio_queue_push(q, pcm, 8, 100) == RECORD_AUDIO_OK);
      assert(record_audio_queue_finish(q));
   }
   w = record_mediacodec_worker_new(&video, channels ? &audio : NULL,
         q, &mux, channels, 48000);
   assert(w);
   assert(!record_mediacodec_worker_finish(w, -1, 960));
   if (!late_close)
   {
      assert(record_mediacodec_worker_finish(w, 33333, 960));
      assert(record_mediacodec_worker_finish(w, 33333, 960));
      assert(!record_mediacodec_worker_finish(w, 33334, 960));
   }
   for (i = 0; i < 128; i++)
   {
      if (late_close && i == 16)
         assert(record_mediacodec_worker_finish(w, 33333, 960));
      result = record_mediacodec_worker_step(w);
      if (result == RECORD_MC_WORKER_END || result == RECORD_MC_WORKER_ERROR)
         break;
      assert(!record_mediacodec_worker_free(w));
   }
   assert(i < 128);
   assert(result == (failure ? RECORD_MC_WORKER_ERROR : RECORD_MC_WORKER_END));
   calls = mux.calls + video.pumps + audio.pumps;
   assert(record_mediacodec_worker_step(w) == result);
   assert(calls == mux.calls + video.pumps + audio.pumps);
   if (!failure)
   {
      assert(mux.finished && video.releases == 2 && video.finish_calls == 1);
      if (channels)
         assert(audio.frames == 8 && audio.releases == 2
               && audio.finish_calls == 3);
   }
   assert(record_mediacodec_worker_free(w));
   if (q)
   {
      while (record_audio_queue_peek(q, &borrow, &frames, &timestamp)
            == RECORD_AUDIO_OK)
         assert(record_audio_queue_ack(q));
      assert(record_audio_queue_free(q));
   }
}

static void contracts(void)
{
   record_mediacodec_encoder_t video, audio;
   record_mux_mediacodec_t mux;
   record_mediacodec_worker_t *w;
   record_audio_queue_t *q;
   int16_t pcm[8];
   memset(&video, 0, sizeof(video));
   memset(&audio, 0, sizeof(audio));
   memset(&mux, 0, sizeof(mux));
   memset(pcm, 0, sizeof(pcm));
   fault = 0;
   audio.audio = true;
   q = record_audio_queue_new(1, 8, 256);
   assert(q);
   assert(!record_mediacodec_worker_new(NULL, NULL, NULL, NULL, 0, 0));
   assert(!record_mediacodec_worker_new(&video, &audio, NULL, &mux, 1, 48000));
   assert(!record_mediacodec_worker_new(&video, NULL, q, &mux, 1, 48000));
   assert(!record_mediacodec_worker_new(&video, &audio, q, &mux, 3, 48000));
   assert(!record_mediacodec_worker_new(&video, &audio, q, &mux, 1, 0));
   w = record_mediacodec_worker_new(&video, &audio, q, &mux, 1, 1);
   assert(w);
   assert(!record_mediacodec_worker_finish(w, 33333, (uint64_t)-1));
   assert(record_mediacodec_worker_free(w));
   w = record_mediacodec_worker_new(&video, NULL, NULL, &mux, 0, 0);
   assert(w);
   assert(record_mediacodec_worker_step(w) == RECORD_MC_WORKER_PROGRESS);
   assert(!record_mediacodec_worker_free(w));
   record_mediacodec_worker_abort(w);
   assert(record_mediacodec_worker_step(w) == RECORD_MC_WORKER_ERROR);
   assert(record_mediacodec_worker_free(w));
   assert(record_audio_queue_push(q, pcm, 8, 100) == RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(q));
   w = record_mediacodec_worker_new(&video, &audio, q, &mux, 1, 48000);
   assert(w && record_mediacodec_worker_finish(w, 33333, 107));
   assert(record_mediacodec_worker_step(w) == RECORD_MC_WORKER_ERROR);
   assert(!audio.inputs);
   assert(record_mediacodec_worker_free(w));
   assert(record_audio_queue_ack(q));
   assert(record_audio_queue_free(q));
}

int main(void)
{
   unsigned a, v, channels, failure;
   for (channels = 0; channels <= 2; channels++)
      for (v = 0; v < 8; v++)
         for (a = 0; a < 8; a++)
         {
            exercise(v, a, channels, 0, false);
            exercise(v, a, channels, 0, true);
         }
   for (failure = 1; failure <= 13; failure++)
      exercise(0, 4, 2, failure, false);
   contracts();
   assert(record_mediacodec_worker_free(NULL));
   record_mediacodec_worker_abort(NULL);
   puts("PASS: Android recording worker PCM backpressure, mux startup and drain");
   return 0;
}
