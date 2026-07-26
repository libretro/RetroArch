/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_rpng.c).
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

/* Regression coverage for rpng_process_ihdr's dimension and
 * size guards.
 *
 * The picture is two-layer:
 *
 *  - On all hosts a 4 GiB output guard (width*height*4) plus a
 *    4 GiB pass_size guard reject images whose decoded buffer
 *    cannot be addressed.  Together with the (size_t) casts at
 *    the per-row malloc sites these prevent the original heap
 *    overflow on any platform regardless of dimensions.
 *
 *  - On 32-bit hosts an additional 0x4000 (16384) dimension cap
 *    rejects images that would demand more than a few hundred MB
 *    of decoded pixels.  These would fail to allocate anyway on
 *    a 32-bit address space, but a tight cap turns the failure
 *    into a clean reject rather than a partially-set-up parser
 *    state.  64-bit hosts do not cap here, allowing legitimate
 *    large images (cf. IrfanView's tens-of-thousands-pixel
 *    routine support).
 *
 * Tests below are platform-gated to match.  The strict
 * regression cases (the 0x4001 / 30000-squared bug shapes) only
 * fire on 32-bit; 64-bit gets the looser sanity coverage. */

#include <check.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include <formats/rpng.h>

#define SUITE_NAME "rpng"

/* PNG file signature, replicated from rpng_internal.h (which is
 * not part of the public install set). */
static const uint8_t png_magic[8] = {
   0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
};

/* Build a minimal valid-shape PNG buffer containing the file
 * signature and a single IHDR chunk with the supplied dimensions,
 * followed by a trailing-padding chunk-header so that
 * rpng_iterate_image's post-IHDR pointer-advance check does not
 * push past buff_end on the same call (a successful IHDR-accept
 * call must leave buff_data <= buff_end so a subsequent iterate
 * could read the next chunk).  The CRC is set to zero - rpng's
 * iterate path does not validate IHDR CRC, so this is sufficient
 * to exercise rpng_process_ihdr. */
static size_t make_ihdr_only_png(uint8_t *out, size_t out_size,
      uint32_t width, uint32_t height,
      uint8_t depth, uint8_t color_type)
{
   /* 8 (magic) + 4 (length) + 4 (type) + 13 (IHDR data) + 4 (CRC)
    * + 8 (room for next chunk header) = 41 */
   size_t len = 0;
   if (out_size < 41)
      return 0;

   memcpy(out + len, png_magic, 8);
   len += 8;

   /* IHDR chunk length = 13, big-endian */
   out[len++] = 0; out[len++] = 0; out[len++] = 0; out[len++] = 13;

   /* "IHDR" */
   out[len++] = 'I'; out[len++] = 'H'; out[len++] = 'D'; out[len++] = 'R';

   /* width, big-endian */
   out[len++] = (uint8_t)(width  >> 24);
   out[len++] = (uint8_t)(width  >> 16);
   out[len++] = (uint8_t)(width  >>  8);
   out[len++] = (uint8_t)(width  >>  0);

   /* height, big-endian */
   out[len++] = (uint8_t)(height >> 24);
   out[len++] = (uint8_t)(height >> 16);
   out[len++] = (uint8_t)(height >>  8);
   out[len++] = (uint8_t)(height >>  0);

   out[len++] = depth;
   out[len++] = color_type;
   out[len++] = 0; /* compression */
   out[len++] = 0; /* filter */
   out[len++] = 0; /* interlace */

   /* CRC placeholder; rpng_iterate_image does not validate it */
   out[len++] = 0; out[len++] = 0; out[len++] = 0; out[len++] = 0;

   /* Trailing 8 bytes so the post-IHDR pointer advance leaves
    * buff_data <= buff_end (rpng_iterate_image returns false if
    * the advance pushes past the end).  Contents do not matter -
    * the test does not call rpng_iterate_image again. */
   out[len++] = 0; out[len++] = 0; out[len++] = 0; out[len++] = 0;
   out[len++] = 0; out[len++] = 0; out[len++] = 0; out[len++] = 0;

   return len;
}

/* Helper: try to parse an IHDR-only PNG with the supplied
 * dimensions and depth/color_type, returning the result of
 * rpng_iterate_image. */
static bool try_iterate(uint32_t w, uint32_t h, uint8_t depth, uint8_t ctype)
{
   uint8_t buf[64];
   size_t  len;
   rpng_t *rpng;
   bool    ret;

   len = make_ihdr_only_png(buf, sizeof(buf), w, h, depth, ctype);
   ck_assert(len > 0);

   rpng = rpng_alloc();
   ck_assert(rpng != NULL);
   ck_assert(rpng_set_buf_ptr(rpng, buf, len));
   ck_assert(rpng_start(rpng));

   ret = rpng_iterate_image(rpng);

   rpng_free(rpng);
   return ret;
}


