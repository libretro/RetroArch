/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_large_file_test.c).
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

/* Regression test for files past the 2 GiB and 4 GiB lines.
 *
 * RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS puts a stream on the
 * unbuffered descriptor path, which is also the only path a mapped
 * file uses.  That path seeked with lseek() and off_t, which is 32
 * bits on every Windows build (long stays 32-bit there even on x64)
 * and on any Unix without large-file support.  Everything below is a
 * case that failed on a Windows target because of it, or a case the
 * replacement could plausibly get wrong.
 *
 * The failure was not a clean error, which is what makes it worth
 * pinning: _lseek does not return -1 on a file it cannot express, it
 * returns a truncated negative (-1073741824 seeking to the end of a
 * 3 GiB file), so a "== -1" test reads it as success.  Measured on the
 * unfixed tree, x86_64 Windows: size 0, SEEK_END failing, tell at end
 * 2600000008.  On 32-bit Windows, where the mapping cannot fit the
 * address space and every operation falls back to the descriptor:
 * size -1073741824, seeks failing outright, reads returning zeros.
 *
 * The 4 GiB fixture exists because a 2 GiB fix can still be wrong one
 * dword up: a seek offset splits into a signed high and unsigned low
 * half on Windows, and nothing below 4 GiB exercises a high half other
 * than 0 or -1.  Negative SEEK_CUR/SEEK_END offsets are here for the
 * same reason - they are the sign-extension cases of that split.
 *
 * Fixtures are sparse and built through the platform's own facilities
 * rather than through the code under test, so a broken seek cannot
 * quietly produce a broken fixture and call it a pass.  A filesystem
 * that will not hold them (FAT32 tops out below the 4 GiB case, and a
 * small tmpfs may refuse both) skips rather than fails.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./vfs_large_file_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* 3 GiB: past LONG_MAX, the case seen in the field. */
#define BIG_SIZE    3221225472LL
#define BIG_MARK    2600000000LL
/* 5 GiB: past UINT32_MAX, so the high dword of an offset is 1. */
#define HUGE_SIZE   5368709120LL
#define HUGE_MARK   4300000000LL

#define SENTINEL    "LRPS2!!!"
#define SENTINEL_SZ 8

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

/* Build a sparse fixture of `size` bytes with SENTINEL written at
 * `mark` and again at the very end, without going through RFILE - the
 * point is to have a fixture whose geometry is known good even when
 * the code under test cannot seek.  Returns false when the platform or
 * filesystem will not hold it, which is a skip, not a failure. */
static bool make_fixture(const char *path, int64_t size, int64_t mark)
{
#if defined(_WIN32)
   HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
   LONG  hi;
   DWORD lo, written, junk;

   if (h == INVALID_HANDLE_VALUE)
      return false;

   /* Best-effort: NTFS allocates lazily anyway, this just makes it
    * explicit.  Ignored where the filesystem has no such notion. */
   DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &junk, NULL);

   hi = (LONG)(size >> 32);
   SetLastError(NO_ERROR);
   lo = SetFilePointer(h, (LONG)(DWORD)(size & 0xFFFFFFFFu), &hi, FILE_BEGIN);
   if ((lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
         || !SetEndOfFile(h))
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
    }

   hi = (LONG)(mark >> 32);
   SetLastError(NO_ERROR);
   lo = SetFilePointer(h, (LONG)(DWORD)(mark & 0xFFFFFFFFu), &hi, FILE_BEGIN);
   if (lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
   }
   if (!WriteFile(h, SENTINEL, SENTINEL_SZ, &written, NULL)
         || written != SENTINEL_SZ)
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
   }

   hi = (LONG)((size - SENTINEL_SZ) >> 32);
   SetLastError(NO_ERROR);
   SetFilePointer(h, (LONG)(DWORD)((size - SENTINEL_SZ) & 0xFFFFFFFFu), &hi,
         FILE_BEGIN);
   WriteFile(h, SENTINEL, SENTINEL_SZ, &written, NULL);

   CloseHandle(h);
   return true;
#else
   int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
   if (fd < 0)
      return false;

   if (     ftruncate(fd, (off_t)size) != 0
         || sizeof(off_t) < sizeof(int64_t))
   {
      close(fd);
      remove(path);
      return false;
   }
   if (     lseek(fd, (off_t)mark, SEEK_SET) != (off_t)mark
         || write(fd, SENTINEL, SENTINEL_SZ) != SENTINEL_SZ)
   {
      close(fd);
      remove(path);
      return false;
   }
   if (lseek(fd, (off_t)(size - SENTINEL_SZ), SEEK_SET)
         == (off_t)(size - SENTINEL_SZ))
      (void)!write(fd, SENTINEL, SENTINEL_SZ);

   close(fd);
   return true;
#endif
}

/* Every check that a stream owes a caller about a large file.  Run
 * once per open mode: the hinted (descriptor/mapped) path is the one
 * that broke, the unhinted (buffered) path is the control that always
 * worked, and the two disagreeing is itself the bug. */
