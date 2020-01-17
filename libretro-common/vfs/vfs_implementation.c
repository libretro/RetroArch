/* Copyright  (C) 2010-2019 The RetroArch team
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

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(_WIN32)
#  ifdef _MSC_VER
#    define setmode _setmode
#  endif
#include <sys/stat.h>
#  ifdef _XBOX
#    include <xtl.h>
#    define INVALID_FILE_ATTRIBUTES -1
#  else

#    include <fcntl.h>
#    include <direct.h>
#    include <windows.h>
#  endif
#    include <io.h>
#else
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  if defined(PS2)
#    include <fileXio_rpc.h>
#    include <fileXio_cdvd.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  if !defined(VITA)
#  include <dirent.h>
#  endif
#  include <unistd.h>
#  if defined(ORBIS)
#  include <sys/fcntl.h>
#  include <sys/dirent.h>
#  include <orbisFile.h>
#  endif
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

/* TODO: Some things are duplicated but I'm really afraid of breaking other platforms by touching this */
#if defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#  include <psp2/io/stat.h>
#elif defined(ORBIS)
#  include <orbisFile.h>
#  include <ps4link.h>
#  include <sys/dirent.h>
#  include <sys/fcntl.h>
#elif !defined(_WIN32)
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  if defined(PS2)
#    include <fileXio_rpc.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <unistd.h>
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP) || defined(PS2)
#include <unistd.h> /* stat() is defined here */
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#ifdef __HAIKU__
#include <kernel/image.h>
#endif
#ifndef __MACH__
#include <compat/strl.h>
#include <compat/posix_string.h>
#endif
#include <compat/strcasestr.h>
#include <retro_miscellaneous.h>
#include <encodings/utf.h>

#if defined(_WIN32)
#ifndef _XBOX
#if defined(_MSC_VER) && _MSC_VER <= 1200
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#endif
#elif defined(VITA)
#define SCE_ERROR_ERRNO_EEXIST 0x80010011
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(ORBIS)
#include <orbisFile.h>
#include <sys/fcntl.h>
#include <sys/dirent.h>
#endif
#if defined(PSP)
#include <pspkernel.h>
#endif

#if defined(PS2)
#include <fileXio_rpc.h>
#include <fileXio.h>
#endif

#if defined(__CELLOS_LV2__)
#include <cell/cell_fs.h>
#endif

#if defined(VITA)
#define FIO_S_ISDIR SCE_S_ISDIR
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#if defined(_WIN32)
#if !defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER >= 1400)
#define ATLEAST_VC2005
#endif
#endif

#include <vfs/vfs_implementation.h>
#include <libretro.h>
#include <memmap.h>
#include <encodings/utf.h>
#include <compat/fopen_utf8.h>
#include <file/file_path.h>

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#define RFILE_HINT_UNBUFFERED (1 << 8)

int64_t retro_vfs_file_seek_internal(libretro_vfs_implementation_file *stream, int64_t offset, int whence)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
         return retro_vfs_file_seek_cdrom(stream, offset, whence);
#endif
/* VC2005 and up have a special 64-bit fseek */
#ifdef ATLEAST_VC2005
      return _fseeki64(stream->fp, offset, whence);
#elif defined(__CELLOS_LV2__) || defined(_MSC_VER) && _MSC_VER <= 1310
      return fseek(stream->fp, (long)offset, whence);
#elif defined(PS2)
      {
         int64_t ret = fileXioLseek(fileno(stream->fp), (off_t)offset, whence);
         /* fileXioLseek could return positive numbers */
         if (ret > 0)
            return 0;
         return ret;
      }
#elif defined(ORBIS)
      {
         int ret = orbisLseek(stream->fd, offset, whence);
         if (ret < 0)
            return -1;
         return 0;
      }
#else
      return fseeko(stream->fp, (off_t)offset, whence);
