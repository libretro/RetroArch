/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (input_remap_bounds_test.c).
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

/* Regression test for the input-remap analog-axis OOB-write
 * defences in configuration.c::input_remapping_load_file() and
 * input/input_driver.c (the analog_value[][8] indexing in the
 * remap-poll path).
 *
 * Pre-fix, two related primitives existed:
 *
 *   1. input_remapping_load_file fed config_get_int's return
 *      directly into settings->uints.input_remap_ids[i][j]
 *      without any range check.  A malformed .rmp file with
 *      e.g.  input_player1_a_btn = 999  set the stored value
 *      to 999.
 *
 *   2. The poll-time use site at input/input_driver.c (line
 *      ~7392 pre-fix, on the button -> analog branch) then
 *      did:
 *         handle->analog_value[i]
 *               [remap_button - RARCH_FIRST_CUSTOM_BIND] = ...
 *      with no bound check on remap_button at all, OOB-writing
 *      up to ~1007 elements (~2014 bytes) past the end of
 *      analog_value, into adjacent fields of input_mapper_t
 *      (keys[], key_button[], buttons[]).
 *
 *   3. The other use site at line ~7427 pre-fix (analog ->
 *      analog branch) had a bound check, but the bound used
 *      sizeof(handle->analog_value[i]) which is 16 bytes, not
 *      8 elements.  Indices 8..15 passed the check and OOB-
 *      wrote.
 *
 * The fix is two-pronged: load-time validation in
 * configuration.c rejects values outside [0, ANALOG_BIND_LIST_END)
 * (and clamps to RARCH_UNMAPPED), and the use sites in
 * input_driver.c bound on ARRAY_SIZE rather than sizeof.
 *
 * IMPORTANT: this test keeps verbatim copies of the post-fix
 * predicates from configuration.c and input/input_driver.c.
 * If those amend, the copies below must follow.  Following
 * the convention used by archive_name_safety_test and
 * http_method_match_test in this directory tree.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Mirror constants from input/input_defines.h.  Production
 * values, not test fakes -- if these ever change in
 * input_defines.h, update here too. */
#define RARCH_FIRST_CUSTOM_BIND        16
#define RARCH_ANALOG_BIND_LIST_END     24
#define RARCH_UNMAPPED                 1024

/* Mirror: array size of analog_value's inner dimension from
 * input/input_types.h (int16_t analog_value[MAX_USERS][8]). */
#define ANALOG_VALUE_PER_USER 8

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static int failures = 0;

/* === verbatim copy of the load-time clamp from
 *     configuration.c::input_remapping_load_file().  Returns
 *     the value that would be stored in
 *     settings->uints.input_remap_ids[][] given a raw
 *     config_get_int result. === */
static int validate_loaded_remap(int _remap)
{
   if (_remap == -1)
      _remap = RARCH_UNMAPPED;

   if (   _remap != (int)RARCH_UNMAPPED
       && (_remap < 0 || _remap >= (int)RARCH_ANALOG_BIND_LIST_END))
      _remap = RARCH_UNMAPPED;

   return _remap;
}
/* === end verbatim copy === */

/* Mirror analog_value layout for two users.  Sized so that an
 * OOB write past user 0's slice goes into user 1's slice
 * (which is what the production layout does), and ASan flags
 * a write past the entire allocation. */
typedef struct {
   int16_t analog_value[2][ANALOG_VALUE_PER_USER];
} mock_mapper_t;

/* === verbatim copy of the analog-axis remap-write block from
 *     input/input_driver.c, the button -> analog branch (line
 *     ~7392 in the patched code).  The function here is a
 *     thin wrapper around the inner write so the test can
 *     drive it with arbitrary remap_button values. === */
static int do_button_to_analog_write(mock_mapper_t *handle,
      unsigned i, unsigned remap_button, int16_t button_value)
{
   unsigned remap_axis_bind = remap_button - RARCH_FIRST_CUSTOM_BIND;
   int      invert          = 1;

   if (remap_axis_bind >= ARRAY_SIZE(handle->analog_value[i]))
      return 0;  /* skipped: out of range */

   if (remap_button % 2 != 0)
      invert = -1;

   handle->analog_value[i][remap_axis_bind] = button_value * invert;
   return 1;  /* wrote */
}
/* === end verbatim copy === */

