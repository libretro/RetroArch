/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rmp4.h>
#include <formats/rh265.h>
#ifdef HAVE_THREADS
#include <rthreads/tpool.h>
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #x); goto done; } } while (0)

static int compare_picture(rh265_video *decoder, FILE *reference)
{
   uint8_t row[64];
   unsigned k;
   if (rh265_video_bit_depth(decoder) != 8) return 0;
   for (k = 0; k < 3; k++)
   {
      int stride, width, height, y;
      const uint8_t *plane = rh265_video_plane(decoder, k, &stride, &width, &height);
      if (!plane || width != (k ? 32 : 64) || height != width) return 0;
      for (y = 0; y < height; y++)
         if (fread(row, 1, width, reference) != (size_t)width
               || memcmp(row, plane + (size_t)y * stride, width))
            return 0;
   }
   return 1;
}

static int compare_time(FILE *times, int64_t timestamp, int64_t first)
{
   unsigned long expected;
   if (fscanf(times, "%lu", &expected) != 1) return 0;
   return timestamp - first == (int64_t)expected * 1000000;
}

int main(int argc, char **argv)
{
   FILE *input = NULL, *reference = NULL, *times = NULL;
   long length;
   uint8_t *data = NULL;
   rmp4_t *demux = NULL;
   rh265_video *decoder = NULL;
   rmp4_packet packet;
   const rmp4_track *track;
   const int64_t *pts;
   int64_t first, timestamp = 0;
   uint32_t samples = 0, j;
   int selected = -1, i, ret, frames = 0, status = 1;
#ifdef HAVE_THREADS
   tpool_t *pool = NULL;
#endif
   CHECK(argc == 4 || argc == 5);
   input = fopen(argv[1], "rb");
   CHECK(input && !fseek(input, 0, SEEK_END));
   length = ftell(input);
   CHECK(length > 0 && !fseek(input, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)length);
   CHECK(data && fread(data, 1, (size_t)length, input) == (size_t)length);
   demux = rmp4_open_memory(data, (size_t)length);
   decoder = rh265_video_open();
   CHECK(demux && decoder);
   for (i = 0; i < rmp4_num_tracks(demux); i++)
      if (rmp4_get_track(demux, i)->type == RMP4_TRACK_VIDEO)
      {
         selected = i;
         break;
      }
   CHECK(selected >= 0);
   track = rmp4_get_track(demux, selected);
   CHECK(!rh265_video_set_extradata(decoder,
            track->codec_private, track->codec_private_size));
   pts = rmp4_track_pts(demux, selected, &samples);
   CHECK(pts && samples && samples <= 128);
   first = pts[0];
   for (j = 1; j < samples; j++)
      if (pts[j] < first) first = pts[j];
   if (argc == 5)
   {
      rh265_video_set_contexts(decoder, 4);
      rh265_video_set_publish_delay(200);
#ifdef HAVE_THREADS
      pool = tpool_create_with_stack_size(3, 512 * 1024);
      CHECK(pool);
      rh265_video_set_thread_pool(decoder, pool, 4);
#endif
   }
   reference = fopen(argv[2], "rb");
   times = fopen(argv[3], "r");
   CHECK(reference && times);
   while ((ret = rmp4_read_packet(demux, &packet)) == 1)
   {
      int64_t previous = timestamp;
      if (packet.track != selected) continue;
      CHECK(!rh265_video_set_extradata(decoder,
               track->codec_private, track->codec_private_size));
      ret = rh265_video_decode_timed(decoder, packet.data, packet.size,
            packet.timestamp, &timestamp);
      CHECK(ret == 0 || ret == 1);
      if (ret == 1)
      {
         CHECK(++frames <= 128 && compare_picture(decoder, reference));
         CHECK(compare_time(times, timestamp, first));
      }
      else CHECK(timestamp == previous);
   }
   CHECK(ret == 0 || ret == 1);
   while ((ret = rh265_video_drain_timed(decoder, &timestamp)) == 0)
   {
      CHECK(++frames <= 128 && compare_picture(decoder, reference));
      CHECK(compare_time(times, timestamp, first));
   }
   CHECK(ret == -1 && fgetc(reference) == EOF && !ferror(reference));
   printf("HEVC output flags: %d pictures PASS\n", frames);
   status = 0;
done:
   if (reference) fclose(reference);
   if (times) fclose(times);
   if (input) fclose(input);
   if (decoder) rh265_video_close(decoder);
#ifdef HAVE_THREADS
   if (pool) tpool_destroy(pool);
#endif
   if (demux) rmp4_close(demux);
   free(data);
   return status;
}
