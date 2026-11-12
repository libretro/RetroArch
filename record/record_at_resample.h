/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AT_RESAMPLE_H
#define RECORD_AT_RESAMPLE_H
#include "record_audio_queue.h"
RETRO_BEGIN_DECLS
typedef struct record_at_resample record_at_resample_t;
enum record_at_resample_result
{
   RECORD_AT_RESAMPLE_ERROR = -1, RECORD_AT_RESAMPLE_BUSY,
   RECORD_AT_RESAMPLE_PROGRESS, RECORD_AT_RESAMPLE_END
};
/* One recording worker consumes the caller-owned queue until free.
 * Input timestamps start at zero and remain contiguous. */
record_at_resample_t *record_at_resample_new(record_audio_queue_t *source,
      double input_rate, unsigned output_rate);
/* Writes at most 1024 frames directly into a caller-owned AAC input loan.
 * Output must not alias the source queue; it is not retained after return. */
int record_at_resample_read(record_at_resample_t *converter, int16_t *output,
      size_t capacity, size_t *frames);
bool record_at_resample_timing(const record_at_resample_t *converter,
      uint64_t *input_frames, uint64_t *output_frames);
/* Reset must retire native input borrows before acknowledging the queue.
 * Failure retains ownership for retry. Free requires END or cancellation. */
bool record_at_resample_cancel(record_at_resample_t *converter);
bool record_at_resample_free(record_at_resample_t *converter);
int32_t record_at_resample_error(const record_at_resample_t *converter);
RETRO_END_DECLS
#endif
