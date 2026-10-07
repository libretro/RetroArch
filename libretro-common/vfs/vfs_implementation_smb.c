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
#ifdef HAVE_RETROSMB
#include <net/net_smb2_compat.h>
#else
/* a system libsmb2 (6.0 or later: the share enumeration API of
 * libsmb2-share-enum.h) */
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-share-enum.h>
#endif
#include <net/net_socket.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <net/net_compat.h>
#include <vfs/vfs_implementation.h>
#include <vfs/vfs_prefetch.h>
#include "vfs_implementation_smb.h"


#define SMB_PREFIX "smb://"

#include <retro_atomic.h>
#if defined(HAVE_THREADS) && !defined(HAVE_RETROSMB)
#include <rthreads/rthreads.h>
#endif

/* libsmb2 keeps every context on one global list, which
 * smb2_init_context() pushes to, smb2_destroy_context() unlinks from
 * and smb2_context_active() walks, all without a lock; contexts made or
 * dropped on two threads at once can lose an entry (the connection then
 * reads as dead) or break the list. Every call that touches the list
 * goes through here, under a lock of our own for the list operation
 * alone: none of the three does network I/O. The cleanroom client has
 * no such list, and single-threaded builds none to guard. */
#if defined(HAVE_THREADS) && !defined(HAVE_RETROSMB)
static retro_atomic_ptr_t smb_list_lock_ptr;   /* slock_t * */

static slock_t *smb_list_lock(void)
{
   slock_t *l = (slock_t*)retro_atomic_load_acquire_ptr(&smb_list_lock_ptr);
   if (!l)
   {
      slock_t *fresh = slock_new();
      if (!fresh)
         return NULL;
      if (retro_atomic_cas_ptr(&smb_list_lock_ptr, NULL, fresh))
         l = fresh;
      else
      {
         slock_free(fresh);             /* another thread published first */
         l = (slock_t*)retro_atomic_load_acquire_ptr(&smb_list_lock_ptr);
      }
   }
   return l;
}

static struct smb2_context *smb_ctx_new(void)
{
   struct smb2_context *ctx;
   slock_t *l = smb_list_lock();
   if (!l)
      return NULL;
   slock_lock(l);
   ctx = smb2_init_context();
   slock_unlock(l);
   return ctx;
}

static void smb_ctx_free(struct smb2_context *ctx)
{
   slock_t *l = smb_list_lock();
   if (!l)
      return;
   slock_lock(l);
   smb2_destroy_context(ctx);
   slock_unlock(l);
}

static int smb_ctx_active(struct smb2_context *ctx)
{
   int r;
   slock_t *l = smb_list_lock();
   if (!l)
      return 0;
   slock_lock(l);
   r = smb2_context_active(ctx);
   slock_unlock(l);
   return r;
}
#else
#define smb_ctx_new()        smb2_init_context()
#define smb_ctx_free(ctx)    smb2_destroy_context(ctx)
#define smb_ctx_active(ctx)  smb2_context_active(ctx)
#endif

/* The connection pool, without a lock. A pool is built for one
 * connection key (server, share, credentials) and published with a
 * CAS; each slot is one connection and a busy flag that a caller
 * swaps from 0 to 1 to take the connection alone. An SMB file or
 * directory handle belongs to its session, so a stream keeps its slot
 * from open to close while stat and enumeration borrow one per call.
 * When every slot is busy, or the key has changed while slots are
 * still held, the caller connects privately for its own use: slower,
 * never blocked, never waiting behind someone else's stalled server.
 * Slots connect lazily, on first take. */
#define SMB_MAX_SLOTS 16

struct smb_slot
{
   struct smb2_context *ctx;
   retro_atomic_int_t   busy;
};

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
   char realm[128];
   char kdc[256];
   unsigned readahead;
};

struct smb_pool
{
   struct smb_conn_key key;
   retro_atomic_int_t  auth_mode;   /* resolved by the first connect */
   unsigned            count;
   struct smb_slot     slots[SMB_MAX_SLOTS];
};

static retro_atomic_ptr_t smb_pool_ptr;   /* struct smb_pool * */
static retro_atomic_ptr_t smb_cfg_ptr;    /* const struct smb_settings * */

static const struct smb_settings *smb_cfg_get(void)
{
   return (const struct smb_settings*)retro_atomic_load_acquire_ptr(&smb_cfg_ptr);
}

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
   const struct smb_settings *smb_cfg = smb_cfg_get();
   if (smb_cfg && smb_cfg->share && *smb_cfg->share)
   {
      strlcpy(s, smb_cfg->share, len);
      return;
   }

   if (!smb_url_share(path, s, len))
      s[0] = '\0';
}

