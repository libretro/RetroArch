/* Copyright  (C) 2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (test_stdstring.c).
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

#include <utils/md5.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>

#define SUITE_NAME "hash"

START_TEST (test_md5)
{
   uint8_t output[16];
   MD5_CTX ctx;
   MD5_Init(&ctx);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e",
      output, 16));
   MD5_Init(&ctx);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6",
      output, 16));
   MD5_Init(&ctx);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Update(&ctx, "The quick brown fox jumps over the lazy dog", 43);
   MD5_Final(output, &ctx);
   ck_assert(!memcmp(
      "\x4e\x67\xdb\x4a\x7a\x40\x6b\x0c\xfd\xad\xd8\x87\xcd\xe7\x88\x8e",
      output, 16));
}
END_TEST

START_TEST (test_crc32)
{
   char buf1[] = "retroarch";
   char buf2[] = "12345678";
   char buf3[] = "The quick brown fox jumps over the lazy dog";
   uint32_t test1 = encoding_crc32(0, (uint8_t*)buf1, strlen(buf1));
   uint32_t test2 = encoding_crc32(0, (uint8_t*)buf2, strlen(buf2));
   uint32_t test3 = encoding_crc32(0, (uint8_t*)buf3, strlen(buf3));
   ck_assert_uint_eq(0x3cae141a, test1);
   ck_assert_uint_eq(0x9ae0daaf, test2);
   ck_assert_uint_eq(0x414fa339, test3);
}
END_TEST

#define CRC32_BUFFER_SIZE 1048576
#define CRC32_MAX_MB 64

/**
 * Calculate a CRC32 from the first part of the given file.
 * "first part" being the first (CRC32_BUFFER_SIZE * CRC32_MAX_MB)
 * bytes.
 *
 * Returns: the crc32, or 0 if there was an error.
 */
static uint32_t file_crc32(uint32_t crc, const char *path)
{
   unsigned i;
   RFILE *file        = NULL;
   unsigned char *buf = NULL;
   if (!path)
      return 0;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0)))
      return 0;

   if (!(buf = (unsigned char*)malloc(CRC32_BUFFER_SIZE)))
   {
      filestream_close(file);
      return 0;
   }

   for (i = 0; i < CRC32_MAX_MB; i++)
   {
      int64_t nread = filestream_read(file, buf, CRC32_BUFFER_SIZE);
      if (nread < 0)		
      {
         free(buf);
         filestream_close(file);
         return 0;
      }

      crc = encoding_crc32(crc, buf, (size_t)nread);
      if (filestream_eof(file))
         break;
   }
   free(buf);
   filestream_close(file);
   return crc;
}

START_TEST (test_crc32_file)
{
   char tmpfile[512];
   FILE *fd;
   tmpnam(tmpfile);
   fd = fopen(tmpfile, "wb");
   ck_assert(fd != NULL);
   fwrite("12345678", 1, 8, fd);
   fclose(fd);

   ck_assert_uint_eq(file_crc32(0, tmpfile), 0x9ae0daaf);
   /* Error checking */
   ck_assert_uint_eq(file_crc32(0, "/this/path/should/not/exist"), 0);
   ck_assert_uint_eq(file_crc32(0, NULL), 0);
}
END_TEST

Suite *create_suite(void)
{
   Suite *s = suite_create(SUITE_NAME);

   TCase *tc_core = tcase_create("Core");
   tcase_add_test(tc_core, test_md5);
   tcase_add_test(tc_core, test_crc32);
   tcase_add_test(tc_core, test_crc32_file);
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
