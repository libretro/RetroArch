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
#include <string.h>

#include <net/net_nfs3.h>
#include <net/net_compat.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <retro_atomic.h>
#include <vfs/vfs_prefetch.h>
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_nfs.h"
#include "vfs_cfg_snapshot.h"

#define NFS_PREFIX "nfs://"
#define NFS_MAX_SLOTS 16

/* The connection pool, without a lock. Each slot is one connection
 * and a busy flag; a caller takes a slot by swapping the flag from 0
 * to 1, uses the connection alone, and swaps it back. An NFS file
 * handle is valid on any connection to its export, so a stream holds
 * no slot between calls - every read borrows one. When every slot is
 * busy the caller makes a private connection for that one call and
 * drops it afterwards: slower, never blocked, never waiting on a
 * stalled server through someone else's slot. The pool is published
 * once and kept until nfs_shutdown(): new settings close the idle
 * connections, and a slot serves only callers that ask for the
 * parameters it connected with. */

/* What a connection was made with. An open file or directory keeps
 * the ones it was opened with: its handle means nothing on another
 * version or port. */
struct nfs_params
{
   unsigned nfs_port;
   unsigned mount_port;
   unsigned version;
};

struct nfs_slot
{
   struct rnfs_ctx   *ctx;
   retro_atomic_int_t busy;
   /* written by the holder only */
   struct nfs_params  params;
   char               server[256];
   char               export_path[512];
};

struct nfs_pool
{
   struct nfs_slot    slots[NFS_MAX_SLOTS];
   retro_atomic_int_t count;
};

static retro_atomic_ptr_t nfs_pool_ptr;           /* struct nfs_pool * */
/* The settings as last handed over: a copy that never changes, held
 * between nfs_cfg_enter() and nfs_cfg_leave() (vfs_cfg_snapshot.h) */
struct nfs_cfg_snap
{
   vfs_cfg_hdr_t       hdr;
   struct nfs_settings s;
   char                server[256];
   char                export_path[512];
};
static vfs_cfg_slot_t nfs_cfg_slot;
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

static void nfs_note_error(const char *msg);

/* The settings (NULL: none), valid until nfs_cfg_leave(*@h). */
static const struct nfs_settings *nfs_cfg_enter(vfs_cfg_hdr_t **h)
{
   *h = vfs_cfg_enter(&nfs_cfg_slot);
   return *h ? &((struct nfs_cfg_snap*)*h)->s : NULL;
}

static void nfs_cfg_leave(vfs_cfg_hdr_t *h)
{
   vfs_cfg_leave(&nfs_cfg_slot, h);
}

/* What the client applies on every take, whatever a connection was
 * made with: nothing a handle depends on. */
struct nfs_numbers
{
   unsigned timeout;
   unsigned readahead;          /* octets; 0: one request per read */
   unsigned num_contexts;
};

