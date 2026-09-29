/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_implementation_nfs.c).
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

#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_nfs3.h>
#include <net/net_compat.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_atomic.h>
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_nfs.h"

#define NFS_PREFIX "nfs://"
#define NFS_MAX_SLOTS 16

/* The connection pool, without a lock. Each slot is one connection
 * and a busy flag; a caller takes a slot by swapping the flag from 0
 * to 1, uses the connection alone, and swaps it back. An NFS file
 * handle is valid on any connection to its export, so a stream holds
 * no slot between calls - every read borrows one. When every slot is
 * busy the caller makes a private connection for that one call and
 * drops it afterwards: slower, never blocked, never waiting on a
 * stalled server through someone else's slot. The pool itself is
 * published once by nfs_init_cfg() and only replaced while nothing
 * holds a slot. */
struct nfs_slot
{
   struct rnfs_ctx   *ctx;
   retro_atomic_int_t busy;
   char               export_path[512];   /* written by the holder only */
};

struct nfs_pool
{
   struct nfs_slot slots[NFS_MAX_SLOTS];
   unsigned        count;
};

static retro_atomic_ptr_t nfs_pool_ptr;           /* struct nfs_pool * */
static retro_atomic_ptr_t nfs_cfg_ptr;            /* const struct nfs_settings * */
/* Last error for nfs_get_last_error(), used only for diagnostics off
 * the hot path. Writers may run on several pool threads at once, so
 * each publishes into its own ring slot (claimed by an atomic counter)
 * and then advances an atomic "latest" index; the reader loads that
 * index. Distinct slots per writer means no buffer is written by two
 * threads at once, and the only shared scalars are atomic. */
#define NFS_ERR_SLOTS 8
static char               nfs_err_ring[NFS_ERR_SLOTS][128];
static retro_atomic_int_t nfs_err_seq;    /* next slot to hand out */
static retro_atomic_int_t nfs_err_latest; /* slot last published, or -1 */

static const struct nfs_settings *nfs_cfg(void)
{
   return (const struct nfs_settings*)retro_atomic_load_acquire_ptr(&nfs_cfg_ptr);
}

/* Splits nfs://server/... into the server and the part after it. */
static bool nfs_url_split(const char *url, char *server, size_t server_len,
      const char **rest)
{
   const char *p, *e;
   size_t n;
   if (!url || !string_starts_with(url, NFS_PREFIX))
      return false;
   p = url + STRLEN_CONST(NFS_PREFIX);
   e = strchr(p, '/');
   n = e ? (size_t)(e - p) : strlen(p);
   if (!n || n >= server_len)
      return false;
   memcpy(server, p, n);
   server[n] = '\0';
   *rest = e ? e : p + n;
   return true;
}

/* Resolves a URL to the export to mount and the path within it. With
 * an export in the settings, the URL path is relative to it (under the
 * subdir, when set); otherwise the URL's first component is the export
 * itself, as nfs://server/export/... */
static bool nfs_resolve(const char *url, char *server, size_t server_len,
      char *export_path, size_t export_len, char *path, size_t path_len)
{
   const char *rest;
   const struct nfs_settings *cfg = nfs_cfg();
   if (!nfs_url_split(url, server, server_len, &rest))
      return false;
   if (cfg && cfg->export_path && *cfg->export_path)
   {
      strlcpy(export_path, cfg->export_path, export_len);
      path[0] = '\0';
      if (cfg->subdir && *cfg->subdir)
      {
         strlcpy(path, cfg->subdir, path_len);
         strlcat(path, "/", path_len);
      }
      strlcat(path, rest, path_len);
   }
   else
   {
      /* nfs://server/export/path: where the export ends is the
       * server's to say, so the split is found at connect time by
       * mounting prefixes; the whole remainder is handed over here. */
      export_path[0] = '\0';
      strlcpy(path, rest, path_len);
   }
   return true;
}

static void nfs_pool_free(struct nfs_pool *p)
{
   unsigned i;
   if (!p)
      return;
   for (i = 0; i < p->count; i++)
      rnfs_free(p->slots[i].ctx);
   free(p);
}

static int nfs_pool_busy(const struct nfs_pool *p)
{
   unsigned i;
   for (i = 0; p && i < p->count; i++)
      if (retro_atomic_load_acquire_int(&p->slots[i].busy))
         return 1;
   return 0;
}

/* Settings are optional: an nfs://server/export/... URL carries all a
 * connection needs, so the pool comes up with defaults when there are
 * none. A new pool replaces the old one only when nothing holds a
 * slot; otherwise the settings are published and the old pool stays
 * until the next call here. */
