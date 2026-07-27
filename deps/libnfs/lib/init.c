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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include "slist.h"
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"

#ifdef HAVE_LIBKRB5
#include "krb5-wrapper.h"
#endif

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

static const char *oom = "out of memory";

uint64_t rpc_current_time(void)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
	return (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
#else
	return (uint64_t)time(NULL) * 1000;
#endif
}

uint64_t rpc_current_time_us(void)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &tp);
	return (uint64_t)tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
#else
	return (uint64_t)time(NULL) * 1000000;
#endif
}

int rpc_set_hash_size(struct rpc_context *rpc, int hashes)
{
	uint32_t i;

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        rpc->num_hashes = hashes;
        free(rpc->waitpdu);
	rpc->waitpdu = malloc(sizeof(struct rpc_queue) * rpc->num_hashes);
        if (rpc->waitpdu == NULL) {
                return -1;
        }
	for (i = 0; i < rpc->num_hashes; i++)
		rpc_reset_queue(&rpc->waitpdu[i]);
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
        return 0;
}

int nfs_set_hash_size(struct nfs_context *nfs, int hashes)
{
        return rpc_set_hash_size(nfs->rpc, hashes);
}

struct rpc_context *rpc_init_context(void)
{
	struct rpc_context *rpc;
	static uint32_t salt = 0;

	rpc = calloc(1, sizeof(struct rpc_context));
	if (rpc == NULL) {
		return NULL;
	}

	if (rpc_set_hash_size(rpc, DEFAULT_HASHES)) {
                free(rpc);
		return NULL;
	}

	rpc->magic = RPC_CONTEXT_MAGIC;
        rpc->inpos  = 0;
        rpc->state = READ_RM;

#ifdef HAVE_MULTITHREADING
	nfs_mt_mutex_init(&rpc->rpc_mutex);
#ifndef HAVE_STDATOMIC_H
	nfs_mt_mutex_init(&rpc->atomic_int_mutex);
#endif
#endif /* HAVE_MULTITHREADING */

 	rpc->auth = authunix_create_default();
	if (rpc->auth == NULL) {
		free(rpc->waitpdu);
		free(rpc);
		return NULL;
	}
	// Add PID to rpc->xid for easier debugging, making sure to cast
	// pid to 32-bit type to avoid invalid left-shifts.
	rpc->xid = salt + (uint32_t)rpc_current_time() + ((uint32_t)getpid() << 16);
	salt += 0x01000000;
	rpc->fd = -1;
	rpc->tcp_syncnt = RPC_PARAM_UNDEFINED;
#if defined(WIN32) || defined(ANDROID) || defined(PS3_PPU)
	rpc->uid = 65534;
	rpc->gid = 65534;
#else
	rpc->uid = getuid();
	rpc->gid = getgid();
#endif
	rpc_reset_queue(&rpc->outqueue);
	/* Default is no limit */
	rpc->max_waitpdu_len = 0;

	/*
	 * Default RPC timeout is 60 secs, but it will later be updated if user
	 * has passed the timeo=<int> mount option. Another way to set the RPC
	 * timeout is by calling the nfs_set_timeout()/rpc_set_timeout()
	 * function but the mount option should be prefered by new users.
	 */
	rpc->timeout = 60 * 1000;

	/*
	 * Default value of retrans starts as 0, i.e., no retries.
	 * Only after mount completes successfully and the rpc_context is used
	 * for NFS requests, do we set rpc->retrans to the value set by retrans=<int>
	 * mount parameter. See rpc_set_resiliency().
	 */
	rpc->retrans = 0;

	/* Default is to timeout after 100ms of poll(2) */
	rpc->poll_timeout = 100;

	return rpc;
}

static int
is_nonblocking(int s)
{
#if defined(WIN32)
    return 0;
#else
	int v;
	v = fcntl(s, F_GETFL, 0);
	return (v & O_NONBLOCK) != 0;
#endif
}

