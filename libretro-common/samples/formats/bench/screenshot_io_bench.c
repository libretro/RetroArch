/* Benchmark for the screenshot-task edge I/O strategy introduced by the
 * formats I/O decoupling work: whole-image in-memory encode followed by
 * a single bulk filestream_write_file() versus the historical pattern
 * of streaming the encode straight into the target file.
 *
 * The "old" modes faithfully replicate the pre-decoupling task flow so
 * the comparison stays honest as the tree moves on:
 *
 *   png_old : allocate a full-frame BGR24 buffer, flip-copy the source
 *             into it (what the scaler did for the no-resize case),
 *             then stream-encode into the target file.
 *   png_new : string-encode directly from the source with a negative
 *             stride (no copy), then one bulk write.
 *   bmp_old : header write plus one filestream_write() per row through
 *             an RFILE, matching the old rbmp_save_image().
 *   bmp_new : rbmp_save_image_string() plus one bulk write.
 *
 * Wall time per frame is reported; peak RSS (getrusage) is reported
 * for single-mode runs, where the process-wide peak is attributable
 * to one strategy.  Point
 * --out at slow or contended media (SD card, network mount, Android
 * SAF-backed storage) to measure the syscall-pattern difference the
 * redesign targets; on fast local filesystems stdio buffering hides
 * most of it and the delta is dominated by the removed flip-copy.
 *
 * Usage:
 *   screenshot_io_bench [--mode all|png_old|png_new|bmp_old|bmp_new]
 *                       [--pattern gradient|noise]
 *                       [--width N] [--height N] [--iters N]
 *                       [--out PATH]
 *
 * Build (see Makefile):
 *   make -C libretro-common/samples/formats/bench
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>

#include <libretro.h>
#include <formats/rpng.h>
#include <formats/rbmp.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>

static uint32_t prng_state = 0x1234567u;
static uint32_t prng(void)
{
   prng_state ^= prng_state << 13;
   prng_state ^= prng_state >> 17;
   prng_state ^= prng_state << 5;
   return prng_state;
}

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int run_png_old(const uint8_t *frame, unsigned w, unsigned h,
      const char *path)
{
   /* Old task flow: full-frame conversion buffer, flip-copy, then a
    * streamed encode holding the file handle for the whole run. */
   int ok           = 0;
   size_t frame_sz  = (size_t)w * h * 3;
   uint8_t *out     = (uint8_t*)malloc(frame_sz);
   unsigned j;

   if (!out)
      return 0;
   for (j = 0; j < h; j++)
      memcpy(out + (size_t)j * w * 3,
             frame + (size_t)(h - 1 - j) * w * 3, (size_t)w * 3);
   {
      intfstream_t *f = intfstream_open_file(path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (f)
      {
         ok = rpng_save_image_stream(out, f, w, h, (signed)(w * 3),
               3, NULL);
         intfstream_close(f);
         free(f);
      }
   }
   free(out);
   return ok;
}

static int run_png_new(const uint8_t *frame, unsigned w, unsigned h,
      const char *path)
{
   uint64_t n   = 0;
   int ok       = 0;
   uint8_t *buf = rpng_save_image_bgr24_string(
         frame + (size_t)(h - 1) * w * 3, w, h,
         -(signed)(w * 3), &n);
   if (buf)
   {
      ok = filestream_write_file(path, buf, (int64_t)n);
      free(buf);
   }
   return ok;
}

static int run_bmp_old(const uint8_t *frame, unsigned w, unsigned h,
      const char *path)
{
   /* Old rbmp_save_image(): header write plus one filestream_write per
    * row.  The header content is irrelevant to the I/O pattern being
    * measured, so a zeroed placeholder is written. */
   uint8_t hdr[54];
   unsigned j;
   RFILE *f = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!f)
      return 0;
   memset(hdr, 0, sizeof(hdr));
   hdr[0] = 'B';
   hdr[1] = 'M';
   filestream_write(f, hdr, sizeof(hdr));
   for (j = 0; j < h; j++)
      filestream_write(f, frame + (size_t)j * w * 3, (size_t)w * 3);
   filestream_close(f);
   return 1;
}