/* === verbatim copy of the analog-axis remap-write block from
 *     input/input_driver.c, the analog -> analog branch (line
 *     ~7427 in the patched code). === */
static int do_analog_to_analog_write(mock_mapper_t *handle,
      unsigned i, unsigned remap_axis, int16_t axis_value)
{
   unsigned remap_axis_bind = remap_axis - RARCH_FIRST_CUSTOM_BIND;

   if (remap_axis_bind < ARRAY_SIZE(handle->analog_value[i]))
   {
      handle->analog_value[i][remap_axis_bind] = axis_value;
      return 1;
   }
   return 0;
}
/* === end verbatim copy === */

/* ---- tests ------------------------------------------------ */

static void test_validate_accepts_legitimate_button_indices(void)
{
   int i;
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (validate_loaded_remap(i) != i)
      {
         printf("[ERROR] validate clamped legitimate button %d\n", i);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] validate accepts legitimate button indices [0, 16)\n");
}

static void test_validate_accepts_legitimate_analog_indices(void)
{
   int i;
   for (i = RARCH_FIRST_CUSTOM_BIND; i < RARCH_ANALOG_BIND_LIST_END; i++)
   {
      if (validate_loaded_remap(i) != i)
      {
         printf("[ERROR] validate clamped legitimate analog %d\n", i);
         failures++;
         return;
      }
   }
   printf("[SUCCESS] validate accepts legitimate analog indices [16, 24)\n");
}

static void test_validate_accepts_unmapped_sentinel(void)
{
   /* -1 in the file -> RARCH_UNMAPPED in storage */
   if (validate_loaded_remap(-1) != RARCH_UNMAPPED)
   {
      printf("[ERROR] validate did not convert -1 to RARCH_UNMAPPED\n");
      failures++;
   }
   /* RARCH_UNMAPPED literal also accepted */
   else if (validate_loaded_remap(RARCH_UNMAPPED) != RARCH_UNMAPPED)
   {
      printf("[ERROR] validate clamped RARCH_UNMAPPED\n");
      failures++;
   }
   else
      printf("[SUCCESS] validate accepts -1 / RARCH_UNMAPPED sentinels\n");
}

static void test_validate_clamps_out_of_range(void)
{
   /* Values just outside the legal range and pathological large
    * values must all clamp to RARCH_UNMAPPED. */
   const int bad_values[] = {
      RARCH_ANALOG_BIND_LIST_END,           /* first invalid */
      999,                                  /* nominal attacker value */
      RARCH_UNMAPPED - 1,                   /* one below sentinel */
      RARCH_UNMAPPED + 1,                   /* one above sentinel */
      0x7FFFFFFF,                           /* INT_MAX */
      -2,                                   /* below -1 sentinel */
      -1000,                                /* large negative */
   };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(bad_values); i++)
   {
      int got = validate_loaded_remap(bad_values[i]);
      if (got != RARCH_UNMAPPED)
      {
         printf("[ERROR] validate(%d) = %d, expected RARCH_UNMAPPED\n",
               bad_values[i], got);
         failures++;
      }
   }
   if (i == ARRAY_SIZE(bad_values))
      printf("[SUCCESS] validate clamps %zu out-of-range values to RARCH_UNMAPPED\n",
            ARRAY_SIZE(bad_values));
}

