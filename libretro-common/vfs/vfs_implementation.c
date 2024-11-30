/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <string/stdstring.h> /* string_is_empty */

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
#  include <sys/types.h>
#  include <sys/stat.h>
#  if !defined(VITA)
#  include <dirent.h>
#  endif
#  include <unistd.h>
#  if defined(WIIU)
#  include <malloc.h>
#  endif
#endif

#include <fcntl.h>

/* TODO: Some things are duplicated but I'm really afraid of breaking other platforms by touching this */
#if defined(VITA)
#  include <psp2/io/fcntl.h>
#  include <psp2/io/dirent.h>
#  include <psp2/io/stat.h>
#elif !defined(_WIN32)
#  if defined(PSP)
#    include <pspiofilemgr.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>
#  include <unistd.h>
#endif

#if defined(__QNX__) || defined(PSP)
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


#if defined(PSP)
#include <pspkernel.h>
#endif

#if defined(__PS3__) || defined(__PSL1GHT__)
#define FS_SUCCEEDED 0
#define FS_TYPE_DIR 1
#ifdef __PSL1GHT__
#include <lv2/sysfs.h>
#ifndef O_RDONLY
#define O_RDONLY SYS_O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY SYS_O_WRONLY
#endif
#ifndef O_CREAT
#define O_CREAT SYS_O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC SYS_O_TRUNC
#endif
#ifndef O_RDWR
#define O_RDWR SYS_O_RDWR
#endif
#else
#include <cell/cell_fs.h>
#ifndef O_RDONLY
#define O_RDONLY CELL_FS_O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY CELL_FS_O_WRONLY
#endif
#ifndef O_CREAT
#define O_CREAT CELL_FS_O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC CELL_FS_O_TRUNC
#endif
#ifndef O_RDWR
#define O_RDWR CELL_FS_O_RDWR
#endif
#ifndef sysFsStat
#define sysFsStat cellFsStat
#endif
#ifndef sysFSDirent
#define sysFSDirent CellFsDirent
#endif
#ifndef sysFsOpendir
#define sysFsOpendir cellFsOpendir
#endif
#ifndef sysFsReaddir
#define sysFsReaddir cellFsReaddir
#endif
#ifndef sysFSDirent
#define sysFSDirent CellFsDirent
#endif
#ifndef sysFsClosedir
#define sysFsClosedir cellFsClosedir
#endif
#endif
#endif

#if defined(VITA)
#define FIO_S_ISDIR SCE_S_ISDIR
#endif

#if defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1400
#define ATLEAST_VC2005
#endif
#endif

#include <vfs/vfs_implementation.h>
#include <libretro.h>
#if defined(HAVE_MMAP)
#include <memmap.h>
#endif
#include <encodings/utf.h>
#include <compat/fopen_utf8.h>
#include <file/file_path.h>

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#if (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE - 0) >= 200112) || (defined(__POSIX_VISIBLE) && __POSIX_VISIBLE >= 200112) || (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112) || __USE_LARGEFILE || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64)
#ifndef HAVE_64BIT_OFFSETS
#define HAVE_64BIT_OFFSETS
#endif
#endif

#define RFILE_HINT_UNBUFFERED (1 << 8)

