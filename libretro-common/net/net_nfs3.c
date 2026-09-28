/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_nfs3.c).
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
#include <stdio.h>

#include <net/net_nfs3.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#if !defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX) && !defined(RARCH_CONSOLE)
#include <unistd.h>
#include <sys/types.h>
#define RNFS_HAVE_GETUID 1
#endif

/* ---- ONC RPC ------------------------------------------------------ */

#define PMAP_PROG   100000
#define PMAP_VERS   2
#define PMAP_GETPORT 3
#define MOUNT_PROG  100005
#define MOUNT_VERS  3
#define MOUNT_MNT   1
#define MOUNT_UMNT  3
#define NFS_PROG    100003
#define NFS_VERS    3

#define NFS_NULL        0
#define NFS_GETATTR     1
#define NFS_SETATTR     2
#define NFS_LOOKUP      3
#define NFS_ACCESS      4
#define NFS_READ        6
#define NFS_WRITE       7
#define NFS_CREATE      8
#define NFS_MKDIR       9
#define NFS_REMOVE      12
#define NFS_RENAME      14
#define NFS_READDIR     16
#define NFS_READDIRPLUS 17

#define NFS3_OK          0
#define NFS3ERR_NOENT    2
#define NFS3ERR_NOTSUPP  10004
#define NFS3_FHSIZE      64
/* Default transfer size; FSINFO raises it to what the server allows,
 * up to NFS3_LARGE_IO, and the buffers grow with it. */
#define NFS3_MAX_IO      (64 * 1024)
#define NFS3_LARGE_IO    (1024 * 1024)
#define RNFS_BUF_SIZE(io) ((io) + 4096)
#define RNFS_RX_SIZE     RNFS_BUF_SIZE(NFS3_MAX_IO)
#define NFS_FSINFO       19

#define NF3REG 1
#define NF3DIR 2

struct rnfs_fh
{
   uint8_t  data[NFS3_FHSIZE];
   uint32_t len;
};

struct rnfs_file
{
   struct rnfs_fh fh;
   uint64_t offset;
   uint64_t size;
};

struct rnfs_dir
{
   struct rnfs_dirent ent;
   struct rnfs_fh fh;
   uint8_t *buf;         /* one READDIRPLUS reply's entry list */
   size_t   buf_len;
   size_t   buf_off;
   uint64_t cookie;
   uint8_t  cookieverf[8];
   uint8_t  done;
   uint8_t  plain;       /* server has no READDIRPLUS: READDIR + LOOKUP */
};

struct rnfs_ctx
{
   uint8_t *rx;
   uint8_t *tx;
   struct rnfs_fh root;
   uint32_t xid;
   uint32_t status;
   uint32_t io_size;         /* largest read or write, from FSINFO */
   size_t   buf_size;        /* rx and tx capacity */
   uint32_t uid;
   uint32_t gid;
   unsigned timeout;
   int      fd;              /* NFS connection */
   uint16_t nfs_port;
   uint16_t mount_port;
   uint8_t  connected;
   char     server[256];
   char     export_path[512];
   char     error[128];
};

static void rnfs_err(struct rnfs_ctx *c, const char *msg)
{
   strlcpy(c->error, msg, sizeof(c->error));
}

/* XDR writer / reader over a byte buffer. */
struct xdr
{
   uint8_t *p;
   uint8_t *end;
   int      fail;
};

static void xdr_u32(struct xdr *x, uint32_t v)
{
   if (x->end - x->p < 4) { x->fail = 1; return; }
   x->p[0] = (uint8_t)(v >> 24); x->p[1] = (uint8_t)(v >> 16);
   x->p[2] = (uint8_t)(v >> 8);  x->p[3] = (uint8_t)v;
   x->p += 4;
}

static void xdr_u64(struct xdr *x, uint64_t v)
{
   xdr_u32(x, (uint32_t)(v >> 32));
   xdr_u32(x, (uint32_t)v);
}

static void xdr_opaque(struct xdr *x, const void *d, size_t len)
{
   size_t pad = (4 - (len & 3)) & 3;
   xdr_u32(x, (uint32_t)len);
   if ((size_t)(x->end - x->p) < len + pad) { x->fail = 1; return; }
   memcpy(x->p, d, len);
   memset(x->p + len, 0, pad);
   x->p += len + pad;
}

static void xdr_string(struct xdr *x, const char *s)
{
   xdr_opaque(x, s, strlen(s));
}

static void xdr_fh(struct xdr *x, const struct rnfs_fh *fh)
{
   xdr_opaque(x, fh->data, fh->len);
}

