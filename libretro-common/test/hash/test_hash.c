/* Copyright  (C) 2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_hash.c).
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

#include <check.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <lrc_hash.h>

#define SUITE_NAME "hash"

START_TEST (test_sha256)
{
   char output[65];
   sha256_hash(output, (uint8_t*)"abc", 3);
   ck_assert(!strcmp(output,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}
END_TEST

START_TEST (test_sha1)
{
   char output[41];
   char tmpfile[512];
   FILE *fd;
   tmpnam(tmpfile);
   fd = fopen(tmpfile, "wb");
   ck_assert(fd != NULL);
   fwrite("abc", 1, 3, fd);
   fclose(fd);
   sha1_calculate(tmpfile, output);

   ck_assert(!strcmp(output,
      "A9993E364706816ABA3E25717850C26C9CD0D89D"));
}
END_TEST

/* Lengths straddling the point where the padding stops fitting in the
 * block it started in: 55 leaves exactly enough room for the length
 * octets, 56 does not, and 119 repeats 55 one block further in. */
START_TEST (test_sha_padding_boundary)
{
   char    output[65];
   uint8_t buf[119];
   uint8_t digest[20];
   unsigned i;

   memset(buf, 'a', sizeof(buf));

   sha256_hash(output, buf, 55);
   ck_assert(!strcmp(output,
      "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
   sha256_hash(output, buf, 56);
   ck_assert(!strcmp(output,
      "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
   sha256_hash(output, buf, 119);
   ck_assert(!strcmp(output,
      "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb"));

   SHA1Digest(buf, 55, digest);
   for (i = 0; i < 20; i++)
      snprintf(output + 2 * i, 3, "%02X", (unsigned)digest[i]);
   ck_assert(!strcmp(output, "C1C8BBDC22796E28C0E15163D20899B65621D65A"));

   SHA1Digest(buf, 56, digest);
   for (i = 0; i < 20; i++)
      snprintf(output + 2 * i, 3, "%02X", (unsigned)digest[i]);
   ck_assert(!strcmp(output, "C2DB330F6083854C99D4B5BFB6E8F29F201BE699"));

   SHA1Digest(buf, 119, digest);
   for (i = 0; i < 20; i++)
      snprintf(output + 2 * i, 3, "%02X", (unsigned)digest[i]);
   ck_assert(!strcmp(output, "EE971065AAA017E0632A8CA6C77BB3BF8B1DFC56"));
}
END_TEST

/* SHA1Digest() takes whole blocks where they lie and SHA1DigestMany()
 * runs two messages side by side; both against the stream fed in
 * small pieces, which goes a block at a time through its own buffer,
 * at the lengths either side of each padding case, with a stride that
 * leaves the messages unaligned. */
START_TEST (test_sha1_many)
{
   static const size_t lens[] = { 0, 1, 55, 56, 63, 64, 65, 119, 120, 160, 620, 1024, 1500 };
   static const unsigned counts[] = { 1, 2, 3, 8, 31 };
   uint8_t *buf = (uint8_t*)malloc(31 * 1507 + 1);
   uint8_t  got[31 * 20], want[20], one[20];
   unsigned i, j, k, m;

   ck_assert(buf != NULL);
   for (i = 0; i < 31 * 1507 + 1; i++)
      buf[i] = (uint8_t)(i * 131 + (i >> 7));
   for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++)
      for (j = 0; j < sizeof(counts) / sizeof(counts[0]); j++)
      {
         size_t stride = lens[i] + 7;
         SHA1DigestMany(buf + 1, stride, lens[i], counts[j], got);
         for (k = 0; k < counts[j]; k++)
         {
            struct sha1_state st;
            const uint8_t *msg = buf + 1 + k * stride;
            size_t at = 0;
            sha1_stream_init(&st);
            for (m = 1; at < lens[i]; m = m * 3 % 17 + 1)
            {
               size_t piece = lens[i] - at < m ? lens[i] - at : m;
               sha1_stream_update(&st, msg + at, piece);
               at += piece;
            }
            ck_assert(sha1_stream_final(&st, want));
            SHA1Digest(msg, lens[i], one);
            ck_assert_msg(!memcmp(one, want, 20),
                  "SHA1Digest, length %u", (unsigned)lens[i]);
            ck_assert_msg(!memcmp(got + 20 * k, want, 20),
                  "SHA1DigestMany, length %u, message %u of %u",
                  (unsigned)lens[i], k, counts[j]);
         }
      }
   free(buf);

   /* and one long enough to take many blocks in a run */
   {
      char     output[41];
      uint8_t  digest[20];
      uint8_t *a = (uint8_t*)malloc(1000000);
      ck_assert(a != NULL);
      memset(a, 'a', 1000000);
      SHA1Digest(a, 1000000, digest);
      for (i = 0; i < 20; i++)
         snprintf(output + 2 * i, 3, "%02X", (unsigned)digest[i]);
      ck_assert(!strcmp(output, "34AA973CD4C4DAA4F61EEB2BDBAD27316534016F"));
      free(a);
   }
}
END_TEST

START_TEST (test_djb2)
{
   ck_assert_uint_eq(djb2_calculate("retroarch"), 0xFADF3BCF);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_sha256);
   tcase_add_test(tc_core, test_sha1);
   tcase_add_test(tc_core, test_sha_padding_boundary);
   tcase_add_test(tc_core, test_sha1_many);
   tcase_add_test(tc_core, test_djb2);
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