/* ------------------------------------------------------------------ */
/* Abandoned-decode teardown sweep.
 *
 * A decode can be torn down from outside at ANY machine state: the
 * everyday case is a cancelled thumbnail task calling rpng_free while
 * iterate or process is mid-flight.  This class of defect does not
 * show up in completion-path tests at all: the caller-pointer walk
 * fixed in "rpng: never walk the caller's output pointer" survived
 * every byte-exactness harness and corrupted the heap only on
 * abandonment (Windows STATUS_HEAP_CORRUPTION out of
 * task_image_load_free).
 *
 * So: decode a small image once to completion for a reference
 * checksum, then abandon a fresh decode after every possible number
 * of iterate calls and after every possible number of process calls,
 * running a full fresh decode after each abandonment and asserting
 * the reference checksum still comes out.  The runtime assertion
 * catches state leakage; the memory assertions belong to the
 * sanitizer this suite is expected to also run under (ASan or
 * valgrind), which turns any interior free, double free, leak or
 * overflow in the teardown paths into a hard failure. */

static const uint8_t rpng_sweep_adam7[7][4] = {
   { 0, 0, 8, 8 }, { 4, 0, 8, 8 }, { 0, 4, 4, 8 }, { 2, 0, 4, 4 },
   { 0, 2, 2, 4 }, { 1, 0, 2, 2 }, { 0, 1, 1, 2 }
};

static size_t rpng_sweep_emit_chunk(uint8_t *out, const char *type,
      const uint8_t *payload, size_t plen)
{
   size_t len = 0;
   out[len++] = (uint8_t)(plen >> 24);
   out[len++] = (uint8_t)(plen >> 16);
   out[len++] = (uint8_t)(plen >>  8);
   out[len++] = (uint8_t)(plen >>  0);
   memcpy(out + len, type, 4);
   len += 4;
   if (plen)
      memcpy(out + len, payload, plen);
   len += plen;
   /* CRC unvalidated by the iterate path; zero. */
   out[len++] = 0; out[len++] = 0; out[len++] = 0; out[len++] = 0;
   return len;
}

/* 24x17 RGBA8, filter 0 on every row, deterministic pixels. */
#define RPNG_SWEEP_W 24
#define RPNG_SWEEP_H 17

static size_t rpng_sweep_build_png(uint8_t *out, size_t out_size,
      int interlace)
{
   uint8_t  raw[8 * 1024];
   uint8_t  ihdr[13];
   size_t   rlen = 0;
   size_t   len  = 0;
   uLongf   zlen;
   uint8_t  zbuf[16 * 1024];
   unsigned x, y;

   if (!interlace)
   {
      for (y = 0; y < RPNG_SWEEP_H; y++)
      {
         raw[rlen++] = 0;
         for (x = 0; x < RPNG_SWEEP_W; x++)
         {
            raw[rlen++] = (uint8_t)(x * 7 + y);
            raw[rlen++] = (uint8_t)(y * 5 + x);
            raw[rlen++] = (uint8_t)(x ^ y);
            raw[rlen++] = 255;
         }
      }
   }
   else
   {
      unsigned p;
      for (p = 0; p < 7; p++)
      {
         unsigned ox = rpng_sweep_adam7[p][0], oy = rpng_sweep_adam7[p][1];
         unsigned sx = rpng_sweep_adam7[p][2], sy = rpng_sweep_adam7[p][3];
         for (y = oy; y < RPNG_SWEEP_H; y += sy)
         {
            raw[rlen++] = 0;
            for (x = ox; x < RPNG_SWEEP_W; x += sx)
            {
               raw[rlen++] = (uint8_t)(x * 7 + y);
               raw[rlen++] = (uint8_t)(y * 5 + x);
               raw[rlen++] = (uint8_t)(x ^ y);
               raw[rlen++] = 255;
            }
         }
      }
   }

   zlen = (uLongf)sizeof(zbuf);
   if (compress2(zbuf, &zlen, raw, (uLong)rlen, 6) != Z_OK)
      return 0;
   if (out_size < 8 + 25 + (12 + zlen) + 12)
      return 0;

   memcpy(out + len, png_magic, 8);
   len += 8;

   ihdr[0]  = 0; ihdr[1]  = 0; ihdr[2]  = 0; ihdr[3]  = RPNG_SWEEP_W;
   ihdr[4]  = 0; ihdr[5]  = 0; ihdr[6]  = 0; ihdr[7]  = RPNG_SWEEP_H;
   ihdr[8]  = 8;                    /* depth       */
   ihdr[9]  = 6;                    /* RGBA        */
   ihdr[10] = 0; ihdr[11] = 0;
   ihdr[12] = (uint8_t)interlace;
   len += rpng_sweep_emit_chunk(out + len, "IHDR", ihdr, 13);
   len += rpng_sweep_emit_chunk(out + len, "IDAT", zbuf, (size_t)zlen);
   len += rpng_sweep_emit_chunk(out + len, "IEND", NULL, 0);
   return len;
}

