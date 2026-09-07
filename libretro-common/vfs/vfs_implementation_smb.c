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
#include <stdint.h>  /* UINT32_MAX, INT64_MAX -- via compat shim on VC6 */
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-dcerpc.h>
#include <smb2/libsmb2-dcerpc-srvsvc.h>
#include <net/net_socket.h>
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
/* Auth mode that actually worked at init, reused when healing contexts */
static int resolved_auth_mode = RETRO_SMB2_SEC_NTLMSSP;

/* Extracts the first path component after the server from an smb:// URL.
 * Returns false when the URL names a server and nothing else. */
static bool smb_url_share(const char *path, char *s, size_t len)
{
   const char *p;
   const char *end;
   size_t _len;

   s[0] = '\0';

   if (!path || !string_starts_with(path, SMB_PREFIX))
      return false;

   p = path + STRLEN_CONST(SMB_PREFIX);

   while (*p && *p != '/')
      p++;
   if (*p != '/')
      return false;
   p++;

   end = p;
   while (*end && *end != '/')
      end++;

   if (end == p)
      return false;

   _len = (size_t)(end - p);
   if (_len >= len)
      return false;

   memcpy(s, p, _len);
   s[_len] = '\0';
   return true;
}

/* The share a path resolves to: a configured share covers every path, and
 * otherwise the share is the leading component of the URL itself. */
static void smb_effective_share(const char *path, char *s, size_t len)
{
   if (smb_cfg && smb_cfg->share && *smb_cfg->share)
   {
      strlcpy(s, smb_cfg->share, len);
      return;
   }

   if (!smb_url_share(path, s, len))
      s[0] = '\0';
}

/* A context is bound to one share for as long as the tree connect lives,
 * so the pool has to come down and back up for a settings change to be
 * observable.  This is the snapshot the live pool was built from. */
struct smb_conn_key
{
   unsigned timeout;
   unsigned num_contexts;
   unsigned auth_mode;
   char server_address[256];
   char share[256];
   char username[256];
   char password[256];
   char workgroup[256];
};

static struct smb_conn_key smb_active_key;

/* Open streams and directory handles carry raw context pointers, so the
 * pool may only be recycled while none of them are outstanding. */
static int smb_live_handles = 0;

static void smb_conn_key_fill(struct smb_conn_key *key, const char *share)
{
   memset(key, 0, sizeof(*key));

   if (!smb_cfg)
      return;

   if (share)
      strlcpy(key->share, share, sizeof(key->share));

   if (smb_cfg->server_address)
      strlcpy(key->server_address, smb_cfg->server_address,
            sizeof(key->server_address));
   if (smb_cfg->username)
      strlcpy(key->username, smb_cfg->username, sizeof(key->username));
   if (smb_cfg->password)
      strlcpy(key->password, smb_cfg->password, sizeof(key->password));
   if (smb_cfg->workgroup)
      strlcpy(key->workgroup, smb_cfg->workgroup, sizeof(key->workgroup));

   key->timeout      = smb_cfg->timeout;
   key->num_contexts = smb_cfg->num_contexts;
   key->auth_mode    = smb_cfg->auth_mode;
}

static struct smb2_context *get_smb_context(void)
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

/* Create and connect one context using the auth mode resolved at init */
static struct smb2_context *smb_create_context(void)
{
   const char *username = NULL;
   struct smb2_context *ctx;

   if (!*smb_active_key.server_address)
      return NULL;

   if (!(ctx = smb2_init_context()))
      return NULL;

   if (*smb_active_key.username)
   {
      username = smb_active_key.username;
      smb2_set_user(ctx, username);
   }
   if (*smb_active_key.password)
      smb2_set_password(ctx, smb_active_key.password);
   if (*smb_active_key.workgroup)
      smb2_set_domain(ctx, smb_active_key.workgroup);
   smb2_set_timeout(ctx, smb_active_key.timeout);
   smb2_set_security_mode(ctx, resolved_auth_mode);
   smb2_set_authentication(ctx, resolved_auth_mode);

   if (smb2_connect_share(ctx, smb_active_key.server_address,
            smb_active_key.share, username) < 0)
   {
      smb2_destroy_context(ctx);
      return NULL;
   }

   return ctx;
}

/* Connect the replacement before destroying the dead context so a failed
 * reconnect leaves the pool unchanged. */