struct rpc_context *rpc_init_server_context(int s)
{
	struct rpc_context *rpc;

	rpc = calloc(1, sizeof(struct rpc_context));
	if (rpc == NULL) {
		return NULL;
	}

	rpc->magic = RPC_CONTEXT_MAGIC;

	rpc->is_server_context = 1;
	rpc->fd = s;
	rpc->is_connected = 1;

	rpc->is_nonblocking = is_nonblocking(s);

        rpc->is_udp = rpc_is_udp_socket(rpc);
	rpc_reset_queue(&rpc->outqueue);

#ifdef HAVE_MULTITHREADING
        nfs_mt_mutex_init(&rpc->rpc_mutex);
#ifndef HAVE_STDATOMIC_H
	nfs_mt_mutex_init(&rpc->atomic_int_mutex);
#endif
#endif /* HAVE_MULTITHREADING */
	return rpc;
}

#ifdef HAVE_SO_BINDTODEVICE
void rpc_set_interface(struct rpc_context *rpc, const char *ifname)
{
	/*
	 * This only copies the interface information into the RPC
	 * structure.  It doesn't stop whatever interface is being used. The
	 * connection needs to be restarted for that happen. In other words,
	 * set this before you connect.
	 */
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (ifname) {
		/*
		 * Allow at one-less character just-in-case IFNAMSIZ for
		 * the defined platform does not include the NUL-terminator.
		 */
		strncpy(rpc->ifname, ifname, sizeof(rpc->ifname) - 1);
	}
}
#endif

void rpc_set_debug(struct rpc_context *rpc, int level)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->debug = level;
}

struct rpc_context *rpc_init_udp_context(void)
{
	struct rpc_context *rpc;

	rpc = rpc_init_context();
	if (rpc != NULL) {
		rpc->is_udp = 1;
	}

	return rpc;
}

int rpc_set_username(struct rpc_context *rpc, const char *username)
{
#ifdef HAVE_LIBKRB5
        free(discard_const(rpc->username));
        rpc->username = NULL;
                
        if (username == NULL) {
                return 0;
        }
        rpc->username = strdup(username);
	if (rpc->username == NULL) {
		rpc_set_error(rpc,
			      "Out of memory: Failed to allocate username");
		return -1;
	}
#endif
        return 0;
}

void rpc_set_auth(struct rpc_context *rpc, struct AUTH *auth)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->auth != NULL) {
		auth_destroy(rpc->auth);
	}
	rpc->auth = auth;
}

static void rpc_set_uid_gid(struct rpc_context *rpc, int uid, int gid) {
	if (uid != rpc->uid || gid != rpc->gid) {
		struct AUTH *auth = libnfs_authunix_create("libnfs", uid, gid, 0, NULL);
		if (auth != NULL) {
			rpc_set_auth(rpc, auth);
			rpc->uid = uid;
			rpc->gid = gid;
		}
	}
}

void rpc_set_uid(struct rpc_context *rpc, int uid) {
	rpc_set_uid_gid(rpc, uid, rpc->gid);
}

void rpc_set_gid(struct rpc_context *rpc, int gid) {
	rpc_set_uid_gid(rpc, rpc->uid, gid);
}

void rpc_set_auxiliary_gids(struct rpc_context *rpc, uint32_t len, uint32_t* gids) {
	struct AUTH *auth = libnfs_authunix_create("libnfs", rpc->uid, rpc->gid, len, gids);
	if (auth != NULL) {
		rpc_set_auth(rpc, auth);
	}
}

void rpc_set_error(struct rpc_context *rpc, const char *error_string, ...)
{
        va_list ap;
	char *old_error_string = NULL;
        
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	old_error_string = rpc->error_string;
        va_start(ap, error_string);
	rpc->error_string = malloc(1024);
        if (rpc->error_string == NULL) {
                rpc->error_string = discard_const(oom);
                goto finished;
        }
	vsnprintf(rpc->error_string, 1024, error_string, ap);
        va_end(ap);

	RPC_LOG(rpc, 1, "error: %s", rpc->error_string);

 finished:
        if (old_error_string && old_error_string != oom) {
                free(old_error_string);
        }
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
}

