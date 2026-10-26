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
#include "../../../formats/subtitle/rsubtitle_vtt.c"
#include "../../../formats/subtitle/rsubtitle_vtt_css.c"
#include "../../../formats/subtitle/rsubtitle_vtt_css_tree.c"
#include "../../../formats/subtitle/rsubtitle_vtt_selector.c"
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

static void test_vtt_failures(void)
{
   unsigned failure;
   for (failure = 1; failure <= 64; failure++)
   {
      char *text;
      rsubtitle_srt_t *track;
      size_t length;
      unsigned i, observed;
      int status = -1;
      fail_at = 0;
      text = (char*)fault_malloc(32768);
      assert(text);
      memcpy(text, "WEBVTT\n\n", 8);
      length = 8;
      for (i = 0; i < 600; i++)
      {
         int n = sprintf(text + length, i & 1 ? "STYLE\n::cue { color: lime; }\n\n" : "REGION\nid:lower width:60%%\n\n");
         assert(n > 0 && n < 80);
         length += (size_t)n;
      }
      for (i = 0; i < 80; i++)
      {
         int n = sprintf(text + length,
               "id%u\n00:00:%02u.000 --> 00:00:%02u.000 align:center\n<v Alice &amp; Bob><b.hot>cue %u &NotEqualTilde;</b></v>\n\n",
               i, (79 - i) % 50, (79 - i) % 50 + 1, i);
         assert(n > 0 && n < 128);
         length += (size_t)n;
      }
      calls = 0;
      fail_at = failure;
      track = rsubtitle_vtt_begin(text, length);
      if (track)
         do status = rsubtitle_srt_iterate(track, 1); while (!status);
      observed = calls;
      if (failure > observed)
         assert(track && status == 1 && rsubtitle_srt_count(track) == 80
               && rsubtitle_vtt_block_count(track) == 600
               && rsubtitle_vtt_region_count(track) == 300);
      else
         assert(!track || status == -1);
      rsubtitle_srt_free(track);
      assert(!live);
      if (failure > observed) break;
   }
   assert(failure > 8 && failure < 64);
}

static void test_vtt_embedded_failures(void)
{
   static const char payload[] = "x\0id\0align:start\0";
   unsigned failure;
   for (failure = 1; failure <= 64; failure++)
   {
      char *text, *header;
      rsubtitle_srt_span_t *spans;
      rsubtitle_vtt_span_t *metadata;
      rsubtitle_srt_t *track;
      size_t length = 8;
      unsigned i, observed;
      int status = -1;
      fail_at = 0;
      text = (char*)fault_malloc(sizeof(payload));
      header = (char*)fault_malloc(32768);
      spans = (rsubtitle_srt_span_t*)fault_calloc(80, sizeof(*spans));
      metadata = (rsubtitle_vtt_span_t*)fault_calloc(80, sizeof(*metadata));
      assert(text && header && spans && metadata);
      memcpy(text, payload, sizeof(payload));
      memcpy(header, "WEBVTT\n\n", 8);
      for (i = 0; i < 600; i++)
      {
         int n = sprintf(header + length, i & 1 ? "STYLE\n::cue { color: lime; }\n\n" : "REGION\nid:lower width:60%%\n\n");
         assert(n > 0 && n < 80);
         length += (size_t)n;
      }
      for (i = 0; i < 80; i++)
      {
         spans[i].start_us = 80 - i;
         spans[i].end_us = 81 - i;
         spans[i].length = 1;
         metadata[i].identifier_offset = 2;
         metadata[i].identifier_length = 2;
         metadata[i].settings_offset = 5;
         metadata[i].settings_length = 11;
      }
      calls = 0;
      fail_at = failure;
      track = rsubtitle_vtt_begin_cues(text, sizeof(payload), spans, 80,
            metadata, header, length);
      if (track)
         do status = rsubtitle_srt_iterate(track, 1); while (!status);
      observed = calls;
      if (failure > observed)
         assert(track && status == 1 && rsubtitle_srt_count(track) == 80
               && rsubtitle_vtt_block_count(track) == 600
               && rsubtitle_vtt_region_count(track) == 300);
      else
         assert(!track || status == -1);
      rsubtitle_srt_free(track);
      assert(!live);
      if (failure > observed) break;
   }
   assert(failure > 8 && failure < 64);
}

