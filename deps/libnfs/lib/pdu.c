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

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if defined(HAVE_SYS_UIO_H) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/uio.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "slist.h"
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"

#ifdef HAVE_LIBKRB5
#include "krb5-wrapper.h"
#endif

void rpc_reset_queue(struct rpc_queue *q)
{
	q->head = NULL;
	q->tail = NULL;
}

/*
 * Push to the tail end of the queue
 */
void rpc_enqueue(struct rpc_queue *q, struct rpc_pdu *pdu)
{
	if (q->head == NULL) {
	        assert(q->tail == NULL);
		q->head = pdu;
        } else {
                assert(pdu != q->head);
                assert(pdu != q->tail);
		q->tail->next = pdu;
        }
	q->tail = pdu;
	pdu->next = NULL;
}

/*
 * Return pdu to outqueue to be retransmitted.
 * If there are more than one PDUs already in outqueue, this adds it right
 * after the head, not at the head. The idea is that the PDU at the head
 * may be half-sent, so it's not safe to replace the head. Also since we
 * usually want this pdu to be sent immediately we don't want to add it to
 * the end.
 * Even when it's safe to add to head (from rpc_reconnect_requeue()), it's ok
 * to add after head.
 */
void rpc_return_to_outqueue(struct rpc_context *rpc, struct rpc_pdu *pdu)
{
        if (rpc->outqueue.head == NULL) {
                rpc->outqueue.head = rpc->outqueue.tail = pdu;
                pdu->next = NULL;
        } else if (rpc->outqueue.head == rpc->outqueue.tail) {
                rpc->outqueue.head->next = pdu;
                rpc->outqueue.tail = pdu;
                pdu->next = NULL;
        } else {
                pdu->next = rpc->outqueue.head->next;
                rpc->outqueue.head->next = pdu;
        }

        /*
         * Only already transmitted PDUs are added back to outqueue, so sending
         * it out will entail a retransmit.
         */
        INC_STATS(rpc, num_retransmitted);

        /*
         * Reset output and input cursors as we have to re-send the whole pdu
         * again (and read back the response fresh into pdu->in).
         */
        pdu->out.num_done = 0;
        rpc_reset_cursor(rpc, &pdu->in);
}

/*
 * Remove pdu from q.
 * If found it'll remove the pdu and update q->head and q->tail correctly.
 * Returns 0 if remove_pdu not found in q else returns 1.
 */
int rpc_remove_pdu_from_queue(struct rpc_queue *q, struct rpc_pdu *remove_pdu)
{
        if (q->head != NULL) {
                struct rpc_pdu *pdu = q->head;

                assert(q->tail != NULL);

                /*
                 * remove_pdu is the head pdu.
                 * Change the head to point to the next pdu.
                 * If tail is also pointing to remove_pdu, this means it's the
                 * only PDU and after removing that we will have an empty list.
                 */
                if (q->head == remove_pdu) {
                        q->head = remove_pdu->next;
                        if (q->tail == remove_pdu) {
                                assert(remove_pdu->next == NULL);
                                q->tail = NULL;
                                assert(q->head == NULL);
                        } else {
                                assert(q->head != NULL);
                        }

                        remove_pdu->next = NULL;
                        return 1;
                }

                /*
                 * remove_pdu is not the head pdu.
                 * Search for it and if found, remove it, and update tail if
                 * tail is pointing to remove_pdu.
                 */
                while (pdu->next && pdu->next != remove_pdu) {
                        pdu = pdu->next;
                }

                if (pdu->next == NULL) {
                        /* remove_pdu not found in q */
                        return 0;
                }

                pdu->next = remove_pdu->next;

                if (q->tail == remove_pdu) {
                        q->tail = pdu;
                }

                remove_pdu->next = NULL;

                return 1;
        } else {
                assert(q->tail == NULL);
                /* not found */
                return 0;
        }
}

static int rpc_remove_pdu_from_queue_unlocked(struct rpc_context *rpc,
                                              struct rpc_queue *q,
                                              struct rpc_pdu *remove_pdu)
{
        int ret;

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        ret = rpc_remove_pdu_from_queue(q, remove_pdu);
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        return ret;
}
        
unsigned int rpc_hash_xid(struct rpc_context *rpc, uint32_t xid)
{
	return (xid * 7919) % rpc->num_hashes;
}

#define PAD_TO_8_BYTES(x) ((x + 0x07) & ~0x07)

static struct rpc_pdu *rpc_allocate_reply_pdu(struct rpc_context *rpc,
                                              struct rpc_msg *res,
                                              size_t alloc_hint)
{
	struct rpc_pdu *pdu;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	pdu = malloc(sizeof(struct rpc_pdu) + ZDR_ENCODEBUF_MINSIZE + alloc_hint);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory: Failed to allocate pdu structure and encode buffer");
		return NULL;
	}
	memset(pdu, 0, sizeof(struct rpc_pdu));
        pdu->discard_after_sending = 1;
	pdu->xid                = 0;
	pdu->cb                 = NULL;
	pdu->private_data       = NULL;
	pdu->zdr_decode_fn      = NULL;
	pdu->zdr_decode_bufsize = 0;

	pdu->outdata.data = (char *)(pdu + 1);

        pdu->out.iov = pdu->out.fast_iov;
        pdu->out.iov_capacity = RPC_FAST_VECTORS;

        /* Add an iovector for the record marker. Ignored for UDP */
        rpc_add_iovector(rpc, &pdu->out, pdu->outdata.data, 4, NULL);

	zdrmem_create(&pdu->zdr, &pdu->outdata.data[4],
                      ZDR_ENCODEBUF_MINSIZE + alloc_hint, ZDR_ENCODE);

	if (zdr_replymsg(rpc, &pdu->zdr, res) == 0) {
		rpc_set_error(rpc, "zdr_replymsg failed with %s",
			      rpc_get_error(rpc));
		zdr_destroy(&pdu->zdr);
		free(pdu);
		return NULL;
	}

        /* Add an iovector for the header */
        rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[4],
                         zdr_getpos(&pdu->zdr), NULL);

	return pdu;
}

