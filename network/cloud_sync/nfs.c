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

#include <net/net_nfs3.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#include "../cloud_sync_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include <compat/strl.h>

#define NFSPFX "[NFS] "
#define NFS_STATUS_NOENT 2
#define NFS_IO_BUFFER_SIZE (64 * 1024)

typedef struct
{
   struct rnfs_ctx *ctx;
   char subdir[PATH_MAX_LENGTH];
} nfs_sync_state_t;

static nfs_sync_state_t nfs_st = {0};

static void nfs_sync_build_path(char *dest, size_t dest_size,
      const char *subdir, const char *path)
{
   size_t len = 0;

   dest[0] = '\0';
   if (subdir && *subdir)
   {
      while (*subdir == '/')
         subdir++;
      len = strlcpy(dest, subdir, dest_size);
      if (len > 0 && len < dest_size && dest[len - 1] != '/')
      {
         dest[len++] = '/';
         dest[len]   = '\0';
      }
   }
   if (path)
   {
      while (*path == '/')
         path++;
      if (len < dest_size)
         strlcpy(dest + len, path, dest_size - len);
   }
}

static bool nfs_sync_ensure_dir(struct rnfs_ctx *ctx, const char *path)
{
   char buf[PATH_MAX_LENGTH];
   char *p;

   strlcpy(buf, path, sizeof(buf));
   for (p = buf; ; p++)
   {
      struct rnfs_stat st;
      char saved;

      if (*p != '/' && *p != '\0')
         continue;
      saved = *p;
      *p    = '\0';
      if (buf[0])
      {
         if (rnfs_stat(ctx, buf, &st) == 0)
         {
            if (!st.is_dir)
            {
               RARCH_ERR(NFSPFX "'%s' is not a directory\n", buf);
               return false;
            }
         }
         else if (rnfs_get_status(ctx) == NFS_STATUS_NOENT)
         {
            if (rnfs_mkdir(ctx, buf) != 0)
            {
               RARCH_ERR(NFSPFX "mkdir '%s' failed: %s\n",
                     buf, rnfs_get_error(ctx));
               return false;
            }
         }
         else
         {
            RARCH_ERR(NFSPFX "stat '%s' failed: %s\n",
                  buf, rnfs_get_error(ctx));
            return false;
         }
      }
      *p = saved;
      if (!saved)
         break;
   }
   return true;
}

static bool nfs_sync_ensure_parent_dir(struct rnfs_ctx *ctx,
      const char *path)
{
   char dir[PATH_MAX_LENGTH];
   char *last_slash;

   strlcpy(dir, path, sizeof(dir));
   last_slash = strrchr(dir, '/');
   if (!last_slash)
      return true;
   *last_slash = '\0';
   return nfs_sync_ensure_dir(ctx, dir);
}

