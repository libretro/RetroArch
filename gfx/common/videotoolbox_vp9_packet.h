/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_PACKET_H
#define RETROARCH_VIDEOTOOLBOX_VP9_PACKET_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>

/* Owner-thread staging; accepted bytes survive borrowed input reuse.
 * The cold owner supplies storage, including the maximum 34-byte index. */
typedef struct videotoolbox_vp9_packet
{
   uint8_t *data;
   size_t capacity, size;
   uint32_t sizes[8];
   unsigned count;
   int ready, display_slot;
} videotoolbox_vp9_packet_t;

static INLINE bool videotoolbox_vp9_packet_init(
      videotoolbox_vp9_packet_t *p, uint8_t *data, size_t capacity)
{
   if (!p)
      return false;
   memset(p, 0, sizeof(*p));
   p->display_slot = -1;
   if (!data || capacity <= 34)
      return false;
   p->data     = data;
   p->capacity = capacity;
   return true;
}

/* Reset only after the native sample no longer borrows this storage. */
static INLINE void videotoolbox_vp9_packet_reset(videotoolbox_vp9_packet_t *p)
{
   if (!p)
      return;
   p->size         = 0;
   p->count        = 0;
   p->ready        = 0;
   p->display_slot = -1;
}

/* Seal at display, eight frames, or terminal input (including hidden-only).
 * The index uses four little-endian bytes per coded frame, VP9 Annex B. */
static INLINE int videotoolbox_vp9_packet_seal(videotoolbox_vp9_packet_t *p)
{
   unsigned i, byte;
   uint8_t marker;
   size_t offset;
   if (!p || !p->data || !p->count || p->count > 8)
      return -1;
   if (p->ready)
      return 0;
   if (p->count > 1)
   {
      offset            = p->size;
      marker            = (uint8_t)(0xc0 | (3 << 3) | (p->count - 1));
      p->data[offset++] = marker;
      for (i = 0; i < p->count; i++)
         for (byte = 0; byte < 4; byte++)
            p->data[offset++] = (uint8_t)(p->sizes[i] >> (byte * 8));
      p->data[offset++] = marker;
      p->size           = offset;
   }
   p->ready = 1;
   return 0;
}

/* 0 accepted, 1 sealed/busy without acceptance, negative invalid/too large.
 * A displayed frame (including show-existing) supplies its logical output slot.
 * Flush a full hidden-only batch before retrying the next coded frame. */
static INLINE int videotoolbox_vp9_packet_append(videotoolbox_vp9_packet_t *p,
      const uint8_t *data, size_t size, bool display, int slot)
{
   if (!p || !p->data || !data || !size || size > (uint32_t)-1
         || (display && (slot < 0 || slot >= 32)))
      return -1;
   if (p->ready)
      return 1;
   if (p->count >= 8 || p->capacity <= 34 || size > p->capacity - 34
         || p->size > p->capacity - 34)
      return -1;
   if (size > p->capacity - 34 - p->size)
   {
      if (videotoolbox_vp9_packet_seal(p))
         return -1;
      return 1;
   }
   memmove(p->data + p->size, data, size);
   p->sizes[p->count++] = (uint32_t)size;
   p->size += size;
   if (display)
      p->display_slot = slot;
   if (display || p->count == 8)
      return videotoolbox_vp9_packet_seal(p);
   return 0;
}
#endif