static uint32_t xdr_get_u32(struct xdr *x)
{
   uint32_t v;
   if (x->end - x->p < 4) { x->fail = 1; return 0; }
   v = ((uint32_t)x->p[0] << 24) | ((uint32_t)x->p[1] << 16)
     | ((uint32_t)x->p[2] << 8) | x->p[3];
   x->p += 4;
   return v;
}

static uint64_t xdr_get_u64(struct xdr *x)
{
   uint64_t hi = xdr_get_u32(x);
   return (hi << 32) | xdr_get_u32(x);
}

/* Returns a pointer into the buffer; *len the octet count. */
static const uint8_t *xdr_get_opaque(struct xdr *x, size_t *len)
{
   const uint8_t *d;
   size_t n = xdr_get_u32(x);
   size_t pad = (4 - (n & 3)) & 3;
   if (x->fail || (size_t)(x->end - x->p) < n + pad) { x->fail = 1; *len = 0; return NULL; }
   d = x->p;
   x->p += n + pad;
   *len = n;
   return d;
}

static int xdr_get_fh(struct xdr *x, struct rnfs_fh *fh)
{
   size_t n;
   const uint8_t *d = xdr_get_opaque(x, &n);
   if (!d || n > NFS3_FHSIZE || n == 0)
      return -1;
   memcpy(fh->data, d, n);
   fh->len = (uint32_t)n;
   return 0;
}

/* fattr3: keeps size, mtime and the type. */
static void xdr_get_fattr(struct xdr *x, struct rnfs_stat *st)
{
   uint32_t type = xdr_get_u32(x);
   xdr_get_u32(x); xdr_get_u32(x); xdr_get_u32(x); xdr_get_u32(x); /* mode nlink uid gid */
   st->size = xdr_get_u64(x);
   xdr_get_u64(x);                            /* used */
   xdr_get_u32(x); xdr_get_u32(x);            /* rdev */
   xdr_get_u64(x); xdr_get_u64(x);            /* fsid fileid */
   xdr_get_u32(x); xdr_get_u32(x);            /* atime */
   st->mtime = xdr_get_u32(x); xdr_get_u32(x);
   xdr_get_u32(x); xdr_get_u32(x);            /* ctime */
   st->is_dir = (type == NF3DIR);
}

static void xdr_get_post_op_attr(struct xdr *x, struct rnfs_stat *st)
{
   if (xdr_get_u32(x))
      xdr_get_fattr(x, st);
}

static void xdr_get_wcc_data(struct xdr *x)
{
   struct rnfs_stat st;
   if (xdr_get_u32(x))                        /* pre_op_attr */
   {
      xdr_get_u64(x); xdr_get_u32(x); xdr_get_u32(x); xdr_get_u32(x); xdr_get_u32(x);
   }
   xdr_get_post_op_attr(x, &st);
}

/* ---- transport ---------------------------------------------------- */

static int rnfs_tcp_connect(struct rnfs_ctx *c, uint16_t port)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void**)&addr, port, c->server, SOCKET_TYPE_STREAM, AF_UNSPEC);
   if (fd < 0 || !addr)
   {
      if (addr)
         freeaddrinfo_retro(addr);
      rnfs_err(c, "cannot resolve server");
      return -1;
   }
   if (!socket_connect_with_timeout(fd, addr, (int)c->timeout * 1000))
   {
      freeaddrinfo_retro(addr);
      socket_close(fd);
      rnfs_err(c, "connect failed");
      return -1;
   }
   freeaddrinfo_retro(addr);
   socket_set_block(fd, false);
   return fd;
}

/* One RPC call on @fd: builds the header, sends the record, reads the
 * reply record into c->rx and positions @reply at the results. */
