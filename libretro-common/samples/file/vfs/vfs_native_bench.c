/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_native_bench.c).
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

/* Read-only stream throughput through filestream: the C runtime path
 * (vfs_native_bench) against VFS_WIN32_NATIVE (vfs_native_bench_native),
 * the same source built twice. On anything but Windows the two are the
 * same build.
 *
 *   sequential reads of 1, 16, 256, 4096 and 65536 bytes, and 1 MiB
 *   4 KiB at random offsets by seek and read, and by filestream_read_at
 *   4 KiB at random offsets by filestream_read_at from 4 threads
 *   open, read 4 KiB, close, over 1000 small files
 *
 * Each case runs five times and reports the best, in ns per call and
 * MB/s; the file is read once first so every case runs from the cache.
 * Usage: vfs_native_bench [file]   (default: makes a 64 MiB one here) */

#include <retro_posix_source.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/file_stream.h>
#include <rthreads/rthreads.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#define FILE_BYTES  (64u << 20)
#define RUNS        5
#define RANDOM_OPS  20000
#define SMALL_FILES 1000

static double now_ns(void)
{
#ifdef _WIN32
   static LARGE_INTEGER f;
   LARGE_INTEGER        c;
   if (!f.QuadPart)
      QueryPerformanceFrequency(&f);
   QueryPerformanceCounter(&c);
   return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
#else
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
#endif
}

static uint8_t *buf;
static int64_t  size;

static void report(const char *what, double ns, double calls, double bytes)
{
   printf("%-34s %10.1f ns/call %9.1f MB/s\n", what, ns / calls,
         bytes / (ns / 1e9) / 1e6);
}

static void sequential(const char *path, int64_t chunk)
{
   double  best = 0, bytes = 0, calls = 0;
   int     r;
   char    what[64];
   int64_t limit = chunk < 256 ? (int64_t)(4u << 20) : size;
   for (r = 0; r < RUNS; r++)
   {
      RFILE  *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      int64_t done = 0, got;
      double  t0, t;
      unsigned n = 0;
      if (!f)
         return;
      t0 = now_ns();
      while (done < limit && (got = filestream_read(f, buf, chunk)) > 0)
      {
         done += got;
         n++;
      }
      t = now_ns() - t0;
      filestream_close(f);
      if (!r || t < best)
      {
         best  = t;
         bytes = (double)done;
         calls = (double)n;
      }
   }
   snprintf(what, sizeof(what), "sequential %lld B", (long long)chunk);
   report(what, best, calls, bytes);
}

static void random_reads(const char *path, int at)
{
   double best = 0;
   int    r;
   for (r = 0; r < RUNS; r++)
   {
      RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      unsigned x = 12345, k;
      double   t0, t;
      if (!f)
         return;
      t0 = now_ns();
      for (k = 0; k < RANDOM_OPS; k++)
      {
         int64_t off;
         x   = x * 1103515245u + 12345u;
         off = (int64_t)((x >> 4) % (uint32_t)(size - 4096));
         if (at)
         {
            if (filestream_read_at(f, buf, 4096, off) != 4096)
               break;
         }
         else
         {
            filestream_seek(f, off, RETRO_VFS_SEEK_POSITION_START);
            if (filestream_read(f, buf, 4096) != 4096)
               break;
         }
      }
      t = now_ns() - t0;
      filestream_close(f);
      if (!r || t < best)
         best = t;
   }
   report(at ? "random 4 KiB, read_at" : "random 4 KiB, seek+read", best,
         RANDOM_OPS, (double)RANDOM_OPS * 4096);
}

struct worker
{
   RFILE   *f;
   unsigned seed;
};

static void work(void *ud)
{
   struct worker *w = (struct worker*)ud;
   uint8_t  b[4096];
   unsigned x = w->seed, k;
   for (k = 0; k < RANDOM_OPS / 4; k++)
   {
      x = x * 1103515245u + 12345u;
      filestream_read_at(w->f, b, sizeof(b),
            (int64_t)((x >> 4) % (uint32_t)(size - 4096)));
   }
}

static void threaded_reads(const char *path)
{
   double best = 0;
   int    r, i;
   RFILE *f    = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!f || filestream_read_at(f, buf, 1, 0) != 1)
   {
      printf("%-34s no reads at offsets here\n", "random 4 KiB, read_at x4");
      filestream_close(f);
      return;
   }
   for (r = 0; r < RUNS; r++)
   {
      sthread_t    *t[4];
      struct worker w[4];
      double        t0 = now_ns(), dt;
      for (i = 0; i < 4; i++)
      {
         w[i].f    = f;
         w[i].seed = 7u + 977u * (unsigned)i;
         t[i]      = sthread_create(work, &w[i]);
      }
      for (i = 0; i < 4; i++)
         if (t[i])
            sthread_join(t[i]);
      dt = now_ns() - t0;
      if (!r || dt < best)
         best = dt;
   }
   filestream_close(f);
   report("random 4 KiB, read_at x4 threads", best, RANDOM_OPS,
         (double)RANDOM_OPS * 4096);
}

static void small_files(void)
{
   char     name[64];
   unsigned i;
   double   best = 0;
   int      r;
   for (i = 0; i < SMALL_FILES; i++)
   {
      snprintf(name, sizeof(name), "vfs_bench_small_%04u.bin", i);
      filestream_write_file(name, buf, 4096);
   }
   for (r = 0; r < RUNS; r++)
   {
      double t0 = now_ns(), t;
      for (i = 0; i < SMALL_FILES; i++)
      {
         RFILE *f;
         snprintf(name, sizeof(name), "vfs_bench_small_%04u.bin", i);
         if ((f = filestream_open(name, RETRO_VFS_FILE_ACCESS_READ,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            filestream_read(f, buf, 4096);
            filestream_close(f);
         }
      }
      t = now_ns() - t0;
      if (!r || t < best)
         best = t;
   }
   for (i = 0; i < SMALL_FILES; i++)
   {
      snprintf(name, sizeof(name), "vfs_bench_small_%04u.bin", i);
      remove(name);
   }
   report("open + read 4 KiB + close", best, SMALL_FILES,
         (double)SMALL_FILES * 4096);
}

int main(int argc, char **argv)
{
   const char *path = argc > 1 ? argv[1] : "vfs_bench_64m.bin";
   int         made = 0;
   RFILE      *f;
   if (!(buf = (uint8_t*)malloc(1u << 20)))
      return 1;
   memset(buf, 0x5A, 1u << 20);
   if (argc < 2)
   {
      unsigned i;
      if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         return 1;
      for (i = 0; i < FILE_BYTES >> 20; i++)
         filestream_write(f, buf, 1u << 20);
      filestream_close(f);
      made = 1;
   }
   if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return 1;
   size = filestream_get_size(f);
   while (filestream_read(f, buf, 1u << 20) > 0) { }
   filestream_close(f);
   if (size < 8192)
      return 1;

#ifdef VFS_WIN32_NATIVE
   printf("VFS_WIN32_NATIVE, %lld bytes\n", (long long)size);
#else
   printf("C runtime streams, %lld bytes\n", (long long)size);
#endif
   sequential(path, 1);
   sequential(path, 16);
   sequential(path, 256);
   sequential(path, 4096);
   sequential(path, 65536);
   sequential(path, 1 << 20);
   random_reads(path, 0);
   random_reads(path, 1);
   threaded_reads(path);
   small_files();

   if (made)
      remove(path);
   free(buf);
   return 0;
}
