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
#ifdef RARCH_CONSOLE
#include <retro_timers.h>
#endif
#include <string/stdstring.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

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

/* NFSv4.0 (RFC 7530): everything is one COMPOUND procedure */
#define NFS4_COMPOUND     1
#define OP_ACCESS         3
#define OP_CLOSE          4
#define OP_CREATE         6
#define OP_GETATTR        9
#define OP_GETFH          10
#define OP_LOOKUP         15
#define OP_OPEN           18
#define OP_OPEN_CONFIRM   20
#define OP_PUTFH          22
#define OP_PUTROOTFH      24
#define OP_READ           25
/* NFSv4.2 (RFC 7862): a read that says which runs are holes */
#define OP_READ_PLUS      68
#define NFS4_CONTENT_DATA 0
#define NFS4_CONTENT_HOLE 1
#define OP_ILLEGAL        10044
#define OP_READDIR        26
#define OP_REMOVE         28
#define OP_RENAME         29
#define OP_RENEW          30
#define OP_SAVEFH         32
#define OP_SETATTR        34
#define OP_SETCLIENTID    35
#define OP_SETCLIENTID_CONFIRM 36
#define OP_RESTOREFH      31
#define OP_WRITE_OP       38
/* NFSv4.1 (RFC 8881): sessions */
#define OP_EXCHANGE_ID    42
#define OP_CREATE_SESSION 43
#define OP_DESTROY_SESSION 44
#define OP_SEQUENCE       53
#define OP_DESTROY_CLIENTID 57
#define OP_RECLAIM_COMPLETE 58
#define NFS4ERR_STALE_CLIENTID 10022
#define NFS4ERR_EXPIRED        10011
#define NFS4ERR_BAD_STATEID    10025
#define NFS4ERR_OLD_STATEID    10024
#define NFS4ERR_GRACE          10013
#define NFS4ERR_DELAY          10008
#define NFS4ERR_STALE_STATEID  10023
#define NFS4ERR_ADMIN_REVOKED  10047
#define NFS4ERR_MINOR_VERS_MISMATCH 10021
#define NFS4ERR_BADSESSION     10052
#define NFS4ERR_COMPLETE_ALREADY 10054
#define NFS4ERR_DEADSESSION    10078
#define NF4DIR            2
/* attribute bits: type(1) size(4) in word 0, time_modify(53) in word 1 */
#define FATTR4_W0_TYPE    (1u << 1)
#define FATTR4_W0_SIZE    (1u << 4)
#define FATTR4_W1_MODE    (1u << (33 - 32))
#define FATTR4_W1_TIME_MODIFY (1u << (53 - 32))

#define NFS3_OK          0
#define NFS3ERR_NOENT    2
#define NFS3ERR_STALE    70
#define NFS3ERR_BADHANDLE 10001
#define NFS3ERR_NOTSUPP  10004
#define NFS3_FHSIZE      64
/* Default transfer size; FSINFO raises it to what the server allows,
 * up to NFS3_LARGE_IO, and the buffers grow with it. */
#define NFS3_MAX_IO      (64 * 1024)
/* The largest transfer asked for once the server allows it. The 3DS
 * keeps the 64 KiB start: its RAM is small, four connections at 1 MiB
 * would hold 8 MiB of buffers, and its Wi-Fi gains nothing from them. */
#ifdef _3DS
#define NFS3_LARGE_IO    (64 * 1024)
#else
#define NFS3_LARGE_IO    (1024 * 1024)
#endif
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
   uint8_t  stateid[16];     /* v4: from OPEN, all-zero for the anonymous one */
   uint8_t  opened;          /* v4: an OPEN state to CLOSE */
   char    *path;            /* v4, opened: to open again if the server lost it */
   /* v4, opened: on its context's list of files holding open state,
    * which the context takes back whenever it identifies itself anew */
   struct rnfs_ctx  *owner;
   struct rnfs_file *next, *prev;
   /* read-ahead: the last fetched window, so a run of small
    * sequential reads costs one round trip per window */
   uint8_t *ra;
   uint64_t ra_off;
   size_t   ra_len;
   size_t   ra_cap;
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

#define RNFS_DCACHE_SIZE 16
/* NFSv4.1 session slots asked for: one per READ the pipeline keeps in
 * flight (RNFS_PIPELINE). */
#define RNFS_SLOTS 8
#define RNFS_DCACHE_PATH 256

