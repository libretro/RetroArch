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
 * the wrap (mappos=10, len=UINT64_MAX-10, sum wraps to 0).  Post-
 * patch the function returns at most 0 (no bytes left after mappos
 * advanced to mapsize) and does not corrupt memory.
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
   if (retro_vfs_file_seek_impl(stream, 10, 0 /*SEEK_SET*/) != 10)
   {
      printf("[ERROR] seek to 10 failed\n");
      failures++;
   }

   /* Crafted len chosen so that (mappos + len) wraps uint64_t past 0
    * and lands at or below mapsize (=16), bypassing the pre-patch
    * bound check and causing an unclamped memcpy off the end.
    *
    * With mappos=10, mapsize=16:
    *   naive check: mappos + len > mapsize
    *     We need (mappos + len) to wrap past zero in uint64_t.
    *     Wrap happens when sum >= 2^64, i.e. len >= 2^64 - mappos.
    *     With mappos=10, pick len = UINT64_MAX - 9 so that sum =
    *     UINT64_MAX + 1 wraps to 0.  Check "0 > 16" is FALSE; the
    *     clamp is SKIPPED.  Memcpy then reads UINT64_MAX - 9 bytes
    *     starting at mapped[10].  Crash / OOB.
    *
    * Post-patch: remaining = mapsize - mappos = 6.  len (very
    * large) > remaining, so len clamps to 6.  memcpy reads 6 bytes
    * from mapped[10..15].  Safe. */
   {
      uint64_t evil_len = (uint64_t)-1 - 9;    /* UINT64_MAX - 9 */
      rc = retro_vfs_file_read_impl(stream, buf, evil_len);

      /* Post-patch contract: rc is at most mapsize - mappos = 6.
       * Pre-patch would either return a huge number or crash. */
      if (rc < 0 || rc > 6)
      {
         printf("[ERROR] overflow read returned rc=%lld (want 0..6)\n",
               (long long)rc);
         failures++;
      }
      else
         printf("[SUCCESS] overflow read clamped to rc=%lld\n",
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
