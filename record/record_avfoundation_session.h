/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AVFOUNDATION_SESSION_H
#define RECORD_AVFOUNDATION_SESSION_H
#include "record_avfoundation_worker.h"
RETRO_BEGIN_DECLS
typedef struct record_avfoundation_session record_avfoundation_session_t;
typedef struct record_avfoundation_session_ops
{
   /* Worker-only: bind the first borrowed video sample's actual format. */
   void *(*open)(void *context,void *sample);
   record_avfoundation_worker_ops_t mux;
   bool (*cancel)(void *mux);
   bool (*free_mux)(void *mux);
   /* Quiesces native encoder callbacks; may block this I/O worker only.
    * Always called before video lease abandonment, including normal EOS. */
   void (*destroy_encoder)(void *context);
   /* Runs last, after GPU retirement and both queues are destroyed.
    * False retains context for a later retirement poll. */
   bool (*destroy_context)(void *context);
} record_avfoundation_session_ops_t;
enum record_avfoundation_session_error
{
   RECORD_AVF_SESSION_NONE, RECORD_AVF_SESSION_CANCELLED,
   RECORD_AVF_SESSION_VIDEO, RECORD_AVF_SESSION_MUX, RECORD_AVF_SESSION_EMPTY
};
/* Cold I/O-worker construction. Success adopts queue, video, optional audio
 * and context; NULL adopts nothing. Video must use this queue, context must
 * own its encoder and retain capture until destroy_context. Copies ops.
 * Audio may be queued before the first video sample; mux opens only then. */
record_avfoundation_session_t *record_avfoundation_session_new(
      record_gpu_queue_t *queue,record_gpu_worker_vt_t *video,
      record_audio_queue_t *audio,void *context,
      const record_avfoundation_session_ops_t *ops);
/* One PCM producer. Whole-packet BUSY consumes nothing. Source sample clock. */
int record_avfoundation_session_audio(record_avfoundation_session_t *session,
      const int16_t *samples,size_t frames,uint64_t timestamp);
/* Each producer closes after its final call returns. The video producer uses
 * the adopted queue/capture directly until closure. No access after closure.
 * Cancellation never closes another thread's input on its behalf. */
bool record_avfoundation_session_video_finish(record_avfoundation_session_t *session);
bool record_avfoundation_session_audio_finish(record_avfoundation_session_t *session);
void record_avfoundation_session_cancel(record_avfoundation_session_t *session);
/* One dedicated I/O worker. Uses RECORD_AVF_WORKER_* results. ERROR/END are
 * terminal only after native callbacks, GPU leases and queued PCM retire.
 * Errors are observable earlier via error(). No scheduler is installed here. */
int record_avfoundation_session_step(record_avfoundation_session_t *session);
enum record_avfoundation_session_error record_avfoundation_session_error(
      const record_avfoundation_session_t *session);
/* All callers quiesced. Refuses until terminal retirement, including startup
 * failure/cancellation. Does not join threads or wait for GPU completion. */
bool record_avfoundation_session_free(record_avfoundation_session_t *session);
/* Dedicated worker only, after BUSY. True schedules native completion polling;
 * false awaits producer input/closure. No resource/service changes. */
bool record_avfoundation_session_retry_needed(const record_avfoundation_session_t *session);
RETRO_END_DECLS
#endif
