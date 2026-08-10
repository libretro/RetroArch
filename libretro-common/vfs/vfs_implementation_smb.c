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
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <net/net_compat.h>
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_smb.h"

#define SMB_PREFIX "smb://"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

/* Context pool.
 *
 * libsmb2's synchronous API is not thread-safe per context: each
 * smb2_context owns a socket and a PDU queue, and two overlapping
 * synchronous calls on one context corrupt both. VFS operations can
 * arrive from any thread (task threads, menu, main loop), and open
 * files keep using their assigned context for their whole lifetime,
 * so every operation on a context must hold that context's lock.
 *
 * Streams and directory handles do not store context pointers. A
 * healed or shut-down context is destroyed, so a cached pointer
 * dangles and even a liveness probe on it reads freed memory. They
 * store a (slot, generation) handle instead: the generation bumps
 * whenever a slot's context is destroyed, and an operation whose
 * generation no longer matches fails closed under the slot lock
 * without touching freed memory. This also holds in single-threaded
 * builds, where the same stale-pointer problem existed whenever a
 * connection was healed.
 *
 * Lock order: the pool lock may be taken alone, a slot lock may be
 * taken alone, and where both are needed the pool lock is taken
 * first. The pool lock guards initialization state, the pool size and
 * the round-robin cursor; each slot lock guards that slot's context
 * pointer, its generation and all libsmb2 calls on it.
 *
 * The lock objects are created when the configuration is first
 * published via smb_init_cfg() - on the main thread during startup,
 * before any thread can reach an smb:// path (operations refuse to
 * run before a configuration is published) - and are deliberately
 * never destroyed, so no thread can ever block on a freed lock. */

#define SMB_POOL_CAP 64

/* Handle packing: bit 0 marks a valid handle, bits 1..8 carry the
 * slot, and the generation lives above that. 22 generation bits keep
 * the value positive in a 32-bit intptr_t; comparisons mask the
 * slot's generation the same way, so wrap-around stays consistent. */
#define SMB_HANDLE_GEN_BITS 22
#define SMB_HANDLE_GEN_MASK ((1u << SMB_HANDLE_GEN_BITS) - 1u)

struct smb_slot
{
   struct smb2_context *ctx;
   unsigned gen;
};

static struct smb_slot smb_pool[SMB_POOL_CAP];
/* The parameters the current pool was built with. A publication whose
 * effective parameters differ tears the pool down so the next smb://
 * access rebuilds it with the new values; an identical republication
 * leaves the pool alone. subdir is excluded: it only affects path
 * building, which reads the live configuration. */
struct smb_pool_params
{
   char server[256];
   char share[256];
   char username[256];
   char password[256];
   char workgroup[256];
   unsigned timeout;
   unsigned num_contexts;
   unsigned auth_mode;
};
static struct smb_pool_params smb_built;
static unsigned smb_pool_size            = 0;
static unsigned smb_pool_rr              = 0;
static bool smb_initialized              = false;
static const struct smb_settings *smb_cfg = NULL;
/* Auth mode that actually worked at init, reused when healing */
static int resolved_auth_mode            = RETRO_SMB2_SEC_NTLMSSP;

#ifdef HAVE_THREADS
static slock_t *smb_pool_lock            = NULL;
static slock_t *smb_slot_lock[SMB_POOL_CAP];
static unsigned smb_slot_locks_made      = 0;
#define SMB_POOL_LOCK()      slock_lock(smb_pool_lock)
#define SMB_POOL_UNLOCK()    slock_unlock(smb_pool_lock)
#define SMB_SLOT_LOCK(i)     slock_lock(smb_slot_lock[i])
#define SMB_SLOT_UNLOCK(i)   slock_unlock(smb_slot_lock[i])
#define SMB_LOCKS_READY()    (smb_pool_lock != NULL)
#else
#define SMB_POOL_LOCK()      do { } while (0)
#define SMB_POOL_UNLOCK()    do { } while (0)
#define SMB_SLOT_LOCK(i)     do { } while (0)
#define SMB_SLOT_UNLOCK(i)   do { } while (0)
#define SMB_LOCKS_READY()    (true)
#endif

