/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_aac_audio.h"
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MF_AUDIO
#include "record_mf_audio.h"

record_aac_audio_t *record_aac_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   return (record_aac_audio_t*)record_mf_audio_new(rate, channels, bitrate);
}

int record_aac_audio_input(record_aac_audio_t *e, int16_t **samples, size_t *frames)
{
   return record_mf_audio_input((record_mf_audio_t*)e, samples, frames);
}

int record_aac_audio_submit(record_aac_audio_t *e, size_t frames, uint64_t index)
{
   return record_mf_audio_submit((record_mf_audio_t*)e, frames, index);
}

bool record_aac_audio_release(record_aac_audio_t *e)
{
   return record_mf_audio_release((record_mf_audio_t*)e);
}

bool record_aac_audio_config(record_aac_audio_t *e, const void **data, size_t *size)
{
   return record_mf_audio_config((record_mf_audio_t*)e, data, size);
}

int record_aac_audio_finish(record_aac_audio_t *e)
{
   return record_mf_audio_finish((record_mf_audio_t*)e);
}

bool record_aac_audio_cancel(record_aac_audio_t *e)
{
   return record_mf_audio_cancel((record_mf_audio_t*)e);
}

bool record_aac_audio_free(record_aac_audio_t *e)
{
   return record_mf_audio_free((record_mf_audio_t*)e);
}

uint32_t record_aac_audio_error(const record_aac_audio_t *e)
{
   return record_mf_audio_error((const record_mf_audio_t*)e);
}

int record_aac_audio_pump(record_aac_audio_t *e, record_aac_audio_packet_t *packet)
{
   return record_mf_audio_pump((record_mf_audio_t*)e, packet);
}

#elif defined(HAVE_RAAC)
#include <formats/raac_encode.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

struct record_aac_audio
{
   uint64_t next, pending_index;
   uintptr_t owner;
   raac_encoder_t *encoder;
   size_t bytes, pending_frames, used, fill;
   unsigned rate, channels;
   int16_t input[RAAC_ENCODE_FRAME * 2];
   uint8_t output[RAAC_ENCODE_MAX_FRAME], config[2];
   bool loan, packet, locked, primed, started, flushed, finishing, ended, cancelled, failed;
};

static bool record_aac_owner(const record_aac_audio_t *e)
{
   if (!e)
      return false;
#ifdef HAVE_THREADS
   return e->owner == sthread_get_current_thread_id();
#else
   return true;
#endif
}

static bool record_aac_time(uint64_t index, unsigned rate, int64_t *time)
{
   uint64_t whole = index / rate, part = index % rate * 10000000 / rate;
   if (whole > ((uint64_t)INT64_MAX - part) / 10000000)
      return false;
   *time = (int64_t)(whole * 10000000 + part);
   return true;
}

record_aac_audio_t *record_aac_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   record_aac_audio_t *e;
   raac_encoder_config_t config;
   if ((rate != 44100 && rate != 48000) || (channels != 1 && channels != 2)
         || (bitrate != 96000 && bitrate != 128000 && bitrate != 160000 && bitrate != 192000))
      return NULL;
   e = (record_aac_audio_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   config.sample_rate = e->rate = rate;
   config.channels = e->channels = channels;
   config.bitrate = bitrate;
   e->encoder = raac_encoder_new(&config);
   if (!e->encoder || raac_encoder_asc(e->encoder, e->config, sizeof(e->config)) != 2)
   {
      raac_encoder_free(e->encoder);
      free(e);
      return NULL;
   }
#ifdef HAVE_THREADS
   e->owner = sthread_get_current_thread_id();
#endif
   return e;
}

int record_aac_audio_input(record_aac_audio_t *e, int16_t **samples, size_t *frames)
{
   if (samples)
      *samples = NULL;
   if (frames)
      *frames = 0;
   if (!record_aac_owner(e) || !samples || !frames || e->failed || e->cancelled)
      return RECORD_AAC_AUDIO_ERROR;
   if (e->packet)
      return RECORD_AAC_AUDIO_BUSY;
   if (e->finishing)
      return RECORD_AAC_AUDIO_ERROR;
   e->loan = true;
   *samples = e->input + e->fill * e->channels;
   *frames = RAAC_ENCODE_FRAME - e->fill;
   return RECORD_AAC_AUDIO_PROGRESS;
}

static int record_aac_encode(record_aac_audio_t *e, bool tail)
{
   size_t frames = e->fill;
   if (frames && frames < RAAC_ENCODE_FRAME)
      memset(e->input + frames * e->channels, 0,
            (RAAC_ENCODE_FRAME - frames) * e->channels * sizeof(int16_t));
   e->bytes = raac_encoder_encode_s16(e->encoder, tail ? NULL : e->input,
         e->output, sizeof(e->output), false);
   if (!e->bytes)
   {
      e->failed = true;
      return RECORD_AAC_AUDIO_ERROR;
   }
   /* The first transform output precedes the source by one frame. */
   e->packet = e->primed;
   if (!e->primed)
      e->pending_frames = frames;
   e->primed = true;
   if (!tail)
      e->used = frames;
   e->fill = 0;
   return RECORD_AAC_AUDIO_PROGRESS;
}

