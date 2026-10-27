/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_zst_test.c).
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

/* A .zst archive read whole through file_archive_compressed_read() and
 * extracted through a transfer, from a plain file and through a VFS
 * that lends the whole file in place: the member comes out exact every
 * way, and where the VFS lends the frame it is decoded there rather
 * than read. A frame with a window smaller than itself is extracted a
 * block at a time; with its stated size or its checksum wrong it is
 * neither read nor extracted. The member's CRC, which a frame does not
 * hold, is had by decoding it. */

/* the lending VFS installs the implementation's own functions as a
 * frontend's */
#define VFS_FRONTEND

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <file/archive_file.h>
#include <encodings/crc32.h>
#include <encodings/rzstd.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#include "archive_zst_fixtures.h"

#define ZST_PATH  "archive_zst_test.bin.zst"
#define OUT_PATH  "archive_zst_test.out"
#define DATA_LEN  (3 * 1024 * 1024 + 17)
#define WIN_PATH  "archive_zst_win.bin.zst"
#define WIN_LEN   655300

static unsigned       failures;
static uint8_t       *data;
static uint8_t       *frame;
static size_t         frame_len;
static uint64_t       read_octets;

static void fail(const char *what, const char *detail)
{
   printf("[FAIL] %s: %s\n", what, detail);
   failures++;
}

static const uint8_t *lend_mapped_ptr(void *hfile, int64_t *len)
{
   (void)hfile;
   *len = (int64_t)frame_len;
   return frame;
}

static int64_t count_read(struct retro_vfs_file_handle *h, void *s,
      uint64_t len)
{
   int64_t r = retro_vfs_file_read_impl(h, s, len);
   if (r > 0)
      read_octets += (uint64_t)r;
   return r;
}

static void install_vfs(bool lend)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;

   memset(&iface, 0, sizeof(iface));
   iface.get_path = retro_vfs_file_get_path_impl;
   iface.open     = retro_vfs_file_open_impl;
   iface.close    = retro_vfs_file_close_impl;
   iface.size     = retro_vfs_file_size_impl;
   iface.tell     = retro_vfs_file_tell_impl;
   iface.seek     = retro_vfs_file_seek_impl;
   iface.read     = count_read;
   iface.write    = retro_vfs_file_write_impl;
   iface.flush    = retro_vfs_file_flush_impl;
   iface.remove   = retro_vfs_file_remove_impl;
   iface.rename   = retro_vfs_file_rename_impl;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &iface;
   filestream_vfs_init(&info);
   filestream_set_mapped_ptr_cb(lend ? lend_mapped_ptr : NULL);
   read_octets = 0;
}

static int extract_cb(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint64_t csize, uint64_t size,
      uint32_t checksum, struct archive_extract_userdata *userdata)
{
   if (file_archive_perform_mode(OUT_PATH, valid_exts, cdata, cmode,
            csize, size, checksum, userdata))
      (*(unsigned*)userdata->cb_data)++;
   return 1;
}

static void check_read(const char *what, bool lend)
{
   void    *buf = NULL;
   int64_t  len = 0;
   char     detail[96];

   install_vfs(lend);
   if (     !file_archive_compressed_read(ZST_PATH "#archive_zst_test.bin",
               &buf, NULL, &len)
         || len != DATA_LEN || memcmp(buf, data, DATA_LEN))
      fail(what, "member differs");
   else if (lend && read_octets >= frame_len)
   {
      snprintf(detail, sizeof(detail), "%llu octets read, not lent",
            (unsigned long long)read_octets);
      fail(what, detail);
   }
   free(buf);
}