static intptr_t smb_handle_pack(unsigned slot, unsigned gen)
{
   return (intptr_t)(
           (((uintptr_t)(gen & SMB_HANDLE_GEN_MASK)) << 9)
         | (((uintptr_t)(slot & 0xFFu)) << 1)
         | 1u);
}

static bool smb_handle_unpack(intptr_t handle,
      unsigned *slot, unsigned *gen)
{
   if (!(handle & 1))
      return false;
   *slot = (unsigned)(((uintptr_t)handle >> 1) & 0xFFu);
   *gen  = (unsigned)(((uintptr_t)handle >> 9) & SMB_HANDLE_GEN_MASK);
   return true;
}

/* Caller holds the slot lock. NULL when the handle is stale. */
static struct smb2_context *smb_slot_ctx(unsigned slot, unsigned gen)
{
   if (slot >= SMB_POOL_CAP)
      return NULL;
   if ((smb_pool[slot].gen & SMB_HANDLE_GEN_MASK) != gen)
      return NULL;
   return smb_pool[slot].ctx;
}

static void smb_cfg_str(char *dst, size_t sz, const char *src)
{
   if (src)
      strlcpy(dst, src, sz);
   else
      dst[0] = '\0';
}

static void smb_snapshot_params(struct smb_pool_params *p)
{
   memset(p, 0, sizeof(*p));
   if (!smb_cfg)
      return;
   smb_cfg_str(p->server,    sizeof(p->server),    smb_cfg->server_address);
   smb_cfg_str(p->share,     sizeof(p->share),     smb_cfg->share);
   smb_cfg_str(p->username,  sizeof(p->username),  smb_cfg->username);
   smb_cfg_str(p->password,  sizeof(p->password),  smb_cfg->password);
   smb_cfg_str(p->workgroup, sizeof(p->workgroup), smb_cfg->workgroup);
   p->timeout      = smb_cfg->timeout;
   p->num_contexts = smb_cfg->num_contexts;
   p->auth_mode    = smb_cfg->auth_mode;
}

static unsigned smb_effective_timeout(void)
{
   unsigned t = smb_cfg ? smb_cfg->timeout : 0;
   /* 0 is not a valid timeout */
   return (t == 0) ? RETRO_SMB2_DEFAULT_CLIENT_TIMEOUT : t;
}

/* Create, configure and connect one context with an explicit auth
 * mode. Returns NULL on any failure; never touches the pool. */
static struct smb2_context *smb_connect_new_context(int auth_mode)
{
   char server[256];
   char share[256];
   const char *username = NULL;
   struct smb2_context *ctx;

   if (!smb_cfg || !smb_cfg->server_address || !*smb_cfg->server_address)
      return NULL;

   if (!(ctx = smb2_init_context()))
      return NULL;

   strlcpy(server, smb_cfg->server_address, sizeof(server));
   if (smb_cfg->share && *smb_cfg->share)
      strlcpy(share, smb_cfg->share, sizeof(share));
   else
      share[0] = '\0';

