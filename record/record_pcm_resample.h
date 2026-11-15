/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PCM_RESAMPLE_H
#define RECORD_PCM_RESAMPLE_H
#include "record_audio_queue.h"
RETRO_BEGIN_DECLS
typedef struct record_pcm_resample record_pcm_resample_t;
enum record_pcm_resample_result
{
   RECORD_PCM_ERROR = -1, RECORD_PCM_BUSY, RECORD_PCM_PROGRESS,
   RECORD_PCM_PACKET, RECORD_PCM_END
};
/* Cold allocation. One worker borrows source through cancellation/free.
 * Input is contiguous mono/stereo S16. Conversion uses source-zero alignment. */
record_pcm_resample_t *record_pcm_resample_new(record_audio_queue_t *source,
      double source_rate, unsigned output_rate);
/* One bounded conversion action (<=256 input frames). PACKET borrows output
 * until ack; repeated peeks preserve its bytes, count and encoder timestamp. */
int record_pcm_resample_peek(record_pcm_resample_t *converter,
      const int16_t **samples, size_t *frames, uint64_t *timestamp);
bool record_pcm_resample_ack(record_pcm_resample_t *converter);
bool record_pcm_resample_duration(const record_pcm_resample_t *converter,
      uint64_t source_frames, uint64_t *output_frames);
bool record_pcm_resample_end(const record_pcm_resample_t *converter,
      uint64_t *source_frames, uint64_t *output_frames);
/* Processing owner, releases any queue loan. Unread input remains caller-owned.
 * Free is valid after END or cancellation only. */
bool record_pcm_resample_cancel(record_pcm_resample_t *converter);
bool record_pcm_resample_free(record_pcm_resample_t *converter);
RETRO_END_DECLS
#endif
