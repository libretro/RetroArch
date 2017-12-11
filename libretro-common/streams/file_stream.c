/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_stream.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(_WIN32)
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
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  if !defined(VITA)
#  include <dirent.h>
#  endif
#  include <unistd.h>
#endif

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
#define O_RDONLY CELL_FS_O_RDONLY
#define O_WRONLY CELL_FS_O_WRONLY
#define O_CREAT CELL_FS_O_CREAT
#define O_TRUNC CELL_FS_O_TRUNC
#define O_RDWR CELL_FS_O_RDWR
#else
#include <fcntl.h>
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#define RFILE_HINT_UNBUFFERED (1 << 8)

#include <libretro.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <memmap.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

struct RFILE
{
   unsigned hints;
   char *ext;
   int64_t size;
   FILE *fp;

#if !defined(_WIN32) || defined(LEGACY_WIN32)
#define MODE_STR_READ "r"
#define MODE_STR_READ_UNBUF "rb"
#define MODE_STR_WRITE_UNBUF "wb"
#define MODE_STR_WRITE_PLUS "w+"
#else
#define MODE_STR_READ L"r"
#define MODE_STR_READ_UNBUF L"rb"
#define MODE_STR_WRITE_UNBUF L"wb"
#define MODE_STR_WRITE_PLUS L"w+"
#endif

#if defined(HAVE_MMAP)
   uint8_t *mapped;
   uint64_t mappos;
   uint64_t mapsize;
#endif
   int fd;
   char *buf;
};

const char *filestream_get_ext(RFILE *stream)
{
   if (!stream)
      return NULL;
   return stream->ext;
}

int64_t filestream_get_size(RFILE *stream)
{
   if (!stream)
      return 0;
   return stream->size;
}

void filestream_set_size(RFILE *stream)
{
   if (!stream)
      return;

   filestream_seek(stream, 0, SEEK_SET);
   filestream_seek(stream, 0, SEEK_END);

   stream->size = filestream_tell(stream);

   filestream_seek(stream, 0, SEEK_SET);
}

/**
 * filestream_open:
 * @path               : path to file
 * @mode               : file mode to use when opening (read/write)
 * @bufsize            : optional buffer size (-1 or 0 to use default)
 *
 * Opens a file for reading or writing, depending on the requested mode.
 * Returns a pointer to an RFILE if opened successfully, otherwise NULL.
 **/
RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
   int            flags    = 0;
#if !defined(_WIN32) || defined(LEGACY_WIN32)
   const char *mode_str    = NULL;
#else
   const wchar_t *mode_str = NULL;
#endif
   RFILE        *stream    = (RFILE*)calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   (void)flags;

   stream->hints           = hints;

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP && mode == RETRO_VFS_FILE_ACCESS_READ)
      stream->hints |= RFILE_HINT_UNBUFFERED;
   else
