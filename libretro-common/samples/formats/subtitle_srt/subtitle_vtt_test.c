/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_vtt_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rsubtitle_vtt.h>

static rsubtitle_srt_t *parse(const char *text, size_t work)
{
   size_t size = strlen(text), ticks = 0;
   char *owned = (char*)malloc(size ? size : 1);
   rsubtitle_srt_t *track;
   int status = 0;
   assert(owned);
   memcpy(owned, text, size);
   track = rsubtitle_vtt_begin(owned, size);
   if (!track) return NULL;
   assert(!rsubtitle_srt_iterate(track, 0));
   assert(!rsubtitle_vtt_region_count(track) && !rsubtitle_vtt_region(track, 0));
   assert(!rsubtitle_vtt_cue_info(track, 0) && !rsubtitle_vtt_block_count(track));
   assert(!rsubtitle_vtt_css_node_count(track) && !rsubtitle_vtt_css_node(track, 0));
   while (!status && ticks++ < size * 8 + 100)
      status = rsubtitle_srt_iterate(track, work);
   assert(status);
   if (status < 0)
   {
      rsubtitle_srt_free(track);
      return NULL;
   }
   return track;
}

static void metadata(void)
{
   static const char text[] =
      "\357\273\277WEBVTT test\r\n\r\n"
      "NOTE global comment\r\nnot a cue\r\n\r\n"
      "STYLE\r\n::cue { color: lime; }\r\n\r\n"
      "REGION \t\r\nid:lower\r\nwidth:60%\r\n\r\n"
      "later\r\n00:02.000 --> 00:04.000 align:start position:20%\r\n"
      "<v Alice>second &amp; \303\251\r\n\r\n"
      "00:01.000 --> 00:03.000\r\nfirst\r\n \t\r\nline\r\n\r\n"
      "STYLE\r\n::cue { color: red; }\r\n\r\n"
      "clock\r\n02:03:04.005 --> 02:03:05.100 region:lower\r\nlast";
   size_t work;
   for (work = 1; work < 8; work++)
   {
      rsubtitle_srt_t *track = parse(text, work);
      const rsubtitle_srt_cue_t *cue;
      const rsubtitle_vtt_cue_t *info;
      const rsubtitle_vtt_block_t *block;
      assert(track && rsubtitle_srt_count(track) == 3);
      cue = rsubtitle_srt_cue(track, 0);
      info = rsubtitle_vtt_cue_info(track, 0);
      assert(cue && cue->start_us == 1000000 && cue->end_us == 3000000);
      assert(!strcmp(cue->text, "first\n \t\nline"));
      assert(info && !info->identifier && !strcmp(info->settings, ""));
      assert(!info->flags && !info->timestamp_origin_ns);
      cue = rsubtitle_srt_cue(track, 1);
      info = rsubtitle_vtt_cue_info(track, 1);
      assert(cue && !strcmp(cue->text, "<v Alice>second &amp; \303\251"));
      assert(info && !strcmp(info->identifier, "later"));
      assert(!strcmp(info->settings, "align:start position:20%"));
      cue = rsubtitle_srt_cue(track, 2);
      info = rsubtitle_vtt_cue_info(track, 2);
      assert(cue && cue->start_us == (int64_t)7384005 * 1000);
      assert(cue->end_us == (int64_t)7385100 * 1000 && !strcmp(cue->text, "last"));
      assert(!strcmp(info->identifier, "clock") && !strcmp(info->settings, "region:lower"));
      assert(rsubtitle_srt_find(track, 2500000) == 0);
      assert(rsubtitle_srt_find_from(track, 2500000, 1) == 1);
      assert(rsubtitle_vtt_block_count(track) == 2);
      block = rsubtitle_vtt_block(track, 0);
      assert(block && block->kind == RSUBTITLE_VTT_STYLE);
      assert(block->css_root < rsubtitle_vtt_css_node_count(track));
      assert(rsubtitle_vtt_css_node(track, block->css_root)->token.kind == RSUBTITLE_CSS_ROOT);
      assert(!strcmp(block->text, "::cue { color: lime; }"));
      assert(block->length == strlen(block->text));
      block = rsubtitle_vtt_block(track, 1);
      assert(block && block->kind == RSUBTITLE_VTT_REGION);
      assert(!strcmp(block->text, "id:lower\nwidth:60%"));
      assert(!rsubtitle_vtt_block(track, 2) && !rsubtitle_vtt_cue_info(track, 3));
      rsubtitle_srt_free(track);
   }
}

static void timing_recovery(void)
{
   static const struct
   {
      const char *timing;
      unsigned start_ms;
      unsigned count;
   } cases[] = {
      {"00:00.000-->00:01.000", 0, 1},
      {"00:00.000 -->00:01.000", 0, 1},
      {"00:00.000--> 00:01.000", 0, 1},
      {"1:02:03.004 --> 1:02:04.004", 3723004, 1},
      {"001:02:03.004 --> 001:02:04.004", 3723004, 1},
      {"\f00:00.000\f-->\f00:01.000", 0, 1},
      {"00:00.000 --> 00:01.000align:start", 0, 1},
      {"0:00.000 --> 00:01.000", 0, 0},
      {"000:00.000 --> 00:01.000", 0, 0},
      {"60:00.000 --> 61:00.000", 0, 0},
      {"1:60:00.000 --> 2:00:01.000", 0, 0},
      {"1:00:60.000 --> 2:00:01.000", 0, 0},
      {"00:00.0000 --> 00:01.000", 0, 0},
      {"00:00.00 --> 00:01.000", 0, 0},
      {"00:00.000 -> 00:01.000", 0, 0}
   };
   size_t i, work;
   for (work = 1; work <= 7; work += 6)
      for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      {
         char input[128];
         rsubtitle_srt_t *track;
         strcpy(input, "WEBVTT\n\n");
         strcat(input, cases[i].timing);
         strcat(input, "\ncaption\n");
         track = parse(input, work);
         assert(track && rsubtitle_srt_count(track) == cases[i].count);
         if (cases[i].count)
         {
            const rsubtitle_srt_cue_t *cue = rsubtitle_srt_cue(track, 0);
            assert(cue->start_us == (int64_t)cases[i].start_ms * 1000);
            assert(cue->end_us == cue->start_us + 1000000);
         }
         rsubtitle_srt_free(track);
      }
}

