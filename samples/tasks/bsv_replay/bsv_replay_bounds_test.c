/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bsv_replay_bounds_test.c).
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

/* Regression test for .bsv replay-file bounds in input/bsv/bsvmovie.c.
 *
 * The .bsv replay format stores per-frame input events as:
 *   uint8_t  key_event_count;
 *   bsv_key_data_t   key_events[key_event_count];
 *   uint16_t input_event_count;       (versions > 0)
 *   bsv_input_data_t input_events[input_event_count];
 *
 * The runtime stores those into fixed-size arrays:
 *   bsv_key_data_t   key_events[128];        (struct bsv_movie)
 *   bsv_input_data_t input_events[512];      (struct bsv_movie)
 *
 * Pre-fix, the read loop iterated count-times into the
 * fixed-size array with no upper bound check.  A malformed
 * .bsv with key_event_count = 255 wrote up to (255-128)*12 =
 * 1524 bytes past key_events[]; with input_event_count =
 * 65535, up to (65535-512)*8 = 520184 bytes past
 * input_events[].  bsv_movie_t is heap-allocated via calloc
 * in tasks/task_movie.c so this was a heap-buffer-overflow
 * with attacker-chosen 8- or 12-byte payloads at attacker-
 * chosen offsets.
 *
 * Reachability: user opens a malicious .bsv file (Menu ->
 * Load Replay File or --bsvplay).  Same threat class as
 * other file-loading bugs (CDFS / CHD / BPS).
 *
 * Fix: in input/bsv/bsvmovie.c, reject the file as malformed
 * if either count exceeds its array size.  A legitimate
 * writer cannot produce more events than the array holds
 * because the same array backs the writer's append path.
 *
 * Version-2 replays also serialize separate unencoded, encoded, and
 * compressed encoded checkpoint sizes. For an uncompressed raw
 * checkpoint, the loader allocates cur_save using the unencoded size
 * then reads the compressed encoded size into it. A malformed replay
 * can therefore overflow cur_save unless compressed_encoded_size is
 * no larger than encoded_size and encoded_size is no larger than size.
 *
 * IMPORTANT: this test keeps verbatim copies of the post-fix
 * bound-check predicates. If input/bsv/bsvmovie.c amends a predicate,
 * the copy below must follow. Convention used by
 * archive_name_safety_test, http_method_match_test,
 * video_shader_wildcard_test, input_remap_bounds_test in
 * this directory tree.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Mirror constants from input/input_driver.h. */
#define KEY_EVENTS_CAP    128
#define INPUT_EVENTS_CAP  512
#define REPLAY_COMPRESSION_NONE 0
#define REPLAY_ENCODING_RAW     0

/* Mirror layout of bsv_key_data and bsv_input_data from
 * input/input_driver.h lines 221-240. */
typedef struct {
   uint8_t  down;
   uint8_t  _padding;
   uint16_t mod;
   uint32_t code;
   uint32_t character;
} mock_key_data_t;

typedef struct {
   uint8_t  port;
   uint8_t  device;
   uint8_t  idx;
   uint8_t  _padding;
   uint16_t id;
   int16_t  value;
} mock_input_data_t;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int failures = 0;

/* === verbatim copy of the post-fix bound-check predicate
 *     from input/bsv/bsvmovie.c.  Returns 1 if the count is
 *     valid, 0 if it would have OOB-written.  If
 *     bsvmovie.c's predicate amends, this copy must follow.
 *     === */
static int key_event_count_in_bounds(unsigned key_event_count,
      unsigned key_events_cap)
{
   if (key_event_count > key_events_cap)
      return 0;
   return 1;
}

static int input_event_count_in_bounds(unsigned input_event_count,
      unsigned input_events_cap)
{
   if (input_event_count > input_events_cap)
      return 0;
   return 1;
}

