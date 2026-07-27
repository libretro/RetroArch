/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - The RetroArch Team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <nfsc/libnfs.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <net/net_compat.h>
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_nfs.h"

#define NFS_PREFIX "nfs://"
/* NF3DIR from libnfs-raw-nfs.h — avoid pulling the full raw header. */
#define RETRO_NFS_TYPE_DIR 2
#define RETRO_NFS_VERSION_3 3

static struct nfs_context **nfs_context_pool = NULL;
static int nfs_next_context_index = 0;
static bool nfs_initialized = false;
static int nfs_max_context_configured = 0;
static const struct nfs_settings *nfs_cfg = NULL;
static char nfs_last_error[256] = {0};

static void nfs_set_last_error(const char *msg)
{
   if (msg && *msg)
      strlcpy(nfs_last_error, msg, sizeof(nfs_last_error));
   else
      nfs_last_error[0] = '\0';
}

const char *nfs_get_last_error(void)
{
   return nfs_last_error;
}

static struct nfs_context *get_nfs_context(void)
{
   int idx;

   if (!nfs_initialized)
      return NULL;

   if (!nfs_context_pool || nfs_max_context_configured == 0)
      return NULL;

   if (nfs_next_context_index < 0 || nfs_next_context_index >= nfs_max_context_configured)
      nfs_next_context_index = 0;

   idx = nfs_next_context_index;
   nfs_next_context_index = (nfs_next_context_index + 1) % nfs_max_context_configured;

   if (!nfs_context_pool[idx])
      return NULL;

   return nfs_context_pool[idx];
}

static void nfs_reset(unsigned num_contexts)
{
   unsigned i;

   for (i = 0; i < num_contexts; i++)
   {
      if (nfs_context_pool[i])
         nfs_destroy_context(nfs_context_pool[i]);
   }

   free(nfs_context_pool);
   nfs_context_pool = NULL;

   nfs_initialized = false;
   nfs_next_context_index = 0;
   nfs_max_context_configured = 0;
}

bool nfs_init_cfg(const struct nfs_settings *new_cfg)
{
   nfs_cfg = new_cfg;
   return true;
}

static bool nfs_init(void)
{
   char server[256];
   char export_path[PATH_MAX_LENGTH];
   unsigned i;
   unsigned max_nfs_contexts;

   if (nfs_initialized)
      return true;

   if (!network_init())
   {
      nfs_set_last_error("network_init failed");
      return false;
   }

   if (!nfs_cfg || !nfs_cfg->server_address || !*nfs_cfg->server_address)
   {
      nfs_set_last_error("NFS server not configured");
      return false;
   }

   if (!nfs_cfg->export_path || !*nfs_cfg->export_path)
   {
      nfs_set_last_error("NFS export not configured");
      return false;
   }

   max_nfs_contexts = nfs_cfg->num_contexts;
   if (max_nfs_contexts == 0)
      max_nfs_contexts = 1;

   nfs_context_pool = (struct nfs_context **)calloc(
         max_nfs_contexts, sizeof(struct nfs_context *));
   if (!nfs_context_pool)
      return false;

   strlcpy(server, nfs_cfg->server_address, sizeof(server));
   /* MOUNT protocol expects an absolute export path. */
   if (nfs_cfg->export_path[0] == '/')
      strlcpy(export_path, nfs_cfg->export_path, sizeof(export_path));
   else
   {
      export_path[0] = '/';
      strlcpy(export_path + 1, nfs_cfg->export_path,
            sizeof(export_path) - 1);
   }

   nfs_set_last_error(NULL);

   for (i = 0; i < max_nfs_contexts; i++)
   {
      struct nfs_context *nfs;
      const char *err;

      nfs = nfs_init_context();
      if (!nfs)
      {
         nfs_set_last_error("nfs_init_context failed");
         nfs_reset((unsigned)nfs_max_context_configured);
         return false;
      }

      /* NFSv3, no Kerberos. */
      nfs_set_version(nfs, RETRO_NFS_VERSION_3);

      if (nfs_cfg->timeout)
         nfs_set_timeout(nfs, (int)nfs_cfg->timeout * 1000);

      if (nfs_mount(nfs, server, export_path) != 0)
      {
         err = nfs_get_error(nfs);
         nfs_set_last_error(err && *err ? err : "nfs_mount failed");
         nfs_destroy_context(nfs);
         nfs_reset((unsigned)nfs_max_context_configured);
         return false;
      }

      nfs_context_pool[i] = nfs;
      nfs_max_context_configured = (int)i + 1;
   }

   nfs_initialized = true;
   return true;
}

