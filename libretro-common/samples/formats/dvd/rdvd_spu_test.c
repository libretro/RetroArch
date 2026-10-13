/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_spu.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static const uint8_t unit[] = {0, 9, 0, 4, 0, 0, 0, 4, 0xff};

static void test_framing(void)
{
   rdvd_spu_packet_t view;
   rdvd_spu_stream_t *stream = rdvd_spu_stream_new();
   uint8_t joined[sizeof(unit) * 2];
   size_t consumed;
   unsigned split, i;
   CHECK(stream != NULL);
   if (!stream) return;
   CHECK(rdvd_spu_packet_read(unit, sizeof(unit), &view) && view.data == unit);
   for (i = 0; i < sizeof(unit); i++) CHECK(!rdvd_spu_packet_read(unit, i, &view));
   CHECK(rdvd_spu_stream_feed(stream, unit, sizeof(unit), 90, &consumed, &view) == RDVD_SPU_READY);
   CHECK(consumed == sizeof(unit) && view.data == unit && view.pts == 90);
   for (split = 1; split < sizeof(unit); split++)
   {
      rdvd_spu_stream_reset(stream);
      CHECK(rdvd_spu_stream_feed(stream, unit, split, 100, &consumed, &view) == RDVD_SPU_MORE);
      CHECK(consumed == split && !view.data);
      CHECK(rdvd_spu_stream_feed(stream, unit + split, sizeof(unit) - split, 200,
               &consumed, &view) == RDVD_SPU_READY);
      CHECK(consumed == sizeof(unit) - split && view.pts == 100);
      CHECK(view.data != unit && !memcmp(view.data, unit, sizeof(unit)));
   }
   rdvd_spu_stream_reset(stream);
   for (i = 0; i < sizeof(unit); i++)
   {
      enum rdvd_spu_result expected = i + 1 == sizeof(unit) ? RDVD_SPU_READY : RDVD_SPU_MORE;
      CHECK(rdvd_spu_stream_feed(stream, unit + i, 1, 300 + i, &consumed, &view) == expected);
      CHECK(consumed == 1);
   }
   CHECK(view.pts == 300);
   memcpy(joined, unit, sizeof(unit)); memcpy(joined + sizeof(unit), unit, sizeof(unit));
   CHECK(rdvd_spu_stream_feed(stream, joined, sizeof(joined), 400, &consumed, &view) == RDVD_SPU_READY);
   CHECK(consumed == sizeof(unit) && view.data == joined);
   CHECK(rdvd_spu_stream_feed(stream, joined + consumed, sizeof(unit), 500, &consumed, &view) == RDVD_SPU_READY);
   CHECK(view.data == joined + sizeof(unit) && view.pts == 500);
   CHECK(rdvd_spu_stream_feed(stream, NULL, 0, 0, &consumed, &view) == RDVD_SPU_MORE);
   CHECK(!consumed && !view.data);
   rdvd_spu_stream_free(stream);
}

static void test_bad_input(void)
{
   uint8_t bad[sizeof(unit)];
   rdvd_spu_packet_t view;
   rdvd_spu_stream_t *stream = rdvd_spu_stream_new();
   size_t consumed;
   unsigned offset;
   CHECK(stream != NULL);
   if (!stream) return;
   memcpy(bad, unit, sizeof(unit));
   for (offset = 0; offset <= 255; offset++)
   {
      bad[3] = (uint8_t)offset;
      CHECK(rdvd_spu_packet_read(bad, sizeof(bad), &view) == (offset == 4));
   }
   bad[1] = 0;
   CHECK(rdvd_spu_stream_feed(stream, bad, sizeof(bad), 0, &consumed, &view) == RDVD_SPU_ERROR);
   CHECK(!consumed && !view.data);
   CHECK(rdvd_spu_stream_feed(stream, unit, 1, 1, &consumed, &view) == RDVD_SPU_MORE);
   rdvd_spu_stream_reset(stream);
   CHECK(rdvd_spu_stream_feed(stream, unit, sizeof(unit), 2, &consumed, &view) == RDVD_SPU_READY);
   CHECK(view.data == unit && view.pts == 2);
   CHECK(rdvd_spu_stream_feed(stream, NULL, 1, 0, &consumed, &view) == RDVD_SPU_ERROR);
   rdvd_spu_stream_free(stream);
   rdvd_spu_stream_free(NULL);
}

static void test_controls(void)
{
   static const uint8_t input[] = {
      0, 32, 0, 6, 0x90, 0xa0, 0, 1, 0, 6,
      0, 1, 3, 0x12, 0x34, 4, 0x56, 0x78,
      5, 0, 0x10, 3, 0, 0x20, 4, 6, 0, 4, 0, 5, 2, 0xff
   };
   static const unsigned commands[] = {0, 1, 3, 4, 5, 6, 2};
   uint8_t bad[sizeof(input)];
   rdvd_spu_packet_t packet;
   rdvd_spu_control_t c;
   rdvd_spu_event_t e;
   unsigned i, n;
   CHECK(rdvd_spu_packet_read(input, sizeof(input), &packet));
   CHECK(rdvd_spu_control_begin(&packet, 0, &c));
   CHECK(c.delay == 1 && c.next_offset == 6);
   for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
   {
      CHECK(rdvd_spu_control_next(&c, &e) == RDVD_SPU_READY && e.command == commands[i]);
      if (e.command == 3 || e.command == 4)
         for (n = 0; n < 4; n++) CHECK(e.values[n] == (e.command == 3 ? 4u : 8u) - n);
      if (e.command == 5)
         CHECK(e.values[0] == 1 && e.values[1] == 3 && e.values[2] == 2 && e.values[3] == 4);
      if (e.command == 6) CHECK(e.values[0] == 4 && e.values[1] == 5);
   }
   CHECK(rdvd_spu_control_next(&c, &e) == RDVD_SPU_MORE);
   for (n = 11; n < sizeof(input); n++)
   {
      enum rdvd_spu_result result;
      memcpy(bad, input, sizeof(input)); bad[1] = (uint8_t)n;
      CHECK(rdvd_spu_packet_read(bad, n, &packet));
      CHECK(rdvd_spu_control_begin(&packet, 0, &c));
      do { result = rdvd_spu_control_next(&c, &e); } while (result == RDVD_SPU_READY);
      CHECK(result == RDVD_SPU_ERROR);
   }
   memcpy(bad, input, sizeof(input)); bad[10] = 7;
   CHECK(rdvd_spu_packet_read(bad, sizeof(bad), &packet));
   CHECK(rdvd_spu_control_begin(&packet, 0, &c));
   CHECK(rdvd_spu_control_next(&c, &e) == RDVD_SPU_UNSUPPORTED);
   bad[9] = 5;
   CHECK(!rdvd_spu_control_begin(&packet, 0, &c));
   bad[9] = 7;
   CHECK(!rdvd_spu_control_begin(&packet, 0, &c));
   CHECK(!rdvd_spu_control_begin(NULL, 0, &c));
   CHECK(!rdvd_spu_control_begin(&packet, 0, NULL));
}

int main(void)
{
   test_framing(); test_bad_input();
   test_controls();
   printf("DVD SPU tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