#endif
   }
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
               return -1;

            stream->mappos = offset;
            break;

         case SEEK_CUR:
            if (  (offset < 0 && stream->mappos + offset > stream->mappos) ||
                  (offset > 0 && stream->mappos + offset < stream->mappos))
               return -1;

            stream->mappos += offset;
            break;

         case SEEK_END:
            if (stream->mapsize + offset < stream->mapsize)
               return -1;

            stream->mappos = stream->mapsize + offset;
            break;
      }
      return stream->mappos;
   }
#endif

   if (lseek(stream->fd, offset, whence) < 0)
      return -1;

   return 0;
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

libretro_vfs_implementation_file *retro_vfs_file_open_impl(
      const char *path, unsigned mode, unsigned hints)
{
   int                                flags = 0;
   const char                     *mode_str = NULL;
   libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)
      calloc(1, sizeof(*stream));
#if defined(VFS_FRONTEND) || defined(HAVE_CDROM)
   int                             path_len = (int)strlen(path);
#endif

#ifdef VFS_FRONTEND
   const char                 *dumb_prefix  = "vfsonly://";
   size_t                   dumb_prefix_siz = strlen(dumb_prefix);
   int                      dumb_prefix_len = (int)dumb_prefix_siz;

   if (path_len >= dumb_prefix_len)
   {
      if (!memcmp(path, dumb_prefix, dumb_prefix_len))
         path += dumb_prefix_siz;
   }
#endif

#ifdef HAVE_CDROM
   {
      const char *cdrom_prefix = "cdrom://";
      size_t cdrom_prefix_siz = strlen(cdrom_prefix);
      int cdrom_prefix_len = (int)cdrom_prefix_siz;

      if (path_len > cdrom_prefix_len)
      {
         if (!memcmp(path, cdrom_prefix, cdrom_prefix_len))
         {
            path += cdrom_prefix_siz;
            stream->scheme = VFS_SCHEME_CDROM;
         }
      }
   }
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
#if !defined(ORBIS)
#if defined(PS2)
         flags   |= FIO_S_IRUSR | FIO_S_IWUSR;
#elif !defined(_WIN32)
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         mode_str = "w+b";
         flags    = O_RDWR | O_CREAT | O_TRUNC;
#if !defined(ORBIS)
#if defined(PS2)
         flags   |= FIO_S_IRUSR | FIO_S_IWUSR;
#elif !defined(_WIN32)
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
      case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
         mode_str = "r+b";

         flags    = O_RDWR;
#if !defined(ORBIS)
#if defined(PS2)
         flags   |= FIO_S_IRUSR | FIO_S_IWUSR;
#elif !defined(_WIN32)
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
#endif
         break;

      default:
         goto error;
   }

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef ORBIS
      int fd = orbisOpen(path, flags, 0644);
      if (fd < 0)
      {
         stream->fd = -1;
         goto error;
      }
      stream->fd = fd;
#else
      FILE *fp;
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
      {
         retro_vfs_file_open_cdrom(stream, path, mode, hints);
#if defined(_WIN32) && !defined(_XBOX)
         if (!stream->fh)
            goto error;
#else
         if (!stream->fp)
            goto error;
#endif
      }
      else
#endif
      {
         fp = (FILE*)fopen_utf8(path, mode_str);

         if (!fp)
            goto error;

         stream->fp  = fp;
      }
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
#if !defined(PS2) && !defined(PSP)
      if (stream->scheme != VFS_SCHEME_CDROM)
      {
         stream->buf = (char*)calloc(1, 0x4000);
         if (stream->fp)
            setvbuf(stream->fp, stream->buf, _IOFBF, 0x4000);
      }
#endif
#endif
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
      stream->fd          = open(path, flags, 0);
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
#ifdef ORBIS
   stream->size = orbisLseek(stream->fd, 0, SEEK_END);
   orbisLseek(stream->fd, 0, SEEK_SET);