static int checkpoint_sizes_in_bounds(uint32_t compression,
      uint32_t encoding, uint32_t compressed_encoded_size,
      uint32_t encoded_size, uint32_t size)
{
   if (   compression == REPLAY_COMPRESSION_NONE
       && compressed_encoded_size > encoded_size)
      return 0;
   if (   encoding == REPLAY_ENCODING_RAW
       && encoded_size > size)
      return 0;
   return 1;
}
/* === end verbatim copy === */

static void write_le32(uint8_t *dst, uint32_t value)
{
   dst[0] = (uint8_t)(value >> 0);
   dst[1] = (uint8_t)(value >> 8);
   dst[2] = (uint8_t)(value >> 16);
   dst[3] = (uint8_t)(value >> 24);
}

static uint32_t read_le32(const uint8_t *src)
{
   return ((uint32_t)src[0] << 0)
      | ((uint32_t)src[1] << 8)
      | ((uint32_t)src[2] << 16)
      | ((uint32_t)src[3] << 24);
}

/* Model the raw, uncompressed checkpoint read in
 * bsv_movie_load_checkpoint(). The checkpoint starts at its three
 * serialized size fields; the compression and encoding are supplied
 * by the enclosing replay record. ASan catches the memcpy below if
 * the production bounds predicate is removed. */
static int simulate_raw_checkpoint_load(const uint8_t *checkpoint,
      size_t checkpoint_len)
{
   uint32_t size;
   uint32_t encoded_size;
   uint32_t compressed_encoded_size;
   uint8_t *save;

   if (checkpoint_len < 3 * sizeof(uint32_t))
      return 0;

   size                    = read_le32(checkpoint + 0);
   encoded_size            = read_le32(checkpoint + 4);
   compressed_encoded_size = read_le32(checkpoint + 8);

   if (   !checkpoint_sizes_in_bounds(REPLAY_COMPRESSION_NONE,
               REPLAY_ENCODING_RAW, compressed_encoded_size,
               encoded_size, size)
       || compressed_encoded_size > checkpoint_len - 3 * sizeof(uint32_t))
      return 0;

   save = (uint8_t*)malloc(size);
   if (!save)
      return 0;

   memcpy(save, checkpoint + 3 * sizeof(uint32_t),
         compressed_encoded_size);
   free(save);
   return 1;
}

/* Drive a write loop the way the production read path does.
 * Returns 1 if all writes succeeded, 0 if the bound rejected.
 * Used to confirm under ASan that out-of-range counts no
 * longer cause OOB writes when the bound fires.
 *
 * Buffer is sized exactly to the cap so any reintroduction of
 * the unbounded loop is flagged by ASan on the first OOB
 * index. */
static int simulate_input_event_read(unsigned count)
{
   /* Allocate exactly the production cap (no slack) so ASan
    * flags the first OOB index. */
   mock_input_data_t *events = (mock_input_data_t*)
         malloc(sizeof(mock_input_data_t) * INPUT_EVENTS_CAP);
   unsigned i;
   if (!events)
      return 0;

   if (!input_event_count_in_bounds(count, INPUT_EVENTS_CAP))
   {
      free(events);
      return 0;  /* correctly rejected */
   }

   for (i = 0; i < count; i++)
   {
      events[i].port    = 1;
      events[i].device  = 2;
      events[i].idx     = 3;
      events[i].id      = 0xCAFE;
      events[i].value   = 0x1234;
   }

   free(events);
   return 1;
}

static int simulate_key_event_read(unsigned count)
{
   mock_key_data_t *events = (mock_key_data_t*)
         malloc(sizeof(mock_key_data_t) * KEY_EVENTS_CAP);
   unsigned i;
   if (!events)
      return 0;

   if (!key_event_count_in_bounds(count, KEY_EVENTS_CAP))
   {
      free(events);
      return 0;
   }

   for (i = 0; i < count; i++)
   {
      events[i].down       = 1;
      events[i].mod        = 0;
      events[i].code       = 0x1234;
      events[i].character  = 'A';
   }

   free(events);
   return 1;
}