static void regions(void)
{
   static const char *parts[] = {
      "WEBVTT\n\nREGION\nid:lower width:40.5% lines:4 regionanchor:10.25%,90.125% "
      "viewportanchor:20.5%,80.25% scroll:up width:bad lines:4.0 scroll:none "
      "regionanchor:100%,101% width:.5% width:101% viewportanchor:30%,40%,50% id:\n\n",
      "REGION\nid:lower width:60%\n\nREGION\nid:collision1794\n\nREGION\njunk\n\n",
      "00:03.000 --> 00:04.000 region:lower\nthree\n\n"
      "00:00.000 --> 00:01.000 region:missing\nzero\n\n"
      "00:01.000 --> 00:02.000 region:lower line:0\none\n\n",
      "00:02.000 --> 00:03.000 region:lower vertical:rl\ntwo\n\n"
      "00:04.000 --> 00:05.000 region:lower size:50%\nfour\n\n",
      "00:05.000 --> 00:06.000 line:0 region:lower\nfive\n\n"
      "00:06.000 --> 00:07.000 region:lower position:50%\nsix\n\n"
      "00:07.000 --> 00:08.000 region:lower region:missing\nseven\n\nREGION\nid:late\n"
   };
   char *input = (char*)malloc(2048);
   size_t i, work, used = 0;
   assert(input);
   for (i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
   {
      size_t size = strlen(parts[i]);
      assert(size < 2048 - used);
      memcpy(input + used, parts[i], size); used += size;
   }
   input[used] = 0;
   for (work = 1; work < 8; work++)
   {
      rsubtitle_srt_t *track = parse(input, work);
      const rsubtitle_vtt_region_t *region;
      assert(track && rsubtitle_srt_count(track) == 8 && rsubtitle_vtt_region_count(track) == 4);
      region = rsubtitle_vtt_region(track, 0);
      assert(region && region->identifier_length == 5 && region->width == 40.5);
      assert(region->identifier == rsubtitle_vtt_block(track, 0)->text + 3);
      assert(region->lines == 4 && region->scroll_up);
      assert(region->anchor_x == 10.25 && region->anchor_y == 90.125);
      assert(region->viewport_x == 20.5 && region->viewport_y == 80.25);
      region = rsubtitle_vtt_region(track, 1);
      assert(region && region->width == 60.0 && region->lines == 3 && !region->scroll_up);
      assert(!region->anchor_x && region->anchor_y == 100.0
            && !region->viewport_x && region->viewport_y == 100.0);
      for (i = 0; i < 8; i++)
         assert(rsubtitle_vtt_cue_region(track, i) == ((i == 3 || i == 5 || i == 6) ? region : NULL));
      assert(!rsubtitle_vtt_cue_region(track, 8) && !rsubtitle_vtt_region(track, 4));
      assert(!rsubtitle_vtt_region(track, 3)->identifier_length);
      rsubtitle_srt_free(track);
   }
   free(input);
   assert(!rsubtitle_vtt_region_count(NULL) && !rsubtitle_vtt_region(NULL, 0)
         && !rsubtitle_vtt_cue_region(NULL, 0));
}

static void region_limits(void)
{
   char *input = (char*)malloc(65000);
   size_t used, i;
   rsubtitle_srt_t *track;
   const rsubtitle_vtt_region_t *region;
   assert(input);
   strcpy(input, "WEBVTT\n\nREGION\nid:"); used = strlen(input);
   memset(input + used, 'x', 700); used += 700;
   memcpy(input + used, " width:", 7); used += 7;
   memset(input + used, '0', 60000); used += 60000;
   strcpy(input + used, "50% lines:42949672960");
   track = parse(input, 1);
   assert(track && !rsubtitle_srt_count(track) && rsubtitle_vtt_region_count(track) == 1);
   region = rsubtitle_vtt_region(track, 0);
   assert(region && region->identifier_length == 700 && region->width == 50.0
         && region->lines == (unsigned)-1);
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\nREGION\nid:edge width:60% width:99.99999999999999999% "
         "regionanchor:20%,30% regionanchor:99.99999999999999999%,99.99999999999999999%", 1);
   region = rsubtitle_vtt_region(track, 0);
   assert(region && region->width == 100.0 && region->anchor_x == 100.0 && region->anchor_y == 100.0);
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\n00:00.000 --> 00:01.000 position:40% position:99.99999999999999999%\ntext", 1);
   assert(track && rsubtitle_vtt_cue_settings(track, 0)->position == 100.0);
   rsubtitle_srt_free(track);
   strcpy(input, "WEBVTT\n\n"); used = strlen(input);
   for (i = 0; i < 300; i++)
   {
      static const char definition[] = "REGION\nid:lower width:30%\n\n";
      memcpy(input + used, definition, sizeof(definition) - 1);
      used += sizeof(definition) - 1;
   }
   strcpy(input + used, "00:00.000 --> 00:01.000 region:lower\ncaption");
   track = parse(input, 1);
   assert(track && rsubtitle_vtt_region_count(track) == 300);
   assert(rsubtitle_vtt_cue_region(track, 0) == rsubtitle_vtt_region(track, 299));
   rsubtitle_srt_free(track);
   free(input);
}

static void recovery(void)
{
   rsubtitle_srt_t *track = parse("WEBVTT\n\n"
         "NOTE comment\n00:00.000 --> 00:59.000\nhidden\n\n"
         "bad\n00:60.000 --> 00:61.000\nignored\n\n"
         "00:00,000 --> 00:01,000\nignored\n\n"
         "00:00.000->00:01.000\nignored\n\n"
         "id\n00:00.000 --> 00:01.000\nfirst\n"
         "00:01.000 --> 00:02.000\nsecond\n\n"
         "00:02.000 --> 00:03.000\n\n"
         "STYLE\nignored after cue\n\n", 1);
   const rsubtitle_vtt_cue_t *info;
   assert(track && rsubtitle_srt_count(track) == 2);
   assert(!strcmp(rsubtitle_srt_cue(track, 0)->text, "first"));
   assert(!strcmp(rsubtitle_srt_cue(track, 1)->text, "second"));
   info = rsubtitle_vtt_cue_info(track, 1);
   assert(info && !info->identifier);
   assert(!rsubtitle_vtt_block_count(track));
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\nSTYLE\ninvalid --> style\n\n"
         "00:01.000 --> 00:02.000\nvisible", 3);
   assert(track && rsubtitle_srt_count(track) == 1 && !rsubtitle_vtt_block_count(track));
   rsubtitle_srt_free(track);
   track = parse("WEBVTT", 1);
   assert(track && !rsubtitle_srt_count(track));
   assert(rsubtitle_srt_find(track, 0) == 0);
   rsubtitle_srt_free(track);
   assert(!parse("WEBVTTbad\n\n", 1));
   assert(!parse("WEBVTT --> bad\n\n", 1));
   assert(!parse("WEBVTT\n\n00:00.000 --> 00:01.000\n\377", 1));
}

static void limits(void)
{
   static const char suffix[] = "\n\n00:00.000 --> 00:01.000\ncaption";
   char *text = (char*)malloc(70000);
   rsubtitle_srt_t *track;
   size_t length;
   assert(text);
   memcpy(text, "WEBVTT\n\nSTYLE\n", 14);
   memset(text + 14, 'a', 65536);
   memcpy(text + 14 + 65536, suffix, sizeof(suffix));
   track = parse(text, 4096);
   assert(track && rsubtitle_srt_count(track) == 1);
   assert(rsubtitle_vtt_block(track, 0)->length == 65536);
   rsubtitle_srt_free(track);
   text[14 + 65536] = 'a';
   memcpy(text + 14 + 65537, suffix, sizeof(suffix));
   assert(!parse(text, 4096));
   memcpy(text, "WEBVTT\n\n", 8);
   memset(text + 8, 'a', 128);
   length = 8 + 128;
   memcpy(text + length, "\n00:00.000 --> 00:01.000\ncaption",
         sizeof("\n00:00.000 --> 00:01.000\ncaption"));
   track = parse(text, 1);
   assert(track && strlen(rsubtitle_vtt_cue_info(track, 0)->identifier) == 128);
   rsubtitle_srt_free(track);
   text[length++] = 'a';
   memcpy(text + length, "\n00:00.000 --> 00:01.000\ncaption",
         sizeof("\n00:00.000 --> 00:01.000\ncaption"));
   assert(!parse(text, 1));
   free(text);
   track = parse("WEBVTT\n\n2562047788:00:53.775 --> 2562047788:00:54.775\nlast", 1);
   assert(track && rsubtitle_srt_count(track) == 1);
   assert(rsubtitle_srt_cue(track, 0)->end_us
         == (int64_t)((((uint64_t)-1 >> 1) / 1000) * 1000));
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\n2562047788:00:53.775 --> 2562047788:00:54.776\ninvalid", 1);
   assert(track && !rsubtitle_srt_count(track));
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\n00:00.000 --> 00:01.000\n\n"
         "STYLE\n::cue { color: lime; }\n\n"
         "NOTE_A\n00:01.000 --> 00:02.000\nvisible", 1);
   assert(track && rsubtitle_srt_count(track) == 1 && !rsubtitle_vtt_block_count(track));
   assert(!strcmp(rsubtitle_vtt_cue_info(track, 0)->identifier, "NOTE_A"));
   rsubtitle_srt_free(track);
}

static void cancellation(void)
{
   static const char text[] = "WEBVTT\n\nSTYLE\n::cue { color: lime; }\n\n"
      "REGION\nid:lower\n\nid\n00:00.000 --> 00:01.000 region:lower\ntext";
   size_t i, size = sizeof(text) - 1;
   for (i = 0; i < 512; i++)
   {
      char *owned = (char*)malloc(size);
      rsubtitle_srt_t *track;
      assert(owned);
      memcpy(owned, text, size);
      track = rsubtitle_vtt_begin(owned, size);
      assert(track);
      rsubtitle_srt_iterate(track, i);
      rsubtitle_srt_free(track);
   }
}

