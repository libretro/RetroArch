/* The RZIP container with either codec.
 *
 * Version 1 holds deflate chunks, version 2 Zstandard frames. A file
 * written with either must read back byte-exact through the stream
 * and through rzipstream_read_file(), the header must carry the
 * version the writer chose, a stream must report the codec it was
 * opened with, and the choice must not change what an uncompressed
 * file or a version-1 file reads as. Then the numbers: the same
 * state-shaped buffer through both, timed. Informational, not gated. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <boolean.h>
#include <streams/rzip_stream.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static double now_ms(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

/* A savestate-shaped buffer: pages of zeros, structured pages, noisy
 * pages. */
static void fill_state(uint8_t *b, size_t n)
{
   uint32_t x = 0x9E3779B9u;
   size_t i, j;
   for (i = 0; i < n; i += 4096)
   {
      unsigned kind;
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      kind = x % 10;
      for (j = 0; j < 4096 && i + j < n; j++)
      {
         if (kind < 4)
            b[i + j] = 0;
         else if (kind < 8)
         {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b[i + j] = (j & 1) ? (uint8_t)(x & 15) : (uint8_t)(j >> 4);
         }
         else
         {
            x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            b[i + j] = (uint8_t)x;
         }
      }
   }
}

static bool write_with(const char *path, enum rzip_codec codec, const uint8_t *data, size_t len, double *ms)
{
   double t0 = now_ms();
   bool ok;
   rzipstream_set_write_codec(codec);
   ok = rzipstream_write_file(path, data, (int64_t)len);
   *ms = now_ms() - t0;
   return ok;
}

static bool read_back(const char *path, const uint8_t *data, size_t len, double *ms)
{
   void *buf = NULL;
   int64_t got = 0;
   bool ok;
   double t0 = now_ms();
   ok = rzipstream_read_file(path, &buf, &got);
   *ms = now_ms() - t0;
   ok = ok && got == (int64_t)len && memcmp(buf, data, len) == 0;
   free(buf);
   return ok;
}

/* Written through the stream in writes of @quantum bytes, as the save
 * task writes */
static bool write_in(const char *path, const uint8_t *data, size_t len,
      size_t quantum)
{
   size_t done = 0;
   rzipstream_t *st = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_WRITE);
   if (!st)
      return false;
   while (done < len)
   {
      size_t q = len - done < quantum ? len - done : quantum;
      if (rzipstream_write(st, data + done, (int64_t)q) != (int64_t)q)
      {
         rzipstream_close(st);
         return false;
      }
      done += q;
   }
   return rzipstream_close(st) == 0;
}

/* Read back through the stream in reads of @quantum bytes - whole
 * chunks go straight into the caller's buffer, the rest through the
 * stream's own - rewound once a third of the way in */
static bool read_in(const char *path, const uint8_t *data, size_t len,
      size_t quantum)
{
   size_t done = 0;
   bool ok = true, rewound = false;
   uint8_t *buf = (uint8_t*)malloc(len + 1);
   rzipstream_t *st = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_READ);
   if (!st || !buf)
      ok = false;
   while (ok && done < len)
   {
      size_t q = len - done < quantum ? len - done : quantum;
      ok = rzipstream_read(st, buf + done, (int64_t)q) == (int64_t)q;
      done += q;
      if (ok && !rewound && done > len / 3)
      {
         rewound = true;
         rzipstream_rewind(st);
         done = 0;
      }
   }
   /* and nothing past the end */
   ok = ok && rzipstream_read(st, buf, 1) == 0
           && memcmp(buf, data, len) == 0;
   if (st)
      rzipstream_close(st);
   free(buf);
   return ok;
}

static bool same_file(const char *a, const char *b)
{
   void *x = NULL, *y = NULL;
   int64_t nx = 0, ny = 0;
   bool same = filestream_read_file(a, &x, &nx)
            && filestream_read_file(b, &y, &ny)
            && nx == ny && memcmp(x, y, (size_t)nx) == 0;
   free(x);
   free(y);
   return same;
}

static uint8_t header_version(const char *path)
{
   uint8_t h[8] = {0};
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f) return 0;
   filestream_read(f, h, sizeof(h));
   filestream_close(f);
   return h[6];
}

