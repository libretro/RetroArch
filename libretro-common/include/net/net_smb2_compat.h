/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_smb2_compat.h).
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

#ifndef _LIBRETRO_NET_SMB2_COMPAT_H
#define _LIBRETRO_NET_SMB2_COMPAT_H

/* The libsmb2 surface RetroArch's SMB consumers were written against
 * (vfs/vfs_implementation_smb.c, network/cloud_sync/smb.c), over the
 * built-in client in net_smb2.c. Included in place of <smb2/libsmb2.h>
 * under HAVE_RETROSMB so those files stay one source for both
 * providers. Only what they call is here. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/* O_ACCMODE is POSIX, not C89; MSVC has none of these. */
#ifndef O_ACCMODE
#define O_ACCMODE 3
#endif
#include <retro_inline.h>
#include <net/net_smb2.h>
#include <compat/strl.h>

struct smb2_context;

/* libsmb2 keeps these on the context; the built-in client takes them
 * at connect time, so they sit beside it here. */
struct smb2_compat_ctx
{
   struct rsmb_ctx *c;
   int      auth_mode;
   char     realm[128];
   char     kdc[256];
   uint16_t kdc_port;
};

/* struct smb2fh is the client's struct rsmb_file under libsmb2's name:
 * no handle of its own, so a context freeing its files frees these,
 * as libsmb2's does. */
struct smb2fh;
#define SMB2_FH(fh) ((struct rsmb_file*)(void*)(fh))
struct smb2_stat_64 { uint32_t smb2_type; uint64_t smb2_size; uint64_t smb2_mtime; };
struct smb2dirent { const char *name; struct smb2_stat_64 st; };
struct smb2dir { struct rsmb_dir *d; struct smb2dirent ent; };

#define SMB2_TYPE_FILE      0
#define SMB2_TYPE_DIRECTORY 1
#define SMB2_TYPE_LINK      2

/* Signing is always on with the built-in client; the flag exists so a
 * caller can keep passing it. */
#define SMB2_NEGOTIATE_SIGNING_ENABLED 0x01

#define SMB2_SEC_UNDEFINED 0
#define SMB2_SEC_NTLMSSP   1
#define SMB2_SEC_KRB5      2

static INLINE struct smb2_context *smb2_init_context(void)
{
   struct smb2_compat_ctx *x = (struct smb2_compat_ctx*)calloc(1, sizeof(*x));
   if (!x)
      return NULL;
   if (!(x->c = rsmb_new()))
   {
      free(x);
      return NULL;
   }
   return (struct smb2_context*)x;
}

#define SMB2_CTX(ctx) (((struct smb2_compat_ctx*)(ctx))->c)

static INLINE void smb2_destroy_context(struct smb2_context *ctx)
{
   if (!ctx)
      return;
   rsmb_free(SMB2_CTX(ctx));
   free(ctx);
}
static INLINE void smb2_set_user(struct smb2_context *ctx, const char *u)     { rsmb_set_user(SMB2_CTX(ctx), u); }
static INLINE void smb2_set_password(struct smb2_context *ctx, const char *p) { rsmb_set_password(SMB2_CTX(ctx), p); }
static INLINE void smb2_set_domain(struct smb2_context *ctx, const char *d)   { rsmb_set_domain(SMB2_CTX(ctx), d); }
static INLINE void smb2_set_timeout(struct smb2_context *ctx, int seconds)    { rsmb_set_timeout(SMB2_CTX(ctx), seconds > 0 ? (unsigned)seconds : 0); }
static INLINE void smb2_set_readahead(struct smb2_context *ctx, uint32_t bytes){ rsmb_set_readahead(SMB2_CTX(ctx), bytes); }
static INLINE void smb2_set_security_mode(struct smb2_context *ctx, int mode) { ((struct smb2_compat_ctx*)ctx)->auth_mode = mode; }
static INLINE void smb2_set_authentication(struct smb2_context *ctx, int mode){ ((struct smb2_compat_ctx*)ctx)->auth_mode = mode; }

/* The realm and KDC a KRB5 request uses (a libsmb2 build reads them
 * from krb5.conf instead; this extension is guarded HAVE_RETROSMB by
 * the caller). */
static INLINE void smb2_set_kerberos(struct smb2_context *ctx, const char *realm,
      const char *kdc, uint16_t port)
{
   struct smb2_compat_ctx *x = (struct smb2_compat_ctx*)ctx;
   strlcpy(x->realm, realm ? realm : "", sizeof(x->realm));
   strlcpy(x->kdc, kdc ? kdc : "", sizeof(x->kdc));
   x->kdc_port = port;
}

/* A KRB5 request needs a realm and succeeds only when the session
 * really came from a ticket, so a pool that tries Kerberos first
 * records the truth and its NTLMSSP attempt still runs; an NTLMSSP
 * request never touches the KDC. */
