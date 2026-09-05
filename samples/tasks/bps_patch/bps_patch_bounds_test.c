/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bps_patch_bounds_test.c).
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

/* Regression test for the .bps patch parser bounds in
 * tasks/task_patch.c::bps_apply_patch.
 *
 * The .bps format is a variable-length-integer-driven
 * stream of action commands operating on three buffers
 * (modify, source, target).  Pre-this-patch the action
 * loop performed every read/write without bounds:
 *
 *   case TARGET_READ:
 *     while (_len--) {
 *       uint8_t data = bps_read(&bps);
 *       bps.target_data[bps.output_offset++] = data;   // OOB write
 *       ...
 *     }
 *
 *   case SOURCE_COPY:
 *     bps.source_offset += offset;   // unbounded signed offset
 *     while (_len--) {
 *       uint8_t data = bps.source_data[bps.source_offset++];   // OOB read
 *       bps.target_data[bps.output_offset++] = data;   // OOB write
 *       ...
 *     }
 *
 * A malicious .bps patch could write attacker-chosen bytes
 * past the malloc'd target_data buffer (heap-buffer-overflow
 * WRITE), read past source_data (heap-info leak that flows
 * into target_data and the patch checksum, exposing heap
 * contents to a process that reads the patched output), and
 * read past modify_data via TARGET_READ when the per-command
 * _len exceeded the remaining bytes.
 *
 * Plus the size-prelude: the three variable-length integers
 * decoded immediately after the "BPS1" header (modify_source_
 * size, modify_target_size, modify_markup_size) had no upper
 * bound.  modify_target_size silently truncated to size_t on
 * 32-bit hosts producing a smaller allocation than
 * bps.target_length expected; modify_markup_size of UINT64_MAX
 * drove the markup-skip loop into reading the entire patch
 * buffer and beyond.
 *
 * Reachability: user has soft-patching enabled, places a
 * malicious .bps next to a ROM (or downloads a "ROM pack"
 * containing patches).  Same threat class as CDFS / CHD / BSV
 * file-load bugs.
 *
 * Fix: bound _len against (target_length - output_offset)
 * once at the top of each loop iteration; bound source/target
 * offset+_len against the respective buffers; bound the size
 * prelude against SIZE_MAX and remaining patch bytes.
 *
 * IMPORTANT: this test keeps verbatim copies of the post-fix
 * predicates from tasks/task_patch.c.  If task_patch.c amends,
 * the copies below must follow.  Convention used by
 * archive_name_safety_test, http_method_match_test,
 * video_shader_wildcard_test, input_remap_bounds_test,
 * bsv_replay_bounds_test in this directory tree.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

enum bps_mode_t { SOURCE_READ = 0, TARGET_READ, SOURCE_COPY, TARGET_COPY };

struct mock_bps {
   const uint8_t *modify_data;
   const uint8_t *source_data;
   uint8_t       *target_data;
   uint64_t       modify_length;
   uint64_t       source_length;
   uint64_t       target_length;
   uint64_t       modify_offset;
   uint64_t       source_offset;
   uint64_t       target_offset;
   uint64_t       output_offset;
};

static int failures = 0;

static uint8_t bps_read(struct mock_bps *b)
{
   /* Test scope: assume caller has bounded the read.  If a
    * test would have caused this to OOB, the post-fix
    * predicate should have rejected the patch first. */
   return b->modify_data[b->modify_offset++];
}

static uint64_t bps_decode(struct mock_bps *b)
{
   uint64_t data = 0, shift = 1;
   for (;;)
   {
      uint8_t x = bps_read(b);
      data += (x & 0x7f) * shift;
      if (x & 0x80) break;
      shift <<= 7;
      data += shift;
   }
   return data;
}

/* === verbatim copy of the post-fix action loop predicates
 *     from tasks/task_patch.c::bps_apply_patch.  Returns 0
 *     on success, non-zero on a bounds-rejection (the
 *     production code returns specific patch_error values;
 *     this mock returns generic non-zero).  If task_patch.c
 *     amends the predicates, this copy must follow. === */
