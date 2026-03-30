/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <sys/stat.h>

#include <boolean.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h> /* stat() is defined here */
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <sys/statvfs.h>
#endif

/* TODO/FIXME - globals */
static retro_vfs_stat_t path_stat32_cb = retro_vfs_stat_impl;
static retro_vfs_stat_64_t path_stat64_cb = retro_vfs_stat_64_impl;
static retro_vfs_mkdir_t path_mkdir_cb = retro_vfs_mkdir_impl;

void path_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface* 
      vfs_iface           = vfs_info->iface;

   path_stat32_cb         = retro_vfs_stat_impl;
   path_stat64_cb         = retro_vfs_stat_64_impl;
   path_mkdir_cb          = retro_vfs_mkdir_impl;

   if (vfs_info->required_interface_version < PATH_REQUIRED_VFS_VERSION || !vfs_iface)
      return;

   path_stat32_cb         = vfs_iface->stat;
   path_mkdir_cb          = vfs_iface->mkdir;

   if (vfs_info->required_interface_version >= STAT64_REQUIRED_VFS_VERSION)
      path_stat64_cb = vfs_iface->stat_64;
   else
      path_stat64_cb = NULL;
}

int path_stat(const char *path)
{
   /* Use 64‑bit stat if available, else fallback */
   return path_stat64_cb ? path_stat64_cb(path, NULL) : path_stat32_cb(path, NULL);
}

/**
 * path_is_directory:
 * @path               : path
 *
 * Checks if path is a directory.
 *
 * @return true if path is a directory, otherwise false.
 */
bool path_is_directory(const char *path)
{
   if (path_stat64_cb)
      return (path_stat64_cb(path, NULL) & RETRO_VFS_STAT_IS_DIRECTORY) != 0;
   return (path_stat32_cb(path, NULL) & RETRO_VFS_STAT_IS_DIRECTORY) != 0;
}

bool path_is_empty_directory(const char *dir)
{
    struct retro_vfs_dir_handle *d;
    const char *name;

    d = retro_vfs_opendir_impl(dir, false);
    if (!d)
        return false;

    while (retro_vfs_readdir_impl(d))
    {
        name = retro_vfs_dirent_get_name_impl(d);

        /* Skip . and .. */
        if (name &&
            strcmp(name, ".") != 0 &&
            strcmp(name, "..") != 0)
        {
            retro_vfs_closedir_impl(d);
            return false;
        }
    }

    retro_vfs_closedir_impl(d);
    return true;
}

bool path_is_character_special(const char *path)
{
   if (path_stat64_cb)
      return (path_stat64_cb(path, NULL) & RETRO_VFS_STAT_IS_CHARACTER_SPECIAL) != 0;
   return (path_stat32_cb(path, NULL) & RETRO_VFS_STAT_IS_CHARACTER_SPECIAL) != 0;
}

bool path_is_valid(const char *path)
{
   if (path_stat64_cb)
      return (path_stat64_cb(path, NULL) & RETRO_VFS_STAT_IS_VALID) != 0;
   return (path_stat32_cb(path, NULL) & RETRO_VFS_STAT_IS_VALID) != 0;
}

int64_t path_get_size(const char *path)
{
   int64_t filesize = 0;
   int32_t filesize32 = 0;

   if (path_stat64_cb && path_stat64_cb(path, &filesize) != 0)
      return filesize;

   /* Fallback: 32-bit stat */
   if (path_stat32_cb && path_stat32_cb(path, &filesize32) != 0)
      return (int64_t)filesize32;

   return -1;
}

/**
 * path_mkdir:
 * @dir                : directory
 *
 * Create directory on filesystem.
 * 
 * Recursive function.
 *
 * @return true if directory could be created, otherwise false.
 **/
bool path_mkdir(const char *dir)
{
   bool norecurse     = false;
   char     *basedir  = NULL;

   if (!(dir && *dir))
      return false;

   /* Use heap. Real chance of stack 
    * overflow if we recurse too hard. */
   if (!(basedir = strdup(dir)))
      return false;

   path_parent_dir(basedir, strlen(basedir));

   if (!*basedir || !strcmp(basedir, dir))
   {
      free(basedir);
      return false;
   }

   if (     path_is_directory(basedir)
         || path_mkdir(basedir))
      norecurse = true;

   free(basedir);

   if (norecurse)
   {
      int ret = path_mkdir_cb(dir);

      /* Don't treat this as an error. */
      if (ret == -2 && path_is_directory(dir))
         return true;
      else if (ret == 0)
         return true;
   }
   return false;
}

int64_t path_get_free_space(const char *path)
{
#if defined(_WIN32)
   ULARGE_INTEGER free_bytes_available;
   if (GetDiskFreeSpaceExA(path, &free_bytes_available, NULL, NULL))
      return (int64_t)free_bytes_available.QuadPart;
   return -1;
#elif defined(__unix__) || defined(__APPLE__)
   struct statvfs fs;
   if (statvfs(path, &fs) == 0)
      return (int64_t)fs.f_bavail * (int64_t)fs.f_frsize;
   return -1;
#else
   /* Unsupported platform */
   return -1;
#endif
}