struct rnfs_ctx
{
   uint8_t *rx;
   uint8_t *tx;
   struct rnfs_fh root;
   uint32_t xid;
   uint32_t status;
   uint32_t io_size;         /* largest read or write, from FSINFO */
   uint32_t readahead;       /* window per file, 0 = off */
   size_t   buf_size;        /* rx and tx capacity */
   uint32_t uid;
   uint32_t gid;
   unsigned timeout;
   int      fd;              /* NFS connection */
   uint16_t nfs_port;
   uint16_t mount_port;
   uint8_t  connected;
   uint8_t  version;         /* 3 or 4 */
   /* NFSv4 client state: a client id from SETCLIENTID, and one
    * open-owner whose seqid advances with each OPEN / CLOSE */
   uint64_t clientid;
   uint32_t open_seq;
   uint8_t  owner[8];
   /* NFSv4.1 and later: the minor version in use, and the session every
    * request after its setup runs in - one slot per request in flight,
    * each with its own sequence number */
   uint8_t  minor;
   uint8_t  have_session;
   /* the client's identity, made once per context and kept across
    * reconnects, so a server that held on to this client's state (a
    * courteous server, or one that lost only the session) gives it
    * back instead of starting a stranger */
   uint8_t  verf[8];
   uint8_t  have_identity;
   struct rnfs_file *stateful;      /* v4 files holding open state */
   uint8_t  reclaiming;
   uint8_t  redialing;
   /* why the last READ or WRITE failed, for its one retry */
   uint8_t  lost_session;
   uint8_t  stale_state;
   uint8_t  sessionid[16];
   uint32_t slots;                  /* granted, at most RNFS_SLOTS */
   uint8_t  read_plus;              /* 4.2: READ_PLUS, until the server refuses it */
   uint32_t slot_seq[RNFS_SLOTS];
   char     server[256];
   char     export_path[512];
   char     error[128];
   /* NFSv3: directories walked through recently, so an open in one of
    * them is a single LOOKUP instead of one per path component. Handles
    * are stable; one the server calls stale empties the cache and the
    * walk starts again from the root. */
   struct rnfs_dent
   {
      struct rnfs_fh fh;
      uint32_t       use;           /* last touched, for eviction */
      char           path[RNFS_DCACHE_PATH];
   } dcache[RNFS_DCACHE_SIZE];
   uint32_t dcache_tick;
   uint32_t calls;                  /* RPCs made, for tests */
   uint64_t rx_bytes;               /* RPC replies received, for tests */
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

static int xdr_get_post_op_attr(struct xdr *x, struct rnfs_stat *st)
{
   if (!xdr_get_u32(x))
      return 0;
   xdr_get_fattr(x, st);
   return 1;
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
/* Build and send one call; its xid is returned. */
static int rnfs_rpc_send(struct rnfs_ctx *c, int fd, uint32_t prog, uint32_t vers,
      uint32_t proc, const uint8_t *args, size_t args_len, uint32_t *xid_out)
{
   struct xdr x;
   uint8_t   *msg = c->tx;
   uint32_t   xid = ++c->xid;
   uint32_t   len;
   int        to = (int)c->timeout * 1000;
   static const char machine[] = "retroarch";

   *xid_out = xid;
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
   return 0;
}

/* Receive one reply record into c->rx; its xid in @xid_out and the
 * reply cursor placed after the accepted-reply header. */
static int rnfs_rpc_recv(struct rnfs_ctx *c, int fd, uint32_t *xid_out, struct xdr *reply)
{
   uint32_t rlen;
   int      to = (int)c->timeout * 1000;

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
      c->rx_bytes += frag;
      if (last)
         break;
   }

   reply->p = c->rx; reply->end = c->rx + rlen; reply->fail = 0;
   *xid_out = xdr_get_u32(reply);
   if (xdr_get_u32(reply) != 1 || xdr_get_u32(reply) != 0)
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

static void rnfs_drop(struct rnfs_ctx *c);

static int rnfs_rpc(struct rnfs_ctx *c, int fd, uint32_t prog, uint32_t vers,
      uint32_t proc, const uint8_t *args, size_t args_len, struct xdr *reply)
{
   uint32_t xid, got;
   if (rnfs_rpc_send(c, fd, prog, vers, proc, args, args_len, &xid) != 0)
      goto fail;
   do
   {
      if (rnfs_rpc_recv(c, fd, &got, reply) != 0)
         goto fail;
   } while (got != xid);
   return 0;
fail:
   /* a connection that failed a call is not trusted again: a reply
    * arriving late would answer the next call. The next call dials
    * afresh. */
   if (fd == c->fd)
      rnfs_drop(c);
   return -1;
}

/* NFS call; on success the nfsstat3 is in c->status and @reply sits
 * after it. */
static void rnfs_dcache_flush(struct rnfs_ctx *c);

static int rnfs_redial(struct rnfs_ctx *c);
static int rnfs_walk(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len);

static int rnfs_call(struct rnfs_ctx *c, uint32_t proc,
      const uint8_t *args, size_t args_len, struct xdr *reply)
{
   uint8_t *held = NULL;
   int      ret  = 0;
   c->calls++;
   if (c->fd < 0)
   {
      /* dialling again talks through c->rx and c->tx: arguments a
       * caller built there (WRITE builds in place, saving a copy) are
       * set aside first, or they go out as whatever the dial left */
      if (     (args >= c->rx && args < c->rx + c->buf_size)
            || (args >= c->tx && args < c->tx + c->buf_size))
      {
         if (!(held = (uint8_t*)malloc(args_len)))
            return -1;
         memcpy(held, args, args_len);
         args = held;
      }
      if (rnfs_redial(c) != 0)
      {
         free(held);
         rnfs_err(c, "not connected");
         return -1;
      }
   }
   if (rnfs_rpc(c, c->fd, NFS_PROG, NFS_VERS, proc, args, args_len, reply) != 0)
      ret = -1;
   else
   {
      c->status = xdr_get_u32(reply);
      ret       = reply->fail ? -1 : 0;
   }
   free(held);
   return ret;
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


/* ---- NFSv4.0 -------------------------------------------------------- */

/* A short wait. On a desktop an empty select with a timeout, without
 * dragging in the timer header (which pulls platform threading
 * includes); the consoles' socket layers do not all sleep in a select
 * with no descriptors, so there the platform's own sleep. */
static void nfs4_wait_ms(unsigned ms)
{
#ifdef RARCH_CONSOLE
   retro_sleep(ms);
#else
   struct timeval tv;
   tv.tv_sec  = ms / 1000;
   tv.tv_usec = (ms % 1000) * 1000;
   select(0, NULL, NULL, NULL, &tv);
#endif
}

/* Client verifier and open-owner: these need to be unique per client
 * instance, not secret, and the NFS client must not depend on the
 * crypto module (consoles build it without) - so a clock-and-counter
 * mix does. */
static void nfs4_unique(uint8_t *out, size_t len)
{
   /* No shared mutable state: two threads seeding at once still differ
    * because each has its own @out and stack-local addresses. */
   size_t   i;
   uint64_t v = (uint64_t)cpu_features_get_time_usec();
   v ^= (uint64_t)(uintptr_t)out << 17;
   v ^= (uint64_t)(uintptr_t)&i << 3;
   v ^= (uint64_t)(uintptr_t)&v >> 5;
   for (i = 0; i < len; i++)
   {
      v ^= v >> 12; v ^= v << 25; v ^= v >> 27;
      out[i] = (uint8_t)((v * 2685821657736338717ull) >> 56);
   }
}

static int nfs4_session_setup(struct rnfs_ctx *c);

/* The COMPOUND header: an empty tag, the minor version in use, and the
 * operation count - one more when a SEQUENCE leads. */
static void nfs4_head(struct xdr *x, const struct rnfs_ctx *c, unsigned nops,
      int sequenced)
{
   xdr_u32(x, 0);                       /* tag: empty */
   xdr_u32(x, c->minor);
   xdr_u32(x, nops + (sequenced ? 1 : 0));
}

/* SEQUENCE on @slot, whose sequence number advances; the server holds
 * each slot's last request, so a number reused is answered from that
 * reply instead of carried out. */
static void nfs4_sequence(struct xdr *x, struct rnfs_ctx *c, unsigned slot)
{
   xdr_u32(x, OP_SEQUENCE);
   memcpy(x->p, c->sessionid, 16); x->p += 16;
   xdr_u32(x, ++c->slot_seq[slot]);
   xdr_u32(x, slot);
   xdr_u32(x, c->slots - 1);            /* highest slot in use */
   xdr_u32(x, 0);                       /* cachethis: no */
}

/* The SEQUENCE result: its status. When the server refused the
 * SEQUENCE itself it did not take the number, so it is given back. */
static uint32_t nfs4_sequence_res(struct xdr *r, struct rnfs_ctx *c,
      unsigned slot)
{
   uint32_t st;
   xdr_get_u32(r);                      /* opnum */
   st = xdr_get_u32(r);
   if (st != NFS3_OK)
   {
      c->slot_seq[slot]--;
      return st;
   }
   r->p += 16;                          /* sessionid */
   xdr_get_u32(r); xdr_get_u32(r);      /* sequenceid, slotid */
   xdr_get_u32(r); xdr_get_u32(r);      /* highest, target highest */
   xdr_get_u32(r);                      /* status flags */
   return r->fail ? 1 : NFS3_OK;
}

/* Session setup operations run outside a session; everything else in
 * one, once there is one. */
static int nfs4_sequenced(const struct rnfs_ctx *c, const uint8_t *ops)
{
   uint32_t op;
   if (!c->have_session)
      return 0;
   op = ((uint32_t)ops[0] << 24) | ((uint32_t)ops[1] << 16)
      | ((uint32_t)ops[2] << 8) | ops[3];
   return op != OP_EXCHANGE_ID && op != OP_CREATE_SESSION
       && op != OP_DESTROY_SESSION && op != OP_DESTROY_CLIENTID;
}

/* One COMPOUND: @ops is the body after tag / minorversion / numops.
 * The reply is positioned after the tag and numres - and, in a session,
 * after the SEQUENCE result, so callers read their own results the same
 * in every minor version with nfs4_res(). */
static int nfs4_compound(struct rnfs_ctx *c, const uint8_t *ops, size_t ops_len,
      unsigned nops, struct xdr *reply)
{
   /* NFS4ERR_GRACE while the server recovers after a restart and
    * NFS4ERR_DELAY are "ask again shortly", not failures: the request
    * is repeated a second apart, on a fixed bound so a stuck server
    * still cannot block a caller forever - the one wait a v4 client
    * cannot avoid, and only ever right after a server came up. A lost
    * session (the lease ran out while idle) is set up again, once. */
   unsigned tries   = 15;
   int      renewed = 0;

   if (c->fd < 0 && rnfs_redial(c) != 0)
   {
      rnfs_err(c, "not connected");
      return -1;
   }
   for (;;)
   {
      uint8_t *args      = c->rx;       /* free until the reply lands */
      int      sequenced = nfs4_sequenced(c, ops);
      struct xdr x;
      size_t   tl;
      uint32_t sst       = NFS3_OK;

      x.p = args; x.end = c->rx + c->buf_size; x.fail = 0;
      nfs4_head(&x, c, nops, sequenced);
      if (sequenced)
         nfs4_sequence(&x, c, 0);
      if (x.fail || (size_t)(x.end - x.p) < ops_len)
         return -1;
      memcpy(x.p, ops, ops_len); x.p += ops_len;

      if (rnfs_rpc(c, c->fd, NFS_PROG, 4, NFS4_COMPOUND, args,
               (size_t)(x.p - args), reply) != 0)
         return -1;
      c->status = xdr_get_u32(reply);   /* overall status */
      xdr_get_opaque(reply, &tl);       /* tag */
      xdr_get_u32(reply);               /* numres */
      if (sequenced)
         sst = nfs4_sequence_res(reply, c, 0);

      if (     sequenced && !renewed
            && (sst == NFS4ERR_BADSESSION || sst == NFS4ERR_DEADSESSION
               || sst == NFS4ERR_EXPIRED  || sst == NFS4ERR_STALE_CLIENTID))
      {
         renewed          = 1;
         c->have_session  = 0;
         if (nfs4_session_setup(c) != 0)
            return -1;
         continue;
      }
      if ((c->status == NFS4ERR_GRACE || c->status == NFS4ERR_DELAY) && tries--)
      {
         nfs4_wait_ms(1000);
         continue;
      }
      break;
   }
   return reply->fail ? -1 : 0;
}

/* Next result: its status, positioned at the body. */
static uint32_t nfs4_res(struct xdr *r)
{
   xdr_get_u32(r);                      /* opnum */
   return xdr_get_u32(r);
}

static void nfs4_put_fh(struct xdr *x, const struct rnfs_fh *fh)
{
   xdr_u32(x, OP_PUTFH);
   xdr_fh(x, fh);
}

/* GETATTR request for type, size, time_modify */
static void nfs4_getattr_req(struct xdr *x)
{
   xdr_u32(x, OP_GETATTR);
   xdr_u32(x, 2);
   xdr_u32(x, FATTR4_W0_TYPE | FATTR4_W0_SIZE);
   xdr_u32(x, FATTR4_W1_TIME_MODIFY);
}

/* fattr4 reply: only the bits the request named can be set; each
 * present attribute follows in bit order. */
static int nfs4_get_fattr(struct xdr *r, struct rnfs_stat *st)
{
   uint32_t n = xdr_get_u32(r), w0 = 0, w1 = 0;
   size_t vl;
   const uint8_t *v;
   struct xdr a;
   if (n >= 1) w0 = xdr_get_u32(r);
   if (n >= 2) w1 = xdr_get_u32(r);
   while (n > 2) { xdr_get_u32(r); n--; }
   v = xdr_get_opaque(r, &vl);
   if (!v)
      return -1;
   a.p = (uint8_t*)v; a.end = (uint8_t*)v + vl; a.fail = 0;
   memset(st, 0, sizeof(*st));
   if (w0 & FATTR4_W0_TYPE)
      st->is_dir = xdr_get_u32(&a) == NF4DIR;
   if (w0 & FATTR4_W0_SIZE)
      st->size = xdr_get_u64(&a);
   if (w1 & FATTR4_W1_TIME_MODIFY)
   {
      st->mtime = xdr_get_u64(&a);
      xdr_get_u32(&a);
   }
   return a.fail ? -1 : 0;
}

/* Client id for this connection: SETCLIENTID then its CONFIRM. */
static void nfs4_state_link(struct rnfs_ctx *c, struct rnfs_file *f)
{
   if (f->owner)
      return;
   f->owner = c;
   f->prev  = NULL;
   f->next  = c->stateful;
   if (c->stateful)
      c->stateful->prev = f;
   c->stateful = f;
}

static void nfs4_state_unlink(struct rnfs_file *f)
{
   struct rnfs_ctx *c = f->owner;
   if (!c)
      return;
   if (f->prev)
      f->prev->next = f->next;
   else
      c->stateful = f->next;
   if (f->next)
      f->next->prev = f->prev;
   f->owner = NULL;
   f->next  = f->prev = NULL;
}

static int nfs4_open_reclaim(struct rnfs_ctx *c, struct rnfs_file *f);

/* After a restart the server grants a client it knew its opens back
 * during its grace period - and refuses new ones until grace ends
 * (90 seconds on Linux). Each file holding open state is claimed back
 * as soon as the client has identified itself again; where the server
 * did not restart this fails harmlessly (NO_GRACE) and the state it
 * kept goes on working. */
static void nfs4_reclaim_all(struct rnfs_ctx *c)
{
   struct rnfs_file *f;
   if (c->reclaiming)
      return;                          /* a session set up again mid-reclaim */
   c->reclaiming = 1;
   for (f = c->stateful; f; f = f->next)
      nfs4_open_reclaim(c, f);
   c->reclaiming = 0;
}

/* The verifier and open-owner, made the first time this context
 * identifies itself and kept for its life. */
static void nfs4_identity(struct rnfs_ctx *c)
{
   if (c->have_identity)
      return;
   nfs4_unique(c->verf, 8);
   nfs4_unique(c->owner, 8);
   c->have_identity = 1;
}

static int nfs4_setclientid(struct rnfs_ctx *c)
{
   uint8_t ops[256];
   struct xdr x, r;
   uint8_t verf[8];
   uint64_t clientid;
   char id[32];
   uint32_t st;
   unsigned i;
   static const char hex[] = "0123456789abcdef";

   nfs4_identity(c);
   memcpy(verf, c->verf, 8);
   /* client id string: "retroarch-" and the verifier in hex, unique
    * per context, no secrecy needed */
   memcpy(id, "retroarch-", 10);
   for (i = 0; i < 8; i++)
   {
      id[10 + i * 2]     = hex[verf[i] >> 4];
      id[10 + i * 2 + 1] = hex[verf[i] & 15];
   }
   id[26] = '\0';
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   xdr_u32(&x, OP_SETCLIENTID);
   memcpy(x.p, verf, 8); x.p += 8;
   xdr_string(&x, id);
   xdr_u32(&x, 0x40000000);             /* callback program: none we serve */
   xdr_string(&x, "tcp");
   xdr_string(&x, "0.0.0.0.0.0");
   xdr_u32(&x, 1);                      /* callback_ident */
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 1, &r) != 0)
      return -1;
   if (c->status != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "setclientid refused");
      return -1;
   }
   clientid = xdr_get_u64(&r);
   memcpy(verf, r.p, 8);
   x.p = ops; x.fail = 0;
   xdr_u32(&x, OP_SETCLIENTID_CONFIRM);
   xdr_u64(&x, clientid);
   memcpy(x.p, verf, 8); x.p += 8;
   if (nfs4_compound(c, ops, (size_t)(x.p - ops), 1, &r) != 0)
      return -1;
   st = nfs4_res(&r);
   if (c->status != NFS3_OK || st != NFS3_OK)
   {
      rnfs_err(c, "setclientid confirm refused");
      return -1;
   }
   c->clientid = clientid;
   c->open_seq = 0;
   nfs4_reclaim_all(c);
   return 0;
}

