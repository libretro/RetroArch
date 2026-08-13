/* Contract test for whole-file reads
 * (filestream_read_file(), libretro-common/streams/file_stream.c) and
 * for the descriptor path that
 * RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK selects in
 * retro_vfs_file_open_impl().
 *
 * Two things are under test here, and they have to land together.
 *
 * 1. The hint.  Before it, RFILE_HINT_UNBUFFERED - the flag that puts
 *    a stream on open()/read()/close() with no stdio in between - was
 *    only ever set inside '#ifdef VFS_HAVE_FILE_MAPPING', so every
 *    target without mmap or Win32 file mappings was pinned to
 *    buffered stdio no matter what the caller asked for.  On newlib
 *    (Vita, 3DS, Wii U, Switch) fread() on a buffered stream refills
 *    through the stream buffer and memcpy()s each refill out of it,
 *    so a whole-file read cost one read() per buffer-full plus a
 *    second copy of every byte.
 *
 * 2. The loop.  filestream_read_file() issued exactly one
 *    filestream_read() for the whole file and treated the count it
 *    got back as the answer.  fread() loops internally, so on the
 *    buffered path the count was always complete and the missing loop
 *    never showed.  read(2) has no such obligation, and neither does
 *    a frontend-supplied VFS: either may return fewer bytes and
 *    expect to be asked again.  Routing this function onto the
 *    descriptor path without adding the loop would turn a latent bug
 *    into a live one - silently truncated fonts, shaders and ROMs.
 *
 * The short-read cases below run through a wrapper VFS installed with
 * filestream_vfs_init(), because a real read(2) on a local file will
 * not come up short on demand.  test_short_reads() is the one that
 * discriminates: pre-patch it reports 7 bytes for a 1 MiB file.
 *
 * A truncated *file* - one that shrank between the size query and the
 * read - is not an error and must still succeed with the byte count
 * that was actually there; test_file_shrank() pins that, since the
 * obvious way to write the loop (demand the full count or fail) would
 * regress it.
 *
 * Run under -fsanitize=address,undefined (with leak detection on) and
 * under -fsanitize=thread; the error-injection case exists partly to
 * prove the failure path frees its buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/file_stream.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>

/* Whole-file reads are issued from several task threads at once in
 * the frontend, so the concurrency case below is worth running
 * wherever threads exist - not only where pthreads do.  Windows gets
 * the same test through _beginthreadex. */
#if defined(_WIN32)
#include <windows.h>
#include <process.h>
#define BULK_TEST_THREADS 1
typedef HANDLE          bulk_thread_t;
typedef unsigned        bulk_thread_ret_t;
#define BULK_THREAD_CALL __stdcall
#define BULK_THREAD_DONE 0
#elif defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#define BULK_TEST_THREADS 1
typedef pthread_t       bulk_thread_t;
typedef void           *bulk_thread_ret_t;
#define BULK_THREAD_CALL
#define BULK_THREAD_DONE NULL
#endif

static int failures = 0;
static int checks   = 0;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         failures++; \
         printf("  FAIL: "); \
         printf(__VA_ARGS__); \
         printf("\n"); \
      } \
   } while (0)

/* ------------------------------------------------------------------ */
/* Fixtures                                                           */
/* ------------------------------------------------------------------ */

/* Deterministic, position-dependent bytes: a buffer assembled out of
 * order, or one whose tail was never written, does not compare equal
 * by accident the way a constant fill can. */
static uint8_t fixture_byte(int64_t i)
{
   return (uint8_t)((i * 31u) ^ (i >> 8) ^ 0xA5u);
}

static int fixture_write(const char *path, int64_t size)
{
   int64_t  i;
   FILE    *f = fopen(path, "wb");

   if (!f)
      return 0;

   for (i = 0; i < size; i++)
   {
      if (fputc(fixture_byte(i), f) == EOF)
      {
         fclose(f);
         return 0;
      }
   }

   return fclose(f) == 0;
}

