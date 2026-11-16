/* Positional I/O on a VFS stream (VFS API v6: retro_vfs_file_read_at_impl,
 * retro_vfs_file_write_at_impl, filestream_read_at, filestream_write_at):
 * a read or write at an offset given with the call, which leaves the
 * stream's own position alone, so several threads share one stream with
 * no lock.
 *
 * - four threads read at offsets all over one stream at once, while the
 *   main thread reads it front to back with the ordinary read: every byte
 *   read either way is the file's byte at that place - no read lands
 *   where another thread's seek left the position, which is what a seek
 *   and a read per thread does without a lock;
 * - the same on a buffered stream, a descriptor stream
 *   (SEQUENTIAL_BULK) and a mapped one (FREQUENT_ACCESS);
 * - a read at or past the end is 0 bytes, one across it is short;
 * - four threads append records to one stream at once: the file holds
 *   every record, whole, each thread's in the order it wrote them;
 * - a write at an offset lands there and nowhere else;
 * - a stream without positional I/O says -1 (a NULL one here).
 *
 * Where the platform has no positional I/O at all the calls answer -1
 * and the test says so and passes: the caller's fallback is the seek
 * and read it had before. Built with VFS_POSITIONAL_TEST_REQUIRED on a
 * Unix host, which always has pread(), or on Windows NT, which always
 * has a second handle, a -1 there fails instead - the build without
 * HAVE_MMAP (vfs_positional_nommap_test) holds that as Android builds,
 * vfs_positional_pathreopen_test the path reopen,
 * vfs_positional_ntverify_test the handle opened relative to the
 * stream's, checked against it as NT before Vista has it checked, and
 * vfs_positional_wide_test twelve threads at once - more than a
 * Windows stream keeps events for, so some calls make their own. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <libretro.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>
#include <retro_atomic.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE test_thread_t;
#define THREAD_RET DWORD WINAPI
#define THREAD_ARG LPVOID
static void thread_start(test_thread_t *t, LPTHREAD_START_ROUTINE fn, void *arg)
{
   *t = CreateThread(NULL, 0, fn, arg, 0, NULL);
}
static void thread_join(test_thread_t t)
{
   WaitForSingleObject(t, INFINITE);
   CloseHandle(t);
}
#else
#include <pthread.h>
typedef pthread_t test_thread_t;
#define THREAD_RET void *
#define THREAD_ARG void *
static void thread_start(test_thread_t *t, void *(*fn)(void*), void *arg)
{
   pthread_create(t, NULL, fn, arg);
}
static void thread_join(test_thread_t t)
{
   pthread_join(t, NULL);
}
#endif

#define FILE_SIZE  (4u << 20)
#ifndef THREADS
#define THREADS    4
#endif
#define READS      20000
#define RECORDS    5000
#define RECORD_LEN 64

static int failures;

#if defined(VFS_POSITIONAL_TEST_REQUIRED) && (defined(_WIN32) \
   || defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))
#define POSITIONAL_REQUIRED 1
#else
#define POSITIONAL_REQUIRED 0
#endif
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* The file's byte at @at */
static uint8_t pattern(uint32_t at)
{
   return (uint8_t)((at * 2654435761u) >> 24);
}

static libretro_vfs_implementation_file *shared;
static retro_atomic_int_t wrong_reads;

static THREAD_RET reader(THREAD_ARG arg)
{
   uint32_t x = (uint32_t)(uintptr_t)arg * 2246822519u + 1;
   uint8_t buf[512];
   int i;
   for (i = 0; i < READS; i++)
   {
      uint32_t at, len, k;
      int64_t got;
      x   = x * 1103515245u + 12345u;
      at  = (x >> 8) % (FILE_SIZE - sizeof(buf));
      len = 1 + (x >> 4) % sizeof(buf);
      got = retro_vfs_file_read_at_impl(shared, buf, len, at);
      if (got != (int64_t)len)
      {
         retro_atomic_fetch_add_int(&wrong_reads, 1);
         continue;
      }
      for (k = 0; k < len; k++)
         if (buf[k] != pattern(at + k))
         {
            retro_atomic_fetch_add_int(&wrong_reads, 1);
            break;
         }
   }
   return 0;
}

static THREAD_RET appender(THREAD_ARG arg)
{
   uint8_t rec[RECORD_LEN];
   uint32_t id = (uint32_t)(uintptr_t)arg;
   uint32_t i;
   for (i = 0; i < RECORDS; i++)
   {
      memset(rec, (int)(id * 16 + (i & 15)), sizeof(rec));
      memcpy(rec, &id, 4);
      memcpy(rec + 4, &i, 4);
      if (retro_vfs_file_write_at_impl(shared, rec, sizeof(rec),
               RETRO_VFS_WRITE_AT_APPEND) != (int64_t)sizeof(rec))
         retro_atomic_fetch_add_int(&wrong_reads, 1);
   }
   return 0;
}

