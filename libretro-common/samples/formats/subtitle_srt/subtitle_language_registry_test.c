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
#include <stdlib.h>
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

static void grammar(rsubtitle_vtt_tag_t *tag, const char *input,
      size_t length, int expected)
{
   size_t ticks = 0, bound = (length + 1) * 128 + 1024;
   int status = 0;
   assert(rsubtitle_vtt_tag_begin(tag, input, length));
   while (!status && ticks++ < bound)
      status = rsubtitle_vtt_tag_step(tag);
   if (ticks > bound || status != expected)
      fprintf(stderr, "tag %.*s: got %d, expected %d after %lu steps\n",
            (int)(length < 80 ? length : 80), input ? input : "",
            status, expected, (unsigned long)ticks);
   assert(ticks <= bound && status == expected);
   assert(rsubtitle_vtt_tag_step(tag) == status);
}

static void tag_grammar(void)
{
   static const char *valid[] = {
      "en", "de-CH-1901", "sl-rozaj-biske-1994", "en-Latn-US",
      "zh-cmn-Hans-CN", "zh-abc-def-ghi-Hans-419-1abc-abcde",
      "abcd-Latn-US", "abcdefgh-Latn-419", "qaa-Qaaa-QM",
      "en-b-ccc-bbb-a-aaa-X-xyz", "x-a-b-c", "x-i-default",
      "en-a-aaa-x-a-a", "en-abcde-abcdef-1234-a-aa-b-bb", "en-a-aa-Latn-US"
   };
   static const char *invalid[] = {
      "", "a", "i-unknown", "e1", "1en", "en_Us", "-en", "en-",
      "en--US", "en-abcdefghi", "en-Latn-Cyrl", "en-US-US",
      "zh-abc-def-ghi-jkl", "abcd-abc", "en-a", "x", "en-x",
      "en-a-x-foo", "en-a-b-foo", "en-a-aa-A-bb", "en-0-aa-0-bb",
      "sl-rozaj-ROZAJ", "de-1901-1901", "en-abcde-abcdef-AbCdE",
      "en-!", "en-\300", "en-abc-1a"
   };
   rsubtitle_vtt_tag_t *tag = rsubtitle_vtt_tag_new();
   char singletons[256];
   char *many = (char*)malloc(65537);
   size_t i, n = 2;
   unsigned c;
   assert(tag && many);
   assert(rsubtitle_vtt_tag_step(tag) == -1);
   for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
      grammar(tag, valid[i], strlen(valid[i]), 1);
   for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
      grammar(tag, invalid[i], strlen(invalid[i]), 2);
   grammar(tag, "en\0US", 5, 2);
   for (i = 0; i < ALIAS_COUNT; i++)
      if (vtt_language_aliases[i].kind == RSUBTITLE_ALIAS_TAG)
      {
         const char *key = text(vtt_language_aliases[i].key);
         grammar(tag, key, strlen(key), 1);
      }
   memcpy(singletons, "en", 2);
   for (c = '0'; c <= 'z'; c++)
      if ((c <= '9' || c >= 'a') && c != 'x')
      {
         singletons[n++] = '-';
         singletons[n++] = (char)c;
         memcpy(singletons + n, "-aa", 3);
         n += 3;
      }
   grammar(tag, singletons, n, 1);
   memcpy(singletons + n, "-a-bb", 5);
   grammar(tag, singletons, n + 5, 2);
   memcpy(many, "en", 2);
   n = 2;
   for (i = 0; i < 9000; i++)
   {
      unsigned value = (unsigned)i, j;
      many[n++] = '-';
      many[n++] = 'a';
      for (j = 0; j < 4; j++)
      {
         many[n++] = (char)('a' + value % 26);
         value /= 26;
      }
   }
   grammar(tag, many, n, 1);
   memcpy(many + n, "-aaaaa", 6);
   grammar(tag, many, n + 6, 2);
   for (i = 0; i < 128; i++)
   {
      size_t j;
      assert(rsubtitle_vtt_tag_begin(tag, "en-abcde-abcde", 14));
      for (j = 0; j < i; j++)
         if (rsubtitle_vtt_tag_step(tag))
            break;
      grammar(tag, "en-abcde-abcdef", 15, 1);
   }
   assert(!rsubtitle_vtt_tag_begin(tag, many, 65537));
   assert(rsubtitle_vtt_tag_step(tag) == -1);
   assert(!rsubtitle_vtt_tag_begin(tag, NULL, 1));
   grammar(tag, NULL, 0, 2);
   assert(!rsubtitle_vtt_tag_begin(NULL, "en", 2));
   assert(rsubtitle_vtt_tag_step(NULL) == -1);
   rsubtitle_vtt_tag_free(tag);
   rsubtitle_vtt_tag_free(NULL);
   free(many);
}

