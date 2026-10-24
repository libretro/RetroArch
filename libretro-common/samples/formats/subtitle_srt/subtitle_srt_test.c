/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_srt_test.c).
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
#include <formats/rsubtitle_srt.h>

static rsubtitle_srt_t *parse(const char *text)
{
   size_t size = strlen(text);
   char *owned = (char*)malloc(size ? size : 1);
   assert(owned);
   memcpy(owned, text, size);
   return rsubtitle_srt_take(owned, size);
}

static void basic(void)
{
   rsubtitle_srt_t *track = parse("\357\273\2779\r\n"
         "00:00:02,000 --> 00:00:04,000\r\nsecond\r\n\r\n"
         "3\r\n00:00:01,000 --> 00:00:03,000\r\n"
         "hello\r\nworld \303\251\r\n \t\r\n"
         "00:00:02.000 --> 00:00:02.500\nthird");
   const rsubtitle_srt_cue_t *cue;
   assert(track && rsubtitle_srt_count(track) == 3);
   cue = rsubtitle_srt_cue(track, 0);
   assert(cue->start_us == 1000000 && cue->end_us == 3000000);
   assert(!strcmp(cue->text, "hello\nworld \303\251"));
   assert(cue->length == strlen(cue->text));
   assert(!strcmp(rsubtitle_srt_cue(track, 1)->text, "second"));
   assert(!strcmp(rsubtitle_srt_cue(track, 2)->text, "third"));
   assert(rsubtitle_srt_find(track, -1) == 3);
   assert(rsubtitle_srt_find(track, 999999) == 3);
   assert(rsubtitle_srt_find(track, 1000000) == 0);
   assert(rsubtitle_srt_find(track, 2500000) == 0);
   assert(rsubtitle_srt_find(track, 3000000) == 1);
   assert(rsubtitle_srt_find(track, 4000000) == 3);
   assert(!rsubtitle_srt_cue(track, 3));
   rsubtitle_srt_free(track);
   track = parse("2562047788:00:54,774 --> 2562047788:00:54,775\n"
         "\360\237\216\254");
   assert(track);
   cue = rsubtitle_srt_cue(track, 0);
   assert(cue->end_us == (int64_t)((uint64_t)-1 >> 1) / 1000 * 1000);
   assert(cue->end_us - cue->start_us == 1000 && cue->length == 4);
   rsubtitle_srt_free(track);
   assert(!rsubtitle_srt_cue(NULL, 0) && !rsubtitle_srt_count(NULL));
   assert(!rsubtitle_srt_find(NULL, 0));
   rsubtitle_srt_free(NULL);
}

static void malformed(void)
{
   static const char *cases[] = {
      "", "1", "1\n", "text", "00:00:00,000 --> 00:00:01,000\n",
      "00:00:00,000 --> 00:00:00,000\na",
      "00:00:02,000 --> 00:00:01,000\na",
      "00:60:00,000 --> 01:00:01,000\na",
      "00:00:60,000 --> 00:01:01,000\na",
      "00:00:00,0000 --> 00:00:01,000\na",
      "00:00:00,00 --> 00:00:01,000\na",
      "00:00:00,000 -> 00:00:01,000\na",
      "00:00:00,000 --> 00:00:01,000x\na",
      "999999999999999:00:00,000 --> 999999999999999:00:01,000\na",
      "2562047788:00:54,774 --> 2562047788:00:54,776\na",
      "00:00:00,000 --> 00:00:01,000\n\300\257",
      "00:00:00,000 --> 00:00:01,000\n\355\240\200",
      "00:00:00,000 --> 00:00:01,000\n\364\220\200\200",
      "00:00:00,000 --> 00:00:01,000\n\342\202"
   };
   size_t i, header;
   char *text;
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      assert(!parse(cases[i]));
   text = (char*)malloc(1);
   assert(text);
   assert(!rsubtitle_srt_take(text, (size_t)16 * 1024 * 1024 + 1));
   text = (char*)calloc(70000, 1);
   assert(text);
   strcpy(text, "00:00:00,000 --> 00:00:01,000\n");
   header = strlen(text);
   memset(text + header, 'x', 66000);
   assert(!rsubtitle_srt_take(text, header + 66000));
}

