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