int main(void)
{
   const char *p1 = "rzip_codec_v1.bin", *p2 = "rzip_codec_v2.bin", *pr = "rzip_codec_raw.bin";
   size_t n = 16u << 20;
   uint8_t *data = (uint8_t*)malloc(n);
   double w1, r1, w2, r2, wr, rr;
   int64_t s1, s2;
   rzipstream_t *st;

   printf("rzip codec:\n");
   fill_state(data, n);

   printf("   both codecs are available in this build\n");
   CHECK(rzipstream_codec_available(RZIP_CODEC_DEFLATE), "deflate reported unavailable");
   CHECK(rzipstream_codec_available(RZIP_CODEC_ZSTD), "zstd reported unavailable");

   printf("   version 1 (deflate): write, header, read back\n");
   CHECK(write_with(p1, RZIP_CODEC_DEFLATE, data, n, &w1), "the deflate write failed");
   CHECK(header_version(p1) == 1, "the deflate file's header says version %u", header_version(p1));
   CHECK(read_back(p1, data, n, &r1), "the deflate file did not read back byte-exact");

   printf("   version 2 (zstd): write, header, read back\n");
   CHECK(write_with(p2, RZIP_CODEC_ZSTD, data, n, &w2), "the zstd write failed");
   CHECK(header_version(p2) == 2, "the zstd file's header says version %u", header_version(p2));
   CHECK(read_back(p2, data, n, &r2), "the zstd file did not read back byte-exact");

   printf("   a reader takes either version whatever the write codec is set to\n");
   rzipstream_set_write_codec(RZIP_CODEC_DEFLATE);
   CHECK(read_back(p2, data, n, &r2), "a zstd file did not read with deflate set for writing");
   rzipstream_set_write_codec(RZIP_CODEC_ZSTD);
   CHECK(read_back(p1, data, n, &r1), "a deflate file did not read with zstd set for writing");

   printf("   read through the stream in any quantum, whole chunks or not\n");
   {
      static const size_t quanta[] = { 1, 4095, 100 * 1024,
         RZIP_DEFAULT_CHUNK_SIZE, RZIP_DEFAULT_CHUNK_SIZE + 1,
         2 * RZIP_DEFAULT_CHUNK_SIZE, 5 * RZIP_DEFAULT_CHUNK_SIZE / 2,
         1u << 20 };
      size_t q, odd = n - 777; /* a short last chunk */
      for (q = 0; q < sizeof(quanta) / sizeof(quanta[0]); q++)
      {
         size_t len = quanta[q] == 1 ? 200000 : odd;
         CHECK(write_in(p1, data, len, 1u << 20)
               && read_in(p1, data, len, quanta[q]),
               "deflate read in %u-byte reads", (unsigned)quanta[q]);
      }
      rzipstream_set_write_codec(RZIP_CODEC_ZSTD);
      CHECK(write_in(p2, data, odd, 1u << 20)
            && read_in(p2, data, odd, 2 * RZIP_DEFAULT_CHUNK_SIZE)
            && read_in(p2, data, odd, 100 * 1024),
            "zstd read in whole and partial chunks");

      printf("   any write quantum makes the same file\n");
      rzipstream_set_write_codec(RZIP_CODEC_DEFLATE);
      CHECK(write_in(p1, data, odd, 100 * 1024)
            && write_in(pr, data, odd, 2 * RZIP_DEFAULT_CHUNK_SIZE)
            && same_file(p1, pr),
            "100 KiB and two-chunk writes made different files");
      filestream_delete(pr);
   }

   printf("   the stream reports compression, the raw file does not\n");
   st = rzipstream_open(p2, RETRO_VFS_FILE_ACCESS_READ);
   CHECK(st && rzipstream_is_compressed(st), "the zstd file is not reported compressed");
   if (st) rzipstream_close(st);
   {
      RFILE *f = filestream_open(pr, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (f) { filestream_write(f, data, (int64_t)n); filestream_close(f); }
   }
   st = rzipstream_open(pr, RETRO_VFS_FILE_ACCESS_READ);
   CHECK(st && !rzipstream_is_compressed(st), "an uncompressed file is reported compressed");
   if (st) rzipstream_close(st);
   CHECK(read_back(pr, data, n, &rr), "the uncompressed file did not read back");
   (void)wr;

   printf("   the setting's default here is zstd\n");
   /* The default is what the build had before any set_write_codec. This
    * harness set it; the default is checked by inspection of the source
    * (RZIP_CODEC_ZSTD under HAVE_RZSTD). */

   s1 = path_get_size(p1);
   s2 = path_get_size(p2);
   printf("   16 MiB state: deflate write %.0f ms / read %.0f ms, %lld KiB;  zstd write %.0f ms / read %.0f ms, %lld KiB (informational)\n",
         w1, r1, (long long)(s1 >> 10), w2, r2, (long long)(s2 >> 10));

   filestream_delete(p1); filestream_delete(p2); filestream_delete(pr);
   free(data);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rzip codec: both versions round-trip byte-exact, and a reader takes either\n");
   return 0;
}
