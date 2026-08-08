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
#endif/*WIN32*/

#include <stdio.h>
#include <errno.h>
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "libnfs-raw-mount.h"

struct rpc_pdu *
rpc_mount3_null_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for mount/null call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for mount/null call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount3_mnt_task(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_MNT, cb, private_data, (zdrproc_t)zdr_mountres3, sizeof(mountres3));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for mount/mnt call");
		return NULL;
	}

	if (zdr_dirpath(&pdu->zdr, &export) == 0) {
		rpc_set_error(rpc, "ZDR error. Failed to encode mount/mnt call");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for mount/mnt call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount3_dump_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_DUMP, cb, private_data, (zdrproc_t)zdr_mountlist, sizeof(mountlist));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for mount/dump");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue mount/dump pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount3_umnt_task(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_UMNT, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for mount/umnt");
		return NULL;
	}

	if (zdr_dirpath(&pdu->zdr, &export) == 0) {
		rpc_set_error(rpc, "failed to encode dirpath for mount/umnt");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue mount/umnt pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount3_umntall_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_UMNTALL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for mount/umntall");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue mount/umntall pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount3_export_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V3, MOUNT3_EXPORT, cb, private_data, (zdrproc_t)zdr_exports, sizeof(exports));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for mount/export");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue mount/export pdu");
		return NULL;
	}

	return pdu;
}

char *mountstat3_to_str(int st)
{
	enum mountstat3 stat = st;

	char *str = "unknown mount stat";
	switch (stat) {
	case MNT3_OK:		  str="MNT3_OK"; break;
	case MNT3ERR_PERM:	  str="MNT3ERR_PERM"; break;
	case MNT3ERR_NOENT:	  str="MNT3ERR_NOENT"; break;
	case MNT3ERR_IO:	  str="MNT3ERR_IO"; break;
	case MNT3ERR_ACCES:	  str="MNT3ERR_ACCES"; break;
	case MNT3ERR_NOTDIR:	  str="MNT3ERR_NOTDIR"; break;
	case MNT3ERR_INVAL:	  str="MNT3ERR_INVAL"; break;
	case MNT3ERR_NAMETOOLONG: str="MNT3ERR_NAMETOOLONG"; break;
	case MNT3ERR_NOTSUPP:	  str="MNT3ERR_NOTSUPP"; break;
	case MNT3ERR_SERVERFAULT: str="MNT3ERR_SERVERFAULT"; break;
	}
	return str;
}


int mountstat3_to_errno(int st)
{
	enum mountstat3 stat = st;

	switch (stat) {
	case MNT3_OK:		  return 0; break;
	case MNT3ERR_PERM:	  return -EPERM; break;
	case MNT3ERR_NOENT:	  return -EPERM; break;
	case MNT3ERR_IO:	  return -EIO; break;
	case MNT3ERR_ACCES:	  return -EACCES; break;
	case MNT3ERR_NOTDIR:	  return -ENOTDIR; break;
	case MNT3ERR_INVAL:	  return -EINVAL; break;
	case MNT3ERR_NAMETOOLONG: return -E2BIG; break;
	case MNT3ERR_NOTSUPP:	  return -EINVAL; break;
	case MNT3ERR_SERVERFAULT: return -EIO; break;
	}
	return -ERANGE;
}

struct rpc_pdu *
rpc_mount1_null_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for MOUNT1/NULL call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for MOUNT1/NULL call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount1_mnt_task(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_MNT, cb, private_data, (zdrproc_t)zdr_mountres1, sizeof(mountres1));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for MOUNT1/MNT call");
		return NULL;
	}

	if (zdr_dirpath(&pdu->zdr, &export) == 0) {
		rpc_set_error(rpc, "ZDR error. Failed to encode MOUNT1/MNT call");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for MOUNT1/MNT call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount1_dump_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_DUMP, cb, private_data, (zdrproc_t)zdr_mountlist, sizeof(mountlist));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for MOUNT1/DUMP");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue MOUNT1/DUMP pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount1_umnt_task(struct rpc_context *rpc, rpc_cb cb, char *export, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_UMNT, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for MOUNT1/UMNT");
		return NULL;
	}

	if (zdr_dirpath(&pdu->zdr, &export) == 0) {
		rpc_set_error(rpc, "failed to encode dirpath for MOUNT1/UMNT");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue MOUNT1/UMNT pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount1_umntall_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_UMNTALL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for MOUNT1/UMNTALL");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue MOUNT1/UMNTALL pdu");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_mount1_export_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, MOUNT_PROGRAM, MOUNT_V1, MOUNT1_EXPORT, cb, private_data, (zdrproc_t)zdr_exports, sizeof(exports));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Failed to allocate pdu for MOUNT1/EXPORT");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Failed to queue MOUNT1/EXPORT pdu");
		return NULL;
	}

	return pdu;
}

