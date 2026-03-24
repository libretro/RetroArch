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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#include "../cloud_sync_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define SMBPFX "[SMB] "

/* ========== State ========== */

typedef struct
{
   struct smb2_context *ctx;
   char subdir[PATH_MAX_LENGTH];
} smb_sync_state_t;

static smb_sync_state_t smb_st = {0};

/* ========== Helpers ========== */

/* Build an SMB-relative path: {subdir}/{path}.
 * libsmb2 paths must NOT have a leading '/'. */
static void smb_sync_build_path(char *dest, size_t dest_size,
      const char *subdir, const char *path)
{
   dest[0] = '\0';

   if (!string_is_empty(subdir))
   {
      strlcpy(dest, subdir, dest_size);
      if (dest[0] && dest[strlen(dest) - 1] != '/')
         strlcat(dest, "/", dest_size);
   }

   if (path)
   {
      while (*path == '/')
         path++;
      strlcat(dest, path, dest_size);
   }
}

/* Recursively ensure all directories in `path` exist.
 * Walks each '/'-separated segment, calling smb2_mkdir() for each.
 * Ignores "already exists" errors. */
static bool smb_sync_ensure_dir(struct smb2_context *ctx, const char *path)
{
   char buf[PATH_MAX_LENGTH];
   char *p;
   int rc;

   strlcpy(buf, path, sizeof(buf));

   for (p = buf; *p; p++)
   {
      if (*p != '/')
         continue;

      *p = '\0';
      if (buf[0])
      {
         rc = smb2_mkdir(ctx, buf);
         if (rc < 0 && rc != -EEXIST)
         {
            RARCH_ERR(SMBPFX "mkdir '%s' failed: %s\n",
                  buf, smb2_get_error(ctx));
            return false;
         }
      }
      *p = '/';
   }

   /* Final segment (if path doesn't end with '/') */
   if (buf[0])
   {
      rc = smb2_mkdir(ctx, buf);
      if (rc < 0 && rc != -EEXIST)
      {
         RARCH_ERR(SMBPFX "mkdir '%s' failed: %s\n",
               buf, smb2_get_error(ctx));
         return false;
      }
   }

   return true;
}

/* Ensure parent directories of `path` exist.
 * E.g., for "saves/snes/game.srm", ensures "saves/" and "saves/snes/" exist. */
static bool smb_sync_ensure_parent_dir(struct smb2_context *ctx, const char *path)
{
   char dir[PATH_MAX_LENGTH];
   char *last_slash;

   strlcpy(dir, path, sizeof(dir));
   last_slash = strrchr(dir, '/');
   if (!last_slash)
      return true; /* no parent directory */

   *last_slash = '\0';
   return smb_sync_ensure_dir(ctx, dir);
}

/* ========== Driver functions ========== */

static bool smb_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   settings_t *settings = config_get_ptr();
   const char *server   = settings->arrays.smb_client_server_address;
   const char *share    = settings->arrays.smb_client_share;
   const char *user     = settings->arrays.smb_client_username;
   const char *password = settings->arrays.smb_client_password;
   const char *workgroup = settings->arrays.smb_client_workgroup;
   const char *subdir   = settings->arrays.smb_client_subdir;
   unsigned auth_mode   = settings->uints.smb_client_auth_mode;
   unsigned timeout     = settings->uints.smb_client_timeout;
   struct smb2_context *ctx;
   int rc;

   if (string_is_empty(server) || string_is_empty(share))
   {
      RARCH_ERR(SMBPFX "Server address and share must be configured\n");
      cb(user_data, NULL, false, NULL);
      return true;
   }

   ctx = smb2_init_context();
   if (!ctx)
   {
      RARCH_ERR(SMBPFX "Failed to create SMB context\n");
      cb(user_data, NULL, false, NULL);
      return true;
   }

   if (!string_is_empty(user))
      smb2_set_user(ctx, user);
   if (!string_is_empty(password))
      smb2_set_password(ctx, password);
   if (!string_is_empty(workgroup))
      smb2_set_domain(ctx, workgroup);
   if (timeout > 0)
      smb2_set_timeout(ctx, (int)timeout);
   smb2_set_security_mode(ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
   smb2_set_authentication(ctx, (int)auth_mode);

   RARCH_LOG(SMBPFX "Connecting to %s/%s as %s\n",
         server, share, string_is_empty(user) ? "(anonymous)" : user);

   rc = smb2_connect_share(ctx, server, share,
         string_is_empty(user) ? NULL : user);
   if (rc < 0)
   {
      RARCH_ERR(SMBPFX "Connect failed: %s\n", smb2_get_error(ctx));
      smb2_destroy_context(ctx);
      cb(user_data, NULL, false, NULL);
      return true;
   }

   smb_st.ctx = ctx;

   /* Build cloud sync subdir: {smb_client_subdir}/cloud_sync */
   smb_st.subdir[0] = '\0';
   if (!string_is_empty(subdir))
   {
      strlcpy(smb_st.subdir, subdir, sizeof(smb_st.subdir));
      strlcat(smb_st.subdir, "/", sizeof(smb_st.subdir));
   }
   strlcat(smb_st.subdir, "cloud_sync", sizeof(smb_st.subdir));

   /* Ensure subdir exists */
   {
      if (!smb_sync_ensure_dir(ctx, smb_st.subdir))
      {
         RARCH_ERR(SMBPFX "Failed to create subdir '%s'\n", smb_st.subdir);
         smb2_disconnect_share(ctx);
         smb2_destroy_context(ctx);
         smb_st.ctx = NULL;
         cb(user_data, NULL, false, NULL);
         return true;
      }
   }

   RARCH_LOG(SMBPFX "Connected successfully\n");
   cb(user_data, NULL, true, NULL);
   return true;
}

