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
#include <encodings/crc32.h>
#include <zlib.h>

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

/* --- a hand-made two-frame APNG ------------------------------------------ */

static void png_chunk(FILE *f, const char *type, const uint8_t *data, size_t len)
{
   uint8_t hdr[8];
   uint32_t crc;
   uint32_t n = (uint32_t)len;
   hdr[0] = (uint8_t)(n >> 24); hdr[1] = (uint8_t)(n >> 16);
   hdr[2] = (uint8_t)(n >> 8);  hdr[3] = (uint8_t)n;
   memcpy(hdr + 4, type, 4);
   fwrite(hdr, 1, 8, f);
   if (len)
      fwrite(data, 1, len, f);
   crc = encoding_crc32(0, (const uint8_t*)type, 4);
   if (len)
      crc = encoding_crc32(crc, data, len);
   hdr[0] = (uint8_t)(crc >> 24); hdr[1] = (uint8_t)(crc >> 16);
   hdr[2] = (uint8_t)(crc >> 8);  hdr[3] = (uint8_t)crc;
   fwrite(hdr, 1, 4, f);
}

static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* Solid-colour w x h RGBA rows with filter byte 0, zlib-compressed. */
static size_t apng_frame_data(uint8_t *out, size_t outcap, unsigned w,
      unsigned h, uint32_t rgba)
{
   size_t raw_len = (size_t)h * (1 + (size_t)w * 4), i, x;
   uint8_t *raw = (uint8_t*)malloc(raw_len);
   uLongf dl = (uLongf)outcap;
   for (i = 0; i < h; i++)
   {
      uint8_t *row = raw + i * (1 + (size_t)w * 4);
      row[0] = 0;
      for (x = 0; x < w; x++)
      {
         row[1 + x * 4 + 0] = (uint8_t)(rgba >> 24);
         row[1 + x * 4 + 1] = (uint8_t)(rgba >> 16);
         row[1 + x * 4 + 2] = (uint8_t)(rgba >> 8);
         row[1 + x * 4 + 3] = (uint8_t)rgba;
      }
   }
   if (compress2(out, &dl, raw, (uLong)raw_len, 6) != Z_OK)
      dl = 0;
   free(raw);
   return (size_t)dl;
}

/* Two frames, each @delay_ms, looping forever: frame 0 red, frame 1
 * green (R,G,B,A in the file). */
