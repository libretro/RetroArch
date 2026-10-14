/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_LPCM_H
#define LIBRETRO_RDVD_LPCM_H
#include <formats/rdvd_packet.h>
#include <formats/rlpcm.h>
RETRO_BEGIN_DECLS
#define RDVD_LPCM_BUFFER_BYTES 84

typedef struct rdvd_lpcm
{
   const uint8_t *input;
   size_t remaining;
   size_t used;
   size_t block_bytes;
   rlpcm_format_t format;
   unsigned block_frames;
   unsigned index;
   uint8_t buffer[RDVD_LPCM_BUFFER_BYTES];
   bool configured;
   bool synchronized;
   bool borrowing;
} rdvd_lpcm_t;

typedef struct rdvd_lpcm_block
{
   const uint8_t *data;
   size_t size;
   size_t frames;
   rlpcm_format_t format;
} rdvd_lpcm_block_t;

enum rdvd_lpcm_result
{
   RDVD_LPCM_MORE = 0,
   RDVD_LPCM_BLOCK,
   RDVD_LPCM_ERROR
};

/* One selected substream, one owner. State must remain at a stable address.
 * Whole blocks borrow PES bytes; only boundary fragments copy into state. */
bool rdvd_lpcm_init(rdvd_lpcm_t *audio, unsigned index);
/* Drops partial packing blocks and invalidates views after discontinuity. */
void rdvd_lpcm_reset(rdvd_lpcm_t *audio);
/* Drain next to MORE before advancing PES input. Initial acquisition uses
 * the LPCM access-unit pointer (four means payload byte zero). Continued
 * input must be contiguous. Format changes are refused until reset. */
bool rdvd_lpcm_feed(rdvd_lpcm_t *audio, const rdvd_packet_t *packet);
/* max_frames must fit at least one format block; output rounds down to
 * whole blocks. Views expire at next/feed/reset and can be decoded by rlpcm.
 * MORE with used != 0 at EOF is truncation. No allocation, I/O or waiting. */
enum rdvd_lpcm_result rdvd_lpcm_next(rdvd_lpcm_t *audio,
      size_t max_frames, rdvd_lpcm_block_t *out);
RETRO_END_DECLS
#endif
