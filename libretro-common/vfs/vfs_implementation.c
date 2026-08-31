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
#include <limits.h>  /* INT_MAX, LONG_MAX -- both C89 */
#include <sys/types.h>

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
#if defined(__linux__)
#include <linux/falloc.h>
#endif

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

#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1400
#define ATLEAST_VC2005
#endif
#endif

#include <vfs/vfs_implementation.h>
#include <libretro.h>
#if defined(HAVE_MMAP)
#include <memmap.h>
#include <sys/mman.h>
#endif
#include <encodings/utf.h>
#include <compat/fopen_utf8.h>
#include <file/file_path.h>

#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
#include <vfs/vfs_implementation_saf.h>
#endif

#ifdef HAVE_SMBCLIENT
#include "vfs_implementation_smb.h"
#endif

#if (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE - 0) >= 200112) || (defined(__POSIX_VISIBLE) && __POSIX_VISIBLE >= 200112) || (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112) || __USE_LARGEFILE || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64)
#ifndef HAVE_64BIT_OFFSETS
#define HAVE_64BIT_OFFSETS
#endif
#endif

#define RFILE_HINT_UNBUFFERED (1 << 8)

/* Largest count handed to one fread/fwrite/read/write call.  bionic's
 * stdio routes counts through an int-typed callback (aborting via
 * FORTIFY above INT_MAX) and one Linux read()/write() syscall moves at
 * most MAX_RW_COUNT; both fit comfortably under 1 GiB. */
#define VFS_STDIO_IO_CHUNK_MAX (1024 * 1024 * 1024)

/* 64-bit seek on the descriptor path.
 *
 * The buffered path seeks with _fseeki64/fseeko.  The descriptor path
 * - the one RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS selects, and
 * the only one a memory-mapped file ever uses - seeked with lseek()
 * and off_t.  off_t is 32 bits on every Windows build, 64-bit ones
 * included (long is 32-bit there), and on any Unix built without
 * large-file support.  A file of 2 GiB or more therefore could not be
 * seeked, and because retro_vfs_file_open_impl() sizes a new handle by
 * seeking to its end, the failure landed on stream->size and
 * stream->mapsize at open: the whole file came back as size 0 or a
 * negative one, and the mapping was skipped.  Disc images are exactly
 * the files that cross that line.
 *
 * The width is settled at compile time, in this order:
 *
 *   Windows    SetFilePointer with the high-order dword, on the
 *              handle behind the descriptor.  Deliberately not the
 *              CRT's _lseeki64: that arrived with Visual C++ 4 and is
 *              exported by the msvcrt.dll of that era onwards, but
 *              Windows 95 shipped without an msvcrt.dll at all and
 *              which one a 9x box ended up with came down to what an
 *              installer happened to drop there.  SetFilePointer has
 *              no such history - it is the original Win32 seek, in
 *              kernel32 since NT 3.1 and Windows 95 RTM, and it is
 *              what the CRT's own _lseeki64 calls underneath.  (Not
 *              SetFilePointerEx, which is XP and up and would be the
 *              one call here that 9x could not make.)  The Xbox XDKs
 *              have it too, alongside the _get_osfhandle this file
 *              already uses for the Win32 mapping path, so no console
 *              needs a case of its own.  Note that on the original
 *              Xbox the buffered path is stuck at 32 bits regardless:
 *              that CRT has fseek(long) and no _fseeki64, so a large
 *              file there is reachable only through this path.
 *
 *              INVALID_SET_FILE_POINTER is also a legitimate low
 *              dword for a file this size, hence the SetLastError
 *              dance rather than a bare comparison.
 *
 *              Store/UWP targets, where SetFilePointer is outside the
 *              app API partition, take _lseeki64 instead - every CRT
 *              new enough to build for that target has it.
 *
 *   Android    lseek64.  Bionic declares it in <unistd.h> at every
 *              API level with no feature macro required, which is the
 *              point: _FILE_OFFSET_BITS=64 is not usable on Android
 *              before API 24, where asking for it removes mmap() from
 *              the ABI rather than widening it.
 *
 *   otherwise  lseek with off_t, whose width is the build's business
 *              and not this file's.  An earlier version of this tried
 *              to widen it here by defining _FILE_OFFSET_BITS and
 *              _LARGEFILE64_SOURCE above the includes, which was wrong
 *              twice: this file is #included into griffin.c as the
 *              424th member of that translation unit, so "before every
 *              header" is not a promise it can keep, and the macros it
 *              set were read further down as capability tests -
 *              _LARGEFILE64_SOURCE selecting struct stat64/stat64(),
 *              which 3DS, PSP, DJGPP and Darwin do not have, and
 *              _FILE_OFFSET_BITS forcing HAVE_64BIT_OFFSETS and its
 *              fseeko/ftello onto every non-Windows target.  A build
 *              wanting a wider off_t passes -D_FILE_OFFSET_BITS=64
 *              itself, which is the only place it can be set
 *              consistently for a whole program anyway.  Where off_t
 *              is narrow, an offset it cannot represent is refused
 *              here rather than silently truncated.
 */
/* 64-bit seek on the buffered path.
 *
 * The existing ladder asked for _fseeki64 only under ATLEAST_VC2005
 * (an _MSC_VER test, so never true for MinGW) and otherwise for
 * fseeko under HAVE_64BIT_OFFSETS, whose POSIX feature macros no
 * Windows toolchain defines by itself.  A MinGW build that did not
 * happen to pass -D_FILE_OFFSET_BITS=64 therefore fell all the way
 * through to fseek(fp, (long)offset, whence) and could not seek a
 * stream past 2 GiB - the same failure as the descriptor path, in the
 * path callers reach without asking for any hint at all.  Measured on
 * an unfixed tree, both Windows targets: size -1073741824, seeks
 * failing, tell reporting the truncation.
 *
 * Windows tiers, widest provenance last:
 *
 *   _fseeki64      MSVC 2005 and up, and MinGW/MinGW-w64, which
 *                  declare it and import it from msvcrt.
 *   fgetpos/       C89, in every CRT that has ever existed, including
 *   fsetpos        the 2002 Xbox one - and on Windows fpos_t is a
 *                  64-bit scalar (__int64) rather than the struct the
 *                  standard permits, so it carries the whole range.
 *                  This is the tier VC6 and the Xbox XDK land on.
 *                  SEEK_END needs a length, taken from the handle
 *                  with GetFileSize rather than from a CRT call, for
 *                  the same reason SetFilePointer was chosen below.
 *   fseek          Anything else: unchanged behaviour, with an
 *                  unrepresentable offset refused rather than
 *                  silently truncated.
 */
#if defined(_WIN32)
#if defined(ATLEAST_VC2005) || defined(__MINGW32__)
#define RETRO_VFS_HAVE_FSEEKI64 1
#elif !defined(__STDC__) && defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 64
#define RETRO_VFS_HAVE_FPOS64 1
#endif
#endif

#if defined(_WIN32) && !defined(RETRO_VFS_HAVE_FSEEKI64) && defined(RETRO_VFS_HAVE_FPOS64)
static int64_t retro_vfs_fp_length64(FILE *fp)
{
   HANDLE h;
   DWORD  lo, hi = 0;

   fflush(fp);
   h = (HANDLE)_get_osfhandle(_fileno(fp));
   if (h == INVALID_HANDLE_VALUE)
      return -1;

   SetLastError(NO_ERROR);
   lo = GetFileSize(h, &hi);
   if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
      return -1;

   return (int64_t)(((uint64_t)hi << 32) | (uint64_t)lo);
}
#endif

static int retro_vfs_fp_seek64(FILE *fp, int64_t offset, int whence)
{
#if defined(RETRO_VFS_HAVE_FSEEKI64)
   return _fseeki64(fp, offset, whence);
#elif defined(RETRO_VFS_HAVE_FPOS64)
   fpos_t pos;

   switch (whence)
   {
      case SEEK_SET:
         break;
      case SEEK_CUR:
         if (fgetpos(fp, &pos) != 0)
            return -1;
         offset += (int64_t)pos;
         break;
      case SEEK_END:
         {
            const int64_t len = retro_vfs_fp_length64(fp);
            if (len < 0)
               return -1;
            offset += len;
         }
         break;
      default:
         return -1;
   }

   if (offset < 0)
      return -1;
   pos = (fpos_t)offset;
   return fsetpos(fp, &pos) == 0 ? 0 : -1;
#elif defined(HAVE_64BIT_OFFSETS)
   return fseeko(fp, (off_t)offset, whence);
#else
   if (sizeof(long) < sizeof(int64_t))
   {
      if (     offset >  (int64_t)LONG_MAX
            || offset < -(int64_t)LONG_MAX - 1)
         return -1;
   }
   return fseek(fp, (long)offset, whence) != 0 ? -1 : 0;
#endif
}

static int64_t retro_vfs_fp_tell64(FILE *fp)
{
#if defined(RETRO_VFS_HAVE_FSEEKI64)
   return _ftelli64(fp);
#elif defined(RETRO_VFS_HAVE_FPOS64)
   fpos_t pos;
   if (fgetpos(fp, &pos) != 0)
      return -1;
   return (int64_t)pos;
#elif defined(HAVE_64BIT_OFFSETS)
   return ftello(fp);
#else
   return ftell(fp);
#endif
}

