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
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <vfs/vfs_implementation.h>
#include "../configuration.h"
#include "../verbosity.h"
#include "vfs_implementation_smb.h"

#define SMB_PREFIX "smb://"

static struct smb2_context *smb_context = NULL;
static bool smb_initialized = false;

/* Initialize SMB context */
static bool smb_init(void)
{
   settings_t *settings;
   char server[256];
   char share[256];
   char *username = NULL;
   const char *p;
   const char *start;
   size_t len;

   if (smb_initialized && smb_context)
      return true;

   RARCH_DBG("[SMB] Initializing SMB client context\n");

   settings = config_get_ptr();
   if (!settings)
   {
      RARCH_ERR("[SMB] Cannot retrieve settings for authentication\n");
      return false;
   }

   smb_context = smb2_init_context();
   if (!smb_context)
   {
      RARCH_ERR("[SMB] Failed to create new context\n");
      return false;
   }

   /* Parse server and share from settings */
   if (string_is_empty(settings->arrays.smb_client_server_address))
   {
      RARCH_ERR("[SMB] Server address not configured\n");
      smb2_destroy_context(smb_context);
      smb_context = NULL;
      return false;
   }

   strlcpy(server, settings->arrays.smb_client_server_address, sizeof(server));

   /* Set credentials */
   if (!string_is_empty(settings->arrays.smb_client_username))
   {
      username = settings->arrays.smb_client_username;
      smb2_set_user(smb_context, username);
   }

   if (!string_is_empty(settings->arrays.smb_client_password))
      smb2_set_password(smb_context, settings->arrays.smb_client_password);

   if (!string_is_empty(settings->arrays.smb_client_workgroup))
      smb2_set_domain(smb_context, settings->arrays.smb_client_workgroup);

   if (!string_is_empty(settings->arrays.smb_client_share))
      strlcpy(share, settings->arrays.smb_client_share, sizeof(share));
   else
      share[0] = '\0';

   switch(settings->ints.smb_client_auth_mode)
   {
      case SMB2_SEC_NTLMSSP:
         smb2_set_security_mode(smb_context, SMB2_SEC_NTLMSSP);
         smb2_set_authentication(smb_context, SMB2_SEC_NTLMSSP);
         break;
      case SMB2_SEC_KRB5:
         smb2_set_security_mode(smb_context, SMB2_SEC_KRB5);
         smb2_set_authentication(smb_context, SMB2_SEC_KRB5);
         break;
      case SMB2_SEC_UNDEFINED:
      default:
         break;
   }

   /* Connect to share */
   if (smb2_connect_share(smb_context, server, share, username) < 0)
   {
      RARCH_ERR("[SMB] Failed to connect to %s/%s: %s\n",
                server, share, smb2_get_error(smb_context));
      smb2_destroy_context(smb_context);
      smb_context = NULL;
      return false;
   }

   smb_initialized = true;
   RARCH_DBG("[SMB] SMB client initialized successfully\n");

   return true;
}

/* Shutdown SMB context */
void smb_shutdown(void)
{
   if (smb_context)
   {
      RARCH_DBG("[SMB] Shutting down SMB client\n");
      smb2_disconnect_share(smb_context);
      smb2_destroy_context(smb_context);
      smb_context = NULL;
      smb_initialized = false;
   }
}

