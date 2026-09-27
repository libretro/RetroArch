/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AUDIO_PIPELINE_STRETCH_H
#define AUDIO_PIPELINE_STRETCH_H

#include <retro_spsc.h>
#include "audio_pipeline_layout.h"

RETRO_BEGIN_DECLS

typedef struct audio_pipeline_stretch audio_pipeline_stretch_t;

struct audio_pipeline_stretch_block
{
   const void *data;
   size_t frames;
   /* Source frames released immediately by WSOLA/LPF; direct views release their
    * source only when acknowledged with consume(). */
   size_t input_used;
   unsigned layout;
   uint32_t reset_serial;
   /* Unchanged source frames; consume() releases their ring storage.
    * The view may use one-frame wrap scratch. */
   bool passthrough;
};

/* Create while processing is stopped. The caller owns ring, metadata and the
 * aligned native output buffer for the lifetime of the stage. The output
 * buffer must not overlap ring storage. The producer
 * publishes whole native frames and preceding transport metadata. This stage
 * exclusively owns the ring tail and metadata retirement; do not read/skip
 * either queue elsewhere. Ordered cutoff metadata enables native filtering
 * after transport, in the existing output buffer. Dry inactive blocks retain
 * direct ring views. No SRC, device pacing or format conversion here. */
audio_pipeline_stretch_t *audio_pipeline_stretch_new(unsigned rate,
      unsigned channels, bool is_float, uint32_t search_channels,
      retro_spsc_t *ring, audio_pipeline_layout_t *metadata,
      void *output, size_t output_frames);
void audio_pipeline_stretch_free(audio_pipeline_stretch_t *state);
/* Consumer only. True guarantees no retained output/synthesis can run without
 * more source. Pending metadata must still be serviced before waiting.
 * False is conservative: call next() to determine whether progress is possible. */
bool audio_pipeline_stretch_needs_input(const audio_pipeline_stretch_t *state);

/* Consumer only. One bounded processing step. Pending output is returned
 * before any new source/control is read. Input budget bounds new source work;
 * zero allows pending synthesis/exit to drain. Zero output budget is inert.
 * A successful step may consume input without producing output. Compare the
 * returned reset serial even on empty blocks, resetting downstream SRC when
 * it changes. Layout changes drain the old native tail with its old layout
 * before advancing the serial; explicit resets discard that tail instead.
 * Data stays valid until consume/discard/free. Do not write to a direct view. */
bool audio_pipeline_stretch_next(audio_pipeline_stretch_t *state,
      size_t input_budget, size_t output_budget,
      struct audio_pipeline_stretch_block *block);

/* Acknowledge at most the last offered frame count. Partial acknowledgement
 * retains the rest without copying. Zero acknowledges nothing. */
bool audio_pipeline_stretch_consume(audio_pipeline_stretch_t *state,
      size_t frames);

/* EOF, after all queued source has been consumed. Drain bounded native output
 * until complete, acknowledging each block as usual. No padding is appended.
 * Further source requires a discontinuity/reset. Zero output budget is inert. */
bool audio_pipeline_stretch_finish(audio_pipeline_stretch_t *state,
      size_t output_budget, struct audio_pipeline_stretch_block *block,
      bool *complete);

/* Discontinuity: cancel retained output/history, discard exactly frames from
 * the source ring and advance reset_serial. Zero resets history without
 * dropping queued source. Oversized requests fail without changing anything.
 * The caller must also abandon/reset its downstream pending output and SRC. */
bool audio_pipeline_stretch_discard(audio_pipeline_stretch_t *state,
      size_t frames);

RETRO_END_DECLS
#endif
