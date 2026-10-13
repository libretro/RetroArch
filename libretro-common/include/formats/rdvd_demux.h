/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_DEMUX_H
#define LIBRETRO_RDVD_DEMUX_H
#include <formats/rmpeg1_ps.h>
#include <formats/rdvd_packet.h>
RETRO_BEGIN_DECLS
typedef struct rdvd_demux
{
   rmpeg1_ps_t *ps;
   bool borrowing;
} rdvd_demux_t;
typedef struct rdvd_demux_packet
{
   rmpeg1_ps_packet_t pes;
   rdvd_packet_t dvd;
   bool dvd_valid;
} rdvd_demux_packet_t;
/* Borrows an exclusively owned parser and resets it. No parser allocation. */
bool rdvd_demux_init(rdvd_demux_t *demux, rmpeg1_ps_t *ps);
/* Input stays immutable until next returns false. Whole PES payloads borrow
 * input; boundary fragments use the parser's existing assembly buffer. */
bool rdvd_demux_feed(rdvd_demux_t *demux, const uint8_t *data, size_t size);
/* Views expire on the next feed/next/reset. Unknown private payloads retain
 * their PES view with dvd_valid false. Failure clears the output. */
bool rdvd_demux_next(rdvd_demux_t *demux, rdvd_demux_packet_t *out);
void rdvd_demux_reset(rdvd_demux_t *demux);
RETRO_END_DECLS
#endif
