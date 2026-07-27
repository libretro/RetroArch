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

#ifdef WIN32
#include <win32/win32_compat.h>
#else
#include <sys/stat.h>
#endif/*WIN32*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "libnfs-raw-nfs.h"


struct rpc_pdu *
rpc_nfsacl3_null_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFSACL_PROGRAM, NFSACL_V3, NFSACL3_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nfsacl/null call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nfsacl/null call");
		return NULL;
	}

	return pdu;
}


struct rpc_pdu *
rpc_nfsacl3_getacl_task(struct rpc_context *rpc, rpc_cb cb, struct GETACL3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFSACL_PROGRAM, NFSACL_V3, NFSACL3_GETACL, cb, private_data, (zdrproc_t)zdr_GETACL3res, sizeof(GETACL3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nfsacl/getacl call");
		return NULL;
	}

	if (zdr_GETACL3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode GETACL3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nfsacl/getacl call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nfsacl3_setacl_task(struct rpc_context *rpc, rpc_cb cb, struct SETACL3args *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NFSACL_PROGRAM, NFSACL_V3, NFSACL3_SETACL, cb, private_data, (zdrproc_t)zdr_SETACL3res, sizeof(SETACL3res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nfsacl/setacl call");
		return NULL;
	}

	if (zdr_SETACL3args(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode SETACL3args");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nfsacl/setacl call");
		return NULL;
	}

	return pdu;
}

