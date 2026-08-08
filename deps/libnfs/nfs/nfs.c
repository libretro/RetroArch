/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#include "libnfs-raw-nfs.h"

char *nfsstat3_to_str(int error)
{
	switch (error) {
	case NFS3_OK: return "NFS3_OK"; break;
	case NFS3ERR_PERM: return "NFS3ERR_PERM"; break;
	case NFS3ERR_NOENT: return "NFS3ERR_NOENT"; break;
	case NFS3ERR_IO: return "NFS3ERR_IO"; break;
	case NFS3ERR_NXIO: return "NFS3ERR_NXIO"; break;
	case NFS3ERR_ACCES: return "NFS3ERR_ACCES"; break;
	case NFS3ERR_EXIST: return "NFS3ERR_EXIST"; break;
	case NFS3ERR_XDEV: return "NFS3ERR_XDEV"; break;
	case NFS3ERR_NODEV: return "NFS3ERR_NODEV"; break;
	case NFS3ERR_NOTDIR: return "NFS3ERR_NOTDIR"; break;
	case NFS3ERR_ISDIR: return "NFS3ERR_ISDIR"; break;
	case NFS3ERR_INVAL: return "NFS3ERR_INVAL"; break;
	case NFS3ERR_FBIG: return "NFS3ERR_FBIG"; break;
	case NFS3ERR_NOSPC: return "NFS3ERR_NOSPC"; break;
	case NFS3ERR_ROFS: return "NFS3ERR_ROFS"; break;
	case NFS3ERR_MLINK: return "NFS3ERR_MLINK"; break;
	case NFS3ERR_NAMETOOLONG: return "NFS3ERR_NAMETOOLONG"; break;
	case NFS3ERR_NOTEMPTY: return "NFS3ERR_NOTEMPTY"; break;
	case NFS3ERR_DQUOT: return "NFS3ERR_DQUOT"; break;
	case NFS3ERR_STALE: return "NFS3ERR_STALE"; break;
	case NFS3ERR_REMOTE: return "NFS3ERR_REMOTE"; break;
	case NFS3ERR_BADHANDLE: return "NFS3ERR_BADHANDLE"; break;
	case NFS3ERR_NOT_SYNC: return "NFS3ERR_NOT_SYNC"; break;
	case NFS3ERR_BAD_COOKIE: return "NFS3ERR_BAD_COOKIE"; break;
	case NFS3ERR_NOTSUPP: return "NFS3ERR_NOTSUPP"; break;
	case NFS3ERR_TOOSMALL: return "NFS3ERR_TOOSMALL"; break;
	case NFS3ERR_SERVERFAULT: return "NFS3ERR_SERVERFAULT"; break;
	case NFS3ERR_BADTYPE: return "NFS3ERR_BADTYPE"; break;
	case NFS3ERR_JUKEBOX: return "NFS3ERR_JUKEBOX"; break;
	};
	return "unknown nfs error";
}

