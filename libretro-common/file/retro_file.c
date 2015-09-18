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

#if 0
#define HAVE_BUFFERED_IO 1
#endif

struct RFILE
{
#if defined(PSP) || defined(VITA)
   SceUID fd;
#elif defined(HAVE_BUFFERED_IO)
   FILE *fd;
#else
   int fd;
#endif
};

RFILE *retro_fopen(const char *path, unsigned mode, ssize_t len)
{
   RFILE *stream = (RFILE*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   switch (mode)
   {
      case RFILE_MODE_READ:
#if defined(VITA) || defined(PSP)
         stream->fd = sceIoOpen(path, O_RDONLY, 0777);
#elif defined(HAVE_BUFFERED_IO)
         stream->fd = fopen(path, "rb");
#else
         stream->fd = open(path, O_RDONLY);
#endif
         break;
      case RFILE_MODE_WRITE:
#if defined(VITA) || defined(PSP)
         stream->fd = sceIoOpen(path, O_WRONLY | O_CREAT, 0777);
#elif defined(HAVE_BUFFERED_IO)
         stream->fd = fopen(path, "wb");
#else
         stream->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
#endif
         break;
      case RFILE_MODE_READ_WRITE:
#if defined(VITA) || defined(PSP)
         stream->fd = sceIoOpen(path, O_RDWR, 0777);
#elif defined(HAVE_BUFFERED_IO)
         stream->fd = fopen(path, "w+");
#else
#ifdef _WIN32
         stream->fd = open(path, O_RDWR | O_BINARY);
#else
         stream->fd = open(path, O_RDWR);
#endif
#endif
         break;
   }

#if defined(HAVE_BUFFERED_IO)
   if (!stream->fd)
#else
   if (stream->fd == -1)
#endif
      goto error;

   return stream;

error:
   retro_fclose(stream);
   return NULL;
}

ssize_t retro_fseek(RFILE *stream, ssize_t offset, int whence)
{
   if (!stream)
      return -1;

#if defined(VITA) || defined(PSP)
   return sceIoLseek(stream->fd, (SceOff)offset, whence);
#elif defined(HAVE_BUFFERED_IO)
   return fseek(stream->fd, (long)offset, whence);
#else
   return lseek(stream->fd, offset, whence);
#endif
}

ssize_t retro_fread(RFILE *stream, void *s, size_t len)
{
   if (!stream)
      return -1;
#if defined(VITA) || defined(PSP)
   return sceIoRead(stream->fd, s, len);
#elif defined(HAVE_BUFFERED_IO)
   return fread(s, len, 1, stream->fd);
#else
   return read(stream->fd, s, len);
#endif
}

ssize_t retro_fwrite(RFILE *stream, const void *s, size_t len)
{
   if (!stream)
      return -1;
#if defined(VITA) || defined(PSP)
   return sceIoWrite(stream->fd, s, len);
#elif defined(HAVE_BUFFERED_IO)
   return fwrite(s, len, 1, stream->fd);
#else
   return write(stream->fd, s, len);
#endif
}

void retro_fclose(RFILE *stream)
{
   if (!stream)
      return;

#if defined(VITA) || defined(PSP)
   if (stream->fd > 0)
      sceIoClose(stream->fd);
#elif defined(HAVE_BUFFERED_IO)
   if (stream->fd)
      fclose(stream->fd);
#else
   if (stream->fd > 0)
      close(stream->fd);
#endif
   free(stream);
}

static bool retro_fread_iterate(RFILE *stream, char *s, size_t len, ssize_t *bytes_written)
{
   *bytes_written = retro_fread(stream, s, len);
#if defined(HAVE_BUFFERED_IO)
   return (*bytes_written < 0);
#else
   return (*bytes_written < 0 && errno == EINTR);
#endif
}

bool retro_fmemcpy(const char *path, char *s, size_t len, ssize_t *bytes_written)
{
   RFILE *stream = retro_fopen(path, RFILE_MODE_READ, -1);
   if (!stream)
      return false;

   while(retro_fread_iterate(stream, s, len, bytes_written));

   retro_fclose(stream);
   if (*bytes_written < 0)
      return false;

   /* NULL-terminate the string. */
   s[*bytes_written] = '\0';

   return true;
}
