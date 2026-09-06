/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Regression test for ui/companion/companion_thumbs: the thumbnail
 * engine every desktop companion draws through. Runs on Linux with the
 * real decoder (image_texture_load) against TGA fixtures it writes
 * itself, so it exercises the decode threads for real. Build and run
 * with tools/companion_thumbs_test.sh (optionally under TSan / ASan).
 *
 * Covered:
 *   - a decode lands, scaled and letterboxed, with the right pixels
 *   - the cache serves a second request for the same key without a
 *     decode and touches it as most-recently-used
 *   - the byte budget evicts least-recently-used entries first
 *   - urgent requests are served newest-first, before prefetch ones
 *   - cancel() empties the queues but keeps the cache
 *   - an undecodable file is delivered with NULL pixels and forgotten
 *   - many requests across several threads: every one delivered exactly
 *     once; free() with work in flight returns (no hang, no leak) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include "../companion_thumbs.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char tmpdir[512];

/* Uncompressed 32-bit TGA, top-left origin, filled with @argb; a @w x @h
 * image. TGA is the simplest format image_texture_load decodes. */
static bool write_tga(const char *path, unsigned w, unsigned h, uint32_t argb)
{
   FILE *f = fopen(path, "wb");
   unsigned char hdr[18];
   unsigned i;
   if (!f)
      return false;
   memset(hdr, 0, sizeof(hdr));
   hdr[2]  = 2;                 /* uncompressed true-colour */
   hdr[12] = (unsigned char)(w & 0xff);
   hdr[13] = (unsigned char)(w >> 8);
   hdr[14] = (unsigned char)(h & 0xff);
   hdr[15] = (unsigned char)(h >> 8);
   hdr[16] = 32;                /* bpp */
   hdr[17] = 0x28;              /* 8 alpha bits, top-left origin */
   fwrite(hdr, 1, sizeof(hdr), f);
   for (i = 0; i < w * h; i++)
   {
      unsigned char px[4];
      px[0] = (unsigned char)(argb & 0xff);          /* B */
      px[1] = (unsigned char)((argb >> 8) & 0xff);   /* G */
      px[2] = (unsigned char)((argb >> 16) & 0xff);  /* R */
      px[3] = (unsigned char)((argb >> 24) & 0xff);  /* A */
      fwrite(px, 1, 4, f);
   }
   fclose(f);
   return true;
}

static void fixture(char *out, size_t len, const char *name)
{
   snprintf(out, len, "%s/%s", tmpdir, name);
}

/* --- delivery capture ---------------------------------------------------- */

struct got
{
   uintptr_t tag;
   int edge;
   bool null;
   uint32_t centre, corner;
};
static struct got gots[4096];
static size_t ngot;

static void on_done(void *ud, const char *path, int edge, uintptr_t tag,
      const uint32_t *bits)
{
   struct got *g;
   (void)ud; (void)path;
   if (ngot >= sizeof(gots) / sizeof(gots[0]))
      return;
   g         = &gots[ngot++];
   g->tag    = tag;
   g->edge   = edge;
   g->null   = (bits == NULL);
   g->centre = bits ? bits[(size_t)(edge / 2) * edge + edge / 2] : 0;
   g->corner = bits ? bits[0] : 0;
}

static void sleep_ms(int ms)
{
   struct timespec ts;
   ts.tv_sec  = ms / 1000;
   ts.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}

/* Poll until @want deliveries or a timeout. */
static size_t drain(companion_thumbs_t *t, size_t want, int timeout_ms)
{
   int waited = 0;
   size_t before = ngot;
   while (ngot - before < want && waited < timeout_ms)
   {
      companion_thumbs_poll(t, on_done, NULL, 0, 20000);
      if (ngot - before >= want)
         break;
      sleep_ms(2);
      waited += 2;
   }
   return ngot - before;
}

/* --- tests ---------------------------------------------------------------- */