int nfsstat3_to_errno(int error)
{
	switch (error) {
	case NFS3_OK:             return 0; break;
	case NFS3ERR_PERM:        return -EPERM; break;
	case NFS3ERR_NOENT:       return -ENOENT; break;
	case NFS3ERR_IO:          return -EIO; break;
	case NFS3ERR_NXIO:        return -ENXIO; break;
	case NFS3ERR_ACCES:       return -EACCES; break;
	case NFS3ERR_EXIST:       return -EEXIST; break;
	case NFS3ERR_XDEV:        return -EXDEV; break;
	case NFS3ERR_NODEV:       return -ENODEV; break;
	case NFS3ERR_NOTDIR:      return -ENOTDIR; break;
	case NFS3ERR_ISDIR:       return -EISDIR; break;
	case NFS3ERR_INVAL:       return -EINVAL; break;
	case NFS3ERR_FBIG:        return -EFBIG; break;
	case NFS3ERR_NOSPC:       return -ENOSPC; break;
	case NFS3ERR_ROFS:        return -EROFS; break;
	case NFS3ERR_MLINK:       return -EMLINK; break;
	case NFS3ERR_NAMETOOLONG: return -ENAMETOOLONG; break;
	case NFS3ERR_NOTEMPTY:    return -ENOTEMPTY; break;
	case NFS3ERR_DQUOT:       return -ERANGE; break;
	case NFS3ERR_STALE:       return -ESTALE; break;
	case NFS3ERR_REMOTE:      return -EIO; break;
	case NFS3ERR_BADHANDLE:   return -EIO; break;
	case NFS3ERR_NOT_SYNC:    return -EIO; break;
	case NFS3ERR_BAD_COOKIE:  return -EIO; break;
	case NFS3ERR_NOTSUPP:     return -EINVAL; break;
	case NFS3ERR_TOOSMALL:    return -EIO; break;
	case NFS3ERR_SERVERFAULT: return -EIO; break;
	case NFS3ERR_BADTYPE:     return -EINVAL; break;
	case NFS3ERR_JUKEBOX:     return -EAGAIN; break;
	};
	return -ERANGE;
}


/*
 * NFSv3
 */