   if (smb_cfg->username && *smb_cfg->username)
   {
      username = smb_cfg->username;
      smb2_set_user(ctx, username);
   }
   if (smb_cfg->password && *smb_cfg->password)
      smb2_set_password(ctx, smb_cfg->password);
   if (smb_cfg->workgroup && *smb_cfg->workgroup)
      smb2_set_domain(ctx, smb_cfg->workgroup);
   smb2_set_timeout(ctx, (int)smb_effective_timeout());
   /* smb2_set_security_mode() takes the SMB2_NEGOTIATE_SIGNING_*
    * flags, not the SMB2_SEC_* authentication constants. Passing the
    * auth mode here happened to request signing-required whenever
    * Kerberos was selected; signing negotiation and authentication
    * are independent knobs. */
   smb2_set_security_mode(ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
   smb2_set_authentication(ctx, auth_mode);

   if (smb2_connect_share(ctx, server, share, username) < 0)
   {
      smb2_destroy_context(ctx);
      return NULL;
   }

   return ctx;
}

/* Destroy a slot's context (if any) and bump its generation so every
 * outstanding handle on it fails closed. Caller holds the slot lock. */
static void smb_slot_destroy(unsigned slot)
{
   if (smb_pool[slot].ctx)
   {
      smb2_destroy_context(smb_pool[slot].ctx);
      smb_pool[slot].ctx = NULL;
   }
   smb_pool[slot].gen++;
}

/* Replace a slot's context with a freshly connected one, using the
 * auth mode resolved at init. Caller holds the slot lock. Returns the
 * new context, or NULL with the slot left empty. */
static struct smb2_context *smb_slot_recreate(unsigned slot)
{
   smb_slot_destroy(slot);
   smb_pool[slot].ctx = smb_connect_new_context(resolved_auth_mode);
   return smb_pool[slot].ctx;
}

/* Echo failure on an established session means the transport is
 * dead; replace it in place. Caller holds the slot lock. Returns the
 * context to retry on, or NULL when no retry is possible. */
static struct smb2_context *smb_slot_heal_if_dead(unsigned slot,
      struct smb2_context *ctx)
{
   if (smb2_echo(ctx) == 0)
      return NULL;
   return smb_slot_recreate(slot);
}

bool smb_init_cfg(const struct smb_settings *new_cfg)
{
#ifdef HAVE_THREADS
   /* Publications happen on the main thread (startup, configuration
    * loads and saves); see the pool comment. The bootstrap pool lock
    * is created only once a publication carries a configured server
    * address: SMB connections use only the configured server - never
    * the host embedded in an smb:// URL - so with no server
    * configured SMB cannot function this session and needs nothing.
    * A session with no intention of using SMB therefore allocates
    * exactly nothing here. Configuring a server in the menu takes
    * effect at the next publication, which the menu flow reaches by
    * saving the configuration (manually or via save-on-exit); the
    * per-slot locks are created later still, under the pool lock on
    * first actual smb:// access, sized to the configured pool. */
   if (     new_cfg
         && new_cfg->server_address
         && *new_cfg->server_address
         && !smb_pool_lock
         && !(smb_pool_lock = slock_new()))
      return false;
#endif
   smb_cfg = new_cfg;
   /* If a pool is live and this publication changes any parameter it
    * was built with, tear it down; the next smb:// access rebuilds it
    * lazily with the new values. Publications run on the main thread
    * and smb_shutdown() takes every lock, so in-flight operations
    * finish first and their stale generation handles fail closed. */
   if (smb_initialized)
   {
      struct smb_pool_params now;
      smb_snapshot_params(&now);
      if (memcmp(&now, &smb_built, sizeof(now)) != 0)
      {
         fprintf(stderr,
               "smb: settings changed; rebuilding the context pool "
               "on next use\n");
         smb_shutdown();
      }
   }
   return true;
}

/* Make sure slot locks 0..count-1 exist. Caller holds the pool lock,
 * which orders creation before any use: operations reach a slot lock
 * only through a handle, handles only exist after initialization, and
 * initialization runs under the pool lock. Lock objects are kept for
 * the lifetime of the process; a later re-initialization with a
 * larger pool only creates the missing ones. */
static bool smb_slot_locks_ensure(unsigned count)
{
#ifdef HAVE_THREADS
   while (smb_slot_locks_made < count)
   {
      if (!(smb_slot_lock[smb_slot_locks_made] = slock_new()))
         return false;
      smb_slot_locks_made++;
   }
#else
   (void)count;
#endif
   return true;
}

/* Initialize the SMB context pool. Serialized by the pool lock, so a
 * lazy first use from two threads initializes exactly once. */
static bool smb_init(void)
{
   unsigned i;
   unsigned want;
   bool ok = true;

   if (!SMB_LOCKS_READY())
   {
      /* A server address is visible through the aliased configuration
       * strings but no publication has carried it yet: the SMB
       * settings were changed after the last configuration load.
       * They take effect at the next publication (configuration save
       * or load). */
      if (smb_cfg && smb_cfg->server_address && *smb_cfg->server_address)
         fprintf(stderr,
               "smb_init: SMB configured after startup; save the "
               "configuration to activate it\n");
      return false;
   }

   SMB_POOL_LOCK();

   if (smb_initialized)
   {
      SMB_POOL_UNLOCK();
      return true;
   }

   if (!smb_cfg || !smb_cfg->server_address || !*smb_cfg->server_address)
   {
      fprintf(stderr, "smb_init - error - config not initialized\n");
      SMB_POOL_UNLOCK();
      return false;
   }

   if (!network_init())
   {
      SMB_POOL_UNLOCK();
      return false;
   }

   want = smb_cfg->num_contexts;
   /* this should always be a positive number 1 or more */
   if (want == 0)
      want = RETRO_SMB2_DEFAULT_MAX_CLIENTS;
   if (want > SMB_POOL_CAP)
      want = SMB_POOL_CAP;

   if (!smb_slot_locks_ensure(want))
   {
      SMB_POOL_UNLOCK();
      return false;
   }

   /* Resolve the auth mode first. An explicit configuration is taken
    * as-is; SEC_UNDEFINED probes KRB5 and falls back to NTLMSSP, the
    * same order as before. The probe connection is kept as slot 0. */
   SMB_SLOT_LOCK(0);
   smb_slot_destroy(0);
   switch (smb_cfg->auth_mode)
   {
      case RETRO_SMB2_SEC_NTLMSSP:
      case RETRO_SMB2_SEC_KRB5:
         resolved_auth_mode = (int)smb_cfg->auth_mode;
         smb_pool[0].ctx    = smb_connect_new_context(resolved_auth_mode);
         break;
      case RETRO_SMB2_SEC_UNDEFINED:
      default:
         resolved_auth_mode = RETRO_SMB2_SEC_KRB5;
         smb_pool[0].ctx    = smb_connect_new_context(resolved_auth_mode);
         if (!smb_pool[0].ctx)
         {
            resolved_auth_mode = RETRO_SMB2_SEC_NTLMSSP;
            smb_pool[0].ctx    = smb_connect_new_context(resolved_auth_mode);
         }
         break;
   }
   if (!smb_pool[0].ctx)
      ok = false;
   SMB_SLOT_UNLOCK(0);

   if (!ok)
   {
      fprintf(stderr, "smb_init: error - failed to connect\n");
      SMB_POOL_UNLOCK();
      return false;
   }

   for (i = 1; i < want; i++)
   {
      SMB_SLOT_LOCK(i);
      smb_slot_destroy(i);
      smb_pool[i].ctx = smb_connect_new_context(resolved_auth_mode);
      if (!smb_pool[i].ctx)
         ok = false;
      SMB_SLOT_UNLOCK(i);

      if (!ok)
      {
         unsigned j;
         fprintf(stderr,
               "smb_init: error - failed to connect context %u\n", i);
         for (j = 0; j < i; j++)
         {
            SMB_SLOT_LOCK(j);
            smb_slot_destroy(j);
            SMB_SLOT_UNLOCK(j);
         }
         SMB_POOL_UNLOCK();
         return false;
      }
   }

   smb_pool_size   = want;
   smb_pool_rr     = 0;
   smb_snapshot_params(&smb_built);
   smb_initialized = true;

   SMB_POOL_UNLOCK();
   return true;
}

/* Pick a slot round-robin and return it locked, with its context
 * (recreated in place if a heal left it empty). Returns false when
 * the pool is unavailable. On success the caller owns the slot lock
 * and must release it with SMB_SLOT_UNLOCK(*slot). */
static bool smb_slot_acquire(unsigned *slot, struct smb2_context **ctx)
{
   unsigned s;

   if (!smb_init())
      return false;

   SMB_POOL_LOCK();
   if (!smb_initialized || smb_pool_size == 0)
   {
      SMB_POOL_UNLOCK();
      return false;
   }
   s = smb_pool_rr;
   smb_pool_rr = (smb_pool_rr + 1) % smb_pool_size;
#ifdef HAVE_THREADS
   {
      /* Prefer an idle slot: a long-held slot (a cloud sync transfer,
       * a large read) should not stall unrelated operations that the
       * round-robin happens to land on it. Fall back to blocking on
       * the round-robin slot when everything is busy. */
      unsigned n = smb_pool_size;
      unsigned i, t;
      bool got = false;
      SMB_POOL_UNLOCK();
      for (i = 0; i < n; i++)
      {
         t = (s + i) % n;
         if (slock_try_lock(smb_slot_lock[t]))
         {
            s = t;
            got = true;
            break;
         }
      }
      if (!got)
         SMB_SLOT_LOCK(s);
   }
#else
   SMB_POOL_UNLOCK();
#endif
   if (!smb_pool[s].ctx)
      smb_slot_recreate(s);
   if (!smb_pool[s].ctx)
   {
      SMB_SLOT_UNLOCK(s);
      return false;
   }
   *slot = s;
   *ctx  = smb_pool[s].ctx;
   return true;
}

/* Public pool access for other in-process SMB consumers (the cloud
 * sync driver): acquire returns a connected context with its slot
 * lock held; every libsmb2 call on the context must happen before the
 * matching release. heal_if_dead mirrors the VFS operations' retry:
 * on a failed call, probe the transport and replace the context in
 * place (caller keeps holding the same slot). */
bool smb_pool_acquire(unsigned *slot, struct smb2_context **ctx)
{
   return smb_slot_acquire(slot, ctx);
}

void smb_pool_release(unsigned slot)
{
   if (slot < SMB_POOL_CAP)
      SMB_SLOT_UNLOCK(slot);
}

struct smb2_context *smb_pool_heal_if_dead(unsigned slot,
      struct smb2_context *ctx)
{
   if (slot >= SMB_POOL_CAP)
      return NULL;
   return smb_slot_heal_if_dead(slot, ctx);
}


/* Shutdown SMB context - called on exit */
void smb_shutdown(void)
{
   unsigned i;

   if (!SMB_LOCKS_READY())
      return;

   SMB_POOL_LOCK();
   if (!smb_initialized)
   {
      SMB_POOL_UNLOCK();
      return;
   }

   for (i = 0; i < smb_pool_size; i++)
   {
      SMB_SLOT_LOCK(i);
      if (smb_pool[i].ctx)
         smb2_disconnect_share(smb_pool[i].ctx);
      smb_slot_destroy(i);
      SMB_SLOT_UNLOCK(i);
   }

   smb_pool_size   = 0;
   smb_pool_rr     = 0;
   smb_initialized = false;
   SMB_POOL_UNLOCK();
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
   struct smb2fh *fh;
   int flags = 0;
   unsigned slot;
   struct smb2_context *smb_context;

   if (!stream)
      return false;

   /* reset file handle */
   stream->smb_fh = (intptr_t)0;
   stream->smb_ctx = (intptr_t)0;

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

   if (!smb_slot_acquire(&slot, &smb_context))
      return false;

   fh = smb2_open(smb_context, full_path, flags);
   if (!fh)
   {
      if ((smb_context = smb_slot_heal_if_dead(slot, smb_context)))
         fh = smb2_open(smb_context, full_path, flags);
      if (!fh)
      {
         SMB_SLOT_UNLOCK(slot);
         return false;
      }
   }

   stream->smb_fh  = (intptr_t)(uintptr_t)fh;
   stream->smb_ctx = smb_handle_pack(slot, smb_pool[slot].gen);
   stream->scheme  = VFS_SCHEME_SMB; /* ensure SMB dispatch on IO calls */
   SMB_SLOT_UNLOCK(slot);
   return true;
}

int64_t retro_vfs_file_read_smb(libretro_vfs_implementation_file *stream,
   void *s, uint64_t len)
{
   uint8_t *ptr               = (uint8_t*)s;
   uint64_t total             = 0;
   unsigned slot, gen;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!stream || !s || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (len == 0)
      return 0;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      /* Context healed away or shut down: the file handle died with
       * it. Fail closed without touching freed memory. */
      SMB_SLOT_UNLOCK(slot);
      stream->smb_fh  = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      return -1;
   }

