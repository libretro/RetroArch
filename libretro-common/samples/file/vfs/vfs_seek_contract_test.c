/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_seek_contract_test.c).
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

/* Companion to vfs_large_file_test.  That one asks whether a large file
 * can be read at all; this one asks what happens at the edges, which is
 * where the 2 GiB bug actually hid for as long as it did.
 *
 * 1. A seek that cannot be satisfied must say so.
 *
 *    The descriptor path tested its seek with "== -1".  _lseek does not
 *    return -1 on an offset it cannot express - it returns a truncated
 *    value (measured: -1073741824 seeking to the end of a 3 GiB file) -
 *    so the failure was read as success and the wrong position was
 *    handed onwards.  What makes that shape survivable is that nothing
 *    ever cross-checked the two: a caller that seeks and then asks
 *    where it is would have caught it immediately.
 *
 *    So the property pinned here is not "large seeks work" but the
 *    weaker one that holds for every file: a seek either succeeds and
 *    tell() agrees with it, or it fails and tell() is where it was.
 *    There is no third answer, and a truncating implementation can only
 *    produce the third answer.
 *
 * 2. EOF must mean the same thing on both paths.
 *
 *    A mapped stream ends at mapsize, a buffered one at whatever the
 *    C library says, and a descriptor one at whatever read() returns.
 *    Three implementations of one boundary, selected by a hint the
 *    caller passes for performance reasons and does not expect to
 *    change semantics.  The size query that broke went through the
 *    mapping's own bookkeeping, so a wrong size and a wrong EOF are
 *    the same defect seen from two directions.
 *
 * 3. A stat and a stream must agree about how big a file is.
 *
 *    path_get_size() reaches the size through stat, filestream through
 *    the open handle: different code, different width hazards (on
 *    Windows st_size is 32-bit unless the _stat64 family is used, and
 *    the struct and the function have to match or the size truncates
 *    silently).  Callers routinely use one to validate the other, so
 *    the two disagreeing is a bug even when neither is obviously
 *    wrong on its own.
 *
 * The fixture is 4 GiB + a little, so offsets have a non-zero high
 * dword and st_size cannot fit in 32 bits.  It is sparse and built
 * through the platform's own facilities rather than through the code
 * under test.  Where the filesystem will not hold it the large cases
 * skip, and the small-file cases - which carry most of the contract
 * checks - still run.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 *         make MMAP=0     (to put the unmapped descriptor path under test)
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/* 4 GiB + 4096: the high dword of an offset is 1 and st_size needs
 * more than 32 bits, but only just - a truncating size returns 4096,
 * which is a plausible-looking number rather than an obvious zero. */
#define HUGE_SIZE   4294971392LL
#define HUGE_MARK   4294961152LL   /* 8 KiB below the end, past 4 GiB */

#define SMALL_SIZE  4096LL
#define SMALL_MARK  2048LL

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

/* As in vfs_large_file_test: the fixture must not be built through the
 * code under test, or a broken seek writes a broken fixture and the
 * test agrees with itself. */
static bool make_fixture(const char *path, int64_t size, int64_t mark)
{
#if defined(_WIN32)
   LARGE_INTEGER li;
   DWORD         wrote = 0;
   HANDLE        h     = CreateFileA(path, GENERIC_WRITE, 0, NULL,
         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

   if (h == INVALID_HANDLE_VALUE)
      return false;

   li.QuadPart = size;
   if (     !SetFilePointerEx(h, li, NULL, FILE_BEGIN)
         || !SetEndOfFile(h))
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
   }

   li.QuadPart = mark;
   if (     !SetFilePointerEx(h, li, NULL, FILE_BEGIN)
         || !WriteFile(h, SENTINEL, SENTINEL_SZ, &wrote, NULL)
         || wrote != SENTINEL_SZ)
   {
      CloseHandle(h);
      DeleteFileA(path);
      return false;
   }

   CloseHandle(h);
   return true;
#else
   int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);

   if (fd < 0)
      return false;

   if (     ftruncate(fd, (off_t)size) != 0
         || lseek(fd, (off_t)mark, SEEK_SET) != (off_t)mark
         || write(fd, SENTINEL, SENTINEL_SZ) != SENTINEL_SZ)
   {
      close(fd);
      remove(path);
      return false;
   }

   close(fd);
   return true;
