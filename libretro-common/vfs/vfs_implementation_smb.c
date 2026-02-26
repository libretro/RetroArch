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
#include <net/net_compat.h>
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_smb.h"

#define SMB_PREFIX "smb://"

static struct smb2_context **smb_context_pool = NULL;
static int next_context_index = 0;
static bool smb_initialized = false;
static int max_context_configured = 0;
static const struct smb_settings *smb_cfg = NULL;

static struct smb2_context *get_smb_context()
{
   int idx;

   if (!smb_initialized)
      return NULL;

   if (!smb_context_pool || max_context_configured == 0)
      return NULL;

   if (next_context_index < 0 || next_context_index >= max_context_configured)
      next_context_index = 0;

   idx = next_context_index;
   next_context_index = (next_context_index + 1) % max_context_configured;

   if (!smb_context_pool[idx])
      return NULL;

   return smb_context_pool[idx];
}

void reset(unsigned num_contexts)
{
   for (unsigned i = 0; i < num_contexts; i++)
   {
      if(smb_context_pool[i])
         smb2_destroy_context(smb_context_pool[i]);
   }

   free(smb_context_pool);
   smb_context_pool = NULL;

   smb_initialized = false;
   next_context_index = 0;
   max_context_configured = 0;
}

bool smb_init_cfg(const struct smb_settings *new_cfg)
{
   smb_cfg = new_cfg;
   return true;
}

/* Initialize SMB context */
static bool smb_init()
{
   char server[256];
   char share[256];
   char *username = NULL;
   unsigned i;
   int error_no = 0;

   if (smb_initialized)
      return true;

   if (!network_init())
      return false;

   if (!smb_cfg || string_is_empty(smb_cfg->server_address))
      return false;

   unsigned max_smb_contexts = smb_cfg->num_contexts;
   smb_context_pool = calloc(max_smb_contexts, sizeof(struct smb2_context *));

   if (!smb_context_pool)
      return false;

   for (i = 0; i < max_smb_contexts; i++)
   {
      struct smb2_context *smb_context = smb2_init_context();

      if (!smb_context)
      {
         reset(max_context_configured);
         return false;
      }

      strlcpy(server, smb_cfg->server_address, sizeof(server));

      /* Set credentials */
      if (!string_is_empty(smb_cfg->username))
      {
         username = (char*)smb_cfg->username;
         smb2_set_user(smb_context, username);
      }

      if (!string_is_empty(smb_cfg->password))
         smb2_set_password(smb_context, smb_cfg->password);

      if (!string_is_empty(smb_cfg->workgroup))
         smb2_set_domain(smb_context, smb_cfg->workgroup);

      if (!string_is_empty(smb_cfg->share))
         strlcpy(share, smb_cfg->share, sizeof(share));
      else
         share[0] = '\0';

      /* set timeout */
      smb2_set_timeout(smb_context, smb_cfg->timeout);

      /* SMB2_SEC_ defines missing on system headers but provided with latest libsmb2 */
      switch(smb_cfg->auth_mode)
      {
         case RETRO_SMB2_SEC_NTLMSSP:
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            break;
         case RETRO_SMB2_SEC_KRB5:
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_KRB5);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_KRB5);
            break;
         case RETRO_SMB2_SEC_UNDEFINED:
         default:
            /* Only probe auth mode on the first context */
            if (i == 0)
            {
               /* first try SMB2_SEC_KRB5 */
               smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_KRB5);
               smb2_set_authentication(smb_context, RETRO_SMB2_SEC_KRB5);

               if (smb2_connect_share(smb_context, server, share, username) == 0)
               {
                  /* KRB5 worked — use it for all remaining contexts */
                  smb_context_pool[i] = smb_context;
                  max_context_configured = i + 1;
                  continue;
               }

               /* reset to we can use it again */
               smb2_destroy_context(smb_context);
               smb_context = smb2_init_context();

               if (!smb_context)
               {
                  reset(max_context_configured);
                  return false;
               }

               smb2_set_user(smb_context, username);
               if (!string_is_empty(smb_cfg->password))
                  smb2_set_password(smb_context, smb_cfg->password);
               if (!string_is_empty(smb_cfg->workgroup))
                  smb2_set_domain(smb_context, smb_cfg->workgroup);
               smb2_set_timeout(smb_context, smb_cfg->timeout);
            }

            /* if that fails, try SMB2_SEC_KRB5 in fallthrough */
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            break;
      }

      /* Connect to share */
      if ((error_no = smb2_connect_share(smb_context, server, share, username)) < 0)
      {
         smb2_destroy_context(smb_context);
         reset(max_context_configured);
         return false;
      }

      smb_context_pool[i] = smb_context;
      max_context_configured = i + 1;
   }

   smb_initialized = true;

   return true;
}

