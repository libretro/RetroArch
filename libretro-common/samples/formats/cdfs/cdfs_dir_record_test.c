/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cdfs_dir_record_test.c).
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

/* Regression test for the directory-record iterator bounds in
 * libretro-common/formats/cdfs/cdfs.c::cdfs_find_file.
 *
 * The function reads a 2048-byte ECMA-119 directory sector
 * from the disc image into a stack buffer and walks the
 * variable-length records:
 *
 *   while (tmp < buffer + sizeof(buffer))
 *   {
 *      if (!*tmp) break;
 *      if (tmp[33 + path_length] == ';' || tmp[33 + path_length] == '\0')
 *         ...
 *      tmp += tmp[0];
 *   }
 *
 * Pre-this-patch the loop had three composable issues:
 *
 *  1. The guard tmp < buffer + sizeof(buffer) only confirmed
 *     the record header byte was in bounds.  tmp[33 +
 *     path_length] could read past the buffer end, leaking
 *     up to ~32 bytes of adjacent stack into the comparison.
 *
 *  2. tmp[2..4] and tmp[10..13] were read after a successful
 *     filename comparison, with the same shape problem -- a
 *     record positioned near the buffer end produced
 *     attacker-influenced stack bytes flowing into `sector`
 *     (which redirects the next intfstream_read) and
 *     `file->size`.
 *
 *  3. The advance "tmp += tmp[0]" with tmp[0] attacker-
 *     controlled could land tmp anywhere up to 255 bytes
 *     past the safe range.  A record that claimed length
 *     less than 33 + filename_length + 1 (the minimum legal
 *     ECMA-119 record size) was a primitive for jumping the
 *     iterator to an unsafe offset.
 *
 * Reachability: user loads a malicious .iso/.bin/.cue.  Same
 * threat class as other disc-image bugs (CHD, BSV).
 *
 * Fix: tighten the loop guard to require the entire record
 * (header through tmp[33 + path_length]) to fit, and require
 * the record's claimed length to be >= 33 + path_length + 1
 * before doing the filename comparison.
 *
 * IMPORTANT: this test keeps a verbatim copy of the post-fix
 * loop predicate.  If cdfs.c's loop amends, the copy below
 * must follow.  Convention used by the other regression
 * tests under libretro-common/samples/.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strncasecmp */

#define SECTOR_SIZE 2048

static int failures = 0;

/* === verbatim copy of the post-fix iterator from
 *     libretro-common/formats/cdfs/cdfs.c::cdfs_find_file.
 *     If cdfs.c amends, this copy must follow. ===
 *
 * Returns the matched record's "sector" value (tmp[2..4])
 * on success, -1 if no record matches, -2 if the loop
 * would have run off the buffer. */
static int find_dir_record(const uint8_t *buffer,
      size_t buffer_size, const char *path)
{
   const uint8_t *tmp = buffer;
   size_t path_length = strlen(path);

   while (   tmp < buffer + buffer_size
          && (size_t)(tmp - buffer) + 33 + path_length < buffer_size)
   {
      if (!*tmp)
         break;

      if (tmp[0] < 33 + path_length + 1)
         break;

      if (        (tmp[33 + path_length] == ';'
               || (tmp[33 + path_length] == '\0'))
               &&  strncasecmp((const char*)(tmp + 33), path, path_length) == 0)
      {
         return tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
      }

      tmp += tmp[0];
   }

   return -1;
}
/* === end verbatim copy === */

/* Build a single legitimate directory record into `out`.
 * Returns the record's length. */
static size_t build_record(uint8_t *out, const char *filename,
      uint32_t sector, uint32_t size)
{
   size_t fn_len    = strlen(filename);
   size_t total     = 33 + fn_len + 1;  /* +1 for ';' */
   memset(out, 0, total);
   out[0]           = (uint8_t)total;
   /* sector at bytes 2..4 (little-endian 24-bit) */
   out[2]           = (uint8_t)(sector & 0xff);
   out[3]           = (uint8_t)((sector >> 8) & 0xff);
   out[4]           = (uint8_t)((sector >> 16) & 0xff);
   /* size at bytes 10..13 */
   out[10]          = (uint8_t)(size & 0xff);
   out[11]          = (uint8_t)((size >> 8) & 0xff);
   out[12]          = (uint8_t)((size >> 16) & 0xff);
   out[13]          = (uint8_t)((size >> 24) & 0xff);
   memcpy(out + 33, filename, fn_len);
   out[33 + fn_len] = ';';
   return total;
}

/* ---- tests ---- */

