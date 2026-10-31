/* Oracle for how the VFS reads the result of close() on its descriptor
 * path.  The VFS is compiled here with -DPS2, so whole-file reads and
 * writes take the descriptor path as they do on the console, and every
 * close() it makes goes through a stand-in with ps2sdk's semantics:
 * success returns the closed descriptor number (iomanX passes it
 * through), failure a negative value.
 *
 * What these lanes pin:
 *
 *   read     - filestream_read_file() returns the whole file when a
 *              successful close() reports a positive number.
 *   close    - filestream_close() on a descriptor stream reports 0.
 *   write    - filestream_write_file() and the read back agree.
 *   failure  - a close() that does fail still fails the close and the
 *              whole-file read.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <boolean.h>
#include <streams/file_stream.h>

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

#define FIXTURE "close_result.bin"
#define WRITTEN "close_result_w.bin"

static const char payload[] =
   "display_name = \"Close result fixture\"\n"
   "supported_extensions = \"bin|img\"\n";

static unsigned n_close;
static bool fail_next_close;

/* close() as ps2sdk's libcglue returns it: the descriptor number on
 * success, a negative value on failure. */
int vfs_close_result_close(int fd)
{
   int ret = close(fd);
   n_close++;
   if (fail_next_close)
   {
      fail_next_close = false;
      errno           = EIO;
      return -1;
   }
   if (ret != 0)
      return ret;
   /* Descriptor 0 is stdin and never reaches here; any other success
    * reports a positive number, as iomanX does. */
   return fd;
}

static bool write_fixture(const char *path, const char *data, size_t len)
{
   FILE *fp = fopen(path, "wb");
   if (!fp)
      return false;
   if (fwrite(data, 1, len, fp) != len)
   {
      fclose(fp);
      return false;
   }
   return fclose(fp) == 0;
}

static void test_read(void)
{
   void   *buf = NULL;
   int64_t len = 0;
   unsigned before;
   int64_t ret;

   CHECK(write_fixture(FIXTURE, payload, sizeof(payload) - 1),
         "write fixture");

   before = n_close;
   ret    = filestream_read_file(FIXTURE, &buf, &len);
   CHECK(n_close > before, "read: whole-file read closed a descriptor");
   CHECK(ret == 1, "read: filestream_read_file succeeds");
   CHECK(len == (int64_t)(sizeof(payload) - 1), "read: whole length");
   CHECK(buf && memcmp(buf, payload, sizeof(payload) - 1) == 0,
         "read: bytes match");
   CHECK(buf && ((char*)buf)[sizeof(payload) - 1] == '\0',
         "read: buffer terminated");
   free(buf);
}

static void test_close(void)
{
   RFILE   *file;
   unsigned before = n_close;
   file = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(file != NULL, "close: open");
   if (!file)
      return;
   CHECK(filestream_close(file) == 0, "close: positive close() is success");
   CHECK(n_close > before, "close: stream went through close()");
}

static void test_write(void)
{
   void   *buf = NULL;
   int64_t len = 0;

   remove(WRITTEN);
   CHECK(filestream_write_file(WRITTEN, payload, sizeof(payload) - 1),
         "write: filestream_write_file succeeds");
   CHECK(filestream_read_file(WRITTEN, &buf, &len) == 1, "write: read back");
   CHECK(len == (int64_t)(sizeof(payload) - 1)
         && buf && memcmp(buf, payload, sizeof(payload) - 1) == 0,
         "write: bytes round-trip");
   free(buf);
}

static void test_failure(void)
{
   RFILE   *file;
   void    *buf = NULL;
   int64_t  len = 0;

   file = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(file != NULL, "failure: open");
   if (file)
   {
      fail_next_close = true;
      CHECK(filestream_close(file) == -1, "failure: failed close reported");
   }

   fail_next_close = true;
   CHECK(filestream_read_file(FIXTURE, &buf, &len) == 0,
         "failure: failed close fails the whole-file read");
   CHECK(buf == NULL, "failure: no buffer handed out");
   fail_next_close = false;
}

int main(void)
{
   test_read();
   test_close();
   test_write();
   test_failure();

   remove(FIXTURE);
   remove(WRITTEN);

   if (failures)
   {
      fprintf(stderr, "vfs_close_result_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("vfs_close_result_test: all checks passed\n");
   return 0;
}