   fh = (struct smb2fh *)(intptr_t)stream->smb_fh;

   /* libsmb2 silently caps each smb2_read() to max_read_size / credits
    * (often 64 KiB-1 MiB).  Archive parsers (ZIP EOCD / central directory)
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
      {
         SMB_SLOT_UNLOCK(slot);
         return (total > 0) ? (int64_t)total : -1;
      }
      if (ret == 0)
         break; /* EOF */

      total += (uint64_t)ret;
   }

   SMB_SLOT_UNLOCK(slot);
   return (int64_t)total;
}

int64_t retro_vfs_file_write_smb(libretro_vfs_implementation_file *stream,
   const void *s, uint64_t len)
{
   const uint8_t *ptr         = (const uint8_t*)s;
   uint64_t total             = 0;
   unsigned slot, gen;
   struct smb2_context *ctx;
   struct smb2fh *fh;

   if (!stream || !s || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (len == 0)
      return 0;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      SMB_SLOT_UNLOCK(slot);
      stream->smb_fh  = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      return -1;
   }

   fh = (struct smb2fh *)(intptr_t)stream->smb_fh;

   /* Same max_write_size / credits cap as reads - accumulate. */
   while (total < len)
   {
      uint64_t want = len - total;
      int ret;

      if (want > (uint64_t)UINT32_MAX)
         want = UINT32_MAX;

      ret = smb2_write(ctx, fh, ptr + total, (uint32_t)want);
      if (ret < 0)
      {
         SMB_SLOT_UNLOCK(slot);
         return (total > 0) ? (int64_t)total : -1;
      }
      if (ret == 0)
         break;

      total += (uint64_t)ret;
   }

   SMB_SLOT_UNLOCK(slot);
   return (int64_t)total;
}

