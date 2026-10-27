/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_DISPATCH_MEDIACODEC_H
#define RECORD_DISPATCH_MEDIACODEC_H
#include "record_session_mediacodec.h"
RETRO_BEGIN_DECLS
typedef struct record_dispatch_mediacodec record_dispatch_mediacodec_t;
enum record_dispatch_mediacodec_state
{
   RECORD_MC_DISPATCH_STARTING,
   RECORD_MC_DISPATCH_READY,
   RECORD_MC_DISPATCH_DRAINING,
   RECORD_MC_DISPATCH_DONE,
   RECORD_MC_DISPATCH_FAILED
};
/* Cold controller. Copies request/path and starts an independent I/O thread.
 * JNI admission, codec/file startup and native finalization run there.
 * Unsupported/threadless builds return NULL. GPU executor remains separate. */
record_dispatch_mediacodec_t *record_dispatch_mediacodec_new(
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_audio_frames, size_t audio_capacity, const char *path);
/* GPU executor, after READY. Window remains borrowed until gpu_released.
 * No producer may use the session directly. */
void *record_dispatch_mediacodec_surface(record_dispatch_mediacodec_t *job);
/* Immutable cold configuration, valid until collection. */
bool record_dispatch_mediacodec_video_format(
      const record_dispatch_mediacodec_t *job, unsigned *width,
      unsigned *height, unsigned *color);
/* Cold GPU-executor construction/collection only. One owner per input window;
 * claim survives native retirement until executor collection. */
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *job);
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *job);
/* PCM producer; STARTING is BUSY and copies nothing. Explicit sample clock. */
int record_dispatch_mediacodec_audio(record_dispatch_mediacodec_t *job,
      const int16_t *samples, size_t frames, uint64_t timestamp);
/* PCM producer after last push returns, even on startup failure/cancellation.
 */
bool record_dispatch_mediacodec_audio_finish(
      record_dispatch_mediacodec_t *job, uint64_t end_sample);
/* GPU executor, after producer closure and independent read retirement, then
 * after Surface/context users retire. Required even if startup failed and no
 * GPU resources were created. Source end is immutable. No native calls. */
bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *job, int64_t end_us);
bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *job);
/* Wake output service after GPU posting/retirement. Nonblocking notification.
 */
void record_dispatch_mediacodec_notify(record_dispatch_mediacodec_t *job);
void record_dispatch_mediacodec_cancel(record_dispatch_mediacodec_t *job);
bool record_dispatch_mediacodec_cancelled(
      const record_dispatch_mediacodec_t *job);
/* DONE/FAILED mean worker-owned resources retired. Startup failure still
 * awaits producer/GPU closure. error/file_error/system_error are optional. */
enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *job, int *error, int *file_error,
      int *system_error);
/* Cold controller, all callers quiescent. Refuses active resources. Detached
 * worker normally needs no join; detach failure joins only after retirement. */
bool record_dispatch_mediacodec_free(record_dispatch_mediacodec_t *job);
RETRO_END_DECLS
#endif