static void smb_conn_key_fill(struct smb_conn_key *key, const char *share)
{
   const struct smb_settings *cfg = smb_cfg_get();
   memset(key, 0, sizeof(*key));
   if (!cfg)
      return;
   if (share)
      strlcpy(key->share, share, sizeof(key->share));
   if (cfg->server_address)
      strlcpy(key->server_address, cfg->server_address, sizeof(key->server_address));
   if (cfg->username)
      strlcpy(key->username, cfg->username, sizeof(key->username));
   if (cfg->password)
      strlcpy(key->password, cfg->password, sizeof(key->password));
   if (cfg->workgroup)
      strlcpy(key->workgroup, cfg->workgroup, sizeof(key->workgroup));
   if (cfg->realm)
      strlcpy(key->realm, cfg->realm, sizeof(key->realm));
   if (cfg->kdc)
      strlcpy(key->kdc, cfg->kdc, sizeof(key->kdc));
   key->timeout      = cfg->timeout ? cfg->timeout : RETRO_SMB2_DEFAULT_CLIENT_TIMEOUT;
   key->num_contexts = cfg->num_contexts ? cfg->num_contexts : RETRO_SMB2_DEFAULT_MAX_CLIENTS;
   key->auth_mode    = cfg->auth_mode;
   key->readahead    = cfg->readahead;
}

/* A context set up from @key with @auth, connected to the key's share. */
static struct smb2_context *smb_connect_with(const struct smb_conn_key *key, int auth)
{
   struct smb2_context *ctx = smb_ctx_new();
   const char *username = *key->username ? key->username : NULL;
   if (!ctx)
      return NULL;
   if (username)
      smb2_set_user(ctx, username);
   if (*key->password)
      smb2_set_password(ctx, key->password);
   if (*key->workgroup)
      smb2_set_domain(ctx, key->workgroup);
   smb2_set_timeout(ctx, key->timeout);
   smb2_set_security_mode(ctx, auth);
   smb2_set_authentication(ctx, auth);
#ifdef HAVE_RETROSMB
   smb2_set_kerberos(ctx, key->realm, key->kdc, 0);
   /* 0 is off: one request per read, as before read-ahead existed */
   smb2_set_readahead(ctx, key->readahead * 1024);
#endif
   if (smb2_connect_share(ctx, key->server_address, key->share, username) < 0)
   {
      smb_ctx_free(ctx);
      return NULL;
   }
   return ctx;
}

/* Connect for @pool: the configured auth mode, or - when undefined -
 * Kerberos first and NTLMSSP after it, the first success settling the
 * mode for the pool. */
static struct smb2_context *smb_create_context(struct smb_pool *pool)
{
   int mode = retro_atomic_load_acquire_int(&pool->auth_mode);
   struct smb2_context *ctx;
   if (mode != RETRO_SMB2_SEC_UNDEFINED)
      return smb_connect_with(&pool->key, mode);
   if ((ctx = smb_connect_with(&pool->key, RETRO_SMB2_SEC_KRB5)))
   {
      retro_atomic_store_release_int(&pool->auth_mode, RETRO_SMB2_SEC_KRB5);
      return ctx;
   }
   if ((ctx = smb_connect_with(&pool->key, RETRO_SMB2_SEC_NTLMSSP)))
      retro_atomic_store_release_int(&pool->auth_mode, RETRO_SMB2_SEC_NTLMSSP);
   return ctx;
}

static void smb_pool_free(struct smb_pool *p)
{
   unsigned i;
   if (!p)
      return;
   for (i = 0; i < p->count; i++)
      if (p->slots[i].ctx)
      {
         smb2_disconnect_share(p->slots[i].ctx);
         smb_ctx_free(p->slots[i].ctx);
      }
   free(p);
}

static int smb_pool_busy(const struct smb_pool *p)
{
   unsigned i;
   for (i = 0; p && i < p->count; i++)
      if (retro_atomic_load_acquire_int(&p->slots[i].busy))
         return 1;
   return 0;
}

bool smb_init_cfg(const struct smb_settings *new_cfg)
{
   retro_atomic_store_release_ptr(&smb_cfg_ptr, (void*)new_cfg);
   return true;
}

/* The pool for @want_share: the published one when its key matches,
 * else a new one built here and published with a CAS (a losing
 * builder drops its copy). A pool whose slots are still held by open
 * handles is left in place; callers then connect privately until the
 * next call finds it idle and replaces it. */
static struct smb_pool *smb_pool_for(const char *want_share)
{
   struct smb_conn_key key;
   struct smb_pool *cur, *fresh;

   smb_conn_key_fill(&key, want_share);
   if (!*key.server_address)
      return NULL;
   cur = (struct smb_pool*)retro_atomic_load_acquire_ptr(&smb_pool_ptr);
   if (cur && memcmp(&cur->key, &key, sizeof(key)) == 0)
      return cur;
   if (cur && smb_pool_busy(cur))
      return NULL;                       /* private connections meanwhile */
   if (!network_init())
      return NULL;
   if (!(fresh = (struct smb_pool*)calloc(1, sizeof(*fresh))))
      return NULL;
   fresh->key   = key;
   fresh->count = key.num_contexts > SMB_MAX_SLOTS ? SMB_MAX_SLOTS : key.num_contexts;
   retro_atomic_store_release_int(&fresh->auth_mode, (int)key.auth_mode);
   if (retro_atomic_cas_ptr(&smb_pool_ptr, cur, fresh))
   {
      smb_pool_free(cur);
      return fresh;
   }
   free(fresh);
   return (struct smb_pool*)retro_atomic_load_acquire_ptr(&smb_pool_ptr);
}

