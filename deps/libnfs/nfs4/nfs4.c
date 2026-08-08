/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PS2_EE
#include "ps2_compat.h"
#endif

#ifdef WIN32
#include <win32/win32_compat.h>
#else
#include <sys/stat.h>
#endif/*WIN32*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "libnfs-raw-nfs4.h"

char *
nfsstat4_to_str(int error)
{
	switch (error) {
        case NFS4_OK: return "NFS4_OK"; break;
        case NFS4ERR_PERM: return "NFS4ERR_PERM"; break;
        case NFS4ERR_NOENT: return "NFS4ERR_NOENT"; break;
        case NFS4ERR_IO: return "NFS4ERR_IO"; break;
        case NFS4ERR_NXIO: return "NFS4ERR_NXIO"; break;
        case NFS4ERR_ACCESS: return "NFS4ERR_ACCESS"; break;
        case NFS4ERR_EXIST: return "NFS4ERR_EXIST"; break;
        case NFS4ERR_XDEV: return "NFS4ERR_XDEV"; break;
        case NFS4ERR_NOTDIR: return "NFS4ERR_NOTDIR"; break;
        case NFS4ERR_ISDIR: return "NFS4ERR_ISDIR"; break;
        case NFS4ERR_INVAL: return "NFS4ERR_INVAL"; break;
        case NFS4ERR_FBIG: return "NFS4ERR_FBIG"; break;
        case NFS4ERR_NOSPC: return "NFS4ERR_NOSPC"; break;
        case NFS4ERR_ROFS: return"NFS4ERR_ROFS"; break;
        case NFS4ERR_MLINK: return "NFS4ERR_MLINK"; break;
        case NFS4ERR_NAMETOOLONG: return "NFS4ERR_NAMETOOLONG"; break;
        case NFS4ERR_NOTEMPTY: return "NFS4ERR_NOTEMPTY"; break;
        case NFS4ERR_DQUOT: return "NFS4ERR_DQUOT"; break;
        case NFS4ERR_STALE: return "NFS4ERR_STALE"; break;
        case NFS4ERR_BADHANDLE: return "NFS4ERR_BADHANDLE"; break;
        case NFS4ERR_BAD_COOKIE: return "NFS4ERR_BAD_COOKIE"; break;
        case NFS4ERR_NOTSUPP: return "NFS4ERR_NOTSUPP"; break;
        case NFS4ERR_TOOSMALL: return "NFS4ERR_TOOSMALL"; break;
        case NFS4ERR_SERVERFAULT: return "NFS4ERR_SERVERFAULT"; break;
        case NFS4ERR_BADTYPE: return "NFS4ERR_BADTYPE"; break;
        case NFS4ERR_DELAY: return "NFS4ERR_DELAY"; break;
        case NFS4ERR_SAME: return "NFS4ERR_SAME"; break;
        case NFS4ERR_DENIED: return "NFS4ERR_DENIED"; break;
        case NFS4ERR_EXPIRED: return "NFS4ERR_EXPIRED"; break;
        case NFS4ERR_LOCKED: return "NFS4ERR_LOCKED"; break;
        case NFS4ERR_GRACE: return "NFS4ERR_GRACE"; break;
        case NFS4ERR_FHEXPIRED: return "NFS4ERR_FHEXPIRED"; break;
        case NFS4ERR_SHARE_DENIED: return "NFS4ERR_SHARE_DENIED"; break;
        case NFS4ERR_WRONGSEC: return "NFS4ERR_WRONGSEC"; break;
        case NFS4ERR_CLID_INUSE: return "NFS4ERR_CLID_INUSE"; break;
        case NFS4ERR_RESOURCE: return "NFS4ERR_RESOURCE"; break;
        case NFS4ERR_MOVED: return "NFS4ERR_MOVED"; break;
        case NFS4ERR_NOFILEHANDLE: return "NFS4ERR_NOFILEHANDLE"; break;
        case NFS4ERR_MINOR_VERS_MISMATCH:
                return "NFS4ERR_MINOR_VERS_MISMATCH"; break;
        case NFS4ERR_STALE_CLIENTID: return "NFS4ERR_STALE_CLIENTID"; break;
        case NFS4ERR_STALE_STATEID: return "NFS4ERR_STALE_STATEID"; break;
        case NFS4ERR_OLD_STATEID: return "NFS4ERR_OLD_STATEID"; break;
        case NFS4ERR_BAD_STATEID: return "NFS4ERR_BAD_STATEID"; break;
        case NFS4ERR_BAD_SEQID: return "NFS4ERR_BAD_SEQID"; break;
        case NFS4ERR_NOT_SAME: return "NFS4ERR_NOT_SAME"; break;
        case NFS4ERR_LOCK_RANGE: return "NFS4ERR_LOCK_RANGE"; break;
        case NFS4ERR_SYMLINK: return "NFS4ERR_SYMLINK"; break;
        case NFS4ERR_RESTOREFH: return "NFS4ERR_RESTOREFH"; break;
        case NFS4ERR_LEASE_MOVED: return "NFS4ERR_LEASE_MOVED"; break;
        case NFS4ERR_ATTRNOTSUPP: return "NFS4ERR_ATTRNOTSUPP"; break;
        case NFS4ERR_NO_GRACE: return "NFS4ERR_NO_GRACE"; break;
        case NFS4ERR_RECLAIM_BAD: return "NFS4ERR_RECLAIM_BAD"; break;
        case NFS4ERR_RECLAIM_CONFLICT: return "NFS4ERR_RECLAIM_CONFLICT"; break;
        case NFS4ERR_BADZDR: return "NFS4ERR_BADZDR"; break;
        case NFS4ERR_LOCKS_HELD: return "NFS4ERR_LOCKS_HELD"; break;
        case NFS4ERR_OPENMODE: return "NFS4ERR_OPENMODE"; break;
        case NFS4ERR_BADOWNER: return "NFS4ERR_BADOWNER"; break;
        case NFS4ERR_BADCHAR: return "NFS4ERR_BADCHAR"; break;
        case NFS4ERR_BADNAME: return "NFS4ERR_BADNAME"; break;
        case NFS4ERR_BAD_RANGE: return "NFS4ERR_BAD_RANGE"; break;
        case NFS4ERR_LOCK_NOTSUPP: return "NFS4ERR_LOCK_NOTSUPP"; break;
        case NFS4ERR_OP_ILLEGAL: return "NFS4ERR_OP_ILLEGAL"; break;
        case NFS4ERR_DEADLOCK: return "NFS4ERR_DEADLOCK"; break;
        case NFS4ERR_FILE_OPEN: return "NFS4ERR_FILE_OPEN"; break;
        case NFS4ERR_ADMIN_REVOKED: return "NFS4ERR_ADMIN_REVOKED"; break;
        case NFS4ERR_CB_PATH_DOWN: return "NFS4ERR_CB_PATH_DOWN"; break;
	};
	return "unknown nfsv4 error";
}