static int fixture_matches(const uint8_t *buf, int64_t size)
{
   int64_t i;

   for (i = 0; i < size; i++)
      if (buf[i] != fixture_byte(i))
         return 0;

   return 1;
}

/* ------------------------------------------------------------------ */
/* Wrapper VFS: delegates everything to the built-in implementation,  */
/* but can make read() behave the way a legal-but-awkward backend     */
/* would.                                                             */
/* ------------------------------------------------------------------ */

static int64_t wrap_size       = -1; /* override the reported size     */
static int64_t wrap_chunk      = 0;  /* cap per read call; 0 = no cap  */
static int64_t wrap_stop_after = -1; /* deliver N bytes, then EOF      */
static int     wrap_fail_at    = -1; /* return -1 on the Nth call      */
static int64_t wrap_delivered  = 0;
static int     wrap_calls      = 0;

static void wrap_reset(void)
{
   wrap_size       = -1;
   wrap_chunk      = 0;
   wrap_stop_after = -1;
   wrap_fail_at    = -1;
   wrap_delivered  = 0;
   wrap_calls      = 0;
}

static int64_t wrap_read(struct retro_vfs_file_handle *stream,
      void *s, uint64_t len)
{
   int64_t got;

   if (wrap_fail_at >= 0 && wrap_calls == wrap_fail_at)
   {
      wrap_calls++;
      return -1;
   }
   wrap_calls++;

   if (wrap_stop_after >= 0)
   {
      if (wrap_delivered >= wrap_stop_after)
         return 0;
      if ((int64_t)len > wrap_stop_after - wrap_delivered)
         len = (uint64_t)(wrap_stop_after - wrap_delivered);
   }

   if (wrap_chunk > 0 && (int64_t)len > wrap_chunk)
      len = (uint64_t)wrap_chunk;

   got = retro_vfs_file_read_impl(
         (libretro_vfs_implementation_file*)stream, s, len);

   if (got > 0)
      wrap_delivered += got;

   return got;
}

/* Report whatever size the test asked for.  Platforms disagree about
 * what a directory's length is - Linux says INT64_MAX, others report
 * a small plausible number or zero - and this is how each of those
 * answers gets reproduced on whichever machine is running. */
static int64_t wrap_size_cb(struct retro_vfs_file_handle *stream)
{
   if (wrap_size >= 0)
      return wrap_size;
   return retro_vfs_file_size_impl(
         (libretro_vfs_implementation_file*)stream);
}

static struct retro_vfs_interface wrap_iface;

static void wrap_install(void)
{
   struct retro_vfs_interface_info info;

   /* The implementation functions are declared in terms of
    * libretro_vfs_implementation_file, which is the same struct as
    * retro_vfs_file_handle under a different name depending on
    * VFS_FRONTEND; the frontend builds this file with that defined
    * and needs no casts, the samples do not. */
   memset(&wrap_iface, 0, sizeof(wrap_iface));
   wrap_iface.get_path = (retro_vfs_get_path_t)retro_vfs_file_get_path_impl;
   wrap_iface.open     = (retro_vfs_open_t)retro_vfs_file_open_impl;
   wrap_iface.close    = (retro_vfs_close_t)retro_vfs_file_close_impl;
   wrap_iface.size     = wrap_size_cb;
   wrap_iface.tell     = (retro_vfs_tell_t)retro_vfs_file_tell_impl;
   wrap_iface.seek     = (retro_vfs_seek_t)retro_vfs_file_seek_impl;
   wrap_iface.read     = wrap_read;
   wrap_iface.write    = (retro_vfs_write_t)retro_vfs_file_write_impl;
   wrap_iface.flush    = (retro_vfs_flush_t)retro_vfs_file_flush_impl;
   wrap_iface.remove   = retro_vfs_file_remove_impl;
   wrap_iface.rename   = retro_vfs_file_rename_impl;
   wrap_iface.truncate = (retro_vfs_truncate_t)retro_vfs_file_truncate_impl;

   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &wrap_iface;

   filestream_vfs_init(&info);
}