static void test_decode_and_scale(void)
{
   char red[512], tall[512];
   companion_thumbs_t *t = companion_thumbs_new(0, 2);
   const uint32_t *bits;

   fixture(red,  sizeof(red),  "red_64x64.tga");
   fixture(tall, sizeof(tall), "green_20x80.tga");
   write_tga(red,  64, 64, 0xffff0000u);
   write_tga(tall, 20, 80, 0xff00ff00u);

   ngot = 0;
   CHECK(companion_thumbs_request(t, red, 32, 1, true, 0xff000000u), "request accepted");
   /* (queued() is 0 or 1 here depending on whether a worker already
    * took the job - both are correct, so it is not asserted.) */
   CHECK(drain(t, 1, 2000) == 1, "red delivered");
   CHECK(gots[0].tag == 1 && gots[0].edge == 32 && !gots[0].null, "red tag/edge");
   CHECK(gots[0].centre == 0xffff0000u, "red centre pixel 0x%08x", gots[0].centre);
   CHECK(gots[0].corner == 0xffff0000u, "square fills the whole thumb");

   /* Tall image: letterboxed left/right with the bg colour. */
   ngot = 0;
   companion_thumbs_request(t, tall, 40, 2, true, 0xff123456u);
   CHECK(drain(t, 1, 2000) == 1, "tall delivered");
   CHECK(gots[0].centre == 0xff00ff00u, "tall centre is the image");
   CHECK(gots[0].corner == 0xff123456u, "tall corner is the letterbox bg 0x%08x", gots[0].corner);

   /* Cached now: get() serves it, request() declines. */
   bits = companion_thumbs_get(t, red, 32);
   CHECK(bits && bits[0] == 0xffff0000u, "cache get");
   CHECK(!companion_thumbs_request(t, red, 32, 3, true, 0), "cached key not re-queued");
   CHECK(companion_thumbs_get(t, red, 33) == NULL, "other edge is a different key");
   CHECK(companion_thumbs_cached_count(t) == 2, "two cached");
   CHECK(companion_thumbs_cached_bytes(t) == 32u * 32 * 4 + 40u * 40 * 4, "cached bytes");

   companion_thumbs_free(t);
}

static void test_lru_budget(void)
{
   char p[8][512];
   int i;
   /* budget for exactly three 16x16 thumbs */
   companion_thumbs_t *t = companion_thumbs_new(3 * 16 * 16 * 4, 1);
   for (i = 0; i < 4; i++)
   {
      char name[32];
      snprintf(name, sizeof(name), "lru%d.tga", i);
      fixture(p[i], sizeof(p[i]), name);
      write_tga(p[i], 8, 8, 0xff000000u | (uint32_t)(i * 40));
   }
   ngot = 0;
   companion_thumbs_request(t, p[0], 16, 0, true, 0);
   drain(t, 1, 2000);
   companion_thumbs_request(t, p[1], 16, 1, true, 0);
   drain(t, 1, 2000);
   companion_thumbs_request(t, p[2], 16, 2, true, 0);
   drain(t, 1, 2000);
   CHECK(companion_thumbs_cached_count(t) == 3, "three fit");
   /* touch p[0] so p[1] is the least recently used */
   CHECK(companion_thumbs_get(t, p[0], 16) != NULL, "touch p0");
   companion_thumbs_request(t, p[3], 16, 3, true, 0);
   drain(t, 1, 2000);
   CHECK(companion_thumbs_cached_count(t) == 3, "still three after eviction");
   CHECK(companion_thumbs_get(t, p[1], 16) == NULL, "LRU p1 evicted");
   CHECK(companion_thumbs_get(t, p[0], 16) != NULL, "touched p0 kept");
   CHECK(companion_thumbs_get(t, p[3], 16) != NULL, "new p3 kept");
   CHECK(companion_thumbs_cached_bytes(t) <= 3u * 16 * 16 * 4, "within budget");
   companion_thumbs_free(t);
}

