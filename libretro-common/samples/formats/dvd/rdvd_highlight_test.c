/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_packet.h>
#include <formats/rmpeg1_ps.h>

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

static void fill_buttons(void)
{
   static const uint8_t geometry[10] = {
      0x85, 0x50, 0xaa, 0x45, 0x60, 0xbb, 1, 2, 3, 4
   };
   unsigned i, n;
   for (i = 0; i < 36; i++)
   {
      uint8_t *p = pci + 142 + 18 * i;
      memcpy(p, geometry, sizeof(geometry));
      for (n = 0; n < 8; n++) p[10 + n] = (uint8_t)(i + n);
   }
}

static void test_buttons(void)
{
   rdvd_packet_t p;
   rdvd_button_t b;
   unsigned groups, group, number, n;
   for (groups = 1; groups <= 3; groups++)
   {
      p = fixture(groups, 36 / groups);
      fill_buttons();
      for (group = 0; group < groups; group++)
         for (number = 1; number <= 36 / groups; number++)
         {
            unsigned slot = group * (36 / groups) + number - 1;
            CHECK(rdvd_button_read(&p, group, number, &b));
            CHECK(b.x_start == 85 && b.x_end == 170 && b.color == 2);
            CHECK(b.y_start == 86 && b.y_end == 187 && b.auto_action == 1);
            CHECK(b.up == 1 && b.down == 2 && b.left == 3 && b.right == 4);
            CHECK(b.command == pci + 142 + slot * 18 + 10);
            for (n = 0; n < 8; n++) CHECK(b.command[n] == slot + n);
         }
      CHECK(!rdvd_button_read(&p, groups, 1, &b) && !b.command);
      CHECK(!rdvd_button_read(&p, 0, 0, &b));
      CHECK(!rdvd_button_read(&p, 0, 36 / groups + 1, &b));
      CHECK(!rdvd_button_read(&p, (unsigned)-1, 1, &b));
      CHECK(!rdvd_button_read(&p, 0, (unsigned)-1, &b));
   }
   for (n = 0; n < 4; n++)
   {
      p = fixture(1, 4); fill_buttons();
      pci[142 + 6 + n] = 5;
      CHECK(!rdvd_button_read(&p, 0, 1, &b) && !b.command);
   }
   p = fixture(1, 4); fill_buttons(); pci[142 + 2] = 1;
   CHECK(!rdvd_button_read(&p, 0, 1, &b));
   p = fixture(1, 4); fill_buttons(); pci[142 + 5] = 1;
   CHECK(!rdvd_button_read(&p, 0, 1, &b));
   p = fixture(1, 4); fill_buttons(); pci[142 + 3] = 0x85;
   CHECK(!rdvd_button_read(&p, 0, 1, &b));
   p = fixture(1, 4); fill_buttons();
   pci[142] = 0xbf; pci[143] = 0xf3; pci[144] = 0xff;
   pci[145] = 0x7f; pci[146] = 0xf3; pci[147] = 0xff;
   CHECK(rdvd_button_read(&p, 0, 1, &b));
   CHECK(b.x_start == 1023 && b.x_end == 1023 && b.y_start == 1023 && b.y_end == 1023);
   p = fixture(1, 4); fill_buttons(); pci[97] = 0;
   CHECK(!rdvd_button_read(&p, 0, 1, &b));
   CHECK(!rdvd_button_read(NULL, 0, 1, &b));
   CHECK(!rdvd_button_read(&p, 0, 1, NULL));
}

static void test_colors(void)
{
   rdvd_packet_t p = fixture(1, 4);
   rdvd_button_color_t c;
   unsigned color, state, i;
   for (color = 1; color <= 3; color++)
      for (state = 0; state < 2; state++)
      {
         unsigned offset = 118 + (color - 1) * 8 + state * 4;
         pci[offset] = (uint8_t)(0x10 + color); pci[offset + 1] = (uint8_t)(0x30 + state);
         pci[offset + 2] = (uint8_t)(0x50 + color); pci[offset + 3] = (uint8_t)(0x70 + state);
         CHECK(rdvd_button_color_read(&p, color, state != 0, &c));
         for (i = 0; i < 4; i++)
         {
            CHECK(c.indices[i] == (i == 0 ? state : i == 1 ? 3 : i == 2 ? color : 1));
            CHECK(c.alpha[i] == (i == 0 ? state : i == 1 ? 7 : i == 2 ? color : 5));
         }
      }
   CHECK(!rdvd_button_color_read(&p, 0, false, &c) && !c.indices[0]);
   CHECK(!rdvd_button_color_read(&p, 4, false, &c));
   CHECK(!rdvd_button_color_read(&p, (unsigned)-1, false, &c));
   CHECK(!rdvd_button_color_read(NULL, 1, false, &c));
   CHECK(!rdvd_button_color_read(&p, 1, false, NULL));
}