static int64_t retro_vfs_fd_seek64(int fd, int64_t offset, int whence)
{
#if defined(_WIN32) && (!defined(WINAPI_FAMILY) || defined(_CRT_USE_WINAPI_FAMILY_DESKTOP_APP))
   HANDLE h = (HANDLE)_get_osfhandle(fd);
   LONG   hi;
   DWORD  lo;
   DWORD  method;

   if (h == INVALID_HANDLE_VALUE)
      return -1;

   switch (whence)
   {
      case SEEK_SET:
         method = FILE_BEGIN;
         break;
      case SEEK_CUR:
         method = FILE_CURRENT;
         break;
      case SEEK_END:
         method = FILE_END;
         break;
      default:
         return -1;
   }

   /* Two's complement split: a negative offset relative to CURRENT or
    * END arrives as a sign-extended high dword, which is what
    * SetFilePointer's signed PLONG wants. */
   hi = (LONG)(offset >> 32);
   SetLastError(NO_ERROR);
   lo = SetFilePointer(h, (LONG)(DWORD)(offset & 0xFFFFFFFFu), &hi, method);
   if (lo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
      return -1;

   return (int64_t)(((uint64_t)(DWORD)hi << 32) | (uint64_t)lo);
#elif defined(_WIN32)
   return (int64_t)_lseeki64(fd, (__int64)offset, whence);
#elif defined(__ANDROID__)
   return (int64_t)lseek64(fd, (off64_t)offset, whence);
#elif defined(VITA)
   /* SCE_SEEK_SET/CUR/END are 0/1/2, the same values as SEEK_*. */
   {
      SceOff pos = sceIoLseek(fd, (SceOff)offset, whence);
      return (pos < 0) ? -1 : (int64_t)pos;
   }
#else
   /* Compile-time: the cast is lossless exactly when off_t is wide
    * enough, and the guard costs nothing when it is (the comparison
    * folds away). */
   if (sizeof(off_t) < sizeof(int64_t))
   {
      if (     offset >  (int64_t)LONG_MAX
            || offset < -(int64_t)LONG_MAX - 1)
         return -1;
   }
   return (int64_t)lseek(fd, (off_t)offset, whence);
#endif
}

/* Keeps a cold path that needs a PATH_MAX_LENGTH scratch buffer out of
 * a caller that would otherwise not need a frame at all. Under -Os the
 * compiler is already optimizing for size and the forced outlining only
 * buys a call, so it is disabled there. */
#if defined(__OPTIMIZE_SIZE__)
#define VFS_NOINLINE
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define VFS_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define VFS_NOINLINE __declspec(noinline)
#else
#define VFS_NOINLINE
#endif

#ifdef HAVE_CDROM
static int path_is_cdrom(const char *p)
{
   return (p
         && p[0] == 'c' && p[1] == 'd' && p[2] == 'r'
         && p[3] == 'o' && p[4] == 'm' && p[5] == ':'
         && p[6] == '/' && p[7] == '/' && p[8] != '\0');
}
#endif

#ifdef HAVE_SMBCLIENT
static int path_is_smb(const char *p)
{
   return (p
         && p[0] == 's' && p[1] == 'm' && p[2] == 'b'
         && p[3] == ':' && p[4] == '/' && p[5] == '/'
         && p[6] != '\0');
}
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
static int path_is_saf(const char *p)
{
   return (p
         && p[0] == 's' && p[1] == 'a' && p[2] == 'f'
         && p[3] == ':' && p[4] == '/' && p[5] == '/'
         && p[6] != '\0');
}
#endif

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
#ifdef HAVE_SMBCLIENT
      if (stream->scheme == VFS_SCHEME_SMB)
         return retro_vfs_file_seek_smb(stream, offset, whence);
#endif
      return retro_vfs_fp_seek64(stream->fp, offset, whence);
   }
#ifdef VFS_HAVE_FILE_MAPPING
   /* Need to check stream->mapped because this function is
    * called in filestream_open() */
   if (stream->mapped && (stream->hints &
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS))
   {
      /* fseek() returns error on under/overflow but
       * allows cursor > EOF for
       read-only file descriptors. */
      /* The file position must never be negative. */
      switch (whence)
      {
         case SEEK_SET:
            if (offset < 0)
               return -1;

            stream->mappos = offset;
            break;

         case SEEK_CUR:
            if ((int64_t)stream->mappos + offset < 0)
              return -1;

            stream->mappos += offset;
            break;

         case SEEK_END:
            /* RETRO_VFS_SEEK_POSITION_END states offset should be negative.
             * However, this is impractical because we would be forcing the
             * end of file to always be off by one.
             */
            if (offset > 0 || (int64_t)stream->mapsize + offset < 0)
               return -1;

            stream->mappos = stream->mapsize + offset;
            break;
      }
      return 0;
   }
#endif

   return retro_vfs_fd_seek64(stream->fd, offset, whence) == -1 ? -1 : 0;
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
      calloc(1, sizeof(*stream));

   if (!stream)
      return NULL;

   stream->fd                     = -1;
   stream->hints                  = hints;
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
   if (path_is_cdrom(path))
   {
      /* The sector cache and cue sheet state only exist for cdrom://
       * handles, so they are allocated here rather than carried by
       * every open file. Zeroed, because the old inline member was
       * zeroed by the calloc() above and the cdrom paths rely on
       * that (cue_buf NULL, last_frame_valid false, cur_track 0). */
      if (!(stream->cdrom = (vfs_cdrom_t*)calloc(1, sizeof(*stream->cdrom))))
      {
         free(stream);
         return NULL;
      }
      path             += sizeof("cdrom://")-1;
      stream->scheme    = VFS_SCHEME_CDROM;
   }
#endif

#ifdef HAVE_SMBCLIENT
   if (path_is_smb(path))
   {
      stream->scheme    = VFS_SCHEME_SMB;
   }
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
   if (path_is_saf(path))
   {
      stream->scheme    = VFS_SCHEME_SAF;
   }
#endif

   if (path)
      stream->orig_path = strdup(path);

#ifdef VFS_HAVE_FILE_MAPPING
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS && mode == RETRO_VFS_FILE_ACCESS_READ)
      stream->hints |= RFILE_HINT_UNBUFFERED;
   else
#endif
      stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;

#ifdef VFS_HAVE_DESCRIPTOR_IO
   /* A read-once-and-close stream wants the descriptor path for the
    * opposite reason a mapped one does: not to keep the bytes around,
    * but to avoid a buffer it will never reuse.  Deliberately after
    * the block above and never touching the mapping hint, so a
    * stream asking for both still maps and behaves exactly as before.
    *
    * Restricted to VFS_SCHEME_NONE: the descriptor branch below knows
    * about plain files and (on Android) SAF, and has no case for
    * cdrom:// or smb://, which have their own read/seek entry points
    * that only the buffered branch dispatches to. */
   if (     (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK)
         && mode == RETRO_VFS_FILE_ACCESS_READ
         && stream->scheme == VFS_SCHEME_NONE)
      stream->hints |= RFILE_HINT_UNBUFFERED;
#ifdef VFS_HAVE_DESCRIPTOR_WRITE
   /* A write-once stream is the same shape: every byte it will ever
    * write arrives in one call, so a stdio buffer only adds a copy
    * and a flush. */
   if (     (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK)
         && (     mode == RETRO_VFS_FILE_ACCESS_WRITE
               || mode == RETRO_VFS_FILE_ACCESS_READ_WRITE)
         && stream->scheme == VFS_SCHEME_NONE)
      stream->hints |= RFILE_HINT_UNBUFFERED;
#endif
#endif

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
#if defined(_WIN32)
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         mode_str = "w+b";
         flags    = O_RDWR | O_CREAT | O_TRUNC;
#if defined(_WIN32)
         flags   |= O_BINARY;
#endif
         break;

      case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
      case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
         mode_str = "r+b";

         flags    = O_RDWR;
#if defined(_WIN32)
         flags   |= O_BINARY;