static int run_bmp_new(const uint8_t *frame, unsigned w, unsigned h,
      const char *path)
{
   size_t n     = 0;
   int ok       = 0;
   uint8_t *buf = rbmp_save_image_string(frame, w, h, w * 3,
         RBMP_SOURCE_TYPE_BGR24, &n);
   if (buf)
   {
      ok = filestream_write_file(path, buf, (int64_t)n);
      free(buf);
   }
   return ok;
}

typedef int (*bench_fn)(const uint8_t*, unsigned, unsigned, const char*);

static const struct
{
   const char *name;
   bench_fn fn;
} modes[] = {
   { "png_old", run_png_old },
   { "png_new", run_png_new },
   { "bmp_old", run_bmp_old },
   { "bmp_new", run_bmp_new },
};

int main(int argc, char **argv)
{
   const char *mode    = "all";
   const char *pattern = "gradient";
   const char *path    = "/tmp/screenshot_io_bench.img";
   unsigned w          = 3840;
   unsigned h          = 2160;
   int iters           = 3;
   size_t frame_sz;
   uint8_t *frame;
   size_t i;
   int a;

   for (a = 1; a + 1 < argc; a += 2)
   {
      if (!strcmp(argv[a], "--mode"))
         mode = argv[a + 1];
      else if (!strcmp(argv[a], "--pattern"))
         pattern = argv[a + 1];
      else if (!strcmp(argv[a], "--width"))
         w = (unsigned)atoi(argv[a + 1]);
      else if (!strcmp(argv[a], "--height"))
         h = (unsigned)atoi(argv[a + 1]);
      else if (!strcmp(argv[a], "--iters"))
         iters = atoi(argv[a + 1]);
      else if (!strcmp(argv[a], "--out"))
         path = argv[a + 1];
      else
      {
         fprintf(stderr, "unknown option %s\n", argv[a]);
         return 1;
      }
   }
   if (!w || !h || iters < 1)
   {
      fprintf(stderr, "invalid dimensions or iteration count\n");
      return 1;
   }

   frame_sz = (size_t)w * h * 3;
   frame    = (uint8_t*)malloc(frame_sz);
   if (!frame)
   {
      fprintf(stderr, "frame allocation failed\n");
      return 1;
   }
   if (!strcmp(pattern, "noise"))
      for (i = 0; i < frame_sz; i++)
         frame[i] = (uint8_t)prng();
   else
      for (i = 0; i < frame_sz; i++)
         frame[i] = (uint8_t)((i / 3) & 0xFF);

   printf("%ux%u %s, %d iteration(s), out=%s\n",
         w, h, pattern, iters, path);

   for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
   {
      double total = 0.0;
      double best  = 0.0;
      int it;

      if (strcmp(mode, "all") && strcmp(mode, modes[i].name))
         continue;

      for (it = 0; it < iters; it++)
      {
         double t0 = now_ms();
         double dt;
         if (!modes[i].fn(frame, w, h, path))
         {
            fprintf(stderr, "%s: run failed\n", modes[i].name);
            free(frame);
            return 1;
         }
         dt     = now_ms() - t0;
         total += dt;
         if (it == 0 || dt < best)
            best = dt;
      }
      if (!strcmp(mode, "all"))
         printf("%-8s avg %8.1f ms  best %8.1f ms\n",
               modes[i].name, total / iters, best);
      else
      {
         /* getrusage peaks are process-wide, so RSS is only meaningful
          * when a single mode ran in this process. */
         struct rusage ru;
         getrusage(RUSAGE_SELF, &ru);
         printf("%-8s avg %8.1f ms  best %8.1f ms  peakRSS %7.1f MiB\n",
               modes[i].name, total / iters, best,
               ru.ru_maxrss / 1024.0);
      }
   }

   filestream_delete(path);
   free(frame);
   return 0;
}