struct rpc_pdu *rpc_allocate_pdu2(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, zdrproc_t zdr_decode_fn, int zdr_decode_bufsize, size_t alloc_hint, int iovcnt_hint)
{
	struct rpc_pdu *pdu;
	int pdu_size;
#ifdef HAVE_LIBKRB5
        uint32_t val;
#endif

#ifdef HAVE_TLS
	/*
	 * Caller overloads procedure to convey they want to send AUTH_TLS instead of
	 * AUTH_NONE for the NULL RPC.
	 */
	const bool_t send_auth_tls = !!(procedure & 0x80000000U);
	procedure = (procedure & 0x7FFFFFFFU);

	/* AUTH_TLS can only be sent for NFS NULL RPC */
	assert(!send_auth_tls || (program == NFS_PROGRAM && procedure == 0));
#endif /* HAVE_TLS */

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Since we already know how much buffer we need for the decoding
	 * we can just piggyback in the same alloc as for the pdu.
	 */
	pdu_size = PAD_TO_8_BYTES(sizeof(struct rpc_pdu));
	pdu_size += PAD_TO_8_BYTES(zdr_decode_bufsize);

	pdu = malloc(pdu_size + ZDR_ENCODEBUF_MINSIZE + alloc_hint);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory: Failed to allocate pdu structure and encode buffer");
		return NULL;
	}
	memset(pdu, 0, pdu_size);
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	pdu->xid                = rpc->xid++;
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	pdu->cb                 = cb;
	pdu->private_data       = private_data;
	pdu->zdr_decode_fn      = zdr_decode_fn;
	pdu->zdr_decode_bufsize = zdr_decode_bufsize;

        if (iovcnt_hint > RPC_FAST_VECTORS) {
                pdu->out.iov = (struct rpc_iovec *) calloc(iovcnt_hint, sizeof(struct rpc_iovec));
                if (pdu->out.iov == NULL) {
                    rpc_set_error(rpc, "Out of memory: Failed to allocate out.iov");
                    goto failed2;
                }
                pdu->out.iov_capacity = iovcnt_hint;
        } else {
                pdu->out.iov = pdu->out.fast_iov;
                pdu->out.iov_capacity = RPC_FAST_VECTORS;
        }

        /*
         * Rest of the code depends on this, so assert it here.
         * If the caller uses this pdu for issuing a zero-copy READ,
         * pdu->in.base will be set to point to the dynamically allocated
         * iovec array.
         */
        assert(pdu->in.base == NULL);

	pdu->outdata.data = ((char *)pdu + pdu_size);

        /* Add an iovector for the record marker. Ignored for UDP */
        rpc_add_iovector(rpc, &pdu->out, pdu->outdata.data, 4, NULL);

        zdrmem_create(&pdu->zdr, &pdu->outdata.data[4],
                      ZDR_ENCODEBUF_MINSIZE + alloc_hint - 4, ZDR_ENCODE);
	memset(&pdu->msg, 0, sizeof(struct rpc_msg));
	pdu->msg.xid                = pdu->xid;
        pdu->msg.direction          = CALL;
	pdu->msg.body.cbody.rpcvers = RPC_MSG_VERSION;
	pdu->msg.body.cbody.prog    = program;
	pdu->msg.body.cbody.vers    = version;
	pdu->msg.body.cbody.proc    = procedure;

	pdu->do_not_retry      = (program != NFS_PROGRAM);

	/* For NULL RPC RFC recommends to use NULL authentication */
	if (procedure == 0) {
		pdu->msg.body.cbody.cred.oa_flavor    = AUTH_NONE;
		pdu->msg.body.cbody.cred.oa_length    = 0;
		pdu->msg.body.cbody.cred.oa_base      = NULL;
		/*
		 * NULL RPC is like a ping which is sent right after connection
		 * establishment. The transport is still not used for sending
		 * other RPCs. It's best not to retry NULL RPC and let the caller
		 * truthfully know about the transport status.
		 */
		pdu->do_not_retry                = TRUE;
	} else {
		pdu->msg.body.cbody.cred    = rpc->auth->ah_cred;
	}

	pdu->msg.body.cbody.verf    = rpc->auth->ah_verf;

#ifdef HAVE_TLS
	/* Should not be already set */
	assert(pdu->expect_starttls == FALSE);

	if (send_auth_tls) {
		pdu->msg.body.cbody.cred.oa_flavor    = AUTH_TLS;
		pdu->msg.body.cbody.cred.oa_length    = 0;
		pdu->msg.body.cbody.cred.oa_base      = NULL;

		pdu->expect_starttls 		 = TRUE;
        }
#endif /* HAVE_TLS */