static void lookup_oracle(void)
{
   char *data = (char*)malloc(64 * 128);
   size_t length = 0, i;
   unsigned ms;
   rsubtitle_srt_t *track;
   assert(data);
   for (i = 0; i < 64; i++)
   {
      unsigned start = ((unsigned)i * 1709) % 50000;
      unsigned end = start + 1000 + ((unsigned)i * 317) % 8000;
      int n = sprintf(data + length,
            "%u\n00:00:%02u,%03u --> 00:00:%02u,%03u\ncue %u\n\n",
            (unsigned)i, start / 1000, start % 1000,
            end / 1000, end % 1000, (unsigned)i);
      assert(n > 0 && n < 128);
      length += (size_t)n;
   }
   track = rsubtitle_srt_take(data, length);
   assert(track && rsubtitle_srt_count(track) == 64);
   for (ms = 0; ms <= 60000; ms += 317)
   {
      size_t expected = 64;
      int64_t time_us = (int64_t)ms * 1000;
      for (i = 0; i < 64; i++)
      {
         const rsubtitle_srt_cue_t *cue = rsubtitle_srt_cue(track, i);
         if (cue->start_us <= time_us && cue->end_us > time_us)
         {
            expected = i;
            break;
         }
      }
      assert(rsubtitle_srt_find(track, time_us) == expected);
      for (i = 0; i <= 64; i++)
      {
         size_t j;
         expected = 64;
         for (j = i; j < 64; j++)
         {
            const rsubtitle_srt_cue_t *cue = rsubtitle_srt_cue(track, j);
            if (cue->start_us <= time_us && cue->end_us > time_us)
            {
               expected = j;
               break;
            }
         }
         assert(rsubtitle_srt_find_from(track, time_us, i) == expected);
      }
   }
   rsubtitle_srt_free(track);
}

static void incremental(void)
{
   const char *text = "2\r\n00:00:02,000 --> 00:00:04,000\r\nsecond\r\n\r\n"
      "1\n00:00:01,000 --> 00:00:03,000\nfirst \360\237\216\254\n\n"
      "3\n00:00:02,000 --> 00:00:02,500\nthird";
   size_t size = strlen(text), work, cut;
   for (work = 0; work < 18; work++)
   {
      char *owned = (char*)malloc(size);
      rsubtitle_srt_t *track;
      unsigned ticks = 0;
      int status = 0;
      assert(owned);
      memcpy(owned, text, size);
      track = rsubtitle_srt_begin(owned, size);
      assert(track && !rsubtitle_srt_iterate(track, 0));
      assert(!rsubtitle_srt_count(track) && !rsubtitle_srt_cue(track, 0));
      if (work)
      {
         while (!status && ticks++ < 4096)
            status = rsubtitle_srt_iterate(track, work);
         assert(status == 1 && rsubtitle_srt_count(track) == 3);
         assert(rsubtitle_srt_iterate(track, work) == 1);
         assert(!strcmp(rsubtitle_srt_cue(track, 0)->text,
                  "first \360\237\216\254"));
         assert(!strcmp(rsubtitle_srt_cue(track, 1)->text, "second"));
         assert(rsubtitle_srt_find(track, 3000000) == 1);
      }
      rsubtitle_srt_free(track);
   }
   for (cut = 0; cut < 512; cut++)
   {
      char *owned = (char*)malloc(size);
      rsubtitle_srt_t *track;
      assert(owned);
      memcpy(owned, text, size);
      track = rsubtitle_srt_begin(owned, size);
      assert(track);
      rsubtitle_srt_iterate(track, cut);
      rsubtitle_srt_free(track);
   }
}

int main(void)
{
   basic();
   malformed();
   lookup_oracle();
   incremental();
   puts("PASS: SubRip text, ordering, overlap lookup and malformed input");
   return 0;
}