#endif
      stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP;

   switch (mode)
   {
      case RETRO_VFS_FILE_ACCESS_READ:
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_READ_UNBUF;
         /* No "else" here */
         flags    = O_RDONLY;
         break;
      case RETRO_VFS_FILE_ACCESS_WRITE:
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_WRITE_UNBUF;
         else
         {
            flags    = O_WRONLY | O_CREAT | O_TRUNC;
#ifndef _WIN32
            flags   |=  S_IRUSR | S_IWUSR;
#endif
         }
         break;
      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
            mode_str = MODE_STR_WRITE_PLUS;
         else
         {
            flags    = O_RDWR;
#ifdef _WIN32
            flags   |= O_BINARY;
#endif
         }
         break;
         /* TODO/FIXME - implement */
      case RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
         break;
   }

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0 && mode_str)
   {
#if defined(_WIN32) && !defined(_XBOX)
#if defined(LEGACY_WIN32)
      char *path_local    = utf8_to_local_string_alloc(path);
      stream->fp          = fopen(path_local, mode_str);
      if (path_local)
         free(path_local);
#else
      wchar_t * path_wide = utf8_to_utf16_string_alloc(path);
      stream->fp          = _wfopen(path_wide, mode_str);
      if (path_wide)
         free(path_wide);
#endif
#else
      stream->fp = fopen(path, mode_str);
#endif

      if (!stream->fp)
         goto error;

      /* Regarding setvbuf:
       *
       * https://www.freebsd.org/cgi/man.cgi?query=setvbuf&apropos=0&sektion=0&manpath=FreeBSD+11.1-RELEASE&arch=default&format=html
       *
       * If the size argument is not zero but buf is NULL, a buffer of the given size will be allocated immediately, and
       * released on close. This is an extension to ANSI C.
       *
       * Since C89 does not support specifying a null buffer with a non-zero size, we create and track our own buffer for it.
       */
      /* TODO: this is only useful for a few platforms, find which and add ifdef */
      stream->buf = (char*)calloc(1, 0x4000);
      setvbuf(stream->fp, stream->buf, _IOFBF, 0x4000);
   }
   else
   {
#if defined(_WIN32) && !defined(_XBOX)
#if defined(LEGACY_WIN32)
      char *path_local    = utf8_to_local_string_alloc(path);
      stream->fd          = open(path_local, flags, 0);
      if (path_local)
         free(path_local);
#else
      wchar_t * path_wide = utf8_to_utf16_string_alloc(path);
      stream->fd          = _wopen(path_wide, flags, 0);
      if (path_wide)
         free(path_wide);
#endif
#else
      stream->fd = open(path, flags, 0);
#endif

      if (stream->fd == -1)
         goto error;
#ifdef HAVE_MMAP
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
      {
         stream->mappos  = 0;
         stream->mapped  = NULL;
         stream->mapsize = filestream_seek(stream, 0, SEEK_END);

         if (stream->mapsize == (uint64_t)-1)
            goto error;

         filestream_rewind(stream);

         stream->mapped = (uint8_t*)mmap((void*)0,
               stream->mapsize, PROT_READ,  MAP_SHARED, stream->fd, 0);

         if (stream->mapped == MAP_FAILED)
            stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP;
      }
#endif
   }

   {
      const char *ld = (const char*)strrchr(path, '.');
      if (ld)
         stream->ext = strdup(ld + 1);
   }

   filestream_set_size(stream);

   return stream;

error:
   filestream_close(stream);
   return NULL;
}

char *filestream_gets(RFILE *stream, char *s, size_t len)
{
   if (!stream)
      return NULL;
   return fgets(s, (int)len, stream->fp);
}

int filestream_getc(RFILE *stream)
{
   char c = 0;
   (void)c;
   if (!stream)
      return 0;
   return fgetc(stream->fp);
}

