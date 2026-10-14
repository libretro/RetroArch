/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rlpcm.h>
#include <formats/rdvd_lpcm.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

static rlpcm_format_t format(unsigned channels, unsigned bits)
{
   rlpcm_format_t f;
   memset(&f, 0, sizeof(f));
   f.kind = RLPCM_KIND_DVD; f.bits = bits; f.channels = channels;
   f.big_endian = true; f.sample_rate = 48000;
   return f;
}

static void test_wire_vectors(void)
{
   const uint8_t packed20[10] = {0x12,0x34,0x56,0x78,0x80,0,0xff,0xff,0xab,0xcd};
   const uint8_t packed24[12] = {0x12,0x34,0x56,0x78,0x80,0,0xff,0xff,0xa1,0xb2,0xc3,0xd4};
   const uint8_t mono20[10] = {0x12,0x34,0x56,0x78,0xab,0x80,0,0xff,0xff,0xcd};
   const uint8_t mono24[12] = {0x12,0x34,0x56,0x78,0xa1,0xb2,0x80,0,0xff,0xff,0xc3,0xd4};
   const int32_t values20[4] = {0x1234a,0x5678b,-524276,-3};
   const int32_t values24[4] = {0x1234a1,0x5678b2,-8388413,-44};
   unsigned bits, channels, i;
   for (bits = 20; bits <= 24; bits += 4)
      for (channels = 1; channels <= 2; channels++)
      {
         rlpcm_format_t f = format(channels, bits);
         const uint8_t *wire = bits == 20 ? (channels == 1 ? mono20 : packed20)
            : (channels == 1 ? mono24 : packed24);
         const int32_t *values = bits == 20 ? values20 : values24;
         size_t bytes = bits == 20 ? 10 : 12;
         unsigned count = 4 / channels;
         float pcm[4]; uint8_t encoded[12]; int16_t pcm16[4];
         CHECK(rlpcm_decode_f32(&f, wire, bytes, pcm, count) == count);
         for (i = 0; i < 4; i++)
            CHECK(pcm[i] == (float)values[i] / (bits == 20 ? 524288.0f : 8388608.0f));
         CHECK(rlpcm_encode_f32(&f, pcm, count, encoded, bytes) == count);
         CHECK(!memcmp(encoded, wire, bytes));
         CHECK(rlpcm_decode_s16(&f, wire, bytes, pcm16, count) == count);
         CHECK(pcm16[0] == 0x1235 && pcm16[1] == 0x5679);
         CHECK(pcm16[2] == -32767 && pcm16[3] == 0);
      }
}

static void test_blocks(void)
{
   unsigned channels, bits;
   for (channels = 1; channels <= 8; channels++)
      for (bits = 16; bits <= 24; bits += 4)
      {
         rlpcm_format_t f = format(channels, bits);
         size_t bytes, n;
         unsigned frames, i;
         float input[32], output[32]; uint8_t wire[96]; int16_t out16[32];
         CHECK(rlpcm_dvd_block_shape(&f, &bytes, &frames));
         CHECK(bytes <= 84 && bytes * 8 == frames * channels * bits);
         for (i = 0; i < frames * channels; i++)
            input[i] = (float)((int)i - 16) / 32.0f;
         memset(wire, 0xee, sizeof(wire));
         CHECK(rlpcm_encode_f32(&f, input, frames, wire, bytes) == frames);
         CHECK(wire[bytes] == 0xee);
         CHECK(rlpcm_decode_f32(&f, wire, bytes, output, frames) == frames);
         CHECK(!memcmp(input, output, frames * channels * sizeof(float)));
         for (n = 0; n < bytes; n++)
         {
            CHECK(!rlpcm_decode_f32(&f, wire, n, output, frames));
            CHECK(!rlpcm_decode_s16(&f, wire, n, out16, frames));
            CHECK(!rlpcm_encode_f32(&f, input, frames, wire, n));
         }
         for (i = 0; i < frames; i++)
            CHECK(!rlpcm_decode_f32(&f, wire, bytes, output, i));
      }
   {
      rlpcm_format_t f = format(2, 24);
      const uint8_t maximum[12] = {0x7f,0xff,0x80,0,0x7f,0xff,0x80,0,0xff,0,0xff,0};
      int16_t out[4]; size_t bytes; unsigned frames;
      CHECK(rlpcm_decode_s16(&f, maximum, sizeof(maximum), out, 2) == 2);
      CHECK(out[0] == 32767 && out[1] == -32768);
      f.channels = 0; CHECK(!rlpcm_dvd_block_shape(&f, &bytes, &frames));
      CHECK(!bytes && !frames);
      f.channels = 9; CHECK(!rlpcm_dvd_block_shape(&f, &bytes, &frames));
      f.channels = 2; f.bits = 28; CHECK(!rlpcm_dvd_block_shape(&f, &bytes, &frames));
   }
}