static void nfs_backup_path(char *dest, size_t len, const char *path)
{
   time_t t;
   struct tm tm_buf;
   char ts[32];

   time(&t);
   rtime_localtime(&t, &tm_buf);
   snprintf(ts, sizeof(ts), "-%02d%02d%02d-%02d%02d%02d",
         tm_buf.tm_year % 100, tm_buf.tm_mon + 1, tm_buf.tm_mday,
         tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
   strlcpy(dest, path, len);
   strlcat(dest, ts, len);
}

static bool nfs_sync_begin(cloud_sync_complete_handler_t cb,
      void *user_data)
{
   settings_t *settings       = config_get_ptr();
   const char *server         = settings->arrays.nfs_server;
   const char *export_path    = settings->arrays.nfs_export;
   const char *subdir         = settings->arrays.nfs_subdir;
   struct rnfs_ctx *ctx       = NULL;
   size_t len                 = 0;

   if (!server || !*server || !export_path || !*export_path)
   {
      RARCH_ERR(NFSPFX "Server address and export must be configured\n");
      cb(user_data, NULL, false, NULL);
      return true;
   }
   if (!(ctx = rnfs_new()))
   {
      cb(user_data, NULL, false, NULL);
      return true;
   }

   rnfs_set_timeout(ctx, settings->uints.nfs_timeout);
   rnfs_set_ports(ctx, (uint16_t)settings->uints.nfs_port,
         (uint16_t)settings->uints.nfs_mount_port);
   rnfs_set_version(ctx, settings->uints.nfs_version);
   rnfs_set_readahead(ctx, settings->uints.nfs_readahead * 1024);

   RARCH_LOG(NFSPFX "Connecting to %s:%s\n", server, export_path);
   if (rnfs_connect(ctx, server, export_path) != 0)
   {
      RARCH_ERR(NFSPFX "Connect failed: %s\n", rnfs_get_error(ctx));
      rnfs_free(ctx);
      cb(user_data, NULL, false, NULL);
      return true;
   }

   nfs_st.ctx       = ctx;
   nfs_st.subdir[0] = '\0';
   if (subdir && *subdir)
   {
      while (*subdir == '/')
         subdir++;
      len = strlcpy(nfs_st.subdir, subdir, sizeof(nfs_st.subdir));
      if (len < sizeof(nfs_st.subdir))
         len += strlcpy_lit(nfs_st.subdir + len, "/",
               sizeof(nfs_st.subdir) - len);
   }
   if (len < sizeof(nfs_st.subdir))
      strlcpy_lit(nfs_st.subdir + len, "cloud_sync",
            sizeof(nfs_st.subdir) - len);

   if (!nfs_sync_ensure_dir(ctx, nfs_st.subdir))
   {
      rnfs_free(ctx);
      nfs_st.ctx       = NULL;
      nfs_st.subdir[0] = '\0';
      cb(user_data, NULL, false, NULL);
      return true;
   }

   RARCH_LOG(NFSPFX "Connected successfully\n");
   cb(user_data, NULL, true, NULL);
   return true;
}

static bool nfs_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   if (nfs_st.ctx)
   {
      rnfs_free(nfs_st.ctx);
      nfs_st.ctx = NULL;
   }
   nfs_st.subdir[0] = '\0';
   cb(user_data, NULL, true, NULL);
   return true;
}

static bool nfs_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char nfs_path[PATH_MAX_LENGTH];
   struct rnfs_stat st;
   struct rnfs_file *remote = NULL;
   RFILE *local             = NULL;
   uint8_t *buf             = NULL;
   bool success             = false;

   nfs_sync_build_path(nfs_path, sizeof(nfs_path), nfs_st.subdir, path);
   if (rnfs_stat(nfs_st.ctx, nfs_path, &st) != 0)
   {
      if (rnfs_get_status(nfs_st.ctx) == NFS_STATUS_NOENT)
      {
         cb(user_data, path, true, NULL);
         return true;
      }
      RARCH_ERR(NFSPFX "Stat '%s' failed: %s\n",
            nfs_path, rnfs_get_error(nfs_st.ctx));
      cb(user_data, path, false, NULL);
      return true;
   }

   remote = rnfs_open(nfs_st.ctx, nfs_path, RNFS_O_RDONLY);
   local  = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ_WRITE, 0);
   buf    = (uint8_t*)malloc(NFS_IO_BUFFER_SIZE);
   if (remote && local && buf)
   {
      int64_t count;
      success = true;
      while ((count = rnfs_read(nfs_st.ctx, remote, buf,
                  NFS_IO_BUFFER_SIZE)) > 0)
         if (filestream_write(local, buf, count) != count)
         {
            success = false;
            break;
         }
      if (count < 0)
         success = false;
   }
   if (!success)
      RARCH_ERR(NFSPFX "Read '%s' failed: %s\n",
            nfs_path, rnfs_get_error(nfs_st.ctx));
   free(buf);
   if (remote)
      rnfs_close(nfs_st.ctx, remote);
   if (!success && local)
   {
      filestream_close(local);
      local = NULL;
   }
   else if (local)
      filestream_seek(local, 0, SEEK_SET);

   cb(user_data, path, success, local);
   return true;
}

static bool nfs_write_file(const char *path, RFILE *local)
{
   struct rnfs_file *remote;
   uint8_t *buf;
   int64_t count = 0;
   bool success = true;

   if (!(remote = rnfs_open(nfs_st.ctx, path,
               RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC)))
      return false;
   if (!(buf = (uint8_t*)malloc(NFS_IO_BUFFER_SIZE)))
   {
      rnfs_close(nfs_st.ctx, remote);
      return false;
   }
   if (filestream_seek(local, 0, SEEK_SET) < 0)
      success = false;
   while (success && (count = filestream_read(local, buf,
               NFS_IO_BUFFER_SIZE)) > 0)
   {
      int64_t offset = 0;
      while (offset < count)
      {
         int64_t written = rnfs_write(nfs_st.ctx, remote,
               buf + offset, (size_t)(count - offset));
         if (written <= 0)
         {
            success = false;
            break;
         }
         offset += written;
      }
   }
   if (count < 0)
      success = false;
   free(buf);
   if (rnfs_close(nfs_st.ctx, remote) != 0)
      success = false;
   return success;
}

