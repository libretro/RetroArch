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
   RDVD_SPU_ERROR = -1,
   RDVD_SPU_MORE = 0,
   RDVD_SPU_READY = 1
};

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
RETRO_END_DECLS
#endif