int64_t retro_vfs_file_seek_smb(libretro_vfs_implementation_file *stream,
   int64_t offset, int whence)
{
   unsigned slot, gen;
   struct smb2fh *fh;
   struct smb2_context *ctx;
   int64_t ret;

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   /* Only allow valid values */
   if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
      return -1;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      SMB_SLOT_UNLOCK(slot);
      stream->smb_fh  = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      return -1;
   }

   fh  = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   ret = (smb2_lseek(ctx, fh, offset, whence, NULL) == -EINVAL) ? -1 : 0;
   SMB_SLOT_UNLOCK(slot);
   return ret;
}

/* return the current byte offset in an open file */
int64_t retro_vfs_file_tell_smb(libretro_vfs_implementation_file *stream)
{
   uint64_t cur = 0;
   unsigned slot, gen;
   struct smb2fh *fh;
   struct smb2_context *ctx;
   int64_t ret;

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      SMB_SLOT_UNLOCK(slot);
      stream->smb_fh  = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      return -1;
   }

   fh  = (struct smb2fh *)(void *)(uintptr_t)stream->smb_fh;
   ret = (smb2_lseek(ctx, fh, 0, SEEK_CUR, &cur) == -EINVAL)
         ? -1 : (int64_t)cur;
   SMB_SLOT_UNLOCK(slot);
   return ret;
}