static rsubtitle_srt_t *embedded_begin(unsigned mode)
{
   static const char payload[] = "second\0later\0align:start\0first\0early\0\0region:lower\0";
   static const char prelude[] = "WEBVTT\n\nSTYLE\n::cue { color: lime; }\n\n"
      "REGION\nid:lower";
   static const char invalid[] = "WEBVTT\n\n00:00.000 --> 00:01.000\ncue";
   const char *source = mode == 7 ? invalid : prelude;
   size_t header_size = strlen(source);
   char *data = (char*)malloc(sizeof(payload));
   char *header = (char*)malloc(header_size);
   rsubtitle_srt_span_t *spans = (rsubtitle_srt_span_t*)calloc(2, sizeof(*spans));
   rsubtitle_vtt_span_t *info = (rsubtitle_vtt_span_t*)calloc(2, sizeof(*info));
   assert(data && header && spans && info);
   memcpy(data, payload, sizeof(payload));
   memcpy(header, source, header_size);
   spans[0].start_us = 2000000; spans[0].end_us = 3000000;
   spans[0].offset = 0; spans[0].length = 6;
   spans[1].start_us = 1000000; spans[1].end_us = 2000000;
   spans[1].offset = 25; spans[1].length = 5;
   info[0].identifier_offset = 7; info[0].identifier_length = 5;
   info[0].settings_offset = 13; info[0].settings_length = 11;
   info[1].identifier_offset = 31; info[1].identifier_length = 5;
   info[1].settings_offset = 38; info[1].settings_length = 12;
   info[0].timestamp_origin_ns = 2000000000;
   info[0].flags = RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS;
   info[1].timestamp_origin_ns = -1234567;
   info[1].flags = RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS;
   if (mode == 1) info[0].identifier_offset = (size_t)-1;
   if (mode == 2) info[0].settings_length = (size_t)-1;
   if (mode == 3) data[13] = (char)255;
   if (mode == 4) data[8] = 0;
   if (mode == 5) data[8] = '\n';
   if (mode == 6) memcpy(data + 7, "-->", 3);
   if (mode == 9) info[0].flags = 2;
   if (mode == 8)
   {
      free(info);
      info = NULL;
   }
   return rsubtitle_vtt_begin_cues(data, sizeof(payload), spans, 2, info,
         header, header_size);
}

static void embedded(void)
{
   unsigned mode;
   size_t work;
   for (mode = 0; mode < 10; mode++)
      for (work = 1; work < 8; work++)
      {
         rsubtitle_srt_t *track = embedded_begin(mode);
         const rsubtitle_vtt_cue_t *info;
         const rsubtitle_vtt_block_t *block;
         int status = 0;
         unsigned ticks = 0;
         assert(track && !rsubtitle_srt_count(track)
               && !rsubtitle_vtt_block_count(track));
         while (!status && ticks++ < 4096)
            status = rsubtitle_srt_iterate(track, work);
         if (mode && mode != 8)
         {
            assert(status == -1 && !rsubtitle_srt_count(track)
                  && !rsubtitle_vtt_cue_info(track, 0));
            rsubtitle_srt_free(track);
            continue;
         }
         assert(status == 1 && rsubtitle_srt_count(track) == 2);
         assert(!strcmp(rsubtitle_srt_cue(track, 0)->text, "first"));
         assert(!strcmp(rsubtitle_srt_cue(track, 1)->text, "second"));
         info = rsubtitle_vtt_cue_info(track, 0);
         assert(info && !strcmp(info->settings, mode == 8 ? "" : "region:lower"));
         assert(info->flags == (mode == 8 ? 0 : RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS));
         assert(info->timestamp_origin_ns == (mode == 8 ? 0 : -1234567));
         assert(mode == 8 ? !info->identifier : !strcmp(info->identifier, "early"));
         info = rsubtitle_vtt_cue_info(track, 1);
         assert(info && info->flags == (mode == 8 ? 0 : RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS));
         assert(info->timestamp_origin_ns == (mode == 8 ? 0 : 2000000000));
         assert(mode == 8 ? !info->identifier : !strcmp(info->identifier, "later"));
         assert(!strcmp(info->settings, mode == 8 ? "" : "align:start"));
         assert(rsubtitle_vtt_block_count(track) == 2);
         block = rsubtitle_vtt_block(track, 0);
         assert(block && block->kind == RSUBTITLE_VTT_STYLE
               && !strcmp(block->text, "::cue { color: lime; }"));
         assert(block->css_root < rsubtitle_vtt_css_node_count(track));
         assert(rsubtitle_vtt_css_node(track, block->css_root)->token.kind == RSUBTITLE_CSS_ROOT);
         block = rsubtitle_vtt_block(track, 1);
         assert(block && block->kind == RSUBTITLE_VTT_REGION
               && !strcmp(block->text, "id:lower"));
         assert(!rsubtitle_vtt_block(track, 2));
         assert(rsubtitle_vtt_region_count(track) == 1);
         assert(rsubtitle_vtt_cue_region(track, 0) == (mode == 8 ? NULL : rsubtitle_vtt_region(track, 0)));
         assert(!rsubtitle_vtt_cue_region(track, 1));
         rsubtitle_srt_free(track);
      }
   for (work = 0; work < 256; work++)
   {
      rsubtitle_srt_t *track = embedded_begin(0);
      assert(track);
      rsubtitle_srt_iterate(track, work);
      rsubtitle_srt_free(track);
   }
}

static void timestamp_clock(void)
{
   static const int64_t origins[] = {1, -1, 1000, -1000, 1001, -1001,
      -400000123, 500000123};
   static const int64_t expected[] = {1, 0, 1, -1, 2, -1, -400000, 500001};
   rsubtitle_vtt_cue_t cue;
   int64_t actual, maximum = (int64_t)(((uint64_t)-1) >> 1);
   size_t i;
   memset(&cue, 0, sizeof(cue));
   cue.timestamp_origin_ns = maximum;
   assert(rsubtitle_vtt_timestamp_us(&cue, 1234567, &actual) && actual == 1234567);
   cue.flags = RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS;
   for (i = 0; i < sizeof(origins) / sizeof(origins[0]); i++)
   {
      cue.timestamp_origin_ns = origins[i];
      assert(rsubtitle_vtt_timestamp_us(&cue, 0, &actual) && actual == expected[i]);
      assert(rsubtitle_vtt_timestamp_us(&cue, 1000000, &actual)
            && actual == 1000000 + expected[i]);
   }
   cue.timestamp_origin_ns = maximum;
   actual = 7;
   assert(!rsubtitle_vtt_timestamp_us(&cue, maximum, &actual) && actual == 7);
   assert(rsubtitle_vtt_timestamp_us(&cue, maximum - (maximum / 1000 + 1), &actual)
         && actual == maximum);
   cue.timestamp_origin_ns = -maximum - 1;
   assert(rsubtitle_vtt_timestamp_us(&cue, maximum, &actual)
         && actual == maximum - maximum / 1000);
   assert(!rsubtitle_vtt_timestamp_us(&cue, -1, &actual));
   cue.flags = 2;
   assert(!rsubtitle_vtt_timestamp_us(&cue, 0, &actual));
   assert(!rsubtitle_vtt_timestamp_us(NULL, 0, &actual));
   assert(!rsubtitle_vtt_timestamp_us(&cue, 0, NULL));
}

static rsubtitle_srt_t *text_track(const char *body, size_t work)
{
   static const char prefix[] = "WEBVTT\n\n00:00.000 --> 00:10.000\n";
   size_t size = sizeof(prefix) - 1 + strlen(body);
   char *input = (char*)malloc(size + 1);
   rsubtitle_srt_t *track;
   assert(input);
   memcpy(input, prefix, sizeof(prefix) - 1);
   strcpy(input + sizeof(prefix) - 1, body);
   track = parse(input, work);
   free(input);
   return track;
}

static void plain(const rsubtitle_srt_t *track, size_t cue, char *output, size_t size)
{
   size_t i, used = 0, count = rsubtitle_vtt_node_count(track, cue);
   for (i = 0; i < count; i++)
   {
      const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, i);
      assert(node && node->value);
      if (!i) assert(node->kind == RSUBTITLE_VTT_ROOT && node->parent == RSUBTITLE_VTT_NO_NODE);
      else assert(node->parent < i);
      if (node->first_child != RSUBTITLE_VTT_NO_NODE)
      {
         assert(node->first_child < count);
         assert(rsubtitle_vtt_node(track, cue, node->first_child)->parent == i);
      }
      if (node->next != RSUBTITLE_VTT_NO_NODE)
      {
         assert(node->next < count);
         assert(rsubtitle_vtt_node(track, cue, node->next)->parent == node->parent);
      }
      if (node->kind != RSUBTITLE_VTT_TEXT) continue;
      assert(used + node->value_length < size);
      memcpy(output + used, node->value, node->value_length);
      used += node->value_length;
   }
   output[used] = 0;
}