static void test_css_failures(void)
{
   unsigned failure;
   for (failure = 1; failure <= 3; failure++)
   {
      rsubtitle_vtt_css_t *css;
      rsubtitle_vtt_css_token_t token;
      int status = -1;
      unsigned observed;
      calls = 0; fail_at = failure;
      css = rsubtitle_vtt_css_new();
      if (css && rsubtitle_vtt_css_begin(css, "::cue { color: red; }", 20))
      {
         do status = rsubtitle_vtt_css_step(css, &token); while (status == 0 || status == 1);
      }
      observed = calls;
      assert(failure > observed ? status == 2 : status == -1);
      rsubtitle_vtt_css_free(css);
      assert(!live);
   }
   fail_at = 0;
   {
      rsubtitle_vtt_css_t *css = rsubtitle_vtt_css_new();
      rsubtitle_vtt_css_token_t token;
      int status;
      assert(css && rsubtitle_vtt_css_begin(css, "x", 1));
      do status = rsubtitle_vtt_css_step(css, &token); while (status == 0 || status == 1);
      assert(status == 2);
      fail_at = calls + 1;
      assert(!rsubtitle_vtt_css_begin(css, "longer", 6));
      fail_at = 0;
      assert(rsubtitle_vtt_css_begin(css, "longer", 6));
      rsubtitle_vtt_css_free(css);
      assert(!live);
   }
   for (failure = 0; failure < 160; failure++)
   {
      static const char input[] = "::cue(.r\\65 d) { font: 1.2e2px 'a\\20 b'; color: url(a\\)b); }";
      rsubtitle_vtt_css_t *css;
      rsubtitle_vtt_css_token_t token;
      unsigned i, observed;
      fail_at = 0;
      css = rsubtitle_vtt_css_new();
      assert(css && rsubtitle_vtt_css_begin(css, input, sizeof(input) - 1));
      observed = calls;
      for (i = 0; i < failure; i++)
      {
         int status = rsubtitle_vtt_css_step(css, &token);
         assert(status >= 0);
         if (status == 2) break;
      }
      assert(calls == observed);
      rsubtitle_vtt_css_free(css);
      assert(!live);
   }
}

static void test_css_tree_failures(void)
{
   char *input;
   size_t length = 0;
   unsigned i, failure;
   fail_at = 0;
   input = (char*)fault_malloc(32768);
   assert(input);
   input[length++] = 'f'; input[length++] = '(';
   for (i = 0; i < 600; i++) length += sprintf(input + length, "name%u ", i);
   input[length++] = '"'; memset(input + length, 'x', 5000); length += 5000;
   input[length++] = '"'; input[length++] = ')';
   for (failure = 1; failure < 64; failure++)
   {
      rsubtitle_vtt_css_tree_t *tree;
      size_t root, ticks = 0;
      unsigned observed;
      int status = -1;
      calls = 0; fail_at = failure;
      tree = rsubtitle_vtt_css_tree_new();
      if (tree && rsubtitle_vtt_css_tree_begin(tree, input, length, &root))
         do status = rsubtitle_vtt_css_tree_step(tree); while (!status && ticks++ < length * 5 + 100);
      observed = calls;
      assert(failure > observed ? status == 1 : status == -1);
      rsubtitle_vtt_css_tree_free(tree);
      assert(live == 1);
      if (failure > observed) break;
   }
   assert(failure > 8 && failure < 64);
   for (i = 0; i < 24; i++)
   {
      rsubtitle_vtt_css_tree_t *tree;
      size_t root, ticks, limit = i * (length * 5 + 100) / 23;
      int status = 0;
      fail_at = 0;
      tree = rsubtitle_vtt_css_tree_new();
      assert(tree && rsubtitle_vtt_css_tree_begin(tree, input, length, &root));
      for (ticks = 0; !status && ticks < limit; ticks++) status = rsubtitle_vtt_css_tree_step(tree);
      assert(status >= 0);
      if (!status) assert(!rsubtitle_vtt_css_tree_count(tree));
      rsubtitle_vtt_css_tree_free(tree);
      assert(live == 1);
   }
   fault_free(input);
   assert(!live);
}