int retro_vfs_file_close_smb(libretro_vfs_implementation_file *stream)
{
   int ret;
   unsigned slot, gen;
   struct smb2_context *ctx;

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      /* Context healed away or shut down: the file handle was freed
       * with it. Just clear the stream. */
      SMB_SLOT_UNLOCK(slot);
      stream->smb_fh  = (intptr_t)-1;
      stream->smb_ctx = (intptr_t)0;
      return -1;
   }

   ret = smb2_close(ctx, (struct smb2fh *)(intptr_t)stream->smb_fh);
   SMB_SLOT_UNLOCK(slot);

   stream->smb_fh  = (intptr_t)-1;
   stream->smb_ctx = (intptr_t)0;

   return ret;
}

smb_dir_handle* retro_vfs_opendir_smb(const char *path, bool include_hidden)
{
   char full_path[PATH_MAX_LENGTH];
   struct smb2dir *dir;
   unsigned slot;
   struct smb2_context *smb_context;
   smb_dir_handle *handle;

   (void)include_hidden;

   if (!smb_build_path(full_path, sizeof(full_path), path))
      return NULL;

   /* Root-of-share: bare "/" should become "" for libsmb2 opendir */
   if (full_path[0] == '/' && full_path[1] == '\0')
      full_path[0] = '\0';

   /* Strip leading slash ONLY for non-empty subpaths */
   if (full_path[0] == '/' && full_path[1] != '\0')
      memmove(full_path, full_path + 1, strlen(full_path));

   if (!smb_slot_acquire(&slot, &smb_context))
      return NULL;

   dir = smb2_opendir(smb_context, full_path);
   if (!dir)
   {
      if ((smb_context = smb_slot_heal_if_dead(slot, smb_context)))
         dir = smb2_opendir(smb_context, full_path);
      if (!dir)
      {
         SMB_SLOT_UNLOCK(slot);
         return NULL;
      }
   }

   if (!(handle = (smb_dir_handle*)calloc(1, sizeof(*handle))))
   {
      smb2_closedir(smb_context, dir);
      SMB_SLOT_UNLOCK(slot);
      return NULL;
   }

   handle->dir  = dir;
   handle->slot = slot;
   handle->gen  = smb_pool[slot].gen;
   SMB_SLOT_UNLOCK(slot);
   return handle;
}