int
nfsstat4_to_errno(int error)
{
	switch (error) {
        case NFS4_OK: return 0;
        case NFS4ERR_PERM: return -EPERM;
        case NFS4ERR_NOENT: return -ENOENT ;
        case NFS4ERR_IO: return -EIO;
        case NFS4ERR_NXIO: return -ENXIO;
        case NFS4ERR_ACCESS: return -EACCES ;
        case NFS4ERR_EXIST: return -EEXIST;
        case NFS4ERR_XDEV: return -EXDEV;
        case NFS4ERR_NOTDIR: return -ENOTDIR ;
        case NFS4ERR_ISDIR: return -EISDIR ;
        case NFS4ERR_INVAL: return -EINVAL;
        case NFS4ERR_FBIG: return -EFBIG;
        case NFS4ERR_NOSPC: return -ENOSPC;
        case NFS4ERR_ROFS: return -EROFS;
        case NFS4ERR_MLINK: return -EMLINK;
        case NFS4ERR_NAMETOOLONG: return -ENAMETOOLONG;
        case NFS4ERR_NOTEMPTY: return -ENOTEMPTY;
        case NFS4ERR_DQUOT: return -ERANGE;
        case NFS4ERR_STALE: return -EIO;
        case NFS4ERR_BADHANDLE: return -EINVAL;
        case NFS4ERR_BAD_COOKIE: return -EINVAL;
        case NFS4ERR_NOTSUPP: return -EINVAL;
        case NFS4ERR_TOOSMALL: return -EIO;
        case NFS4ERR_SERVERFAULT: return -EIO;
        case NFS4ERR_BADTYPE: return -EINVAL;
        case NFS4ERR_DELAY: return -EIO;
        case NFS4ERR_SAME: return -EIO;
        case NFS4ERR_DENIED: return -EIO;
        case NFS4ERR_EXPIRED: return -EAGAIN;
        case NFS4ERR_LOCKED: return -EIO;
        case NFS4ERR_GRACE: return -EIO;
        case NFS4ERR_FHEXPIRED: return -EAGAIN;
        case NFS4ERR_SHARE_DENIED: return -EIO;
        case NFS4ERR_WRONGSEC: return -EIO;
        case NFS4ERR_CLID_INUSE: return -EIO;
        case NFS4ERR_RESOURCE: return -EIO;
        case NFS4ERR_MOVED: return -EIO;
        case NFS4ERR_NOFILEHANDLE: return -EIO;
        case NFS4ERR_MINOR_VERS_MISMATCH: return -EIO;
        case NFS4ERR_STALE_CLIENTID: return -EIO;
        case NFS4ERR_STALE_STATEID: return -EIO;
        case NFS4ERR_OLD_STATEID: return -EIO;
        case NFS4ERR_BAD_STATEID: return -EINVAL;
        case NFS4ERR_BAD_SEQID: return -EINVAL;
        case NFS4ERR_NOT_SAME: return -EIO;
        case NFS4ERR_LOCK_RANGE: return -EIO;
        case NFS4ERR_SYMLINK: return -EIO;
        case NFS4ERR_RESTOREFH: return -EIO;
        case NFS4ERR_ATTRNOTSUPP: return -EINVAL;
        case NFS4ERR_NO_GRACE: return -EIO;
        case NFS4ERR_RECLAIM_BAD: return -EIO;
        case NFS4ERR_RECLAIM_CONFLICT: return -EIO;
        case NFS4ERR_BADZDR: return -EINVAL;
        case NFS4ERR_LOCKS_HELD: return -EIO;
        case NFS4ERR_OPENMODE: return -EIO;
        case NFS4ERR_BADOWNER: return -EINVAL;
        case NFS4ERR_BADCHAR: return -EINVAL;
        case NFS4ERR_BADNAME: return -EINVAL;
        case NFS4ERR_BAD_RANGE: return -EINVAL;
        case NFS4ERR_LOCK_NOTSUPP: return -EINVAL;
        case NFS4ERR_OP_ILLEGAL: return -EIO;
        case NFS4ERR_DEADLOCK: return -EIO;
        case NFS4ERR_FILE_OPEN: return -EIO;
        case NFS4ERR_ADMIN_REVOKED: return -EIO;
        case NFS4ERR_CB_PATH_DOWN: return -EIO;
	};
	return -ERANGE;
}