int64_t retro_vfs_file_seek_internal(
      libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
         return retro_vfs_file_seek_cdrom(stream, offset, whence);
#endif
#ifdef ATLEAST_VC2005
      /* VC2005 and up have a special 64-bit fseek */
      return _fseeki64(stream->fp, offset, whence);
#elif defined(HAVE_64BIT_OFFSETS)
      return fseeko(stream->fp, (off_t)offset, whence);
#else
      return fseek(stream->fp, (long)offset, whence);
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

   if (lseek(stream->fd, (off_t)offset, whence) < 0)
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
   libretro_vfs_implementation_file *stream =
      (libretro_vfs_implementation_file*)
      malloc(sizeof(*stream));

   if (!stream)
      return NULL;

   stream->fd                     = 0;
   stream->hints                  = hints;
   stream->size                   = 0;
   stream->buf                    = NULL;
   stream->fp                     = NULL;
#ifdef _WIN32
   stream->fh                     = 0;
#endif
   stream->orig_path              = NULL;
   stream->mappos                 = 0;
   stream->mapsize                = 0;
   stream->mapped                 = NULL;
   stream->scheme                 = VFS_SCHEME_NONE;

#ifdef VFS_FRONTEND
   if (     path
         && path[0] == 'v'
         && path[1] == 'f'
         && path[2] == 's'
         && path[3] == 'o'
         && path[4] == 'n'
         && path[5] == 'l'
         && path[6] == 'y'
         && path[7] == ':'
         && path[8] == '/'
         && path[9] == '/')
         path             += sizeof("vfsonly://")-1;
#endif

#ifdef HAVE_CDROM
   stream->cdrom.cue_buf          = NULL;
   stream->cdrom.cue_len          = 0;
   stream->cdrom.byte_pos         = 0;
   stream->cdrom.drive            = 0;
   stream->cdrom.cur_min          = 0;
   stream->cdrom.cur_sec          = 0;
   stream->cdrom.cur_frame        = 0;
   stream->cdrom.cur_track        = 0;
   stream->cdrom.cur_lba          = 0;
   stream->cdrom.last_frame_lba   = 0;
   stream->cdrom.last_frame[0]    = '\0';
   stream->cdrom.last_frame_valid = false;

   if (     path
         && path[0] == 'c'
         && path[1] == 'd'
         && path[2] == 'r'
         && path[3] == 'o'
         && path[4] == 'm'
         && path[5] == ':'
         && path[6] == '/'
         && path[7] == '/'
         && path[8] != '\0')
   {
      path             += sizeof("cdrom://")-1;
      stream->scheme    = VFS_SCHEME_CDROM;
   }
#endif

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
#if !defined(_WIN32)
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         mode_str = "w+b";
         flags    = O_RDWR | O_CREAT | O_TRUNC;
#if !defined(_WIN32)
         flags   |= S_IRUSR | S_IWUSR;
#else
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
      case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
         mode_str = "r+b";

         flags    = O_RDWR;
#if !defined(_WIN32)
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
         if (!(fp = (FILE*)fopen_utf8(path, mode_str)))
            goto error;

         stream->fp  = fp;
      }

      /* Regarding setvbuf:
       *
       * https://www.freebsd.org/cgi/man.cgi?query=setvbuf&apropos=0&sektion=0&manpath=FreeBSD+11.1-RELEASE&arch=default&format=html
       *
       * If the size argument is not zero but buf is NULL,
       * a buffer of the given size will be allocated immediately, and
       * released on close. This is an extension to ANSI C.
       *
       * Since C89 does not support specifying a NULL buffer
       * with a non-zero size, we create and track our own buffer for it.
       */
      /* TODO: this is only useful for a few platforms,
       * find which and add ifdef */
#if defined(_3DS)
      if (stream->scheme != VFS_SCHEME_CDROM)
      {
         stream->buf = (char*)calloc(1, 0x10000);
         if (stream->fp)
            setvbuf(stream->fp, stream->buf, _IOFBF, 0x10000);
      }
#elif defined(WIIU)
      if (stream->scheme != VFS_SCHEME_CDROM)
      {
         const int bufsize = 128 * 1024;
         stream->buf = (char*)memalign(0x40, bufsize);
         if (stream->fp)
            setvbuf(stream->fp, stream->buf, _IOFBF, bufsize);
         stream->buf = (char*)calloc(1, 0x4000);
         if (stream->fp)
            setvbuf(stream->fp, stream->buf, _IOFBF, 0x4000);
      }
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

         if ((stream->mapped = (uint8_t*)mmap((void*)0,
               stream->mapsize, PROT_READ,  MAP_SHARED, stream->fd, 0)) == MAP_FAILED)
            stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
      }
