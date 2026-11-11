/* A stream opened to read and write with
 * RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK, on the descriptor path
 * where the platform lists descriptor writes (the desktops among
 * them): what an emulated file system does with it, in place.
 *
 * What is asserted:
 *   - the stream is on the descriptor, not stdio (where listed),
 *   - writes at any position, reads back what was written, and the
 *     size follows,
 *   - another handle sees a write at once, with no flush,
 *   - flush succeeds, truncate shortens and grows,
 *   - reopening to update keeps the contents,
 *   - a file it creates is readable and writable by its owner
 *     afterwards, as one fopen() creates is. */

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include <libretro.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s\n", msg); \
         failures++; \
      } \
      else \
         printf("ok   %s\n", msg); \
   } while (0)

#define PATH "vfs_descriptor_rw.bin"
#define BULK RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK

static void check_path(libretro_vfs_implementation_file *s, const char *what)
{
   char msg[128];
#if defined(VFS_HAVE_DESCRIPTOR_IO) && defined(VFS_HAVE_DESCRIPTOR_WRITE)
   snprintf(msg, sizeof(msg), "%s: on the descriptor", what);
   CHECK(s->fp == NULL && s->fd >= 0, msg);
#else
   snprintf(msg, sizeof(msg), "%s: buffered (not listed)", what);
   CHECK(s->fp != NULL, msg);
#endif
}

int main(void)
{
   libretro_vfs_implementation_file *s;
   libretro_vfs_implementation_file *other;
   char buf[64];

   remove(PATH);

   /* Create */
   s = retro_vfs_file_open_impl(PATH, RETRO_VFS_FILE_ACCESS_READ_WRITE, BULK);
   CHECK(s != NULL, "create: opened");
   if (!s)
      return 1;
   check_path(s, "create");
   CHECK(retro_vfs_file_write_impl(s, "hello world", 11) == 11, "write: whole");
   CHECK(retro_vfs_file_size_impl(s) == 11, "write: size follows");

   /* Another handle sees it, without a flush (stdio would still hold
    * it, where the platform is not listed) */
#if defined(VFS_HAVE_DESCRIPTOR_IO) && defined(VFS_HAVE_DESCRIPTOR_WRITE)
   other = retro_vfs_file_open_impl(PATH, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(other != NULL, "other handle: opened");
   if (other)
   {
      memset(buf, 0, sizeof(buf));
      CHECK(retro_vfs_file_read_impl(other, buf, sizeof(buf)) == 11
            && memcmp(buf, "hello world", 11) == 0,
            "other handle: sees the write at once");
      retro_vfs_file_close_impl(other);
   }
#else
   (void)other;
#endif

   /* Overwrite in place, read back */
   CHECK(retro_vfs_file_seek_impl(s, 6, RETRO_VFS_SEEK_POSITION_START) == 0,
         "seek: in place");
   CHECK(retro_vfs_file_write_impl(s, "there", 5) == 5, "overwrite: written");
   CHECK(retro_vfs_file_seek_impl(s, 0, RETRO_VFS_SEEK_POSITION_START) == 0,
         "seek: start");
   memset(buf, 0, sizeof(buf));
   CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == 11
         && memcmp(buf, "hello there", 11) == 0, "read back: overwritten");
   CHECK(retro_vfs_file_tell_impl(s) == 11, "tell: after the read");

   /* Past the end grows it */
   CHECK(retro_vfs_file_seek_impl(s, 0, RETRO_VFS_SEEK_POSITION_END) == 0,
         "seek: end");
   CHECK(retro_vfs_file_write_impl(s, "!", 1) == 1, "append: written");
   CHECK(retro_vfs_file_size_impl(s) == 12, "append: size follows");

   CHECK(retro_vfs_file_flush_impl(s) == 0, "flush: succeeds");

   /* Truncate shorter, then longer */
   CHECK(retro_vfs_file_truncate_impl(s, 5) == 0, "truncate: shorter");
   CHECK(retro_vfs_file_size_impl(s) == 5, "truncate: size 5");
   CHECK(retro_vfs_file_truncate_impl(s, 8) == 0, "truncate: longer");
   CHECK(retro_vfs_file_size_impl(s) == 8, "truncate: size 8");
   CHECK(retro_vfs_file_seek_impl(s, 0, RETRO_VFS_SEEK_POSITION_START) == 0,
         "seek: start again");
   memset(buf, 0x55, sizeof(buf));
   CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == 8
         && memcmp(buf, "hello\0\0\0", 8) == 0, "truncate: zero filled");
   CHECK(retro_vfs_file_close_impl(s) == 0, "close: succeeds");

   /* Reopen to update: the contents stay */
   s = retro_vfs_file_open_impl(PATH,
         RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         BULK);
   CHECK(s != NULL, "update: opened");
   if (s)
   {
      check_path(s, "update");
      CHECK(retro_vfs_file_size_impl(s) == 8, "update: size kept");
      memset(buf, 0, sizeof(buf));
      CHECK(retro_vfs_file_read_impl(s, buf, 5) == 5
            && memcmp(buf, "hello", 5) == 0, "update: contents kept");
      CHECK(retro_vfs_file_write_impl(s, "XYZ", 3) == 3, "update: written");
      retro_vfs_file_close_impl(s);
   }

#if !defined(_WIN32)
   {
      struct stat st;
      CHECK(stat(PATH, &st) == 0 && (st.st_mode & 0600) == 0600,
            "create: owner can read and write it");
   }
#else
   /* Windows: a file created read-only refuses this */
   s = retro_vfs_file_open_impl(PATH,
         RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(s != NULL, "create: file is writable afterwards");
   if (s)
      retro_vfs_file_close_impl(s);
#endif

   CHECK(remove(PATH) == 0, "cleanup: removed");

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall checks passed\n");
   return 0;
}