bool nfs_init_cfg(const struct nfs_settings *new_cfg)
{
   struct nfs_pool *old = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   struct nfs_pool *p;
   unsigned n = (new_cfg && new_cfg->num_contexts) ? new_cfg->num_contexts : RETRO_NFS_DEFAULT_NUM_CONTEXTS;

   retro_atomic_store_release_ptr(&nfs_cfg_ptr, (void*)new_cfg);
   if (n > NFS_MAX_SLOTS)
      n = NFS_MAX_SLOTS;
   if (old && nfs_pool_busy(old))
      return true;
   if (!(p = (struct nfs_pool*)calloc(1, sizeof(*p))))
      return false;
   p->count = n;
   if (retro_atomic_cas_ptr(&nfs_pool_ptr, old, p))
      nfs_pool_free(old);
   else
      free(p);          /* someone else published one meanwhile */
   return true;
}

void nfs_shutdown(void)
{
   struct nfs_pool *old = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   if (old && retro_atomic_cas_ptr(&nfs_pool_ptr, old, NULL))
      nfs_pool_free(old);
}

/* Record a diagnostic string from whichever thread hit the error. */
static void nfs_note_error(const char *msg)
{
   int slot = retro_atomic_fetch_add_int(&nfs_err_seq, 1);
   slot &= (NFS_ERR_SLOTS - 1);
   strlcpy(nfs_err_ring[slot], msg, sizeof(nfs_err_ring[slot]));
   retro_atomic_store_release_int(&nfs_err_latest, slot);
}

const char *nfs_get_last_error(void)
{
   int slot = retro_atomic_load_acquire_int(&nfs_err_latest);
   if (slot < 0)
      return "";
   return nfs_err_ring[slot & (NFS_ERR_SLOTS - 1)];
}

/* A connection to @server:@export for one caller: a free slot already
 * there, else a free slot reconnected, else a private connection.
 * *slot is NULL for a private one. */
static struct rnfs_ctx *nfs_take(const char *server, const char *export_path,
      struct nfs_slot **slot)
{
   struct nfs_pool *p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   const struct nfs_settings *cfg = nfs_cfg();
   struct rnfs_ctx *c = NULL;
   unsigned i, pass;

   *slot = NULL;
   if (!p)
   {
      nfs_init_cfg(cfg);
      p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   }
   /* pass 0: a free slot on the right export; pass 1: any free slot */
   for (pass = 0; p && pass < 2 && !c; pass++)
      for (i = 0; i < p->count; i++)
      {
         struct nfs_slot *s = &p->slots[i];
         if (!retro_atomic_cas_int(&s->busy, 0, 1))
            continue;
         if (pass == 0 && (!s->ctx || rnfs_get_fd(s->ctx) < 0
                  || strcmp(s->export_path, export_path) != 0))
         {
            retro_atomic_store_release_int(&s->busy, 0);
            continue;
         }
         c     = s->ctx;
         *slot = s;
         break;
      }
   if (!c && *slot)
   {
      /* a free slot pointing elsewhere or not yet connected */
      if (!(*slot)->ctx)
      {
         network_init();
         (*slot)->ctx = rnfs_new();
      }
      c = (*slot)->ctx;
   }
   if (!c)
   {
      /* every slot busy: a private connection for this call */
      network_init();
      c = rnfs_new();
      *slot = NULL;
   }
   if (!c)
      return NULL;
   if (rnfs_get_fd(c) < 0 || !*slot || strcmp((*slot)->export_path, export_path) != 0)
   {
      rnfs_set_timeout(c, cfg && cfg->timeout ? cfg->timeout : RETRO_NFS_DEFAULT_TIMEOUT);
      if (cfg)
      {
         rnfs_set_ports(c, (uint16_t)cfg->nfs_port, (uint16_t)cfg->mount_port);
         rnfs_set_version(c, cfg->version ? cfg->version : 3);
      }
      if (rnfs_connect(c, server, export_path) != 0)
      {
         nfs_note_error(rnfs_get_error(c));
         if (*slot)
         {
            (*slot)->export_path[0] = '\0';
            retro_atomic_store_release_int(&(*slot)->busy, 0);
         }
         else
            rnfs_free(c);
         return NULL;
      }
      if (*slot)
         strlcpy((*slot)->export_path, export_path, sizeof((*slot)->export_path));
   }
   return c;
}

static void nfs_give(struct rnfs_ctx *c, struct nfs_slot *slot)
{
   if (slot)
      retro_atomic_store_release_int(&slot->busy, 0);
   else
      rnfs_free(c);
}

/* With no export in the settings the URL's path starts with it: a
 * slot already mounted on a prefix of @path is preferred, otherwise
 * prefixes are tried shortest first until the server accepts one.
 * *rel points at the remainder of @path under the export. */
