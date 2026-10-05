/*  RetroArch - A frontend for libretro.
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <compat/strl.h>
#include <net/net_compat.h>
#include <net/net_nfs3.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#include "../cloud_sync_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define NFSPFX "[NFS] "

/* Downloads land in <file>.rafetching and are renamed over the local
 * file only once complete; uploads land in <path>.rauploading on the
 * server and are renamed into place the same way. */
#define NFS_LOCAL_TMP_SUFFIX  ".rafetching"
#define NFS_REMOTE_TMP_SUFFIX ".rauploading"

/* Per-session scratch, allocated once in begin: path buffers and the
 * transfer buffer stay off the task thread's stack and out of the
 * per-file path. */
typedef struct
{
   uint8_t io[64 * 1024];
   char base[PATH_MAX_LENGTH];      /* <nfs_subdir>/cloud_sync */
   char remote[PATH_MAX_LENGTH];    /* the file on the server */
   char tmp[PATH_MAX_LENGTH];       /* <remote>.rauploading */
   char backup[PATH_MAX_LENGTH];    /* <remote>-<yymmdd-hhmmss>[-n] */
   char dir[PATH_MAX_LENGTH];       /* directories made on the way */
   char local_tmp[PATH_MAX_LENGTH]; /* <file>.rafetching */
} nfs_sync_scratch_t;

typedef struct
{
   struct rnfs_ctx *ctx;
   nfs_sync_scratch_t *s;
} nfs_sync_state_t;

static nfs_sync_state_t nfs_st;

/* @a/@b into @s; false when it does not fit, so a truncated name is
 * never used. */
static bool nfs_sync_join(char *s, size_t len, const char *a, const char *b)
{
   size_t _len = strlcpy(s, a, len);
   if (_len + 1 >= len)
      return false;
   s[_len++] = '/';
   while (*b == '/')
      b++;
   return strlcpy(s + _len, b, len - _len) < len - _len;
}

static bool nfs_sync_suffix(char *s, size_t len, const char *a,
      const char *suffix)
{
   size_t _len = strlcpy(s, a, len);
   return _len < len && strlcpy(s + _len, suffix, len - _len) < len - _len;
}

/* Makes each missing directory along @buf, which is edited in place
 * while it runs and left as it was. A directory another client makes
 * between the lookup and MKDIR is accepted. */
static bool nfs_sync_mkdirs(struct rnfs_ctx *ctx, char *buf)
{
   char *p;
   for (p = buf; ; p++)
   {
      struct rnfs_stat st;
      char saved;
      bool ok = true;

      if (*p != '/' && *p != '\0')
         continue;
      saved = *p;
      *p    = '\0';
      if (buf[0])
      {
         if (rnfs_stat(ctx, buf, &st) == 0)
            ok = st.is_dir != 0;
         else if (rnfs_get_status(ctx) == RNFS_STATUS_NOENT)
            ok = rnfs_mkdir(ctx, buf) == 0
               || (rnfs_stat(ctx, buf, &st) == 0 && st.is_dir);
         else
            ok = false;
         if (!ok)
            RARCH_ERR(NFSPFX "Could not make directory '%s': %s\n",
                  buf, rnfs_get_error(ctx));
      }
      *p = saved;
      if (!ok)
         return false;
      if (!saved)
         break;
   }
   return true;
}

/* The directory @remote goes in: one lookup when it is there already. */
static bool nfs_sync_parent_dir(struct rnfs_ctx *ctx, const char *remote,
      char *dir, size_t len)
{
   struct rnfs_stat st;
   char *slash;

   if (strlcpy(dir, remote, len) >= len)
      return false;
   if (!(slash = strrchr(dir, '/')))
      return true;
   *slash = '\0';
   if (rnfs_stat(ctx, dir, &st) == 0)
      return st.is_dir != 0;
   if (rnfs_get_status(ctx) != RNFS_STATUS_NOENT)
      return false;
   return nfs_sync_mkdirs(ctx, dir);
}

