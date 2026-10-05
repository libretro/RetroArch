/* In-memory stand-in for net_nfs3.c; see fake_nfs.h. */
#include <stdlib.h>
#include <string.h>

#include <net/net_nfs3.h>

#include "fake_nfs.h"

#define FAKE_NFS3ERR_IO    5
#define FAKE_NFS3ERR_EXIST 17
#define FAKE_NFS3ERR_ISDIR 21

fake_nfs_t fake_nfs;

struct rnfs_ctx
{
   uint32_t status;
   char     error[64];
};

struct rnfs_file
{
   fake_node_t *node;
   uint64_t     pos;
};

void fake_nfs_reset(void)
{
   int i;
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
      free(fake_nfs.nodes[i].data);
   memset(&fake_nfs, 0, sizeof(fake_nfs));
   fake_nfs.fail_op = -1;
}

void fake_nfs_fail(int op, unsigned nth, uint32_t status)
{
   fake_nfs.fail_path[0] = '\0';
   fake_nfs.fail_op     = op;
   fake_nfs.fail_nth    = nth;
   fake_nfs.fail_status = status;
}

void fake_nfs_reset_fail(void)
{
   fake_nfs.fail_op      = -1;
   fake_nfs.fail_path[0] = '\0';
}

void fake_nfs_fail_path(int op, const char *path, uint32_t status)
{
   fake_nfs.fail_op     = op;
   fake_nfs.fail_nth    = 0;
   fake_nfs.fail_status = status;
   strcpy(fake_nfs.fail_path, path);
}

static const char *fake_norm(const char *path)
{
   while (*path == '/')
      path++;
   return path;
}

fake_node_t *fake_nfs_find(const char *path)
{
   int i;
   path = fake_norm(path);
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
      if (fake_nfs.nodes[i].used && !strcmp(fake_nfs.nodes[i].path, path))
         return &fake_nfs.nodes[i];
   return NULL;
}

unsigned fake_nfs_count(void)
{
   unsigned n = 0;
   int      i;
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
      if (fake_nfs.nodes[i].used)
         n++;
   return n;
}

static fake_node_t *fake_new(const char *path, int dir)
{
   int i;
   path = fake_norm(path);
   if (strlen(path) >= sizeof(fake_nfs.nodes[0].path))
      return NULL;
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
      if (!fake_nfs.nodes[i].used)
      {
         fake_node_t *n = &fake_nfs.nodes[i];
         memset(n, 0, sizeof(*n));
         n->used = 1;
         n->dir  = dir;
         strcpy(n->path, path);
         return n;
      }
   return NULL;
}

/* the directory @path sits in exists (the export root always does) */
static int fake_parent_ok(const char *path)
{
   char         buf[sizeof(fake_nfs.nodes[0].path)];
   char        *slash;
   fake_node_t *p;
   strcpy(buf, fake_norm(path));
   if (!(slash = strrchr(buf, '/')))
      return 1;
   *slash = '\0';
   return (p = fake_nfs_find(buf)) && p->dir;
}

fake_node_t *fake_nfs_put(const char *path, const char *text)
{
   fake_node_t *n = fake_nfs_find(path);
   if (!n)
      n = fake_new(path, 0);
   free(n->data);
   n->size = strlen(text);
   n->data = (uint8_t*)malloc(n->size + 1);
   memcpy(n->data, text, n->size + 1);
   return n;
}

fake_node_t *fake_nfs_mkdir_node(const char *path)
{
   fake_node_t *n = fake_nfs_find(path);
   return n ? n : fake_new(path, 1);
}

static void fake_drop(fake_node_t *n)
{
   free(n->data);
   memset(n, 0, sizeof(*n));
}

/* Counts the call; true when it is the one set to fail. */
static int fake_fails(struct rnfs_ctx *c, int op, const char *path)
{
   fake_nfs.calls[op]++;
   c->status = RNFS_STATUS_NONE;
   if (fake_nfs.fail_op == op && (fake_nfs.fail_path[0]
            ? path && !strcmp(fake_norm(path), fake_nfs.fail_path)
            : fake_nfs.calls[op] == fake_nfs.fail_nth))
   {
      c->status = fake_nfs.fail_status;
      strcpy(c->error, "injected failure");
      return 1;
   }
   return 0;
}

static int fake_answer(struct rnfs_ctx *c, uint32_t status)
{
   c->status = status;
   if (status)
      strcpy(c->error, "server refused");
   return status ? -1 : 0;
}

struct rnfs_ctx *rnfs_new(void) { return (struct rnfs_ctx*)calloc(1, sizeof(struct rnfs_ctx)); }
void rnfs_free(struct rnfs_ctx *c) { free(c); }
void rnfs_set_timeout(struct rnfs_ctx *c, unsigned seconds) { (void)c; (void)seconds; }
void rnfs_set_version(struct rnfs_ctx *c, unsigned version) { (void)c; (void)version; }
void rnfs_set_readahead(struct rnfs_ctx *c, uint32_t bytes) { (void)c; (void)bytes; }
void rnfs_set_ports(struct rnfs_ctx *c, uint16_t a, uint16_t b) { (void)c; (void)a; (void)b; }
const char *rnfs_get_error(const struct rnfs_ctx *c) { return c->error; }
uint32_t rnfs_get_status(const struct rnfs_ctx *c) { return c->status; }