static int rnfs_rpc(struct rnfs_ctx *c, int fd, uint32_t prog, uint32_t vers,
      uint32_t proc, const uint8_t *args, size_t args_len, struct xdr *reply)
{
   struct xdr x;
   uint8_t   *msg = c->tx;
   uint32_t   xid = ++c->xid;
   uint32_t   len, rlen;
   int        to = (int)c->timeout * 1000;
   static const char machine[] = "retroarch";

   x.p = msg + 4; x.end = msg + c->buf_size; x.fail = 0;
   xdr_u32(&x, xid);
   xdr_u32(&x, 0);                            /* CALL */
   xdr_u32(&x, 2);                            /* RPC version */
   xdr_u32(&x, prog);
   xdr_u32(&x, vers);
   xdr_u32(&x, proc);
   /* AUTH_UNIX credentials */
   xdr_u32(&x, 1);
   xdr_u32(&x, 4 + 4 + 12 + 4 + 4 + 4 + 4);   /* stamp machine(9->12) uid gid gids[1] */
   xdr_u32(&x, 0);                            /* stamp */
   xdr_string(&x, machine);
   xdr_u32(&x, c->uid);
   xdr_u32(&x, c->gid);
   xdr_u32(&x, 1);
   xdr_u32(&x, c->gid);
   xdr_u32(&x, 0); xdr_u32(&x, 0);            /* AUTH_NULL verifier */
   if (args_len)
   {
      if ((size_t)(x.end - x.p) < args_len) { x.fail = 1; }
      else { memcpy(x.p, args, args_len); x.p += args_len; }
   }
   if (x.fail)
   {
      rnfs_err(c, "request too large");
      return -1;
   }
   len = (uint32_t)(x.p - (msg + 4));
   msg[0] = (uint8_t)(0x80 | (len >> 24)); msg[1] = (uint8_t)(len >> 16);
   msg[2] = (uint8_t)(len >> 8); msg[3] = (uint8_t)len;
   if (!socket_send_all_blocking_with_timeout(fd, msg, 4 + len, to, true))
   {
      rnfs_err(c, "send failed or timed out");
      return -1;
   }

   /* reply: one or more fragments, concatenated */
   rlen = 0;
   for (;;)
   {
      uint8_t  hdr[4];
      uint32_t frag, last;
      if (!socket_receive_all_blocking_with_timeout(fd, hdr, 4, to))
      {
         rnfs_err(c, "receive failed or timed out");
         return -1;
      }
      last = hdr[0] & 0x80;
      frag = ((uint32_t)(hdr[0] & 0x7f) << 24) | ((uint32_t)hdr[1] << 16)
           | ((uint32_t)hdr[2] << 8) | hdr[3];
      if (rlen + frag > c->buf_size)
      {
         rnfs_err(c, "reply too large");
         return -1;
      }
      if (!socket_receive_all_blocking_with_timeout(fd, c->rx + rlen, frag, to))
      {
         rnfs_err(c, "receive failed or timed out");
         return -1;
      }
      rlen += frag;
      if (last)
         break;
   }

   reply->p = c->rx; reply->end = c->rx + rlen; reply->fail = 0;
   if (xdr_get_u32(reply) != xid || xdr_get_u32(reply) != 1 || xdr_get_u32(reply) != 0)
   {
      rnfs_err(c, "rpc reply rejected");
      return -1;
   }
   xdr_get_u32(reply);                        /* verifier flavor */
   {
      size_t vl;
      xdr_get_opaque(reply, &vl);
   }
   if (xdr_get_u32(reply) != 0 || reply->fail)
   {
      rnfs_err(c, "rpc call not accepted");
      return -1;
   }
   return 0;
}

/* NFS call; on success the nfsstat3 is in c->status and @reply sits
 * after it. */
static int rnfs_call(struct rnfs_ctx *c, uint32_t proc,
      const uint8_t *args, size_t args_len, struct xdr *reply)
{
   if (c->fd < 0)
   {
      rnfs_err(c, "not connected");
      return -1;
   }
   if (rnfs_rpc(c, c->fd, NFS_PROG, NFS_VERS, proc, args, args_len, reply) != 0)
      return -1;
   c->status = xdr_get_u32(reply);
   return reply->fail ? -1 : 0;
}

static uint16_t rnfs_getport(struct rnfs_ctx *c, uint32_t prog, uint32_t vers)
{
   uint8_t  args[16];
   struct xdr x, reply;
   uint16_t port = 0;
   int fd = rnfs_tcp_connect(c, 111);
   if (fd < 0)
      return 0;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_u32(&x, prog); xdr_u32(&x, vers); xdr_u32(&x, 6); xdr_u32(&x, 0);
   if (rnfs_rpc(c, fd, PMAP_PROG, PMAP_VERS, PMAP_GETPORT, args, 16, &reply) == 0)
      port = (uint16_t)xdr_get_u32(&reply);
   socket_close(fd);
   if (!port)
      rnfs_err(c, "portmapper has no such service");
   return port;
}

/* ---- context ------------------------------------------------------ */

