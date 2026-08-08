/*
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#include "libnfs-raw-nsm.h"

struct rpc_pdu *
rpc_nsm1_null_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_NULL, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/null call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/null call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_stat_task(struct rpc_context *rpc, rpc_cb cb, struct NSM1_STATargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_STAT, cb, private_data, (zdrproc_t)zdr_NSM1_STATres, sizeof(NSM1_STATres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/stat call");
		return NULL;
	}

	if (zdr_NSM1_STATargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NSM1_STATargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/stat call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_mon_task(struct rpc_context *rpc, rpc_cb cb, struct NSM1_MONargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_MON, cb, private_data, (zdrproc_t)zdr_NSM1_MONres, sizeof(NSM1_MONres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/mon call");
		return NULL;
	}

	if (zdr_NSM1_MONargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NSM1_MONargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/mon call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_unmon_task(struct rpc_context *rpc, rpc_cb cb, struct NSM1_UNMONargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_UNMON, cb, private_data, (zdrproc_t)zdr_NSM1_UNMONres, sizeof(NSM1_UNMONres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/unmon call");
		return NULL;
	}

	if (zdr_NSM1_UNMONargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NSM1_UNMONargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/unmon call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_unmonall_task(struct rpc_context *rpc, rpc_cb cb, struct NSM1_UNMONALLargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_UNMON_ALL, cb, private_data, (zdrproc_t)zdr_NSM1_UNMONALLres, sizeof(NSM1_UNMONALLres));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/unmonall call");
		return NULL;
	}

	if (zdr_NSM1_UNMONALLargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NSM1_UNMONALLargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/unmonall call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_simucrash_task(struct rpc_context *rpc, rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_SIMU_CRASH, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/simucrash call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/simucrash call");
		return NULL;
	}

	return pdu;
}

struct rpc_pdu *
rpc_nsm1_notify_task(struct rpc_context *rpc, rpc_cb cb, struct NSM1_NOTIFYargs *args, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, NSM_PROGRAM, NSM_V1, NSM1_NOTIFY, cb, private_data, (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu for nsm/notify call");
		return NULL;
	}

	if (zdr_NSM1_NOTIFYargs(&pdu->zdr, args) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode NSM1_NOTIFYargs");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu for nsm/notify call");
		return NULL;
	}

	return pdu;
}

char *nsmstat1_to_str(int st)
{
	enum nsmstat1 stat = st;

	char *str = "unknown n1m stat";
	switch (stat) {
	case NSM_STAT_SUCC: str="NSM_STAT_SUCC";break;
	case NSM_STAT_FAIL: str="NSM_STAT_FAIL";break;
	}
	return str;
}
