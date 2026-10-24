/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_stream_test.c).
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

/* 7z and RAR archives opened through a VFS that has no mapping and
 * answers reads in pieces, the way an smb:// or nfs:// stream does:
 * listing an archive reads its headers through the one stream the
 * archive layer opened and nothing more, and members still extract
 * whole and correct.
 *
 * The 7z fixture is a Copy-coder archive built here: its signature
 * header and end header are below, its 64 KiB member is regenerated
 * from the same generator that made it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <streams/file_stream.h>
#include <encodings/crc32.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <vfs/vfs_implementation.h>

#define BIG_LEN   65536
#define PIECE     1024   /* the most one read answers */
#define FIXTURE   "archive_stream_test.7z"

static const uint8_t head_7z[32] =
{
   0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0x00, 0x04, 0xe0, 0x0b, 0xaf, 0x35,
   0x0a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x0d, 0xe4, 0x49, 0x20
};

static const uint8_t tail_7z[98] =
{
   0x01, 0x04, 0x06, 0x00, 0x02, 0x09, 0xc1, 0x00, 0x00, 0x0a, 0x00, 0x07,
   0x0b, 0x02, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x0c, 0xc1, 0x00,
   0x00, 0x0a, 0x00, 0x08, 0x0a, 0x01, 0xa3, 0x73, 0xe5, 0x12, 0x29, 0xdf,
   0x28, 0xde, 0x00, 0x00, 0x05, 0x02, 0x19, 0x01, 0x00, 0x11, 0x25, 0x00,
   0x62, 0x00, 0x69, 0x00, 0x67, 0x00, 0x2e, 0x00, 0x62, 0x00, 0x69, 0x00,
   0x6e, 0x00, 0x00, 0x00, 0x73, 0x00, 0x6d, 0x00, 0x61, 0x00, 0x6c, 0x00,
   0x6c, 0x00, 0x2e, 0x00, 0x74, 0x00, 0x78, 0x00, 0x74, 0x00, 0x00, 0x00,
   0x15, 0x0a, 0x01, 0x00, 0x20, 0x80, 0xa4, 0x81, 0x20, 0x80, 0xa4, 0x81,
   0x00, 0x00
};

static const char small_txt[] = "stream me\n";

static unsigned failures;
static unsigned opens;
static uint64_t bytes_read;

static void fail(const char *what, const char *detail)
{
   printf("[FAIL] %s: %s\n", what, detail);
   failures++;
}

static void big_member(uint8_t *out)
{
   uint32_t x = 1;
   size_t   i;
   for (i = 0; i < BIG_LEN; i++)
   {
      x      = x * 1103515245u + 12345u;
      out[i] = (uint8_t)(x >> 16);
   }
}

/* The VFS a network share presents: no mapping, reads in pieces. */
static const char *fe_get_path(struct retro_vfs_file_handle *h)
{
   return retro_vfs_file_get_path_impl((libretro_vfs_implementation_file*)h);
}

static struct retro_vfs_file_handle *fe_open(const char *path,
      unsigned mode, unsigned hints)
{
   opens++;
   return (struct retro_vfs_file_handle*)retro_vfs_file_open_impl(path, mode, hints);
}

static int fe_close(struct retro_vfs_file_handle *h)
{
   return retro_vfs_file_close_impl((libretro_vfs_implementation_file*)h);
}

static int64_t fe_size(struct retro_vfs_file_handle *h)
{
   return retro_vfs_file_size_impl((libretro_vfs_implementation_file*)h);
}

static int64_t fe_tell(struct retro_vfs_file_handle *h)
{
   return retro_vfs_file_tell_impl((libretro_vfs_implementation_file*)h);
}

static int64_t fe_seek(struct retro_vfs_file_handle *h, int64_t off, int whence)
{
   return retro_vfs_file_seek_impl((libretro_vfs_implementation_file*)h, off, whence);
}

static int64_t fe_read(struct retro_vfs_file_handle *h, void *s, uint64_t len)
{
   int64_t r;
   if (len > PIECE)
      len = PIECE;
   r = retro_vfs_file_read_impl((libretro_vfs_implementation_file*)h, s, len);
   if (r > 0)
      bytes_read += (uint64_t)r;
   return r;
}

static int64_t fe_write(struct retro_vfs_file_handle *h, const void *s, uint64_t len)
{
   return retro_vfs_file_write_impl((libretro_vfs_implementation_file*)h, s, len);
}

static int fe_flush(struct retro_vfs_file_handle *h)
{
   return retro_vfs_file_flush_impl((libretro_vfs_implementation_file*)h);
}

static int fe_remove(const char *path)
{
   return retro_vfs_file_remove_impl(path);
}

static int fe_rename(const char *a, const char *b)
{
   return retro_vfs_file_rename_impl(a, b);
}

