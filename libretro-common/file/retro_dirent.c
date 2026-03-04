/* Copyright  (C) 2010-2020 The RetroArch team
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

#include <retro_common.h>
#include <boolean.h>
#include <retro_dirent.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>
#include <string.h>

/*
 * Bundle all VFS callbacks into one struct so they share a single cache
 * line, reducing pointer-chase cost on every hot-path dispatch.
 */
static struct
{
   retro_vfs_opendir_t         opendir;
   retro_vfs_readdir_t         readdir;
   retro_vfs_dirent_get_name_t dirent_get_name;
   retro_vfs_dirent_is_dir_t   dirent_is_dir;
   retro_vfs_closedir_t        closedir;
} s_cbs; /* zero-initialised by C static storage rules */

/*
 * Convenience macro: cast rdir once to the internal handle type.
 * Avoids a repeated, verbose cast at every call site.
 */
#define RDIR_HANDLE(p) ((struct retro_vfs_dir_handle *)(p))

void dirent_vfs_init(const struct retro_vfs_interface_info *vfs_info)
{
   const struct retro_vfs_interface *vfs_iface;

   /* Wipe the whole bundle in one memset instead of five NULL assignments. */
   memset(&s_cbs, 0, sizeof(s_cbs));

   vfs_iface = vfs_info->iface;

   if (vfs_info->required_interface_version < DIRENT_REQUIRED_VFS_VERSION
         || !vfs_iface)
      return;

   s_cbs.opendir         = vfs_iface->opendir;
   s_cbs.readdir         = vfs_iface->readdir;
   s_cbs.dirent_get_name = vfs_iface->dirent_get_name;
   s_cbs.dirent_is_dir   = vfs_iface->dirent_is_dir;
   s_cbs.closedir        = vfs_iface->closedir;
}

struct RDIR *retro_opendir_include_hidden(const char *name, bool include_hidden)
{
   if (s_cbs.opendir)
      return (struct RDIR *)s_cbs.opendir(name, include_hidden);
   return (struct RDIR *)retro_vfs_opendir_impl(name, include_hidden);
}

struct RDIR *retro_opendir(const char *name)
{
   /*
    * Inline the hidden=false case rather than forwarding to
    * retro_opendir_include_hidden so the compiler can fold the constant
    * boolean and eliminate the extra call frame.
    */
   if (s_cbs.opendir)
      return (struct RDIR *)s_cbs.opendir(name, false);
   return (struct RDIR *)retro_vfs_opendir_impl(name, false);
}

bool retro_dirent_error(struct RDIR *rdir)
{
   /* Left for compatibility – always succeeds */
   (void)rdir;
   return false;
}

int retro_readdir(struct RDIR *rdir)
{
   if (s_cbs.readdir)
      return s_cbs.readdir(RDIR_HANDLE(rdir));
   return retro_vfs_readdir_impl(RDIR_HANDLE(rdir));
}

const char *retro_dirent_get_name(struct RDIR *rdir)
{
   if (s_cbs.dirent_get_name)
      return s_cbs.dirent_get_name(RDIR_HANDLE(rdir));
   return retro_vfs_dirent_get_name_impl(RDIR_HANDLE(rdir));
}

/**
 * retro_dirent_is_dir:
 * @rdir   : pointer to the directory entry.
 * @unused : deprecated, included for compatibility reasons, pass NULL.
 *
 * Returns: true if the directory listing entry is a directory, false otherwise.
 */
bool retro_dirent_is_dir(struct RDIR *rdir, const char *unused)
{
   if (s_cbs.dirent_is_dir)
      return s_cbs.dirent_is_dir(RDIR_HANDLE(rdir));
   return retro_vfs_dirent_is_dir_impl(RDIR_HANDLE(rdir));
}

void retro_closedir(struct RDIR *rdir)
{
   if (s_cbs.closedir)
      s_cbs.closedir(RDIR_HANDLE(rdir));
   else
      retro_vfs_closedir_impl(RDIR_HANDLE(rdir));
}