struct rpc_pdu *rpc_nfs4_null_task(struct rpc_context *rpc, rpc_cb cb,
                                   void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS4_PROGRAM, NFS_V4, NFSPROC4_NULL, cb,
                               private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu "
                              "for NFS4/NULL call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for "
                              "NFS4/NULL call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs4_compound_task2(struct rpc_context *rpc, rpc_cb cb,
                                        struct COMPOUND4args *args,
                                        void *private_data,
                                        size_t alloc_hint)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu2(rpc, NFS4_PROGRAM, NFS_V4, NFSPROC4_COMPOUND,
                               cb, private_data, (zdrproc_t)zdr_COMPOUND4res,
                               sizeof(COMPOUND4res),
                               alloc_hint, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for "
                              "NFS4/COMPOUND call");
		return NULL;
	}

	if (zdr_COMPOUND4args(&pdu->zdr,  args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode COMPOUND4args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for "
                              "NFS4/COMPOUND4 call");
		return NULL;
	}

	return pdu;
}


struct rpc_pdu *rpc_nfs4_compound_task(struct rpc_context *rpc, rpc_cb cb,
                                       struct COMPOUND4args *args,
                                       void *private_data)
{
        return rpc_nfs4_compound_task2(rpc, cb, args, private_data, 0);
}

