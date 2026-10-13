/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_packet.h>

static uint8_t pci[RDVD_PCI_BYTES - 1];
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

static rdvd_packet_t fixture(unsigned groups, unsigned count)
{
   rdvd_packet_t p;
   memset(pci, 0, sizeof(pci));
   memset(&p, 0, sizeof(p));
   pci[97] = 1;
   pci[110] = (uint8_t)(groups << 4);
   pci[113] = (uint8_t)count;
   p.data = pci;
   p.size = sizeof(pci);
   p.kind = RDVD_PACKET_PCI;
   return p;
}

static void test_highlight(void)
{
   rdvd_packet_t p;
   rdvd_highlight_t h;
   unsigned status, groups, count, n;
   for (status = 1; status <= 3; status++)
      for (groups = 0; groups <= 3; groups++)
         for (count = 0; count < 64; count++)
         {
            bool valid = !count || (groups && count <= 36 / groups);
            p = fixture(groups, count);
            pci[97] = (uint8_t)status;
            memset(&h, 0xff, sizeof(h));
            CHECK(rdvd_highlight_read(&p, &h) == valid);
            if (valid)
               CHECK(h.status == status && h.groups == groups && h.buttons == count);
            else
               CHECK(!h.status && !h.buttons && !h.groups);
         }
   p = fixture(3, 12);
   pci[98] = 0x80; pci[101] = 1;
   pci[102] = 0xfe; pci[103] = 0xdc; pci[104] = 0xba; pci[105] = 0x98;
   pci[106] = 0x12; pci[107] = 0x34; pci[108] = 0x56; pci[109] = 0x78;
   pci[110] |= 1; pci[111] = 0x24;
   pci[112] = 17; pci[114] = 12; pci[116] = 5; pci[117] = 12;
   CHECK(rdvd_highlight_read(&p, &h));
   CHECK(h.start_pts == UINT32_C(0x80000001) && h.end_pts == UINT32_C(0xfedcba98));
   CHECK(h.selection_end_pts == UINT32_C(0x12345678));
   CHECK(h.display_types[0] == 1 && h.display_types[1] == 2 && h.display_types[2] == 4);
   CHECK(h.button_offset == 17 && h.selectable_buttons == 12);
   CHECK(h.forced_select == 5 && h.forced_activate == 12);
   for (n = 0; n < sizeof(pci); n++)
   {
      p.size = n;
      CHECK(!rdvd_highlight_read(&p, &h) && !h.status);
   }
   p.size = sizeof(pci) + 1;
   CHECK(!rdvd_highlight_read(&p, &h));
   p = fixture(1, 3); pci[114] = 4;
   CHECK(!rdvd_highlight_read(&p, &h));
   p = fixture(1, 3); pci[116] = 4;
   CHECK(!rdvd_highlight_read(&p, &h));
   p = fixture(1, 3); pci[117] = 4;
   CHECK(!rdvd_highlight_read(&p, &h));
   pci[97] = 0; /* Inactive packets need no inherited button validation. */
   CHECK(rdvd_highlight_read(&p, &h) && !h.status && !h.buttons);
   p.kind = RDVD_PACKET_DSI;
   CHECK(!rdvd_highlight_read(&p, &h));
   CHECK(!rdvd_highlight_read(NULL, &h));
   CHECK(!rdvd_highlight_read(&p, NULL));
}

int main(void)
{
   test_highlight();
   printf("DVD highlight tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