#endif
         break;

      default:
         goto error;
   }

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      FILE *fp;
      switch (stream->scheme)
      {
#ifdef HAVE_CDROM
         case VFS_SCHEME_CDROM:
            retro_vfs_file_open_cdrom(stream, path, mode, hints);
#if defined(_WIN32) && !defined(_XBOX)
            if (!stream->fh)
               goto error;
#elif defined(__APPLE__)
            if (!stream->iokit_mmc)
               goto error;
#else
            if (!stream->fp)
               goto error;
#endif
            break;
#endif

#ifdef HAVE_SMBCLIENT
         case VFS_SCHEME_SMB:
            if (!retro_vfs_file_open_smb(stream, path, mode, hints))
               goto error;
            break;
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
         case VFS_SCHEME_SAF:
            {
               struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
               int fd;
               if (!retro_vfs_path_split_saf(&saf_split_result, path))
                  goto error;
               fd = retro_vfs_file_open_saf(saf_split_result.tree, saf_split_result.path, mode);
               free(saf_split_result.path);
               free(saf_split_result.tree);
               if (fd == -1)
                  goto error;
               stream->fp = fdopen(fd, mode_str);
               if (!stream->fp)
               {
                  close(fd);
                  goto error;
               }
            }
            break;
#endif

         default:
            if (!(fp = (FILE*)fopen_utf8(path, mode_str)))
            {
#ifdef IOS
               if (errno == EEXIST)
               {
                  retro_vfs_file_remove_impl(path);
                  fp = (FILE*)fopen_utf8(path, mode_str);
               }
               if (!fp)
#endif
               goto error;
            }
            stream->fp  = fp;
            break;
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
      /* stdio sizes a file stream's buffer from st_blksize, which the
       * filesystem reports and which is 4 KiB on every platform
       * checked.  Callers here write in much smaller pieces than that
       * - a JSON writer flushing a kilobyte at a time, for instance -
       * so that default decides how many syscalls a file costs.
       *
       * Measured writing 8 MiB in 1 KiB pieces, default against
       * 64 KiB:
       *
       *   desktop Linux   14.02 ms -> 3.05 ms   reads 1.68 -> 0.75 ms
       *   macOS            9.35 ms -> 1.58 ms   reads 1.59 -> 0.80 ms
       *
       * Sixteen times fewer syscalls for the same bytes, and the
       * dearer a syscall is the more that is worth - which is why the
       * platforms with the slowest storage are the ones that already
       * did this, and with the largest buffers.
       *
       * This is deliberately not gated on a platform list.  Such a
       * list has to be maintained, is wrong the moment a target is
       * added, and would leave that target silently on 4 KiB forever -
       * which is exactly the state everything but the two consoles
       * below was in.  The rest of this codebase does not gate it
       * either: config_file.c and verbosity.c both buffer
       * unconditionally.
       *
       * The memory is one allocation per open file, released when the
       * file closes, and a handful of files are open at once.  Where
       * even that is too much the allocation simply fails and the C
       * library default is used, so a platform under real pressure
       * degrades rather than breaks.  A platform wanting a different
       * size says so, as the two below do.
       *
       * It is malloc rather than calloc because nothing ever reads
       * these bytes before stdio writes them - the buffer is handed
       * straight to setvbuf and otherwise only freed, and the WiiU
       * path below has always used non-zeroing memalign.  Zeroing it
       * was the single dearest part of opening a small file: 3000
       * opens of 256-byte files measured 2.9-3.1 us each with the
       * zeroing and 1.8 us without, so scan-shaped and thumbnail-
       * shaped workloads - thousands of opens, few bytes each - spent
       * more time clearing buffers than reading files. */
#if defined(_3DS)
      if (stream->scheme != VFS_SCHEME_CDROM)
      {
         stream->buf = (char*)malloc(0x10000);
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
      }
#else
      if (stream->scheme != VFS_SCHEME_CDROM)
      {
         const int bufsize = 64 * 1024;
         /* NULL rather than a buffer of our own, so the C library
          * allocates and owns it.
          *
          * Who owns the buffer is not a detail on every library.
          * Apple's fread() has a fast path that reads a large request
          * straight into the caller's buffer instead of copying it
          * through the stream, and gates it on __SMBF - "this buffer
          * is mine" - which setvbuf() clears when handed a buffer
          * from outside (Libc stdio/FreeBSD/setvbuf.c sets __SMBF
          * only on the branch that allocates, and fread.c tests it;
          * see also radars 5980080 and 6180417).  So the 64 KiB
          * buffer added here for write throughput was disqualifying
          * the read fast path on every Apple target, and measured at
          * roughly half the whole-file read speed on macOS.
          *
          * The size argument still applies, so writes keep the buffer
          * they were given.  A library that will not allocate for a
          * NULL buffer leaves the stream on its own default, which is
          * slower for small writes but correct - the same outcome the
          * malloc failing used to produce. */
         if (stream->fp)
            setvbuf(stream->fp, NULL, _IOFBF, bufsize);
      }
#endif
   }
   else
   {
      switch (stream->scheme)
      {
#if defined(ANDROID) && defined(HAVE_SAF)
         case VFS_SCHEME_SAF:
            {
               struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
               if (!retro_vfs_path_split_saf(&saf_split_result, path))
                  goto error;
               stream->fd = retro_vfs_file_open_saf(saf_split_result.tree, saf_split_result.path, mode);
               free(saf_split_result.path);
               free(saf_split_result.tree);
            }
            break;
#endif

         default:
            {
#if defined(_WIN32) && !defined(_XBOX)
#if defined(LEGACY_WIN32_RUNTIME)
               if (win32_needs_local_encoding())
               {
                  char *path_local    = utf8_to_local_string_alloc(path);
                  stream->fd          = open(path_local, flags, 0);
                  if (path_local)
                     free(path_local);
               }
               else
               {
                  wchar_t * path_wide = utf8_to_utf16_string_alloc(path);
                  stream->fd          = _wopen(path_wide, flags, 0);
                  if (path_wide)
                     free(path_wide);
               }
#elif defined(LEGACY_WIN32)
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
#elif defined(VITA)
               {
                  int sce_flags = 0;
                  SceUID uid;

                  /* The POSIX flags above are for the open() targets. */
                  (void)flags;

                  switch (mode)
                  {
                     case RETRO_VFS_FILE_ACCESS_READ:
                        sce_flags = SCE_O_RDONLY;
                        break;
                     case RETRO_VFS_FILE_ACCESS_WRITE:
                        sce_flags = SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC;
                        break;
                     case RETRO_VFS_FILE_ACCESS_READ_WRITE:
                        sce_flags = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC;
                        break;
                     default:
                        sce_flags = SCE_O_RDWR;
                        break;
                  }

                  /* SceUID error codes are negative; the descriptor
                   * path only ever tests for -1. */
                  uid        = sceIoOpen(path, sce_flags, 0777);
                  stream->fd = (uid < 0) ? -1 : (int)uid;
               }
#else
               stream->fd          = open(path, flags, S_IRUSR | S_IWUSR);
#endif
            }
            break;
      }

      if (stream->fd == -1)
         goto error;

#ifdef VFS_HAVE_FILE_MAPPING
      if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      {
         stream->mappos  = 0;
         stream->mapped  = NULL;

         retro_vfs_file_seek_internal(stream, 0, SEEK_END);

         stream->mapsize = retro_vfs_file_tell_impl(stream);
         if (stream->mapsize == (uint64_t)-1)
            goto error;

         retro_vfs_file_seek_internal(stream, 0, SEEK_SET);

#if defined(HAVE_MMAP)
         /* mmap() takes a size_t.  On a 32-bit host a file larger than
          * the address space would truncate to its low bits on the way
          * in - and a truncated length can still map successfully,
          * leaving a short mapping while mapsize goes on claiming the
          * whole file, so every read past the cut would fault instead
          * of coming up short.  Refuse the mapping and let the
          * descriptor path below serve the file, exactly as a failed
          * mmap() does.  Folds away where size_t is 64 bits. */
         if (stream->mapsize > (uint64_t)(size_t)-1)
         {
            stream->mapped = NULL;
            stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
         }
         else if ((stream->mapped = (uint8_t*)mmap((void*)0,
               (size_t)stream->mapsize, PROT_READ,  MAP_SHARED, stream->fd, 0)) == MAP_FAILED)
         {
            stream->mapped = NULL;
            stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
         }
#else
         {
            /* Win32 file mapping.  A zero-length file cannot be mapped
             * (CreateFileMapping rejects it), and a failed mapping of
             * any kind - including a >4GB file on a 32-bit process,
             * where MapViewOfFile cannot find address space - degrades
             * to the ordinary descriptor path by dropping the hint,
             * exactly as the POSIX branch does. */
            HANDLE fh = (HANDLE)_get_osfhandle(stream->fd);
            stream->map_handle = NULL;
            if (fh != INVALID_HANDLE_VALUE && stream->mapsize > 0)
               stream->map_handle = CreateFileMapping(fh, NULL,
                     PAGE_READONLY, 0, 0, NULL);
            if (stream->map_handle)
            {
               stream->mapped = (uint8_t*)MapViewOfFile(stream->map_handle,
                     FILE_MAP_READ, 0, 0, 0);
               if (!stream->mapped)
               {
                  CloseHandle(stream->map_handle);
                  stream->map_handle = NULL;
               }
            }
            if (!stream->mapped)
               stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;
         }
#endif
      }
#endif
   }
#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
   {
      retro_vfs_file_seek_cdrom(stream, 0, SEEK_END);

      stream->size = retro_vfs_file_tell_impl(stream);

      retro_vfs_file_seek_cdrom(stream, 0, SEEK_SET);
   }
   else
#endif
   /* A truncating open leaves a zero-length file positioned at 0,
    * so the probe below - a seek to the end, a tell and a seek back,
    * each a system call - would only ever confirm that. */
   if (     mode != RETRO_VFS_FILE_ACCESS_WRITE
         && mode != RETRO_VFS_FILE_ACCESS_READ_WRITE)
   {
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

#ifdef HAVE_SMBCLIENT
   if (stream->scheme == VFS_SCHEME_SMB)
   {
      retro_vfs_file_close_smb(stream);
      goto smbend;
   }
#endif

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
      if (stream->fp)
         fclose(stream->fp);
   }
   else
   {
#ifdef VFS_HAVE_FILE_MAPPING
      if (stream->mapped && (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS))
      {
#if defined(HAVE_MMAP)
         munmap(stream->mapped, stream->mapsize);
#else
         UnmapViewOfFile(stream->mapped);
         if (stream->map_handle)
         {
            CloseHandle(stream->map_handle);
            stream->map_handle = NULL;
         }
#endif
      }
#endif
   }

   if (stream->fd >= 0)
   {
#if defined(VITA)
      sceIoClose((SceUID)stream->fd);
#else
      close(stream->fd);
#endif
   }
#ifdef HAVE_CDROM
end:
   /* Reached both by the goto above and by fall-through from the
    * non-cdrom path, where stream->cdrom is NULL. */
   if (stream->cdrom)
   {
      if (stream->cdrom->cue_buf)
         free(stream->cdrom->cue_buf);
      free(stream->cdrom);
      stream->cdrom = NULL;
   }
#endif
#ifdef HAVE_SMBCLIENT
smbend:
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
   if (!stream)
      return -1;
#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
      return retro_vfs_file_error_cdrom(stream);