static int read_shared(const char *path, unsigned hints, const char *what)
{
   test_thread_t t[THREADS];
   uint8_t buf[4096];
   uint8_t one;
   uint32_t at = 0, wrong_seq = 0;
   int i;

   shared = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ, hints);
   CHECK(shared != NULL);
   if (!shared)
      return 0;
   if (retro_vfs_file_read_at_impl(shared, &one, 1, 0) < 0)
   {
      printf("vfs positional: no positional I/O for a %s stream here, skipped\n", what);
      CHECK(!POSITIONAL_REQUIRED);
      retro_vfs_file_close_impl(shared);
      return 0;
   }

   retro_atomic_int_init(&wrong_reads, 0);
   for (i = 0; i < THREADS; i++)
      thread_start(&t[i], reader, (void*)(uintptr_t)(i + 1));
   /* Front to back with the ordinary read, the others reading meanwhile */
   while (at < FILE_SIZE)
   {
      int64_t got = retro_vfs_file_read_impl(shared, buf, sizeof(buf));
      int64_t k;
      if (got <= 0)
         break;
      for (k = 0; k < got; k++)
         if (buf[k] != pattern(at + (uint32_t)k))
         {
            wrong_seq++;
            break;
         }
      at += (uint32_t)got;
   }
   for (i = 0; i < THREADS; i++)
      thread_join(t[i]);
   CHECK(at == FILE_SIZE);
   CHECK(wrong_seq == 0);
   CHECK(retro_atomic_load_acquire_int(&wrong_reads) == 0);

   /* The end */
   CHECK(retro_vfs_file_read_at_impl(shared, buf, 16, FILE_SIZE) == 0);
   CHECK(retro_vfs_file_read_at_impl(shared, buf, 16, FILE_SIZE + 100) == 0);
   CHECK(retro_vfs_file_read_at_impl(shared, buf, 16, FILE_SIZE - 5) == 5);
   CHECK(retro_vfs_file_read_at_impl(shared, buf, 16, -1) == -1);

   printf("vfs positional: %s stream: %d reads at offsets on %d threads, %d wrong; %u sequential blocks wrong\n",
         what, READS * THREADS, THREADS, retro_atomic_load_acquire_int(&wrong_reads), wrong_seq);
   retro_vfs_file_close_impl(shared);
   return 1;
}

int main(void)
{
   static const char path[]   = "vfs_positional.bin";
   static const char path_w[] = "vfs_positional_append.bin";
   uint8_t *whole = (uint8_t*)malloc(FILE_SIZE);
   uint8_t one;
   uint32_t i;
   int had;
   RFILE *f;

   if (!whole)
      return 1;
   for (i = 0; i < FILE_SIZE; i++)
      whole[i] = pattern(i);
   CHECK(filestream_write_file(path, whole, FILE_SIZE));

   had  = read_shared(path, RETRO_VFS_FILE_ACCESS_HINT_NONE, "buffered");
   had |= read_shared(path, RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK, "descriptor");
   had |= read_shared(path, RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS, "mapped");

   CHECK(retro_vfs_file_read_at_impl(NULL, &one, 1, 0) == -1);
   CHECK(retro_vfs_file_write_at_impl(NULL, &one, 1, 0) == -1);

   if (had)
   {
      /* Appends from four threads */
      test_thread_t t[THREADS];
      uint32_t next[THREADS + 1];
      uint32_t torn = 0, out_of_order = 0;
      int64_t size;
      int k;

      shared = retro_vfs_file_open_impl(path_w, RETRO_VFS_FILE_ACCESS_READ_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(shared != NULL);
      retro_atomic_int_init(&wrong_reads, 0);
      for (k = 0; k < THREADS; k++)
         thread_start(&t[k], appender, (void*)(uintptr_t)(k + 1));
      for (k = 0; k < THREADS; k++)
         thread_join(t[k]);
      CHECK(retro_atomic_load_acquire_int(&wrong_reads) == 0);

      /* A write at an offset: there and nowhere else */
      memset(whole, 0xAB, 16);
      CHECK(retro_vfs_file_write_at_impl(shared, whole, 16, 8) == 16);
      retro_vfs_file_close_impl(shared);

      f    = filestream_open(path_w, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      size = f ? filestream_get_size(f) : -1;
      CHECK(size == (int64_t)THREADS * RECORDS * RECORD_LEN);
      memset(next, 0, sizeof(next));
      for (i = 0; f && i < THREADS * RECORDS; i++)
      {
         uint8_t rec[RECORD_LEN];
         uint32_t id, seq, b;
         if (filestream_read_at(f, rec, sizeof(rec), (int64_t)i * RECORD_LEN) != (int64_t)sizeof(rec))
         {
            torn++;
            continue;
         }
         if (i == 0)
         {
            /* The sixteen bytes written over the first record */
            for (b = 8; b < 24; b++)
               if (rec[b] != 0xAB)
                  torn++;
            continue;
         }
         memcpy(&id, rec, 4);
         memcpy(&seq, rec + 4, 4);
         if (id < 1 || id > THREADS)
         {
            torn++;
            continue;
         }
         for (b = 8; b < RECORD_LEN; b++)
            if (rec[b] != (uint8_t)(id * 16 + (seq & 15)))
            {
               torn++;
               break;
            }
         if (seq < next[id])
            out_of_order++;
         next[id] = seq + 1;
      }
      if (f)
         filestream_close(f);
      CHECK(torn == 0);
      CHECK(out_of_order == 0);
      printf("vfs positional: %d records appended by %d threads: %u torn, %u out of a thread's order\n",
            THREADS * RECORDS, THREADS, torn, out_of_order);
   }

   remove(path);
   remove(path_w);
   free(whole);
   printf("vfs positional: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