static struct smb2_context *smb_heal_context(struct smb2_context *dead)
{
   int i;
   struct smb2_context *fresh;

   for (i = 0; i < max_context_configured; i++)
      if (smb_context_pool[i] == dead)
         break;
   if (i == max_context_configured)
      return NULL;

   if (!(fresh = smb_create_context()))
      return NULL;

   smb2_destroy_context(dead);
   smb_context_pool[i] = fresh;
   return fresh;
}

/* Echo failure on an established session means the transport is dead */
static struct smb2_context *smb_heal_if_dead(struct smb2_context *ctx)
{
   if (smb2_echo(ctx) == 0)
      return NULL;
   return smb_heal_context(ctx);
}

static void smb_reset(unsigned num_contexts)
{
   unsigned i;

   for (i = 0; i < num_contexts; i++)
   {
      if (smb_context_pool[i])
         smb2_destroy_context(smb_context_pool[i]);
   }

   free(smb_context_pool);
   smb_context_pool = NULL;

   smb_initialized = false;
   next_context_index = 0;
   max_context_configured = 0;
   smb_live_handles = 0;
   memset(&smb_active_key, 0, sizeof(smb_active_key));
}

bool smb_init_cfg(const struct smb_settings *new_cfg)
{
   smb_cfg = new_cfg;
   return true;
}

/* Initialize SMB context */
static bool smb_init(const char *want_share)
{
   char server[256];
   char share[256];
   char *username = NULL;
   unsigned i;
   unsigned max_smb_contexts;
   unsigned timeout;
   unsigned auth_mode;
   int error_no = 0;
   struct smb_conn_key key;

   smb_conn_key_fill(&key, want_share);

   if (smb_initialized)
   {
      if (memcmp(&key, &smb_active_key, sizeof(key)) == 0)
         return true;

      /* Outstanding handles still point into the pool; the new settings
       * are picked up once the last one is released. */
      if (smb_live_handles > 0)
         return true;

      smb_shutdown();
   }

   if (!network_init())
      return false;

   if (!smb_cfg || (!smb_cfg->server_address || !*smb_cfg->server_address))
   {
      fprintf(stderr, "smb_init - error - config not initialized\n");
      return false;
   }

   max_smb_contexts = smb_cfg->num_contexts;
   timeout          = smb_cfg->timeout;
   auth_mode        = smb_cfg->auth_mode;

   /* Always one or more */
   if (max_smb_contexts == 0)
      max_smb_contexts = RETRO_SMB2_DEFAULT_MAX_CLIENTS;

   if (timeout == 0)
      timeout = RETRO_SMB2_DEFAULT_CLIENT_TIMEOUT;

   smb_context_pool = calloc(max_smb_contexts, sizeof(struct smb2_context *));

   if (!smb_context_pool)
      return false;

   for (i = 0; i < max_smb_contexts; i++)
   {
      struct smb2_context *smb_context = smb2_init_context();

      if (!smb_context)
      {
         fprintf(stderr, "smb_init: error - no smb_context for %d\n", i);
         smb_reset(max_context_configured);
         return false;
      }

      strlcpy(server, smb_cfg->server_address, sizeof(server));

      /* Set credentials */
      if (smb_cfg->username && *smb_cfg->username)
      {
         username = (char*)smb_cfg->username;
         smb2_set_user(smb_context, username);
      }

      if (smb_cfg->password && *smb_cfg->password)
         smb2_set_password(smb_context, smb_cfg->password);

      if (smb_cfg->workgroup && *smb_cfg->workgroup)
         smb2_set_domain(smb_context, smb_cfg->workgroup);

      strlcpy(share, key.share, sizeof(share));

      /* set timeout */
      smb2_set_timeout(smb_context, timeout);

      /* SMB2_SEC_ defines missing on system headers but provided with latest libsmb2 */
      switch(auth_mode)
      {
         case RETRO_SMB2_SEC_NTLMSSP:
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            resolved_auth_mode = RETRO_SMB2_SEC_NTLMSSP;
            auth_mode = resolved_auth_mode;
            break;
         case RETRO_SMB2_SEC_KRB5:
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_KRB5);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_KRB5);
            resolved_auth_mode = RETRO_SMB2_SEC_KRB5;
            auth_mode = resolved_auth_mode;
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
                  resolved_auth_mode = RETRO_SMB2_SEC_KRB5;
                  smb_context_pool[i] = smb_context;
                  max_context_configured = i + 1;
                  continue;
               }

               /* reset to we can use it again */
               smb2_destroy_context(smb_context);
               smb_context = smb2_init_context();

               if (!smb_context)
               {
                  fprintf(stderr, "smb_init - error - no context\n");
                  smb_reset(max_context_configured);
                  return false;
               }

               /* reset credentials */
               if (smb_cfg->username && *smb_cfg->username)
               {
                  username = (char*)smb_cfg->username;
                  smb2_set_user(smb_context, username);
               }

               if (smb_cfg->password && *smb_cfg->password)
                  smb2_set_password(smb_context, smb_cfg->password);

               if (smb_cfg->workgroup && *smb_cfg->workgroup)
                  smb2_set_domain(smb_context, smb_cfg->workgroup);

               strlcpy(share, key.share, sizeof(share));

               smb2_set_timeout(smb_context, timeout);
            }

            /* if that fails, try SMB2_SEC_KRB5 in fallthrough */
            smb2_set_security_mode(smb_context, RETRO_SMB2_SEC_NTLMSSP);
            smb2_set_authentication(smb_context, RETRO_SMB2_SEC_NTLMSSP);

            resolved_auth_mode = RETRO_SMB2_SEC_NTLMSSP;
            auth_mode = resolved_auth_mode;
      }

      /* Connect to share */
      if ((error_no = smb2_connect_share(smb_context, server, share, username)) < 0)
      {
         fprintf(stderr, "smb_init: error - failed to connect - error_no: %d\n", error_no);
         smb2_destroy_context(smb_context);
         smb_reset(max_context_configured);
         return false;
      }

      smb_context_pool[i] = smb_context;
      max_context_configured = i + 1;
   }

   smb_active_key  = key;
   smb_initialized = true;

   return true;
}

