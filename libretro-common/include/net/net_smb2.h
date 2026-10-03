/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_smb2.h).
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

#ifndef _LIBRETRO_NET_SMB2_H
#define _LIBRETRO_NET_SMB2_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* SMB2/3 client on the cleanroom crypto: dialects 2.0.2 through 3.1.1,
 * NTLMv2 through SPNEGO, signing (HMAC-SHA256 on 2.x, AES-CMAC on 3.x)
 * and sealing (AES-CCM on 3.0 / 3.0.2, AES-GCM on 3.1.1) when the
 * server or share asks for it. One connection, one session, one tree
 * per context; calls are synchronous and the socket is blocking.
 * Paths are UTF-8 with '/' or '\\' separators, relative to the share. */

struct rsmb_ctx;
struct rsmb_file;
struct rsmb_dir;

#define RSMB_O_RDONLY 0x0
#define RSMB_O_WRONLY 0x1
#define RSMB_O_RDWR   0x2
#define RSMB_O_CREAT  0x40
#define RSMB_O_TRUNC  0x200

struct rsmb_stat
{
   uint64_t size;
   uint64_t mtime;      /* seconds since 1970 */
   int      is_dir;
};

struct rsmb_dirent
{
   struct rsmb_stat st;
   char name[256];
};

struct rsmb_ctx *rsmb_new(void);
void rsmb_free(struct rsmb_ctx *c);

/* All three are copied; NULL means empty. */
void rsmb_set_credentials(struct rsmb_ctx *c, const char *user,
      const char *password, const char *domain);
void rsmb_set_user(struct rsmb_ctx *c, const char *user);
void rsmb_set_password(struct rsmb_ctx *c, const char *password);
void rsmb_set_domain(struct rsmb_ctx *c, const char *domain);
/* Socket connect and per-request timeout, seconds; 0 restores the
 * default of 10. */
/* TCP port; 0 (the default) means 445. */
void rsmb_set_port(struct rsmb_ctx *c, uint16_t port);
/**
 * rsmb_set_kerberos:
 * @realm    : the Kerberos realm ("EXAMPLE.COM"); NULL or "" turns
 *             Kerberos off again
 * @kdc      : the KDC host; NULL or "" uses the SMB server itself,
 *             which is right for an Active Directory domain controller
 * @port     : KDC port, 0 for 88
 *
 * With a realm set, rsmb_connect() authenticates the session with a
 * Kerberos ticket for cifs/server (AS and TGS exchanges with the user
 * and password from rsmb_set_credentials, then an RFC 4121 token in
 * SPNEGO). Should the KDC be unreachable or refuse, the session falls
 * back to NTLMSSP as before; rsmb_used_kerberos() says which one it
 * ended up with.
 **/
void rsmb_set_kerberos(struct rsmb_ctx *c, const char *realm, const char *kdc, uint16_t port);
int  rsmb_used_kerberos(const struct rsmb_ctx *c);
void rsmb_set_timeout(struct rsmb_ctx *c, unsigned seconds);

/**
 * rsmb_connect:
 * @server            : host name or address; port 445.
 * @share             : share name without slashes.
 *
 * Returns: 0 on success, -1 with rsmb_get_error() set.
 **/
int rsmb_connect(struct rsmb_ctx *c, const char *server, const char *share);
void rsmb_disconnect(struct rsmb_ctx *c);

struct rsmb_file *rsmb_open(struct rsmb_ctx *c, const char *path, int flags);
int64_t rsmb_read(struct rsmb_ctx *c, struct rsmb_file *f, void *buf, size_t len);
/**
 * rsmb_set_readahead:
 * @bytes    : window per open file; 0 turns read-ahead off
 *
 * Reads smaller than the window are served from a window fetched
 * with pipelined READs, so a run of small sequential reads costs one
 * round trip per window instead of one per read. Defaults to 1 MiB
 * (the large-I/O size) once negotiated; the window is allocated on the
 * first small read of each file and freed with it.
 **/
void rsmb_set_readahead(struct rsmb_ctx *c, uint32_t bytes);
int64_t rsmb_write(struct rsmb_ctx *c, struct rsmb_file *f, const void *buf, size_t len);
/* whence: 0 set, 1 cur, 2 end. Returns the new position or -1. */
int64_t rsmb_seek(struct rsmb_ctx *c, struct rsmb_file *f, int64_t off, int whence);
int64_t rsmb_tell(const struct rsmb_file *f);
int rsmb_close(struct rsmb_ctx *c, struct rsmb_file *f);
/* Sets the end of file. */
int rsmb_ftruncate(struct rsmb_ctx *c, struct rsmb_file *f, uint64_t size);
/* Per-request I/O limits negotiated with the server. */
uint32_t rsmb_max_read(const struct rsmb_ctx *c);
uint32_t rsmb_max_write(const struct rsmb_ctx *c);

int rsmb_stat(struct rsmb_ctx *c, const char *path, struct rsmb_stat *st);
int rsmb_unlink(struct rsmb_ctx *c, const char *path);
int rsmb_mkdir(struct rsmb_ctx *c, const char *path);
int rsmb_rename(struct rsmb_ctx *c, const char *from, const char *to);

struct rsmb_dir *rsmb_opendir(struct rsmb_ctx *c, const char *path);
/* NULL at the end; "." and ".." are skipped. */
const struct rsmb_dirent *rsmb_readdir(struct rsmb_ctx *c, struct rsmb_dir *d);
void rsmb_closedir(struct rsmb_ctx *c, struct rsmb_dir *d);

/* One share as SRVSVC reports it. */
struct rsmb_share
{
   char     name[256];
   uint32_t type;       /* STYPE_*: 0 disk, 1 printer, 2 device, 3 IPC;
                           0x80000000 hidden, 0x40000000 temporary */
};

/**
 * rsmb_enum_shares:
 *
 * NetrShareEnum level 1 through the srvsvc pipe; @c must be connected
 * to the server's IPC$ share. Fills up to @max entries.
 *
 * Returns: number of shares reported (may exceed @max), or -1.
 **/
int rsmb_enum_shares(struct rsmb_ctx *c, struct rsmb_share *out, unsigned max);

int rsmb_echo(struct rsmb_ctx *c);
const char *rsmb_get_error(const struct rsmb_ctx *c);
/* Last NT status, for callers that map them. */
uint32_t rsmb_get_status(const struct rsmb_ctx *c);
/* The signing algorithm in use: 0 HMAC-SHA256 (2.x), 1 AES-CMAC,
 * 2 AES-GMAC (3.1.1, when the server chose it). */
unsigned rsmb_get_sign_alg(const struct rsmb_ctx *c);
int rsmb_get_fd(const struct rsmb_ctx *c);

RETRO_END_DECLS

#endif
