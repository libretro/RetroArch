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
#if defined(__linux__)
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#endif
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

/* The identity a stable open hands back is the one the stream answers
 * with afterwards; a refused open hands back none */
static void test_open_stable_identity(void)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_file_identity a, b;
   uint8_t buf[16];
   memset(&a, 0xaa, sizeof(a));
   s = retro_vfs_file_open_stable_impl(FILE_A, &a);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(retro_vfs_file_identity_impl(s, &b));
      CHECK(!memcmp(&a, &b, sizeof(a)));
      CHECK(a.size == LEN && retro_vfs_file_size_impl(s) == LEN);
      CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
      CHECK(!memcmp(buf, data, sizeof(buf)));
#ifdef _WIN32
      CHECK(!filestream_write_file(FILE_A, data, 1));
#endif
      retro_vfs_file_close_impl(s);
   }
   memset(&a, 0xaa, sizeof(a));
   memset(&b, 0, sizeof(b));
   CHECK(retro_vfs_file_open_stable_impl(DIR_NAME, &a) == NULL);
   CHECK(!memcmp(&a, &b, sizeof(a)));
   memset(&a, 0xaa, sizeof(a));
   CHECK(retro_vfs_file_open_stable_impl(DIR_NAME "/none", &a) == NULL);
   CHECK(!memcmp(&a, &b, sizeof(a)));
   CHECK(retro_vfs_file_open_stable_impl(FILE_A, NULL) == NULL);
}

/* Identity by path: what is there, followed through links; missing,
 * or can't be told. Owner, group and permissions where POSIX has them. */
static void test_stat_identity(void)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_file_identity a, b;
#ifndef _WIN32
   struct stat st;
#endif
   s = retro_vfs_file_open_stable_impl(FILE_A, &a);
   CHECK(s != NULL);
   if (s)
      retro_vfs_file_close_impl(s);
   CHECK(a.type == RETRO_VFS_IDENTITY_REGULAR);
   CHECK(retro_vfs_stat_identity_impl(FILE_A, &b) == 1);
   CHECK(b.type == RETRO_VFS_IDENTITY_REGULAR);
   CHECK(b.dev == a.dev && !memcmp(b.id, a.id, sizeof(a.id))
         && b.size == a.size && b.mtime == a.mtime);
   CHECK(b.flags == a.flags && b.uid == a.uid && b.gid == a.gid
         && b.perm == a.perm);
#ifndef _WIN32
   CHECK(stat(FILE_A, &st) == 0);
   CHECK(a.flags & RETRO_VFS_IDENTITY_HAS_OWNER);
   CHECK(a.uid == (uint32_t)st.st_uid && a.gid == (uint32_t)st.st_gid);
   CHECK(a.perm == (uint16_t)(st.st_mode & 07777));
#else
   CHECK(!(a.flags & RETRO_VFS_IDENTITY_HAS_OWNER) && !a.uid && !a.perm);
#endif
   /* A folder */
   CHECK(retro_vfs_stat_identity_impl(DIR_NAME, &b) == 1);
   CHECK(b.type == RETRO_VFS_IDENTITY_DIRECTORY);
   /* Nothing there */
   memset(&b, 0xaa, sizeof(b));
   CHECK(retro_vfs_stat_identity_impl(DIR_NAME "/none", &b) == 0);
   CHECK(b.type == 0 && b.size == 0);
   CHECK(retro_vfs_stat_identity_impl(DIR_NAME "/none/x", &b) == 0);
   CHECK(retro_vfs_stat_identity_impl("", &b) == -1);
   CHECK(retro_vfs_stat_identity_impl(FILE_A, NULL) == -1);
#ifndef _WIN32
   /* A file where a folder should be can't be told */
   memset(&b, 0xaa, sizeof(b));
   CHECK(retro_vfs_stat_identity_impl(FILE_A "/x", &b) == -1);
   CHECK(b.type == 0 && b.size == 0);
   /* Through a link, the file it names */
   CHECK(symlink("a.bin", DIR_NAME "/link") == 0);
   CHECK(retro_vfs_stat_identity_impl(DIR_NAME "/link", &b) == 1);
   CHECK(!memcmp(b.id, a.id, sizeof(a.id)) && b.dev == a.dev);
   unlink(DIR_NAME "/link");
   /* A FIFO is there, and is neither */
   CHECK(mkfifo(DIR_NAME "/fifo2", 0600) == 0);
   CHECK(retro_vfs_stat_identity_impl(DIR_NAME "/fifo2", &b) == 1);
   CHECK(b.type == RETRO_VFS_IDENTITY_OTHER);
   unlink(DIR_NAME "/fifo2");
   /* A folder we may not search: can't be told, which is not missing */
   if (geteuid() == 0)
      printf("note: running as root, unsearchable folder not checked\n");
   else
   {
      CHECK(path_mkdir(DIR_NAME "/locked"));
      CHECK(filestream_write_file(DIR_NAME "/locked/f", data, 1));
      CHECK(chmod(DIR_NAME "/locked", 0600) == 0);
      CHECK(retro_vfs_stat_identity_impl(DIR_NAME "/locked/f", &b) == -1);
      CHECK(chmod(DIR_NAME "/locked", 0700) == 0);
      retro_vfs_file_remove_impl(DIR_NAME "/locked/f");
      path_rmdir(DIR_NAME "/locked");
   }