/* NFSv4.1+: the client's identity (EXCHANGE_ID), a session to run in
 * (CREATE_SESSION) and the word that it reclaims nothing from before
 * (RECLAIM_COMPLETE, which servers wait for before new opens).
 *
 * Returns: 0; 1 when the server does not speak this minor version;
 * -1 on any other failure. */
static int nfs4_session_setup(struct rnfs_ctx *c)
{
   uint8_t  ops[256];
   struct xdr x, r;
   uint8_t  verf[8];
   char     id[32];
   uint32_t seq, st, i;
   size_t   n;
   static const char hex[] = "0123456789abcdef";

   c->have_session = 0;
   nfs4_identity(c);
   memcpy(verf, c->verf, 8);
   memcpy(id, "retroarch-", 10);
   for (i = 0; i < 8; i++)
   {
      id[10 + i * 2]     = hex[verf[i] >> 4];
      id[10 + i * 2 + 1] = hex[verf[i] & 15];
   }
   id[26] = '\0';

   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   xdr_u32(&x, OP_EXCHANGE_ID);
   memcpy(x.p, verf, 8); x.p += 8;      /* co_verifier */
   xdr_string(&x, id);                  /* co_ownerid */
   xdr_u32(&x, 0x00010000);             /* EXCHGID4_FLAG_USE_NON_PNFS */
   xdr_u32(&x, 0);                      /* state protection: none */
   xdr_u32(&x, 0);                      /* client implementation id: none */
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 1, &r) != 0)
      return -1;
   if (c->status == NFS4ERR_MINOR_VERS_MISMATCH)
      return 1;
   if (c->status != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "exchange_id refused");
      return -1;
   }
   c->clientid = xdr_get_u64(&r);
   seq         = xdr_get_u32(&r);

   x.p = ops; x.fail = 0;
   xdr_u32(&x, OP_CREATE_SESSION);
   xdr_u64(&x, c->clientid);
   xdr_u32(&x, seq);
   xdr_u32(&x, 0);                      /* flags: no persistence, no back channel use */
   /* fore channel: this client's buffers, one slot per pipelined READ */
   xdr_u32(&x, 0);                      /* header padding */
   xdr_u32(&x, (uint32_t)c->buf_size);  /* max request */
   xdr_u32(&x, (uint32_t)c->buf_size);  /* max response */
   xdr_u32(&x, 4096);                   /* max response cached */
   xdr_u32(&x, 16);                     /* max operations */
   xdr_u32(&x, RNFS_SLOTS);             /* max requests */
   xdr_u32(&x, 0);                      /* no RDMA */
   /* back channel: not served, kept small */
   xdr_u32(&x, 0);
   xdr_u32(&x, 4096);
   xdr_u32(&x, 4096);
   xdr_u32(&x, 0);
   xdr_u32(&x, 2);
   xdr_u32(&x, 1);
   xdr_u32(&x, 0);
   xdr_u32(&x, 0x40000000);             /* callback program: none we serve */
   xdr_u32(&x, 1);                      /* one callback security flavor: */
   xdr_u32(&x, 0);                      /* AUTH_NONE */
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 1, &r) != 0)
      return -1;
   if (c->status != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "create_session refused");
      return -1;
   }
   memcpy(c->sessionid, r.p, 16); r.p += 16;
   xdr_get_u32(&r);                     /* sequence */
   xdr_get_u32(&r);                     /* flags */
   xdr_get_u32(&r);                     /* fore: header padding */
   xdr_get_u32(&r); xdr_get_u32(&r);    /* max request, max response */
   xdr_get_u32(&r); xdr_get_u32(&r);    /* max cached, max operations */
   n = xdr_get_u32(&r);                 /* max requests: the slots granted */
   if (r.fail || !n)
   {
      rnfs_err(c, "create_session reply");
      return -1;
   }
   c->slots     = (uint32_t)(n < RNFS_SLOTS ? n : RNFS_SLOTS);
   c->read_plus = c->minor >= 2;
   for (i = 0; i < RNFS_SLOTS; i++)
      c->slot_seq[i] = 0;
   c->have_session = 1;
   c->open_seq     = 0;
   /* opens this client held are claimed back before it says it has
    * reclaimed all it will */
   nfs4_reclaim_all(c);

   x.p = ops; x.fail = 0;
   xdr_u32(&x, OP_RECLAIM_COMPLETE);
   xdr_u32(&x, 0);                      /* for every file system */
   if (nfs4_compound(c, ops, (size_t)(x.p - ops), 1, &r) != 0)
      return -1;
   st = nfs4_res(&r);
   if (st != NFS3_OK && st != NFS4ERR_COMPLETE_ALREADY)
   {
      rnfs_err(c, "reclaim_complete refused");
      return -1;
   }
   return 0;
}

/* The newest minor version the server speaks, down to 4.0. */
static int nfs4_establish(struct rnfs_ctx *c)
{
   static const uint8_t minors[] = { 2, 1 };
   unsigned i;
   for (i = 0; i < sizeof(minors); i++)
   {
      int ret;
      c->minor = minors[i];
      if ((ret = nfs4_session_setup(c)) == 0)
         return 0;
      if (ret < 0)
         return -1;
   }
   c->minor = 0;
   return nfs4_setclientid(c);
}

/* The export root: PUTROOTFH then LOOKUP down the pseudo path. */
/* PUTROOTFH, a LOOKUP per component of @path, GETFH: the handle of
 * @path in the server's pseudo-filesystem into c->root.
 *
 * Returns: 0; -1 when the path is not there; -2 when the call itself
 * failed, which no other path would change. */
static int nfs4_mount_path(struct rnfs_ctx *c, const char *path)
{
   size_t   cap = NFS3_FHSIZE + 64 + strlen(path) * 2 + 32;
   uint8_t *ops = (uint8_t*)malloc(cap);
   struct xdr x, r;
   const char *p = path;
   unsigned n = 1, i;
   int ret = -1;

   if (!ops)
      return -1;
   x.p = ops; x.end = ops + cap; x.fail = 0;
   xdr_u32(&x, OP_PUTROOTFH);
   for (;;)
   {
      const char *e;
      size_t len;
      while (*p == '/')
         p++;
      if (!*p)
         break;
      e   = strchr(p, '/');
      len = e ? (size_t)(e - p) : strlen(p);
      xdr_u32(&x, OP_LOOKUP);
      xdr_opaque(&x, p, len);
      n++;
      p = e ? e : p + len;
   }
   xdr_u32(&x, OP_GETFH); n++;
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), n, &r) != 0)
   {
      ret = -2;
      goto done;
   }
   for (i = 0; i < n - 1; i++)
      if (nfs4_res(&r) != NFS3_OK)
         goto done;
   if (nfs4_res(&r) != NFS3_OK || xdr_get_fh(&r, &c->root) != 0)
      goto done;
   ret = 0;