static void exercise(const char *path, int64_t size, int64_t mark,
      unsigned hints, const char *what)
{
   char     label[160];
   char     buf[SENTINEL_SZ];
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, hints);
   int64_t  got;

   snprintf(label, sizeof(label), "[%s] open", what);
   CHECK(f != NULL, label);
   if (!f)
      return;

   /* The original symptom: a size that came back 0 or truncated
    * negative.  Both are "<= 0", which is what callers test, and both
    * made a perfectly good file look unopenable. */
   got = filestream_get_size(f);
   snprintf(label, sizeof(label), "[%s] size is positive (got %lld)",
         what, (long long)got);
   CHECK(got > 0, label);
   snprintf(label, sizeof(label), "[%s] size is exact (%lld)",
         what, (long long)size);
   CHECK(got == size, label);

   /* Absolute seek past the line, and the read that proves the
    * position was real rather than merely reported. */
   snprintf(label, sizeof(label), "[%s] seek SET to %lld",
         what, (long long)mark);
   CHECK(filestream_seek(f, mark, RETRO_VFS_SEEK_POSITION_START) == 0, label);
   got = filestream_tell(f);
   snprintf(label, sizeof(label), "[%s] tell after SET (got %lld)",
         what, (long long)got);
   CHECK(got == mark, label);

   memset(buf, 0, sizeof(buf));
   snprintf(label, sizeof(label), "[%s] read at %lld",
         what, (long long)mark);
   CHECK(filestream_read(f, buf, SENTINEL_SZ) == SENTINEL_SZ, label);
   snprintf(label, sizeof(label), "[%s] bytes at %lld are the sentinel",
         what, (long long)mark);
   CHECK(memcmp(buf, SENTINEL, SENTINEL_SZ) == 0, label);

   /* SEEK_END on a file whose end does not fit in 31 bits. */
   snprintf(label, sizeof(label), "[%s] seek END", what);
   CHECK(filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_END) == 0, label);
   got = filestream_tell(f);
   snprintf(label, sizeof(label), "[%s] tell at END (got %lld)",
         what, (long long)got);
   CHECK(got == size, label);

   /* Negative SEEK_END: the high half of the offset is sign-extended,
    * which is the half a naive split gets wrong. */
   snprintf(label, sizeof(label), "[%s] seek END-%d", what, SENTINEL_SZ);
   CHECK(filestream_seek(f, -(int64_t)SENTINEL_SZ,
            RETRO_VFS_SEEK_POSITION_END) == 0, label);
   memset(buf, 0, sizeof(buf));
   filestream_read(f, buf, SENTINEL_SZ);
   snprintf(label, sizeof(label), "[%s] bytes at END-%d are the sentinel",
         what, SENTINEL_SZ);
   CHECK(memcmp(buf, SENTINEL, SENTINEL_SZ) == 0, label);

   /* Relative seeks in both directions across the same line. */
   filestream_seek(f, mark, RETRO_VFS_SEEK_POSITION_START);
   CHECK(filestream_seek(f, 1000, RETRO_VFS_SEEK_POSITION_CURRENT) == 0,
         (snprintf(label, sizeof(label), "[%s] seek CUR +1000", what), label));
   got = filestream_tell(f);
   snprintf(label, sizeof(label), "[%s] tell after CUR +1000 (got %lld)",
         what, (long long)got);
   CHECK(got == mark + 1000, label);

   CHECK(filestream_seek(f, -1000, RETRO_VFS_SEEK_POSITION_CURRENT) == 0,
         (snprintf(label, sizeof(label), "[%s] seek CUR -1000", what), label));
   got = filestream_tell(f);
   snprintf(label, sizeof(label), "[%s] tell after CUR -1000 (got %lld)",
         what, (long long)got);
   CHECK(got == mark, label);

   /* A mapping must describe the whole file or not exist.  A short
    * mapping that still reports the full length is worse than none:
    * mmap() takes a size_t, and a length truncated on the way in can
    * map successfully, leaving reads past the cut to fault rather than
    * come up short. */
   {
      int64_t        maplen = 0;
      const uint8_t *map    = filestream_get_mapped_ptr(f, &maplen);

      if (map)
      {
         snprintf(label, sizeof(label),
               "[%s] mapped length is the whole file (got %lld)",
               what, (long long)maplen);
         CHECK(maplen == size, label);
         snprintf(label, sizeof(label),
               "[%s] mapped bytes at %lld are the sentinel",
               what, (long long)mark);
         CHECK(memcmp(map + mark, SENTINEL, SENTINEL_SZ) == 0, label);
      }
      else
      {
         snprintf(label, sizeof(label),
               "[%s] no mapping (legal: refused, unsupported, or no hint)",
               what);
         SKIP(label);
      }
   }

   filestream_close(f);
}

static void run_fixture(const char *path, int64_t size, int64_t mark,
      const char *name)
{
   char what[96];

   if (!make_fixture(path, size, mark))
   {
      snprintf(what, sizeof(what),
            "%s fixture (%lld bytes) - filesystem will not hold it",
            name, (long long)size);
      SKIP(what);
      return;
   }

   snprintf(what, sizeof(what), "%s hinted", name);
   exercise(path, size, mark,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS, what);

   snprintf(what, sizeof(what), "%s unhinted", name);
   exercise(path, size, mark, RETRO_VFS_FILE_ACCESS_HINT_NONE, what);

   remove(path);
}

int main(void)
{
   printf("sizeof(off_t) = %d\n", (int)sizeof(off_t));

   run_fixture("large_3gib.bin",  BIG_SIZE,  BIG_MARK,  "3 GiB");
   run_fixture("large_5gib.bin",  HUGE_SIZE, HUGE_MARK, "5 GiB");

   if (test_fails)
      printf("\n%d check(s) failed\n", test_fails);
   else
      printf("\nall checks passed\n");

   return test_fails ? 1 : 0;
}
