/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_PYROWAVE_PCM_H
#define RECORD_PYROWAVE_PCM_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
#include "record_gpu_queue.h"

RETRO_BEGIN_DECLS
typedef struct record_pyrowave_pcm record_pyrowave_pcm_t;

typedef struct record_pyrowave_pcm_view
{
   uint64_t first_sample;
   const void *data;
   size_t frames;
   unsigned slot;
} record_pyrowave_pcm_view_t;

/* Cold fixed pool for one PCM producer and one consumer. Bytes are interleaved
 * S16LE, including on big-endian hosts. Indices use source sample frames, not
 * microseconds. Each session owns a separate pool; both sides quiesce before
 * destruction. Results use RECORD_GPU_QUEUE_* ownership conventions. */
record_pyrowave_pcm_t *record_pyrowave_pcm_new(unsigned channels,
      unsigned maximum_frames, unsigned slots);
bool record_pyrowave_pcm_compatible(const record_pyrowave_pcm_t *pcm,
      unsigned channels, unsigned maximum_frames);
/* Producer writes directly into the lent span. At most one reservation;
 * failed submit retains it for retry/cancel. Successful submit publishes the
 * bytes unchanged. Forward sample-index gaps are allowed; overlaps are not. */
int record_pyrowave_pcm_acquire(record_pyrowave_pcm_t *pcm, void **data, size_t *capacity);
bool record_pyrowave_pcm_submit(record_pyrowave_pcm_t *pcm,
      uint64_t first_sample, size_t frames);
bool record_pyrowave_pcm_cancel(record_pyrowave_pcm_t *pcm);
bool record_pyrowave_pcm_finish(record_pyrowave_pcm_t *pcm);
/* Consumer borrows each view until release, across partial device writes or
 * transport would-block. No payload staging, allocation, clocks or waits. */
int record_pyrowave_pcm_take(record_pyrowave_pcm_t *pcm, record_pyrowave_pcm_view_t *view);
bool record_pyrowave_pcm_release(record_pyrowave_pcm_t *pcm, unsigned slot);
/* Refuses reserved, queued or borrowed slots, including after finish. */
bool record_pyrowave_pcm_free(record_pyrowave_pcm_t *pcm);
RETRO_END_DECLS
#endif
