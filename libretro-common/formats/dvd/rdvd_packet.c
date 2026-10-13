/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_packet.h>

bool rdvd_packet_read(uint8_t stream_id, const uint8_t *data, size_t size,
      rdvd_packet_t *out)
{
   rdvd_packet_t view;
   size_t header;
   unsigned id;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!data || !size)
      return false;
   memset(&view, 0, sizeof(view));
   id = data[0];
   if (stream_id == 0xbf)
   {
      if (id == 0 && size == RDVD_PCI_BYTES)
         view.kind = RDVD_PACKET_PCI;
      else if (id == 1 && size == RDVD_DSI_BYTES)
         view.kind = RDVD_PACKET_DSI;
      else
         return false;
      header = 1;
   }
   else if (stream_id != 0xbd)
      return false;
   else if (id >= 0x20 && id <= 0x3f)
   {
      view.kind = RDVD_PACKET_SUBPICTURE;
      view.index = id - 0x20;
      header = 1;
   }
   else if (id >= 0x80 && id <= 0x8f)
   {
      view.kind = id < 0x88 ? RDVD_PACKET_AC3 : RDVD_PACKET_DTS;
      view.index = id & 7;
      header = 4;
   }
   else if (id >= 0xa0 && id <= 0xa7)
   {
      view.kind = RDVD_PACKET_LPCM;
      view.index = id & 7;
      header = 7;
   }
   else
      return false;
   if (size < header)
      return false;
   if (view.kind == RDVD_PACKET_LPCM)
   {
      unsigned depth = data[5] >> 6;
      unsigned rate = (data[5] >> 4) & 3;
      if (depth == 3 || rate > 1)
         return false;
      view.sample_bits = 16 + 4 * depth;
      view.sample_rate = rate ? 96000 : 48000;
      view.channels = 1 + (data[5] & 7);
   }
   if (header >= 4)
   {
      view.frame_count = data[1];
      /* Retain the DVD pointer verbatim, not as a validated data offset. */
      view.first_access_unit = ((unsigned)data[2] << 8) | data[3];
   }
   view.data = data + header;
   view.size = size - header;
   *out = view;
   return true;
}

static uint32_t rdvd_u32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
      | ((uint32_t)p[2] << 8) | p[3];
}

bool rdvd_nav_read(const rdvd_packet_t *packet, rdvd_nav_t *out)
{
   rdvd_nav_t nav;
   const uint8_t *p;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!packet || !packet->data)
      return false;
   memset(&nav, 0, sizeof(nav));
   p = packet->data;
   if (packet->kind == RDVD_PACKET_PCI && packet->size == RDVD_PCI_BYTES - 1)
   {
      nav.sector = rdvd_u32(p);
      nav.start_pts = rdvd_u32(p + 12);
      nav.end_pts = rdvd_u32(p + 16);
   }
   else if (packet->kind == RDVD_PACKET_DSI && packet->size == RDVD_DSI_BYTES - 1)
   {
      nav.scr = rdvd_u32(p);
      nav.sector = rdvd_u32(p + 4);
      nav.end_sector_offset = rdvd_u32(p + 8);
      nav.vob_id = ((unsigned)p[24] << 8) | p[25];
      nav.cell_id = p[27];
   }
   else
      return false;
   nav.kind = packet->kind;
   *out = nav;
   return true;
}