#endif
#ifdef HAVE_SMBCLIENT
    if (stream->scheme == VFS_SCHEME_SMB)
        return retro_vfs_file_error_smb(stream);
#endif
   if (!stream->fp)
      return -1;
   return ferror(stream->fp);
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (stream)
      return stream->size;
   return 0;
}

/* Deallocate a range without changing the file's length. See
 * filestream_punch_hole for why this is a capability rather than a
 * guarantee: most backends cannot do it, and callers fall back to writing
 * zeroes. Returns 0 on success, -1 otherwise. */
#if defined(_WIN32) && !defined(_XBOX)
/* The pieces of winioctl.h that retro_vfs_file_punch_hole_impl needs,
 * declared here rather than by including that header.
 *
 * winioctl.h defines the storage class GUIDs, and in a unity build --
 * griffin.c, where this file is compiled alongside the D3D headers that
 * set INITGUID -- those definitions collide with the ones already emitted
 * in the same translation unit, which is twenty-odd C2374 "redefinition;
 * multiple initialization" errors on MSVC. Nothing here needs a GUID.
 *
 * The two control codes are the documented CTL_CODE expansions:
 *   FSCTL_SET_SPARSE    = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
 *   FSCTL_SET_ZERO_DATA = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA)
 * Each is guarded, so a translation unit that has already seen
 * winioctl.h through some other path keeps that header's definitions. */
#ifndef FSCTL_SET_SPARSE
#define FSCTL_SET_SPARSE 0x000900c4
#endif
#ifndef FSCTL_SET_ZERO_DATA
#define FSCTL_SET_ZERO_DATA 0x000980c8
#endif
#ifndef FILE_ZERO_DATA_INFORMATION_DEFINED
#define FILE_ZERO_DATA_INFORMATION_DEFINED
typedef struct _RETRO_FILE_ZERO_DATA_INFORMATION
{
   LARGE_INTEGER FileOffset;
   LARGE_INTEGER BeyondFinalZero;
} RETRO_FILE_ZERO_DATA_INFORMATION;
#endif
#endif

#if defined(_WIN32) && !defined(_XBOX)
/* FILE_NAME_NORMALIZED came in with the Vista SDK, the same generation as
 * GetFinalPathNameByHandleW itself.  The function is resolved by name at
 * run time below precisely so that older toolchains still build this
 * file; the flag has to clear the same bar, so supply the documented
 * value when the SDK does not. */
#ifndef FILE_NAME_NORMALIZED
#define FILE_NAME_NORMALIZED 0x0
#endif
#endif

/* Allocation unit of the filesystem holding this file: the smallest span
 * retro_vfs_file_punch_hole_impl can actually free. 0 when unknown.
 *
 * This exists so a caller never has to reach for the descriptor itself.
 * On NTFS this is the compression unit rather than the cluster, because
 * that is what the filesystem actually deallocates in.
 *
 * PCSX2's ATA backend derived both the cluster size and the filesystem
 * name by calling
 * GetFinalPathNameByHandle on a HANDLE it pulled out of a FILE*, which is
 * the one thing that kept it off the VFS -- the question is about the
 * filesystem, not the file, and the backend that owns the handle is the
 * right place to answer it. */
int64_t retro_vfs_file_get_sparse_granularity_impl(
      libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return 0;

/* Excludes __WINRT__ for the same reason features_cpu.c does: the probe
 * below reaches kernel32 through GetModuleHandle, and that entry point is
 * not in the Windows Store API partition -- dylib_proc() says as much and
 * refuses to call it there.  A UWP build reports "unknown" and skips
 * sparse handling, which is already the documented behaviour for any
 * system that cannot resolve these names. */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   {
      /* GetFinalPathNameByHandleW and GetVolumeInformationByHandleW are
       * Vista and later. Importing them statically costs more than the
       * feature is worth: the loader resolves imports before main, so a
       * binary that names them will not start at all on 9x or XP, and
       * mingw does not even declare them below _WIN32_WINNT 0x0600, which
       * is what the i686 MXE build targets. Resolved by name instead, so
       * older systems simply report "unknown" and skip sparse handling. */
      typedef DWORD (WINAPI *final_path_t)(HANDLE, LPWSTR, DWORD, DWORD);
      typedef BOOL  (WINAPI *vol_info_t)(HANDLE, LPWSTR, DWORD, LPDWORD,
            LPDWORD, LPDWORD, LPWSTR, DWORD);
      typedef BOOL  (WINAPI *vol_path_t)(LPCWSTR, LPWSTR, DWORD);
      typedef BOOL  (WINAPI *disk_free_t)(LPCWSTR, LPDWORD, LPDWORD,
            LPDWORD, LPDWORD);

      static final_path_t p_final_path = NULL;
      static vol_info_t   p_vol_info   = NULL;
      static vol_path_t   p_vol_path   = NULL;
      static disk_free_t  p_disk_free  = NULL;
      static int          resolved     = 0;

      DWORD    sectors_per_cluster = 0;
      DWORD    bytes_per_sector    = 0;
      DWORD    tmp1                = 0;
      DWORD    tmp2                = 0;
      DWORD    len;
      HANDLE   handle              = stream->fh;
      wchar_t *final_path;
      wchar_t  root[MAX_PATH + 1];
      wchar_t  fs_name[MAX_PATH + 1];
      int64_t  cluster;

      if (!resolved)
      {
         HMODULE k32 = GetModuleHandleA("kernel32.dll");

         resolved = 1;
         if (k32)
         {
            p_final_path = (final_path_t)GetProcAddress(k32,
                  "GetFinalPathNameByHandleW");
            p_vol_info   = (vol_info_t)GetProcAddress(k32,
                  "GetVolumeInformationByHandleW");
            /* Wide GetVolumePathName is 2000 and later, and 9x does not
             * export it at all -- a static import would stop the binary
             * loading there, so every one of these is resolved by name. */
            p_vol_path   = (vol_path_t)GetProcAddress(k32,
                  "GetVolumePathNameW");
            p_disk_free  = (disk_free_t)GetProcAddress(k32,
                  "GetDiskFreeSpaceW");
         }
      }

      if (!p_final_path || !p_vol_path || !p_disk_free)
         return 0;

      if (!handle || handle == INVALID_HANDLE_VALUE)
      {
         if (!stream->fp)
            return 0;
         handle = (HANDLE)_get_osfhandle(_fileno(stream->fp));
         if (handle == INVALID_HANDLE_VALUE)
            return 0;
      }

      /* The volume that matters is the one the file really lives on, so
       * the path has to be resolved through any junctions first. */
      len = p_final_path(handle, NULL, 0, FILE_NAME_NORMALIZED);
      if (len == 0)
         return 0;

      final_path = (wchar_t*)calloc(len + 1, sizeof(wchar_t));
      if (!final_path)
         return 0;

      if (p_final_path(handle, final_path, len, FILE_NAME_NORMALIZED) == 0)
      {
         free(final_path);
         return 0;
      }

      /* GetVolumePathNameW gives the mount point, which is what
       * GetDiskFreeSpaceW wants, and unlike splitting the string by hand
       * it copes with a path mounted on a folder rather than a letter. */
      if (!p_vol_path(final_path, root, MAX_PATH))
      {
         free(final_path);
         return 0;
      }
      free(final_path);

      if (!p_disk_free(root, &sectors_per_cluster, &bytes_per_sector,
               &tmp1, &tmp2))
         return 0;

      cluster = (int64_t)sectors_per_cluster * (int64_t)bytes_per_sector;

      /* On NTFS the cluster size is not the answer. A sparse file is
       * deallocated in compression units, which are 16 clusters, capped
       * at 64K -- so a 4K-cluster volume, the common case, frees in 64K
       * chunks and punching a single 4K cluster frees nothing at all.
       * Other filesystems deallocate by cluster, so the cluster size
       * stands. Without the volume query, assume the conservative case. */
      if (!p_vol_info)
         return cluster;

      if (p_vol_info(handle, NULL, 0, NULL, NULL, NULL, fs_name, MAX_PATH)
            && !wcscmp(fs_name, L"NTFS"))
      {
         const int64_t unit = cluster * 16;
         return (unit > 65536) ? 65536 : unit;
      }

      return cluster;
   }
#elif !defined(_WIN32) && !defined(VITA) && !defined(PSP) && !defined(PS2) && !defined(ORBIS) && !defined(GEKKO) && !defined(_3DS)
   {
      struct stat st;
      int         fd = stream->fd;

      if (fd < 0)
      {
         if (!stream->fp)
            return 0;
         fd = fileno(stream->fp);
         if (fd < 0)
            return 0;
      }

      if (fstat(fd, &st) != 0)
         return 0;
      if (st.st_blksize <= 0)
         return 0;

      return (int64_t)st.st_blksize;
   }
#else
   return 0;
#endif
}