/* libsmb2 exposes share enumeration through the async API only, so the reply
 * is pumped here off a context of its own.  srvsvc requires IPC$, which is a
 * different tree connect to the one the pool holds. */
struct smb_enum_state
{
   struct srvsvc_NetrShareEnum_rep *rep;
   int status;
   bool finished;
};

static void smb_share_enum_cb(struct smb2_context *ctx, int status,
      void *command_data, void *private_data)
{
   struct smb_enum_state *state = (struct smb_enum_state*)private_data;

   (void)ctx;

   state->status   = status;
   state->rep      = (struct srvsvc_NetrShareEnum_rep*)command_data;
   state->finished = true;
}

static int smb_wait_for_reply(struct smb2_context *ctx,
      const bool *finished, unsigned timeout)
{
   time_t start = time(NULL);

   while (!*finished)
   {
      struct pollfd pfd;

      memset(&pfd, 0, sizeof(pfd));
      pfd.fd     = smb2_get_fd(ctx);
      pfd.events = (short)smb2_which_events(ctx);

      if (socket_poll(&pfd, 1, 1000) < 0)
         return -1;
      if ((unsigned)(time(NULL) - start) > timeout)
         return -1;
      if (pfd.revents == 0)
         continue;
      if (smb2_service(ctx, pfd.revents) < 0)
         return -1;
   }

   return 0;
}

static void smb_free_share_list(char **shares, unsigned count)
{
   unsigned i;

   if (!shares)
      return;

   for (i = 0; i < count; i++)
      free(shares[i]);
   free(shares);
}

/* Collects the disk shares the server exports.  Hidden and administrative
 * shares are left out, as are printer, device and IPC entries. */
