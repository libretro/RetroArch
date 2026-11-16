/* vfs_stable_test: RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE,
 * retro_vfs_file_identity_impl() and retro_vfs_file_map_impl().
 *
 * A stable open reads a regular file on the descriptor path and refuses
 * anything else (a directory, a FIFO, without waiting on it); on Windows
 * nothing writes, renames or deletes the file while it is open. Identity
 * comes from the open handle, and on POSIX matches stat() of the path.
 * The on-demand mapping is the file's bytes, leaves the read position
 * alone, is the same mapping on a second call and is gone at close.
 * Opens without the hint behave exactly as they did. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vfs/vfs_implementation.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static unsigned checks, failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define DIR_NAME "vfs_stable_test_tmp"
#define FILE_A   DIR_NAME "/a.bin"
#define FILE_B   DIR_NAME "/b.bin"
#define FILE_E   DIR_NAME "/empty.bin"
#define LEN      (65536 + 3)

static uint8_t data[LEN];

static void test_stable_read_and_map(void)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_file_identity id;
   const uint8_t *p, *q;
   uint8_t buf[100];
   int64_t len = 0, len2 = 0;
#ifndef _WIN32
   struct stat st;
#endif

   s = retro_vfs_file_open_impl(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(retro_vfs_file_size_impl(s) == LEN);

   CHECK(retro_vfs_file_identity_impl(s, &id));
   CHECK(id.size == LEN);
#ifndef _WIN32
   CHECK(stat(FILE_A, &st) == 0);
   CHECK(id.dev == (uint64_t)st.st_dev);
   CHECK(id.mtime == (int64_t)st.st_mtime);
   CHECK(id.ctime == (int64_t)st.st_ctime);
   {
      uint64_t ino = 0;
      unsigned i;
      for (i = 0; i < 8; i++)
         ino = (ino << 8) | id.id[8 + i];
      CHECK(ino == (uint64_t)st.st_ino);
   }
#else
   /* Written in the last minute, in seconds from 1970 */
   CHECK(id.mtime > 1700000000);
#endif

   CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
   CHECK(!memcmp(buf, data, sizeof(buf)));

   /* The mapping is the file, and reads don't see it */
   p = retro_vfs_file_map_impl(s, &len);
#ifdef VFS_HAVE_FILE_MAPPING
   CHECK(p != NULL && len == LEN);
#else
   /* Nothing maps in this build: the caller reads */
   CHECK(p == NULL && len == 0);
#endif
   if (p)
      CHECK(!memcmp(p, data, LEN));
   CHECK(retro_vfs_file_tell_impl(s) == sizeof(buf));
   CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
   CHECK(!memcmp(buf, data + 100, sizeof(buf)));
   q = retro_vfs_file_map_impl(s, &len2);
   CHECK(q == p && len2 == len);
   /* Not a FREQUENT_ACCESS mapping: the copy-free accessor for that
    * one still says there is none */
   CHECK(retro_vfs_file_get_mapped_ptr_impl(s, NULL) == NULL);

#ifdef _WIN32
   /* Nothing writes, renames or deletes it while it is open */
   CHECK(!filestream_write_file(FILE_A, data, 1));
   CHECK(retro_vfs_file_remove_impl(FILE_A) != 0);
   CHECK(retro_vfs_file_rename_impl(FILE_A, FILE_B) != 0);
   /* Read by anyone */
   {
      RFILE *r = filestream_open(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(r != NULL);
      if (r)
         filestream_close(r);
   }
#endif
   CHECK(retro_vfs_file_close_impl(s) == 0);
#ifdef _WIN32
   /* And writable again once it is closed */
   CHECK(filestream_write_file(FILE_A, data, LEN));
#endif
}