bool nfs_probe_connection(void)
{
   /* Remount with current settings so browse reflects edits without restart. */
   nfs_shutdown();
   return nfs_init();
}

void nfs_shutdown(void)
{
   if (!nfs_initialized || nfs_max_context_configured == 0)
      return;

   nfs_reset((unsigned)nfs_max_context_configured);
}

/* Build path relative to the mounted export from settings / URL. */
static bool nfs_build_path(char *dest, size_t dest_size, const char *relative_path)
{
   char temp_path[PATH_MAX_LENGTH];
   const char *p;
   size_t _len = 0;
   size_t sz   = sizeof(temp_path);

   if (string_starts_with(relative_path, NFS_PREFIX))
   {
      p = relative_path + strlen(NFS_PREFIX);
      /* Skip server */
      while (*p && *p != '/')
         p++;
      /* Skip export (next path component, may contain multiple segments
       * matching settings export — fall back to rest of URL after server). */
      if (*p == '/')
      {
         char export_norm[PATH_MAX_LENGTH];
         const char *export_cfg = (nfs_cfg && nfs_cfg->export_path)
            ? nfs_cfg->export_path : NULL;
         size_t export_len;

         /* Match mount-time normalization (leading '/'). */
         export_norm[0] = '\0';
         if (export_cfg && *export_cfg)
         {
            if (export_cfg[0] == '/')
               strlcpy(export_norm, export_cfg, sizeof(export_norm));
            else
            {
               export_norm[0] = '/';
               strlcpy(export_norm + 1, export_cfg, sizeof(export_norm) - 1);
            }
         }
         export_len = strlen(export_norm);

         if (export_len > 0 && string_starts_with(p, export_norm))
         {
            p += export_len;
            if (*p == '/')
               p++;
            strlcpy(dest, p, dest_size);
            return true;
         }

         /* No configured export match: keep path after server. */
         strlcpy(dest, p, dest_size);
         return true;
      }

      dest[0] = '\0';
      return true;
   }

   temp_path[0] = '\0';

   if (nfs_cfg && nfs_cfg->subdir && *nfs_cfg->subdir)
   {
      _len = strlcpy(temp_path, nfs_cfg->subdir, sz);
      if (_len > 0 && _len < sz && temp_path[_len - 1] != '/')
      {
         temp_path[_len++] = '/';
         temp_path[_len  ] = '\0';
      }
   }

   if (relative_path && relative_path[0])
   {
      if (relative_path[0] == '/')
         relative_path++;
      if (_len < sz)
         strlcpy(temp_path + _len, relative_path, sz - _len);
   }

   strlcpy(dest, temp_path, dest_size);
   return true;
}