static bool smb_enum_shares(char ***out, unsigned *out_count)
{
   struct smb_enum_state state;
   struct smb2_context *ctx;
   struct srvsvc_SHARE_INFO_1_CONTAINER *level1;
   char **shares;
   unsigned count = 0;
   unsigned i;
   unsigned timeout;

   *out       = NULL;
   *out_count = 0;

   if (!smb_cfg || !smb_cfg->server_address || !*smb_cfg->server_address)
      return false;

   if (!network_init())
      return false;

   if (!(ctx = smb2_init_context()))
      return false;

   if (!(timeout = smb_cfg->timeout))
      timeout = RETRO_SMB2_DEFAULT_CLIENT_TIMEOUT;

   if (smb_cfg->username && *smb_cfg->username)
      smb2_set_user(ctx, smb_cfg->username);
   if (smb_cfg->password && *smb_cfg->password)
      smb2_set_password(ctx, smb_cfg->password);
   if (smb_cfg->workgroup && *smb_cfg->workgroup)
      smb2_set_domain(ctx, smb_cfg->workgroup);
   smb2_set_timeout(ctx, timeout);
   smb2_set_security_mode(ctx, resolved_auth_mode);
   smb2_set_authentication(ctx, resolved_auth_mode);

   if (smb2_connect_share(ctx, smb_cfg->server_address, "IPC$",
            smb_cfg->username) < 0)
   {
      smb2_destroy_context(ctx);
      return false;
   }

   memset(&state, 0, sizeof(state));

   if (smb2_share_enum_async(ctx, SHARE_INFO_1,
            smb_share_enum_cb, &state) != 0)
   {
      smb2_disconnect_share(ctx);
      smb2_destroy_context(ctx);
      return false;
   }

   if (smb_wait_for_reply(ctx, &state.finished, timeout) < 0
         || state.status != 0
         || !state.rep)
   {
      if (state.rep)
         smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb2_destroy_context(ctx);
      return false;
   }

#ifdef SMB2_SHARE_ENUM_UNION_NAMED_U
   level1 = &state.rep->ses.ShareInfo.u.Level1;
#else
   /* prebuilt libsmb2 (retroarch-apple-deps): anonymous union */
   level1 = &state.rep->ses.ShareInfo.Level1;
#endif

   if (     !level1->Buffer
         || !level1->Buffer->share_info_1
         || level1->EntriesRead == 0)
   {
      smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb2_destroy_context(ctx);
      return false;
   }

   if (!(shares = (char**)calloc(level1->EntriesRead, sizeof(char*))))
   {
      smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb2_destroy_context(ctx);
      return false;
   }

   for (i = 0; i < level1->EntriesRead; i++)
   {
      const struct srvsvc_SHARE_INFO_1 *info =
         &level1->Buffer->share_info_1[i];
      const char *name = info->netname.utf8;

      if (!name || !*name)
         continue;
      if ((info->type & 3) != SHARE_TYPE_DISKTREE)
         continue;
      if (info->type & (SHARE_TYPE_HIDDEN | SHARE_TYPE_TEMPORARY))
         continue;
      if (name[strlen(name) - 1] == '$')
         continue;

      {
         size_t _len = strlen(name) + 1;

         if (!(shares[count] = (char*)malloc(_len)))
            break;
         memcpy(shares[count], name, _len);
         count++;
      }
   }

   smb2_free_data(ctx, state.rep);
   smb2_disconnect_share(ctx);
   smb2_destroy_context(ctx);

   if (count == 0)
   {
      smb_free_share_list(shares, count);
      return false;
   }

   *out       = shares;
   *out_count = count;
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
void smb_shutdown(void)
{
   int i;

   if(!smb_initialized || max_context_configured == 0)
      return;

   for (i = 0; i < max_context_configured; i++)
      smb_close_context(i);

   smb_reset(max_context_configured);
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

   {
      size_t _len = 0;
      size_t sz   = sizeof(temp_path);

      /* Add base folder if specified */
      if (smb_cfg->subdir && *smb_cfg->subdir)
      {
         _len = strlcpy(temp_path, smb_cfg->subdir, sz);
         if (_len > 0 && _len < sz && temp_path[_len - 1] != '/')
         {
            temp_path[_len++] = '/';
            temp_path[_len  ] = '\0';
         }
      }

      /* Add relative path if provided */
      if (relative_path && relative_path[0])
      {
         if (relative_path[0] == '/')
            relative_path++;
         if (_len < sz)
            strlcpy(temp_path + _len, relative_path, sz - _len);
      }
   }

   strlcpy(dest, temp_path, dest_size);

   return true;
}

bool retro_vfs_file_open_smb(libretro_vfs_implementation_file *stream,
   const char *path, unsigned mode, unsigned hints)
{
   char full_path[PATH_MAX_LENGTH];
   char share[256];
   struct smb2fh *fh;
   int flags = 0;
   struct smb2_context *smb_context;

   if (!stream)
      return false;

   /* reset file handle */
   stream->smb_fh = (intptr_t)0;
   stream->smb_ctx = (intptr_t)0;

   smb_effective_share(path, share, sizeof(share));
   if (!*share)
      return false;

   if (!smb_init(share))
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

   if (!(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) &&
       (mode & RETRO_VFS_FILE_ACCESS_WRITE))
      flags |= O_CREAT | O_TRUNC;

   fh = smb2_open(smb_context, full_path, flags);
   if (!fh)
   {
      if ((smb_context = smb_heal_if_dead(smb_context)))
         fh = smb2_open(smb_context, full_path, flags);
      if (!fh)
         return false;
   }

   stream->smb_fh = (intptr_t)(uintptr_t)fh;
   stream->smb_ctx = (intptr_t)(uintptr_t)smb_context;
   stream->scheme = VFS_SCHEME_SMB; /* ensure SMB dispatch on IO calls */
   smb_live_handles++;
   return true;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   uint8_t *ptr               = (uint8_t*)s;
   uint64_t total             = 0;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!smb_initialized || !stream || !s || !stream->smb_fh)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx || !smb2_context_active(ctx))
      return -1;

   fh = (struct smb2fh *)(intptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   /* libsmb2 silently caps each smb2_read() to max_read_size / credits
    * (often 64 KiB–1 MiB).  Archive parsers (ZIP EOCD / central directory)
    * and other VFS callers compare against the exact requested length, so
    * a single short read looks like I/O failure and yields an empty
    * "Browse Archive" list.  Loop like fread() on a regular file. */
   while (total < len)
   {
      uint64_t want = len - total;
      int ret;

      if (want > (uint64_t)UINT32_MAX)
         want = UINT32_MAX;

      ret = smb2_read(ctx, fh, ptr + total, (uint32_t)want);
      if (ret < 0)
         return (total > 0) ? (int64_t)total : -1;
      if (ret == 0)
         break; /* EOF */

      total += (uint64_t)ret;
   }

   return (int64_t)total;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   const uint8_t *ptr         = (const uint8_t*)s;
   uint64_t total             = 0;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!smb_initialized || !stream || !s || !stream->smb_fh)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx || !smb2_context_active(ctx))
      return -1;

   fh = (struct smb2fh *)(intptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   /* Same max_write_size / credits cap as reads — accumulate. */
   while (total < len)
   {
      uint64_t want = len - total;
      int ret;

      if (want > (uint64_t)UINT32_MAX)
         want = UINT32_MAX;

      ret = smb2_write(ctx, fh, ptr + total, (uint32_t)want);
      if (ret < 0)
         return (total > 0) ? (int64_t)total : -1;
      if (ret == 0)
         break;

      total += (uint64_t)ret;
   }

   return (int64_t)total;
}