static void check_extract(const char *what, bool lend)
{
   struct archive_extract_userdata userdata;
   file_archive_transfer_t state;
   unsigned extracted = 0;
   bool     returnerr = true;
   void    *got       = NULL;
   int64_t  got_len   = 0;
   uint64_t archive_read;
   char     detail[96];

   install_vfs(lend);
   memset(&state, 0, sizeof(state));
   memset(&userdata, 0, sizeof(userdata));
   state.type       = ARCHIVE_TRANSFER_INIT;
   userdata.cb_data = &extracted;
   while (file_archive_parse_file_iterate(&state, &returnerr, ZST_PATH,
            NULL, extract_cb, &userdata) == 0) { }
   archive_read = read_octets;

   if (extracted != 1)
      fail(what, "not extracted");
   else if (!filestream_read_file(OUT_PATH, &got, &got_len)
         || got_len != DATA_LEN || memcmp(got, data, DATA_LEN))
      fail(what, "member differs");
   else if (lend && archive_read >= frame_len)
   {
      snprintf(detail, sizeof(detail), "%llu octets read, not lent",
            (unsigned long long)archive_read);
      fail(what, detail);
   }
   free(got);
   remove(OUT_PATH);
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

/* fx_win_zst extracted and read, and refused with its stated size one
 * out or its checksum wrong */
static void check_window(void)
{
   uint8_t *want = (uint8_t*)malloc(WIN_LEN);
   uint8_t  fx[sizeof(fx_win_zst)];
   unsigned i, k;

   if (!want)
   {
      fail("window", "no memory");
      return;
   }
   gen(want, 131000, 7, 2);
   for (i = 1; i < 5; i++)
      memcpy(want + i * 131000, want, 131000);
   gen(want + 655000, 300, 9, 26);

   install_vfs(false);
   for (k = 0; k < 3; k++)
   {
      struct archive_extract_userdata userdata;
      file_archive_transfer_t state;
      unsigned extracted = 0;
      bool     returnerr = true;
      void    *got       = NULL;
      int64_t  got_len   = 0;
      FILE    *f;
      const char *what = k == 2 ? "window, checksum wrong"
         : k ? "window, size wrong" : "window";

      memcpy(fx, fx_win_zst, sizeof(fx));
      /* the low octet of the content size, after the magic, the frame
       * header descriptor and the window descriptor */
      if (k == 1)
         fx[6] ^= 1;
      /* the frame's last octet: its checksum's highest */
      if (k == 2)
         fx[sizeof(fx) - 1] ^= 1;
      if (     !(f = fopen(WIN_PATH, "wb"))
            || fwrite(fx, 1, sizeof(fx), f) != sizeof(fx))
      {
         fail(what, "cannot write");
         if (f)
            fclose(f);
         continue;
      }
      fclose(f);
      memset(&state, 0, sizeof(state));
      memset(&userdata, 0, sizeof(userdata));
      state.type       = ARCHIVE_TRANSFER_INIT;
      userdata.cb_data = &extracted;
      while (file_archive_parse_file_iterate(&state, &returnerr, WIN_PATH,
               NULL, extract_cb, &userdata) == 0) { }
      if (k)
      {
         if (extracted || filestream_read_file(OUT_PATH, &got, &got_len))
            fail(what, "extracted");
         free(got);
         got = NULL;
         if (file_archive_compressed_read(WIN_PATH "#archive_zst_win.bin",
                  &got, NULL, &got_len))
            fail(what, "read");
      }
      else if (extracted != 1)
         fail(what, "not extracted");
      else if (!filestream_read_file(OUT_PATH, &got, &got_len)
            || got_len != WIN_LEN || memcmp(got, want, WIN_LEN))
         fail(what, "member differs");
      free(got);
      remove(OUT_PATH);
   }
   remove(WIN_PATH);
   free(want);
}

int main(void)
{
   size_t   cap;
   uint32_t x = 1;
   size_t   i;
   FILE    *f;

   if (!(data = (uint8_t*)malloc(DATA_LEN)))
      return 2;
   for (i = 0; i < DATA_LEN; i++)
   {
      x       = x * 1103515245u + 12345u;
      data[i] = (uint8_t)('a' + ((x >> 16) % 11) + ((i >> 13) & 7));
   }
   cap = rzstd_compress_bound(DATA_LEN);
   if (     !(frame = (uint8_t*)malloc(cap))
         || rzstd_encode(frame, cap, data, DATA_LEN, 3, &frame_len)
            != RZSTD_PROCESS_END
         || !(f = fopen(ZST_PATH, "wb")))
      return 2;
   if (fwrite(frame, 1, frame_len, f) != frame_len)
      return 2;
   fclose(f);

   check_read("read, plain file", false);
   check_read("read, lent", true);
   check_extract("extract, plain file", false);
   check_extract("extract, lent", true);
   check_window();
   install_vfs(false);
   if (     file_archive_get_file_crc32(ZST_PATH "#archive_zst_test.bin")
         != encoding_crc32(0, data, DATA_LEN))
      fail("file_archive_get_file_crc32", "not the member's");

   remove(ZST_PATH);
   free(frame);
   free(data);
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("archive_zst_test: all checks passed\n");
   return 0;
}
