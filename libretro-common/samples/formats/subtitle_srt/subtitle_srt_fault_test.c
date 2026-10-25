/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_srt_fault_test.c).
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

static unsigned live, calls, fail_at;

static void *fault_malloc(size_t size)
{
   void *p;
   if (++calls == fail_at) return NULL;
   p = malloc(size);
   if (p) live++;
   return p;
}

static void *fault_calloc(size_t count, size_t size)
{
   void *p;
   if (++calls == fail_at) return NULL;
   p = calloc(count, size);
   if (p) live++;
   return p;
}

static void *fault_realloc(void *old, size_t size)
{
   void *p;
   int new_allocation = !old;
   if (++calls == fail_at) return NULL;
   p = realloc(old, size);
   if (p && new_allocation) live++;
   return p;
}

static void fault_free(void *p)
{
   if (p)
   {
      assert(live);
      live--;
      free(p);
   }
}

#define malloc fault_malloc
#define calloc fault_calloc
#define realloc fault_realloc
#define free fault_free
#include "../../../formats/subtitle/rsubtitle_srt.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void test_cue_failures(void)
{
   unsigned failure;
   for (failure = 1; failure <= 16; failure++)
   {
      char *text;
      rsubtitle_srt_span_t *spans;
      rsubtitle_srt_t *track;
      unsigned i, observed;
      int status = -1;
      fail_at = 0;
      text = (char*)fault_malloc(2);
      spans = (rsubtitle_srt_span_t*)fault_malloc(80 * sizeof(*spans));
      assert(text && spans);
      memcpy(text, "a", 2);
      for (i = 0; i < 80; i++)
      {
         spans[i].start_us = 80 - i;
         spans[i].end_us = 100;
         spans[i].offset = 0;
         spans[i].length = 1;
      }
      calls = 0;
      fail_at = failure;
      track = rsubtitle_srt_begin_cues(text, 2, spans, 80);
      if (track)
         do
         {
            status = rsubtitle_srt_iterate(track, 1);
         } while (!status);
      observed = calls;
      if (failure > observed)
         assert(track && status == 1 && rsubtitle_srt_count(track) == 80);
      else
         assert(!track || status == -1);
      rsubtitle_srt_free(track);
      assert(!live);
      if (failure > observed)
         break;
   }
   assert(failure > 3 && failure < 16);
}

int main(void)
{
   unsigned failure;
   for (failure = 1; failure <= 16; failure++)
   {
      char *text = (char*)fault_malloc(80 * 80);
      rsubtitle_srt_t *track;
      size_t length = 0;
      unsigned i, observed;
      assert(text);
      for (i = 0; i < 80; i++)
      {
         int n = sprintf(text + length,
               "%u\n00:00:%02u,000 --> 00:00:%02u,000\ncue %u\n\n",
               i, (79 - i) % 50, (79 - i) % 50 + 1, i);
         assert(n > 0 && n < 80);
         length += (size_t)n;
      }
      calls = 0;
      fail_at = failure;
      track = rsubtitle_srt_take(text, length);
      observed = calls;
      if (track)
      {
         assert(failure > observed && rsubtitle_srt_count(track) == 80);
         rsubtitle_srt_free(track);
      }
      else
         assert(failure <= observed);
      assert(!live);
      if (failure > observed) break;
   }
   assert(failure > 3 && failure < 16);
   test_cue_failures();
   puts("PASS: SubRip allocation failures release adopted input and cue storage");
   return 0;
}
