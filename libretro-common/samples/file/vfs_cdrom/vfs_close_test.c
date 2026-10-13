/* Real native VFS/filestream close, with final OS errors injected after
 * releasing the underlying resource. Sanitizers check wrapper ownership. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <streams/file_stream.h>
#include <vfs/vfs_implementation.h>

static unsigned failures, stdio_closes, descriptor_closes, callback_closes;
static int fail_stdio, fail_descriptor;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static int injected_fclose(FILE *file)
{
   int result = fclose(file);
   stdio_closes++;
   return fail_stdio ? EOF : result;
}
static int injected_close(int fd)
{
   int result = close(fd);
   descriptor_closes++;
   return fail_descriptor ? -1 : result;
}
#define fclose injected_fclose
#define close injected_close
#include "../../../vfs/vfs_implementation.c"
#undef fclose
#undef close

static int RETRO_CALLCONV callback_close(struct retro_vfs_file_handle *file)
{
   callback_closes++;
   CHECK(retro_vfs_file_close_impl((libretro_vfs_implementation_file*)file) == 0);
   return -1; /* VFS contract consumes the handle on failure too. */
}

int main(void)
{
   unsigned before;
   RFILE *file;
   libretro_vfs_implementation_file *native;
   struct retro_vfs_interface iface = {0};
   struct retro_vfs_interface_info info;
   void *contents = NULL;
   int64_t length;
   file = filestream_open("close_result.bin", RETRO_VFS_FILE_ACCESS_WRITE, 0);
   CHECK(file != NULL);
   if (file)
   {
      CHECK(filestream_write(file, "data", 4) == 4);
      before = stdio_closes;
      fail_stdio = 1;
      CHECK(filestream_close(file) == -1);
      CHECK(stdio_closes == before + 1);
      fail_stdio = 0;
   }
   native = retro_vfs_file_open_impl("close_result.bin", RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(native != NULL);
   if (native)
   {
      char buf[4];
      CHECK(retro_vfs_file_read_impl(native, buf, 4) == 4);
      CHECK(buf[0] == 'd' && buf[3] == 'a');
      CHECK(retro_vfs_file_close_impl(native) == 0);
   }
   native = (libretro_vfs_implementation_file*)calloc(1, sizeof(*native));
   CHECK(native != NULL);
   if (native)
   {
      native->fd = open("close_result.bin", O_RDONLY);
      native->hints = RFILE_HINT_UNBUFFERED;
      CHECK(native->fd >= 0);
      before = descriptor_closes;
      fail_descriptor = 1;
      CHECK(retro_vfs_file_close_impl(native) == -1);
      CHECK(descriptor_closes == before + 1);
      fail_descriptor = 0;
   }
   iface.close = callback_close;
   info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   info.iface = &iface;
   filestream_vfs_init(&info);
   file = filestream_open("close_result.bin", RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(file != NULL);
   if (file)
      CHECK(filestream_close(file) == -1);
   CHECK(callback_closes == 1);
   info.iface = NULL;
   filestream_vfs_init(&info);
   fail_stdio = 1;
   fail_descriptor = 1;
   before = stdio_closes + descriptor_closes;
   CHECK(filestream_exists("close_result.bin"));
   CHECK(stdio_closes + descriptor_closes == before + 1);
   before = stdio_closes + descriptor_closes;
   CHECK(filestream_read_file("close_result.bin", &contents, &length) == 0);
   CHECK(contents == NULL && length == -1 && stdio_closes + descriptor_closes == before + 1);
   CHECK(!filestream_matches_buf("close_result.bin", "data", 4));
   CHECK(!filestream_write_file("close_empty.bin", "", 0));
   before = stdio_closes + descriptor_closes;
   CHECK(filestream_read_file("close_empty.bin", &contents, &length) == 0);
   CHECK(contents == NULL && length == -1 && stdio_closes + descriptor_closes == before + 1);
   CHECK(!filestream_write_file("close_write.bin", "new", 3));
   CHECK(!filestream_write_file_atomic("close_result.bin", "replacement", 11));
   fail_stdio = 0;
   fail_descriptor = 0;
   CHECK(!filestream_exists("close_result.bin.tmp"));
   CHECK(filestream_matches_buf("close_result.bin", "data", 4));
   CHECK(filestream_read_file("close_empty.bin", &contents, &length) == 1);
   CHECK(contents && length == 0);
   free(contents); contents = NULL;
   CHECK(filestream_read_file("close_result.bin", &contents, &length) == 1);
   CHECK(contents && length == 4 && memcmp(contents, "data", 4) == 0);
   free(contents);
   CHECK(filestream_write_file_atomic("close_result.bin", "replacement", 11));
   CHECK(filestream_matches_buf("close_result.bin", "replacement", 11));
   remove("close_empty.bin");
   remove("close_write.bin");
   remove("close_result.bin");
   printf("VFS close tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