void rpc_set_error_locked(struct rpc_context *rpc, const char *error_string, ...)
{
	va_list ap;
	char *old_error_string = rpc->error_string;

	va_start(ap, error_string);
	rpc->error_string = malloc(1024);
        if (rpc->error_string == NULL) {
                free(old_error_string);
                rpc->error_string = discard_const(oom);
                return;
        }
	vsnprintf(rpc->error_string, 1024, error_string, ap);
	va_end(ap);

	RPC_LOG(rpc, 1, "error: %s", rpc->error_string);

        if (old_error_string && old_error_string != oom) {
                free(old_error_string);
        }
}

char *rpc_get_error(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return rpc->error_string ? rpc->error_string : "";
}

void rpc_get_stats(struct rpc_context *rpc, struct rpc_stats *stats)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        *stats = rpc->stats;

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        return;
}

static void rpc_purge_all_pdus(struct rpc_context *rpc, int status, const char *error)
{
	struct rpc_queue outqueue;
	struct rpc_pdu *pdu;
	int i;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Remove all entries from each queue before cancellation to prevent
	 * the callbacks manipulating entries that are about to be removed.
	 *
	 * This code assumes that the callbacks will not enqueue any new
	 * pdus when called.
	 */

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	outqueue = rpc->outqueue;

	rpc_reset_queue(&rpc->outqueue);
	while ((pdu = outqueue.head) != NULL) {
		outqueue.head = pdu->next;
                pdu->next = NULL;
                if (pdu->cb) {
                        pdu->cb(rpc, status, (void *) error, pdu->private_data);
                }
		rpc_free_pdu(rpc, pdu);
	}
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

	for (i = 0; i < rpc->num_hashes; i++) {
		struct rpc_queue waitqueue;

#ifdef HAVE_MULTITHREADING
                if (rpc->multithreading_enabled) {
                        nfs_mt_mutex_lock(&rpc->rpc_mutex);
                }
#endif /* HAVE_MULTITHREADING */
		waitqueue = rpc->waitpdu[i];
		rpc_reset_queue(&rpc->waitpdu[i]);
#ifdef HAVE_MULTITHREADING
                if (rpc->multithreading_enabled) {
                        nfs_mt_mutex_unlock(&rpc->rpc_mutex);
                }
#endif /* HAVE_MULTITHREADING */
		while((pdu = waitqueue.head) != NULL) {
			waitqueue.head = pdu->next;
			pdu->next = NULL;
                        if (pdu->cb) {
                                pdu->cb(rpc, status, (void *) error, pdu->private_data);
                        }
			rpc_free_pdu(rpc, pdu);
		}
	}

	assert(!rpc->outqueue.head);
}

void rpc_error_all_pdus(struct rpc_context *rpc, const char *error)
{
	rpc_purge_all_pdus(rpc, RPC_STATUS_ERROR, error);
}

static void rpc_free_fragment(struct rpc_fragment *fragment)
{
	if (fragment->data != NULL) {
		free(fragment->data);
	}
	free(fragment);
}

void rpc_free_all_fragments(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	while (rpc->fragments != NULL) {
	      struct rpc_fragment *fragment = rpc->fragments;

	      rpc->fragments = fragment->next;
	      rpc_free_fragment(fragment);
	}
}

int rpc_add_fragment(struct rpc_context *rpc, char *data, uint32_t size)
{
	struct rpc_fragment *fragment;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	fragment = malloc(sizeof(struct rpc_fragment));
	if (fragment == NULL) {
		return -1;
	}

	fragment->size = size;
	fragment->data = malloc(fragment->size);
	if(fragment->data == NULL) {
		free(fragment);
		return -1;
	}

	memcpy(fragment->data, data, fragment->size);
	LIBNFS_LIST_ADD_END(&rpc->fragments, fragment);
	return 0;
}