static void test_selector_failures(void)
{
   static const char input[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab";
   unsigned cut;
   rsubtitle_vtt_selector_pattern_t *pattern;
   rsubtitle_vtt_selector_match_t match;
   calls = 0; fail_at = 1;
   assert(!rsubtitle_vtt_selector_pattern_new() && !live);
   fail_at = 0; pattern = rsubtitle_vtt_selector_pattern_new(); assert(pattern);
   fail_at = calls + 1;
   assert(!rsubtitle_vtt_selector_pattern_begin(pattern, input, sizeof(input) - 1,
            RSUBTITLE_SELECTOR_SUBSTRING, 0));
   assert(rsubtitle_vtt_selector_pattern_step(pattern) == -1);
   assert(!rsubtitle_vtt_selector_match_begin(&match, pattern, input, sizeof(input) - 1));
   rsubtitle_vtt_selector_pattern_free(pattern); assert(!live);
   for (cut = 0; cut < 160; cut++)
   {
      unsigned tick, before;
      int status = 0;
      fail_at = 0; pattern = rsubtitle_vtt_selector_pattern_new(); assert(pattern);
      assert(rsubtitle_vtt_selector_pattern_begin(pattern, input, sizeof(input) - 1,
               RSUBTITLE_SELECTOR_SUBSTRING, 0));
      before = calls; fail_at = calls + 1;
      for (tick = 0; tick < cut && !status; tick++) status = rsubtitle_vtt_selector_pattern_step(pattern);
      if (status == 1)
      {
         assert(rsubtitle_vtt_selector_match_begin(&match, pattern, input, sizeof(input) - 1));
         status = 0;
         for (tick = 0; tick < cut && !status; tick++) status = rsubtitle_vtt_selector_match_step(&match);
         assert(status >= 0);
      }
      assert(calls == before);
      rsubtitle_vtt_selector_pattern_free(pattern); assert(!live);
   }
   fail_at = 0;
}

static void test_language_key_failures(void)
{
   static const char input[] = "WEBVTT\n\n00:00.000 --> 00:01.000\n"
      "<lang iw-BU><b>a</b><lang cmn>b</lang><lang>c</lang>"
      "<lang not well formed>d</lang></lang><lang iw-BU>e</lang>";
   unsigned failure;
   size_t full_steps = 0, cut;
   for (failure = 1; failure < 128; failure++)
   {
      char *text;
      rsubtitle_srt_t *track;
      size_t steps = 0;
      unsigned observed;
      int status = -1;
      fail_at = 0;
      text = (char*)fault_malloc(sizeof(input) - 1);
      assert(text);
      memcpy(text, input, sizeof(input) - 1);
      calls = 0;
      fail_at = failure;
      track = rsubtitle_vtt_begin(text, sizeof(input) - 1);
      if (track)
         do
         {
            status = rsubtitle_srt_iterate(track, 1);
            assert(++steps < 10000);
         } while (!status);
      observed = calls;
      assert(failure > observed ? status == 1 : status == -1);
      rsubtitle_srt_free(track);
      assert(!live);
      if (failure > observed)
      {
         full_steps = steps;
         break;
      }
   }
   assert(failure > 8 && failure < 128 && full_steps);
   for (cut = 0; cut <= full_steps; cut++)
   {
      char *text;
      rsubtitle_srt_t *track;
      size_t step, length = 99;
      int status = 0;
      fail_at = 0;
      text = (char*)fault_malloc(sizeof(input) - 1);
      assert(text);
      memcpy(text, input, sizeof(input) - 1);
      track = rsubtitle_vtt_begin(text, sizeof(input) - 1);
      assert(track);
      for (step = 0; !status && step < cut; step++)
         status = rsubtitle_srt_iterate(track, 1);
      assert(status >= 0);
      if (!status)
         assert(!rsubtitle_vtt_node_language_key(track, 0, 1, NULL, 0, &length) && !length);
      rsubtitle_srt_free(track);
      assert(!live);
   }
}

static void test_track_language_failures(void)
{
   static const char input[] = "WEBVTT\n\n00:00.000 --> 00:01.000\nx\n";
   unsigned failure, steps = 0, cut;
   for (failure = 1; failure < 128; failure++)
   {
      char *data, *language;
      rsubtitle_srt_t *track;
      unsigned observed;
      int status = 0, adopted;
      fail_at = 0;
      data = (char*)fault_malloc(sizeof(input) - 1);
      language = (char*)fault_malloc(2);
      assert(data && language);
      memcpy(data, input, sizeof(input) - 1);
      memcpy(language, "iw", 2);
      track = rsubtitle_vtt_begin(data, sizeof(input) - 1);
      assert(track);
      calls = 0;
      fail_at = failure;
      adopted = rsubtitle_vtt_take_language(track, language, 2);
      if (adopted)
         while (!status && steps++ < 10000)
            status = rsubtitle_srt_iterate(track, 1);
      observed = calls;
      assert(failure > observed ? adopted && status == 1 : !adopted || status == -1);
      rsubtitle_srt_free(track);
      assert(!live);
      if (failure > observed) break;
      steps = 0;
   }
   assert(failure < 128 && steps);
   for (cut = 0; cut <= steps; cut++)
   {
      char *data, *language;
      rsubtitle_srt_t *track;
      unsigned i;
      int status = 0;
      fail_at = 0;
      data = (char*)fault_malloc(sizeof(input) - 1);
      language = (char*)fault_malloc(2);
      assert(data && language);
      memcpy(data, input, sizeof(input) - 1);
      memcpy(language, "iw", 2);
      track = rsubtitle_vtt_begin(data, sizeof(input) - 1);
      assert(track && rsubtitle_vtt_take_language(track, language, 2));
      for (i = 0; !status && i < cut; i++)
         status = rsubtitle_srt_iterate(track, 1);
      assert(status >= 0);
      rsubtitle_srt_free(track);
      assert(!live);
   }
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
   test_vtt_failures();
   test_vtt_embedded_failures();
   test_css_failures();
   test_css_tree_failures();
   test_selector_failures();
   test_language_key_failures();
   test_track_language_failures();
   puts("PASS: subtitle allocation failures release adopted input, cues and metadata");
   return 0;
}