struct rnfs_ctx *rnfs_new(void)
{
   struct rnfs_ctx *c = (struct rnfs_ctx*)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->rx = (uint8_t*)malloc(RNFS_RX_SIZE);
   c->tx = (uint8_t*)malloc(RNFS_RX_SIZE);
   if (!c->rx || !c->tx)
   {
      free(c->rx); free(c->tx); free(c);
      return NULL;
   }
   c->fd       = -1;
   c->timeout  = 10;
   c->io_size  = NFS3_MAX_IO;
   c->buf_size = RNFS_RX_SIZE;
#ifdef RNFS_HAVE_GETUID
   c->uid = (uint32_t)getuid();
   c->gid = (uint32_t)getgid();
#else
   c->uid = 1000;
   c->gid = 1000;
#endif
   return c;
}

void rnfs_free(struct rnfs_ctx *c)
{
   if (!c)
      return;
   rnfs_disconnect(c);
   free(c->rx);
   free(c->tx);
   free(c);
}

void rnfs_set_timeout(struct rnfs_ctx *c, unsigned seconds) { c->timeout = seconds ? seconds : 10; }
void rnfs_set_identity(struct rnfs_ctx *c, uint32_t uid, uint32_t gid) { c->uid = uid; c->gid = gid; }
void rnfs_set_ports(struct rnfs_ctx *c, uint16_t nfs_port, uint16_t mount_port)
{
   c->nfs_port   = nfs_port;
   c->mount_port = mount_port;
}
const char *rnfs_get_error(const struct rnfs_ctx *c) { return c->error; }
uint32_t    rnfs_get_status(const struct rnfs_ctx *c) { return c->status; }
int         rnfs_get_fd(const struct rnfs_ctx *c)     { return c->fd; }

int rnfs_connect(struct rnfs_ctx *c, const char *server, const char *export_path)
{
   uint16_t mport, nport;
   int      mfd;
   uint8_t  args[600];
   struct xdr x, reply;

   rnfs_disconnect(c);
   strlcpy(c->server, server, sizeof(c->server));
   strlcpy(c->export_path, export_path, sizeof(c->export_path));

   mport = c->mount_port ? c->mount_port : rnfs_getport(c, MOUNT_PROG, MOUNT_VERS);
   if (!mport)
      return -1;
   nport = c->nfs_port ? c->nfs_port : rnfs_getport(c, NFS_PROG, NFS_VERS);
   if (!nport)
      return -1;

   /* MOUNT: root file handle for the export */
   if ((mfd = rnfs_tcp_connect(c, mport)) < 0)
      return -1;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_string(&x, export_path);
   if (x.fail || rnfs_rpc(c, mfd, MOUNT_PROG, MOUNT_VERS, MOUNT_MNT, args,
            (size_t)(x.p - args), &reply) != 0)
   {
      socket_close(mfd);
      return -1;
   }
   socket_close(mfd);
   c->status = xdr_get_u32(&reply);
   if (c->status != NFS3_OK || xdr_get_fh(&reply, &c->root) != 0)
   {
      rnfs_err(c, c->status == 13 ? "mount refused (access denied)" : "mount failed");
      return -1;
   }

   if ((c->fd = rnfs_tcp_connect(c, nport)) < 0)
      return -1;
   c->connected = 1;

   /* FSINFO: the transfer sizes the server prefers; buffers follow */
   c->io_size = NFS3_MAX_IO;
   {
      uint8_t  fargs[NFS3_FHSIZE + 4];
      struct rnfs_stat st;
      x.p = fargs; x.end = fargs + sizeof(fargs); x.fail = 0;
      xdr_fh(&x, &c->root);
      if (rnfs_call(c, NFS_FSINFO, fargs, (size_t)(x.p - fargs), &reply) == 0
            && c->status == NFS3_OK)
      {
         uint32_t rtmax, wtmax, io;
         xdr_get_post_op_attr(&reply, &st);
         rtmax = xdr_get_u32(&reply); xdr_get_u32(&reply); xdr_get_u32(&reply);
         wtmax = xdr_get_u32(&reply);
         io = rtmax < wtmax ? rtmax : wtmax;
         if (!reply.fail && io > NFS3_MAX_IO)
         {
            uint8_t *nrx, *ntx;
            if (io > NFS3_LARGE_IO)
               io = NFS3_LARGE_IO;
            nrx = (uint8_t*)realloc(c->rx, RNFS_BUF_SIZE(io));
            if (nrx)
               c->rx = nrx;
            ntx = (uint8_t*)realloc(c->tx, RNFS_BUF_SIZE(io));
            if (ntx)
               c->tx = ntx;
            if (nrx && ntx)
            {
               c->io_size  = io;
               c->buf_size = RNFS_BUF_SIZE(io);
            }
         }
      }
   }
   return 0;
}