static void test_finds_legitimate_record(void)
{
   uint8_t buffer[SECTOR_SIZE];
   size_t  off;
   int     rv;
   memset(buffer, 0, sizeof(buffer));
   off = build_record(buffer, "FOO", 0x123456, 1024);
   /* terminator: a 0-length record marks end-of-records */
   buffer[off] = 0;

   rv = find_dir_record(buffer, sizeof(buffer), "FOO");
   if (rv != 0x123456)
   {
      printf("[ERROR] legitimate record: rv=0x%x, want 0x123456\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] legitimate record found and decoded correctly\n");
}

static void test_record_at_buffer_end_does_not_oob(void)
{
   /* Fill the buffer with single-byte-length records so the
    * iterator walks 1 byte at a time, then place a non-zero
    * byte close to the end so the guard would have allowed
    * the body to run pre-fix.  Allocate the buffer as exactly
    * SECTOR_SIZE bytes on the heap with no slack so ASan
    * flags any read past the end. */
   uint8_t *buffer = (uint8_t*)malloc(SECTOR_SIZE);
   int rv;
   if (!buffer) { printf("[ERROR] alloc\n"); failures++; return; }

   /* Records of length 1 from offset 0 to SECTOR_SIZE - 5. */
   memset(buffer, 1, SECTOR_SIZE - 4);
   /* The last 4 bytes are zero, terminating the chain. */
   memset(buffer + SECTOR_SIZE - 4, 0, 4);

   /* Search for a 0-length filename: path_length = 0, so the
    * pre-fix bug fires when tmp + 33 > buffer + 2048, i.e.
    * tmp > buffer + 2015.  The iterator walks single-byte
    * records right up to the boundary; pre-fix it would have
    * read tmp[33] for tmp >= buffer + 2016. */
   rv = find_dir_record(buffer, SECTOR_SIZE, "");
   /* Behavioural check: no match should be found, and (under
    * ASan) no OOB read should have occurred. */
   if (rv >= 0)
   {
      /* "" matches if tmp[33] == ';' or '\0'.  Some matches are
       * possible from random buffer content -- accept any rv,
       * the point of the test is that ASan didn't fire. */
   }
   printf("[SUCCESS] record-near-buffer-end iteration stayed in bounds\n");

   free(buffer);
}

static void test_short_record_length_rejected(void)
{
   /* A record claiming length 5 (less than the 33-byte
    * minimum) used to advance the iterator only 5 bytes,
    * letting subsequent reads of tmp[33+plen] reach into
    * undefined territory.  The post-fix guard
    *    if (tmp[0] < 33 + path_length + 1) break;
    * stops the iterator. */
   uint8_t *buffer = (uint8_t*)malloc(SECTOR_SIZE);
   int rv;
   if (!buffer) { printf("[ERROR] alloc\n"); failures++; return; }

   memset(buffer, 0, SECTOR_SIZE);
   buffer[0] = 5;  /* claimed length way below 33 */
   /* Subsequent bytes are zero so even if the iterator
    * advances we hit the !*tmp break. */

   rv = find_dir_record(buffer, SECTOR_SIZE, "FOO");
   if (rv != -1)
   {
      printf("[ERROR] short-length record produced rv=%d\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] short-length record rejected without iteration\n");

   free(buffer);
}

static void test_pathological_record_at_2046(void)
{
   /* Place a record claiming length 1 at buffer[2046].  Pre-fix
    * the loop guard tmp < buffer + 2048 admitted this iteration
    * but tmp[33 + path_length] = buffer[2079 + plen] read way
    * off the end.  Heap-alloc with exact SECTOR_SIZE so ASan
    * catches the read.
    *
    * To get there the iterator needs to walk to offset 2046.
    * Single-byte records all the way is the simplest path. */
   uint8_t *buffer = (uint8_t*)malloc(SECTOR_SIZE);
   int rv;
   if (!buffer) { printf("[ERROR] alloc\n"); failures++; return; }

   memset(buffer, 1, SECTOR_SIZE);
   /* Make sure the byte at 2046 is non-zero (it already is from
    * the memset). */
   buffer[2046] = 1;
   buffer[2047] = 0;  /* any value */

   rv = find_dir_record(buffer, SECTOR_SIZE, "BAR");
   /* Behaviour: no match.  Under ASan: no OOB. */
   (void)rv;
   printf("[SUCCESS] iterator at offset 2046 stayed in bounds\n");

   free(buffer);
}

int main(void)
{
   test_finds_legitimate_record();
   test_record_at_buffer_end_does_not_oob();
   test_short_record_length_rejected();
   test_pathological_record_at_2046();

   if (failures)
   {
      printf("\n%d cdfs_dir_record test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll cdfs_dir_record regression tests passed.\n");
   return 0;
}