#else
   /* A device is there, and is neither */
   CHECK(retro_vfs_stat_identity_impl("NUL", &b) == 1);
   CHECK(b.type == RETRO_VFS_IDENTITY_OTHER);
#endif
}

#if defined(__linux__)
/* The mount /proc gives for a descriptor, 0 when none */
static unsigned long fdinfo_mnt(int fd)
{
   char name[64];
   char text[512];
   char *at;
   size_t got;
   FILE *fp;
   sprintf(name, "/proc/self/fdinfo/%d", fd);
   if (!(fp = fopen(name, "r")))
      return 0;
   got       = fread(text, 1, sizeof(text) - 1, fp);
   text[got] = '\0';
   fclose(fp);
   return (at = strstr(text, "mnt_id:")) ? strtoul(at + 7, NULL, 10) : 0;
}

static bool magic_kept(unsigned long t)
{
   return t == 0xEF53UL || t == 0x58465342UL || t == 0x9123683EUL
      || t == 0xF2F52010UL || t == 0x01021994UL || t == 0x2FC12FC1UL
      || t == 0xCA451A4EUL;
}
#endif

/* A file's volume: its mount flags and filesystem as statfs/statvfs
 * tell them, its mount as /proc does, Windows' drive type */
static void test_volume(void)
{
   libretro_vfs_implementation_file *s, *t;
   struct retro_vfs_file_identity id;
   struct retro_vfs_volume v, w;
#if defined(__linux__)
   struct statfs fs;
   struct statvfs svfs;
#endif
   s = retro_vfs_file_open_stable_impl(FILE_A, &id);
   t = retro_vfs_file_open_stable_impl(FILE_A, &id);
   CHECK(s && t);
   if (!s || !t)
      goto end;
   CHECK(!retro_vfs_file_volume_impl(NULL, 0, &v));
   CHECK(!retro_vfs_file_volume_impl(s, 0, NULL));
   CHECK(retro_vfs_file_volume_impl(s, 0, &v) && !v.flags && !v.kind);
   CHECK(retro_vfs_stat_volume_impl(DIR_NAME "/none",
            RETRO_VFS_VOLUME_WANT_FLAGS, &v) == 0);
   CHECK(retro_vfs_stat_volume_impl("", 0, &v) == -1);
#if defined(__linux__)
   CHECK(retro_vfs_file_volume_impl(s, RETRO_VFS_VOLUME_WANT_FLAGS, &v));
   CHECK(statfs(FILE_A, &fs) == 0 && statvfs(FILE_A, &svfs) == 0);
   CHECK(!!(v.flags & RETRO_VFS_VOLUME_READ_ONLY)
         == !!(svfs.f_flag & ST_RDONLY));
   CHECK(!!(v.flags & RETRO_VFS_VOLUME_CTIME_KEPT)
         == magic_kept((unsigned long)fs.f_type));
   CHECK(!(v.flags & RETRO_VFS_VOLUME_MOUNT));
   CHECK(retro_vfs_stat_volume_impl(FILE_A, RETRO_VFS_VOLUME_WANT_FLAGS,
            &w) == 1 && w.flags == v.flags);
   /* Two opens share a mount, the one /proc names; by path too */
   CHECK(retro_vfs_file_volume_impl(s, RETRO_VFS_VOLUME_WANT_MOUNT
            | RETRO_VFS_VOLUME_WANT_FLAGS, &v));
   {
      int fd = open(FILE_A, O_RDONLY);
      CHECK(fd >= 0);
      CHECK((v.flags & RETRO_VFS_VOLUME_MOUNT) && v.mount
            && v.mount == fdinfo_mnt(fd));
      if (fd >= 0)
         close(fd);
   }
   CHECK(retro_vfs_file_volume_impl(t, RETRO_VFS_VOLUME_WANT_MOUNT, &w)
         && w.mount == v.mount);
   CHECK(retro_vfs_stat_volume_impl(FILE_A, RETRO_VFS_VOLUME_WANT_MOUNT,
            &w) == 1 && w.mount == v.mount);
   CHECK(retro_vfs_stat_volume_impl("/proc/self/exe",
            RETRO_VFS_VOLUME_WANT_MOUNT, &w) == 1 && w.mount);
   CHECK(retro_vfs_stat_volume_impl(DIR_NAME "/none/x",
            RETRO_VFS_VOLUME_WANT_MOUNT, &w) == 0);
   /* /proc is another mount, and keeps no ctime */
   CHECK(retro_vfs_stat_volume_impl("/proc/self/status",
            RETRO_VFS_VOLUME_WANT_MOUNT | RETRO_VFS_VOLUME_WANT_FLAGS, &w)
         == 1);
   CHECK(w.mount && w.mount != v.mount
         && !(w.flags & RETRO_VFS_VOLUME_CTIME_KEPT));
#elif defined(_WIN32)
   {
      char full[1024];
      libretro_vfs_implementation_file *a;
      CHECK(!retro_vfs_file_volume_impl(s, RETRO_VFS_VOLUME_WANT_MOUNT, &v));
      /* A relative path names no drive */
      CHECK(retro_vfs_file_volume_impl(s, RETRO_VFS_VOLUME_WANT_KIND, &v)
            && v.kind == 0);
      CHECK(GetFullPathNameA(FILE_A, sizeof(full), full, NULL) > 0);
      a = retro_vfs_file_open_stable_impl(full, &id);
      CHECK(a != NULL);
      if (a)
      {
         CHECK(retro_vfs_file_volume_impl(a, RETRO_VFS_VOLUME_WANT_KIND, &v));
         printf("note: %s is drive kind %u\n", full, (unsigned)v.kind);
         CHECK(v.kind == RETRO_VFS_VOLUME_FIXED);
         if (retro_vfs_file_volume_impl(a, RETRO_VFS_VOLUME_WANT_FLAGS, &v))
            CHECK(!(v.flags & (RETRO_VFS_VOLUME_READ_ONLY
                        | RETRO_VFS_VOLUME_CTIME_KEPT)));
         else
            printf("note: no volume flags here\n");
         retro_vfs_file_close_impl(a);
      }
      CHECK(retro_vfs_stat_volume_impl(full, RETRO_VFS_VOLUME_WANT_KIND,
               &w) == 1 && w.kind == RETRO_VFS_VOLUME_FIXED);
   }
#endif
end:
   if (s)
      retro_vfs_file_close_impl(s);
   if (t)
      retro_vfs_file_close_impl(t);
}

