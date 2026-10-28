/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_bz2_test.c).
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

/* bzip2 in the archive layer: zip members stored with method 12, and
 * .bz2 files, which stand for the file named as they are without the
 * extension. Each is listed, read whole with
 * file_archive_compressed_read() and extracted through a transfer, and
 * must come out as it went in, zip members through an entry source as
 * well; a zip member whose recorded CRC is wrong is refused, and a "#"
 * after ".bz2" names a member. A .bz2 is kept from its listing when it
 * is small and decoded again when it is not, through a ring where it is
 * not mapped, and a changed one leaves nothing behind. The streams are
 * bz_write.h's, as the rbzip2 sample makes them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>

#include "../../encodings/rbzip2/bz_write.h"

#define ZIP_PATH  "archive_bz2_test.zip"
#define BZ2_PATH  "archive_bz2_test.bin.bz2"
#define OUT_PATH  "archive_bz2_test.out"
#define N_MEMBERS 3

static unsigned failures;

static void fail(const char *what, const char *detail)
{
   printf("[FAIL] %s: %s\n", what, detail);
   failures++;
}

static void put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
   put16(p, v & 0xffff);
   put16(p + 2, v >> 16);
}

static void fill(uint8_t *d, size_t n, unsigned k)
{
   uint32_t x = 7 + k;
   size_t   i;
   for (i = 0; i < n; i++)
   {
      x    = x * 1103515245u + 12345u;
      d[i] = (uint8_t)(k == 1 ? (x >> 16) : 'a' + (x >> 16) % (3 + k) + (i >> 10) % 3);
   }
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

/* the member's octets, as read back every way there is */
static void check_member(const char *what, const char *archive,
      const char *name, const uint8_t *want, size_t n, uint32_t crc)
{
   char     path[256];
   void    *buf = NULL;
   int64_t  len = 0;

   snprintf(path, sizeof(path), "%s#%s", archive, name);
   if (     !file_archive_compressed_read(path, &buf, NULL, &len)
         || len != (int64_t)n || memcmp(buf, want, n))
      fail(what, "read whole");
   free(buf);
   if (file_archive_get_file_crc32(path) != crc)
      fail(what, "listed CRC");
}

static int extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   char out[64];
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

static void check_extracted(const char *what, const char *name,
      const uint8_t *want, size_t n)
{
   char     out[64];
   void    *got = NULL;
   int64_t  got_len = 0;
   if (name)
      snprintf(out, sizeof(out), "%s.%s", OUT_PATH, name);
   else
      snprintf(out, sizeof(out), "%s", OUT_PATH);
   if (     !filestream_read_file(out, &got, &got_len)
         || got_len != (int64_t)n || memcmp(got, want, n))
      fail(what, "extracted");
   free(got);
   remove(out);
}