static int apply_action_loop(struct mock_bps *bps)
{
   while (bps->modify_offset < bps->modify_length - 12)
   {
      uint64_t _len = bps_decode(bps);
      unsigned mode = _len & 3;
      _len          = (_len >> 2) + 1;

      if (   bps->output_offset >= bps->target_length
          || _len               >  bps->target_length - bps->output_offset)
         return 1;  /* PATCH_TARGET_INVALID */

      switch (mode)
      {
         case SOURCE_READ:
            if (bps->output_offset + _len > bps->source_length)
               return 2;  /* PATCH_SOURCE_INVALID */
            while (_len--)
            {
               uint8_t data = bps->source_data[bps->output_offset];
               bps->target_data[bps->output_offset++] = data;
            }
            break;

         case TARGET_READ:
            if (bps->modify_offset + _len > bps->modify_length - 12)
               return 3;  /* PATCH_PATCH_INVALID */
            while (_len--)
            {
               uint8_t data = bps_read(bps);
               bps->target_data[bps->output_offset++] = data;
            }
            break;

         case SOURCE_COPY:
         case TARGET_COPY:
         {
            int    offset = (int)bps_decode(bps);
            bool   negative = offset & 1;
            offset >>= 1;
            if (negative)
               offset = -offset;

            if (mode == SOURCE_COPY)
            {
               bps->source_offset += offset;
               if (   bps->source_offset > bps->source_length
                   || _len               > bps->source_length - bps->source_offset)
                  return 2;
               while (_len--)
               {
                  uint8_t data = bps->source_data[bps->source_offset++];
                  bps->target_data[bps->output_offset++] = data;
               }
            }
            else
            {
               bps->target_offset += offset;
               if (   bps->target_offset > bps->target_length
                   || _len               > bps->target_length - bps->target_offset)
                  return 1;
               while (_len--)
               {
                  uint8_t data = bps->target_data[bps->target_offset++];
                  bps->target_data[bps->output_offset++] = data;
               }
               break;
            }
            break;
         }
      }
   }
   return 0;
}
/* === end verbatim copy === */

/* Helper: run the post-fix predicates against a patch+source+
 * target setup, with target_data heap-allocated to exactly
 * the legitimate target_length so ASan flags any OOB write. */
static int run_apply(const uint8_t *patch, size_t patch_len,
      const uint8_t *src, size_t src_len, size_t target_len)
{
   struct mock_bps bps;
   uint8_t *target;
   int      rv;

   if (!(target = (uint8_t*)malloc(target_len ? target_len : 1)))
      return -1;
   memset(target, 0, target_len ? target_len : 1);

   memset(&bps, 0, sizeof(bps));
   bps.modify_data   = patch;
   bps.source_data   = src;
   bps.target_data   = target;
   bps.modify_length = patch_len;
   bps.source_length = src_len;
   bps.target_length = target_len;
   /* Skip the "BPS1" header + size prelude.  The test
    * harness sets modify_offset directly; in production the
    * same offset is reached by the header read and three
    * bps_decode calls before the action loop. */
   bps.modify_offset = 7;  /* "BPS1" + 0x80, 0x80, 0x80 (zero sizes) */

   rv = apply_action_loop(&bps);

   free(target);
   return rv;
}

/* Build a minimal valid header + 3 zero size declarations
 * starting at out, returning the bytes written. */
static size_t put_header(uint8_t *out)
{
   out[0] = 'B'; out[1] = 'P'; out[2] = 'S'; out[3] = '1';
   out[4] = 0x80;  /* source size = 0 (single-byte: high bit set, value 0) */
   out[5] = 0x80;  /* target size = 0 */
   out[6] = 0x80;  /* markup size = 0 */
   return 7;
}

/* ---- tests ------------------------------------------------ */