static size_t find_node(const rsubtitle_srt_t *track, size_t cue,
      enum rsubtitle_vtt_node_kind kind, size_t start)
{
   size_t count = rsubtitle_vtt_node_count(track, cue);
   for (; start < count; start++)
      if (rsubtitle_vtt_node(track, cue, start)->kind == kind) return start;
   return RSUBTITLE_VTT_NO_NODE;
}

static void text_nodes(void)
{
   static const char body[] = "A<b.hot..loud>B<i>I</b>J</i>K</b><u>U</u>"
      "<ruby>\346\274\242<rt>\343\201\213\343\202\223</ruby><rt>outside</rt>"
      "<v  Alice &amp;  Bob >voice</v><lang en-US><c.blue>lang</c></lang>"
      "<unknown>x</unknown><00:01.001>T";
   size_t work;
   for (work = 1; work < 8; work++)
   {
      rsubtitle_srt_t *track = text_track(body, work);
      const rsubtitle_vtt_node_t *node;
      const char *name;
      char buffer[256];
      size_t b, i, ruby, rt, voice, language, timestamp, length;
      assert(track);
      plain(track, 0, buffer, sizeof(buffer));
      assert(!strcmp(buffer, "ABIJKU\346\274\242\343\201\213\343\202\223outsidevoicelangxT"));
      node = rsubtitle_vtt_node(track, 0, 1);
      assert(node->kind == RSUBTITLE_VTT_TEXT && node->value == rsubtitle_srt_cue(track, 0)->text);
      b = find_node(track, 0, RSUBTITLE_VTT_BOLD, 0);
      i = find_node(track, 0, RSUBTITLE_VTT_ITALIC, 0);
      assert(b != RSUBTITLE_VTT_NO_NODE && i != RSUBTITLE_VTT_NO_NODE);
      assert(rsubtitle_vtt_node(track, 0, i)->parent == b);
      assert(rsubtitle_vtt_node(track, 0, b)->class_count == 2);
      name = rsubtitle_vtt_node_class(track, 0, b, 0, &length);
      assert(name && length == 3 && !memcmp(name, "hot", 3));
      name = rsubtitle_vtt_node_class(track, 0, b, 1, &length);
      assert(name && length == 4 && !memcmp(name, "loud", 4));
      assert(!rsubtitle_vtt_node_class(track, 0, b, 2, &length) && !length);
      ruby = find_node(track, 0, RSUBTITLE_VTT_RUBY, 0);
      rt = find_node(track, 0, RSUBTITLE_VTT_RUBY_TEXT, 0);
      assert(ruby != RSUBTITLE_VTT_NO_NODE && rt != RSUBTITLE_VTT_NO_NODE);
      assert(rsubtitle_vtt_node(track, 0, rt)->parent == ruby);
      assert(find_node(track, 0, RSUBTITLE_VTT_RUBY_TEXT, rt + 1) == RSUBTITLE_VTT_NO_NODE);
      voice = find_node(track, 0, RSUBTITLE_VTT_VOICE, 0);
      node = rsubtitle_vtt_node(track, 0, voice);
      assert(node && node->value_length == 11 && !memcmp(node->value, "Alice & Bob", 11));
      language = find_node(track, 0, RSUBTITLE_VTT_LANGUAGE, 0);
      node = rsubtitle_vtt_node(track, 0, language);
      assert(node && node->value_length == 5 && !memcmp(node->value, "en-US", 5));
      timestamp = find_node(track, 0, RSUBTITLE_VTT_TIMESTAMP, 0);
      node = rsubtitle_vtt_node(track, 0, timestamp);
      assert(node && node->timestamp_us == 1001000 && !node->value_length);
      assert(!rsubtitle_vtt_node(track, 0, rsubtitle_vtt_node_count(track, 0)));
      assert(!rsubtitle_vtt_node_count(track, 1) && !rsubtitle_vtt_node(NULL, 0, 0));
      rsubtitle_srt_free(track);
   }
}

static void entities(void)
{
   static const char body[] = "&amp;&lt;&gt;&nbsp;&lrm;&rlm; &notit; &NotEqualTilde; "
      "&#65; &#x1f600; &#128; &#0; &#xD800; &#1114112; &#; &#xZ; &bogus; &amp";
   static const char expected[] = "&<>\302\240\342\200\216\342\200\217 \302\254it; "
      "\342\211\202\314\270 A \360\237\230\200 \342\202\254 "
      "\357\277\275 \357\277\275 \357\277\275 &#; &#xZ; &bogus; &";
   size_t work;
   for (work = 1; work < 8; work++)
   {
      rsubtitle_srt_t *track = text_track(body, work);
      char buffer[512];
      assert(track);
      plain(track, 0, buffer, sizeof(buffer));
      assert(!strcmp(buffer, expected));
      rsubtitle_srt_free(track);
      track = text_track("<v A &lt;b&gt; &#32; B &>x", work);
      assert(track);
      {
         const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, 0,
               find_node(track, 0, RSUBTITLE_VTT_VOICE, 0));
         assert(node && node->value_length == 9 && !memcmp(node->value, "A <b> B &", 9));
      }
      rsubtitle_srt_free(track);
   }
}

static void text_recovery(void)
{
   rsubtitle_srt_t *track = text_track("<b", 1);
   const rsubtitle_vtt_node_t *node;
   char buffer[128];
   size_t index;
   assert(track && rsubtitle_vtt_node_count(track, 0) == 2);
   assert(rsubtitle_vtt_node(track, 0, 1)->kind == RSUBTITLE_VTT_BOLD);
   rsubtitle_srt_free(track);
   track = text_track("<v Alice", 1);
   assert(track);
   node = rsubtitle_vtt_node(track, 0, 1);
   assert(node && node->kind == RSUBTITLE_VTT_VOICE
         && node->value_length == 5 && !memcmp(node->value, "Alice", 5));
   rsubtitle_srt_free(track);
   track = text_track("<1:02:03.004><02:03.004><00:01.000x><60:00.001>x", 1);
   assert(track);
   index = find_node(track, 0, RSUBTITLE_VTT_TIMESTAMP, 0);
   assert(rsubtitle_vtt_node(track, 0, index)->timestamp_us == (int64_t)3723004 * 1000);
   index = find_node(track, 0, RSUBTITLE_VTT_TIMESTAMP, index + 1);
   assert(rsubtitle_vtt_node(track, 0, index)->timestamp_us == 123004000);
   assert(find_node(track, 0, RSUBTITLE_VTT_TIMESTAMP, index + 1) == RSUBTITLE_VTT_NO_NODE);
   plain(track, 0, buffer, sizeof(buffer));
   assert(!strcmp(buffer, "x"));
   rsubtitle_srt_free(track);
   track = parse("WEBVTT\n\n00:02.000 --> 00:03.000\n<b>later</b>\n\n"
         "00:01.000 --> 00:02.000\n<i>earlier</i>", 1);
   assert(track && rsubtitle_srt_count(track) == 2);
   assert(find_node(track, 0, RSUBTITLE_VTT_ITALIC, 0) != RSUBTITLE_VTT_NO_NODE);
   assert(find_node(track, 1, RSUBTITLE_VTT_BOLD, 0) != RSUBTITLE_VTT_NO_NODE);
   plain(track, 0, buffer, sizeof(buffer)); assert(!strcmp(buffer, "earlier"));
   plain(track, 1, buffer, sizeof(buffer)); assert(!strcmp(buffer, "later"));
   rsubtitle_srt_free(track);
}

static void text_limits(void)
{
   unsigned depth;
   for (depth = 127; depth < 129; depth++)
   {
      char *body = (char*)malloc(depth * 7 + 2);
      size_t i, size = 0;
      rsubtitle_srt_t *track;
      assert(body);
      for (i = 0; i < depth; i++) { memcpy(body + size, "<b>", 3); size += 3; }
      body[size++] = 'x';
      for (i = 0; i < depth; i++) { memcpy(body + size, "</b>", 4); size += 4; }
      body[size] = 0;
      track = text_track(body, 1);
      assert(depth == 127 ? track != NULL : track == NULL);
      rsubtitle_srt_free(track);
      free(body);
   }
   {
      char *body = (char*)malloc(65536);
      char buffer[8];
      rsubtitle_srt_t *track;
      assert(body);
      body[0] = '&'; body[1] = '#'; memset(body + 2, '9', 65530);
      body[65532] = ';'; body[65533] = 0;
      track = text_track(body, 7);
      assert(track);
      plain(track, 0, buffer, sizeof(buffer));
      assert(!strcmp(buffer, "\357\277\275"));
      rsubtitle_srt_free(track); free(body);
   }
}