done:
   free(ops);
   return ret;
}

/* The export as a v4 path. Servers may root the v4 namespace below the
 * v3 export paths (Linux's fsid=0, a Ganesha Pseudo of /): /media
 * exported as the root is "/" to v4, and /media/user is "/user". When
 * the export as given is not there and its first component is not in
 * the namespace either, that component is taken to lie above the v4
 * root and is dropped, down to the root itself, so the path a user
 * knows from v3 or another client finds its directory. A first
 * component the namespace does have means the path is simply wrong:
 * that is refused. */
static int nfs4_mount(struct rnfs_ctx *c)
{
   const char *p = c->export_path;

   for (;;)
   {
      char        first[256];
      const char *e;
      size_t      len;
      int         ret = nfs4_mount_path(c, p);
      if (ret == 0)
         return 0;
      if (ret == -2)
         return -1;                    /* the call failed, not the path */
      while (*p == '/')
         p++;
      if (!*p)
         break;                        /* the root itself was not there */
      e   = strchr(p, '/');
      len = e ? (size_t)(e - p) : strlen(p);
      if (!e || len >= sizeof(first))
      {
         /* one component left and not found: the namespace root is
          * all that remains to try */
         p = "";
         continue;
      }
      memcpy(first, p, len);
      first[len] = '\0';
      if ((ret = nfs4_mount_path(c, first)) == 0)
         break;                        /* it is there: the rest is wrong */
      if (ret == -2)
         return -1;
      p = e;
   }
   rnfs_err(c, "export not found");
   return -1;
}

static int nfs4_walk(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len);

/* Walk @path from the root with LOOKUPs in one COMPOUND, ending with
 * GETFH and GETATTR; with @parent the last component is left for the
 * caller in @last. */
static int nfs4_walk_once(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len)
{
   /* PUTFH, a LOOKUP per path component, GETFH, GETATTR: sized for the
    * path on the heap rather than a large stack frame */
   size_t   cap = NFS3_FHSIZE + 64 + strlen(path) * 2 + 300;
   uint8_t *ops = (uint8_t*)malloc(cap);
   struct xdr x, r;
   const char *p = path;
   unsigned n = 1, i;
   int ret = -1;

   if (!ops)
      return -1;
   if (last)
      last[0] = '\0';
   x.p = ops; x.end = ops + cap; x.fail = 0;
   nfs4_put_fh(&x, &c->root);
   for (;;)
   {
      const char *e, *q;
      size_t len;
      while (*p == '/')
         p++;
      if (!*p)
         break;
      e   = strchr(p, '/');
      len = e ? (size_t)(e - p) : strlen(p);
      if (len >= 256)
         goto done;
      if (parent)
      {
         q = e;
         while (q && *q == '/')
            q++;
         if (!q || !*q)
         {
            if (len + 1 > last_len)
               goto done;
            memcpy(last, p, len);
            last[len] = '\0';
            break;
         }
      }
      xdr_u32(&x, OP_LOOKUP);
      xdr_opaque(&x, p, len);
      n++;
      p = e ? e : p + len;
   }
   xdr_u32(&x, OP_GETFH); n++;
   nfs4_getattr_req(&x); n++;
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), n, &r) != 0)
      goto done;
   for (i = 0; i < n - 2; i++)
      if (nfs4_res(&r) != NFS3_OK)
      {
         rnfs_err(c, "not found");
         goto done;
      }
   if (nfs4_res(&r) != NFS3_OK || xdr_get_fh(&r, fh) != 0)
   {
      rnfs_err(c, "not found");
      goto done;
   }
   if (nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "getattr failed");
      goto done;
   }
   {
      struct rnfs_stat tmp;
      if (nfs4_get_fattr(&r, &tmp) != 0)
         goto done;
      if (st)
         *st = tmp;
   }
   ret = 0;
done:
   free(ops);
   return ret;
}

/* OPEN with create in @dir, then OPEN_CONFIRM when asked; the result
 * stateid is CLOSEd by rnfs_close. */
static int nfs4_open_create(struct rnfs_ctx *c, const struct rnfs_fh *dir,
      const char *name, int truncate, struct rnfs_file *f)
{
   uint8_t ops[NFS3_FHSIZE + 8 + 300];
   struct xdr x, r;
   uint32_t rflags;
   size_t sl;
   const uint8_t *sid;

   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, dir);
   xdr_u32(&x, OP_OPEN);
   xdr_u32(&x, c->open_seq);
   xdr_u32(&x, 2);                      /* OPEN4_SHARE_ACCESS_WRITE */
   xdr_u32(&x, 0);                      /* share_deny none */
   xdr_u64(&x, c->clientid);
   xdr_opaque(&x, c->owner, 8);         /* open_owner */
   xdr_u32(&x, 1);                      /* OPEN4_CREATE */
   xdr_u32(&x, 0);                      /* UNCHECKED4 */
   xdr_u32(&x, 2); xdr_u32(&x, truncate ? FATTR4_W0_SIZE : 0); xdr_u32(&x, FATTR4_W1_MODE);
   if (truncate)
   {
      xdr_u32(&x, 12);
      xdr_u64(&x, 0);
      xdr_u32(&x, 0644);
   }
   else
   {
      xdr_u32(&x, 4);
      xdr_u32(&x, 0644);
   }
   xdr_u32(&x, 0);                      /* CLAIM_NULL */
   xdr_string(&x, name);
   xdr_u32(&x, OP_GETFH);
   nfs4_getattr_req(&x);
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 4, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "create failed");
      return -1;
   }
   c->open_seq++;
   /* stateid, change_info(atomic + 2 x u64), rflags, attrset, delegation */
   sid = r.p; r.p += 16;
   xdr_get_u32(&r); xdr_get_u64(&r); xdr_get_u64(&r);
   rflags = xdr_get_u32(&r);
   {
      uint32_t nb = xdr_get_u32(&r);
      while (nb--) xdr_get_u32(&r);
   }
   {
      uint32_t dt = xdr_get_u32(&r);   /* delegation: none expected */
      if (dt != 0)
         return -1;
   }
   memcpy(f->stateid, sid, 16);
   if (nfs4_res(&r) != NFS3_OK || xdr_get_fh(&r, &f->fh) != 0)
      return -1;
   if (nfs4_res(&r) == NFS3_OK)
   {
      struct rnfs_stat st;
      if (nfs4_get_fattr(&r, &st) == 0)
         f->size = st.size;
   }
   f->opened = 1;
   nfs4_state_link(c, f);
   if (rflags & 2)                      /* OPEN4_RESULT_CONFIRM */
   {
      x.p = ops; x.fail = 0;
      nfs4_put_fh(&x, &f->fh);
      xdr_u32(&x, OP_OPEN_CONFIRM);
      memcpy(x.p, f->stateid, 16); x.p += 16;
      xdr_u32(&x, c->open_seq);
      if (nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
         return -1;
      if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
      {
         rnfs_err(c, "open confirm failed");
         return -1;
      }
      c->open_seq++;
      memcpy(f->stateid, r.p, 16);
   }
   (void)sl;
   return 0;
}

/* Open state the server no longer has: its lease ran out on a server
 * that reclaims, or it restarted. Read-only files carry none. */
static int nfs4_state_lost(uint32_t st)
{
   return st == NFS4ERR_STALE_STATEID || st == NFS4ERR_BAD_STATEID
       || st == NFS4ERR_EXPIRED       || st == NFS4ERR_ADMIN_REVOKED;
}

/* NFSv4.0 after a server restart: take the open back (CLAIM_PREVIOUS)
 * by its handle, as the same client the server knew before, which it
 * grants during its grace period - when an ordinary open is refused
 * until grace ends, which on Linux is 90 seconds.
 *
 * Returns: 0; -1 with c->status NFS4ERR_NO_GRACE when the server is
 * not in grace (the open goes by path then), or another failure. */
static int nfs4_open_reclaim(struct rnfs_ctx *c, struct rnfs_file *f)
{
   uint8_t  ops[NFS3_FHSIZE + 96];
   struct xdr x, r;
   uint32_t rflags, st;

   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &f->fh);
   xdr_u32(&x, OP_OPEN);
   xdr_u32(&x, c->open_seq);
   xdr_u32(&x, 2);                      /* OPEN4_SHARE_ACCESS_WRITE, as opened */
   xdr_u32(&x, 0);                      /* share_deny none */
   xdr_u64(&x, c->clientid);
   xdr_opaque(&x, c->owner, 8);         /* open_owner */
   xdr_u32(&x, 0);                      /* OPEN4_NOCREATE */
   xdr_u32(&x, 1);                      /* CLAIM_PREVIOUS */
   xdr_u32(&x, 0);                      /* delegation: none */
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || (st = nfs4_res(&r)) != NFS3_OK)
      return -1;
   c->open_seq++;
   memcpy(f->stateid, r.p, 16); r.p += 16;
   xdr_get_u32(&r); xdr_get_u64(&r); xdr_get_u64(&r);   /* change_info */
   rflags = xdr_get_u32(&r);
   {
      uint32_t nb = xdr_get_u32(&r);    /* attrset */
      while (nb-- && !r.fail)
         xdr_get_u32(&r);
   }
   if (r.fail || xdr_get_u32(&r) != 0)  /* no delegation asked, none taken */
      return -1;
   f->opened = 1;
   nfs4_state_link(c, f);
   if (rflags & 2)                      /* OPEN4_RESULT_CONFIRM */
   {
      x.p = ops; x.fail = 0;
      nfs4_put_fh(&x, &f->fh);
      xdr_u32(&x, OP_OPEN_CONFIRM);
      memcpy(x.p, f->stateid, 16); x.p += 16;
      xdr_u32(&x, c->open_seq);
      if (nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0
            || nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
         return -1;
      c->open_seq++;
      memcpy(f->stateid, r.p, 16);
   }
   return 0;
}

