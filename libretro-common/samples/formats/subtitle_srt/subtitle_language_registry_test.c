/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (subtitle_language_registry_test.c).
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
#include <string.h>
#include "../../../formats/subtitle/rsubtitle_vtt_internal.h"
#include "../../../formats/subtitle/rsubtitle_languages.h"

#define ALIAS_COUNT (sizeof(vtt_language_aliases) / sizeof(vtt_language_aliases[0]))

typedef char alias_is_compact[sizeof(struct vtt_language_alias) <= 8 ? 1 : -1];

static const char *text(unsigned offset)
{
   assert(offset < sizeof(vtt_language_strings));
   assert(memchr(vtt_language_strings + offset, 0,
            sizeof(vtt_language_strings) - offset));
   return vtt_language_strings + offset;
}

static const struct vtt_language_alias *lookup(unsigned kind, const char *key)
{
   size_t i;
   for (i = 0; i < ALIAS_COUNT; i++)
      if (vtt_language_aliases[i].kind == kind
            && !strcmp(text(vtt_language_aliases[i].key), key))
         return &vtt_language_aliases[i];
   return NULL;
}

static void incremental(unsigned kind, const char *key, size_t length,
      const struct vtt_language_alias *expected)
{
   rsubtitle_vtt_alias_lookup_t state;
   rsubtitle_vtt_alias_value_t value;
   size_t levels = 0, count = ALIAS_COUNT, ticks = 0, bound;
   int status = 0;
   while (count)
   {
      count /= 2;
      levels++;
   }
   bound = (levels + 1) * ((length < 64 ? length : 64) + 2) + 1;
   assert(rsubtitle_vtt_alias_begin(&state, (enum rsubtitle_vtt_alias_kind)kind,
            key, length));
   while (!status && ticks++ < bound)
      status = rsubtitle_vtt_alias_step(&state);
   assert(status == (expected ? 1 : 2) && ticks <= bound);
   assert(rsubtitle_vtt_alias_step(&state) == status);
   value.preferred = value.extlang_prefix = "stale";
   value.preferred_length = value.extlang_prefix_length = 5;
   assert(rsubtitle_vtt_alias_result(&state, &value) == !!expected);
   if (expected)
   {
      rsubtitle_vtt_alias_value_t again;
      assert(value.preferred_length == strlen(text(expected->preferred)));
      assert(value.extlang_prefix_length == strlen(text(expected->extlang_prefix)));
      assert(!memcmp(value.preferred, text(expected->preferred), value.preferred_length));
      assert(!memcmp(value.extlang_prefix, text(expected->extlang_prefix), value.extlang_prefix_length));
      assert(rsubtitle_vtt_alias_result(&state, &again));
      assert(again.preferred == value.preferred && again.extlang_prefix == value.extlang_prefix);
   }
   else
      assert(!value.preferred && !value.preferred_length
            && !value.extlang_prefix && !value.extlang_prefix_length);
}

static void mapping(unsigned kind, const char *key,
      const char *preferred, const char *prefix)
{
   const struct vtt_language_alias *row = lookup(kind, key);
   assert(row);
   assert(!strcmp(text(row->preferred), preferred));
   assert(!strcmp(text(row->extlang_prefix), prefix));
}