/* A connection to @want_share for one caller. *slot is the pool slot
 * held, or NULL for a private connection the caller owns. */
static struct smb2_context *smb_take(const char *want_share, struct smb_slot **slot)
{
   struct smb_pool *pool = smb_pool_for(want_share);
   struct smb_conn_key key;
   unsigned i;

   *slot = NULL;
   if (pool)
      for (i = 0; i < pool->count; i++)
      {
         struct smb_slot *sl = &pool->slots[i];
         if (!retro_atomic_cas_int(&sl->busy, 0, 1))
            continue;
         if (!sl->ctx || !smb_ctx_active(sl->ctx))
         {
            if (sl->ctx)
               smb_ctx_free(sl->ctx);
            sl->ctx = smb_create_context(pool);
         }
         if (sl->ctx)
         {
            *slot = sl;
            return sl->ctx;
         }
         retro_atomic_store_release_int(&sl->busy, 0);
         return NULL;                    /* the server is not answering */
      }
   /* every slot busy, or no usable pool: a private connection */
   smb_conn_key_fill(&key, want_share);
   if (!*key.server_address || !network_init())
      return NULL;
   {
      struct smb_pool tmp;
      memset(&tmp, 0, sizeof(tmp));
      tmp.key = key;
      retro_atomic_store_release_int(&tmp.auth_mode,
            pool ? retro_atomic_load_acquire_int(&pool->auth_mode) : (int)key.auth_mode);
      return smb_create_context(&tmp);
   }
}

static void smb_give(struct smb2_context *ctx, struct smb_slot *slot)
{
   if (slot)
      retro_atomic_store_release_int(&slot->busy, 0);
   else if (ctx)
   {
      smb2_disconnect_share(ctx);
      smb_ctx_free(ctx);
   }
}

/* A dead transport while a slot is held: reconnect it in place (the
 * holder is the only user) or, for a private connection, afresh. */
static struct smb2_context *smb_heal(struct smb2_context *ctx, struct smb_slot *slot,
      const char *want_share)
{
   struct smb_pool *pool = (struct smb_pool*)retro_atomic_load_acquire_ptr(&smb_pool_ptr);
   struct smb2_context *fresh;
   if (smb2_echo(ctx) == 0)
      return NULL;
   if (slot && pool)
      fresh = smb_create_context(pool);
   else
   {
      struct smb_slot *dummy;
      fresh = smb_take(want_share, &dummy);
      if (fresh && dummy)
      {
         /* took a pool slot for a private caller: keep it private */
         retro_atomic_store_release_int(&dummy->busy, 0);
         fresh = NULL;
      }
   }
   if (!fresh)
      return NULL;
   smb_ctx_free(ctx);
   if (slot)
      slot->ctx = fresh;
   return fresh;
}

/* libsmb2 exposes share enumeration through the async API only, so the reply
 * is pumped here off a context of its own.  srvsvc requires IPC$, which is a
 * different tree connect to the one the pool holds. */
#ifndef HAVE_RETROSMB
/* libsmb2 changed its share enumeration types in place (upstream
 * 48e2461, 2026-10-01): the srvsvc_* structures, SHARE_INFO_1 and
 * SRVSVC_SHARE_TYPE_* gave way to a flat smb2_share_enum_reply,
 * SMB2_SHARE_INFO_1 and SMB2_SHARE_TYPE_*. The call and the level-1
 * fields used here are the same in both, so map the names. */
#ifdef SMB2_SHARE_TYPE_DISKTREE
typedef struct smb2_share_enum_reply smb_share_rep_t;
typedef struct smb2_share_info_1     smb_share_info1_t;
#define SMB_SHARE_LEVEL_1         SMB2_SHARE_INFO_1
#define SMB_STYPE_DISKTREE        SMB2_SHARE_TYPE_DISKTREE
#define SMB_STYPE_HIDDEN          SMB2_SHARE_TYPE_HIDDEN
#define SMB_STYPE_TEMPORARY       SMB2_SHARE_TYPE_TEMPORARY
#define SMB_REP_COUNT(rep)        ((rep)->entries_read)
#define SMB_REP_INFO1(rep)        ((rep)->share_info.info_1)
#else
typedef struct srvsvc_NetrShareEnum_rep smb_share_rep_t;
typedef struct srvsvc_SHARE_INFO_1      smb_share_info1_t;
#define SMB_SHARE_LEVEL_1         SHARE_INFO_1
#define SMB_STYPE_DISKTREE        SRVSVC_SHARE_TYPE_DISKTREE
#define SMB_STYPE_HIDDEN          SRVSVC_SHARE_TYPE_HIDDEN
#define SMB_STYPE_TEMPORARY       SRVSVC_SHARE_TYPE_TEMPORARY
#define SMB_REP_COUNT(rep)        ((rep)->ses.ShareEnum.Level1.EntriesRead)
#define SMB_REP_INFO1(rep)        ((rep)->ses.ShareEnum.Level1.share_info_1)
#endif

