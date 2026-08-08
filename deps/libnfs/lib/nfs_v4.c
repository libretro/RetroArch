/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2017 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
/*
 * High level api to nfsv4 filesystems
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef AROS
#include "aros_compat.h"
#endif

#ifdef PS2_EE
#include "ps2_compat.h"
#endif

#ifdef PS3_PPU
#include "ps3_compat.h"
#endif

#ifdef WIN32
#include <win32/win32_compat.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#define PRIu64 "llu"
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#if defined(__ANDROID__) && !defined(HAVE_SYS_STATVFS_H)
#define statvfs statfs
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_GETPWNAM
#include <pwd.h>
#endif

#ifdef WIN32
#include <win32/win32_compat.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libnfs-zdr.h"
#include "slist.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-raw-nfs4.h"
#include "libnfs-private.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

struct nfs4_cb_data;
typedef int (*op_filler)(struct nfs4_cb_data *data, nfs_argop4 *op);

struct lookup_link_data {
        unsigned int idx;
};

typedef void (*blob_free)(void *);

struct nfs4_blob {
        int       len;
        void     *val;
        blob_free free;
};

/* Function and arguments to append the requested operations we want
 * for the resolved path.
 */
struct lookup_filler {
        op_filler func;
        int max_op;
        int flags;
        void *data;  /* Freed by nfs4_cb_data destructor */

        struct nfs4_blob blob0;
        struct nfs4_blob blob1;
        struct nfs4_blob blob2;
        struct nfs4_blob blob3;
};

struct rw_data {
        uint64_t offset;
        int update_pos;
};

struct nfs4_cb_data {
        struct nfs_context *nfs;
/* Do not follow symlinks for the final component on a lookup.
 * I.e. stat vs lstat
 */
#define LOOKUP_FLAG_NO_FOLLOW    0x0001
#define LOOKUP_FLAG_IS_STATVFS64 0x0002
#define MUTEX_HELD               0x0004
        int flags;

        /* Internal callback for open-with-continuation use */
        rpc_cb open_cb;

        /* Application callback and data */
        nfs_cb cb;
        void *private_data;

        /* Used to track open_owner during open() */
        uint32_t open_owner;
        
        /* internal callback */
        rpc_cb continue_cb;

        char *path; /* path to lookup */
        struct lookup_filler filler;

        /* Data we need when resolving a symlink in the path */
        struct lookup_link_data link;

        /* Data we need for updating offset in read/write */
        struct rw_data rw_data;
};

static uint32_t standard_attributes[2] = {
        (1 << FATTR4_TYPE |
         1 << FATTR4_SIZE |
         1 << FATTR4_FSID |
         1 << FATTR4_FILEID),
        (1 << (FATTR4_MODE - 32) |
         1 << (FATTR4_NUMLINKS - 32) |
         1 << (FATTR4_OWNER - 32) |
         1 << (FATTR4_OWNER_GROUP - 32) |
         1 << (FATTR4_RAWDEV - 32) |
         1 << (FATTR4_SPACE_USED - 32) |
         1 << (FATTR4_TIME_ACCESS - 32) |
         1 << (FATTR4_TIME_METADATA - 32) |
         1 << (FATTR4_TIME_MODIFY - 32))
};
static uint32_t statvfs_attributes[2] = {
        (1 << FATTR4_FSID |
         1 << FATTR4_FILES_AVAIL |
         1 << FATTR4_FILES_FREE |
         1 << FATTR4_FILES_TOTAL |
         1 << FATTR4_MAXNAME),
        (1 << (FATTR4_SPACE_AVAIL - 32) |
         1 << (FATTR4_SPACE_FREE - 32) |
         1 << (FATTR4_SPACE_TOTAL - 32))
};

static uint32_t getacl_attributes[1] = {
        (1 << FATTR4_ACL )
};

static uint32_t rwmax_attributes[1] = {
        (1 << FATTR4_MAXREAD |
         1 << FATTR4_MAXWRITE )
};

static int
nfs4_open_async_internal(struct nfs_context *nfs, struct nfs4_cb_data *data,
                         int flags, int mode);

/* Caller will free the returned path. */
static char *
nfs4_resolve_path(struct nfs_context *nfs, const char *path)
{
        char *new_path = NULL;

        /* Absolute paths we just use as is.
         * Relateive paths have cwd prepended to them and then become
         * absolute paths too.
         */
        if (path[0] == '/') {
                new_path = strdup(path);
        } else {
                new_path = malloc(strlen(path) + strlen(nfs->nfsi->cwd) + 2);
                if (new_path != NULL) {
                        sprintf(new_path, "%s/%s", nfs->nfsi->cwd, path);
                }
        }
        if (new_path == NULL) {
                nfs_set_error(nfs, "Out of memory: failed to "
                              "allocate path string");
                return NULL;
        }

        if (nfs_normalize_path(nfs, new_path)) {
                nfs_set_error(nfs, "Failed to normalize real path. %s",
                              nfs_get_error(nfs));
                free(new_path);
                return NULL;
        }

        return new_path;
}

static void
free_nfs4_cb_data(struct nfs4_cb_data *data)
{
#ifdef HAVE_MULTITHREADING
        if (data->flags & MUTEX_HELD) {
                nfs_mt_mutex_unlock(&data->nfs->nfsi->nfs4_open_call_mutex);
        }
#endif        
        free(data->path);
        free(data->filler.data);
        if (data->filler.blob0.val && data->filler.blob0.free) {
                data->filler.blob0.free(data->filler.blob0.val);
        }
        if (data->filler.blob1.val && data->filler.blob1.free) {
                data->filler.blob1.free(data->filler.blob1.val);
        }
        if (data->filler.blob2.val && data->filler.blob2.free) {
                data->filler.blob2.free(data->filler.blob2.val);
        }
        if (data->filler.blob3.val && data->filler.blob3.free) {
                data->filler.blob3.free(data->filler.blob3.val);
        }
        free(data);
}

static struct nfs4_cb_data *
init_cb_data_full_path(struct nfs_context *nfs, const char *path)
{
        struct nfs4_cb_data *data;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return NULL;
        }

        data->nfs          = nfs;
        data->path = nfs4_resolve_path(nfs, path);
        if (data->path == NULL) {
                free_nfs4_cb_data(data);
                return NULL;
        }

        return data;
}

static void
data_split_path(struct nfs4_cb_data *data)
{
        char *path;
        path = strrchr(data->path, '/');
        if (path == data->path) {
                char *ptr;

                for (ptr = data->path; *ptr; ptr++) {
                        *ptr = *(ptr + 1);
                }
                /* No path to lookup */
                data->filler.data = data->path;
                data->path = strdup("/");
        } else {
                *path++ = 0;
                data->filler.data = strdup(path);
        }
}

static struct nfs4_cb_data *
init_cb_data_split_path(struct nfs_context *nfs, const char *orig_path)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_full_path(nfs, orig_path);
        if (data == NULL) {
                return NULL;
        }

        data_split_path(data);
        return data;
}

