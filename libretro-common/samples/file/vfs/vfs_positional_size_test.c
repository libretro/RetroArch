/* vfs_positional_size_test: a stream's size after positional writes.
 *
 * retro_vfs_file_write_at_impl() and filestream_write_at() write without
 * moving the position, from any number of threads; afterwards the
 * stream's size (retro_vfs_file_size_impl(), filestream_get_size(),
 * filestream_eof()) is the file's, as it is after an ordinary write:
 * grown by a write past the end, unchanged by one inside, grown by each
 * append, and after writers on four threads at once, the end of the
 * furthest. For descriptor (SEQUENTIAL_BULK) and stdio streams. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vfs/vfs_implementation.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <rthreads/rthreads.h>

static unsigned checks, failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define DIR_NAME "vfs_positional_size_test_tmp"
#define FILE_P   DIR_NAME "/p.bin"

#define THREADS  4
#define RECORDS  500
#define RECORD   64

static uint8_t data[4096];

struct writer
{
   libretro_vfs_implementation_file *s;
   unsigned which;
   bool ok;
};

/* Records interleaved by thread: thread t writes records t, t + 4, ... */
static void writer_run(void *arg)
{
   struct writer *w = (struct writer*)arg;
   unsigned i;
   w->ok = true;
   for (i = w->which; i < THREADS * RECORDS; i += THREADS)
      if (retro_vfs_file_write_at_impl(w->s, data, RECORD,
               (int64_t)i * RECORD) != RECORD)
         w->ok = false;
}

static void run(const char *what, unsigned hints)
{
   libretro_vfs_implementation_file *s;
   struct writer w[THREADS];
   sthread_t *t[THREADS];
   unsigned i;
   printf("-- %s\n", what);

   retro_vfs_file_remove_impl(FILE_P);
   s = retro_vfs_file_open_impl(FILE_P, RETRO_VFS_FILE_ACCESS_READ_WRITE,
         hints);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(retro_vfs_file_write_impl(s, data, 10) == 10);
   CHECK(retro_vfs_file_size_impl(s) == 10);
   /* Past the end: grown to its end */
   if (retro_vfs_file_write_at_impl(s, data, 20, 100) < 0)
   {
      printf("note: no positional I/O here, not checked\n");
      retro_vfs_file_close_impl(s);
      return;
   }
   CHECK(retro_vfs_file_size_impl(s) == 120);
   /* Inside: unchanged */
   CHECK(retro_vfs_file_write_at_impl(s, data, 30, 40) == 30);
   CHECK(retro_vfs_file_size_impl(s) == 120);
   /* The position is where the ordinary write left it */
   CHECK(retro_vfs_file_tell_impl(s) == 10);
   /* Truncated under them, then written past the new end */
   CHECK(retro_vfs_file_truncate_impl(s, 50) == 0);
   CHECK(retro_vfs_file_size_impl(s) == 50);
   CHECK(retro_vfs_file_write_at_impl(s, data, 5, 60) == 5);
   CHECK(retro_vfs_file_size_impl(s) == 65);

   /* Four threads at once */
   for (i = 0; i < THREADS; i++)
   {
      w[i].s     = s;
      w[i].which = i;
      w[i].ok    = false;
      t[i]       = sthread_create(writer_run, &w[i]);
      CHECK(t[i] != NULL);
   }
   for (i = 0; i < THREADS; i++)
      if (t[i])
      {
         sthread_join(t[i]);
         CHECK(w[i].ok);
      }
   CHECK(retro_vfs_file_size_impl(s) == (int64_t)THREADS * RECORDS * RECORD);
   CHECK(retro_vfs_file_close_impl(s) == 0);
   CHECK(path_get_size(FILE_P) == (int64_t)THREADS * RECORDS * RECORD);

   /* Appends, on a stream written only by them */
   retro_vfs_file_remove_impl(FILE_P);
   s = retro_vfs_file_open_impl(FILE_P, RETRO_VFS_FILE_ACCESS_READ_WRITE,
         hints);
   CHECK(s != NULL);
   if (!s)
      return;
   for (i = 1; i <= 3; i++)
   {
      CHECK(retro_vfs_file_write_at_impl(s, data, 16,
               RETRO_VFS_WRITE_AT_APPEND) == 16);
      CHECK(retro_vfs_file_size_impl(s) == (int64_t)(16 * i));
   }
   CHECK(retro_vfs_file_close_impl(s) == 0);
   CHECK(path_get_size(FILE_P) == 48);
}

/* The same through filestream, with eof */
static void run_filestream(unsigned hints)
{
   RFILE *f;
   printf("-- filestream, hints %u\n", hints);
   retro_vfs_file_remove_impl(FILE_P);
   f = filestream_open(FILE_P, RETRO_VFS_FILE_ACCESS_READ_WRITE, hints);
   CHECK(f != NULL);
   if (!f)
      return;
   CHECK(filestream_write(f, data, 10) == 10);
   if (filestream_write_at(f, data, 20, 100) < 0)
   {
      printf("note: no positional I/O here, not checked\n");
      filestream_close(f);
      return;
   }
   CHECK(filestream_get_size(f) == 120);
   /* At 10 of 120: not the end */
   CHECK(filestream_tell(f) == 10);
   CHECK(filestream_eof(f) == 0);
   CHECK(filestream_seek(f, 0, RETRO_VFS_SEEK_POSITION_END) == 0);
   CHECK(filestream_tell(f) == 120);
   CHECK(filestream_eof(f) == EOF);
   CHECK(filestream_close(f) == 0);
   CHECK(path_get_size(FILE_P) == 120);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(data); i++)
      data[i] = (uint8_t)(i * 29 + 3);
   path_mkdir(DIR_NAME);
   run("descriptor", RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   run("stdio", RETRO_VFS_FILE_ACCESS_HINT_NONE);
   run_filestream(RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK);
   run_filestream(RETRO_VFS_FILE_ACCESS_HINT_NONE);
   retro_vfs_file_remove_impl(FILE_P);
   path_rmdir(DIR_NAME);
   printf("vfs_positional_size_test: %u checks, %u failures\n",
         checks, failures);
   return failures ? 1 : 0;
}