int rnfs_connect(struct rnfs_ctx *c, const char *server, const char *export_path)
{
   (void)server; (void)export_path;
   return fake_answer(c, 0);
}

struct rnfs_file *rnfs_open(struct rnfs_ctx *c, const char *path, int flags)
{
   fake_node_t      *n;
   struct rnfs_file *f;
   if (fake_fails(c, FAKE_OP_OPEN, path))
      return NULL;
   n = fake_nfs_find(path);
   if (n && n->dir)
   {
      fake_answer(c, FAKE_NFS3ERR_ISDIR);
      return NULL;
   }
   if (!n)
   {
      if (!(flags & RNFS_O_CREAT) || !fake_parent_ok(path))
      {
         fake_answer(c, RNFS_STATUS_NOENT);
         return NULL;
      }
      if (!(n = fake_new(path, 0)))
      {
         fake_answer(c, FAKE_NFS3ERR_IO);
         return NULL;
      }
   }
   if (flags & RNFS_O_TRUNC)
   {
      free(n->data);
      n->data = NULL;
      n->size = 0;
   }
   f       = (struct rnfs_file*)calloc(1, sizeof(*f));
   f->node = n;
   fake_answer(c, 0);
   return f;
}

int64_t rnfs_read(struct rnfs_ctx *c, struct rnfs_file *f, void *buf, size_t len)
{
   size_t left;
   if (fake_fails(c, FAKE_OP_READ, f->node->path))
      return -1;
   left = f->pos < f->node->size ? (size_t)(f->node->size - f->pos) : 0;
   if (len > left)
      len = left;
   memcpy(buf, f->node->data + f->pos, len);
   f->pos += len;
   fake_answer(c, 0);
   return (int64_t)len;
}

int64_t rnfs_write(struct rnfs_ctx *c, struct rnfs_file *f, const void *buf, size_t len)
{
   fake_node_t *n = f->node;
   if (fake_fails(c, FAKE_OP_WRITE, f->node->path))
      return -1;
   if (f->pos + len > n->size)
   {
      n->data = (uint8_t*)realloc(n->data, (size_t)(f->pos + len));
      n->size = (size_t)(f->pos + len);
   }
   memcpy(n->data + f->pos, buf, len);
   f->pos += len;
   fake_answer(c, 0);
   return (int64_t)len;
}

int rnfs_close(struct rnfs_ctx *c, struct rnfs_file *f)
{
   int failed = fake_fails(c, FAKE_OP_CLOSE, f->node->path);
   free(f);
   return failed ? -1 : fake_answer(c, 0);
}

int rnfs_stat(struct rnfs_ctx *c, const char *path, struct rnfs_stat *st)
{
   fake_node_t *n;
   if (fake_fails(c, FAKE_OP_STAT, path))
      return -1;
   if (!*fake_norm(path))
   {
      st->size = 0; st->mtime = 0; st->is_dir = 1;
      return fake_answer(c, 0);
   }
   if (!(n = fake_nfs_find(path)))
      return fake_answer(c, RNFS_STATUS_NOENT);
   st->size   = n->size;
   st->mtime  = 0;
   st->is_dir = n->dir;
   return fake_answer(c, 0);
}

int rnfs_unlink(struct rnfs_ctx *c, const char *path)
{
   fake_node_t *n;
   if (fake_fails(c, FAKE_OP_UNLINK, path))
      return -1;
   if (!(n = fake_nfs_find(path)))
      return fake_answer(c, RNFS_STATUS_NOENT);
   fake_drop(n);
   return fake_answer(c, 0);
}

int rnfs_mkdir(struct rnfs_ctx *c, const char *path)
{
   if (fake_fails(c, FAKE_OP_MKDIR, path))
      return -1;
   if (fake_nfs.mkdir_race)
   {
      fake_nfs_mkdir_node(path);
      return fake_answer(c, FAKE_NFS3ERR_EXIST);
   }
   if (fake_nfs_find(path))
      return fake_answer(c, FAKE_NFS3ERR_EXIST);
   if (!fake_parent_ok(path))
      return fake_answer(c, RNFS_STATUS_NOENT);
   return fake_answer(c, fake_new(path, 1) ? 0 : FAKE_NFS3ERR_IO);
}

/* replaces an existing @to in one step, as RENAME does */
int rnfs_rename(struct rnfs_ctx *c, const char *from, const char *to)
{
   fake_node_t *src, *dst;
   if (fake_fails(c, FAKE_OP_RENAME, from))
      return -1;
   if (!(src = fake_nfs_find(from)) || !fake_parent_ok(to))
      return fake_answer(c, RNFS_STATUS_NOENT);
   if (strlen(fake_norm(to)) >= sizeof(src->path))
      return fake_answer(c, FAKE_NFS3ERR_IO);
   if ((dst = fake_nfs_find(to)) && dst != src)
      fake_drop(dst);
   strcpy(src->path, fake_norm(to));
   return fake_answer(c, 0);
}
