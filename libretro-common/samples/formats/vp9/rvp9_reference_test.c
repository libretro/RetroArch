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

static unsigned explicit_w = 64, explicit_h = 48;

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
   if (explicit_size) { bits(explicit_w - 1, 16); bits(explicit_h - 1, 16); }
   bits(0, 1);                       /* render size */
   bits(0, 1); bits(1, 1);           /* high precision, switchable */
   bits(0, 2);                       /* context */
   bits(0, 6); bits(0, 3); bits(0, 1); /* loop filter */
   bits(0, 8); bits(0, 3);           /* quantizer and deltas */
   bits(0, 1);                       /* segmentation */
   if (explicit_size && explicit_w > 1024) bits(0, 1); /* tile cols */
   bits(0, 1);                       /* tile rows */
   bits(1, 16);                      /* compressed header size */
   return (bit_pos + 7) / 8;
}

static size_t key_header(int ten_bit, unsigned partition_size)
{
   memset(packet, 0, sizeof(packet));
   bit_pos = 0;
   bits(2, 2); bits(ten_bit ? 1 : 0, 2); bits(0, 1); /* profile 0 or 2 */
   bits(0, 1); bits(1, 1); bits(1, 1); /* key, show, resilient */
   bits(0x49, 8); bits(0x83, 8); bits(0x42, 8);
   if (ten_bit) bits(0, 1);
   bits(1, 3); bits(0, 1);           /* color space, range */
   bits(63, 16); bits(63, 16); bits(0, 1); /* size, render size */
   bits(0, 2);                       /* context */
   bits(0, 6); bits(0, 3); bits(0, 1); /* loop filter */
   bits(0, 8); bits(0, 3);           /* quantizer and deltas */
   bits(0, 1); bits(0, 1);           /* segmentation, tile rows */
   bits(partition_size, 16);
   return (bit_pos + 7) / 8;
}

static size_t sized_key_header(unsigned w, unsigned h)
{
   memset(packet, 0, sizeof(packet));
   bit_pos = 0;
   bits(2, 2); bits(0, 2); bits(0, 1); /* marker, profile, show existing */
   bits(0, 1); bits(1, 1); bits(1, 1); /* key, show, resilient */
   bits(0x49, 8); bits(0x83, 8); bits(0x42, 8);
   bits(1, 3); bits(0, 1);           /* color space, range */
   bits(w - 1, 16); bits(h - 1, 16); bits(0, 1); /* size, render size */
   bits(0, 2);                       /* context */
   bits(0, 6); bits(0, 3); bits(0, 1); /* loop filter */
   bits(0, 8); bits(0, 3);           /* quantizer and deltas */
   bits(0, 1); bits(0, 1); bits(0, 1); /* segmentation, tile cols, rows */
   bits(1, 16);                      /* compressed header size */
   return (bit_pos + 7) / 8;
}

/* A compressed header that does not fit must be refused before any
 * allocation, on both the 8-bit and the high-bit-depth path. */
static int truncated_partition_tests(rvp9_dec *d)
{
   int ten_bit, show;
   size_t len;
   for (ten_bit = 0; ten_bit < 2; ten_bit++)
   {
      len = key_header(ten_bit, 1024);  /* runs past the frame */
      if (rvp9_decode_frame(d, packet, len + 1, &show) != -12) return 1;
      len = key_header(ten_bit, 8);     /* leaves no tile data */
      if (rvp9_decode_frame(d, packet, len + 8, &show) != -12) return 1;
      len = key_header(ten_bit, 0);     /* absent */
      if (rvp9_decode_frame(d, packet, len + 8, &show) != -12) return 1;
      if (d->buf_y) return 1;
   }
   return 0;
}

/* The compressed header starts with a marker bit that must be zero. */
static int marker_bit_tests(void)
{
   int ten_bit;
   for (ten_bit = 0; ten_bit < 2; ten_bit++)
   {
      int show, result;
      size_t len = key_header(ten_bit, 8);
      rvp9_dec *d = (rvp9_dec*)calloc(1, sizeof(*d));
      if (!d) return 1;
      packet[len] = 0x80;
      result = rvp9_decode_frame(d, packet, len + 9, &show);
      rvp9_free(d);
      free(d);
      if (result != -11) return 1;
   }
   return 0;
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
   if (truncated_partition_tests(d)) return 9;
   if (marker_bit_tests()) return 10;

   /* Coded size limit, on both explicitly sized header forms. */
   len = sized_key_header(16832, 16832);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL)
         || hd.w != 16832 || hd.h != 16832
         || hd.first_partition_size != 1)
      return 11;
   len = sized_key_header(16833, 1);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL) != -1) return 12;
   len = sized_key_header(1, 16833);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL) != -1) return 13;
   len = sized_key_header(65536, 65536);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL) != -1) return 14;
   explicit_w = 16832; explicit_h = 16832;
   len = inter_header(0, 0, 1);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL)
         || hd.w != 16832 || hd.h != 16832
         || hd.first_partition_size != 1)
      return 15;
   explicit_w = 16833; explicit_h = 1;
   len = inter_header(0, 0, 1);
   if (rvp9_parse_uncompressed(packet, len, &hd, NULL) != -1) return 16;
   free(d);
   puts("VP9 reference-slot size tests passed (21 selections)");
   return 0;
}