int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
   int64_t offset, int whence)
{
   struct smb2fh *fh;
   struct smb2_context *ctx;

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
   if (!ctx || !smb2_context_active(ctx))
      return -1;

   /* Only allow valid values */
   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
      return -1;

   if (smb2_lseek(ctx, fh, offset, whence, NULL) == -EINVAL)
      return -1;

   return 0;
}

/* return the current byte offset in an open file */
int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream)
{
   uint64_t cur = 0;
   struct smb2fh *fh;
   struct smb2_context *ctx;

   if (!smb_initialized || !stream || !stream->smb_ctx)
      return -1;

   if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx || !smb2_context_active(ctx))
      return -1;

   if (smb2_lseek(ctx, fh, 0, SEEK_CUR, &cur) == -EINVAL)
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

   if (!stream || !stream->smb_fh)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;

   /* Context healed away: leak the fh rather than use-after-free */
   if (!smb2_context_active(ctx))
   {
      stream->smb_fh = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      if (smb_live_handles > 0)
         smb_live_handles--;
      return -1;
   }

   ret = smb2_close(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh);

   stream->smb_fh = (intptr_t)-1;
   stream->smb_ctx = (intptr_t)0;
   if (smb_live_handles > 0)
      smb_live_handles--;

   return ret;
}