static void nfs_cfg_numbers(struct nfs_numbers *n)
{
   vfs_cfg_hdr_t *h;
   const struct nfs_settings *cfg = nfs_cfg_enter(&h);
   n->timeout      = cfg && cfg->timeout ? cfg->timeout : RETRO_NFS_DEFAULT_TIMEOUT;
   n->readahead    = cfg ? cfg->readahead * 1024 : 0;
   n->num_contexts = cfg && cfg->num_contexts ? cfg->num_contexts
                                              : RETRO_NFS_DEFAULT_NUM_CONTEXTS;
   nfs_cfg_leave(h);
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

/* With a configured export, the URL path is relative to it.
 * Otherwise mount prefixes of nfs://server/export/path. *@export_path
 * points into the settings held in *@h, which the caller leaves with
 * nfs_cfg_leave() once done with it; nothing is held on failure. */
static bool nfs_resolve(const char *url, char *server, size_t server_len,
      const char **export_path, char *path, size_t path_len,
      struct nfs_params *params, vfs_cfg_hdr_t **h)
{
   const char *rest;
   const struct nfs_settings *cfg = nfs_cfg_enter(h);
   if (!nfs_url_split(url, server, server_len, &rest))
   {
      nfs_note_error("bad URL: no server in it");
      nfs_cfg_leave(*h);
      *h = NULL;
      return false;
   }
   params->nfs_port   = cfg ? cfg->nfs_port   : 0;
   params->mount_port = cfg ? cfg->mount_port : 0;
   params->version    = cfg && cfg->version == 4 ? 4 : 3;
   /* nfs://server/export/path with no export set: where the export
    * ends is the server's to say, so the split is found at connect time
    * by mounting prefixes; the whole remainder is handed over here. */
   *export_path = cfg && *cfg->export_path ? cfg->export_path : "";
   strlcpy(path, rest, path_len);
   return true;
}

static void nfs_pool_free(struct nfs_pool *p)
{
   unsigned i;
   if (!p)
      return;
   for (i = 0; i < NFS_MAX_SLOTS; i++)
      rnfs_free(p->slots[i].ctx);
   free(p);
}

static int nfs_pool_busy(const struct nfs_pool *p)
{
   unsigned i;
   for (i = 0; p && i < NFS_MAX_SLOTS; i++)
      if (retro_atomic_load_acquire_int(&p->slots[i].busy))
         return 1;
   return 0;
}

/* The pool, published with @n slots when there is none yet. */
static struct nfs_pool *nfs_pool_get(unsigned n)
{
   struct nfs_pool *p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   if (p)
      return p;
   if (!(p = (struct nfs_pool*)calloc(1, sizeof(*p))))
      return NULL;
   retro_atomic_int_init(&p->count, (int)(n > NFS_MAX_SLOTS ? NFS_MAX_SLOTS : n));
   if (!retro_atomic_cas_ptr(&nfs_pool_ptr, NULL, p))
      free(p);          /* someone else published one meanwhile */
   return (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
}

/* Settings are optional: an nfs://server/export/... URL carries all a
 * connection needs, so the pool comes up with defaults when there are
 * none. @new_cfg is copied, strings included, so the caller may change
 * or free it at once. New settings wait for no slot: idle connections
 * close here. */
bool nfs_init_cfg(const struct nfs_settings *new_cfg)
{
   struct nfs_cfg_snap *snap = NULL;
   struct nfs_pool *p;
   unsigned i;
   unsigned n = (new_cfg && new_cfg->num_contexts) ? new_cfg->num_contexts : RETRO_NFS_DEFAULT_NUM_CONTEXTS;

   if (n > NFS_MAX_SLOTS)
      n = NFS_MAX_SLOTS;
   if (new_cfg)
   {
      if (!(snap = (struct nfs_cfg_snap*)calloc(1, sizeof(*snap))))
         return false;
      snap->s = *new_cfg;
      if (new_cfg->server_address)
         strlcpy(snap->server, new_cfg->server_address, sizeof(snap->server));
      if (new_cfg->export_path)
         strlcpy(snap->export_path, new_cfg->export_path, sizeof(snap->export_path));
      snap->s.server_address = snap->server;
      snap->s.export_path    = snap->export_path;
   }
   vfs_cfg_publish(&nfs_cfg_slot, snap ? &snap->hdr : NULL);
   if ((p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr)))
   {
      retro_atomic_store_release_int(&p->count, (int)n);
      for (i = 0; i < NFS_MAX_SLOTS; i++)
      {
         struct nfs_slot *s = &p->slots[i];
         if (!retro_atomic_cas_int(&s->busy, 0, 1))
            continue;
         rnfs_free(s->ctx);
         s->ctx            = NULL;
         s->export_path[0] = '\0';
         retro_atomic_store_release_int(&s->busy, 0);
      }
      return true;
   }
   return nfs_pool_get(n) != NULL;
}

void nfs_shutdown(void)
{
   struct nfs_pool *old = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   /* A slot still held is a call still in flight: a task the exit's
    * drain left stuck on a dead server.  Unpublish the pool so nothing
    * new takes from it, but do not free it under that call - the
    * process is on its way out. */
   if (     old
         && retro_atomic_cas_ptr(&nfs_pool_ptr, old, NULL)
         && !nfs_pool_busy(old))
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

/* A connection to @server:@export made with @want for one caller: a
 * free slot already there, else a free slot reconnected, else a
 * private connection. *slot is NULL for a private one. */
static struct rnfs_ctx *nfs_take(const char *server, const char *export_path,
      const struct nfs_params *want, struct nfs_slot **slot)
{
   struct nfs_pool *p;
   struct nfs_numbers num;
   struct rnfs_ctx *c = NULL;
   unsigned i, n, pass;

   *slot = NULL;
   nfs_cfg_numbers(&num);
   p = nfs_pool_get(num.num_contexts);
   n = p ? (unsigned)retro_atomic_load_acquire_int(&p->count) : 0;
   /* pass 0: a free slot on the right server and export, connected
    * with @want; pass 1: a free slot with no connection, so an open
    * file's slot on other parameters is not taken from it at once;
    * pass 2: any free slot */
   for (pass = 0; pass < 3 && !*slot; pass++)
      for (i = 0; i < n; i++)
      {
         struct nfs_slot *s = &p->slots[i];
         if (!retro_atomic_cas_int(&s->busy, 0, 1))
            continue;
         if (     (pass == 0 && (!s->ctx || rnfs_get_fd(s->ctx) < 0
                  || memcmp(&s->params, want, sizeof(*want)) != 0
                  || strcmp(s->server, server) != 0
                  || strcmp(s->export_path, export_path) != 0))
               || (pass == 1 && s->ctx && rnfs_get_fd(s->ctx) >= 0))
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
   {
      nfs_note_error("out of memory");
      return NULL;
   }
   if (     rnfs_get_fd(c) < 0 || !*slot
         || memcmp(&(*slot)->params, want, sizeof(*want)) != 0
         || strcmp((*slot)->server, server) != 0
         || strcmp((*slot)->export_path, export_path) != 0)
   {
      rnfs_set_timeout(c, num.timeout);
      rnfs_set_ports(c, (uint16_t)want->nfs_port, (uint16_t)want->mount_port);
      rnfs_set_version(c, want->version);
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
      {
         (*slot)->params = *want;
         strlcpy((*slot)->server, server, sizeof((*slot)->server));
         strlcpy((*slot)->export_path, export_path, sizeof((*slot)->export_path));
      }
   }
   /* client side only, so they follow the settings on any connection */
   rnfs_set_timeout(c, num.timeout);
   rnfs_set_readahead(c, num.readahead);
   return c;
}

static void nfs_give(struct rnfs_ctx *c, struct nfs_slot *slot)
{
   struct nfs_pool *p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   unsigned i;
   if (!slot)
   {
      rnfs_free(c);
      return;
   }
   /* past a lowered count no take reaches the slot, and nfs_init_cfg()
    * skipped it while held: close it here */
   for (i = p ? (unsigned)retro_atomic_load_acquire_int(&p->count) : NFS_MAX_SLOTS;
         i < NFS_MAX_SLOTS; i++)
      if (slot == &p->slots[i])
      {
         rnfs_free(slot->ctx);
         slot->ctx            = NULL;
         slot->export_path[0] = '\0';
         break;
      }
   retro_atomic_store_release_int(&slot->busy, 0);
}

/* With no export in the settings the URL's path starts with it: a
 * slot already mounted on a prefix of @path is preferred, otherwise
 * prefixes are tried shortest first until the server accepts one.
 * *rel points at the remainder of @path under the export. */
static struct rnfs_ctx *nfs_take_url(const char *server, const char *path,
      const struct nfs_params *want, const char **rel, struct nfs_slot **slot)
{
   struct nfs_pool *p = (struct nfs_pool*)retro_atomic_load_acquire_ptr(&nfs_pool_ptr);
   char export_buf[512];
   unsigned i;
   unsigned count = p ? (unsigned)retro_atomic_load_acquire_int(&p->count) : 0;
   const char *q;
   bool tried = false;

   for (i = 0; i < count; i++)
   {
      struct nfs_slot *s = &p->slots[i];
      size_t el;
      if (!retro_atomic_cas_int(&s->busy, 0, 1))
         continue;
      el = strlen(s->export_path);
      if (s->ctx && el && rnfs_get_fd(s->ctx) >= 0
            && memcmp(&s->params, want, sizeof(*want)) == 0
            && strcmp(s->server, server) == 0
            && strncmp(path, s->export_path, el) == 0
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
      tried = true;
      if ((c = nfs_take(server, export_buf, want, slot)))
      {
         *rel = path + n;
         return c;
      }
      if (!e)
         break;
      q = e;
   }
   /* otherwise the last export tried has said why */
   if (!tried)
      nfs_note_error(*q ? "export path too long"
            : "no export: set NFS Export, or use nfs://server/export/path");
   return NULL;
}

/* nfs_take() for a resolved URL: the configured export, or the path's. */
static struct rnfs_ctx *nfs_take_resolved(const char *server,
      const char *export_path, char *rel, const struct nfs_params *want,
      struct nfs_slot **slot)
{
   if (*export_path)
      return nfs_take(server, export_path, want, slot);
   {
      const char *tail = NULL;
      struct rnfs_ctx *c = nfs_take_url(server, rel, want, &tail, slot);
      if (c)
         memmove(rel, tail, strlen(tail) + 1);
      return c;
   }
}

bool nfs_probe_connection(void)
{
   char server[256], path[8];
   char url[800];
   const char *export_path;
   struct nfs_params want;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   vfs_cfg_hdr_t *h;
   const struct nfs_settings *cfg = nfs_cfg_enter(&h);
   bool ok = cfg && *cfg->server_address;
   if (ok)
   {
      strlcpy(url, NFS_PREFIX, sizeof(url));
      strlcat(url, cfg->server_address, sizeof(url));
      strlcat(url, "/", sizeof(url));
   }
   nfs_cfg_leave(h);
   if (!ok || !nfs_resolve(url, server, sizeof(server), &export_path, path, sizeof(path), &want, &h))
      return false;
   if ((c = nfs_take_resolved(server, export_path, path, &want, &slot)))
   {
      ok = rnfs_ping(c) == 0;
      nfs_give(c, slot);
   }
   else
      ok = false;
   nfs_cfg_leave(h);
   return ok;
}

/* ---- files -------------------------------------------------------- */

/* A stream keeps the resolved server and export and the parameters it
 * was opened with (its handle works on any such connection to that
 * export) beside the rnfs_file. */
struct nfs_stream
{
   struct rnfs_file *f;
   struct nfs_params params;
   char server[256];
   char export_path[512];
};

static struct rnfs_ctx *nfs_stream_take(const struct nfs_stream *st, struct nfs_slot **slot)
{
   return nfs_take(st->server, st->export_path, &st->params, slot);
}

/* The background prefetcher of a read-only stream: a connection held
 * for its lifetime and a handle of its own. */
struct nfs_prefetch
{
   struct vfs_prefetch *p;
   struct rnfs_ctx     *c;
   struct nfs_slot     *slot;
   struct rnfs_file    *f;
   uint64_t             from_window;  /* octets served by the prefetcher */
   uint64_t             direct;       /* octets it missed, read directly */
};

/* What the prefetchers served and missed, in KiB, over the streams
 * closed since nfs_take_readahead_stats() last took them. */
static retro_atomic_int_t nfs_ra_window_kib;
static retro_atomic_int_t nfs_ra_direct_kib;

void nfs_take_readahead_stats(unsigned *window_kib, unsigned *direct_kib)
{
   *window_kib = (unsigned)retro_atomic_exchange_int(&nfs_ra_window_kib, 0);
   *direct_kib = (unsigned)retro_atomic_exchange_int(&nfs_ra_direct_kib, 0);
}

#ifdef HAVE_THREADS
static int64_t nfs_prefetch_fetch(void *user, uint64_t off, uint8_t *buf, size_t len)
{
   struct nfs_prefetch *np = (struct nfs_prefetch*)user;
   size_t done = 0;
   if (rnfs_seek(np->c, np->f, (int64_t)off, 0) < 0)
      return -1;
   while (done < len)
   {
      int64_t r = rnfs_read(np->c, np->f, buf + done, len - done);
      if (r < 0)
         return done ? (int64_t)done : -1;
      if (r == 0)
         break;
      done += (size_t)r;
   }
   return (int64_t)done;
}
#endif

static void nfs_prefetch_start(libretro_vfs_implementation_file *stream,
      const struct nfs_stream *ns, const char *rel)
{
#ifdef HAVE_THREADS
   struct nfs_numbers num;
   struct nfs_prefetch *np;
   size_t window;
   nfs_cfg_numbers(&num);
   window = num.readahead;
   /* read-ahead off: no second connection, no thread */
   if (!window)
      return;
   if (!(np = (struct nfs_prefetch*)calloc(1, sizeof(*np))))
      return;
   if (!(np->c = nfs_stream_take(ns, &np->slot)))
   {
      free(np);
      return;
   }
   /* windows are fetched whole and pipelined; the client's own window
    * would only copy them once more */
   rnfs_set_readahead(np->c, 0);
   if (!(np->f = rnfs_open(np->c, rel, RNFS_O_RDONLY)))
   {
      rnfs_set_readahead(np->c, (uint32_t)window);
      nfs_give(np->c, np->slot);
      free(np);
      return;
   }
   if (!(np->p = vfs_prefetch_new(nfs_prefetch_fetch, np, window, 2, 0)))
   {
      rnfs_close(np->c, np->f);
      rnfs_set_readahead(np->c, (uint32_t)window);
      nfs_give(np->c, np->slot);
      free(np);
      return;
   }
   stream->nfs_prefetch = (intptr_t)(uintptr_t)np;
#else
   (void)stream; (void)ns; (void)rel;
#endif
}

static void nfs_prefetch_stop(libretro_vfs_implementation_file *stream)
{
   struct nfs_prefetch *np = (struct nfs_prefetch*)(void*)(uintptr_t)stream->nfs_prefetch;
   struct nfs_numbers num;
   if (!np)
      return;
   vfs_prefetch_free(np->p);              /* joins the thread first */
   retro_atomic_fetch_add_int(&nfs_ra_window_kib, (int)((np->from_window + 512) >> 10));
   retro_atomic_fetch_add_int(&nfs_ra_direct_kib, (int)((np->direct + 512) >> 10));
   rnfs_close(np->c, np->f);
   nfs_cfg_numbers(&num);
   rnfs_set_readahead(np->c, num.readahead);
   nfs_give(np->c, np->slot);
   free(np);
   stream->nfs_prefetch = (intptr_t)0;
}

bool retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   char server[256], rel[PATH_MAX_LENGTH];
   const char *export_path;
   struct nfs_params want;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_file *f;
   struct rnfs_stat st;
   struct nfs_stream *ns;
   vfs_cfg_hdr_t *h;
   int flags = 0;
   (void)hints;

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
   if (!nfs_resolve(path, server, sizeof(server), &export_path, rel, sizeof(rel), &want, &h))
      return false;
   if (!(c = nfs_take_resolved(server, export_path, rel, &want, &slot)))
   {
      nfs_cfg_leave(h);
      return false;
   }
   if (!(ns = (struct nfs_stream*)calloc(1, sizeof(*ns))))
   {
      nfs_give(c, slot);
      nfs_cfg_leave(h);
      return false;
   }
   if (!(f = rnfs_open(c, rel, flags)))
   {
      nfs_note_error(rnfs_get_error(c));
      nfs_give(c, slot);
      nfs_cfg_leave(h);
      free(ns);
      return false;
   }
   stream->size = rnfs_stat(c, rel, &st) == 0 ? (int64_t)st.size : 0;
   ns->f      = f;
   ns->params = want;
   strlcpy(ns->server, server, sizeof(ns->server));
   /* with no export set, the one the URL's path was mounted on */
   strlcpy(ns->export_path, rnfs_get_export(c), sizeof(ns->export_path));
   nfs_give(c, slot);
   nfs_cfg_leave(h);
   stream->nfs_fh  = (intptr_t)ns;
   stream->nfs_ctx = 0;
   stream->nfs_prefetch = 0;
   if (flags == RNFS_O_RDONLY)
      nfs_prefetch_start(stream, ns, rel);
   return true;
}

/* Lends what the prefetcher holds at the position, without a copy;
 * -1 when it holds nothing there, and the caller reads instead. */
int64_t retro_vfs_file_view_nfs(libretro_vfs_implementation_file *stream,
      uint64_t len, const uint8_t **ptr)
{
   struct nfs_stream   *ns = (struct nfs_stream*)stream->nfs_fh;
   struct nfs_prefetch *np;
   int64_t              got;

   if (!ns || !stream->nfs_prefetch)
      return -1;
   np = (struct nfs_prefetch*)(void*)(uintptr_t)stream->nfs_prefetch;
   if ((got = vfs_prefetch_view(np->p, (uint64_t)rnfs_tell(ns->f), ptr)) <= 0)
      return got;
   if ((uint64_t)got > len)
      got = (int64_t)len;
   rnfs_seek(NULL, ns->f, got, 1);
   np->from_window += (uint64_t)got;
   return got;
}

int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
      void *buf, uint64_t len)
{
   struct nfs_stream *ns = (struct nfs_stream*)stream->nfs_fh;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   int64_t r;
   int64_t windowed = 0;
   if (!ns)
      return -1;
   /* the prefetcher's windows first; what they do not cover, or all of
    * it on a miss, the stream reads from the network itself, and the
    * thread starts over from there */
   if (stream->nfs_prefetch)
   {
      struct nfs_prefetch *np = (struct nfs_prefetch*)(void*)(uintptr_t)stream->nfs_prefetch;
      int64_t got = vfs_prefetch_read(np->p, (uint64_t)rnfs_tell(ns->f), (uint8_t*)buf, (size_t)len);
      if (got >= 0)
      {
         rnfs_seek(NULL, ns->f, got, 1);
         np->from_window += (uint64_t)got;
         /* all of it, or a window that ends the file */
         if (got == 0 || (uint64_t)got == len)
            return got;
         windowed = got;
      }
   }
   if (!(c = nfs_stream_take(ns, &slot)))
      return windowed ? windowed : -1;
   r = rnfs_read(c, ns->f, (uint8_t*)buf + windowed, (size_t)(len - (uint64_t)windowed));
   nfs_give(c, slot);
   /* what the prefetcher had nothing ready for */
   if (r > 0 && stream->nfs_prefetch)
      ((struct nfs_prefetch*)(void*)(uintptr_t)stream->nfs_prefetch)->direct += (uint64_t)r;
   if (r < 0)
      return windowed ? windowed : r;
   return windowed + r;
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
   if (rnfs_seek(NULL, ns->f, offset, w) < 0)
      return -1;
   if (stream->nfs_prefetch)
   {
      struct nfs_prefetch *np = (struct nfs_prefetch*)(void*)(uintptr_t)stream->nfs_prefetch;
      vfs_prefetch_seek(np->p, (uint64_t)rnfs_tell(ns->f));
   }
   return 0;
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
      struct nfs_slot *slot;
      struct rnfs_ctx *c;
      nfs_prefetch_stop(stream);
      /* on a connection: NFSv4 sends its CLOSE, v3 has nothing to say */
      if ((c = nfs_stream_take(ns, &slot)))
      {
         rnfs_close(c, ns->f);
         nfs_give(c, slot);
      }
      else
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
   char server[256], rel[PATH_MAX_LENGTH];
   const char *export_path;
   struct nfs_params want;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_stat st;
   vfs_cfg_hdr_t *h;
   int ret = 0;
   if (!nfs_resolve(path, server, sizeof(server), &export_path, rel, sizeof(rel), &want, &h))
      return 0;
   if ((c = nfs_take_resolved(server, export_path, rel, &want, &slot)))
   {
      if (rnfs_stat(c, rel, &st) == 0)
      {
         if (size)
            *size = (int64_t)st.size;
         ret = RETRO_VFS_STAT_IS_VALID | (st.is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
      }
      nfs_give(c, slot);
   }
   nfs_cfg_leave(h);
   return ret;
}

/* ---- directories -------------------------------------------------- */

/* A directory listing pages through READDIRPLUS with a cookie that
 * also works on any connection, so a handle keeps only what it needs
 * to borrow one per page. */
struct nfs_dirstate
{
   struct rnfs_dir *d;
   struct nfs_params params;
   char server[256];
   char export_path[512];
};

nfs_dir_handle *retro_vfs_opendir_nfs(const char *path, bool include_hidden)
{
   char server[256], rel[PATH_MAX_LENGTH];
   const char *export_path;
   struct nfs_params want;
   struct nfs_slot *slot;
   struct rnfs_ctx *c;
   struct rnfs_dir *d;
   nfs_dir_handle *dh;
   struct nfs_dirstate *ds;
   vfs_cfg_hdr_t *h;
   (void)include_hidden;
   if (!nfs_resolve(path, server, sizeof(server), &export_path, rel, sizeof(rel), &want, &h))
      return NULL;
   if (!(c = nfs_take_resolved(server, export_path, rel, &want, &slot)))
   {
      nfs_cfg_leave(h);
      return NULL;
   }
   if (!(d = rnfs_opendir(c, rel)))
   {
      nfs_note_error(rnfs_get_error(c));
      nfs_give(c, slot);
      nfs_cfg_leave(h);
      return NULL;
   }
   dh = (nfs_dir_handle*)calloc(1, sizeof(*dh));
   ds = (struct nfs_dirstate*)calloc(1, sizeof(*ds));
   if (!dh || !ds)
   {
      nfs_note_error("out of memory");
      rnfs_closedir(c, d);
      nfs_give(c, slot);
      nfs_cfg_leave(h);
      free(dh);
      free(ds);
      return NULL;
   }
   ds->d      = d;
   ds->params = want;
   strlcpy(ds->server, server, sizeof(ds->server));
   strlcpy(ds->export_path, rnfs_get_export(c), sizeof(ds->export_path));
   nfs_give(c, slot);
   nfs_cfg_leave(h);
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
   if (!ds || !(c = nfs_take(ds->server, ds->export_path, &ds->params, &slot)))
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