/* A free name beside @remote for the copy a non-destructive replace or
 * delete keeps: <remote>-<yymmdd-hhmmss>, then -2 to -9 within the same
 * second. False when none is free or the server does not answer. */
static bool nfs_sync_backup_name(struct rnfs_ctx *ctx, char *s, size_t len,
      const char *remote)
{
   char      ts[24];
   struct tm tm_buf;
   time_t    t;
   size_t    _len;
   unsigned  n;

   time(&t);
   rtime_localtime(&t, &tm_buf);
   if (!strftime(ts, sizeof(ts), "-%y%m%d-%H%M%S", &tm_buf))
      return false;
   if (     (_len = strlcpy(s, remote, len)) >= len
         || (_len += strlcpy(s + _len, ts, len - _len)) + 3 > len)
      return false;
   for (n = 1; n <= 9; n++)
   {
      struct rnfs_stat st;
      if (n > 1)
      {
         s[_len]     = '-';
         s[_len + 1] = (char)('0' + n);
         s[_len + 2] = '\0';
      }
      if (rnfs_stat(ctx, s, &st) != 0)
         return rnfs_get_status(ctx) == RNFS_STATUS_NOENT;
   }
   return false;
}

static bool nfs_sync_begin(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   settings_t         *settings = config_get_ptr();
   const char         *server   = settings->arrays.nfs_server;
   const char         *export_path = settings->arrays.nfs_export;
   const char         *subdir   = settings->arrays.nfs_subdir;
   nfs_sync_scratch_t *s        = NULL;
   struct rnfs_ctx    *ctx      = NULL;
   size_t              _len     = 0;

   if (string_is_empty(server) || string_is_empty(export_path))
   {
      RARCH_ERR(NFSPFX "Server address and export must be configured\n");
      goto fail;
   }
   network_init();
   if (     !(s   = (nfs_sync_scratch_t*)malloc(sizeof(*s)))
         || !(ctx = rnfs_new()))
      goto fail;

   rnfs_set_timeout(ctx, settings->uints.nfs_timeout);
   rnfs_set_ports(ctx, (uint16_t)settings->uints.nfs_port,
         (uint16_t)settings->uints.nfs_mount_port);
   rnfs_set_version(ctx, settings->uints.nfs_version);
   rnfs_set_readahead(ctx, settings->uints.nfs_readahead * 1024);

   RARCH_LOG(NFSPFX "Connecting to %s:%s\n", server, export_path);
   if (rnfs_connect(ctx, server, export_path) != 0)
   {
      RARCH_ERR(NFSPFX "Connect failed: %s\n", rnfs_get_error(ctx));
      goto fail;
   }

   /* <nfs_subdir>/cloud_sync with no empty components: leading,
    * trailing and repeated slashes in the setting are dropped */
   for (; subdir && *subdir; subdir++)
   {
      if (*subdir == '/' && (!_len || s->base[_len - 1] == '/'))
         continue;
      if (_len + 1 >= sizeof(s->base))
         break;
      s->base[_len++] = *subdir;
   }
   if (_len && s->base[_len - 1] != '/')
      s->base[_len++] = '/';
   if (     (subdir && *subdir)
         || strlcpy(s->base + _len, "cloud_sync", sizeof(s->base) - _len)
            >= sizeof(s->base) - _len)
   {
      RARCH_ERR(NFSPFX "NFS subdirectory is too long\n");
      goto fail;
   }
   strlcpy(s->dir, s->base, sizeof(s->dir));
   if (!nfs_sync_mkdirs(ctx, s->dir))
      goto fail;

   nfs_st.ctx = ctx;
   nfs_st.s   = s;
   RARCH_LOG(NFSPFX "Connected successfully\n");
   cb(user_data, NULL, true, NULL);
   return true;

fail:
   if (ctx)
      rnfs_free(ctx);
   free(s);
   cb(user_data, NULL, false, NULL);
   return true;
}

static bool nfs_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   if (nfs_st.ctx)
      rnfs_free(nfs_st.ctx);
   free(nfs_st.s);
   nfs_st.ctx = NULL;
   nfs_st.s   = NULL;
   cb(user_data, NULL, true, NULL);
   return true;
}

