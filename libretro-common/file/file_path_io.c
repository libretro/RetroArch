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

#include <retro_posix_source.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

/* MinGW's <sys/stat.h> defines stat (and in some configs mkdir) as
 * function-like macros that map to _stat64/_stati64/_mkdir.  This must
 * be undone BEFORE including libretro.h below: otherwise the macro
 * rewrites the struct member name in the retro_vfs_interface definition
 * (retro_vfs_stat_t stat -> _stat64), and then the use sites
 * vfs_iface->stat / vfs_iface->mkdir no longer match it, breaking the
 * Windows build.  The POSIX functions are still reachable via their
 * real names where this TU needs them (it doesn't call stat()/mkdir()
 * directly; all I/O goes through the VFS *_impl callbacks). */
#ifdef stat
#undef stat
#endif
#ifdef mkdir
#undef mkdir
#endif

#include <boolean.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#ifdef _WIN32
#include <direct.h>
#if !defined(_XBOX)
#include <windows.h>
#endif
#else
#include <unistd.h> /* stat() is defined here */
#endif

/* Free-space query: only where the call is known to exist.  Everything
 * else reports "unknown" (-1) rather than guessing at a header. */
#if defined(__ANDROID__)
#include <sys/vfs.h>          /* statfs: every API level, statvfs is 19+ */
#define PATH_FREE_SPACE_STATFS
#elif defined(__linux__) || defined(__APPLE__) || defined(__NetBSD__) \
   || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__HAIKU__) \
   || defined(__EMSCRIPTEN__) || (defined(__FreeBSD__) && !defined(ORBIS))
#include <sys/statvfs.h>
#define PATH_FREE_SPACE_STATVFS
#endif

/* <direct.h> (MinGW, included just above) can re-establish the mkdir
 * macro after the earlier #undef, so drop it again here before the use
 * site below.  Same rationale for stat, defensively. */
#ifdef stat
#undef stat
#endif
#ifdef mkdir
#undef mkdir
#endif

/* TODO/FIXME - globals */
static retro_vfs_stat_t path_stat32_cb = retro_vfs_stat_impl;
static retro_vfs_stat_64_t path_stat64_cb = retro_vfs_stat_64_impl;
static retro_vfs_mkdir_t path_mkdir_cb = retro_vfs_mkdir_impl;
/* VFS API v5.  NULL when a frontend older than v5 is in use, so the
 * wrappers report failure instead of touching the local file system
 * behind a foreign frontend's back. */
static retro_vfs_set_readonly_t path_set_readonly_cb = retro_vfs_set_readonly_impl;
static retro_vfs_get_mtime_t    path_get_mtime_cb    = retro_vfs_get_mtime_impl;
static retro_vfs_set_mtime_t    path_set_mtime_cb    = retro_vfs_set_mtime_impl;

void path_vfs_init(const struct retro_vfs_interface_info* vfs_info)
{
   const struct retro_vfs_interface* 
      vfs_iface           = vfs_info->iface;

   path_stat32_cb         = retro_vfs_stat_impl;
   path_stat64_cb         = retro_vfs_stat_64_impl;
   path_mkdir_cb          = retro_vfs_mkdir_impl;
   path_set_readonly_cb   = retro_vfs_set_readonly_impl;
   path_get_mtime_cb      = retro_vfs_get_mtime_impl;
   path_set_mtime_cb      = retro_vfs_set_mtime_impl;

   if (vfs_info->required_interface_version < PATH_REQUIRED_VFS_VERSION || !vfs_iface)
      return;

   path_stat32_cb         = vfs_iface->stat;
   path_mkdir_cb          = vfs_iface->mkdir;

   if (vfs_info->required_interface_version >= STAT64_REQUIRED_VFS_VERSION)
      path_stat64_cb = vfs_iface->stat_64;
   else
      path_stat64_cb = NULL;

   /* Members a v5 frontend left NULL stay NULL: the wrappers then
    * report "unavailable" rather than dereferencing them. */
   path_set_readonly_cb = NULL;
   path_get_mtime_cb    = NULL;
   path_set_mtime_cb    = NULL;
   if (vfs_info->required_interface_version >= METADATA_REQUIRED_VFS_VERSION)
   {
      path_set_readonly_cb = vfs_iface->set_readonly;
      path_get_mtime_cb    = vfs_iface->get_mtime;
      path_set_mtime_cb    = vfs_iface->set_mtime;
   }
}

