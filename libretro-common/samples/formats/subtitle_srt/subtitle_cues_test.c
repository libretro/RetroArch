/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rsubtitle_srt.h>

static rsubtitle_srt_t *make_track(unsigned invalid, char **owned)
{
   static const char text[] = "later\0first \xc3\xa9\0long\0same\0";
   char *data = (char*)malloc(sizeof(text));
   rsubtitle_srt_span_t *spans = (rsubtitle_srt_span_t*)calloc(4, sizeof(*spans));
   assert(data && spans);
   memcpy(data, text, sizeof(text));
   spans[0].start_us = 1234567;
   spans[0].end_us = 2345678;
   spans[0].length = 5;
   spans[1].start_us = 1;
   spans[1].end_us = 3;
   spans[1].offset = 6;
   spans[1].length = 8;
   spans[2].start_us = -1;
   spans[2].end_us = 3000000;
   spans[2].offset = 15;
   spans[2].length = 4;
   spans[3].start_us = 1;
   spans[3].end_us = 10;
   spans[3].offset = 20;
   spans[3].length = 4;
   if (invalid == 1)
      spans[1].offset = (size_t)-1;
   if (invalid == 2)
      spans[1].length = (size_t)-1;
   if (invalid == 3)
      data[14] = 'x';
   if (invalid == 4)
      spans[1].end_us = spans[1].start_us;
   if (invalid == 5)
      data[12] = (char)0xc0;
   if (invalid == 6)
      data[7] = 0;
   if (invalid == 7)
      spans[1].length = 7;
   if (owned)
      *owned = data;
   return rsubtitle_srt_begin_cues(data, sizeof(text), spans, 4);
}

int main(void)
{
   rsubtitle_srt_t *track;
   const rsubtitle_srt_cue_t *cue;
   char *owned;
   unsigned invalid, cancel;
   int status = 0;
   size_t steps = 0;
   track = make_track(0, &owned);
   assert(track && !rsubtitle_srt_count(track));
   assert(rsubtitle_srt_iterate(track, 0) == 0);
   while (!(status = rsubtitle_srt_iterate(track, 1)))
      assert(++steps < 256);
   assert(status == 1 && steps > 25 && rsubtitle_srt_count(track) == 4);
   cue = rsubtitle_srt_cue(track, 0);
   assert(cue && cue->text == owned + 15 && cue->start_us == -1);
   assert(rsubtitle_srt_cue(track, 1)->text == owned + 6);
   assert(rsubtitle_srt_cue(track, 2)->text == owned + 20);
   cue = rsubtitle_srt_cue(track, 3);
   assert(cue->text == owned && cue->start_us == 1234567 && cue->end_us == 2345678);
   assert(rsubtitle_srt_find(track, 2) == 0);
   assert(rsubtitle_srt_find_from(track, 2, 1) == 1);
   assert(rsubtitle_srt_find_from(track, 2, 2) == 2);
   assert(rsubtitle_srt_find_from(track, 3, 1) == 2);
   assert(rsubtitle_srt_find_from(track, 1234567, 1) == 3);
   assert(rsubtitle_srt_find(track, 3000000) == 4);
   rsubtitle_srt_free(track);
   for (invalid = 1; invalid <= 7; invalid++)
   {
      track = make_track(invalid, NULL);
      assert(track);
      steps = 0;
      while (!(status = rsubtitle_srt_iterate(track, 1)))
         assert(++steps < 256);
      assert(status == -1 && !rsubtitle_srt_count(track));
      rsubtitle_srt_free(track);
   }
   for (cancel = 0; cancel < 80; cancel++)
   {
      track = make_track(0, NULL);
      assert(track);
      rsubtitle_srt_iterate(track, cancel);
      rsubtitle_srt_free(track);
   }
   return 0;
}