/* The local file is replaced only by a complete download: it is
 * written beside it and renamed over it, so a failure at any point
 * leaves the local file as it was. Success with no file means the
 * server answered that the file is not there. */
static bool nfs_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   nfs_sync_scratch_t *s      = nfs_st.s;
   struct rnfs_ctx    *ctx    = nfs_st.ctx;
   struct rnfs_file   *remote = NULL;
   RFILE              *local  = NULL;
   int64_t             count  = 0;
   bool                ok     = true;

   if (     !nfs_sync_join(s->remote, sizeof(s->remote), s->base, path)
         || !nfs_sync_suffix(s->local_tmp, sizeof(s->local_tmp), file,
               NFS_LOCAL_TMP_SUFFIX))
   {
      RARCH_ERR(NFSPFX "Path too long for '%s'\n", path);
      cb(user_data, path, false, NULL);
      return true;
   }
   if (!(remote = rnfs_open(ctx, s->remote, RNFS_O_RDONLY)))
   {
      if (rnfs_get_status(ctx) == RNFS_STATUS_NOENT)
      {
         cb(user_data, path, true, NULL);
         return true;
      }
      RARCH_ERR(NFSPFX "Open '%s' failed: %s\n",
            s->remote, rnfs_get_error(ctx));
      cb(user_data, path, false, NULL);
      return true;
   }
   if (!(local = filestream_open(s->local_tmp, RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      RARCH_ERR(NFSPFX "Could not create '%s'\n", s->local_tmp);
      rnfs_close(ctx, remote);
      cb(user_data, path, false, NULL);
      return true;
   }
   while ((count = rnfs_read(ctx, remote, s->io, sizeof(s->io))) > 0)
      if (filestream_write(local, s->io, count) != count)
      {
         ok = false;
         break;
      }
   if (count < 0)
   {
      RARCH_ERR(NFSPFX "Read '%s' failed: %s\n",
            s->remote, rnfs_get_error(ctx));
      ok = false;
   }
   rnfs_close(ctx, remote);
   if (filestream_close(local) != 0)
      ok = false;
   if (ok && filestream_rename(s->local_tmp, file) != 0)
   {
      RARCH_ERR(NFSPFX "Could not replace '%s'\n", file);
      ok = false;
   }
   if (!ok)
   {
      filestream_delete(s->local_tmp);
      cb(user_data, path, false, NULL);
      return true;
   }
   local = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   cb(user_data, path, local != NULL, local);
   return true;
}

static bool nfs_write_file(struct rnfs_ctx *ctx, uint8_t *buf, size_t len,
      const char *path, RFILE *local)
{
   struct rnfs_file *remote;
   int64_t           count = 0;
   bool              ok    = true;

   if (!(remote = rnfs_open(ctx, path,
               RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC)))
      return false;
   if (filestream_seek(local, 0, RETRO_VFS_SEEK_POSITION_START) != 0)
      ok = false;
   while (ok && (count = filestream_read(local, buf, len)) > 0)
   {
      int64_t offset = 0;
      while (offset < count)
      {
         int64_t written = rnfs_write(ctx, remote, buf + offset,
               (size_t)(count - offset));
         if (written <= 0)
         {
            ok = false;
            break;
         }
         offset += written;
      }
   }
   if (count < 0)
      ok = false;
   if (rnfs_close(ctx, remote) != 0)
      ok = false;
   return ok;
}

/* The upload is written to <path>.rauploading and renamed over <path>,
 * which RENAME does in one step. With destructive sync off the copy
 * being replaced is first renamed to a backup name, and put back if the
 * final rename fails. Whenever the server's answer is missing the
 * upload fails rather than skip the backup. */
static bool nfs_update(const char *path, RFILE *rfile,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   nfs_sync_scratch_t *s        = nfs_st.s;
   struct rnfs_ctx    *ctx      = nfs_st.ctx;
   settings_t         *settings = config_get_ptr();
   bool                backed_up = false;
   struct rnfs_stat    st;

   if (     !nfs_sync_join(s->remote, sizeof(s->remote), s->base, path)
         || !nfs_sync_suffix(s->tmp, sizeof(s->tmp), s->remote,
               NFS_REMOTE_TMP_SUFFIX))
   {
      RARCH_ERR(NFSPFX "Path too long for '%s'\n", path);
      cb(user_data, path, false, rfile);
      return true;
   }
   if (!nfs_sync_parent_dir(ctx, s->remote, s->dir, sizeof(s->dir)))
   {
      RARCH_ERR(NFSPFX "No directory for '%s': %s\n",
            s->remote, rnfs_get_error(ctx));
      cb(user_data, path, false, rfile);
      return true;
   }
   if (!nfs_write_file(ctx, s->io, sizeof(s->io), s->tmp, rfile))
   {
      RARCH_ERR(NFSPFX "Write '%s' failed: %s\n",
            s->tmp, rnfs_get_error(ctx));
      goto fail;
   }

   if (     !settings->bools.cloud_sync_destructive
         && !string_is_equal(path, CLOUD_SYNC_SERVER_MANIFEST))
   {
      if (rnfs_stat(ctx, s->remote, &st) == 0)
      {
         if (     !nfs_sync_backup_name(ctx, s->backup, sizeof(s->backup),
                     s->remote)
               || rnfs_rename(ctx, s->remote, s->backup) != 0)
         {
            RARCH_ERR(NFSPFX "Could not keep the old '%s': %s\n",
                  s->remote, rnfs_get_error(ctx));
            goto fail;
         }
         backed_up = true;
      }
      else if (rnfs_get_status(ctx) != RNFS_STATUS_NOENT)
      {
         RARCH_ERR(NFSPFX "Stat '%s' failed: %s\n",
               s->remote, rnfs_get_error(ctx));
         goto fail;
      }
   }

   if (rnfs_rename(ctx, s->tmp, s->remote) != 0)
   {
      RARCH_ERR(NFSPFX "Rename '%s' -> '%s' failed: %s\n",
            s->tmp, s->remote, rnfs_get_error(ctx));
      if (backed_up)
         rnfs_rename(ctx, s->backup, s->remote);
      goto fail;
   }
   cb(user_data, path, true, rfile);
   return true;

fail:
   rnfs_unlink(ctx, s->tmp);
   cb(user_data, path, false, rfile);
   return true;
}

/* A file the server answers is not there counts as removed. With
 * destructive sync off the file is renamed to a backup name instead. */
static bool nfs_free(const char *path, cloud_sync_complete_handler_t cb,
      void *user_data)
{
   nfs_sync_scratch_t *s        = nfs_st.s;
   struct rnfs_ctx    *ctx      = nfs_st.ctx;
   settings_t         *settings = config_get_ptr();
   struct rnfs_stat    st;
   int                 rc;

   if (!nfs_sync_join(s->remote, sizeof(s->remote), s->base, path))
   {
      RARCH_ERR(NFSPFX "Path too long for '%s'\n", path);
      cb(user_data, path, false, NULL);
      return true;
   }
   if (settings->bools.cloud_sync_destructive)
      rc = rnfs_unlink(ctx, s->remote);
   else if ((rc = rnfs_stat(ctx, s->remote, &st)) == 0)
      rc = nfs_sync_backup_name(ctx, s->backup, sizeof(s->backup), s->remote)
         ? rnfs_rename(ctx, s->remote, s->backup) : -1;
   if (rc != 0 && rnfs_get_status(ctx) == RNFS_STATUS_NOENT)
      rc = 0;
   if (rc != 0)
      RARCH_ERR(NFSPFX "Could not remove '%s': %s\n",
            s->remote, rnfs_get_error(ctx));
   cb(user_data, path, rc == 0, NULL);
   return true;
}

cloud_sync_driver_t cloud_sync_nfs = {
   nfs_sync_begin,
   nfs_sync_end,
   nfs_read,
   nfs_update,
   nfs_free,
   "nfs",
   CLOUD_SYNC_DRIVER_FLG_BLOCKING
};
