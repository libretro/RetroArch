/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_7z_test.c).
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

/* The 7z backend through the archive layer: a solid LZMA archive, a
 * stored one and a BCJ2 one 7-Zip wrote are listed, every member read
 * whole with file_archive_compressed_read() and every member extracted
 * through one transfer, solid members one after another from the
 * folder held, or out of a folder over SEVENZIP_FOLDER_KEEP streamed,
 * each going on from the one before. Built with HAVE_MMAP the archives
 * are mapped, and the members handed out where the folder cache holds
 * them; every member must come out as it went in either way. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>

#include "archive_7z_fixtures.h"
#include "../../formats/r7z/r7z_stored_fixtures.h"
#include "../../formats/r7z/r7z_bcj2_fixtures.h"

#define OUT_PREFIX "archive_7z_test_out_"

static unsigned failures;

struct member
{
   const char *name;
   size_t      size;
   int         kind;
};

static void fail(const char *what, const char *member)
{
   printf("[FAIL] %s: %s\n", what, member);
   failures++;
}

/* r7z_bcj2_test.c's program-like octets */
static void program(uint8_t *d, size_t n)
{
   static const uint8_t push[3] = { 0x55, 0x89, 0xe5 };
   static const uint8_t jz[6]   = { 0x0f, 0x84, 0x10, 0x00, 0x00, 0x00 };
   static const uint8_t ret[4]  = { 0x8b, 0x45, 0x08, 0xc3 };
   uint32_t x = 1, j = 0;
   size_t   at = 0;
   while (at < n)
   {
      uint8_t  ins[6];
      size_t   len, k;
      uint32_t kind;
      j++;
      kind = j % 5;
      if (j % 61 == 0)
      {
         x    = x * 1103515245u + 12345u;
         kind = (x >> 16) % 5;
      }
      if (kind < 2)
      {
         uint32_t tgt = kind == 0 ? 0x1000u * ((j / 5) % 3) : 0x800u;
         uint32_t rel = tgt - (uint32_t)(at + 5);
         ins[0] = kind == 0 ? 0xe8 : 0xe9;
         ins[1] = (uint8_t)rel;
         ins[2] = (uint8_t)(rel >> 8);
         ins[3] = (uint8_t)(rel >> 16);
         ins[4] = (uint8_t)(rel >> 24);
         len    = 5;
      }
      else if (kind == 2)
      {
         memcpy(ins, push, 3);
         len = 3;
      }
      else if (kind == 3)
      {
         memcpy(ins, jz, 6);
         len = 6;
      }
      else
      {
         memcpy(ins, ret, 4);
         len = 4;
      }
      for (k = 0; k < len && at < n; k++)
         d[at++] = ins[k];
   }
}

static uint8_t *expected(const struct member *m)
{
   uint8_t *d = (uint8_t*)malloc(m->size ? m->size : 1);
   size_t   i;
   for (i = 0; i < m->size; i++)
      d[i] = (uint8_t)(m->kind == 0 ? ((i * 13) ^ (i >> 3))
                     : m->kind == 1 ? (i * 7 + (i >> 9)) : (i >> 4));
   if (m->kind == 3)
      program(d, m->size);
   return d;
}

