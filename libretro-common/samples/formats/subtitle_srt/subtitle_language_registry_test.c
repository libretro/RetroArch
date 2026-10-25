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
   size_t low = 0, high = ALIAS_COUNT;
   while (low < high)
   {
      size_t mid = low + (high - low) / 2;
      const struct vtt_language_alias *row = &vtt_language_aliases[mid];
      int order = row->kind < kind ? -1 : row->kind > kind ? 1
         : strcmp(text(row->key), key);
      if (order < 0)
         low = mid + 1;
      else if (order > 0)
         high = mid;
      else
         return row;
   }
   return NULL;
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
      const struct vtt_language_alias *row = &vtt_language_aliases[i];
      const char *key = text(row->key);
      const char *preferred = text(row->preferred);
      const char *prefix = text(row->extlang_prefix);
      assert(row->kind < 6 && *key);
      counts[row->kind]++;
      assert((row->kind == VTT_LANGUAGE_EXTLANG) == !!*prefix);
      assert(*preferred || row->kind == VTT_LANGUAGE_TAG);
      if (i)
      {
         const struct vtt_language_alias *previous = row - 1;
         assert(previous->kind < row->kind || (previous->kind == row->kind
                  && strcmp(text(previous->key), key) < 0));
      }
      assert(lookup(row->kind, key) == row);
   }
   assert(!memcmp(counts, expected, sizeof(counts)));
   mapping(VTT_LANGUAGE_PRIMARY, "iw", "he", "");
   mapping(VTT_LANGUAGE_PRIMARY, "bh", "bih", "");
   mapping(VTT_LANGUAGE_REGION, "bu", "mm", "");
   mapping(VTT_LANGUAGE_VARIANT, "heploc", "alalc97", "");
   mapping(VTT_LANGUAGE_EXTLANG, "cmn", "cmn", "zh");
   mapping(VTT_LANGUAGE_EXTLANG, "hak", "hak", "zh");
   mapping(VTT_LANGUAGE_TAG, "i-klingon", "tlh", "");
   mapping(VTT_LANGUAGE_TAG, "i-default", "", "");
   mapping(VTT_LANGUAGE_TAG, "zh-min", "", "");
   assert(!lookup(VTT_LANGUAGE_PRIMARY, "en"));
   assert(!lookup(VTT_LANGUAGE_PRIMARY, ""));
   assert(!lookup(VTT_LANGUAGE_TAG, "zzzzzzzz"));
   assert(!lookup(6, "iw"));
   puts("PASS: compact typed IANA aliases, extlang prefixes and unmapped grandfathered tags");
   return 0;
}