static rdvd_packet_t packet(const uint8_t *data, size_t size,
      const rlpcm_format_t *f)
{
   rdvd_packet_t p;
   memset(&p, 0, sizeof(p));
   p.kind = RDVD_PACKET_LPCM; p.first_access_unit = 4;
   p.sample_bits = f->bits; p.channels = f->channels;
   p.sample_rate = f->sample_rate; p.data = data; p.size = size;
   return p;
}

static void test_packet_splits(void)
{
   unsigned channels, bits;
   for (channels = 1; channels <= 8; channels++)
      for (bits = 16; bits <= 24; bits += 4)
      {
         rlpcm_format_t f = format(channels, bits);
         size_t bytes, split;
         unsigned frames, i;
         uint8_t wire[168]; float input[64];
         CHECK(rlpcm_dvd_block_shape(&f, &bytes, &frames));
         for (i = 0; i < 2 * frames * channels; i++)
            input[i] = (float)((int)i - 32) / 64.0f;
         CHECK(rlpcm_encode_f32(&f, input, frames * 2, wire, bytes * 2) == frames * 2);
         for (split = 1; split < bytes * 2; split++)
         {
            rdvd_lpcm_t audio;
            rdvd_lpcm_block_t block;
            rdvd_packet_t p = packet(wire, split, &f);
            unsigned phase, blocks = 0;
            enum rdvd_lpcm_result result;
            CHECK(rdvd_lpcm_init(&audio, 0));
            for (phase = 0; phase < 2; phase++)
            {
               if (phase) { p = packet(wire + split, bytes * 2 - split, &f); p.first_access_unit = 0; }
               CHECK(rdvd_lpcm_feed(&audio, &p));
               CHECK(!rdvd_lpcm_feed(&audio, &p));
               while ((result = rdvd_lpcm_next(&audio, frames, &block)) == RDVD_LPCM_BLOCK)
               {
                  float output[32];
                  CHECK(block.size == bytes && block.frames == frames);
                  CHECK(!memcmp(block.data, wire + blocks * bytes, bytes));
                  if (phase && split % bytes && blocks == split / bytes)
                     CHECK(block.data == audio.buffer);
                  else
                     CHECK(block.data == wire + blocks * bytes);
                  CHECK(rlpcm_decode_f32(&block.format, block.data, block.size, output, frames) == frames);
                  CHECK(!memcmp(output, input + blocks * frames * channels, frames * channels * sizeof(float)));
                  blocks++;
               }
               CHECK(result == RDVD_LPCM_MORE && !block.data && !audio.borrowing);
            }
            CHECK(blocks == 2 && !audio.used);
         }
      }
}

static void test_packet_contracts(void)
{
   uint8_t bytes[24] = {0};
   rlpcm_format_t f = format(2, 24);
   rdvd_packet_t p = packet(bytes, sizeof(bytes), &f);
   rdvd_lpcm_t audio;
   rdvd_lpcm_block_t block;
   CHECK(!rdvd_lpcm_init(NULL, 0)); CHECK(!rdvd_lpcm_init(&audio, 8));
   CHECK(rdvd_lpcm_init(&audio, 0));
   p.index = 1; CHECK(!rdvd_lpcm_feed(&audio, &p)); p.index = 0;
   p.first_access_unit = 3; CHECK(!rdvd_lpcm_feed(&audio, &p));
   p.first_access_unit = 29; CHECK(!rdvd_lpcm_feed(&audio, &p));
   p.first_access_unit = 0; CHECK(rdvd_lpcm_feed(&audio, &p));
   CHECK(!audio.synchronized && rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_MORE);
   p.first_access_unit = 4; p.size = 1;
   CHECK(rdvd_lpcm_feed(&audio, &p));
   CHECK(rdvd_lpcm_next(&audio, 1, &block) == RDVD_LPCM_ERROR && audio.borrowing);
   CHECK(rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_MORE && audio.used == 1);
   p.sample_bits = 16; CHECK(!rdvd_lpcm_feed(&audio, &p) && audio.used == 1);
   rdvd_lpcm_reset(&audio); CHECK(!audio.used && !audio.configured);
   p.sample_bits = 24; p.size = sizeof(bytes); CHECK(rdvd_lpcm_feed(&audio, &p));
   CHECK(rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_BLOCK && block.size == 24 && block.frames == 4);
   CHECK(block.data == bytes);
   CHECK(rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_MORE);
   rdvd_lpcm_reset(&audio); p.first_access_unit = 7;
   CHECK(rdvd_lpcm_feed(&audio, &p));
   CHECK(rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_BLOCK && block.data == bytes + 3);
   CHECK(rdvd_lpcm_next(&audio, 4, &block) == RDVD_LPCM_MORE && audio.used == 9);
}

int main(void)
{
   test_wire_vectors(); test_blocks(); test_packet_splits(); test_packet_contracts();
   printf("DVD LPCM tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