struct smb_enum_state
{
   smb_share_rep_t *rep;
   int status;
   bool finished;
};

static void smb_share_enum_cb(struct smb2_context *ctx, int status,
      void *command_data, void *private_data)
{
   struct smb_enum_state *state = (struct smb_enum_state*)private_data;

   (void)ctx;

   state->status   = status;
   state->rep      = (smb_share_rep_t*)command_data;
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

#endif

static void smb_free_share_list(char **shares, unsigned count)
{
   unsigned i;

   if (!shares)
      return;

   for (i = 0; i < count; i++)
      free(shares[i]);
   free(shares);
}

#ifdef HAVE_RETROSMB
/* Collects the disk shares the server exports through the built-in
 * client's NetrShareEnum. Hidden and administrative shares are left
 * out, as are printer, device and IPC entries. */
static bool smb_enum_shares(char ***out, unsigned *out_count)
{
   const struct smb_settings *smb_cfg = smb_cfg_get();
   struct rsmb_ctx   *ctx;
   struct rsmb_share *list;
   char **shares;
   unsigned count = 0;
   int      n, i;

   *out       = NULL;
   *out_count = 0;

   if (!smb_cfg || !smb_cfg->server_address || !*smb_cfg->server_address)
      return false;
   if (!network_init())
      return false;
   if (!(ctx = rsmb_new()))
      return false;
   rsmb_set_credentials(ctx, smb_cfg->username, smb_cfg->password, smb_cfg->workgroup);
   rsmb_set_timeout(ctx, smb_cfg->timeout ? smb_cfg->timeout : RETRO_SMB2_DEFAULT_CLIENT_TIMEOUT);
   /* a ticket when a realm is configured and the mode allows it; the
    * client falls back to NTLMSSP on its own */
   if (smb_cfg->realm && *smb_cfg->realm && smb_cfg->auth_mode != RETRO_SMB2_SEC_NTLMSSP)
      rsmb_set_kerberos(ctx, smb_cfg->realm, smb_cfg->kdc, 0);
   if (rsmb_connect(ctx, smb_cfg->server_address, "IPC$") != 0)
   {
      rsmb_free(ctx);
      return false;
   }
   if (!(list = (struct rsmb_share*)calloc(256, sizeof(*list))))
   {
      rsmb_free(ctx);
      return false;
   }
   n = rsmb_enum_shares(ctx, list, 256);
   rsmb_free(ctx);
   if (n <= 0 || !(shares = (char**)calloc((size_t)n, sizeof(char*))))
   {
      free(list);
      return false;
   }
   if (n > 256)
      n = 256;
   for (i = 0; i < n; i++)
   {
      const char *name = list[i].name;
      size_t _len;
      if (!*name || (list[i].type & 3) != 0 || (list[i].type & 0xc0000000u)
            || name[strlen(name) - 1] == '$')
         continue;
      _len = strlen(name) + 1;
      if (!(shares[count] = (char*)malloc(_len)))
         break;
      memcpy(shares[count], name, _len);
      count++;
   }
   free(list);
   if (count == 0)
   {
      smb_free_share_list(shares, count);
      return false;
   }
   *out       = shares;
   *out_count = count;
   return true;
}
#else
/* Collects the disk shares the server exports.  Hidden and administrative
 * shares are left out, as are printer, device and IPC entries. */
static bool smb_enum_shares(char ***out, unsigned *out_count)
{
   const struct smb_settings *smb_cfg = smb_cfg_get();
   struct smb_enum_state state;
   struct smb2_context *ctx;
   const smb_share_info1_t *info1;
   uint32_t entries;
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

   if (!(ctx = smb_ctx_new()))
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
   {
      /* the mode the pool settled on, or the configured one */
      struct smb_pool *pool = (struct smb_pool*)retro_atomic_load_acquire_ptr(&smb_pool_ptr);
      int mode = pool ? retro_atomic_load_acquire_int(&pool->auth_mode) : (int)smb_cfg->auth_mode;
      if (mode == RETRO_SMB2_SEC_UNDEFINED)
         mode = RETRO_SMB2_SEC_NTLMSSP;
      smb2_set_security_mode(ctx, mode);
      smb2_set_authentication(ctx, mode);
#ifdef HAVE_RETROSMB
      smb2_set_kerberos(ctx, smb_cfg->realm, smb_cfg->kdc, 0);
#endif
   }

   if (smb2_connect_share(ctx, smb_cfg->server_address, "IPC$",
            smb_cfg->username) < 0)
   {
      smb_ctx_free(ctx);
      return false;
   }

   memset(&state, 0, sizeof(state));

   if (smb2_share_enum_async(ctx, SMB_SHARE_LEVEL_1,
            smb_share_enum_cb, &state) != 0)
   {
      smb2_disconnect_share(ctx);
      smb_ctx_free(ctx);
      return false;
   }

   if (smb_wait_for_reply(ctx, &state.finished, timeout) < 0
         || state.status != 0
         || !state.rep)
   {
      if (state.rep)
         smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb_ctx_free(ctx);
      return false;
   }

   info1   = SMB_REP_INFO1(state.rep);
   entries = SMB_REP_COUNT(state.rep);

   if (!info1 || entries == 0)
   {
      smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb_ctx_free(ctx);
      return false;
   }

   if (!(shares = (char**)calloc(entries, sizeof(char*))))
   {
      smb2_free_data(ctx, state.rep);
      smb2_disconnect_share(ctx);
      smb_ctx_free(ctx);
      return false;
   }

   for (i = 0; i < entries; i++)
   {
      const smb_share_info1_t *info = &info1[i];
      const char *name = info->netname;

      if (!name || !*name)
         continue;
      if ((info->type & 3) != SMB_STYPE_DISKTREE)
         continue;
      if (info->type & (SMB_STYPE_HIDDEN | SMB_STYPE_TEMPORARY))
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
   smb_ctx_free(ctx);

   if (count == 0)
   {
      smb_free_share_list(shares, count);
      return false;
   }

   *out       = shares;
   *out_count = count;
   return true;
}
#endif

void smb_shutdown(void)
{
   struct smb_pool *old = (struct smb_pool*)retro_atomic_load_acquire_ptr(&smb_pool_ptr);
   if (old && retro_atomic_cas_ptr(&smb_pool_ptr, old, NULL))
      smb_pool_free(old);
}

/* Build full SMB path from settings */
static bool smb_build_path(char *dest, size_t dest_size, const char *relative_path)
{
   char temp_path[PATH_MAX_LENGTH];
   const struct smb_settings *smb_cfg = smb_cfg_get();
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

/* The background prefetcher of a read-only stream: a connection and a
 * handle of its own, so its fetches never wait on the reader's. */
struct smb_prefetch
{
   struct vfs_prefetch *p;
   struct smb2_context *ctx;
   struct smb_slot     *slot;
   struct smb2fh       *fh;
   uint64_t             from_window;  /* octets served by the prefetcher */
   uint64_t             direct;       /* octets it missed, read directly */
};

/* What the prefetchers served and missed, in KiB, over the streams
 * closed since smb_take_readahead_stats() last took them. */
static retro_atomic_int_t smb_ra_window_kib;
static retro_atomic_int_t smb_ra_direct_kib;

void smb_take_readahead_stats(unsigned *window_kib, unsigned *direct_kib)
{
   *window_kib = (unsigned)retro_atomic_exchange_int(&smb_ra_window_kib, 0);
   *direct_kib = (unsigned)retro_atomic_exchange_int(&smb_ra_direct_kib, 0);
}

#ifdef HAVE_THREADS
static int64_t smb_prefetch_fetch(void *user, uint64_t off, uint8_t *buf, size_t len)
{
   struct smb_prefetch *sp = (struct smb_prefetch*)user;
   size_t done = 0;
   if (smb2_lseek(sp->ctx, sp->fh, (int64_t)off, SEEK_SET, NULL) == -EINVAL)
      return -1;
   while (done < len)
   {
      int r = smb2_read(sp->ctx, sp->fh, buf + done, (uint32_t)(len - done));
      if (r < 0)
         return done ? (int64_t)done : -1;
      if (r == 0)
         break;
      done += (size_t)r;
   }
   return (int64_t)done;
}
#endif

/* Start prefetching @path for a stream opened read-only. Nothing
 * happens without threads, without a spare connection, or with
 * read-ahead off: the stream then reads as before, holding no second
 * connection and no thread. */
static void smb_prefetch_start(libretro_vfs_implementation_file *stream,
      const char *share, const char *path)
{
#ifdef HAVE_THREADS
   const struct smb_settings *cfg = smb_cfg_get();
   struct smb_prefetch *sp;
   size_t window = cfg ? (size_t)cfg->readahead * 1024 : 0;
   if (!window)
      return;
   if (!(sp = (struct smb_prefetch*)calloc(1, sizeof(*sp))))
      return;
   if (!(sp->ctx = smb_take(share, &sp->slot)))
   {
      free(sp);
      return;
   }
#ifdef HAVE_RETROSMB
   /* windows are fetched whole and pipelined; the client's own window
    * would only copy them once more */
   smb2_set_readahead(sp->ctx, 0);
#endif
   if (!(sp->fh = smb2_open(sp->ctx, path, O_RDONLY)))
   {
      smb_give(sp->ctx, sp->slot);
      free(sp);
      return;
   }
   if (!(sp->p = vfs_prefetch_new(smb_prefetch_fetch, sp, window, 2, 0)))
   {
      smb2_close(sp->ctx, sp->fh);
      smb_give(sp->ctx, sp->slot);
      free(sp);
      return;
   }
   stream->smb_prefetch = (intptr_t)(uintptr_t)sp;
#else
   (void)stream; (void)share; (void)path;
#endif
}

static void smb_prefetch_stop(libretro_vfs_implementation_file *stream)
{
   struct smb_prefetch *sp = (struct smb_prefetch*)(void*)(uintptr_t)stream->smb_prefetch;
   if (!sp)
      return;
   vfs_prefetch_free(sp->p);              /* joins the thread first */
   retro_atomic_fetch_add_int(&smb_ra_window_kib, (int)((sp->from_window + 512) >> 10));
   retro_atomic_fetch_add_int(&smb_ra_direct_kib, (int)((sp->direct + 512) >> 10));
   if (smb_ctx_active(sp->ctx))
      smb2_close(sp->ctx, sp->fh);
#ifdef HAVE_RETROSMB
   {
      /* the pooled connection goes back with its window as configured */
      const struct smb_settings *cfg = smb_cfg_get();
      smb2_set_readahead(sp->ctx, cfg ? cfg->readahead * 1024 : 0);
   }
#endif
   smb_give(sp->ctx, sp->slot);
   free(sp);
   stream->smb_prefetch = (intptr_t)0;
}

#ifndef HAVE_RETROSMB
static void smb_close_dropped(struct smb2_context *ctx, int status,
      void *command_data, void *private_data)
{
   (void)ctx; (void)status; (void)command_data; (void)private_data;
}
#endif

/* How a stream's file is opened again after its connection went: the
 * share, the path in it, and the open flags less create and truncate,
 * so opening again never loses what was written. */
struct smb_reopen
{
   char share[256];
   char path[PATH_MAX_LENGTH];
   int  flags;
};

/* The stream's connection went - the server restarted, or dropped it
 * after an idle spell - or its handle did: the connection is mended in
 * place, the file opened again, and the position restored, so the call
 * that found it can go on. Its position is the client's own record,
 * good whatever happened to the server. */
static bool smb_stream_mend(libretro_vfs_implementation_file *stream)
{
   struct smb_reopen   *ro   = (struct smb_reopen*)(void*)(uintptr_t)stream->smb_reopen;
   struct smb2_context *ctx  = (struct smb2_context*)(void*)(uintptr_t)stream->smb_ctx;
   struct smb_slot     *slot = (struct smb_slot*)(void*)(uintptr_t)stream->smb_slot;
   struct smb2fh       *fh   = (struct smb2fh*)(void*)(uintptr_t)stream->smb_fh;
   struct smb2_context *fresh;
   bool                 prefetch = stream->smb_prefetch != 0;
   uint64_t             pos      = 0;

   if (!ro || !ctx || !fh)
      return false;
   smb2_lseek(ctx, fh, 0, SEEK_CUR, &pos);
   if (prefetch)
      smb_prefetch_stop(stream);
   if (smb_ctx_active(ctx) && smb2_echo(ctx) == 0)
      smb2_close(ctx, fh);                 /* the connection lives: only the handle went */
   else
   {
#ifndef HAVE_RETROSMB
      /* libsmb2 frees a handle only through its close: queued here (it
       * sends nothing yet), it is answered with a shutdown, and the
       * handle freed, as the dead context is let go below. The built-in
       * client frees a context's files with it. */
      smb2_close_async(ctx, fh, smb_close_dropped, NULL);
#endif
      if (!(fresh = smb_heal(ctx, slot, ro->share)))
      {
         stream->smb_fh = (intptr_t)0;
         return false;
      }
      ctx = fresh;                         /* the old one went, handles and all */
   }
   stream->smb_ctx = (intptr_t)(uintptr_t)ctx;
   if (!(fh = smb2_open(ctx, ro->path, ro->flags)))
   {
      stream->smb_fh = (intptr_t)0;
      return false;
   }
   smb2_lseek(ctx, fh, (int64_t)pos, SEEK_SET, NULL);
   stream->smb_fh = (intptr_t)(uintptr_t)fh;
   if (prefetch)
      smb_prefetch_start(stream, ro->share, ro->path);
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
   struct smb_slot *slot;
   if (!stream)
      return false;

   /* reset file handle */
   stream->smb_fh = (intptr_t)0;
   stream->smb_ctx = (intptr_t)0;
   stream->smb_prefetch = (intptr_t)0;

   smb_effective_share(path, share, sizeof(share));
   if (!*share)
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

   /* the stream holds its connection from here to close */
   if (!(smb_context = smb_take(share, &slot)))
      return false;
   fh = smb2_open(smb_context, full_path, flags);
   if (!fh)
   {
      if ((smb_context = smb_heal(smb_context, slot, share)))
         fh = smb2_open(smb_context, full_path, flags);
      if (!fh)
      {
         smb_give(smb_context, slot);
         return false;
      }
   }
   stream->smb_fh   = (intptr_t)(uintptr_t)fh;
   stream->smb_ctx  = (intptr_t)(uintptr_t)smb_context;
   stream->smb_slot = (intptr_t)(uintptr_t)slot;
   {
      struct smb_reopen *ro = (struct smb_reopen*)malloc(sizeof(*ro));
      if (ro)
      {
         strlcpy(ro->share, share, sizeof(ro->share));
         strlcpy(ro->path, full_path, sizeof(ro->path));
         ro->flags = flags & ~(O_CREAT | O_TRUNC | O_EXCL);
      }
      stream->smb_reopen = (intptr_t)(uintptr_t)ro;
   }
   stream->scheme   = VFS_SCHEME_SMB; /* ensure SMB dispatch on IO calls */
   if (flags == O_RDONLY)
      smb_prefetch_start(stream, share, full_path);
   return true;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   uint8_t *ptr               = (uint8_t*)s;
   uint64_t total             = 0;
   bool mended                = false;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!stream || !s || !stream->smb_fh)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;
   if (!smb_ctx_active(ctx))
   {
      if (!smb_stream_mend(stream))
         return -1;
      mended = true;
      ctx    = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   }

   fh = (struct smb2fh *)(intptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   /* the prefetcher's windows first; on a miss the stream reads the
    * network itself, and the thread starts over from here */
   if (stream->smb_prefetch)
   {
      struct smb_prefetch *sp = (struct smb_prefetch*)(void*)(uintptr_t)stream->smb_prefetch;
      uint64_t pos = 0;
      if (smb2_lseek(ctx, fh, 0, SEEK_CUR, &pos) != -EINVAL)
      {
         int64_t got = vfs_prefetch_read(sp->p, pos, ptr, (size_t)len);
         if (got >= 0)
         {
            smb2_lseek(ctx, fh, got, SEEK_CUR, NULL);
            sp->from_window += (uint64_t)got;
            return got;
         }
      }
   }

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
      if (ret < 0 && !mended)
      {
         /* the connection or the handle went: mend it and read on from
          * the position the stream had reached */
         mended = true;
         if (smb_stream_mend(stream))
         {
            ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
            fh  = (struct smb2fh *)(intptr_t)stream->smb_fh;
            continue;
         }
      }
      if (ret < 0)
         return (total > 0) ? (int64_t)total : -1;
      if (ret == 0)
         break; /* EOF */

      total += (uint64_t)ret;
   }

   /* a read the prefetcher had nothing ready for */
   if (stream->smb_prefetch)
      ((struct smb_prefetch*)(void*)(uintptr_t)stream->smb_prefetch)->direct += total;
   return (int64_t)total;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   const uint8_t *ptr         = (const uint8_t*)s;
   uint64_t total             = 0;
   bool mended                = false;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!stream || !s || !stream->smb_fh)
      return -1;

   if (len == 0)
      return 0;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx)
      return -1;
   if (!smb_ctx_active(ctx))
   {
      if (!smb_stream_mend(stream))
         return -1;
      mended = true;
      ctx    = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   }

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
      if (ret < 0 && !mended)
      {
         /* the connection or the handle went: mend it and write this
          * piece again at the position the stream had reached */
         mended = true;
         if (smb_stream_mend(stream))
         {
            ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
            fh  = (struct smb2fh *)(intptr_t)stream->smb_fh;
            continue;
         }
      }
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

   if (!stream || !stream->smb_ctx)
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
   if (!smb_ctx_active(ctx))
   {
      if (!smb_stream_mend(stream))
         return -1;
      ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
      fh  = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   }

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

   if (!stream || !stream->smb_ctx)
      return -1;

   if (stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   fh = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   if (!fh)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx || !smb_ctx_active(ctx))
      return -1;

   if (smb2_lseek(ctx, fh, 0, SEEK_CUR, &cur) == -EINVAL)
      return -1;

   return (int64_t)cur;
}

int retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream)
{
   int ret;
   struct smb2_context *ctx;
   struct smb_slot *slot;
   if (!stream || !stream->smb_fh)
      return -1;
   ctx  = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   slot = (struct smb_slot *)(void *)(uintptr_t)stream->smb_slot;
   if (!ctx)
      return -1;
   smb_prefetch_stop(stream);
   /* a dead transport: the handle went with the session */
   if (!smb_ctx_active(ctx))
      ret = -1;
   else
      ret = smb2_close(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh);
   stream->smb_fh   = (intptr_t)-1;
   stream->smb_ctx  = (intptr_t)0;
   stream->smb_slot = (intptr_t)0;
   free((void*)(uintptr_t)stream->smb_reopen);
   stream->smb_reopen = (intptr_t)0;
   smb_give(ctx, slot);
   return ret;
}

smb_dir_handle* retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   char share[256];
   struct smb2dir *dir;
   struct smb2_context *smb_context;
   struct smb_slot *slot;
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
      handle->slot        = NULL;
      handle->shares      = shares;
      handle->share_count = count;
      handle->share_index = 0;
      return handle;
   }

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return NULL;

   /* Root-of-share: bare "/" should become "" for libsmb2 opendir */
   if (full_path[0] == '/' && full_path[1] == '\0')
      full_path[0] = '\0';
   /* Strip leading slash for non-root subpaths */
   else if (full_path[0] == '/' && full_path[1] != '\0')
      memmove(full_path, full_path + 1, strlen(full_path));

   /* the listing holds its connection until closedir */
   if (!(smb_context = smb_take(share, &slot)))
      return NULL;
   dir = smb2_opendir(smb_context, full_path);
   if (!dir)
   {
      if ((smb_context = smb_heal(smb_context, slot, share)))
         dir = smb2_opendir(smb_context, full_path);
      if (!dir)
      {
         smb_give(smb_context, slot);
         return NULL;
      }
   }
   handle = (smb_dir_handle*)malloc(sizeof(smb_dir_handle));
   if (!handle)
   {
      smb2_closedir(smb_context, dir);
      smb_give(smb_context, slot);
      return NULL;
   }
   handle->ctx         = smb_context;
   handle->dir         = dir;
   handle->slot        = slot;
   handle->shares      = NULL;
   handle->share_count = 0;
   handle->share_index = 0;
   return handle;
}