/* Full decode; returns process_image's final code and the FNV-1a of
 * the ARGB output.  iterate_calls/process_calls report how many steps
 * each phase took (for sizing the sweep); either may be NULL. */
static int rpng_sweep_full_decode(uint8_t *buf, size_t len,
      uint64_t *fnv_out, int *iterate_calls, int *process_calls)
{
   rpng_t   *r    = rpng_alloc();
   uint32_t *data = NULL;
   unsigned  w = 0, h = 0;
   int       ret  = 0, ic = 0, pc = 0;
   uint64_t  fnv  = 1469598103934665603ULL;

   ck_assert(r != NULL);
   ck_assert(rpng_set_buf_ptr(r, buf, len));
   ck_assert(rpng_start(r));
   while (rpng_iterate_image(r))
      ic++;
   ic++;
   do
   {
      ret = rpng_process_image(r, (void**)&data, len, &w, &h, true);
      pc++;
   } while (ret == 0);

   if (ret == 1 && data)
   {
      size_t i;
      for (i = 0; i < (size_t)w * h * 4; i++)
         fnv = (fnv ^ ((uint8_t*)data)[i]) * 1099511628211ULL;
   }
   free(data);
   rpng_free(r);
   if (fnv_out)       *fnv_out       = fnv;
   if (iterate_calls) *iterate_calls = ic;
   if (process_calls) *process_calls = pc;
   return ret;
}

static void rpng_sweep_run(int interlace)
{
   uint8_t  png[24 * 1024];
   size_t   len;
   uint64_t ref_fnv = 0, fnv;
   int      ic = 0, pc = 0, k;

   len = rpng_sweep_build_png(png, sizeof(png), interlace);
   ck_assert(len > 0);

   ck_assert_int_eq(rpng_sweep_full_decode(png, len, &ref_fnv, &ic, &pc), 1);

   /* Abandon during the chunk walk, after every possible number of
    * iterate calls. */
   for (k = 1; k <= ic; k++)
   {
      rpng_t *r = rpng_alloc();
      int     j;
      ck_assert(r != NULL);
      ck_assert(rpng_set_buf_ptr(r, png, len));
      ck_assert(rpng_start(r));
      for (j = 0; j < k && rpng_iterate_image(r); j++) { }
      rpng_free(r);

      ck_assert_int_eq(rpng_sweep_full_decode(png, len, &fnv, NULL, NULL), 1);
      ck_assert(fnv == ref_fnv);
   }

   /* Abandon during processing, after every possible number of
    * process calls.  The output buffer belongs to the caller and is
    * freed here after every abandonment - exactly the
    * task_image_load_free pattern that detected the caller-pointer
    * walk as heap corruption. */
   for (k = 1; k <= pc; k++)
   {
      rpng_t   *r    = rpng_alloc();
      uint32_t *data = NULL;
      unsigned  w = 0, h = 0;
      int       j, ret = 0;
      ck_assert(r != NULL);
      ck_assert(rpng_set_buf_ptr(r, png, len));
      ck_assert(rpng_start(r));
      while (rpng_iterate_image(r)) { }
      for (j = 0; j < k && ret == 0; j++)
         ret = rpng_process_image(r, (void**)&data, len, &w, &h, true);
      free(data);
      rpng_free(r);

      ck_assert_int_eq(rpng_sweep_full_decode(png, len, &fnv, NULL, NULL), 1);
      ck_assert(fnv == ref_fnv);
   }
}

START_TEST (test_rpng_abandoned_decode_teardown_regular)
{
   rpng_sweep_run(0);
}
END_TEST

START_TEST (test_rpng_abandoned_decode_teardown_adam7)
{
   rpng_sweep_run(1);
}
END_TEST

