/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path_io.c).
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
#include <time.h>
#include <errno.h>

#include <sys/stat.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

/* TODO: There are probably some unnecessary things on this huge include list now but I'm too afraid to touch it */
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
#ifdef _MSC_VER
#define setmode _setmode
#endif
#include <sys/stat.h>
#ifdef _XBOX
#include <xtl.h>
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
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

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP) || defined(PS2)
#include <unistd.h> /* stat() is defined here */
#endif

#if !defined(RARCH_CONSOLE) && defined(RARCH_INTERNAL)
#ifdef __WINRT__
#include <uwp/uwp_func.h>
#endif
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

static retro_vfs_stat_t path_stat_cb   = retro_vfs_stat_impl;
static retro_vfs_mkdir_t path_mkdir_cb = retro_vfs_mkdir_impl;

void path_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface* 
      vfs_iface           = vfs_info->iface;

   path_stat_cb           = retro_vfs_stat_impl;
   path_mkdir_cb          = retro_vfs_mkdir_impl;

   if (vfs_info->required_interface_version < PATH_REQUIRED_VFS_VERSION || !vfs_iface)
      return;

   path_stat_cb           = vfs_iface->stat;
   path_mkdir_cb          = vfs_iface->mkdir;
}

int path_stat(const char *path)
{
   return path_stat_cb(path, NULL);
}

/**
 * path_is_directory:
 * @path               : path
 *
 * Checks if path is a directory.
 *
 * Returns: true (1) if path is a directory, otherwise false (0).
 */
bool path_is_directory(const char *path)
{
   return (path_stat_cb(path, NULL) & RETRO_VFS_STAT_IS_DIRECTORY) != 0;
}

bool path_is_character_special(const char *path)
{
   return (path_stat_cb(path, NULL) & RETRO_VFS_STAT_IS_CHARACTER_SPECIAL) != 0;
}

bool path_is_valid(const char *path)
{
   return (path_stat_cb(path, NULL) & RETRO_VFS_STAT_IS_VALID) != 0;
}

int32_t path_get_size(const char *path)
{
   int32_t filesize = 0;
   if (path_stat_cb(path, &filesize) != 0)
      return filesize;

   return -1;
}

/**
 * path_mkdir:
 * @dir                : directory
 *
 * Create directory on filesystem.
 *
 * Returns: true (1) if directory could be created, otherwise false (0).
 **/
bool path_mkdir(const char *dir)
{
   bool         sret  = false;
   bool norecurse     = false;
   char     *basedir  = NULL;

   if (!(dir && *dir))
      return false;

   /* Use heap. Real chance of stack 
    * overflow if we recurse too hard. */
   basedir            = strdup(dir);

   if (!basedir)
	   return false;

   path_parent_dir(basedir);

   if (!*basedir || !strcmp(basedir, dir))
   {
      free(basedir);
      return false;
   }

#if defined(GEKKO)
   {
      size_t len = strlen(basedir);

      /* path_parent_dir() keeps the trailing slash.
       * On Wii, mkdir() fails if the path has a
       * trailing slash...
       * We must therefore remove it. */
      if (len > 0)
         if (basedir[len - 1] == '/')
            basedir[len - 1] = '\0';
   }
#endif

   if (path_is_directory(basedir))
      norecurse = true;
   else
   {
      sret      = path_mkdir(basedir);

      if (sret)
         norecurse = true;
   }

   free(basedir);

   if (norecurse)
   {
      int ret = path_mkdir_cb(dir);

      /* Don't treat this as an error. */
      if (ret == -2 && path_is_directory(dir))
         return true;

      return (ret == 0);
   }

   return sret;
}