#endif
   }
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
   return ferror(stream->fp);
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (stream)
      return stream->size;
   return 0;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length)
{
#ifdef _WIN32
   if (stream && _chsize(_fileno(stream->fp), length) == 0)
   {
	   stream->size = length;
	   return 0;
   }
#elif !defined(VITA) && !defined(PSP) && !defined(PS2) && !defined(ORBIS) && (!defined(SWITCH) || defined(HAVE_LIBNX))
   if (stream && ftruncate(fileno(stream->fp), (off_t)length) == 0)
   {
      stream->size = length;
      return 0;
   }
#endif
   return -1;
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
#ifdef ATLEAST_VC2005
      /* VC2005 and up have a special 64-bit ftell */
      return _ftelli64(stream->fp);
#elif defined(HAVE_64BIT_OFFSETS)
      return ftello(stream->fp);
#else
      return ftell(stream->fp);
#endif
   }
#ifdef HAVE_MMAP
   /* Need to check stream->mapped because this function
    * is called in filestream_open() */
   if (stream->mapped && stream->hints &
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      return stream->mappos;
#endif
   if (lseek(stream->fd, 0, SEEK_CUR) < 0)
      return -1;

   return 0;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream,
      int64_t offset, int seek_position)
{
   return retro_vfs_file_seek_internal(stream, offset, seek_position);
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
      return fread(s, 1, (size_t)len, stream->fp);
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
   int64_t pos    = 0;
   ssize_t result = -1;

   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      pos    = retro_vfs_file_tell_impl(stream);
      result = fwrite(s, 1, (size_t)len, stream->fp);

      if (result != -1 && pos + result > stream->size)
         stream->size = pos + result;

      return result;
   }
#ifdef HAVE_MMAP
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      return -1;
#endif

   pos    = retro_vfs_file_tell_impl(stream);
   result = write(stream->fd, s, (size_t)len);

   if (result != -1 && pos + result > stream->size)
      stream->size = pos + result;

   return result;
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (stream && fflush(stream->fp) == 0)
      return 0;
   return -1;
}