static void text_chunks(void)
{
   static const char prefix[] = "00:01.000 --> 00:02.000\n<v ";
   static const char suffix[] = ">x</v>\n\n";
   char *input = (char*)malloc(130000);
   size_t size = 8, i, index;
   rsubtitle_srt_t *track;
   assert(input);
   memcpy(input, "WEBVTT\n\n", 8);
   for (i = 0; i < 3; i++)
   {
      memcpy(input + size, prefix, sizeof(prefix) - 1);
      size += sizeof(prefix) - 1;
      memset(input + size, (int)('a' + i), 40000);
      size += 40000;
      memcpy(input + size, suffix, sizeof(suffix) - 1);
      size += sizeof(suffix) - 1;
   }
   input[size] = 0;
   track = parse(input, 7);
   assert(track && rsubtitle_srt_count(track) == 3);
   for (i = 0; i < 3; i++)
   {
      const rsubtitle_vtt_node_t *node;
      index = find_node(track, i, RSUBTITLE_VTT_VOICE, 0);
      node = rsubtitle_vtt_node(track, i, index);
      assert(node && node->value_length == 40000);
      for (index = 0; index < node->value_length; index++)
         assert(node->value[index] == (char)('a' + i));
   }
   rsubtitle_srt_free(track);
   size = 0;
   for (i = 0; i < 300; i++)
   {
      memcpy(input + size, "<c.blue>x</c>", 13);
      size += 13;
   }
   input[size] = 0;
   track = text_track(input, 1);
   assert(track && rsubtitle_vtt_node_count(track, 0) == 601);
   for (i = 1; i < 601; i += 2)
   {
      const char *name = rsubtitle_vtt_node_class(track, 0, i, 0, &size);
      assert(name && size == 4 && !memcmp(name, "blue", 4));
   }
   rsubtitle_srt_free(track);
   free(input);
}

static rsubtitle_srt_t *settings_track(const char *settings, size_t work)
{
   char input[256];
   assert(strlen(settings) <= 128);
   sprintf(input, "WEBVTT\n\n00:00.000 --> 00:01.000 %s\nx", settings);
   return parse(input, work);
}

static void cue_settings(void)
{
   size_t work;
   for (work = 1; work < 8; work++)
   {
      rsubtitle_srt_t *track = settings_track("", work);
      const rsubtitle_vtt_settings_t *settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->size == 100.0 && settings->line_auto
            && settings->position_auto && settings->snap_to_lines);
      assert(settings->align == RSUBTITLE_VTT_CENTER
            && settings->line_align == RSUBTITLE_VTT_START
            && settings->position_align == RSUBTITLE_VTT_AUTO
            && settings->direction == RSUBTITLE_VTT_HORIZONTAL && !settings->region);
      assert(!rsubtitle_vtt_cue_settings(track, 1) && !rsubtitle_vtt_cue_settings(NULL, 0));
      rsubtitle_srt_free(track);
      track = settings_track("line:-2.5,end position:12.5%,line-left size:80% align:right vertical:rl", work);
      settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->line == -2.5 && !settings->line_auto
            && settings->snap_to_lines && settings->line_align == RSUBTITLE_VTT_END);
      assert(settings->position == 12.5 && !settings->position_auto
            && settings->position_align == RSUBTITLE_VTT_LEFT && settings->size == 80.0);
      assert(settings->align == RSUBTITLE_VTT_RIGHT && settings->direction == RSUBTITLE_VTT_VERTICAL_RL);
      rsubtitle_srt_free(track);
      track = settings_track("region:one line:25%,center region:two size:75% region:three position:80%,line-right", work);
      settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->line == 25.0 && !settings->snap_to_lines
            && settings->line_align == RSUBTITLE_VTT_CENTER);
      assert(settings->position == 80.0 && settings->position_align == RSUBTITLE_VTT_RIGHT);
      assert(settings->region_length == 5 && !memcmp(settings->region, "three", 5));
      rsubtitle_srt_free(track);
      track = settings_track("region:one vertical:lr vertical:bad", work);
      settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->direction == RSUBTITLE_VTT_VERTICAL_LR
            && !settings->region && !settings->region_length);
      rsubtitle_srt_free(track);
      track = settings_track("line:5%,end line:-5% line:2,bad position:20%,center position:30%,auto size:101% align:CENTER", work);
      settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->line == 5.0 && settings->line_align == RSUBTITLE_VTT_END
            && !settings->snap_to_lines && settings->position == 20.0
            && settings->position_align == RSUBTITLE_VTT_CENTER && settings->size == 100.0);
      assert(settings->align == RSUBTITLE_VTT_CENTER);
      rsubtitle_srt_free(track);
      track = settings_track("line:.5 line:5. line:+1 line:1e2 position:10 size:100.1% size:0% align:start align:left", work);
      settings = rsubtitle_vtt_cue_settings(track, 0);
      assert(settings && settings->line_auto && settings->position_auto && settings->size == 0.0
            && settings->align == RSUBTITLE_VTT_LEFT);
      rsubtitle_srt_free(track);
   }
}

static void style_components(void)
{
   static const char input[] = "WEBVTT\n\nSTYLE\n"
      "::cue(.red[voice=\"A; } B\"]) { color:rgb(1,2%,3);font-family:\"A\\20 B\" }\n\n"
      "REGION\nid:bottom\n\nSTYLE\n::cue-region(#bottom){color:yellow}\n\n"
      "00:00.000 --> 00:01.000\ncaption";
   char *data = (char*)malloc(sizeof(input) - 1);
   rsubtitle_srt_t *track;
   const rsubtitle_vtt_block_t *first, *second;
   size_t ticks = 0, i;
   unsigned seen = 0;
   int status = 0;
   assert(data);
   memcpy(data, input, sizeof(input) - 1);
   track = rsubtitle_vtt_begin(data, sizeof(input) - 1);
   assert(track);
   while (!status && ticks++ < sizeof(input) * 8)
   {
      status = rsubtitle_srt_iterate(track, 1);
      if (!status)
         assert(!rsubtitle_vtt_css_node_count(track) && !rsubtitle_vtt_css_node(track, 0));
   }
   assert(status == 1 && rsubtitle_srt_count(track) == 1 && rsubtitle_vtt_block_count(track) == 3);
   first = rsubtitle_vtt_block(track, 0); second = rsubtitle_vtt_block(track, 2);
   assert(first->css_root == 0 && second->css_root > first->css_root
         && second->css_root < rsubtitle_vtt_css_node_count(track));
   assert(rsubtitle_vtt_block(track, 1)->css_root == RSUBTITLE_VTT_NO_NODE);
   for (i = 0; i < rsubtitle_vtt_css_node_count(track); i++)
   {
      const rsubtitle_vtt_css_node_t *node = rsubtitle_vtt_css_node(track, i);
      if (node->token.kind == RSUBTITLE_CSS_STRING)
      {
         if (!strcmp(node->token.text, "A; } B")) seen |= 1;
         if (!strcmp(node->token.text, "A B")) seen |= 2;
      }
      if (node->token.kind == RSUBTITLE_CSS_PERCENT && node->token.number == 2) seen |= 4;
      if (node->token.kind == RSUBTITLE_CSS_ROOT)
         assert(node->parent == RSUBTITLE_VTT_NO_NODE && node->next == RSUBTITLE_VTT_NO_NODE);
   }
   assert(seen == 7 && !rsubtitle_vtt_css_node(track, i));
   assert(!rsubtitle_vtt_css_node_count(NULL) && !rsubtitle_vtt_css_node(NULL, 0));
   rsubtitle_srt_free(track);
}

static void style_depth_limit(void)
{
   unsigned depth;
   for (depth = 128; depth <= 129; depth++)
   {
      char input[512];
      size_t size = 0, ticks = 0;
      char *data;
      rsubtitle_srt_t *track;
      int status = 0;
      size += sprintf(input + size, "WEBVTT\n\nSTYLE\n");
      memset(input + size, '(', depth); size += depth;
      input[size++] = 'x';
      memset(input + size, ')', depth); size += depth;
      size += sprintf(input + size, "\n\n00:00.000 --> 00:01.000\ncaption");
      data = (char*)malloc(size);
      assert(data);
      memcpy(data, input, size);
      track = rsubtitle_vtt_begin(data, size);
      assert(track);
      while (!status && ticks++ < size * 8) status = rsubtitle_srt_iterate(track, 1);
      assert(depth == 128 ? status == 1 : status == -1);
      if (status < 0)
         assert(!rsubtitle_srt_count(track) && !rsubtitle_vtt_css_node_count(track)
               && !rsubtitle_vtt_block_count(track));
      rsubtitle_srt_free(track);
   }
}