/* Build full SMB path from settings */
static bool smb_build_path(char *dest, size_t dest_size, const char *relative_path)
{
   settings_t *settings = config_get_ptr();
   char temp_path[PATH_MAX_LENGTH];
   const char *p;
   
   if (!settings)
   {
      RARCH_ERR("[SMB] Cannot retrieve settings\n");
      return false;
   }

   /* If already has smb:// prefix, extract just the path component */
   if (string_starts_with(relative_path, SMB_PREFIX))
   {
      p = relative_path + strlen(SMB_PREFIX);
      /* Skip server */
      while (*p && *p != '/')
         p++;
      if (*p == '/')
         p++;
      /* Skip share */
      while (*p && *p != '/')
         p++;

      strlcpy(dest, p, dest_size);
      return true;
   }

   /* Build path from settings */
   temp_path[0] = '\0';

   /* Add base folder if specified */
   if (!string_is_empty(settings->arrays.smb_client_subdir))
   {
      strlcpy(temp_path, settings->arrays.smb_client_subdir, sizeof(temp_path));
      if (temp_path[strlen(temp_path) - 1] != '/')
         strlcat(temp_path, "/", sizeof(temp_path));
   }

   /* Add relative path if provided */
   if (relative_path && relative_path[0])
   {
      if (relative_path[0] == '/')
         relative_path++;
      strlcat(temp_path, relative_path, sizeof(temp_path));
   }

   strlcpy(dest, temp_path, dest_size);
   RARCH_DBG("[SMB] Built path: %s\n", dest);

   return true;
}

bool retro_vfs_file_open_smb(libretro_vfs_implementation_file *stream,
   const char *path, unsigned mode, unsigned hints)
{
   char full_path[PATH_MAX_LENGTH];
   struct smb2fh *fh;
   int flags = 0;

   if (!stream)
      return false;

   /* reset file handle */
   stream->smb_fh = (intptr_t)0;

   if (!smb_init() || !smb_context)
   {
      RARCH_ERR("[SMB] Failed to initialize SMB context\n");
      return false;
   }

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return false;

   /* Strip leading slash ONLY for non-empty subpaths */
   if (full_path[0] == '/' && full_path[1] != '\0')
      strlcpy(full_path, full_path + 1, sizeof(full_path));

   /* Do not treat empty string as a file path */
   if (full_path[0] == '\0')
   {
      RARCH_ERR("[SMB] Refusing to open empty file path\n");
      return false;
   }

   /* Convert mode to SMB flags safely */
   if (mode & RETRO_VFS_FILE_ACCESS_READ)
   {
      if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
         flags = O_RDWR;
      else
         flags = O_RDONLY;
   }
   else if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
   {
      flags = O_WRONLY;
   }

   if ((mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) &&
       (mode & RETRO_VFS_FILE_ACCESS_WRITE))
      flags |= O_CREAT;

   fh = smb2_open(smb_context, full_path, flags);
   if (!fh)
   {
      RARCH_ERR("[SMB] Failed to open '%s': %s\n",
                full_path, smb2_get_error(smb_context));
      return false;
   }

   stream->smb_fh = (intptr_t)(uintptr_t)fh;
   stream->scheme = VFS_SCHEME_SMB; /* ensure SMB dispatch on IO calls */
   RARCH_DBG("[SMB] Opened file: %s (fd: %p)\n", full_path, fh);
   return true;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   int ret;

   if (!stream || stream->smb_fh < 0)
      return -1;

   ret = smb2_read(smb_context, (struct smb2fh *)(intptr_t)stream->smb_fh, s, len);
   if (ret < 0)
      RARCH_ERR("[SMB] Read error: %s\n", smb2_get_error(smb_context));

   return ret;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   int ret;

   if (!stream || stream->smb_fh < 0)
      return -1;

   ret = smb2_write(smb_context, (struct smb2fh *)(intptr_t)stream->smb_fh, (void*)s, len);
   if (ret < 0)
      RARCH_ERR("[SMB] Write error: %s\n", smb2_get_error(smb_context));

   return ret;
}

int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
   int64_t offset, int whence)
{
   uint64_t newpos = 0;
   struct smb2fh *fh;
   int64_t ret;

   if (!stream || !smb_context)
       return -1;

   /* fd holds the pointer returned by smb2_open(); */
   if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
       return -1;

   /* Reconstruct the exact pointer safely */
   fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   if (!fh)
       return -1;

   /* Only allow valid values */
   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
       return -1;

   /* libsmb2 returns status via ret, and the new offset via out param */
   ret = smb2_lseek(smb_context, fh, offset, whence, &newpos);
   if (ret < 0)
   {
       RARCH_ERR("[SMB] Seek error: %s\n", smb2_get_error(smb_context));
       return -1;
   }

   return (int64_t)newpos;
}

