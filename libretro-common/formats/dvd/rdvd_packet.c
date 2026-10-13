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
   if (stream_id != 0xbd || !data || !size)
      return false;
   memset(&view, 0, sizeof(view));
   id = data[0];
   if (id >= 0x20 && id <= 0x3f)
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