int retro_vfs_file_punch_hole_impl(libretro_vfs_implementation_file *stream,
      int64_t offset, int64_t len)
{
   if (!stream || offset < 0 || len <= 0)
      return -1;

#if defined(_WIN32) && !defined(_XBOX)
   {
      RETRO_FILE_ZERO_DATA_INFORMATION zero_info;
      DWORD                      returned = 0;
      HANDLE                     handle   = stream->fh;

      if (!handle || handle == INVALID_HANDLE_VALUE)
      {
         if (!stream->fp)
            return -1;
         handle = (HANDLE)_get_osfhandle(_fileno(stream->fp));
         if (handle == INVALID_HANDLE_VALUE)
            return -1;
      }

      /* The file has to be marked sparse before a zero-data range does
       * anything; on a non-sparse file the call succeeds and writes real
       * zeroes, which is correct but saves nothing. Marking is idempotent. */
      {
         DWORD tmp = 0;
         DeviceIoControl(handle, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &tmp, NULL);
      }

      zero_info.FileOffset.QuadPart      = offset;
      zero_info.BeyondFinalZero.QuadPart = offset + len;

      if (!DeviceIoControl(handle, FSCTL_SET_ZERO_DATA, &zero_info,
               sizeof(zero_info), NULL, 0, &returned, NULL))
         return -1;
      return 0;
   }
#elif defined(__linux__) && defined(FALLOC_FL_PUNCH_HOLE)
   {
      int fd = stream->fd;

      if (fd < 0)
      {
         if (!stream->fp)
            return -1;
         fd = fileno(stream->fp);
         if (fd < 0)
            return -1;
      }

      if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
               (off_t)offset, (off_t)len) != 0)
         return -1;
      return 0;
   }
#else
   /* No hole punching on this platform. */
   (void)offset;
   (void)len;
   return -1;
#endif
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t len)
{
#ifdef _WIN32
   /* _chsize takes a long and silently truncates lengths > LONG_MAX
    * (2 GiB on Windows) -- present on all Windows CRTs including
    * VC6.  _chsize_s takes __int64 and was added in the Secure CRT
    * (VS 2005, _MSC_VER 1400).  Prefer the 64-bit variant when
    * available, and on older MSVC / MinGW with legacy msvcrt fall
    * back to _chsize only for lengths that fit in long -- return
    * an error for larger lengths rather than silently truncating
    * the file. */
#if defined(_MSC_VER) && _MSC_VER >= 1400
   if (stream && stream->fp && _chsize_s(_fileno(stream->fp), len) == 0)
   {
	   stream->size = len;
	   return 0;
   }
#else
   if (stream && stream->fp && len >= 0 && len <= (int64_t)LONG_MAX
         && _chsize(_fileno(stream->fp), (long)len) == 0)
   {
	   stream->size = len;
	   return 0;
   }
#endif
#elif !defined(VITA) && !defined(PSP) && !defined(PS2) && !defined(ORBIS) && (!defined(SWITCH) || defined(HAVE_LIBNX))
   if (stream && stream->fp && ftruncate(fileno(stream->fp), (off_t)len) == 0)
   {
      stream->size = len;
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
#ifdef HAVE_SMBCLIENT
      if (stream->scheme == VFS_SCHEME_SMB)
         return retro_vfs_file_tell_smb(stream);
#endif
      return retro_vfs_fp_tell64(stream->fp);
   }
#ifdef VFS_HAVE_FILE_MAPPING
   /* Need to check stream->mapped because this function
    * is called in filestream_open() */
   if (stream->mapped && (stream->hints &
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS))
      return stream->mappos;
#endif
   return retro_vfs_fd_seek64(stream->fd, 0, SEEK_CUR);
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

   /* A length with the top bit set is a negative int64 that arrived
    * through the unsigned parameter - no legitimate caller reads 8EiB.
    * The stdio path below hands len to fread() unclamped, where bionic
    * FORTIFY turns it into a process abort (observed in the field);
    * fail the read instead, like the mapped path's clamp below already
    * does for its overflow case. */
   if (len > (uint64_t)INT64_MAX)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_CDROM
      if (stream->scheme == VFS_SCHEME_CDROM)
         return retro_vfs_file_read_cdrom(stream, s, len);
#endif
#ifdef HAVE_SMBCLIENT
      if (stream->scheme == VFS_SCHEME_SMB)
         return retro_vfs_file_read_smb(stream, s, len);
#endif
      /* bionic's stdio dispatches fread through the FILE's int-typed
       * read callback: a single count above INT_MAX truncates to a
       * negative int inside libc, __sread sign-extends it back into
       * read(), and FORTIFY aborts the process ("read: count ... >
       * SSIZE_MAX").  Chunk the transfer so no single libc call sees
       * a count any stdio cannot take. */
      {
         uint8_t *p     = (uint8_t*)s;
         uint64_t total = 0;
         while (len != 0)
         {
            size_t _chunk = (len > VFS_STDIO_IO_CHUNK_MAX)
                  ? (size_t)VFS_STDIO_IO_CHUNK_MAX : (size_t)len;
            size_t _got   = fread(p, 1, _chunk, stream->fp);
            total        += _got;
            if (_got < _chunk)
               break;
            p            += _got;
            len          -= _got;
         }
         return (int64_t)total;
      }
   }
#ifdef VFS_HAVE_FILE_MAPPING
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
   {
      if (stream->mappos >= stream->mapsize)
      {
         /* At or past EOF: 0 bytes is the correct return for
          * fread-style semantics on a legitimate read that reached
          * EOF, -1 if we were already past EOF (corrupt state). */
         if (stream->mappos == stream->mapsize)
            return 0;
         return -1;
      }

      /* Clamp len against the remaining mapped bytes.  Done as an
       * unsigned subtraction *before* computing mappos+len to avoid
       * integer overflow: mappos+len can wrap past mapsize when
       * both operands are large uint64_t values, defeating the
       * naive "mappos + len > mapsize" bound check. */
      {
         uint64_t remaining = stream->mapsize - stream->mappos;
         if (len > remaining)
            len = remaining;
      }

      memcpy(s, &stream->mapped[stream->mappos], (size_t)len);
      stream->mappos += len;

      return (int64_t)len;
   }
#endif

   /* One read() moves at most MAX_RW_COUNT (INT_MAX & PAGE_MASK) on
    * Linux and returns short past it, so a large request needs the
    * same chunk loop as the stdio path to honor the full count. */
   {
      uint8_t *p     = (uint8_t*)s;
      uint64_t total = 0;
      while (len != 0)
      {
         size_t  _chunk = (len > VFS_STDIO_IO_CHUNK_MAX)
               ? (size_t)VFS_STDIO_IO_CHUNK_MAX : (size_t)len;
#if defined(VITA)
         ssize_t _got   = (ssize_t)sceIoRead((SceUID)stream->fd, p, (SceSize)_chunk);
#else
         ssize_t _got   = read(stream->fd, p, _chunk);
#endif
         if (_got < 0)
            return (total != 0) ? (int64_t)total : -1;
         total += (uint64_t)_got;
         if ((size_t)_got < _chunk)
            break;
         p     += _got;
         len   -= (uint64_t)_got;
      }
      return (int64_t)total;
   }
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
   int64_t pos = 0;
   ssize_t ret = -1;

   if (!stream)
      return -1;

   if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
   {
#ifdef HAVE_SMBCLIENT
      if (stream->scheme == VFS_SCHEME_SMB)
      {
         pos = retro_vfs_file_tell_smb(stream);
         ret = retro_vfs_file_write_smb(stream, s, len);
         if (ret != -1 && pos + ret > stream->size)
            stream->size = pos + ret;
         return ret;
      }
#endif
      pos = retro_vfs_file_tell_impl(stream);
      /* Same int-typed stdio callback contract as the read side:
       * chunk so no single fwrite count exceeds what bionic takes. */
      {
         const uint8_t *p = (const uint8_t*)s;
         uint64_t total   = 0;
         while (len != 0)
         {
            size_t _chunk = (len > VFS_STDIO_IO_CHUNK_MAX)
                  ? (size_t)VFS_STDIO_IO_CHUNK_MAX : (size_t)len;
            size_t _put   = fwrite(p, 1, _chunk, stream->fp);
            total        += _put;
            if (_put < _chunk)
               break;
            p            += _put;
            len          -= _put;
         }
         ret = (ssize_t)total;
      }

      if (ret > 0 && pos + ret > stream->size)
         stream->size = pos + ret;

      return ret;
   }
#ifdef VFS_HAVE_FILE_MAPPING
   if (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS)
      return -1;
#endif

   pos = retro_vfs_file_tell_impl(stream);
   /* write() shares read()'s MAX_RW_COUNT clamp; loop for the full
    * count. */
   {
      const uint8_t *p = (const uint8_t*)s;
      uint64_t total   = 0;
      while (len != 0)
      {
         size_t  _chunk = (len > VFS_STDIO_IO_CHUNK_MAX)
               ? (size_t)VFS_STDIO_IO_CHUNK_MAX : (size_t)len;
#if defined(VITA)
         ssize_t _put   = (ssize_t)sceIoWrite((SceUID)stream->fd, p, (SceSize)_chunk);
#else
         ssize_t _put   = write(stream->fd, p, _chunk);
#endif
         if (_put < 0)
         {
            if (total == 0)
               return -1;
            break;
         }
         total += (uint64_t)_put;
         if ((size_t)_put < _chunk)
            break;
         p     += _put;
         len   -= (uint64_t)_put;
      }
      ret = (ssize_t)total;
   }

   if (ret != -1 && pos + ret > stream->size)
      stream->size = pos + ret;

   return ret;
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream)
      return -1;
#ifdef HAVE_CDROM
   if (stream->scheme == VFS_SCHEME_CDROM)
      return 0;
#endif
#ifdef HAVE_SMBCLIENT
   if (stream->scheme == VFS_SCHEME_SMB)
      return 0;
#endif
   if (stream->fp && fflush(stream->fp) == 0)
      return 0;
   return -1;
}

