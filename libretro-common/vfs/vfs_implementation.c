/* Copyright  (C) 2010-2018 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation.c).
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
#include <sys/types.h>

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

#if defined(_WIN32) && !defined(_XBOX)
#if !defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER >= 1400)
#define ATLEAST_VC2005
#endif
#endif

#ifdef RARCH_INTERNAL
#ifndef VFS_FRONTEND
#define VFS_FRONTEND
#endif
#endif

#include <vfs/vfs_implementation.h>
#include <libretro.h>
#include <memmap.h>
#include <encodings/utf.h>
#include <compat/fopen_utf8.h>

#define RFILE_HINT_UNBUFFERED (1 << 8)

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
   int fd;
   unsigned hints;
   int64_t size;
   char *buf;
   FILE *fp;
   char* orig_path;
#if defined(HAVE_MMAP)
   uint64_t mappos;
   uint64_t mapsize;
   uint8_t *mapped;
#endif
};

int64_t retro_vfs_file_seek_internal(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
{
   if (!stream)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
/* VC2005 and up have a special 64-bit fseek */
#ifdef ATLEAST_VC2005
      return _fseeki64(stream->fp, offset, whence);
#elif defined(__CELLOS_LV2__) || defined(_MSC_VER) && _MSC_VER <= 1310
      return fseek(stream->fp, (long)offset, whence);
#else
      return fseeko(stream->fp, (off_t)offset, whence);
#endif

#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function is
    * called in filestream_open() */
   if (stream->mapped && stream->hints & 
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
   {
      /* fseek() returns error on under/overflow but 
       * allows cursor > EOF for
       read-only file descriptors. */
      switch (whence)
      {
         case SEEK_SET:
            if (offset < 0)
               goto error;

            stream->mappos = offset;
            break;

         case SEEK_CUR:
            if ((offset   < 0 && stream->mappos + offset > stream->mappos) ||
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

/**
 * retro_vfs_file_open_impl:
 * @path               : path to file
 * @mode               : file mode to use when opening (read/write)
 * @hints              :
 *
 * Opens a file for reading or writing, depending on the requested mode.
 * Returns a pointer to an RFILE if opened successfully, otherwise NULL.
 **/

libretro_vfs_implementation_file *retro_vfs_file_open_impl(const char *path, unsigned mode, unsigned hints)
{
   int                                flags = 0;
   const char                     *mode_str = NULL;
   libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));

#ifdef VFS_FRONTEND
   const char                 *dumb_prefix  = "vfsonly://";

   if (!memcmp(path, dumb_prefix, strlen(dumb_prefix)))
      path += strlen(dumb_prefix);
#endif

   if (!stream)
      return NULL;

   (void)flags;

   stream->hints           = hints;
   stream->orig_path       = strdup(path);

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS && mode == RETRO_VFS_FILE_ACCESS_READ)
      stream->hints |= RFILE_HINT_UNBUFFERED;
   else
#endif
      stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;

   switch (mode)
   {
      case RETRO_VFS_FILE_ACCESS_READ:
         mode_str = "rb";

         flags    = O_RDONLY;
#ifdef _WIN32
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_WRITE:
         mode_str = "wb";

         flags    = O_WRONLY | O_CREAT | O_TRUNC;
#ifndef _WIN32
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         mode_str = "w+b";

         flags    = O_RDWR | O_CREAT | O_TRUNC;
#ifndef _WIN32
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
      case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
         mode_str = "r+b";

         flags    = O_RDWR;
#ifndef _WIN32
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
         break;
         
      default:
         goto error;
   }

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      FILE   *fp = (FILE*)fopen_utf8(path, mode_str);

      if (!fp)
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
      stream->fp  = fp;
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
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      {
         stream->mappos  = 0;
         stream->mapped  = NULL;
         stream->mapsize = retro_vfs_file_seek_internal(stream, 0, SEEK_END);

         if (stream->mapsize == (uint64_t)-1)
            goto error;

         retro_vfs_file_seek_internal(stream, 0, SEEK_SET);

         stream->mapped = (uint8_t*)mmap((void*)0,
               stream->mapsize, PROT_READ,  MAP_SHARED, stream->fd, 0);

         if (stream->mapped == MAP_FAILED)
            stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
      }
#endif
   }

   retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
   retro_vfs_file_seek_internal(stream, 0, SEEK_END);

   stream->size = retro_vfs_file_tell_impl(stream);

   retro_vfs_file_seek_internal(stream, 0, SEEK_SET);

   return stream;

error:
   retro_vfs_file_close_impl(stream);
   return NULL;
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      if (stream->fp)
         fclose(stream->fp);
   }
   else
   {
#ifdef HAVE_MMAP
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
         munmap(stream->mapped, stream->mapsize);
#endif
   }

   if (stream->fd > 0)
      close(stream->fd);
   if (stream->buf)
      free(stream->buf);
   if (stream->orig_path)
      free(stream->orig_path);
   free(stream);

   return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
   return ferror(stream->fp);
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return 0;
   return stream->size;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
