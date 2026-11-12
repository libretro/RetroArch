/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_positional_off32_test.c).
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

/* Positional reads and writes where off_t is 32 bits (32-bit Linux
 * without _FILE_OFFSET_BITS=64, 32-bit Android before API 21): this
 * links a vfs_implementation.o built with VFS_POSITIONAL_OFF_T=int32_t,
 * which is that build's arithmetic on a 64-bit host.
 *
 * - at an offset past 4 GiB, read_at answers -1 or the bytes there -
 *   never the bytes 4 GiB lower, where a cast to 32 bits lands;
 * - write_at past 4 GiB answers -1 and changes nothing lower down;
 * - the seek and read the caller falls back to reaches the bytes;
 * - an offset that fits still reads at the offset.
 *
 * The fixture is a sparse 5 GiB file; a filesystem that will not hold
 * one, and Windows, where the positional path is ReOpenFile's and this
 * build changes nothing, skip. */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#if defined(_WIN32)
int main(void)
{
   printf("vfs positional off32: Windows, skipped\n");
   return 0;
}
#else
#include <fcntl.h>
#include <unistd.h>

#define HIGH_AT  (((int64_t)9) << 29)   /* 4.5 GiB */
#define LOW_AT   (HIGH_AT - (((int64_t)1) << 32))
#define SIZE     (((int64_t)5) << 30)

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: check failed: %s\n", \
      __FILE__, __LINE__, #c); failures++; } } while (0)

static const char path[] = "vfs_positional_off32.bin";

/* 'L' at 4.5 GiB - 4 GiB, 'H' at 4.5 GiB, written with the host's own
 * 64-bit offsets so the fixture does not depend on the code under test */
static int make_fixture(void)
{
   int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
   if (fd < 0)
      return 0;
   if (     sizeof(off_t) < sizeof(int64_t)
         || ftruncate(fd, (off_t)SIZE) != 0
         || pwrite(fd, "L", 1, (off_t)LOW_AT) != 1
         || pwrite(fd, "H", 1, (off_t)HIGH_AT) != 1)
   {
      close(fd);
      remove(path);
      return 0;
   }
   close(fd);
   return 1;
}

int main(void)
{
   libretro_vfs_implementation_file *f;
   unsigned char c = 0;
   int64_t r;

   if (!make_fixture())
   {
      printf("vfs positional off32: no 5 GiB sparse fixture here, skipped\n");
      return 0;
   }

   f = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL);
   if (f)
   {
      /* past 4 GiB: refused, or the right byte */
      r = retro_vfs_file_read_at_impl(f, &c, 1, HIGH_AT);
      CHECK(r == -1 || (r == 1 && c == 'H'));
      CHECK(c != 'L');

      /* a write there must not land 4 GiB lower */
      r = retro_vfs_file_write_at_impl(f, "X", 1, HIGH_AT);
      CHECK(r == -1 || r == 1);

      /* the fallback reaches it */
      c = 0;
      CHECK(retro_vfs_file_seek_impl(f, HIGH_AT,
               RETRO_VFS_SEEK_POSITION_START) >= 0);
      CHECK(retro_vfs_file_tell_impl(f) == HIGH_AT);
      CHECK(retro_vfs_file_read_impl(f, &c, 1) == 1);
      CHECK(c == 'H' || (r == 1 && c == 'X'));

      /* an offset that fits reads where it says */
      c = 0;
      r = retro_vfs_file_read_at_impl(f, &c, 1, LOW_AT);
#if defined(__linux__)
      CHECK(r == 1 && c == 'L');
#else
      CHECK(r == -1 || (r == 1 && c == 'L'));
#endif

      retro_vfs_file_close_impl(f);
   }

   remove(path);
   printf("vfs positional off32: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
#endif