static INLINE int smb2_connect_share(struct smb2_context *ctx, const char *server,
      const char *share, const char *user)
{
   struct smb2_compat_ctx *x = (struct smb2_compat_ctx*)ctx;
   (void)user;
   if (x->auth_mode == SMB2_SEC_KRB5)
   {
      if (!x->realm[0])
         return -1;
      rsmb_set_kerberos(x->c, x->realm, x->kdc, x->kdc_port);
      if (rsmb_connect(x->c, server, share) != 0)
         return -1;
      if (!rsmb_used_kerberos(x->c))
      {
         rsmb_disconnect(x->c);
         return -1;
      }
      return 0;
   }
   rsmb_set_kerberos(x->c, NULL, NULL, 0);
   return rsmb_connect(x->c, server, share);
}
static INLINE int  smb2_disconnect_share(struct smb2_context *ctx) { rsmb_disconnect(SMB2_CTX(ctx)); return 0; }
static INLINE int  smb2_context_active(struct smb2_context *ctx)   { return rsmb_get_fd(SMB2_CTX(ctx)) >= 0; }
static INLINE int  smb2_echo(struct smb2_context *ctx)             { return rsmb_echo(SMB2_CTX(ctx)); }
static INLINE const char *smb2_get_error(struct smb2_context *ctx) { return rsmb_get_error(SMB2_CTX(ctx)); }

static INLINE struct smb2fh *smb2_open(struct smb2_context *ctx, const char *path, int flags)
{
   int rf = 0;
   if ((flags & O_ACCMODE) == O_WRONLY) rf = RSMB_O_WRONLY;
   else if ((flags & O_ACCMODE) == O_RDWR) rf = RSMB_O_RDWR;
   if (flags & O_CREAT) rf |= RSMB_O_CREAT;
   if (flags & O_TRUNC) rf |= RSMB_O_TRUNC;
   return (struct smb2fh*)(void*)rsmb_open(SMB2_CTX(ctx), path, rf);
}
static INLINE int smb2_read(struct smb2_context *ctx, struct smb2fh *fh, uint8_t *buf, uint32_t len)
{
   int64_t r = rsmb_read(SMB2_CTX(ctx), SMB2_FH(fh), buf, len);
   return r < 0 ? -1 : (int)r;
}
static INLINE int smb2_write(struct smb2_context *ctx, struct smb2fh *fh, const uint8_t *buf, uint32_t len)
{
   int64_t r = rsmb_write(SMB2_CTX(ctx), SMB2_FH(fh), buf, len);
   return r < 0 ? -1 : (int)r;
}
static INLINE int64_t smb2_lseek(struct smb2_context *ctx, struct smb2fh *fh,
      int64_t offset, int whence, uint64_t *current_offset)
{
   /* libsmb2 reports failure as -EINVAL, which the VFS tests for. */
   int64_t r = rsmb_seek(SMB2_CTX(ctx), SMB2_FH(fh),
         offset, whence == SEEK_SET ? 0 : whence == SEEK_CUR ? 1 : 2);
   if (r < 0)
      return -EINVAL;
   if (current_offset)
      *current_offset = (uint64_t)r;
   return r;
}
static INLINE int smb2_close(struct smb2_context *ctx, struct smb2fh *fh)
{
   int r;
   if (!fh)
      return -1;
   r = rsmb_close(SMB2_CTX(ctx), SMB2_FH(fh));
   return r;
}
static INLINE int smb2_ftruncate(struct smb2_context *ctx, struct smb2fh *fh, uint64_t size)
{
   return rsmb_ftruncate(SMB2_CTX(ctx), SMB2_FH(fh), size);
}
static INLINE uint32_t smb2_get_max_read_size(struct smb2_context *ctx)  { return rsmb_max_read(SMB2_CTX(ctx)); }
static INLINE uint32_t smb2_get_max_write_size(struct smb2_context *ctx) { return rsmb_max_write(SMB2_CTX(ctx)); }
static INLINE int smb2_stat(struct smb2_context *ctx, const char *path, struct smb2_stat_64 *st)
{
   struct rsmb_stat rs;
   if (rsmb_stat(SMB2_CTX(ctx), path, &rs) != 0)
      return -1;
   st->smb2_type  = rs.is_dir ? SMB2_TYPE_DIRECTORY : SMB2_TYPE_FILE;
   st->smb2_size  = rs.size;
   st->smb2_mtime = rs.mtime;
   return 0;
}
static INLINE int smb2_unlink(struct smb2_context *ctx, const char *path)  { return rsmb_unlink(SMB2_CTX(ctx), path); }
static INLINE int smb2_mkdir(struct smb2_context *ctx, const char *path)   { return rsmb_mkdir(SMB2_CTX(ctx), path); }
static INLINE int smb2_rename(struct smb2_context *ctx, const char *a, const char *b) { return rsmb_rename(SMB2_CTX(ctx), a, b); }

static INLINE struct smb2dir *smb2_opendir(struct smb2_context *ctx, const char *path)
{
   struct smb2dir *d = (struct smb2dir*)calloc(1, sizeof(*d));
   if (!d)
      return NULL;
   if (!(d->d = rsmb_opendir(SMB2_CTX(ctx), path)))
   {
      free(d);
      return NULL;
   }
   return d;
}
static INLINE struct smb2dirent *smb2_readdir(struct smb2_context *ctx, struct smb2dir *d)
{
   const struct rsmb_dirent *e = rsmb_readdir(SMB2_CTX(ctx), d->d);
   if (!e)
      return NULL;
   d->ent.name          = e->name;
   d->ent.st.smb2_type  = e->st.is_dir ? SMB2_TYPE_DIRECTORY : SMB2_TYPE_FILE;
   d->ent.st.smb2_size  = e->st.size;
   d->ent.st.smb2_mtime = e->st.mtime;
   return &d->ent;
}
static INLINE void smb2_closedir(struct smb2_context *ctx, struct smb2dir *d)
{
   if (!d)
      return;
   rsmb_closedir(SMB2_CTX(ctx), d->d);
   free(d);
}

#endif
