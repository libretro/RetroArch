/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_MUX_AVFOUNDATION_H
#define RECORD_MUX_AVFOUNDATION_H
#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_mux_avfoundation record_mux_avfoundation_t;
typedef struct record_mux_avfoundation_config
{
   const void *video_format; /* Borrowed H.264/HEVC CMVideoFormatDescription. */
   int64_t epoch;
   unsigned timescale, audio_rate, channels, audio_layout, audio_bitrate, max_audio_frames;
} record_mux_avfoundation_config_t;
enum record_mux_avfoundation_result
{ RECORD_AVF_ERROR=-1, RECORD_AVF_BUSY, RECORD_AVF_OK };
/* All operations belong to one dedicated recording worker. Compressed video
 * passes through unchanged; S16 interleaved PCM uses its declared rate/layout.
 * Audio fields all zero disable audio. Preserve existing output files. */
record_mux_avfoundation_t *record_mux_avfoundation_new(
      const record_mux_avfoundation_config_t *config,const char *path);
/* BUSY takes nothing. Video sample is borrowed only during this call; caller
 * retains it across BUSY. Accepted PCM is copied on the worker, never dropped.
 * Timestamps use the source clock; no implicit rebasing or gap removal. */
int record_mux_avfoundation_video(record_mux_avfoundation_t *mux,void *sample);
int record_mux_avfoundation_audio(record_mux_avfoundation_t *mux,
      const int16_t *samples,size_t frames,uint64_t timestamp);
/* Starts asynchronous finalization once, then polls: no semaphore/wait.
 * Call only after both input streams are closed and all samples acknowledged. */
int record_mux_avfoundation_finish(record_mux_avfoundation_t *mux);
/* Cancellation/free refuse an in-flight finalization callback. Poll it first;
 * no callback may access freed storage. Normal free requires finalization. */
bool record_mux_avfoundation_cancel(record_mux_avfoundation_t *mux);
bool record_mux_avfoundation_free(record_mux_avfoundation_t *mux);
RETRO_END_DECLS
#endif
