/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_audio_clock.h"
#include <stdlib.h>
#include <string.h>

#define RECORD_PYROWAVE_AUDIO_SPANS 32

typedef struct record_pyrowave_audio_span
{
   uint64_t first;
   uint32_t input, output, accepted;
} record_pyrowave_audio_span_t;

struct record_pyrowave_audio_clock
{
   record_pyrowave_clock_t *map;
   record_pyrowave_audio_span_t span[RECORD_PYROWAVE_AUDIO_SPANS];
   uint64_t generation, epoch, written, native_first, accepted_end, played;
   uint64_t source_first, source_end, mapped_end;
   uint32_t source_frames, input_used;
   unsigned head, count;
   bool initialized, begun, failed, accepted, polled;
};

record_pyrowave_audio_clock_t *record_pyrowave_audio_clock_new(uint64_t generation)
{
   record_pyrowave_audio_clock_t *c;
   if (!generation) return NULL;
   c = (record_pyrowave_audio_clock_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->map = record_pyrowave_clock_new(256);
   if (!c->map) { free(c); return NULL; }
   c->generation = generation;
   return c;
}

bool record_pyrowave_audio_clock_observe(record_pyrowave_audio_clock_t *c,
      const audio_presentation_position_t *device, record_pyrowave_clock_position_t *source)
{
   if (!source) return false;
   memset(source, 0, sizeof(*source));
   if (!c || !device || c->failed) return false;
   source->generation = c->generation;
   source->epoch = device->epoch;
   if (device->failed || !device->epoch || device->played > device->written
         || (c->initialized && (c->epoch != device->epoch || device->written < c->written))
         || (c->accepted && device->written < c->accepted_end))
   { c->failed = true; return false; }
   if (!c->initialized)
   {
      if (!record_pyrowave_clock_reset(c->map, c->generation, device->epoch))
      { c->failed = true; return false; }
      c->epoch = device->epoch; c->initialized = true;
      c->native_first = device->written;
   }
   c->written = device->written;
   if (device->valid || device->drained)
   {
      if (c->polled && device->played < c->played)
      { c->failed = true; return false; }
      c->played = device->played; c->polled = true;
      record_pyrowave_clock_poll(c->map, device->played, source);
      if (!device->valid) source->valid = false;
   }
   return !c->failed;
}

bool record_pyrowave_audio_clock_begin(record_pyrowave_audio_clock_t *c,
      uint64_t first, uint32_t frames)
{
   if (!c || c->failed || !c->initialized || c->count || !frames
         || first > (uint64_t)-1 - frames || (c->begun && first < c->source_end)
         || (c->begun && c->input_used != c->source_frames)
         || record_pyrowave_clock_available(c->map) < RECORD_PYROWAVE_AUDIO_SPANS) return false;
   c->source_first = first; c->source_frames = frames;
   c->input_used = 0; c->source_end = first + frames; c->begun = true;
   return true;
}

bool record_pyrowave_audio_clock_produced(record_pyrowave_audio_clock_t *c,
      uint32_t offset, uint32_t input, uint32_t output)
{
   record_pyrowave_audio_span_t *span;
   if (!c || c->failed || !c->begun || !input || offset != c->input_used
         || input > c->source_frames - c->input_used
         || (output && (c->count == RECORD_PYROWAVE_AUDIO_SPANS
            || record_pyrowave_clock_available(c->map) <= c->count))) return false;
   if (output)
   {
      span = &c->span[(c->head + c->count) % RECORD_PYROWAVE_AUDIO_SPANS];
      span->first = c->source_first + offset; span->input = input;
      span->output = output; span->accepted = 0; c->count++;
   }
   c->input_used += input;
   return true;
}

bool record_pyrowave_audio_clock_writable(const record_pyrowave_audio_clock_t *c)
{
   return c && !c->failed && c->count
      && record_pyrowave_clock_available(c->map) >= c->count;
}

bool record_pyrowave_audio_clock_accepted(record_pyrowave_audio_clock_t *c,
      uint64_t first, uint32_t frames)
{
   uint32_t remaining = frames;
   uint64_t available = 0;
   unsigned i;
   if (!record_pyrowave_audio_clock_writable(c) || !frames
         || first > (uint64_t)-1 - frames || first < c->native_first
         || (c->accepted && first < c->accepted_end)) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_audio_span_t *span = &c->span[(c->head + i) % RECORD_PYROWAVE_AUDIO_SPANS];
      available += span->output - span->accepted;
   }
   if (frames > available) return false;
   while (remaining)
   {
      record_pyrowave_audio_span_t *span = &c->span[c->head];
      uint32_t take = span->output - span->accepted;
      if (take > remaining) take = remaining;
      if (!record_pyrowave_clock_commit(c->map, first, take,
               span->first, span->input, span->output, span->accepted))
      { c->failed = true; return false; }
      span->accepted += take; remaining -= take; first += take;
      c->mapped_end = span->first + (uint64_t)span->accepted * span->input / span->output;
      if (span->accepted == span->output)
      { c->head = (c->head + 1) % RECORD_PYROWAVE_AUDIO_SPANS; c->count--; }
   }
   c->accepted_end = first; c->accepted = true;
   return true;
}

bool record_pyrowave_audio_clock_pending(const record_pyrowave_audio_clock_t *c)
{ return c && c->count != 0; }
bool record_pyrowave_audio_clock_has_output(const record_pyrowave_audio_clock_t *c)
{ return c && (c->accepted || c->count); }
uint64_t record_pyrowave_audio_clock_end_sample(const record_pyrowave_audio_clock_t *c)
{ return c ? c->mapped_end : 0; }
bool record_pyrowave_audio_clock_drained(const record_pyrowave_audio_clock_t *c)
{
   return c && !c->failed && c->accepted && !c->count && c->input_used == c->source_frames
      && record_pyrowave_clock_drained(c->map, c->mapped_end);
}
bool record_pyrowave_audio_clock_failed(const record_pyrowave_audio_clock_t *c)
{ return !c || c->failed; }
void record_pyrowave_audio_clock_fail(record_pyrowave_audio_clock_t *c)
{ if (c) c->failed = true; }
void record_pyrowave_audio_clock_free(record_pyrowave_audio_clock_t *c)
{
   if (!c) return;
   record_pyrowave_clock_free(c->map); free(c);
}