static int extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   char out[256];
   snprintf(out, sizeof(out), OUT_PREFIX "%s", name);
   if (file_archive_perform_mode(out, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      (*(unsigned*)userdata->cb_data)++;
   return 1;
}

static void check(const char *path, const uint8_t *fx, size_t fx_len,
      const struct member *m, unsigned n)
{
   struct archive_extract_userdata userdata;
   file_archive_transfer_t state;
   struct string_list *list;
   unsigned extracted = 0, i;
   bool     returnerr = true;
   FILE    *f = fopen(path, "wb");

   if (!f || fwrite(fx, 1, fx_len, f) != fx_len)
      fail("cannot write", path);
   if (f)
      fclose(f);

   if (!(list = file_archive_get_file_list(path, NULL)) || list->size != n)
      fail("listed", path);
   string_list_free(list);

   for (i = 0; i < n; i++)
   {
      char     member[256];
      void    *buf  = NULL;
      int64_t  len  = 0;
      uint8_t *want = expected(&m[i]);
      snprintf(member, sizeof(member), "%s#%s", path, m[i].name);
      if (     !file_archive_compressed_read(member, &buf, NULL, &len)
            || len != (int64_t)m[i].size || memcmp(buf, want, m[i].size)
            || ((char*)buf)[len] != '\0')
         fail("read whole", member);
      free(buf);
      free(want);
   }

   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   state.type       = ARCHIVE_TRANSFER_INIT;
   userdata.cb_data = &extracted;
   while (file_archive_parse_file_iterate(&state, &returnerr, path,
            NULL, extract_cb, &userdata) == 0) { }
   if (extracted != n)
      fail("not every member extracted", path);
   for (i = 0; i < n; i++)
   {
      char     out[256];
      void    *got = NULL;
      int64_t  got_len = 0;
      uint8_t *want = expected(&m[i]);
      snprintf(out, sizeof(out), OUT_PREFIX "%s", m[i].name);
      if (     !filestream_read_file(out, &got, &got_len)
            || got_len != (int64_t)m[i].size || memcmp(got, want, m[i].size))
         fail("extracted", m[i].name);
      free(got);
      free(want);
      remove(out);
   }
   remove(path);
}

/* @fx with an octet of its packed stream changed: big.bin is not left
 * behind, whether that fails its CRC or its stream */
static void check_refused(const char *path, const uint8_t *fx, size_t fx_len,
      size_t at)
{
   struct archive_extract_userdata userdata;
   file_archive_transfer_t state;
   unsigned extracted = 0;
   bool     returnerr = true;
   uint8_t *bad = (uint8_t*)malloc(fx_len);
   FILE    *f;
   memcpy(bad, fx, fx_len);
   bad[at] ^= 0x20;
   if (!(f = fopen(path, "wb")) || fwrite(bad, 1, fx_len, f) != fx_len)
      fail("cannot write", path);
   if (f)
      fclose(f);
   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   state.type       = ARCHIVE_TRANSFER_INIT;
   userdata.cb_data = &extracted;
   while (file_archive_parse_file_iterate(&state, &returnerr, path,
            NULL, extract_cb, &userdata) == 0) { }
   if (path_is_valid(OUT_PREFIX "big.bin"))
      fail("left behind with its stream changed", path);
   remove(OUT_PREFIX "a.bin");
   remove(OUT_PREFIX "big.bin");
   remove(OUT_PREFIX "c.bin");
   remove(path);
   free(bad);
}

int main(void)
{
   static const struct member solid[3] = {
      { "a.bin", 1500, 0 }, { "b.bin", 900, 1 }, { "c.bin", 20000, 2 } };
   static const struct member stored[2] = {
      { "a.bin", 1500, 0 }, { "b.bin", 900, 1 } };
   static const struct member bcj2[1] = { { "prog.bin", 6000, 3 } };
   /* big.bin, between the others in its folder, is streamed out where
    * the folder is LZMA's or LZMA2's */
   static const struct member big[3] = {
      { "a.bin", 1500, 0 }, { "big.bin", 700000, 2 }, { "c.bin", 900, 1 } };

   check("archive_7z_test_solid.7z", fx_solid, sizeof(fx_solid), solid, 3);
   check("archive_7z_test_stored.7z", fx_stored, sizeof(fx_stored), stored, 2);
   check("archive_7z_test_bcj2.7z", fx_bcj2_copy, sizeof(fx_bcj2_copy), bcj2, 1);
   check("archive_7z_test_lzma2.7z", fx_big_lzma2, sizeof(fx_big_lzma2), big, 3);
   check("archive_7z_test_lzma.7z", fx_big_lzma, sizeof(fx_big_lzma), big, 3);
   check("archive_7z_test_deflate.7z", fx_big_deflate, sizeof(fx_big_deflate),
         big, 3);
   /* three quarters into the packed stream, which runs from 32 to the
    * header the start header points at: big.bin's share of it */
   check_refused("archive_7z_test_bad.7z", fx_big_lzma2, sizeof(fx_big_lzma2),
         32 + (fx_big_lzma2[12] | (fx_big_lzma2[13] << 8)) * 3 / 4);
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("archive_7z_test: every member read and extracted\n");
   return 0;
}