/* fs-verity: sealed and measured by the kernel, in the folder
 * VFS_VERITY_DIR names (a filesystem with it on); elsewhere "not here" */
static void test_verity(void)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_file_identity id;
   uint8_t d[RETRO_VFS_VERITY_MAX], e[RETRO_VFS_VERITY_MAX];
   char path[1024];
   const char *dir = getenv("VFS_VERITY_DIR");
   size_t n;
   CHECK(!retro_vfs_file_verity_measure_impl(NULL, d, sizeof(d)));
   CHECK(retro_vfs_file_verity_enable_impl(NULL) == -1);
   /* the test folder: no seal to measure */
   if ((s = retro_vfs_file_open_stable_impl(FILE_A, &id)))
   {
      CHECK(!retro_vfs_file_verity_measure_impl(s, d, sizeof(d)));
      retro_vfs_file_close_impl(s);
   }
   if (!dir)
   {
      printf("note: VFS_VERITY_DIR not set, sealing not checked\n");
      return;
   }
   snprintf(path, sizeof(path), "%s/sealed.bin", dir);
   retro_vfs_file_remove_impl(path);
   CHECK(filestream_write_file(path, data, sizeof(data)));
   CHECK((s = retro_vfs_file_open_stable_impl(path, &id)) != NULL);
   if (!s)
      return;
   CHECK(!retro_vfs_file_verity_measure_impl(s, d, sizeof(d)));
   if (retro_vfs_file_verity_enable_impl(s) == 0)
   {
      printf("note: no fs-verity on this kernel or filesystem\n");
      retro_vfs_file_close_impl(s);
      retro_vfs_file_remove_impl(path);
      return;
   }
   CHECK((n = retro_vfs_file_verity_measure_impl(s, d, sizeof(d))) == 32);
   /* Again: already sealed; too small a buffer tells nothing */
   CHECK(retro_vfs_file_verity_enable_impl(s) == 1);
   CHECK(!retro_vfs_file_verity_measure_impl(s, e, 31));
   retro_vfs_file_close_impl(s);
   CHECK((s = retro_vfs_file_open_stable_impl(path, &id)) != NULL);
   if (s)
   {
      CHECK(retro_vfs_file_verity_measure_impl(s, e, sizeof(e)) == n
            && !memcmp(d, e, n));
      retro_vfs_file_close_impl(s);
   }
   /* Sealed for good: no writer opens it */
   CHECK(!retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ_WRITE
            | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, 0));
   retro_vfs_file_remove_impl(path);
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
   test_open_stable_identity();
   test_stat_identity();
   test_volume();
   test_verity();
   test_identity_follows_handle();
   retro_vfs_file_remove_impl(FILE_A);
   retro_vfs_file_remove_impl(FILE_B);
   retro_vfs_file_remove_impl(FILE_E);
   path_rmdir(DIR_NAME);
   printf("vfs_stable_test: %u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