void rnfs_disconnect(struct rnfs_ctx *c)
{
   if (c->fd >= 0)
      socket_close(c->fd);
   c->fd        = -1;
   c->connected = 0;
}

/* ---- path resolution ---------------------------------------------- */

/* Walks @path from the export root with LOOKUP, one component at a
 * time. With @parent set, stops before the last component and returns
 * it in @last (for CREATE / MKDIR / REMOVE). */
static int rnfs_walk(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len)
{
   const char *p = path;
   struct rnfs_stat tmp;
   uint8_t args[NFS3_FHSIZE + 4 + 260];
   struct xdr x, reply;

   *fh = c->root;
   memset(&tmp, 0, sizeof(tmp));
   tmp.is_dir = 1;
   if (last)
      last[0] = '\0';

   for (;;)
   {
      const char *e;
      size_t n;
      while (*p == '/')
         p++;
      if (!*p)
         break;
      e = strchr(p, '/');
      n = e ? (size_t)(e - p) : strlen(p);
      if (n >= 256)
      {
         rnfs_err(c, "name too long");
         return -1;
      }
      if (parent)
      {
         const char *q = e;
         while (q && *q == '/')
            q++;
         if (!q || !*q)
         {
            /* last component */
            if (n + 1 > last_len)
               return -1;
            memcpy(last, p, n);
            last[n] = '\0';
            break;
         }
      }
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, fh);
      xdr_opaque(&x, p, n);
      if (x.fail || rnfs_call(c, NFS_LOOKUP, args, (size_t)(x.p - args), &reply) != 0)
         return -1;
      if (c->status != NFS3_OK)
      {
         rnfs_err(c, c->status == NFS3ERR_NOENT ? "not found" : "lookup failed");
         return -1;
      }
      if (xdr_get_fh(&reply, fh) != 0)
         return -1;
      xdr_get_post_op_attr(&reply, &tmp);
      p = e ? e : p + n;
   }
   if (st)
   {
      /* attributes for the target: LOOKUP's post_op_attr may be
       * absent, so ask when the walk did not deliver them. */
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, fh);
      if (rnfs_call(c, NFS_GETATTR, args, (size_t)(x.p - args), &reply) != 0
            || c->status != NFS3_OK)
      {
         rnfs_err(c, "getattr failed");
         return -1;
      }
      xdr_get_fattr(&reply, st);
   }
   return 0;
}

/* ---- files -------------------------------------------------------- */

static int rnfs_set_size(struct rnfs_ctx *c, const struct rnfs_fh *fh, uint64_t size)
{
   uint8_t args[NFS3_FHSIZE + 64];
   struct xdr x, reply;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_fh(&x, fh);
   xdr_u32(&x, 0); xdr_u32(&x, 0); xdr_u32(&x, 0);   /* mode uid gid: no change */
   xdr_u32(&x, 1); xdr_u64(&x, size);                 /* size */
   xdr_u32(&x, 0); xdr_u32(&x, 0);                    /* atime mtime: don't change */
   xdr_u32(&x, 0);                                    /* guard: none */
   if (rnfs_call(c, NFS_SETATTR, args, (size_t)(x.p - args), &reply) != 0
         || c->status != NFS3_OK)
   {
      rnfs_err(c, "setattr failed");
      return -1;
   }
   return 0;
}

struct rnfs_file *rnfs_open(struct rnfs_ctx *c, const char *path, int flags)
{
   struct rnfs_file *f = (struct rnfs_file*)calloc(1, sizeof(*f));
   struct rnfs_stat st;
   char last[256];
   struct rnfs_fh dir;

   if (!f)
      return NULL;
   memset(&st, 0, sizeof(st));