int main(void)
{
   static const unsigned expected[] = {112, 258, 0, 6, 1, 51};
   unsigned counts[6] = {0, 0, 0, 0, 0, 0};
   size_t i;
   assert(ALIAS_COUNT == 428);
   assert(vtt_language_strings[0] == 0 && sizeof(vtt_language_strings) < 65536);
   for (i = 0; i < ALIAS_COUNT; i++)
   {
      char upper[65];
      size_t j, length;
      const struct vtt_language_alias *row = &vtt_language_aliases[i];
      const char *key = text(row->key);
      const char *preferred = text(row->preferred);
      const char *prefix = text(row->extlang_prefix);
      assert(row->kind < 6 && *key);
      counts[row->kind]++;
      assert((row->kind == RSUBTITLE_ALIAS_EXTLANG) == !!*prefix);
      assert(*preferred || row->kind == RSUBTITLE_ALIAS_TAG);
      if (i)
      {
         const struct vtt_language_alias *previous = row - 1;
         assert(previous->kind < row->kind || (previous->kind == row->kind
                  && strcmp(text(previous->key), key) < 0));
      }
      assert(lookup(row->kind, key) == row);
      incremental(row->kind, key, strlen(key), row);
      length = strlen(key);
      assert(length < sizeof(upper));
      for (j = 0; j < length; j++)
         upper[j] = key[j] >= 'a' && key[j] <= 'z' ? key[j] - 'a' + 'A' : key[j];
      incremental(row->kind, upper, length, row);
   }
   assert(!memcmp(counts, expected, sizeof(counts)));
   mapping(RSUBTITLE_ALIAS_LANGUAGE, "iw", "he", "");
   mapping(RSUBTITLE_ALIAS_LANGUAGE, "bh", "bih", "");
   mapping(RSUBTITLE_ALIAS_REGION, "bu", "mm", "");
   mapping(RSUBTITLE_ALIAS_VARIANT, "heploc", "alalc97", "");
   mapping(RSUBTITLE_ALIAS_EXTLANG, "cmn", "cmn", "zh");
   mapping(RSUBTITLE_ALIAS_EXTLANG, "hak", "hak", "zh");
   mapping(RSUBTITLE_ALIAS_TAG, "i-klingon", "tlh", "");
   mapping(RSUBTITLE_ALIAS_TAG, "i-default", "", "");
   mapping(RSUBTITLE_ALIAS_TAG, "zh-min", "", "");
   assert(!lookup(RSUBTITLE_ALIAS_LANGUAGE, "en"));
   assert(!lookup(RSUBTITLE_ALIAS_LANGUAGE, ""));
   assert(!lookup(RSUBTITLE_ALIAS_TAG, "zzzzzzzz"));
   assert(!lookup(6, "iw"));
   incremental(RSUBTITLE_ALIAS_LANGUAGE, "en", 2, NULL);
   incremental(RSUBTITLE_ALIAS_LANGUAGE, NULL, 0, NULL);
   incremental(RSUBTITLE_ALIAS_LANGUAGE, "iw\0x", 4, NULL);
   incremental(RSUBTITLE_ALIAS_LANGUAGE, "i", 1, NULL);
   incremental(RSUBTITLE_ALIAS_TAG, "i-default-extra", 15, NULL);
   incremental(RSUBTITLE_ALIAS_TAG, "\300", 1, NULL);
   {
      rsubtitle_vtt_alias_lookup_t state;
      rsubtitle_vtt_alias_value_t value;
      char long_key[65];
      size_t ticks = 0;
      int status = 0;
      memset(long_key, 'x', sizeof(long_key));
      incremental(RSUBTITLE_ALIAS_TAG, long_key, sizeof(long_key), NULL);
      assert(rsubtitle_vtt_alias_begin(&state, RSUBTITLE_ALIAS_TAG, "i-default", 9));
      assert(!rsubtitle_vtt_alias_step(&state));
      assert(!rsubtitle_vtt_alias_result(&state, &value) && !value.preferred);
      assert(rsubtitle_vtt_alias_begin(&state, RSUBTITLE_ALIAS_LANGUAGE, "iw", 2));
      while (!status && ticks++ < 64)
         status = rsubtitle_vtt_alias_step(&state);
      assert(status == 1 && rsubtitle_vtt_alias_result(&state, &value));
      assert(!strcmp(value.preferred, "he"));
      assert(!rsubtitle_vtt_alias_begin(&state, (enum rsubtitle_vtt_alias_kind)6, "iw", 2));
      assert(rsubtitle_vtt_alias_step(&state) == -1);
      assert(!rsubtitle_vtt_alias_result(&state, &value) && !value.preferred);
      assert(!rsubtitle_vtt_alias_begin(&state, (enum rsubtitle_vtt_alias_kind)-1, "iw", 2));
      assert(!rsubtitle_vtt_alias_begin(&state, RSUBTITLE_ALIAS_LANGUAGE, NULL, 1));
      assert(!rsubtitle_vtt_alias_begin(NULL, RSUBTITLE_ALIAS_LANGUAGE, "iw", 2));
      assert(rsubtitle_vtt_alias_step(NULL) == -1);
      assert(!rsubtitle_vtt_alias_result(NULL, &value));
      assert(!rsubtitle_vtt_alias_result(&state, NULL));
   }
   puts("PASS: compact typed IANA aliases, extlang prefixes and unmapped grandfathered tags");
   return 0;
}
