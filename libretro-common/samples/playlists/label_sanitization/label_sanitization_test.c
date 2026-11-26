/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (label_sanitization_test.c).
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

/* The playlist label sanitizers rewrite a label in place and return
 * its new length.  Each case runs on a heap copy of exactly the
 * input's size, so a write past the label is a sanitizer report, and
 * the returned length is held to strlen() of the result: the menu
 * appends the inline core name at that length. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <playlists/label_sanitization.h>

typedef size_t (*sanitize_fn)(char *label);

static unsigned failures = 0;

static void check(const char *name, sanitize_fn fn,
      const char *in, const char *want)
{
   size_t in_len = strlen(in);
   size_t got_len;
   char *label   = (char*)malloc(in_len + 1);

   if (!label)
   {
      failures++;
      return;
   }
   memcpy(label, in, in_len + 1);
   got_len = fn(label);

   if (strcmp(label, want))
   {
      fprintf(stderr, "FAIL %s(\"%s\"): \"%s\", not \"%s\"\n",
            name, in, label, want);
      failures++;
   }
   if (got_len != strlen(label))
   {
      fprintf(stderr, "FAIL %s(\"%s\"): returned %u for a %u-byte "
            "result\n", name, in, (unsigned)got_len,
            (unsigned)strlen(label));
      failures++;
   }
   free(label);
}

#define CHECK(fn, in, want) check(#fn, fn, in, want)

int main(void)
{
   size_t i, long_len = 5000;
   char *long_in, *long_want;

   CHECK(label_remove_parens, "Title (USA) (Rev 1)", "Title");
   CHECK(label_remove_parens, "Title [!] (USA)", "Title [!]");
   CHECK(label_remove_brackets, "Title [!] (USA)", "Title (USA)");
   CHECK(label_remove_parens_and_brackets, "Title (USA) [!]", "Title");
   CHECK(label_keep_region, "Title (USA) (Rev 1) [!]", "Title (USA)");
   CHECK(label_keep_disc, "Title (Europe) (Disc 2)", "Title (Disc 2)");
   CHECK(label_keep_disc, "Title (disk 1) (Japan)", "Title (disk 1)");
   CHECK(label_keep_region_and_disc,
         "Title (Japan) (Disc 1) (Rev A)", "Title (Japan) (Disc 1)");

   /* whitespace: leading dropped, runs collapsed, trailing trimmed */
   CHECK(label_remove_parens, "  A   B  (x) ", "A B");
   CHECK(label_remove_parens, "A (x) B", "A B");

   /* nothing left, nothing given, an unclosed block */
   CHECK(label_remove_parens, "(USA)", "");
   CHECK(label_remove_parens, "", "");
   CHECK(label_remove_parens, "Title (USA", "Title");
   CHECK(label_remove_parens, "Title", "Title");

   /* a label of any length is read to its end */
   long_in   = (char*)malloc(long_len + 7);
   long_want = (char*)malloc(long_len + 1);
   if (long_in && long_want)
   {
      for (i = 0; i < long_len; i++)
         long_in[i] = long_want[i] = (char)('a' + (i % 26));
      memcpy(long_in + long_len, " (USA)", 7);
      long_want[long_len] = '\0';
      CHECK(label_remove_parens, long_in, long_want);
   }
   else
      failures++;
   free(long_in);
   free(long_want);

   if (failures)
   {
      fprintf(stderr, "FAIL label_sanitization_test: %u failures\n",
            failures);
      return 1;
   }
   printf("PASS label_sanitization_test\n");
   return 0;
}