   if (flags & RNFS_O_CREAT)
   {
      uint8_t args[NFS3_FHSIZE + 4 + 260 + 40];
      struct xdr x, reply;
      if (rnfs_walk(c, path, &dir, NULL, 1, last, sizeof(last)) != 0 || !last[0])
      {
         free(f);
         return NULL;
      }
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, &dir);
      xdr_string(&x, last);
      xdr_u32(&x, 0);                                 /* UNCHECKED */
      xdr_u32(&x, 1); xdr_u32(&x, 0644);              /* mode */
      xdr_u32(&x, 0); xdr_u32(&x, 0);                 /* uid gid */
      xdr_u32(&x, (flags & RNFS_O_TRUNC) ? 1 : 0);    /* size */
      if (flags & RNFS_O_TRUNC)
         xdr_u64(&x, 0);
      xdr_u32(&x, 0); xdr_u32(&x, 0);                 /* times */
      if (x.fail || rnfs_call(c, NFS_CREATE, args, (size_t)(x.p - args), &reply) != 0
            || c->status != NFS3_OK)
      {
         rnfs_err(c, "create failed");
         free(f);
         return NULL;
      }
      if (!xdr_get_u32(&reply) || xdr_get_fh(&reply, &f->fh) != 0)
      {
         /* no handle came back: look it up */
         if (rnfs_walk(c, path, &f->fh, &st, 0, NULL, 0) != 0)
         {
            free(f);
            return NULL;
         }
      }
      else
         xdr_get_post_op_attr(&reply, &st);
      if ((flags & RNFS_O_TRUNC) && st.size)
      {
         rnfs_set_size(c, &f->fh, 0);
         st.size = 0;
      }
   }
   else
   {
      if (rnfs_walk(c, path, &f->fh, &st, 0, NULL, 0) != 0)
      {
         free(f);
         return NULL;
      }
      if (st.is_dir)
      {
         rnfs_err(c, "is a directory");
         free(f);
         return NULL;
      }
      if ((flags & RNFS_O_TRUNC) && st.size)
      {
         if (rnfs_set_size(c, &f->fh, 0) != 0)
         {
            free(f);
            return NULL;
         }
         st.size = 0;
      }
   }
   f->size = st.size;
   return f;
}

int64_t rnfs_read(struct rnfs_ctx *c, struct rnfs_file *f, void *buf, size_t len)
{
   uint8_t *out = (uint8_t*)buf;
   size_t   done = 0;
   while (done < len)
   {
      uint8_t args[NFS3_FHSIZE + 16];
      struct xdr x, reply;
      struct rnfs_stat st;
      uint32_t count, eof;
      const uint8_t *d;
      size_t dl, n = len - done;
      if (n > c->io_size)
         n = c->io_size;
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, &f->fh);
      xdr_u64(&x, f->offset);
      xdr_u32(&x, (uint32_t)n);
      if (rnfs_call(c, NFS_READ, args, (size_t)(x.p - args), &reply) != 0)
         return -1;
      if (c->status != NFS3_OK)
      {
         rnfs_err(c, "read failed");
         return -1;
      }
      xdr_get_post_op_attr(&reply, &st);
      count = xdr_get_u32(&reply);
      eof   = xdr_get_u32(&reply);
      d     = xdr_get_opaque(&reply, &dl);
      if (!d || dl != count || dl > n)
      {
         rnfs_err(c, "bad read reply");
         return -1;
      }
      memcpy(out + done, d, dl);
      done      += dl;
      f->offset += dl;
      if (eof || dl < n)
         break;
   }
   return (int64_t)done;
}

int64_t rnfs_write(struct rnfs_ctx *c, struct rnfs_file *f, const void *buf, size_t len)
{
   const uint8_t *in = (const uint8_t*)buf;
   size_t done = 0;
   while (done < len)
   {
      /* the payload is built in the receive buffer, which is free
       * until the reply lands, and copied behind the header from there */
      uint8_t *args = c->rx;
      struct xdr x, reply;
      uint32_t count;
      size_t   n = len - done;
      if (n > c->io_size)
         n = c->io_size;
      x.p = args; x.end = c->rx + c->buf_size; x.fail = 0;
      xdr_fh(&x, &f->fh);
      xdr_u64(&x, f->offset);
      xdr_u32(&x, (uint32_t)n);
      xdr_u32(&x, 2);                            /* FILE_SYNC */
      xdr_opaque(&x, in + done, n);
      if (x.fail)
         return -1;
      if (rnfs_call(c, NFS_WRITE, args, (size_t)(x.p - args), &reply) != 0)
         return -1;
      if (c->status != NFS3_OK)
      {
         rnfs_err(c, "write failed");
         return -1;
      }
      xdr_get_wcc_data(&reply);
      count = xdr_get_u32(&reply);
      if (reply.fail || count > n)
         return -1;
      done      += count;
      f->offset += count;
      if (f->offset > f->size)
         f->size = f->offset;
      if (count == 0)
         break;
   }
   return (int64_t)done;
}

int64_t rnfs_seek(struct rnfs_ctx *c, struct rnfs_file *f, int64_t off, int whence)
{
   int64_t base = whence == 0 ? 0 : whence == 1 ? (int64_t)f->offset : (int64_t)f->size;
   (void)c;
   if (whence < 0 || whence > 2 || base + off < 0)
      return -1;
   f->offset = (uint64_t)(base + off);
   return (int64_t)f->offset;
}