bool retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
   const char *path, unsigned mode, unsigned hints)
{
   char full_path[PATH_MAX_LENGTH];
   struct nfsfh *fh = NULL;
   struct nfs_context *nfs;
   int flags = 0;
   int ret;

   (void)hints;

   if (!stream)
      return false;

   stream->nfs_fh  = (intptr_t)0;
   stream->nfs_ctx = (intptr_t)0;

   if (mode & RETRO_VFS_FILE_ACCESS_READ)
   {
      if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
         flags = O_RDWR;
      else
         flags = O_RDONLY;
   }
   else if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
      flags = O_WRONLY;
   else
      return false;

   /* Match local VFS: WRITE creates; without UPDATE_EXISTING also truncates. */
   if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
   {
      flags |= O_CREAT;
      if (!(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING))
         flags |= O_TRUNC;
   }

   if (!nfs_init())
      return false;

   nfs = get_nfs_context();
   if (!nfs)
      return false;

   if (!nfs_build_path(full_path, sizeof(full_path), path))
      return false;

   if (full_path[0] == '\0')
      strlcpy(full_path, "/", sizeof(full_path));

   if (flags & O_CREAT)
      ret = nfs_open2(nfs, full_path, flags, 0644, &fh);
   else
      ret = nfs_open(nfs, full_path, flags, &fh);

   if (ret != 0 || !fh)
   {
      const char *err = nfs_get_error(nfs);
      char msg[256];
      snprintf(msg, sizeof(msg), "nfs_open(\"%.180s\"): %.40s",
            full_path, (err && *err) ? err : "failed");
      nfs_set_last_error(msg);
      return false;
   }

   stream->nfs_fh  = (intptr_t)(uintptr_t)fh;
   stream->nfs_ctx = (intptr_t)(uintptr_t)nfs;
   stream->scheme  = VFS_SCHEME_NFS;
   return true;
}

int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   uint8_t *ptr   = (uint8_t *)s;
   uint64_t total = 0;
   struct nfs_context *ctx;
   struct nfsfh *fh;

   if (!nfs_initialized || !stream || !s || stream->nfs_fh == 0)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   fh  = (struct nfsfh *)(void *)(uintptr_t)stream->nfs_fh;
   if (!ctx || !fh)
      return -1;

   /* libnfs may return short reads; accumulate like a regular fread(). */
   while (total < len)
   {
      size_t want = (size_t)(len - total);
      int ret;

      if (want > (size_t)INT32_MAX)
         want = (size_t)INT32_MAX;

      ret = nfs_read(ctx, fh, ptr + total, want);
      if (ret < 0)
         return (total > 0) ? (int64_t)total : -1;
      if (ret == 0)
         break;

      total += (uint64_t)ret;
   }

   return (int64_t)total;
}

int64_t retro_vfs_file_write_nfs(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   const uint8_t *ptr = (const uint8_t *)s;
   uint64_t total     = 0;
   struct nfs_context *ctx;
   struct nfsfh *fh;

   if (!nfs_initialized || !stream || !s || stream->nfs_fh == 0)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   fh  = (struct nfsfh *)(void *)(uintptr_t)stream->nfs_fh;
   if (!ctx || !fh)
      return -1;

   while (total < len)
   {
      uint64_t want = len - total;
      int ret;

      if (want > (uint64_t)INT32_MAX)
         want = (uint64_t)INT32_MAX;

      ret = nfs_write(ctx, fh, ptr + total, want);
      if (ret < 0)
         return (total > 0) ? (int64_t)total : -1;
      if (ret == 0)
         break;

      total += (uint64_t)ret;
   }

   return (int64_t)total;
}

int64_t retro_vfs_file_seek_nfs(libretro_vfs_implementation_file *stream,
   int64_t offset, int whence)
{
   uint64_t newpos = 0;
   struct nfsfh *fh;
   struct nfs_context *ctx;
   int ret;

   if (!nfs_initialized || !stream || !stream->nfs_ctx)
      return -1;

   if (stream->nfs_fh == 0 || stream->nfs_fh == (intptr_t)-1)
      return -1;

   fh  = (struct nfsfh *)(void *)(uintptr_t)stream->nfs_fh;
   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   if (!fh || !ctx)
      return -1;

   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
      return -1;

   ret = nfs_lseek(ctx, fh, offset, whence, &newpos);
   if (ret < 0)
      return -1;

   return (int64_t)newpos;
}

int64_t retro_vfs_file_tell_nfs(libretro_vfs_implementation_file *stream)
{
   uint64_t cur = 0;
   struct nfsfh *fh;
   struct nfs_context *ctx;
   int ret;

   if (!nfs_initialized || !stream || !stream->nfs_ctx)
      return -1;

   if (stream->nfs_fh == 0 || stream->nfs_fh == (intptr_t)-1)
      return -1;

   fh  = (struct nfsfh *)(void *)(uintptr_t)stream->nfs_fh;
   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   if (!fh || !ctx)
      return -1;

   ret = nfs_lseek(ctx, fh, 0, SEEK_CUR, &cur);
   if (ret < 0)
      return -1;

   return (int64_t)cur;
}