int record_aac_audio_submit(record_aac_audio_t *e, size_t frames, uint64_t index)
{
   int64_t start, end;
   if (!record_aac_owner(e) || !e->loan || !frames || frames > RAAC_ENCODE_FRAME - e->fill
         || e->failed || e->cancelled || e->finishing
         || (e->started && index != e->next) || index > (uint64_t)-1 - frames
         || !record_aac_time(index, e->rate, &start)
         || !record_aac_time(index + frames, e->rate, &end)
         || !record_aac_time(index + RAAC_ENCODE_FRAME - e->fill, e->rate, &end))
      return RECORD_AAC_AUDIO_ERROR;
   if (e->packet)
      return RECORD_AAC_AUDIO_BUSY;
   if (!e->started)
      e->pending_index = index;
   e->started = true;
   e->loan = false;
   e->fill += frames;
   e->next = index + frames;
   return e->fill == RAAC_ENCODE_FRAME ? record_aac_encode(e, false) : RECORD_AAC_AUDIO_PROGRESS;
}

int record_aac_audio_pump(record_aac_audio_t *e, record_aac_audio_packet_t *packet)
{
   int64_t end;
   if (packet)
      memset(packet, 0, sizeof(*packet));
   if (!record_aac_owner(e) || !packet || e->failed || e->cancelled)
      return RECORD_AAC_AUDIO_ERROR;
   if (e->packet)
   {
      if (!record_aac_time(e->pending_index, e->rate, &packet->timestamp)
            || !record_aac_time(e->pending_index + RAAC_ENCODE_FRAME, e->rate, &end))
      {
         e->failed = true;
         return RECORD_AAC_AUDIO_ERROR;
      }
      e->locked = true;
      packet->duration = end - packet->timestamp;
      packet->data = e->output;
      packet->size = e->bytes;
      return RECORD_AAC_AUDIO_PACKET;
   }
   if (e->finishing)
   {
      if (e->fill)
         return record_aac_encode(e, false);
      if (e->primed && !e->flushed)
      {
         e->flushed = true;
         return record_aac_encode(e, true);
      }
      e->ended = true;
      return RECORD_AAC_AUDIO_END;
   }
   return RECORD_AAC_AUDIO_BUSY;
}

bool record_aac_audio_release(record_aac_audio_t *e)
{
   if (!record_aac_owner(e) || !e->locked)
      return false;
   e->locked = e->packet = false;
   e->pending_index += e->pending_frames;
   e->pending_frames = e->used;
   return true;
}

bool record_aac_audio_config(record_aac_audio_t *e, const void **data, size_t *size)
{
   if (data)
      *data = NULL;
   if (size)
      *size = 0;
   if (!record_aac_owner(e) || !data || !size || e->cancelled || e->failed)
      return false;
   *data = e->config;
   *size = sizeof(e->config);
   return true;
}

int record_aac_audio_finish(record_aac_audio_t *e)
{
   if (!record_aac_owner(e) || e->failed || e->cancelled)
      return RECORD_AAC_AUDIO_ERROR;
   if (e->packet || e->loan)
      return RECORD_AAC_AUDIO_BUSY;
   if (e->finishing)
      return RECORD_AAC_AUDIO_PROGRESS;
   e->finishing = true;
   return RECORD_AAC_AUDIO_PROGRESS;
}

bool record_aac_audio_cancel(record_aac_audio_t *e)
{
   if (!record_aac_owner(e) || e->locked)
      return false;
   e->packet = false;
   e->loan = false;
   e->cancelled = true;
   return true;
}

bool record_aac_audio_free(record_aac_audio_t *e)
{
   if (!e)
      return true;
   if (!record_aac_owner(e) || e->packet || e->loan || (!e->ended && !e->cancelled))
      return false;
   raac_encoder_free(e->encoder);
   free(e);
   return true;
}

uint32_t record_aac_audio_error(const record_aac_audio_t *e)
{
   return record_aac_owner(e) && e->failed ? 1 : 0;
}
#else
record_aac_audio_t *record_aac_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   (void)rate;
   (void)channels;
   (void)bitrate;
   return NULL;
}
int record_aac_audio_input(record_aac_audio_t *e, int16_t **samples, size_t *frames)
{
   (void)e;
   if (samples)
      *samples = NULL;
   if (frames)
      *frames = 0;
   return RECORD_AAC_AUDIO_ERROR;
}
int record_aac_audio_submit(record_aac_audio_t *e, size_t frames, uint64_t index)
{
   (void)e;
   (void)frames;
   (void)index;
   return RECORD_AAC_AUDIO_ERROR;
}
int record_aac_audio_pump(record_aac_audio_t *e, record_aac_audio_packet_t *packet)
{
   (void)e;
   if (packet)
      memset(packet, 0, sizeof(*packet));
   return RECORD_AAC_AUDIO_ERROR;
}
bool record_aac_audio_release(record_aac_audio_t *e)
{
   (void)e;
   return false;
}
bool record_aac_audio_config(record_aac_audio_t *e, const void **data, size_t *size)
{
   (void)e;
   if (data)
      *data = NULL;
   if (size)
      *size = 0;
   return false;
}
int record_aac_audio_finish(record_aac_audio_t *e)
{
   (void)e;
   return RECORD_AAC_AUDIO_ERROR;
}
bool record_aac_audio_cancel(record_aac_audio_t *e)
{
   (void)e;
   return false;
}
bool record_aac_audio_free(record_aac_audio_t *e)
{
   return e == NULL;
}
uint32_t record_aac_audio_error(const record_aac_audio_t *e)
{
   (void)e;
   return 0;
}
#endif
