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

int main(void)
{
   test_private_streams();
   printf("DVD packet tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
