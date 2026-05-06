/* Regression tests for libretro-db/rmsgpack.c.
 *
 * Exercises the attacker-controlled-length guards in
 * rmsgpack_read_buff (str8/str16/str32 + bin8/bin16/bin32),
 * rmsgpack_read_map (map16 + map32) and rmsgpack_read_array
 * (array16 + array32).  Pre-patch each of these accepted a uint
 * length field straight from the input and either
 *   (a) malloc'd len bytes (read_buff) -- a 5-byte STR32 header
 *       0xdb 0xff 0xff 0xff 0xfe demanded a ~4 GiB malloc that
 *       Linux overcommit happily granted, OOM-killing the
 *       process; or
 *   (b) calloc'd len * sizeof(struct) for the dom map/array
 *       items array (~80 bytes / pair) -- a 5-byte MAP32 header
 *       0xdf 0x10 0x00 0x00 0x00 demanded a ~21 GiB calloc.
 *
 * Post-patch each length is bounded against the remaining bytes
 * in the stream (a buffer can't legitimately claim more bytes
 * than are left, a map can't have more than remaining/2 pairs,
 * an array can't have more than remaining elements).  Streams
 * that don't know their size fall through unchecked, which
 * preserves behavior for any non-seekable consumer.
 *
 * Tests against in-memory streams (intfstream_open_memory), so
 * they work without any filesystem fixtures and the bound fires
 * because memory streams report a known size.
 *
 * Subtests:
 *   1. STR32 with len 0xFFFFFFFE on a 5-byte stream -- rejected.
 *   2. MAP32 with len 0x10000000 on a 5-byte stream -- rejected.
 *   3. ARRAY32 with len 0xFFFFFFFF on a 5-byte stream -- rejected.
 *   4. Truncated ARRAY16 claiming 10 entries with 5 bytes
 *      available -- rejected.
 *   5. Valid fixmap of 2 entries with valid contents -- accepted.
 *   6. Valid array of 4 fixints -- accepted.
 *   7. Valid maximal STR8 (len 0xFF) with that many bytes
 *      following -- accepted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <streams/interface_stream.h>

/* Avoid pulling in libretro.h for this sample.  Match the
 * approach used by libretro-common/samples/file/vfs. */
#ifndef RETRO_VFS_FILE_ACCESS_READ
#define RETRO_VFS_FILE_ACCESS_READ              (1 << 0)
#endif
#ifndef RETRO_VFS_FILE_ACCESS_HINT_NONE
#define RETRO_VFS_FILE_ACCESS_HINT_NONE         0
#endif

#include "../../rmsgpack.h"
#include "../../rmsgpack_dom.h"

static int failures = 0;

static int run_dom_read(const uint8_t *data, size_t len)
{
   intfstream_t *fd;
   struct rmsgpack_dom_value v;
   int rv;

   /* intfstream_open_memory takes ownership of nothing; the
    * caller's buffer must outlive the stream.  Pass our local
    * buffer directly. */
   fd = intfstream_open_memory((void *)data,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         len);
   if (!fd)
      return -2;

   rv = rmsgpack_dom_read(fd, &v);
   if (rv == 0)
      rmsgpack_dom_value_free(&v);
   intfstream_close(fd);
   /* intfstream_close closes the inner stream but does not free
    * the intfstream_t struct (existing libretro-common
    * convention -- see core_info.c, core_backup.c, cdfs.c which
    * also free after close).  ASan flags this as a 48-byte leak
    * if we don't free it ourselves. */
   free(fd);
   return rv;
}

static void test_str32_huge_len(void)
{
   /* STR32 with len = 0xFFFFFFFE.  Pre-patch: malloc(0xFFFFFFFF)
    * succeeds via overcommit on Linux, then the subsequent
    * (*pbuff)[read_len] = 0 page-faults the 4 GiB region.  Post-
    * patch the bound rejects len > remaining_bytes (5). */
   const uint8_t buf[] = { 0xdb, 0xff, 0xff, 0xff, 0xfe };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv == 0)
   {
      printf("[ERROR] STR32 with len=0xFFFFFFFE accepted on 5-byte stream\n");
      failures++;
   }
   else
      printf("[SUCCESS] STR32 with len=0xFFFFFFFE rejected\n");
}

