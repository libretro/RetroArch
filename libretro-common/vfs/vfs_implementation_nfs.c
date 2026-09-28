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
#include <vfs/vfs_implementation.h>
#include "vfs_implementation_nfs.h"

/* One lock over the whole backend: the contexts below are not safe to
 * share between threads, and the VFS is entered from several (content
 * loading, cloud sync, thumbnails). Calls to one server serialise;
 * they are network-bound anyway. Published once with a CAS so two
 * first callers cannot each install their own. */
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
static retro_atomic_ptr_t nfs_lock_ptr;
static slock_t *nfs_lock_get(void)
{
   slock_t *l = (slock_t*)retro_atomic_load_acquire_ptr(&nfs_lock_ptr);
   if (!l)
   {
      slock_t *fresh = slock_new();
      if (retro_atomic_cas_ptr(&nfs_lock_ptr, NULL, fresh))
         l = fresh;
      else
      {
         slock_free(fresh);
         l = (slock_t*)retro_atomic_load_acquire_ptr(&nfs_lock_ptr);
      }
   }
   return l;
}
#define NFS_LOCK()   slock_lock(nfs_lock_get())
#define NFS_UNLOCK() slock_unlock(nfs_lock_get())
#else
#define NFS_LOCK()   do { } while (0)
#define NFS_UNLOCK() do { } while (0)
#endif

#define NFS_PREFIX "nfs://"

/* One connection per pool slot; a stream or directory keeps the slot
 * it opened on. Calls on one slot are serialised by the caller's own
 * use of that stream, which is what the pool is for. */
struct nfs_slot
{
   struct rnfs_ctx *ctx;
   unsigned         users;
   char             export_path[512];
};

