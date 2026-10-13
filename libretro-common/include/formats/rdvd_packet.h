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
   RDVD_PACKET_SUBPICTURE,
   RDVD_PACKET_PCI,
   RDVD_PACKET_DSI
};

typedef struct rdvd_packet
{
   const uint8_t *data;
   size_t size;
   enum rdvd_packet_kind kind;
   unsigned first_access_unit;
   unsigned frame_count;
   unsigned index;
   unsigned sample_rate;
   unsigned sample_bits;
   unsigned channels;
} rdvd_packet_t;

#define RDVD_PCI_BYTES 980
#define RDVD_DSI_BYTES 1018

typedef struct rdvd_nav
{
   uint32_t sector;
   uint32_t start_pts;
   uint32_t end_pts;
   uint32_t scr;
   uint32_t end_sector_offset;
   unsigned vob_id;
   unsigned cell_id;
   enum rdvd_packet_kind kind;
} rdvd_nav_t;

typedef struct rdvd_highlight
{
   uint32_t start_pts;
   uint32_t end_pts;
   uint32_t selection_end_pts;
   unsigned status;
   unsigned groups;
   unsigned buttons;
   unsigned selectable_buttons;
   unsigned button_offset;
   unsigned forced_select;
   unsigned forced_activate;
   unsigned display_types[3];
} rdvd_highlight_t;

/* Parses DVD-Video private_stream_1/2 payloads after the PES header.
 * No allocation or copying. The input must remain valid while the view is
 * used; a demuxer-owned view expires with its originating PES packet.
 * Unknown IDs and truncated headers return false and clear the output.
 * LPCM sample packing and subpicture assembly belong to their consumers. */
bool rdvd_packet_read(uint8_t stream_id, const uint8_t *data, size_t size,
      rdvd_packet_t *out);

/* Reads general PCI/DSI information from a complete NAV packet view.
 * This does not execute navigation commands or interpret menu highlights.
 * The DSI end-sector field is relative to the NAV sector, not an address.
 * Failure clears the output. */
bool rdvd_nav_read(const rdvd_packet_t *packet, rdvd_nav_t *out);

/* Reads PCI highlight metadata, clearing the output on malformed input.
 * Status 0 means no highlight; 1/2/3 are the DVD update modes. This parser
 * does not merge inherited highlight state from preceding NAV packets.
 * Display types retain the DVD aspect/letterbox/pan-scan bit masks.
 * Button numbers are local to a group; button_offset is retained verbatim. */
bool rdvd_highlight_read(const rdvd_packet_t *packet, rdvd_highlight_t *out);

RETRO_END_DECLS
#endif