static void test_input_lookup(void)
{
   rdvd_packet_t p = fixture(1, 4);
   unsigned result, direction;
   fill_buttons();
   for (direction = 0; direction < 4; direction++)
   {
      CHECK(rdvd_button_neighbor(&p, 0, 1, (enum rdvd_button_direction)direction, &result));
      CHECK(result == direction + 1);
   }
   pci[148] = 0;
   CHECK(rdvd_button_neighbor(&p, 0, 1, RDVD_BUTTON_UP, &result) && result == 1);
   pci[142 + 18 + 2] = 1; /* The referenced button has an inverted rectangle. */
   CHECK(!rdvd_button_neighbor(&p, 0, 1, RDVD_BUTTON_DOWN, &result) && !result);
   p = fixture(1, 36); fill_buttons(); pci[148] = 36;
   CHECK(rdvd_button_neighbor(&p, 0, 1, RDVD_BUTTON_UP, &result) && result == 36);
   CHECK(!rdvd_button_neighbor(&p, 0, 1, (enum rdvd_button_direction)-1, &result));
   CHECK(!rdvd_button_neighbor(&p, 0, 1, (enum rdvd_button_direction)4, &result));
   CHECK(!rdvd_button_neighbor(&p, 1, 1, RDVD_BUTTON_UP, &result));
   CHECK(!rdvd_button_neighbor(&p, 0, 0, RDVD_BUTTON_UP, &result));
   CHECK(!rdvd_button_neighbor(&p, 0, 1, RDVD_BUTTON_UP, NULL));
   p = fixture(2, 18); fill_buttons();
   pci[142] = 0; pci[143] = 0; pci[144] = 50;
   pci[145] = 0; pci[146] = 0; pci[147] = 50;
   CHECK(rdvd_button_find(&p, 0, 0, 0, &result) && result == 1);
   CHECK(rdvd_button_find(&p, 0, 100, 100, &result) && result == 2);
   CHECK(rdvd_button_find(&p, 1, 100, 100, &result) && result == 1);
   CHECK(rdvd_button_find(&p, 1, 85, 86, &result) && result == 1);
   CHECK(rdvd_button_find(&p, 1, 170, 187, &result) && result == 1);
   CHECK(!rdvd_button_find(&p, 1, 171, 187, &result) && !result);
   CHECK(!rdvd_button_find(&p, 1, 1024, 0, &result));
   CHECK(!rdvd_button_find(&p, 1, 0, (unsigned)-1, &result));
   CHECK(!rdvd_button_find(&p, 2, 100, 100, &result));
   CHECK(!rdvd_button_find(NULL, 0, 100, 100, &result));
   CHECK(!rdvd_button_find(&p, 0, 100, 100, NULL));
   pci[144] = 1; pci[143] = 0x50;
   CHECK(!rdvd_button_find(&p, 0, 100, 100, &result) && !result);
}

static void test_pes_highlight(void)
{
   static uint8_t input[6 + RDVD_PCI_BYTES];
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rmpeg1_ps_packet_t pes;
   rdvd_packet_t p;
   rdvd_button_t b;
   unsigned split;
   CHECK(ps != NULL);
   if (!ps) return;
   p = fixture(3, 12);
   fill_buttons();
   memset(input, 0, sizeof(input));
   input[2] = 1; input[3] = 0xbf;
   input[4] = 3; input[5] = 0xd4;
   memcpy(input + 7, p.data, p.size);
   CHECK(rmpeg1_ps_borrow(ps, input, sizeof(input)) == sizeof(input));
   CHECK(rmpeg1_ps_next(ps, &pes) == 1);
   CHECK(rdvd_packet_read(pes.stream_id, pes.data, pes.size, &p));
   CHECK(rdvd_button_read(&p, 2, 12, &b) && b.command == input + 789);
   for (split = 1; split < sizeof(input); split++)
   {
      rmpeg1_ps_reset(ps);
      CHECK(rmpeg1_ps_borrow(ps, input, split) == split);
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
      CHECK(rmpeg1_ps_borrow(ps, input + split, sizeof(input) - split) == sizeof(input) - split);
      CHECK(rmpeg1_ps_next(ps, &pes) == 1);
      CHECK(rdvd_packet_read(pes.stream_id, pes.data, pes.size, &p));
      CHECK(rdvd_button_read(&p, 2, 12, &b));
      CHECK(b.command[0] == 35 && b.command[7] == 42);
   }
   rmpeg1_ps_free(ps);
}

int main(void)
{
   test_highlight();
   test_buttons();
   test_colors();
   test_input_lookup();
   test_pes_highlight();
   printf("DVD highlight tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
