/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef LIBRETRO_RDVD_PACKET_H
#define LIBRETRO_RDVD_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rdvd_packet_kind
{
   RDVD_PACKET_NONE = 0,
   RDVD_PACKET_AC3,
   RDVD_PACKET_DTS,
   RDVD_PACKET_LPCM,
   RDVD_PACKET_SUBPICTURE
};

typedef struct rdvd_packet
{
   const uint8_t *data;
   size_t size;
   enum rdvd_packet_kind kind;
   unsigned first_access_unit;
   unsigned frame_count;
   unsigned index;
} rdvd_packet_t;

/* Parses DVD-Video private_stream_1 payloads after the PES header.
 * No allocation or copying. The input must remain valid while the view is
 * used; a demuxer-owned view expires with its originating PES packet.
 * Unknown IDs and truncated headers return false and clear the output.
 * LPCM sample packing and subpicture assembly belong to their consumers. */
bool rdvd_packet_read(uint8_t stream_id, const uint8_t *data, size_t size,
      rdvd_packet_t *out);

RETRO_END_DECLS
#endif