static void wrap_uninstall(void)
{
   struct retro_vfs_interface_info info;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = NULL;
   filestream_vfs_init(&info);
}

/* ------------------------------------------------------------------ */
/* Tests                                                              */
/* ------------------------------------------------------------------ */

/* The hint has to actually reach the descriptor branch, and has to
 * leave every other kind of open exactly where it was. */
static void test_path_selection(void)
{
   const char *path = "bulk_select.bin";
   libretro_vfs_implementation_file *s;

   printf("test_path_selection\n");

   if (!fixture_write(path, 4096))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }

   s = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(s != NULL, "bulk open failed");
   if (s)
   {
#ifdef VFS_HAVE_DESCRIPTOR_IO
      CHECK(s->fp == NULL, "bulk read still went through stdio");
      CHECK(s->fd >= 0, "bulk read has no descriptor");
      CHECK(s->mapped == NULL, "bulk read mapped the file");
#else
      CHECK(s->fp != NULL, "no descriptor path here, must stay buffered");
#endif
      CHECK(s->size == 4096, "size wrong: %lld", (long long)s->size);
      retro_vfs_file_close_impl(s);
   }

   /* No hint: unchanged, buffered. */
   s = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(s != NULL, "plain open failed");
   if (s)
   {
      CHECK(s->fp != NULL, "plain open left stdio");
      retro_vfs_file_close_impl(s);
   }

   /* Write modes must ignore it: the descriptor branch below the hint
    * is only wired for reading, and write() through it would bypass
    * the size bookkeeping the buffered path does. */
   s = retro_vfs_file_open_impl("bulk_select_w.bin",
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(s != NULL, "bulk write open failed");
   if (s)
   {
      CHECK(s->fp != NULL, "bulk hint diverted a write to the descriptor path");
      retro_vfs_file_close_impl(s);
   }

   /* Both hints at once: the mapping wins and nothing about that
    * path changes. */
   s = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ,
           RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS
         | RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(s != NULL, "combined-hint open failed");
   if (s)
   {
#ifdef VFS_HAVE_FILE_MAPPING
      CHECK(s->mapped != NULL, "combined hints lost the mapping");
#endif
      retro_vfs_file_close_impl(s);
   }

   remove(path);
   remove("bulk_select_w.bin");
}

/* Sizes around the buffer sizes involved: the 64 KiB stdio buffer the
 * other path uses, and the 16 KiB read-ahead buffer in RFILE. */
static void test_roundtrip(void)
{
   static const int64_t sizes[] =
   {
      0, 1, 2, 255, 4095, 4096, 16383, 16384, 16385,
      65535, 65536, 65537, 1048576, 1048583
   };
   const char *path = "bulk_roundtrip.bin";
   size_t      i;

   printf("test_roundtrip\n");

   for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
   {
      void    *buf = NULL;
      int64_t  len = 0;
      int64_t  rc;

      if (!fixture_write(path, sizes[i]))
      {
         printf("  SKIP: cannot create %lld-byte fixture\n",
               (long long)sizes[i]);
         continue;
      }

      rc = filestream_read_file(path, &buf, &len);

      CHECK(rc == 1, "%lld: read failed", (long long)sizes[i]);
      CHECK(len == sizes[i], "%lld: got %lld bytes",
            (long long)sizes[i], (long long)len);
      if (buf)
      {
         CHECK(fixture_matches((const uint8_t*)buf, len),
               "%lld: content mismatch", (long long)sizes[i]);
         /* The documented NUL past the end - the reason the buffer is
          * one byte larger than the file. */
         CHECK(((const char*)buf)[len] == '\0',
               "%lld: missing terminator", (long long)sizes[i]);
         free(buf);
      }
      else
         CHECK(0, "%lld: no buffer", (long long)sizes[i]);
   }

   remove(path);
}

