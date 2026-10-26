/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_view_test.c).
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

/* filestream_view(), a read that lends instead of copying: what it
 * lends is the file's, at the position, and the position moves past
 * it exactly as a read of as many octets would; -1 lends nothing and
 * moves nothing. Lent from the lookahead gets/getc fill, from a
 * mapping, and from a frontend VFS's mapping. sha1_calculate(), which
 * hashes through it, gives the file's digest either way.
 *
 * Built with MMAP=0 the mapped cases check the fallback instead: no
 * mapping, so nothing lent, and the reads still right. */

/* the shim installs the implementation's own functions as a frontend's */
#define VFS_FRONTEND

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <lrc_hash.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#define FILE_LEN   300000
#define FIXTURE    "vfs_view_fixture.bin"
#define FIXTURE_SHA1 "93AB28A4C60467112AFAD1AB7F7E3FD587812B83"

static int fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         fails++; \
      } \
   } while (0)

static uint8_t body[FILE_LEN];

static void make_body(void)
{
   uint32_t i;
   for (i = 0; i < FILE_LEN; i++)
      body[i] = (uint8_t)((i * 2654435761u) >> 11);
}

static bool lent_is(const uint8_t *ptr, int64_t n, size_t at)
{
   return n >= 0 && at + (size_t)n <= FILE_LEN
      && (!n || !memcmp(ptr, body + at, (size_t)n));
}

/* Lends from @f until the end, checking each run against the body
 * from @at, and that the position follows. */
static bool view_to_end(RFILE *f, size_t at, int64_t step)
{
   for (;;)
   {
      const uint8_t *ptr = NULL;
      int64_t        n   = filestream_view(f, step, &ptr);
      if (n < 0 || !lent_is(ptr, n, at) || n > step)
         return false;
      if (!n)
         return at == FILE_LEN;
      at += (size_t)n;
      if (filestream_tell(f) != (int64_t)at)
         return false;
   }
}

static void test_mapped(void)
{
   RFILE         *f = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   const uint8_t *ptr = NULL;
   uint8_t        got[16];
   int64_t        n;

   CHECK(f != NULL, "fixture opens with the mapping hint");
   if (!f)
      return;
   if (!filestream_get_mapped_ptr(f, NULL))
   {
      printf("no mapping on this build: checking the fallback\n");
      CHECK(filestream_view(f, 100, &ptr) == -1,
            "nothing mapped, nothing lent");
      CHECK(filestream_tell(f) == 0, "and the position stays put");
      CHECK(filestream_read(f, got, 16) == 16 && !memcmp(got, body, 16),
            "a read after a refused lend reads from the start");
      filestream_close(f);
      return;
   }

   n = filestream_view(f, 100, &ptr);
   CHECK(n == 100 && lent_is(ptr, n, 0), "a lend runs from the start");
   CHECK(filestream_tell(f) == 100, "and moves the position past it");
   CHECK(view_to_end(f, 100, 1 << 20), "lends in place to the end");
   CHECK(filestream_view(f, 10, &ptr) == 0, "the end lends 0");

   CHECK(filestream_seek(f, 5000, RETRO_VFS_SEEK_POSITION_START) == 0,
         "seeks after lending");
   n = filestream_view(f, 7, &ptr);
   CHECK(n == 7 && lent_is(ptr, n, 5000), "a lend after a seek is there");
   CHECK(filestream_read(f, got, 16) == 16 && !memcmp(got, body + 5007, 16),
         "a read after a lend picks up after it");
   n = filestream_view(f, 3, &ptr);
   CHECK(n == 3 && lent_is(ptr, n, 5023), "a lend after a read too");
   filestream_close(f);
}

static void test_unmapped(void)
{
   RFILE         *f = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   const uint8_t *ptr = NULL;
   uint8_t        got[64];

   CHECK(f != NULL, "fixture opens without the hint");
   if (!f)
      return;
   CHECK(filestream_seek(f, 1234, RETRO_VFS_SEEK_POSITION_START) == 0,
         "seeks");
   CHECK(filestream_view(f, 64, &ptr) == -1, "an unmapped file lends nothing");
   CHECK(filestream_tell(f) == 1234, "and the position stays put");
   CHECK(filestream_read(f, got, 64) == 64 && !memcmp(got, body + 1234, 64),
         "the read after it reads there");
   CHECK(filestream_view(NULL, 1, &ptr) == -1, "a NULL stream lends nothing");
   CHECK(filestream_view(f, 1, NULL) == -1, "nor without a pointer to fill");
   CHECK(filestream_view(f, -1, &ptr) == -1, "nor a negative length");
   CHECK(filestream_view(f, 0, &ptr) == 0, "a zero length lends 0");
   CHECK(filestream_tell(f) == 1298, "and none of them move the position");
   filestream_close(f);
}

/* gets/getc fill the lookahead; a lend drains it before anything else */
static void test_lookahead(void)
{
   RFILE         *f = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   const uint8_t *ptr  = NULL;
   size_t         at   = 1;
   bool           ok   = true;
   uint8_t        got[64];

   CHECK(f != NULL, "fixture opens for the lookahead");
   if (!f)
      return;
   CHECK(filestream_getc(f) == body[0], "getc reads the first octet");
   for (;;)
   {
      int64_t n = filestream_view(f, 4096, &ptr);
      if (n < 0)
         break;
      if (!n || !lent_is(ptr, n, at))
      {
         ok = false;
         break;
      }
      at += (size_t)n;
   }
   CHECK(ok && at > 1, "the lookahead is lent, in order");
   CHECK(filestream_tell(f) == (int64_t)at, "and the position follows it");
   CHECK(filestream_read(f, got, 64) == 64 && !memcmp(got, body + at, 64),
         "a read after the lookahead is drained reads on from there");
   filestream_close(f);
}

