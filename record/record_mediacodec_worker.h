/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_MEDIACODEC_WORKER_H
#define RECORD_MEDIACODEC_WORKER_H
#include "record_audio_queue.h"
#include "record_mux_mediacodec.h"
#include "record_aac_probe.h"
RETRO_BEGIN_DECLS
typedef struct record_mediacodec_worker record_mediacodec_worker_t;
enum record_mediacodec_worker_result
{
   RECORD_MC_WORKER_ERROR = -1,
   RECORD_MC_WORKER_BUSY,
   RECORD_MC_WORKER_PROGRESS,
   RECORD_MC_WORKER_END
};
/* One I/O worker borrows all components. Audio encoder and queue must both
 * be present or absent. channels/rate match their cold configuration. */
record_mediacodec_worker_t *record_mediacodec_worker_new(
      record_mediacodec_encoder_t *video, record_mediacodec_encoder_t *audio,
      record_audio_queue_t *pcm, record_mux_mediacodec_t *mux,
      unsigned channels, unsigned rate);
/* Before service starts; caller retains the probe until worker retirement. */
bool record_mediacodec_worker_calibrate(record_mediacodec_worker_t *worker,
      record_aac_probe_t *probe);
/* Completed calibrated recording only; source frames exclude the prefix. */
bool record_mediacodec_worker_audio_edit(
      const record_mediacodec_worker_t *worker, uint64_t *skip,
      uint64_t *frames, unsigned *roll);
/* Worker-only, after GPU producer retirement and PCM producer closure.
 * Source ends share the recording epoch. Same-end retries are idempotent. */
bool record_mediacodec_worker_finish(record_mediacodec_worker_t *worker,
      int64_t video_end_us, uint64_t audio_end_sample);
/* One output action per encoder and one bounded PCM input action. Borrows
 * survive backpressure. No per-packet allocation, wait or GPU submission. */
int record_mediacodec_worker_step(record_mediacodec_worker_t *worker);
/* Controller still owns encoder/GPU retirement, mux cancellation and PCM
 * drain on failure. Free refuses active service unless aborted. */
void record_mediacodec_worker_abort(record_mediacodec_worker_t *worker);
bool record_mediacodec_worker_free(record_mediacodec_worker_t *worker);
RETRO_END_DECLS
#endif