#endif
}

/* (1) A seek either lands where it said or leaves the position alone.
 *
 * Every case here is checked against tell(), because the return value
 * on its own is exactly what a truncating implementation gets wrong. */
static void check_seek_contract(const char *path, int64_t size,
      int64_t mark, unsigned hints, const char *what)
{
   char     label[192];
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, hints);
   int64_t  here;
   int      rv;

   snprintf(label, sizeof(label), "[%s] open", what);
   CHECK(f != NULL, label);
   if (!f)
      return;

   /* Establish a known position to measure the failures against. */
   filestream_seek(f, mark, RETRO_VFS_SEEK_POSITION_START);
   here = filestream_tell(f);
   snprintf(label, sizeof(label), "[%s] tell agrees with the seek that succeeded", what);
   CHECK(here == mark, label);

   /* A negative absolute position is not a position.  Whatever the
    * implementation returns, it must not have moved. */
   rv = filestream_seek(f, -1, RETRO_VFS_SEEK_POSITION_START);
   snprintf(label, sizeof(label), "[%s] seek to -1 reports failure", what);
   CHECK(rv != 0, label);
   snprintf(label, sizeof(label), "[%s] position unchanged after failed seek", what);
   CHECK(filestream_tell(f) == mark, label);

   /* Same, relative: mark - (mark + 1) is below the start of the file. */
   rv = filestream_seek(f, -(mark + 1), RETRO_VFS_SEEK_POSITION_CURRENT);
   snprintf(label, sizeof(label), "[%s] relative seek below zero reports failure", what);
   CHECK(rv != 0, label);
   snprintf(label, sizeof(label), "[%s] position unchanged after failed relative seek", what);
   CHECK(filestream_tell(f) == mark, label);

   /* Past the end is where the two paths genuinely disagree, and the
    * disagreement is not this test's to settle: seeking beyond EOF on
    * a read stream is legal in C and legal for lseek(), and the
    * buffered path allows it, while the mapped path refuses it because
    * a position outside the mapping has nothing behind it.  Both are
    * defensible; a caller that gets one or the other depending on a
    * performance hint is not.  Recorded rather than asserted, so that
    * the day someone unifies them this line says which way it went -
    * and the property that does hold either way is checked, because
    * that is the one truncation breaks. */
   filestream_seek(f, mark, RETRO_VFS_SEEK_POSITION_START);
   rv   = filestream_seek(f, 1, RETRO_VFS_SEEK_POSITION_END);
   here = filestream_tell(f);
   printf("note: [%s] seek past END %s\n", what,
         rv == 0 ? "allowed (buffered/descriptor semantics)"
                 : "refused (mapped semantics)");
   snprintf(label, sizeof(label),
         "[%s] past-END seek either lands where it said or does not move", what);
   CHECK((rv == 0 && here == size + 1) || (rv != 0 && here == mark), label);

   /* And where the seek was allowed, there are no bytes out there.
    * Where it was refused the position never left the file, so there
    * is nothing to say - reading would just read the file again. */
   if (rv == 0)
   {
      char eofbuf[8];

      snprintf(label, sizeof(label),
            "[%s] no bytes readable past END", what);
      CHECK(filestream_read(f, eofbuf, sizeof(eofbuf)) <= 0, label);
   }
   else
   {
      snprintf(label, sizeof(label),
            "[%s] past-END read (seek was refused, position never left the file)",
            what);
      SKIP(label);
   }

   /* And the one that matters: after all of that, the stream still
    * knows where it is and can still read the right bytes.  A
    * truncating seek that was mistaken for a success would have left
    * the position somewhere else entirely. */
   {
      char buf[SENTINEL_SZ];

      filestream_seek(f, mark, RETRO_VFS_SEEK_POSITION_START);
      memset(buf, 0, sizeof(buf));
      snprintf(label, sizeof(label),
            "[%s] still reads the sentinel after the failed seeks", what);
      CHECK(     filestream_read(f, buf, SENTINEL_SZ) == SENTINEL_SZ
              && memcmp(buf, SENTINEL, SENTINEL_SZ) == 0, label);
   }

   filestream_close(f);
}