struct rpc_pdu *rpc_nfs4_readv_task(struct rpc_context *rpc, rpc_cb cb,
                                   const struct iovec *iov, int iovcnt,
                                   struct COMPOUND4args *args,
                                   void *private_data)
{
	struct rpc_pdu *pdu;
	int i;

        if (iovcnt == 0 || iov == NULL) {
		rpc_set_error(rpc, "Invalid arguments: iov and iovcnt must be specified");
		return NULL;
        }

        /*
         * It's disallowed since it's not tested. It may work.
         */
        if (iovcnt > 1 && rpc->is_udp) {
		rpc_set_error(rpc, "Invalid arguments: Vectored read not supported for UDP transport");
		return NULL;
        }

        /*
         * Don't accept more iovecs than what readv() can handle.
         */
        if (iovcnt > RPC_MAX_VECTORS) {
		rpc_set_error(rpc, "Invalid arguments: iovcnt must be <= %d", RPC_MAX_VECTORS);
		return NULL;
        }

	pdu = rpc_allocate_pdu2(rpc, NFS4_PROGRAM, NFS_V4, NFSPROC4_COMPOUND,
                               cb, private_data, (zdrproc_t)zdr_COMPOUND4res,
                                sizeof(COMPOUND4res), 0, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for "
                              "NFS4/COMPOUND call");
		return NULL;
	}

	if (zdr_COMPOUND4args(&pdu->zdr,  args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode COMPOUND4args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	pdu->in.base = (struct iovec *) malloc(sizeof(struct iovec) * iovcnt * 2);
	if (!pdu->in.base) {
		rpc_set_error(rpc, "error: Failed to allocate memory");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

        pdu->in.iov = pdu->in.base;
        pdu->in.iov_ref = pdu->in.base + iovcnt;
	pdu->in.iovcnt = pdu->in.iovcnt_ref = iovcnt;

        for (i = 0; i < iovcnt; i++) {
                pdu->in.iov[i] = pdu->in.iov_ref[i] = iov[i];
                pdu->in.remaining_size += iov[i].iov_len;
        }

        pdu->requested_read_count = pdu->in.remaining_size;
        pdu->zero_copy_iov = 1;

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for "
                              "NFS4/COMPOUND4 call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs4_read_task(struct rpc_context *rpc, rpc_cb cb,
                                   void *buf, size_t count,
                                   struct COMPOUND4args *args,
                                   void *private_data)
{
	struct iovec iov;

	iov.iov_base = buf;
	iov.iov_len = count;

	return rpc_nfs4_readv_task(rpc, cb, &iov, 1, args, private_data);
}

struct rpc_pdu *rpc_nfs4_writev_task(struct rpc_context *rpc, rpc_cb cb,
                                     const struct iovec *iov, int iovcnt,
                                     struct COMPOUND4args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;
        int start;
        uint32_t len;
        static uint32_t zero_padding;
        int i;

        if (iovcnt == 0 || iov == NULL) {
		rpc_set_error(rpc, "Invalid arguments: iov and iovcnt must be specified");
		return NULL;
        }

        if (iovcnt > 1 && rpc->is_udp) {
		rpc_set_error(rpc, "Invalid arguments: Vectored write not supported for UDP transport");
		return NULL;
        }

	pdu = rpc_allocate_pdu2(rpc, NFS4_PROGRAM, NFS_V4, NFSPROC4_COMPOUND,
                                cb, private_data, (zdrproc_t)zdr_COMPOUND4res,
                                sizeof(COMPOUND4res), 0, iovcnt);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for "
                              "NFS4/COMPOUND call");
		return NULL;
	}

        start = zdr_getpos(&pdu->zdr);

	if (zdr_COMPOUND4args(&pdu->zdr,  args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode COMPOUND4args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

        /* Add an iovector for the COMPOUND4/.../WRITE4 header */
        if (rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[start + 4],
                             zdr_getpos(&pdu->zdr) - start, NULL) < 0) {
		rpc_free_pdu(rpc, pdu);
		return NULL;
        }

        /* Calculate data length to encode in the RPC request */
        len = 0;
        for (i = 0; i < iovcnt; i++) {
                len += iov[i].iov_len;
        }

        /* Add an iovector for the length of the byte/array blob */
        start = zdr_getpos(&pdu->zdr);
        zdr_u_int(&pdu->zdr, &len);
        if (rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[start + 4],
                             4, NULL) < 0) {
		rpc_free_pdu(rpc, pdu);
		return NULL;
        }

        /* Add iovector(s) for the data itself */
        for (i = 0; i < iovcnt; i++) {
                if (rpc_add_iovector(rpc, &pdu->out,
                                     iov[i].iov_base,
                                     iov[i].iov_len, NULL) < 0) {
                        rpc_free_pdu(rpc, pdu);
                        return NULL;
                }
        }

        /* We may need to pad this to 4 byte boundary */
        if (len & 0x03) {
                if (rpc_add_iovector(rpc, &pdu->out, (char *)&zero_padding,
                                     4 - (len & 0x03),
                                     NULL) < 0) {
                        rpc_free_pdu(rpc, pdu);
                        return NULL;
                }
        }

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for "
                              "NFS4/COMPOUND4 call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs4_write_task(struct rpc_context *rpc, rpc_cb cb,
                                    const void *buf, size_t count,
                                    struct COMPOUND4args *args,
                                    void *private_data)
{
        /*
         * We don't have an iovec definition with const iov_base pointer, but
         * rpc_nfs4_writev_task() won't modify the contents of *buf, so the
         * following cast is safe.
         */
        struct iovec iov;

        iov.iov_base = (void *) buf;
        iov.iov_len = count;

        return rpc_nfs4_writev_task(rpc, cb, &iov, 1, args, private_data);
}
