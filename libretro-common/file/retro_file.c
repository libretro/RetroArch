#include <retro_file.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
#  include <compat/posix_string.h>
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else
#    include <io.h>
#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#elif defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#else
#include <fcntl.h>
#endif

struct RFILE
{
   int fd;
};

RFILE *retro_fopen(const char *path, unsigned mode, ssize_t len)
{
   RFILE *stream = (RFILE*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   switch (mode)
   {
      case RFILE_MODE_READ:
         stream->fd = open(path, O_RDONLY);
         break;
      case RFILE_MODE_WRITE:
         stream->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
         break;
      case RFILE_MODE_READ_WRITE:
#ifdef _WIN32
         stream->fd = open(path, O_RDWR | O_BINARY);
#else
         stream->fd = open(path, O_RDWR);
#endif
         break;
   }

   return stream;
}

ssize_t retro_fseek(RFILE *stream, ssize_t offset, int whence)
{
   if (!stream)
      return -1;

   return lseek(stream->fd, offset, whence);
}

ssize_t retro_fread(RFILE *stream, void *s, size_t len)
{
   if (!stream)
      return -1;
   return read(stream->fd, s, len);
}

ssize_t retro_fwrite(RFILE *stream, const void *s, size_t len)
{
   if (!stream)
      return -1;
   return write(stream->fd, s, len);
}

void retro_fclose(RFILE *stream)
{
   if (!stream)
      return;

   close(stream->fd);
   free(stream);
}

bool retro_fmemcpy(const char *path, char *s, size_t len, ssize_t *bytes_written)
{
   RFILE *stream = retro_fopen(path, RFILE_MODE_READ, -1);
   if (!stream)
      return false;

   do
   {
      *bytes_written = retro_fread(stream, s, len);
   }while(*bytes_written < 0 && errno == EINTR);

   retro_fclose(stream);
   if (*bytes_written < 0)
      return false;

   /* NULL-terminate the string. */
   s[*bytes_written] = '\0';

   return true;
}
