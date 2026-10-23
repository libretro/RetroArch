/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Mux ownership acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../record/record_mux_mediacodec.h"
#include "mediacodec_encode_test_types.h"
static AMediaMuxer native_mux;
static AMediaFormat video, audio, bad;
static int stage, lookup, missing, libraries, muxes, starts, stops, writes;
static int allocation_fail;
static size_t written_track;
static const uint8_t *written_data;
static AMediaCodecBufferInfo written_info;
static void *test_allocate(size_t count, size_t bytes)
{
   return allocation_fail ? NULL : calloc(count, bytes);
}
static AMediaMuxer *make(int fd, OutputFormat format)
{
   assert(fd == 42 && format == AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
   if (stage == 2)
      return NULL;
   muxes++;
   return &native_mux;
}
static media_status_t destroy(AMediaMuxer *mux)
{
   assert(mux == &native_mux);
   muxes--;
   return 0;
}
static media_status_t start(AMediaMuxer *mux)
{
   assert(mux == &native_mux);
   starts++;
   return stage == 4 ? -99 : 0;
}
static media_status_t stop(AMediaMuxer *mux)
{
   assert(mux == &native_mux);
   stops++;
   return stage == 6 ? -99 : 0;
}
static ssize_t track(AMediaMuxer *mux, const AMediaFormat *format)
{
   assert(mux == &native_mux && (format == &video || format == &audio));
   if (stage == 3)
      return -99;
   return format == &video || stage == 8 ? 5 : 2;
}
static bool get_string(AMediaFormat *format, const char *key, const char **out)
{
   assert(!strcmp(key, "mime"));
   if (stage == 7)
      return false;
   *out = format == &video  ? "video/hevc"
         : format == &audio ? "audio/mp4a-latm"
                            : "application/octet-stream";
   return true;
}
static media_status_t write_sample(AMediaMuxer *mux, size_t index,
      const uint8_t *data, const AMediaCodecBufferInfo *info)
{
   assert(mux == &native_mux && (index == 5 || index == 2) && data
         && !info->offset);
   writes++;
   written_track = index;
   written_data = data;
   written_info = *info;
   return stage == 5 ? -99 : 0;
}
dylib_t dylib_load(const char *name)
{
   assert(!strcmp(name, "libmediandk.so"));
   lookup = 0;
   if (stage == 1)
      return NULL;
   libraries++;
   return (void *)1;
}
void dylib_close(dylib_t library)
{
   assert(library && !muxes);
   libraries--;
}
function_t dylib_proc(dylib_t library, const char *symbol)
{
   assert(library);
   if (++lookup == missing)
      return NULL;
#define FN(text, function)                                                     \
   if (!strcmp(symbol, text))                                                  \
   return (function_t)function
   FN("AMediaMuxer_new", make);
   FN("AMediaMuxer_start", start);
   FN("AMediaMuxer_stop", stop);
   FN("AMediaMuxer_delete", destroy);
   FN("AMediaMuxer_addTrack", track);
   FN("AMediaMuxer_writeSampleData", write_sample);
   FN("AMediaFormat_getString", get_string);
#undef FN
   assert(0);
   return NULL;
}
#define calloc test_allocate
#include "../../../record/record_mux_mediacodec.c"
#undef calloc
static void balanced(void)
{
   assert(!libraries && !muxes);
}
static record_mux_mediacodec_t *ready(bool with_audio)
{
   record_mux_mediacodec_t *m = record_mux_mediacodec_new(42, with_audio);
   assert(m && !record_mux_mediacodec_start(m));
   assert(record_mux_mediacodec_track(m, false, &video));
   if (with_audio)
   {
      assert(!record_mux_mediacodec_start(m));
      assert(record_mux_mediacodec_track(m, true, &audio));
   }
   assert(record_mux_mediacodec_start(m) == 1);
   assert(record_mux_mediacodec_start(m) == 1);
   return m;
}
int main(void)
{
   static const uint8_t bytes[8] = {1, 2, 3};
   record_mediacodec_packet_t packet;
   record_mux_mediacodec_t *m;
   int i, before;
   memset(&packet, 0, sizeof(packet));
   packet.data = bytes;
   packet.size = sizeof(bytes);
   packet.flags = 1;
   packet.pts_us = 1000;
   assert(!record_mux_mediacodec_new(-1, false));
   allocation_fail = 1;
   assert(!record_mux_mediacodec_new(42, false));
   allocation_fail = 0;
   for (missing = 1; missing <= 7; missing++)
   {
      assert(!record_mux_mediacodec_new(42, false));
      balanced();
   }
   missing = 0;
   for (stage = 1; stage <= 2; stage++)
   {
      assert(!record_mux_mediacodec_new(42, false));
      balanced();
   }
   stage = 0;
   before = starts;
   m = ready(true);
   assert(starts == before + 1 && !record_mux_mediacodec_finish(m));
   assert(!record_mux_mediacodec_track(m, false, &video));
   assert(record_mux_mediacodec_write(m, false, &packet));
   assert(written_track == 5 && written_data == bytes && written_info.size == 8
         && written_info.presentationTimeUs == 1000 && written_info.flags == 1);
   packet.pts_us = 500;
   packet.flags = 4;
   assert(record_mux_mediacodec_write(m, false, &packet));
   assert(written_info.presentationTimeUs == 500 && !written_info.flags);
   before = writes;
   packet.flags = 2;
   packet.pts_us = -1;
   assert(record_mux_mediacodec_write(m, false, &packet) && writes == before);
   packet.flags = 0;
   packet.pts_us = 0;
   assert(record_mux_mediacodec_write(m, true, &packet) && written_track == 2);
   assert(record_mux_mediacodec_end(m, false, 2000));
   assert(!written_info.size && written_info.flags == 4
         && written_info.presentationTimeUs == 2000);
   before = writes;
   assert(record_mux_mediacodec_end(m, false, 2000) && writes == before);
   assert(!record_mux_mediacodec_end(m, false, 2001));
   assert(!record_mux_mediacodec_write(m, false, &packet));
   assert(!record_mux_mediacodec_finish(m));
   assert(record_mux_mediacodec_end(m, true, 21333));
   before = stops;
   assert(record_mux_mediacodec_finish(m) == 1);
   assert(record_mux_mediacodec_finish(m) == 1 && stops == before + 1);
   record_mux_mediacodec_free(m);
   assert(stops == before + 1);
   balanced();
   for (i = 3; i <= 8; i++)
   {
      m = record_mux_mediacodec_new(42, i == 8);
      if (i != 3 && i != 7)
         assert(record_mux_mediacodec_track(m, false, &video));
      stage = i;
      if (i == 3 || i == 7)
         assert(!record_mux_mediacodec_track(m, false, &video));
      else if (i == 4)
         assert(record_mux_mediacodec_start(m) == -1);
      else if (i == 8)
         assert(!record_mux_mediacodec_track(m, true, &audio));
      else
      {
         assert(record_mux_mediacodec_start(m) == 1);
         packet.flags = 1;
         packet.pts_us = 1000;
         if (i == 5)
            assert(!record_mux_mediacodec_write(m, false, &packet));
         else
         {
            assert(record_mux_mediacodec_write(m, false, &packet));
            assert(record_mux_mediacodec_end(m, false, 2000));
            before = stops;
            assert(
                  record_mux_mediacodec_finish(m) == -1 && stops == before + 1);
         }
      }
      before = stops;
      record_mux_mediacodec_free(m);
      if (i == 6)
         assert(stops == before);
      balanced();
      stage = 0;
   }
   m = record_mux_mediacodec_new(42, false);
   assert(!record_mux_mediacodec_track(m, false, &bad));
   assert(record_mux_mediacodec_start(m) == -1);
   record_mux_mediacodec_free(m);
   balanced();
   for (i = 0; i < 5; i++)
   {
      m = ready(false);
      packet.data = bytes;
      packet.size = sizeof(bytes);
      packet.pts_us = 1000;
      packet.flags = 1;
      if (i == 0)
         packet.pts_us = -1;
      if (i == 1)
         packet.data = NULL;
      if (i == 2)
         packet.flags = 0;
      if (i == 3)
         packet.flags = 8;
      if (i == 4)
         packet.size = (size_t)INT_MAX + 1;
      assert(!record_mux_mediacodec_write(m, false, &packet));
      assert(record_mux_mediacodec_finish(m) == -1);
      record_mux_mediacodec_free(m);
      balanced();
   }
   packet.data = bytes;
   packet.size = sizeof(bytes);
   packet.flags = 1;
   packet.pts_us = 1000;
   m = ready(false);
   assert(record_mux_mediacodec_write(m, false, &packet));
   before = writes;
   assert(!record_mux_mediacodec_end(m, false, 1000) && writes == before);
   assert(record_mux_mediacodec_finish(m) == -1);
   record_mux_mediacodec_free(m);
   balanced();
   m = ready(false);
   assert(record_mux_mediacodec_write(m, false, &packet));
   stage = 5;
   assert(!record_mux_mediacodec_end(m, false, 2000));
   assert(record_mux_mediacodec_finish(m) == -1);
   record_mux_mediacodec_free(m);
   balanced();
   stage = 0;
   puts("PASS: worker MP4 mux track/packet ownership, source timestamps, "
        "explicit final durations, native faults and cancellation; APIs "
        "mocked");
   return 0;
}
