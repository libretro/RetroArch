/* Regression test for retro_vfs_file_read_cdrom cuesheet byte_pos
 * wrap (libretro-common/vfs/vfs_implementation_cdrom.c).
 *
 * Pre-patch, the cuesheet read path contained:
 *
 *   if ((int64_t)len >= (int64_t)stream->cdrom.cue_len
 *         - stream->cdrom.byte_pos)
 *       len = stream->cdrom.cue_len - stream->cdrom.byte_pos - 1;
 *   memcpy(s, stream->cdrom.cue_buf + stream->cdrom.byte_pos, len);
 *
 * cue_len is size_t, byte_pos is int64_t.  The subtraction
 * "cue_len - byte_pos - 1" is performed as unsigned (size_t).  If
 * byte_pos >= cue_len, the subtraction wraps to a huge size_t and
 * memcpy reads off the end of cue_buf.  byte_pos can be set to any
 * value by seek, which does not clamp.
 *
 * Post-patch, the read clamps against the remaining bytes with
 * unsigned subtraction *after* checking that byte_pos is in range,
 * and returns 0 (EOF) if byte_pos is out of range.
 *
 * Post-patch also returns 0 when the computed clamp would be
 * negative, rather than wrapping to SIZE_MAX.
 *
 * The test directly constructs a libretro_vfs_implementation_file
 * with a small cue_buf, sets byte_pos to a value >= cue_len, and
 * calls the read function.  Under ASan the pre-patch behaviour
 * fires heap-buffer-overflow; post-patch returns 0 cleanly.
 *
 * Note: this is a Linux-only test because HAVE_CDROM is the only
 * configuration that compiles the cuesheet code path.  The same
 * bug exists in the Windows branch of the same file but would
 * require Win32 headers to exercise directly.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <vfs/vfs.h>

/* Forward-declare the function we're testing.  The real declaration
 * lives in vfs_implementation_cdrom.h which requires various
 * platform headers we don't want to drag in for a unit test. */
int64_t retro_vfs_file_read_cdrom(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len);

static int failures = 0;

static void test_cuesheet_byte_pos_wrap(void)
{
   libretro_vfs_implementation_file stream;
   char out[64];
   int64_t rc;
   const char *fake_cue = "FILE \"track01.bin\" BINARY\nTRACK 01 MODE1/2048\n";
   size_t cue_size      = strlen(fake_cue) + 1;  /* incl NUL */

   memset(&stream, 0, sizeof(stream));

   stream.cdrom.cue_buf = strdup(fake_cue);
   if (!stream.cdrom.cue_buf)
   {
      printf("[ERROR] strdup failed\n");
      failures++;
      return;
   }
   stream.cdrom.cue_len = cue_size;

   /* orig_path must end in ".cue" for the read function to dispatch
    * into the cuesheet path.  The function calls path_get_extension
    * on it. */
   stream.orig_path = strdup("test.cue");
   if (!stream.orig_path)
   {
      free(stream.cdrom.cue_buf);
      printf("[ERROR] strdup failed (orig_path)\n");
      failures++;
      return;
   }

   /* Baseline: read at byte_pos=0 with small len should succeed. */
   stream.cdrom.byte_pos = 0;
   rc = retro_vfs_file_read_cdrom(&stream, out, 10);
   if (rc < 0 || rc > 10)
   {
      printf("[ERROR] baseline read: rc=%lld, want 0..10\n",
            (long long)rc);
      failures++;
   }
   else
      printf("[SUCCESS] baseline cuesheet read rc=%lld\n", (long long)rc);

   /* Attack 1: byte_pos at cue_len.  Pre-patch:
    *   cue_len - byte_pos - 1 = 0 - 1 = SIZE_MAX (size_t wrap)
    *   len = SIZE_MAX, memcpy reads gigabytes -> OOB.
    * Post-patch: byte_pos >= cue_len, return 0. */
   stream.cdrom.byte_pos = (int64_t)cue_size;
   rc = retro_vfs_file_read_cdrom(&stream, out, 10);
   if (rc != 0)
   {
      printf("[ERROR] byte_pos==cue_len: rc=%lld, want 0\n",
            (long long)rc);
      failures++;
   }
   else
      printf("[SUCCESS] byte_pos==cue_len returned 0\n");

   /* Attack 2: byte_pos past cue_len.  Pre-patch: same size_t wrap. */
   stream.cdrom.byte_pos = (int64_t)cue_size + 100;
   rc = retro_vfs_file_read_cdrom(&stream, out, 10);
   if (rc != 0)
   {
      printf("[ERROR] byte_pos > cue_len: rc=%lld, want 0\n",
            (long long)rc);
      failures++;
   }
   else
      printf("[SUCCESS] byte_pos > cue_len returned 0\n");

   /* Attack 3: negative byte_pos.  Pre-patch: cast to size_t gives
    * SIZE_MAX-ish, then cue_len - huge wraps around into a small
    * positive value, then memcpy reads gigabytes off cue_buf.
    * Post-patch: byte_pos < 0, return 0. */
   stream.cdrom.byte_pos = -1;
   rc = retro_vfs_file_read_cdrom(&stream, out, 10);
   if (rc != 0)
   {
      printf("[ERROR] byte_pos < 0: rc=%lld, want 0\n", (long long)rc);
      failures++;
   }
   else
      printf("[SUCCESS] byte_pos < 0 returned 0\n");

   /* Attack 4: huge len at legitimate byte_pos.  Pre-patch: the
    * (int64_t)len cast makes large len negative and the bound
    * check behaves unpredictably.  With len = UINT64_MAX the cast
    * is -1, and the check is "-1 >= cue_len - byte_pos" which for
    * small cue_len-byte_pos is false -- so the clamp is SKIPPED
    * and the full UINT64_MAX reaches memcpy.  OOB.
    * Post-patch: clamp is unsigned, len is compared to unsigned
    * remaining, clamp always fires correctly. */
   stream.cdrom.byte_pos = 5;
   rc = retro_vfs_file_read_cdrom(&stream, out, (uint64_t)-1);
   {
      int64_t max_expected = (int64_t)cue_size - 5 - 1;
      if (rc < 0 || rc > max_expected)
      {
         printf("[ERROR] huge len: rc=%lld, want 0..%lld\n",
               (long long)rc, (long long)max_expected);
         failures++;
      }
      else
         printf("[SUCCESS] huge len clamped to rc=%lld\n", (long long)rc);
   }

   free(stream.cdrom.cue_buf);
   free(stream.orig_path);
}

int main(void)
{
   test_cuesheet_byte_pos_wrap();

   if (failures)
   {
      printf("\n%d cdrom test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll cdrom regression tests passed.\n");
   return 0;
}
