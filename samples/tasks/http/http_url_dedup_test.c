/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_url_dedup_test.c).
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

/* Regression test for the duplicate-download suppression key in
 * tasks/task_http.c and tasks/task_http_emscripten.c.
 *
 * Both files store the request URL in a fixed-size member:
 *
 *     char connection_url[NAME_MAX_LENGTH];
 *
 * populated with strlcpy(), and task_http_finder() compares that
 * stored copy against the raw candidate URL to decide whether a GET
 * is already in flight:
 *
 *     return string_is_equal(http->connection_url, (const char*)user_data);
 *
 * NAME_MAX_LENGTH is 256 on most targets and 128 on the small-path
 * platforms (see libretro-common/include/retro_miscellaneous.h).
 *
 * The asymmetry is the bug: the *stored* side is truncated by
 * strlcpy, the *candidate* side is not.  Once a URL reaches
 * NAME_MAX_LENGTH bytes the two can never compare equal, so
 * task_http_finder() returns false for every candidate and the
 * concurrency guard in task_push_http_transfer_generic() -- the one
 * whose comment reads "Concurrent download of the same file is not
 * allowed" -- silently stops guarding anything.
 *
 * Note what this is *not*: truncation cannot cause a false positive.
 * Two distinct long URLs sharing a prefix do not get conflated,
 * because the candidate is never truncated to match.  The failure is
 * one-directional, which is why it has gone unnoticed -- nothing
 * breaks loudly, downloads just quietly stop being deduplicated.
 *
 * The consequence is not merely wasted bandwidth.  Two tasks for the
 * same URL both reach their completion callback, and callbacks such
 * as cb_http_task_download_pl_thumbnail() in
 * tasks/task_pl_thumbnail_download.c end in
 *
 *     filestream_write_file(transf->path, data->data, data->len)
 *
 * against an identical path.  Two writers, one file, no ordering:
 * a torn or interleaved image on disk.  Playlist thumbnail URLs are
 * the natural trigger, since they concatenate a long CDN base path
 * with a percent-encoded game title and routinely exceed 256 bytes.
 *
 * Fixed by making connection_url a heap-owned copy of the full URL in
 * both backends.  This test carries both key shapes: `key_owned`
 * mirrors what task_http.c does now and carries the real assertions,
 * while `key_fixed` reproduces the old fixed-buffer shape so the
 * failure mode stays documented and a revert is caught.
 *
 * The finder predicate is static to task_http.c and not exposed in a
 * header, so this test keeps behavioural copies as the oracles -- the
 * same convention http_method_match_test.c and
 * archive_name_safety_test.c use.  If task_http.c changes how the key
 * is derived, update the copy here to match.
 *
 * Build standalone:
 *   cc -Wall -std=gnu99 -g -O0 -o http_url_dedup_test http_url_dedup_test.c
 *   ./http_url_dedup_test
 *
 * Also worth running under ASan/LSan, which covers the heap-owned
 * replacement key's allocation and teardown:
 *   make clean all SANITIZER=address && ./http_url_dedup_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mirrors retro_miscellaneous.h.  Override on the command line to
 * exercise the small-path platforms:  -DNAME_MAX_LENGTH=128 */
#ifndef NAME_MAX_LENGTH
#define NAME_MAX_LENGTH 256
#endif

static int failures;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* Behavioural copy of compat/strl.c::strlcpy -- truncating, always
 * NUL-terminating, returning the length of the source. */
static size_t t_strlcpy(char *dest, const char *source, size_t size)
{
   size_t      src_size = 0;
   const char *psrc     = source;

   if (size)
   {
      size_t n = size;
      while (--n && (*dest++ = *source++))
         ;
      *dest = '\0';
   }

   while (*psrc++)
      src_size++;

   return src_size;
}

/* --------------------------------------------------------------- */
/* Oracle A: the current key -- fixed buffer, silently truncating.   */
/* --------------------------------------------------------------- */

struct key_fixed
{
   char connection_url[NAME_MAX_LENGTH];
};

static void key_fixed_set(struct key_fixed *k, const char *url)
{
   k->connection_url[0] = '\0';
   t_strlcpy(k->connection_url, url, sizeof(k->connection_url));
}

