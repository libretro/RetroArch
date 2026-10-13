/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rdvd_packet.h>
#include <formats/rdvd_demux.h>
#include <formats/rmpeg1_ps.h>

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

static void check_pes(const rmpeg1_ps_packet_t *pes)
{
   rdvd_packet_t dvd;
   CHECK(pes->type == RMPEG1_PS_PRIVATE_1 && pes->stream_id == 0xbd);
   CHECK(pes->pts == 90000 && pes->dts == 45000);
   CHECK(rdvd_packet_read(pes->stream_id, pes->data, pes->size, &dvd));
   CHECK(dvd.kind == RDVD_PACKET_AC3 && dvd.index == 0);
   CHECK(dvd.data == pes->data + 4 && dvd.size == 2);
   CHECK(dvd.data[0] == 0x0b && dvd.data[1] == 0x77);
}

static void test_program_stream(void)
{
   static const uint8_t input[] = {
      0, 0, 1, 0xbd, 0, 19, 0x80, 0xc0, 10,
      0x31, 0, 5, 0xbf, 0x21, 0x11, 0, 3, 0x5f, 0x91,
      0x80, 1, 0, 1, 0x0b, 0x77
   };
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rmpeg1_ps_packet_t pes;
   unsigned split;
   CHECK(ps != NULL);
   if (!ps) return;
   CHECK(rmpeg1_ps_borrow(ps, input, sizeof(input)) == sizeof(input));
   CHECK(rmpeg1_ps_next(ps, &pes) == 1);
   CHECK(pes.data == input + 19);
   check_pes(&pes);
   CHECK(rmpeg1_ps_next(ps, &pes) == 0);
   for (split = 1; split < sizeof(input); split++)
   {
      rmpeg1_ps_reset(ps);
      CHECK(rmpeg1_ps_borrow(ps, input, split) == split);
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
      CHECK(rmpeg1_ps_borrow(ps, input + split, sizeof(input) - split) == sizeof(input) - split);
      CHECK(rmpeg1_ps_next(ps, &pes) == 1);
      check_pes(&pes);
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
   }
   rmpeg1_ps_reset(ps);
   for (split = 0; split < sizeof(input); split++)
   {
      CHECK(rmpeg1_ps_write(ps, input + split, 1) == 1);
      CHECK(rmpeg1_ps_next(ps, &pes) == (split == sizeof(input) - 1));
   }
   check_pes(&pes);
   rmpeg1_ps_free(ps);
}

static void test_timestamp_damage(void)
{
   static const uint8_t good[] = {
      0, 0, 1, 0xbd, 0, 19, 0x80, 0xc0, 10,
      0x31, 0, 5, 0xbf, 0x21, 0x11, 0, 3, 0x5f, 0x91,
      0x80, 1, 0, 1, 0x0b, 0x77
   };
   static const unsigned markers[] = {9, 11, 13, 14, 16, 18};
   uint8_t input[sizeof(good) * 2];
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rmpeg1_ps_packet_t pes;
   unsigned i;
   CHECK(ps != NULL);
   if (!ps) return;
   for (i = 0; i < 8; i++)
   {
      memcpy(input, good, sizeof(good));
      memcpy(input + sizeof(good), good, sizeof(good));
      if (i < 6) input[markers[i]] &= 0xfe;
      else input[i == 6 ? 9 : 14] ^= 0x10;
      rmpeg1_ps_reset(ps);
      CHECK(rmpeg1_ps_borrow(ps, input, sizeof(input)) == sizeof(input));
      CHECK(rmpeg1_ps_next(ps, &pes) == 1);
      CHECK(pes.data == input + sizeof(good) + 19);
      check_pes(&pes);
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
   }
   rmpeg1_ps_free(ps);
}

static void test_mpeg1_timestamps(void)
{
   static const uint8_t good[] = {
      0, 0, 1, 0xbd, 0, 16,
      0x31, 0, 5, 0xbf, 0x21, 0x11, 0, 3, 0x5f, 0x91,
      0x80, 1, 0, 1, 0x0b, 0x77
   };
   static const uint8_t pts_only[] = {
      0, 0, 1, 0xbd, 0, 11,
      0x2f, 0xff, 0xff, 0xff, 0xff, 0x80, 1, 0, 1, 0x0b, 0x77
   };
   static const unsigned markers[] = {6, 8, 10, 11, 13, 15};
   uint8_t input[sizeof(good) * 2];
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rmpeg1_ps_packet_t pes;
   unsigned i;
   CHECK(ps != NULL);
   if (!ps) return;
   CHECK(rmpeg1_ps_borrow(ps, good, sizeof(good)) == sizeof(good));
   CHECK(rmpeg1_ps_next(ps, &pes) == 1);
   check_pes(&pes);
   for (i = 0; i < 7; i++)
   {
      memcpy(input, good, sizeof(good));
      memcpy(input + sizeof(good), good, sizeof(good));
      if (i < 6) input[markers[i]] &= 0xfe;
      else input[11] ^= 0x10;
      rmpeg1_ps_reset(ps);
      CHECK(rmpeg1_ps_borrow(ps, input, sizeof(input)) == sizeof(input));
      CHECK(rmpeg1_ps_next(ps, &pes) == 1);
      CHECK(pes.data == input + sizeof(good) + 16);
      check_pes(&pes);
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
   }
   rmpeg1_ps_reset(ps);
   CHECK(rmpeg1_ps_borrow(ps, pts_only, sizeof(pts_only)) == sizeof(pts_only));
   CHECK(rmpeg1_ps_next(ps, &pes) == 1);
   CHECK(pes.pts == (((uint64_t)1 << 33) - 1) && pes.dts == RMPEG1_PS_NO_PTS);
   rmpeg1_ps_free(ps);
}

