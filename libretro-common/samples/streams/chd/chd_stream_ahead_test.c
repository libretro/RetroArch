/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_stream_ahead_test.c).
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

/* chd_stream's decode-ahead: reads long enough to start the thread
 * must return exactly the image, read in bulk, a sector at a time,
 * across seeks in both directions, and on an audio track, whose
 * octet pairs the stream swaps; and a stream closed while the thread
 * is busy must come apart cleanly. The images are uncompressed
 * version 5 CD images written here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <streams/chd_stream.h>
#include <streams/file_stream.h>

#define UNIT_BYTES   2448
#define SECTOR_BYTES 2352
#define FRAMES       640
#define HUNK_FRAMES  8
#define HUNK_BYTES   (UNIT_BYTES * HUNK_FRAMES)
#define HUNKS        (FRAMES / HUNK_FRAMES)
#define TRACK_BYTES  ((size_t)FRAMES * SECTOR_BYTES)

static int fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         fails++; \
      } \
   } while (0)

static uint8_t stored(uint32_t frame, uint32_t i)
{
   return (uint8_t)(((frame * 2654435761u) ^ (i * 40503u) ^ 0x5Au) >> 3);
}

/* What the stream returns at track offset @at: an audio track comes
 * back with each octet pair swapped. */
static uint8_t expect(bool audio, size_t at)
{
   uint32_t f = (uint32_t)(at / SECTOR_BYTES);
   uint32_t i = (uint32_t)(at % SECTOR_BYTES);
   return stored(f, audio ? (i ^ 1) : i);
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)v;
}

static void put64(uint8_t *p, uint64_t v)
{
   put32(p,     (uint32_t)(v >> 32));
   put32(p + 4, (uint32_t)v);
}

/* Hunk i lives at (i + 1) * HUNK_BYTES: map value 0 is a hole. */
static bool write_image(const char *path, const char *type)
{
   char     meta[160];
   size_t   meta_len, meta_off, len;
   uint8_t *img;
   uint32_t f, i;
   bool     ok;

   sprintf(meta, "TRACK:1 TYPE:%s SUBTYPE:NONE FRAMES:%u "
         "PREGAP:0 PGTYPE:MODE1 PGSUB:NONE POSTGAP:0", type, FRAMES);
   meta_len = strlen(meta) + 1;
   meta_off = (size_t)(HUNKS + 1) * HUNK_BYTES;
   len      = meta_off + 16 + meta_len;
   if (!(img = (uint8_t*)calloc(1, len)))
      return false;
   memcpy(img, "MComprHD", 8);
   put32(img + 8,  124);
   put32(img + 12, 5);
   put64(img + 32, (uint64_t)FRAMES * UNIT_BYTES);
   put64(img + 40, 124);
   put64(img + 48, meta_off);
   put32(img + 56, HUNK_BYTES);
   put32(img + 60, UNIT_BYTES);
   for (i = 0; i < HUNKS; i++)
      put32(img + 124 + i * 4, i + 1);
   for (f = 0; f < FRAMES; f++)
   {
      uint8_t *fr = img + (size_t)(f / HUNK_FRAMES + 1) * HUNK_BYTES
                  + (size_t)(f % HUNK_FRAMES) * UNIT_BYTES;
      for (i = 0; i < SECTOR_BYTES; i++)
         fr[i] = stored(f, i);
   }
   memcpy(img + meta_off, "CHT2", 4);
   img[meta_off + 4] = 0x01;
   img[meta_off + 5] = (uint8_t)(meta_len >> 16);
   img[meta_off + 6] = (uint8_t)(meta_len >> 8);
   img[meta_off + 7] = (uint8_t)meta_len;
   memcpy(img + meta_off + 16, meta, meta_len);

   ok = filestream_write_file(path, img, (int64_t)len);
   free(img);
   return ok;
}

static bool same(bool audio, const uint8_t *buf, size_t at, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
      if (buf[i] != expect(audio, at + i))
         return false;
   return true;
}

/* The whole track in calls of @chunk octets. */
static bool read_through(const char *path, bool audio, size_t chunk,
      uint8_t *buf)
{
   chdstream_t *s = chdstream_open(path, 1);
   size_t       at = 0;
   bool         ok = s != NULL;

   while (ok && at < TRACK_BYTES)
   {
      ssize_t n = chdstream_read(s, buf, chunk);
      if (n <= 0 || !same(audio, buf, at, (size_t)n))
         ok = false;
      else
         at += (size_t)n;
   }
   if (ok && chdstream_read(s, buf, chunk) != 0)
      ok = false;
   chdstream_close(s);
   return ok;
}

/* Sector runs at offsets that jump back and forth over the track,
 * each run long enough to keep the thread going. */
static bool read_jumping(const char *path, bool audio, uint8_t *buf)
{
   static const uint32_t starts[] = { 0, 400, 37, 300, 300, 5, 560, 129 };
   chdstream_t *s = chdstream_open(path, 1);
   unsigned     k;
   bool         ok = s != NULL;

   for (k = 0; ok && k < sizeof(starts) / sizeof(starts[0]); k++)
   {
      uint32_t f;
      if (chdstream_seek(s, (int64_t)starts[k] * SECTOR_BYTES, SEEK_SET))
         ok = false;
      for (f = starts[k]; ok && f < starts[k] + 72 && f < FRAMES; f++)
         if (     chdstream_read(s, buf, SECTOR_BYTES) != SECTOR_BYTES
               || !same(audio, buf, (size_t)f * SECTOR_BYTES,
                  SECTOR_BYTES))
            ok = false;
   }
   chdstream_close(s);
   return ok;
}

/* Starts the thread with a bulk read, then closes at once. */
static bool close_busy(const char *path, uint8_t *buf)
{
   int r;
   for (r = 0; r < 20; r++)
   {
      chdstream_t *s = chdstream_open(path, 1);
      if (!s || chdstream_read(s, buf, HUNK_BYTES * 3) != HUNK_BYTES * 3)
      {
         chdstream_close(s);
         return false;
      }
      chdstream_close(s);
   }
   return true;
}

int main(void)
{
   static const char *data  = "chd_stream_ahead_data.chd";
   static const char *audio = "chd_stream_ahead_audio.chd";
   uint8_t *buf = (uint8_t*)malloc(TRACK_BYTES);

   if (!buf || !write_image(data, "MODE1_RAW") || !write_image(audio, "AUDIO"))
   {
      printf("FAIL: could not write the images\n");
      return 1;
   }

   CHECK(read_through(data, false, 256 * 1024, buf),
         "a track read in bulk returns the image");
   CHECK(read_through(data, false, SECTOR_BYTES, buf),
         "a track read a sector at a time returns the image");
   CHECK(read_through(data, false, TRACK_BYTES, buf),
         "a track read in one call returns the image");
   CHECK(read_through(audio, true, 256 * 1024, buf),
         "an audio track read in bulk comes back swapped");
   CHECK(read_through(audio, true, SECTOR_BYTES, buf),
         "an audio track read a sector at a time comes back swapped");
   CHECK(read_jumping(data, false, buf),
         "runs read across seeks back and forth return the image");
   CHECK(read_jumping(audio, true, buf),
         "and on an audio track");
   CHECK(close_busy(data, buf),
         "a stream closes while the thread decodes");

   filestream_delete(data);
   filestream_delete(audio);
   free(buf);

   if (fails)
   {
      printf("%d FAIL\n", fails);
      return 1;
   }
   printf("chd_stream_ahead_test: PASS\n");
   return 0;
}