static void test_zip(bool wrong_crc)
{
   static const size_t sizes[N_MEMBERS] = { 70000, 333, 260000 };
   uint8_t *data[N_MEMBERS], *packed[N_MEMBERS], *zip;
   size_t   plen[N_MEMBERS], at = 0, cd, cap = 64;
   uint32_t crc[N_MEMBERS], off[N_MEMBERS];
   char     name[16];
   unsigned i;

   for (i = 0; i < N_MEMBERS; i++)
   {
      size_t c = sizes[i] + sizes[i] / 2 + 4096;
      data[i]   = (uint8_t*)malloc(sizes[i]);
      packed[i] = (uint8_t*)malloc(c);
      fill(data[i], sizes[i], i);
      crc[i]  = encoding_crc32(0, data[i], sizes[i]) ^ (wrong_crc ? 1u : 0u);
      plen[i] = bz_write(packed[i], c, data[i], sizes[i], 1 + i * 4, i);
      cap    += 30 + 46 + 2 * 16 + plen[i];
   }
   zip = (uint8_t*)malloc(cap + 22);
   for (i = 0; i < N_MEMBERS; i++)
   {
      sprintf(name, "m%u.bin", i);
      off[i] = (uint32_t)at;
      memset(zip + at, 0, 30);
      put32(zip + at, 0x04034b50u);
      put16(zip + at + 4, 46);
      put16(zip + at + 8, 12);
      put32(zip + at + 14, crc[i]);
      put32(zip + at + 18, (uint32_t)plen[i]);
      put32(zip + at + 22, (uint32_t)sizes[i]);
      put16(zip + at + 26, (unsigned)strlen(name));
      memcpy(zip + at + 30, name, strlen(name));
      at += 30 + strlen(name);
      memcpy(zip + at, packed[i], plen[i]);
      at += plen[i];
   }
   cd = at;
   for (i = 0; i < N_MEMBERS; i++)
   {
      sprintf(name, "m%u.bin", i);
      memset(zip + at, 0, 46);
      put32(zip + at, 0x02014b50u);
      put16(zip + at + 4, 46);
      put16(zip + at + 6, 46);
      put16(zip + at + 10, 12);
      put32(zip + at + 16, crc[i]);
      put32(zip + at + 20, (uint32_t)plen[i]);
      put32(zip + at + 24, (uint32_t)sizes[i]);
      put16(zip + at + 28, (unsigned)strlen(name));
      put32(zip + at + 42, off[i]);
      memcpy(zip + at + 46, name, strlen(name));
      at += 46 + strlen(name);
   }
   memset(zip + at, 0, 22);
   put32(zip + at, 0x06054b50u);
   put16(zip + at + 8, N_MEMBERS);
   put16(zip + at + 10, N_MEMBERS);
   put32(zip + at + 12, (uint32_t)(at - cd));
   put32(zip + at + 16, (uint32_t)cd);
   at += 22;
   if (!write_file(ZIP_PATH, zip, at))
      fail("zip", "cannot write");

   if (wrong_crc)
   {
      void   *buf = NULL;
      int64_t len = 0;
      if (extract_all(ZIP_PATH) != 0)
         fail("zip, CRCs wrong", "extracted");
      if (file_archive_compressed_read(ZIP_PATH "#m1.bin", &buf, NULL, &len))
         fail("zip, CRCs wrong", "read whole");
      free(buf);
      {
         int64_t  usize = 0;
         uint8_t *got   = (uint8_t*)malloc(sizes[0]);
         file_archive_entry_source_t *src =
               file_archive_entry_source_open(ZIP_PATH "#m0.bin", &usize);
         if (!src || file_archive_entry_source_read(src, got, 4096) >= 0)
            fail("zip, CRCs wrong", "read through an entry source");
         if (src)
            file_archive_entry_source_close(src);
         free(got);
      }
      for (i = 0; i < N_MEMBERS; i++)
      {
         sprintf(name, "m%u.bin", i);
         {
            char out[64];
            snprintf(out, sizeof(out), "%s.%s", OUT_PATH, name);
            remove(out);
         }
      }
   }
   else
   {
      if (extract_all(ZIP_PATH) != N_MEMBERS)
         fail("zip", "not every member extracted");
      for (i = 0; i < N_MEMBERS; i++)
      {
         char     member[64];
         int64_t  usize = 0, done = 0, got_n;
         uint8_t *got = (uint8_t*)malloc(sizes[i] + 1);
         file_archive_entry_source_t *src;
         sprintf(name, "m%u.bin", i);
         check_extracted("zip", name, data[i], sizes[i]);
         check_member("zip", ZIP_PATH, name, data[i], sizes[i], crc[i]);
         /* through an entry source, as content loads it */
         snprintf(member, sizeof(member), "%s#%s", ZIP_PATH, name);
         if (     !(src = file_archive_entry_source_open(member, &usize))
               || usize != (int64_t)sizes[i])
            fail("zip", "no entry source");
         else
         {
            while ((got_n = file_archive_entry_source_read(src, got + done,
                        4096)) > 0)
               done += got_n;
            if (done != (int64_t)sizes[i] || memcmp(got, data[i], sizes[i]))
               fail("zip", "differs through an entry source");
         }
         if (src)
            file_archive_entry_source_close(src);
         free(got);
      }
   }
   remove(ZIP_PATH);
   for (i = 0; i < N_MEMBERS; i++)
   {
      free(data[i]);
      free(packed[i]);
   }
   free(zip);
}