static struct rnfs_ctx *nfs_take_url(const char *server, const char *path,
      const char **rel, struct nfs_slot **slot)
{
   struct nfs_pool *p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   char export_buf[512];
   unsigned i;
   const char *q;

   for (i = 0; p && i < p->count; i++)
   {
      struct nfs_slot *s = &p->slots[i];
      size_t el;
      if (!retro_atomic_cas_int(&s->busy, 0, 1))
         continue;
      el = strlen(s->export_path);
      if (s->ctx && el && rnfs_get_fd(s->ctx) >= 0 && strncmp(path, s->export_path, el) == 0
            && (path[el] == '/' || path[el] == '\0'))
      {
         *rel  = path + el;
         *slot = s;
         return s->ctx;
      }
      retro_atomic_store_release_int(&s->busy, 0);
   }
   q = path;
   for (;;)
   {
      const char *e;
      size_t n;
      struct rnfs_ctx *c;
      while (*q == '/')
         q++;
      if (!*q)
         break;
      e = strchr(q, '/');
      n = (size_t)((e ? e : q + strlen(q)) - path);
      if (n >= sizeof(export_buf))
         break;
      memcpy(export_buf, path, n);
      export_buf[n] = '\0';
      if ((c = nfs_take(server, export_buf, slot)))
      {
         *rel = path + n;
         return c;
      }
      if (!e)
         break;
      q = e;
   }
   return NULL;
}

/* nfs_take() for a resolved URL: the configured export, or the path's. */
static struct rnfs_ctx *nfs_take_resolved(const char *server,
      const char *export_path, char *rel, struct nfs_slot **slot)
{
   if (*export_path)
      return nfs_take(server, export_path, slot);
   {
      const char *tail = NULL;
      struct rnfs_ctx *c = nfs_take_url(server, rel, &tail, slot);
      if (c)
         memmove(rel, tail, strlen(tail) + 1);
      return c;
   }
}

bool nfs_probe_connection(void)
{
   char server[256], export_path[512], path[8];
   char url[800];
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   const struct nfs_settings *cfg = nfs_cfg();
   bool ok;
   if (!cfg || !cfg->server_address || !*cfg->server_address)
      return false;
   strlcpy(url, NFS_PREFIX, sizeof(url));
   strlcat(url, cfg->server_address, sizeof(url));
   strlcat(url, "/", sizeof(url));
   if (!nfs_resolve(url, server, sizeof(server), export_path, sizeof(export_path), path, sizeof(path)))
      return false;
   if (!(c = nfs_take_resolved(server, export_path, path, &slot)))
      return false;
   ok = rnfs_ping(c) == 0;
   nfs_give(c, slot);
   return ok;
}

/* ---- files -------------------------------------------------------- */

/* A stream keeps the resolved server and export (its handle works on
 * any connection to that export) beside the rnfs_file. */
struct nfs_stream
{
   struct rnfs_file *f;
   char server[256];
   char export_path[512];
};

static struct rnfs_ctx *nfs_stream_take(const struct nfs_stream *st, struct nfs_slot **slot)
{
   return nfs_take(st->server, st->export_path, slot);
}

bool retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_file *f;
   struct rnfs_stat st;
   struct nfs_stream *ns;
   int flags = 0;
   (void)hints;

   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return false;
   switch (mode & 0xf)
   {
      case RETRO_VFS_FILE_ACCESS_READ:
         flags = RNFS_O_RDONLY;
         break;
      case RETRO_VFS_FILE_ACCESS_WRITE:
         flags = RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC;
         break;
      case RETRO_VFS_FILE_ACCESS_READ_WRITE:
         flags = RNFS_O_RDWR | RNFS_O_CREAT;
         if (!(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING))
            flags |= RNFS_O_TRUNC;
         break;
      default:
         return false;
   }
   if (!(c = nfs_take_resolved(server, export_path, rel, &slot)))
      return false;
   if (!(ns = (struct nfs_stream*)calloc(1, sizeof(*ns))))
   {
      nfs_give(c, slot);
      return false;
   }
   if (!(f = rnfs_open(c, rel, flags)))
   {
      nfs_note_error(rnfs_get_error(c));
      nfs_give(c, slot);
      free(ns);
      return false;
   }
   stream->size = rnfs_stat(c, rel, &st) == 0 ? (int64_t)st.size : 0;
   ns->f = f;
   strlcpy(ns->server, server, sizeof(ns->server));
   /* the export a URL-only mount ended up on is the slot's; a private
    * connection reports it through the context */
   strlcpy(ns->export_path, slot ? slot->export_path : export_path, sizeof(ns->export_path));
   if (!*ns->export_path)
      strlcpy(ns->export_path, export_path, sizeof(ns->export_path));
   nfs_give(c, slot);
   stream->nfs_fh  = (intptr_t)ns;
   stream->nfs_ctx = 0;
   return true;
}

int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
      void *buf, uint64_t len)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   int64_t r;
   if (!ns || !(c = nfs_stream_take(ns, &slot)))
      return -1;
   r = rnfs_read(c, ns->f, buf, (size_t)len);
   nfs_give(c, slot);
   return r;
}

