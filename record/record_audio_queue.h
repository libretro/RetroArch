/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AUDIO_QUEUE_H
#define RECORD_AUDIO_QUEUE_H
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_audio_queue record_audio_queue_t;
enum record_audio_queue_result
{ RECORD_AUDIO_ERROR=-1, RECORD_AUDIO_BUSY, RECORD_AUDIO_OK, RECORD_AUDIO_END };
/* Cold allocation. One PCM producer and one worker. S16 interleaved packets
 * preserve explicit sample-clock timestamps, including gaps. Capacity is a
 * hard byte budget including per-packet headers, not a rounded ring limit. */
record_audio_queue_t *record_audio_queue_new(unsigned channels,
      size_t max_frames,size_t capacity_bytes);
/* Producer: no allocation/wait. BUSY copies nothing and advances no clock.
 * Input is borrowed for this call only. Nonoverlapping monotonic timestamps
 * and complete packets are required; no silent trimming or replacement. */
int record_audio_queue_push(record_audio_queue_t *queue,const int16_t *samples,
      size_t frames,uint64_t timestamp);
/* Producer, after its last push returns. Idempotent; no pushes after close. */
bool record_audio_queue_finish(record_audio_queue_t *queue);
/* Worker: borrowed staging remains unchanged through repeated peeks/BUSY at
 * the downstream mux. Only ack permits the next packet to replace it. END
 * follows producer closure and acknowledgement of every complete packet. */
int record_audio_queue_peek(record_audio_queue_t *queue,const int16_t **samples,
      size_t *frames,uint64_t *timestamp);
bool record_audio_queue_ack(record_audio_queue_t *queue);
/* Both sides quiesced. Refuses queued or borrowed samples, including after
 * closure. A controller must drain/ack before destroying failed recordings. */
bool record_audio_queue_free(record_audio_queue_t *queue);
RETRO_END_DECLS
#endif