/* Output then input needs a positioning call between them; a lend is
 * input. */
static void test_update_stream(void)
{
   static const char *path = "vfs_view_update.bin";
   RFILE         *f;
   const uint8_t *ptr = NULL;
   uint8_t        got[8];

   filestream_write_file(path, body, 4096);
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL, "an update stream opens");
   if (!f)
      return;
   CHECK(filestream_write(f, "abcd", 4) == 4, "writes");
   CHECK(filestream_view(f, 8, &ptr) == -1, "an update stream lends nothing");
   CHECK(filestream_read(f, got, 8) == 8 && !memcmp(got, body + 4, 8),
         "the read after write and lend reads after the write");
   filestream_close(f);
   filestream_delete(path);
}

/* A frontend VFS that hands out its mapping through the callback */
static struct retro_vfs_interface shim_iface;
static const uint8_t *shim_map;
static int64_t        shim_map_len;

static const uint8_t *shim_mapped_ptr(void *hfile, int64_t *len)
{
   (void)hfile;
   *len = shim_map_len;
   return shim_map;
}

static void install_shim_vfs(bool on)
{
   struct retro_vfs_interface_info info;

   memset(&shim_iface, 0, sizeof(shim_iface));
   shim_iface.get_path = retro_vfs_file_get_path_impl;
   shim_iface.open     = retro_vfs_file_open_impl;
   shim_iface.close    = retro_vfs_file_close_impl;
   shim_iface.size     = retro_vfs_file_size_impl;
   shim_iface.tell     = retro_vfs_file_tell_impl;
   shim_iface.seek     = retro_vfs_file_seek_impl;
   shim_iface.read     = retro_vfs_file_read_impl;
   shim_iface.write    = retro_vfs_file_write_impl;
   shim_iface.flush    = retro_vfs_file_flush_impl;
   shim_iface.remove   = retro_vfs_file_remove_impl;
   shim_iface.rename   = retro_vfs_file_rename_impl;
   shim_iface.truncate = retro_vfs_file_truncate_impl;

   info.required_interface_version = on ? FILESTREAM_REQUIRED_VFS_VERSION : 0;
   info.iface                      = on ? &shim_iface : NULL;
   filestream_vfs_init(&info);
   filestream_set_mapped_ptr_cb(on ? shim_mapped_ptr : NULL);
}

static void test_frontend_mapping(void)
{
   RFILE         *f;
   const uint8_t *ptr = NULL;
   uint8_t        got[32];
   int64_t        n;

   /* the frontend maps only the first part: lent there, read after */
   shim_map     = body;
   shim_map_len = 70000;
   install_shim_vfs(true);
   f = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL, "fixture opens through the frontend VFS");
   if (f)
   {
      filestream_seek(f, 69000, RETRO_VFS_SEEK_POSITION_START);
      n = filestream_view(f, 1 << 20, &ptr);
      CHECK(n == 1000 && ptr == body + 69000,
            "a frontend's mapping is lent, to its end");
      CHECK(filestream_tell(f) == 70000, "and the position follows");
      CHECK(filestream_view(f, 16, &ptr) == -1, "past it nothing is lent");
      CHECK(filestream_read(f, got, 32) == 32
            && !memcmp(got, body + 70000, 32),
            "and the read reads on from there");
      filestream_close(f);
   }

   shim_map = NULL;
   if ((f = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      CHECK(filestream_view(f, 16, &ptr) == -1,
            "a frontend VFS without a mapping lends nothing");
      filestream_close(f);
   }
   install_shim_vfs(false);
}

static void test_sha1(void)
{
   char digest[41];

   CHECK(sha1_calculate(FIXTURE, digest) == 0
         && !strcmp(digest, FIXTURE_SHA1),
         "sha1_calculate gives the file's digest");
   filestream_write_file("vfs_view_empty.bin", body, 0);
   CHECK(sha1_calculate("vfs_view_empty.bin", digest) == 0
         && !strcmp(digest, "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"),
         "and an empty file's");
   filestream_delete("vfs_view_empty.bin");
   CHECK(sha1_calculate("vfs_view_missing.bin", digest) == -1,
         "and fails on a missing one");
}

int main(void)
{
   /* a lend that never moves hangs a hash loop; show what failed first */
   setvbuf(stdout, NULL, _IONBF, 0);
   make_body();
   if (!filestream_write_file(FIXTURE, body, FILE_LEN))
   {
      printf("FAIL: could not write the fixture\n");
      return 1;
   }

   test_mapped();
   test_unmapped();
   test_lookahead();
   test_update_stream();
   test_frontend_mapping();
   test_sha1();

   filestream_delete(FIXTURE);
   if (fails)
   {
      printf("%d FAIL\n", fails);
      return 1;
   }
   printf("vfs_view_test: PASS\n");
   return 0;
}