#else
#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
   {
      retro_vfs_file_seek_cdrom(stream, 0, SEEK_SET);
      retro_vfs_file_seek_cdrom(stream, 0, SEEK_END);

      stream->size = retro_vfs_file_tell_impl(stream);

      retro_vfs_file_seek_cdrom(stream, 0, SEEK_SET);
   }
   else
#endif
   {
      retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
      retro_vfs_file_seek_internal(stream, 0, SEEK_END);

      stream->size = retro_vfs_file_tell_impl(stream);

      retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
   }
#endif
   return stream;

error:
   retro_vfs_file_close_impl(stream);
   return NULL;
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;

#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
   {
      retro_vfs_file_close_cdrom(stream);
      goto end;
   }
#endif

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      if (stream->fp)
      {
         fclose(stream->fp);
      }
   }
   else
   {
#ifdef HAVE_MMAP
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
         munmap(stream->mapped, stream->mapsize);
#endif
   }

   if (stream->fd > 0)
   {
#ifdef ORBIS
      orbisClose(stream->fd);
      stream->fd = -1;
#else
      close(stream->fd);
#endif
   }
#ifdef HAVE_CDROM
end:
   if (stream->cdrom.cue_buf)
      free(stream->cdrom.cue_buf);
#endif
   if (stream->buf)
      free(stream->buf);

   if (stream->orig_path)
      free(stream->orig_path);

   free(stream);

   return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
      return retro_vfs_file_error_cdrom(stream);
#endif
#ifdef ORBIS
   /* TODO/FIXME - implement this? */
   return 0;
#else
   return ferror(stream->fp);
#endif
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (stream)
      return stream->size;
   return 0;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length)
{
   if (!stream)
      return -1;

#ifdef _WIN32
   if (_chsize(_fileno(stream->fp), length) != 0)
      return -1;
#elif !defined(VITA) && !defined(PSP) && !defined(PS2) && !defined(ORBIS) && (!defined(SWITCH) || defined(HAVE_LIBNX))
   if (ftruncate(fileno(stream->fp), length) != 0)
      return -1;
#endif

   return 0;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
         return retro_vfs_file_tell_cdrom(stream);
#endif
#ifdef ORBIS
      {
         int64_t ret = orbisLseek(stream->fd, 0, SEEK_CUR);
         if (ret < 0)
            return -1;
         return ret;
      }
#else
      /* VC2005 and up have a special 64-bit ftell */
#ifdef ATLEAST_VC2005
      return _ftelli64(stream->fp);
#else
      return ftell(stream->fp);
#endif
#endif
   }
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

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream,
      int64_t offset, int seek_position)
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

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream,
      void *s, uint64_t len)
{
   if (!stream || !s)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
         return retro_vfs_file_read_cdrom(stream, s, len);
#endif
#ifdef ORBIS
      if (orbisRead(stream->fd, s, (size_t)len) < 0)
         return -1;
      return 0;
#else
      return fread(s, 1, (size_t)len, stream->fp);
#endif
   }
#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
   {
      if (stream->mappos > stream->mapsize)
         return -1;

      if (stream->mappos + len > stream->mapsize)
         len = stream->mapsize - stream->mappos;

      memcpy(s, &stream->mapped[stream->mappos], len);
      stream->mappos += len;

      return len;
   }
#endif

   return read(stream->fd, s, (size_t)len);
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef ORBIS
      if (orbisWrite(stream->fd, s, (size_t)len) < 0)
         return -1;
      return 0;
#else
      return fwrite(s, 1, (size_t)len, stream->fp);
#endif
   }

#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      return -1;
#endif
   return write(stream->fd, s, (size_t)len);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;
#ifdef ORBIS
   return 0;
#else
   return fflush(stream->fp) == 0 ? 0 : -1;
#endif
}