int64_t rnfs_tell(const struct rnfs_file *f) { return (int64_t)f->offset; }

int rnfs_ftruncate(struct rnfs_ctx *c, struct rnfs_file *f, uint64_t size)
{
   if (rnfs_set_size(c, &f->fh, size) != 0)
      return -1;
   f->size = size;
   if (f->offset > size)
      f->offset = size;
   return 0;
}

int rnfs_close(struct rnfs_ctx *c, struct rnfs_file *f)
{
   (void)c;
   free(f);
   return 0;
}

int rnfs_stat(struct rnfs_ctx *c, const char *path, struct rnfs_stat *st)
{
   struct rnfs_fh fh;
   return rnfs_walk(c, path, &fh, st, 0, NULL, 0);
}

/* diropargs3 call with no results to keep: REMOVE, MKDIR (with sattr3). */
static int rnfs_dirop(struct rnfs_ctx *c, uint32_t proc, const char *path, int mkdir_attrs)
{
   struct rnfs_fh dir;
   char last[256];
   uint8_t args[NFS3_FHSIZE + 4 + 260 + 40];
   struct xdr x, reply;
   if (rnfs_walk(c, path, &dir, NULL, 1, last, sizeof(last)) != 0 || !last[0])
      return -1;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_fh(&x, &dir);
   xdr_string(&x, last);
   if (mkdir_attrs)
   {
      xdr_u32(&x, 1); xdr_u32(&x, 0755);
      xdr_u32(&x, 0); xdr_u32(&x, 0); xdr_u32(&x, 0); xdr_u32(&x, 0); xdr_u32(&x, 0);
   }
   if (x.fail || rnfs_call(c, proc, args, (size_t)(x.p - args), &reply) != 0)
      return -1;
   if (c->status != NFS3_OK)
   {
      rnfs_err(c, proc == NFS_REMOVE ? "remove failed" : "mkdir failed");
      return -1;
   }
   return 0;
}

int rnfs_unlink(struct rnfs_ctx *c, const char *path) { return rnfs_dirop(c, NFS_REMOVE, path, 0); }
int rnfs_mkdir(struct rnfs_ctx *c, const char *path)  { return rnfs_dirop(c, NFS_MKDIR, path, 1); }

int rnfs_rename(struct rnfs_ctx *c, const char *from, const char *to)
{
   struct rnfs_fh fdir, tdir;
   char flast[256], tlast[256];
   uint8_t args[2 * (NFS3_FHSIZE + 4 + 260)];
   struct xdr x, reply;
   if (rnfs_walk(c, from, &fdir, NULL, 1, flast, sizeof(flast)) != 0 || !flast[0])
      return -1;
   if (rnfs_walk(c, to, &tdir, NULL, 1, tlast, sizeof(tlast)) != 0 || !tlast[0])
      return -1;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_fh(&x, &fdir); xdr_string(&x, flast);
   xdr_fh(&x, &tdir); xdr_string(&x, tlast);
   if (x.fail || rnfs_call(c, NFS_RENAME, args, (size_t)(x.p - args), &reply) != 0
         || c->status != NFS3_OK)
   {
      rnfs_err(c, "rename failed");
      return -1;
   }
   return 0;
}

/* ---- directories -------------------------------------------------- */

struct rnfs_dir *rnfs_opendir(struct rnfs_ctx *c, const char *path)
{
   struct rnfs_dir *d = (struct rnfs_dir*)calloc(1, sizeof(*d));
   struct rnfs_stat st;
   if (!d)
      return NULL;
   if (rnfs_walk(c, path, &d->fh, &st, 0, NULL, 0) != 0 || !st.is_dir)
   {
      if (!c->error[0] || st.is_dir == 0)
         rnfs_err(c, "not a directory");
      free(d);
      return NULL;
   }
   if (!(d->buf = (uint8_t*)malloc(NFS3_MAX_IO)))
   {
      free(d);
      return NULL;
   }
   return d;
}

/* One READDIRPLUS round into d->buf: the raw entry list, kept as XDR.
 * A server without READDIRPLUS (unfs3, some embedded ones) answers
 * NOTSUPP; from then on plain READDIR is used and each entry's
 * attributes come from a LOOKUP. */