static void test_nav_program_stream(void)
{
   static uint8_t input[6 + RDVD_DSI_BYTES];
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rmpeg1_ps_packet_t pes;
   rdvd_packet_t view;
   rdvd_nav_t nav;
   unsigned id;
   CHECK(ps != NULL);
   if (!ps) return;
   for (id = 0; id < 2; id++)
   {
      size_t size = id ? RDVD_DSI_BYTES : RDVD_PCI_BYTES;
      memset(input, 0, sizeof(input));
      input[2] = 1; input[3] = 0xbf;
      input[4] = (uint8_t)(size >> 8); input[5] = (uint8_t)size;
      input[6] = (uint8_t)id;
      rmpeg1_ps_reset(ps);
      CHECK(rmpeg1_ps_borrow(ps, input, size + 6) == size + 6);
      CHECK(rmpeg1_ps_next(ps, &pes) == 1 && pes.type == RMPEG1_PS_PRIVATE_2);
      CHECK(pes.data == input + 6 && pes.size == size);
      CHECK(rdvd_packet_read(pes.stream_id, pes.data, pes.size, &view));
      CHECK(view.data == input + 7 && rdvd_nav_read(&view, &nav));
      CHECK(nav.kind == (id ? RDVD_PACKET_DSI : RDVD_PACKET_PCI));
      CHECK(rmpeg1_ps_next(ps, &pes) == 0);
   }
   rmpeg1_ps_free(ps);
}

static void test_vob(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *input;
   long length;
   rmpeg1_ps_t *ps;
   rmpeg1_ps_packet_t pes;
   unsigned video = 0, audio = 0, pci = 0, dsi = 0;
   CHECK(file != NULL);
   if (!file) return;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || length > 8 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
   {
      CHECK(false);
      fclose(file);
      return;
   }
   input = (uint8_t*)malloc((size_t)length);
   CHECK(input != NULL);
   if (!input)
   {
      fclose(file);
      return;
   }
   if (fread(input, 1, (size_t)length, file) != (size_t)length)
   {
      CHECK(false);
      free(input);
      fclose(file);
      return;
   }
   CHECK(fclose(file) == 0);
   ps = rmpeg1_ps_init(0);
   CHECK(ps != NULL);
   if (ps)
   {
      CHECK(rmpeg1_ps_borrow(ps, input, (size_t)length) == (size_t)length);
      while (rmpeg1_ps_next(ps, &pes))
      {
         rdvd_packet_t dvd;
         rdvd_nav_t nav;
         if (pes.type == RMPEG1_PS_VIDEO)
            video++;
         if (pes.type != RMPEG1_PS_PRIVATE_1 && pes.type != RMPEG1_PS_PRIVATE_2)
            continue;
         CHECK(rdvd_packet_read(pes.stream_id, pes.data, pes.size, &dvd));
         if (dvd.kind == RDVD_PACKET_AC3) audio++;
         if (dvd.kind == RDVD_PACKET_PCI || dvd.kind == RDVD_PACKET_DSI)
         {
            rdvd_highlight_t highlight;
            CHECK(dvd.data == pes.data + 1 && rdvd_nav_read(&dvd, &nav));
            if (dvd.kind == RDVD_PACKET_PCI)
               CHECK(rdvd_highlight_read(&dvd, &highlight));
            if (dvd.kind == RDVD_PACKET_PCI) pci++;
            else dsi++;
         }
      }
      CHECK(video && audio && pci && dsi);
      printf("VOB packets: video=%u AC3=%u PCI=%u DSI=%u\n", video, audio, pci, dsi);
      rmpeg1_ps_free(ps);
   }
   free(input);
}

static void test_demux_bridge(void)
{
   uint8_t data[30] = {0,0,1,0xbd,0,12,0x80,0,0,0x80,1,0,0,0x11,0x22,0x33,0x44,0x55,
      0,0,1,0xe0,0,6,0x80,0,0,0xaa,0xbb,0xcc};
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rdvd_demux_t demux;
   rdvd_demux_packet_t packet;
   CHECK(ps != NULL); if (!ps) return;
   CHECK(rdvd_demux_init(&demux, ps));
   CHECK(rdvd_demux_feed(&demux, data, sizeof(data)));
   CHECK(!rdvd_demux_feed(&demux, data, sizeof(data)));
   CHECK(rdvd_demux_next(&demux, &packet));
   CHECK(packet.dvd_valid && packet.dvd.kind == RDVD_PACKET_AC3);
   CHECK(packet.dvd.data == data + 13 && packet.dvd.size == 5);
   CHECK(rdvd_demux_next(&demux, &packet));
   CHECK(!packet.dvd_valid && packet.pes.data == data + 27 && packet.pes.size == 3);
   CHECK(!rdvd_demux_next(&demux, &packet) && !demux.borrowing && !packet.pes.data);
   rdvd_demux_reset(&demux);
   CHECK(rdvd_demux_feed(&demux, data, 5));
   CHECK(!rdvd_demux_next(&demux, &packet));
   CHECK(rdvd_demux_feed(&demux, data + 5, sizeof(data) - 5));
   CHECK(rdvd_demux_next(&demux, &packet) && packet.dvd_valid && packet.dvd.data[0] == 0x11);
   rdvd_demux_reset(&demux);
   CHECK(!demux.borrowing && !rdvd_demux_next(&demux, &packet));
   rmpeg1_ps_free(ps);
}
int main(int argc, char **argv)
{
   test_demux_bridge();
   test_private_streams();
   test_lpcm();
   test_nav();
   test_program_stream();
   test_timestamp_damage();
   test_mpeg1_timestamps();
   test_nav_program_stream();
   if (argc == 2) test_vob(argv[1]);
   else CHECK(argc == 1);
   printf("DVD packet tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