int retro_vfs_file_remove_impl(const char *path)
{
   if (path && *path)
   {
      int ret          = -1;
#if defined(_WIN32) && !defined(_XBOX)
      /* Win32 (no Xbox) */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500
      char *path_local = NULL;
      if ((path_local = utf8_to_local_string_alloc(path)))
      {
         /* We need to check if path is a directory */
         if ((retro_vfs_stat_impl(path, NULL) & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
            ret = _rmdir(path_local);
         else
            ret = remove(path_local);
         free(path_local);
      }
#else
      wchar_t *path_wide = NULL;
      if ((path_wide = utf8_to_utf16_string_alloc(path)))
      {
         /* We need to check if path is a directory */
         if ((retro_vfs_stat_impl(path, NULL) & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
            ret = _wrmdir(path_wide);
         else
            ret = _wremove(path_wide);
         free(path_wide);
      }
#endif
#else
      ret = remove(path);
#endif
      if (ret == 0)
         return 0;
   }
   return -1;
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
   int ret                   = RETRO_VFS_STAT_IS_VALID;

   if (!path || !*path)
      return 0;
   {
#if defined(VITA)
      /* Vita / PSP */
      SceIoStat stat_buf;
      int dir_ret;
      char *tmp                 = strdup(path);
      size_t len                = strlen(tmp);
      if (tmp[len-1] == '/')
         tmp[len-1]             = '\0';

      dir_ret                   = sceIoGetstat(tmp, &stat_buf);
      free(tmp);
      if (dir_ret < 0)
         return 0;

      if (size)
         *size                  = (int32_t)stat_buf.st_size;

      if (FIO_S_ISDIR(stat_buf.st_mode))
         ret              |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(__PSL1GHT__) || defined(__PS3__)
      /* Lowlevel Lv2 */
      sysFSStat stat_buf;

      if (sysFsStat(path, &stat_buf) < 0)
         return 0;

      if (size)
         *size = (int32_t)stat_buf.st_size;

      if ((stat_buf.st_mode & S_IFMT) == S_IFDIR)
         ret  |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(_WIN32)
      /* Windows */
      struct _stat stat_buf;
#if defined(LEGACY_WIN32)
      char *path_local          = utf8_to_local_string_alloc(path);
      DWORD file_info           = GetFileAttributes(path_local);

      if (!string_is_empty(path_local))
         _stat(path_local, &stat_buf);

      if (path_local)
         free(path_local);
#else
      wchar_t *path_wide        = utf8_to_utf16_string_alloc(path);
      DWORD file_info           = GetFileAttributesW(path_wide);

      _wstat(path_wide, &stat_buf);

      if (path_wide)
         free(path_wide);
#endif
      if (file_info == INVALID_FILE_ATTRIBUTES)
         return 0;

      if (size)
         *size = (int32_t)stat_buf.st_size;

      if (file_info & FILE_ATTRIBUTE_DIRECTORY)
         ret  |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(GEKKO)
      /* On GEKKO platforms, paths cannot have
       * trailing slashes - we must therefore
       * remove them */
      size_t len;
      char *path_buf = NULL;
      struct stat stat_buf;

      if (!(path_buf = strdup(path)))
         return 0;

      if ((len = strlen(path_buf)) > 0)
         if (path_buf[len - 1] == '/')
            path_buf[len - 1] = '\0';

      if (stat(path_buf, &stat_buf) < 0)
      {
         free(path_buf);
         return 0;
      }

      free(path_buf);
      
      if (size)
         *size = (int32_t)stat_buf.st_size;

      if (S_ISDIR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_DIRECTORY;
      if (S_ISCHR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
#else
      /* Every other platform */
      struct stat stat_buf;

      if (stat(path, &stat_buf) < 0)
         return 0;

      if (size)
         *size = (int32_t)stat_buf.st_size;

      if (S_ISDIR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_DIRECTORY;
      if (S_ISCHR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
#endif
   }
   return ret;
}

#if defined(VITA)
#define path_mkdir_error(ret) (((ret) == SCE_ERROR_ERRNO_EEXIST))
#elif defined(PSP) || defined(PS2) || defined(_3DS) || defined(WIIU) || defined(SWITCH)
#define path_mkdir_error(ret) ((ret) == -1)
#else
#define path_mkdir_error(ret) ((ret) < 0 && errno == EEXIST)
#endif

int retro_vfs_mkdir_impl(const char *dir)
{
#if defined(_WIN32)
#ifdef LEGACY_WIN32
   int ret        = _mkdir(dir);
#else
   wchar_t *dir_w = utf8_to_utf16_string_alloc(dir);
   int       ret  = -1;

   if (dir_w)
   {
      ret = _wmkdir(dir_w);
      free(dir_w);
   }
#endif
#elif defined(IOS)
   int ret = mkdir(dir, 0755);
#elif defined(VITA)
   int ret = sceIoMkdir(dir, 0777);
#elif defined(__QNX__)
   int ret = mkdir(dir, 0777);
#elif defined(GEKKO) || defined(WIIU)
   /* On GEKKO platforms, mkdir() fails if
    * the path has a trailing slash. We must
    * therefore remove it. */
   int ret = -1;
   if (!string_is_empty(dir))
   {
      char *dir_buf = strdup(dir);

      if (dir_buf)
      {
         size_t len = strlen(dir_buf);

         if (len > 0)
            if (dir_buf[len - 1] == '/')
               dir_buf[len - 1] = '\0';

         ret = mkdir(dir_buf, 0750);

         free(dir_buf);
      }
   }
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
#elif defined(VITA)
   SceUID directory;
   SceIoDirent entry;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   int error;
   int directory;
   sysFSDirent entry;
#else
   DIR *directory;
   const struct dirent *entry;
#endif
};

static bool dirent_check_error(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
   return (rdir->directory == INVALID_HANDLE_VALUE);
#elif defined(VITA) || defined(ORBIS)
   return (rdir->directory < 0);
#elif defined(__PSL1GHT__) || defined(__PS3__)
   return (rdir->error != FS_SUCCEEDED);
#else
   return !(rdir->directory);
#endif
}

libretro_vfs_implementation_dir *retro_vfs_opendir_impl(
      const char *name, bool include_hidden)
{
#if defined(_WIN32)
   char path_buf[1024];
   size_t copied      = 0;
#if defined(LEGACY_WIN32)
   char *path_local   = NULL;
#else
   wchar_t *path_wide = NULL;
#endif
#endif
   libretro_vfs_implementation_dir *rdir;

   /* Reject NULL or empty string paths*/
   if (!name || (*name == 0))
      return NULL;

   /*Allocate RDIR struct. Tidied later with retro_closedir*/
   if (!(rdir = (libretro_vfs_implementation_dir*)
            calloc(1, sizeof(*rdir))))
      return NULL;

   rdir->orig_path       = strdup(name);

#if defined(_WIN32)
   copied                = strlcpy(path_buf, name, sizeof(path_buf));

   /* Non-NT platforms don't like extra slashes in the path */
   if (path_buf[copied - 1] != '\\')
      path_buf [copied++]  = '\\';

   path_buf[copied  ]      = '*';
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

#elif defined(VITA)
   rdir->directory       = sceIoDopen(name);
#elif defined(_3DS)
   rdir->directory       = !string_is_empty(name) ? opendir(name) : NULL;
   rdir->entry           = NULL;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   rdir->error           = sysFsOpendir(name, &rdir->directory);
#else
   rdir->directory       = opendir(name);
   rdir->entry           = NULL;
#endif

#ifdef _WIN32
   if (include_hidden)
      rdir->entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
   else
      rdir->entry.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
#else
   (void)include_hidden;
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
#elif defined(VITA)
   return (sceIoDread(rdir->directory, &rdir->entry) > 0);
#elif defined(__PSL1GHT__) || defined(__PS3__)
   uint64_t nread;
   rdir->error = sysFsReaddir(rdir->directory, &rdir->entry, &nread);
   return (nread != 0);
#else
   return ((rdir->entry = readdir(rdir->directory)) != NULL);
#endif
}

const char *retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir *rdir)
{
#if defined(_WIN32)
#if defined(LEGACY_WIN32)
   char *name       = local_to_utf8_string_alloc(rdir->entry.cFileName);
#else
   char *name       = utf16_to_utf8_string_alloc(rdir->entry.cFileName);
#endif
   memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
   strlcpy((char*)rdir->entry.cFileName, name, sizeof(rdir->entry.cFileName));
   if (name)
      free(name);
   return (char*)rdir->entry.cFileName;
#elif defined(VITA) || defined(__PSL1GHT__) || defined(__PS3__)
   return rdir->entry.d_name;
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
#elif defined(VITA)
   const SceIoDirent *entry     = (const SceIoDirent*)&rdir->entry;
   return SCE_S_ISDIR(entry->d_stat.st_mode);
#elif defined(__PSL1GHT__) || defined(__PS3__)
   sysFSDirent *entry          = (sysFSDirent*)&rdir->entry;
   return (entry->d_type == FS_TYPE_DIR);
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
   fill_pathname_join_special(path, rdir->orig_path, retro_vfs_dirent_get_name_impl(rdir), sizeof(path));
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
#elif defined(VITA)
   sceIoDclose(rdir->directory);
#elif defined(__PSL1GHT__) || defined(__PS3__)
   rdir->error = sysFsClosedir(rdir->directory);
#else
   if (rdir->directory)
      closedir(rdir->directory);
#endif

   if (rdir->orig_path)
      free(rdir->orig_path);
   free(rdir);
   return 0;
}
