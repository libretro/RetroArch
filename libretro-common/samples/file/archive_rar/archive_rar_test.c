/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (archive_rar_test.c).
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

/* RAR archives through the archive layer, as the frontend reaches them:
 * a .rar path is an archive (the file browser's Open Archive / Load
 * Archive), "x.rar#member" names a member, the layer picks the RAR
 * backend, lists the members it can unpack, reads one into memory or
 * out to a file, and gives its CRC-32. The archives are the reader's
 * own, in ../../formats/rrar/archives. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <encodings/crc32.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

static unsigned failures = 0;

static void fail(const char *what, const char *detail)
{
   fprintf(stderr, "FAIL %s: %s\n", what, detail ? detail : "");
   failures++;
}

static void archive_path(char *s, size_t len, const char *dir,
      const char *archive, const char *member)
{
   strlcpy(s, dir, len);
   strlcat(s, "/", len);
   strlcat(s, archive, len);
   if (member)
   {
      strlcat(s, "#", len);
      strlcat(s, member, len);
   }
}

static void check_paths(void)
{
   const char *p = "roms/set.rar#dir/game.bin";
   const struct file_archive_file_backend *b =
      file_archive_get_file_backend("roms/Set.RAR");

   if (!path_is_compressed_file("roms/set.rar"))
      fail("path_is_compressed_file", "set.rar is not an archive");
   if (!path_is_compressed_file("roms/SET.Rar"))
      fail("path_is_compressed_file", "SET.Rar is not an archive");
   if (path_is_compressed_file("roms/set.rarx"))
      fail("path_is_compressed_file", "set.rarx is an archive");
   if (path_get_archive_delim(p) != p + strlen("roms/set.rar"))
      fail("path_get_archive_delim", p);
   if (!b || b != file_archive_get_rar_file_backend()
         || !string_is_equal(b->ident, "rar"))
      fail("file_archive_get_file_backend", "not the RAR backend");
}

static bool list_has(const struct string_list *list, const char *name)
{
   size_t i;
   for (i = 0; list && i < list->size; i++)
      if (string_is_equal(list->elems[i].data, name))
         return true;
   return false;
}

static void check_list(const char *dir)
{
   char path[2048];
   struct string_list *list;

   archive_path(path, sizeof(path), dir, "compress_best.rar", NULL);
   if (!(list = file_archive_get_file_list(path, NULL)))
      fail("list compress_best.rar", "no list");
   else
   {
      if (     list->size != 4
            || !list_has(list, "LibarchiveAddingTest.html")
            || !list_has(list, "testlink")
            || !list_has(list, "testdir/test.txt")
            || !list_has(list, "testdir/LibarchiveAddingTest.html"))
         fail("list compress_best.rar", "not its four members");
      string_list_free(list);
   }

   /* Solid: only the first member stands alone, the rest are passed
    * over rather than listed and then refused. */
   archive_path(path, sizeof(path), dir, "rar5_multiple_files_solid.rar", NULL);
   if (!(list = file_archive_get_file_list(path, NULL)))
      fail("list rar5_multiple_files_solid.rar", "no list");
   else
   {
      if (list->size != 1 || !list_has(list, "test1.bin"))
         fail("list rar5_multiple_files_solid.rar", "not its first member alone");
      string_list_free(list);
   }
}

static void check_read(const char *dir, const char *archive,
      const char *member, int64_t size, uint32_t crc)
{
   char path[2048];
   void *buf   = NULL;
   int64_t len = 0;

   archive_path(path, sizeof(path), dir, archive, member);
   if (!file_archive_compressed_read(path, &buf, NULL, &len) || !buf)
      fail("read", path);
   else if (len != size)
      fail("read size", path);
   else if (encoding_crc32(0, (const uint8_t*)buf, (size_t)len) != crc)
      fail("read CRC-32", path);
   free(buf);

   if (file_archive_get_file_crc32(path) != crc)
      fail("file_archive_get_file_crc32", path);
}

static void check_extract(const char *dir)
{
   char path[2048];
   const char *out = "archive_rar_test.out";
   void *buf       = NULL;
   int64_t len     = 0;

   filestream_delete(out);
   archive_path(path, sizeof(path), dir, "rar5_stored.rar", "helloworld.txt");
   if (!file_archive_compressed_read(path, NULL, out, &len))
      fail("extract", path);
   else if (!filestream_read_file(out, &buf, &len) || len != 29
         || encoding_crc32(0, (const uint8_t*)buf, (size_t)len) != 0x95a043b4u)
      fail("extracted file", out);
   free(buf);
   filestream_delete(out);
}

static void check_refused(const char *dir)
{
   char path[2048];
   void *buf   = NULL;
   int64_t len = 0;
   RFILE *f;
   struct string_list *list;
   const char *junk = "archive_rar_test_junk.rar";

   archive_path(path, sizeof(path), dir, "rar5_multiple_files_solid.rar", "test2.bin");
   if (file_archive_compressed_read(path, &buf, NULL, &len))
      fail("solid member", "read where it cannot be unpacked alone");
   free(buf);

   /* Named .rar, not a RAR archive. */
   if ((f = filestream_open(junk, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      static const char not_rar[] = "PK\x03\x04 not a rar archive";
      filestream_write(f, not_rar, sizeof(not_rar) - 1);
      filestream_close(f);
   }
   list = file_archive_get_file_list(junk, NULL);
   if (list && list->size)
      fail("junk.rar", "listed members");
   string_list_free(list);
   filestream_delete(junk);
}

int main(int argc, char *argv[])
{
   const char *dir = argc > 1 ? argv[1] : "../../formats/rrar/archives";

   check_paths();
   check_list(dir);
   check_read(dir, "compress_best.rar", "testdir/test.txt", 20, 0xbec8a242u);
   check_read(dir, "compress_best.rar", "LibarchiveAddingTest.html", 20111, 0x5e05a663u);
   check_read(dir, "rar5_compressed.rar", "test.bin", 1200, 0x7cca70cdu);
   check_read(dir, "rar5_arm.rar", "elf-Linux-ARMv7-ls", 90808, 0x886f91ebu);
   check_read(dir, "filter.rar", "bsdcat.exe", 204288, 0x4db10349u);
   check_extract(dir);
   check_refused(dir);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("PASS archive_rar_test\n");
   return 0;
}
