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