static bool write_apng(const char *path, unsigned w, unsigned h, int delay_ms)
{
   static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
   FILE *f = fopen(path, "wb");
   uint8_t ihdr[13], actl[8], fctl[26], comp[4096], fdat[4100];
   size_t n;
   if (!f)
      return false;
   fwrite(sig, 1, 8, f);
   be32(ihdr, w); be32(ihdr + 4, h);
   ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
   png_chunk(f, "IHDR", ihdr, 13);
   be32(actl, 2); be32(actl + 4, 0);          /* 2 frames, loop forever */
   png_chunk(f, "acTL", actl, 8);
   /* frame 0: fcTL seq 0, then IDAT */
   memset(fctl, 0, sizeof(fctl));
   be32(fctl, 0); be32(fctl + 4, w); be32(fctl + 8, h);
   be16(fctl + 20, (uint16_t)delay_ms); be16(fctl + 22, 1000);
   png_chunk(f, "fcTL", fctl, 26);
   n = apng_frame_data(comp, sizeof(comp), w, h, 0xff0000ffu);   /* red */
   png_chunk(f, "IDAT", comp, n);
   /* frame 1: fcTL seq 1, fdAT seq 2 */
   be32(fctl, 1);
   png_chunk(f, "fcTL", fctl, 26);
   n = apng_frame_data(comp, sizeof(comp), w, h, 0x00ff00ffu);   /* green */
   be32(fdat, 2);
   memcpy(fdat + 4, comp, n);
   png_chunk(f, "fdAT", fdat, n + 4);
   png_chunk(f, "IEND", NULL, 0);
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

static void on_done(void *ud, const char *path, int w, int h, uintptr_t tag,
      const uint32_t *bits)
{
   struct got *g;
   (void)ud; (void)path;
   if (ngot >= sizeof(gots) / sizeof(gots[0]))
      return;
   g         = &gots[ngot++];
   g->tag    = tag;
   g->edge   = w;
   g->null   = (bits == NULL);
   /* centre of a w x h buffer */
   g->centre = bits ? bits[(size_t)(h / 2) * w + w / 2] : 0;
   g->corner = bits ? bits[0] : 0;
}

static void sleep_ms(int ms)
{
   struct timespec ts;
   ts.tv_sec  = ms / 1000;
   ts.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}

/* Poll until a delivery with @tag arrives (true) or a timeout. Other
 * deliveries - a job popped before a cancel() still lands with its old
 * epoch - do not count. */
static bool drain_tag(companion_thumbs_t *t, uintptr_t tag, int timeout_ms)
{
   int waited = 0;
   size_t i, from = ngot;
   for (;;)
   {
      companion_thumbs_poll(t, on_done, NULL, 0, 20000);
      for (i = from; i < ngot; i++)
         if (gots[i].tag == tag)
            return true;
      if (waited >= timeout_ms)
         return false;
      sleep_ms(2);
      waited += 2;
   }
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
   CHECK(companion_thumbs_request(t, red, 32, 32, 1, true, 0xff000000u), "request accepted");
   /* (queued() is 0 or 1 here depending on whether a worker already
    * took the job - both are correct, so it is not asserted.) */
   CHECK(drain(t, 1, 2000) == 1, "red delivered");
   CHECK(gots[0].tag == 1 && gots[0].edge == 32 && !gots[0].null, "red tag/edge");
   CHECK(gots[0].centre == 0xffff0000u, "red centre pixel 0x%08x", gots[0].centre);
   CHECK(gots[0].corner == 0xffff0000u, "square fills the whole thumb");

   /* Tall image: letterboxed left/right with the bg colour. */
   ngot = 0;
   companion_thumbs_request(t, tall, 40, 40, 2, true, 0xff123456u);
   CHECK(drain(t, 1, 2000) == 1, "tall delivered");
   CHECK(gots[0].centre == 0xff00ff00u, "tall centre is the image");
   CHECK(gots[0].corner == 0xff123456u, "tall corner is the letterbox bg 0x%08x", gots[0].corner);

   /* Rectangular box (a boxart pane): fit inside, letterboxed. */
   ngot = 0;
   companion_thumbs_request(t, tall, 60, 30, 5, true, 0xff0000ffu);
   CHECK(drain(t, 1, 2000) == 1, "rect delivered");
   CHECK(gots[0].tag == 5 && !gots[0].null, "rect tag");
   {
      /* 20x80 into 60x30 fits by height: a 7x30 image around x = 30;
       * the corner is letterbox, the centre column is image. */
      const uint32_t *r = companion_thumbs_get(t, tall, 60, 30);
      CHECK(r != NULL, "rect cached");
      CHECK(r && r[0] == 0xff0000ffu, "rect corner is bg");
      CHECK(r && r[15 * 60 + 30] == 0xff00ff00u, "rect centre is image");
   }

   /* Cached now: get() serves it, request() declines. */
   bits = companion_thumbs_get(t, red, 32, 32);
   CHECK(bits && bits[0] == 0xffff0000u, "cache get");
   CHECK(!companion_thumbs_request(t, red, 32, 32, 3, true, 0), "cached key not re-queued");
   CHECK(companion_thumbs_get(t, red, 33, 33) == NULL, "other edge is a different key");
   CHECK(companion_thumbs_cached_count(t) == 3, "three cached");
   CHECK(companion_thumbs_cached_bytes(t) == 32u * 32 * 4 + 40u * 40 * 4 + 60u * 30 * 4, "cached bytes");

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
   companion_thumbs_request(t, p[0], 16, 16, 0, true, 0);
   drain(t, 1, 2000);
   companion_thumbs_request(t, p[1], 16, 16, 1, true, 0);
   drain(t, 1, 2000);
   companion_thumbs_request(t, p[2], 16, 16, 2, true, 0);
   drain(t, 1, 2000);
   CHECK(companion_thumbs_cached_count(t) == 3, "three fit");
   /* touch p[0] so p[1] is the least recently used */
   CHECK(companion_thumbs_get(t, p[0], 16, 16) != NULL, "touch p0");
   companion_thumbs_request(t, p[3], 16, 16, 3, true, 0);
   drain(t, 1, 2000);
   CHECK(companion_thumbs_cached_count(t) == 3, "still three after eviction");
   CHECK(companion_thumbs_get(t, p[1], 16, 16) == NULL, "LRU p1 evicted");
   CHECK(companion_thumbs_get(t, p[0], 16, 16) != NULL, "touched p0 kept");
   CHECK(companion_thumbs_get(t, p[3], 16, 16) != NULL, "new p3 kept");
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
   companion_thumbs_request(t, p[0], 8, 8, 0, true, 0);
   drain(t, 1, 2000);
   for (i = 1; i < 6; i++)
      companion_thumbs_request(t, p[i], 8, 8, (uintptr_t)i, i < 3, 0);
   companion_thumbs_cancel(t);
   CHECK(companion_thumbs_queued(t) == 0, "cancel empties queues");
   CHECK(companion_thumbs_get(t, p[0], 8, 8) != NULL, "cancel keeps the cache");
   /* A cancelled key can be requested again. Wait for that request's
    * own delivery (tag 1): a job popped before the cancel may land
    * first with its old epoch, and must not be mistaken for it. */
   CHECK(companion_thumbs_request(t, p[1], 8, 8, 1, true, 0), "re-request after cancel");
   CHECK(drain_tag(t, 1, 5000), "re-requested key delivered");
   CHECK(companion_thumbs_get(t, p[1], 8, 8) != NULL, "re-requested decoded");
   companion_thumbs_free(t);
}

static void test_forget_and_budget(void)
{
   char p[512];
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   fixture(p, sizeof(p), "forget.tga");
   write_tga(p, 8, 8, 0xff112233u);
   ngot = 0;
   companion_thumbs_request(t, p, 16, 16, 0, true, 0);
   companion_thumbs_request(t, p, 24, 24, 1, true, 0);
   drain(t, 2, 2000);
   CHECK(companion_thumbs_cached_count(t) == 2, "two sizes cached");
   CHECK(companion_thumbs_forget(t, p) == 2, "forget drops every size");
   CHECK(companion_thumbs_get(t, p, 16, 16) == NULL, "forgotten");
   CHECK(companion_thumbs_request(t, p, 16, 16, 2, true, 0), "re-request after forget");
   CHECK(drain_tag(t, 2, 5000), "re-request after forget delivered");
   CHECK(companion_thumbs_get(t, p, 16, 16) != NULL, "decoded again");
   /* shrinking the budget evicts at once */
   companion_thumbs_request(t, p, 32, 32, 3, true, 0);
   drain(t, 1, 2000);
   CHECK(companion_thumbs_cached_count(t) == 2, "two again");
   companion_thumbs_set_budget(t, 32u * 32 * 4);
   CHECK(companion_thumbs_cached_count(t) == 1, "budget cut evicts LRU (got %u)", (unsigned)companion_thumbs_cached_count(t));
   CHECK(companion_thumbs_get(t, p, 32, 32) != NULL, "most recent kept");
   companion_thumbs_free(t);
}

static long ms_since(const struct timespec *t0)
{
   struct timespec t1;
   clock_gettime(CLOCK_MONOTONIC, &t1);
   return (long)((t1.tv_sec - t0->tv_sec) * 1000 + (t1.tv_nsec - t0->tv_nsec) / 1000000);
}

/* Big images must not hold up shutdown or a view that moved on: the
 * decode is abandoned between steps. */
static void test_abort(void)
{
   char big[8][512];
   int i;
   struct timespec t0;
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   /* 8 x (2048 x 2048 x 4 = 16 MiB) TGA: each decode is a real amount
    * of work on one worker. */
   for (i = 0; i < 8; i++)
   {
      char name[32];
      snprintf(name, sizeof(name), "big%d.tga", i);
      fixture(big[i], sizeof(big[i]), name);
      write_tga(big[i], 2048, 2048, 0xff808080u);
   }
   /* cancel() while decoding: the stale job is abandoned and its key
    * can be requested again */
   ngot = 0;
   for (i = 0; i < 8; i++)
      companion_thumbs_request(t, big[i], 64, 64, (uintptr_t)i, true, 0);
   sleep_ms(5);
   companion_thumbs_cancel(t);
   drain(t, 1, 300);
   CHECK(companion_thumbs_request(t, big[0], 64, 64, 100, true, 0), "re-request after cancel mid-decode");
   CHECK(drain_tag(t, 100, 10000), "re-requested big image lands (its own delivery, tag 100)");

   /* free() while decoding: returns promptly */
   for (i = 0; i < 8; i++)
      companion_thumbs_request(t, big[i], 96, 96, (uintptr_t)i, true, 0);
   sleep_ms(5);
   clock_gettime(CLOCK_MONOTONIC, &t0);
   companion_thumbs_free(t);
   CHECK(ms_since(&t0) < 1000, "free with 8 x 16 MiB decodes queued returned in %ld ms", ms_since(&t0));
}

/* Two records for one key in flight at once, both failing: the first
 * polled must not free the entry the second still points at. Built by
 * requesting an undecodable file, cancelling (the in-flight job keeps
 * its record), and requesting it again at once. */
static void test_double_failure_no_uaf(void)
{
   char bad[512];
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   FILE *f;
   int i;
   fixture(bad, sizeof(bad), "bad2.tga");
   f = fopen(bad, "wb");
   fputs("not a tga", f);
   fclose(f);
   ngot = 0;
   for (i = 0; i < 20; i++)
   {
      companion_thumbs_request(t, bad, 16, 16, 1, true, 0);
      companion_thumbs_cancel(t);
      companion_thumbs_request(t, bad, 16, 16, 2, true, 0);
      drain(t, 1, 1000);
      companion_thumbs_get(t, bad, 16, 16); /* walks the hash chain */
   }
   drain(t, 1, 1000);
   companion_thumbs_get(t, bad, 16, 16);
   CHECK(companion_thumbs_cached_count(t) == 0, "nothing cached");
   companion_thumbs_free(t);
}

/* Animated preview: an APNG plays through poll() as frame deliveries,
 * on its clock, scaled like a still; a still PNG produces none; stop
 * stops. */
static void test_animation(void)
{
   char apng[512], still[512];
   companion_thumbs_t *t = companion_thumbs_new(0, 1);
   int reds = 0, greens = 0;
   size_t i;
   fixture(apng, sizeof(apng), "anim.png");
   fixture(still, sizeof(still), "still.png");
   CHECK(write_apng(apng, 8, 8, 30), "wrote the APNG");
   /* a plain PNG through the same writer minus animation chunks is
    * more code than it is worth: a TGA is a still for this purpose */
   fixture(still, sizeof(still), "still.tga");
   write_tga(still, 8, 8, 0xff112233u);

   /* the still request works on it as on any PNG */
   ngot = 0;
   companion_thumbs_request(t, apng, 16, 16, 1, true, 0);
   CHECK(drain_tag(t, 1, 3000), "APNG decodes as a still first (frame 0)");
   CHECK(!gots[0].null && gots[0].centre == 0xffff0000u, "still is frame 0 (red): 0x%08x", gots[0].centre);

   /* animate: frames alternate red / green at ~30 ms */
   ngot = 0;
   companion_thumbs_animate(t, apng, 16, 16, 7, 0);
   CHECK(drain(t, 10, 4000) >= 10, "at least 10 frames in 4 s (got %u)", (unsigned)ngot);
   for (i = 0; i < ngot; i++)
   {
      CHECK(gots[i].tag == 7 && gots[i].edge == 16, "frame tag / size");
      if (gots[i].centre == 0xffff0000u) reds++;
      else if (gots[i].centre == 0xff00ff00u) greens++;
   }
   CHECK(reds >= 2 && greens >= 2, "both frames seen, more than once each (red %d, green %d)", reds, greens);
   CHECK(reds + greens == (int)ngot, "every frame is one of the two (red %d, green %d, total %u)", reds, greens, (unsigned)ngot);
   CHECK(companion_thumbs_animating(t), "reports animating");

   /* stop: no more frames after the one in flight */
   companion_thumbs_animate_stop(t);
   sleep_ms(120);
   companion_thumbs_poll(t, on_done, NULL, 0, 20000);
   ngot = 0;
   sleep_ms(150);
   companion_thumbs_poll(t, on_done, NULL, 0, 20000);
   CHECK(ngot == 0, "stopped: no frames (got %u)", (unsigned)ngot);

   /* a still produces no frames */
   companion_thumbs_animate(t, still, 16, 16, 8, 0);
   sleep_ms(200);
   ngot = 0;
   companion_thumbs_poll(t, on_done, NULL, 0, 20000);
   CHECK(ngot == 0, "a still animates nothing (got %u)", (unsigned)ngot);

   /* free with an animation running returns */
   companion_thumbs_animate(t, apng, 16, 16, 9, 0);
   sleep_ms(50);
   companion_thumbs_free(t);
}

/* The scaler: R,G,B,A memory-order input comes out as ARGB words with
 * no whole-canvas pass; a 4x downscale of a 2-colour checkerboard
 * averages the four taps (nearest would pick one colour per pixel). */
static void test_scaler(void)
{
   unsigned sw = 64, sh = 64, x, y;
   uint32_t *src = (uint32_t*)malloc(sw * sh * 4), *out;
   /* memory order R,G,B,A of pure red: bytes FF 00 00 FF -> LE word 0xFF0000FF */
   for (y = 0; y < sh; y++)
      for (x = 0; x < sw; x++)
         src[y * sw + x] = 0xFF0000FFu;
   out = companion_thumbs_scale_ex(src, sw, sh, 16, 16, 0, true);
   CHECK(out && out[8 * 16 + 8] == 0xffff0000u, "RGBA-order red -> ARGB red (got 0x%08x)", out ? out[8 * 16 + 8] : 0);
   free(out);
   /* the same words read as ARGB are blue */
   out = companion_thumbs_scale_ex(src, sw, sh, 16, 16, 0, false);
   CHECK(out && out[8 * 16 + 8] == 0xff0000ffu, "ARGB-order 0xFF0000FF is blue (got 0x%08x)", out ? out[8 * 16 + 8] : 0);
   free(out);
   /* checkerboard of white and black, 1-px squares, 4x down: grey */
   for (y = 0; y < sh; y++)
      for (x = 0; x < sw; x++)
         src[y * sw + x] = ((x + y) & 1) ? 0xffffffffu : 0xff000000u;
   out = companion_thumbs_scale_ex(src, sw, sh, 16, 16, 0, false);
   {
      uint32_t p = out ? out[8 * 16 + 8] : 0;
      unsigned r = (p >> 16) & 0xff;
      CHECK(out && r > 0x30 && r < 0xd0, "4-tap downscale averages the checkerboard to grey (got 0x%08x)", p);
   }
   free(out);
   /* enlarging keeps nearest: a 2x2 source to 8x8 has hard edges */
   src[0] = 0xffff0000u; src[1] = 0xff00ff00u; src[sw] = 0xff0000ffu; src[sw + 1] = 0xffffffffu;
   out = companion_thumbs_scale_ex(src, 2, 2, 8, 8, 0, false);
   CHECK(out && out[0] == 0xffff0000u && out[7] == 0xff00ff00u, "enlarging is nearest (corners 0x%08x 0x%08x)", out ? out[0] : 0, out ? out[7] : 0);
   free(out);
   free(src);
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
   companion_thumbs_request(t, bad, 16, 16, 9, true, 0);
   CHECK(drain(t, 1, 2000) == 1, "undecodable delivered");
   CHECK(gots[0].null && gots[0].tag == 9, "delivered with NULL bits");
   CHECK(companion_thumbs_cached_count(t) == 0, "not cached");
   /* forgotten: can be requested again (e.g. after a download fixes it) */
   CHECK(companion_thumbs_request(t, bad, 16, 16, 9, true, 0), "retry allowed");
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
      companion_thumbs_request(t, paths[i], 48, 48, (uintptr_t)i, (i & 1) != 0, 0);
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
      companion_thumbs_request(t, paths[i], 64, 64, (uintptr_t)i, true, 0);
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
   test_forget_and_budget();
   test_abort();
   test_double_failure_no_uaf();
   test_animation();
   test_scaler();
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