int retro_vfs_file_close_nfs(libretro_vfs_implementation_file *stream)
{
   int ret;
   struct nfs_context *ctx;

   if (!nfs_initialized)
      return -1;

   if (!stream || stream->nfs_fh == 0 || stream->nfs_fh == (intptr_t)-1)
      return -1;

   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   if (!ctx)
      return -1;

   ret = nfs_close(ctx, (struct nfsfh *)(void *)(uintptr_t)stream->nfs_fh);

   stream->nfs_fh  = (intptr_t)-1;
   stream->nfs_ctx = (intptr_t)0;

   return ret;
}

nfs_dir_handle *retro_vfs_opendir_nfs(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   struct nfsdir *dir = NULL;
   struct nfs_context *nfs;
   nfs_dir_handle *handle;
   int ret;

   (void)include_hidden;

   if (!nfs_init())
      return NULL;

   if (!nfs_build_path(full_path, sizeof(full_path), path))
      return NULL;

   if (full_path[0] == '\0')
      strlcpy(full_path, "/", sizeof(full_path));

   nfs = get_nfs_context();
   if (!nfs)
      return NULL;

   ret = nfs_opendir(nfs, full_path, &dir);
   if (ret != 0 || !dir)
      return NULL;

   handle = (nfs_dir_handle *)malloc(sizeof(*handle));
   if (!handle)
   {
      nfs_closedir(nfs, dir);
      return NULL;
   }

   handle->ctx = nfs;
   handle->dir = dir;
   return handle;
}

struct nfs_dirent *retro_vfs_readdir_nfs(nfs_dir_handle *dh)
{
   struct nfsdirent *ent;
   static struct nfs_dirent result;

   if (!nfs_initialized || !dh || !dh->ctx || !dh->dir)
      return NULL;

   ent = nfs_readdir(dh->ctx, dh->dir);
   if (!ent)
      return NULL;

   memset(&result, 0, sizeof(result));
   strlcpy(result.name, ent->name ? ent->name : "", sizeof(result.name));
   result.type = (ent->type == RETRO_NFS_TYPE_DIR) ? 1 : 0;
   result.size = (int64_t)ent->size;

   return &result;
}

int retro_vfs_closedir_nfs(nfs_dir_handle *dh)
{
   if (!nfs_initialized || !dh || !dh->ctx || !dh->dir)
      return -1;

   nfs_closedir(dh->ctx, dh->dir);
   free(dh);
   return 0;
}

int retro_vfs_stat_nfs(const char *path, int64_t *size)
{
   char rel_path[PATH_MAX_LENGTH];
   struct nfs_stat_64 st;
   struct nfs_context *nfs;

   if (!nfs_init())
      return 0;

   if (!nfs_build_path(rel_path, sizeof(rel_path), path))
      return 0;

   if (rel_path[0] == '\0')
      strlcpy(rel_path, "/", sizeof(rel_path));

   nfs = get_nfs_context();
   if (!nfs)
      return 0;

   if (nfs_stat64(nfs, rel_path, &st) != 0)
      return 0;

   if (size)
      *size = (st.nfs_size > (uint64_t)INT64_MAX)
         ? INT64_MAX
         : (int64_t)st.nfs_size;

   return RETRO_VFS_STAT_IS_VALID |
         ((st.nfs_mode & S_IFMT) == S_IFDIR ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}

int retro_vfs_file_error_nfs(libretro_vfs_implementation_file *stream)
{
   struct nfs_context *ctx;
   const char *err;

   if (!nfs_initialized)
      return -1;

   if (!stream || stream->nfs_fh == 0 || stream->nfs_fh == (intptr_t)-1)
      return -1;

   if (!stream->nfs_ctx)
      return -1;

   ctx = (struct nfs_context *)(void *)(uintptr_t)stream->nfs_ctx;
   if (!ctx)
      return -1;

   err = nfs_get_error(ctx);
   if (err && err[0] != '\0')
      return -1;

   return 0;
}