static void test_target_read_oob_write_rejected(void)
{
   /* Craft: TARGET_READ command with _len = 32 (encoded as 0xFD).
    * Target buffer is 4 bytes.  Pre-fix: 32 bytes written into a
    * 4-byte allocation (heap-buffer-overflow WRITE).  Post-fix:
    * the top-of-loop bound rejects with PATCH_TARGET_INVALID. */
   uint8_t patch[64];
   size_t off = put_header(patch);
   int    i, rv;
   patch[off++] = 0xFD;  /* mode=1 (TARGET_READ), _len = (125>>2)+1 = 32 */
   for (i = 0; i < 32; i++)
      patch[off++] = (uint8_t)(0xAA + i);
   for (i = 0; i < 12; i++)
      patch[off++] = 0;  /* trailing checksums (12 bytes) */

   rv = run_apply(patch, off, NULL, 0, 4);
   if (rv == 0)
   {
      printf("[ERROR] TARGET_READ with _len=32 into target=4 was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] TARGET_READ over-length rejected without OOB write\n");
}

static void test_source_copy_oob_read_rejected(void)
{
   /* SOURCE_COPY with offset = 100 against a source of 8 bytes.
    * After the offset, source_offset = 100 > source_length = 8
    * which the new bound rejects.  Pre-fix this would have run
    * source_data[100++] for _len iterations -- heap-buffer-
    * overflow READ. */
   uint8_t patch[64], src[8] = {1,2,3,4,5,6,7,8};
   size_t off = put_header(patch);
   int rv, i;
   /* Encode: mode=SOURCE_COPY (2), _len=4.
    * stored_len = ((4-1) << 2) | 2 = 14 = 0x0E.
    * High bit not set, so it's not a single-byte encoding.
    * For the test we want a single-byte command so just use
    * 0x8E (14 + 0x80). bps_decode: x=0x8E, x&0x7F=0x0E, data=14,
    * x&0x80=>break.  So _len=4, mode=2. */
   patch[off++] = 0x8E;
   /* offset = 200 (positive, so encoded value = 200<<1 = 400).
    * 400 > 127 so multi-byte: low 7 bits = 400 & 0x7f = 16 (0x10),
    * second byte = (400>>7)+something... easier to use a small
    * positive offset directly.  offset = 100 -> encoded = 200.
    * 200 > 127 so 2-byte encoding:
    *   first byte: 200 & 0x7F = 72 = 0x48 (high bit clear)
    *   shift <<= 7 (=128), data += 128 => data = 72 + 128 = 200
    *   ... wait that's the decoder, not the encoder.
    * For decoder result 200:
    *   byte 1: x1 (high bit clear), data += x1 * 1 = x1, then shift=128, data += 128
    *   byte 2: x2 (high bit set), data += (x2 & 0x7F) * 128
    *   want data = 200
    *   200 = x1 + 128 + (x2 & 0x7F) * 128
    *      = x1 + 128 * (1 + (x2 & 0x7F))
    *   if x2 & 0x7F = 0, then 200 = x1 + 128, x1 = 72.
    *   So bytes: 0x48, 0x80 (x1=0x48, x2=0x80).  Verify:
    *   data=0, shift=1; read 0x48, data += 0x48*1 = 72, no high bit, shift<<=7 (=128), data += 128 = 200, loop;
    *   read 0x80, data += 0 * 128 = 200, high bit => break.  ✓ */
   patch[off++] = 0x48;
   patch[off++] = 0x80;
   /* trailing 12 bytes */
   for (i = 0; i < 12; i++)
      patch[off++] = 0;

   rv = run_apply(patch, off, src, sizeof(src), 256);
   if (rv == 0)
   {
      printf("[ERROR] SOURCE_COPY with offset=100 vs src=8 was accepted\n");
      failures++;
   }
   else
      printf("[SUCCESS] SOURCE_COPY over-length rejected without OOB read\n");
}

static void test_legitimate_target_read_succeeds(void)
{
   /* TARGET_READ with _len=4 into target=4. */
   uint8_t patch[64];
   size_t off = put_header(patch);
   int rv, i;
   /* _len=4: stored = (3<<2)|1 = 13.  Single byte 0x8D. */
   patch[off++] = 0x8D;
   patch[off++] = 'A'; patch[off++] = 'B';
   patch[off++] = 'C'; patch[off++] = 'D';
   for (i = 0; i < 12; i++)
      patch[off++] = 0;

   rv = run_apply(patch, off, NULL, 0, 4);
   if (rv != 0)
   {
      printf("[ERROR] legitimate TARGET_READ rv=%d\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] legitimate TARGET_READ accepted\n");
}

static void test_legitimate_source_copy_succeeds(void)
{
   uint8_t patch[64], src[8] = {1,2,3,4,5,6,7,8};
   size_t off = put_header(patch);
   int rv, i;
   /* SOURCE_COPY mode=2, _len=4 -> stored = (3<<2)|2 = 14, byte 0x8E */
   patch[off++] = 0x8E;
   /* offset = 0: encoded value = 0, single-byte 0x80 */
   patch[off++] = 0x80;
   for (i = 0; i < 12; i++)
      patch[off++] = 0;

   rv = run_apply(patch, off, src, sizeof(src), 8);
   if (rv != 0)
   {
      printf("[ERROR] legitimate SOURCE_COPY rv=%d\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] legitimate SOURCE_COPY accepted\n");
}

static void test_target_read_at_exact_capacity(void)
{
   /* TARGET_READ with _len exactly equal to target_length.
    * Should succeed (boundary). */
   uint8_t patch[64];
   size_t off = put_header(patch);
   int rv, i;
   /* _len=8: stored=(7<<2)|1=29, byte 0x9D */
   patch[off++] = 0x9D;
   for (i = 0; i < 8; i++)
      patch[off++] = 'X';
   for (i = 0; i < 12; i++)
      patch[off++] = 0;

   rv = run_apply(patch, off, NULL, 0, 8);
   if (rv != 0)
   {
      printf("[ERROR] boundary TARGET_READ (_len=target_length=8) rejected, rv=%d\n", rv);
      failures++;
   }
   else
      printf("[SUCCESS] boundary TARGET_READ (_len=target_length) accepted\n");
}

int main(void)
{
   test_target_read_oob_write_rejected();
   test_source_copy_oob_read_rejected();
   test_legitimate_target_read_succeeds();
   test_legitimate_source_copy_succeeds();
   test_target_read_at_exact_capacity();

   if (failures)
   {
      printf("\n%d bps_patch_bounds test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll bps_patch_bounds regression tests passed.\n");
   return 0;
}