void rpc_destroy_context(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc_purge_all_pdus(rpc, RPC_STATUS_CANCEL, NULL);

	rpc_free_all_fragments(rpc);

        if (rpc->auth) {
                auth_destroy(rpc->auth);
                rpc->auth =NULL;
        }

	if (rpc->fd != -1) {
 		close(rpc->fd);
	}

	if (rpc->error_string && rpc->error_string != oom) {
		free(rpc->error_string);
		rpc->error_string = NULL;
	}

        free(rpc->waitpdu);
        rpc->waitpdu = NULL;
	free(rpc->inbuf);
	rpc->inbuf = NULL;

	rpc->magic = 0;
#ifdef HAVE_MULTITHREADING
        nfs_mt_mutex_destroy(&rpc->rpc_mutex);
#ifndef HAVE_STDATOMIC_H
        nfs_mt_mutex_destroy(&rpc->atomic_int_mutex);
#endif
#endif /* HAVE_MULTITHREADING */
#ifdef HAVE_LIBKRB5
        if (rpc->auth_data) {
                krb5_free_auth_data(rpc->auth_data);
        }
        free(discard_const(rpc->username));
        free(rpc->context);
#endif /* HAVE_LIBKRB5 */
	free(rpc->server);
	free(rpc);
}

void rpc_set_mountport(struct rpc_context *rpc, int port)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->mountport = port;
}

int rpc_get_mountport(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return rpc->mountport;
}

void rpc_set_poll_timeout(struct rpc_context *rpc, int poll_timeout)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->poll_timeout = poll_timeout;
}

int rpc_get_poll_timeout(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return rpc->poll_timeout;
}

void rpc_set_timeout(struct rpc_context *rpc, int timeout_msecs)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->timeout = timeout_msecs;
}

int rpc_get_timeout(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return rpc->timeout;
}

int rpc_register_service(struct rpc_context *rpc, int program, int version,
                         struct service_proc *procs, int num_procs)
{
        struct rpc_endpoint *endpoint;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (!rpc->is_server_context) {
		rpc_set_error(rpc, "Not a server context.");
                return -1;
        }

        endpoint = malloc(sizeof(*endpoint));
        if (endpoint == NULL) {
		rpc_set_error(rpc, "Out of memory: Failed to allocate endpoint "
                              "structure");
                return -1;
        }

        endpoint->program = program;
        endpoint->version = version;
        endpoint->procs = procs;
        endpoint->num_procs = num_procs;
        endpoint->next = rpc->endpoints;
        rpc->endpoints = endpoint;

        return 0;
}

void rpc_free_iovector(struct rpc_context *rpc, struct rpc_io_vectors *v)
{
        int i;

        assert(v->niov <= v->iov_capacity);

        for (i = 0; i < v->niov; i++) {
                if (v->iov[i].free) {
                        v->iov[i].free(v->iov[i].buf);
                }
        }
        v->niov = 0;

        if (v->iov != v->fast_iov) {
                assert(v->iov_capacity > RPC_FAST_VECTORS &&
                       v->iov_capacity <= RPC_MAX_VECTORS);
                free(v->iov);
        } else {
                assert(v->iov_capacity == RPC_FAST_VECTORS);
        }
}

int rpc_add_iovector(struct rpc_context *rpc, struct rpc_io_vectors *v,
                      char *buf, int len, void (*free)(void *))
{
        if (v->niov >= v->iov_capacity) {
                rpc_set_error(rpc, "Too many io vectors");
                return -1;
        }

        v->iov[v->niov].buf = buf;
        v->iov[v->niov].len = len;
        v->iov[v->niov].free = free;
        v->niov++;

        return 0;
}

/*
 * Advance the cursor by len bytes.
 * This must be called after reading len bytes into the cursor, so that the
 * subsequent data can be correctly read into iov[].
 */
