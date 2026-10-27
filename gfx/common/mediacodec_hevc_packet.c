/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_hevc_packet.h"
#include <string.h>
#define MEDIACODEC_PACKET_MAX (64u * 1024u * 1024u)
static int mediacodec_hevc_packet_overlap(
      const void *a, size_t na, const void *b, size_t nb)
{
   uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
   if (na > (uintptr_t)-1 - x || nb > (uintptr_t)-1 - y)
      return 1;
   return x < y + nb && y < x + na;
}
static int mediacodec_hevc_packet_pack(const rh265_nal_t *nals, unsigned count,
      int parameter_sets, int type, int temporal, size_t coded, uint8_t *out,
      size_t capacity, size_t *needed)
{
   size_t total = 0, bytes = 0, offset = 0;
   unsigned i;
   if (!needed)
      return -1;
   *needed = 0;
   if (!nals || !count || (!out && capacity))
      return -1;
   for (i = 0; i < count; i++)
   {
      const rh265_nal_t *nal = &nals[i];
      unsigned nal_type;
      if (!nal->data || nal->size < 3 || nal->size > MEDIACODEC_PACKET_MAX - 4
            || (parameter_sets && nal->size > 65536))
         return -1;
      nal_type = (nal->data[0] >> 1) & 63;
      if ((nal->data[0] & 0x81) || (nal->data[1] >> 3) || !(nal->data[1] & 7))
         return -1;
      if (parameter_sets)
      {
         if (nal_type != 32 + i || (nal->data[1] & 7) != 1)
            return -1;
      }
      else if (!(nal_type <= 9 || (nal_type >= 16 && nal_type <= 21))
            || (int)nal_type != type || (int)(nal->data[1] & 7) - 1 != temporal)
         return -1;
      if (total > MEDIACODEC_PACKET_MAX - 4 - nal->size)
         return -1;
      total += 4 + nal->size;
      bytes += nal->size;
   }
   if (!parameter_sets && bytes != coded)
      return -1;
   if (!out || capacity < total)
   {
      *needed = total;
      return 0;
   }
   /* Each accepted NAL contributes at least seven bytes to the 64 MiB
    * bound, so descriptor byte multiplication also fits 32-bit size_t. */
   if (mediacodec_hevc_packet_overlap(
             out, total, nals, (size_t)count * sizeof(*nals)))
      return -1;
   for (i = 0; i < count; i++)
      if (mediacodec_hevc_packet_overlap(
                out, total, nals[i].data, nals[i].size))
         return -1;
   for (i = 0; i < count; i++)
   {
      out[offset++] = 0;
      out[offset++] = 0;
      out[offset++] = 0;
      out[offset++] = 1;
      memcpy(out + offset, nals[i].data, nals[i].size);
      offset += nals[i].size;
   }
   *needed = total;
   return 1;
}
int mediacodec_hevc_csd(const rh265_nal_t sets[3], uint8_t *output,
      size_t capacity, size_t *needed)
{
   return mediacodec_hevc_packet_pack(
         sets, 3, 1, 0, 0, 0, output, capacity, needed);
}
int mediacodec_hevc_packet(const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned count, uint8_t *output,
      size_t capacity, size_t *needed)
{
   if (needed)
      *needed = 0;
   if (!au || !au->num_slices || au->num_slices > count)
      return -1;
   return mediacodec_hevc_packet_pack(slices, au->num_slices, 0,
         au->first.nal_type, au->first.temporal_id, au->coded_bytes, output,
         capacity, needed);
}