struct smbc_dirent* retro_vfs_readdir_smb(smb_dir_handle* dh)
{
   struct smb2dirent *ent;
   struct smbc_dirent *result;

   if (!dh)
      return NULL;
   result = &dh->ent;

   if (dh->shares)
   {
      if (dh->share_index >= dh->share_count)
         return NULL;

      memset(result, 0, sizeof(*result));
      strlcpy(result->name, dh->shares[dh->share_index++],
            sizeof(result->name));
      result->type = RETRO_SMB_DIRENT_DIR;
      result->size = 0;

      return result;
   }

   if (!dh->ctx || !dh->dir || !smb_ctx_active(dh->ctx))
      return NULL;

   ent = smb2_readdir(dh->ctx, dh->dir);
   if (!ent)
      return NULL;

   memset(result, 0, sizeof(*result));
   strlcpy(result->name, ent->name ? ent->name : "", sizeof(result->name));

   result->type = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY)
      ? RETRO_SMB_DIRENT_DIR
      : RETRO_SMB_DIRENT_FILE;
   result->size = ent->st.smb2_size;

   return result;
}

int retro_vfs_closedir_smb(smb_dir_handle* dh)
{
   if (!dh)
      return -1;

   if (dh->shares)
   {
      smb_free_share_list(dh->shares, dh->share_count);
      free(dh);
      return 0;
   }
   if (!dh->ctx || !dh->dir)
      return -1;
   /* a dead transport: the directory handle went with the session */
   if (smb_ctx_active(dh->ctx))
      smb2_closedir(dh->ctx, dh->dir);
   smb_give(dh->ctx, (struct smb_slot*)dh->slot);
   free(dh);
   return 0;
}

