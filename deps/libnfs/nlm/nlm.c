/*
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#endif/*WIN32*/

#include <stdio.h>
#include <errno.h>
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "libnfs-raw-nlm.h"

struct rpc_pdu *
rpc_nlm4_null_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/null call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/null call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_test_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_TESTargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_TEST, cb, private_data, (zdrproc_t)zdr_NLM4_TESTres, sizeof(NLM4_TESTres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/test call");
		return NULL;
	}

	if (zdr_NLM4_TESTargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_TESTargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/test call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_lock_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_LOCKargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_LOCK, cb, private_data, (zdrproc_t)zdr_NLM4_LOCKres, sizeof(NLM4_LOCKres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/lock call");
		return NULL;
	}

	if (zdr_NLM4_LOCKargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_LOCKargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/lock call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_cancel_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_CANCargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_CANCEL, cb, private_data, (zdrproc_t)zdr_NLM4_CANCres, sizeof(NLM4_CANCres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/cancel call");
		return NULL;
	}

	if (zdr_NLM4_CANCargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_CANCargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/cancel call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_unlock_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_UNLOCKargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_UNLOCK, cb, private_data, (zdrproc_t)zdr_NLM4_UNLOCKres, sizeof(NLM4_UNLOCKres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/unlock call");
		return NULL;
	}

	if (zdr_NLM4_UNLOCKargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_UNLOCKargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/unlock call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_share_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_SHAREargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_SHARE, cb, private_data, (zdrproc_t)zdr_NLM4_SHAREres, sizeof(NLM4_SHAREres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/lock call");
		return NULL;
	}

	if (zdr_NLM4_SHAREargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_LOCKargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/lock call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nlm4_unshare_task(struct rpc_context *rpc, rpc_cb cb, struct NLM4_SHAREargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NLM_PROGRAM, NLM_V4, NLM4_UNSHARE, cb, private_data, (zdrproc_t)zdr_NLM4_UNSHAREres, sizeof(NLM4_UNSHAREres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nlm/lock call");
		return NULL;
	}

	if (zdr_NLM4_UNSHAREargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NLM4_LOCKargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nlm/lock call");
		return NULL;
	}

	return pdu;
}

char *nlmstat4_to_str(int st)
{
	enum nlmstat4 stat = st;

	char *str = "unknown nlm stat";
	switch (stat) {
	case NLM4_GRANTED: str="NLM4_GRANTED";break;
	case NLM4_DENIED: str="NLM4_DENIED";break;
	case NLM4_DENIED_NOLOCKS: str="NLM4_DENIED_NOLOCKS";break;
	case NLM4_BLOCKED: str="NLM4_BLOCKED";break;
	case NLM4_DENIED_GRACE_PERIOD: str="NLM4_DENIED_GRACE_PERIOD";break;
	case NLM4_DEADLCK: str="NLM4_DEADLCK";break;
	case NLM4_ROFS: str="NLM4_ROFS";break;
	case NLM4_STALE_FH: str="NLM4_STALE_FH";break;
	case NLM4_FBIG: str="NLM4_FBIG";break;
	case NLM4_FAILED: str="NLM4_FAILED";break;
	}
	return str;
}




