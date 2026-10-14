/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_AC3_H
#define LIBRETRO_RDVD_AC3_H
#include <formats/rdvd_packet.h>
#include <formats/rac3.h>
RETRO_BEGIN_DECLS
#define RDVD_AC3_BUFFER_BYTES 3840

typedef struct rdvd_ac3
{
   const uint8_t *input;
   size_t remaining;
   uint8_t *buffer;
   size_t used;
   unsigned index;
   bool synchronized;
   bool borrowing;
   bool failed;
} rdvd_ac3_t;

typedef struct rdvd_ac3_frame
{
   const uint8_t *data;
   size_t size;
   rac3_frame_info_t info;
} rdvd_ac3_frame_t;

enum rdvd_ac3_result
{
   RDVD_AC3_MORE = 0,
   RDVD_AC3_FRAME,
   RDVD_AC3_ERROR
};

/* Caller owns at least BUFFER_BYTES of assembly storage, separate from PES
 * input. A single owner feeds one selected substream in uninterrupted order.
 * Whole frames borrow PES input; only spanning frames copy into storage. */
bool rdvd_ac3_init(rdvd_ac3_t *audio, unsigned index,
      uint8_t *buffer, size_t capacity);
/* Invalidates frames and drops fragments. Required after seek/discontinuity. */
void rdvd_ac3_reset(rdvd_ac3_t *audio);
/* Drain next to MORE before feeding again or releasing PES input. Other
 * substreams must be filtered by the caller. The first access-unit pointer
 * establishes initial sync; subsequent bytes must remain continuous. */
bool rdvd_ac3_feed(rdvd_ac3_t *audio, const rdvd_packet_t *packet);
/* Frames expire at next/feed/reset. Header framing is not CRC validation or
 * decoding. ERROR is sticky until reset; MORE with used != 0 at EOF is a
 * truncated frame. Neither call allocates, waits or performs I/O. */
enum rdvd_ac3_result rdvd_ac3_next(rdvd_ac3_t *audio,
      rdvd_ac3_frame_t *frame);
RETRO_END_DECLS
#endif