static void test_stable_refuses(void)
{
   libretro_vfs_implementation_file *s;
   /* Not a regular file */
   s = retro_vfs_file_open_impl(DIR_NAME, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s == NULL);
   if (s)
      retro_vfs_file_close_impl(s);
   /* Missing */
   s = retro_vfs_file_open_impl(DIR_NAME "/none", RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s == NULL);
#ifndef _WIN32
   /* A FIFO with no writer: refused at once, not waited on (the alarm
    * kills a run that waits) */
   CHECK(mkfifo(DIR_NAME "/fifo", 0600) == 0);
   alarm(10);
   s = retro_vfs_file_open_impl(DIR_NAME "/fifo", RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   alarm(0);
   CHECK(s == NULL);
   if (s)
      retro_vfs_file_close_impl(s);
   unlink(DIR_NAME "/fifo");
#endif
}

static void test_others_unchanged(void)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_file_identity id;
   int64_t len = 1;
   uint8_t buf[16];

   /* Buffered (stdio) streams have no identity or on-demand mapping */
   s = retro_vfs_file_open_impl(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!retro_vfs_file_identity_impl(s, &id));
      CHECK(retro_vfs_file_map_impl(s, &len) == NULL && len == 0);
      CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
      CHECK(!memcmp(buf, data, sizeof(buf)));
      retro_vfs_file_close_impl(s);
   }

   /* A bulk read stream has an identity where it takes the descriptor
    * path, and its reads are as before */
   s = retro_vfs_file_open_impl(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(s != NULL);
   if (s)
   {
#ifdef VFS_HAVE_DESCRIPTOR_IO
      CHECK(retro_vfs_file_identity_impl(s, &id) && id.size == LEN);
#else
      CHECK(!retro_vfs_file_identity_impl(s, &id));
#endif
      CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
      CHECK(!memcmp(buf, data, sizeof(buf)));
#ifdef _WIN32
      /* No stable hint: shared for writing as always */
      CHECK(filestream_write_file(FILE_A, data, LEN));
#endif
      retro_vfs_file_close_impl(s);
   }

   /* The hint means nothing to a write: it opens and writes as before */
   s = retro_vfs_file_open_impl(FILE_B, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(retro_vfs_file_write_impl(s, data, 10) == 10);
      CHECK(retro_vfs_file_close_impl(s) == 0);
   }
   CHECK(path_get_size(FILE_B) == 10);

   /* An empty file opens, and has nothing to map */
   CHECK(filestream_write_file(FILE_E, data, 0));
   s = retro_vfs_file_open_impl(FILE_E, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s != NULL);
   if (s)
   {
      len = 1;
      CHECK(retro_vfs_file_identity_impl(s, &id) && id.size == 0);
      CHECK(retro_vfs_file_map_impl(s, &len) == NULL && len == 0);
      retro_vfs_file_close_impl(s);
   }
}

/* A new file put at the path is a new identity; the open stream keeps
 * the one it opened */
static void test_identity_follows_handle(void)
{
   libretro_vfs_implementation_file *s, *t;
   struct retro_vfs_file_identity a, a2, b;
   s = retro_vfs_file_open_impl(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(s && retro_vfs_file_identity_impl(s, &a));
   CHECK(filestream_write_file(FILE_B, data, 1000));
#ifdef _WIN32
   /* Windows can't rename over a file that is open */
   retro_vfs_file_close_impl(s);
   s = NULL;
   CHECK(retro_vfs_file_remove_impl(FILE_A) == 0);
#endif
   CHECK(retro_vfs_file_rename_impl(FILE_B, FILE_A) == 0);
   t = retro_vfs_file_open_impl(FILE_A, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_IMPL_STABLE);
   CHECK(t && retro_vfs_file_identity_impl(t, &b));
   CHECK(b.size == 1000);
   CHECK(memcmp(a.id, b.id, sizeof(a.id)) || a.dev != b.dev);
   if (s)
   {
      CHECK(retro_vfs_file_identity_impl(s, &a2));
      CHECK(!memcmp(a.id, a2.id, sizeof(a.id)) && a2.size == LEN);
      retro_vfs_file_close_impl(s);
   }
   if (t)
      retro_vfs_file_close_impl(t);
}

int main(void)
{
   unsigned i;
   uint32_t x = 2463534242u;
   for (i = 0; i < LEN; i++)
   {
      x ^= x << 13;
      x ^= x >> 17;
      x ^= x << 5;
      data[i] = (uint8_t)x;
   }
   path_mkdir(DIR_NAME);
   if (!filestream_write_file(FILE_A, data, LEN))
   {
      printf("FAIL: cannot write %s\n", FILE_A);
      return 1;
   }
   test_stable_read_and_map();
   test_stable_refuses();
   test_others_unchanged();
   test_identity_follows_handle();
   retro_vfs_file_remove_impl(FILE_A);
   retro_vfs_file_remove_impl(FILE_B);
   retro_vfs_file_remove_impl(FILE_E);
   path_rmdir(DIR_NAME);
   printf("vfs_stable_test: %u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