static int rnfs_readdir_fill(struct rnfs_ctx *c, struct rnfs_dir *d)
{
   uint8_t args[NFS3_FHSIZE + 32];
   struct xdr x, reply;
   struct rnfs_stat st;
   size_t n;

   for (;;)
   {
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, &d->fh);
      xdr_u64(&x, d->cookie);
      memcpy(x.p, d->cookieverf, 8); x.p += 8;   /* fixed 8 octets, no length */
      if (!d->plain)
         xdr_u32(&x, 8192);                       /* dircount */
      xdr_u32(&x, NFS3_MAX_IO);                   /* (max)count */
      if (rnfs_call(c, d->plain ? NFS_READDIR : NFS_READDIRPLUS, args,
               (size_t)(x.p - args), &reply) != 0)
         return -1;
      if (c->status == NFS3ERR_NOTSUPP && !d->plain)
      {
         d->plain = 1;
         continue;
      }
      break;
   }
   if (c->status != NFS3_OK)
   {
      rnfs_err(c, "readdir failed");
      return -1;
   }
   xdr_get_post_op_attr(&reply, &st);
   memcpy(d->cookieverf, reply.p, 8); reply.p += 8;
   n = (size_t)(reply.end - reply.p);
   if (n > NFS3_MAX_IO)
      n = NFS3_MAX_IO;
   memcpy(d->buf, reply.p, n);
   d->buf_len = n;
   d->buf_off = 0;
   return 0;
}

/* Attributes of one entry by LOOKUP, for the plain READDIR path. */
static void rnfs_entry_attrs(struct rnfs_ctx *c, struct rnfs_dir *d)
{
   uint8_t args[NFS3_FHSIZE + 4 + 260];
   struct xdr x, reply;
   struct rnfs_fh fh;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   xdr_fh(&x, &d->fh);
   xdr_string(&x, d->ent.name);
   if (x.fail || rnfs_call(c, NFS_LOOKUP, args, (size_t)(x.p - args), &reply) != 0
         || c->status != NFS3_OK || xdr_get_fh(&reply, &fh) != 0)
      return;
   xdr_get_post_op_attr(&reply, &d->ent.st);
}

const struct rnfs_dirent *rnfs_readdir(struct rnfs_ctx *c, struct rnfs_dir *d)
{
   for (;;)
   {
      struct xdr x;
      const uint8_t *name;
      size_t name_len;
      if (d->done)
         return NULL;
      if (d->buf_off >= d->buf_len)
      {
         if (rnfs_readdir_fill(c, d) != 0)
            return NULL;
      }
      x.p = d->buf + d->buf_off; x.end = d->buf + d->buf_len; x.fail = 0;
      if (!xdr_get_u32(&x))                    /* value_follows */
      {
         d->done = 1;                          /* then eof flag; either way the list ended */
         if (!x.fail && xdr_get_u32(&x) == 0)
         {
            /* eof false: more to fetch */
            d->done = 0;
            d->buf_off = d->buf_len;
            if (rnfs_readdir_fill(c, d) != 0)
               return NULL;
            continue;
         }
         return NULL;
      }
      xdr_get_u64(&x);                         /* fileid */
      name = xdr_get_opaque(&x, &name_len);
      d->cookie = xdr_get_u64(&x);
      memset(&d->ent.st, 0, sizeof(d->ent.st));
      if (!d->plain)
      {
         xdr_get_post_op_attr(&x, &d->ent.st);
         if (xdr_get_u32(&x))                  /* name_handle present */
         {
            size_t hl;
            xdr_get_opaque(&x, &hl);
         }
      }
      if (x.fail || !name)
      {
         d->done = 1;
         return NULL;
      }
      d->buf_off = (size_t)(x.p - d->buf);
      if (name_len >= sizeof(d->ent.name))
         name_len = sizeof(d->ent.name) - 1;
      memcpy(d->ent.name, name, name_len);
      d->ent.name[name_len] = '\0';
      if (strcmp(d->ent.name, ".") == 0 || strcmp(d->ent.name, "..") == 0)
         continue;
      if (d->plain)
         rnfs_entry_attrs(c, d);
      return &d->ent;
   }
}

void rnfs_closedir(struct rnfs_ctx *c, struct rnfs_dir *d)
{
   (void)c;
   if (!d)
      return;
   free(d->buf);
   free(d);
}

int rnfs_ping(struct rnfs_ctx *c)
{
   struct xdr reply;
   if (c->fd < 0)
      return -1;
   return rnfs_rpc(c, c->fd, NFS_PROG, NFS_VERS, NFS_NULL, NULL, 0, &reply);
}
