/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_AV1_PACKET_H
#define RETROARCH_VIDEOTOOLBOX_AV1_PACKET_H
#include <formats/rav1.h>

typedef struct videotoolbox_av1_packet
{
   uint8_t *data;
   size_t capacity, size;
   unsigned count;
   int ready, display_slot;
} videotoolbox_av1_packet_t;

static INLINE bool videotoolbox_av1_packet_init(
      videotoolbox_av1_packet_t *p, uint8_t *data, size_t capacity)
{
   if (!p)
      return false;
   memset(p, 0, sizeof(*p));
   p->display_slot = -1;
   if (!data || !capacity || capacity > 64u * 1024 * 1024)
      return false;
   p->data = data;
   p->capacity = capacity;
   return true;
}

/* Reset only after the native sample releases its borrowed storage. */
static INLINE void videotoolbox_av1_packet_reset(videotoolbox_av1_packet_t *p)
{
   if (!p)
      return;
   p->size = 0;
   p->count = 0;
   p->ready = 0;
   p->display_slot = -1;
}

static INLINE int videotoolbox_av1_packet_seal(videotoolbox_av1_packet_t *p)
{
   if (!p || !p->data || !p->count || p->count > 8 || !p->size
         || p->size > p->capacity)
      return -1;
   p->ready = 1;
   return 0;
}

/* 0 copied, 1 sealed/busy without acceptance, negative invalid/too large.
 * Shared AV1 parsing supplies one FRAME or show-existing FRAME_HEADER OBU.
 * Rebuild size fields before publishing immutable sample storage. */
static INLINE int videotoolbox_av1_packet_append(videotoolbox_av1_packet_t *p,
      const uint8_t *data, size_t size, bool display, int slot)
{
   rav1_obu_t obu;
   uint8_t header[10];
   size_t offset = 0, length, prefix, total;
   if (!p || !p->data || !data || !size || !p->capacity
         || p->capacity > 64u * 1024 * 1024 || p->size > p->capacity
         || p->count > 8 || (display && (slot < 0 || slot >= 32)))
      return -1;
   if (p->ready)
      return 1;
   if (rav1_obu_next(&obu, data, size, &offset, true) != 1 || offset != size
         || (obu.type != 6 && obu.type != 3) || obu.temporal_id
         || obu.spatial_id || !obu.payload_size || (obu.type == 3 && !display))
      return -1;
   prefix = 1;
   header[0] = (uint8_t)(data[0] | 2);
   if (data[0] & 4)
      header[prefix++] = data[1];
   length = obu.payload_size;
   do
   {
      if (prefix == sizeof(header))
         return -1;
      header[prefix++] = (uint8_t)((length & 127) | (length > 127 ? 128 : 0));
      length >>= 7;
   } while (length);
   if (obu.payload_size > p->capacity
         || prefix > p->capacity - obu.payload_size)
      return -1;
   total = prefix + obu.payload_size;
   if (p->count == 8 || total > p->capacity - p->size)
      return videotoolbox_av1_packet_seal(p) ? -1 : 1;
   memmove(p->data + p->size + prefix, obu.payload, obu.payload_size);
   memcpy(p->data + p->size, header, prefix);
   p->size += total;
   p->count++;
   if (display)
      p->display_slot = slot;
   if (display || p->count == 8)
      return videotoolbox_av1_packet_seal(p);
   return 0;
}
#endif
