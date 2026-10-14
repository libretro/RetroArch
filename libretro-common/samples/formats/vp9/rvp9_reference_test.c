/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../formats/vp9/rvp9.c"

static unsigned bit_pos;
static uint8_t packet[64];

static void bits(unsigned value, unsigned count)
{
   while (count--)
   {
      packet[bit_pos >> 3] |= ((value >> count) & 1) << (7 - (bit_pos & 7));
      bit_pos++;
   }
}

static size_t inter_header(unsigned selected, unsigned slot, int explicit_size)
{
   unsigned i;
   memset(packet, 0, sizeof(packet));
   bit_pos = 0;
   bits(2, 2); bits(0, 2); bits(0, 1); /* marker, profile, show existing */
   bits(1, 1); bits(1, 1); bits(1, 1); /* inter, show, resilient */
   bits(0, 8);                       /* refresh flags */
   for (i = 0; i < 3; i++)
   {
      bits(i == selected ? slot : 0, 3);
      bits(0, 1);
   }
   for (i = 0; i < 3; i++)
   {
      bits(!explicit_size && i == selected, 1);
      if (!explicit_size && i == selected) break;
   }
   if (explicit_size) { bits(63, 16); bits(47, 16); }
   bits(0, 1);                       /* render size */
   bits(0, 1); bits(1, 1);           /* high precision, switchable */
   bits(0, 2);                       /* context */
   bits(0, 6); bits(0, 3); bits(0, 1); /* loop filter */
   bits(0, 8); bits(0, 3);           /* quantizer and deltas */
   bits(0, 1); bits(0, 1);           /* segmentation, tile rows */
   bits(1, 16);                      /* compressed header size */
   return (bit_pos + 7) / 8;
}

int main(void)
{
   rvp9_dec *d = (rvp9_dec*)calloc(1, sizeof(*d));
   unsigned selected, slot;
   int show, result, dims[RVP9_REF_FRAMES * 2];
   size_t len;
   rvp9_hdr hd;
   if (!d) return 1;
   for (slot = 0; slot < RVP9_REF_FRAMES; slot++)
   {
      dims[slot * 2] = 64 + slot * 8;
      dims[slot * 2 + 1] = 48 + slot * 8;
      d->ref_frame_map[slot] = slot;
      d->fbs[slot].valid = 1;
      d->fbs[slot].w = dims[slot * 2];
      d->fbs[slot].h = dims[slot * 2 + 1];
   }
   for (selected = 0; selected < 3; selected++)
      for (slot = 1; slot < RVP9_REF_FRAMES; slot++)
      {
         len = inter_header(selected, slot, 0);
         memset(&hd, 0, sizeof(hd));
         if (rvp9_parse_uncompressed(packet, len, &hd, dims)
               || hd.w != dims[slot * 2] || hd.h != dims[slot * 2 + 1])
            return 2;
         /* Other references deliberately differ: reject scaling before
          * touching reconstruction, after resolving the selected slot. */
         result = rvp9_decode_frame(d, packet, len, &show);
         if (result != -6 || show != -1 || d->hd.w != hd.w || d->hd.h != hd.h)
            return 3;
         dims[slot * 2] = 0;
         if (rvp9_parse_uncompressed(packet, len, &hd, dims) != -9) return 4;
         dims[slot * 2] = d->fbs[slot].w;
         dims[slot * 2 + 1] = 0;
         if (rvp9_parse_uncompressed(packet, len, &hd, dims) != -9) return 7;
         dims[slot * 2 + 1] = d->fbs[slot].h;
         d->ref_frame_map[slot] = -1;
         if (rvp9_decode_frame(d, packet, len, &show) != -9) return 5;
         d->ref_frame_map[slot] = RVP9_FRAME_BUFS;
         if (rvp9_decode_frame(d, packet, len, &show) != -9) return 8;
         d->ref_frame_map[slot] = slot;
      }
   len = inter_header(0, 0, 1);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL) || hd.w != 64 || hd.h != 48)
      return 6;
   free(d);
   puts("VP9 reference-slot size tests passed (21 selections)");
   return 0;
}