int retro_vfs_file_remove_impl(const char *path)
{
#if defined(_WIN32) && !defined(_XBOX)
   /* Win32 (no Xbox) */

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
   char *path_local    = NULL;
#else
   wchar_t *path_wide  = NULL;
#endif
   if (!path || !*path)
      return -1;
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
   return -1;
#elif defined(ORBIS)
   /* Orbis
    * TODO/FIXME - stub for now */
   return 0;
#else
   if (remove(path) == 0)
      return 0;
   return -1;
#endif
}

int retro_vfs_file_rename_impl(const char *old_path, const char *new_path)
{
#if defined(_WIN32) && !defined(_XBOX)
   /* Win32 (no Xbox) */
   int ret                 = -1;
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
   char *old_path_local    = NULL;
#else
   wchar_t *old_path_wide  = NULL;
#endif

   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
   old_path_local = utf8_to_local_string_alloc(old_path);

   if (old_path_local)
   {
      char *new_path_local = utf8_to_local_string_alloc(new_path);

      if (new_path_local)
      {
         if (rename(old_path_local, new_path_local) == 0)
            ret = 0;
         free(new_path_local);
      }

      free(old_path_local);
   }
#else
   old_path_wide = utf8_to_utf16_string_alloc(old_path);

   if (old_path_wide)
   {
      wchar_t *new_path_wide = utf8_to_utf16_string_alloc(new_path);

      if (new_path_wide)
      {
         if (_wrename(old_path_wide, new_path_wide) == 0)
            ret = 0;
         free(new_path_wide);
      }

      free(old_path_wide);
   }
#endif
   return ret;

#elif defined(ORBIS)
   /* Orbis */
   /* TODO/FIXME - Stub for now */
   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;
   return 0;

#else
   /* Every other platform */
   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;
   return rename(old_path, new_path) == 0 ? 0 : -1;
#endif
}

const char *retro_vfs_file_get_path_impl(
      libretro_vfs_implementation_file *stream)
{
   /* should never happen, do something noisy so caller can be fixed */
   if (!stream)
      abort();
   return stream->orig_path;
}

int retro_vfs_stat_impl(const char *path, int32_t *size)
{
#if defined(VITA) || defined(PSP)
   /* Vita / PSP */
   SceIoStat buf;
   int stat_ret;
   bool is_dir               = false;
   bool is_character_special = false;
   char *tmp                 = NULL;
   size_t len                = 0;

   if (!path || !*path)
      return 0;

   tmp                       = strdup(path);
   len                       = strlen(tmp);
   if (tmp[len-1] == '/')
      tmp[len-1] = '\0';

   stat_ret = sceIoGetstat(tmp, &buf);
   free(tmp);
   if (stat_ret < 0)
      return 0;

   if (size)
      *size = (int32_t)buf.st_size;

   is_dir = FIO_S_ISDIR(buf.st_mode);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);

#elif defined(ORBIS)
   /* Orbis */
   bool is_dir, is_character_special;
   int dir_ret;

   if (!path || !*path)
      return 0;

   if (size)
      *size = (int32_t)buf.st_size;

   dir_ret = orbisDopen(path);
   is_dir  = dir_ret > 0;
   orbisDclose(dir_ret);

   is_character_special = S_ISCHR(buf.st_mode);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);

#elif defined(PS2)
   /* PS2 */
   iox_stat_t buf;
   bool is_dir;
   bool is_character_special = false;
   char *tmp                 = NULL;
   size_t len                = 0;

   if (!path || !*path)
      return 0;

   tmp        = strdup(path);
   len        = strlen(tmp);
   if (tmp[len-1] == '/')
      tmp[len-1] = '\0';

   fileXioGetStat(tmp, &buf);
   free(tmp);

   if (size)
      *size = (int32_t)buf.size;

   if (!buf.mode)
   {
      /* if fileXioGetStat fails */
      int dir_ret = fileXioDopen(path);
      is_dir      =  dir_ret > 0;
      if (is_dir) {
         fileXioDclose(dir_ret);
      }
   }
   else
      is_dir = FIO_S_ISDIR(buf.mode);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);

