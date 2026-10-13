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

int main(void)
{
   test_framing(); test_bad_input();
   printf("DVD SPU tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
