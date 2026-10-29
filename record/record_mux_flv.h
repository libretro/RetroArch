/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_MUX_FLV_H
#define RECORD_MUX_FLV_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct record_mux_flv record_mux_flv_t;
typedef struct record_flv_config
{
   const void *avcc, *asc;
   size_t avcc_size, asc_size, max_packet_size;
   unsigned video_timescale, audio_timescale;
} record_flv_config_t;

typedef struct record_flv_span
{
   const void *data;
   size_t size;
} record_flv_span_t;

enum record_flv_result
{
   RECORD_FLV_ERROR = -1, RECORD_FLV_BUSY, RECORD_FLV_PROGRESS,
   RECORD_FLV_PACKET, RECORD_FLV_END
};

RETRO_BEGIN_DECLS
/* Single worker owner. Copies AVCDecoderConfigurationRecord and AAC-LC ASC
 * during cold initialization. At least one track; AVC uses four-byte lengths. */
record_mux_flv_t *record_mux_flv_new(const record_flv_config_t *config);
/* Track 1: one length-prefixed AVC access unit; track 2: one raw AAC unit.
 * Accepted input stays borrowed until output release, error or free.
 * BUSY takes nothing. Timestamps use the configured per-track timescale;
 * their millisecond representation must fit a nonnegative signed 32-bit value. */
int record_mux_flv_push(record_mux_flv_t *mux, unsigned track,
      const void *data, size_t size, uint64_t dts, uint64_t pts, bool keyframe);
/* One AVC NAL validation step. PACKET repeats until release; BUSY needs input. */
int record_mux_flv_step(record_mux_flv_t *mux);
/* Up to three borrowed spans. No payload copy, allocation, I/O or wait. */
bool record_mux_flv_get(record_mux_flv_t *mux,
      const record_flv_span_t **spans, size_t *count);
bool record_mux_flv_release(record_mux_flv_t *mux);
/* Requires acknowledged output. No seek or file trailer. */
bool record_mux_flv_finish(record_mux_flv_t *mux);
void record_mux_flv_free(record_mux_flv_t *mux);
RETRO_END_DECLS
#endif