int retro_vfs_file_remove_impl(const char *path)
{
   if (path && *path)
   {
      int ret          = -1;

#if defined(ANDROID) && defined(HAVE_SAF)
      if (path_is_saf(path))
      {
         struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
         if (!retro_vfs_path_split_saf(&saf_split_result, path))
            return -1;
         ret = retro_vfs_file_remove_saf(saf_split_result.tree, saf_split_result.path);
         free(saf_split_result.path);
         free(saf_split_result.tree);
         return ret;
      }
#endif

#if defined(_WIN32) && !defined(_XBOX)
      /* Win32 (no Xbox) */
#if defined(LEGACY_WIN32_RUNTIME)
      if (win32_needs_local_encoding())
      {
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
      }
      else
      {
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
      }
#elif defined(LEGACY_WIN32)
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
#if defined(ANDROID) && defined(HAVE_SAF)
      if (path_is_saf(old_path) && path_is_saf(new_path))
      {
         int ret;
         struct libretro_vfs_implementation_saf_path_split_result saf_split_result_old, saf_split_result_new;
         if (!retro_vfs_path_split_saf(&saf_split_result_old, old_path))
            return -1;
         if (!retro_vfs_path_split_saf(&saf_split_result_new, new_path))
         {
            free(saf_split_result_old.path);
            free(saf_split_result_old.tree);
            return -1;
         }
         ret = retro_vfs_file_rename_saf(saf_split_result_old.tree, saf_split_result_old.path, saf_split_result_new.tree, saf_split_result_new.path);
         free(saf_split_result_new.path);
         free(saf_split_result_new.tree);
         free(saf_split_result_old.path);
         free(saf_split_result_old.tree);
         return ret;
      }
#endif

#if defined(_WIN32) && !defined(_XBOX)
   /* Win32 (no Xbox) */
   int ret                 = -1;

   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;

   /* Neither rename() nor _wrename() can replace an existing
    * destination on Windows, which breaks writing to a temporary and
    * renaming it over the original.  MoveFileExW with
    * MOVEFILE_REPLACE_EXISTING gives the POSIX overwrite behaviour.
    * It is unsupported on 9x, so the local-encoding path replaces by
    * removing the destination, and only after a plain rename fails. */
#if defined(LEGACY_WIN32_RUNTIME)
   if (win32_needs_local_encoding())
   {
      char *old_path_local = utf8_to_local_string_alloc(old_path);

      if (old_path_local)
      {
         char *new_path_local = utf8_to_local_string_alloc(new_path);

         if (new_path_local)
         {
            if (rename(old_path_local, new_path_local) == 0)
               ret = 0;
            else if (remove(new_path_local) == 0 &&
                  rename(old_path_local, new_path_local) == 0)
               ret = 0;
            free(new_path_local);
         }

         free(old_path_local);
      }
   }
   else
   {
      wchar_t *old_path_wide = utf8_to_utf16_string_alloc(old_path);

      if (old_path_wide)
      {
         wchar_t *new_path_wide = utf8_to_utf16_string_alloc(new_path);

         if (new_path_wide)
         {
            if (MoveFileExW(old_path_wide, new_path_wide,
                  MOVEFILE_REPLACE_EXISTING))
               ret = 0;
            free(new_path_wide);
         }

         free(old_path_wide);
      }
   }
#elif defined(LEGACY_WIN32)
   {
      char *old_path_local = utf8_to_local_string_alloc(old_path);

      if (old_path_local)
      {
         char *new_path_local = utf8_to_local_string_alloc(new_path);

         if (new_path_local)
         {
            if (rename(old_path_local, new_path_local) == 0)
               ret = 0;
            else if (remove(new_path_local) == 0 &&
                  rename(old_path_local, new_path_local) == 0)
               ret = 0;
            free(new_path_local);
         }

         free(old_path_local);
      }
   }
#else
   {
      wchar_t *old_path_wide = utf8_to_utf16_string_alloc(old_path);

      if (old_path_wide)
      {
         wchar_t *new_path_wide = utf8_to_utf16_string_alloc(new_path);

         if (new_path_wide)
         {
            if (MoveFileExW(old_path_wide, new_path_wide,
                  MOVEFILE_REPLACE_EXISTING))
               ret = 0;
            free(new_path_wide);
         }

         free(old_path_wide);
      }
   }
#endif
   return ret;

#elif defined(VITA)
   /* rename() here means "replace": the Win32 branch above says so
    * with MOVEFILE_REPLACE_EXISTING and POSIX says so by definition,
    * and the write-to-temporary-then-rename pattern - playlists, the
    * config file, core info - is built on it.  The kernel's own
    * sceIoRename() refuses an existing destination, and newlib's
    * rename() bridges that gap by sceIoRemove()ing the destination
    * first and renaming after.  Between those two calls nothing is
    * on disk, and if the rename then fails the caller's temporary
    * is discarded on top: the file that was being replaced is
    * simply gone.
    *
    * So call the kernel directly and, when the destination is in the
    * way, move it aside rather than delete it.  The old file is only
    * removed once its replacement is in place, and is put back if
    * the replacement cannot be. */
   {
      SceIoStat st;
      size_t _len;
      char *aside;
      int ret;

      if (!old_path || !*old_path || !new_path || !*new_path)
         return -1;

      if (sceIoRename(old_path, new_path) >= 0)
         return 0;

      /* Only worth trying when there is a destination to move aside. */
      if (sceIoGetstat(new_path, &st) < 0)
         return -1;

      _len  = strlen(new_path);
      if (!(aside = (char*)malloc(_len + sizeof(".old"))))
         return -1;
      memcpy(aside, new_path, _len);
      memcpy(aside + _len, ".old", sizeof(".old"));

      ret = -1;
      sceIoRemove(aside);              /* a leftover from an earlier run */
      if (sceIoRename(new_path, aside) >= 0)
      {
         if (sceIoRename(old_path, new_path) >= 0)
         {
            sceIoRemove(aside);
            ret = 0;
         }
         else
            sceIoRename(aside, new_path);
      }

      free(aside);
      return ret;
   }
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
   if (!stream)
      return NULL;
   return stream->orig_path;
}

const uint8_t *retro_vfs_file_get_mapped_ptr_impl(
      libretro_vfs_implementation_file *stream, int64_t *len)
{
   if (len)
      *len = 0;
#ifdef VFS_HAVE_FILE_MAPPING
   /* Gate on the hint as well as the pointer, matching the read and
    * seek paths: those consult 'mapped' only under the hint, so the
    * map is authoritative for the file contents only when the hint
    * put it there. */
   if (     stream
         && stream->mapped
         && (stream->hints & RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS))
   {
      if (len)
         *len = (int64_t)stream->mapsize;
      return stream->mapped;
   }
#else
   (void)stream;
#endif
   return NULL;
}

#if defined(_WIN32) && !defined(VITA) && !defined(__PSL1GHT__) && !defined(__PS3__)
#if defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
/* Use _stat64 explicitly to match the struct _stat64 buffer the caller
 * declares.  The bare _stat is a macro that expands to _stat64i32 on
 * VS2005+ (or _stat32 with _USE_32BIT_TIME_T), neither of which match
 * struct _stat64 -- passing the wrong struct silently truncates
 * st_size.  _stat64 has been in MSVC since VS2003 (_MSC_VER >= 1300)
 * and is provided by mingw-w64.  VC6 has no 64-bit time_t at all;
 * _stati64 is the only match. */
static int vfs_stat_win32_ansi(const char *path,
      struct _stat64 *stat_buf, DWORD *file_info)
{
   char *path_local = utf8_to_local_string_alloc(path);

   if (!path_local)
      return 0;

   *file_info       = GetFileAttributes(path_local);

#if defined(_MSC_VER) && _MSC_VER < 1300
   if (     *file_info == INVALID_FILE_ATTRIBUTES
         || _stati64(path_local, (struct _stati64*)stat_buf) != 0)
#else
   if (     *file_info == INVALID_FILE_ATTRIBUTES
         || _stat64(path_local, stat_buf) != 0)
#endif
   {
      free(path_local);
      return 0;
   }

   free(path_local);
   return 1;
}
#endif

#if !defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
static int vfs_stat_win32_wide(const char *path,
      struct _stat64 *stat_buf, DWORD *file_info)
{
   wchar_t *path_wide = utf8_to_utf16_string_alloc(path);

   if (!path_wide)
      return 0;

   *file_info         = GetFileAttributesW(path_wide);

   if (     *file_info == INVALID_FILE_ATTRIBUTES
         || _wstat64(path_wide, stat_buf) != 0)
   {
      free(path_wide);
      return 0;
   }

   free(path_wide);
   return 1;
}
#endif
#endif

int retro_vfs_stat_64_impl(const char *path, int64_t *size)
{
   int ret                   = RETRO_VFS_STAT_IS_VALID;

   if (!path || !*path)
      return 0;

#ifdef HAVE_SMBCLIENT
   if (path_is_smb(path))
      return retro_vfs_stat_smb(path, size);
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
   if (path_is_saf(path))
   {
      struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
      if (!retro_vfs_path_split_saf(&saf_split_result, path))
         return 0;
      ret = retro_vfs_stat_saf(saf_split_result.tree, saf_split_result.path, size);
      free(saf_split_result.path);
      free(saf_split_result.tree);
      return ret;
   }
#endif

   {
#if defined(VITA)
      /* Vita / PSP */
      SceIoStat stat_buf;
      int dir_ret;
      char path_buf[PATH_MAX_LENGTH];
      size_t _len               = strlcpy(path_buf, path, sizeof(path_buf));
      if (_len > 0 && path_buf[_len-1] == '/')
          path_buf[_len-1]      = '\0';

      dir_ret                   = sceIoGetstat(path_buf, &stat_buf);
      if (dir_ret < 0)
         return 0;

      if (size)
         *size                  = (int64_t)stat_buf.st_size;

      if (FIO_S_ISDIR(stat_buf.st_mode))
         ret              |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(__PSL1GHT__) || defined(__PS3__)
      /* Lowlevel Lv2 */
      sysFSStat stat_buf;

      if (sysFsStat(path, &stat_buf) < 0)
         return 0;

      if (size)
         *size = (int64_t)stat_buf.st_size;

      if ((stat_buf.st_mode & S_IFMT) == S_IFDIR)
         ret  |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(_WIN32)
      /* Windows
       * Older MSVC _stat may fail on directory paths 
       * with a trailing backslash */
      struct _stat64 stat_buf;
      char path_buf[PATH_MAX_LENGTH];
      const char *stat_path = path;
      DWORD file_info;
      size_t _len = strlcpy(path_buf, path, sizeof(path_buf));

      if (_len > 0 && _len < sizeof(path_buf))
      {
         while (_len > 0 && 
               (path_buf[_len - 1] == '\\' || path_buf[_len - 1] == '/'))
         {
            /* Keep drive roots like "C:\" intact */
            if (_len == 3 &&
                  ((((path_buf[0] >= 'A') && (path_buf[0] <= 'Z')) ||
                    ((path_buf[0] >= 'a') && (path_buf[0] <= 'z'))) &&
                   path_buf[1] == ':' && path_buf[2] == '\\'))
               break;

            path_buf[--_len] = '\0';
         }

         stat_path = path_buf;
      }
#if defined(LEGACY_WIN32_RUNTIME)
      if (win32_needs_local_encoding())
      {
         if (!vfs_stat_win32_ansi(stat_path, &stat_buf, &file_info))
            return 0;
      }
      else if (!vfs_stat_win32_wide(stat_path, &stat_buf, &file_info))
         return 0;
#elif defined(LEGACY_WIN32)
      if (!vfs_stat_win32_ansi(stat_path, &stat_buf, &file_info))
         return 0;
#else
      if (!vfs_stat_win32_wide(stat_path, &stat_buf, &file_info))
         return 0;
#endif

      if (size)
         *size = (int64_t)stat_buf.st_size;

      if (file_info & FILE_ATTRIBUTE_DIRECTORY)
         ret  |= RETRO_VFS_STAT_IS_DIRECTORY;
#elif defined(GEKKO)
      /* On GEKKO platforms, paths cannot have
       * trailing slashes - we must therefore
       * remove them */
      size_t _len;
      char path_buf[PATH_MAX_LENGTH];
      struct stat stat_buf;

      _len = strlcpy(path_buf, path, sizeof(path_buf));
      if (_len > 0 && path_buf[_len - 1] == '/')
          path_buf[_len - 1] = '\0';

      if (stat(path_buf, &stat_buf) < 0)
         return 0;

      if (size)
         *size = (int64_t)stat_buf.st_size;

      if (S_ISDIR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_DIRECTORY;
      if (S_ISCHR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
#else
      /* Every other platform */
/* _LARGEFILE64_SOURCE is a request for the LFS64 API, not evidence
 * that it exists: Darwin, the 3DS and PSP toolchains, DJGPP and musl
 * from 1.2.4 have no stat64 to offer whoever asks.  Paired with a libc
 * known to carry it, so that a build defining the macro for its own
 * reasons cannot turn this into a compile error. */
#if defined(_LARGEFILE64_SOURCE) \
      && (defined(__GLIBC__) || defined(__ANDROID__) || defined(__UCLIBC__))
      struct stat64 stat_buf;
      if (stat64(path, &stat_buf) < 0)
         return 0;
#else
      struct stat stat_buf;

      if (stat(path, &stat_buf) < 0)
         return 0;
#endif

      if (size)
         *size = (int64_t)stat_buf.st_size;

      if (S_ISDIR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_DIRECTORY;
      if (S_ISCHR(stat_buf.st_mode))
         ret |= RETRO_VFS_STAT_IS_CHARACTER_SPECIAL;
#endif
   }
   return ret;
}

int retro_vfs_stat_impl(const char *path, int32_t *size)
{
   int64_t size64 = 0;
   int ret = retro_vfs_stat_64_impl(path, size ? &size64 : NULL);

   /* If a file is larger than 2 GiB, size64 holds the correct value
    * but a naked (int32_t) cast would truncate -- worse, on files in
    * (INT32_MAX, UINT32_MAX] the high bit wraps and callers see a
    * negative size that they may interpret as an error.  Saturate to
    * INT_MAX so a caller using the legacy API gets a clamped-large
    * value rather than a corrupted one, and migrate to
    * retro_vfs_stat_64_impl for files that need the real size.
    * INT_MAX is used instead of INT32_MAX for C89 / VC6 portability
    * (stdint.h's INT32_MAX is a C99 addition; INT_MAX is C89).  int
    * is 32-bit on all MSVC targets including VC6, so INT_MAX ==
    * INT32_MAX everywhere this code runs. */
   if (size)
      *size = (size64 > (int64_t)INT_MAX) ? INT_MAX : (int32_t)size64;

   return ret;
}

#if defined(VITA)
#define path_mkdir_err(ret) (((ret) == SCE_ERROR_ERRNO_EEXIST))
#elif defined(PSP) || defined(PS2) || defined(_3DS) || defined(WIIU) || defined(SWITCH)
#define path_mkdir_err(ret) ((ret) == -1)
#else
#define path_mkdir_err(ret) ((ret) < 0 && errno == EEXIST)
#endif

int retro_vfs_mkdir_impl(const char *dir)
{
#if defined(ANDROID) && defined(HAVE_SAF)
   if (path_is_saf(dir))
   {
      int ret;
      struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
      if (!retro_vfs_path_split_saf(&saf_split_result, dir))
         return -1;
      ret = retro_vfs_mkdir_saf(saf_split_result.tree, saf_split_result.path);
      free(saf_split_result.path);
      free(saf_split_result.tree);
      return ret;
   }
   else
#endif
   {
#if defined(_WIN32)
#if defined(LEGACY_WIN32_RUNTIME)
      int ret        = -1;

      if (win32_needs_local_encoding())
         ret         = _mkdir(dir);
      else
      {
         wchar_t *dir_w = utf8_to_utf16_string_alloc(dir);

         if (dir_w)
         {
            ret = _wmkdir(dir_w);
            free(dir_w);
         }
      }
#elif defined(LEGACY_WIN32)
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
      int ret       = -1;
      char *dir_buf = strdup(dir);

      if (dir_buf)
      {
         size_t _len = strlen(dir_buf);

         if (_len > 0)
            if (dir_buf[_len - 1] == '/')
               dir_buf[_len - 1] = '\0';

         ret = mkdir(dir_buf, 0750);

         free(dir_buf);
      }
#else
      int ret = mkdir(dir, 0750);
#endif

      if (path_mkdir_err(ret))
         return -2;
      return ret < 0 ? -1 : 0;
   }
}

#if defined(_WIN32)
/* Worst-case UTF-8 for a find-data name, terminator included.
 *
 * W: cFileName is WCHAR[MAX_PATH].  A BMP unit is at most 3 UTF-8 bytes;
 * a surrogate pair is 2 units for 4 bytes, so 3 per unit bounds both.
 *
 * A: cFileName is CHAR[MAX_PATH] in the local codepage.  One SBCS byte
 * is one BMP character, so it is bounded by the same 3; DBCS spends two
 * bytes per character and is cheaper still.
 *
 * The old code decoded back over cFileName itself, which is 520 bytes on
 * the W path and 260 on the A path - short of this bound whenever the
 * name is not ASCII, so a 174-character CJK name (255 is legal on NTFS)
 * was silently strlcpy()-truncated.  It also destroyed the source
 * encoding in place, making the function return garbage if ever called
 * twice on one entry. */
#define VFS_WIN32_NAME_UTF8_MAX (MAX_PATH * 3 + 1)
#endif

#ifdef VFS_FRONTEND
struct retro_vfs_dir_handle
#else
struct libretro_vfs_implementation_dir
#endif
{
   char* orig_path;
#if defined(_WIN32)
#if defined(LEGACY_WIN32_RUNTIME)
   /* Unlike every other site, the difference here is a type, not a
    * call: the handle carries the find-data across retro_readdir()
    * calls.  A union costs the larger of the two (the W form, by the
    * width of cFileName) and lets both live in one handle. */
   union
   {
      WIN32_FIND_DATA  a;
      WIN32_FIND_DATAW w;
   } entry;
#elif defined(LEGACY_WIN32)
   WIN32_FIND_DATA entry;
#else
   WIN32_FIND_DATAW entry;
#endif
   HANDLE directory;
   bool next;
   char path[PATH_MAX_LENGTH];
   char name_utf8[VFS_WIN32_NAME_UTF8_MAX];
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
#if defined(ANDROID) && defined(HAVE_SAF)
   libretro_vfs_implementation_saf_dir *saf_directory;
#endif
#ifdef HAVE_SMBCLIENT
   smb_dir_handle* smb_handle;
   char smb_path[PATH_MAX_LENGTH];
   bool smb_is_dir;
#endif
};

static bool dirent_check_err(libretro_vfs_implementation_dir *rdir)
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
   size_t _len;
#if defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
   char *path_local   = NULL;
#endif
#if !defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
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
   if (rdir->orig_path == NULL)
   {
      free(rdir);
      return NULL;
   }

#ifdef HAVE_SMBCLIENT
   if (path_is_smb(name))
   {
      smb_dir_handle *dh = retro_vfs_opendir_smb(name, include_hidden);
      if (!dh || !dh->dir)
      {
         free(rdir->orig_path);
         free(rdir);
         return NULL;
      }
      rdir->smb_handle  = dh;
      rdir->smb_path[0] = '\0';
      rdir->smb_is_dir  = false;
      return rdir;
   }
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
   rdir->saf_directory = NULL;

   if (path_is_saf(name))
   {
      struct libretro_vfs_implementation_saf_path_split_result saf_split_result;
      if (!retro_vfs_path_split_saf(&saf_split_result, name))
      {
         free(rdir->orig_path);
         free(rdir);
         return NULL;
      }
      rdir->saf_directory = retro_vfs_opendir_saf(saf_split_result.tree, saf_split_result.path, include_hidden);
      free(saf_split_result.path);
      free(saf_split_result.tree);
      if (rdir->saf_directory == NULL)
      {
         free(rdir->orig_path);
         free(rdir);
         return NULL;
      }
      return rdir;
   }
#endif

#if defined(_WIN32)
   _len = strlcpy(path_buf, name, sizeof(path_buf));
   /* Non-NT platforms don't like extra slashes in the path */
   if (path_buf[_len - 1] != '\\')
      path_buf [_len++]    = '\\';

   path_buf[_len    ]      = '*';
   path_buf[_len + 1]      = '\0';
#if defined(LEGACY_WIN32_RUNTIME)
   if (win32_needs_local_encoding())
   {
      path_local           = utf8_to_local_string_alloc(path_buf);
      rdir->directory      = FindFirstFileA(path_local, &rdir->entry.a);
      if (path_local)
         free(path_local);
   }
   else
   {
      path_wide            = utf8_to_utf16_string_alloc(path_buf);
      rdir->directory      = FindFirstFileW(path_wide, &rdir->entry.w);
      if (path_wide)
         free(path_wide);
   }
#elif defined(LEGACY_WIN32)
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
   rdir->directory       = opendir(name);
   rdir->entry           = NULL;
#elif defined(__PSL1GHT__) || defined(__PS3__)
   rdir->error           = sysFsOpendir(name, &rdir->directory);
#else
   rdir->directory       = opendir(name);
   rdir->entry           = NULL;
#endif

#ifdef _WIN32
#if defined(LEGACY_WIN32_RUNTIME)
   /* Same field, same offset in both arms of the union - but C wants
    * one of them named, so follow whichever the handle is using. */
   if (win32_needs_local_encoding())
   {
      if (include_hidden)
         rdir->entry.a.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
      else
         rdir->entry.a.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
   }
   else
   {
      if (include_hidden)
         rdir->entry.w.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
      else
         rdir->entry.w.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
   }
#else
   if (include_hidden)
      rdir->entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
   else
      rdir->entry.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
#endif
#else
   (void)include_hidden;
#endif

   if (rdir->directory && !dirent_check_err(rdir))
      return rdir;

   retro_vfs_closedir_impl(rdir);
   return NULL;
}

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir *rdir)
{
#ifdef HAVE_SMBCLIENT
   if (rdir->smb_handle)
   {
      struct smbc_dirent *de = retro_vfs_readdir_smb(rdir->smb_handle);
      if (!de)
         return false;
      strlcpy(rdir->smb_path, de->name, sizeof(rdir->smb_path));
      rdir->smb_is_dir = (de->type == RETRO_SMB_DIRENT_DIR);
      return true;
   }
   /* If we opened an SMB path but failed, do not fall through to native readdir */
   if (path_is_smb(rdir->orig_path))
      return false;
#endif
#if defined(ANDROID) && defined(HAVE_SAF)
   if (rdir->saf_directory != NULL)
      return retro_vfs_readdir_saf(rdir->saf_directory);
#endif

#if defined(_WIN32)
   if (rdir->next)
#if defined(LEGACY_WIN32_RUNTIME)
   {
      if (win32_needs_local_encoding())
         return (FindNextFileA(rdir->directory, &rdir->entry.a) != 0);
      return (FindNextFileW(rdir->directory, &rdir->entry.w) != 0);
   }
#elif defined(LEGACY_WIN32)
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

#if defined(_WIN32)
#if defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
/* Which codepages actually need converting is a question for
 * encodings/, so ask it there rather than restate it here; on targets
 * where the answer is "none" this allocates nothing. */
static const char *vfs_win32_name_local(
      libretro_vfs_implementation_dir *rdir, const char *src)
{
   if (!local_to_utf8_string(src, rdir->name_utf8, sizeof(rdir->name_utf8)))
      return NULL;
   return rdir->name_utf8;
}
#endif

#if !defined(LEGACY_WIN32) || defined(LEGACY_WIN32_RUNTIME)
/* utf16_conv_utf8() writes without an output bound and does not
 * terminate; VFS_WIN32_NAME_UTF8_MAX is what makes the first safe and
 * the store below covers the second.  Taking it directly is what drops
 * the per-entry allocation - utf16_to_utf8_string_alloc() and
 * utf16_to_char_string() both malloc internally. */
static const char *vfs_win32_name_utf16(
      libretro_vfs_implementation_dir *rdir, const wchar_t *src)
{
   char *name;
   size_t out_len = 0;
   size_t in_len  = 0;

   while (src[in_len])
      in_len++;

   if (utf16_conv_utf8((uint8_t*)rdir->name_utf8, &out_len,
            (const uint16_t*)src, in_len))
   {
      rdir->name_utf8[out_len] = '\0';
      return rdir->name_utf8;
   }

   /* Unpaired surrogate: utf16_conv_utf8() stops at one, and NTFS
    * permits them in names.  WideCharToMultiByte() substitutes U+FFFD
    * and carries on, so fall back to it rather than hand back a name
    * truncated at the bad unit.  Rare enough that the allocation this
    * costs does not matter. */
   if (!(name = utf16_to_utf8_string_alloc(src)))
      return NULL;
   strlcpy(rdir->name_utf8, name, sizeof(rdir->name_utf8));
   free(name);
   return rdir->name_utf8;
}
#endif
#endif

const char *retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir *rdir)
{
#ifdef HAVE_SMBCLIENT
   if (rdir->smb_handle)
      return rdir->smb_path;
#endif
#if defined(ANDROID) && defined(HAVE_SAF)
   if (rdir->saf_directory != NULL)
      return retro_vfs_dirent_get_name_saf(rdir->saf_directory);
   else
#endif
   {
#if defined(_WIN32)
#if defined(LEGACY_WIN32_RUNTIME)
      if (win32_needs_local_encoding())
         return vfs_win32_name_local(rdir, rdir->entry.a.cFileName);
      return vfs_win32_name_utf16(rdir, rdir->entry.w.cFileName);
#elif defined(LEGACY_WIN32)
      return vfs_win32_name_local(rdir, rdir->entry.cFileName);
#else
      return vfs_win32_name_utf16(rdir, rdir->entry.cFileName);
#endif
#elif defined(VITA) || defined(__PSL1GHT__) || defined(__PS3__)
      return rdir->entry.d_name;
#else
      if (!rdir || !rdir->entry)
         return NULL;
      return rdir->entry->d_name;
#endif
   }
}

#if !defined(_WIN32) && !defined(VITA) && !defined(__PSL1GHT__) && !defined(__PS3__)
/* Split out of retro_vfs_dirent_is_dir_impl() so that the d_type test
 * there does not have to carry this scratch buffer. Filesystems that
 * populate d_type - ext4, APFS, NTFS - answer from the dirent alone and
 * never reach this, but the array and the struct stat were declared in
 * the same scope as the test, so every entry paid a 2224-byte frame
 * plus, under -fstack-protector-strong, a canary written and re-read at
 * offset 2200 of a frame the fast path otherwise never touches. */
static VFS_NOINLINE bool retro_vfs_dirent_is_dir_stat(
      libretro_vfs_implementation_dir *rdir)
{
   struct stat buf;
   char path[PATH_MAX_LENGTH];

   fill_pathname_join_special(path, rdir->orig_path,
         retro_vfs_dirent_get_name_impl(rdir), sizeof(path));
   if (stat(path, &buf) < 0)
      return false;
   return S_ISDIR(buf.st_mode);
}
#endif

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir *rdir)
{
#ifdef HAVE_SMBCLIENT
   if (rdir->smb_handle)
      return rdir->smb_is_dir;
#endif
#if defined(ANDROID) && defined(HAVE_SAF)
   if (rdir->saf_directory != NULL)
      return retro_vfs_dirent_is_dir_saf(rdir->saf_directory);
   else
#endif
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
#if defined(DT_DIR)
      const struct dirent *entry = (const struct dirent*)rdir->entry;
      if (entry->d_type == DT_DIR)
         return true;
      /* This can happen on certain file systems. */
      if (!(entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK))
         return false;
#endif
      /* dirent struct doesn't have d_type, do it the slow way ... */
      return retro_vfs_dirent_is_dir_stat(rdir);
#endif
   }
}

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir *rdir)
{
   int ret = 0;

   if (!rdir)
      return -1;

#ifdef HAVE_SMBCLIENT
   if (rdir->smb_handle)
   {
      retro_vfs_closedir_smb(rdir->smb_handle);
      rdir->smb_handle = NULL;
   }
#endif

#if defined(ANDROID) && defined(HAVE_SAF)
   if (rdir->saf_directory != NULL)
      ret = retro_vfs_closedir_saf(rdir->saf_directory);
   else
#endif
   {
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
   }

   if (rdir->orig_path)
      free(rdir->orig_path);
   free(rdir);
   return ret;
}