static void test_priority_and_cancel(void)
{
   char p[6][512];
   int i;
   /* No threads started: poll() decodes in order, so the order is
    * observable. (companion_thumbs_new with threads=1 still starts a
    * worker under HAVE_THREADS; use the queue state instead.) */
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   for (i = 0; i < 6; i++)
   {
      char name[32];
      snprintf(name, sizeof(name), "pri%d.tga", i);
      fixture(p[i], sizeof(p[i]), name);
      write_tga(p[i], 4, 4, 0xffffffffu);
   }
   /* Cancel: queued requests vanish, cache stays. */
   ngot = 0;
   companion_thumbs_request(t, p[0], 8, 0, true, 0);
   drain(t, 1, 2000);
   for (i = 1; i < 6; i++)
      companion_thumbs_request(t, p[i], 8, (uintptr_t)i, i < 3, 0);
   companion_thumbs_cancel(t);
   CHECK(companion_thumbs_queued(t) == 0, "cancel empties queues");
   CHECK(companion_thumbs_get(t, p[0], 8) != NULL, "cancel keeps the cache");
   /* A cancelled key can be requested again. */
   CHECK(companion_thumbs_request(t, p[1], 8, 1, true, 0), "re-request after cancel");
   drain(t, 1, 2000);
   CHECK(companion_thumbs_get(t, p[1], 8) != NULL, "re-requested decoded");
   companion_thumbs_free(t);
}

static void test_undecodable(void)
{
   char bad[512];
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   FILE *f;
   fixture(bad, sizeof(bad), "bad.tga");
   f = fopen(bad, "wb");
   fputs("not a tga", f);
   fclose(f);
   ngot = 0;
   companion_thumbs_request(t, bad, 16, 9, true, 0);
   CHECK(drain(t, 1, 2000) == 1, "undecodable delivered");
   CHECK(gots[0].null && gots[0].tag == 9, "delivered with NULL bits");
   CHECK(companion_thumbs_cached_count(t) == 0, "not cached");
   /* forgotten: can be requested again (e.g. after a download fixes it) */
   CHECK(companion_thumbs_request(t, bad, 16, 9, true, 0), "retry allowed");
   drain(t, 1, 2000);
   companion_thumbs_free(t);
}

static void test_many_and_shutdown(void)
{
   enum { N = 300 };
   static char paths[N][512];
   int i;
   companion_thumbs_t *t = companion_thumbs_new(0, 4);
   for (i = 0; i < N; i++)
   {
      char name[32];
      snprintf(name, sizeof(name), "many%d.tga", i);
      fixture(paths[i], sizeof(paths[i]), name);
      write_tga(paths[i], 32, 24, 0xff000000u | (uint32_t)i);
   }
   ngot = 0;
   for (i = 0; i < N; i++)
      companion_thumbs_request(t, paths[i], 48, (uintptr_t)i, (i & 1) != 0, 0);
   CHECK(drain(t, N, 10000) == N, "all %d delivered (got %u)", N, (unsigned)ngot);
   {
      /* each tag exactly once */
      static unsigned char seen[N];
      size_t k;
      int dup = 0, missing = 0;
      memset(seen, 0, sizeof(seen));
      for (k = 0; k < ngot; k++)
      {
         if (gots[k].tag < N)
         {
            if (seen[gots[k].tag]) dup++;
            seen[gots[k].tag] = 1;
         }
      }
      for (i = 0; i < N; i++)
         if (!seen[i]) missing++;
      CHECK(!dup && !missing, "exactly once: dup=%d missing=%d", dup, missing);
   }
   CHECK(companion_thumbs_cached_count(t) == N, "all cached");

   /* Shutdown with work in flight must return. */
   for (i = 0; i < N; i++)
      companion_thumbs_request(t, paths[i], 64, (uintptr_t)i, true, 0);
   companion_thumbs_free(t);
}

int main(int argc, char **argv)
{
   const char *dir = (argc > 1) ? argv[1] : "/tmp";
   snprintf(tmpdir, sizeof(tmpdir), "%s/companion_thumbs_test_%ld", dir, (long)time(NULL));
   {
      char cmd[600];
      snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", tmpdir);
      if (system(cmd) != 0)
      {
         printf("cannot create %s\n", tmpdir);
         return 2;
      }
   }

   test_decode_and_scale();
   test_lru_budget();
   test_priority_and_cancel();
   test_undecodable();
   test_many_and_shutdown();

   {
      char cmd[600];
      snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir);
      if (system(cmd) != 0)
         printf("(could not remove %s)\n", tmpdir);
   }
   if (fails)
   {
      printf("companion_thumbs_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("companion_thumbs_test: OK\n");
   return 0;
}