int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream)
{
    uint64_t cur = 0;
    struct smb2fh *fh;
    int64_t ret;

    if (!stream || !smb_context)
        return -1;

    if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
        return -1;

    fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
    if (!fh)
        return -1;

    ret = smb2_lseek(smb_context, fh, 0, SEEK_CUR, &cur);
    if (ret < 0)
    {
        RARCH_ERR("[SMB] Tell error: %s\n", smb2_get_error(smb_context));
        return -1;
    }

    return (int64_t)cur;
}

int retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream)
{
   int ret;

   if (!stream || stream->smb_fh < 0)
      return -1;

   ret = smb2_close(smb_context, (struct smb2fh *)(intptr_t)stream->smb_fh);
   if (ret < 0)
      RARCH_WARN("[SMB] Error closing: %s\n", smb2_get_error(smb_context));

   return ret;
}

intptr_t retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   struct smb2dir *dir;

   (void)include_hidden;

   if (!smb_init())
      return (intptr_t)0;

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return (intptr_t)0;

   /* If we have a leading slash AND a non-empty remainder, strip it.
    * Do NOT convert empty string to "." — root listing worked with "" */
   if (full_path[0] == '/' && full_path[1] != '\0')
      strlcpy(full_path, full_path + 1, sizeof(full_path));

   dir = smb2_opendir(smb_context, full_path);
   if (!dir)
   {
      RARCH_ERR("[SMB] opendir '%s' failed: %s\n", full_path, smb2_get_error(smb_context));
      return (intptr_t)0;
   }

   return (intptr_t)dir;
}

struct smbc_dirent* retro_vfs_readdir_smb(intptr_t dh)
{
   struct smb2dirent *ent;
   static struct smbc_dirent result;

   if (!dh)
      return NULL;

   ent = smb2_readdir(smb_context, (struct smb2dir *)dh);
   if (!ent)
      return NULL;

   memset(&result, 0, sizeof(result));
   strlcpy(result.name, ent->name ? ent->name : "", sizeof(result.name));

   result.type = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) ? 1 : 0;
   result.size = ent->st.smb2_size;

   return &result;
}

int retro_vfs_closedir_smb(intptr_t dh)
{
   if (!dh)
      return -1;

   smb2_closedir(smb_context, (struct smb2dir *)dh);
   return 0;
}

int retro_vfs_stat_smb(const char *path, int32_t *size)
{
    char rel_path[PATH_MAX_LENGTH];
    struct smb2_stat_64 st;

    if (!smb_init())
        return 0;

    if (!smb_build_path(rel_path, sizeof(rel_path), path))
        return 0;

    /* Root-of-share => "." */
    if (rel_path[0] == '\0')
        strlcpy(rel_path, ".", sizeof(rel_path));

    /* Strip leading slash safely (preserve NULL terminator) */
    if (rel_path[0] == '/' && rel_path[1] != '\0')
        strlcpy(rel_path, rel_path + 1, sizeof(rel_path));

    if (smb2_stat(smb_context, rel_path, &st) < 0)
    {
        RARCH_ERR("[SMB] stat '%s' failed: %s\n", rel_path, smb2_get_error(smb_context));
        return 0;
    }

    if (size)
        *size = (int32_t)st.smb2_size;

    return RETRO_VFS_STAT_IS_VALID |
           (st.smb2_type == SMB2_TYPE_DIRECTORY ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}

int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream)
{
    if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
        return -1;

    if (!smb_context)
        return -1;

    const char *err = smb2_get_error(smb_context);
    if (err && err[0] != '\0')
    {
        RARCH_ERR("[SMB] Last error: %s\n", err);
        return -1;
    }

    return 0;
}
