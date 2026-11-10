/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef RFLV_MUX_H
#define RFLV_MUX_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

typedef struct rflv_mux rflv_mux_t;
typedef struct rflv_config
{
   const void *avcc, *asc;
   size_t avcc_size, asc_size, max_packet_size;
   unsigned video_timescale, audio_timescale;
} rflv_config_t;

typedef struct rflv_span
{
   const void *data;
   size_t size;
} rflv_span_t;

typedef struct rflv_tag
{
   rflv_span_t spans[2];
   uint32_t timestamp;
   unsigned type;
} rflv_tag_t;

enum rflv_result
{
   RFLV_ERROR = -1, RFLV_BUSY, RFLV_PROGRESS,
   RFLV_PACKET, RFLV_END
};

RETRO_BEGIN_DECLS
/* Single worker owner. Copies AVCDecoderConfigurationRecord and AAC-LC ASC
 * during cold initialization. At least one track; AVC uses four-byte lengths. */
rflv_mux_t *rflv_mux_new(const rflv_config_t *config);
/* Enhanced RTMP HEVC. avcc/avcc_size carry an hvcC record with four-byte
 * NAL lengths. Other fields retain their meaning; max_packet_size <= 0xfffff7.
 * Media input stays borrowed; native Annex B requires a separate adapter. */
rflv_mux_t *rflv_mux_new_hevc(const rflv_config_t *config);
/* Native AVC encoder adapter. avcc contains Annex B SPS/PPS (one each).
 * Cold allocation reserves conversion storage up to max_packet_size.
 * Other configuration fields retain their ordinary meaning. */
rflv_mux_t *rflv_mux_new_annexb(const rflv_config_t *config);
/* Track 1: one length-prefixed AVC/HEVC access unit; track 2: one raw AAC unit.
 * Accepted input stays borrowed until output release, error or free.
 * BUSY takes nothing. Timestamps use the configured per-track timescale;
 * their millisecond representation must fit a nonnegative signed 32-bit value. */
int rflv_mux_push(rflv_mux_t *mux, unsigned track,
      const void *data, size_t size, uint64_t dts, uint64_t pts, bool keyframe);
/* Video only, on a mux created by new_annexb. Immutable native output stays
 * borrowed until release/error/free. Each step adapts one NAL into reserved
 * storage: format conversion requires a copy; ordinary push still borrows. */
int rflv_mux_push_annexb(rflv_mux_t *mux, const void *data, size_t size,
      uint64_t dts, uint64_t pts, bool keyframe);
/* One NAL validation step. PACKET repeats until release; BUSY needs input. */
int rflv_mux_step(rflv_mux_t *mux);
/* Up to three borrowed spans. No payload copy, allocation, I/O or wait. */
bool rflv_mux_get(rflv_mux_t *mux,
      const rflv_span_t **spans, size_t *count);
/* Ready codec payload for streaming, without the file tag header/trailer.
 * False for the initial file header. Borrows last until mux output release.
 * A streaming consumer must return its borrow before releasing mux output. */
bool rflv_mux_get_tag(rflv_mux_t *mux, rflv_tag_t *tag);
bool rflv_mux_release(rflv_mux_t *mux);
/* Requires acknowledged output. No seek or file trailer. */
bool rflv_mux_finish(rflv_mux_t *mux);
void rflv_mux_free(rflv_mux_t *mux);
RETRO_END_DECLS
#endif
