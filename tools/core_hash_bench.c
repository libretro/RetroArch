/* core_hash_bench: how long a core's first-load SHA-256 takes, read into
 * a buffer (as core_hash_step does) against hashed straight from a
 * mapping of the file.
 *
 * Standalone timing for the core signature check; nothing here runs in
 * RetroArch.  The file is hashed warm (in the page cache), which is the
 * case a mapping can win: cold, both ways wait on the same disk reads.
 *
 *   cc -O2 -I../libretro-common/include core_hash_bench.c \
 *      ../libretro-common/hash/lrc_hash.c \
 *      ../libretro-common/features/features_cpu.c \
 *      ../libretro-common/streams/file_stream.c \
 *      ../libretro-common/vfs/vfs_implementation.c \
 *      ../libretro-common/file/file_path.c \
 *      ../libretro-common/string/stdstring.c \
 *      ../libretro-common/compat/compat_strl.c \
 *      ../libretro-common/encodings/encoding_utf.c \
 *      -o core_hash_bench
 *   ./core_hash_bench some_core_libretro.so [passes]
 *
 * Every way prints the digest so a mismatch shows at once. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <lrc_hash.h>
#include <features/features_cpu.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#define STEP (64 * 1024)

typedef struct
{
#ifdef _WIN32
   HANDLE h;
#else
   int fd;
#endif
   uint64_t size;
} bench_file_t;

static int bench_open(bench_file_t *f, const char *path)
{
#ifdef _WIN32
   LARGE_INTEGER sz;
   f->h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   if (f->h == INVALID_HANDLE_VALUE || !GetFileSizeEx(f->h, &sz))
      return 0;
   f->size = (uint64_t)sz.QuadPart;
#else
   struct stat st;
   if ((f->fd = open(path, O_RDONLY)) < 0 || fstat(f->fd, &st) != 0)
      return 0;
   f->size = (uint64_t)st.st_size;
#endif
   return 1;
}

static void bench_close(bench_file_t *f)
{
#ifdef _WIN32
   CloseHandle(f->h);
#else
   close(f->fd);
#endif
}

static void bench_rewind(bench_file_t *f)
{
#ifdef _WIN32
   LARGE_INTEGER z;
   z.QuadPart = 0;
   SetFilePointerEx(f->h, z, NULL, FILE_BEGIN);
#else
   lseek(f->fd, 0, SEEK_SET);
#endif
}

static int64_t bench_read(bench_file_t *f, void *buf, size_t len)
{
#ifdef _WIN32
   DWORD got = 0;
   if (!ReadFile(f->h, buf, (DWORD)len, &got, NULL))
      return -1;
   return got;
#else
   return read(f->fd, buf, len);
#endif
}

/* As core_hash_step: chunk-sized reads into one heap buffer */
static int hash_read(bench_file_t *f, size_t chunk, uint8_t *out)
{
   struct sha256_state sha;
   uint8_t *buf = (uint8_t*)malloc(chunk);
   int64_t got;
   if (!buf)
      return 0;
   bench_rewind(f);
   sha256_stream_init(&sha, 0);
   while ((got = bench_read(f, buf, chunk)) > 0)
      sha256_stream_update(&sha, buf, (size_t)got);
   sha256_stream_final(&sha, out);
   free(buf);
   return got == 0;
}

/* Hashed from a mapping in STEP slices, as a sliced task would walk it */
static int hash_map(bench_file_t *f, int populate, uint8_t *out)
{
   struct sha256_state sha;
   const uint8_t *p;
   uint64_t off;
#ifdef _WIN32
   HANDLE m = CreateFileMappingA(f->h, NULL, PAGE_READONLY, 0, 0, NULL);
   (void)populate;
   if (!m)
      return 0;
   p = (const uint8_t*)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
   if (!p)
   {
      CloseHandle(m);
      return 0;
   }
#else
   int flags = MAP_PRIVATE;
#ifdef MAP_POPULATE
   if (populate)
      flags |= MAP_POPULATE;
#endif
   p = (const uint8_t*)mmap(NULL, (size_t)f->size, PROT_READ, flags,
         f->fd, 0);
   if (p == (const uint8_t*)MAP_FAILED)
      return 0;
   if (!populate)
      madvise((void*)p, (size_t)f->size, MADV_SEQUENTIAL);
#endif
   sha256_stream_init(&sha, 0);
   for (off = 0; off < f->size; off += STEP)
   {
      uint64_t n = f->size - off;
      sha256_stream_update(&sha, p + off, (size_t)(n < STEP ? n : STEP));
   }
   sha256_stream_final(&sha, out);
#ifdef _WIN32
   UnmapViewOfFile(p);
   CloseHandle(m);
#else
   munmap((void*)p, (size_t)f->size);
#endif
   return 1;
}

static void hex(const uint8_t *d, char *s)
{
   int i;
   for (i = 0; i < 32; i++)
      sprintf(s + i * 2, "%02x", d[i]);
}

int main(int argc, char **argv)
{
   static const char *names[4] = { "read 64 KiB", "read 1 MiB",
      "map", "map + populate" };
   double best[4];
   char want[65], got[65];
   bench_file_t f;
   int passes = argc > 2 ? atoi(argv[2]) : 9;
   int way, pass;
   if (argc < 2 || !bench_open(&f, argv[1]))
   {
      fprintf(stderr, "usage: %s FILE [passes]\n", argv[0]);
      return 1;
   }
   want[0] = '\0';
   for (way = 0; way < 4; way++)
      best[way] = 1e30;
   /* Warm the cache, then interleave the ways so drift hits them all */
   for (pass = 0; pass <= passes; pass++)
      for (way = 0; way < 4; way++)
      {
         uint8_t d[32];
         retro_time_t t0 = cpu_features_get_time_usec();
         int ok = way == 0 ? hash_read(&f, STEP, d)
               : way == 1 ? hash_read(&f, 1024 * 1024, d)
               : hash_map(&f, way == 3, d);
         double ms = (double)(cpu_features_get_time_usec() - t0) / 1000.0;
         if (!ok)
         {
            fprintf(stderr, "%s failed\n", names[way]);
            return 1;
         }
         hex(d, got);
         if (!want[0])
            strcpy(want, got);
         else if (strcmp(want, got))
         {
            fprintf(stderr, "%s: digest mismatch\n", names[way]);
            return 1;
         }
         if (pass && ms < best[way])
            best[way] = ms;
      }
   printf("%s: %.1f MiB, sha256 %s\n", argv[1],
         (double)f.size / (1024.0 * 1024.0), want);
   for (way = 0; way < 4; way++)
      printf("  %-15s %8.2f ms  %7.0f MiB/s\n", names[way], best[way],
            (double)f.size / (1024.0 * 1024.0) / (best[way] / 1000.0));
   bench_close(&f);
   return 0;
}
