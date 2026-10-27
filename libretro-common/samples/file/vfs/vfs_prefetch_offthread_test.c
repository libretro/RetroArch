/* Mapped read-ahead never makes the reader wait (Windows).
 *
 * PrefetchVirtualMemory is not documented to return before its reads
 * complete. This build points the call at a stand-in that sleeps the
 * way a cold disk would, then reads a mapped file through in order:
 *
 *    - no read waits on the stand-in, and it never runs on the reader;
 *    - each hint starts past the read that made it, not on it;
 *    - no byte of the file is asked for twice.
 *
 * Elsewhere there is nothing to check and the test passes.
 *
 * Build:  make CC=x86_64-w64-mingw32-gcc MMAP=0 vfs_prefetch_offthread_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>

#ifdef _WIN32
#include <windows.h>

#include "prefetch_hook.h"

#define FIXTURE_LEN (16u * 1024u * 1024u)
#define CHUNK       (64u * 1024u)
#define SLOW_MS     200
/* far under SLOW_MS, far over a 64 KiB copy */
#define READ_LIMIT_MS 100

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
   } while (0)

static volatile LONG     hook_calls;
static volatile LONG     hook_on_reader;
static DWORD             reader_thread;
static volatile LONG64   hook_bytes;
static uint8_t *volatile hook_first;

int vfs_prefetch_test_hook(void *addr, size_t len)
{
   if (InterlockedIncrement(&hook_calls) == 1)
      hook_first = (uint8_t*)addr;
   if (GetCurrentThreadId() == reader_thread)
      InterlockedIncrement(&hook_on_reader);
   InterlockedExchangeAdd64(&hook_bytes, (LONG64)len);
   Sleep(SLOW_MS);
   return 1;
}

static bool write_fixture(const char *path)
{
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint8_t *buf;
   size_t   i;
   bool     ok;

   if (!f)
      return false;
   if (!(buf = (uint8_t*)malloc(FIXTURE_LEN)))
   {
      filestream_close(f);
      return false;
   }
   for (i = 0; i < FIXTURE_LEN; i++)
      buf[i] = (uint8_t)(i * 31u);
   ok = filestream_write(f, buf, FIXTURE_LEN) == FIXTURE_LEN;
   free(buf);
   filestream_close(f);
   return ok;
}

int main(void)
{
   const char    *path = "vfs_prefetch_offthread.bin";
   RFILE         *f;
   const uint8_t *map;
   int64_t        maplen = 0;
   uint8_t       *buf    = (uint8_t*)malloc(CHUNK);
   LARGE_INTEGER  freq, t0, t1;
   double         worst  = 0.0;
   uint64_t       off;
   int            i;

   if (!buf || !write_fixture(path))
   {
      printf("FAIL: cannot write %s\n", path);
      return 1;
   }
   reader_thread = GetCurrentThreadId();
   QueryPerformanceFrequency(&freq);

   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   CHECK(f != NULL, "open");
   map = f ? filestream_get_mapped_ptr(f, &maplen) : NULL;
   CHECK(map != NULL && maplen == FIXTURE_LEN, "mapped");
   if (map)
   {
      for (off = 0; off + CHUNK <= FIXTURE_LEN; off += CHUNK)
      {
         double ms;
         QueryPerformanceCounter(&t0);
         if (filestream_read(f, buf, CHUNK) != (int64_t)CHUNK
               || buf[CHUNK - 1] != (uint8_t)((off + CHUNK - 1) * 31u))
         {
            CHECK(0, "read back");
            break;
         }
         QueryPerformanceCounter(&t1);
         ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0
            / (double)freq.QuadPart;
         if (ms > worst)
            worst = ms;
      }
      /* the thread runs on its own: wait until it has gone a few of
       * its sleeps without starting another */
      {
         LONG seen = -1;
         for (i = 0; i < 40 && seen != hook_calls; i++)
         {
            seen = hook_calls;
            Sleep(SLOW_MS * 3);
         }
      }
      printf("hints: %ld, %lld bytes, slowest read %.2f ms\n",
            (long)hook_calls, (long long)hook_bytes, worst);
      CHECK(hook_calls > 1, "a sequential reader is read ahead of");
      CHECK(hook_on_reader == 0, "the prefetch ran on the reader's thread");
      CHECK(worst < READ_LIMIT_MS, "a read waited on the prefetch");
      CHECK(hook_first >= map + CHUNK,
            "the first hint covers the read that made it");
   }
   if (f)
      CHECK(filestream_close(f) == 0, "close");
   /* the whole file, a unit's rounding at most, never twice */
   CHECK((uint64_t)hook_bytes <= FIXTURE_LEN, "a byte was asked for twice");
   filestream_delete(path);
   free(buf);

   if (test_fails)
   {
      printf("%d check(s) failed\n", test_fails);
      return 1;
   }
   printf("PASS\n");
   return 0;
}
#else
int main(void)
{
   printf("SKIP: Windows read-ahead only\nPASS\n");
   return 0;
}
#endif
