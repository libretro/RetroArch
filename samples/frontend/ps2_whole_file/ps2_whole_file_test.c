/* Whole-file reads and writes on the PS2, run on the console's own
 * runtime: built with the PS2 toolchain (make PS2=1) against the
 * shipping file_stream.c, vfs_implementation.c and config_file.c, and
 * run under an emulator through fileXio on host:, the same stack the
 * frontend reads its core info files and retroarch.cfg through.  A
 * plain 'make check' builds and runs the same checks on the host
 * (Linux, or Windows with CC=x86_64-w64-mingw32-gcc EXE=.exe
 * RUNNER=wine).
 *
 * What it pins:
 *
 *   read   - filestream_read_file() returns the whole file.
 *   config - a .info file parses through config_file, as core_info.c
 *            loads it.
 *   close  - a stream opened for a bulk read closes with 0.
 *   write  - filestream_write_file() round-trips through a read.
 *
 * The last line is "ps2_whole_file_test: PASS" or the failure count;
 * CI matches on it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PS2
#include <ps2_all_drivers.h>
#define FIXTURE_DIR "host:"
#else
#define FIXTURE_DIR ""
#endif

#include <boolean.h>
#include <streams/file_stream.h>
#include <file/config_file.h>

#define FIXTURE FIXTURE_DIR "ps2_whole_file.info"
#define WRITTEN FIXTURE_DIR "ps2_whole_file_w.bin"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (cond) \
         printf("ok   %s\n", msg); \
      else \
      { \
         printf("FAIL %s (line %d)\n", msg, __LINE__); \
         failures++; \
      } \
   } while (0)

static const char payload[] =
   "# Software Information\n"
   "display_name = \"Whole file fixture\"\n"
   "supported_extensions = \"bin|img\"\n";

static bool write_fixture(const char *path)
{
   size_t len = sizeof(payload) - 1;
   FILE  *fp  = fopen(path, "wb");
   if (!fp)
      return false;
   if (fwrite(payload, 1, len, fp) != len)
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
   int64_t ret = filestream_read_file(FIXTURE, &buf, &len);
   CHECK(ret == 1, "read: filestream_read_file succeeds");
   CHECK(len == (int64_t)(sizeof(payload) - 1), "read: whole length");
   CHECK(buf && !memcmp(buf, payload, sizeof(payload) - 1),
         "read: bytes match");
   free(buf);
}

static void test_config(void)
{
   const struct config_entry_list *entry = NULL;
   config_file_t *conf = config_file_new_from_path_to_string(FIXTURE);
   CHECK(conf != NULL, "config: info file loads");
   if (!conf)
      return;
   entry = config_get_entry(conf, "display_name");
   CHECK(entry && entry->value
         && !strcmp(entry->value, "Whole file fixture"),
         "config: display_name parsed");
   config_file_free(conf);
}

static void test_close(void)
{
   RFILE *file = filestream_open(FIXTURE, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   CHECK(file != NULL, "close: bulk open");
   if (file)
      CHECK(filestream_close(file) == 0, "close: returns 0");
}

static void test_write(void)
{
   void   *buf = NULL;
   int64_t len = 0;
   CHECK(filestream_write_file(WRITTEN, payload, sizeof(payload) - 1),
         "write: filestream_write_file succeeds");
   CHECK(filestream_read_file(WRITTEN, &buf, &len) == 1
         && len == (int64_t)(sizeof(payload) - 1)
         && buf && !memcmp(buf, payload, sizeof(payload) - 1),
         "write: bytes round-trip");
   free(buf);
}

int main(void)
{
#ifdef PS2
   init_fileXio_driver();
#endif
   if (!write_fixture(FIXTURE))
   {
      printf("ps2_whole_file_test: cannot write fixture\n");
      return 1;
   }

   test_read();
   test_config();
   test_close();
   test_write();

   remove(FIXTURE);
   remove(WRITTEN);

   if (failures)
      printf("ps2_whole_file_test: %u FAILED\n", failures);
   else
      printf("ps2_whole_file_test: PASS\n");
   return failures ? 1 : 0;
}
