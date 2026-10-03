/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_nfs3.h).
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

#ifndef _LIBRETRO_NET_NFS3_H
#define _LIBRETRO_NET_NFS3_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* NFSv3 client (RFC 1813) over ONC RPC on TCP with AUTH_UNIX: the
 * portmapper for the MOUNT and NFS ports unless given, MOUNT v3 for the
 * root handle, then LOOKUP / GETATTR / ACCESS / READ / WRITE / CREATE /
 * SETATTR / REMOVE / MKDIR / RENAME / READDIRPLUS. One server, one
 * export per context; calls are synchronous and bounded by the context
 * timeout. Paths are '/'-separated relative to the export. No Kerberos,
 * no NFSv4. */

struct rnfs_ctx;
struct rnfs_file;
struct rnfs_dir;

#define RNFS_O_RDONLY 0x0
#define RNFS_O_WRONLY 0x1
#define RNFS_O_RDWR   0x2
#define RNFS_O_CREAT  0x40
#define RNFS_O_TRUNC  0x200

struct rnfs_stat
{
   uint64_t size;
   uint64_t mtime;
   int      is_dir;
};

struct rnfs_dirent
{
   struct rnfs_stat st;
   char name[256];
};

struct rnfs_ctx *rnfs_new(void);
void rnfs_free(struct rnfs_ctx *c);

/* Seconds for connect and each call; 0 restores the default of 10. */
void rnfs_set_timeout(struct rnfs_ctx *c, unsigned seconds);
/* AUTH_UNIX identity; defaults to the process's own on POSIX, 1000
 * elsewhere. */
void rnfs_set_identity(struct rnfs_ctx *c, uint32_t uid, uint32_t gid);
/* 3 (the default) or 4: NFSv4.0 needs no portmapper or MOUNT protocol,
 * the export is its pseudo-filesystem path (as the server names it,
 * e.g. "/export"), and the default port is 2049. */
void rnfs_set_version(struct rnfs_ctx *c, unsigned version);

/**
 * rnfs_set_readahead:
 * @bytes    : window per open file; 0 turns read-ahead off
 *
 * Reads smaller than the window are served from a window fetched with
 * pipelined READs, so a run of small sequential reads costs one round
 * trip per window instead of one per read. Defaults to 1 MiB; the
 * window is allocated on the first small read of each file and freed
 * with it.
 **/
void rnfs_set_readahead(struct rnfs_ctx *c, uint32_t bytes);
/* Explicit ports; 0 (the default) asks the portmapper. */
void rnfs_set_ports(struct rnfs_ctx *c, uint16_t nfs_port, uint16_t mount_port);

/**
 * rnfs_connect:
 * @export_path       : As the server exports it, e.g. "/export/roms".
 *
 * Returns: 0 on success, -1 with rnfs_get_error() set.
 **/
int rnfs_connect(struct rnfs_ctx *c, const char *server, const char *export_path);
void rnfs_disconnect(struct rnfs_ctx *c);

struct rnfs_file *rnfs_open(struct rnfs_ctx *c, const char *path, int flags);
int64_t rnfs_read(struct rnfs_ctx *c, struct rnfs_file *f, void *buf, size_t len);
int64_t rnfs_write(struct rnfs_ctx *c, struct rnfs_file *f, const void *buf, size_t len);
int64_t rnfs_seek(struct rnfs_ctx *c, struct rnfs_file *f, int64_t off, int whence);
int64_t rnfs_tell(const struct rnfs_file *f);
int rnfs_ftruncate(struct rnfs_ctx *c, struct rnfs_file *f, uint64_t size);
int rnfs_close(struct rnfs_ctx *c, struct rnfs_file *f);

int rnfs_stat(struct rnfs_ctx *c, const char *path, struct rnfs_stat *st);
int rnfs_unlink(struct rnfs_ctx *c, const char *path);
int rnfs_mkdir(struct rnfs_ctx *c, const char *path);
int rnfs_rename(struct rnfs_ctx *c, const char *from, const char *to);

struct rnfs_dir *rnfs_opendir(struct rnfs_ctx *c, const char *path);
const struct rnfs_dirent *rnfs_readdir(struct rnfs_ctx *c, struct rnfs_dir *d);
void rnfs_closedir(struct rnfs_ctx *c, struct rnfs_dir *d);

/* NULL procedure round trip. */
int rnfs_ping(struct rnfs_ctx *c);
const char *rnfs_get_error(const struct rnfs_ctx *c);

/* Remote calls this connection has made, for tests that count round
 * trips. */
uint32_t rnfs_get_call_count(const struct rnfs_ctx *c);

/* For NFSv4: the minor version in use - the newest of 2, 1 and 0 the
 * server speaks, chosen when connecting. */
unsigned rnfs_get_minor_version(const struct rnfs_ctx *c);

/* Octets of replies received, and whether reads go as NFSv4.2
 * READ_PLUS (holes sent as their extent): for tests. */
uint64_t rnfs_get_rx_bytes(const struct rnfs_ctx *c);
int rnfs_get_read_plus(const struct rnfs_ctx *c);
/* Last nfsstat3, for callers that map them. */
uint32_t rnfs_get_status(const struct rnfs_ctx *c);
int rnfs_get_fd(const struct rnfs_ctx *c);

RETRO_END_DECLS

#endif
