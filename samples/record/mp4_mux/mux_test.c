/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../record/record_mux_mp4.c"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)
int main(void)
{
   static const uint8_t headers[] = {0,0,0,1,0x67,100,0,30,0x80,0,0,1,0x68,0x80};
   static const uint8_t frame[] = {0,0,1,0x65,0x80};
   static const uint8_t bad[] = {0,0,1,0x65,0x80,0,0,1};
   record_mp4_config_t c;
   record_mux_mp4_t *m;
   const void *bytes, *again;
   size_t size, again_size, i;
   uint8_t mutated[sizeof(headers)], audio[768] = {0x0b,0x77,0,0,0x14,0x40,0x40,0};
   memset(&c, 0, sizeof(c));
   CHECK(!record_mux_mp4_new(NULL)); CHECK(!record_mux_mp4_new(&c));
   c.headers = headers; c.headers_size = sizeof(headers); c.max_packet_size = 64;
   c.width = 64; c.height = 48; c.timescale = 30000; c.bit_depth = 8;
   CHECK(!record_mux_mp4_get(NULL, &bytes, &size));
   CHECK(!record_mux_mp4_release(NULL) && !record_mux_mp4_finish(NULL));
   record_mux_mp4_free(NULL);
   c.color_primaries = 9;
   CHECK(!record_mux_mp4_new(&c));
   c.transfer_characteristics = 16; c.matrix_coefficients = 9;
   m = record_mux_mp4_new(&c); CHECK(m != NULL);
   CHECK(record_mux_mp4_get(m, &bytes, &size));
   for (i = 4; i + 15 <= size; i++)
      if (!memcmp((const uint8_t*)bytes + i, "colrnclx", 8)) break;
   CHECK(i + 15 <= size);
   CHECK(!memcmp((const uint8_t*)bytes + i + 8, "\0\11\0\20\0\11\0", 7));
   record_mux_mp4_free(m);
   c.color_primaries = 65536; CHECK(!record_mux_mp4_new(&c));
   c.color_primaries = c.transfer_characteristics = c.matrix_coefficients = 0;
   c.full_range = true; CHECK(!record_mux_mp4_new(&c)); c.full_range = false;
   m = record_mux_mp4_new(&c); CHECK(m != NULL);
   CHECK(record_mux_mp4_get(m, &bytes, &size) && size > 500);
   CHECK(!record_mux_mp4_finish(m));
   CHECK(record_mux_mp4_push(m, 99, NULL, 0, 0, 0, false) == RECORD_MP4_BUSY);
   CHECK(record_mux_mp4_get(m, &again, &again_size));
   CHECK(bytes == again && size == again_size);
   CHECK(record_mux_mp4_release(m) && !record_mux_mp4_release(m));
   CHECK(!record_mux_mp4_get(m, &bytes, &size));
   CHECK(record_mux_mp4_push(m, 2, frame, sizeof(frame), 0, 1001, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 0, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 1001, false) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 1, bad, sizeof(bad), 0, 1001, true) == RECORD_MP4_ERROR);
   CHECK(m->sequence == 0 && !m->seen[0]);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), UINT64_MAX, 1, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), ((uint64_t)1 << 32) + 5, 1001, true) == RECORD_MP4_OK);
   CHECK(record_mux_mp4_get(m, &bytes, &size));
   CHECK(m->sequence == 1 && m->end[0] == ((uint64_t)1 << 32) + 1006);
   CHECK(m->data[60] == 0 && m->data[61] == 0 && m->data[62] == 0 && m->data[63] == 1);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 1, true) == RECORD_MP4_BUSY);
   CHECK(record_mux_mp4_release(m));
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 1001, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), m->end[0] + 99, 1001, false) == RECORD_MP4_OK);
   CHECK(record_mux_mp4_release(m));
   m->sequence = UINT32_MAX;
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), m->end[0], 1001, false) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_finish(m) && record_mux_mp4_finish(m));
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 1, true) == RECORD_MP4_ERROR);
   record_mux_mp4_free(m);
   c.max_packet_size = 1;
   m = record_mux_mp4_new(&c); CHECK(m != NULL); CHECK(record_mux_mp4_release(m));
   CHECK(record_mux_mp4_push(m, 1, frame, sizeof(frame), 0, 1, true) == RECORD_MP4_ERROR);
   record_mux_mp4_free(m); c.max_packet_size = 64;
   for (i = 0; i < sizeof(headers); i++)
   {
      c.headers_size = i; m = record_mux_mp4_new(&c); record_mux_mp4_free(m);
      memcpy(mutated, headers, sizeof(headers)); mutated[i] ^= 0xff;
      c.headers = mutated; c.headers_size = sizeof(headers);
      m = record_mux_mp4_new(&c); record_mux_mp4_free(m); c.headers = headers;
   }
   c.headers_size = sizeof(headers); c.ac3_header_size = 1;
   CHECK(!record_mux_mp4_new(&c)); c.ac3_header_size = 0;
   c.bit_depth = 10; CHECK(!record_mux_mp4_new(&c)); c.bit_depth = 8;
   c.codec = (enum record_mp4_codec)99; CHECK(!record_mux_mp4_new(&c)); c.codec = RECORD_MP4_H264;
   c.max_packet_size = 4096; c.ac3_header = audio; c.ac3_header_size = sizeof(audio);
   m = record_mux_mp4_new(&c); CHECK(m != NULL); CHECK(record_mux_mp4_release(m));
   CHECK(record_mux_mp4_push(m, 2, audio, sizeof(audio)-1, 0, 1536, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_push(m, 2, audio, sizeof(audio), 0, 1, true) == RECORD_MP4_ERROR);
   audio[4] ^= 0x40;
   CHECK(record_mux_mp4_push(m, 2, audio, sizeof(audio), 0, 1536, true) == RECORD_MP4_ERROR);
   audio[4] ^= 0x40;
   CHECK(record_mux_mp4_push(m, 2, audio, sizeof(audio), 0, 1536, true) == RECORD_MP4_OK);
   CHECK(!record_mux_mp4_finish(m)); CHECK(record_mux_mp4_release(m));
   CHECK(record_mux_mp4_push(m, 2, audio, sizeof(audio), 1, 1536, true) == RECORD_MP4_ERROR);
   CHECK(record_mux_mp4_finish(m)); record_mux_mp4_free(m);
   c.ac3_header = NULL; c.ac3_header_size = 0;
   c.max_packet_size = (size_t)-1; CHECK(!record_mux_mp4_new(&c));
   puts("PASS: MP4 bounds, ownership, retries, 64-bit timing and finalization");
   return 0;
}