#elif defined(__CELLOS_LV2__)
   /* CellOS Lv2 */
   bool is_dir;
   bool is_character_special = false;
   CellFsStat buf;

   if (!path || !*path)
      return 0;
   if (cellFsStat(path, &buf) < 0)
      return 0;

   if (size)
      *size = (int32_t)buf.st_size;

   is_dir = ((buf.st_mode & S_IFMT) == S_IFDIR);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);

#elif defined(_WIN32)
   /* Windows */
   bool is_dir;
   DWORD file_info;
   struct _stat buf;
   bool is_character_special = false;
#if defined(LEGACY_WIN32)
   char *path_local          = NULL;
#else
   wchar_t *path_wide        = NULL;
#endif

   if (!path || !*path)
      return 0;

#if defined(LEGACY_WIN32)
   path_local = utf8_to_local_string_alloc(path);
   file_info  = GetFileAttributes(path_local);

   if (!string_is_empty(path_local))
      _stat(path_local, &buf);

   if (path_local)
      free(path_local);
#else
   path_wide = utf8_to_utf16_string_alloc(path);
   file_info = GetFileAttributesW(path_wide);

   _wstat(path_wide, &buf);

   if (path_wide)
      free(path_wide);
#endif

   if (file_info == INVALID_FILE_ATTRIBUTES)
      return 0;

   if (size)
      *size = (int32_t)buf.st_size;

   is_dir = (file_info & FILE_ATTRIBUTE_DIRECTORY);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);

#else
   /* Every other platform */
   bool is_dir, is_character_special;
   struct stat buf;

   if (!path || !*path)
      return 0;
   if (stat(path, &buf) < 0)
      return 0;

   if (size)
      *size             = (int32_t)buf.st_size;

   is_dir               = S_ISDIR(buf.st_mode);
   is_character_special = S_ISCHR(buf.st_mode);

   return RETRO_VFS_STAT_IS_VALID | (is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0) | (is_character_special ? RETRO_VFS_STAT_IS_CHARACTER_SPECIAL : 0);
#endif
}

#if defined(VITA)
#define path_mkdir_error(ret) (((ret) == SCE_ERROR_ERRNO_EEXIST))
#elif defined(PSP) || defined(PS2) || defined(_3DS) || defined(WIIU) || defined(SWITCH) || defined(ORBIS)
#define path_mkdir_error(ret) ((ret) == -1)
#else
#define path_mkdir_error(ret) ((ret) < 0 && errno == EEXIST)
#endif

int retro_vfs_mkdir_impl(const char *dir)
{
#if defined(_WIN32)
#ifdef LEGACY_WIN32
   int ret       = _mkdir(dir);
#else
   wchar_t *dirW = utf8_to_utf16_string_alloc(dir);
   int       ret = -1;

   if (dirW)
   {
      ret = _wmkdir(dirW);
      free(dirW);
   }
#endif
#elif defined(IOS)
   int ret = mkdir(dir, 0755);
#elif defined(VITA) || defined(PSP)
   int ret = sceIoMkdir(dir, 0777);
#elif defined(PS2)
   int ret = fileXioMkdir(dir, 0777);
#elif defined(ORBIS)
   int ret = orbisMkdir(dir, 0755);
#elif defined(__QNX__)
   int ret = mkdir(dir, 0777);
#else
   int ret = mkdir(dir, 0750);
#endif

   if (path_mkdir_error(ret))
      return -2;
   return ret < 0 ? -1 : 0;
}