static void normalized(rsubtitle_vtt_normalize_t *normal, const char *input,
      unsigned extlang, const char *expected, unsigned borrowed)
{
   const char *result;
   size_t length, i, ticks = 0, bound = (strlen(input) + 1) * 256 + 4096;
   int status = 0;
   assert(rsubtitle_vtt_normalize_begin(normal, input, strlen(input), extlang));
   assert(!rsubtitle_vtt_normalize_result(normal, &length) && !length);
   while (!status && ticks++ < bound)
      status = rsubtitle_vtt_normalize_step(normal);
   assert(ticks <= bound && status == (expected ? 1 : 2));
   assert(rsubtitle_vtt_normalize_step(normal) == status);
   result = rsubtitle_vtt_normalize_result(normal, &length);
   if (!expected)
   {
      assert(!result && !length);
      return;
   }
   if (!result || length != strlen(expected))
      fprintf(stderr, "normal %s: got %.*s, expected %s\n", input,
            (int)length, result ? result : "", expected);
   assert(result && length == strlen(expected));
   for (i = 0; i < length; i++)
   {
      unsigned a = (unsigned char)result[i], b = (unsigned char)expected[i];
      if (a >= 'A' && a <= 'Z')
         a += 'a' - 'A';
      if (b >= 'A' && b <= 'Z')
         b += 'a' - 'A';
      if (a != b)
         fprintf(stderr, "normal %s: got %.*s, expected %s\n", input,
               (int)length, result, expected);
      assert(a == b);
   }
   assert(!borrowed || result == input);
}

static void normalized_match(const char *range, const char *tag, int expected)
{
   rsubtitle_vtt_normalize_t *r = rsubtitle_vtt_normalize_new();
   rsubtitle_vtt_normalize_t *t = rsubtitle_vtt_normalize_new();
   rsubtitle_vtt_language_match_t match;
   const char *rk, *tk = "";
   size_t rn, tn = 0, ticks;
   int status;
   assert(r && t);
   assert(rsubtitle_vtt_normalize_begin(r, range, strlen(range),
            RSUBTITLE_NORMALIZE_RANGE | RSUBTITLE_NORMALIZE_EXTLANG));
   status = 0;
   for (ticks = 0; !status && ticks < 10000; ticks++)
      status = rsubtitle_vtt_normalize_step(r);
   assert(status == 1);
   rk = rsubtitle_vtt_normalize_result(r, &rn);
   if (*tag)
   {
      assert(rsubtitle_vtt_normalize_begin(t, tag, strlen(tag), RSUBTITLE_NORMALIZE_EXTLANG));
      status = 0;
      for (ticks = 0; !status && ticks < 10000; ticks++)
         status = rsubtitle_vtt_normalize_step(t);
      assert(status == 1);
      tk = rsubtitle_vtt_normalize_result(t, &tn);
   }
   assert(rsubtitle_vtt_language_begin(&match, rk, rn, tk, tn));
   status = 0;
   for (ticks = 0; !status && ticks < 10000; ticks++)
      status = rsubtitle_vtt_language_step(&match);
   assert(status == expected);
   rsubtitle_vtt_normalize_free(r);
   rsubtitle_vtt_normalize_free(t);
}