struct smbc_dirent* retro_vfs_readdir_smb(smb_dir_handle* dh)
{
   struct smb2dirent *ent;
   struct smb2_context *ctx;

   if (!dh || !dh->dir)
      return NULL;

   SMB_SLOT_LOCK(dh->slot);
   if (!(ctx = smb_slot_ctx(dh->slot, dh->gen)))
   {
      /* Context healed away or shut down: the directory handle died
       * with it. Fail closed without touching freed memory. */
      SMB_SLOT_UNLOCK(dh->slot);
      dh->dir = NULL;
      return NULL;
   }

   ent = smb2_readdir(ctx, dh->dir);
   if (!ent)
   {
      SMB_SLOT_UNLOCK(dh->slot);
      return NULL;
   }

   strlcpy(dh->ent.name, ent->name, sizeof(dh->ent.name));
   dh->ent.type = (ent->st.smb2_type == SMB2_TYPE_DIRECTORY) ? 1 : 0;
   dh->ent.size = ent->st.smb2_size;
   SMB_SLOT_UNLOCK(dh->slot);

   return &dh->ent;
}

int retro_vfs_closedir_smb(smb_dir_handle* dh)
{
   struct smb2_context *ctx;

   if (!dh)
      return -1;

   if (dh->dir)
   {
      SMB_SLOT_LOCK(dh->slot);
      if ((ctx = smb_slot_ctx(dh->slot, dh->gen)))
         smb2_closedir(ctx, dh->dir);
      /* else: the directory handle was freed with its context */
      SMB_SLOT_UNLOCK(dh->slot);
   }

   free(dh);
   return 0;
}

int retro_vfs_stat_smb(const char *path, int64_t *size)
{
   char rel_path[PATH_MAX_LENGTH];
   struct smb2_stat_64 st;
   unsigned slot;
   struct smb2_context *smb_context;

   if (!smb_build_path(rel_path, sizeof(rel_path), path))
      return 0;

   /* Root-of-share: normalize "/" to "" for libsmb2 */
   if (rel_path[0] == '/' && rel_path[1] == '\0')
      rel_path[0] = '\0';

   /* Strip leading slash safely (preserve NULL terminator) */
   if (rel_path[0] == '/' && rel_path[1] != '\0')
      memmove(rel_path, rel_path + 1, strlen(rel_path));

   if (!smb_slot_acquire(&slot, &smb_context))
   {
      fprintf(stderr, "retro_vfs_stat_smb: no smb_context!\n");
      return 0;
   }

   if (smb2_stat(smb_context, rel_path, &st) < 0)
   {
      if (!(smb_context = smb_slot_heal_if_dead(slot, smb_context)))
      {
         SMB_SLOT_UNLOCK(slot);
         return 0;
      }
      if (smb2_stat(smb_context, rel_path, &st) < 0)
      {
         SMB_SLOT_UNLOCK(slot);
         return 0;
      }
   }
   SMB_SLOT_UNLOCK(slot);

   /* smb2_size is uint64_t; *size is int64_t.  A naked cast on
    * a >8 EiB size would be implementation-defined - clamp. */
   if (size)
      *size = (st.smb2_size > (uint64_t)INT64_MAX)
         ? INT64_MAX
         : (int64_t)st.smb2_size;

   return RETRO_VFS_STAT_IS_VALID |
         (st.smb2_type == SMB2_TYPE_DIRECTORY ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}

int retro_vfs_file_error_smb(libretro_vfs_implementation_file *stream)
{
   unsigned slot, gen;
   struct smb2_context *ctx;
   const char *err;
   int ret = 0;

   if (!stream || stream->smb_fh == 0 || stream->smb_fh == (intptr_t)-1)
      return -1;

   if (!smb_handle_unpack(stream->smb_ctx, &slot, &gen))
      return -1;

   SMB_SLOT_LOCK(slot);
   if (!(ctx = smb_slot_ctx(slot, gen)))
   {
      SMB_SLOT_UNLOCK(slot);
      return -1;
   }

   err = smb2_get_error(ctx);
   if (err && *err)
      ret = -1;
   SMB_SLOT_UNLOCK(slot);
   return ret;
}