static bool smb_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   if (smb_st.ctx)
   {
      smb2_disconnect_share(smb_st.ctx);
      smb2_destroy_context(smb_st.ctx);
      smb_st.ctx = NULL;
   }
   smb_st.subdir[0] = '\0';

   cb(user_data, NULL, true, NULL);
   return true;
}

static bool smb_read(const char *path, const char *file,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char smb_path[PATH_MAX_LENGTH];
   struct smb2_stat_64 st;
   struct smb2fh *fh;
   uint8_t *buf       = NULL;
   uint64_t file_size = 0;
   uint64_t offset    = 0;
   uint32_t chunk;
   RFILE *rfile       = NULL;
   int rc;

   smb_sync_build_path(smb_path, sizeof(smb_path), smb_st.subdir, path);

   /* Check if file exists */
   rc = smb2_stat(smb_st.ctx, smb_path, &st);
   if (rc < 0)
   {
      if (-rc == ENOENT)
      {
         /* File doesn't exist on server — that's a valid "success" */
         RARCH_DBG(SMBPFX "File not found: %s\n", smb_path);
         cb(user_data, path, true, NULL);
         return true;
      }
      RARCH_ERR(SMBPFX "Stat '%s' failed: %s\n",
            smb_path, smb2_get_error(smb_st.ctx));
      cb(user_data, path, false, NULL);
      return true;
   }

   file_size = st.smb2_size;

   fh = smb2_open(smb_st.ctx, smb_path, O_RDONLY);
   if (!fh)
   {
      RARCH_ERR(SMBPFX "Failed to open '%s': %s\n",
            smb_path, smb2_get_error(smb_st.ctx));
      cb(user_data, path, false, NULL);
      return true;
   }

   buf = (uint8_t *)malloc((size_t)file_size);
   if (!buf)
   {
      smb2_close(smb_st.ctx, fh);
      cb(user_data, path, false, NULL);
      return true;
   }

   chunk = smb2_get_max_read_size(smb_st.ctx);
   while (offset < file_size)
   {
      uint32_t to_read = (uint32_t)((file_size - offset) < chunk
            ? (file_size - offset) : chunk);
      int n = smb2_read(smb_st.ctx, fh, buf + offset, to_read);
      if (n < 0)
      {
         RARCH_ERR(SMBPFX "Read error on '%s': %s\n",
               smb_path, smb2_get_error(smb_st.ctx));
         free(buf);
         smb2_close(smb_st.ctx, fh);
         cb(user_data, path, false, NULL);
         return true;
      }
      if (n == 0)
         break; /* EOF */
      offset += (uint32_t)n;
   }

   smb2_close(smb_st.ctx, fh);

   /* Write to local file */
   rfile = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ_WRITE, 0);
   if (!rfile)
   {
      RARCH_ERR(SMBPFX "Failed to open local file '%s'\n", file);
      free(buf);
      cb(user_data, path, false, NULL);
      return true;
   }

   filestream_write(rfile, buf, (int64_t)offset);
   filestream_seek(rfile, 0, SEEK_SET);
   free(buf);

   RARCH_DBG(SMBPFX "Read %s (%u bytes)\n", smb_path, (unsigned)offset);
   cb(user_data, path, true, rfile);
   return true;
}

