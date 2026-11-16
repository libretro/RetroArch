/* vfs_size_contract_test: what a write stream says its size is.
 *
 * After every write, writev, seek and truncate on a stream - the
 * descriptor path (SEQUENTIAL_BULK) for writing, reading and writing,
 * and updating a file that exists, and the stdio path for comparison -
 * retro_vfs_file_size_impl(), filestream_get_size() and filestream_eof()
 * give the file's size as the writes made it, and that is the size on
 * disk. Pins the answers down so how a stream keeps its size can change
 * without them changing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vfs/vfs_implementation.h>
#include <streams/file_stream.h>
#include <file/file_path.h>

static unsigned checks, failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define DIR_NAME "vfs_size_contract_test_tmp"
#define FILE_W   DIR_NAME "/w.bin"

static uint8_t data[256];

/* The stream's size, and the same on disk once flushed (stdio buffers) */
#define SIZE_IS(s, n) do { \
   CHECK(retro_vfs_file_size_impl(s) == (n)); \
   CHECK(retro_vfs_file_flush_impl(s) == 0); \
   CHECK(path_get_size(FILE_W) == (n)); } while (0)

static void run(const char *what, unsigned mode, unsigned hints,
      int64_t start)
{
   libretro_vfs_implementation_file *s;
   struct retro_vfs_span spans[3];
   uint8_t buf[8];
   printf("-- %s\n", what);
   s = retro_vfs_file_open_impl(FILE_W, mode, hints);
   CHECK(s != NULL);
   if (!s)
      return;
   SIZE_IS(s, start);
   if (start)
      CHECK(retro_vfs_file_seek_impl(s, 0, RETRO_VFS_SEEK_POSITION_END) == 0
         && retro_vfs_file_tell_impl(s) == start);

   /* Appends grow it */
   CHECK(retro_vfs_file_write_impl(s, data, 10) == 10);
   SIZE_IS(s, start + 10);
   CHECK(retro_vfs_file_write_impl(s, data, 20) == 20);
   SIZE_IS(s, start + 30);
   /* A write inside it doesn't */
   CHECK(retro_vfs_file_seek_impl(s, start + 5, RETRO_VFS_SEEK_POSITION_START) == 0
         && retro_vfs_file_tell_impl(s) == start + 5);
   CHECK(retro_vfs_file_write_impl(s, data, 3) == 3);
   SIZE_IS(s, start + 30);
   /* One across its end grows it by what passes the end */
   CHECK(retro_vfs_file_seek_impl(s, start + 25, RETRO_VFS_SEEK_POSITION_START) == 0
         && retro_vfs_file_tell_impl(s) == start + 25);
   CHECK(retro_vfs_file_write_impl(s, data, 10) == 10);
   SIZE_IS(s, start + 35);
   /* A seek past the end grows nothing; the write after it does */
   CHECK(retro_vfs_file_seek_impl(s, start + 50, RETRO_VFS_SEEK_POSITION_START) == 0
         && retro_vfs_file_tell_impl(s) == start + 50);
   SIZE_IS(s, start + 35);
   CHECK(retro_vfs_file_write_impl(s, data, 4) == 4);
   SIZE_IS(s, start + 54);
   /* A failed or empty write changes nothing */
   CHECK(retro_vfs_file_write_impl(s, data, 0) == 0);
   SIZE_IS(s, start + 54);
   /* Gathered writes */
   spans[0].data = data;      spans[0].len = 7;
   spans[1].data = data + 7;  spans[1].len = 0;
   spans[2].data = data + 7;  spans[2].len = 9;
   CHECK(retro_vfs_file_writev_impl(s, spans, 3) == 16);
   SIZE_IS(s, start + 70);
   /* Truncated, then written at the new end */
   CHECK(retro_vfs_file_truncate_impl(s, start + 20) == 0);
   SIZE_IS(s, start + 20);
   CHECK(retro_vfs_file_seek_impl(s, 0, RETRO_VFS_SEEK_POSITION_END) == 0
         && retro_vfs_file_tell_impl(s) == start + 20);
   CHECK(retro_vfs_file_write_impl(s, data, 6) == 6);
   SIZE_IS(s, start + 26);
   /* Read back where it can be: reads don't change it */
   if (mode != RETRO_VFS_FILE_ACCESS_WRITE)
   {
      CHECK(retro_vfs_file_seek_impl(s, start, RETRO_VFS_SEEK_POSITION_START) == 0
         && retro_vfs_file_tell_impl(s) == start);
      CHECK(retro_vfs_file_read_impl(s, buf, sizeof(buf)) == sizeof(buf));
      /* the first write's, up to where the one inside it began */
      CHECK(!memcmp(buf, data, 5));
      SIZE_IS(s, start + 26);
   }
   /* Size queried twice in a row says the same */
   CHECK(retro_vfs_file_size_impl(s) == retro_vfs_file_size_impl(s));
   CHECK(retro_vfs_file_close_impl(s) == 0);
   CHECK(path_get_size(FILE_W) == start + 26);
}

/* The same through filestream, with eof at each end */
static void run_filestream(unsigned hints)
{
   RFILE *f = filestream_open(FILE_W, RETRO_VFS_FILE_ACCESS_READ_WRITE,
         hints);
   printf("-- filestream, hints %u\n", hints);
   CHECK(f != NULL);
   if (!f)
      return;
   CHECK(filestream_get_size(f) == 0);
   CHECK(filestream_write(f, data, 40) == 40);
   CHECK(filestream_get_size(f) == 40);
   CHECK(filestream_eof(f) == EOF);
   CHECK(filestream_seek(f, 10, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(filestream_eof(f) == 0);
   CHECK(filestream_write(f, data, 50) == 50);
   CHECK(filestream_get_size(f) == 60);
   CHECK(filestream_eof(f) == EOF);
   CHECK(filestream_tell(f) == 60);
   CHECK(filestream_close(f) == 0);
   CHECK(path_get_size(FILE_W) == 60);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(data); i++)
      data[i] = (uint8_t)(i * 37 + 11);
   path_mkdir(DIR_NAME);

   retro_vfs_file_remove_impl(FILE_W);
   run("descriptor, write", RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK, 0);
   retro_vfs_file_remove_impl(FILE_W);
   run("descriptor, read and write", RETRO_VFS_FILE_ACCESS_READ_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK, 0);
   /* An existing file of 100, updated in place */
   CHECK(filestream_write_file(FILE_W, data, 100));
   run("descriptor, update existing", RETRO_VFS_FILE_ACCESS_READ_WRITE
         | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING,
         RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK, 100);
   retro_vfs_file_remove_impl(FILE_W);
   run("stdio, read and write", RETRO_VFS_FILE_ACCESS_READ_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE, 0);

   retro_vfs_file_remove_impl(FILE_W);
   run_filestream(RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   retro_vfs_file_remove_impl(FILE_W);
   run_filestream(RETRO_VFS_FILE_ACCESS_HINT_NONE);

   retro_vfs_file_remove_impl(FILE_W);
   path_rmdir(DIR_NAME);
   printf("vfs_size_contract_test: %u checks, %u failures\n",
         checks, failures);
   return failures ? 1 : 0;
}
