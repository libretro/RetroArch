/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rmpeg1_video.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
/* Generated red 16x16 progressive I picture, external FFmpeg fixture. */
static const uint8_t fixture[] = {0x0,0x0,0x1,0xb3,0x1,0x0,0x10,0x13,0xff,0xff,0xe0,0x18,0x0,0x0,0x1,0xb5,0x14,0x8a,0x0,0x1,0x0,0x0,0x0,0x0,0x1,0xb8,0x0,0x8,0x0,0x40,0x0,0x0,0x1,0x0,0x0,0xf,0xff,0xf8,0x0,0x0,0x1,0xb5,0x8f,0xff,0xf3,0x41,0x80,0x0,0x0,0x1,0x1,0x1b,0xf2,0x14,0xa5,0x2f,0x99,0xbf,0x70,0x80};

static void run_small(const uint8_t *data, size_t size, bool supported, size_t chunk)
{
   rmpeg1_video_t *v = rmpeg1_video_init();
   rmpeg1_video_frame_t frame;
   size_t at = 0; unsigned frames = 0;
   CHECK(v != NULL); if (!v) return;
   while (at < size)
   {
      size_t n = size - at; if (n > chunk) n = chunk;
      CHECK(rmpeg1_video_write(v, data + at, n) == n); at += n;
      while (rmpeg1_video_decode(v, &frame)) frames++;
   }
   rmpeg1_video_flush(v);
   while (rmpeg1_video_decode(v, &frame))
   {
      frames++;
      CHECK(frame.width == 16 && frame.height == 16 && frame.coding_type == 1);
      CHECK(frame.y[0] >= 80 && frame.y[0] <= 82);
      CHECK(frame.cb[0] >= 89 && frame.cb[0] <= 91);
      CHECK(frame.cr[0] >= 239);
   }
   CHECK(frames == (supported ? 1u : 0u));
   CHECK(supported ? rmpeg1_video_errors(v) == 0 : rmpeg1_video_skipped(v) != 0);
   rmpeg1_video_free(v);
}

static void test_gates(void)
{
   uint8_t data[sizeof(fixture)];
   size_t i, sequence = 0, picture = 0, chunk;
   const unsigned picture_bits[] = {20,22,25,26,27,28,29,30,31,32,33};
   for (i = 0; i + 5 < sizeof(fixture); i++)
      if (!fixture[i] && !fixture[i+1] && fixture[i+2] == 1 && fixture[i+3] == 0xb5)
      {
         if (fixture[i+4] >> 4 == 1) sequence = i + 4;
         if (fixture[i+4] >> 4 == 8) picture = i + 4;
      }
   CHECK(sequence && picture);
   for (chunk = 1; chunk <= sizeof(fixture); chunk++)
      run_small(fixture, sizeof(fixture), true, chunk);
   for (i = 0; i < sizeof(picture_bits)/sizeof(picture_bits[0]); i++)
   {
      unsigned bit = picture_bits[i];
      memcpy(data, fixture, sizeof(data));
      data[picture + bit/8] ^= (uint8_t)(1u << (7 - bit%8));
      run_small(data, sizeof(data), false, 1);
   }
   memcpy(data, fixture, sizeof(data)); data[sequence + 1] ^= 8;
   run_small(data, sizeof(data), false, 1);
}

static void test_reference(const char *path, const char *reference)
{
   FILE *input = fopen(path, "rb"), *oracle = fopen(reference, "rb");
   rmpeg1_video_t *v = rmpeg1_video_init();
   rmpeg1_video_frame_t frame;
   uint8_t bytes[1024];
   unsigned frames = 0, peak = 0;
   unsigned long samples = 0, difference = 0;
   bool finished = false;
   CHECK(input && oracle && v); if (!input || !oracle || !v) return;
   while (!finished)
   {
      size_t n = fread(bytes, 1, sizeof(bytes), input);
      CHECK(rmpeg1_video_write(v, bytes, n) == n);
      if (!n) { rmpeg1_video_flush(v); finished = true; }
      while (rmpeg1_video_decode(v, &frame))
      {
         unsigned plane, y, x;
         frames++;
         for (plane = 0; plane < 3; plane++)
         {
            const uint8_t *p = plane == 0 ? frame.y : plane == 1 ? frame.cb : frame.cr;
            unsigned w = plane == 0 ? frame.width : (frame.width + 1)/2;
            unsigned h = plane == 0 ? frame.height : (frame.height + 1)/2;
            unsigned stride = plane == 0 ? frame.y_stride : frame.c_stride;
            for (y = 0; y < h; y++) for (x = 0; x < w; x++)
            {
               int expected = fgetc(oracle);
               unsigned error;
               CHECK(expected != EOF); if (expected == EOF) break;
               error = (unsigned)abs((int)p[y * stride + x] - expected);
               if (error > peak) peak = error;
               difference += error; samples++;
            }
         }
      }
   }
   CHECK(frames == 3 && fgetc(oracle) == EOF);
   CHECK(!rmpeg1_video_errors(v) && !rmpeg1_video_skipped(v));
   CHECK(peak <= 3 && difference <= samples);
   printf("H262 intra: frames=%u peak=%u mean=%f\n", frames, peak,
      samples ? (double)difference / samples : 0.0);
   fclose(input); fclose(oracle); rmpeg1_video_free(v);
}

int main(int argc, char **argv)
{
   test_gates();
   if (argc == 3) test_reference(argv[1], argv[2]); else CHECK(argc == 1);
   printf("H262 intra tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