#ifdef VFS_FRONTEND
struct retro_vfs_dir_handle
#else
struct libretro_vfs_implementation_dir
#endif
{
   char* orig_path;
#if defined(_WIN32)
#if defined(LEGACY_WIN32)
   WIN32_FIND_DATA entry;
#else
   WIN32_FIND_DATAW entry;
#endif
   HANDLE directory;
   bool next;
   char path[PATH_MAX_LENGTH];
#elif defined(VITA) || defined(PSP)
   SceUID directory;
   SceIoDirent entry;
#elif defined(PS2)
   int directory;
   iox_dirent_t entry;
#elif defined(__CELLOS_LV2__)
   CellFsErrno error;
   int directory;
   CellFsDirent entry;
#elif defined(ORBIS)
   int directory;
   struct dirent entry;
#else
   DIR *directory;
   const struct dirent *entry;
#endif
};

static bool dirent_check_error(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
   return (rdir->directory == INVALID_HANDLE_VALUE);
#elif defined(VITA) || defined(PSP) || defined(PS2) || defined(ORBIS)
   return (rdir->directory < 0);
#elif defined(__CELLOS_LV2__)
   return (rdir->error != CELL_FS_SUCCEEDED);
#else
   return !(rdir->directory);
#endif
}

libretro_vfs_implementation_dir *retro_vfs_opendir_impl(
      const char *name, bool include_hidden)
{
#if defined(_WIN32)
   unsigned path_len;
   char path_buf[1024];
   size_t copied      = 0;
#if defined(LEGACY_WIN32)
   char *path_local   = NULL;
#else
   wchar_t *path_wide = NULL;
#endif
#endif
   libretro_vfs_implementation_dir *rdir;

   /*Reject null or empty string paths*/
   if (!name || (*name == 0))
      return NULL;

   /*Allocate RDIR struct. Tidied later with retro_closedir*/
   rdir = (libretro_vfs_implementation_dir*)calloc(1, sizeof(*rdir));
   if (!rdir)
      return NULL;

   rdir->orig_path       = strdup(name);

#if defined(_WIN32)
   path_buf[0]           = '\0';
   path_len              = strlen(name);

   copied                = strlcpy(path_buf, name, sizeof(path_buf));

   /* Non-NT platforms don't like extra slashes in the path */
   if (name[path_len - 1] != '\\')
      path_buf[copied++]   = '\\';

   path_buf[copied]        = '*';
   path_buf[copied+1]      = '\0';

#if defined(LEGACY_WIN32)
   path_local              = utf8_to_local_string_alloc(path_buf);
   rdir->directory         = FindFirstFile(path_local, &rdir->entry);

   if (path_local)
      free(path_local);
#else
   path_wide               = utf8_to_utf16_string_alloc(path_buf);
   rdir->directory         = FindFirstFileW(path_wide, &rdir->entry);

   if (path_wide)
      free(path_wide);
#endif

#elif defined(VITA) || defined(PSP)
   rdir->directory       = sceIoDopen(name);
#elif defined(PS2)
   rdir->directory       = ps2fileXioDopen(name);
#elif defined(_3DS)
   rdir->directory       = !string_is_empty(name) ? opendir(name) : NULL;
   rdir->entry           = NULL;
#elif defined(__CELLOS_LV2__)
   rdir->error           = cellFsOpendir(name, &rdir->directory);
#elif defined(ORBIS)
   rdir->directory       = orbisDopen(name);
#else
   rdir->directory       = opendir(name);
   rdir->entry           = NULL;
#endif

#ifdef _WIN32
   if (include_hidden)
      rdir->entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
   else
      rdir->entry.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
#endif

   if (rdir->directory && !dirent_check_error(rdir))
      return rdir;

   retro_vfs_closedir_impl(rdir);
   return NULL;
}

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
   if (rdir->next)
#if defined(LEGACY_WIN32)
      return (FindNextFile(rdir->directory, &rdir->entry) != 0);
#else
      return (FindNextFileW(rdir->directory, &rdir->entry) != 0);
#endif

   rdir->next = true;
   return (rdir->directory != INVALID_HANDLE_VALUE);
#elif defined(VITA) || defined(PSP)
   return (sceIoDread(rdir->directory, &rdir->entry) > 0);
