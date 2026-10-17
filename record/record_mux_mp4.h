/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_MUX_MP4_H
#define RECORD_MUX_MP4_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

enum record_mp4_codec
{
   RECORD_MP4_H264, RECORD_MP4_HEVC, RECORD_MP4_AV1
};
enum record_mp4_result
{
   RECORD_MP4_ERROR = -1, RECORD_MP4_BUSY, RECORD_MP4_OK
};
typedef struct record_mux_mp4 record_mux_mp4_t;
typedef struct record_mp4_config
{
   const void *headers;
   size_t headers_size, max_packet_size;
   unsigned width, height, timescale, bit_depth;
   enum record_mp4_codec codec;
   /* Optional one AC-3 syncframe supplies dac3 and sample rate/channel count.
    * NULL disables audio. Audio timestamps use the parsed sample rate. */
   const void *ac3_header;
   size_t ac3_header_size;
} record_mp4_config_t;

/* Single owner. Headers are Annex B SPS/PPS or VPS/SPS/PPS, or sized AV1
 * sequence OBUs. One parameter set of each kind, 4:2:0, 8/10-bit, no B frames.
 * Startup allocates a bounded output buffer and copies configuration. */
record_mux_mp4_t *record_mux_mp4_new(const record_mp4_config_t *config);
/* Initialization and each fragment stay borrowed until release. BUSY takes
 * nothing. No allocation, file I/O, waits or retained input pointers in push.
 * Video input is Annex B or sized AV1 OBUs; audio is one complete AC-3 frame.
 * Track 1 is video, 2 audio. DTS=PTS, in the track's timescale; nonzero
 * duration, no overlap per track, video starts with a random-access packet. */
int record_mux_mp4_push(record_mux_mp4_t *mux, unsigned track,
      const void *data, size_t size, uint64_t timestamp, uint32_t duration,
      bool keyframe);
bool record_mux_mp4_get(record_mux_mp4_t *mux, const void **data, size_t *size);
bool record_mux_mp4_release(record_mux_mp4_t *mux);
/* Refuses unacknowledged output. Finalization requires no seek or trailer;
 * caller still owns asynchronous output drain/close and error reporting. */
bool record_mux_mp4_finish(record_mux_mp4_t *mux);
void record_mux_mp4_free(record_mux_mp4_t *mux);
RETRO_END_DECLS
#endif