static void install_vfs(void)
{
   static struct retro_vfs_interface iface;
   struct retro_vfs_interface_info info;
   memset(&iface, 0, sizeof(iface));
   iface.get_path = fe_get_path;
   iface.open     = fe_open;
   iface.close    = fe_close;
   iface.size     = fe_size;
   iface.tell     = fe_tell;
   iface.seek     = fe_seek;
   iface.read     = fe_read;
   iface.write    = fe_write;
   iface.flush    = fe_flush;
   iface.remove   = fe_remove;
   iface.rename   = fe_rename;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface                      = &iface;
   filestream_vfs_init(&info);
}

static bool write_fixture(void)
{
   uint8_t *big = (uint8_t*)malloc(BIG_LEN);
   FILE    *f;
   bool     ok;
   if (!big)
      return false;
   big_member(big);
   if (!(f = fopen(FIXTURE, "wb")))
   {
      free(big);
      return false;
   }
   ok =     fwrite(head_7z, 1, sizeof(head_7z), f) == sizeof(head_7z)
         && fwrite(big, 1, BIG_LEN, f) == BIG_LEN
         && fwrite(small_txt, 1, sizeof(small_txt) - 1, f) == sizeof(small_txt) - 1
         && fwrite(tail_7z, 1, sizeof(tail_7z), f) == sizeof(tail_7z);
   fclose(f);
   free(big);
   return ok;
}

/* Lists @path: one open, and at most @max_read octets read for it. */
static void check_list(const char *what, const char *path,
      const char *want, unsigned want_count, uint64_t max_read)
{
   struct string_list *list;
   char detail[256];
   unsigned i;
   unsigned before = failures;
   bool found = false;

   opens      = 0;
   bytes_read = 0;
   if (!(list = file_archive_get_file_list(path, NULL)))
   {
      fail(what, "no list");
      return;
   }
   for (i = 0; i < list->size; i++)
      if (string_is_equal(list->elems[i].data, want))
         found = true;
   if (list->size != want_count || !found)
   {
      snprintf(detail, sizeof(detail), "%u members, %s %s",
            (unsigned)list->size, want, found ? "listed" : "missing");
      fail(what, detail);
   }
   string_list_free(list);
   if (opens != 1)
   {
      snprintf(detail, sizeof(detail), "opened %u times", opens);
      fail(what, detail);
   }
   if (bytes_read > max_read)
   {
      snprintf(detail, sizeof(detail), "read %llu octets to list it, at most %llu wanted",
            (unsigned long long)bytes_read, (unsigned long long)max_read);
      fail(what, detail);
   }
   if (failures == before)
      printf("[PASS] %s: listed with %llu octets read\n", what,
            (unsigned long long)bytes_read);
}

static void check_member(const char *what, const char *path,
      const uint8_t *want, size_t want_len)
{
   void   *buf = NULL;
   int64_t len = 0;
   unsigned before = failures;

   if (!file_archive_compressed_read(path, &buf, NULL, &len) || !buf)
      fail(what, "not read");
   else if (len != (int64_t)want_len)
      fail(what, "wrong length");
   else if (memcmp(buf, want, want_len) != 0)
      fail(what, "wrong content");
   free(buf);
   if (failures == before)
      printf("[PASS] %s\n", what);
}

static void check_crc(const char *what, const char *path,
      size_t want_len, uint32_t want_crc)
{
   void   *buf = NULL;
   int64_t len = 0;

   if (!file_archive_compressed_read(path, &buf, NULL, &len) || !buf)
      fail(what, "not read");
   else if (len != (int64_t)want_len
         || encoding_crc32(0, (const uint8_t*)buf, want_len) != want_crc)
      fail(what, "wrong member");
   else
      printf("[PASS] %s\n", what);
   free(buf);
}

int main(int argc, char *argv[])
{
   const char *rar_dir = argc > 1 ? argv[1] : "../../formats/rrar/archives";
   char path[PATH_MAX_LENGTH];
   uint8_t *big;

   if (!write_fixture())
   {
      printf("[FAIL] could not write %s\n", FIXTURE);
      return 1;
   }
   install_vfs();

   /* 7z: the signature header and the end header are all a listing
    * needs (and the signature once more, checked by the backend); the
    * member data between them is never touched */
   check_list("7z list", FIXTURE, "big.bin", 2,
         sizeof(head_7z) + sizeof(tail_7z) + 6);
   if (!(big = (uint8_t*)malloc(BIG_LEN)))
      return 1;
   big_member(big);
   check_member("7z big.bin", FIXTURE "#big.bin", big, BIG_LEN);
   check_member("7z small.txt", FIXTURE "#small.txt",
         (const uint8_t*)small_txt, sizeof(small_txt) - 1);
   free(big);

   /* RAR: its block headers, not the packed member */
   fill_pathname_join_special(path, rar_dir, "filter.rar", sizeof(path));
   check_list("rar list", path, "bsdcat.exe", 1, 4096);
   strlcat(path, "#bsdcat.exe", sizeof(path));
   check_crc("rar bsdcat.exe", path, 204288, 0x4db10349u);

   remove(FIXTURE);
   if (failures)
   {
      printf("archive_stream_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("archive_stream_test: all checks passed\n");
   return 0;
}