static struct nfs_slot *nfs_pool     = NULL;
static unsigned         nfs_pool_len = 0;
static const struct nfs_settings *nfs_cfg = NULL;
static char             nfs_last_error[128];

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
   if (!nfs_url_split(url, server, server_len, &rest))
      return false;
   if (nfs_cfg && nfs_cfg->export_path && *nfs_cfg->export_path)
   {
      strlcpy(export_path, nfs_cfg->export_path, export_len);
      path[0] = '\0';
      if (nfs_cfg->subdir && *nfs_cfg->subdir)
      {
         strlcpy(path, nfs_cfg->subdir, path_len);
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

static void nfs_shutdown_impl(void);

static bool nfs_init_cfg_impl(const struct nfs_settings *new_cfg)
{
   nfs_shutdown_impl();
   nfs_cfg = new_cfg;
   nfs_pool_len = (new_cfg && new_cfg->num_contexts) ? new_cfg->num_contexts : RETRO_NFS_DEFAULT_NUM_CONTEXTS;
   if (nfs_pool_len > 16)
      nfs_pool_len = 16;
   if (!(nfs_pool = (struct nfs_slot*)calloc(nfs_pool_len, sizeof(*nfs_pool))))
   {
      nfs_pool_len = 0;
      return false;
   }
   return true;
}

static void nfs_shutdown_impl(void)
{
   unsigned i;
   for (i = 0; i < nfs_pool_len; i++)
      rnfs_free(nfs_pool[i].ctx);
   free(nfs_pool);
   nfs_pool     = NULL;
   nfs_pool_len = 0;
}

const char *nfs_get_last_error(void)
{
   return nfs_last_error;
}

static struct nfs_slot *nfs_acquire(const char *server, const char *export_path);

/* With no export in the settings the URL's path starts with it. A slot
 * already mounted on a prefix of @path is reused; otherwise prefixes
 * are tried shortest first until the server accepts one. On success
 * *rel points at the remainder of @path under the export. */
static struct nfs_slot *nfs_acquire_url(const char *server, const char *path,
      const char **rel)
{
   char     export_buf[512];
   unsigned i;
   const char *p;

   for (i = 0; i < nfs_pool_len; i++)
   {
      struct nfs_slot *s = &nfs_pool[i];
      size_t el = strlen(s->export_path);
      if (s->ctx && el && strncmp(path, s->export_path, el) == 0
            && (path[el] == '/' || path[el] == '\0'))
      {
         *rel = path + el;
         return nfs_acquire(server, s->export_path);
      }
   }
   p = path;
   for (;;)
   {
      const char *e;
      size_t n;
      struct nfs_slot *s;
      while (*p == '/')
         p++;
      if (!*p)
         break;
      e = strchr(p, '/');
      n = (size_t)((e ? e : p + strlen(p)) - path);
      if (n >= sizeof(export_buf))
         break;
      memcpy(export_buf, path, n);
      export_buf[n] = '\0';
      if ((s = nfs_acquire(server, export_buf)))
      {
         *rel = path + n;
         return s;
      }
      if (!e)
         break;
      p = e;
   }
   return NULL;
}

/* Least-used slot, connected to @server:@export (reconnecting when it
 * pointed elsewhere or dropped). NULL when nothing can be had. */
static struct nfs_slot *nfs_acquire(const char *server, const char *export_path)
{
   struct nfs_slot *best = NULL;
   unsigned i;

   if (!nfs_pool_len && !nfs_init_cfg(nfs_cfg))
   {
      strlcpy(nfs_last_error, "out of memory", sizeof(nfs_last_error));
      return NULL;
   }
   for (i = 0; i < nfs_pool_len; i++)
   {
      struct nfs_slot *s = &nfs_pool[i];
      if (s->ctx && rnfs_get_fd(s->ctx) >= 0 && strcmp(s->export_path, export_path) == 0)
      {
         if (!best || s->users < best->users)
            best = s;
      }
   }
   if (!best)
      for (i = 0; i < nfs_pool_len; i++)
         if (nfs_pool[i].users == 0)
         {
            best = &nfs_pool[i];
            break;
         }
   if (!best)
      best = &nfs_pool[0];

   if (!best->ctx)
   {
      network_init();
      if (!(best->ctx = rnfs_new()))
         return NULL;
   }
   if (rnfs_get_fd(best->ctx) < 0 || strcmp(best->export_path, export_path) != 0)
   {
      rnfs_set_timeout(best->ctx, nfs_cfg && nfs_cfg->timeout ? nfs_cfg->timeout : RETRO_NFS_DEFAULT_TIMEOUT);
      if (nfs_cfg)
         rnfs_set_ports(best->ctx, (uint16_t)nfs_cfg->nfs_port, (uint16_t)nfs_cfg->mount_port);
      if (rnfs_connect(best->ctx, server, export_path) != 0)
      {
         strlcpy(nfs_last_error, rnfs_get_error(best->ctx), sizeof(nfs_last_error));
         return NULL;
      }
      strlcpy(best->export_path, export_path, sizeof(best->export_path));
   }
   best->users++;
   return best;
}

/* nfs_acquire() for a resolved URL: cfg export, or probe the path. */
static struct nfs_slot *nfs_acquire_resolved(const char *server,
      const char *export_path, char *rel, size_t rel_len)
{
   if (*export_path)
      return nfs_acquire(server, export_path);
   {
      const char *tail = NULL;
      struct nfs_slot *s = nfs_acquire_url(server, rel, &tail);
      if (s)
         memmove(rel, tail, strlen(tail) + 1);
      (void)rel_len;
      return s;
   }
}

static void nfs_release(struct nfs_slot *s)
{
   if (s && s->users)
      s->users--;
}

static bool nfs_probe_connection_impl(void)
{
   char server[256], export_path[512], path[8];
   char url[800];
   struct nfs_slot *s;
   if (!nfs_cfg || !nfs_cfg->server_address || !*nfs_cfg->server_address)
      return false;
   strlcpy(url, NFS_PREFIX, sizeof(url));
   strlcat(url, nfs_cfg->server_address, sizeof(url));
   strlcat(url, "/", sizeof(url));
   if (!nfs_resolve(url, server, sizeof(server), export_path, sizeof(export_path), path, sizeof(path)))
      return false;
   if (!(s = nfs_acquire(server, export_path)))
      return false;
   {
      bool ok = rnfs_ping(s->ctx) == 0;
      nfs_release(s);
      return ok;
   }
}

/* ---- files -------------------------------------------------------- */

static bool retro_vfs_file_open_nfs_impl(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *s;
   struct rnfs_file *f;
   struct rnfs_stat st;
   int flags = 0;
   (void)hints;

   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return false;
   if (!(s = nfs_acquire_resolved(server, export_path, rel, sizeof(rel))))
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
         nfs_release(s);
         return false;
   }
   if (!(f = rnfs_open(s->ctx, rel, flags)))
   {
      strlcpy(nfs_last_error, rnfs_get_error(s->ctx), sizeof(nfs_last_error));
      nfs_release(s);
      return false;
   }
   stream->nfs_fh  = (intptr_t)f;
   stream->nfs_ctx = (intptr_t)s;
   stream->size    = rnfs_stat(s->ctx, rel, &st) == 0 ? (int64_t)st.size : 0;
   return true;
}

static int64_t retro_vfs_file_read_nfs_impl(libretro_vfs_implementation_file *stream,
      void *buf, uint64_t len)
{
   struct nfs_slot *s = (struct nfs_slot*)stream->nfs_ctx;
   struct rnfs_file *f = (struct rnfs_file*)stream->nfs_fh;
   if (!s || !f)
      return -1;
   return rnfs_read(s->ctx, f, buf, (size_t)len);
}

static int64_t retro_vfs_file_write_nfs_impl(libretro_vfs_implementation_file *stream,
      const void *buf, uint64_t len)
{
   struct nfs_slot *s = (struct nfs_slot*)stream->nfs_ctx;
   struct rnfs_file *f = (struct rnfs_file*)stream->nfs_fh;
   if (!s || !f)
      return -1;
   return rnfs_write(s->ctx, f, buf, (size_t)len);
}

static int64_t retro_vfs_file_seek_nfs_impl(libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   struct nfs_slot *s = (struct nfs_slot*)stream->nfs_ctx;
   struct rnfs_file *f = (struct rnfs_file*)stream->nfs_fh;
   int w;
   if (!s || !f)
      return -1;
   switch (whence)
   {
      case RETRO_VFS_SEEK_POSITION_START:   w = 0; break;
      case RETRO_VFS_SEEK_POSITION_CURRENT: w = 1; break;
      case RETRO_VFS_SEEK_POSITION_END:     w = 2; break;
      default:
         return -1;
   }
   return rnfs_seek(s->ctx, f, offset, w) < 0 ? -1 : 0;
}

int64_t retro_vfs_file_tell_nfs(libretro_vfs_implementation_file *stream)
{
   struct rnfs_file *f = (struct rnfs_file*)stream->nfs_fh;
   return f ? rnfs_tell(f) : -1;
}

static int retro_vfs_file_close_nfs_impl(libretro_vfs_implementation_file *stream)
{
   struct nfs_slot *s = (struct nfs_slot*)stream->nfs_ctx;
   struct rnfs_file *f = (struct rnfs_file*)stream->nfs_fh;
   if (s && f)
      rnfs_close(s->ctx, f);
   nfs_release(s);
   stream->nfs_fh  = 0;
   stream->nfs_ctx = 0;
   return 0;
}

int retro_vfs_file_error_nfs(libretro_vfs_implementation_file *stream)
{
   (void)stream;
   return 0;
}

static int retro_vfs_stat_nfs_impl(const char *path, int64_t *size)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *s;
   struct rnfs_stat st;
   int ret = 0;
   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return 0;
   if (!(s = nfs_acquire_resolved(server, export_path, rel, sizeof(rel))))
      return 0;
   if (rnfs_stat(s->ctx, rel, &st) == 0)
   {
      if (size)
         *size = (int64_t)st.size;
      ret = RETRO_VFS_STAT_IS_VALID | (st.is_dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
   }
   nfs_release(s);
   return ret;
}

/* ---- directories -------------------------------------------------- */

static nfs_dir_handle *retro_vfs_opendir_nfs_impl(const char *path, bool include_hidden)
{
   char server[256], export_path[512], rel[PATH_MAX_LENGTH];
   struct nfs_slot *s;
   struct rnfs_dir *d;
   nfs_dir_handle *dh;
   (void)include_hidden;
   if (!nfs_resolve(path, server, sizeof(server), export_path, sizeof(export_path), rel, sizeof(rel)))
      return NULL;
   if (!(s = nfs_acquire_resolved(server, export_path, rel, sizeof(rel))))
      return NULL;
   if (!(d = rnfs_opendir(s->ctx, rel)))
   {
      strlcpy(nfs_last_error, rnfs_get_error(s->ctx), sizeof(nfs_last_error));
      nfs_release(s);
      return NULL;
   }
   if (!(dh = (nfs_dir_handle*)calloc(1, sizeof(*dh))))
   {
      rnfs_closedir(s->ctx, d);
      nfs_release(s);
      return NULL;
   }
   dh->ctx = s;
   dh->dir = d;
   return dh;
}

static struct nfs_dirent *retro_vfs_readdir_nfs_impl(nfs_dir_handle *dh)
{
   struct nfs_slot *s = (struct nfs_slot*)dh->ctx;
   const struct rnfs_dirent *e = rnfs_readdir(s->ctx, (struct rnfs_dir*)dh->dir);
   if (!e)
      return NULL;
   strlcpy(dh->ent.name, e->name, sizeof(dh->ent.name));
   dh->ent.type = e->st.is_dir ? RETRO_NFS_DIRENT_DIR : RETRO_NFS_DIRENT_FILE;
   dh->ent.size = (int64_t)e->st.size;
   return &dh->ent;
}

static int retro_vfs_closedir_nfs_impl(nfs_dir_handle *dh)
{
   struct nfs_slot *s;
   if (!dh)
      return -1;
   s = (struct nfs_slot*)dh->ctx;
   rnfs_closedir(s->ctx, (struct rnfs_dir*)dh->dir);
   nfs_release(s);
   free(dh);
   return 0;
}

bool nfs_init_cfg(const struct nfs_settings *new_cfg)
{
   bool r;
   NFS_LOCK();
   r = nfs_init_cfg_impl(new_cfg);
   NFS_UNLOCK();
   return r;
}

bool nfs_probe_connection(void)
{
   bool r;
   NFS_LOCK();
   r = nfs_probe_connection_impl();
   NFS_UNLOCK();
   return r;
}

bool retro_vfs_file_open_nfs(libretro_vfs_implementation_file *stream,
      const char *path, unsigned mode, unsigned hints)
{
   bool r;
   NFS_LOCK();
   r = retro_vfs_file_open_nfs_impl(stream, path, mode, hints);
   NFS_UNLOCK();
   return r;
}

int64_t retro_vfs_file_read_nfs(libretro_vfs_implementation_file *stream,
      void *buf, uint64_t len)
{
   int64_t r;
   NFS_LOCK();
   r = retro_vfs_file_read_nfs_impl(stream, buf, len);
   NFS_UNLOCK();
   return r;
}

int64_t retro_vfs_file_write_nfs(libretro_vfs_implementation_file *stream,
      const void *buf, uint64_t len)
{
   int64_t r;
   NFS_LOCK();
   r = retro_vfs_file_write_nfs_impl(stream, buf, len);
   NFS_UNLOCK();
   return r;
}

int64_t retro_vfs_file_seek_nfs(libretro_vfs_implementation_file *stream,
      int64_t offset, int whence)
{
   int64_t r;
   NFS_LOCK();
   r = retro_vfs_file_seek_nfs_impl(stream, offset, whence);
   NFS_UNLOCK();
   return r;
}

int retro_vfs_file_close_nfs(libretro_vfs_implementation_file *stream)
{
   int r;
   NFS_LOCK();
   r = retro_vfs_file_close_nfs_impl(stream);
   NFS_UNLOCK();
   return r;
}

int retro_vfs_stat_nfs(const char *path, int64_t *size)
{
   int r;
   NFS_LOCK();
   r = retro_vfs_stat_nfs_impl(path, size);
   NFS_UNLOCK();
   return r;
}

nfs_dir_handle *retro_vfs_opendir_nfs(const char *path, bool include_hidden)
{
   nfs_dir_handle *r;
   NFS_LOCK();
   r = retro_vfs_opendir_nfs_impl(path, include_hidden);
   NFS_UNLOCK();
   return r;
}

struct nfs_dirent *retro_vfs_readdir_nfs(nfs_dir_handle *dh)
{
   struct nfs_dirent *r;
   NFS_LOCK();
   r = retro_vfs_readdir_nfs_impl(dh);
   NFS_UNLOCK();
   return r;
}

int retro_vfs_closedir_nfs(nfs_dir_handle *dh)
{
   int r;
   NFS_LOCK();
   r = retro_vfs_closedir_nfs_impl(dh);
   NFS_UNLOCK();
   return r;
}

void nfs_shutdown(void)
{
   NFS_LOCK();
   nfs_shutdown_impl();
   NFS_UNLOCK();
}
