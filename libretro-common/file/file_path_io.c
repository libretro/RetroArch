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
#include <string/stdstring.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h> /* stat() is defined here */
#endif

/* TODO/FIXME - globals */
static retro_vfs_stat_t  path_stat_cb  = retro_vfs_stat_impl;
static retro_vfs_mkdir_t path_mkdir_cb = retro_vfs_mkdir_impl;

void path_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface *vfs_iface;

   /* Reset to defaults unconditionally first */
   path_stat_cb  = retro_vfs_stat_impl;
   path_mkdir_cb = retro_vfs_mkdir_impl;

   /* Single combined guard: reject unsupported version or null interface */
   if (!vfs_info)
      return;

   vfs_iface = vfs_info->iface;

   if (vfs_info->required_interface_version < PATH_REQUIRED_VFS_VERSION || !vfs_iface)
      return;

   path_stat_cb  = vfs_iface->stat;
   path_mkdir_cb = vfs_iface->mkdir;
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
 * @return true if path is a directory, otherwise false.
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
 * Create directory on filesystem, creating intermediate parent
 * directories as needed (iterative, not recursive).
 *
 * Converts the original tail-recursive design into an explicit
 * iterative loop to avoid stack growth on deep paths, while
 * keeping a single heap allocation for the working buffer.
 *
 * @return true if directory exists or was created, otherwise false.
 **/
bool path_mkdir(const char *dir)
{
   size_t  len;
   char   *buf;
   char   *cur;
   int     ret;

   if (!dir || !*dir)
      return false;

   len = strlen(dir);
   buf = (char *)malloc(len + 1);
   if (!buf)
      return false;

   memcpy(buf, dir, len + 1);

   /* Walk up to find the highest missing ancestor, then
    * create directories on the way back down — all without
    * extra heap allocations or recursion. */
   cur = buf + len;
   for (;;)
   {
      /* Trim trailing separators to get the true parent */
      path_parent_dir(buf, (size_t)(cur - buf));

      /* parent_dir wrote a NUL; find it */
      cur = buf + strlen(buf);

      /* Reached filesystem root or an empty string */
      if (cur == buf || !strcmp(buf, dir))
      {
         free(buf);
         return false;
      }

      /* Stop as soon as we hit an existing directory */
      if (path_is_directory(buf))
         break;
   }

   /* Restore each component from the bottom up and mkdir it.
    * We reconstruct the path by re-appending the segments we
    * stripped during the upward walk. */
   while ((size_t)(cur - buf) < len)
   {
      /* Re-extend to next separator / end-of-original-path */
      *cur = dir[(size_t)(cur - buf)]; /* restore separator */
      while (       (size_t)(cur - buf)   < len
             && dir[(size_t)(cur - buf)] != '\0'
             && dir[(size_t)(cur - buf)] != '/'
#ifdef _WIN32
             && dir[(size_t)(cur - buf)] != '\\'
#endif
            )
      {
         cur++;
         *cur = dir[(size_t)(cur - buf)];
      }
      *cur = '\0';

      ret = path_mkdir_cb(buf);
      /* -2 means "already exists"; verify it really is a dir */
      if (ret == -2)
      {
         if (!path_is_directory(buf))
         {
            free(buf);
            return false;
         }
      }
      else if (ret != 0)
      {
         free(buf);
         return false;
      }
   }

   free(buf);
   return true;
}
