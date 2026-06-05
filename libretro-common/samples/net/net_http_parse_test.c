/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http_parse_test.c).
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

#include <stdio.h>
#include <string.h>
#include <net/net_http_parse.h>

static int failures = 0;

static void test_happy_path(void)
{
   char link[1024];
   char name[1024];
   const char *line = "<a href=\"http://www.test.com/somefile.zip\">Test</a>\n";
   int rc;

   link[0] = name[0] = '\0';
   rc = string_parse_html_anchor(line, link, name, sizeof(link), sizeof(name));

   if (rc != 0)
   {
      printf("[ERROR] happy path: rc=%d, want 0\n", rc);
      failures++;
      return;
   }
   if (strcmp(link, "http://www.test.com/somefile.zip") != 0)
   {
      printf("[ERROR] happy path: link='%s'\n", link);
      failures++;
      return;
   }
   if (strcmp(name, "Test") != 0)
   {
      printf("[ERROR] happy path: name='%s'\n", name);
      failures++;
      return;
   }
   printf("[SUCCESS] happy path: link='%s' name='%s'\n", link, name);
}

static void test_no_anchor(void)
{
   char link[64];
   char name[64];
   int rc;

   rc = string_parse_html_anchor("no anchor here", link, name,
         sizeof(link), sizeof(name));

   if (rc == 0)
   {
      printf("[ERROR] no-anchor: rc=0, want 1\n");
      failures++;
      return;
   }
   printf("[SUCCESS] no-anchor returned %d\n", rc);
}

static void test_undersized_link_buffer(void)
{
   /* The href is 41 characters.  Supply a 10-byte link buffer.
    * Pre-patch: memcpy copies 41 bytes into link[10] -- stack
    * smashing / canary hit / ASan heap-buffer-overflow.
    * Post-patch: len clamps to 9, link[9] = NUL, safe truncation. */
   char link[10];   /* deliberately too small */
   char name[64];
   char canary[16]; /* stack canary to detect over-write */
   const char *line = "<a href=\"http://www.test.com/somefile.zip\">Test</a>";
   int rc;

   /* Fill canary with a known pattern.  A pre-patch run smashes
    * link[] and onto the adjacent stack slot; the canary gets
    * corrupted.  Post-patch leaves it alone. */
   memset(canary, 0xAA, sizeof(canary));
   link[0] = name[0] = '\0';

   rc = string_parse_html_anchor(line, link, name,
         sizeof(link), sizeof(name));
   (void)rc;

   /* Check the canary is intact.  (Result of rc is don't-care:
    * post-patch should still return 0 with a truncated link.) */
   {
      size_t i;
      for (i = 0; i < sizeof(canary); ++i)
      {
         if ((unsigned char)canary[i] != 0xAA)
         {
            printf("[ERROR] undersized-link: canary corrupted at offset %zu\n", i);
            failures++;
            return;
         }
      }
   }

   /* The link buffer must be NUL-terminated somewhere in [0, 9]. */
   {
      int found_nul = 0;
      size_t i;
      for (i = 0; i < sizeof(link); ++i)
      {
         if (link[i] == '\0')
         {
            found_nul = 1;
            break;
         }
      }
      if (!found_nul)
      {
         printf("[ERROR] undersized-link: output not NUL-terminated\n");
         failures++;
         return;
      }
   }

   printf("[SUCCESS] undersized-link: truncated link='%s' (buffer safe)\n", link);
}

static void test_undersized_name_buffer(void)
{
   char link[256];
   char name[3];    /* deliberately too small for "Test" */
   char canary[16];
   const char *line = "<a href=\"http://x.com/\">Test</a>";
   int rc;

   memset(canary, 0xBB, sizeof(canary));
   link[0] = name[0] = '\0';

   rc = string_parse_html_anchor(line, link, name,
         sizeof(link), sizeof(name));
   (void)rc;

   {
      size_t i;
      for (i = 0; i < sizeof(canary); ++i)
      {
         if ((unsigned char)canary[i] != 0xBB)
         {
            printf("[ERROR] undersized-name: canary corrupted at offset %zu\n", i);
            failures++;
            return;
         }
      }
   }
   {
      int found_nul = 0;
      size_t i;
      for (i = 0; i < sizeof(name); ++i)
      {
         if (name[i] == '\0')
         {
            found_nul = 1;
            break;
         }
      }
      if (!found_nul)
      {
         printf("[ERROR] undersized-name: output not NUL-terminated\n");
         failures++;
         return;
      }
   }
   printf("[SUCCESS] undersized-name: truncated name='%s' (buffer safe)\n", name);
}

int main(void)
{
   test_happy_path();
   test_no_anchor();
   test_undersized_link_buffer();
   test_undersized_name_buffer();

   if (failures)
   {
      printf("\n%d net_http_parse test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll net_http_parse regression tests passed.\n");
   return 0;
}
