/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_SPU_H
#define LIBRETRO_RDVD_SPU_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS
#define RDVD_SPU_NO_PTS ((uint64_t)-1)

typedef struct rdvd_spu_packet
{
   const uint8_t *data;
   size_t size;
   uint64_t pts;
   unsigned control_offset;
} rdvd_spu_packet_t;

typedef struct rdvd_spu_stream rdvd_spu_stream_t;
enum rdvd_spu_result
{
   RDVD_SPU_UNSUPPORTED = -2,
   RDVD_SPU_ERROR = -1,
   RDVD_SPU_MORE = 0,
   RDVD_SPU_READY = 1
};

typedef struct rdvd_spu_control
{
   rdvd_spu_packet_t packet;
   unsigned cursor;
   unsigned end;
   unsigned delay;
   unsigned next_offset;
   bool stopped;
} rdvd_spu_control_t;

typedef struct rdvd_spu_event
{
   unsigned command;
   unsigned values[4];
} rdvd_spu_event_t;

/* Reads one exact standard DVD-Video SPU unit, borrowing its bytes.
 * This validates framing, not the control program or bitmap data. */
bool rdvd_spu_packet_read(const uint8_t *data, size_t size, rdvd_spu_packet_t *out);

/* One stream per selected subpicture ID; reset on seek/discontinuity.
 * A reusable 65535-byte buffer is allocated only for fragmented input. */
rdvd_spu_stream_t *rdvd_spu_stream_new(void);
void rdvd_spu_stream_free(rdvd_spu_stream_t *stream);
void rdvd_spu_stream_reset(rdvd_spu_stream_t *stream);

/* Feed the payload after the DVD substream ID. Returns one ready unit or
 * MORE, with the consumed prefix length. Feed any remaining bytes again.
 * PTS belongs to the first nonempty fragment. Whole units borrow input;
 * fragmented units borrow stream storage. Input and stream must remain
 * alive through use; views expire at the next feed/reset/free.
 * ERROR clears assembly: discard the remaining payload. */
enum rdvd_spu_result rdvd_spu_stream_feed(rdvd_spu_stream_t *stream,
      const uint8_t *data, size_t size, uint64_t pts, size_t *consumed,
      rdvd_spu_packet_t *out);

/* offset 0 selects the first control sequence. delay is in 1024/90000 s
 * units relative to packet PTS. next_offset equals this offset on the last
 * sequence; otherwise it moves forward. The control view borrows input. */
bool rdvd_spu_control_begin(const rdvd_spu_packet_t *packet, unsigned offset,
      rdvd_spu_control_t *out);

/* READY emits commands 0..6: force/start/stop, palette[4], alpha[4],
 * rectangle {x0,x1,y0,y1}, field offsets {top,bottom}. MORE terminates a
 * sequence; ERROR/UNSUPPORTED stop it. Validate through the terminator
 * before applying events. Command 7 (regional colour changes) is not yet
 * implemented. No display state is changed by this reader. */
enum rdvd_spu_result rdvd_spu_control_next(rdvd_spu_control_t *control,
      rdvd_spu_event_t *out);

/* Decodes one standard 2-bit SPU row directly to caller-owned indices 0..3.
 * start/next are byte offsets within the unit; rows end on byte boundaries.
 * Width is 1..4096. Use separate cursors for the two interlaced fields.
 * Output must not overlap input. No allocation or RGB expansion.
 * Failure clears next; discard the row's
 * contents, which may contain a successfully decoded prefix. */
bool rdvd_spu_decode_row(const rdvd_spu_packet_t *packet, unsigned start,
      unsigned width, uint8_t *output, size_t capacity, unsigned *next);
RETRO_END_DECLS
#endif
