/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_clock.h"
#include <stdlib.h>
#include <string.h>

typedef struct record_pyrowave_clock_span
{
   uint64_t device_first, source_first;
   uint32_t device_frames, source_frames, block_frames, block_offset;
} record_pyrowave_clock_span_t;

struct record_pyrowave_clock
{
   record_pyrowave_clock_span_t *spans;
   uint64_t generation, epoch, cursor, device_end, source_end, last_sample;
   unsigned capacity, head, count;
   bool initialized, polled, failed, committed, last_valid;
};

record_pyrowave_clock_t *record_pyrowave_clock_new(unsigned capacity)
{
   record_pyrowave_clock_t *c;
   if (!capacity || capacity > 256) return NULL;
   c = (record_pyrowave_clock_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->spans = (record_pyrowave_clock_span_t*)calloc(capacity, sizeof(*c->spans));
   if (!c->spans)
   {
      free(c);
      return NULL;
   }
   c->capacity = capacity;
   return c;
}

bool record_pyrowave_clock_reset(record_pyrowave_clock_t *c, uint64_t generation, uint64_t epoch)
{
   if (!c || !generation || !epoch || (c->initialized && c->generation == generation && epoch <= c->epoch)) return false;
   c->generation = generation; c->epoch = epoch;
   c->head = c->count = 0; c->cursor = c->device_end = c->source_end = 0;
   c->initialized = true; c->polled = c->failed = c->committed = c->last_valid = false;
   return true;
}

bool record_pyrowave_clock_room(const record_pyrowave_clock_t *c)
{
   return c && c->initialized && !c->failed && c->count < c->capacity;
}

unsigned record_pyrowave_clock_available(const record_pyrowave_clock_t *c)
{
   return c && c->initialized && !c->failed ? c->capacity - c->count : 0;
}

bool record_pyrowave_clock_commit(record_pyrowave_clock_t *c,
      uint64_t device_first, uint32_t device_frames, uint64_t source_first, uint32_t source_frames,
      uint32_t block_frames, uint32_t block_offset)
{
   record_pyrowave_clock_span_t *span;
   uint64_t first_sample, end_sample;
   if (!record_pyrowave_clock_room(c) || !device_frames || !source_frames || !block_frames
         || block_offset > block_frames || device_frames > block_frames - block_offset
         || device_first > (uint64_t)-1 - device_frames || source_first > (uint64_t)-1 - source_frames
         || (c->committed && device_first < c->device_end)) return false;
   first_sample = source_first + (uint64_t)block_offset * source_frames / block_frames;
   end_sample = source_first + ((uint64_t)block_offset + device_frames) * source_frames / block_frames;
   if (c->committed && first_sample < c->source_end) return false;
   span = &c->spans[(c->head + c->count) % c->capacity];
   span->device_first = device_first; span->device_frames = device_frames;
   span->source_first = source_first; span->source_frames = source_frames;
   span->block_frames = block_frames; span->block_offset = block_offset;
   c->device_end = device_first + device_frames; c->source_end = end_sample;
   c->count++; c->committed = true;
   return true;
}

bool record_pyrowave_clock_poll(record_pyrowave_clock_t *c, uint64_t device_frame,
      record_pyrowave_clock_position_t *position)
{
   unsigned i, count;
   if (!position) return false;
   memset(position, 0, sizeof(*position));
   if (!c || !c->initialized) return false;
   position->generation = c->generation; position->epoch = c->epoch;
   if (c->polled && device_frame < c->cursor) c->failed = true;
   if (c->failed) return false;
   if (c->polled && device_frame == c->cursor && c->last_valid)
   {
      position->source_sample = c->last_sample; position->valid = true;
   }
   c->cursor = device_frame; c->polled = true; count = c->count;
   for (i = 0; i < count; i++)
   {
      record_pyrowave_clock_span_t *span = &c->spans[c->head];
      uint64_t end = span->device_first + span->device_frames;
      if (device_frame < span->device_first) break;
      if (device_frame <= end)
      {
         position->source_sample = span->source_first
            + (span->block_offset + device_frame - span->device_first) * span->source_frames / span->block_frames;
         position->valid = true;
      }
      if (device_frame < end) break;
      c->head = (c->head + 1) % c->capacity; c->count--;
      if (device_frame == end && (!c->count || c->spans[c->head].device_first != end)) break;
   }
   c->last_valid = position->valid; c->last_sample = position->source_sample;
   return position->valid;
}

bool record_pyrowave_clock_drained(const record_pyrowave_clock_t *c,
      uint64_t source_end)
{
   return c && c->initialized && c->committed && c->polled && !c->failed
      && c->source_end == source_end && c->cursor >= c->device_end;
}

void record_pyrowave_clock_free(record_pyrowave_clock_t *c)
{
   if (!c) return;
   free(c->spans); free(c);
}