int64_t retro_vfs_file_write_nfs(libretro_vfs_implementation_file *stream,
      const void *buf, uint64_t len)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   int64_t r;
   if (!ns || !(c = nfs_stream_take(ns, &slot)))
      return -1;
   r = rnfs_write(c, ns->f, buf, (size_t)len);
   nfs_give(c, slot);
   return r;
}

int64_t retro_vfs_file_seek_nfs(libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   int w;
   if (!ns)
      return -1;
   switch (whence)
   {
      case RETRO_VFS_SEEK_POSITION_START:   w = 0; break;
      case RETRO_VFS_SEEK_POSITION_CURRENT: w = 1; break;
      case RETRO_VFS_SEEK_POSITION_END:     w = 2; break;
      default:
         return -1;
   }
   /* the seek is bookkeeping on the file, no connection involved */
   return rnfs_seek(NULL, ns->f, offset, w) < 0 ? -1 : 0;
}

int64_t retro_vfs_file_tell_nfs(libretro_vfs_implementation_file *stream)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   return ns ? rnfs_tell(ns->f) : -1;
}

int retro_vfs_file_close_nfs(libretro_vfs_implementation_file *stream)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   if (ns)
   {
      rnfs_close(NULL, ns->f);
      free(ns);
   }
   stream->nfs_fh  = 0;
   stream->nfs_ctx = 0;
   return 0;
}

int retro_vfs_file_error_nfs(libretro_vfs_implementation_file *stream)
{
   (void)stream;
   return 0;
}

int retro_vfs_stat_nfs(const char *path, int64_t *size)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_stat st;
   int ret = 0;
   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return 0;
   if (!(c = nfs_take_resolved(server, export_path, rel, &slot)))
      return 0;
   if (rnfs_stat(c, rel, &st) == 0)
   {
      if (size)
         *size = (int64_t)st.size;
      ret = RETRO_VFS_STAT_IS_VALID | (st.is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
   }
   nfs_give(c, slot);
   return ret;
}

/* ---- directories -------------------------------------------------- */

/* A directory listing pages through READDIRPLUS with a cookie that
 * also works on any connection, so a handle keeps only what it needs
 * to borrow one per page. */
struct nfs_dirstate
{
   struct rnfs_dir *d;
   char server[256];
   char export_path[512];
};

nfs_dir_handle *retro_vfs_opendir_nfs(const char *path, bool include_hidden)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_dir *d;
   nfs_dir_handle *dh;
   struct nfs_dirstate *ds;
   (void)include_hidden;
   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return NULL;
   if (!(c = nfs_take_resolved(server, export_path, rel, &slot)))
      return NULL;
   if (!(d = rnfs_opendir(c, rel)))
   {
      nfs_note_error(rnfs_get_error(c));
      nfs_give(c, slot);
      return NULL;
   }
   dh = (nfs_dir_handle*)calloc(1, sizeof(*dh));
   ds = (struct nfs_dirstate*)calloc(1, sizeof(*ds));
   if (!dh || !ds)
   {
      rnfs_closedir(c, d);
      nfs_give(c, slot);
      free(dh);
      free(ds);
      return NULL;
   }
   ds->d = d;
   strlcpy(ds->server, server, sizeof(ds->server));
   strlcpy(ds->export_path, slot ? slot->export_path : export_path, sizeof(ds->export_path));
   if (!*ds->export_path)
      strlcpy(ds->export_path, export_path, sizeof(ds->export_path));
   nfs_give(c, slot);
   dh->ctx = ds;
   dh->dir = d;
   return dh;
}

struct nfs_dirent *retro_vfs_readdir_nfs(nfs_dir_handle *dh)
{
   struct nfs_dirstate *ds = (struct nfs_dirstate*)dh->ctx;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   const struct rnfs_dirent *e;
   if (!ds || !(c = nfs_take(ds->server, ds->export_path, &slot)))
      return NULL;
   e = rnfs_readdir(c, ds->d);
   nfs_give(c, slot);
   if (!e)
      return NULL;
   strlcpy(dh->ent.name, e->name, sizeof(dh->ent.name));
   dh->ent.type = e->st.is_dir ? RETRO_NFS_DIRENT_DIR : RETRO_NFS_DIRENT_FILE;
   dh->ent.size = (int64_t)e->st.size;
   return &dh->ent;
}

int retro_vfs_closedir_nfs(nfs_dir_handle *dh)
{
   struct nfs_dirstate *ds;
   if (!dh)
      return -1;
   ds = (struct nfs_dirstate*)dh->ctx;
   if (ds)
   {
      rnfs_closedir(NULL, ds->d);
      free(ds);
   }
   free(dh);
   return 0;
}
