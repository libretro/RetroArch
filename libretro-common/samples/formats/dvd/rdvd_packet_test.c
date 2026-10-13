/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_packet.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

static void test_private_streams(void)
{
   uint8_t bytes[12] = {0};
   rdvd_packet_t view;
   unsigned id, n;
   for (id = 0; id < 256; id++)
   {
      size_t header = 0;
      enum rdvd_packet_kind kind = RDVD_PACKET_NONE;
      if (id >= 0x20 && id <= 0x3f)
      {
         header = 1;
         kind = RDVD_PACKET_SUBPICTURE;
      }
      if (id >= 0x80 && id <= 0x8f)
      {
         header = 4;
         kind = id < 0x88 ? RDVD_PACKET_AC3 : RDVD_PACKET_DTS;
      }
      if (id >= 0xa0 && id <= 0xa7)
      {
         header = 7;
         kind = RDVD_PACKET_LPCM;
      }
      bytes[0] = (uint8_t)id;
      bytes[1] = 3;
      bytes[2] = 0xab;
      bytes[3] = 0xcd;
      for (n = 0; n < sizeof(bytes); n++)
      {
         bool valid = header && n >= header;
         memset(&view, 0xff, sizeof(view));
         CHECK(rdvd_packet_read(0xbd, bytes, n, &view) == valid);
         if (valid)
         {
            CHECK(view.data == bytes + header && view.size == n - header);
            CHECK(view.kind == kind);
            CHECK(view.index == (kind == RDVD_PACKET_SUBPICTURE ? id - 0x20 : id & 7));
            if (header >= 4)
               CHECK(view.frame_count == 3 && view.first_access_unit == 0xabcd);
         }
         else
            CHECK(!view.data && !view.size && view.kind == RDVD_PACKET_NONE);
      }
   }
   CHECK(!rdvd_packet_read(0xbd, NULL, 12, &view));
   CHECK(!rdvd_packet_read(0xbd, bytes, 12, NULL));
   CHECK(!rdvd_packet_read(0xe0, bytes, 12, &view));
   CHECK(!view.data && view.kind == RDVD_PACKET_NONE);
}

static void test_lpcm(void)
{
   uint8_t bytes[8] = {0xa3, 1, 0, 0, 0, 0, 0x80, 0x12};
   rdvd_packet_t view;
   unsigned config;
   for (config = 0; config < 256; config++)
   {
      unsigned depth = config / 64;
      unsigned rate = (config / 16) % 4;
      bool valid = depth < 3 && rate < 2;
      bytes[5] = (uint8_t)config;
      CHECK(rdvd_packet_read(0xbd, bytes, sizeof(bytes), &view) == valid);
      if (valid)
      {
         CHECK(view.sample_bits == 16 + depth * 4);
         CHECK(view.sample_rate == (rate ? 96000u : 48000u));
         CHECK(view.channels == 1 + config % 8);
         CHECK(view.index == 3 && view.data == bytes + 7 && view.size == 1);
      }
      else
         CHECK(!view.data && !view.sample_rate && !view.channels);
   }
}

static void test_nav(void)
{
   static uint8_t bytes[RDVD_DSI_BYTES + 2];
   rdvd_packet_t view;
   rdvd_nav_t nav;
   unsigned id, n;
   for (id = 0; id < 3; id++)
   {
      bytes[0] = (uint8_t)id;
      for (n = 0; n <= sizeof(bytes); n++)
      {
         bool valid = (id == 0 && n == RDVD_PCI_BYTES)
            || (id == 1 && n == RDVD_DSI_BYTES);
         CHECK(rdvd_packet_read(0xbf, bytes, n, &view) == valid);
         if (valid)
            CHECK(view.data == bytes + 1 && view.size == n - 1);
         else
            CHECK(!view.data && view.kind == RDVD_PACKET_NONE);
      }
   }
   bytes[0] = 0;
   bytes[1] = 0x12; bytes[2] = 0x34; bytes[3] = 0x56; bytes[4] = 0x78;
   bytes[13] = 0xfe; bytes[14] = 0xdc; bytes[15] = 0xba; bytes[16] = 0x98;
   bytes[17] = 0x80; bytes[18] = 0x00; bytes[19] = 0x00; bytes[20] = 0x01;
   CHECK(rdvd_packet_read(0xbf, bytes, RDVD_PCI_BYTES, &view));
   CHECK(rdvd_nav_read(&view, &nav));
   CHECK(nav.kind == RDVD_PACKET_PCI && nav.sector == UINT32_C(0x12345678));
   CHECK(nav.start_pts == UINT32_C(0xfedcba98) && nav.end_pts == UINT32_C(0x80000001));
   CHECK(!nav.scr && !nav.vob_id && !nav.cell_id);
   memset(bytes, 0, sizeof(bytes));
   bytes[0] = 1; bytes[1] = 0x80; bytes[4] = 1;
   bytes[5] = 0x12; bytes[8] = 0x34; bytes[12] = 0x56;
   bytes[25] = 0xab; bytes[26] = 0xcd; bytes[28] = 0xef;
   CHECK(rdvd_packet_read(0xbf, bytes, RDVD_DSI_BYTES, &view));
   CHECK(rdvd_nav_read(&view, &nav));
   CHECK(nav.kind == RDVD_PACKET_DSI && nav.scr == UINT32_C(0x80000001));
   CHECK(nav.sector == UINT32_C(0x12000034) && nav.end_sector_offset == 0x56);
   CHECK(nav.vob_id == 0xabcd && nav.cell_id == 0xef && !nav.start_pts);
   view.size--;
   CHECK(!rdvd_nav_read(&view, &nav) && nav.kind == RDVD_PACKET_NONE);
   CHECK(!rdvd_nav_read(NULL, &nav) && !nav.sector);
   CHECK(!rdvd_nav_read(&view, NULL));
}

int main(void)
{
   test_private_streams();
   test_lpcm();
   test_nav();
   printf("DVD packet tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
