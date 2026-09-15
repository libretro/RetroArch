/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_mapped_ptr_test.c).
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

/* Regression test for two things that meet at the same place: what a
 * stream can tell you about a file before you copy it.
 *
 * 1. filestream_read_file() on a file that reports no size.
 *
 *    It sized its allocation from filestream_get_size() and read
 *    exactly that many bytes, so a file stat'ing as zero came back
 *    empty - with a *success* return, indistinguishable from a
 *    genuinely empty file.  Every procfs entry stats as zero, so the
 *    /proc/apm battery probe, the /proc/modules check in cdrom.c and
 *    the dingux /proc/jz/battery reader all silently read nothing.
 *    (mem_stats.c reads /proc/meminfo through a raw open() and
 *    read() precisely to dodge this.)
 *
 *    A stat size is a hint, not a contract.  The fix reads to EOF
 *    when there is no usable hint - and the test asserts on real
 *    procfs where the platform has it, because a fixture cannot
 *    reproduce "stats as 0 but yields bytes" on a normal filesystem.
 *    A genuinely empty file must still read as empty, which is the
 *    case the fix could plausibly break.
 *
 * 2. filestream_get_mapped_ptr(), the borrowed view of a mapped file.
 *
 *    RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS has always mmap'd the
 *    file, but the mapping was reachable only from inside the read
 *    path, which memcpy's out of it - so asking for a map bought a
 *    copy of data that was already addressable.  The contract the
 *    accessor has to keep is narrow and worth pinning: the bytes are
 *    the file's, the length is the whole file, it is NULL without the
 *    hint, and it stays consistent with what filestream_read()
 *    returns from the same stream.
 *
 *    NULL is a legal answer everywhere (no HAVE_MMAP, other schemes,
 *    a frontend VFS), so every check here is conditional on having
 *    got a pointer at all - a platform without mappings must pass by
 *    skipping, not by failing.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./vfs_mapped_ptr_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
      else \
         printf("ok:   %s\n", msg); \
   } while (0)

#define SKIP(msg) printf("skip: %s\n", (msg))

static void write_file(const char *path, const void *data, size_t len)
{
   FILE *f = fopen(path, "wb");
   if (len)
      fwrite(data, 1, len, f);
   fclose(f);
}

int main(void)
{
   /* Big enough to span pages, odd enough to leave a sub-page tail. */
   size_t   len  = 300 * 1024 + 77;
   uint8_t *body = (uint8_t*)malloc(len);
   size_t   i;

   for (i = 0; i < len; i++)
      body[i] = (uint8_t)(i * 31 + (i >> 8));

   write_file("mapped_fixture.bin", body, len);
   write_file("mapped_empty.bin", NULL, 0);

   /* ---- 1a: a file that reports no size still reads ---- */
   {
      void   *buf = NULL;
      int64_t got = -1;
      /* /proc/cpuinfo stats as 0 bytes and yields many; no ordinary
       * file can stand in for that, so skip where it is absent. */
      if (filestream_exists("/proc/cpuinfo"))
      {
         int64_t rv = filestream_read_file("/proc/cpuinfo", &buf, &got);
         CHECK(rv == 1, "size-0 file: read reports success");
         CHECK(got > 0, "size-0 file: bytes actually delivered");
         CHECK(buf && ((char*)buf)[got] == '\0',
               "size-0 file: result is NUL-terminated at the length");
         free(buf);
      }
      else
         SKIP("no procfs on this platform (size-0 read lane)");
   }

   /* ---- 1b: a genuinely empty file is still empty ---- */
   {
      void   *buf = NULL;
      int64_t got = -1;
      int64_t rv  = filestream_read_file("mapped_empty.bin", &buf, &got);
      CHECK(rv == 1, "empty file: read reports success");
      CHECK(got == 0, "empty file: zero bytes");
      CHECK(buf && ((char*)buf)[0] == '\0', "empty file: empty string");
      free(buf);
   }

   /* ---- 1c: a missing file is still a failure ---- */
   {
      void   *buf = NULL;
      int64_t got = 0;
      int64_t rv  = filestream_read_file("mapped_missing.bin", &buf, &got);
      CHECK(rv == 0, "missing file: read reports failure");
      CHECK(buf == NULL, "missing file: no buffer handed back");
   }

   /* ---- 1d: an ordinary sized file is unchanged ---- */
   {
      void   *buf = NULL;
      int64_t got = -1;
      int64_t rv  = filestream_read_file("mapped_fixture.bin", &buf, &got);
      CHECK(rv == 1 && got == (int64_t)len, "sized file: full length read");
      CHECK(buf && memcmp(buf, body, len) == 0,
            "sized file: contents intact");
      free(buf);
   }

   /* ---- 2a: the mapped view, with the hint ---- */
   {
      RFILE *f = filestream_open("mapped_fixture.bin",
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      CHECK(f != NULL, "fixture opens with the mapping hint");
      if (f)
      {
         int64_t        map_len = -1;
         const uint8_t *map     = filestream_get_mapped_ptr(f, &map_len);

         if (map)
         {
            CHECK(map_len == (int64_t)len,
                  "mapped view spans the whole file");
            CHECK(memcmp(map, body, len) == 0,
                  "mapped view holds the file's bytes");
            /* The point of the accessor: the same bytes a read would
             * have copied, without the copy.  Checked over the whole
             * file, not a prefix - consumers hash or compare the
             * entire mapping (task_cloudsync's MD5 does exactly
             * that), so a view that agreed only at the start would
             * silently produce a different digest from the read path
             * on the same content. */
            {
               uint8_t *probe = (uint8_t*)malloc(len);
               int64_t  got   = 0;
               size_t   off   = 0;

               while (off < len)
               {
                  got = filestream_read(f, probe + off,
                        (int64_t)(len - off));
                  if (got <= 0)
                     break;
                  off += (size_t)got;
               }
               CHECK(off == len,
                     "sequential read from the same stream reaches EOF");
               CHECK(off == len && memcmp(probe, map, len) == 0,
                     "read agrees with the mapped view over the whole file");
               free(probe);
            }
         }
         else
            SKIP("no mapping available (mapped-view lane)");
         filestream_close(f);
      }
   }

   /* ---- 2b: no hint, no map ---- */
   {
      RFILE *f = filestream_open("mapped_fixture.bin",
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(f != NULL, "fixture opens without the hint");
      if (f)
      {
         int64_t        map_len = -1;
         const uint8_t *map     = filestream_get_mapped_ptr(f, &map_len);
         CHECK(map == NULL, "no hint: no mapped view offered");
         CHECK(map_len == 0, "no hint: length reported as zero");
         filestream_close(f);
      }
   }

   /* ---- 2c: defensive arguments ---- */
   {
      const uint8_t *map = filestream_get_mapped_ptr(NULL, NULL);
      CHECK(map == NULL, "NULL stream answers NULL rather than faulting");
   }

   /* ---- 2d: an empty file must not hand back a bogus view ---- */
   {
      RFILE *f = filestream_open("mapped_empty.bin",
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      if (f)
      {
         int64_t        map_len = -1;
         const uint8_t *map     = filestream_get_mapped_ptr(f, &map_len);
         CHECK(!map || map_len == 0,
               "empty file: no view, or a view of zero bytes");
         filestream_close(f);
      }
      else
         SKIP("empty fixture would not open with the hint");
   }

   /* ---- 3: filestream_matches_buf ---- */
   {
      uint8_t *other = (uint8_t*)malloc(len);

      /* Which lane the checks below actually exercise.  With mappings
       * available the compare runs against the map in one memcmp and
       * the windowed fallback never executes, so the boundary cases
       * are only meaningful in an MMAP=0 build - run both. */
#ifdef HAVE_MMAP
      printf("info: built with mappings; matches_buf compares in place\n");
#else
      printf("info: built without mappings; matches_buf uses the window\n");
#endif

      memcpy(other, body, len);
      CHECK(filestream_matches_buf("mapped_fixture.bin", other, len),
            "matches_buf: identical content matches");

      /* The three places a difference can hide.  A window-at-a-time
       * compare that mishandled its offsets would pass the first and
       * fail one of the others. */
      other[0] ^= 0xff;
      CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len),
            "matches_buf: difference at the first byte is caught");
      other[0] ^= 0xff;

      other[len / 2] ^= 0xff;
      CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len),
            "matches_buf: difference mid-file is caught");
      other[len / 2] ^= 0xff;

      other[len - 1] ^= 0xff;
      CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len),
            "matches_buf: difference at the last byte is caught");
      other[len - 1] ^= 0xff;

      /* A window boundary is where an off-by-one would live.  This
       * takes the window straight from the header rather than
       * repeating the number here.  It has already been resized once,
       * and a boundary case aimed at the old value is not a boundary
       * case at all - just another mid-file check wearing the name. */
      {
         size_t win = FILESTREAM_MATCHES_BUF_WINDOW;
         size_t at[] = { win - 1, win, win + 1, 2 * win, 8 * win };
         size_t k;
         for (k = 0; k < sizeof(at) / sizeof(at[0]); k++)
         {
            if (at[k] >= len)
               continue;
            other[at[k]] ^= 0xff;
            CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len),
                  "matches_buf: difference at a window boundary is caught");
            other[at[k]] ^= 0xff;
         }
      }

      /* Size disagreements resolve without reading, and must not
       * report a match on a prefix. */
      CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len - 1),
            "matches_buf: shorter length does not match");
      CHECK(!filestream_matches_buf("mapped_fixture.bin", other, len + 1),
            "matches_buf: longer length does not match");

      CHECK(!filestream_matches_buf("mapped_missing.bin", other, len),
            "matches_buf: missing file does not match");
      CHECK(!filestream_matches_buf(NULL, other, len),
            "matches_buf: NULL path answers false rather than faulting");

      /* An empty file against zero bytes is a match, and is the one
       * case where a NULL data pointer is legal. */
      CHECK(filestream_matches_buf("mapped_empty.bin", NULL, 0),
            "matches_buf: empty file matches zero bytes");
      CHECK(!filestream_matches_buf("mapped_empty.bin", other, len),
            "matches_buf: empty file does not match a non-empty buffer");

      free(other);
   }

   /* ---- 4: no stale bytes from a recycled stdio buffer ---- */
   {
      /* The VFS hands stdio a per-open buffer and no longer zeroes it
       * (nothing reads those bytes before stdio writes them, and the
       * WiiU path never zeroed).  The regression that would introduce
       * is stale content surfacing through a read - most plausibly on
       * a short read near EOF, where the buffer is only partly
       * filled, and on a stream opened right after one that left
       * different bytes in a recycled allocation.
       *
       * So: alternate between two files of different lengths and
       * distinct fills, reading each to EOF a byte-count at a time
       * that does not divide either length, and verify every byte.
       * Under the old zeroing build this passes trivially; it is here
       * to keep passing without it. */
      const char *pa = "stale_a.bin";
      const char *pb = "stale_b.bin";
      size_t   la    = 40000;
      size_t   lb    = 9001;
      uint8_t *ba    = (uint8_t*)malloc(la);
      uint8_t *bb    = (uint8_t*)malloc(lb);
      uint8_t *got   = (uint8_t*)malloc(la);
      int      round;
      bool     clean = true;

      for (i = 0; i < la; i++)
         ba[i] = (uint8_t)(0xA0 + (i & 0x0f));
      for (i = 0; i < lb; i++)
         bb[i] = (uint8_t)(0x50 + (i & 0x0f));
      write_file(pa, ba, la);
      write_file(pb, bb, lb);

      for (round = 0; round < 40 && clean; round++)
      {
         const char    *path = (round & 1) ? pb : pa;
         const uint8_t *want = (round & 1) ? bb : ba;
         size_t         len2 = (round & 1) ? lb : la;
         size_t         off  = 0;
         RFILE         *f    = filestream_open(path,
               RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE);

         if (!f)
         {
            clean = false;
            break;
         }
         /* 4093 is prime, so the final read of each file is short and
          * the buffer is left partly filled. */
         while (off < len2)
         {
            int64_t n = filestream_read(f, got + off, 4093);
            if (n <= 0)
               break;
            off += (size_t)n;
         }
         if (off != len2 || memcmp(got, want, len2) != 0)
            clean = false;
         /* Past EOF must deliver nothing rather than buffer residue. */
         {
            uint8_t tail[64];
            memset(tail, 0xCC, sizeof(tail));
            if (filestream_read(f, tail, (int64_t)sizeof(tail)) > 0)
               clean = false;
            for (i = 0; i < sizeof(tail); i++)
               if (tail[i] != 0xCC)
                  clean = false;
         }
         filestream_close(f);
      }
      CHECK(clean,
            "recycled stdio buffers leak no stale bytes across opens");

      free(ba);
      free(bb);
      free(got);
      remove(pa);
      remove(pb);
   }

   free(body);
   remove("mapped_fixture.bin");
   remove("mapped_empty.bin");

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== vfs_mapped_ptr_test: all tests pass ==\n");
   return 0;
}
