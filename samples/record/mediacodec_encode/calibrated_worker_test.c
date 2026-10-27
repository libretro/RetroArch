/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_mediacodec_worker.h"

struct record_mediacodec_encoder
{
   uint64_t accepted;
   unsigned packets, calls;
   bool audio, format, eos, held;
};
struct record_mux_mediacodec
{
   unsigned packets[2], ends[2];
   bool format[2], finished;
};
struct record_aac_probe
{
   unsigned packets, steps;
   bool configured, ready;
};
static int16_t prefix[RECORD_AAC_PROBE_FRAMES];
static const unsigned char config[] = {0x11, 0x88};
static const unsigned char encoded = 1;
static unsigned fault;
static bool empty;

const int16_t *record_aac_probe_input(record_aac_probe_t *p,
      size_t at, size_t *frames)
{
   assert(p && at < RECORD_AAC_PROBE_FRAMES);
   *frames = RECORD_AAC_PROBE_FRAMES - at;
   return prefix + at;
}
bool record_aac_probe_config(record_aac_probe_t *p,
      const void *data, size_t bytes)
{
   assert(!p->configured && data == config && bytes == sizeof(config));
   p->configured = true;
   return fault != 3;
}
bool record_aac_probe_packet(record_aac_probe_t *p,
      const void *data, size_t bytes)
{
   assert(p->configured && data == &encoded && bytes == 1);
   p->packets++;
   return true;
}
int record_aac_probe_step(record_aac_probe_t *p)
{
   if (fault == 4)
      return -1;
   if (p->packets < 8)
      return 0;
   if (++p->steps < 3)
      return 1;
   p->ready = true;
   return 2;
}
bool record_aac_probe_delay(const record_aac_probe_t *p, unsigned *delay)
{
   if (!p->ready)
      return false;
   *delay = fault == 7 ? 4096 : 2048;
   return true;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *e,
      const void **data, size_t *bytes)
{
   assert(e->audio && e->format);
   *data = config;
   *bytes = sizeof(config);
   return true;
}
int record_mediacodec_encoder_audio_submit(record_mediacodec_encoder_t *e,
      const int16_t *samples, size_t frames, uint64_t timestamp,
      size_t *accepted)
{
   size_t count = frames > 317 ? 317 : frames;
   size_t i;
   assert(e->audio && !e->eos && timestamp == e->accepted);
   *accepted = 0;
   if (++e->calls % 3 == 0)
      return 0;
   if (timestamp < RECORD_AAC_PROBE_FRAMES)
      assert(samples == prefix + timestamp);
   else
      for (i = 0; i < count; i++)
         assert(samples[i] == 7);
   *accepted = fault == 2 ? frames + 1 : count;
   e->accepted += count;
   return 1;
}
int record_mediacodec_encoder_audio_finish(record_mediacodec_encoder_t *e,
      uint64_t end)
{
   assert(e->audio && end == RECORD_AAC_PROBE_FRAMES + (empty ? 0 : 17)
         && e->accepted == end);
   e->eos = true;
   return 1;
}
bool record_mediacodec_encoder_finish(record_mediacodec_encoder_t *e)
{
   assert(!e->audio);
   e->eos = true;
   return true;
}
int record_mediacodec_encoder_pump(record_mediacodec_encoder_t *e,
      record_mediacodec_packet_t *packet)
{
   unsigned limit;
   if (!e->format)
   {
      e->format = true;
      return RECORD_MC_FORMAT;
   }
   limit = e->audio ? (unsigned)(e->accepted / 1024) : 1;
   if (e->audio && e->eos)
      limit = fault == 5 ? 7 : empty ? 8 : 11;
   if (e->packets < limit)
   {
      e->held = true;
      packet->data = &encoded;
      packet->size = 1;
      packet->pts_us = e->audio ? -99 : 0;
      packet->flags = 1;
      return RECORD_MC_PACKET;
   }
   return e->eos ? RECORD_MC_END : RECORD_MC_BUSY;
}
void *record_mediacodec_encoder_format(record_mediacodec_encoder_t *e)
{
   assert(e->format);
   return e;
}
bool record_mediacodec_encoder_release(record_mediacodec_encoder_t *e)
{
   assert(e->held);
   e->held = false;
   e->packets++;
   return true;
}
bool record_mux_mediacodec_track(record_mux_mediacodec_t *m,
      bool audio, void *format)
{
   assert(format && !m->format[audio]);
   m->format[audio] = true;
   return true;
}
int record_mux_mediacodec_start(record_mux_mediacodec_t *m)
{
   return m->format[0] && m->format[1];
}
bool record_mux_mediacodec_write(record_mux_mediacodec_t *m, bool audio,
      const record_mediacodec_packet_t *p)
{
   assert(p->data == &encoded && p->size == 1);
   assert(p->pts_us == (audio
               ? (int64_t)((uint64_t)m->packets[1] * 1024 * 1000000 / 48000)
               : 0));
   m->packets[audio]++;
   return true;
}
bool record_mux_mediacodec_end(record_mux_mediacodec_t *m,
      bool audio, int64_t end)
{
   assert(end == (audio ? (empty ? 170666 : 234666) : 1000000));
   m->ends[audio]++;
   return true;
}
int record_mux_mediacodec_finish(record_mux_mediacodec_t *m)
{
   assert(m->ends[0] && m->ends[1]);
   m->finished = true;
   return 1;
}
static void exercise(unsigned failure, bool cancel, bool no_pcm)
{
   record_mediacodec_encoder_t video, audio;
   record_mux_mediacodec_t mux;
   record_aac_probe_t probe;
   record_mediacodec_worker_t *w;
   record_audio_queue_t *q;
   int16_t pcm[17];
   const int16_t *samples;
   size_t frames;
   uint64_t timestamp, skip = 0, source = 0;
   unsigned i, roll = 0;
   int result = 0;
   memset(&video, 0, sizeof(video));
   memset(&audio, 0, sizeof(audio));
   memset(&mux, 0, sizeof(mux));
   memset(&probe, 0, sizeof(probe));
   audio.audio = true;
   fault = failure;
   empty = no_pcm;
   for (i = 0; i < 17; i++)
      pcm[i] = 7;
   q = record_audio_queue_new(1, 17, 256);
   assert(q);
   if (!empty)
      assert(record_audio_queue_push(q, pcm, 17, 0) == RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(q));
   w = record_mediacodec_worker_new(&video, &audio, q, &mux, 1, 48000);
   assert(w && record_mediacodec_worker_calibrate(w, &probe));
   assert(!record_mediacodec_worker_calibrate(w, &probe));
   assert(!record_mediacodec_worker_audio_edit(w, &skip, &source, &roll));
   assert(!record_mediacodec_worker_finish(w, 1000000, (uint64_t)-1));
   assert(record_mediacodec_worker_finish(w, 1000000, empty ? 0 : 17));
   assert(record_mediacodec_worker_finish(w, 1000000, empty ? 0 : 17));
   for (i = 0; i < 256; i++)
   {
      if (cancel && i == 2)
         record_mediacodec_worker_abort(w);
      result = record_mediacodec_worker_step(w);
      assert(!record_mediacodec_worker_calibrate(w, &probe));
      if (result == RECORD_MC_WORKER_ERROR || result == RECORD_MC_WORKER_END)
         break;
   }
   assert(i < 256);
   if (failure || cancel)
   {
      assert(result == RECORD_MC_WORKER_ERROR && !mux.finished);
      assert(!record_mediacodec_worker_audio_edit(w, &skip, &source, &roll));
   }
   else
   {
      assert(result == RECORD_MC_WORKER_END && mux.finished);
      assert(record_mediacodec_worker_audio_edit(w, &skip, &source, &roll));
      assert(skip == 10240 && source == (empty ? 0 : 17) && roll == 2);
      assert(audio.accepted == (empty ? 8192 : 8209)
            && mux.packets[1] == (empty ? 8 : 11));
   }
   assert(record_mediacodec_worker_step(w) == result);
   assert(record_mediacodec_worker_free(w));
   while (record_audio_queue_peek(q, &samples, &frames, &timestamp)
         == RECORD_AUDIO_OK)
      assert(record_audio_queue_ack(q));
   assert(record_audio_queue_free(q));
}
int main(void)
{
   exercise(0, false, false);
   exercise(0, false, true);
   exercise(2, false, false);
   exercise(3, false, false);
   exercise(4, false, false);
   exercise(5, false, false);
   exercise(7, false, false);
   exercise(0, true, false);
   puts("PASS: calibrated worker prefix backpressure, coded/source clocks, "
        "trim ownership, early EOS and cancellation; codecs/probe mocked");
   return 0;
}
