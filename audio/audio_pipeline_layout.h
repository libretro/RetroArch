/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AUDIO_PIPELINE_LAYOUT_H
#define AUDIO_PIPELINE_LAYOUT_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

#define AUDIO_PIPELINE_LAYOUT_CAPACITY 64
#define AUDIO_PIPELINE_TEMPO_MASK UINT32_C(0x003fffff)
#define AUDIO_PIPELINE_STRETCH UINT32_C(0x00400000)
#define AUDIO_PIPELINE_RESET UINT32_C(0x80000000)

/* Metadata only. One producer publishes a boundary BEFORE the corresponding
 * audio head. One consumer observes the audio head BEFORE calling limit.
 * Audio positions use the byte ring's wrapping size_t counters. */
typedef struct audio_pipeline_layout
{
   retro_atomic_size_t head;
   unsigned published_layout;
   uint32_t published_control, published_cutoff;
   uint8_t producer_pad[56];
   retro_atomic_size_t tail;
   unsigned current_layout;
   uint32_t current_control, reset_serial, current_cutoff;
   uint8_t consumer_pad[52];
   struct { size_t position; unsigned layout; uint32_t control, cutoff; } events[AUDIO_PIPELINE_LAYOUT_CAPACITY];
} audio_pipeline_layout_t;

/* Initialize/reset only with both owners stopped, alongside the audio ring. */
static INLINE void audio_pipeline_layout_init(audio_pipeline_layout_t *q,
      unsigned layout)
{
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
   q->published_layout = q->current_layout = layout;
   q->published_control = q->current_control = 65536;
   q->reset_serial = 0;
   q->published_cutoff = q->current_cutoff = 0;
}

/* Producer only; control may include a forced discontinuity. */
static INLINE bool audio_pipeline_layout_event(audio_pipeline_layout_t *q,
      size_t position, unsigned layout, uint32_t control)
{
   size_t head = retro_atomic_load_relaxed_size(&q->head);
   size_t tail = retro_atomic_load_acquire_size(&q->tail);
   size_t slot;
   if (head - tail == AUDIO_PIPELINE_LAYOUT_CAPACITY) return false;
   slot = head & (AUDIO_PIPELINE_LAYOUT_CAPACITY - 1);
   q->events[slot].position = position;
   q->events[slot].layout   = layout;
   q->events[slot].control  = control;
   q->events[slot].cutoff   = q->published_cutoff;
   q->published_layout     = layout;
   q->published_control    = control & ~AUDIO_PIPELINE_RESET;
   retro_atomic_store_release_size(&q->head, head + 1);
   return true;
}

/* Publish an LPF target at a source boundary. Zero disables it; nonzero Hz
 * is clamped by the native filter to the core sample rate. Other controls
 * remain unchanged. A full queue leaves the request unpublished. */
static INLINE bool audio_pipeline_layout_publish_cutoff(
      audio_pipeline_layout_t *q, size_t position, uint32_t cutoff)
{
   uint32_t previous = q->published_cutoff;
   if (cutoff == previous) return true;
   q->published_cutoff = cutoff;
   if (audio_pipeline_layout_event(q, position,
            q->published_layout, q->published_control)) return true;
   q->published_cutoff = previous;
   return false;
}

/* Producer only. False means metadata is full: do not publish new-layout
 * audio until this succeeds. Unchanged layouts touch no shared cursor.
 * Layout-only publication preserves the current transport request. */
static INLINE bool audio_pipeline_layout_publish(audio_pipeline_layout_t *q,
      size_t position, unsigned layout)
{
   if (layout == q->published_layout) return true;
   return audio_pipeline_layout_event(q, position, layout, q->published_control);
}

/* Publish layout, transport and LPF together, BEFORE their source audio. Tempo is
 * source frames/output frame in Q16, 0.25..32 when active; inactive uses 1x.
 * False leaves metadata unchanged: retry the request before publishing audio.
 * reset requests a DSP-history discard here, even for an unchanged request.
 * cutoff is core-rate Hz, or zero for dry. */
static INLINE bool audio_pipeline_layout_publish_processing(
      audio_pipeline_layout_t *q, size_t position, unsigned layout,
      uint32_t tempo_q16, bool active, bool reset, uint32_t cutoff)
{
   uint32_t control = 65536;
   uint32_t previous = q->published_cutoff;
   if (active)
   {
      if (tempo_q16 < 16384 || tempo_q16 > 2097152) return false;
      control = tempo_q16 | AUDIO_PIPELINE_STRETCH;
   }
   if (!reset && layout == q->published_layout && control == q->published_control
         && cutoff == previous)
      return true;
   if (reset) control |= AUDIO_PIPELINE_RESET;
   q->published_cutoff = cutoff;
   if (audio_pipeline_layout_event(q, position, layout, control)) return true;
   q->published_cutoff = previous;
   return false;
}

/* Transport-only changes preserve the published filter target. */
static INLINE bool audio_pipeline_layout_publish_transport(
      audio_pipeline_layout_t *q, size_t position, unsigned layout,
      uint32_t tempo_q16, bool active, bool reset)
{
   return audio_pipeline_layout_publish_processing(q, position, layout,
         tempo_q16, active, reset, q->published_cutoff);
}

/* Consumer only. Bound a read to one layout and retire boundaries passed by
 * an explicit consumer discard. capacity is the audio ring's capacity (at
 * most SIZE_MAX/2); bytes was obtained from its acquired head snapshot.
 * Compare reset_serial before/after: every retired reset advances it, including
 * resets crossed by a discard or multiple requests at the same position. */
static INLINE size_t audio_pipeline_layout_limit_impl(audio_pipeline_layout_t *q,
      size_t position, size_t bytes, size_t capacity, bool transport)
{
   size_t tail = retro_atomic_load_relaxed_size(&q->tail);
   size_t head = retro_atomic_load_acquire_size(&q->head);
   size_t first = tail;
   while (tail != head)
   {
      size_t slot = tail & (AUDIO_PIPELINE_LAYOUT_CAPACITY - 1);
      size_t distance = q->events[slot].position - position;
      if (distance && distance <= capacity)
      {
         if (bytes > distance) bytes = distance;
         break;
      }
      q->current_layout = q->events[slot].layout;
      if (transport)
      {
         q->current_control = q->events[slot].control & ~AUDIO_PIPELINE_RESET;
         q->current_cutoff = q->events[slot].cutoff;
         if (q->events[slot].control & AUDIO_PIPELINE_RESET) q->reset_serial++;
      }
      tail++;
   }
   if (tail != first) retro_atomic_store_release_size(&q->tail, tail);
   return bytes;
}

/* Legacy layout-only consumers do not interpret transport requests. */
static INLINE size_t audio_pipeline_layout_limit(audio_pipeline_layout_t *q,
      size_t position, size_t bytes, size_t capacity)
{
   return audio_pipeline_layout_limit_impl(q, position, bytes, capacity, false);
}

/* Use this reader for every retirement on a transport-enabled pipeline,
 * including zero-byte reads and discards. A layout-only reader loses resets. */
static INLINE size_t audio_pipeline_layout_limit_transport(audio_pipeline_layout_t *q,
      size_t position, size_t bytes, size_t capacity)
{
   return audio_pipeline_layout_limit_impl(q, position, bytes, capacity, true);
}
#endif