#elif defined(PS2)
   iox_dirent_t record;
   int ret = ps2fileXioDread(rdir->directory, &record);
   rdir->entry = record;
   return ( ret > 0);
#elif defined(__CELLOS_LV2__)
   uint64_t nread;
   rdir->error = cellFsReaddir(rdir->directory, &rdir->entry, &nread);
   return (nread != 0);
#elif defined(ORBIS)
   return (orbisDread(rdir->directory, &rdir->entry) > 0);
#else
   return ((rdir->entry = readdir(rdir->directory)) != NULL);
#endif
}

const char *retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
#if defined(LEGACY_WIN32)
   {
      char *name_local = local_to_utf8_string_alloc(rdir->entry.cFileName);
      memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
      strlcpy(rdir->entry.cFileName, name_local, sizeof(rdir->entry.cFileName));

      if (name_local)
         free(name_local);
   }
#else
   {
      char *name       = utf16_to_utf8_string_alloc(rdir->entry.cFileName);
      memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
      strlcpy((char*)rdir->entry.cFileName, name, sizeof(rdir->entry.cFileName));

      if (name)
         free(name);
   }
#endif
   return (char*)rdir->entry.cFileName;
#elif defined(VITA) || defined(PSP) || defined(__CELLOS_LV2__) || defined(ORBIS)
   return rdir->entry.d_name;
#elif defined(PS2)
   return rdir->entry.name;
#else
   if (!rdir || !rdir->entry)
      return NULL;
   return rdir->entry->d_name;
#endif
}

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
   const WIN32_FIND_DATA *entry = (const WIN32_FIND_DATA*)&rdir->entry;
   return entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
#elif defined(PSP) || defined(VITA)
   const SceIoDirent *entry = (const SceIoDirent*)&rdir->entry;
#if defined(PSP)
   return (entry->d_stat.st_attr & FIO_SO_IFDIR) == FIO_SO_IFDIR;
#elif defined(VITA)
   return SCE_S_ISDIR(entry->d_stat.st_mode);
#endif
#elif defined(PS2)
   const iox_dirent_t *entry = (const iox_dirent_t*)&rdir->entry;
   return FIO_S_ISDIR(entry->stat.mode);
#elif defined(__CELLOS_LV2__)
   CellFsDirent *entry = (CellFsDirent*)&rdir->entry;
   return (entry->d_type == CELL_FS_TYPE_DIRECTORY);
#elif defined(ORBIS)
   const struct dirent *entry = &rdir->entry;
   if (entry->d_type == DT_DIR)
      return true;
   if (!(entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK))
      return false;
#else
   struct stat buf;
   char path[PATH_MAX_LENGTH];
#if defined(DT_DIR)
   const struct dirent *entry = (const struct dirent*)rdir->entry;
   if (entry->d_type == DT_DIR)
      return true;
   /* This can happen on certain file systems. */
   if (!(entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK))
      return false;
#endif
   /* dirent struct doesn't have d_type, do it the slow way ... */
   path[0] = '\0';
   fill_pathname_join(path, rdir->orig_path, retro_vfs_dirent_get_name_impl(rdir), sizeof(path));
   if (stat(path, &buf) < 0)
      return false;
   return S_ISDIR(buf.st_mode);
#endif
}

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir *rdir)
{
   if (!rdir)
      return -1;

#if defined(_WIN32)
   if (rdir->directory != INVALID_HANDLE_VALUE)
      FindClose(rdir->directory);
#elif defined(VITA) || defined(PSP)
   sceIoDclose(rdir->directory);
#elif defined(PS2)
   ps2fileXioDclose(rdir->directory);
#elif defined(__CELLOS_LV2__)
   rdir->error = cellFsClosedir(rdir->directory);
#elif defined(ORBIS)
   orbisDclose(rdir->directory);
#else
   if (rdir->directory)
      closedir(rdir->directory);
#endif

   if (rdir->orig_path)
      free(rdir->orig_path);
   free(rdir);
   return 0;
}