int retro_vfs_stat_smb(const char *path, int64_t *size)
{
   char rel_path[PATH_MAX_LENGTH];
   char share[256];
   struct smb2_stat_64 st;
   struct smb2_context *smb_context;
   struct smb_slot *slot;

   smb_effective_share(path, share, sizeof(share));

   /* The server root, and every share reached from it, is a directory */
   if (!*share)
   {
      if (size)
         *size = 0;
      return RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY;
   }

   if (!smb_build_path(rel_path, sizeof(rel_path), path))
      return 0;

   /* Root-of-share: normalize "/" to "" for libsmb2 */
   if (rel_path[0] == '/' && rel_path[1] == '\0')
      rel_path[0] = '\0';

   /* Strip leading slash safely (preserve NULL terminator) */
   if (rel_path[0] == '/' && rel_path[1] != '\0')
      memmove(rel_path, rel_path + 1, strlen(rel_path));

   /* a connection for this one call */
   if (!(smb_context = smb_take(share, &slot)))
      return 0;
   if (smb2_stat(smb_context, rel_path, &st) < 0)
   {
      if (!(smb_context = smb_heal(smb_context, slot, share))
            || smb2_stat(smb_context, rel_path, &st) < 0)
      {
         smb_give(smb_context, slot);
         return 0;
      }
   }
   smb_give(smb_context, slot);

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

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (!stream->smb_ctx)
      return -1;

   ctx = (struct smb2_context *)(void *)(uintptr_t)stream->smb_ctx;
   if (!ctx || !smb_ctx_active(ctx))
      return -1;

   err = smb2_get_error(ctx);
   if (err && err[0] != '\0')
      return -1;

   return 0;
}