/* The regression this exists for: a backend that hands back a little
 * at a time still has to produce the whole file. */
static void test_short_reads(void)
{
   static const int64_t chunks[] = { 1, 7, 1000, 65536 };
   const char *path  = "bulk_short.bin";
   const int64_t size = 1048576;
   size_t i;

   printf("test_short_reads\n");

   if (!fixture_write(path, size))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }

   wrap_install();

   for (i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++)
   {
      void    *buf = NULL;
      int64_t  len = 0;
      int64_t  rc;

      wrap_reset();
      wrap_chunk = chunks[i];

      rc = filestream_read_file(path, &buf, &len);

      CHECK(rc == 1, "chunk %lld: read failed", (long long)chunks[i]);
      CHECK(len == size, "chunk %lld: got %lld of %lld bytes",
            (long long)chunks[i], (long long)len, (long long)size);
      if (buf)
      {
         CHECK(fixture_matches((const uint8_t*)buf, len),
               "chunk %lld: content mismatch", (long long)chunks[i]);
         CHECK(((const char*)buf)[len] == '\0',
               "chunk %lld: missing terminator", (long long)chunks[i]);
         free(buf);
      }
   }

   wrap_uninstall();
   wrap_reset();
   remove(path);
}

/* A file that lost bytes between the size query and the read is not a
 * failure; it is a shorter file.  Reporting what was there is the
 * behaviour this function has always had. */
static void test_file_shrank(void)
{
   const char   *path = "bulk_shrank.bin";
   const int64_t size = 65536;
   void         *buf  = NULL;
   int64_t       len  = 0;
   int64_t       rc;

   printf("test_file_shrank\n");

   if (!fixture_write(path, size))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }

   wrap_install();
   wrap_reset();
   wrap_stop_after = 40000;

   rc = filestream_read_file(path, &buf, &len);

   CHECK(rc == 1, "short file reported as failure");
   CHECK(len == 40000, "got %lld bytes, expected 40000", (long long)len);
   if (buf)
   {
      CHECK(fixture_matches((const uint8_t*)buf, len), "content mismatch");
      CHECK(((const char*)buf)[len] == '\0', "terminator not at short length");
      free(buf);
   }

   wrap_uninstall();
   wrap_reset();
   remove(path);
}

/* An error partway through has to fail the call outright and hand
 * back nothing - no half-filled buffer presented as a whole file, and
 * nothing left allocated.  LSan is the judge of the second half. */
static void test_read_error(void)
{
   const char   *path = "bulk_error.bin";
   const int64_t size = 1048576;
   int           at;

   printf("test_read_error\n");

   if (!fixture_write(path, size))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }

   wrap_install();

   /* First call and a later one, so both the "nothing read yet" and
    * the "partially filled" branches are covered. */
   for (at = 0; at <= 3; at += 3)
   {
      void    *buf = NULL;
      int64_t  len = 0;
      int64_t  rc;

      wrap_reset();
      wrap_chunk   = 100000;
      wrap_fail_at = at;

      rc = filestream_read_file(path, &buf, &len);

      CHECK(rc == 0, "fail@%d: error not reported", at);
      CHECK(buf == NULL, "fail@%d: buffer handed back on failure", at);
      CHECK(len == -1, "fail@%d: len is %lld, expected -1",
            at, (long long)len);
      free(buf);
   }

   wrap_uninstall();
   wrap_reset();
   remove(path);
}

/* Nothing in a whole-file read may translate bytes.  This is aimed
 * squarely at Windows, where the descriptor path opens with open()/
 * _wopen() rather than fopen(mode "rb"): without O_BINARY in the
 * flags, 0x0D 0x0A collapses to 0x0A and a 0x1A ends the file early,
 * and the result is a short buffer that looks exactly like a shorter
 * file.  It is worth running everywhere, since a text-mode default
 * anywhere else would show up here too. */