#ifdef HAVE_LIBKRB5
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        if (rpc->sec != RPC_SEC_UNDEFINED) {
                ZDR tmpzdr;
                int level = RPC_GSS_SVC_NONE;

                pdu->gss_seqno = rpc->gss_seqno;

                zdrmem_create(&tmpzdr, pdu->creds, 64, ZDR_ENCODE);
                switch (rpc->sec) {
                case RPC_SEC_UNDEFINED:
                        break;
                case RPC_SEC_KRB5:
                        level = RPC_GSS_SVC_NONE;
                        break;
                case RPC_SEC_KRB5I:
                        if (pdu->gss_seqno > 0) {
                                level = RPC_GSS_SVC_INTEGRITY;
                        }
                        break;
                case RPC_SEC_KRB5P:
                        if (pdu->gss_seqno > 0) {
                                level = RPC_GSS_SVC_PRIVACY;
                        }
                        break;
                }
                if (libnfs_authgss_gen_creds(rpc, &tmpzdr, level) < 0) {
                        zdr_destroy(&tmpzdr);
                        rpc_set_error(rpc, "zdr_callmsg failed with %s",
                                      rpc_get_error(rpc));
                        goto failed;
                }
                pdu->msg.body.cbody.cred.oa_flavor = AUTH_GSS;
                pdu->msg.body.cbody.cred.oa_length = tmpzdr.pos;
                pdu->msg.body.cbody.cred.oa_base = pdu->creds;
                zdr_destroy(&tmpzdr);

                rpc->gss_seqno++;
                if (rpc->gss_seqno > 1) {
                        pdu->msg.body.cbody.verf.oa_flavor = AUTH_GSS;
                        pdu->msg.body.cbody.verf.gss_context = rpc->gss_context;
                }
        }
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
#endif /* HAVE_LIBKRB5 */

	if (zdr_callmsg(rpc, &pdu->zdr, &pdu->msg) == 0) {
		rpc_set_error(rpc, "zdr_callmsg failed with %s",
			      rpc_get_error(rpc));
                goto failed;
	}

#ifdef HAVE_LIBKRB5
        switch (rpc->sec) {
        case RPC_SEC_UNDEFINED:
        case RPC_SEC_KRB5:
                break;
        case RPC_SEC_KRB5P:
        case RPC_SEC_KRB5I:
                if (pdu->gss_seqno > 0) {
                        pdu->start_of_payload = zdr_getpos(&pdu->zdr);
                        val = 0; /* dummy length, will fill in below once we know */
                        if (!libnfs_zdr_u_int(&pdu->zdr, &val)) {
                                goto failed;
                       }
                        val = pdu->gss_seqno;
                        if (!libnfs_zdr_u_int(&pdu->zdr, &val)) {
                                goto failed;
                        }
                }
                break;
        }
#endif /* HAVE_LIBKRB5 */

        /* Add an iovector for the header */
        rpc_add_iovector(rpc, &pdu->out, &pdu->outdata.data[4],
                         zdr_getpos(&pdu->zdr), NULL);

	return pdu;
 failed:
        rpc_set_error(rpc, "zdr_callmsg failed with %s",
                      rpc_get_error(rpc));
        zdr_destroy(&pdu->zdr);
 failed2:
        free(pdu);
        return NULL;
}

struct rpc_pdu *rpc_allocate_pdu(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, zdrproc_t zdr_decode_fn, int zdr_decode_bufsize)
{
	return rpc_allocate_pdu2(rpc, program, version, procedure, cb, private_data, zdr_decode_fn, zdr_decode_bufsize, 0, 0);
}

void rpc_free_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu)
{
#ifdef HAVE_LIBKRB5
        uint32_t min;
#endif /* HAVE_LIBKRB5 */

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (pdu->zdr_decode_buf != NULL) {
		zdr_free(pdu->zdr_decode_fn, pdu->zdr_decode_buf);
	}

#ifdef HAVE_LIBKRB5
        gss_release_buffer(&min, &pdu->output_buffer);
#endif /* HAVE_LIBKRB5 */
	zdr_destroy(&pdu->zdr);

        rpc_free_iovector(rpc, &pdu->out);
        rpc_free_cursor(rpc, &pdu->in);
        free(pdu);
}

void rpc_set_next_xid(struct rpc_context *rpc, uint32_t xid)
{
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	rpc->xid = xid;
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
}

void pdu_set_timeout(struct rpc_context *rpc, struct rpc_pdu *pdu, uint64_t now_msecs)
{
	if (rpc->timeout <= 0) {
		/* RPC request never times out */
		pdu->timeout = 0;
		return;
	}

	/* If user hasn't passed the current time, get it now */
	if (now_msecs == 0) {
		now_msecs = rpc_current_time();
	}

	/*
	 * If pdu->timeout is 0 it means either this is the first time we are
	 * setting the timeout for this RPC request or it has already timed out.
	 * In both these cases we reset pdu->timeout to rpc->timeout from now.
	 * If pdu->timeout is not 0 it means that the RPC has not yet timed out
	 * and hence we leave it unchanged.
	 */
	if (pdu->timeout == 0) {
		pdu->timeout = now_msecs + rpc->timeout;
#ifndef HAVE_CLOCK_GETTIME
		/* If we do not have GETTIME we fallback to time() which
		 * has 1s granularity for its timestamps.
		 * We thus need to bump the timeout by 1000ms
		 * so that the PDU will timeout within 1.0 - 2.0 seconds.
		 * Otherwise setting a 1s timeout would trigger within
		 * 0.001 - 1.0s.
		 */
		pdu->timeout += 1000;
#endif
	}

        /*
         * On major timeout we reset both major_timeout and timeout.
         * Note that timeout can be updated multiple times before a major
         * timeout, depending on the value of rpc->retrans.
         */
	if (pdu->major_timeout == 0) {
		pdu->major_timeout = now_msecs + (rpc->timeout * rpc->retrans);
		pdu->timeout = now_msecs + rpc->timeout;
#ifndef HAVE_CLOCK_GETTIME
		pdu->major_timeout += 1000;
		pdu->timeout += 1000;
#endif
                /* Never less than pdu->timeout */
                if (pdu->major_timeout < pdu->timeout) {
                        pdu->major_timeout = pdu->timeout;
                }
        }
}

