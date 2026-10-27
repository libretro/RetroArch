/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_zip_methods_test.c).
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

/* Zip members stored with Deflate64 (method 9) and LZMA (method 14),
 * in archives 7-Zip wrote. Each member is listed, read whole, extracted
 * through a transfer and read through an entry source, and must come
 * out as it went in; a member whose recorded CRC is wrong is refused,
 * and so is an LZMA member whose header does not hold 5 octets of
 * properties. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>

#include "archive_zip_methods_fixtures.h"

#define ZIP_PATH "archive_zip_methods_test.zip"
#define OUT_PATH "archive_zip_methods_test.out"

#define FAR_LEN  66300
#define NEAR_LEN 6000

static unsigned failures;
static uint8_t  far_data[FAR_LEN];
static uint8_t  near_data[NEAR_LEN];

static void fail(const char *what, const char *detail)
{
   printf("[FAIL] %s: %s\n", what, detail);
   failures++;
}

static void gen(uint8_t *d, size_t n, uint32_t x, unsigned alpha)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      x    = x * 1103515245u + 12345u;
      d[i] = (uint8_t)('a' + (x >> 16) % alpha);
   }
}

/* far.bin: a block of 33000, again, then 300 more; near.bin: 2000
 * three times */
static void make_contents(void)
{
   unsigned i;
   gen(far_data, 33000, 1, 2);
   memcpy(far_data + 33000, far_data, 33000);
   gen(far_data + 66000, 300, 2, 2);
   gen(near_data, 2000, 3, 7);
   for (i = 1; i < 3; i++)
      memcpy(near_data + i * 2000, near_data, 2000);
}

static bool write_file(const char *path, const uint8_t *d, size_t n)
{
   FILE *f = fopen(path, "wb");
   bool  ok;
   if (!f)
      return false;
   ok = fwrite(d, 1, n, f) == n;
   fclose(f);
   return ok;
}

static uint32_t get32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   char out[96];
   snprintf(out, sizeof(out), "%s.%s", OUT_PATH, path_basename(name));
   if (file_archive_perform_mode(out, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      (*(unsigned*)userdata->cb_data)++;
   return 1;
}

static unsigned extract_all(const char *archive)
{
   struct archive_extract_userdata userdata;
   file_archive_transfer_t state;
   unsigned extracted = 0;
   bool     returnerr = true;
   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   state.type       = ARCHIVE_TRANSFER_INIT;
   userdata.cb_data = &extracted;
   while (file_archive_parse_file_iterate(&state, &returnerr, archive,
            NULL, extract_cb, &userdata) == 0) { }
   return extracted;
}

/* the member's octets, read every way there is */
static void check_member(const char *what, const char *name,
      const uint8_t *want, size_t n)
{
   char     path[128], out[96];
   void    *buf = NULL;
   int64_t  len = 0, usize = 0, done = 0, got_n;
   uint8_t *got = (uint8_t*)malloc(n + 1);
   file_archive_entry_source_t *src;

   snprintf(path, sizeof(path), "%s#%s", ZIP_PATH, name);
   if (     !file_archive_compressed_read(path, &buf, NULL, &len)
         || len != (int64_t)n || memcmp(buf, want, n))
      fail(what, "read whole");
   free(buf);
   if (file_archive_get_file_crc32(path) != encoding_crc32(0, want, n))
      fail(what, "listed CRC");

   snprintf(out, sizeof(out), "%s.%s", OUT_PATH, name);
   buf = NULL;
   if (     !filestream_read_file(out, &buf, &len)
         || len != (int64_t)n || memcmp(buf, want, n))
      fail(what, "extracted");
   free(buf);
   remove(out);

   if (     !(src = file_archive_entry_source_open(path, &usize))
         || usize != (int64_t)n)
      fail(what, "no entry source");
   else
   {
      while ((got_n = file_archive_entry_source_read(src, got + done,
                  4096)) > 0)
         done += got_n;
      if (done != (int64_t)n || memcmp(got, want, n))
         fail(what, "differs through an entry source");
   }
   if (src)
      file_archive_entry_source_close(src);
   free(got);
}

/* none of far.bin comes out, every way it is asked for */
static void check_refused(const char *what)
{
   void    *buf = NULL;
   int64_t  len = 0, usize = 0;
   uint8_t *got = (uint8_t*)malloc(FAR_LEN);
   char     out[96];
   file_archive_entry_source_t *src;

   extract_all(ZIP_PATH);
   snprintf(out, sizeof(out), "%s.far.bin", OUT_PATH);
   if (path_is_valid(out))
      fail(what, "extracted");
   remove(out);
   snprintf(out, sizeof(out), "%s.near.bin", OUT_PATH);
   remove(out);
   if (file_archive_compressed_read(ZIP_PATH "#far.bin", &buf, NULL, &len))
      fail(what, "read whole");
   free(buf);
   src = file_archive_entry_source_open(ZIP_PATH "#far.bin", &usize);
   if (src && file_archive_entry_source_read(src, got, FAR_LEN) >= 0)
      fail(what, "read through an entry source");
   if (src)
      file_archive_entry_source_close(src);
   free(got);
}

static void test_archive(const char *what, const uint8_t *fx, size_t len,
      bool lzma)
{
   uint8_t *zip = (uint8_t*)malloc(len);
   const uint8_t *eocd = fx + len - 22;
   const uint8_t *cd   = fx + get32(eocd + 16);
   uint32_t local;
   char     detail[64];
   struct string_list *list;

   /* far.bin is the first member, in the directory and in the file */
   local = get32(cd + 42);

   if (!write_file(ZIP_PATH, fx, len))
      fail(what, "cannot write");
   if (     !(list = file_archive_get_file_list(ZIP_PATH, NULL))
         || list->size != 2)
      fail(what, "listed");
   string_list_free(list);
   if (extract_all(ZIP_PATH) != 2)
      fail(what, "not every member extracted");
   check_member(what, "far.bin", far_data, FAR_LEN);
   check_member(what, "near.bin", near_data, NEAR_LEN);

   /* far.bin's CRC, wrong where both headers record it */
   memcpy(zip, fx, len);
   zip[cd + 16 - fx]  ^= 1;
   zip[local + 14]    ^= 1;
   snprintf(detail, sizeof(detail), "%s, CRC wrong", what);
   if (!write_file(ZIP_PATH, zip, len))
      fail(detail, "cannot write");
   check_refused(detail);

   if (lzma)
   {
      /* the size of the properties, in the LZMA header */
      const uint8_t *lh = fx + local;
      size_t data = local + 30 + (lh[26] | (lh[27] << 8))
                  + (lh[28] | (lh[29] << 8));
      memcpy(zip, fx, len);
      zip[data + 2] = 4;
      snprintf(detail, sizeof(detail), "%s, 4 octets of properties", what);
      if (!write_file(ZIP_PATH, zip, len))
         fail(detail, "cannot write");
      check_refused(detail);
   }

   remove(ZIP_PATH);
   free(zip);
}

int main(void)
{
   make_contents();
   test_archive("Deflate64", fx_deflate64, sizeof(fx_deflate64), false);
#ifdef HAVE_7ZIP
   test_archive("LZMA", fx_lzma, sizeof(fx_lzma), true);
#endif
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("archive_zip_methods_test: Deflate64 and LZMA members read and extracted\n");
   return 0;
}
