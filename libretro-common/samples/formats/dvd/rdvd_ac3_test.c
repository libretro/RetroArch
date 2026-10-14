/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_ac3.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static uint8_t storage[RDVD_AC3_BUFFER_BYTES];
static uint8_t bytes[256];

static rdvd_packet_t packet(const uint8_t *data, size_t size)
{
   rdvd_packet_t p;
   memset(&p, 0, sizeof(p));
   p.data = data; p.size = size; p.kind = RDVD_PACKET_AC3;
   p.frame_count = 1; p.first_access_unit = 1;
   return p;
}

static void test_splits(void)
{
   size_t split;
   for (split = 1; split < sizeof(bytes); split++)
   {
      rdvd_ac3_t audio;
      rdvd_ac3_frame_t frame;
      rdvd_packet_t p = packet(bytes, split);
      unsigned frames = 0;
      enum rdvd_ac3_result result;
      CHECK(rdvd_ac3_init(&audio, 0, storage, sizeof(storage)));
      CHECK(rdvd_ac3_feed(&audio, &p));
      CHECK(!rdvd_ac3_feed(&audio, &p));
      while ((result = rdvd_ac3_next(&audio, &frame)) == RDVD_AC3_FRAME)
      {
         CHECK(frame.data == bytes && frame.size == 128);
         frames++;
      }
      CHECK(result == RDVD_AC3_MORE && !frame.data && !audio.borrowing);
      p = packet(bytes + split, sizeof(bytes) - split);
      CHECK(rdvd_ac3_feed(&audio, &p));
      while ((result = rdvd_ac3_next(&audio, &frame)) == RDVD_AC3_FRAME)
      {
         CHECK(frame.size == 128 && frame.info.samples == 1536);
         CHECK(frame.info.sample_rate == 48000 && frame.info.channels == 2);
         CHECK(!memcmp(frame.data, bytes + frames * 128, 128));
         if (split % 128 && frames == (unsigned)(split / 128))
            CHECK(frame.data == storage);
         else
            CHECK(frame.data == bytes + frames * 128);
         frames++;
      }
      CHECK(result == RDVD_AC3_MORE && frames == 2 && !audio.used);
   }
}

static void test_maximum_frame(void)
{
   static uint8_t maximum[RDVD_AC3_BUFFER_BYTES];
   rdvd_ac3_t audio;
   rdvd_ac3_frame_t frame;
   size_t i;
   memcpy(maximum, bytes, 8);
   maximum[4] = 0xa4; /* 32 kHz, 640 kbit/s. */
   CHECK(rdvd_ac3_init(&audio, 7, storage, sizeof(storage)));
   for (i = 0; i < sizeof(maximum); i++)
   {
      rdvd_packet_t p = packet(maximum + i, 1);
      enum rdvd_ac3_result result;
      p.index = 7;
      if (i) p.frame_count = 0;
      CHECK(rdvd_ac3_feed(&audio, &p));
      result = rdvd_ac3_next(&audio, &frame);
      if (i + 1 == sizeof(maximum))
      {
         CHECK(result == RDVD_AC3_FRAME && frame.data == storage);
         CHECK(frame.size == sizeof(maximum) && frame.info.sample_rate == 32000);
         CHECK(!memcmp(frame.data, maximum, sizeof(maximum)));
         CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_MORE);
      }
      else
         CHECK(result == RDVD_AC3_MORE && audio.used == i + 1);
   }
   CHECK(!audio.used && !audio.borrowing);
}

static void test_contracts(void)
{
   rdvd_ac3_t audio;
   rdvd_ac3_frame_t frame;
   rdvd_packet_t p = packet(bytes, sizeof(bytes));
   uint8_t prefix[131];
   CHECK(!rdvd_ac3_init(NULL, 0, storage, sizeof(storage)));
   CHECK(!rdvd_ac3_init(&audio, 8, storage, sizeof(storage)));
   CHECK(!rdvd_ac3_init(&audio, 0, storage, sizeof(storage) - 1));
   CHECK(rdvd_ac3_init(&audio, 0, storage, sizeof(storage)));
   p.index = 1; CHECK(!rdvd_ac3_feed(&audio, &p));
   p.index = 0; p.kind = RDVD_PACKET_DTS; CHECK(!rdvd_ac3_feed(&audio, &p));
   p.kind = RDVD_PACKET_AC3; p.first_access_unit = 0;
   CHECK(!rdvd_ac3_feed(&audio, &p));
   p.first_access_unit = 258; CHECK(!rdvd_ac3_feed(&audio, &p));
   CHECK(!audio.synchronized);
   p.frame_count = 0; CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_MORE && !audio.used);
   memset(prefix, 0xff, 3); memcpy(prefix + 3, bytes, 128);
   p = packet(prefix, sizeof(prefix)); p.first_access_unit = 4;
   CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_FRAME);
   CHECK(frame.data == prefix + 3);
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_MORE);
   rdvd_ac3_reset(&audio);
   p = packet(bytes, 1); CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_MORE && audio.used == 1);
   rdvd_ac3_reset(&audio); CHECK(!audio.used && !audio.synchronized);
   prefix[0] = 0xff; p = packet(prefix, 128);
   CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_ERROR && !frame.data);
   CHECK(!rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_ERROR);
   rdvd_ac3_reset(&audio); memcpy(prefix, bytes, 128); prefix[5] = 0x50;
   CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_ERROR);
   rdvd_ac3_reset(&audio); p = packet(bytes, 128);
   CHECK(rdvd_ac3_feed(&audio, &p));
   CHECK(rdvd_ac3_next(&audio, NULL) == RDVD_AC3_ERROR && audio.borrowing);
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_FRAME);
   CHECK(rdvd_ac3_next(&audio, &frame) == RDVD_AC3_MORE);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(bytes); i++) bytes[i] = (uint8_t)i;
   for (i = 0; i < 256; i += 128)
   {
      bytes[i] = 0x0b; bytes[i+1] = 0x77; bytes[i+4] = 0;
      bytes[i+5] = 0x40; bytes[i+6] = 0x40; bytes[i+7] = 0xf8;
   }
   test_splits(); test_maximum_frame(); test_contracts();
   printf("DVD AC3 tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
