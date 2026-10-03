/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUDIO_STRETCH_H
#define AUDIO_STRETCH_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define AUDIO_STRETCH_MAX_CHANNELS 11

typedef struct audio_stretch audio_stretch_t;

struct audio_stretch_io
{
   const void *input;
   void *output;
   size_t input_frames;
   size_t output_capacity;
   size_t input_used;
   size_t output_frames;
};

struct audio_stretch_drain_io
{
   void *output;
   size_t output_capacity;
   size_t output_frames;
   /* (size_t)-1 unless a source gap boundary occurs in this call.
    * May equal output_frames: the gap then precedes future output/input. */
   size_t gap_offset;
   bool complete;
};

/* Single-owner engine, interleaved native float or int16 throughout.
 * Rate: 8000..192000 Hz; channels: 1..11 (including canonical pipeline slots).
 * search_channels is a nonzero mask of channel indices; exclude LFE. */
audio_stretch_t *audio_stretch_new(unsigned rate, unsigned channels,
      bool is_float, uint32_t search_channels);
void audio_stretch_free(audio_stretch_t *state);
void audio_stretch_reset(audio_stretch_t *state);
size_t audio_stretch_storage(const audio_stretch_t *state);
unsigned audio_stretch_hop(const audio_stretch_t *state);

/* Tempo: source frames per output frame, 0.25..32. Applied at hop
 * boundaries; fractional advance is retained across calls. Input and output
 * must not overlap. Counts describe actual consumption/production; retry
 * unconsumed input. Zero output capacity consumes nothing. No allocation,
 * device I/O or locking. Invalid requests return false without changing state.
 * A partial hop may be processed with zero input. After a positive-capacity
 * drain call, processing requires reset. Reset discards retained data.
 * Inactive transport must bypass this engine entirely. */
bool audio_stretch_process(audio_stretch_t *state, struct audio_stretch_io *io,
      double tempo);

/* Exit/EOF: drain pending synthesis, then the last overlap, then remaining
 * source lookahead, without padding or new input. No allocations/conversions.
 * A positive-capacity call latches drain mode until reset. Zero capacity is
 * non-mutating; complete reports whether any tail remains. Invalid arguments
 * return false without changing state. A gap is reported once, including at
 * the end of the final buffer if previously skipped source lies before future
 * caller input. The owner must handle that discontinuity (e.g. crossfade);
 * this function preserves available samples and does not invent missing ones. */
bool audio_stretch_drain(audio_stretch_t *state, struct audio_stretch_drain_io *io);

/* Blend matching native spans for entry/exit transitions. The caller owns
 * history and advances offset by frames across fragmented calls. total is
 * 1..65536 frames; a one-frame transition selects incoming. Output may equal
 * either input exactly; otherwise spans must not overlap. No allocation.
 * Invalid arguments return false without writing output. */
bool audio_stretch_crossfade(void *output, const void *outgoing,
      const void *incoming, size_t frames, unsigned channels, bool is_float,
      unsigned offset, unsigned total);

/* Optional single-owner transition holdback; bypass when inactive.
 * One allocation holds at most tail_frames native frames (1..65536).
 * Process input/output must not overlap. Zero output capacity consumes nothing.
 * Boundary overlaps the retained tail with future input; it rejects a second
 * boundary until the first overlap completes. Reset discards all history.
 * Flush latches EOF until reset. An incomplete overlap discards its unused
 * outgoing suffix at EOF once blending has started; an unstarted boundary
 * preserves the outgoing tail. */
typedef struct audio_stretch_transition audio_stretch_transition_t;
audio_stretch_transition_t *audio_stretch_transition_new(unsigned channels,
      bool is_float, unsigned tail_frames);
void audio_stretch_transition_free(audio_stretch_transition_t *state);
void audio_stretch_transition_reset(audio_stretch_transition_t *state);
bool audio_stretch_transition_boundary(audio_stretch_transition_t *state);
bool audio_stretch_transition_process(audio_stretch_transition_t *state,
      struct audio_stretch_io *io);