int rpc_queue_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu)
{
	int i, size = 0, pos;
        uint32_t recordmarker;
#ifdef HAVE_LIBKRB5
        uint32_t maj, min, val, len;
        gss_buffer_desc message_buffer, output_token;
        char *buf;
#endif /* HAVE_LIBKRB5 */

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

#ifdef HAVE_LIBKRB5
        switch (rpc->sec) {
        case RPC_SEC_UNDEFINED:
        case RPC_SEC_KRB5:
                break;
        case RPC_SEC_KRB5I:
                if (pdu->gss_seqno == 0) {
                        break;
                }
                pos = zdr_getpos(&pdu->zdr);
                zdr_setpos(&pdu->zdr, pdu->start_of_payload);
                val = pos - pdu->start_of_payload - 4;
                if (!libnfs_zdr_u_int(&pdu->zdr, &val)) {
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                zdr_setpos(&pdu->zdr, pos);

                /* checksum */
                message_buffer.length = zdr_getpos(&pdu->zdr) - pdu->start_of_payload - 4;
                message_buffer.value = zdr_getptr(&pdu->zdr) + pdu->start_of_payload + 4;
                maj = gss_get_mic(&min, rpc->gss_context,
                                  GSS_C_QOP_DEFAULT,
                                  &message_buffer,
                                  &output_token);
                if (maj != GSS_S_COMPLETE) {
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                buf = output_token.value;
                len = output_token.length;
                if (!libnfs_zdr_bytes(&pdu->zdr, &buf, &len, len)) {
                        gss_release_buffer(&min, &output_token);
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                gss_release_buffer(&min, &output_token);
                break;
        case RPC_SEC_KRB5P:
                if (pdu->gss_seqno == 0) {
                        break;
                }
                pos = zdr_getpos(&pdu->zdr);
                message_buffer.length = zdr_getpos(&pdu->zdr) - pdu->start_of_payload - 4;
                message_buffer.value = zdr_getptr(&pdu->zdr) + pdu->start_of_payload + 4;
                maj = gss_wrap (&min, rpc->gss_context, 1,
                                GSS_C_QOP_DEFAULT,
                                &message_buffer,
                                NULL,
                                &output_token);
                if (maj != GSS_S_COMPLETE) {
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                zdr_setpos(&pdu->zdr, pdu->start_of_payload);
                buf = output_token.value;
                len = output_token.length;
                if (!libnfs_zdr_bytes(&pdu->zdr, &buf, &len, len)) {
                        gss_release_buffer(&min, &output_token);
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                gss_release_buffer(&min, &output_token);
                break;
        }
#endif /* HAVE_LIBKRB5 */

        pos = zdr_getpos(&pdu->zdr);

        /*
         * Now that the RPC is about to be queued, set absolute timeout values
         * for it.
         */
        pdu_set_timeout(rpc, pdu, 0);

        for (i = 1; i < pdu->out.niov; i++) {
                size += pdu->out.iov[i].len;
        }
        pdu->out.total_size = size + 4;

        /* If we need to add any additional iovectors
         *
         * We expect to almost always add an iovector here for the remainder
         * of the outdata marshalling buffer.
         * The exception is WRITE where we add an explicit iovector instead
         * of marshalling it in ZDR. This so that we can do zero-copy for
         * the WRITE path.
         */
        if (pos > size) {
                int count = pos - size;

                if (rpc_add_iovector(rpc, &pdu->out,
                                     &pdu->outdata.data[pdu->out.total_size],
                                     count, NULL) < 0) {
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                pdu->out.total_size += count;
                size = pos;
        }

	/* write recordmarker */
        recordmarker = htonl(size | 0x80000000);
	memcpy(pdu->out.iov[0].buf, &recordmarker, 4);

        pdu->pdu_stats.enqueue_timestamp = rpc_current_time_us();
        pdu->pdu_stats.size = size;
        pdu->pdu_stats.xid = pdu->msg.xid;
        pdu->pdu_stats.direction = CALL;
        pdu->pdu_stats.status = 0;
        pdu->pdu_stats.prog = pdu->msg.body.cbody.prog;
        pdu->pdu_stats.vers = pdu->msg.body.cbody.vers;
        pdu->pdu_stats.proc = pdu->msg.body.cbody.proc;
        pdu->pdu_stats.response_time = 0;
        
	/*
	 * For udp we dont queue, we just send it straight away.
	 *
	 * Another case where we send straight away is the AUTH_TLS NULL RPC.
	 * This is particularly important for the reconnect case where we want to
	 * ensure TLS handshake completes successfully before we can send any of
	 * the queued RPCs waiting. If we do not send here this AUTH_TLS NULL
	 * RPC will need to be queued before all other waiting RPCs and even then
	 * we need to be careful that we don't send any of those RPCs till the
	 * TLS handshake is completed and the connection is secure.
	 * Sending inline here makes the handling simpler in rpc_service().
	 */
        if (rpc->is_udp && rpc->is_server_context) {
                if (sendto(rpc->fd, pdu->zdr.buf, size, MSG_DONTWAIT,
                           (struct sockaddr *)&rpc->udp_dest,
                           sizeof(rpc->udp_dest)) < 0) {
                        rpc_set_error(rpc, "Sendto failed with errno %s", strerror(errno));
                        rpc_free_pdu(rpc, pdu);
                        return -1;
                }
                rpc_free_pdu(rpc, pdu);
                return 0;
        }

	if (rpc->is_udp != 0
#ifdef HAVE_TLS
	    || pdu->expect_starttls
#endif
	) {
		unsigned int hash;

#ifdef HAVE_TLS
		if (pdu->expect_starttls) {
			/* Currently we don't support RPC-with-TLS over UDP */
			assert(!rpc->is_udp);
			assert(!rpc->is_broadcast);

			RPC_LOG(rpc, 2, "Sending AUTH_TLS NULL RPC (%u bytes)",
                                (int)pdu->out.total_size);
		}
#endif

		hash = rpc_hash_xid(rpc, pdu->xid);

#ifdef HAVE_MULTITHREADING
                if (rpc->multithreading_enabled) {
                        nfs_mt_mutex_lock(&rpc->rpc_mutex);
                }
#endif /* HAVE_MULTITHREADING */
                rpc_enqueue(&rpc->waitpdu[hash], pdu);
                rpc->waitpdu_len++;
#ifdef HAVE_MULTITHREADING
                if (rpc->multithreading_enabled) {
                        nfs_mt_mutex_unlock(&rpc->rpc_mutex);
                }
#endif /* HAVE_MULTITHREADING */

                if (rpc->is_broadcast || rpc->is_server_context) {
                        if (sendto(rpc->fd, pdu->zdr.buf, size, MSG_DONTWAIT,
                                   (struct sockaddr *)&rpc->udp_dest,
                                   sizeof(rpc->udp_dest)) < 0) {
                                rpc_set_error(rpc, "Sendto failed with errno %s", strerror(errno));
                                rpc_remove_pdu_from_queue_unlocked(rpc, &rpc->waitpdu[hash], pdu);
                                rpc_free_pdu(rpc, pdu);
                                return -1;
                        }
                        if (rpc->is_server_context) {
                                rpc_remove_pdu_from_queue_unlocked(rpc, &rpc->waitpdu[hash], pdu);
                                rpc_free_pdu(rpc, pdu);
                                return 0;
                        }
                } else {
                        /*
                         * For UDP we don't support vectored write and for TLS
                         * the data will be less, so RPC_FAST_VECTORS should
                         * be sufficient for both cases.
                         */
                        struct iovec iov[RPC_FAST_VECTORS];
                        int niov = pdu->out.niov;
                        /* No record marker for UDP */
                        struct iovec *iovp = (rpc->is_udp ? &iov[1] : &iov[0]);
                        const int iovn = (rpc->is_udp ? niov - 1 : niov);

                        assert(niov <= RPC_FAST_VECTORS);

                        for (i = 0; i < niov; i++) {
                                iov[i].iov_base = pdu->out.iov[i].buf;
                                iov[i].iov_len = pdu->out.iov[i].len;
                        }
                        if (writev(rpc->fd, iovp, iovn) < 0) {
                                rpc_set_error(rpc, "Sendto failed with errno %s", strerror(errno));
                                rpc_remove_pdu_from_queue_unlocked(rpc, &rpc->waitpdu[hash], pdu);
                                rpc_free_pdu(rpc, pdu);
                                return -1;
                        }
                }

		return 0;
	}

	pdu->outdata.size = size;
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        /* Fresh PDU being queued to outqueue, num_done must be 0 */
        assert(pdu->out.num_done == 0);
        rpc_enqueue(&rpc->outqueue, pdu);
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        if (rpc->outqueue.head == pdu) {
                rpc_write_to_socket(rpc);
        }

	return 0;
}

static int rpc_process_reply(struct rpc_context *rpc, ZDR *zdr)
{
	struct rpc_msg msg;
        struct rpc_pdu *pdu = rpc->pdu;
        uint32_t status = 0xffffffff;
        void *data = NULL;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Client got a response for its request */
	INC_STATS(rpc, num_resp_rcvd);

	memset(&msg, 0, sizeof(struct rpc_msg));
	msg.body.rbody.reply.areply.verf = _null_auth;
	if (pdu->zdr_decode_bufsize > 0) {
		pdu->zdr_decode_buf = (char *)pdu + PAD_TO_8_BYTES(sizeof(struct rpc_pdu));
	}
	msg.body.rbody.reply.areply.reply_data.results.where = pdu->zdr_decode_buf;
	msg.body.rbody.reply.areply.reply_data.results.proc  = pdu->zdr_decode_fn;
#ifdef HAVE_LIBKRB5
        if (rpc->sec == RPC_SEC_KRB5I && pdu->gss_seqno > 0) {
                msg.body.rbody.reply.areply.reply_data.results.krb5i = 1;
        }
        if (rpc->sec == RPC_SEC_KRB5P && pdu->gss_seqno > 0) {
                msg.body.rbody.reply.areply.reply_data.results.krb5p = 1;
                msg.body.rbody.reply.areply.reply_data.results.output_buffer = &pdu->output_buffer;
                msg.body.rbody.reply.areply.verf.gss_context = rpc->gss_context;
        }
#endif
	if (zdr_replymsg(rpc, zdr, &msg) == 0) {
		rpc_set_error(rpc, "zdr_replymsg failed in rpc_process_reply: "
			      "%s", rpc_get_error(rpc));
		pdu->cb(rpc, RPC_STATUS_ERROR, "Message rejected by server",
			pdu->private_data);
		if (pdu->zdr_decode_buf != NULL) {
			pdu->zdr_decode_buf = NULL;
		}
		return 0;
	}
	if (msg.body.rbody.stat != MSG_ACCEPTED) {
		pdu->cb(rpc, RPC_STATUS_ERROR, "RPC Packet not accepted by the server", pdu->private_data);
		return 0;
	}
	switch (msg.body.rbody.reply.areply.stat) {
	case SUCCESS:
		/* Last RPC response time for tracking RPC transport health */
		rpc->last_successful_rpc_response = rpc_current_time();
		if (pdu->snr_logged) {
			RPC_LOG(rpc, 1, "[pdu %p] Server %s OK",
				pdu, rpc->server);
		}

                /*
                 * pdu->in.base will be non-NULL if this pdu is used for
                 * zero-copy READ. In that case we still need to read the
                 * data from the socket into the user's zero-copy buffers,
                 * so don't complete it as yet. Caller will arrange to read
                 * the data and complete the PDU once completed.
                 */
#ifdef HAVE_LIBKRB5
                if (rpc->sec != RPC_SEC_KRB5P)
#endif /* HAVE_LIBKRB5 */
                        if (pdu->in.base) {
                                rpc->pdu->free_pdu = 1;
                                break;
                        }

#ifdef HAVE_TLS
		/*
		 * If we are expecting STARTTLS that means we have sent AUTH_TLS
		 * NULL RPC which means user has selected xprtsec=[tls,mtls], in
		 * which case the server MUST support TLS else we must terminate
		 * the RPC session.
		 */
		if (pdu->expect_starttls) {
			const char *const starttls_str = "STARTTLS";
			const int starttls_len = 8;

			if (msg.body.rbody.reply.areply.verf.oa_flavor != AUTH_NONE) {
				RPC_LOG(rpc, 1, "Server sent bad verifier flavor (%d) in response "
					"to AUTH_TLS NULL RPC",
					msg.body.rbody.reply.areply.verf.oa_flavor);
                                status = RPC_STATUS_ERROR;
                                data = "Server sent bad verifier flavor";
				break;
			} else if (msg.body.rbody.reply.areply.verf.oa_length != starttls_len ||
				   memcmp(msg.body.rbody.reply.areply.verf.oa_base,
					  starttls_str, starttls_len)) {
				RPC_LOG(rpc, 1, "Server does not support TLS");
                                status = RPC_STATUS_ERROR;
                                data = "Server does not support TLS";
				break;
			}
		}
#endif /* HAVE_TLS */

#ifdef HAVE_LIBKRB5
                if (msg.body.rbody.reply.areply.verf.oa_flavor == AUTH_GSS) {
                        uint32_t maj, min;
                        gss_buffer_desc message_buffer, token_buffer;
                        uint32_t seqno;

                        /* This is the the gss token from the NULL reply
                         * that finished authentication.
                         */
                        if (pdu->gss_seqno == 0) {
                                struct rpc_gss_init_res *gir = (struct rpc_gss_init_res *)(void *)pdu->zdr_decode_buf;

                                rpc->context_len = gir->handle.handle_len;
                                free(rpc->context);
                                rpc->context = malloc(rpc->context_len);
                                if (rpc->context == NULL) {
                                        status = RPC_STATUS_ERROR;
                                        data = "Failed to allocate rpc->context";
                                        break;
                                }
                                memcpy(rpc->context, gir->handle.handle_val, rpc->context_len);

                                if (krb5_auth_request(rpc, rpc->auth_data,
                                                      (unsigned char *)gir->gss_token.gss_token_val,
                                                      gir->gss_token.gss_token_len) < 0) {
                                        status = RPC_STATUS_ERROR;

                                        data = "krb5_auth_request returned error";
                                        break;
                                }
                        }

                        if (pdu->gss_seqno > 0) {
                                seqno = htonl(pdu->gss_seqno);
                                message_buffer.value = (char *)&seqno;
                                message_buffer.length = 4;

                                token_buffer.value = msg.body.rbody.reply.areply.verf.oa_base;
                                token_buffer.length = msg.body.rbody.reply.areply.verf.oa_length;
                                maj = gss_verify_mic(&min,
                                                     rpc->gss_context,
                                                     &message_buffer,
                                                     &token_buffer,
                                                     GSS_C_QOP_DEFAULT);
                                if (maj) {
                                        status = RPC_STATUS_ERROR;
                                        data = "gss_verify_mic failed for the verifier";
                                        break;
                                }
                        }
                }
                if (pdu->zero_copy_iov && rpc->sec == RPC_SEC_KRB5P) {
                        struct iovec *iov;
                        int num_iov, num, count;

                        if (!zdr_uint32_t(&pdu->zdr, &pdu->read_count)) {
                                status = RPC_STATUS_ERROR;
                                data = "rpc_process_reply: failed to read onc-rpc array length";
                                break;
                        }
                        count = pdu->read_count;
                        if (count > libnfs_zdr_getsize(&pdu->zdr) - libnfs_zdr_getpos(&pdu->zdr)) {
                                count = libnfs_zdr_getsize(&pdu->zdr) - libnfs_zdr_getpos(&pdu->zdr);
                        }
                        iov = pdu->in.iov;
                        num_iov = pdu->in.iovcnt;
                        while(count && num_iov) {
                                num = count;
                                if (num > iov->iov_len) {
                                        num = iov->iov_len;
                                }
                                memcpy(iov->iov_base, libnfs_zdr_getptr(&pdu->zdr) + libnfs_zdr_getpos(&pdu->zdr), num);
                                libnfs_zdr_setpos(&pdu->zdr, libnfs_zdr_getpos(&pdu->zdr) + num);
                                count -= num;
                                iov++;
                                num_iov--;
                        }
                }
#endif /* HAVE_LIBKRB5 */

                status = RPC_STATUS_SUCCESS;
                data = pdu->zdr_decode_buf;
		break;
	case PROG_UNAVAIL:
                status = RPC_STATUS_ERROR;
                data = "Server responded: Program not available";
		break;
	case PROG_MISMATCH:
                status = RPC_STATUS_ERROR;
                data = "Server responded: Program version mismatch";
		break;
	case PROC_UNAVAIL:
                status = RPC_STATUS_ERROR;
                data = "Server responded: Procedure not available";
		break;
	case GARBAGE_ARGS:
                status = RPC_STATUS_ERROR;
                data = "Server responded: Garbage arguments";
		break;
	case SYSTEM_ERR:
                status = RPC_STATUS_ERROR;
                data = "Server responded: System Error";
		break;
	default:
                status = RPC_STATUS_ERROR;
                data = "Unknown rpc response from server";
		break;
	}

        pdu->pdu_stats.size = rpc->rm_xid[0];
        pdu->pdu_stats.direction = REPLY;
        pdu->pdu_stats.status = msg.body.rbody.stat;
        pdu->pdu_stats.response_time = rpc_current_time_us() - pdu->pdu_stats.send_timestamp;
        if (rpc->stats_cb) {
                rpc->stats_cb(rpc, &pdu->pdu_stats, rpc->stats_private_data);
        }

        if (status != 0xffffffff) {
                pdu->cb(rpc, status, data, pdu->private_data);
        }
	return 0;
}

struct _rpc_msg {
        struct rpc_msg call;
        struct sockaddr_storage udp_src;
};

struct rpc_msg *rpc_copy_deferred_call(struct rpc_context *rpc,
                                       struct rpc_msg *call)
{
        struct _rpc_msg *c;

        c = malloc(sizeof(struct _rpc_msg));
        if (c == NULL) {
                return NULL;
        }
        memcpy(c, call, sizeof(struct _rpc_msg));

        return &c->call;
}

void rpc_free_deferred_call(struct rpc_context *rpc,
                            struct rpc_msg *call)
{
        free(call);
}

static int rpc_send_error_reply(struct rpc_context *rpc,
                                struct rpc_msg *call,
                                enum accept_stat err,
                                int min_vers, int max_vers)
{
        struct rpc_pdu *pdu;
        struct _rpc_msg *c = (struct _rpc_msg *)call;
        struct rpc_msg res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	memset(&res, 0, sizeof(struct rpc_msg));
	res.xid                                      = call->xid;
        res.direction                                = REPLY;
        res.body.rbody.stat                          = MSG_ACCEPTED;
        res.body.rbody.reply.areply.reply_data.mismatch_info.low  = min_vers;
        res.body.rbody.reply.areply.reply_data.mismatch_info.high = max_vers;
	res.body.rbody.reply.areply.verf             = _null_auth;
	res.body.rbody.reply.areply.stat             = err;

        if (rpc->is_udp) {
                /* send the reply back to the client */
                memcpy(&rpc->udp_dest, &c->udp_src, sizeof(rpc->udp_dest));
        }

        pdu  = rpc_allocate_reply_pdu(rpc, &res, 0);
        if (pdu == NULL) {
                rpc_set_error(rpc, "Failed to send error_reply: %s",
                              rpc_get_error(rpc));
                return -1;
        }
        return rpc_queue_pdu(rpc, pdu);
}

int rpc_send_reply(struct rpc_context *rpc,
                   struct rpc_msg *call,
                   void *reply,
                   zdrproc_t encode_fn,
                   int alloc_hint)
{
        struct rpc_pdu *pdu;
        struct _rpc_msg *c = (struct _rpc_msg *)call;
        struct rpc_msg res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	memset(&res, 0, sizeof(struct rpc_msg));
	res.xid                                      = call->xid;
        res.direction                                = REPLY;
        res.body.rbody.stat                          = MSG_ACCEPTED;
	res.body.rbody.reply.areply.verf             = _null_auth;
	res.body.rbody.reply.areply.stat             = SUCCESS;

        res.body.rbody.reply.areply.reply_data.results.where = reply;
	res.body.rbody.reply.areply.reply_data.results.proc  = encode_fn;

        if (rpc->is_udp) {
                /* send the reply back to the client */
                memcpy(&rpc->udp_dest, &c->udp_src, sizeof(rpc->udp_dest));
        }

        pdu  = rpc_allocate_reply_pdu(rpc, &res, alloc_hint);
        if (pdu == NULL) {
                rpc_set_error(rpc, "Failed to send error_reply: %s",
                              rpc_get_error(rpc));
                return -1;
        }

        return rpc_queue_pdu(rpc, pdu);
}

static int rpc_process_call(struct rpc_context *rpc, ZDR *zdr)
{
	struct _rpc_msg c;
        struct rpc_endpoint *endpoint;
        int i, min_version = 0, max_version = 0, found_program = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

        memset(&c.call, 0, sizeof(struct rpc_msg));
        if (rpc->is_udp) {
                memcpy(&c.udp_src, &rpc->udp_src, sizeof(rpc->udp_src));
        }
	if (zdr_callmsg(rpc, zdr, &c.call) == 0) {
		rpc_set_error(rpc, "Failed to decode CALL message. %s",
                              rpc_get_error(rpc));
                return rpc_send_error_reply(rpc, &c.call, GARBAGE_ARGS, 0, 0);
        }
        for (endpoint = rpc->endpoints; endpoint; endpoint = endpoint->next) {
                if (c.call.body.cbody.prog == endpoint->program) {
                        if (!found_program) {
                                min_version = max_version = endpoint->version;
                        }
                        if (endpoint->version < min_version) {
                                min_version = endpoint->version;
                        }
                        if (endpoint->version > max_version) {
                                max_version = endpoint->version;
                        }
                        found_program = 1;
                        if (c.call.body.cbody.vers == endpoint->version) {
                                break;
                        }
                }
        }
        if (endpoint == NULL) {
		rpc_set_error(rpc, "No endpoint found for CALL "
                              "program:0x%08x version:%d\n",
                              (int)c.call.body.cbody.prog,
                              (int)c.call.body.cbody.vers);
                if (!found_program) {
                        return rpc_send_error_reply(rpc, &c.call, PROG_UNAVAIL,
                                                    0, 0);
                }
                return rpc_send_error_reply(rpc, &c.call, PROG_MISMATCH,
                                            min_version, max_version);
        }
        for (i = 0; i < endpoint->num_procs; i++) {
                if (endpoint->procs[i].proc == c.call.body.cbody.proc) {
                        if (endpoint->procs[i].decode_buf_size) {
                                c.call.body.cbody.args = zdr_malloc(zdr, endpoint->procs[i].decode_buf_size);
                                memset(c.call.body.cbody.args, 0, endpoint->procs[i].decode_buf_size);
                        }
                        if (!endpoint->procs[i].decode_fn(zdr, c.call.body.cbody.args)) {
                                rpc_set_error(rpc, "Failed to unmarshall "
                                              "call payload");
                                return rpc_send_error_reply(rpc, &c.call, GARBAGE_ARGS, 0 ,0);
                        }
                        return endpoint->procs[i].func(rpc, &c.call, endpoint->procs[i].opaque);
                }
        }

        return rpc_send_error_reply(rpc, &c.call, PROC_UNAVAIL, 0 ,0);
}

struct rpc_pdu *rpc_find_pdu(struct rpc_context *rpc, uint32_t xid)
{
	struct rpc_pdu *pdu, *prev_pdu;
	struct rpc_queue *q;
	unsigned int hash;

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        /* First check outqueue */
	q = &rpc->outqueue;
	prev_pdu = NULL;
	for (pdu=q->head; pdu; pdu=pdu->next) {
		if (pdu->xid != xid) {
			prev_pdu = pdu;
			continue;
		}
		if (rpc->is_udp == 0 || rpc->is_broadcast == 0) {
			/* Singly-linked but we track head and tail */
			if (pdu == q->head)
				q->head = pdu->next;
			if (pdu == q->tail)
				q->tail = prev_pdu;
			if (prev_pdu != NULL)
				prev_pdu->next = pdu->next;
		}
                break;
        }
        if (pdu) {
                goto finished;
        }

	/* Look up the transaction in a hash table of our requests */
	hash = rpc_hash_xid(rpc, rpc->rm_xid[1]);
	q = &rpc->waitpdu[hash];

	/* Follow the hash chain.  Linear traverse singly-linked list,
	 * but track previous entry for optimised removal */
	prev_pdu = NULL;
	for (pdu=q->head; pdu; pdu=pdu->next) {
		if (pdu->xid != xid) {
			prev_pdu = pdu;
			continue;
		}
		if (rpc->is_udp == 0 || rpc->is_broadcast == 0) {
			/* Singly-linked but we track head and tail */
			if (pdu == q->head)
				q->head = pdu->next;
			if (pdu == q->tail)
				q->tail = prev_pdu;
			if (prev_pdu != NULL)
				prev_pdu->next = pdu->next;
			rpc->waitpdu_len--;
		}
                break;
        }
        
 finished:
        if (pdu) {
                pdu->next = NULL;
        }

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        return pdu;
}

int rpc_cancel_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu)
{
        /*
         * Use rpc_find_pdu() to locate it and remove it from the input list.
         */
        pdu = rpc_find_pdu(rpc, pdu->xid);
        if (pdu) {
                rpc_free_pdu(rpc, pdu);
                return 0;
        }

        return -ENOENT;
}

int rpc_process_pdu(struct rpc_context *rpc, char *buf, int size)
{
	ZDR zdr;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	memset(&zdr, 0, sizeof(ZDR));

	zdrmem_create(&zdr, buf, size, ZDR_DECODE);
        if (rpc->is_server_context) {
                int ret;

                ret = rpc_process_call(rpc, &zdr);
                zdr_destroy(&zdr);

                return ret;
        }

#ifdef HAVE_LIBKRB5
        /*
         * For KRB5P and iovectors, i.e. NFS[34]READ we
         * need to use a ZDR that hangs off the PDU so we can
         * access the ZDR and its buffers to manually copy
         * data into the iovectors.
         */
        if (rpc->pdu->zero_copy_iov && rpc->sec == RPC_SEC_KRB5P) {
                zdr_destroy(&rpc->pdu->zdr);
                zdrmem_create(&rpc->pdu->zdr, buf, size, ZDR_DECODE);
                if (rpc_process_reply(rpc, &rpc->pdu->zdr) != 0) {
                        rpc_set_error(rpc, "rpc_procdess_reply failed (for krb5 read)");
                }
        } else
#endif /* HAVE_LIBKRB5 */
                if (rpc_process_reply(rpc, &zdr) != 0) {
                        rpc_set_error(rpc, "rpc_procdess_reply failed");
                }

        if (rpc->fragments == NULL && rpc->pdu && rpc->pdu->in.base) {
                memcpy(&rpc->pdu->zdr, &zdr, sizeof(zdr));
                rpc->pdu->free_zdr = 1;
        } else {
                zdr_destroy(&zdr);
        }
        return 0;
}