static void tag_normalization(void)
{
   rsubtitle_vtt_normalize_t *normal = rsubtitle_vtt_normalize_new();
   rsubtitle_vtt_language_key_t key;
   size_t i, length;
   assert(normal);
   assert(rsubtitle_vtt_normalize_step(normal) == -1);
   normalized(normal, "en-Latn-US", 0, "en-Latn-US", 1);
   normalized(normal, "EN-latn-us", 1, "en-latn-us", 1);
   normalized(normal, "qaa-Qaaa-QM", 1, "qaa-Qaaa-QM", 1);
   normalized(normal, "iw-BU", 0, "he-MM", 0);
   normalized(normal, "bh", 0, "bih", 0);
   normalized(normal, "i-klingon", 1, "tlh", 0);
   normalized(normal, "i-default", 1, "i-default", 1);
   normalized(normal, "i-enochian", 0, "i-enochian", 1);
   normalized(normal, "cmn-Hans-CN", 0, "cmn-Hans-CN", 1);
   normalized(normal, "cmn-Hans-CN", 1, "zh-cmn-Hans-CN", 0);
   normalized(normal, "zh-cmn-Hans-CN", 0, "cmn-Hans-CN", 0);
   normalized(normal, "zh-cmn-Hans-CN", 1, "zh-cmn-Hans-CN", 1);
   normalized(normal, "sgn-US", 0, "ase", 0);
   normalized(normal, "sgn-US", 1, "sgn-ase", 0);
   normalized(normal, "ar-ajp", 0, "apc", 0);
   normalized(normal, "ar-ajp", 1, "ar-apc", 0);
   normalized(normal, "ja-Latn-hepburn-heploc", 0, "ja-Latn-hepburn-alalc97", 0);
   normalized(normal, "ja-Latn-hepburn-heploc-alalc97", 1, "ja-Latn-hepburn-alalc97-alalc97", 0);
   normalized(normal, "en-b-ccc-bbb-a-aaa-X-xyz", 1, "en-a-aaa-b-ccc-bbb-X-xyz", 0);
   normalized(normal, "en-a-aa-b-bb-x-iw-BU", 1, "en-a-aa-b-bb-x-iw-BU", 1);
   normalized(normal, "x-iw-BU-cmn", 1, "x-iw-BU-cmn", 1);
   normalized(normal, "en-a-iw-BU", 1, "en-a-iw-BU", 1);
   normalized(normal, "en-abcde-abcdef", 1, "en-abcde-abcdef", 1);
   normalized(normal, "en-abcde-ABCDE", 1, NULL, 0);
   normalized(normal, "en-a-aa-A-bb", 0, NULL, 0);
   normalized(normal, "en-", 0, NULL, 0);
   normalized(normal, "", 1, NULL, 0);
   normalized(normal, "", 3, "", 1);
   normalized(normal, "*", 3, "*", 1);
   normalized(normal, "*-*-Latn", 3, "*-Latn", 0);
   normalized(normal, "zh-*-cmn-*-Hans", 3, "zh-cmn-Hans", 0);
   normalized(normal, "i-*-klingon", 3, "tlh", 0);
   normalized(normal, "art-*-lojban", 3, "jbo", 0);
   normalized(normal, "*-cmn", 3, "*-cmn", 1);
   normalized(normal, "*-ajp", 3, "*-apc", 0);
   normalized(normal, "en-abcde-abcde", 3, "en-abcde-abcde", 1);
   normalized(normal, "en-b-bb-a-aa-b-cc", 3, "en-b-bb-a-aa-b-cc", 1);
   normalized(normal, "en-b-*-a", 3, "en-b-*-a", 1);
   normalized(normal, "en-b-bb-a", 3, "en-b-bb-a", 1);
   normalized(normal, "en-b-bb-a-*", 3, "en-b-bb-a-*", 1);
   normalized(normal, "en-b-*-bb-a-aa", 3, "en-a-aa-b-*-bb", 0);
   normalized(normal, "cmn-abc-def-ghi", 1, "zh-cmn-abc-def-ghi", 0);
   normalized(normal, "zh-cmn-abc-def-ghi", 3, "zh-cmn-abc-def-ghi", 1);
   normalized(normal, "zh-abc-cmn", 3, "zh-abc-cmn", 1);
   normalized(normal, "en-ajp", 1, "en-apc", 0);
   normalized(normal, "cmn-ajp-cmn-cmn-cmn", 3, "zh-cmn-apc-cmn-cmn-cmn", 0);
   normalized(normal, "en-**", 3, NULL, 0);
   normalized(normal, "zh-cmn-abc-ajp-ajp", 2, "cmn-abc-apc-apc", 0);
   normalized(normal, "zh-cmn-abc-ajp-ajp", 3, "zh-cmn-abc-apc-apc", 0);
   normalized_match("zh", "cmn-abc-def-ghi", 1);
   normalized_match("cmn-*-ghi", "cmn-abc-def-ghi", 1);
   normalized_match("*-ghi", "cmn-abc-def-ghi", 1);
   normalized_match("zh-*-cmn", "cmn-Hans", 1);
   normalized_match("*-ajp", "apc", 1);
   normalized_match("i-*-klingon", "tlh", 1);
   normalized_match("de-DE", "de-x-DE", 2);
   normalized_match("de-*-DE", "de-Deva-DE", 1);
   normalized_match("*", "", 2);
   normalized_match("", "", 1);
   normalized_match("", "und", 2);
   normalized_match("*", "und", 1);
   for (i = 0; i < ALIAS_COUNT; i++)
      if (vtt_language_aliases[i].kind == RSUBTITLE_ALIAS_LANGUAGE)
         normalized(normal, text(vtt_language_aliases[i].key), 0,
               text(vtt_language_aliases[i].preferred), 0);
   for (i = 0; i < 256; i++)
   {
      size_t j;
      assert(rsubtitle_vtt_normalize_begin(normal, "iw-b-ccc-a-aaa", 14, 1));
      for (j = 0; j < i; j++)
         if (rsubtitle_vtt_normalize_step(normal))
            break;
      normalized(normal, "en-abcde-abcdef", 1, "en-abcde-abcdef", 1);
   }
   assert(!rsubtitle_vtt_normalize_begin(normal, "en", 65537, 0));
   assert(rsubtitle_vtt_normalize_step(normal) == -1);
   assert(!rsubtitle_vtt_normalize_result(normal, &length) && !length);
   assert(!rsubtitle_vtt_normalize_begin(normal, NULL, 1, 0));
   assert(!rsubtitle_vtt_normalize_begin(normal, "en", 2, 4));
   assert(!rsubtitle_vtt_normalize_begin(NULL, "en", 2, 0));
   assert(rsubtitle_vtt_normalize_step(NULL) == -1);
   assert(!rsubtitle_vtt_normalize_result(NULL, &length) && !length);
   assert(!rsubtitle_vtt_normalize_take(normal, &key));
   assert(!key.text && !key.length && !key.owned);
   normalized(normal, "iw", 1, "he", 0);
   assert(!rsubtitle_vtt_normalize_take(normal, NULL));
   assert(rsubtitle_vtt_normalize_result(normal, &length) && length == 2);
   assert(rsubtitle_vtt_normalize_take(normal, &key));
   assert(key.owned == key.text && key.length == 2 && !memcmp(key.text, "he", 2));
   assert(!rsubtitle_vtt_normalize_result(normal, &length) && !length);
   assert(rsubtitle_vtt_normalize_step(normal) == -1);
   normalized(normal, "en", 1, "en", 1);
   assert(key.length == 2 && !memcmp(key.text, "he", 2));
   free(key.owned);
   assert(rsubtitle_vtt_normalize_take(normal, &key));
   assert(!key.owned && key.length == 2 && !memcmp(key.text, "en", 2));
   assert(!rsubtitle_vtt_normalize_take(normal, &key));
   assert(!key.text && !key.length && !key.owned);
   assert(!rsubtitle_vtt_normalize_take(NULL, &key));
   normalized(normal, "iw", 1, "he", 0);
   assert(rsubtitle_vtt_normalize_take(normal, &key));
   rsubtitle_vtt_normalize_free(normal);
   assert(key.owned && key.length == 2 && !memcmp(key.text, "he", 2));
   free(key.owned);
   rsubtitle_vtt_normalize_free(NULL);
}

int main(void)
{
   static const unsigned expected[] = {112, 258, 0, 6, 1, 51};
   unsigned counts[6] = {0, 0, 0, 0, 0, 0};
   size_t i;
   tag_grammar();
   tag_normalization();
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