static void temporal_model(const rsubtitle_srt_t *track, size_t cue)
{
   static const int64_t positions[] = {-1, 0, 1000000, 1000001, 5000000, 10000000};
   size_t count = rsubtitle_vtt_node_count(track, cue), i, j, k;
   for (i = 0; i < count; i++)
      for (k = 0; k < sizeof(positions) / sizeof(positions[0]); k++)
      {
         const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, i);
         unsigned expected = 0;
         if (node->kind != RSUBTITLE_VTT_TEXT && node->kind != RSUBTITLE_VTT_TIMESTAMP)
            for (j = 0; j < count; j++)
            {
               const rsubtitle_vtt_node_t *timestamp = rsubtitle_vtt_node(track, cue, j);
               size_t parent;
               if (timestamp->kind != RSUBTITLE_VTT_TIMESTAMP) continue;
               if (j < i && timestamp->timestamp_us > positions[k])
                  expected |= RSUBTITLE_VTT_NODE_FUTURE;
               parent = timestamp->parent;
               while (parent != RSUBTITLE_VTT_NO_NODE && parent != i)
                  parent = rsubtitle_vtt_node(track, cue, parent)->parent;
               if (j > i && parent != i && timestamp->timestamp_us < positions[k])
                  expected |= RSUBTITLE_VTT_NODE_PAST;
            }
         assert(rsubtitle_vtt_node_state(track, cue, i, positions[k]) == expected);
      }
}

static void temporal_chunks(void)
{
   char *input = (char*)malloc(32768);
   size_t cue, i, size = 0;
   uint32_t random = 17;
   rsubtitle_srt_t *track;
   assert(input);
   size += sprintf(input + size, "WEBVTT\n\n");
   for (cue = 0; cue < 3; cue++)
   {
      unsigned depth = 0;
      size += sprintf(input + size, "00:%02u.000 --> 00:%02u.000\n",
            (unsigned)(2 - cue) * 10, (unsigned)(2 - cue) * 10 + 10);
      for (i = 0; i < 180; i++)
      {
         unsigned choice;
         random = random * UINT32_C(1664525) + UINT32_C(1013904223);
         choice = (random >> 16) % 5;
         if (choice == 0 && depth < 16)
         {
            size += sprintf(input + size, "<c.red>"); depth++;
         }
         else if (choice == 1 && depth)
         {
            size += sprintf(input + size, "</c>"); depth--;
         }
         else if (choice == 2)
            size += sprintf(input + size, "<00:0%u.000>", (unsigned)(random % 10));
         else size += sprintf(input + size, "x&amp;");
      }
      while (depth) { size += sprintf(input + size, "</c>"); depth--; }
      size += sprintf(input + size, "\n\n");
   }
   track = parse(input, 1);
   assert(track && rsubtitle_srt_count(track) == 3);
   for (cue = 0; cue < 3; cue++) temporal_model(track, cue);
   rsubtitle_srt_free(track);
   free(input);
}

static void temporal_relative(void)
{
   static const char payload[] = "<00:00.000><c>x</c><00:01.000>";
   char *data = (char*)malloc(sizeof(payload));
   char *header = (char*)malloc(7);
   rsubtitle_srt_span_t *span = (rsubtitle_srt_span_t*)calloc(1, sizeof(*span));
   rsubtitle_vtt_span_t *info = (rsubtitle_vtt_span_t*)calloc(1, sizeof(*info));
   rsubtitle_srt_t *track;
   unsigned ticks = 0;
   int status = 0;
   assert(data && header && span && info);
   memcpy(data, payload, sizeof(payload));
   memcpy(header, "WEBVTT\n", 7);
   span->end_us = 1000000;
   span->length = sizeof(payload) - 1;
   info->identifier_offset = info->settings_offset = sizeof(payload) - 1;
   info->flags = RSUBTITLE_VTT_CUE_RELATIVE_TIMESTAMPS;
   info->timestamp_origin_ns = -1234567;
   track = rsubtitle_vtt_begin_cues(data, sizeof(payload), span, 1, info, header, 7);
   assert(track);
   while (!status && ticks++ < 4096) status = rsubtitle_srt_iterate(track, 1);
   assert(status == 1);
   assert(rsubtitle_vtt_node_state(track, 0, 2, -1235) == RSUBTITLE_VTT_NODE_FUTURE);
   assert(!rsubtitle_vtt_node_state(track, 0, 2, -1234));
   assert(!rsubtitle_vtt_node_state(track, 0, 2, 998766));
   assert(rsubtitle_vtt_node_state(track, 0, 2, 998767) == RSUBTITLE_VTT_NODE_PAST);
   temporal_model(track, 0);
   rsubtitle_srt_free(track);
}

static void temporal_states(void)
{
   static const char input[] = "WEBVTT\n\n00:00.000 --> 00:05.000\n"
      "<00:01.000><c>A<b>B<00:02.000>C</b>D</c>"
      "<00:03.000><i>E</i><00:04.000>\n\n"
      "00:06.000 --> 00:10.000\n<c>no timestamps</c>\n\n"
      "00:11.000 --> 00:16.000\n"
      "<00:14.000><00:11.000><c>x</c><00:13.000><00:12.000>";
   size_t work, i;
   for (work = 1; work <= 7; work += 6)
   {
      rsubtitle_srt_t *track = parse(input, work);
      const rsubtitle_vtt_node_t *node;
      assert(track && rsubtitle_srt_count(track) == 3);
      assert(rsubtitle_vtt_node_count(track, 0) == 13);
      node = rsubtitle_vtt_node(track, 0, 2);
      assert(node && node->kind == RSUBTITLE_VTT_CLASS);
      assert(node->past_timestamp == 10 && node->future_timestamp == 2);
      node = rsubtitle_vtt_node(track, 0, 4);
      assert(node && node->kind == RSUBTITLE_VTT_BOLD);
      assert(node->past_timestamp == 10 && node->future_timestamp == 2);
      assert(rsubtitle_vtt_node_state(track, 0, 2, 999999) == RSUBTITLE_VTT_NODE_FUTURE);
      assert(!rsubtitle_vtt_node_state(track, 0, 2, 1000000));
      assert(!rsubtitle_vtt_node_state(track, 0, 4, 2500000));
      assert(!rsubtitle_vtt_node_state(track, 0, 2, 3000000));
      assert(rsubtitle_vtt_node_state(track, 0, 4, 3000001) == RSUBTITLE_VTT_NODE_PAST);
      node = rsubtitle_vtt_node(track, 0, 10);
      assert(node && node->kind == RSUBTITLE_VTT_ITALIC);
      assert(node->past_timestamp == 13 && node->future_timestamp == 10);
      assert(rsubtitle_vtt_node_state(track, 0, 10, 2999999) == RSUBTITLE_VTT_NODE_FUTURE);
      assert(!rsubtitle_vtt_node_state(track, 0, 10, 3000000));
      assert(!rsubtitle_vtt_node_state(track, 0, 10, 4000000));
      assert(rsubtitle_vtt_node_state(track, 0, 10, 4000001) == RSUBTITLE_VTT_NODE_PAST);
      for (i = 0; i < rsubtitle_vtt_node_count(track, 0); i++)
      {
         node = rsubtitle_vtt_node(track, 0, i);
         if (node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP || !i)
         {
            assert(!rsubtitle_vtt_node_state(track, 0, i, 0));
            assert(!rsubtitle_vtt_node_state(track, 0, i, 5000000));
         }
      }
      node = rsubtitle_vtt_node(track, 1, 1);
      assert(node && !node->past_timestamp && !node->future_timestamp);
      assert(!rsubtitle_vtt_node_state(track, 1, 1, 0));
      assert(!rsubtitle_vtt_node_state(track, 1, 1, 20000000));
      node = rsubtitle_vtt_node(track, 2, 3);
      assert(node && node->past_timestamp == 7 && node->future_timestamp == 2);
      assert(rsubtitle_vtt_node_state(track, 2, 3, 13000000)
            == (RSUBTITLE_VTT_NODE_PAST | RSUBTITLE_VTT_NODE_FUTURE));
      assert(!rsubtitle_vtt_node_state(NULL, 0, 0, 0));
      assert(!rsubtitle_vtt_node_state(track, 3, 0, 0));
      assert(!rsubtitle_vtt_node_state(track, 0, 100, 0));
      rsubtitle_srt_free(track);
   }
}