bool path_is_readonly(const char *path)
{
   if (path_stat64_cb)
      return (path_stat64_cb(path, NULL) & RETRO_VFS_STAT_IS_READONLY) != 0;
   return (path_stat32_cb(path, NULL) & RETRO_VFS_STAT_IS_READONLY) != 0;
}

bool path_set_readonly(const char *path, bool readonly)
{
   if (!path_set_readonly_cb)
      return false;
   return path_set_readonly_cb(path, readonly ? 1 : 0) == 0;
}

bool path_get_mtime(const char *path, int64_t *mtime)
{
   if (!path_get_mtime_cb || !mtime)
      return false;
   return path_get_mtime_cb(path, mtime) == 0;
}

bool path_set_mtime(const char *path, int64_t mtime)
{
   if (!path_set_mtime_cb)
      return false;
   return path_set_mtime_cb(path, mtime) == 0;
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
   struct retro_vfs_dir_handle *d = retro_vfs_opendir_impl(dir, false);

   if (!d)
      return false;

   while (retro_vfs_readdir_impl(d))
   {
      const char *name = retro_vfs_dirent_get_name_impl(d);

      /* ".", ".." and dotfiles (.nomedia, .DS_Store) do not count:
       * the open call does not filter them outside Windows */
      if (name && name[0] != '.')
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

bool path_set_private(const char *path)
{
   return retro_vfs_restrict_permissions_impl(path) == 0;
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
 * path_rmdir:
 * @dir                : directory path.
 *
 * Removes the empty directory @dir.  There is no libretro VFS entry
 * for this, so it always goes through the built-in implementation.
 *
 * @return true if the directory was removed, otherwise false -
 * including a directory that is not empty, and platforms with no
 * directory removal, where an empty directory is simply left.
 **/
bool path_rmdir(const char *dir)
{
   return retro_vfs_rmdir_impl(dir) == 0;
}

static bool path_is_dot_entry(const char *name)
{
   return name[0] == '.'
      && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

/* Appends the separator and @name to the @len characters of @path,
 * a PATH_MAX_LENGTH buffer.  Returns the new length, or 0 if it
 * does not fit. */
static size_t path_tree_append(char *path, size_t len, const char *name)
{
   size_t name_len = strlen(name);
   if (len + 1 + name_len >= PATH_MAX_LENGTH)
      return 0;
   path[len] = PATH_DEFAULT_SLASH_C();
   memcpy(path + len + 1, name, name_len + 1);
   return len + 1 + name_len;
}

/* Drops trailing separators, keeping a root's.  Returns the length. */
static size_t path_tree_trim(char *path)
{
   size_t len = strlen(path);
   while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')
         && path[len - 2] != ':')
      path[--len] = '\0';
   return len;
}

/* Empties and removes the directory in the first @len characters of
 * @path, which grows in place with each entry's name. */
static bool path_remove_tree_dir(char *path, size_t len)
{
   bool ok = true;
   libretro_vfs_implementation_dir *dir =
      retro_vfs_opendir_impl(path, true);

   if (!dir)
      return false;

   while (retro_vfs_readdir_impl(dir))
   {
      size_t      entry_len;
      const char *name = retro_vfs_dirent_get_name_impl(dir);

      if (!name || path_is_dot_entry(name))
         continue;
      if (!(entry_len = path_tree_append(path, len, name)))
      {
         ok = false;
         continue;
      }
      if (retro_vfs_dirent_is_dir_impl(dir))
      {
         /* An empty directory goes at once, and so does a link to
          * one (rmdir on Windows, unlink elsewhere): only a real
          * directory with something in it is walked. */
         if (     retro_vfs_rmdir_impl(path)       != 0
               && retro_vfs_file_remove_impl(path) != 0
               && !path_remove_tree_dir(path, entry_len))
            ok = false;
      }
      else if (retro_vfs_file_remove_impl(path) != 0)
         ok = false;
   }

   retro_vfs_closedir_impl(dir);
   path[len] = '\0';
   return retro_vfs_rmdir_impl(path) == 0 && ok;
}

bool path_remove_tree(const char *path)
{
   bool   ok;
   size_t len;
   char  *buf;

   if (!path || !*path)
      return false;
   /* A file, a link or an empty directory: no walk */
   if (     retro_vfs_rmdir_impl(path)       == 0
         || retro_vfs_file_remove_impl(path) == 0
         || !path_is_valid(path))
      return true;
   if (!path_is_directory(path))
      return false;
   if (!(buf = (char*)malloc(PATH_MAX_LENGTH)))
      return false;
   strlcpy(buf, path, PATH_MAX_LENGTH);
   len = path_tree_trim(buf);
   ok  = path_remove_tree_dir(buf, len);
   free(buf);
   return ok;
}

static bool path_copy_tree_file(const char *src, const char *dst)
{
   struct retro_vfs_copy_handle *copy =
      retro_vfs_copy_begin_impl(src, dst, RETRO_VFS_COPY_OVERWRITE);

   if (!copy)
      return false;
   /* The whole file in one step: the platform's copy primitive at
    * full speed, no buffer of ours */
   while (retro_vfs_copy_step_impl(copy, (int64_t)1 << 62, NULL, NULL)
         == RETRO_VFS_COPY_RUNNING) { }
   return retro_vfs_copy_close_impl(copy) == 0;
}

/* Copies the directory in the first @src_len characters of @src into
 * the one in the first @dst_len of @dst; both grow in place. */
static bool path_copy_tree_dir(char *src, size_t src_len,
      char *dst, size_t dst_len)
{
   bool ok = true;
   libretro_vfs_implementation_dir *dir;

   if (!path_is_directory(dst) && !path_mkdir(dst))
      return false;
   if (!(dir = retro_vfs_opendir_impl(src, true)))
      return false;

   while (retro_vfs_readdir_impl(dir))
   {
      size_t      src_entry, dst_entry;
      const char *name = retro_vfs_dirent_get_name_impl(dir);

      if (!name || path_is_dot_entry(name))
         continue;
      if (     !(src_entry = path_tree_append(src, src_len, name))
            || !(dst_entry = path_tree_append(dst, dst_len, name)))
         ok = false;
      else if (retro_vfs_dirent_is_dir_impl(dir))
      {
         if (!path_copy_tree_dir(src, src_entry, dst, dst_entry))
            ok = false;
      }
      else if (!path_copy_tree_file(src, dst))
         ok = false;
      src[src_len] = '\0';
      dst[dst_len] = '\0';
   }

   retro_vfs_closedir_impl(dir);
   return ok;
}

bool path_copy_tree(const char *src, const char *dst)
{
   bool  ok;
   char *src_buf;
   char *dst_buf;

   if (!src || !*src || !dst || !*dst)
      return false;
   if (!path_is_directory(src))
      return path_copy_tree_file(src, dst);
   if (!(src_buf = (char*)malloc(2 * PATH_MAX_LENGTH)))
      return false;
   dst_buf = src_buf + PATH_MAX_LENGTH;
   strlcpy(src_buf, src, PATH_MAX_LENGTH);
   strlcpy(dst_buf, dst, PATH_MAX_LENGTH);
   ok = path_copy_tree_dir(src_buf, path_tree_trim(src_buf),
         dst_buf, path_tree_trim(dst_buf));
   free(src_buf);
   return ok;
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
/* True for "dev:name", "dev:/name" and "dev:\name" (a trailing
 * separator allowed): a directory whose parent is a device root.  The
 * root cannot be probed everywhere - the PSP firmware refuses a stat of
 * "ms0:/" - and path_parent_dir() turns it into a relative "./", but a
 * mounted device's root always exists. */
static bool path_parent_is_device_root(const char *p)
{
   const char *colon = strchr(p, ':');
   const char *s;

   if (!colon || colon == p)
      return false;
   for (s = p; s < colon; s++)
      if (*s == '/' || *s == '\\')
         return false;

   s = colon + 1;
   if (*s == '/' || *s == '\\')
      s++;
   if (!*s || *s == '/' || *s == '\\')
      return false;
   while (*s && *s != '/' && *s != '\\')
      s++;
   if (*s)
      s++;
   return *s == '\0';
}

bool path_mkdir(const char *dir)
{
   bool norecurse     = false;
   char     *basedir  = NULL;

   if (!(dir && *dir))
      return false;

   /* Nothing to do if it is already there.
    *
    * Without this the common case - which for archive extraction is
    * every member after the first in a given directory - still costs
    * a strdup, a path_parent_dir, a strcmp, a stat of the parent, a
    * mkdir that is guaranteed to fail with EEXIST, and then a stat of
    * the leaf to interpret that failure.  Two directory probes and a
    * doomed syscall to answer a question one probe answers.  On Win32
    * each of those probes is a UTF-16 conversion allocation plus both
    * GetFileAttributesW and _wstat64, so the saving there is larger
    * than the syscall count suggests.
    *
    * This is not a shortcut around the slow path's result: the slow
    * path already returns true for an existing directory, by way of
    * the mkdir -> -2 -> path_is_directory sequence at the bottom.  The
    * one behavioural difference is at a filesystem root ("/", "C:\"),
    * where path_parent_dir empties the string and the !*basedir guard
    * below returns false today; such a directory does exist, so
    * reporting true for it is the correction, not a regression. */
   if (path_is_directory(dir))
      return true;

   if (path_parent_is_device_root(dir))
      norecurse = true;
   else
   {
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
   }

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
#if defined(_WIN32) && !defined(_XBOX)
   ULARGE_INTEGER avail;
#if defined(__WINRT__)
   if (GetDiskFreeSpaceExA(path, &avail, NULL, NULL))
      return (int64_t)avail.QuadPart;
#else
   /* GetDiskFreeSpaceEx is not in the first release of Windows 95, and
    * a static import would stop the binary loading there.  The plain
    * call is everywhere; it tops out at 2GB on old systems, which is
    * still a correct lower bound. */
   typedef BOOL (WINAPI *disk_free_ex_t)(LPCSTR, PULARGE_INTEGER,
         PULARGE_INTEGER, PULARGE_INTEGER);
   static disk_free_ex_t p_disk_free_ex = NULL;
   static int            resolved       = 0;
   DWORD spc, bps, free_clusters, total_clusters;

   if (!resolved)
   {
      HMODULE k32 = GetModuleHandleA("kernel32.dll");
      resolved    = 1;
      if (k32)
         p_disk_free_ex = (disk_free_ex_t)GetProcAddress(k32,
               "GetDiskFreeSpaceExA");
   }

   if (p_disk_free_ex)
   {
      if (p_disk_free_ex(path, &avail, NULL, NULL))
         return (int64_t)avail.QuadPart;
   }
   else if (GetDiskFreeSpaceA(path, &spc, &bps,
            &free_clusters, &total_clusters))
      return (int64_t)free_clusters * (int64_t)spc * (int64_t)bps;
#endif
   return -1;
#elif defined(PATH_FREE_SPACE_STATFS)
   struct statfs fs;
   if (statfs(path, &fs) == 0)
      return (int64_t)fs.f_bavail * (int64_t)fs.f_bsize;
   return -1;
#elif defined(PATH_FREE_SPACE_STATVFS)
   struct statvfs fs;
   if (statvfs(path, &fs) == 0)
      return (int64_t)fs.f_bavail * (int64_t)fs.f_frsize;
   return -1;
#else
   (void)path;
   return -1;
#endif
}