struct rpc_pdu *rpc_nfs3_null_task(struct rpc_context *rpc, rpc_cb cb,
                                   void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/NULL call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/NULL call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_getattr_task(struct rpc_context *rpc, rpc_cb cb,
                                      struct GETATTR3args *args,
                                      void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_GETATTR, cb, private_data, (zdrproc_t)zdr_GETATTR3res, sizeof(GETATTR3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/GETATTR call");
		return NULL;
	}

	if (zdr_GETATTR3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode GETATTR3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/GETATTR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_pathconf_task(struct rpc_context *rpc, rpc_cb cb,
                                       struct PATHCONF3args *args,
                                       void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_PATHCONF, cb, private_data, (zdrproc_t)zdr_PATHCONF3res, sizeof(PATHCONF3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/PATHCONF call");
		return NULL;
	}

	if (zdr_PATHCONF3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode PATHCONF3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/PATHCONF call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_lookup_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct LOOKUP3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_LOOKUP, cb, private_data, (zdrproc_t)zdr_LOOKUP3res, sizeof(LOOKUP3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/LOOKUP call");
		return NULL;
	}

	if (zdr_LOOKUP3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode LOOKUP3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/LOOKUP call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_access_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct ACCESS3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_ACCESS, cb, private_data, (zdrproc_t)zdr_ACCESS3res, sizeof(ACCESS3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/ACCESS call");
		return NULL;
	}

	if (zdr_ACCESS3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode ACCESS3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/ACCESS call");
		return NULL;
	}

	return pdu;
}

uint32_t
zdr_READ3resok_zero_copy (ZDR *zdrs, READ3resok *objp)
{
        if (!zdr_post_op_attr (zdrs, &objp->file_attributes))
                return FALSE;
        if (!zdr_count3 (zdrs, &objp->count))
                return FALSE;
        if (!zdr_bool (zdrs, &objp->eof))
                return FALSE;
	return TRUE;
}

uint32_t
zdr_READ3res_zero_copy (ZDR *zdrs, READ3res *objp)
{
        if (!zdr_nfsstat3 (zdrs, &objp->status))
                return FALSE;
	switch (objp->status) {
	case NFS3_OK:
		 if (!zdr_READ3resok_zero_copy (zdrs, &objp->READ3res_u.resok))
			 return FALSE;
		break;
	default:
		 if (!zdr_READ3resfail (zdrs, &objp->READ3res_u.resfail))
			 return FALSE;
		break;
	}
	return TRUE;
}

struct rpc_pdu *
rpc_nfs3_readv_task(struct rpc_context *rpc, rpc_cb cb,
                    const struct iovec *iov, int iovcnt,
                    struct READ3args *args, void *private_data)
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

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_READ, cb, private_data, (zdrproc_t)zdr_READ3res_zero_copy, sizeof(READ3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/READ call");
		return NULL;
	}
	if (zdr_READ3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READ3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

        /*
         * Allocate twice the iovec space, first half will be used for iov[].
         * This will be updated as data is read into user buffers.
         * Second half is for iov_ref[]. This is not used in happy path. Only
         * if we need to resend the request we need it to reset the cursor to
         * the original iovec.
         * See rpc_reset_cursor().
         */
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
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/READ call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nfs3_read_task(struct rpc_context *rpc, rpc_cb cb,
                   void *buf, size_t count,
                   struct READ3args *args, void *private_data)
{
	struct iovec iov;

	iov.iov_base = buf;
	iov.iov_len = count;

	return rpc_nfs3_readv_task(rpc, cb, &iov, 1, args, private_data);
}

/*
 * Replacement WRITE3args so that we can add the data as an iovector
 * instead of marshalling it in the out buffer.
 * This will marshall the WRITE3args structure except for the final
 * byte/array for the actual data.
 */
uint32_t
zdr_WRITE3args_zerocopy(ZDR *zdrs, WRITE3args *objp)
{
	if (!zdr_nfs_fh3 (zdrs, &objp->file))
                return FALSE;
        if (!zdr_offset3 (zdrs, &objp->offset))
                return FALSE;
        if (!zdr_count3 (zdrs, &objp->count))
                return FALSE;
        if (!zdr_stable_how (zdrs, &objp->stable))
                return FALSE;
	return TRUE;
}

struct rpc_pdu *rpc_nfs3_writev_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct WRITE3args *args,
                                     const struct iovec *iov,
                                     int iovcnt,
                                     void *private_data)
{
	struct rpc_pdu *pdu;
        int start;
        static uint32_t zero_padding;
        uint32_t data_len;

        /*
         * If caller has a single contiguous buffer they can convey it
         * using args.data, and if they have an io vector they can convey
         * that using iov.
         */
        if ((iovcnt == 0) != (iov == NULL)) {
		rpc_set_error(rpc, "Invalid arguments: iov and iovcnt must both be specified together");
		return NULL;
        }

        if (iovcnt && args->data.data_len) {
                /* Warn bad callers */
		rpc_set_error(rpc, "Invalid arguments: args->data.data_len not 0 when iovcnt is non-zero");
		return NULL;
        }

        if (iov && rpc->is_udp) {
		rpc_set_error(rpc, "Invalid arguments: Vectored write not supported for UDP transport");
		return NULL;
        }

        /*
         * We add 4 to the user provided iovcnt to account for one each for
         * the following:
         * - Record marker
         * - RPC header
         * - NFS header
         * - Padding (optional)
         */
	pdu = rpc_allocate_pdu2(rpc, NFS_PROGRAM, NFS_V3, NFS3_WRITE, cb, private_data, (zdrproc_t)zdr_WRITE3res, sizeof(WRITE3res), 0, iovcnt + 4);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/WRITE call");
		return NULL;
	}

        start = zdr_getpos(&pdu->zdr);

	if (zdr_WRITE3args_zerocopy(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode WRITE3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

        /* Add an iovector for the WRITE3 header */
        if (rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[start + 4],
                             zdr_getpos(&pdu->zdr) - start, NULL) < 0) {
		rpc_free_pdu(rpc, pdu);
		return NULL;
        }

        /* Calculate data length to encode in the RPC request */
        if (iov) {
                int i;
                data_len = 0;
                for (i = 0; i < iovcnt; i++) {
                        data_len += iov[i].iov_len;
                }
        } else {
                data_len = args->data.data_len;
        }

        /* Add an iovector for the length of the byte/array blob */
        start = zdr_getpos(&pdu->zdr);
        zdr_u_int(&pdu->zdr, &data_len);
        if (rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[start + 4],
                             4, NULL) < 0) {
		rpc_free_pdu(rpc, pdu);
		return NULL;
        }

        /* Add an iovector for the data itself */
        if (!iov) {
                if (rpc_add_iovector(rpc, &pdu->out, args->data.data_val,
                                     args->data.data_len, NULL) < 0) {
                        rpc_free_pdu(rpc, pdu);
                        return NULL;
                }
        } else {
                int i;
                for (i = 0; i < iovcnt; i++) {
                        if (rpc_add_iovector(rpc, &pdu->out,
                                             iov[i].iov_base,
                                             iov[i].iov_len, NULL) < 0) {
                                rpc_free_pdu(rpc, pdu);
                                return NULL;
                        }
                }
        }

        /* We may need to pad this to 4 byte boundary */
        if (data_len & 0x03) {
                if (rpc_add_iovector(rpc, &pdu->out, (char *)&zero_padding,
                                     4 - (data_len & 0x03), NULL) < 0) {
                        rpc_free_pdu(rpc, pdu);
                        return NULL;
                }
        }

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/WRITE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_write_task(struct rpc_context *rpc, rpc_cb cb,
                                    struct WRITE3args *args,
                                    void *private_data)
{
        return rpc_nfs3_writev_task(rpc, cb, args, NULL, 0, private_data);
}

struct rpc_pdu *rpc_nfs3_commit_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct COMMIT3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_COMMIT, cb, private_data, (zdrproc_t)zdr_COMMIT3res, sizeof(COMMIT3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/COMMIT call");
		return NULL;
	}

	if (zdr_COMMIT3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode COMMIT3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/COMMIT call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nfs3_setattr_task(struct rpc_context *rpc, rpc_cb cb, SETATTR3args *args,
                      void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_SETATTR, cb, private_data, (zdrproc_t)zdr_SETATTR3res, sizeof(SETATTR3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/SETATTR call");
		return NULL;
	}

	if (zdr_SETATTR3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode SETATTR3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/SETATTR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_mkdir_task(struct rpc_context *rpc, rpc_cb cb,
                                    MKDIR3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_MKDIR, cb, private_data, (zdrproc_t)zdr_MKDIR3res, sizeof(MKDIR3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/MKDIR call");
		return NULL;
	}

	if (zdr_MKDIR3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode MKDIR3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/MKDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_rmdir_task(struct rpc_context *rpc, rpc_cb cb,
                                    struct RMDIR3args *args,
                                    void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_RMDIR, cb, private_data, (zdrproc_t)zdr_RMDIR3res, sizeof(RMDIR3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/RMDIR call");
		return NULL;
	}

	if (zdr_RMDIR3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode RMDIR3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/RMDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_create_task(struct rpc_context *rpc, rpc_cb cb,
                                     CREATE3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_CREATE, cb, private_data, (zdrproc_t)zdr_CREATE3res, sizeof(CREATE3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/CREATE call");
		return NULL;
	}

	if (zdr_CREATE3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode CREATE3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/CREATE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_mknod_task(struct rpc_context *rpc, rpc_cb cb,
                                    struct MKNOD3args *args,
                                    void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_MKNOD, cb, private_data, (zdrproc_t)zdr_MKNOD3res, sizeof(MKNOD3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/MKNOD call");
		return NULL;
	}

	if (zdr_MKNOD3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode MKNOD3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/MKNOD call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_remove_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct REMOVE3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_REMOVE, cb, private_data, (zdrproc_t)zdr_REMOVE3res, sizeof(REMOVE3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/REMOVE call");
		return NULL;
	}

	if (zdr_REMOVE3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode REMOVE3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/REMOVE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_readdir_task(struct rpc_context *rpc, rpc_cb cb,
                                      struct READDIR3args *args,
                                      void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_READDIR, cb, private_data, (zdrproc_t)zdr_READDIR3res, sizeof(READDIR3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/READDIR call");
		return NULL;
	}

	if (zdr_READDIR3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READDIR3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/READDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_readdirplus_task(struct rpc_context *rpc, rpc_cb cb,
                                          struct READDIRPLUS3args *args,
                                          void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_READDIRPLUS, cb, private_data, (zdrproc_t)zdr_READDIRPLUS3res, sizeof(READDIRPLUS3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/READDIRPLUS call");
		return NULL;
	}

	if (zdr_READDIRPLUS3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READDIRPLUS3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/READDIRPLUS call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_fsstat_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct FSSTAT3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_FSSTAT, cb, private_data, (zdrproc_t)zdr_FSSTAT3res, sizeof(FSSTAT3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/FSSTAT call");
		return NULL;
	}

	if (zdr_FSSTAT3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode FSSTAT3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/FSSTAT call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_fsinfo_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct FSINFO3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_FSINFO, cb, private_data, (zdrproc_t)zdr_FSINFO3res, sizeof(FSINFO3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/FSINFO call");
		return NULL;
	}

	if (zdr_FSINFO3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode FSINFO3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/FSINFO call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nfs3_readlink_task(struct rpc_context *rpc, rpc_cb cb,
                       READLINK3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_READLINK, cb, private_data, (zdrproc_t)zdr_READLINK3res, sizeof(READLINK3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/READLINK call");
		return NULL;
	}

	if (zdr_READLINK3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READLINK3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/READLINK call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_symlink_task(struct rpc_context *rpc, rpc_cb cb,
                                       SYMLINK3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_SYMLINK, cb, private_data, (zdrproc_t)zdr_SYMLINK3res, sizeof(SYMLINK3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/SYMLINK call");
		return NULL;
	}

	if (zdr_SYMLINK3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode SYMLINK3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/SYMLINK call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_rename_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct RENAME3args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_RENAME, cb, private_data, (zdrproc_t)zdr_RENAME3res, sizeof(RENAME3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/RENAME call");
		return NULL;
	}

	if (zdr_RENAME3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode RENAME3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/RENAME call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs3_link_task(struct rpc_context *rpc, rpc_cb cb,
                                   struct LINK3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V3, NFS3_LINK, cb, private_data, (zdrproc_t)zdr_LINK3res, sizeof(LINK3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/LINK call");
		return NULL;
	}

	if (zdr_LINK3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode LINK3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS3/LINK call");
		return NULL;
	}

	return pdu;
}

/*
 * NFSv2
 */
struct rpc_pdu *rpc_nfs2_null_task(struct rpc_context *rpc, rpc_cb cb,
                                   void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/NULL call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/NULL call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_getattr_task(struct rpc_context *rpc, rpc_cb cb,
                                      struct GETATTR2args *args,
                                      void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_GETATTR, cb, private_data, (zdrproc_t)zdr_GETATTR2res, sizeof(GETATTR2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/GETATTR call");
		return NULL;
	}

	if (zdr_GETATTR2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode GETATTR2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/GETATTR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_setattr_task(struct rpc_context *rpc, rpc_cb cb,
                                      SETATTR2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_SETATTR, cb, private_data, (zdrproc_t)zdr_SETATTR2res, sizeof(SETATTR2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/SETATTR call");
		return NULL;
	}

	if (zdr_SETATTR2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode SETATTR2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/SETATTR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_lookup_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct LOOKUP2args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_LOOKUP, cb, private_data, (zdrproc_t)zdr_LOOKUP2res, sizeof(LOOKUP2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/LOOKUP call");
		return NULL;
	}

	if (zdr_LOOKUP2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode LOOKUP2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/LOOKUP call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_readlink_task(struct rpc_context *rpc, rpc_cb cb,
                                       READLINK2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_READLINK, cb, private_data, (zdrproc_t)zdr_READLINK2res, sizeof(READLINK2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/READLINK call");
		return NULL;
	}

	if (zdr_READLINK2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READLINK2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/READLINK call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_read_task(struct rpc_context *rpc, rpc_cb cb,
                                   struct READ2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_READ, cb, private_data, (zdrproc_t)zdr_READ2res, sizeof(READ2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/READ call");
		return NULL;
	}

	if (zdr_READ2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READ2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/READ call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_write_task(struct rpc_context *rpc, rpc_cb cb,
                                    struct WRITE2args *args,
                                    void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu2(rpc, NFS_PROGRAM, NFS_V2, NFS2_WRITE, cb, private_data, (zdrproc_t)zdr_WRITE2res, sizeof(WRITE2res), args->totalcount, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/WRITE call");
		return NULL;
	}

	if (zdr_WRITE2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode WRITE2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/WRITE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_create_task(struct rpc_context *rpc, rpc_cb cb,
                                     CREATE2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_CREATE, cb, private_data, (zdrproc_t)zdr_CREATE2res, sizeof(CREATE2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/CREATE call");
		return NULL;
	}

	if (zdr_CREATE2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode CREATE2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/CREATE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_remove_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct REMOVE2args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_REMOVE, cb, private_data, (zdrproc_t)zdr_REMOVE2res, sizeof(REMOVE2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS3/REMOVE call");
		return NULL;
	}

	if (zdr_REMOVE2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode REMOVE2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/REMOVE call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_rename_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct RENAME2args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_RENAME, cb, private_data, (zdrproc_t)zdr_RENAME2res, sizeof(RENAME2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/RENAME call");
		return NULL;
	}

	if (zdr_RENAME2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode RENAME2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/RENAME call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_link_task(struct rpc_context *rpc, rpc_cb cb,
                                   LINK2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_LINK, cb, private_data, (zdrproc_t)zdr_LINK2res, sizeof(LINK2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/LINK call");
		return NULL;
	}

	if (zdr_LINK2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode LINK2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/LINK call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_symlink_task(struct rpc_context *rpc, rpc_cb cb,
                                      SYMLINK2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_SYMLINK, cb, private_data, (zdrproc_t)zdr_SYMLINK2res, sizeof(SYMLINK2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/SYMLINK call");
		return NULL;
	}

	if (zdr_SYMLINK2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode SYMLINK2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/SYMLINK call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_mkdir_task(struct rpc_context *rpc, rpc_cb cb,
                                    MKDIR2args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_MKDIR, cb, private_data, (zdrproc_t)zdr_MKDIR2res, sizeof(MKDIR2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/MKDIR call");
		return NULL;
	}

	if (zdr_MKDIR2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode MKDIR2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/MKDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_rmdir_task(struct rpc_context *rpc, rpc_cb cb,
                                    struct RMDIR2args *args,
                                    void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_RMDIR, cb, private_data, (zdrproc_t)zdr_RMDIR2res, sizeof(RMDIR2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/RMDIR call");
		return NULL;
	}

	if (zdr_RMDIR2args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode RMDIR2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/RMDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_readdir_task(struct rpc_context *rpc, rpc_cb cb,
                                      struct READDIR2args *args,
                                      void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_READDIR, cb, private_data, (zdrproc_t)zdr_READDIR2res, sizeof(READDIR2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/READDIR call");
		return NULL;
	}

	if (zdr_READDIR2args(&pdu->zdr,  args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode READDIR2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/READDIR call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *rpc_nfs2_statfs_task(struct rpc_context *rpc, rpc_cb cb,
                                     struct STATFS2args *args,
                                     void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, NFS_V2, NFS2_STATFS, cb, private_data, (zdrproc_t)zdr_STATFS2res, sizeof(STATFS2res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for NFS2/STATFS call");
		return NULL;
	}

	if (zdr_STATFS2args(&pdu->zdr,  args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode STATFS2args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for NFS2/STATFS call");
		return NULL;
	}

	return pdu;
}