void smb_close_context(int index)
{
   if (index < 0 || index >= max_context_configured)
      return;

   if (smb_context_pool[index])
   {
      smb2_disconnect_share(smb_context_pool[index]);
      smb2_destroy_context(smb_context_pool[index]);
      smb_context_pool[index] = NULL;
   }
}

/* Shutdown SMB context - called on exit */
void smb_shutdown()
{
   int i;

   if(!smb_initialized || max_context_configured == 0)
      return;

   for (i = 0; i < max_context_configured; i++)
      smb_close_context(i);

   reset(max_context_configured);
}

/* Build full SMB path from settings */
static bool smb_build_path(char *dest, size_t dest_size, const char *relative_path)
{
   char temp_path[PATH_MAX_LENGTH];
   const char *p;

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
   if (!string_is_empty(smb_cfg->subdir))
   {
      strlcpy(temp_path, smb_cfg->subdir, sizeof(temp_path));
      if (temp_path[0] != '\0' && temp_path[strlen(temp_path) - 1] != '/')
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

   return true;
}

bool retro_vfs_file_open_smb(libretro_vfs_implementation_file *stream,
   const char *path, unsigned mode, unsigned hints)
{
   char full_path[PATH_MAX_LENGTH];
   struct smb2fh *fh;
   int flags = 0;
   struct smb2_context *smb_context;

   if (!stream)
      return false;

   /* reset file handle */
   stream->smb_fh = (intptr_t)0;
   stream->smb_ctx = (intptr_t)0;

   if (!smb_init())
      return false;

   smb_context = get_smb_context();
   if (!smb_context)
      return false;

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return false;

   /* Strip leading slash ONLY for non-empty subpaths */
   if (full_path[0] == '/' && full_path[1] != '\0')
      memmove(full_path, full_path + 1, strlen(full_path));

   /* Do not treat empty string as a file path */
   if (full_path[0] == '\0')
      return false;

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
      return false;

   stream->smb_fh = (intptr_t)(uintptr_t)fh;
   stream->smb_ctx = (intptr_t)(uintptr_t)smb_context;
   stream->scheme = VFS_SCHEME_SMB; /* ensure SMB dispatch on IO calls */
   return true;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   int ret;
   struct smb2_context *ctx;

   if (!smb_initialized || !stream || stream->smb_fh < 0)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
       return -1;

   ret = smb2_read(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh, s, len);

   return ret;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   int ret;
   struct smb2_context *ctx;

   if (!smb_initialized || !stream || stream->smb_fh < 0)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
       return -1;

   ret = smb2_write(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh, (void*)s, len);

   return ret;
}

int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
   int64_t offset, int whence)
{
   uint64_t newpos = 0;
   struct smb2fh *fh;
   struct smb2_context *ctx;
   int64_t ret;

   if (!smb_initialized || !stream || !stream->smb_ctx)
      return -1;

   /* fd holds the pointer returned by smb2_open(); */
   if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   /* Reconstruct the exact pointer safely */
   fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;

   /* Only allow valid values */
   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
      return -1;

   /* libsmb2 returns status via ret, and the new offset via out param */
   ret = smb2_lseek(ctx, fh, offset, whence, &newpos);
   if (ret < 0)
      return -1;

   return (int64_t)newpos;
}