/* gap_offset is always (size_t)-1; zero capacity is a non-mutating query. */
bool audio_stretch_transition_flush(audio_stretch_transition_t *state,
      struct audio_stretch_drain_io *io);

/* Single-consumer engine/transition adapter. Reset on stream discontinuities.
 * Input/output must not overlap. Calls allocate nothing; zero capacity is
 * non-mutating. Retry unconsumed input. A requested exit finishes before a
 * requested re-entry. Inactive frontend paths should bypass this object.
 * Flush accepts no input, latches EOF and requires reset before processing. */
typedef struct audio_stretch_stream audio_stretch_stream_t;
audio_stretch_stream_t *audio_stretch_stream_new(unsigned rate, unsigned channels,
      bool is_float, uint32_t search_channels);
void audio_stretch_stream_free(audio_stretch_stream_t *state);
void audio_stretch_stream_reset(audio_stretch_stream_t *state);
/* True only in raw state with no retained output. The owner may then bypass
 * the adapter until activation. NULL is quiescent. EOF requires reset first. */
bool audio_stretch_stream_quiescent(const audio_stretch_stream_t *state);
/* Consumer only, with the current processing request unchanged. True means
 * no output can be produced without source; exit/EOF remain conservative. */
bool audio_stretch_stream_needs_input(const audio_stretch_stream_t *state);
bool audio_stretch_stream_process(audio_stretch_stream_t *state,
      struct audio_stretch_io *io, double tempo, bool active);
bool audio_stretch_stream_flush(audio_stretch_stream_t *state,
      struct audio_stretch_drain_io *io);

/* Optional caller-owned output block for partial downstream consumption.
 * Bind before use (or after reset); storage must outlive the binding and must
 * not overlap input. Bound streams use push/finish/peek/consume exclusively.
 * Peeked native frames remain stable until consumed or explicitly reset.
 * Reset discards pending output but preserves the binding. Bind NULL/0 to
 * return a reset stream to the direct process/flush API. No copies added. */
bool audio_stretch_stream_bind(audio_stretch_stream_t *state,
      void *output, size_t capacity);
bool audio_stretch_stream_push(audio_stretch_stream_t *state,
      const void *input, size_t frames, size_t *used, double tempo, bool active);
/* Limit newly produced frames to min(limit, bound capacity). A zero limit
 * validates only: no source consumption, output or control-state mutation.
 * Already pending output remains unchanged and must be consumed separately. */
bool audio_stretch_stream_push_limit(audio_stretch_stream_t *state,
      const void *input, size_t frames, size_t *used, double tempo, bool active,
      size_t limit);
/* Inline owners may lend aligned native source storage while fully inactive.
 * Same budgets/counts as push_limit; dry quiescent spans are exposed by peek
 * without copying. The accepted source prefix must remain alive and unchanged
 * until consumed or reset, even though used reports it as accepted immediately.
 * Active/transition output uses the bound buffer. Peeked data is read-only.
 * Pending views survive pushes, partial consumption and finish requests.
 * Never lend source that overlaps the bound output buffer. */
bool audio_stretch_stream_push_view_limit(audio_stretch_stream_t *state,
      const void *input, size_t frames, size_t *used, double tempo, bool active,
      size_t limit);
const void *audio_stretch_stream_peek(const audio_stretch_stream_t *state,
      size_t *frames);
bool audio_stretch_stream_consume(audio_stretch_stream_t *state, size_t frames);
bool audio_stretch_stream_finish(audio_stretch_stream_t *state, bool *complete);
/* Same production limit; zero queries completion without latching EOF. */
bool audio_stretch_stream_finish_limit(audio_stretch_stream_t *state,
      bool *complete, size_t limit);

RETRO_END_DECLS
#endif
