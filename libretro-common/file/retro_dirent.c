/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_dirent.c).
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

#include <retro_common.h>

#include <boolean.h>
#include <retro_dirent.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

static retro_vfs_opendir_t dirent_opendir_cb = NULL;
static retro_vfs_readdir_t dirent_readdir_cb = NULL;
static retro_vfs_dirent_get_name_t dirent_dirent_get_name_cb = NULL;
static retro_vfs_dirent_is_dir_t dirent_dirent_is_dir_cb = NULL;
static retro_vfs_closedir_t dirent_closedir_cb = NULL;

void dirent_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface* vfs_iface;

   dirent_opendir_cb = NULL;
   dirent_readdir_cb = NULL;
   dirent_dirent_get_name_cb = NULL;
   dirent_dirent_is_dir_cb = NULL;
   dirent_closedir_cb = NULL;

   vfs_iface = vfs_info->iface;

   if (vfs_info->required_interface_version < DIRENT_REQUIRED_VFS_VERSION || !vfs_iface)
      return;

   dirent_opendir_cb = vfs_iface->opendir;
   dirent_readdir_cb = vfs_iface->readdir;
   dirent_dirent_get_name_cb = vfs_iface->dirent_get_name;
   dirent_dirent_is_dir_cb = vfs_iface->dirent_is_dir;
   dirent_closedir_cb = vfs_iface->closedir;
}

struct RDIR *retro_opendir_include_hidden(const char *name, bool include_hidden)
{
   if (dirent_opendir_cb != NULL)
      return (struct RDIR *)dirent_opendir_cb(name, include_hidden);
   return (struct RDIR *)retro_vfs_opendir_impl(name, include_hidden);
}

struct RDIR *retro_opendir(const char *name)
{
   return retro_opendir_include_hidden(name, false);
}

bool retro_dirent_error(struct RDIR *rdir)
{
   /* Left for compatibility */
   return false;
}

int retro_readdir(struct RDIR *rdir)
{
   if (dirent_readdir_cb != NULL)
      return dirent_readdir_cb((struct retro_vfs_dir_handle *)rdir);
   return retro_vfs_readdir_impl((struct retro_vfs_dir_handle *)rdir);
}

const char *retro_dirent_get_name(struct RDIR *rdir)
{
   if (dirent_dirent_get_name_cb != NULL)
      return dirent_dirent_get_name_cb((struct retro_vfs_dir_handle *)rdir);
   return retro_vfs_dirent_get_name_impl((struct retro_vfs_dir_handle *)rdir);
}

/**
 *
 * retro_dirent_is_dir:
 * @rdir         : pointer to the directory entry.
 * @unused       : deprecated, included for compatibility reasons, pass NULL
 *
 * Is the directory listing entry a directory?
 *
 * Returns: true if directory listing entry is
 * a directory, false if not.
 */
bool retro_dirent_is_dir(struct RDIR *rdir, const char *unused)
{
   if (dirent_dirent_is_dir_cb != NULL)
      return dirent_dirent_is_dir_cb((struct retro_vfs_dir_handle *)rdir);
   return retro_vfs_dirent_is_dir_impl((struct retro_vfs_dir_handle *)rdir);
}

void retro_closedir(struct RDIR *rdir)
{
   if (dirent_closedir_cb != NULL)
      dirent_closedir_cb((struct retro_vfs_dir_handle *)rdir);
   else
      retro_vfs_closedir_impl((struct retro_vfs_dir_handle *)rdir);
}