smb_dir_handle* retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   char share[256];
   struct smb2dir *dir;
   struct smb2_context *smb_context;
   smb_dir_handle *handle;

   (void)include_hidden;

   smb_effective_share(path, share, sizeof(share));

   /* Nothing to resolve a path against: the listing is the set of shares
    * the server exports. */
   if (!*share)
   {
      char **shares  = NULL;
      unsigned count = 0;

      if (!smb_enum_shares(&shares, &count))
         return NULL;

      if (!(handle = (smb_dir_handle*)malloc(sizeof(smb_dir_handle))))
      {
         smb_free_share_list(shares, count);
         return NULL;
      }

      handle->ctx         = NULL;
      handle->dir         = NULL;
      handle->shares      = shares;
      handle->share_count = count;
      handle->share_index = 0;
      smb_live_handles++;

      return handle;
   }

   if (!smb_init(share))
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
   {
      if ((smb_context = smb_heal_if_dead(smb_context)))
         dir = smb2_opendir(smb_context, full_path);
      if (!dir)
         return NULL;
   }

   handle = (smb_dir_handle*)malloc(sizeof(smb_dir_handle));
   if (!handle)
   {
      smb2_closedir(smb_context, dir);
      return NULL;
   }

   handle->ctx         = smb_context;
   handle->dir         = dir;
   handle->shares      = NULL;
   handle->share_count = 0;
   handle->share_index = 0;
   smb_live_handles++;

   return handle;
}

struct smbc_dirent* retro_vfs_readdir_smb(smb_dir_handle* dh)
{
   struct smb2dirent *ent;
   static struct smbc_dirent result;

   if (!dh)
      return NULL;

   if (dh->shares)
   {
      if (dh->share_index >= dh->share_count)
         return NULL;

      memset(&result, 0, sizeof(result));
      strlcpy(result.name, dh->shares[dh->share_index++],
            sizeof(result.name));
      result.type = RETRO_SMB_DIRENT_DIR;
      result.size = 0;

      return &result;
   }

   if (!smb_initialized)
      return NULL;

   if (!dh->ctx || !dh->dir || !smb2_context_active(dh->ctx))
      return NULL;

   ent = smb2_readdir(dh->ctx, dh->dir);
   if (!ent)
      return NULL;

   memset(&result, 0, sizeof(result));
   strlcpy(result.name, ent->name ? ent->name : "", sizeof(result.name));

   result.type = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY)
      ? RETRO_SMB_DIRENT_DIR
      : RETRO_SMB_DIRENT_FILE;
   result.size = ent->st.smb2_size;

   return &result;
}

int retro_vfs_closedir_smb(smb_dir_handle* dh)
{
   if (!dh)
      return -1;

   if (dh->shares)
   {
      smb_free_share_list(dh->shares, dh->share_count);
      free(dh);
      if (smb_live_handles > 0)
         smb_live_handles--;
      return 0;
   }

   if (!smb_initialized)
      return -1;

   if (!dh->ctx || !dh->dir)
      return -1;

   /* Context healed away: leak the smb2dir rather than use-after-free */
   if (!smb2_context_active(dh->ctx))
   {
      free(dh);
      if (smb_live_handles > 0)
         smb_live_handles--;
      return -1;
   }

   smb2_closedir(dh->ctx, dh->dir);
   free(dh);
   if (smb_live_handles > 0)
      smb_live_handles--;
   return 0;
}

int retro_vfs_stat_smb(const char *path, int64_t *size)
{
   char rel_path[PATH_MAX_LENGTH];
   char share[256];
   struct smb2_stat_64 st;
   struct smb2_context *smb_context;

   smb_effective_share(path, share, sizeof(share));

   /* The server root, and every share reached from it, is a directory */
   if (!*share)
   {
      if (size)
         *size = 0;
      return RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY;
   }

   if (!smb_init(share))
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
   {
      fprintf(stderr, "retro_vfs_stat_smb: no smb_context!\n");
      return 0;
   }

   if (smb2_stat(smb_context, rel_path, &st) < 0)
   {
      if (!(smb_context = smb_heal_if_dead(smb_context)))
         return 0;
      if (smb2_stat(smb_context, rel_path, &st) < 0)
         return 0;
   }

   /* smb2_size is uint64_t; *size is int64_t.  A naked cast on
    * files > INT64_MAX (8 EiB) would produce a negative value
    * that callers may interpret as a stat error.  Saturate to
    * INT64_MAX -- unreachable in practice today, but the fix is
    * cheap and keeps sign semantics sane. */
   if (size)
      *size = (st.smb2_size > (uint64_t)INT64_MAX)
         ? INT64_MAX
         : (int64_t)st.smb2_size;

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
   if (!ctx || !smb2_context_active(ctx))
      return -1;

   err = smb2_get_error(ctx);
   if (err && err[0] != '\0')
      return -1;

   return 0;
}