static int
check_nfs4_error(struct nfs_context *nfs, int status,
                 struct nfs4_cb_data *data, void *command_data,
                 char *op_name)
{
        COMPOUND4res *res = command_data;

        if (status == RPC_STATUS_ERROR) {
                data->cb(-EFAULT, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return 1;
        }
        if (status == RPC_STATUS_CANCEL) {
                data->cb(-EINTR, nfs, "Command was cancelled",
                         data->private_data);
                free_nfs4_cb_data(data);
                return 1;
        }
        if (status == RPC_STATUS_TIMEOUT) {
                data->cb(-EINTR, nfs, "Command timed out",
                         data->private_data);
                free_nfs4_cb_data(data);
                return 1;
        }
        if (res && res->status != NFS4_OK) {
                nfs_set_error(nfs, "NFS4: %s (path %s) failed with "
                              "%s(%d)", op_name,
                              data->path,
                              nfsstat4_to_str(res->status),
                              nfsstat4_to_errno(res->status));
                data->cb(nfsstat4_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return 1;
        }

        return 0;
}

static int
nfs4_find_op(struct nfs_context *nfs, struct nfs4_cb_data *data,
             COMPOUND4res *res, int op, const char *op_name)
{
        int i;

        for (i = 0; i < (int)res->resarray.resarray_len; i++) {
                if (res->resarray.resarray_val[i].resop == op) {
                        break;
                }
        }
        if (i == res->resarray.resarray_len) {
                nfs_set_error(nfs, "No %s result.", op_name);
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return -1;
        }

        return i;
}

static uint64_t
nfs_hton64(uint64_t val)
{
        int i;
        uint64_t res;
        unsigned char *ptr = (void *)&res;

        for (i = 0; i < 8; i++) {
                ptr[7 - i] = val & 0xff;
                val >>= 8;
        }
        return res;
}

static uint64_t
nfs_ntoh64(uint64_t val)
{
        int i;
        uint64_t res;
        unsigned char *ptr = (void *)&res;

        for (i = 0; i < 8; i++) {
                ptr[7 - i] = val & 0xff;
                val >>= 8;
        }
        return res;
}

static uint64_t
nfs_pntoh64(const uint32_t *buf)
{
        uint64_t val;

        val   = ntohl(*(uint32_t *)(void *)buf);
        buf++;
        val <<= 32;
        val  |= ntohl(*(uint32_t *)(void *)buf);

        return val;
}

static int
nfs_get_ugid(struct nfs_context *nfs, const char *buf, int slen, int is_user)
{
        int ugid = 0;
        const char *name = buf;

        while (slen) {
                if (isdigit(*buf)) {
                        ugid *= 10;
                        ugid += *buf - '0';
                } else {
#ifdef HAVE_GETPWNAM
                        struct passwd *pwd = getpwnam(name);
                        if (pwd) {
                                if (is_user) {
                                        return pwd->pw_uid;
                                } else {
                                        return pwd->pw_gid;
                                }
                        }
#else
			(void) name; // Let the compiler know that this variable is intentionally unused, build would fail with -Werror=unused-variable otherwise
#endif
                        return 65534;
                }
                buf++;
                slen--;
        }
        return ugid;
}

#define CHECK_GETATTR_BUF_SPACE(len, size)                              \
    if (len < size) {                                                   \
        nfs_set_error(nfs, "Not enough data in fattr4");                \
        return -1;                                                      \
    }

static uint64_t
specdata4_to_rdev(uint32_t *specdata)
{
        uint64_t rdev = ntohl(specdata[0]);
        return (rdev << 32) | ntohl(specdata[1]);
}

static int
nfs_parse_attributes(struct nfs_context *nfs, struct nfs4_cb_data *data,
                     struct nfs_stat_64 *st, const char *buf, int len)
{
        int type, slen, pad;

        /* Type */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        type = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;
        /* Size */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        st->nfs_size = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        /* FSID */
        CHECK_GETATTR_BUF_SPACE(len, 16);
        st->nfs_dev = ((uint64_t *)buf)[0] ^ ((uint64_t *)buf)[1];
        buf += 16;
        len -= 16;
        /* Inode */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        st->nfs_ino = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        /* Mode */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        st->nfs_mode = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;
        switch (type) {
        case NF4REG:
                st->nfs_mode |= S_IFREG;
                break;
        case NF4DIR:
                st->nfs_mode |= S_IFDIR;
                break;
        case NF4BLK:
                st->nfs_mode |= S_IFBLK;
                break;
        case NF4CHR:
                st->nfs_mode |= S_IFCHR;
                break;
        case NF4LNK:
                st->nfs_mode |= S_IFLNK;
                break;
        case NF4SOCK:
                st->nfs_mode |= S_IFSOCK;
                break;
        case NF4FIFO:
                st->nfs_mode |= S_IFIFO;
                break;
        default:
                break;
        }
        /* Num Links */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        st->nfs_nlink = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;
        /* Owner */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        slen = ntohl(*(uint32_t *)(void *)buf);
        if (slen < 0) {
                return -1;
        }
        buf += 4;
        len -= 4;
        pad = (4 - (slen & 0x03)) & 0x03;
        CHECK_GETATTR_BUF_SPACE(len, slen);
        st->nfs_uid = nfs_get_ugid(nfs, buf, slen, 1);
        buf += slen;
        CHECK_GETATTR_BUF_SPACE(len, pad);
        buf += pad;
        len -= pad;
        /* Group */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        slen = ntohl(*(uint32_t *)(void *)buf);
        if (slen < 0) {
                return -1;
        }
        buf += 4;
        len -= 4;
        pad = (4 - (slen & 0x03)) & 0x03;
        CHECK_GETATTR_BUF_SPACE(len, slen);
        st->nfs_gid = nfs_get_ugid(nfs, buf, slen, 0);
        buf += slen;
        CHECK_GETATTR_BUF_SPACE(len, pad);
        buf += pad;
        len -= pad;
        /* raw device */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        st->nfs_rdev = specdata4_to_rdev((uint32_t *)buf);
        buf += 8;
        len -= 8;
        /* Space Used */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        st->nfs_used = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        /* ATime */
        CHECK_GETATTR_BUF_SPACE(len, 12);
        st->nfs_atime = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        st->nfs_atime_nsec = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;
        /* CTime */
        CHECK_GETATTR_BUF_SPACE(len, 12);
        st->nfs_ctime = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        st->nfs_ctime_nsec = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;
        /* MTime */
        CHECK_GETATTR_BUF_SPACE(len, 12);
        st->nfs_mtime = nfs_pntoh64((uint32_t *)(void *)buf);
        buf += 8;
        len -= 8;
        st->nfs_mtime_nsec = ntohl(*(uint32_t *)(void *)buf);
        buf += 4;
        len -= 4;

        st->nfs_blksize = NFS_BLKSIZE;
        st->nfs_blocks  = (st->nfs_used + NFS_BLKSIZE -1) / NFS_BLKSIZE;

        return 0;
}

static int
nfs4_num_path_components(struct nfs_context *nfs, const char *path)
{
        int i;

        for (i = 0; (path = strchr(path, '/')); path++, i++)
                ;

        return i;
}

static int
nfs4_op_create(struct nfs_context *nfs, nfs_argop4 *op, const char *name,
               nfs_ftype4 type, struct nfs4_blob *attrmask,
               struct nfs4_blob *attr_vals, const char *linkdata, int dev)
{
        CREATE4args *cargs;

        op[0].argop = OP_CREATE;
        cargs = &op[0].nfs_argop4_u.opcreate;
        memset(cargs, 0, sizeof(*cargs));
        cargs->objtype.type = type;
        cargs->objname.utf8string_len = strlen(name);
        cargs->objname.utf8string_val = discard_const(name);
        if (attrmask) {
                cargs->createattrs.attrmask.bitmap4_len = attrmask->len;
                cargs->createattrs.attrmask.bitmap4_val = attrmask->val;
        }
        if (attr_vals) {
                cargs->createattrs.attr_vals.attrlist4_len = attr_vals->len;
                cargs->createattrs.attr_vals.attrlist4_val = attr_vals->val;
        }
        if (linkdata) {
                cargs->objtype.createtype4_u.linkdata.utf8string_len =
                        strlen(linkdata);
                cargs->objtype.createtype4_u.linkdata.utf8string_val =
                        discard_const(linkdata);
        }
        switch (type) {
        case NF4CHR:
                cargs->objtype.type = NF4CHR;
                cargs->objtype.createtype4_u.devdata.specdata1 = major(dev);
                cargs->objtype.createtype4_u.devdata.specdata2 = minor(dev);
                break;
        case NF4BLK:
                cargs->objtype.type = NF4BLK;
                cargs->objtype.createtype4_u.devdata.specdata1 = major(dev);
                cargs->objtype.createtype4_u.devdata.specdata2 = minor(dev);
                break;
        default:
                ;
        }
        return 1;
}

static int
nfs4_op_commit(struct nfs_context *nfs, nfs_argop4 *op)
{
        COMMIT4args *coargs;

        op[0].argop = OP_COMMIT;
        coargs = &op[0].nfs_argop4_u.opcommit;
        coargs->offset = 0;
        coargs->count = 0;

        return 1;
}

static int
nfs4_op_close(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh)
{
        CLOSE4args *clargs;
        int i = 0;

        if (fh->is_dirty) {
                i += nfs4_op_commit(nfs, &op[i]);
        }

        op[i].argop = OP_CLOSE;
        clargs = &op[i++].nfs_argop4_u.opclose;
        clargs->seqid = fh->open_seqid;
        clargs->open_stateid.seqid = fh->stateid.seqid;
        memcpy(clargs->open_stateid.other, fh->stateid.other, 12);

        return i;
}

static int
nfs4_op_access(struct nfs_context *nfs, nfs_argop4 *op, uint32_t access_mask)
{
        ACCESS4args *aargs;

        op[0].argop = OP_ACCESS;
        aargs = &op[0].nfs_argop4_u.opaccess;
        memset(aargs, 0, sizeof(*aargs));
        aargs->access = access_mask;

        return 1;
}

static int
nfs4_op_setclientid(struct nfs_context *nfs, nfs_argop4 *op, verifier4 verifier,
                    const char *client_name)
{
        SETCLIENTID4args *scidargs;

        op[0].argop = OP_SETCLIENTID;
        scidargs = &op[0].nfs_argop4_u.opsetclientid;
        memcpy(scidargs->client.verifier, verifier, sizeof(verifier4));
        scidargs->client.id.id_len = strlen(client_name);
        scidargs->client.id.id_val = discard_const(client_name);
        /* TODO: Decide what we should do here. As long as we only
         * expose a single FD to the application we will not be able to
         * do NFSv4 callbacks easily.
         * Just give it garbage for now until we figure out how we should
         * solve this. Until then we will just have to avoid doing things
         * that require a callback.
         * ( Clients (i.e. Linux) ignore this anyway and just call back to
         *   the originating address and program anyway. )
         */
        scidargs->callback.cb_program = 0; /* NFS4_CALLBACK */
        scidargs->callback.cb_location.r_netid = "tcp";
        scidargs->callback.cb_location.r_addr = "0.0.0.0.0.0";
        scidargs->callback_ident = 0x00000001;

        return 1;
}

static int
nfs4_op_open_confirm(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh)
{
        OPEN_CONFIRM4args *ocargs;

        op[0].argop = OP_OPEN_CONFIRM;
        ocargs = &op[0].nfs_argop4_u.opopen_confirm;
        ocargs->open_stateid.seqid = fh->stateid.seqid;
        memcpy(&ocargs->open_stateid.other, fh->stateid.other, 12);
        ocargs->seqid = fh->open_seqid;

        return 1;
}

static int
nfs4_op_truncate(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
                 void *sabuf)
{
        SETATTR4args *saargs;
        static uint32_t mask[2] = {1 << (FATTR4_SIZE),
                                   1 << (FATTR4_TIME_MODIFY_SET - 32)};

        op[0].argop = OP_SETATTR;
        saargs = &op[0].nfs_argop4_u.opsetattr;
        saargs->stateid.seqid = fh->stateid.seqid;
        memcpy(saargs->stateid.other, fh->stateid.other, 12);

        saargs->obj_attributes.attrmask.bitmap4_len = 2;
        saargs->obj_attributes.attrmask.bitmap4_val = mask;

        saargs->obj_attributes.attr_vals.attrlist4_len = 12;
        saargs->obj_attributes.attr_vals.attrlist4_val = sabuf;

        return 1;
}

static int
nfs4_op_chmod(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
              void *sabuf)
{
        SETATTR4args *saargs;
        static uint32_t mask[2] = {0,
                                   1 << (FATTR4_MODE - 32)};

        op[0].argop = OP_SETATTR;
        saargs = &op[0].nfs_argop4_u.opsetattr;
        if (fh) {
                saargs->stateid.seqid = fh->stateid.seqid;
                memcpy(saargs->stateid.other, fh->stateid.other, 12);
        }

        saargs->obj_attributes.attrmask.bitmap4_len = 2;
        saargs->obj_attributes.attrmask.bitmap4_val = mask;

        saargs->obj_attributes.attr_vals.attrlist4_len = 4;
        saargs->obj_attributes.attr_vals.attrlist4_val = sabuf;

        return 1;
}

static int
nfs4_op_chown(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
              void *sabuf, int len)
{
        SETATTR4args *saargs;
        static uint32_t mask[2] = {0,
                                   1 << (FATTR4_OWNER - 32) |
                                   1 << (FATTR4_OWNER_GROUP - 32)};

        op[0].argop = OP_SETATTR;
        saargs = &op[0].nfs_argop4_u.opsetattr;
        if (fh) {
                saargs->stateid.seqid = fh->stateid.seqid;
                memcpy(saargs->stateid.other, fh->stateid.other, 12);
        }

        saargs->obj_attributes.attrmask.bitmap4_len = 2;
        saargs->obj_attributes.attrmask.bitmap4_val = mask;

        saargs->obj_attributes.attr_vals.attrlist4_len = len;
        saargs->obj_attributes.attr_vals.attrlist4_val = sabuf;

        return 1;
}

static int
nfs4_op_utimes(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
               void *sabuf, int len)
{
        SETATTR4args *saargs;
        static uint32_t mask[2] = {0,
                                   1 << (FATTR4_TIME_ACCESS_SET - 32) |
                                   1 << (FATTR4_TIME_MODIFY_SET - 32)};

        op[0].argop = OP_SETATTR;
        saargs = &op[0].nfs_argop4_u.opsetattr;
        if (fh) {
                saargs->stateid.seqid = fh->stateid.seqid;
                memcpy(saargs->stateid.other, fh->stateid.other, 12);
        }

        saargs->obj_attributes.attrmask.bitmap4_len = 2;
        saargs->obj_attributes.attrmask.bitmap4_val = mask;

        saargs->obj_attributes.attr_vals.attrlist4_len = len;
        saargs->obj_attributes.attr_vals.attrlist4_val = sabuf;

        return 1;
}

static int
nfs4_op_readdir(struct nfs_context *nfs, nfs_argop4 *op, uint64_t cookie)
{
        READDIR4args *rdargs;

        op[0].argop = OP_READDIR;
        rdargs = &op[0].nfs_argop4_u.opreaddir;
        memset(rdargs, 0, sizeof(*rdargs));

        rdargs->cookie = cookie;
        rdargs->dircount = nfs->nfsi->readdir_dircount;
        rdargs->maxcount = nfs->nfsi->readdir_maxcount;
        rdargs->attr_request.bitmap4_len = 2;
        rdargs->attr_request.bitmap4_val = standard_attributes;

        return 1;
}

static int
nfs4_op_rename(struct nfs_context *nfs, nfs_argop4 *op, const char *oldname,
               const char *newname)
{
        RENAME4args *rargs;

        op[0].argop = OP_RENAME;
        rargs = &op[0].nfs_argop4_u.oprename;
        memset(rargs, 0, sizeof(*rargs));
        rargs->oldname.utf8string_len = strlen(oldname);
        rargs->oldname.utf8string_val = discard_const(oldname);
        rargs->newname.utf8string_len = strlen(newname);
        rargs->newname.utf8string_val = discard_const(newname);

        return 1;
}

static int
nfs4_op_read(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
             uint64_t offset, size_t count)
{
        READ4args *rargs;

        op[0].argop = OP_READ;
        rargs = &op[0].nfs_argop4_u.opread;
        rargs->stateid.seqid = fh->stateid.seqid;
        memcpy(rargs->stateid.other, fh->stateid.other, 12);
        rargs->offset = offset;
        rargs->count = count;

        return 1;
}

static int
nfs4_op_write(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
              uint64_t offset, size_t count, const char *buf)
{
        WRITE4args *wargs;

        op[0].argop = OP_WRITE;
        wargs = &op[0].nfs_argop4_u.opwrite;
        wargs->stateid.seqid = fh->stateid.seqid;
        memcpy(wargs->stateid.other, fh->stateid.other, 12);
        wargs->offset = offset;
        if (fh->is_sync) {
                wargs->stable = DATA_SYNC4;
        } else {
                wargs->stable = UNSTABLE4;
                fh->is_dirty = 1;
        }
        wargs->data.data_len = count;
        wargs->data.data_val = discard_const(buf);

        return 1;
}

static int
nfs4_op_getfh(struct nfs_context *nfs, nfs_argop4 *op)
{
        op[0].argop = OP_GETFH;

        return 1;
}

static int
nfs4_op_savefh(struct nfs_context *nfs, nfs_argop4 *op)
{
        op[0].argop = OP_SAVEFH;

        return 1;
}

static int
nfs4_op_link(struct nfs_context *nfs, nfs_argop4 *op, const char *newname)
{
        LINK4args *largs;

        op[0].argop = OP_LINK;
        largs = &op[0].nfs_argop4_u.oplink;
        memset(largs, 0, sizeof(*largs));
        largs->newname.utf8string_len = strlen(newname);
        largs->newname.utf8string_val = discard_const(newname);

        return 1;
}

static int
nfs4_op_putfh(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *nfsfh)
{
        PUTFH4args *pfargs;
        op[0].argop = OP_PUTFH;

        pfargs = &op[0].nfs_argop4_u.opputfh;
        pfargs->object.nfs_fh4_len = nfsfh->fh.len;
        pfargs->object.nfs_fh4_val = nfsfh->fh.val;

        return 1;
}

static int
nfs4_op_lock(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
             nfs_opnum4 cmd, nfs_lock_type4 locktype,
             int reclaim, uint64_t offset, length4 length)
{
        LOCK4args *largs;
        op[0].argop = cmd;

        largs = &op[0].nfs_argop4_u.oplock;
        largs->locktype = locktype;
        largs->reclaim  = reclaim;
        largs->offset   = offset;
        largs->length   = length;

        if (nfs->nfsi->has_lock_owner) {
                largs->locker.new_lock_owner = 0;
                largs->locker.locker4_u.lock_owner.lock_stateid.seqid =
                        fh->lock_stateid.seqid;
                memcpy(largs->locker.locker4_u.lock_owner.lock_stateid.other,
                        fh->lock_stateid.other, 12);
                largs->locker.locker4_u.lock_owner.lock_seqid =
                        fh->lock_seqid;
        } else {
                largs->locker.new_lock_owner = 1;
                largs->locker.locker4_u.open_owner.open_seqid =
                        fh->open_seqid;
                largs->locker.locker4_u.open_owner.open_stateid.seqid =
                        fh->stateid.seqid;
                memcpy(largs->locker.locker4_u.open_owner.open_stateid.other,
                       fh->stateid.other, 12);
                largs->locker.locker4_u.open_owner.lock_owner.clientid =
                        nfs->nfsi->clientid;
                largs->locker.locker4_u.open_owner.lock_owner.owner.owner_len =
                        strlen(nfs->nfsi->client_name);
                largs->locker.locker4_u.open_owner.lock_owner.owner.owner_val =
                        nfs->nfsi->client_name;
                largs->locker.locker4_u.open_owner.lock_seqid =
                        fh->lock_seqid;
        }
        fh->lock_seqid++;

        return 1;
}

static int
nfs4_op_locku(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
              nfs_lock_type4 locktype, uint64_t offset, length4 length)
{
        LOCKU4args *luargs;
        op[0].argop = OP_LOCKU;

        luargs = &op[0].nfs_argop4_u.oplocku;
        luargs->locktype = locktype;
        luargs->offset   = offset;
        luargs->length   = length;

        luargs->seqid = fh->lock_seqid;
        luargs->lock_stateid.seqid = fh->lock_stateid.seqid;
        memcpy(luargs->lock_stateid.other, fh->lock_stateid.other, 12);

        fh->lock_seqid++;

        return 1;
}

static int
nfs4_op_lockt(struct nfs_context *nfs, nfs_argop4 *op, struct nfsfh *fh,
              nfs_lock_type4 locktype, uint64_t offset, length4 length)
{
        LOCKT4args *ltargs;
        op[0].argop = OP_LOCKT;

        ltargs = &op[0].nfs_argop4_u.oplockt;
        ltargs->locktype = locktype;
        ltargs->offset   = offset;
        ltargs->length   = length;

        ltargs->owner.clientid = nfs->nfsi->clientid;
        ltargs->owner.owner.owner_len = strlen(nfs->nfsi->client_name);
        ltargs->owner.owner.owner_val = nfs->nfsi->client_name;

        return 1;
}

static int
nfs4_op_lookup(struct nfs_context *nfs, nfs_argop4 *op, const char *path)
{
        LOOKUP4args *largs;

        op[0].argop = OP_LOOKUP;
        largs = &op[0].nfs_argop4_u.oplookup;
        largs->objname.utf8string_len = strlen(path);
        largs->objname.utf8string_val = discard_const(path);

        return 1;
}

static int
nfs4_op_setclientid_confirm(struct nfs_context *nfs, struct nfs_argop4 *op,
                            uint64_t clientid, verifier4 verifier)
{
        SETCLIENTID_CONFIRM4args *scidcargs;

        op[0].argop = OP_SETCLIENTID_CONFIRM;
        scidcargs = &op[0].nfs_argop4_u.opsetclientid_confirm;
        scidcargs->clientid = clientid;
        memcpy(scidcargs->setclientid_confirm, verifier, NFS4_VERIFIER_SIZE);

        return 1;
}

static int
nfs4_op_putrootfh(struct nfs_context *nfs, nfs_argop4 *op)
{
        op[0].argop = OP_PUTROOTFH;

        return 1;
}

static int
nfs4_op_readlink(struct nfs_context *nfs, nfs_argop4 *op)
{
        op[0].argop = OP_READLINK;

        return 1;
}

static int
nfs4_op_remove(struct nfs_context *nfs, nfs_argop4 *op, const char *name)
{
        REMOVE4args *rmargs;

        op[0].argop = OP_REMOVE;
        rmargs = &op[0].nfs_argop4_u.opremove;
        memset(rmargs, 0, sizeof(*rmargs));
        rmargs->target.utf8string_len = strlen(name);
        rmargs->target.utf8string_val = discard_const(name);

        return 1;
}

static int
nfs4_op_getattr(struct nfs_context *nfs, nfs_argop4 *op,
                uint32_t *attributes, int count)
{
        GETATTR4args *gaargs;

        op[0].argop = OP_GETATTR;
        gaargs = &op[0].nfs_argop4_u.opgetattr;
        memset(gaargs, 0, sizeof(*gaargs));

        gaargs->attr_request.bitmap4_val = attributes;
        gaargs->attr_request.bitmap4_len = count;

        return 1;
}

/*
 * Allocate op and populate the path components.
 * Will mutate path.
 *
 * Returns:
 *     -1 : On error.
 *  <idx> : On success. Idx represents the next free index in op.
 *          Caller must free op.
 */
static int
nfs4_allocate_op(struct nfs_context *nfs, nfs_argop4 **op,
                 char *path, int num_extra)
{
        char *ptr;
        int i, count;

        *op = NULL;

        count = nfs4_num_path_components(nfs, path);

        *op = malloc(sizeof(**op) * (2 + count + num_extra));
        if (*op == NULL) {
                nfs_set_error(nfs, "Failed to allocate op array");
                return -1;
        }

        i = 0;
        if (nfs->nfsi->rootfh.len) {
                struct nfsfh fh;

                fh.fh.len = nfs->nfsi->rootfh.len;
                fh.fh.val = nfs->nfsi->rootfh.val;
                i += nfs4_op_putfh(nfs, &(*op)[i], &fh);
        } else {
                i += nfs4_op_putrootfh(nfs, &(*op)[i]);
        }

        ptr = &path[1];
        while (ptr && *ptr != 0) {
                char *tmp;

                tmp = strchr(ptr, '/');
                if (tmp) {
                        *tmp = 0;
                        tmp = tmp + 1;
                }
                i += nfs4_op_lookup(nfs, &(*op)[i], ptr); 

                ptr = tmp;
        }                

        i += nfs4_op_getattr(nfs, &(*op)[i], standard_attributes, 2);

        return i;
}

static int
nfs4_lookup_path_async(struct nfs_context *nfs,
                       struct nfs4_cb_data *data,
                       rpc_cb cb);

static void
nfs4_lookup_path_2_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        READLINK4res *rlres = NULL;
        char *path, *tmp, *end;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "READLINK")) {
                return;
        }

        path = strdup(data->path);
        if (path == NULL) {
                nfs_set_error(nfs, "Out of memory duplicating path.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        tmp = &path[0];
        while (data->link.idx-- > 1) {
                tmp = strchr(tmp + 1, '/');
        }
        *tmp++ = 0;
        end = strchr(tmp, '/');
        if (end == NULL) {
                /* Symlink was the last component. */
                end = "";
        } else {
                *end++ = 0;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_READLINK, "READLINK")) < 0) {
                free(path);
                return;
        }
        rlres = &res->resarray.resarray_val[i].nfs_resop4_u.opreadlink;
        
        tmp = malloc(strlen(data->path) + 3 + rlres->READLINK4res_u.resok4.link.utf8string_len);
        if (tmp == NULL) {
                nfs_set_error(nfs, "Out of memory duplicating path.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                free_nfs4_cb_data(data);
                free(path);
                return;
        }

        sprintf(tmp, "%s/%.*s/%s",
                path, (int)rlres->READLINK4res_u.resok4.link.utf8string_len,
                rlres->READLINK4res_u.resok4.link.utf8string_val, end);
        free(path);
        free(data->path);
        data->path = tmp;

        if (nfs4_lookup_path_async(nfs, data, data->continue_cb) < 0) {
                data->cb(-ENOMEM, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static int
nfs4_open_readlink(struct rpc_context *rpc, COMPOUND4res *res,
                   struct nfs4_cb_data *data);

static void
nfs4_lookup_path_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4args args;
        nfs_argop4 *op;
        COMPOUND4res *res = command_data;
        int i;
        int resolve_link = 0;
        char *path, *tmp;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (status == RPC_STATUS_ERROR) {
                data->cb(-EFAULT, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        if (status == RPC_STATUS_CANCEL) {
                data->cb(-EINTR, nfs, "Command was cancelled",
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        if (status == RPC_STATUS_TIMEOUT) {
                data->cb(-EINTR, nfs, "Command timed out",
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        if (res->status != NFS4_OK &&
            res->status != NFS4ERR_SYMLINK) {
                nfs_set_error(nfs, "NFS4: (path %s) failed with "
                              "%s(%d)",
                              data->path,
                              nfsstat4_to_str(res->status),
                              nfsstat4_to_errno(res->status));
                data->cb(nfsstat4_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        for (i = 0; i < (int)res->resarray.resarray_len; i++) {
                if (res->resarray.resarray_val[i].resop == OP_GETATTR) {
                        GETATTR4resok *garesok;
                        struct nfs_stat_64 st;

                        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;

                        memset(&st, 0, sizeof(st));
                        if (nfs_parse_attributes(nfs, data, &st,
                                 garesok->obj_attributes.attr_vals.attrlist4_val,
                                 garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                                free_nfs4_cb_data(data);
                                return;
                        }
                        if ((st.nfs_mode & S_IFMT) == S_IFLNK) {
                                /* The final component of the path was a
                                 * symlink so we may need to resolve it.
                                 */
                                resolve_link = 1;
                        }
                }
        }

        /* Open/create is special since the final component for the file
         * object is sent as part of the OP_OPEN command. So even if the
         * directory path is all good and resolved, we still need to check
         * the attributes for the final component and resolve it if it too
         * is a symlink.
         */
        if (!resolve_link) {
                if (nfs4_open_readlink(rpc, res, data) < 0) {
                        /* It was a symlink and we have started trying to
                         * resolve it. Nothing more to do here.
                         */
                        return;
                }
        }

        if (data->flags & LOOKUP_FLAG_NO_FOLLOW) {
                /* Do not resolve the final component of the path
                 * if it is a symlink.
                 */
                resolve_link = 0;
        }

        /* Everything is good so we can just pass it on to the next
         * phase.
         */
        if (res->status == NFS4_OK && !resolve_link) {
                data->continue_cb(rpc, NFS4_OK, res, data);
                return;
        }

        /* Find the lookup that failed and the associated fh */
        data->link.idx = 0;
        for (i = 0; i < (int)res->resarray.resarray_len; i++) {
                if (res->resarray.resarray_val[i].resop == OP_LOOKUP) {
                        if (res->resarray.resarray_val[i].nfs_resop4_u.oplookup.status == NFS4ERR_SYMLINK) {
                                break;
                        }
                        data->link.idx++;
                }
        }

        if (!resolve_link && i == res->resarray.resarray_len) {
                nfs_set_error(nfs, "Symlink not found during lookup.");
                data->cb(-EFAULT, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        /* Build a new path that strips of everything after the symlink. */
        path = strdup(data->path);
        if (path == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to duplicate "
                              "path.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        /* The symlink is not the last component, so find the '/' before
         * the symlink and zero it out.
         */
        if (!resolve_link) {
                tmp = path;
                for (i = 0; i < (int)data->link.idx; i++) {
                        tmp = strchr(tmp + 1, '/');
                }
                *tmp = 0;
        }

        /* We need to resolve the symlink */
        if ((i = nfs4_allocate_op(nfs, &op, path, 1)) < 0) {
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                free(path);
                return;
        }

        /* Append a READLINK command */
        i += nfs4_op_readlink(nfs, &op[i]);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_lookup_path_2_cb, &args,
                                   data) == NULL) {
                nfs_set_error(nfs, "Failed to queue READLINK command. %s",
                              nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                free(path);
                free(op);
                return;
        }
        free(path);
        free(op);
}

static int
nfs4_lookup_path_async(struct nfs_context *nfs,
                       struct nfs4_cb_data *data,
                       rpc_cb cb)
{
        COMPOUND4args args;
        nfs_argop4 *op;
        char *path;
        int i, num_op;

        path = nfs4_resolve_path(nfs, data->path);
        if (path == NULL) {
                return -1;
        }
        free(data->path);
        data->path = path;

        path = strdup(path);
        if (path == NULL) {
                return -1;
        }

        if ((i = nfs4_allocate_op(nfs, &op, path, data->filler.max_op)) < 0) {
                free(path);
                return -1;
        }

        num_op = data->filler.func(data, &op[i]);
        data->continue_cb = cb;

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i + num_op;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_lookup_path_1_cb, &args,
                                   data) == NULL) {
                nfs_set_error(nfs, "Failed to queue LOOKUP command. %s",
                              nfs_get_error(nfs));
                free(path);
                free(op);
                return -1;
        }

        free(path);
        free(op);
        return 0;
}

static int
nfs4_populate_getfh(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        return nfs4_op_getfh(data->nfs, op);
}

static int
nfs4_populate_getattr(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        return nfs4_op_getfh(data->nfs, op);
}

static int
nfs4_populate_access(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        uint32_t mode;

        memcpy(&mode, data->filler.blob3.val, sizeof(uint32_t));

        return nfs4_op_access(data->nfs, op, mode);
}


static int
nfs_parse_rwmax(struct nfs_context *nfs, const char *buf, int len)
{
        /* READ MAX */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        nfs_set_readmax(nfs, nfs_pntoh64((uint32_t *)(void *)buf));
        buf += 8;
        len -= 8;
        /* WRITE MAX */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        nfs_set_writemax(nfs, nfs_pntoh64((uint32_t *)(void *)buf));
        buf += 8;
        len -= 8;

        return 0;
}

static void
nfs4_mount_5_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok;
        int i;

        if (check_nfs4_error(nfs, status, data, res, "RWMAX")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
        if (nfs_parse_rwmax(nfs,
                            garesok->obj_attributes.attr_vals.attrlist4_val,
                            garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
        }
        
	/*
	 * Now the entire mount process (including the NFS FSINFO and GETATTR)
	 * has completed. Any RPC failure till now would have caused the mount
	 * process to fail. Note that it's desirable for the mount process to
	 * fail upfront if it encounters any errors (TCP or RPC), rather than
	 * keep trying indefinitely causing the mount process to "hang", but
	 * from now on the RPC transport will be used to carry NFS RPCs issued
	 * by the application which may not be equipped to handle TCP connection
	 * failure and RPC timeouts, so we set the resiliency parameters of the
	 * rpc_context as selected by the user using the mount options. The
	 * default resiliency parameters emulate the common "hard" mount and
	 * we are resilient to any TCP or RPC connectivity issues.
         */
	rpc_set_resiliency(rpc,
			   nfs->nfsi->auto_reconnect,
			   nfs->nfsi->timeout,
			   nfs->nfsi->retrans);
        
        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_mount_4_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETFH4resok *gfhresok;
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfsfh nfsfh;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "GETFH")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETFH, "GETFH")) < 0) {
                return;
        }
        gfhresok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;

        nfs->nfsi->rootfh.len = gfhresok->object.nfs_fh4_len;
        nfs->nfsi->rootfh.val = malloc(nfs->nfsi->rootfh.len);
        if (nfs->nfsi->rootfh.val == NULL) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        memcpy(nfs->nfsi->rootfh.val,
               gfhresok->object.nfs_fh4_val,
               nfs->nfsi->rootfh.len);

        memset(op, 0, sizeof(op));
        nfsfh.fh = nfs->nfsi->rootfh;
        i = nfs4_op_putfh(nfs, &op[0], &nfsfh);
        i += nfs4_op_getattr(nfs, &op[i], rwmax_attributes, 1);
               
        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(rpc, nfs4_mount_5_cb, &args,
                                   private_data) == NULL) {
                nfs_set_error(nfs, "Failed to queue GETATTR rwmax. %s",
                              nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_mount_3_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "SETCLIENTID_CONFIRM")) {
                return;
        }

        data->filler.func = nfs4_populate_getfh;
        data->filler.max_op = 1;
        data->filler.data = calloc(2, sizeof(uint32_t));
        if (data->filler.data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "data structure.");
                data->cb(-ENOMEM, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }


        if (nfs4_lookup_path_async(nfs, data, nfs4_mount_4_cb) < 0) {
                data->cb(-ENOMEM, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_mount_2_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        COMPOUND4args args;
        nfs_argop4 op[1];
        SETCLIENTID4resok *scidresok;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "SETCLIENTID")) {
                return;
        }

        scidresok = &res->resarray.resarray_val[0].nfs_resop4_u.opsetclientid.SETCLIENTID4res_u.resok4;
        nfs->nfsi->clientid = scidresok->clientid;
        memcpy(nfs->nfsi->setclientid_confirm,
               scidresok->setclientid_confirm,
               NFS4_VERIFIER_SIZE);

        memset(op, 0, sizeof(op));

        i = nfs4_op_setclientid_confirm(nfs, &op[0], nfs->nfsi->clientid,
                                        nfs->nfsi->setclientid_confirm);
               
        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(rpc, nfs4_mount_3_cb, &args,
                                   private_data) == NULL) {
                nfs_set_error(nfs, "Failed to queue SETCLIENTID_CONFIRM. %s",
                              nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_mount_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4args args;
        nfs_argop4 op[1];
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, NULL, "CONNECT")) {
                return;
        }

        memset(op, 0, sizeof(op));

        i = nfs4_op_setclientid(nfs, &op[0], nfs->nfsi->verifier, nfs->nfsi->client_name);
        
        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(rpc, nfs4_mount_2_cb, &args, data) == NULL) {
                nfs_set_error(nfs, "Failed to queue SETCLIENTID. %s",
                              nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

int
nfs4_mount_async(struct nfs_context *nfs, const char *server,
                 const char *export, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        char *new_server, *new_export;
        int port;

        new_server = strdup(server);
	if (new_server == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs server string");
		return -1;
	}
        free(nfs->nfsi->server);
        nfs->nfsi->server = new_server;

	free(nfs->rpc->server);
	nfs->rpc->server = strdup(nfs->nfsi->server);

        new_export = strdup(export);
	if (new_export == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs export string");
		return -1;
	}
        if (nfs_normalize_path(nfs, new_export)) {
                nfs_set_error(nfs, "Bad export path. %s",
                              nfs_get_error(nfs));
                free(new_export);
                return -1;
        }
        free(nfs->nfsi->export);
        nfs->nfsi->export = new_export;


        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "memory for nfs mount data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;
        data->path         = strdup(new_export);

        port = nfs->nfsi->nfsport ? nfs->nfsi->nfsport : 2049;
        if (rpc_connect_port_async(nfs->rpc, server, port,
                                   NFS4_PROGRAM, NFS_V4,
                                   nfs4_mount_1_cb, data) != 0) {
                nfs_set_error(nfs, "Failed to start connection. %s",
                              nfs_get_error(nfs));
                free_nfs4_cb_data(data);
                return -1;
        }
        return 0;
}

static void
nfs4_chdir_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "CHDIR")) {
                return;
        }

        /* Ok, all good. Lets steal the path string. */
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->rpc->rpc_mutex);
        }
#endif
        free(nfs->nfsi->cwd);
        nfs->nfsi->cwd = data->path;
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->rpc->rpc_mutex);
        }
#endif

        data->path = NULL;

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int nfs4_chdir_async(struct nfs_context *nfs, const char *path,
                     nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_getattr;
        data->filler.max_op = 1;
        data->filler.data   = calloc(2, sizeof(uint32_t));
        if (data->filler.data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "data structure.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return -1;
        }

        if (nfs4_lookup_path_async(nfs, data, nfs4_chdir_1_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_xstat64_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok;
        struct nfs_stat_64 st;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "STAT64")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;

        memset(&st, 0, sizeof(st));
        if (nfs_parse_attributes(nfs, data, &st,
                                 garesok->obj_attributes.attr_vals.attrlist4_val,
                                 garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
        }

        data->cb(0, nfs, &st, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_stat64_async(struct nfs_context *nfs, const char *path,
                  int no_follow, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        if (no_follow) {
                data->flags |= LOOKUP_FLAG_NO_FOLLOW;
        }
        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_getattr;
        data->filler.max_op = 1;
        data->filler.data   = calloc(2, sizeof(uint32_t));
        if (data->filler.data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "data structure.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return -1;
        }

        if (nfs4_lookup_path_async(nfs, data, nfs4_xstat64_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

/* Takes object name as filler.data
 * blob0 as the fattr4 attribute mask
 * blob1 as the fattr4 attribute list
 */
static int
nfs4_populate_mkdir(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;

        return nfs4_op_create(nfs, op, data->filler.data, NF4DIR,
                              &data->filler.blob0, &data->filler.blob1,
                              NULL, 0);
}

static void
nfs4_mkdir_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "MKDIR")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_mkdir2_async(struct nfs_context *nfs, const char *path, int mode,
                 nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        uint32_t *u32ptr;

        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_mkdir;
        data->filler.max_op = 1;
        
        /* attribute mask */
        u32ptr = malloc(2 * sizeof(uint32_t));
        if (u32ptr == NULL) {
                nfs_set_error(nfs, "Out of memory allocating bitmap");
                free_nfs4_cb_data(data);
                return -1;
        }
        u32ptr[0] = 0;
        u32ptr[1] = 1 << (FATTR4_MODE - 32);
        data->filler.blob0.len  = 2;
        data->filler.blob0.val  = u32ptr;
        data->filler.blob0.free = free;

        /* attribute values */
        u32ptr = malloc(1 * sizeof(uint32_t));
        if (u32ptr == NULL) {
                nfs_set_error(nfs, "Out of memory allocating attributes");
                free_nfs4_cb_data(data);
                return -1;
        }
        u32ptr[0] = htonl(mode);
        data->filler.blob1.len  = 4;
        data->filler.blob1.val  = u32ptr;
        data->filler.blob1.free = free;

        if (nfs4_lookup_path_async(nfs, data, nfs4_mkdir_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

/* Takes object name as filler.data
 */
static int
nfs4_populate_remove(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;

        return nfs4_op_remove(nfs, op, data->filler.data);
}

static void
nfs4_remove_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "REMOVE")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_remove_async(struct nfs_context *nfs, const char *path,
                  nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_remove;
        data->filler.max_op = 1;

        if (nfs4_lookup_path_async(nfs, data, nfs4_remove_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_rmdir_async(struct nfs_context *nfs, const char *path,
                 nfs_cb cb, void *private_data)
{
        return nfs4_remove_async(nfs, path, cb, private_data);
}
    
static void
nfs_increment_seqid(struct nfsfh *nfsfh, uint32_t status)
{
        /* RFC3530 8.1.5 */
        switch (status) {
        case NFS4ERR_STALE_CLIENTID:
        case NFS4ERR_STALE_STATEID:
        case NFS4ERR_BAD_STATEID:
        case NFS4ERR_BAD_SEQID:
        case NFS4ERR_BADZDR:
        case NFS4ERR_RESOURCE:
        case NFS4ERR_NOFILEHANDLE:
                break;
        default:
                nfsfh->open_seqid++;
        }
}

static void
nfs4_open_setattr_cb(struct rpc_context *rpc, int status, void *command_data,
                     void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        struct nfsfh *fh;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "SETATTR")) {
                return;
        }

        fh = data->filler.blob0.val;
        data->filler.blob0.val = NULL;
        data->cb(0, nfs, fh, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_open_truncate_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *fh = data->filler.blob0.val;
        COMPOUND4res *res = command_data;
        COMPOUND4args args;
        nfs_argop4 op[2];
        int i;

        if (check_nfs4_error(nfs, status, data, res, "OPEN")) {
                return;
        }

        i = nfs4_op_putfh(nfs, op, fh);
        i += nfs4_op_truncate(nfs, &op[i], fh, data->filler.blob3.val);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_open_setattr_cb, &args,
                                   data) == NULL) {
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_open_chmod_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *fh = data->filler.blob0.val;
        COMPOUND4res *res = command_data;
        COMPOUND4args args;
        nfs_argop4 op[2];
        int i;

        if (check_nfs4_error(nfs, status, data, res, "OPEN")) {
                return;
        }

        i = nfs4_op_putfh(nfs, op, fh);
        i += nfs4_op_chmod(nfs, &op[i], fh, data->filler.blob3.val);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_open_setattr_cb, &args,
                                   data) == NULL) {
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_open_confirm_cb(struct rpc_context *rpc, int status, void *command_data,
                     void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        OPEN_CONFIRM4resok *ocresok;
        int i;
        struct nfsfh *fh = data->filler.blob0.val;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (res) {
                nfs_increment_seqid(fh, res->status);
        }

        if (check_nfs4_error(nfs, status, data, res, "OPEN_CONFIRM")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_OPEN_CONFIRM,
                              "OPEN_CONFIRM")) < 0) {
                return;
        }
        ocresok = &res->resarray.resarray_val[i].nfs_resop4_u.opopen_confirm.OPEN_CONFIRM4res_u.resok4;

        fh = data->filler.blob0.val;

        fh->stateid.seqid = ocresok->open_stateid.seqid;
        memcpy(fh->stateid.other, ocresok->open_stateid.other, 12);

        if (data->open_cb) {
                data->open_cb(rpc, status, command_data, private_data);
                return;
        }
        data->filler.blob0.val = NULL;
        data->cb(0, nfs, fh, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_open_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        ACCESS4resok *aresok;
        OPEN4resok *oresok;
        GETFH4resok *gresok;
        int i;
        struct nfsfh *fh;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "OPEN")) {
                return;
        }

        /* Parse Access and check that we have the access that we need */
        if ((i = nfs4_find_op(nfs, data, res, OP_ACCESS, "ACCESS")) < 0) {
                return;
        }
        aresok = &res->resarray.resarray_val[i].nfs_resop4_u.opaccess.ACCESS4res_u.resok4;
        if (aresok->supported != aresok->access) {
                nfs_set_error(nfs, "Insufficient ACCESS. Wanted %08x but "
                              "got %08x.", (int)aresok->access, (int)aresok->supported);
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        /* Parse GetFH */
        if ((i = nfs4_find_op(nfs, data, res, OP_GETFH, "GETFH")) < 0) {
                return;
        }
        gresok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;

        fh = calloc(1, sizeof(*fh));
        if (fh == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        data->filler.blob0.val  = fh;
        data->filler.blob0.free = (blob_free)nfs_free_nfsfh;

        fh->fh.len = gresok->object.nfs_fh4_len;
        fh->fh.val = malloc(fh->fh.len);
        if (fh->fh.val == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        memcpy(fh->fh.val, gresok->object.nfs_fh4_val, fh->fh.len);
        fh->open_seqid = 1;
        fh->open_owner = data->open_owner;
        
        if (data->filler.flags & O_SYNC) {
                fh->is_sync = 1;
        }

        if (data->filler.flags & O_APPEND) {
                fh->is_append = 1;
        }

	if (!(data->filler.flags & (O_WRONLY|O_RDWR))) {
                fh->is_readonly = 1;
        }

        /* Parse Open */
        if ((i = nfs4_find_op(nfs, data, res, OP_OPEN, "OPEN")) < 0) {
                return;
        }
        oresok = &res->resarray.resarray_val[i].nfs_resop4_u.opopen.OPEN4res_u.resok4;
        fh->stateid.seqid = oresok->stateid.seqid;
        memcpy(fh->stateid.other, oresok->stateid.other, 12);


        if (oresok->rflags & OPEN4_RESULT_CONFIRM) {
                COMPOUND4args args;
                nfs_argop4 op[2];

                memset(op, 0, sizeof(op));
                i = nfs4_op_putfh(nfs, &op[0], fh);
                i += nfs4_op_open_confirm(nfs, &op[i], fh);

                memset(&args, 0, sizeof(args));
                args.argarray.argarray_len = i;
                args.argarray.argarray_val = op;

                if (rpc_nfs4_compound_task(rpc, nfs4_open_confirm_cb, &args,
                                           private_data) == NULL) {
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                        return;
                }
                return;
        }

        if (data->open_cb) {
                data->open_cb(rpc, status, command_data, private_data);
                return;
        }
        data->filler.blob0.val = NULL;
        data->cb(0, nfs, fh, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_init_random_verifier(char *verifier)
{
        static uint64_t seed = 0, v;
        int i;

        if (seed == 0) {
                seed = ~rpc_current_time() << 32 | getpid();
        } else {
                seed *= 1337;
        }

        v = seed;
        for (i = 0; i < NFS4_VERIFIER_SIZE; i++) {
                verifier[i] = v & 0xff;
                v >>= 8;
        }
}

/* filler.flags are the open flags
 * filler.data is the object name
 */
static int
nfs4_populate_open(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        OPEN4args *oargs;
        uint32_t access_mask = 0;
        verifier4 verifier;
        int i;

        if (data->filler.flags & O_WRONLY) {
                access_mask |= ACCESS4_MODIFY;
        }
        if (data->filler.flags & O_RDWR) {
                access_mask |= ACCESS4_READ|ACCESS4_MODIFY;
        }
        if (!(data->filler.flags & O_WRONLY)) {
                access_mask |= ACCESS4_READ;
        }
        
        /* Access */
        i = nfs4_op_access(nfs, &op[0], access_mask);

        /* Open */
        op[i].argop = OP_OPEN;
        oargs = &op[i++].nfs_argop4_u.opopen;
        memset(oargs, 0, sizeof(*oargs));

        if (access_mask & ACCESS4_READ) {
                oargs->share_access |= OPEN4_SHARE_ACCESS_READ;
        }
        if (access_mask & ACCESS4_MODIFY) {
                oargs->share_access |= OPEN4_SHARE_ACCESS_WRITE;
        }
        oargs->share_deny = OPEN4_SHARE_DENY_NONE;
        oargs->owner.clientid = nfs->nfsi->clientid;
        oargs->owner.owner.owner_len = 4;
        oargs->owner.owner.owner_val = (char *)&data->open_owner;
        oargs->seqid = 0;
        if (data->filler.flags & O_CREAT) {
                createhow4 *ch;
                fattr4 *fa;

                ch = &oargs->openhow.openflag4_u.how;
                fa = &ch->createhow4_u.createattrs;

                oargs->openhow.opentype = OPEN4_CREATE;
                if (data->filler.flags & O_EXCL) {
                        ch->mode = EXCLUSIVE4;

                        nfs4_init_random_verifier(&verifier[0]);
                        memcpy(ch->createhow4_u.createverf, verifier,
                               sizeof(verifier4));
                } else {
                        ch->mode = UNCHECKED4;
                        fa->attrmask.bitmap4_len = data->filler.blob1.len;
                        fa->attrmask.bitmap4_val = data->filler.blob1.val;

                        fa->attr_vals.attrlist4_len = data->filler.blob2.len;
                        fa->attr_vals.attrlist4_val = data->filler.blob2.val;
                }
        } else {
                oargs->openhow.opentype = OPEN4_NOCREATE;
        }
        oargs->claim.claim = CLAIM_NULL;
        oargs->claim.open_claim4_u.file.utf8string_len =
                strlen(data->filler.data);
        oargs->claim.open_claim4_u.file.utf8string_val =
                data->filler.data;

        /* GetFH */
        i += nfs4_op_getfh(nfs, &op[i]);

        return i;
}

static void
nfs4_open_readlink_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        READLINK4resok *rlresok;
        int i;
        char *path;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "READLINK")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_READLINK, "READLINK")) < 0) {
                return;
        }

        rlresok = &res->resarray.resarray_val[i].nfs_resop4_u.opreadlink.READLINK4res_u.resok4;

        path = malloc(2 + strlen(data->path) +
                      rlresok->link.utf8string_len);
        if (path == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "path");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        sprintf(path, "%s/%.*s", data->path,
                (int)rlresok->link.utf8string_len, rlresok->link.utf8string_val);


        free(data->path);
        data->path = NULL;
        free(data->filler.data);
        data->filler.data = NULL;

        data->path = nfs4_resolve_path(nfs, path);
        free(path);
        if (data->path == NULL) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs),
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        data_split_path(data);

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->nfsi->nfs4_open_counter_mutex);
        }
#endif
        data->open_owner = nfs->nfsi->open_counter++;
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->nfsi->nfs4_open_counter_mutex);
        }
#endif
        
        data->filler.func = nfs4_populate_open;
        data->filler.max_op = 3;
 
        if (nfs4_lookup_path_async(nfs, data, nfs4_open_cb) < 0) {
                data->cb(-ENOMEM, nfs, res, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static int
nfs4_populate_lookup_readlink(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        int i;

        i = nfs4_op_lookup(nfs, &op[0], data->filler.data);
        i += nfs4_op_readlink(nfs, &op[i]);

        return i;
}

/* If the final component in the open was a symlink we need to resolve it and
 * re-try the nfs4_open_async()
 */
static int
nfs4_open_readlink(struct rpc_context *rpc, COMPOUND4res *res,
                   struct nfs4_cb_data *data)
{
        struct nfs_context *nfs = data->nfs;
        int i;

        for (i = 0; i < (int)res->resarray.resarray_len; i++) {
                OPEN4res *ores;

                if (res->resarray.resarray_val[i].resop != OP_OPEN) {
                        continue;
                }
                ores = &res->resarray.resarray_val[i].nfs_resop4_u.opopen;

                if (ores->status != NFS4ERR_SYMLINK) {
                        continue;
                }

                if (data->filler.flags & O_NOFOLLOW) {
                        nfs_set_error(nfs, "Symlink encountered during "
                                      "open(O_NOFOLLOW)");
                        data->cb(-ELOOP, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        return -1;
                }

                /* The object we need to do readlink on is already stored in
                 * data->filler.data so *populate* can just grab it from there.
                 */
                data->filler.func = nfs4_populate_lookup_readlink;
                data->filler.max_op = 2;

                if (nfs4_lookup_path_async(nfs, data,
                                           nfs4_open_readlink_cb) < 0) {
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                        return -1;
                }
                return -1;
        }

        return 0;
}

/*
 * data.blob0 is used for nfsfh
 * data.blob1 is used for the attribute mask in case on O_CREAT
 * data.blob2 is the attribute value in case of O_CREAT
 */
static int
nfs4_open_async_internal(struct nfs_context *nfs, struct nfs4_cb_data *data,
                         int flags, int mode)
{
        if (flags & O_APPEND && !(flags & (O_RDWR|O_WRONLY))) {
                flags &= ~O_APPEND;
        }

        if (flags & O_CREAT) {
                uint32_t *d;

                /* Attribute mask */
                d = malloc(2 * sizeof(uint32_t));
                if (d == NULL) {
                        nfs_set_error(nfs, "Out of memory");
                        free_nfs4_cb_data(data);
                        return -1;
                }
                d[0] = 0;
                d[1] = 1 << (FATTR4_MODE - 32);

                data->filler.blob1.val  = d;
                data->filler.blob1.len  = 2;
                data->filler.blob1.free = free;

                /* Attribute value */
                d = malloc(sizeof(uint32_t));
                if (d == NULL) {
                        nfs_set_error(nfs, "Out of memory");
                        free_nfs4_cb_data(data);
                        return -1;
                }

                *d = htonl(mode);

                data->filler.blob2.val  = d;
                data->filler.blob2.len  = 4;
                data->filler.blob2.free = free;
        }

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->nfsi->nfs4_open_counter_mutex);
        }
#endif        
        data->open_owner = nfs->nfsi->open_counter++;
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->nfsi->nfs4_open_counter_mutex);
        }
#endif        
        data->filler.func = nfs4_populate_open;
        data->filler.max_op = 3;
        data->filler.flags = flags;

        if (nfs4_lookup_path_async(nfs, data, nfs4_open_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_open_async(struct nfs_context *nfs, const char *path, int flags,
                int mode, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        uint32_t m;
        int ret;
        
        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;

        /* O_TRUNC is only valid for O_RDWR or O_WRONLY */
        if (flags & O_TRUNC && !(flags & (O_RDWR|O_WRONLY))) {
                flags &= ~O_TRUNC;
        }
        /* Successful O_EXCL means the file is 0 size already. */
        if (flags & O_EXCL) {
                flags &= ~O_TRUNC;
        }

        if (flags & O_TRUNC) {
                data->open_cb = nfs4_open_truncate_cb;

                data->filler.blob3.val = malloc(12);
                if (data->filler.blob3.val == NULL) {
                        nfs_set_error(nfs, "Out of memory");
                        free_nfs4_cb_data(data);
                        return -1;
                }
                data->filler.blob3.free = free;

                memset(data->filler.blob3.val, 0, 12);
        }
        if (flags & O_EXCL) {
                data->open_cb = nfs4_open_chmod_cb;

                data->filler.blob3.val = malloc(4);
                if (data->filler.blob3.val == NULL) {
                        nfs_set_error(nfs, "Out of memory");
                        free_nfs4_cb_data(data);
                        return -1;
                }
                data->filler.blob3.free = free;

                m = htonl(mode);
                memcpy(data->filler.blob3.val, &m, sizeof(uint32_t));
        }

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->nfsi->nfs4_open_call_mutex);
                data->flags |= MUTEX_HELD;
        }
#endif        
        ret = nfs4_open_async_internal(nfs, data, flags, mode);
        return ret;
}

int
nfs4_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                   void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_getattr(nfs, &op[i], standard_attributes, 2);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_xstat64_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_getacl_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok;
        fattr4_acl acl;
        ZDR zdr;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "GETACL")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;

        memset(&acl, 0, sizeof(acl));
        zdrmem_create(&zdr,
                      garesok->obj_attributes.attr_vals.attrlist4_val,
                      garesok->obj_attributes.attr_vals.attrlist4_len,
                      ZDR_DECODE);
        if (zdr_fattr4_acl(&zdr, &acl)) {
                data->cb(0, nfs, &acl, data->private_data);
        } else {
                data->cb(-EIO, nfs, "Failed to unmarshall fattr4_acl", data->private_data);
        }

        zdr_destroy(&zdr);
        free_nfs4_cb_data(data);
}

int
nfs4_getacl_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                   void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_getattr(nfs, &op[i], getacl_attributes, 1);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_getacl_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}


static void
nfs4_close_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        struct nfsfh *nfsfh = data->filler.blob0.val;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (res) {
                nfs_increment_seqid(nfsfh, res->status);
        }

        if (check_nfs4_error(nfs, status, data, res, "CLOSE")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[3];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->nfsi->nfs4_open_call_mutex);
                data->flags |= MUTEX_HELD;
        }