static int key_fixed_matches(const struct key_fixed *k, const char *url)
{
   /* task_http_finder(): stored (truncated) vs candidate (raw). */
   return strcmp(k->connection_url, url) == 0;
}

/* --------------------------------------------------------------- */
/* Oracle B: the replacement -- heap-owned, full-length.            */
/*                                                                  */
/* A NULL key means "no key", which matches nothing.  An allocation  */
/* failure therefore degrades to "admit the download" rather than    */
/* to a silent drop, which is the safe direction: a redundant        */
/* download costs bandwidth, a dropped one strands the caller's      */
/* completion callback forever.                                     */
/* --------------------------------------------------------------- */

struct key_owned
{
   char *connection_url;
};

static int key_owned_set(struct key_owned *k, const char *url)
{
   size_t n           = strlen(url) + 1;
   k->connection_url  = (char*)malloc(n);
   if (!k->connection_url)
      return 0;
   memcpy(k->connection_url, url, n);
   return 1;
}

static void key_owned_free(struct key_owned *k)
{
   free(k->connection_url);
   k->connection_url = NULL;
}

static int key_owned_matches(const struct key_owned *k, const char *url)
{
   if (!k->connection_url || !url)
      return 0;
   return strcmp(k->connection_url, url) == 0;
}

/* --------------------------------------------------------------- */
/* Fixtures                                                         */
/* --------------------------------------------------------------- */

/* Build a URL shaped like the ones RetroArch actually requests for
 * playlist thumbnails: a long stable CDN prefix, `pad` bytes of
 * filler, then a distinguishing tail. */
static char *make_thumb_url(size_t pad, const char *tail)
{
   static const char base[] =
      "http://thumbnails.libretro.com/"
      "Sony%20-%20PlayStation/Named_Snaps/";
   size_t blen = sizeof(base) - 1;
   size_t tlen = strlen(tail);
   char  *url  = (char*)malloc(blen + pad + tlen + 1);
   if (!url)
      return NULL;
   memcpy(url, base, blen);
   memset(url + blen, 'A', pad);
   memcpy(url + blen + pad, tail, tlen + 1);
   return url;
}

static char *dup_str(const char *s)
{
   size_t n = strlen(s) + 1;
   char  *d = (char*)malloc(n);
   if (d)
      memcpy(d, s, n);
   return d;
}

/* --------------------------------------------------------------- */

/* The core defect: a long URL fails to match itself, so the
 * concurrency guard never fires and duplicate downloads are
 * admitted. */
static void test_long_url_fails_to_dedup_itself(void)
{
   char *url = make_thumb_url(NAME_MAX_LENGTH, "Castlevania%20-%20SOTN.png");
   char *same;
   struct key_fixed ka;
   struct key_owned kb;

   printf("test_long_url_fails_to_dedup_itself\n");
   if (!url)
   {
      printf("  SKIP: out of memory\n");
      return;
   }
   if (!(same = dup_str(url)))
   {
      printf("  SKIP: out of memory\n");
      free(url);
      return;
   }

   CHECK(strlen(url) >= NAME_MAX_LENGTH,
         "fixture must exceed the key buffer (len=%u, buf=%u)",
         (unsigned)strlen(url), (unsigned)NAME_MAX_LENGTH);

   /* The historical failure mode, kept as the reason the production
    * key is heap-owned: reverting connection_url to a fixed buffer
    * reintroduces exactly this. */
   key_fixed_set(&ka, url);
   CHECK(key_fixed_matches(&ka, same) == 0,
         "fixed-size key unexpectedly matched -- this oracle is meant "
         "to reproduce the pre-fix behaviour and no longer does, so "
         "the rest of this test is not proving what it claims");

   /* Production behaviour: the guard works at any length. */
   if (key_owned_set(&kb, url))
   {
      CHECK(key_owned_matches(&kb, same) == 1,
            "owned key must match its own URL regardless of length");
      key_owned_free(&kb);
   }

   free(url);
   free(same);
}