/* (2) EOF means the same thing whichever path served the stream. */
static void check_eof_contract(const char *path, int64_t size,
      unsigned hints, const char *what)
{
   char     label[192];
   char     buf[64];
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, hints);
   int64_t  got;

   snprintf(label, sizeof(label), "[%s] open", what);
   CHECK(f != NULL, label);
   if (!f)
      return;

   /* Exactly at the end: zero bytes, not an error, not a short count
    * of something. */
   CHECK(filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_END) == 0,
         "[eof] seek to END");
   got = filestream_read(f, buf, sizeof(buf));
   snprintf(label, sizeof(label),
         "[%s] read at EOF returns 0 (got %lld)", what, (long long)got);
   CHECK(got == 0, label);

   /* Straddling the end: only what is actually there.  This is the
    * case a length computed as (end - pos) in the wrong width gets
    * wrong, and it cannot be reached by reading at aligned offsets. */
   CHECK(filestream_seek(f, size - 8, RETRO_VFS_SEEK_POSITION_START) == 0,
         "[eof] seek to size-8");
   got = filestream_read(f, buf, sizeof(buf));
   snprintf(label, sizeof(label),
         "[%s] read straddling EOF returns the 8 bytes that exist (got %lld)",
         what, (long long)got);
   CHECK(got == 8, label);

   /* And the position after a short read is the end, not the end plus
    * whatever was asked for. */
   snprintf(label, sizeof(label),
         "[%s] position after short read is EOF", what);
   CHECK(filestream_tell(f) == size, label);

   filestream_close(f);
}

/* (3) The stat and the stream must agree. */
static void check_size_agreement(const char *path, int64_t size,
      const char *what)
{
   char     label[192];
   int64_t  by_stat = path_get_size(path);
   RFILE   *f;
   int64_t  by_stream;
   int64_t  by_seek;

   snprintf(label, sizeof(label),
         "[%s] path_get_size reports the whole file (got %lld)",
         what, (long long)by_stat);
   CHECK(by_stat == size, label);

   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   if (!f)
   {
      CHECK(false, "[size] open");
      return;
   }

   by_stream = filestream_get_size(f);
   filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_END);
   by_seek   = filestream_tell(f);

   snprintf(label, sizeof(label),
         "[%s] filestream_get_size agrees with stat (%lld vs %lld)",
         what, (long long)by_stream, (long long)by_stat);
   CHECK(by_stream == by_stat, label);

   snprintf(label, sizeof(label),
         "[%s] seek-to-end agrees with filestream_get_size (%lld vs %lld)",
         what, (long long)by_seek, (long long)by_stream);
   CHECK(by_seek == by_stream, label);

   filestream_close(f);
}

static void run_fixture(const char *path, int64_t size, int64_t mark,
      const char *name)
{
   char what[128];

   if (!make_fixture(path, size, mark))
   {
      snprintf(what, sizeof(what),
            "%s fixture (%lld bytes) - filesystem will not hold it",
            name, (long long)size);
      SKIP(what);
      return;
   }

   snprintf(what, sizeof(what), "%s hinted", name);
   check_seek_contract(path, size, mark,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS, what);
   check_eof_contract(path, size,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS, what);

   snprintf(what, sizeof(what), "%s unhinted", name);
   check_seek_contract(path, size, mark,
         RETRO_VFS_FILE_ACCESS_HINT_NONE, what);
   check_eof_contract(path, size,
         RETRO_VFS_FILE_ACCESS_HINT_NONE, what);

   snprintf(what, sizeof(what), "%s size", name);
   check_size_agreement(path, size, what);

   remove(path);
}

int main(void)
{
   printf("sizeof(off_t) = %d\n", (int)sizeof(off_t));

   /* The contract cases first, on a file every filesystem can hold, so
    * that a machine which cannot build the large fixture still tests
    * the properties rather than skipping the whole run. */
   run_fixture("seek_small.bin", SMALL_SIZE, SMALL_MARK, "4 KiB");
   run_fixture("seek_4gib.bin",  HUGE_SIZE,  HUGE_MARK,  "4 GiB+4 KiB");

   if (test_fails)
      printf("\n%d check(s) failed\n", test_fails);
   else
      printf("\nall checks passed\n");

   return test_fails ? 1 : 0;
}