static void test_binary_transparency(void)
{
   const char *path = "bulk_binary.bin";
   static const uint8_t payload[] =
   {
      'a', 0x0D, 0x0A, 'b',            /* CRLF                       */
      0x0D, 0x0D, 0x0A, 0x0A,          /* runs of both               */
      'c', 0x1A, 'd',                  /* DOS end-of-file marker     */
      0x00, 0xFF, 0x00,                /* embedded NULs and high bit */
      0x0A, 0x0D,                      /* LFCR, the wrong way round  */
      'e'
   };
   FILE    *f;
   void    *buf = NULL;
   int64_t  len = 0;
   int64_t  rc;

   printf("test_binary_transparency\n");

   if (!(f = fopen(path, "wb")))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }
   if (fwrite(payload, 1, sizeof(payload), f) != sizeof(payload))
   {
      fclose(f);
      printf("  SKIP: cannot write fixture\n");
      return;
   }
   fclose(f);

   rc = filestream_read_file(path, &buf, &len);

   CHECK(rc == 1, "read failed");
   CHECK(len == (int64_t)sizeof(payload),
         "got %lld bytes, expected %lu - byte translation on the read path",
         (long long)len, (unsigned long)sizeof(payload));
   if (buf && len == (int64_t)sizeof(payload))
      CHECK(memcmp(buf, payload, sizeof(payload)) == 0,
            "content altered in transit");
   free(buf);

   remove(path);
}

/* A directory is not a file, but it opens like one: on Linux both
 * fopen() and open() succeed on a directory and seeking to the end
 * reports INT64_MAX.  Feeding that to the allocator used to overflow
 * int64_t on the way ('(size_t)(size + 1)' adds in the signed domain
 * first), which is undefined behaviour and shows up under UBSan.  The
 * call must fail cleanly instead - and on platforms where opening a
 * directory is refused outright it fails one step earlier, which is
 * equally fine. */
static void test_directory_path(void)
{
   void    *buf = NULL;
   int64_t  len = 0;
   int64_t  rc;

   printf("test_directory_path\n");

   /* However the platform presents it. */
   rc = filestream_read_file(".", &buf, &len);
   CHECK(rc == 0, "reading a directory reported success");
   CHECK(buf == NULL, "reading a directory produced a buffer");
   free(buf);

   /* The three answers a platform can give for a directory's length,
    * forced one at a time so every machine runs all of them rather
    * than only its own.  Linux reports INT64_MAX from lseek and is
    * refused by the size ceiling; Darwin reports something small and
    * plausible through stdio and then delivers no bytes; a third
    * could report zero, which is also what an empty file reports.
    * All three must fail, and none may hand back a buffer. */
   wrap_install();

   {
      static const int64_t sizes[] = { 0, 128, 4096 };
      size_t i;

      for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
      {
         wrap_reset();
         wrap_size = sizes[i];
         buf       = NULL;
         len       = 0;

         rc = filestream_read_file(".", &buf, &len);

         CHECK(rc == 0, "directory reporting size %lld read as a file",
               (long long)sizes[i]);
         CHECK(buf == NULL, "directory reporting size %lld produced a buffer",
               (long long)sizes[i]);
         free(buf);

         /* And with the read reporting end-of-stream rather than an
          * error.  Linux answers EISDIR, which fails on its own;
          * Darwin's buffered path answers zero bytes, which without
          * the guard below reads as "the file shrank to nothing" and
          * returns an empty buffer as a success.  Forced here so
          * every platform exercises both answers. */
         wrap_reset();
         wrap_size       = sizes[i];
         wrap_stop_after = 0;
         buf             = NULL;
         len             = 0;

         rc = filestream_read_file(".", &buf, &len);

         CHECK(rc == 0, "directory reporting size %lld and no bytes "
               "read as an empty file", (long long)sizes[i]);
         CHECK(buf == NULL, "directory reporting size %lld and no bytes "
               "produced a buffer", (long long)sizes[i]);
         free(buf);
      }
   }

   wrap_uninstall();
   wrap_reset();
}