/* ---- tests ------------------------------------------------ */

static void test_key_count_legitimate(void)
{
   /* All counts up to and including the cap must succeed. */
   unsigned counts[] = { 0, 1, 64, 127, 128 };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(counts); i++)
   {
      if (!simulate_key_event_read(counts[i]))
      {
         printf("[ERROR] legitimate key_event_count %u rejected\n",
               counts[i]);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] legitimate key_event_counts (0..128) accepted\n");
}

static void test_key_count_oob_rejected(void)
{
   /* Any uint8 value above 128 must reject before any write. */
   unsigned counts[] = { 129, 200, 254, 255 };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(counts); i++)
   {
      if (simulate_key_event_read(counts[i]))
      {
         printf("[ERROR] OOB key_event_count %u was not rejected\n",
               counts[i]);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] OOB key_event_counts (129..255) rejected without writes\n");
}

static void test_input_count_legitimate(void)
{
   unsigned counts[] = { 0, 1, 256, 511, 512 };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(counts); i++)
   {
      if (!simulate_input_event_read(counts[i]))
      {
         printf("[ERROR] legitimate input_event_count %u rejected\n",
               counts[i]);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] legitimate input_event_counts (0..512) accepted\n");
}

static void test_input_count_oob_rejected(void)
{
   /* The pathological values: just-over-cap and uint16 max. */
   unsigned counts[] = { 513, 1024, 32768, 65534, 65535 };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(counts); i++)
   {
      if (simulate_input_event_read(counts[i]))
      {
         printf("[ERROR] OOB input_event_count %u was not rejected\n",
               counts[i]);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] OOB input_event_counts (513..65535) rejected without writes\n");
}

static void test_checkpoint_overflow_rejected(void)
{
   const uint32_t compressed_encoded_size = 4096;
   const size_t checkpoint_len = 3 * sizeof(uint32_t)
      + compressed_encoded_size;
   uint8_t *checkpoint = (uint8_t*)calloc(1, checkpoint_len);

   if (!checkpoint)
   {
      printf("[ERROR] failed to allocate checkpoint test input\n");
      failures++;
      return;
   }

   /* A version-2 raw/uncompressed checkpoint with a one-byte state
    * but 4096 encoded bytes. Before the production check, loading
    * this exact size prelude copied 4096 bytes into a one-byte heap
    * allocation. */
   write_le32(checkpoint + 0, 1);
   write_le32(checkpoint + 4, 1);
   write_le32(checkpoint + 8, compressed_encoded_size);

   if (simulate_raw_checkpoint_load(checkpoint, checkpoint_len))
   {
      printf("[ERROR] oversized checkpoint was not rejected\n");
      failures++;
   }
   else
      printf("[SUCCESS] oversized checkpoint rejected before copy\n");

   free(checkpoint);
}

static void test_checkpoint_smaller_sizes_accepted(void)
{
   if (   !checkpoint_sizes_in_bounds(REPLAY_COMPRESSION_NONE,
               REPLAY_ENCODING_RAW, 8, 12, 16)
       || !checkpoint_sizes_in_bounds(1, REPLAY_ENCODING_RAW, 64, 12, 16))
   {
      printf("[ERROR] smaller checkpoint sizes were rejected\n");
      failures++;
      return;
   }
   printf("[SUCCESS] smaller checkpoint sizes accepted\n");
}

int main(void)
{
   test_key_count_legitimate();
   test_key_count_oob_rejected();
   test_input_count_legitimate();
   test_input_count_oob_rejected();
   test_checkpoint_overflow_rejected();
   test_checkpoint_smaller_sizes_accepted();

   if (failures)
   {
      printf("\n%d bsv_replay_bounds test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll bsv_replay_bounds regression tests passed.\n");
   return 0;
}