static void hint_limits(void)
{
   static const char prefix[] = "WEBVTT\n\n00:00.000 --> 00:01.000\n<c.";
   static const char suffix[] = ".red.bg_blue>x</c>";
   char *input = (char*)malloc(65000);
   size_t i, size = sizeof(prefix) - 1;
   rsubtitle_srt_t *track;
   const rsubtitle_vtt_node_t *node;
   assert(input);
   memcpy(input, prefix, size);
   memset(input + size, 'x', 60000); size += 60000;
   memcpy(input + size, suffix, sizeof(suffix));
   track = parse(input, 1);
   assert(track);
   node = rsubtitle_vtt_node(track, 0, 1);
   assert(node && node->hints.color == UINT32_C(0xff0000ff)
         && node->hints.background == UINT32_C(0x0000ffff));
   rsubtitle_srt_free(track);
   size = sizeof(prefix) - 1;
   for (i = 0; i < 10000; i++)
   {
      memcpy(input + size, "red.", 4); size += 4;
   }
   memcpy(input + size, "white.bg_black>x</c>", sizeof("white.bg_black>x</c>"));
   track = parse(input, 1);
   assert(track);
   node = rsubtitle_vtt_node(track, 0, 1);
   assert(node && node->class_count == 10002
         && node->hints.color == UINT32_C(0xffffffff)
         && node->hints.background == UINT32_C(0x000000ff));
   rsubtitle_srt_free(track);
   free(input);
}

static void presentation_hints(void)
{
   static const char text[] = "WEBVTT\n\n00:00.000 --> 00:02.000\n"
      "<c.yellow.bg_blue.magenta.bg_black>x<b.red>y<i.cyan>z</i></b></c>"
      "<u.lime.bg_white>u</u><v.blue Speaker>v</v>"
      "<lang.black en>l</lang><ruby.white>r<rt.red>t</rt></ruby>"
      "<c.black.white.bg_black.bg_white>w</c>"
      "<c.RED.bg_Blue.green>plain</c><00:01.000>end";
   static const uint32_t colors[] = {
      UINT32_C(0xffffffff), UINT32_C(0x00ff00ff), UINT32_C(0x00ffffff),
      UINT32_C(0xff0000ff), UINT32_C(0xffff00ff), UINT32_C(0xff00ffff),
      UINT32_C(0x0000ffff), UINT32_C(0x000000ff)
   };
   static const char *names[] = {
      "white", "lime", "cyan", "red", "yellow", "magenta", "blue", "black"
   };
   size_t work, i;
   for (work = 1; work <= 7; work += 6)
   {
      rsubtitle_srt_t *track = parse(text, work);
      const rsubtitle_vtt_node_t *node;
      unsigned seen = 0;
      assert(track);
      for (i = 0; i < rsubtitle_vtt_node_count(track, 0); i++)
      {
         node = rsubtitle_vtt_node(track, 0, i);
         assert(node);
         if (node->kind == RSUBTITLE_VTT_TEXT
               || node->kind == RSUBTITLE_VTT_TIMESTAMP || !i)
         {
            assert(!node->hints.flags && !node->hints.color && !node->hints.background);
            continue;
         }
         switch (seen++)
         {
            case 0:
               assert(node->hints.flags == (RSUBTITLE_VTT_HINT_COLOR | RSUBTITLE_VTT_HINT_BACKGROUND));
               assert(node->hints.color == UINT32_C(0xff00ffff)
                     && node->hints.background == UINT32_C(0x000000ff));
               break;
            case 1:
               assert(node->hints.flags == (RSUBTITLE_VTT_HINT_COLOR | RSUBTITLE_VTT_HINT_BOLD));
               assert(node->hints.color == UINT32_C(0xff0000ff));
               break;
            case 2:
               assert(node->hints.flags == (RSUBTITLE_VTT_HINT_COLOR | RSUBTITLE_VTT_HINT_ITALIC));
               assert(node->hints.color == UINT32_C(0x00ffffff));
               break;
            case 3:
               assert(node->hints.flags == (RSUBTITLE_VTT_HINT_COLOR
                        | RSUBTITLE_VTT_HINT_BACKGROUND | RSUBTITLE_VTT_HINT_UNDERLINE));
               assert(node->hints.color == UINT32_C(0x00ff00ff)
                     && node->hints.background == UINT32_C(0xffffffff));
               break;
            case 4: assert(node->hints.color == UINT32_C(0x0000ffff)); break;
            case 5: assert(node->hints.color == UINT32_C(0x000000ff)); break;
            case 6: assert(node->hints.color == UINT32_C(0xffffffff)); break;
            case 7: assert(node->hints.color == UINT32_C(0xff0000ff)); break;
            case 8:
               assert(node->hints.color == UINT32_C(0xffffffff)
                     && node->hints.background == UINT32_C(0xffffffff));
               break;
            case 9: assert(!node->hints.flags); break;
            default: assert(0);
         }
      }
      assert(seen == 10);
      rsubtitle_srt_free(track);
   }
   for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
   {
      char input[128];
      rsubtitle_srt_t *track;
      const rsubtitle_vtt_node_t *node;
      sprintf(input, "WEBVTT\n\n00:00.000 --> 00:01.000\n<c.%s.bg_%s>x</c>", names[i], names[i]);
      track = parse(input, 1);
      assert(track);
      node = rsubtitle_vtt_node(track, 0, 1);
      assert(node && node->hints.color == colors[i] && node->hints.background == colors[i]);
      rsubtitle_srt_free(track);
   }
}

static void applicable_languages(void)
{
   static const char input[] =
      "WEBVTT\n\n00:00:03.000 --> 00:00:04.000\n"
      "<lang fr><b>French</b><lang><i>Untagged</i></lang>French"
      "<lang not well formed><u>Raw</u></lang></lang><c>Fallback</c>\n\n"
      "00:00:01.000 --> 00:00:02.000\n"
      "<lang en-US><c><lang de>German</lang>English</c></lang>Fallback\n\n";
   static const char *fallbacks[] = { "en", "fr-CA", "", "not well formed" };
   rsubtitle_srt_t *track = parse(input, 1);
   size_t cue, i, j;
   assert(track);
   for (cue = 0; cue < rsubtitle_srt_count(track); cue++)
      for (i = 0; i < rsubtitle_vtt_node_count(track, cue); i++)
         for (j = 0; j < sizeof(fallbacks) / sizeof(*fallbacks); j++)
         {
            const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, i);
            const rsubtitle_vtt_node_t *ancestor = node;
            const char *expected = fallbacks[j];
            const char *value;
            size_t length = strlen(expected), actual = 999, depth = 0;
            if (node->kind == RSUBTITLE_VTT_TEXT || node->kind == RSUBTITLE_VTT_TIMESTAMP)
            {
               expected = NULL;
               length = 0;
            }
            else
               for (;;)
               {
                  assert(depth++ <= 128);
                  if (ancestor->kind == RSUBTITLE_VTT_LANGUAGE)
                  {
                     expected = ancestor->value;
                     length = ancestor->value_length;
                     break;
                  }
                  if (ancestor->parent == RSUBTITLE_VTT_NO_NODE)
                     break;
                  ancestor = rsubtitle_vtt_node(track, cue, ancestor->parent);
                  assert(ancestor);
               }
            value = rsubtitle_vtt_node_language(track, cue, i,
                  fallbacks[j], strlen(fallbacks[j]), &actual);
            assert(value == expected && actual == length);
         }
   assert(!rsubtitle_vtt_node_language(track, 999, 0, "en", 2, NULL));
   assert(!rsubtitle_vtt_node_language(track, 0, 999, "en", 2, NULL));
   assert(!rsubtitle_vtt_node_language(track, 0, 0, NULL, 1, NULL));
   assert(!*rsubtitle_vtt_node_language(track, 0, 0, NULL, 0, NULL));
   rsubtitle_srt_free(track);
   for (j = 0; j < 2; j++)
   {
      char *body = (char*)malloc(4096);
      size_t used = 0, length = 999;
      const char *value;
      assert(body);
      for (i = 0; i < 126; i++)
      {
         const char *tag = j && i == 125 ? "<lang>" : "<lang fr>";
         memcpy(body + used, tag, strlen(tag));
         used += strlen(tag);
      }
      memcpy(body + used, "<c>x</c>", 8);
      used += 8;
      for (i = 0; i < 126; i++)
      {
         memcpy(body + used, "</lang>", 7);
         used += 7;
      }
      body[used] = 0;
      track = text_track(body, 1);
      assert(track);
      value = rsubtitle_vtt_node_language(track, 0, 127, "en", 2, &length);
      assert(value && length == (j ? 0 : 2));
      assert(!memcmp(value, j ? "" : "fr", length));
      rsubtitle_srt_free(track);
      free(body);
   }
}