static void test_missing_file(void)
{
   void    *buf = NULL;
   int64_t  len = 0;
   int64_t  rc;

   printf("test_missing_file\n");

   remove("bulk_absent.bin");
   rc = filestream_read_file("bulk_absent.bin", &buf, &len);

   CHECK(rc == 0, "missing file reported as read");
   CHECK(buf == NULL, "missing file produced a buffer");
   free(buf);
}

/* ------------------------------------------------------------------ */
/* Concurrency                                                        */
/* ------------------------------------------------------------------ */

#ifdef BULK_TEST_THREADS
#define BULK_THREADS 8
#define BULK_ITERS   40

static const char *thread_path = "bulk_threads.bin";
static int64_t     thread_size = 262144;
static int         thread_bad[BULK_THREADS];

static bulk_thread_ret_t BULK_THREAD_CALL thread_body(void *arg)
{
   int  idx = *(int*)arg;
   int  i;

   for (i = 0; i < BULK_ITERS; i++)
   {
      void    *buf = NULL;
      int64_t  len = 0;

      if (filestream_read_file(thread_path, &buf, &len) != 1)
      {
         thread_bad[idx]++;
         continue;
      }
      if (len != thread_size || !fixture_matches((const uint8_t*)buf, len))
         thread_bad[idx]++;
      free(buf);
   }

   return BULK_THREAD_DONE;
}

static int bulk_thread_start(bulk_thread_t *th, void *arg)
{
#if defined(_WIN32)
   uintptr_t h = _beginthreadex(NULL, 0, thread_body, arg, 0, NULL);
   if (!h)
      return 0;
   *th = (HANDLE)h;
   return 1;
#else
   return pthread_create(th, NULL, thread_body, arg) == 0;
#endif
}

static void bulk_thread_join(bulk_thread_t th)
{
#if defined(_WIN32)
   WaitForSingleObject(th, INFINITE);
   CloseHandle(th);
#else
   pthread_join(th, NULL);
#endif
}

/* Nothing in this path may keep shared mutable state.  Strongest
 * under -fsanitize=thread, but a plain run still catches a torn or
 * short buffer. */
static void test_concurrent_reads(void)
{
   bulk_thread_t th[BULK_THREADS];
   int       idx[BULK_THREADS];
   int       i;
   int       bad = 0;

   printf("test_concurrent_reads\n");

   if (!fixture_write(thread_path, thread_size))
   {
      printf("  SKIP: cannot create fixture\n");
      return;
   }

   for (i = 0; i < BULK_THREADS; i++)
   {
      thread_bad[i] = 0;
      idx[i]        = i;
      if (!bulk_thread_start(&th[i], &idx[i]))
      {
         printf("  SKIP: cannot spawn threads\n");
         while (--i >= 0)
            bulk_thread_join(th[i]);
         remove(thread_path);
         return;
      }
   }

   for (i = 0; i < BULK_THREADS; i++)
   {
      bulk_thread_join(th[i]);
      bad += thread_bad[i];
   }

   CHECK(bad == 0, "%d concurrent reads came back wrong", bad);
   remove(thread_path);
}
#endif

int main(void)
{
   printf("=== whole-file / bulk read contract ===\n");

   test_path_selection();
   test_roundtrip();
   test_short_reads();
   test_file_shrank();
   test_read_error();
   test_binary_transparency();
   test_directory_path();
   test_missing_file();
#ifdef BULK_TEST_THREADS
   test_concurrent_reads();
#endif

   printf("=== %d checks, %d failures ===\n", checks, failures);
   return failures ? 1 : 0;
}