/* VC2005 and up have a special 64-bit ftell */
#ifdef ATLEAST_VC2005
      return _ftelli64(stream->fp);
#else
      return ftell(stream->fp);
#endif

#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function
    * is called in filestream_open() */
   if (stream->mapped && stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      return stream->mappos;
#endif
   if (lseek(stream->fd, 0, SEEK_CUR) < 0)
      return -1;

   return 0;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream, int64_t offset, int seek_position)
{
   int whence = -1;
   switch (seek_position)
   {
      case RETRO_VFS_SEEK_POSITION_START:
         whence = SEEK_SET;
         break;
      case RETRO_VFS_SEEK_POSITION_CURRENT:
         whence = SEEK_CUR;
         break;
      case RETRO_VFS_SEEK_POSITION_END:
         whence = SEEK_END;
         break;
   }

   return retro_vfs_file_seek_internal(stream, offset, whence);
}

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
   if (!stream || !s)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fread(s, 1, (size_t)len, stream->fp);

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
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

   return read(stream->fd, s, (size_t)len);

error:
   return -1;
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
   if (!stream)
      goto error;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
      return fwrite(s, 1, (size_t)len, stream->fp);

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      goto error;
#endif
   return write(stream->fd, s, (size_t)len);

error:
   return -1;
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;
   return fflush(stream->fp)==0 ? 0 : -1;
}

int retro_vfs_file_remove_impl(const char *path)
{
   char *path_local    = NULL;
   wchar_t *path_wide  = NULL;

   if (!path || !*path)
      return -1;

   (void)path_local;
   (void)path_wide;

#if defined(_WIN32) && !defined(_XBOX)
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
   path_local = utf8_to_local_string_alloc(path);

   if (path_local)
   {
      int ret = remove(path_local);
      free(path_local);

      if (ret == 0)
         return 0;
   }
#else
   path_wide = utf8_to_utf16_string_alloc(path);

   if (path_wide)
   {
      int ret = _wremove(path_wide);
      free(path_wide);

      if (ret == 0)
         return 0;
   }
#endif
#else
   if (remove(path) == 0)
      return 0;
#endif
   return -1;
}

int retro_vfs_file_rename_impl(const char *old_path, const char *new_path)
{
   char *old_path_local    = NULL;
   char *new_path_local    = NULL;
   wchar_t *old_path_wide  = NULL;
   wchar_t *new_path_wide  = NULL;

   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;

   (void)old_path_local;
   (void)new_path_local;
   (void)old_path_wide;
   (void)new_path_wide;

#if defined(_WIN32) && !defined(_XBOX)
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
   old_path_local = utf8_to_local_string_alloc(old_path);
   new_path_local = utf8_to_local_string_alloc(new_path);

   if (old_path_local)
   {
      if (new_path_local)
      {
         int ret = rename(old_path_local, new_path_local);
         free(old_path_local);
         free(new_path_local);
         return ret==0 ? 0 : -1;
      }

      free(old_path_local);
   }

   if (new_path_local)
      free(new_path_local);
#else
   old_path_wide = utf8_to_utf16_string_alloc(old_path);
   new_path_wide = utf8_to_utf16_string_alloc(new_path);

   if (old_path_wide)
   {
      if (new_path_wide)
      {
         int ret = _wrename(old_path_wide, new_path_wide);
         free(old_path_wide);
         free(new_path_wide);
         return ret==0 ? 0 : -1;
      }

      free(old_path_wide);
   }

   if (new_path_wide)
      free(new_path_wide);
#endif
   return -1;
#else
   return rename(old_path, new_path)==0 ? 0 : -1;
#endif
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
   /* should never happen, do something noisy so caller can be fixed */
   if (!stream)
      abort();
   return stream->orig_path;
}