static bool smb_update(const char *path, RFILE *rfile,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char smb_path[PATH_MAX_LENGTH];
   struct smb2fh *fh;
   int64_t file_size;
   uint8_t *buf;
   uint64_t offset = 0;
   uint32_t chunk;

   smb_sync_build_path(smb_path, sizeof(smb_path), smb_st.subdir, path);

   /* Ensure parent directories exist */
   if (!smb_sync_ensure_parent_dir(smb_st.ctx, smb_path))
   {
      RARCH_ERR(SMBPFX "Failed to create parent dirs for '%s'\n", smb_path);
      cb(user_data, path, false, rfile);
      return true;
   }

   /* Read the local file into memory */
   file_size = filestream_get_size(rfile);
   if (file_size < 0)
      file_size = 0;

   buf = (uint8_t *)malloc((size_t)file_size);
   if (!buf && file_size > 0)
   {
      cb(user_data, path, false, rfile);
      return true;
   }

   if (file_size > 0)
      filestream_read(rfile, buf, file_size);

   /* Open remote file for writing (create if needed) */
   fh = smb2_open(smb_st.ctx, smb_path, O_WRONLY | O_CREAT);
   if (!fh)
   {
      RARCH_ERR(SMBPFX "Failed to open '%s' for writing: %s\n",
            smb_path, smb2_get_error(smb_st.ctx));
      free(buf);
      cb(user_data, path, false, rfile);
      return true;
   }

   /* Truncate to 0 in case file already existed with larger content */
   smb2_ftruncate(smb_st.ctx, fh, 0);

   chunk = smb2_get_max_write_size(smb_st.ctx);
   while (offset < (uint64_t)file_size)
   {
      uint32_t to_write = (uint32_t)(((uint64_t)file_size - offset) < chunk
            ? ((uint64_t)file_size - offset) : chunk);
      int n = smb2_write(smb_st.ctx, fh, buf + offset, to_write);
      if (n < 0)
      {
         RARCH_ERR(SMBPFX "Write error on '%s': %s\n",
               smb_path, smb2_get_error(smb_st.ctx));
         free(buf);
         smb2_close(smb_st.ctx, fh);
         cb(user_data, path, false, rfile);
         return true;
      }
      offset += (uint32_t)n;
   }

   smb2_close(smb_st.ctx, fh);
   free(buf);

   RARCH_DBG(SMBPFX "Updated %s (%u bytes)\n",
         smb_path, (unsigned)file_size);
   cb(user_data, path, true, rfile);
   return true;
}

static bool smb_free(const char *path,
      cloud_sync_complete_handler_t cb, void *user_data)
{
   char smb_path[PATH_MAX_LENGTH];
   struct smb2_stat_64 st;
   settings_t *settings = config_get_ptr();
   int rc;

   smb_sync_build_path(smb_path, sizeof(smb_path), smb_st.subdir, path);

   /* Check if file exists */
   rc = smb2_stat(smb_st.ctx, smb_path, &st);
   if (rc < 0)
   {
      if (-rc == ENOENT)
      {
         /* Already gone — success */
         cb(user_data, path, true, NULL);
         return true;
      }
      RARCH_ERR(SMBPFX "Stat '%s' failed: %s\n",
            smb_path, smb2_get_error(smb_st.ctx));
      cb(user_data, path, false, NULL);
      return true;
   }

   if (settings->bools.cloud_sync_destructive)
   {
      rc = smb2_unlink(smb_st.ctx, smb_path);
      if (rc < 0)
      {
         RARCH_ERR(SMBPFX "Delete '%s' failed: %s\n",
               smb_path, smb2_get_error(smb_st.ctx));
         cb(user_data, path, false, NULL);
         return true;
      }
      RARCH_DBG(SMBPFX "Deleted %s\n", smb_path);
   }
   else
   {
      /* Non-destructive: rename with timestamp */
      char new_path[PATH_MAX_LENGTH];
      time_t t;
      struct tm tm_buf;
      char ts[16];

      time(&t);
      rtime_localtime(&t, &tm_buf);
      snprintf(ts, sizeof(ts), "-%02d%02d%02d-%02d%02d%02d",
            tm_buf.tm_year % 100, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

      strlcpy(new_path, smb_path, sizeof(new_path));
      strlcat(new_path, ts, sizeof(new_path));

      /* Ensure parent dir of backup path exists */
      smb_sync_ensure_parent_dir(smb_st.ctx, new_path);

      rc = smb2_rename(smb_st.ctx, smb_path, new_path);
      if (rc < 0)
      {
         RARCH_ERR(SMBPFX "Rename '%s' -> '%s' failed: %s\n",
               smb_path, new_path, smb2_get_error(smb_st.ctx));
         cb(user_data, path, false, NULL);
         return true;
      }
      RARCH_DBG(SMBPFX "Backed up %s -> %s\n", smb_path, new_path);
   }

   cb(user_data, path, true, NULL);
   return true;
}

/* ========== Registration ========== */

cloud_sync_driver_t cloud_sync_smb = {
   smb_sync_begin,
   smb_sync_end,
   smb_read,
   smb_update,
   smb_free,
   "smb"
};