/* The failure is one-directional: distinct long URLs must still be
 * distinct.  Both keys are expected to pass this -- it is here so a
 * future "fix" that normalises or hashes the key to a fixed width
 * cannot quietly introduce the false positive this bug does not
 * have. */
static void test_distinct_long_urls_stay_distinct(void)
{
   size_t pad   = NAME_MAX_LENGTH;
   char  *url_a = make_thumb_url(pad, "Castlevania%20-%20SOTN.png");
   char  *url_b = make_thumb_url(pad, "Silent%20Hill.png");
   struct key_fixed ka;
   struct key_owned kb;

   printf("test_distinct_long_urls_stay_distinct\n");
   if (!url_a || !url_b)
   {
      printf("  SKIP: out of memory\n");
      free(url_a);
      free(url_b);
      return;
   }

   CHECK(strcmp(url_a, url_b) != 0, "fixture URLs must differ");

   key_fixed_set(&ka, url_a);
   CHECK(key_fixed_matches(&ka, url_b) == 0,
         "distinct URLs must not be conflated (fixed key)");

   if (key_owned_set(&kb, url_a))
   {
      CHECK(key_owned_matches(&kb, url_b) == 0,
            "distinct URLs must not be conflated (owned key)");
      key_owned_free(&kb);
   }

   free(url_a);
   free(url_b);
}

/* URLs comfortably inside the buffer must behave identically under
 * both keys -- the replacement must not perturb the common case. */
static void test_short_urls_unaffected(void)
{
   static const char a[] = "http://buildbot.libretro.com/nightly/.index";
   static const char b[] = "http://buildbot.libretro.com/nightly/.index2";
   struct key_fixed ka;
   struct key_owned kb;

   printf("test_short_urls_unaffected\n");

   key_fixed_set(&ka, a);
   CHECK(key_fixed_matches(&ka, a) == 1, "short URL must self-match (fixed)");
   CHECK(key_fixed_matches(&ka, b) == 0, "short distinct URLs must differ (fixed)");

   if (key_owned_set(&kb, a))
   {
      CHECK(key_owned_matches(&kb, a) == 1, "short URL must self-match (owned)");
      CHECK(key_owned_matches(&kb, b) == 0, "short distinct URLs must differ (owned)");
      key_owned_free(&kb);
   }
}

/* Walk the URL length across the buffer boundary and pin down
 * exactly where the fixed key stops working.  Self-match must hold
 * for every length under the owned key; under the fixed key it is
 * expected to hold for len < NAME_MAX_LENGTH and fail from
 * NAME_MAX_LENGTH onward.  Asserting the precise boundary means a
 * change to NAME_MAX_LENGTH, or a partial fix, surfaces here rather
 * than silently shifting. */
static void test_self_match_boundary(void)
{
   size_t len;
   size_t first_fail = 0;

   printf("test_self_match_boundary\n");

   for (len = NAME_MAX_LENGTH - 4; len <= NAME_MAX_LENGTH + 4; len++)
   {
      struct key_fixed ka;
      struct key_owned kb;
      char *u = (char*)malloc(len + 1);
      char *v;

      if (!u)
         continue;
      memset(u, 'u', len);
      u[len] = '\0';
      if (!(v = dup_str(u)))
      {
         free(u);
         continue;
      }

      key_fixed_set(&ka, u);
      if (!key_fixed_matches(&ka, v) && !first_fail)
         first_fail = len;

      if (key_owned_set(&kb, u))
      {
         CHECK(key_owned_matches(&kb, v) == 1,
               "owned key must self-match at len=%u", (unsigned)len);
         key_owned_free(&kb);
      }

      free(u);
      free(v);
   }

   CHECK(first_fail == NAME_MAX_LENGTH,
         "fixed key expected to first fail self-match at len=%u, got %u",
         (unsigned)NAME_MAX_LENGTH, (unsigned)first_fail);
}

int main(void)
{
   printf("task_http URL dedup key regression tests "
          "(NAME_MAX_LENGTH=%u)\n\n", (unsigned)NAME_MAX_LENGTH);

   test_short_urls_unaffected();
   test_distinct_long_urls_stay_distinct();
   test_long_url_fails_to_dedup_itself();
   test_self_match_boundary();

   printf("\n%s (%d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
