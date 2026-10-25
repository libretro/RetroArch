/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_selector_test.c).
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
#include "../../../formats/subtitle/rsubtitle_vtt_internal.h"

static unsigned fold(unsigned c, unsigned insensitive)
{
   return insensitive && c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
}

static int space(unsigned c)
{
   return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int equal(const char *a, const char *b, size_t size, unsigned insensitive)
{
   size_t i;
   for (i = 0; i < size; i++)
      if (fold((unsigned char)a[i], insensitive) != fold((unsigned char)b[i], insensitive)) return 0;
   return 1;
}

static int reference(const char *value, size_t size, const char *needle, size_t length,
      enum rsubtitle_vtt_selector_operator op, unsigned insensitive)
{
   size_t i;
   if (op == RSUBTITLE_SELECTOR_PRESENT) return 1;
   if (length > size) return 0;
   if (op == RSUBTITLE_SELECTOR_EXACT)
      return size == length && equal(value, needle, length, insensitive);
   if (op == RSUBTITLE_SELECTOR_DASH)
      return equal(value, needle, length, insensitive) && (size == length || value[length] == '-');
   if (!length) return 0;
   if (op == RSUBTITLE_SELECTOR_PREFIX) return equal(value, needle, length, insensitive);
   if (op == RSUBTITLE_SELECTOR_SUFFIX) return equal(value + size - length, needle, length, insensitive);
   if (op == RSUBTITLE_SELECTOR_WORD)
      for (i = 0; i < length; i++) if (space((unsigned char)needle[i])) return 0;
   for (i = 0; i <= size - length; i++)
      if (equal(value + i, needle, length, insensitive)
            && (op != RSUBTITLE_SELECTOR_WORD || ((!i || space((unsigned char)value[i - 1]))
               && (i + length == size || space((unsigned char)value[i + length]))))) return 1;
   return 0;
}

static void prepare(rsubtitle_vtt_selector_pattern_t *pattern, const char *value,
      size_t length, enum rsubtitle_vtt_selector_operator op, unsigned insensitive)
{
   size_t ticks = 0;
   int status = 0;
   assert(rsubtitle_vtt_selector_pattern_begin(pattern, value, length, op, insensitive));
   while (!status && ticks++ <= length * 2 + 2) status = rsubtitle_vtt_selector_pattern_step(pattern);
   assert(status == 1);
}

static int match(rsubtitle_vtt_selector_pattern_t *pattern, const char *value, size_t size)
{
   rsubtitle_vtt_selector_match_t state;
   size_t ticks = 0;
   int status = 0;
   assert(rsubtitle_vtt_selector_match_begin(&state, pattern, value, size));
   while (!status && ticks++ <= size * 2 + 2) status = rsubtitle_vtt_selector_match_step(&state);
   assert(status == 1 || status == 2);
   assert(rsubtitle_vtt_selector_match_step(&state) == status);
   return status == 1;
}

static unsigned random_value = 1443;
static unsigned random_byte(void)
{
   random_value = random_value * 1664525u + 1013904223u;
   return random_value >> 16;
}

static void strings(void)
{
   static const char alphabet[] = "aAbBcC- \t\r\n\f\303\251";
   char value[97], needle[25];
   rsubtitle_vtt_selector_pattern_t *pattern = rsubtitle_vtt_selector_pattern_new();
   size_t size, length, i;
   unsigned trial, insensitive, op;
   assert(pattern && rsubtitle_vtt_selector_pattern_step(pattern) == -1);
   for (trial = 0; trial < 2000; trial++)
   {
      size = random_byte() % sizeof(value); length = random_byte() % sizeof(needle);
      for (i = 0; i < size; i++) value[i] = alphabet[random_byte() % (sizeof(alphabet) - 1)];
      for (i = 0; i < length; i++) needle[i] = alphabet[random_byte() % (sizeof(alphabet) - 1)];
      if (trial % 3 == 0 && length <= size) memcpy(needle, value + size - length, length);
      for (insensitive = 0; insensitive < 2; insensitive++)
         for (op = RSUBTITLE_SELECTOR_PRESENT; op <= RSUBTITLE_SELECTOR_SUBSTRING; op++)
         {
            prepare(pattern, needle, length, (enum rsubtitle_vtt_selector_operator)op, insensitive);
            assert(match(pattern, value, size) == reference(value, size, needle, length,
                     (enum rsubtitle_vtt_selector_operator)op, insensitive));
         }
   }
   assert(!rsubtitle_vtt_selector_pattern_begin(pattern, NULL, 1, RSUBTITLE_SELECTOR_EXACT, 0));
   assert(!rsubtitle_vtt_selector_pattern_begin(pattern, "", 65537, RSUBTITLE_SELECTOR_EXACT, 0));
   assert(!rsubtitle_vtt_selector_pattern_begin(pattern, "", 0, RSUBTITLE_SELECTOR_EXACT, 2));
   assert(rsubtitle_vtt_selector_pattern_step(NULL) == -1);
   assert(rsubtitle_vtt_selector_match_step(NULL) == -1);
   rsubtitle_vtt_selector_pattern_free(pattern); rsubtitle_vtt_selector_pattern_free(NULL);
}

static void linear_limits(void)
{
   char *value = (char*)malloc(65536), *needle = (char*)malloc(32768);
   rsubtitle_vtt_selector_pattern_t *pattern = rsubtitle_vtt_selector_pattern_new();
   assert(value && needle && pattern);
   memset(value, 'a', 65536); memset(needle, 'a', 32768); needle[32767] = 'b';
   prepare(pattern, needle, 32768, RSUBTITLE_SELECTOR_SUBSTRING, 0);
   assert(!match(pattern, value, 65536));
   value[65535] = 'b'; assert(match(pattern, value, 65536));
   prepare(pattern, value, 65536, RSUBTITLE_SELECTOR_SUBSTRING, 0);
   assert(match(pattern, value, 65536));
   rsubtitle_vtt_selector_pattern_free(pattern); free(needle); free(value);
}

static int subject_match(rsubtitle_vtt_selector_pattern_t *pattern, rsubtitle_srt_t *track,
      size_t node, enum rsubtitle_vtt_selector_kind kind, int64_t time)
{
   rsubtitle_vtt_selector_subject_t subject;
   unsigned ticks = 0;
   int status = 0;
   assert(rsubtitle_vtt_selector_subject_begin(&subject, pattern, track, 0, node, kind, time));
   while (!status && ticks++ < 4096) status = rsubtitle_vtt_selector_subject_step(&subject);
   assert(status == 1 || status == 2);
   return status == 1;
}

static void subjects(void)
{
   const char *input = "WEBVTT\n\ncue.1\n00:00.000 --> 00:10.000\n"
      "<00:01.000><v.loud.blue Alice &amp; Bob><b.loud>B</b></v>"
      "<lang en-US><i>I</i></lang><00:09.000>"
      "<c><u>U</u></c><ruby>R<rt>T</rt></ruby>";
   char *owned = (char*)malloc(strlen(input));
   rsubtitle_srt_t *track;
   rsubtitle_vtt_selector_pattern_t *pattern = rsubtitle_vtt_selector_pattern_new();
   size_t i, voice = 0, bold = 0, lang = 0, italic = 0;
   unsigned ticks = 0;
   int status = 0;
   assert(owned && pattern); memcpy(owned, input, strlen(input));
   track = rsubtitle_vtt_begin(owned, strlen(input)); assert(track);
   while (!status && ticks++ < 4096) status = rsubtitle_srt_iterate(track, 1);
   assert(status == 1);
   for (i = 0; i < rsubtitle_vtt_node_count(track, 0); i++)
   {
      const rsubtitle_vtt_node_t *node = rsubtitle_vtt_node(track, 0, i);
      int internal = node->kind != RSUBTITLE_VTT_TEXT && node->kind != RSUBTITLE_VTT_TIMESTAMP;
      assert(subject_match(NULL, track, i, RSUBTITLE_SELECTOR_UNIVERSAL, 0) == internal);
      assert(subject_match(NULL, track, i, RSUBTITLE_SELECTOR_ROOT, 0) == (i == 0));
      assert(subject_match(NULL, track, i, RSUBTITLE_SELECTOR_PAST, 5000000)
            == !!(rsubtitle_vtt_node_state(track, 0, i, 5000000) & RSUBTITLE_VTT_NODE_PAST));
      assert(subject_match(NULL, track, i, RSUBTITLE_SELECTOR_FUTURE, 5000000)
            == !!(rsubtitle_vtt_node_state(track, 0, i, 5000000) & RSUBTITLE_VTT_NODE_FUTURE));
      if (internal)
      {
         static const char *names[] = { "", "", "c", "b", "i", "u", "ruby", "rt", "v", "lang", "" };
         prepare(pattern, names[node->kind], strlen(names[node->kind]), RSUBTITLE_SELECTOR_EXACT, 0);
         assert(subject_match(pattern, track, i, RSUBTITLE_SELECTOR_TYPE, 0) == (i != 0));
      }
      if (node->kind == RSUBTITLE_VTT_VOICE) voice = i;
      if (node->kind == RSUBTITLE_VTT_BOLD) bold = i;
      if (node->kind == RSUBTITLE_VTT_LANGUAGE) lang = i;
      if (node->kind == RSUBTITLE_VTT_ITALIC) italic = i;
   }
   assert(voice && bold && lang && italic);
   prepare(pattern, "cue.1", 5, RSUBTITLE_SELECTOR_EXACT, 0);
   assert(subject_match(pattern, track, 0, RSUBTITLE_SELECTOR_ID, 0));
   assert(!subject_match(pattern, track, voice, RSUBTITLE_SELECTOR_ID, 0));
   prepare(pattern, "b", 1, RSUBTITLE_SELECTOR_EXACT, 0);
   assert(subject_match(pattern, track, bold, RSUBTITLE_SELECTOR_TYPE, 0));
   assert(!subject_match(pattern, track, 0, RSUBTITLE_SELECTOR_TYPE, 0));
   prepare(pattern, "blue", 4, RSUBTITLE_SELECTOR_EXACT, 0);
   assert(subject_match(pattern, track, voice, RSUBTITLE_SELECTOR_CLASS, 0));
   assert(!subject_match(pattern, track, bold, RSUBTITLE_SELECTOR_CLASS, 0));
   prepare(pattern, "loud", 4, RSUBTITLE_SELECTOR_EXACT, 0);
   assert(subject_match(pattern, track, voice, RSUBTITLE_SELECTOR_CLASS, 0));
   assert(subject_match(pattern, track, bold, RSUBTITLE_SELECTOR_CLASS, 0));
   prepare(pattern, "alice & bob", 11, RSUBTITLE_SELECTOR_EXACT, 1);
   assert(subject_match(pattern, track, voice, RSUBTITLE_SELECTOR_VOICE, 0));
   assert(!subject_match(pattern, track, lang, RSUBTITLE_SELECTOR_VOICE, 0));
   prepare(pattern, "en", 2, RSUBTITLE_SELECTOR_DASH, 0);
   assert(subject_match(pattern, track, lang, RSUBTITLE_SELECTOR_LANGUAGE, 0));
   assert(!subject_match(pattern, track, italic, RSUBTITLE_SELECTOR_LANGUAGE, 0));
   rsubtitle_vtt_selector_pattern_free(pattern); rsubtitle_srt_free(track);
}

static int language(const char *range, const char *tag)
{
   rsubtitle_vtt_language_match_t state;
   size_t ticks = 0;
   size_t bound = 4 * (strlen(range) + strlen(tag)) + 8;
   int result = 0;
   assert(rsubtitle_vtt_language_begin(&state, range, strlen(range), tag, strlen(tag)));
   while (!result && ticks++ < bound)
      result = rsubtitle_vtt_language_step(&state);
   assert(result && ticks <= bound);
   assert(rsubtitle_vtt_language_step(&state) == result);
   return result;
}

static void languages(void)
{
   static const char *yes[] = {
      "de-DE", "de-de", "de-Latn-DE", "de-Latf-DE", "de-DE-x-goethe",
      "de-Latn-DE-1996", "de-Deva-DE"
   };
   static const char *no[] = { "de", "de-x-DE", "de-Deva" };
   static const char *invalid[] = {
      "-en", "en-", "en--US", "en-**", "e*", "1n", "en_US",
      "abcdefghi", "en-abcdefghi", "en- a", "en-\200", "en-*x"
   };
   rsubtitle_vtt_language_match_t state;
   char *large;
   size_t i;
   for (i = 0; i < sizeof(yes) / sizeof(*yes); i++)
   {
      assert(language("de-*-DE", yes[i]) == 1);
      assert(language("de-DE", yes[i]) == 1);
   }
   for (i = 0; i < sizeof(no) / sizeof(*no); i++)
      assert(language("de-*-DE", no[i]) == 2);
   for (i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
      assert(language(invalid[i], "en-US") == -1);
   assert(language("", "") == 1);
   assert(language("", "en") == 2);
   assert(language("*", "") == 2);
   assert(language("*-*-*", "en") == 1);
   assert(language("*-US", "en-Latn-US") == 1);
   assert(language("*-US", "en-x-US") == 2);
   assert(language("en-x-US", "en-x-US") == 1);
   assert(language("en-a-foo", "en-a-foo") == 1);
   assert(language("en-foo", "en-a-foo") == 2);
   assert(language("en-419", "EN-Latn-419") == 1);
   assert(language("zh-Hant", "zh-Hans") == 2);
   assert(language("en", "english") == 2);
   assert(language("*", "en-") == -1);
   assert(!rsubtitle_vtt_language_begin(NULL, "", 0, "", 0));
   assert(!rsubtitle_vtt_language_begin(&state, NULL, 1, "", 0));
   assert(!rsubtitle_vtt_language_begin(&state, "", 0, NULL, 1));
   assert(!rsubtitle_vtt_language_begin(&state, "en", 65537, "", 0));
   assert(rsubtitle_vtt_language_step(NULL) == -1);
   large = (char*)malloc(65537);
   assert(large);
   large[0] = 'e';
   large[1] = 'n';
   for (i = 2; i < 65536; i++)
      large[i] = i & 1 ? '*' : '-';
   large[65536] = 0;
   assert(language(large, "en") == 1);
   assert(rsubtitle_vtt_language_begin(&state, large, 65536, "en", 2));
   for (i = 0; i < 256; i++)
      assert(!rsubtitle_vtt_language_step(&state));
   assert(rsubtitle_vtt_language_begin(&state, "fr", 2, "en", 2));
   while (!state.result)
      rsubtitle_vtt_language_step(&state);
   assert(state.result == 2);
   large[65535] = 'x';
   assert(language(large, "en") == 2);
   large[65535] = '-';
   assert(language(large, "en") == -1);
   free(large);
}

int main(void)
{
   strings(); linear_limits(); subjects(); languages();
   puts("PASS: bounded selector values, language filtering, cue types, IDs, classes, attributes and temporal predicates");
   return 0;
}
