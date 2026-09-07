/* Regression test for mmap-read integer overflow in
 * retro_vfs_file_read_impl (libretro-common/vfs/vfs_implementation.c).
 *
 * Pre-patch, the function contained:
 *
 *   if (stream->mappos + len > stream->mapsize)
 *       len = stream->mapsize - stream->mappos;
 *   memcpy(s, &stream->mapped[stream->mappos], len);
 *
 * mappos and len are both uint64_t.  When `len` is attacker-chosen
 * and near UINT64_MAX, the addition `mappos + len` wraps past zero
 * and the bound check `> mapsize` evaluates FALSE on the small
 * wrapped value -- the clamp is skipped and memcpy reads `len`
 * bytes off the end of the mapped region.
 *
 * Post-patch the clamp is done as an unsigned subtraction
 * (`remaining = mapsize - mappos; if (len > remaining) len =
 * remaining;`) which cannot wrap.
 *
 * The test mmaps a small file, then directly invokes
 * retro_vfs_file_read_impl with a len value engineered to trigger
 * the wrap (mappos=10, len=UINT64_MAX-9, sum wraps to 0).
 *
 * The function now also rejects any len above INT64_MAX at entry,
 * which covers every length capable of wrapping, so that call fails
 * outright.  The clamp is therefore exercised separately with the
 * largest admitted length, keeping both the guard and the clamp
 * under test.
 *
 * ASan gives the strongest signal: pre-patch runs it fires
 * "heap-buffer-overflow"; post-patch runs complete cleanly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>

/* These constants are defined in libretro.h; redeclare the subset
 * we need so the sample doesn't depend on that header being on the
 * include path. */
#ifndef RETRO_VFS_FILE_ACCESS_READ
#define RETRO_VFS_FILE_ACCESS_READ              (1 << 0)
#endif
#ifndef RETRO_VFS_FILE_ACCESS_HINT_NONE
#define RETRO_VFS_FILE_ACCESS_HINT_NONE         0
#endif
#ifndef RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS
#define RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS (1 << 0)
#endif

static int failures = 0;

static void test_mmap_read_overflow(void)
{
   libretro_vfs_implementation_file *stream;
   const char *tmp_path = "vfs_overflow_test.bin";
   const char  payload[16] = "ABCDEFGHIJKLMNOP";
   char        buf[64];
   int64_t     rc;
   FILE       *fp;

   /* Create a 16-byte file. */
   fp = fopen(tmp_path, "wb");
   if (!fp) { printf("[ERROR] fopen failed\n"); failures++; return; }
   fwrite(payload, 1, sizeof(payload), fp);
   fclose(fp);

   /* Open with frequent-access hint so the implementation uses the
    * mmap code path.  (If HAVE_MMAP is not compiled in, this
    * falls back to buffered reads and the test becomes a smoke
    * test rather than a true discriminator.) */
   stream = retro_vfs_file_open_impl(tmp_path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   if (!stream)
   {
      printf("[ERROR] retro_vfs_file_open_impl failed\n");
      failures++;
      remove(tmp_path);
      return;
   }

   /* Normal read to establish baseline behaviour. */
   rc = retro_vfs_file_read_impl(stream, buf, 4);
   if (rc != 4 || memcmp(buf, payload, 4) != 0)
   {
      printf("[ERROR] baseline read: rc=%lld, want 4\n", (long long)rc);
      failures++;
   }
   else
      printf("[SUCCESS] baseline read returned 4 bytes\n");

   /* Seek to offset 10.  mappos is now 10. */
   if (retro_vfs_file_seek_impl(stream, 10, 0 /*SEEK_SET*/) != 0 || retro_vfs_file_tell_impl(stream) != 10)
   {
      printf("[ERROR] seek to 10 failed\n");
      failures++;
   }

   /* Crafted len chosen so that (mappos + len) wraps uint64_t past 0
    * and lands at or below mapsize (=16), which is what a naive
    * "mappos + len > mapsize" bound check misses.
    *
    * With mappos=10, mapsize=16:
    *     Wrap happens when sum >= 2^64, i.e. len >= 2^64 - mappos.
    *     With mappos=10, pick len = UINT64_MAX - 9 so that sum =
    *     UINT64_MAX + 1 wraps to 0.  Check "0 > 16" is FALSE; the
    *     clamp would be SKIPPED and the memcpy would run off the end
    *     of the mapped region.
    *
    * Such a length no longer reaches the mapped path: the entry
    * guard rejects anything above INT64_MAX, which is every length
    * that can wrap, so the read fails and the wrap is unreachable
    * from any backend. */
   {
      uint64_t evil_len = (uint64_t)-1 - 9;    /* UINT64_MAX - 9 */
      rc = retro_vfs_file_read_impl(stream, buf, evil_len);

      if (rc != -1)
      {
         printf("[ERROR] wrapping read returned rc=%lld (want -1)\n",
               (long long)rc);
         failures++;
      }
      else
         printf("[SUCCESS] wrapping read rejected\n");
   }

   /* The clamp behind that guard is still what keeps an oversized but
    * acceptable length inside the mapping, so exercise it with the
    * largest length the guard admits.  mappos is still 10 because the
    * rejected read above never advanced it: remaining = 6, so the read
    * returns mapped[10..15] and ASan validates the memcpy bound. */
   {
      rc = retro_vfs_file_read_impl(stream, buf, (uint64_t)INT64_MAX);

      if (rc != 6 || memcmp(buf, payload + 10, 6) != 0)
      {
         printf("[ERROR] oversized read: rc=%lld, want 6\n",
               (long long)rc);
         failures++;
      }
      else
         printf("[SUCCESS] oversized read clamped to rc=%lld\n",
               (long long)rc);
   }

   retro_vfs_file_close_impl(stream);
   remove(tmp_path);
}

int main(void)
{
   test_mmap_read_overflow();

   if (failures)
   {
      printf("\n%d vfs test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll vfs regression tests passed.\n");
   return 0;
}