START_TEST (test_rpng_ihdr_dimension_cap_accept_at_limit)
{
   /* 0x4000 == 16384.  Inclusive accept on every platform: on
    * 32-bit this is the boundary of the dimension cap; on 64-bit
    * there is no dimension cap and 16384x16384 RGBA8 is well
    * under the 4 GiB output guard. */
   ck_assert(try_iterate(0x4000u, 0x4000u, 8, 6));
}
END_TEST

#if SIZE_MAX <= 0xFFFFFFFFu
START_TEST (test_rpng_ihdr_dimension_cap_reject_just_over_32bit)
{
   /* 0x4001 must be rejected on 32-bit.  The pre-existing 4 GiB
    * output guard does not catch 16385x16385 RGBA8 (~1.07 GiB),
    * so the 0x4000 cap is what rejects it here.  This case does
    * NOT reproduce on 64-bit, where 16385x16385 is a legitimate
    * (large) image. */
   ck_assert(!try_iterate(0x4001u, 0x4000u, 8, 6));
   ck_assert(!try_iterate(0x4000u, 0x4001u, 8, 6));
   ck_assert(!try_iterate(0x4001u, 0x4001u, 8, 6));
}
END_TEST

START_TEST (test_rpng_ihdr_dimension_cap_reject_30000_squared_32bit)
{
   /* 30000x30000 RGBA8 is the historical worst case on 32-bit:
    * 3.35 GiB of decoded pixels, which on a 32-bit address space
    * cannot be allocated and pre-patch corrupted the heap when
    * the uint32 multiplication width*height*sizeof(uint32_t)
    * wrapped.  The 0x4000 cap catches this.  On 64-bit this is a
    * legitimate-but-large image, accepted by the IHDR guards. */
   ck_assert(!try_iterate(30000u, 30000u, 8, 6));
}
END_TEST
#endif

START_TEST (test_rpng_ihdr_size_cap_reject_uint32_max)
{
   /* PNG-spec maximum dimensions.  Rejected on every platform:
    * on 32-bit the 0x4000 cap catches it first; on 64-bit the
    * 4 GiB output guard does (the math overflows even with
    * 64-bit width arithmetic). */
   ck_assert(!try_iterate(0x7FFFFFFFu, 0x7FFFFFFFu, 8, 6));
   ck_assert(!try_iterate(0x7FFFFFFFu, 1u, 8, 6));
   ck_assert(!try_iterate(1u, 0x7FFFFFFFu, 8, 6));
}
END_TEST

START_TEST (test_rpng_ihdr_dimension_cap_accept_small)
{
   /* Sanity: small valid dimensions still parse on every
    * platform. */
   ck_assert(try_iterate(16u, 16u, 8, 6));
   ck_assert(try_iterate(1u, 1u, 8, 6));
   /* Other supported color/depth combinations at the 0x4000
    * boundary.  16384x16384 RGBA-16 is 2 GiB output -- under the
    * 4 GiB cap on every platform. */
   ck_assert(try_iterate(0x4000u, 0x4000u, 8, 2));   /* RGB */
   ck_assert(try_iterate(0x4000u, 0x4000u, 16, 6));  /* RGBA-16 */
}
END_TEST

START_TEST (test_rpng_ihdr_zero_dimensions_rejected)
{
   /* Pre-existing behavior: zero dimensions are rejected.
    * Verify the cap patch did not regress this. */
   ck_assert(!try_iterate(0u, 16u, 8, 6));
   ck_assert(!try_iterate(16u, 0u, 8, 6));
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_rpng_ihdr_dimension_cap_accept_at_limit);
#if SIZE_MAX <= 0xFFFFFFFFu
   tcase_add_test(tc_core, test_rpng_ihdr_dimension_cap_reject_just_over_32bit);
   tcase_add_test(tc_core, test_rpng_ihdr_dimension_cap_reject_30000_squared_32bit);
#endif
   tcase_add_test(tc_core, test_rpng_ihdr_size_cap_reject_uint32_max);
   tcase_add_test(tc_core, test_rpng_ihdr_dimension_cap_accept_small);
   tcase_add_test(tc_core, test_rpng_abandoned_decode_teardown_regular);
   tcase_add_test(tc_core, test_rpng_abandoned_decode_teardown_adam7);
   tcase_add_test(tc_core, test_rpng_ihdr_zero_dimensions_rejected);
   suite_add_tcase(s, tc_core);

   return s;
}

int main(void)
{
   int num_fail;
   Suite *s = create_suite();
   SRunner *sr = srunner_create(s);
   srunner_run_all(sr, CK_NORMAL);
   num_fail = srunner_ntests_failed(sr);
   srunner_free(sr);
   return (num_fail == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