static bool nfs_update(const char *path, RFILE *rfile,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char nfs_path[PATH_MAX_LENGTH];
   char tmp_path[PATH_MAX_LENGTH];
   char backup_path[PATH_MAX_LENGTH];
   struct rnfs_stat st;
   settings_t *settings = config_get_ptr();
   bool had_old;
   bool keep_old;

   nfs_sync_build_path(nfs_path, sizeof(nfs_path), nfs_st.subdir, path);
   strlcpy(tmp_path, nfs_path, sizeof(tmp_path));
   strlcat(tmp_path, ".rauploading", sizeof(tmp_path));
   if (!nfs_sync_ensure_parent_dir(nfs_st.ctx, nfs_path)
         || !nfs_write_file(tmp_path, rfile))
   {
      rnfs_unlink(nfs_st.ctx, tmp_path);
      cb(user_data, path, false, rfile);
      return true;
   }

   had_old  = rnfs_stat(nfs_st.ctx, nfs_path, &st) == 0;
   keep_old = had_old
         && !settings->bools.cloud_sync_destructive
         && !string_is_equal(path, CLOUD_SYNC_SERVER_MANIFEST);
   if (had_old)
   {
      int rc;
      if (keep_old)
      {
         nfs_backup_path(backup_path, sizeof(backup_path), nfs_path);
         rc = rnfs_rename(nfs_st.ctx, nfs_path, backup_path);
      }
      else
         rc = rnfs_unlink(nfs_st.ctx, nfs_path);
      if (rc != 0)
      {
         RARCH_ERR(NFSPFX "Could not replace '%s': %s\n",
               nfs_path, rnfs_get_error(nfs_st.ctx));
         rnfs_unlink(nfs_st.ctx, tmp_path);
         cb(user_data, path, false, rfile);
         return true;
      }
   }

   if (rnfs_rename(nfs_st.ctx, tmp_path, nfs_path) != 0)
   {
      RARCH_ERR(NFSPFX "Rename '%s' -> '%s' failed: %s\n",
            tmp_path, nfs_path, rnfs_get_error(nfs_st.ctx));
      if (keep_old)
         rnfs_rename(nfs_st.ctx, backup_path, nfs_path);
      rnfs_unlink(nfs_st.ctx, tmp_path);
      cb(user_data, path, false, rfile);
      return true;
   }

   cb(user_data, path, true, rfile);
   return true;
}

static bool nfs_free(const char *path, cloud_sync_complete_handler_t cb,
      void *user_data)
{
   char nfs_path[PATH_MAX_LENGTH];
   struct rnfs_stat st;
   settings_t *settings = config_get_ptr();
   int rc;

   nfs_sync_build_path(nfs_path, sizeof(nfs_path), nfs_st.subdir, path);
   if (rnfs_stat(nfs_st.ctx, nfs_path, &st) != 0)
   {
      if (rnfs_get_status(nfs_st.ctx) == NFS_STATUS_NOENT)
      {
         cb(user_data, path, true, NULL);
         return true;
      }
      cb(user_data, path, false, NULL);
      return true;
   }

   if (settings->bools.cloud_sync_destructive)
      rc = rnfs_unlink(nfs_st.ctx, nfs_path);
   else
   {
      char backup_path[PATH_MAX_LENGTH];
      nfs_backup_path(backup_path, sizeof(backup_path), nfs_path);
      rc = rnfs_rename(nfs_st.ctx, nfs_path, backup_path);
   }
   if (rc != 0)
      RARCH_ERR(NFSPFX "Could not remove '%s': %s\n",
            nfs_path, rnfs_get_error(nfs_st.ctx));
   cb(user_data, path, rc == 0, NULL);
   return true;
}

cloud_sync_driver_t cloud_sync_nfs = {
   nfs_sync_begin,
   nfs_sync_end,
   nfs_read,
   nfs_update,
   nfs_free,
   "nfs"
};