static void prepared_languages(void)
{
   static const char input[] =
      "WEBVTT\n\n00:03.000 --> 00:04.000\n"
      "<lang iw-BU><b>A</b><lang cmn><i>B</i></lang>"
      "<lang><u>C</u></lang><lang not well formed><b>D</b></lang></lang><c>E</c>\n\n"
      "00:01.000 --> 00:02.000\n"
      "<lang en-US><c><lang ar-ajp>F</lang>G</c></lang>H\n\n";
   rsubtitle_srt_t *track = parse(input, 1);
   size_t cue, i;
   assert(track);
   for (cue = 0; cue < rsubtitle_srt_count(track); cue++)
      for (i = 0; i < rsubtitle_vtt_node_count(track, cue); i++)
      {
         const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, cue, i);
         const char *raw, *expected, *key, *again;
         size_t length, raw_length, actual;
         raw = rsubtitle_vtt_node_language(track, cue, i, "zh-cmn", 6, &raw_length);
         expected = raw;
         length = raw_length;
         if (raw && raw_length == 5 && !memcmp(raw, "iw-BU", 5))
         {
            expected = "he-mm";
            length = 5;
         }
         else if (raw && raw_length == 3 && !memcmp(raw, "cmn", 3))
         {
            expected = "zh-cmn";
            length = 6;
         }
         else if (raw && raw_length == 6 && !memcmp(raw, "ar-ajp", 6))
         {
            expected = "ar-apc";
            length = 6;
         }
         key = rsubtitle_vtt_node_language_key(track, cue, i, "zh-cmn", 6, &actual);
         assert(actual == length && !!key == !!expected);
         if (key)
         {
            assert(!memcmp(key, expected, length));
            if (expected == raw) assert(key == raw);
            again = rsubtitle_vtt_node_language_key(track, cue, i, "ar-apc", 6, &actual);
            if (node->language_node) assert(again == key && actual == length);
            else assert(actual == 6 && !memcmp(again, "ar-apc", 6));
         }
      }
   assert(!rsubtitle_vtt_node_language_key(track, 999, 0, "en", 2, NULL));
   assert(!rsubtitle_vtt_node_language_key(track, 0, 999, "en", 2, NULL));
   assert(!rsubtitle_vtt_node_language_key(track, 0, 0, NULL, 1, NULL));
   assert(!*rsubtitle_vtt_node_language_key(track, 0, 0, NULL, 0, NULL));
   rsubtitle_srt_free(track);
   {
      char *body = (char*)malloc(20000);
      const char *shared = NULL;
      size_t used = 0;
      assert(body);
      for (i = 0; i < 600; i++)
      {
         static const char span[] = "<lang iw><b>x</b></lang>";
         memcpy(body + used, span, sizeof(span) - 1);
         used += sizeof(span) - 1;
      }
      body[used] = 0;
      track = text_track(body, 1);
      assert(track);
      for (i = 1; i < rsubtitle_vtt_node_count(track, 0); i++)
      {
         const char *key;
         size_t length;
         if (rsubtitle_vtt_node(track, 0, i)->kind == RSUBTITLE_VTT_TEXT) continue;
         key = rsubtitle_vtt_node_language_key(track, 0, i, NULL, 0, &length);
         assert(key && length == 2 && !memcmp(key, "he", 2));
         if (shared) assert(key == shared);
         shared = key;
      }
      rsubtitle_srt_free(track);
      free(body);
   }
   {
      char *body = (char*)malloc(50000);
      char span[64];
      size_t used = 0;
      assert(body);
      for (i = 0; i < 1200; i++)
      {
         size_t length;
         sprintf(span, "<lang iw-x-%04u><b>x</b></lang>", (unsigned)(i % 600));
         length = strlen(span);
         memcpy(body + used, span, length);
         used += length;
      }
      body[used] = 0;
      track = text_track(body, 13);
      assert(track);
      for (i = 0; i < 600; i++)
      {
         const char *first, *again;
         size_t length, second;
         sprintf(span, "he-x-%04u", (unsigned)i);
         first = rsubtitle_vtt_node_language_key(track, 0, 1 + i * 3, NULL, 0, &length);
         again = rsubtitle_vtt_node_language_key(track, 0, 1 + (i + 600) * 3, NULL, 0, &second);
         assert(first && first == again && length == second && length == strlen(span));
         assert(!memcmp(first, span, length));
      }
      rsubtitle_srt_free(track);
      free(body);
   }
}

static void track_languages(void)
{
   static const char input[] = "WEBVTT\n\n00:00.000 --> 00:01.000\n<b>x</b><lang >y</lang><lang iw>z</lang>\n";
   static const char *raw[] = { "iw", "en-US", "en--US", "" };
   static const char *key[] = { "he", "en-US", "en--US", "" };
   size_t i;
   for (i = 0; i < sizeof(raw) / sizeof(raw[0]); i++)
   {
      size_t n = strlen(raw[i]), length = 99, steps = 0;
      char *data = (char*)malloc(sizeof(input) - 1);
      char *language = (char*)malloc(n ? n : 1);
      rsubtitle_srt_t *track;
      const char *value;
      int status = 0;
      assert(data && language);
      memcpy(data, input, sizeof(input) - 1);
      memcpy(language, raw[i], n);
      track = rsubtitle_vtt_begin(data, sizeof(input) - 1);
      assert(track && rsubtitle_vtt_take_language(track, language, n));
      assert(!rsubtitle_vtt_track_language(track, &length) && !length);
      assert(!rsubtitle_vtt_track_language_key(track, &length) && !length);
      assert(!rsubtitle_vtt_take_language(track, (char*)malloc(1), 0));
      while (!status && steps++ < 10000)
         status = rsubtitle_srt_iterate(track, 1);
      assert(status == 1);
      value = rsubtitle_vtt_track_language(track, &length);
      assert(value == language && length == n && !memcmp(value, raw[i], n));
      value = rsubtitle_vtt_track_language_key(track, &length);
      assert(value && length == strlen(key[i]) && !memcmp(value, key[i], length));
      value = rsubtitle_vtt_node_language(track, 0, 1, NULL, 0, &length);
      assert(value == language && length == n);
      value = rsubtitle_vtt_node_language_key(track, 0, 1, NULL, 0, &length);
      assert(value && length == strlen(key[i]) && !memcmp(value, key[i], length));
      value = rsubtitle_vtt_node_language(track, 0, 1, "", 0, &length);
      assert(value && !length);
      value = rsubtitle_vtt_node_language_key(track, 0, 5, NULL, 0, &length);
      assert(value && length == 2 && !memcmp(value, "he", 2));
      assert(!rsubtitle_vtt_take_language(track, (char*)malloc(1), 0));
      rsubtitle_srt_free(track);
   }
   assert(!rsubtitle_vtt_track_language(NULL, NULL));
   assert(!rsubtitle_vtt_track_language_key(NULL, NULL));
   assert(!rsubtitle_vtt_take_language(NULL, (char*)malloc(1), 0));
}

int main(void)
{
   track_languages();
   prepared_languages();
   applicable_languages();
   metadata();
   embedded();
   timestamp_clock();
   text_nodes();
   presentation_hints();
   hint_limits();
   temporal_states();
   temporal_chunks();
   temporal_relative();
   style_components();
   style_depth_limit();
   entities();
   text_recovery();
   text_limits();
   text_chunks();
   cue_settings();
   timing_recovery();
   regions();
   region_limits();
   recovery();
   limits();
   cancellation();
   puts("PASS: WebVTT incremental cues, retained metadata, recovery and cancellation");
   return 0;
}