/* Open @f again by the path it was opened with, keeping its position:
 * a fresh OPEN state for one the server lost. */
static int nfs4_reopen(struct rnfs_ctx *c, struct rnfs_file *f)
{
   struct rnfs_fh dir;
   char   last[256];
   uint64_t offset = f->offset, size = f->size;
   if (!f->path)
      return -1;
   /* 4.0 after a restart: the server is in grace and refuses a new
    * open until it ends, but grants this client its old ones back. A
    * server that restarted knows the client no more until it says who
    * it is again - the same identity, which the server remembers. 4.1
    * says RECLAIM_COMPLETE when it sets up a session, so its server
    * can end grace for it at once and the open below goes through. */
   if (!c->minor)
   {
      if (nfs4_open_reclaim(c, f) == 0)
         goto done;
      if (     (c->status == NFS4ERR_STALE_CLIENTID || c->status == NFS4ERR_EXPIRED)
            && nfs4_setclientid(c) == 0 && nfs4_open_reclaim(c, f) == 0)
         goto done;
   }
   if (nfs4_walk(c, f->path, &dir, NULL, 1, last, sizeof(last)) != 0 || !last[0])
      return -1;
   if (nfs4_open_create(c, &dir, last, 0, f) != 0)
   {
      /* the client itself expired, not only its open: identify again
       * - the same identity, so a server that kept anything keeps it -
       * then open */
      if (c->status != NFS4ERR_EXPIRED && c->status != NFS4ERR_STALE_CLIENTID)
         return -1;
      if ((c->minor ? nfs4_session_setup(c) : nfs4_setclientid(c)) != 0
            || nfs4_open_create(c, &dir, last, 0, f) != 0)
         return -1;
   }
done:
   f->offset = offset;
   if (size > f->size)
      f->size = size;
   return 0;
}

/* As rnfs_walk(): walked again on a new connection when the old one
 * went. */
static int nfs4_walk(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len)
{
   if (nfs4_walk_once(c, path, fh, st, parent, last, last_len) == 0)
      return 0;
   if (c->fd >= 0 || c->redialing)
      return -1;
   return nfs4_walk_once(c, path, fh, st, parent, last, last_len);
}

static int nfs4_close(struct rnfs_ctx *c, struct rnfs_file *f)
{
   uint8_t ops[NFS3_FHSIZE + 32];
   struct xdr x, r;
   if (!f->opened)
      return 0;
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &f->fh);
   xdr_u32(&x, OP_CLOSE);
   xdr_u32(&x, c->open_seq);
   memcpy(x.p, f->stateid, 16); x.p += 16;
   if (nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
      return -1;
   c->open_seq++;
   f->opened = 0;
   return 0;
}


static int64_t nfs4_write(struct rnfs_ctx *c, struct rnfs_file *f, const void *buf, size_t len)
{
   const uint8_t *in = (const uint8_t*)buf;
   size_t done = 0;
   while (done < len)
   {
      /* the COMPOUND is assembled in rx and the RPC in tx, so the ops
       * need a buffer of their own for a payload this size */
      uint8_t *ops;
      uint8_t  sid[16];                 /* the state this WRITE goes with */
      struct xdr x, r;
      uint32_t count;
      size_t n = len - done;
      if (n > c->io_size)
         n = c->io_size;
      if (!(ops = (uint8_t*)malloc(n + NFS3_FHSIZE + 64)))
         return -1;
      x.p = ops; x.end = ops + n + NFS3_FHSIZE + 64; x.fail = 0;
      nfs4_put_fh(&x, &f->fh);
      xdr_u32(&x, OP_WRITE_OP);
      memcpy(sid, f->stateid, 16);
      memcpy(x.p, sid, 16); x.p += 16;
      xdr_u64(&x, f->offset);
      xdr_u32(&x, 2);                   /* FILE_SYNC4 */
      xdr_opaque(&x, in + done, n);
      if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      {
         free(ops);
         return -1;
      }
      if (     f->opened && nfs4_state_lost(c->status) && !c->stale_state
            && memcmp(sid, f->stateid, 16) != 0)
      {
         /* the call found the connection gone and the dial that mended
          * it took the open back (a reclaim): the state is fresh, only
          * this WRITE carried the old one */
         c->stale_state = 1;
         free(ops);
         continue;
      }
      if (f->opened && nfs4_state_lost(c->status) && !c->stale_state)
      {
         /* the same octets at the same offset, FILE_SYNC: safe to send
          * again once the file is open again */
         c->stale_state = 1;
         free(ops);
         if (nfs4_reopen(c, f) != 0)
         {
            c->stale_state = 0;
            rnfs_err(c, "write: open state lost");
            return -1;
         }
         continue;
      }
      c->stale_state = 0;
      free(ops);
      if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
      {
         rnfs_err(c, "write failed");
         return -1;
      }
      count = xdr_get_u32(&r);
      if (r.fail || count > n)
      {
         rnfs_err(c, "bad write reply");
         return -1;
      }
      done      += count;
      f->offset += count;
      if (f->offset > f->size)
         f->size = f->offset;
      if (count == 0)
         break;
   }
   return (int64_t)done;
}

static int nfs4_set_size(struct rnfs_ctx *c, const struct rnfs_file *f, uint64_t size)
{
   uint8_t ops[NFS3_FHSIZE + 64];
   struct xdr x, r;
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &f->fh);
   xdr_u32(&x, OP_SETATTR);
   memcpy(x.p, f->stateid, 16); x.p += 16;
   xdr_u32(&x, 2); xdr_u32(&x, FATTR4_W0_SIZE); xdr_u32(&x, 0);
   xdr_u32(&x, 8); xdr_u64(&x, size);
   if (nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "setattr failed");
      return -1;
   }
   return 0;
}

/* REMOVE or CREATE(directory) of the last component under its parent. */
static int nfs4_dirop(struct rnfs_ctx *c, int mkdir, const char *path)
{
   struct rnfs_fh dir;
   char last[256];
   uint8_t ops[NFS3_FHSIZE + 8 + 300];
   struct xdr x, r;
   if (nfs4_walk(c, path, &dir, NULL, 1, last, sizeof(last)) != 0 || !last[0])
      return -1;
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &dir);
   if (mkdir)
   {
      xdr_u32(&x, OP_CREATE);
      xdr_u32(&x, NF4DIR);
      xdr_string(&x, last);
      xdr_u32(&x, 2); xdr_u32(&x, 0); xdr_u32(&x, FATTR4_W1_MODE);
      xdr_u32(&x, 4); xdr_u32(&x, 0755);
   }
   else
   {
      xdr_u32(&x, OP_REMOVE);
      xdr_string(&x, last);
   }
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, mkdir ? "mkdir failed" : "remove failed");
      return -1;
   }
   return 0;
}

static int nfs4_rename(struct rnfs_ctx *c, const char *from, const char *to)
{
   struct rnfs_fh fdir, tdir;
   char flast[256], tlast[256];
   uint8_t ops[2 * NFS3_FHSIZE + 16 + 600];
   struct xdr x, r;
   if (nfs4_walk(c, from, &fdir, NULL, 1, flast, sizeof(flast)) != 0 || !flast[0])
      return -1;
   if (nfs4_walk(c, to, &tdir, NULL, 1, tlast, sizeof(tlast)) != 0 || !tlast[0])
      return -1;
   /* saved fh = source dir, current fh = target dir */
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &fdir);
   xdr_u32(&x, OP_SAVEFH);
   nfs4_put_fh(&x, &tdir);
   xdr_u32(&x, OP_RENAME);
   xdr_string(&x, flast);
   xdr_string(&x, tlast);
   if (x.fail || nfs4_compound(c, ops, (size_t)(x.p - ops), 4, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK
         || nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "rename failed");
      return -1;
   }
   return 0;
}

/* One READDIR page into d->buf: entries of cookie, name, fattr4. */
static int nfs4_readdir_fill(struct rnfs_ctx *c, struct rnfs_dir *d)
{
   uint8_t ops[NFS3_FHSIZE + 64];
   struct xdr x, r;
   size_t n;
   x.p = ops; x.end = ops + sizeof(ops); x.fail = 0;
   nfs4_put_fh(&x, &d->fh);
   xdr_u32(&x, OP_READDIR);
   xdr_u64(&x, d->cookie);
   memcpy(x.p, d->cookieverf, 8); x.p += 8;
   xdr_u32(&x, 8192);                   /* dircount */
   xdr_u32(&x, NFS3_MAX_IO);            /* maxcount */
   xdr_u32(&x, 2); xdr_u32(&x, FATTR4_W0_TYPE | FATTR4_W0_SIZE); xdr_u32(&x, FATTR4_W1_TIME_MODIFY);
   if (nfs4_compound(c, ops, (size_t)(x.p - ops), 2, &r) != 0)
      return -1;
   if (nfs4_res(&r) != NFS3_OK || nfs4_res(&r) != NFS3_OK)
   {
      rnfs_err(c, "readdir failed");
      return -1;
   }
   memcpy(d->cookieverf, r.p, 8); r.p += 8;
   n = (size_t)(r.end - r.p);
   if (n > NFS3_MAX_IO)
      n = NFS3_MAX_IO;
   memcpy(d->buf, r.p, n);
   d->buf_len = n;
   d->buf_off = 0;
   return 0;
}

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
   c->version  = 3;
   c->io_size  = NFS3_MAX_IO;
   c->buf_size = RNFS_RX_SIZE;
