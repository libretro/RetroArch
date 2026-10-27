/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_SESSION_MEDIACODEC_H
#define RECORD_SESSION_MEDIACODEC_H
#include "record_mediacodec_worker.h"
RETRO_BEGIN_DECLS
typedef struct record_session_mediacodec record_session_mediacodec_t;
enum record_session_mediacodec_error
{
   RECORD_MC_SESSION_NONE,
   RECORD_MC_SESSION_CANCELLED,
   RECORD_MC_SESSION_OUTPUT,
   RECORD_MC_SESSION_FILE
};
/* Cold I/O worker. Owns encoder(s), PCM queue, mux and exclusive file.
 * Audio is disabled only when all audio arguments are zero. NULL publishes
 * nothing and rolls back cold resources. Startup/final I/O may block here. */
record_session_mediacodec_t *record_session_mediacodec_new(JNIEnv *env,
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_audio_frames, size_t audio_capacity, const char *path);
/* Cold I/O owner, before publishing the session to the GPU executor. Borrowed
 * through gpu_released. Executor owns Surface/capture/draw construction. */
void *record_session_mediacodec_surface(record_session_mediacodec_t *session);
/* One PCM producer. Explicit source sample clock; BUSY copies nothing. */
int record_session_mediacodec_audio(record_session_mediacodec_t *session,
      const int16_t *samples, size_t frames, uint64_t timestamp);
/* Producer or its I/O notification owner after the last push returns,
 * including cancellation. No pushes
 * after close. The end is immutable and includes any dropped source interval.
 */
bool record_session_mediacodec_audio_finish(
      record_session_mediacodec_t *session, uint64_t end_sample);
/* GPU executor or its single I/O notification owner, after producer closure
 * and all source reads retire.
 * Then signal released separately after Surface/context users retire. These
 * notifications publish immutable source ends; they perform no native calls. */
bool record_session_mediacodec_gpu_retired(
      record_session_mediacodec_t *session, int64_t end_us);
bool record_session_mediacodec_gpu_released(
      record_session_mediacodec_t *session);
/* Controller request; does not close either producer. GPU executor observes
 * cancelled and uses its retirement path instead of drawing further frames. */
void record_session_mediacodec_cancel(record_session_mediacodec_t *session);
bool record_session_mediacodec_cancelled(
      const record_session_mediacodec_t *session);
/* Independent I/O executor; never the GPU/frame/audio/task thread. Uses worker
 * results. ERROR/END are terminal only after both producers and GPU users
 * retire. On failure drains at most one queued PCM packet per step. No wait
 * loop or dispatcher here; executor must wake for input/close/cancellation. */
int record_session_mediacodec_step(record_session_mediacodec_t *session);
enum record_session_mediacodec_error record_session_mediacodec_error(
      const record_session_mediacodec_t *session, int *file_error);
/* I/O owner, all callers quiescent; refuses unretired sessions. */
bool record_session_mediacodec_free(record_session_mediacodec_t *session);
RETRO_END_DECLS
#endif