/* return the current byte offset in an open file */
int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream)
{
   uint64_t cur = 0;
   struct smb2fh *fh;
   struct smb2_context *ctx;
   int64_t ret;

   if (!smb_initialized || !stream || !stream->smb_ctx)
      return -1;

   if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;

   ret = smb2_lseek(ctx, fh, 0, SEEK_CUR, &cur);
   if (ret < 0)
      return -1;

   return (int64_t)cur;
}

int retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream)
{
   int ret;
   struct smb2_context *ctx;

   /* during shutdown */
   if (!smb_initialized)
      return -1;

   if (!stream || stream->smb_fh < 0)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;

   ret = smb2_close(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh);

   stream->smb_fh = (intptr_t)-1;
   stream->smb_ctx = (intptr_t)0;

   return ret;
}

smb_dir_handle* retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   struct smb2dir *dir;
   struct smb2_context *smb_context;
   smb_dir_handle *handle;

   (void)include_hidden;

   if (!smb_init())
      return NULL;

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return NULL;

   /* Root-of-share: bare "/" should become "" for libsmb2 opendir */
   if (full_path[0] == '/' && full_path[1] == '\0')
      full_path[0] = '\0';
   /* Strip leading slash for non-root subpaths */
   else if (full_path[0] == '/' && full_path[1] != '\0')
      memmove(full_path, full_path + 1, strlen(full_path));

   smb_context = get_smb_context();
   if (!smb_context)
      return NULL;

   dir = smb2_opendir(smb_context, full_path);
   if (!dir)
      return NULL;

   handle = (smb_dir_handle*)malloc(sizeof(smb_dir_handle));
   if (!handle)
   {
      smb2_closedir(smb_context, dir);
      return NULL;
   }

   handle->ctx = smb_context;
   handle->dir = dir;

   return handle;
}

struct smbc_dirent* retro_vfs_readdir_smb(smb_dir_handle* dh)
{
   struct smb2dirent *ent;
   static struct smbc_dirent result;

   if (!smb_initialized || !dh)
      return NULL;

   if (!dh->ctx || !dh->dir)
      return NULL;

   ent = smb2_readdir(dh->ctx, dh->dir);
   if (!ent)
      return NULL;

   memset(&result, 0, sizeof(result));
   strlcpy(result.name, ent->name ? ent->name : "", sizeof(result.name));

   result.type = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) ? 1 : 0;
   result.size = ent->st.smb2_size;

   return &result;
}

int retro_vfs_closedir_smb(smb_dir_handle* dh)
{
   if (!smb_initialized || !dh)
      return -1;

   if (!dh->ctx || !dh->dir)
      return -1;

   smb2_closedir(dh->ctx, dh->dir);
   free(dh);
   return 0;
}

int retro_vfs_stat_smb(const char *path, int32_t *size)
{
   char rel_path[PATH_MAX_LENGTH];
   struct smb2_stat_64 st;
   struct smb2_context *smb_context;

   if (!smb_init())
      return 0;

   if (!smb_build_path(rel_path, sizeof(rel_path), path))
      return 0;

   /* Root-of-share: normalize "/" to "" for libsmb2 */
   if (rel_path[0] == '/' && rel_path[1] == '\0')
      rel_path[0] = '\0';

   /* Strip leading slash safely (preserve NULL terminator) */
   if (rel_path[0] == '/' && rel_path[1] != '\0')
      memmove(rel_path, rel_path + 1, strlen(rel_path));

   smb_context = get_smb_context();
   if (!smb_context)
      return 0;

   if (smb2_stat(smb_context, rel_path, &st) < 0)
      return 0;

   if (size)
      *size = (int32_t)st.smb2_size;

   return RETRO_VFS_STAT_IS_VALID |
         (st.smb2_type == SMB2_TYPE_DIRECTORY ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}

int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream)
{
   struct smb2_context *ctx;
   const char *err;

   if (!smb_initialized)
      return -1;

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (!stream->smb_ctx)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;

   err = smb2_get_error(ctx);
   if (err && err[0] != '\0')
      return -1;

   return 0;
}