static void test_button_to_analog_in_range(void)
{
   /* Valid remap_button values are RARCH_FIRST_CUSTOM_BIND ..
    * RARCH_ANALOG_BIND_LIST_END - 1, i.e. 16..23. */
   mock_mapper_t handle = {0};
   unsigned k;
   for (k = RARCH_FIRST_CUSTOM_BIND; k < RARCH_ANALOG_BIND_LIST_END; k++)
   {
      memset(&handle, 0, sizeof(handle));
      if (!do_button_to_analog_write(&handle, 0, k, 32767))
      {
         printf("[ERROR] in-range remap_button %u skipped\n", k);
         failures++;
         return;
      }
      /* Check the right slot was written.  invert is -1 for odd
       * remap_button (per production logic). */
      {
         unsigned slot = k - RARCH_FIRST_CUSTOM_BIND;
         int      sign = (k % 2 != 0) ? -1 : 1;
         int16_t  expected = (int16_t)(32767 * sign);
         if (handle.analog_value[0][slot] != expected)
         {
            printf("[ERROR] remap_button %u wrote %d to slot %u, expected %d\n",
                  k, handle.analog_value[0][slot], slot, expected);
            failures++;
            return;
         }
      }
   }
   printf("[SUCCESS] in-range remap_button writes go to the correct slot\n");
}

static void test_button_to_analog_out_of_range_no_oob(void)
{
   /* The bug-trigger values: remap_button = 24..1023.  Pre-fix
    * these wrote up to (1023 - 16) = 1007 elements past
    * analog_value[0].  To make ASan flag OOB writes, allocate
    * the mapper on the heap with the exact bytes the runtime
    * uses (no slack), so any write past index 7 of user 0's
    * row hits invalid-by-malloc-bounds territory under ASan
    * for the very first OOB index. */
   const unsigned bad[] = {
      RARCH_ANALOG_BIND_LIST_END,           /* 24 */
      31,
      100,
      RARCH_UNMAPPED - 1,                   /* 1023 */
   };
   size_t i;
   for (i = 0; i < ARRAY_SIZE(bad); i++)
   {
      /* Allocate a *single-row* mock so any write past
       * analog_value[0][7] runs off the end of the heap
       * allocation and ASan flags it. */
      mock_mapper_t *handle = (mock_mapper_t*)
            malloc(sizeof(int16_t) * ANALOG_VALUE_PER_USER);
      assert(handle);
      memset(handle, 0, sizeof(int16_t) * ANALOG_VALUE_PER_USER);
      if (do_button_to_analog_write(handle, 0, bad[i], 32767))
      {
         printf("[ERROR] out-of-range remap_button %u was written\n", bad[i]);
         failures++;
         free(handle);
         return;
      }
      free(handle);
   }
   printf("[SUCCESS] out-of-range remap_button writes correctly skipped\n");
}

static void test_analog_to_analog_sizeof_vs_array_size_bug(void)
{
   /* Pre-fix the bound was sizeof(analog_value[i]) which is 16
    * (bytes), so remap_axis_bind in [8, 15] passed the check
    * and indexed analog_value[i][8..15] -- OOB.  Post-fix the
    * bound is ARRAY_SIZE which is 8.
    *
    * Test indices 8..15 are rejected.  Allocated to ANALOG_VALUE_
    * PER_USER elements only so ASan flags any reintroduction. */
   unsigned k;
   for (k = RARCH_FIRST_CUSTOM_BIND + ANALOG_VALUE_PER_USER;
        k < RARCH_FIRST_CUSTOM_BIND + 16;
        k++)
   {
      mock_mapper_t *handle = (mock_mapper_t*)
            malloc(sizeof(int16_t) * ANALOG_VALUE_PER_USER);
      assert(handle);
      memset(handle, 0, sizeof(int16_t) * ANALOG_VALUE_PER_USER);
      if (do_analog_to_analog_write(handle, 0, k, 12345))
      {
         printf("[ERROR] historical-bug remap_axis %u was written\n", k);
         failures++;
         free(handle);
         return;
      }
      free(handle);
   }
   printf("[SUCCESS] sizeof-vs-ARRAY_SIZE bug correctly fixed\n");
}

int main(void)
{
   test_validate_accepts_legitimate_button_indices();
   test_validate_accepts_legitimate_analog_indices();
   test_validate_accepts_unmapped_sentinel();
   test_validate_clamps_out_of_range();
   test_button_to_analog_in_range();
   test_button_to_analog_out_of_range_no_oob();
   test_analog_to_analog_sizeof_vs_array_size_bug();

   if (failures)
   {
      printf("\n%d input_remap_bounds test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll input_remap_bounds regression tests passed.\n");
   return 0;
}