#endif        
        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_close(nfs, &op[i], nfsfh);

        data->filler.blob0.val  = nfsfh;
        data->filler.blob0.free = (blob_free)nfs_free_nfsfh;

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_close_cb, &args,
                                   data) == NULL) {
                data->filler.blob0.val = NULL;
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_pread_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        READ4resok *rres = NULL;
        struct nfsfh *nfsfh;
        int i, count;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        nfsfh = data->filler.blob0.val;

        if (check_nfs4_error(nfs, status, data, res, "READ")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_READ, "READ")) < 0) {
                return;
        }
        rres = &res->resarray.resarray_val[i].nfs_resop4_u.opread.READ4res_u.resok4;

        if (data->rw_data.update_pos) {
                nfsfh->offset = data->rw_data.offset + rres->data.data_len;
        }

        count = rres->data.data_len;

        if (count > rpc->pdu->requested_read_count) {
                count = rpc->pdu->requested_read_count;
        }

        data->cb(count, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_pread_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                          void *buf, size_t count, uint64_t offset,
                          nfs_cb cb, void *private_data, int update_pos)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;
        struct rpc_pdu *pdu;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = nfsfh;
        data->filler.blob0.free = NULL;
        data->rw_data.offset = offset;
        data->rw_data.update_pos = update_pos;
        
        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_read(nfs, &op[i], nfsfh, offset, count);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        pdu = rpc_nfs4_read_task(nfs->rpc, nfs4_pread_cb, buf, count, &args,
                                 data);
        if (pdu == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_preadv_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const struct iovec *iov, int iovcnt, uint64_t offset,
                           nfs_cb cb, void *private_data, int update_pos)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;
        struct rpc_pdu *pdu;
        size_t count = 0;
        
        for (i = 0; i < iovcnt; i++) {
                count += iov[i].iov_len;
        }
        
        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = nfsfh;
        data->filler.blob0.free = NULL;
        data->rw_data.offset = offset;
        data->rw_data.update_pos = update_pos;
        
        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_read(nfs, &op[i], nfsfh, offset, count);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        pdu = rpc_nfs4_readv_task(nfs->rpc, nfs4_pread_cb, iov, iovcnt, &args,
                                 data);
        if (pdu == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_symlink_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "SYMLINK")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

