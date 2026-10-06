/* filestream_prefetch(): the read-ahead hint on a local file.
 *
 * The hint asks the OS to start bringing a range of the file in, so a
 * read or a touch of the mapping that follows does not wait on the
 * disk. It can only be a hint, which bounds what a test may assert:
 * the OS may ignore it, and whether a page is resident afterwards is a
 * race with the readahead. What is pinned is the contract around it:
 *
 *    - every shape of call returns: a range inside the file, one
 *      starting inside and running past the end, one entirely past
 *      the end, a zero length, a NULL stream;
 *    - the file's contents are unchanged by any of them, read through
 *      the mapping (FREQUENT_ACCESS) and through the descriptor;
 *    - a frontend VFS gets no call at all: with the read callbacks
 *      installed the hint is a no-op and nothing is dereferenced.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./vfs_prefetch_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

#define FIXTURE_LEN (1024 * 1024 + 777)

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
   } while (0)

static uint8_t pattern(size_t i)
{
   return (uint8_t)((i * 2654435761u) >> 13);
}

static bool write_fixture(const char *path)
{
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint8_t *buf;
   size_t   i;
   bool     ok;

   if (!f)
      return false;
   if (!(buf = (uint8_t*)malloc(FIXTURE_LEN)))
   {
      filestream_close(f);
      return false;
   }
   for (i = 0; i < FIXTURE_LEN; i++)
      buf[i] = pattern(i);
   ok = filestream_write(f, buf, FIXTURE_LEN) == FIXTURE_LEN;
   free(buf);
   filestream_close(f);
   return ok;
}

/* Prefetches every shape of range on @f, then reads the whole file
 * back and compares it with the pattern. */
static void check_stream(RFILE *f, const char *what)
{
   uint8_t *buf = (uint8_t*)malloc(FIXTURE_LEN);
   size_t   i;
   int64_t  got;

   CHECK(buf != NULL, what);
   if (!buf)
      return;

   filestream_prefetch(f, 0, 65536);
   filestream_prefetch(f, 4096 + 100, 300000);
   filestream_prefetch(f, FIXTURE_LEN - 10, 1000000);
   filestream_prefetch(f, FIXTURE_LEN, 4096);
   filestream_prefetch(f, FIXTURE_LEN + 1000000, 4096);
   filestream_prefetch(f, 0, 0);
   filestream_prefetch(f, 0, (uint64_t)-1);
   filestream_prefetch(NULL, 0, 4096);

   filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_START);
   got = filestream_read(f, buf, FIXTURE_LEN);
   CHECK(got == FIXTURE_LEN, what);
   for (i = 0; got == FIXTURE_LEN && i < FIXTURE_LEN; i++)
      if (buf[i] != pattern(i))
         break;
   CHECK(got == FIXTURE_LEN && i == FIXTURE_LEN, what);
   free(buf);
}

/* A frontend VFS: open/read callbacks that count their calls. Nothing
 * in the interface carries a prefetch, so the hint must not reach
 * them and must touch nothing of theirs. */
static int frontend_calls;

static struct retro_vfs_file_handle *fe_open(const char *path,
      unsigned mode, unsigned hints)
{
   frontend_calls++;
   (void)path; (void)mode; (void)hints;
   return (struct retro_vfs_file_handle*)&frontend_calls;
}

static int fe_close(struct retro_vfs_file_handle *h)
{
   frontend_calls++;
   (void)h;
   return 0;
}

static int64_t fe_read(struct retro_vfs_file_handle *h, void *s,
      uint64_t len)
{
   frontend_calls++;
   (void)h; (void)s; (void)len;
   return 0;
}

int main(void)
{
   char   path[256];
   RFILE *f;

   strcpy(path, "vfs_prefetch_fixture.bin");
   if (!write_fixture(path))
   {
      printf("FAIL: cannot write %s\n", path);
      return 1;
   }

   /* mapped, where the platform maps */
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   CHECK(f != NULL, "open with the mapping hint");
   if (f)
   {
      int64_t map_len = 0;
      printf("mapping: %s\n",
            filestream_get_mapped_ptr(f, &map_len) ? "yes" : "no");
      check_stream(f, "mapped stream");
      filestream_close(f);
   }

   /* the descriptor path */
   f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   CHECK(f != NULL, "open without the hint");
   if (f)
   {
      check_stream(f, "descriptor stream");
      filestream_close(f);
   }

   /* a frontend VFS: the hint stops at filestream */
   {
      struct retro_vfs_interface       iface;
      struct retro_vfs_interface_info  info;

      memset(&iface, 0, sizeof(iface));
      iface.open  = fe_open;
      iface.close = fe_close;
      iface.read  = fe_read;
      info.required_interface_version = 1;
      info.iface                      = &iface;
      filestream_vfs_init(&info);

      f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      CHECK(f != NULL, "open through the frontend VFS");
      if (f)
      {
         int before = frontend_calls;
         filestream_prefetch(f, 0, 65536);
         filestream_prefetch(f, FIXTURE_LEN + 1, 1);
         CHECK(frontend_calls == before, "frontend VFS sees no prefetch");
         filestream_close(f);
      }
   }

   remove(path);
   if (test_fails)
   {
      printf("%d FAIL\n", test_fails);
      return 1;
   }
   printf("vfs_prefetch_test: PASS\n");
   return 0;
}