#ifdef RNFS_HAVE_GETUID
   c->uid = (uint32_t)getuid();
   c->gid = (uint32_t)getgid();
#else
   c->uid = 1000;
   c->gid = 1000;
#endif
   c->readahead = 1024 * 1024;         /* one window of small reads per round trip */
   return c;
}

void rnfs_free(struct rnfs_ctx *c)
{
   if (!c)
      return;
   /* files still open are the caller's: they only leave this list */
   while (c->stateful)
      nfs4_state_unlink(c->stateful);
   rnfs_disconnect(c);
   free(c->rx);
   free(c->tx);
   free(c);
}

void rnfs_set_timeout(struct rnfs_ctx *c, unsigned seconds) { c->timeout = seconds ? seconds : 10; }
void rnfs_set_identity(struct rnfs_ctx *c, uint32_t uid, uint32_t gid) { c->uid = uid; c->gid = gid; }
void rnfs_set_version(struct rnfs_ctx *c, unsigned version)
{
   c->version = (uint8_t)(version == 4 ? 4 : 3);
}

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
   rnfs_dcache_flush(c);   /* handles belong to the export mounted before */

   rnfs_disconnect(c);
   strlcpy(c->server, server, sizeof(c->server));
   strlcpy(c->export_path, export_path, sizeof(c->export_path));

   if (c->version == 4)
   {
      /* v4: no MOUNT protocol; the export is a pseudo-filesystem path
       * looked up from the root handle, and the client identifies
       * itself first. 2049 unless told otherwise, portmapper or not. */
      nport = c->nfs_port ? c->nfs_port : 2049;
      if ((c->fd = rnfs_tcp_connect(c, nport)) < 0)
         return -1;
      c->connected = 1;
      c->io_size   = NFS3_MAX_IO;
      if (nfs4_establish(c) != 0 || nfs4_mount(c) != 0)
      {
         rnfs_disconnect(c);
         return -1;
      }
      return 0;
   }

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

/* The connection is gone; the server and export stay, so the next
 * call can dial again. */
static void rnfs_drop(struct rnfs_ctx *c)
{
   if (c->fd >= 0)
      socket_close(c->fd);
   c->fd           = -1;
   c->connected    = 0;
   c->have_session = 0;
}

/* Dial the server and export this context last connected to - after a
 * server restart, a dropped link, a timeout. Not from inside a dial. */
static int rnfs_redial(struct rnfs_ctx *c)
{
   char server[256];
   char export_path[512];
   int  ret;
   if (c->redialing || !c->server[0])
      return -1;
   strlcpy(server, c->server, sizeof(server));
   strlcpy(export_path, c->export_path, sizeof(export_path));
   c->redialing = 1;
   ret          = rnfs_connect(c, server, export_path);
   c->redialing = 0;
   return ret;
}

void rnfs_disconnect(struct rnfs_ctx *c)
{
   if (c->fd >= 0)
      socket_close(c->fd);
   c->fd           = -1;
   c->connected    = 0;
   c->have_session = 0;   /* the server lets it lapse with the lease */
}

/* ---- path resolution ---------------------------------------------- */

/* Walks @path from the export root with LOOKUP, one component at a
 * time. With @parent set, stops before the last component and returns
 * it in @last (for CREATE / MKDIR / REMOVE). */
static void rnfs_dcache_flush(struct rnfs_ctx *c)
{
   unsigned i;
   for (i = 0; i < RNFS_DCACHE_SIZE; i++)
      c->dcache[i].path[0] = '\0';
}

/* The cached handle of directory @key (normalised, no leading '/'). */
static const struct rnfs_fh *rnfs_dcache_get(struct rnfs_ctx *c, const char *key)
{
   unsigned i;
   for (i = 0; i < RNFS_DCACHE_SIZE; i++)
      if (c->dcache[i].path[0] && string_is_equal(c->dcache[i].path, key))
      {
         c->dcache[i].use = ++c->dcache_tick;
         return &c->dcache[i].fh;
      }
   return NULL;
}

static void rnfs_dcache_put(struct rnfs_ctx *c, const char *key,
      const struct rnfs_fh *fh)
{
   unsigned i, victim = 0;
   if (!*key || strlen(key) >= RNFS_DCACHE_PATH)
      return;
   for (i = 0; i < RNFS_DCACHE_SIZE; i++)
   {
      if (c->dcache[i].path[0] && string_is_equal(c->dcache[i].path, key))
      {
         victim = i;
         break;
      }
      if (!c->dcache[i].path[0] || c->dcache[i].use < c->dcache[victim].use)
         victim = i;
      if (!c->dcache[i].path[0])
         break;
   }
   c->dcache[victim].fh  = *fh;
   c->dcache[victim].use = ++c->dcache_tick;
   strlcpy(c->dcache[victim].path, key, RNFS_DCACHE_PATH);
}

/* @path as components joined by single '/', into @out; the length of
 * the directory part (all but the last component) into *@dir_len. */
static int rnfs_normalise(const char *path, char *out, size_t size,
      size_t *dir_len)
{
   size_t o = 0;
   *dir_len = 0;
   for (;;)
   {
      const char *e;
      size_t      n;
      while (*path == '/')
         path++;
      if (!*path)
         break;
      e = strchr(path, '/');
      n = e ? (size_t)(e - path) : strlen(path);
      if (o + n + 2 > size)
         return -1;
      if (o)
         out[o++] = '/';
      *dir_len = o ? o - 1 : 0;
      memcpy(out + o, path, n);
      o   += n;
      path = e ? e : path + n;
   }
   out[o] = '\0';
   return 0;
}

static int rnfs_walk_from(struct rnfs_ctx *c, const char *norm, size_t start,
      struct rnfs_fh *fh, struct rnfs_stat *st, int parent,
      char *last, size_t last_len, int *stale)
{
   const char *p = norm + start;
   struct rnfs_stat tmp;
   uint8_t args[NFS3_FHSIZE + 4 + 260];
   struct xdr x, reply;
   int have_attr = 0;

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
      if (parent && !e)
      {
         /* last component */
         if (n + 1 > last_len)
            return -1;
         memcpy(last, p, n);
         last[n] = '\0';
         break;
      }
      x.p = args; x.end = args + sizeof(args); x.fail = 0;
      xdr_fh(&x, fh);
      xdr_opaque(&x, p, n);
      if (x.fail || rnfs_call(c, NFS_LOOKUP, args, (size_t)(x.p - args), &reply) != 0)
         return -1;
      if (c->status != NFS3_OK)
      {
         /* a cached handle the server no longer knows */
         if (start && (c->status == NFS3ERR_STALE || c->status == NFS3ERR_BADHANDLE))
            *stale = 1;
         rnfs_err(c, c->status == NFS3ERR_NOENT ? "not found" : "lookup failed");
         return -1;
      }
      if (xdr_get_fh(&reply, fh) != 0)
         return -1;
      have_attr = xdr_get_post_op_attr(&reply, &tmp);
      p = e ? e : p + n;
      /* remember directories on the way, and the target if it is one */
      if (have_attr && tmp.is_dir)
      {
         char key[RNFS_DCACHE_PATH];
         size_t k = (size_t)(p - norm);
         if (k < sizeof(key))
         {
            memcpy(key, norm, k);
            key[k] = '\0';
            rnfs_dcache_put(c, key, fh);
         }
      }
   }
   if (st)
   {
      /* LOOKUP's own attributes are the target's when they came; ask
       * only when the walk did not deliver them */
      if (have_attr)
         *st = tmp;
      else
      {
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
   }
   return 0;
}

/* Walk @path from the root (or the nearest directory cached on the
 * way) with LOOKUPs; with @parent the last component is left for the
 * caller in @last. */
static int rnfs_walk_once(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len)
{
   char   norm[RNFS_DCACHE_PATH + 256];
   size_t dir_len, k;
   int    stale = 0;

   if (rnfs_normalise(path, norm, sizeof(norm), &dir_len) != 0)
   {
      rnfs_err(c, "path too long");
      return -1;
   }
   /* the deepest cached directory the path passes through: its own
    * directory part, else a shorter prefix at a component boundary */
   for (k = parent ? dir_len : strlen(norm); k > 0; k--)
   {
      if (norm[k] == '/' || norm[k] == '\0')
      {
         const struct rnfs_fh *hit;
         char save = norm[k];
         norm[k]   = '\0';
         hit       = rnfs_dcache_get(c, norm);
         norm[k]   = save;
         if (hit)
         {
            *fh = *hit;
            if (rnfs_walk_from(c, norm, k, fh, st, parent, last, last_len, &stale) == 0)
               return 0;
            if (!stale)
               return -1;
            rnfs_dcache_flush(c);   /* the server moved on: start over */
            break;
         }
      }
   }
   *fh = c->root;
   return rnfs_walk_from(c, norm, 0, fh, st, parent, last, last_len, &stale);
}

