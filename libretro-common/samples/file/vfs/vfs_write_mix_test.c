/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_write_mix_test.c).
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

/* A stream written the way savers, config writers and patchers write
 * it, in every mode a stream is opened to write in: runs of small
 * writes, writes of every size up to past 64 KiB, writes past the end
 * that leave a gap, seeks of every kind, reads back, positions and
 * sizes after each, flushes, truncates shorter and longer, holes
 * punched, reads at offsets, and closing and opening it again to update it. A copy of
 * the file kept here says what every read, position and size must be,
 * and what the file holds when it is closed. On Windows this is the
 * stream's own buffer holding the writes (VFS_WIN32_NO_NATIVE: the C
 * runtime's). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>
#include <streams/file_stream.h>

#define PATH      "vfs_write_mix_test.bin"
#define MODEL_MAX (2u << 20)
#define POS_MAX   (1u << 20)
#define ROUNDS    12000

static int      failures;
static uint8_t *model;
static uint8_t *scratch;
static uint64_t msize;
static uint64_t mpos;
static uint32_t seed = 0x2545f491u;

#define CHECK(c) do { if (!(c)) { fprintf(stderr, \
      "%s:%d: round %u, mode %u: check failed: %s\n", __FILE__, __LINE__, \
      round_no, mode_no, #c); failures++; } } while (0)

static unsigned round_no;
static unsigned mode_no;

static uint32_t rnd(uint32_t n)
{
   seed ^= seed << 13;
   seed ^= seed >> 17;
   seed ^= seed << 5;
   return n ? seed % n : 0;
}

/* @len bytes written at @at of the copy, zeroes in any gap before them */
static void model_write(uint64_t at, const uint8_t *s, uint64_t len)
{
   if (at > msize)
      memset(model + msize, 0, (size_t)(at - msize));
   memcpy(model + at, s, (size_t)len);
   if (at + len > msize)
      msize = at + len;
}

static void model_truncate(uint64_t len)
{
   if (len > msize)
      memset(model + msize, 0, (size_t)(len - msize));
   msize = len;
}

static void fill(uint8_t *p, uint64_t len)
{
   uint64_t i;
   uint8_t  b = (uint8_t)rnd(256);
   for (i = 0; i < len; i++)
      p[i] = (uint8_t)(b + i * 7 + (i >> 9));
}

/* The file as it stands on disk, read without the stream */
static void check_disk(void)
{
   FILE    *f = fopen(PATH, "rb");
   uint64_t got;
   if (!f)
   {
      CHECK(f != NULL);
      return;
   }
   got = (uint64_t)fread(scratch, 1, MODEL_MAX, f);
   fclose(f);
   CHECK(got == msize);
   CHECK(got != msize || memcmp(scratch, model, (size_t)got) == 0);
}