/* A .bz2 of @n octets: listed, read whole, read to a file and
 * extracted, kept from its listing under ARCHIVE_BZ2_KEEP and decoded
 * again past it; with a byte of it changed, nothing comes out */
static void test_bz2(const char *what, size_t n, unsigned level)
{
   size_t   c    = n + n / 2 + 4096, plen;
   uint8_t *data = (uint8_t*)malloc(n);
   uint8_t *bz   = (uint8_t*)malloc(c);
   struct string_list *list;
   void    *buf = NULL;
   int64_t  len = 0;

   fill(data, n, 2);
   plen = bz_write(bz, c, data, n, level, 9);
   if (!write_file(BZ2_PATH, bz, plen))
      fail(what, "cannot write");
   if (     !(list = file_archive_get_file_list(BZ2_PATH, NULL))
         || list->size != 1
         || strcmp(list->elems[0].data, "archive_bz2_test.bin"))
      fail(what, "listed as its name without the extension");
   string_list_free(list);
   check_member(what, BZ2_PATH, "archive_bz2_test.bin", data, n,
         encoding_crc32(0, data, n));
   if (extract_all(BZ2_PATH) != 1)
      fail(what, "not extracted");
   check_extracted(what, "archive_bz2_test.bin", data, n);
   if (     file_archive_compressed_read(BZ2_PATH "#archive_bz2_test.bin",
               NULL, OUT_PATH, &len) <= 0
         || len != (int64_t)n)
      fail(what, "read to a file");
   check_extracted(what, NULL, data, n);
   if (file_archive_compressed_read(BZ2_PATH "#other.bin", &buf, NULL, &len))
      fail(what, "a member it does not have read");
   free(buf);
   buf = NULL;

   /* a byte of the last block's changed */
   bz[plen - 20] ^= 0x10;
   if (!write_file(BZ2_PATH, bz, plen))
      fail(what, "cannot write");
   if (file_archive_compressed_read(BZ2_PATH "#archive_bz2_test.bin",
            &buf, NULL, &len))
      fail(what, "a changed stream read whole");
   free(buf);
   if (file_archive_compressed_read(BZ2_PATH "#archive_bz2_test.bin",
            NULL, OUT_PATH, &len) || path_is_valid(OUT_PATH))
      fail(what, "a changed stream read to a file");
   remove(OUT_PATH);
   extract_all(BZ2_PATH);
   if (path_is_valid(OUT_PATH ".archive_bz2_test.bin"))
      fail(what, "a changed stream extracted");
   remove(OUT_PATH ".archive_bz2_test.bin");

   remove(BZ2_PATH);
   free(data);
   free(bz);
}

int main(void)
{
   const char *p = "roms/set.BZ2#game.bin";
   if (path_get_archive_delim(p) != p + 12)
      fail("path_get_archive_delim", "\".bz2#\" not a member");
   if (!path_is_compressed_file("roms/set.bz2"))
      fail("path_is_compressed_file", "set.bz2 is not an archive");
   if (!path_is_compressed_file("roms/SET.Bz2"))
      fail("path_is_compressed_file", "SET.Bz2 is not an archive");
   if (path_is_compressed_file("roms/set.bz"))
      fail("path_is_compressed_file", "set.bz is an archive");
   if (!path_is_compressed_file("roms/set.7Z"))
      fail("path_is_compressed_file", "set.7Z is not an archive");
   test_zip(false);
   test_zip(true);
   /* kept from its listing, and too big to be */
   test_bz2(".bz2", 60000, 2);
   test_bz2(".bz2 past what is kept, through the ring", 1000000, 1);
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("archive_bz2_test: zip members and .bz2 files read and extracted\n");
   return 0;
}