static void test_map32_huge_len(void)
{
   /* MAP32 with len = 0x10000000 (2^28 entries).  Pre-patch the
    * dom_read_map_start callback calloc'd ~21 GiB. */
   const uint8_t buf[] = { 0xdf, 0x10, 0x00, 0x00, 0x00 };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv == 0)
   {
      printf("[ERROR] MAP32 with len=0x10000000 accepted on 5-byte stream\n");
      failures++;
   }
   else
      printf("[SUCCESS] MAP32 with len=0x10000000 rejected\n");
}

static void test_array32_huge_len(void)
{
   /* ARRAY32 with len = 0xFFFFFFFF.  Pre-patch the
    * dom_read_array_start callback calloc'd ~96 GiB. */
   const uint8_t buf[] = { 0xdd, 0xff, 0xff, 0xff, 0xff };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv == 0)
   {
      printf("[ERROR] ARRAY32 with len=0xFFFFFFFF accepted on 5-byte stream\n");
      failures++;
   }
   else
      printf("[SUCCESS] ARRAY32 with len=0xFFFFFFFF rejected\n");
}

static void test_array16_truncated(void)
{
   /* ARRAY16 claiming 10 entries with 5 fixint bytes following.
    * The header alone is 3 bytes (0xdc + 2-byte length); 10
    * fixints would be 10 more bytes.  We supply only 5, so the
    * bound (len > remaining) fires before the underlying parse
    * stumbles on truncated data. */
   const uint8_t buf[] = { 0xdc, 0x00, 0x0a, 1, 2, 3, 4, 5 };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv == 0)
   {
      printf("[ERROR] truncated ARRAY16 (claims 10, has 5) accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] truncated ARRAY16 rejected\n");
}

static void test_valid_fixmap(void)
{
   /* fixmap-2: { "foo": 42, "bar": "hi" } */
   const uint8_t buf[] = {
      0x82,
      0xa3, 'f', 'o', 'o',
      0x2a,
      0xa3, 'b', 'a', 'r',
      0xa2, 'h', 'i'
   };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv != 0)
   {
      printf("[ERROR] valid fixmap rejected (rv=%d)\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] valid fixmap accepted\n");
}

static void test_valid_array(void)
{
   /* ARRAY16 of 4 fixints */
   const uint8_t buf[] = { 0xdc, 0x00, 0x04, 1, 2, 3, 4 };
   int rv = run_dom_read(buf, sizeof(buf));
   if (rv != 0)
   {
      printf("[ERROR] valid ARRAY16-of-4 rejected (rv=%d)\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] valid ARRAY16-of-4 accepted\n");
}

static void test_valid_maximal_str8(void)
{
   /* STR8 with the maximum len that fits in a single byte
    * (0xFF), with 255 'a' bytes following.  Pre-patch this
    * worked too; the test confirms post-patch still accepts
    * the boundary. */
   uint8_t buf[2 + 0xFF];
   int rv;
   buf[0] = 0xd9;
   buf[1] = 0xff;
   memset(buf + 2, 'a', 0xff);
   rv = run_dom_read(buf, sizeof(buf));
   if (rv != 0)
   {
      printf("[ERROR] valid STR8 len=0xFF rejected (rv=%d)\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] valid STR8 len=0xFF accepted\n");
}

int main(void)
{
   test_str32_huge_len();
   test_map32_huge_len();
   test_array32_huge_len();
   test_array16_truncated();
   test_valid_fixmap();
   test_valid_array();
   test_valid_maximal_str8();

   if (failures)
   {
      printf("\n%d rmsgpack overflow test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll rmsgpack overflow regression tests passed.\n");
   return 0;
}
