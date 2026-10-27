/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_aac_probe.h"
#include <stdlib.h>
#ifdef HAVE_RAAC
#include <formats/raac.h>
#include <math.h>
#define PROBE_BURST 2048u
#define PROBE_PACKET 1024u
#define PROBE_DELAY 4096u
struct record_aac_probe
{
   double energy, best, second;
   raac_t *decoder;
   float output[RECORD_AAC_PROBE_FRAMES * 2];
   unsigned rate, channels, decoded, candidate, delay;
   int16_t input[RECORD_AAC_PROBE_FRAMES * 2];
   bool configured, failed, ready;
};
record_aac_probe_t *record_aac_probe_new(unsigned rate, unsigned channels)
{
   record_aac_probe_t *p;
   unsigned i, c;
   if (!rate || rate > 192000 || !channels || channels > 2)
      return NULL;
   p = (record_aac_probe_t *)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->rate = rate;
   p->channels = channels;
   p->best = p->second = 1e30;
   for (i = 0; i < PROBE_BURST; i++)
      for (c = 0; c < channels; c++)
      {
         double t = (double)i / PROBE_BURST;
         double phase = (400.0 + 300.0 * c) * i / rate
               + 500.0 * PROBE_BURST / rate * t * t;
         double window = sin(3.141592653589793 * t);
         int16_t value = (int16_t)(12000.0 * window * window
               * sin(6.283185307179586 * phase));
         p->input[(PROBE_BURST + i) * channels + c] = value;
         p->energy += (double)value * value / (32768.0 * 32768.0);
      }
   if (p->energy < 1.0)
   {
      free(p);
      return NULL;
   }
   return p;
}
const int16_t *record_aac_probe_input(record_aac_probe_t *p,
      size_t offset, size_t *frames)
{
   if (frames)
      *frames = 0;
   if (!p || !frames || offset >= RECORD_AAC_PROBE_FRAMES)
      return NULL;
   *frames = RECORD_AAC_PROBE_FRAMES - offset;
   return p->input + offset * p->channels;
}
bool record_aac_probe_config(record_aac_probe_t *p, const void *asc, size_t bytes)
{
   if (!p || p->failed || p->configured || !asc || !bytes)
      return false;
   p->decoder = raac_open((const uint8_t *)asc, bytes);
   if (!p->decoder || raac_sample_rate(p->decoder) != p->rate
         || raac_channels(p->decoder) != p->channels
         || raac_frame_len(p->decoder) != PROBE_PACKET)
   {
      p->failed = true;
      return false;
   }
   p->configured = true;
   return true;
}
bool record_aac_probe_packet(record_aac_probe_t *p, const void *data, size_t bytes)
{
   int frames;
   if (!p || p->failed || !p->configured || !data || !bytes)
      return false;
   if (p->decoded == RECORD_AAC_PROBE_FRAMES)
      return true;
   frames = raac_decode_f32(p->decoder, (const uint8_t *)data, bytes,
         p->output + p->decoded * p->channels);
   if (frames != PROBE_PACKET)
   {
      p->failed = true;
      return false;
   }
   p->decoded += PROBE_PACKET;
   return true;
}
int record_aac_probe_step(record_aac_probe_t *p)
{
   unsigned n, i, count;
   if (!p || p->failed)
      return -1;
   if (p->ready)
      return 2;
   if (p->decoded < RECORD_AAC_PROBE_FRAMES)
      return 0;
   count = PROBE_BURST * p->channels;
   for (n = 0; n < 16 && p->candidate <= PROBE_DELAY; n++, p->candidate++)
   {
      double error = 0;
      const int16_t *ref = p->input + PROBE_BURST * p->channels;
      const float *got = p->output + (PROBE_BURST + p->candidate) * p->channels;
      for (i = 0; i < count; i++)
      {
         double delta = got[i] - (double)ref[i] / 32768.0;
         error += delta * delta;
      }
      if (error < p->best)
      {
         p->second = p->best;
         p->best = error;
         p->delay = p->candidate;
      }
      else if (error < p->second)
         p->second = error;
   }
   if (p->candidate <= PROBE_DELAY)
      return 1;
   if (p->best > p->energy * 0.02 || p->second <= p->best * 2)
   {
      p->failed = true;
      return -1;
   }
   p->ready = true;
   return 2;
}
bool record_aac_probe_delay(const record_aac_probe_t *p, unsigned *delay)
{
   if (!p || !delay || !p->ready || p->failed)
      return false;
   *delay = p->delay;
   return true;
}
void record_aac_probe_free(record_aac_probe_t *p)
{
   if (!p)
      return;
   if (p->decoder)
      raac_close(p->decoder);
   free(p);
}
#else
record_aac_probe_t *record_aac_probe_new(unsigned rate, unsigned channels)
{
   (void)rate;
   (void)channels;
   return NULL;
}
const int16_t *record_aac_probe_input(record_aac_probe_t *p, size_t at, size_t *n)
{
   (void)p;
   (void)at;
   if (n)
      *n = 0;
   return NULL;
}
bool record_aac_probe_config(record_aac_probe_t *p, const void *asc, size_t n)
{
   (void)p;
   (void)asc;
   (void)n;
   return false;
}
bool record_aac_probe_packet(record_aac_probe_t *p, const void *data, size_t n)
{
   (void)p;
   (void)data;
   (void)n;
   return false;
}
int record_aac_probe_step(record_aac_probe_t *p)
{
   (void)p;
   return -1;
}
bool record_aac_probe_delay(const record_aac_probe_t *p, unsigned *delay)
{
   (void)p;
   (void)delay;
   return false;
}
void record_aac_probe_free(record_aac_probe_t *p)
{
   (void)p;
}
#endif
