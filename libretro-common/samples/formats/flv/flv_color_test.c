/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <formats/rflv_mux.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_output(rflv_mux_t *m, FILE *file)
{
   const rflv_span_t *spans;
   rflv_tag_t tag;
   size_t count, i;
   const void *payload;
   while (rflv_mux_get(m, &spans, &count))
   {
      if (rflv_mux_get_tag(m, &tag))
      {
         payload = tag.spans[1].data;
         assert(rflv_mux_step(m) == RFLV_PACKET && rflv_mux_get_tag(m, &tag));
         assert(tag.spans[1].data == payload && !rflv_mux_finish(m));
      }
      for (i = 0; i < count; i++)
         assert(fwrite(spans[i].data, 1, spans[i].size, file) == spans[i].size);
      assert(rflv_mux_release(m));
   }
}

int main(int argc, char **argv)
{
   static const unsigned char asc[] = {18, 16};
   static const unsigned char avcc[] = {1,66,0,30,255,225,0,4,103,66,0,30,1,0,1,104};
   static const uint16_t coordinates[] = {34000,16000,13250,34500,7500,3000,15635,16450};
   unsigned char hvcc[44];
   rflv_config_t config;
   video_color_metadata_t color, bad;
   rflv_mux_t *m;
   const rflv_span_t *spans;
   FILE *file;
   size_t i, at, count;
   unsigned mode, depth;
   assert(argc == 3);
   mode = (unsigned)atoi(argv[2]);
   assert(mode <= 3);
   depth = mode == 1 ? 8 : mode == 2 ? 12 : 10;
   memset(hvcc, 0, sizeof(hvcc));
   hvcc[0] = 1;
   hvcc[13] = 0xf0;
   hvcc[15] = 0xfc; hvcc[16] = 0xfd;
   hvcc[17] = hvcc[18] = (unsigned char)(0xf8 | (depth - 8));
   hvcc[21] = hvcc[22] = 3;
   for (i = 0; i < 3; i++)
   {
      at = 23 + 7 * i;
      hvcc[at] = (unsigned char)(0xa0 + i);
      hvcc[at + 2] = 1; hvcc[at + 4] = 2;
      hvcc[at + 5] = (unsigned char)(64 + 2 * i); hvcc[at + 6] = 1;
   }
   memset(&config, 0, sizeof(config));
   config.avcc = hvcc; config.avcc_size = sizeof(hvcc);
   config.asc = asc; config.asc_size = sizeof(asc);
   config.video_timescale = 90000; config.audio_timescale = 44100;
   config.max_packet_size = 1024;
   memset(&color, 0, sizeof(color));
   color.primaries = 9; color.transfer = mode == 2 ? 18 : 16; color.matrix = 9;
   color.range = 1;
   if (mode == 1)
      memset(&color, 0, sizeof(color));
   if (mode == 0 || mode == 3)
   {
      color.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      color.mastering.luminance_max = mode == 3 ? 100000000 : 10000000;
      color.mastering.luminance_min = mode == 3 ? 1 : 50;
      memcpy(color.mastering.chromaticity, coordinates, sizeof(coordinates));
      if (mode == 3)
         color.mastering.chromaticity[6] = 15636;
      color.max_cll = mode == 3 ? 10000 : 1000;
      color.max_fall = mode == 3 ? 10000 : 400;
   }
   config.avcc = avcc; config.avcc_size = sizeof(avcc);
   m = rflv_mux_new(&config);
   assert(m && !rflv_mux_set_color(m, &color, depth));
   rflv_mux_free(m);
   config.avcc = hvcc; config.avcc_size = sizeof(hvcc);
   m = rflv_mux_new_hevc(&config);
   assert(m && !rflv_mux_set_color(NULL, &color, depth));
   assert(!rflv_mux_set_color(m, NULL, depth));
   assert(!rflv_mux_set_color(m, &color, 9));
   assert(!rflv_mux_set_color(m, &color, depth == 8 ? 10 : 8));
   bad = color; bad.primaries = 256;
   assert(!rflv_mux_set_color(m, &bad, depth));
   bad = color; bad.max_cll = 10001;
   assert(!rflv_mux_set_color(m, &bad, depth));
   bad = color; bad.max_cll = 1; bad.max_fall = 2;
   assert(!rflv_mux_set_color(m, &bad, depth));
   bad = color; bad.mastering.present = 1;
   assert(!rflv_mux_set_color(m, &bad, depth));
   if (color.mastering.present)
   {
      bad = color; bad.mastering.luminance_min = 0;
      assert(!rflv_mux_set_color(m, &bad, depth));
      bad = color; bad.mastering.chromaticity[0] = 37001;
      assert(!rflv_mux_set_color(m, &bad, depth));
   }
   assert(rflv_mux_set_color(m, &color, depth));
   assert(!rflv_mux_set_color(m, &color, depth));
   memset(&color, 0, sizeof(color));
   assert(rflv_mux_get(m, &spans, &count) && count == 1);
   assert(spans[0].size == 13 && !memcmp(spans[0].data, "FLV\1\5", 5));
   file = fopen(argv[1], "wb");
   assert(file);
   write_output(m, file);
   assert(!rflv_mux_set_color(m, &color, depth));
   assert(rflv_mux_step(m) == RFLV_BUSY && rflv_mux_finish(m));
   assert(rflv_mux_step(m) == RFLV_END);
   assert(!fclose(file));
   rflv_mux_free(m);
   puts("PASS: source-owned colorInfo ordering, cold ownership, bounds and lifetime");
   return 0;
}