static void run_mode(unsigned mode, unsigned reopen_mode, bool readable,
      bool creates)
{
   RFILE   *f;
   unsigned r;

   if (!creates)
   {
      /* an update opens what is there */
      FILE *seedf = fopen(PATH, "wb");
      msize = 4096 + rnd(70000);
      fill(model, msize);
      if (seedf)
      {
         fwrite(model, 1, (size_t)msize, seedf);
         fclose(seedf);
      }
   }
   else
      msize = 0;
   mpos = 0;
   if (!(f = filestream_open(PATH, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      CHECK(f != NULL);
      return;
   }
   CHECK(filestream_get_size(f) == (int64_t)msize);

   for (r = 0; r < ROUNDS; r++)
   {
      uint32_t op = rnd(100);
      round_no    = r;
      if (op < 35 || (op < 48 && mpos >= POS_MAX))
      {
         /* a run of small writes */
         uint64_t len = 1 + rnd(300);
         if (mpos + len > MODEL_MAX)
            continue;
         fill(scratch, len);
         CHECK(filestream_write(f, scratch, (int64_t)len) == (int64_t)len);
         model_write(mpos, scratch, len);
         mpos += len;
      }
      else if (op < 48)
      {
         /* up to past the buffer; one in four at least as large */
         uint64_t len = (op < 45) ? 300 + rnd(70000)
            : 65536 + rnd(140000);
         if (mpos + len > MODEL_MAX)
            continue;
         fill(scratch, len);
         CHECK(filestream_write(f, scratch, (int64_t)len) == (int64_t)len);
         model_write(mpos, scratch, len);
         mpos += len;
      }
      else if (op < 60)
      {
         uint64_t to = rnd((uint32_t)(msize + 100000));
         if (to > POS_MAX)
            to = rnd(POS_MAX);
         CHECK(filestream_seek(f, (int64_t)to,
                  RETRO_VFS_SEEK_POSITION_START) >= 0);
         mpos = to;
      }
      else if (op < 65)
      {
         int64_t d = (int64_t)rnd(140000) - 70000;
         if ((int64_t)mpos + d < 0 || mpos + (uint64_t)(d < 0 ? 0 : d) > POS_MAX)
            continue;
         CHECK(filestream_seek(f, d, RETRO_VFS_SEEK_POSITION_CURRENT) >= 0);
         mpos = (uint64_t)((int64_t)mpos + d);
      }
      else if (op < 68)
      {
         int64_t d = -(int64_t)rnd((uint32_t)msize + 1) + (int64_t)rnd(5000);
         if ((int64_t)msize + d < 0 || msize + (uint64_t)(d < 0 ? 0 : d) > POS_MAX)
            continue;
         CHECK(filestream_seek(f, d, RETRO_VFS_SEEK_POSITION_END) >= 0);
         mpos = (uint64_t)((int64_t)msize + d);
      }
      else if (op < 80)
      {
         uint64_t len  = 1 + ((op & 1) ? rnd(300) : rnd(90000));
         uint64_t want = mpos >= msize ? 0
            : (msize - mpos < len ? msize - mpos : len);
         int64_t  got  = filestream_read(f, scratch, (int64_t)len);
         if (!readable)
            CHECK(got <= 0);
         else
         {
            CHECK(got == (int64_t)want);
            CHECK(got != (int64_t)want
                  || memcmp(scratch, model + mpos, (size_t)want) == 0);
            if (got > 0)
               mpos += (uint64_t)got;
         }
      }
      else if (op < 84)
         CHECK(filestream_tell(f) == (int64_t)mpos);
      else if (op < 87)
         CHECK(filestream_get_size(f) == (int64_t)msize);
      else if (op < 89)
      {
         CHECK(filestream_flush(f) == 0);
         CHECK(filestream_tell(f) == (int64_t)mpos);
      }
      else if (op < 92)
      {
         uint64_t to = rnd((uint32_t)(msize + 50000));
         if (to > POS_MAX)
            continue;
         CHECK(filestream_truncate(f, (int64_t)to) == 0);
         model_truncate(to);
         CHECK(filestream_get_size(f) == (int64_t)msize);
         CHECK(filestream_tell(f) == (int64_t)mpos);
      }
      else if (op < 93)
      {
         /* zeroes where the filesystem can free a range, the size kept;
          * -1 where it cannot */
         uint64_t at  = rnd((uint32_t)(msize + 1000));
         uint64_t len = 1 + rnd(150000);
         if (filestream_punch_hole(f, (int64_t)at, (int64_t)len) == 0
               && at < msize)
            memset(model + at, 0, (size_t)(msize - at < len
                     ? msize - at : len));
         CHECK(filestream_get_size(f) == (int64_t)msize);
         CHECK(filestream_tell(f) == (int64_t)mpos);
      }
      else if (op < 96)
      {
         /* at an offset, from the file itself: what was written goes
          * there first */
         uint64_t at   = rnd((uint32_t)(msize + 1000));
         uint64_t len  = 1 + rnd(80000);
         uint64_t want = at >= msize ? 0
            : (msize - at < len ? msize - at : len);
         int64_t  got;
         CHECK(filestream_flush(f) == 0);
         got = filestream_read_at(f, scratch, (int64_t)len, (int64_t)at);
         /* -1 is a stream with no reads at offsets */
         CHECK(got == -1 || got == (int64_t)want);
         CHECK(got != (int64_t)want
               || memcmp(scratch, model + at, (size_t)want) == 0);
         CHECK(filestream_tell(f) == (int64_t)mpos);
      }
      else
      {
         CHECK(filestream_close(f) == 0);
         check_disk();
         if (!(f = filestream_open(PATH, reopen_mode,
                     RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         {
            CHECK(f != NULL);
            return;
         }
         readable = true; /* every update stream reads */
         mpos     = 0;
         CHECK(filestream_get_size(f) == (int64_t)msize);
      }
   }
   CHECK(filestream_close(f) == 0);
   check_disk();
}

/* Writes the stream still holds, then a hole punched and a truncate
 * over them: they go into the file first, so what is left of them is
 * the zeroes and the cut */
static void check_held_writes(void)
{
   RFILE   *f;
   uint8_t  b[4000];
   int      punched;
   round_no = 0;
   mode_no  = 4;
   memset(b, 0xaa, sizeof(b));
   if (!(f = filestream_open(PATH, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      CHECK(f != NULL);
      return;
   }
   CHECK(filestream_write(f, b, sizeof(b)) == (int64_t)sizeof(b));
   punched = filestream_punch_hole(f, 0, 2000) == 0;
   CHECK(filestream_write(f, b, sizeof(b)) == (int64_t)sizeof(b));
   CHECK(filestream_truncate(f, 6000) == 0);
   CHECK(filestream_close(f) == 0);
   memset(model, 0xaa, 6000);
   if (punched)
      memset(model, 0, 2000);
   msize = 6000;
   check_disk();
}

int main(void)
{
   if (     !(model   = (uint8_t*)malloc(MODEL_MAX))
         || !(scratch = (uint8_t*)malloc(MODEL_MAX)))
      return 2;

   check_held_writes();
   mode_no = 0;
   run_mode(RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         false, true);
   mode_no = 1;
   run_mode(RETRO_VFS_FILE_ACCESS_READ_WRITE,
         RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, true, true);
   mode_no = 2;
   run_mode(RETRO_VFS_FILE_ACCESS_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         true, false);
   mode_no = 3;
   run_mode(RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, true, false);

   remove(PATH);
   free(model);
   free(scratch);
   if (failures)
   {
      printf("vfs_write_mix_test: %d check(s) failed\n", failures);
      return 1;
   }
   printf("vfs_write_mix_test: every read, position, size and file matched\n");
   return 0;
}
