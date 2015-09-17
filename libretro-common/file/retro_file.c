#include <retro_file.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

enum
{
   MODE_READ = 0,
   MODE_WRITE,
   MODE_READ_WRITE,
};

struct RFILE *retro_fopen(const char *path, const char *mode, size_t len)
{
   int flags = 0;
   struct RFILE *stream = (struct RFILE*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   if (!strcmp(mode, "w+"))
      flags = MODE_READ_WRITE;

   switch (flags)
   {
      case MODE_READ:
         break;
      case MODE_WRITE:
         break;
      case MODE_READ_WRITE:
#ifdef _WIN32
         stream->fd = open(path, O_RDWR | O_BINARY);
#else
         stream->fd = open(path, O_RDWR);
#endif
         break;
   }

   return stream;
}

ssize_t retro_seek(struct RFILE *stream, ssize_t offset, int whence)
{
   if (!stream)
      return -1;

   return lseek(stream->fd, offset, whence);
}

ssize_t retro_read(struct RFILE *stream, void *s, size_t len)
{
   if (!stream)
      return -1;
   return read(stream->fd, s, len);
}

ssize_t retro_write(struct RFILE *stream, const void *s, size_t len)
{
   if (!stream)
      return -1;
   return write(stream->fd, s, len);
}

void retro_fclose(struct RFILE *stream)
{
   if (!stream)
      return;

   free(stream);
}