/* The NFSv4 minor version the connection settled on (0, 1 or 2). */
unsigned rnfs_get_minor_version(const struct rnfs_ctx *c)
{
   return c->minor;
}

/* A walk looks things up and changes nothing: one that failed because
 * the connection went is walked again on a new one. */
static int rnfs_walk(struct rnfs_ctx *c, const char *path, struct rnfs_fh *fh,
      struct rnfs_stat *st, int parent, char *last, size_t last_len)
{
   if (rnfs_walk_once(c, path, fh, st, parent, last, last_len) == 0)
      return 0;
   if (c->fd >= 0 || c->redialing)
      return -1;
   return rnfs_walk_once(c, path, fh, st, parent, last, last_len);
}

/* Octets of RPC replies this connection has received: for tests that
 * check what crossed the network. */
uint64_t rnfs_get_rx_bytes(const struct rnfs_ctx *c)
{
   return c->rx_bytes;
}

/* READ_PLUS in use for reads (NFSv4.2, until a server refuses it). */
int rnfs_get_read_plus(const struct rnfs_ctx *c)
{
   return c->read_plus;
}

/* RPCs this connection has made: for tests that count round trips. */
uint32_t rnfs_get_call_count(const struct rnfs_ctx *c)
{
   return c->calls;
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

   if (c->version == 4)
   {
      if (flags & RNFS_O_CREAT)
      {
         if (nfs4_walk(c, path, &dir, NULL, 1, last, sizeof(last)) != 0 || !last[0]
               || nfs4_open_create(c, &dir, last, (flags & RNFS_O_TRUNC) != 0, f) != 0)
         {
            free(f);
            return NULL;
         }
         {
            size_t plen = strlen(path) + 1;
            if ((f->path = (char*)malloc(plen)))
               memcpy(f->path, path, plen);
         }
         return f;
      }
      /* reads go with the anonymous stateid: no OPEN state to keep */
      if (nfs4_walk(c, path, &f->fh, &st, 0, NULL, 0) != 0)
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
      if ((flags & RNFS_O_TRUNC) && st.size && nfs4_set_size(c, f, 0) != 0)
      {
         free(f);
         return NULL;
      }
      f->size = (flags & RNFS_O_TRUNC) ? 0 : st.size;
      return f;
   }

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

/* Up to this many READ calls in flight at once. */
#define RNFS_PIPELINE 8

/* Send one READ for @n octets at @off: v3 as its own call, v4 as a
 * PUTFH+READ compound - in a session led by a SEQUENCE on @slot. */
static int rnfs_send_read(struct rnfs_ctx *c, struct rnfs_file *f,
      uint64_t off, size_t n, unsigned slot, uint32_t *xid)
{
   uint8_t args[NFS3_FHSIZE + 128];
   struct xdr x;
   x.p = args; x.end = args + sizeof(args); x.fail = 0;
   if (c->version == 4)
   {
      nfs4_head(&x, c, 2, c->have_session);   /* PUTFH, READ */
      if (c->have_session)
         nfs4_sequence(&x, c, slot);
      nfs4_put_fh(&x, &f->fh);
      /* READ_PLUS takes READ's arguments; its reply sends holes as
       * their extent rather than as zeros */
      xdr_u32(&x, c->read_plus ? OP_READ_PLUS : OP_READ);
      memcpy(x.p, f->stateid, 16); x.p += 16;
      xdr_u64(&x, off);
      xdr_u32(&x, (uint32_t)n);
      if (x.fail)
         return -1;
      return rnfs_rpc_send(c, c->fd, NFS_PROG, 4, NFS4_COMPOUND, args, (size_t)(x.p - args), xid);
   }
   xdr_fh(&x, &f->fh);
   xdr_u64(&x, off);
   xdr_u32(&x, (uint32_t)n);
   if (x.fail)
      return -1;
   return rnfs_rpc_send(c, c->fd, NFS_PROG, NFS_VERS, NFS_READ, args, (size_t)(x.p - args), xid);
}

/* Parse a READ reply at the cursor: data span in @d/@dl, @eof; 1 on
 * success, 0 for a v4 GRACE/DELAY (ask again), -1 on failure. */
static int rnfs_parse_read(struct rnfs_ctx *c, struct xdr *r, unsigned slot,
      uint64_t at, uint64_t size, uint8_t *dst, size_t want, size_t *got,
      uint32_t *eof)
{
   struct rnfs_stat st;
   const uint8_t *d;
   size_t   dl;
   uint32_t count;
   if (c->version == 4)
   {
      size_t   tl;
      uint32_t op, ost;
      c->status = xdr_get_u32(r);
      xdr_get_opaque(r, &tl);              /* tag */
      xdr_get_u32(r);                      /* numres */
      if (c->have_session)
      {
         uint32_t sst = nfs4_sequence_res(r, c, slot);
         if (sst == NFS4ERR_DELAY)
            return 0;
         if (sst != NFS3_OK)
         {
            if (sst == NFS4ERR_BADSESSION || sst == NFS4ERR_DEADSESSION)
               c->lost_session = 1;
            rnfs_err(c, "read: session refused");
            return -1;
         }
      }
      if (nfs4_state_lost(c->status))
         c->stale_state = 1;
      if (c->status == NFS4ERR_GRACE || c->status == NFS4ERR_DELAY)
         return 0;
      if (nfs4_res(r) != NFS3_OK)          /* PUTFH */
      {
         rnfs_err(c, "read failed");
         return -1;
      }
      op  = xdr_get_u32(r);
      ost = xdr_get_u32(r);
      /* a server that has no READ_PLUS after all: plain READ from
       * here on, and this one asked again as that */
      if (     (op == OP_READ_PLUS || op == OP_ILLEGAL)
            && (ost == NFS3ERR_NOTSUPP || ost == OP_ILLEGAL))
      {
         c->read_plus = 0;
         return 2;
      }
      if (c->status != NFS3_OK || ost != NFS3_OK)
      {
         rnfs_err(c, "read failed");
         return -1;
      }
      *eof = xdr_get_u32(r);
      if (op == OP_READ_PLUS)
      {
         /* runs of data and holes, in order from @at: data copied in,
          * holes zeroed here instead of crossing the network */
         uint32_t n     = xdr_get_u32(r);
         size_t   pos   = 0;
         while (n-- && !r->fail)
         {
            uint32_t type = xdr_get_u32(r);
            uint64_t sof  = xdr_get_u64(r);
            uint64_t slen;
            if (sof != at + pos)
               goto broken;               /* runs out of order */
            if (type == NFS4_CONTENT_DATA)
            {
               if (!(d = xdr_get_opaque(r, &dl)) || dl > want - pos)
                  goto broken;
               memcpy(dst + pos, d, dl);
               pos += dl;
            }
            else if (type == NFS4_CONTENT_HOLE)
            {
               slen = xdr_get_u64(r);
               if (slen > want - pos)
                  slen = want - pos;         /* a hole may run on past the request */
               memset(dst + pos, 0, (size_t)slen);
               pos += (size_t)slen;
            }
            else
               goto broken;
         }
         /* nothing read and not at the end: no progress to make; or the
          * end claimed short of the size the file was opened with */
         if (     r->fail || (!pos && want && !*eof)
               || (*eof && at + pos < size))
            goto broken;
         *got = pos;
         return 1;
broken:
         /* a reply no READ could have given - servers exist whose
          * READ_PLUS is broken (nfs-ganesha 4.3 answers every range
          * with one empty data run): plain READ from here on, and this
          * range asked again as that */
         c->read_plus = 0;
         return 2;
      }
      if (!(d = xdr_get_opaque(r, &dl)) || dl > want)
         return -1;
      memcpy(dst, d, dl);
      *got = dl;
      return 1;
   }
   c->status = xdr_get_u32(r);
   if (c->status != NFS3_OK)
   {
      rnfs_err(c, "read failed");
      return -1;
   }
   xdr_get_post_op_attr(r, &st);
   count = xdr_get_u32(r);
   *eof  = xdr_get_u32(r);
   d     = xdr_get_opaque(r, &dl);
   if (!d || dl != count || dl > want)
      return -1;
   memcpy(dst, d, dl);
   *got = dl;
   return 1;
}

/* Fetch @len octets at @off into @out with pipelined READs; the
 * octets read (short at end of file), -1 on failure. */
static int64_t rnfs_fetch_once(struct rnfs_ctx *c, struct rnfs_file *f,
      uint64_t off, uint8_t *out, size_t len)
{
   uint32_t xids[RNFS_PIPELINE];
   size_t   offs[RNFS_PIPELINE], lens[RNFS_PIPELINE];
   unsigned sslot[RNFS_PIPELINE];       /* the session slot each READ is on */
   unsigned inflight = 0, i, grace = 0, short_at = 0;
   /* in a session no more READs than slots, each on its own */
   unsigned depth    = (c->version == 4 && c->have_session && c->slots < RNFS_PIPELINE)
      ? c->slots : RNFS_PIPELINE;
   uint32_t busy     = 0;
   size_t   sent = 0, end = 0;     /* contiguous data: up to the first short reply */
   int      eof = 0, fail = 0;

   if (c->fd < 0)
   {
      rnfs_err(c, "not connected");
      return -1;
   }
   while ((sent < len && !eof && !fail) || inflight)
   {
      while (inflight < depth && sent < len && !eof && !fail)
      {
         size_t   n  = len - sent;
         unsigned sl = 0;
         if (n > c->io_size)
            n = c->io_size;
         while (sl < RNFS_PIPELINE - 1 && (busy & (1u << sl)))
            sl++;
         if (rnfs_send_read(c, f, off + sent, n, sl, &xids[inflight]) != 0)
         {
            rnfs_drop(c);                /* the link went: dial again */
            fail = 1;
            break;
         }
         busy            |= 1u << sl;
         sslot[inflight]  = sl;
         offs[inflight] = sent;
         lens[inflight] = n;
         inflight++;
         sent += n;
      }
      if (!inflight)
         break;
      {
         struct xdr r;
         uint32_t xid, reof = 0;
         size_t dl = 0;
         unsigned slot = RNFS_PIPELINE;
         int pr;
         if (rnfs_rpc_recv(c, c->fd, &xid, &r) != 0)
         {
            rnfs_drop(c);                /* the link went: dial again */
            return -1;
         }
         for (i = 0; i < inflight; i++)
            if (xids[i] == xid)
               slot = i;
         if (slot == RNFS_PIPELINE)
            continue;
         pr = rnfs_parse_read(c, &r, sslot[slot], off + offs[slot], f->size,
               out + offs[slot], lens[slot], &dl, &reof);
         if (pr == 2)
         {
            /* READ_PLUS refused: this chunk again, now as READ */
            if (rnfs_send_read(c, f, off + offs[slot], lens[slot],
                     sslot[slot], &xids[slot]) != 0)
            {
               rnfs_drop(c);
               fail = 1;
            }
            continue;
         }
         if (pr == 0)
         {
            /* v4 GRACE/DELAY: wait and reissue this chunk, bounded */
            if (++grace > 15)
            {
               rnfs_err(c, "server in grace period");
               fail = 1;
            }
            else
            {
               nfs4_wait_ms(1000);
               if (rnfs_send_read(c, f, off + offs[slot], lens[slot],
                        sslot[slot], &xids[slot]) != 0)
               {
                  rnfs_drop(c);
                  fail = 1;
               }
               continue;
            }
         }
         else if (pr < 0 || dl > lens[slot])
         {
            rnfs_err(c, "bad read reply");
            fail = 1;
         }
         else
         {
            /* the data is in place: the parser wrote it there */
            /* a short or empty reply ends the contiguous run there,
             * whatever later chunks answered; only the run counts */
            if (reof || dl < lens[slot])
            {
               eof = 1;
               if (!short_at || offs[slot] + dl < end)
               {
                  end      = offs[slot] + dl;
                  short_at = 1;
               }
            }
         }
         busy       &= ~(1u << sslot[slot]);
         xids[slot]  = xids[inflight - 1];
         offs[slot]  = offs[inflight - 1];
         lens[slot]  = lens[inflight - 1];
         sslot[slot] = sslot[inflight - 1];
         inflight--;
      }
   }
   if (fail)
      return -1;
   return (int64_t)(short_at ? end : sent);
}

/* rnfs_fetch_once(), and again after what broke it is mended - READ
 * changes nothing, so asking twice is safe: a dropped connection is
 * dialled again (a server restart, a timeout), a lost session set up
 * again, a lost open state opened again. At most twice, since a new
 * connection can show the open state went with the old one. */
static int64_t rnfs_fetch(struct rnfs_ctx *c, struct rnfs_file *f,
      uint64_t off, uint8_t *out, size_t len)
{
   unsigned mend;
   int64_t  r = -1;
   for (mend = 0; mend <= 2; mend++)
   {
      c->lost_session = 0;
      c->stale_state  = 0;
      if (c->fd < 0 && rnfs_redial(c) != 0)
         return -1;
      if ((r = rnfs_fetch_once(c, f, off, out, len)) >= 0 || c->redialing)
         break;
      if (c->fd >= 0 && c->lost_session)
      {
         if (nfs4_session_setup(c) != 0)
            break;
      }
      else if (c->fd >= 0 && c->stale_state && f->opened)
      {
         if (nfs4_reopen(c, f) != 0)
            break;
      }
      else if (c->fd >= 0)
         break;                         /* the server said no: final */
   }
   c->lost_session = 0;
   c->stale_state  = 0;
   return r;
}

static int64_t rnfs_write_once(struct rnfs_ctx *c, struct rnfs_file *f,
      const void *buf, size_t len);

/* A write goes to an explicit offset, so sending the same octets there
 * again changes nothing: one that failed because the connection went
 * is written again, from where it began, on a new one. */
int64_t rnfs_write(struct rnfs_ctx *c, struct rnfs_file *f, const void *buf, size_t len)
{
   uint64_t start = f->offset;
   int64_t  r     = rnfs_write_once(c, f, buf, len);
   if (r >= 0 || c->fd >= 0 || c->redialing)
      return r;
   f->offset = start;
   return rnfs_write_once(c, f, buf, len);
}

int64_t rnfs_read(struct rnfs_ctx *c, struct rnfs_file *f, void *buf, size_t len)
{
   uint8_t *out = (uint8_t*)buf;
   int64_t  n;

   if (!len)
      return 0;
   if (!c->readahead || len >= c->readahead)
   {
      f->ra_len = 0;
      n = rnfs_fetch(c, f, f->offset, out, len);
      if (n > 0)
         f->offset += (uint64_t)n;
      return n;
   }
   if (f->ra_len && f->offset >= f->ra_off && f->offset + len <= f->ra_off + f->ra_len)
   {
      memcpy(out, f->ra + (f->offset - f->ra_off), len);
      f->offset += len;
      return (int64_t)len;
   }
   if (!f->ra)
   {
      f->ra_cap = c->readahead;
      if (!(f->ra = (uint8_t*)malloc(f->ra_cap)))
      {
         n = rnfs_fetch(c, f, f->offset, out, len);
         if (n > 0)
            f->offset += (uint64_t)n;
         return n;
      }
   }
   n = rnfs_fetch(c, f, f->offset, f->ra, f->ra_cap);
   if (n < 0)
   {
      f->ra_len = 0;
      return -1;
   }
   f->ra_off = f->offset;
   f->ra_len = (size_t)n;
   if ((size_t)n < len)
      len = (size_t)n;
   memcpy(out, f->ra, len);
   f->offset += len;
   return (int64_t)len;
}

static int64_t rnfs_write_once(struct rnfs_ctx *c, struct rnfs_file *f,
      const void *buf, size_t len)
{
   const uint8_t *in = (const uint8_t*)buf;
   size_t done = 0;
   f->ra_len = 0;                  /* the window is stale past a write */
   if (c->version == 4)
      return nfs4_write(c, f, buf, len);
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
   if (c->version == 4 ? nfs4_set_size(c, f, size) != 0 : rnfs_set_size(c, &f->fh, size) != 0)
      return -1;
   f->ra_len = 0;
   f->size = size;
   if (f->offset > size)
      f->offset = size;
   return 0;
}

int rnfs_close(struct rnfs_ctx *c, struct rnfs_file *f)
{
   int ret = 0;
   if (c && f && c->version == 4 && f->opened)
      ret = nfs4_close(c, f);
   if (f)
   {
      free(f->ra);
      free(f->path);
      nfs4_state_unlink(f);
   }
   free(f);
   return ret;
}

int rnfs_stat(struct rnfs_ctx *c, const char *path, struct rnfs_stat *st)
{
   struct rnfs_fh fh;
   if (c->version == 4)
      return nfs4_walk(c, path, &fh, st, 0, NULL, 0);
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

int rnfs_unlink(struct rnfs_ctx *c, const char *path)
{
   rnfs_dcache_flush(c);
   return c->version == 4 ? nfs4_dirop(c, 0, path) : rnfs_dirop(c, NFS_REMOVE, path, 0);
}
int rnfs_mkdir(struct rnfs_ctx *c, const char *path)
{
   return c->version == 4 ? nfs4_dirop(c, 1, path) : rnfs_dirop(c, NFS_MKDIR, path, 1);
}

int rnfs_rename(struct rnfs_ctx *c, const char *from, const char *to)
{
   struct rnfs_fh fdir, tdir;
   char flast[256], tlast[256];
   uint8_t args[2 * (NFS3_FHSIZE + 4 + 260)];
   struct xdr x, reply;
   rnfs_dcache_flush(c);   /* a directory may move */
   if (c->version == 4)
      return nfs4_rename(c, from, to);
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
   if ((c->version == 4 ? nfs4_walk(c, path, &d->fh, &st, 0, NULL, 0)
                        : rnfs_walk(c, path, &d->fh, &st, 0, NULL, 0)) != 0 || !st.is_dir)
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
         if ((c->version == 4 ? nfs4_readdir_fill(c, d) : rnfs_readdir_fill(c, d)) != 0)
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
            if ((c->version == 4 ? nfs4_readdir_fill(c, d) : rnfs_readdir_fill(c, d)) != 0)
               return NULL;
            continue;
         }
         return NULL;
      }
      memset(&d->ent.st, 0, sizeof(d->ent.st));
      if (c->version == 4)
      {
         /* entry4: cookie, name, fattr4 */
         d->cookie = xdr_get_u64(&x);
         name = xdr_get_opaque(&x, &name_len);
         if (nfs4_get_fattr(&x, &d->ent.st) != 0)
            x.fail = 1;
      }
      else
      {
      xdr_get_u64(&x);                         /* fileid */
      name = xdr_get_opaque(&x, &name_len);
      d->cookie = xdr_get_u64(&x);
      }
      if (c->version != 4 && !d->plain)
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
      if (c->version != 4 && d->plain)
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
   return rnfs_rpc(c, c->fd, NFS_PROG, c->version == 4 ? 4 : NFS_VERS, NFS_NULL, NULL, 0, &reply);
}

void rnfs_set_readahead(struct rnfs_ctx *c, uint32_t bytes)
{
   c->readahead = bytes;
}