/* Takes object name as filler.data
 * blob0 as the target
 */
static int
nfs4_populate_symlink(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;

        return nfs4_op_create(nfs, op, data->filler.data, NF4LNK,
                              NULL, NULL, data->filler.blob0.val, 0);

        return 1;
}

int
nfs4_symlink_async(struct nfs_context *nfs, const char *target,
                   const char *linkname, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, linkname);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_symlink;
        data->filler.max_op = 1;

        data->filler.blob0.val  = strdup(target);
        data->filler.blob0.free = free;

        if (nfs4_lookup_path_async(nfs, data, nfs4_symlink_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_readlink_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        READLINK4resok *rlresok;
        char* target;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "READLINK")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_READLINK, "READLINK")) < 0) {
                return;
        }

        rlresok = &res->resarray.resarray_val[i].nfs_resop4_u.opreadlink.READLINK4res_u.resok4;

        target = strndup(rlresok->link.utf8string_val,
                         rlresok->link.utf8string_len);
        if (target == NULL) {
                data->cb(-ENOMEM, nfs, "Failed to allocate memory",
                         data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        data->filler.blob0.val  = target;
        data->filler.blob0.free = free;        
        data->cb(0, nfs, target, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_readlink(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        int i;

        i = nfs4_op_readlink(nfs, &op[0]);

        return i;
}

int
nfs4_readlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                    void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_readlink;
        data->filler.max_op = 1;
        data->flags |= LOOKUP_FLAG_NO_FOLLOW;

        if (nfs4_lookup_path_async(nfs, data, nfs4_readlink_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_pwrite_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        WRITE4resok *wres = NULL;
        struct nfsfh *nfsfh;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        nfsfh = data->filler.blob0.val;

        if (check_nfs4_error(nfs, status, data, res, "WRITE")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_WRITE, "WRITE")) < 0) {
                return;
        }
        wres = &res->resarray.resarray_val[i].nfs_resop4_u.opwrite.WRITE4res_u.resok4;

        if (data->rw_data.update_pos) {
                nfsfh->offset = data->rw_data.offset + wres->count;
        }

        data->cb(wres->count, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_pwrite_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const void *buf, size_t count, uint64_t offset,
                           nfs_cb cb, void *private_data, int update_pos)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "cb data");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = nfsfh;
        data->filler.blob0.free = NULL;
        data->rw_data.offset = offset;
        data->rw_data.update_pos = update_pos;

        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], nfsfh);
        i += nfs4_op_write(nfs, &op[i], nfsfh, offset, count, buf);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_write_task(nfs->rpc, nfs4_pwrite_cb, buf, count,
                                &args, data) == NULL) {
                nfs_set_error(nfs, "PWRITE "
                        "failed: %s", rpc_get_error(nfs->rpc));
                free_nfs4_cb_data(data);
                return -EIO;
        }

        return 0;
}