ssize_t filestream_seek(RFILE *stream, ssize_t offset, int whence)
{
   if (!stream)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fseek(stream->fp, (long)offset, whence);

#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function is
    * called in filestream_open() */
   if (stream->mapped && stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
   {
      /* fseek() returns error on under/overflow but allows cursor > EOF for
         read-only file descriptors. */
      switch (whence)
      {
         case SEEK_SET:
            if (offset < 0)
               goto error;

            stream->mappos = offset;
            break;

         case SEEK_CUR:
            if ((offset < 0 && stream->mappos + offset > stream->mappos) ||
                  (offset > 0 && stream->mappos + offset < stream->mappos))
               goto error;

            stream->mappos += offset;
            break;

         case SEEK_END:
            if (stream->mapsize + offset < stream->mapsize)
               goto error;

            stream->mappos = stream->mapsize + offset;
            break;
      }
      return stream->mappos;
   }
#endif

   if (lseek(stream->fd, offset, whence) < 0)
      goto error;

   return 0;

error:
   return -1;
}

int filestream_eof(RFILE *stream)
{
   return feof(stream->fp);

   /* TODO: FIXME: I can't figure out why this breaks on Windows.
      The while loop in config_file_new_internal just never exits.
      The current position seems to jump backwards a few lines,
      but it doesn't start until somewhere in the middle of the file.
    */
   /*
   size_t current_position = filestream_tell(stream);
   size_t end_position;

   filestream_seek(stream, 0, SEEK_END);
   end_position = filestream_tell(stream);

   filestream_seek(stream, current_position, SEEK_SET);

   if (current_position >= end_position)
      return 1;
   return 0;
   */
}

ssize_t filestream_tell(RFILE *stream)
{
   if (!stream)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return ftell(stream->fp);

#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function
    * is called in filestream_open() */
   if (stream->mapped && stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
      return stream->mappos;
#endif
   if (lseek(stream->fd, 0, SEEK_CUR) < 0)
      goto error;

   return 0;

error:
   return -1;
}

void filestream_rewind(RFILE *stream)
{
   filestream_seek(stream, 0L, SEEK_SET);
}

ssize_t filestream_read(RFILE *stream, void *s, size_t len)
{
   if (!stream || !s)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fread(s, 1, len, stream->fp);

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
   {
      if (stream->mappos > stream->mapsize)
         goto error;

      if (stream->mappos + len > stream->mapsize)
         len = stream->mapsize - stream->mappos;

      memcpy(s, &stream->mapped[stream->mappos], len);
      stream->mappos += len;

      return len;
   }
#endif

   return read(stream->fd, s, len);

error:
   return -1;
}

int filestream_flush(RFILE *stream)
{
   return fflush(stream->fp);
}

ssize_t filestream_write(RFILE *stream, const void *s, size_t len)
{
   if (!stream)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fwrite(s, 1, len, stream->fp);

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
      goto error;
#endif
   return write(stream->fd, s, len);

error:
   return -1;
}

int filestream_putc(RFILE *stream, int c)
{
   if (!stream)
      return EOF;

   return fputc(c, stream->fp);
}

int filestream_vprintf(RFILE *stream, const char* format, va_list args)
{
	static char buffer[8 * 1024];
	int num_chars = vsprintf(buffer, format, args);

	if (num_chars < 0)
		return -1;
	else if (num_chars == 0)
		return 0;

	return filestream_write(stream, buffer, num_chars);
}

int filestream_printf(RFILE *stream, const char* format, ...)
{
	va_list vl;
   int result;
	va_start(vl, format);
	result = filestream_vprintf(stream, format, vl);
	va_end(vl);
	return result;
}

int filestream_error(RFILE *stream)
{
	return ferror(stream->fp);
}

int filestream_close(RFILE *stream)
{
   if (!stream)
      goto error;

   if (!string_is_empty(stream->ext))
      free(stream->ext);

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      if (stream->fp)
         fclose(stream->fp);
   }
   else
   {
#ifdef HAVE_MMAP
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_MEMORY_MAP)
         munmap(stream->mapped, stream->mapsize);
#endif
   }

   if (stream->fd > 0)
      close(stream->fd);
   if (stream->buf)
      free(stream->buf);
   free(stream);

   return 0;

error:
   return -1;
}

/**
 * filestream_read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf.
 *
 * Returns: number of items read, -1 on error.
 */
int filestream_read_file(const char *path, void **buf, ssize_t *len)
{
   ssize_t ret              = 0;
   ssize_t content_buf_size = 0;
   void *content_buf        = NULL;
   RFILE *file              = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
      goto error;
   }

   if (filestream_seek(file, 0, SEEK_END) != 0)
      goto error;

   content_buf_size = filestream_tell(file);
   if (content_buf_size < 0)
      goto error;

   filestream_rewind(file);

   content_buf = malloc(content_buf_size + 1);

   if (!content_buf)
      goto error;

   ret = filestream_read(file, content_buf, content_buf_size);
   if (ret < 0)
   {
      fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
      goto error;
   }

   filestream_close(file);

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[ret] = '\0';

   if (len)
      *len = ret;

   return 1;

error:
   if (file)
      filestream_close(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

/**
 * filestream_write_file:
 * @path             : path to file.
 * @data             : contents to write to the file.
 * @size             : size of the contents.
 *
 * Writes data to a file.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
bool filestream_write_file(const char *path, const void *data, ssize_t size)
{
   ssize_t ret   = 0;
   RFILE *file   = filestream_open(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!file)
      return false;

   ret = filestream_write(file, data, size);
   filestream_close(file);

   if (ret != size)
      return false;

   return true;
}