void rpc_advance_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
			size_t len)
{
	while (len) {
		assert(v->iovcnt > 0);
		assert(v->remaining_size >= v->iov[0].iov_len);

		if (v->iov[0].iov_len > len) {
			v->iov[0].iov_base = ((uint8_t*) v->iov[0].iov_base) + len;
			v->iov[0].iov_len -= len;
			v->remaining_size -= len;
			break;
		} else {
			len -= v->iov[0].iov_len;
			v->remaining_size -= v->iov[0].iov_len;
			/* Exhausted this iovec completely */
			v->iov++;
			v->iovcnt--;
		}
	}

        /* remaining_size can only be 0 when iovcnt is 0 and v.v. */
	assert((v->iovcnt == 0) == (v->remaining_size == 0));
        assert(v->iovcnt <= v->iovcnt_ref);
        assert(v->iov >= v->base);
        assert(v->iov <= v->iov_ref);
        assert(v->iov_ref == (v->base + v->iovcnt_ref));
}

/*
 * Reduce the size of rpc_iovec_cursor to match new_len if the current size is
 * greater than new_len. If remaining_size <= new_len, then this is a no-op.
 */
void rpc_shrink_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
                       size_t new_len)
{
        int i;
        size_t num_done = 0;

        if (v->remaining_size <= new_len) {
                return;
        }

        for (i = 0; i < v->iovcnt && num_done < new_len; i++) {
                if (v->iov[i].iov_len <= (new_len - num_done)) {
                        num_done += v->iov[i].iov_len;
                        continue;
                }

                v->iov[i].iov_len = (new_len - num_done);
                num_done = new_len;
        }

        v->iovcnt = i;
        v->remaining_size = new_len;

        /* remaining_size can only be 0 when iovcnt is 0 and v.v. */
	assert((v->iovcnt == 0) == (v->remaining_size == 0));
        assert(v->iovcnt <= v->iovcnt_ref);
        assert(v->iov >= v->base);
        assert(v->iov <= v->iov_ref);
        assert(v->iov_ref == (v->base + v->iovcnt_ref));
}

/*
 * memcpy data into the cursor at the current position and advance the cursor
 * to be ready for subsequent data.
 */
void rpc_memcpy_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
		       const void *src, size_t len)
{
	while (len) {
		assert(v->iovcnt > 0);

		if (v->iov[0].iov_len > len) {
			memcpy(v->iov[0].iov_base, src, len);
			v->iov[0].iov_base = ((uint8_t*) v->iov[0].iov_base) + len;
			v->iov[0].iov_len -= len;
			v->remaining_size -= len;
			break;
		} else {
			memcpy(v->iov[0].iov_base, src, v->iov[0].iov_len);
			len -= v->iov[0].iov_len;
			src = ((uint8_t *) src) + v->iov[0].iov_len;
			v->remaining_size -= v->iov[0].iov_len;
			/* Exhausted this iovec completely */
			v->iov++;
			v->iovcnt--;
		}
	}

        /* remaining_size can only be 0 when iovcnt is 0 and v.v. */
	assert((v->iovcnt == 0) == (v->remaining_size == 0));
        assert(v->iovcnt <= v->iovcnt_ref);
        assert(v->iov >= v->base);
        assert(v->iov <= v->iov_ref);
        assert(v->iov_ref == (v->base + v->iovcnt_ref));
}

void rpc_reset_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v)
{
        int i;

        if (!v->base) {
                return;
        }

        assert(v->iovcnt <= v->iovcnt_ref);
        assert(v->iov >= v->base);
        assert(v->iov <= v->iov_ref);
        assert(v->iov_ref == (v->base + v->iovcnt_ref));

        v->iovcnt = v->iovcnt_ref;
        v->iov = v->base;

        v->remaining_size = 0;
        for (i = 0; i < v->iovcnt_ref; i++) {
                v->iov[i] = v->iov_ref[i];
                v->remaining_size += v->iov[i].iov_len;
        }
}

void rpc_free_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v)
{
	free(v->base);
}