static void
nfs4_write_append_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok = NULL;
        struct nfsfh *nfsfh;
        int i;
        uint64_t offset;
        char *buf;
        uint32_t count;
        struct nfs_stat_64 st;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        nfsfh = data->filler.blob0.val;

        buf = data->filler.blob1.val;
        count = data->filler.blob1.len;

        if (check_nfs4_error(nfs, status, data, res, "GETATTR")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }


        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
        if (garesok->obj_attributes.attr_vals.attrlist4_len < 8) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        memset(&st, 0, sizeof(st));
        nfs_parse_attributes(nfs, data, &st,
                             garesok->obj_attributes.attr_vals.attrlist4_val,
                             garesok->obj_attributes.attr_vals.attrlist4_len);
        offset = st.nfs_size;

        if (nfs4_pwrite_async_internal(nfs, nfsfh,
                                       buf, (size_t)count, offset,
                                       data->cb, data->private_data, 1) < 0) {
                free_nfs4_cb_data(data);
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                return;
        }

        free_nfs4_cb_data(data);
}

int
nfs4_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh, uint64_t count,
                const void *buf, nfs_cb cb, void *private_data)
{
        if (nfsfh->is_append) {
                COMPOUND4args args;
                nfs_argop4 op[2];
                struct nfs4_cb_data *data;
                int i;

                data = calloc(1, sizeof(*data));
                if (data == NULL) {
                        nfs_set_error(nfs, "Out of memory. Failed to allocate "
                                      "cb data");
                        return -1;
                }

                data->nfs          = nfs;
                data->cb           = cb;
                data->private_data = private_data;

                data->filler.blob0.val  = nfsfh;
                data->filler.blob0.free = NULL;

                memset(op, 0, sizeof(op));

                i = nfs4_op_putfh(nfs, &op[0], nfsfh);
                i += nfs4_op_getattr(nfs, &op[i], standard_attributes, 2);

                memset(&args, 0, sizeof(args));
                args.argarray.argarray_len = i;
                args.argarray.argarray_val = op;

                data->filler.blob0.val  = nfsfh;
                data->filler.blob0.free = NULL;

                data->filler.blob1.val = discard_const(buf);
                data->filler.blob1.len = (int)count;
                data->filler.blob1.free = NULL;

                if (rpc_nfs4_compound_task2(nfs->rpc, nfs4_write_append_cb,
                                            &args, data, count) == NULL) {
                        nfs_set_error(nfs, "PWRITE "
                                "failed: %s", rpc_get_error(nfs->rpc));
                        free_nfs4_cb_data(data);
                        return -EIO;
                }

                return 0;
        }

        return nfs4_pwrite_async_internal(nfs, nfsfh,
                                          buf, (size_t)count, nfsfh->offset,
                                          cb, private_data, 1);
}

int
nfs4_creat_async(struct nfs_context *nfs, const char *path,
                 int mode, nfs_cb cb, void *private_data)
{
        return nfs4_open_async(nfs, path, O_CREAT|O_WRONLY|O_TRUNC, mode,
                               cb, private_data);
}

int
nfs4_unlink_async(struct nfs_context *nfs, const char *path,
                  nfs_cb cb, void *private_data)
{
        return nfs4_remove_async(nfs, path, cb, private_data);
}

static void
nfs4_link_2_cb(struct rpc_context *rpc, int status, void *command_data,
             void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "LINK")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_link(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *nfsfh = data->filler.blob0.val;

        int i;

        i = nfs4_op_savefh(nfs, &op[0]);
        i += nfs4_op_putfh(nfs, &op[i], nfsfh);
        i += nfs4_op_link(nfs, &op[i], data->filler.data);

        return i;
}

static void
nfs4_link_1_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETFH4resok *gfhresok;
        int i;
        struct nfsfh *fh;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "LINK")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETFH, "GETFH")) < 0) {
                return;
        }
        gfhresok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;

        /* oldpath fh */
        fh = calloc(1, sizeof(*fh));
        if (fh == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        data->filler.blob0.val  = fh;
        data->filler.blob0.free = (blob_free)nfs_free_nfsfh;

        fh->fh.len = gfhresok->object.nfs_fh4_len;
        fh->fh.val = malloc(fh->fh.len);
        if (fh->fh.val == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        memcpy(fh->fh.val, gfhresok->object.nfs_fh4_val, fh->fh.len);


        data->filler.func = nfs4_populate_link;
        data->filler.max_op = 3;

        free(data->path);
        data->path = data->filler.blob1.val;
        data->filler.blob1.val  = NULL;
        data->filler.blob1.free = NULL;

        if (nfs4_lookup_path_async(nfs, data, nfs4_link_2_cb) < 0) {
                data->cb(-EFAULT, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

/*
 * filler.data is the name of the new object
 * blob0 is the filehandle for newpath parent directory.
 * blob1 is oldpath.
 */
int
nfs4_link_async(struct nfs_context *nfs, const char *oldpath,
                const char *newpath, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, newpath);
        if (data == NULL) {
                return -1;
        }

        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_getfh;
        data->filler.max_op = 1;

        /* oldpath */
        data->filler.blob1.val  = strdup(oldpath);
        if (data->filler.blob1.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob1.free = free;

        if (nfs4_lookup_path_async(nfs, data, nfs4_link_1_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_rename_2_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "RENAME")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_rename(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *nfsfh = data->filler.blob0.val;
        int i;

        i = nfs4_op_savefh(nfs, &op[0]);
        i += nfs4_op_putfh(nfs, &op[i], nfsfh);
        i += nfs4_op_rename(nfs, &op[i], data->filler.data,
                            data->filler.blob1.val);

        return i;
}

static void
nfs4_rename_1_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETFH4resok *gfhresok;
        int i;
        struct nfsfh *fh;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "RENAME")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETFH, "GETFH")) < 0) {
                return;
        }
        gfhresok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;

        /* newpath fh */
        fh = calloc(1, sizeof(*fh));
        if (fh == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        data->filler.blob0.val  = fh;
        data->filler.blob0.free = (blob_free)nfs_free_nfsfh;

        fh->fh.len = gfhresok->object.nfs_fh4_len;
        fh->fh.val = malloc(fh->fh.len);
        if (fh->fh.val == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        memcpy(fh->fh.val, gfhresok->object.nfs_fh4_val, fh->fh.len);

        data->filler.blob1.val  = data->filler.data;
        data->filler.blob1.free = free;
        data->filler.data = NULL;

        /* Update path and data to point to the old path/name */
        free(data->path);
        data->path = nfs4_resolve_path(nfs, data->filler.blob2.val);
        if (data->path == NULL) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        data_split_path(data);

        data->filler.func   = nfs4_populate_rename;
        data->filler.max_op = 3;

        if (nfs4_lookup_path_async(nfs, data, nfs4_rename_2_cb) < 0) {
                nfs_set_error(nfs, "Out of memory.");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

/*
 * blob0 is the filehandle for newpath parent directory.
 * blob1 is the new name
 * blob2 is oldpath.
 */
int
nfs4_rename_async(struct nfs_context *nfs, const char *oldpath,
                  const char *newpath, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, newpath);
        if (data == NULL) {
                return -1;
        }

        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_getfh;
        data->filler.max_op = 1;

        /* oldpath */
        data->filler.blob2.val  = strdup(oldpath);
        if (data->filler.blob2.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob2.free = free;

        if (nfs4_lookup_path_async(nfs, data, nfs4_rename_1_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_mknod_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "MKNOD")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_mknod(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        uint32_t mode, *ptr;
        int dev;

        /* Strip off the file type before we marshall it */
        ptr = (void *)data->filler.blob1.val;
        mode = *ptr;
        *ptr = htonl(mode & ~S_IFMT);

        dev = data->filler.blob2.len;

        switch (mode & S_IFMT) {
	case S_IFBLK:
                return nfs4_op_create(nfs, op, data->filler.data, NF4BLK,
                                      &data->filler.blob0, &data->filler.blob1,
                                      NULL, dev);
	case S_IFCHR:
                return nfs4_op_create(nfs, op, data->filler.data, NF4CHR,
                                      &data->filler.blob0, &data->filler.blob1,
                                      NULL, dev);
        }

        return 1;
}

/* Takes object name as filler.data
 * blob0 as attribute mask
 * blob1 as attribute value
 * blob2.len as dev
 */
int
nfs4_mknod_async(struct nfs_context *nfs, const char *path, int mode, int dev,
                 nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        uint32_t *u32ptr;

        switch (mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
                break;
        default:
		nfs_set_error(nfs, "Invalid file type for "
                              "MKNOD call");
		return -1;
        }

        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_mknod;
        data->filler.max_op = 1;

        /* attribute mask */
        u32ptr = malloc(2 * sizeof(uint32_t));
        if (u32ptr == NULL) {
                nfs_set_error(nfs, "Out of memory allocating bitmap");
                return 0;
        }
        u32ptr[0] = 0;
        u32ptr[1] = 1 << (FATTR4_MODE - 32);
        data->filler.blob0.len  = 2;
        data->filler.blob0.val  = u32ptr;
        data->filler.blob0.free = free;

        /* attribute values */
        u32ptr = malloc(1 * sizeof(uint32_t));
        if (u32ptr == NULL) {
                nfs_set_error(nfs, "Out of memory allocating attributes");
                free_nfs4_cb_data(data);
                return -1;
        }
        u32ptr[0] = mode;
        data->filler.blob1.len  = 4;
        data->filler.blob1.val  = u32ptr;
        data->filler.blob1.free = free;

        data->filler.blob2.len  = dev;

        if (nfs4_lookup_path_async(nfs, data, nfs4_mknod_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_parse_readdir(struct nfs_context *nfs, struct nfs4_cb_data *data,
                   READDIR4resok *res);

static void
nfs4_opendir_2_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        READDIR4resok *rdresok;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "READDIR")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_READDIR, "READDIR")) < 0) {
                return;
        }
        rdresok = &res->resarray.resarray_val[i].nfs_resop4_u.opreaddir.READDIR4res_u.resok4;
        nfs4_parse_readdir(nfs, data, rdresok);
}

static void
nfs4_opendir_continue(struct nfs_context *nfs, struct nfs4_cb_data *data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfsfh *fh = data->filler.blob0.val;
        uint64_t cookie;
        int i;

        memcpy(&cookie, data->filler.blob2.val, sizeof(uint64_t));

        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_readdir(nfs, &op[i], cookie);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_opendir_2_cb, &args,
                                   data) == NULL) {
                nfs_set_error(nfs, "Failed to queue READDIR command. %s",
                              nfs_get_error(nfs));
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

static void
nfs4_parse_readdir(struct nfs_context *nfs, struct nfs4_cb_data *data,
                   READDIR4resok *res)
{
	struct nfsdir *nfsdir = data->filler.blob1.val;
        struct entry4 *e;

        e = res->reply.entries;
        while (e) {
                struct nfsdirent *nfsdirent;
                struct nfs_stat_64 st;

                memcpy(data->filler.blob2.val, &e->cookie, sizeof(uint64_t));

		nfsdirent = malloc(sizeof(struct nfsdirent));
		if (nfsdirent == NULL) {
                        nfs_set_error(nfs, "Out of memory.");
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                        return;
                }
		nfsdirent->name = strdup(e->name.utf8string_val);
		if (nfsdirent->name == NULL) {
                        nfs_set_error(nfs, "Out of memory.");
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                        free(nfsdirent);
                        return;
                }

                memset(&st, 0, sizeof(st));
                if (nfs_parse_attributes(nfs, data, &st,
                                         e->attrs.attr_vals.attrlist4_val,
                                         e->attrs.attr_vals.attrlist4_len) < 0) {
                        data->cb(-EINVAL, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                        free(nfsdirent->name);
                        free(nfsdirent);
                        return;
                }

                nfsdirent->inode = st.nfs_ino;
                nfsdirent->mode = (uint32_t)st.nfs_mode;
                switch (st.nfs_mode & S_IFMT) {
                case S_IFREG:
                        nfsdirent->type = NF4REG;
                        break;
                case S_IFDIR:
                        nfsdirent->type = NF4DIR;
                        break;
                case S_IFBLK:
                        nfsdirent->type = NF4BLK;
                        break;
                case S_IFCHR:
                        nfsdirent->type = NF4CHR;
                        break;
                case S_IFLNK:
                        nfsdirent->type = NF4LNK;
                        break;
                case S_IFSOCK:
                        nfsdirent->type = NF4SOCK;
                        break;
                case S_IFIFO:
                        nfsdirent->type = NF4FIFO;
                        break;
                }
                nfsdirent->size = st.nfs_size;
                nfsdirent->atime.tv_sec  = (long)st.nfs_atime;
                nfsdirent->atime.tv_usec = (long)(st.nfs_atime_nsec/1000);
                nfsdirent->atime_nsec    = (uint32_t)st.nfs_atime_nsec;
                nfsdirent->mtime.tv_sec  = (long)st.nfs_mtime;
                nfsdirent->mtime.tv_usec = (long)(st.nfs_mtime_nsec/1000);
                nfsdirent->mtime_nsec    = (uint32_t)st.nfs_mtime_nsec;
                nfsdirent->ctime.tv_sec  = (long)st.nfs_ctime;
                nfsdirent->ctime.tv_usec = (long)(st.nfs_ctime_nsec/1000);
                nfsdirent->ctime_nsec    = (uint32_t)st.nfs_ctime_nsec;
                nfsdirent->uid = (uint32_t)st.nfs_uid;
                nfsdirent->gid = (uint32_t)st.nfs_gid;
                nfsdirent->nlink = (uint32_t)st.nfs_nlink;
                nfsdirent->dev = st.nfs_dev;
                nfsdirent->rdev = st.nfs_rdev;
                nfsdirent->blksize = NFS_BLKSIZE;
                nfsdirent->blocks = st.nfs_blocks;
                nfsdirent->used = st.nfs_used;

		nfsdirent->next  = nfsdir->entries;
		nfsdir->entries  = nfsdirent;
                e = e->nextentry;
        }

        if (res->reply.eof == 0) {
                nfs4_opendir_continue(nfs, data);
                return;
        }

        nfsdir->current = nfsdir->entries;
        data->filler.blob1.val = NULL;
        data->cb(0, nfs, nfsdir, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_opendir_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        struct nfsfh *fh;
        GETFH4resok *gresok;
        READDIR4resok *rdresok;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "READDIR")) {
                return;
        }

        /* Parse GetFH */
        if ((i = nfs4_find_op(nfs, data, res, OP_GETFH, "GETFH")) < 0) {
                return;
        }
        gresok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetfh.GETFH4res_u.resok4;

        fh = calloc(1, sizeof(*fh));
        if (fh == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        data->filler.blob0.val  = fh;
        data->filler.blob0.free = (blob_free)nfs_free_nfsfh;

        fh->fh.len = gresok->object.nfs_fh4_len;
        fh->fh.val = malloc(fh->fh.len);
        if (fh->fh.val == NULL) {
                nfs_set_error(nfs, "Out of memory. Failed to allocate "
                              "nfsfh");
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
        memcpy(fh->fh.val, gresok->object.nfs_fh4_val, fh->fh.len);

        if ((i = nfs4_find_op(nfs, data, res, OP_READDIR, "READDIR")) < 0) {
                return;
        }
        rdresok = &res->resarray.resarray_val[i].nfs_resop4_u.opreaddir.READDIR4res_u.resok4;
        nfs4_parse_readdir(nfs, data, rdresok);
}

static int
nfs4_populate_readdir(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        struct nfs_context *nfs = data->nfs;
        uint64_t cookie;
        int i;

        memcpy(&cookie, data->filler.blob2.val, sizeof(uint64_t));

        i = nfs4_op_getfh(nfs, &op[0]);
        i += nfs4_op_readdir(nfs, &op[i], cookie);

        return i;
}


/* blob0 is the directory filehandle
 * blob1 is nfsdir
 * blob2 is the cookie
 */
int
nfs4_opendir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
        struct nfs4_cb_data *data;
	struct nfsdir *nfsdir;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_readdir;
        data->filler.max_op = 2;

	nfsdir = calloc(1, sizeof(struct nfsdir));
	if (nfsdir == NULL) {
                free_nfs4_cb_data(data);
		nfs_set_error(nfs, "failed to allocate buffer for nfsdir");
		return -1;
	}

        data->filler.blob1.val = nfsdir;
        data->filler.blob1.free = (blob_free)nfs_free_nfsdir;

	data->filler.blob2.val = calloc(1, sizeof(uint64_t));
	if (data->filler.blob2.val == NULL) {
                free_nfs4_cb_data(data);
		nfs_set_error(nfs, "failed to allocate buffer for cookie");
		return -1;
	}
        data->filler.blob2.free = (blob_free)free;

        if (nfs4_lookup_path_async(nfs, data, nfs4_opendir_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_truncate_close_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        struct nfsfh *fh = data->filler.blob0.val;
                
        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (res) {
                nfs_increment_seqid(fh, res->status);
        }

        if (check_nfs4_error(nfs, status, data, res, "CLOSE")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static void
nfs4_truncate_open_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *fh = data->filler.blob0.val;
        COMPOUND4res *res = command_data;
        COMPOUND4args args;
        nfs_argop4 op[4];
        int i;

        if (check_nfs4_error(nfs, status, data, res, "OPEN")) {
                return;
        }

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_truncate(nfs, &op[i], fh, data->filler.blob3.val);
        i += nfs4_op_close(nfs, &op[i], fh);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_truncate_close_cb, &args,
                                   data) == NULL) {
                /* Not much we can do but leak one fd on the server :( */
                data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
                return;
        }
}

/*
 * data.blob3.val is a 12 byte SETATTR buffer for length+update_mtime
 */
int
nfs4_truncate_async(struct nfs_context *nfs, const char *path, uint64_t length,
                    nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->open_cb      = nfs4_truncate_open_cb;

        data->filler.blob3.val = malloc(12);
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob3.free = free;

        memset(data->filler.blob3.val, 0, 12);
        length = nfs_hton64(length);
        memcpy(data->filler.blob3.val, &length, sizeof(uint64_t));

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->nfsi->nfs4_open_call_mutex);
                data->flags |= MUTEX_HELD;
        }
#endif        
        if (nfs4_open_async_internal(nfs, data, O_WRONLY, 0) < 0) {
                return -1;
        }

        return 0;
}

static void
nfs4_fsync_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "FSYNC")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

int
nfs4_fsync_async(struct nfs_context *nfs, struct nfsfh *fh, nfs_cb cb,
                 void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_commit(nfs, &op[i]);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_fsync_cb, &args,
                                   data) == NULL) {
                data->filler.blob0.val = NULL;
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_ftruncate_async(struct nfs_context *nfs, struct nfsfh *fh,
                     uint64_t length, nfs_cb cb, void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob3.val = calloc(1, 12);
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob3.free = free;

        length = nfs_hton64(length);
        memcpy(data->filler.blob3.val, &length, sizeof(uint64_t));
        
        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_truncate(nfs, &op[i], fh, data->filler.blob3.val);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_fsync_cb, &args,
                                   data) == NULL) {
                data->filler.blob0.val = NULL;
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_lseek_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok = NULL;
        struct nfsfh *fh = data->filler.blob0.val;
        struct nfs_stat_64 st;
        int64_t offset;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        memcpy(&offset, data->filler.blob1.val, sizeof(int64_t));
        
        if (check_nfs4_error(nfs, status, data, res, "LSEEK")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;

        memset(&st, 0, sizeof(st));
        nfs_parse_attributes(nfs, data, &st,
                             garesok->obj_attributes.attr_vals.attrlist4_val,
                             garesok->obj_attributes.attr_vals.attrlist4_len);

	if (offset < 0 &&
	    -offset > (int64_t)st.nfs_size) {
                nfs_set_error(nfs, "Negative offset for lseek("
                              "SEET_END)");
		data->cb(-EINVAL, nfs, &fh->offset,
                         data->private_data);
	} else {
		fh->offset = offset + st.nfs_size;
		data->cb(0, nfs, &fh->offset, data->private_data);
	}

        free_nfs4_cb_data(data);
}

/* blob0.val is nfsfh
 * blob1.val is offset
 */
int
nfs4_lseek_async(struct nfs_context *nfs, struct nfsfh *fh, int64_t offset,
                 int whence, nfs_cb cb, void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

	if (whence == SEEK_SET) {
		if (offset < 0) {
                        nfs_set_error(nfs, "Negative offset for lseek("
                                      "SEET_SET)");
			cb(-EINVAL, nfs, &fh->offset, private_data);
		} else {
			fh->offset = offset;
			cb(0, nfs, &fh->offset, private_data);
		}
		return 0;
	}
	if (whence == SEEK_CUR) {
		if (offset < 0 &&
		    fh->offset < (uint64_t)(-offset)) {
                        nfs_set_error(nfs, "Negative offset for lseek("
                                      "SEET_CUR)");
			cb(-EINVAL, nfs, &fh->offset, private_data);
		} else {
			fh->offset += offset;
			cb(0, nfs, &fh->offset, private_data);
		}
		return 0;
	}

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = fh;
        data->filler.blob0.free = NULL;

        data->filler.blob1.val = malloc(sizeof(uint64_t));
        if (data->filler.blob1.val == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                free_nfs4_cb_data(data);
                return -1;
        }
        memcpy(data->filler.blob1.val, &offset, sizeof(uint64_t));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_getattr(nfs, &op[i], standard_attributes, 2);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_lseek_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_lockf_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        LOCK4resok *lresok = NULL;
        LOCKU4res *lures = NULL;
        struct nfsfh *fh = data->filler.blob0.val;
        enum nfs4_lock_op cmd;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        cmd = data->filler.blob1.len;

        if (check_nfs4_error(nfs, status, data, res, "LOCKF")) {
                return;
        }

        switch (cmd) {
        case NFS4_F_LOCK:
        case NFS4_F_TLOCK:
                if ((i = nfs4_find_op(nfs, data, res, OP_LOCK, "LOCK")) < 0) {
                        return;
                }

                lresok = &res->resarray.resarray_val[i].nfs_resop4_u.oplock.LOCK4res_u.resok4;
                nfs->nfsi->has_lock_owner = 1;
                fh->lock_stateid.seqid = lresok->lock_stateid.seqid;
                memcpy(fh->lock_stateid.other, lresok->lock_stateid.other, 12);
                break;
        case NFS4_F_ULOCK:
                if ((i = nfs4_find_op(nfs, data, res, OP_LOCKU, "LOCKU")) < 0) {
                        return;
                }
                lures = &res->resarray.resarray_val[i].nfs_resop4_u.oplocku;
                fh->lock_stateid.seqid = lures->LOCKU4res_u.lock_stateid.seqid;
                memcpy(fh->lock_stateid.other,
                       lures->LOCKU4res_u.lock_stateid.other, 12);
                break;
        case NFS4_F_TEST:
                break;
        }

        data->cb(0, nfs, NULL, data->private_data);

        free_nfs4_cb_data(data);
}

/* blob0.val is nfsfh
 * blob1.len is cmd
 */
int
nfs4_lockf_async(struct nfs_context *nfs, struct nfsfh *fh,
                     enum nfs4_lock_op cmd, uint64_t count,
                     nfs_cb cb, void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = fh;
        data->filler.blob0.free = NULL;

        data->filler.blob1.len = cmd;

        i = nfs4_op_putfh(nfs, &op[0], fh);
        switch (cmd) {
        case NFS4_F_LOCK:
                i += nfs4_op_lock(nfs, &op[i], fh, OP_LOCK, WRITEW_LT,
                                  0, fh->offset, count);
                break;
        case NFS4_F_TLOCK:
                i += nfs4_op_lock(nfs, &op[i], fh, OP_LOCK, WRITE_LT,
                                  0, fh->offset, count);
                break;
        case NFS4_F_ULOCK:
                i += nfs4_op_locku(nfs, &op[i], fh, WRITE_LT,
                                   fh->offset, count);
                break;
        case NFS4_F_TEST:
                i += nfs4_op_lockt(nfs, &op[i], fh, WRITEW_LT,
                                   fh->offset, count);
                break;
        }

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_lockf_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_fcntl_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        LOCK4resok *lresok = NULL;
        struct nfsfh *fh = data->filler.blob0.val;
        enum nfs4_fcntl_op cmd;
        struct nfs4_flock *fl;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        cmd = data->filler.blob1.len;

        if (check_nfs4_error(nfs, status, data, res, "FCNTL")) {
                return;
        }

        switch (cmd) {
        case NFS4_F_SETLK:
        case NFS4_F_SETLKW:
                fl = (struct nfs4_flock *)data->filler.blob1.val;

                switch (fl->l_type) {
                case F_RDLCK:
                case F_WRLCK:
                        if ((i = nfs4_find_op(nfs, data, res, OP_LOCK,
                                              "LOCK")) < 0) {
                                return;
                        }

                        lresok = &res->resarray.resarray_val[i].nfs_resop4_u.oplock.LOCK4res_u.resok4;
                        nfs->nfsi->has_lock_owner = 1;
                        fh->lock_stateid.seqid = lresok->lock_stateid.seqid;
                        memcpy(fh->lock_stateid.other,
                               lresok->lock_stateid.other, 12);
                        break;
                case F_UNLCK:
                        if ((i = nfs4_find_op(nfs, data, res, OP_LOCKU,
                                              "UNLOCK")) < 0) {
                                return;
                        }
                        break;
                }
                break;
        }

        data->cb(0, nfs, NULL, data->private_data);

        free_nfs4_cb_data(data);
}

static int
nfs4_fcntl_async_internal(struct nfs_context *nfs, struct nfsfh *fh,
                          struct nfs4_cb_data *data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_flock *fl;
        enum nfs4_fcntl_op cmd;
        int i, lock_type;

        cmd = data->filler.blob1.len;

        i = nfs4_op_putfh(nfs, &op[0], fh);
        switch (cmd) {
        case NFS4_F_SETLK:
        case NFS4_F_SETLKW:
                fl = data->filler.blob1.val;

                switch (fl->l_type) {
                case F_RDLCK:
                        lock_type = cmd == NFS4_F_SETLK ? READ_LT : READW_LT;
                        i += nfs4_op_lock(nfs, &op[i], fh, OP_LOCK, lock_type,
                                          0, fl->l_start, fl->l_len);
                        break;
                case F_WRLCK:
                        lock_type = cmd == NFS4_F_SETLK ? WRITE_LT : WRITEW_LT;
                        i += nfs4_op_lock(nfs, &op[i], fh, OP_LOCK, lock_type,
                                          0, fl->l_start, fl->l_len);
                        break;
                case F_UNLCK:
                        i += nfs4_op_locku(nfs, &op[i], fh, WRITE_LT,
                                           fl->l_start, fl->l_len);
                        break;
                }
                break;
        }

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_fcntl_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_fcntl_stat_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        struct nfsfh *fh = data->filler.blob0.val;
        enum nfs4_fcntl_op cmd = data->filler.blob1.len;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok;
        struct nfs4_flock *fl;
        struct nfs_stat_64 st;
        int i;

        if (check_nfs4_error(nfs, status, data, res, "STAT64")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;
        memset(&st, 0, sizeof(st));
        if (nfs_parse_attributes(nfs, data, &st,
                                 garesok->obj_attributes.attr_vals.attrlist4_val,
                                 garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                free_nfs4_cb_data(data);
        }

        switch (cmd) {
        case NFS4_F_SETLK:
        case NFS4_F_SETLKW:
                fl = data->filler.blob1.val;

                fl->l_whence = SEEK_SET;
                fl->l_start = st.nfs_size + fl->l_start;
                if (nfs4_fcntl_async_internal(nfs, fh, data)) {
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs4_cb_data(data);
                }
        }
}

/* blob0.val is nfsfh
 * blob1.len is cmd
 * blob1.val is arg
 */
int
nfs4_fcntl_async(struct nfs_context *nfs, struct nfsfh *fh,
                 enum nfs4_fcntl_op cmd, void *arg,
                 nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        struct nfs4_flock *fl;
        COMPOUND4args args;
        nfs_argop4 op[2];
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob0.val  = fh;
        data->filler.blob0.free = NULL;

        data->filler.blob1.len = cmd;
        data->filler.blob1.val = arg;
        data->filler.blob1.free = NULL;

        switch (cmd) {
        case NFS4_F_SETLK:
        case NFS4_F_SETLKW:
                fl = arg;
                switch (fl->l_whence) {
                case SEEK_SET:
                        return nfs4_fcntl_async_internal(nfs, fh, data);
                case SEEK_CUR:
                        fl->l_whence = SEEK_SET;
                        fl->l_start = fh->offset + fl->l_start;
                        return nfs4_fcntl_async_internal(nfs, fh, data);
                case SEEK_END:
                        i = nfs4_op_putfh(nfs, &op[0], fh);
                        i += nfs4_op_getattr(nfs, &op[i], standard_attributes,
                                             2);

                        memset(&args, 0, sizeof(args));
                        args.argarray.argarray_len = i;
                        args.argarray.argarray_val = op;

                        if (rpc_nfs4_compound_task(nfs->rpc,
                                                   nfs4_fcntl_stat_cb,
                                                   &args, data) == NULL) {
                                free_nfs4_cb_data(data);
                                return -1;
                        }
                        return 0;
                }
                nfs_set_error(nfs, "fcntl: unknown fl->whence:%d\n",
                              fl->l_whence);
                free_nfs4_cb_data(data);
                return -1;
        }
        nfs_set_error(nfs, "fcntl: unknown cmd:%d\n", cmd);
        free_nfs4_cb_data(data);
        return -1;
}

static int
nfs_parse_statvfs(struct nfs_context *nfs, struct nfs4_cb_data *data,
                  struct statvfs *svfs, const char *buf, int len)
{
        uint64_t u64;
        uint32_t u32;

	svfs->f_bsize   = NFS_BLKSIZE;
	svfs->f_frsize  = NFS_BLKSIZE;

#if !defined(__ANDROID__)
	svfs->f_flag    = 0;
#endif

        /* FSID
         * NFSv4 FSID is 2*64 bit but statvfs fsid is just an
         * unsigmed long. Mix the 2*64 bits and hope for the best.
         */
        CHECK_GETATTR_BUF_SPACE(len, 16);
        memcpy(&u64, buf, 8);
	svfs->f_fsid = (unsigned long)nfs_ntoh64(u64);
        buf += 8;
        len -= 8;
        memcpy(&u64, buf, 8);
	svfs->f_fsid |= (unsigned long)nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Files Avail */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
#if !defined(__ANDROID__)
	svfs->f_favail  = (fsfilcnt_t)nfs_ntoh64(u64);
#endif
        buf += 8;
        len -= 8;

        /* Files Free */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs->f_ffree  = (fsfilcnt_t)nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Files Total */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs->f_files  = (fsfilcnt_t)nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Max Name */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        memcpy(&u32, buf, 4);
#if !defined(__ANDROID__)
	svfs->f_namemax  = ntohl(u32);
#endif
        buf += 4;
        len -= 4;

        /* Space Avail */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs->f_bavail  = (fsblkcnt_t)(nfs_ntoh64(u64) / svfs->f_frsize);
        buf += 8;
        len -= 8;

        /* Space Free */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs->f_bfree  = (fsblkcnt_t)(nfs_ntoh64(u64) / svfs->f_frsize);
        buf += 8;
        len -= 8;

        /* Space Total */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs->f_blocks  = (fsblkcnt_t)(nfs_ntoh64(u64) / svfs->f_frsize);
        buf += 8;
        len -= 8;

        return 0;
}

static int
nfs_parse_statvfs64(struct nfs_context *nfs, struct nfs4_cb_data *data,
                    struct nfs_statvfs_64 *svfs64, const char *buf, int len)
{
        uint64_t u64;
        uint32_t u32;

	svfs64->f_bsize   = NFS_BLKSIZE;
	svfs64->f_frsize  = NFS_BLKSIZE;
	svfs64->f_flag    = 0;

        /* FSID
         * NFSv4 FSID is 2*64 bit but statvfs fsid is just an
         * unsigmed long. Mix the 2*64 bits and hope for the best.
         */
        CHECK_GETATTR_BUF_SPACE(len, 16);
        memcpy(&u64, buf, 8);
	svfs64->f_fsid = nfs_ntoh64(u64);
        buf += 8;
        len -= 8;
        memcpy(&u64, buf, 8);
	svfs64->f_fsid |= nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Files Avail */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_favail = nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Files Free */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_ffree = nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Files Total */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_files = nfs_ntoh64(u64);
        buf += 8;
        len -= 8;

        /* Max Name */
        CHECK_GETATTR_BUF_SPACE(len, 4);
        memcpy(&u32, buf, 4);
	svfs64->f_namemax = ntohl(u32);
        buf += 4;
        len -= 4;

        /* Space Avail */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_bavail = nfs_ntoh64(u64) / svfs64->f_frsize;
        buf += 8;
        len -= 8;

        /* Space Free */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_bfree = nfs_ntoh64(u64) / svfs64->f_frsize;
        buf += 8;
        len -= 8;

        /* Space Total */
        CHECK_GETATTR_BUF_SPACE(len, 8);
        memcpy(&u64, buf, 8);
	svfs64->f_blocks = nfs_ntoh64(u64) / svfs64->f_frsize;
        buf += 8;
        len -= 8;

        return 0;
}

static void
nfs4_statvfs_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        GETATTR4resok *garesok;
	struct statvfs svfs;
	struct nfs_statvfs_64 svfs64;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "STATVFS")) {
                return;
        }

        memset(&svfs, 0, sizeof(svfs));
        memset(&svfs64, 0, sizeof(svfs64));

        if ((i = nfs4_find_op(nfs, data, res, OP_GETATTR, "GETATTR")) < 0) {
                return;
        }
        garesok = &res->resarray.resarray_val[i].nfs_resop4_u.opgetattr.GETATTR4res_u.resok4;

        if (data->flags & LOOKUP_FLAG_IS_STATVFS64) {
                /* statvfs64 */
                if (nfs_parse_statvfs64(nfs, data, &svfs64,
                              garesok->obj_attributes.attr_vals.attrlist4_val,
                              garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                        data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                        free_nfs4_cb_data(data);
                        return;
                }

                data->cb(0, nfs, &svfs64, data->private_data);
        } else {
                /* statvfs */
                if (nfs_parse_statvfs(nfs, data, &svfs,
                              garesok->obj_attributes.attr_vals.attrlist4_val,
                              garesok->obj_attributes.attr_vals.attrlist4_len) < 0) {
                        data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
                        free_nfs4_cb_data(data);
                        return;
                }

                data->cb(0, nfs, &svfs, data->private_data);
        }

	free_nfs4_cb_data(data);
}

static int
nfs4_statvfs_async_internal(struct nfs_context *nfs, const char *path,
                            int is_statvfs64,
                            nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        COMPOUND4args args;
        struct nfsfh fh;
        nfs_argop4 op[2];
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;
        if (is_statvfs64) {
                data->flags |= LOOKUP_FLAG_IS_STATVFS64;
        }

        fh.fh.len = nfs->nfsi->rootfh.len;
        fh.fh.val = nfs->nfsi->rootfh.val;

        i = nfs4_op_putfh(nfs, &op[0], &fh);
        i += nfs4_op_getattr(nfs, &op[i], statvfs_attributes, 2);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_statvfs_cb, &args,
                                   data) == NULL) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_statvfs_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
        return nfs4_statvfs_async_internal(nfs, path, 0,
                                           cb, private_data);
}

int
nfs4_statvfs64_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                     void *private_data)
{
        return nfs4_statvfs_async_internal(nfs, path, 1,
                                           cb, private_data);
}

static void
nfs4_chmod_cb(struct rpc_context *rpc, int status, void *command_data,
                   void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        if (check_nfs4_error(nfs, status, data, res, "CHMOD")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_chmod(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        return nfs4_op_chmod(data->nfs, op, NULL, data->filler.blob3.val);
}

int
nfs4_chmod_async_internal(struct nfs_context *nfs, const char *path,
                          int no_follow, int mode, nfs_cb cb,
                          void *private_data)
{
        struct nfs4_cb_data *data;
        uint32_t m;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_chmod;
        data->filler.max_op = 1;

        if (no_follow) {
                data->flags |= LOOKUP_FLAG_NO_FOLLOW;
        }

        data->filler.blob3.val = malloc(sizeof(uint32_t));
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob3.free = free;

        m = htonl(mode);
        memcpy(data->filler.blob3.val, &m, sizeof(uint32_t));

        if (nfs4_lookup_path_async(nfs, data, nfs4_chmod_cb) < 0) {
                return -1;
        }

        return 0;
}

int
nfs4_fchmod_async(struct nfs_context *nfs, struct nfsfh *fh, int mode,
                  nfs_cb cb, void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        uint32_t m;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        data->filler.blob3.val = malloc(sizeof(uint32_t));
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                free_nfs4_cb_data(data);
                return -1;
        }
        data->filler.blob3.free = free;

        m = htonl(mode);
        memcpy(data->filler.blob3.val, &m, sizeof(uint32_t));
        
        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_chmod(nfs, &op[i], fh, data->filler.blob3.val);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_fsync_cb, &args,
                                   data) == NULL) {
                data->filler.blob0.val = NULL;
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

#define CHOWN_BLOB_SIZE 64
static int
nfs4_create_chown_buffer(struct nfs_context *nfs, struct nfs4_cb_data *data,
                         int uid, int gid)
{
        char *str;
        int i, l;
        uint32_t len;

        data->filler.blob3.val = calloc(1, CHOWN_BLOB_SIZE);
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                return -1;
        }
        data->filler.blob3.free = free;
        
        i = 0;
        str = data->filler.blob3.val;
        /* UID */
        l = snprintf(&str[i + 4], CHOWN_BLOB_SIZE - 4 - i,
                     "%d", uid);
        if (l < 0) {
                nfs_set_error(nfs, "snprintf failed");
                return -1;
        }
        len = htonl(l);
        /* UID length prefix */
        memcpy(&str[i], &len, sizeof(uint32_t));
        i += 4 + l;
        i = (i + 3) & ~0x03;

        /* GID */
        l = snprintf(&str[i + 4], CHOWN_BLOB_SIZE - 4 - i,
                     "%d", gid);
        if (l < 0) {
                nfs_set_error(nfs, "snprintf failed");
                return -1;
        }
        len = htonl(l);
        /* GID length prefix */
        memcpy(&str[i], &len, sizeof(uint32_t));
        i += 4 + l;
        i = (i + 3) & ~0x03;

        data->filler.blob3.len = i;

        return 0;
}

static void
nfs4_chown_cb(struct rpc_context *rpc, int status, void *command_data,
                   void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        if (check_nfs4_error(nfs, status, data, res, "OPEN")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_chown(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        return nfs4_op_chown(data->nfs, op, NULL, data->filler.blob3.val,
                           data->filler.blob3.len);
}

int
nfs4_chown_async_internal(struct nfs_context *nfs, const char *path,
                          int no_follow, int uid, int gid,
                          nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;

        data = init_cb_data_split_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_chown;
        data->filler.max_op = 1;

        if (no_follow) {
                data->flags |= LOOKUP_FLAG_NO_FOLLOW;
        }

        if (nfs4_create_chown_buffer(nfs, data, uid, gid) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        if (nfs4_lookup_path_async(nfs, data, nfs4_chown_cb) < 0) {
                return -1;
        }

        return 0;
}

int
nfs4_fchown_async(struct nfs_context *nfs, struct nfsfh *fh, int uid, int gid,
                  nfs_cb cb, void *private_data)
{
        COMPOUND4args args;
        nfs_argop4 op[2];
        struct nfs4_cb_data *data;
        int i;

        data = calloc(1, sizeof(*data));
        if (data == NULL) {
                nfs_set_error(nfs, "Out of memory.");
                return -1;
        }

        data->nfs          = nfs;
        data->cb           = cb;
        data->private_data = private_data;

        if (nfs4_create_chown_buffer(nfs, data, uid, gid) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }
        
        memset(op, 0, sizeof(op));

        i = nfs4_op_putfh(nfs, &op[0], fh);
        i += nfs4_op_chown(nfs, &op[i], fh, data->filler.blob3.val,
                           data->filler.blob3.len);

        memset(&args, 0, sizeof(args));
        args.argarray.argarray_len = i;
        args.argarray.argarray_val = op;

        if (rpc_nfs4_compound_task(nfs->rpc, nfs4_fsync_cb, &args,
                                   data) == NULL) {
                data->filler.blob0.val = NULL;
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

static void
nfs4_access_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;
        ACCESS4resok *aresok;
        int i;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "ACCESS")) {
                return;
        }

        if ((i = nfs4_find_op(nfs, data, res, OP_ACCESS, "ACCESS")) < 0) {
                return;
        }

        aresok = &res->resarray.resarray_val[i].nfs_resop4_u.opaccess.ACCESS4res_u.resok4;

        /* access2 */
        if (data->filler.flags) {
                int mode = 0;

                if (aresok->access & ACCESS4_READ) {
                        mode |= R_OK;
                }
                if (aresok->access & ACCESS4_MODIFY) {
                        mode |= W_OK;
                }
                if (aresok->access & ACCESS4_EXECUTE) {
                        mode |= X_OK;
                }
                data->cb(mode, nfs, NULL, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        if (aresok->supported != aresok->access) {
                data->cb(-EACCES, nfs, NULL, data->private_data);
                free_nfs4_cb_data(data);
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_access_internal(struct nfs_context *nfs, const char *path, int mode,
                     int is_access2, nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        uint32_t m;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb            = cb;
        data->private_data  = private_data;
        data->filler.func   = nfs4_populate_access;
        data->filler.max_op = 1;
        data->filler.flags = is_access2;

        data->filler.blob3.val = malloc(sizeof(uint32_t));
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                return -1;
        }
        data->filler.blob3.free = free;

        m = 0;
        if (mode & R_OK) {
                m |= ACCESS4_READ;
        }
        if (mode & W_OK) {
                m |= ACCESS4_MODIFY;
        }
        if (mode & X_OK) {
                m |= ACCESS4_EXECUTE;
        }
        memcpy(data->filler.blob3.val, &m, sizeof(uint32_t));

        if (nfs4_lookup_path_async(nfs, data, nfs4_access_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_access_async(struct nfs_context *nfs, const char *path, int mode,
                  nfs_cb cb, void *private_data)
{
        return nfs4_access_internal(nfs, path, mode, 0,
                                    cb, private_data);
}

int
nfs4_access2_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
        return nfs4_access_internal(nfs, path, R_OK|W_OK|X_OK, 1,
                                    cb, private_data);
}

static void
nfs4_utimes_cb(struct rpc_context *rpc, int status, void *command_data,
                   void *private_data)
{
        struct nfs4_cb_data *data = private_data;
        struct nfs_context *nfs = data->nfs;
        COMPOUND4res *res = command_data;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (check_nfs4_error(nfs, status, data, res, "UTIMES")) {
                return;
        }

        data->cb(0, nfs, NULL, data->private_data);
        free_nfs4_cb_data(data);
}

static int
nfs4_populate_utimes(struct nfs4_cb_data *data, nfs_argop4 *op)
{
        return nfs4_op_utimes(data->nfs, op, NULL, data->filler.blob3.val,
                            data->filler.blob3.len);
}

int
nfs4_utimes_async_internal(struct nfs_context *nfs, const char *path,
                           int no_follow, struct timeval *times,
                           nfs_cb cb, void *private_data)
{
        struct nfs4_cb_data *data;
        char *buf;
        uint32_t u32;
        uint64_t u64;

        data = init_cb_data_full_path(nfs, path);
        if (data == NULL) {
                return -1;
        }

        data->cb           = cb;
        data->private_data = private_data;
        data->filler.func = nfs4_populate_utimes;
        data->filler.max_op = 1;

        if (no_follow) {
                data->flags |= LOOKUP_FLAG_NO_FOLLOW;
        }

        data->filler.blob3.len = 2 * (4 + 8 + 4);
        buf = data->filler.blob3.val = calloc(1, data->filler.blob3.len);
        if (data->filler.blob3.val == NULL) {
                nfs_set_error(nfs, "Out of memory");
                return -1;
        }
        data->filler.blob3.free = free;

        if (times != NULL) {
                /* atime */
                u32 = htonl(SET_TO_CLIENT_TIME4);
                memcpy(buf, &u32, sizeof(uint32_t));
                u64 = nfs_hton64(times[0].tv_sec);
                memcpy(buf + 4, &u64, sizeof(uint64_t));
                u32 = htonl(times[0].tv_usec * 1000);
                memcpy(buf + 12, &u32, sizeof(uint32_t));
                buf += 16;
                /* mtime */
                u32 = htonl(SET_TO_CLIENT_TIME4);
                memcpy(buf, &u32, sizeof(uint32_t));
                u64 = nfs_hton64(times[1].tv_sec);
                memcpy(buf + 4, &u64, sizeof(uint64_t));
                u32 = htonl(times[1].tv_usec * 1000);
                memcpy(buf + 12, &u32, sizeof(uint32_t));
        } else {
                /* atime */
                u32 = htonl(SET_TO_SERVER_TIME4);
                memcpy(buf, &u32, sizeof(uint32_t));
                /* mtime */
                u32 = htonl(SET_TO_SERVER_TIME4);
                memcpy(buf + 4, &u32, sizeof(uint32_t));
                data->filler.blob3.len = 8;
        }

        if (nfs4_lookup_path_async(nfs, data, nfs4_utimes_cb) < 0) {
                free_nfs4_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs4_utime_async(struct nfs_context *nfs, const char *path,
                 struct utimbuf *times, nfs_cb cb, void *private_data)
{
        if (times == NULL) {
                return nfs4_utimes_async_internal(nfs, path, 0, NULL,
                                          cb, private_data);
        }
	struct timeval new_times[2];

        new_times[0].tv_sec  = times->actime;
        new_times[0].tv_usec = 0;
        new_times[1].tv_sec  = times->modtime;
        new_times[1].tv_usec = 0;

        return nfs4_utimes_async_internal(nfs, path, 0, new_times,
                                          cb, private_data);
}
