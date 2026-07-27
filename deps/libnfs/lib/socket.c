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

#ifdef __linux__
#define _GNU_SOURCE
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

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#if defined(HAVE_SYS_UIO_H) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/uio.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include "libnfs-zdr.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-private.h"
#include "slist.h"

#ifdef WIN32
//has to be included after stdlib!!
#include "win32_errnowrapper.h"
#endif

#ifndef MSG_NOSIGNAL
#if (defined(__APPLE__) && defined(__MACH__)) || defined(PS2_EE)
#define MSG_NOSIGNAL 0
#endif
#endif

static int
rpc_reconnect_requeue(struct rpc_context *rpc);

static int
create_socket(int domain, int type, int protocol)
{
#ifdef SOCK_CLOEXEC
#ifdef __linux__
    /* Linux-specific extension (since 2.6.27): set the
	   close-on-exec flag on all sockets to avoid leaking file
	   descriptors to child processes */
	int fd = socket(domain, type|SOCK_CLOEXEC, protocol);
	if (fd >= 0 || errno != EINVAL) {
                if (type == SOCK_DGRAM) {
                        int opt;
                        if (domain == AF_INET) {
                                opt = 1;
                                setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt));
                        } else {
                                opt = 1;
                                setsockopt(fd, IPPROTO_IPV6, IPV6_PKTINFO, &opt, sizeof(opt));
                        }
                }
		return fd;
        }
#endif
#endif

	return socket(domain, type, protocol);
}

static int
set_nonblocking(int fd)
{
	int v = 0;
#if defined(WIN32)
	u_long nonblocking=1;
	v = ioctl(fd, FIONBIO, &nonblocking);
#else
	v = fcntl(fd, F_GETFL, 0);
	v = fcntl(fd, F_SETFL, v | O_NONBLOCK);
#endif
	return v;
}

static void
set_nolinger(int fd)
{
#if !defined(PS2_EE)        
	struct linger lng;
	lng.l_onoff = 1;
	lng.l_linger = 0;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&lng, sizeof(lng));
#endif
}

static int
set_bind_device(int fd, char *ifname)
{
	int rc = 0;

#ifdef HAVE_SO_BINDTODEVICE
	if (*ifname) {
		rc = setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, ifname,
                                strlen(ifname));
	}

#endif
	return rc;
}

#ifdef HAVE_NETINET_TCP_H
static int
set_tcp_sockopt(int sockfd, int optname, int value)
{
	int level;

	#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__sun) || (defined(__APPLE__) && defined(__MACH__))
	struct protoent *buf;

	if ((buf = getprotobyname("tcp")) != NULL)
		level = buf->p_proto;
	else
		return -1;
	#else
		level = SOL_TCP;
	#endif

	return setsockopt(sockfd, level, optname, (char *)&value,
                          sizeof(value));
}

static int
set_keepalive(struct rpc_context *rpc, int sockfd)
{
#ifdef SO_KEEPALIVE
	const int enable_keepalive = 1;

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
		       &enable_keepalive, sizeof(enable_keepalive)) != 0) {
		RPC_LOG(rpc, 2, "setsockopt(SO_KEEPALIVE) failed: %s", strerror(errno));
		return -1;
	}
#endif

	/*
	 * Following code uses Linux specific socket options to change keepalive
	 * settings for the socket.
	 *
	 * TODO: Add for non-Linux clients.
	 */

#if defined(TCP_KEEPIDLE)
	{
		/* First keepalive probe after 60 secs of inactivity */
		const int keepidle_secs = 60;

		if (set_tcp_sockopt(sockfd, TCP_KEEPIDLE, keepidle_secs) != 0) {
			RPC_LOG(rpc, 2, "setsockopt(TCP_KEEPIDLE) failed: %s", strerror(errno));
			return -1;
		}
	}
#endif

#if defined(TCP_KEEPINTVL)
	{
		/* Send keepalive probe every 60 secs */
		const int keepinterval_secs = 60;

		if (set_tcp_sockopt(sockfd, TCP_KEEPINTVL, keepinterval_secs) != 0) {
			RPC_LOG(rpc, 2, "setsockopt(TCP_KEEPINTVL) failed: %s", strerror(errno));
			return -1;
		}
	}
#endif

#if defined(TCP_KEEPCNT)
	{
		/* Terminate connection after 3 failed keepalives */
		const int keepcnt = 3;

		if (set_tcp_sockopt(sockfd, TCP_KEEPCNT, keepcnt) != 0) {
			RPC_LOG(rpc, 2, "setsockopt(TCP_KEEPCNT) failed: %s", strerror(errno));
			return -1;
		}
	}
#endif

	return 0;
}
#endif /* HAVE_NETINET_TCP_H */

int
rpc_get_fd(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->old_fd) {
		return rpc->old_fd;
	}

	return rpc->fd;
}

static int
rpc_has_queue(struct rpc_queue *q)
{
	return q->head != NULL;
}

int
rpc_which_events(struct rpc_context *rpc)
{
	int events;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	events = rpc->is_connected ? POLLIN : POLLOUT;

	if (rpc->is_udp != 0) {
		/* for udp sockets we only wait for pollin */
		return POLLIN;
	}

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	if (rpc_has_queue(&rpc->outqueue)) {
		events |= POLLOUT;
	}
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	return events;
}

void
rpc_disable_socket(struct rpc_context *rpc, int val)
{
        rpc->socket_disabled = val;
}

int
rpc_write_to_socket(struct rpc_context *rpc)
{
        struct rpc_pdu *pdu;
        struct iovec fast_iov[RPC_FAST_VECTORS];
        struct iovec *iov = fast_iov;
        int iovcnt = RPC_FAST_VECTORS;
        int ret = 0;

        assert(rpc->magic == RPC_CONTEXT_MAGIC);
        if (rpc->socket_disabled) {
                return 0;
        }
	if (rpc->fd == -1) {
		rpc_set_error(rpc, "trying to write but not connected");
		return -1;
	}

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        /* Write several pdus at once */
        while ((rpc->max_waitpdu_len == 0 ||
                rpc->max_waitpdu_len > rpc->waitpdu_len) &&
               (pdu = rpc->outqueue.head) != NULL) {
                int niov = 0;
                uint32_t num_pdus = 0;
                char *last_buf = NULL;
                ssize_t count;

                assert(pdu->out.niov <= pdu->out.iov_capacity);
                assert(pdu->out.iov_capacity <= RPC_MAX_VECTORS);

                if (pdu->out.niov > iovcnt && iovcnt != RPC_MAX_VECTORS) {
                        assert(iov == fast_iov);
                        iov = (struct iovec *) calloc(RPC_MAX_VECTORS,
                                                      sizeof(struct iovec));
                        /*
                         * If allocation fails, continue with smaller iov.
                         * It'll require more writev() calls to send out one
                         * pdu, but it'll work.
                         */
                        if (iov != NULL) {
                            iovcnt = RPC_MAX_VECTORS;
                        } else {
                            iov = fast_iov;
                            iovcnt = RPC_FAST_VECTORS;
                        }
                }

                do {
                        size_t num_done = pdu->out.num_done;
                        int pdu_niov = pdu->out.niov;
                        int i;

                        /* Fully sent PDU should not be sitting in outqueue */
                        assert(num_done < pdu->out.total_size);

                        for (i = 0; i < pdu_niov; i++) {
                                char *buf = pdu->out.iov[i].buf;
                                size_t len = pdu->out.iov[i].len;
                                if (num_done >= len) {
                                        num_done -= len;
                                        continue;
                                }
                                buf += num_done;
                                len -= num_done;
                                num_done = 0;

                                /* Concatenate continous blocks */
                                if (last_buf != buf) {
                                        iov[niov].iov_base = buf;
                                        iov[niov].iov_len = len;
                                        niov++;
                                        if (niov >= iovcnt)
                                                break;
                                        last_buf = (buf + len);
                                } else {
                                        iov[niov - 1].iov_len += len;
                                        last_buf += len;
                                }
                        }

                        num_pdus++;
                        pdu = pdu->next;
                } while ((rpc->max_waitpdu_len == 0 ||
                          rpc->max_waitpdu_len > (rpc->waitpdu_len + num_pdus)) &&
                         pdu != NULL && niov < iovcnt);

                /*
                 * We must never be doing 0-byte writes as those can get into
                 * infinite loop.
                 */
                assert(niov > 0);

                count = writev(rpc->fd, iov, niov);
                if (count == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                ret = 0;
                                 goto finished;

                        }
                        rpc_set_error_locked(rpc, "Error when writing to "
					     "socket :%d %s", errno,
					     rpc_get_error(rpc));
                        ret = -1;
                        goto finished;
                }

                /* Check how many pdu we completed */
                while (count > 0 && (pdu = rpc->outqueue.head) != NULL) {
                        size_t remaining = (pdu->out.total_size - pdu->out.num_done);
                        if (remaining <= count) {
                                unsigned int hash;

                                count -= remaining;

                                pdu->out.num_done = pdu->out.total_size;

                                rpc->outqueue.head = pdu->next;
                                if (rpc->outqueue.head == NULL)
                                        rpc->outqueue.tail = NULL;

                                /* RPC sent, original or retransmit */
                                INC_STATS(rpc, num_req_sent);

                                if (pdu->discard_after_sending) {
                                        rpc_free_pdu(rpc, pdu);
                                        ret = 0;
                                        goto finished;
                                }

                                hash = rpc_hash_xid(rpc, pdu->xid);
                                rpc_enqueue(&rpc->waitpdu[hash], pdu);
                                rpc->waitpdu_len++;

                                pdu->pdu_stats.send_timestamp = rpc_current_time_us();
                                if (rpc->stats_cb) {
                                        rpc->stats_cb(rpc, &pdu->pdu_stats, rpc->stats_private_data);
                                }
                        } else {
                                pdu->out.num_done += count;
                                break;
                        }
                }
	}

 finished:
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        /* Free iov if dynamically allocated */
        if (iov != fast_iov) {
                assert(iovcnt > RPC_FAST_VECTORS);
                free(iov);
        }

	return ret;
}

static int adjust_inbuf(struct rpc_context *rpc, uint32_t pdu_size)
{
        char *buf;

        if (rpc->inbuf_size < pdu_size) {
                if (pdu_size > NFS_MAX_XFER_SIZE + 4096) {
                        rpc_set_error(rpc, "Incoming PDU exceeds limit of %d "
                                      "bytes.", NFS_MAX_XFER_SIZE + 4096);
                        return -1;
                }
                buf = realloc(rpc->inbuf, pdu_size);
                if (buf == NULL) {
                        rpc_set_error(rpc, "Failed to allocate buffer of %d "
                                      "bytes for pdu, errno:%d. Closing "
                                      "socket.", (int)pdu_size, errno);
                        return -1;
                }
                rpc->inbuf_size = pdu_size;
                rpc->inbuf = buf;
        }
        return 0;
}

static char *rpc_reassemble_pdu(struct rpc_context *rpc, uint32_t *pdu_size)
{
        struct rpc_fragment *fragment;
 	char *reasbuf = NULL, *ptr;
        uint32_t size;

        size = rpc->inpos;
        for (fragment = rpc->fragments; fragment; fragment = fragment->next) {
                size += fragment->size;
                if (size < fragment->size) {
                        rpc_set_error(rpc, "Fragments too large");
                        rpc_free_all_fragments(rpc);
                        return NULL;
                }
        }

        reasbuf = malloc(size);
        if (reasbuf == NULL) {
                rpc_set_error(rpc, "Failed to reassemble PDU");
                rpc_free_all_fragments(rpc);
                return NULL;
        }
        ptr = reasbuf;
        for (fragment = rpc->fragments; fragment; fragment = fragment->next) {
                memcpy(ptr, fragment->data, fragment->size);
                ptr += fragment->size;
        }
        memcpy(ptr, rpc->inbuf, rpc->inpos);

        *pdu_size = size;
        return reasbuf;
}

static void rpc_finished_pdu(struct rpc_context *rpc)
{
        if (rpc->pdu && rpc->pdu->free_pdu) {
                /*
                 * For zero-copy read, this is where we call the user callback.
                 */
                rpc->pdu->cb(rpc, RPC_STATUS_SUCCESS, rpc->pdu->zdr_decode_buf, rpc->pdu->private_data);
        }
        if (rpc->pdu && rpc->pdu->free_zdr) {
                zdr_destroy(&rpc->pdu->zdr);
        }
        rpc->state = READ_RM;
        rpc->inpos  = 0;
        if (rpc->pdu && (rpc->is_udp == 0 || rpc->is_broadcast == 0)) {
                rpc_free_pdu(rpc, rpc->pdu);
                rpc->pdu = NULL;
        }
}

/*
 * ZeroCopyReadPreamble. The maximum amount of head data we read for a PDU
 * assuming that all onc-rpc and protocol layer headers will fit inside this
 * preamble and that all the remaining data will be READ3/4 payload.
 */
#define ZCRP 1024

#define MAX_UDP_SIZE 65536
#define MAX_FRAGMENT_SIZE 8*1024*1024
static int
rpc_read_from_socket(struct rpc_context *rpc)
{
	ssize_t count;
        int pos;
        uint32_t inbuf_size;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);
        if (rpc->socket_disabled) {
                return 0;
        }
	if (rpc->is_udp) {
		socklen_t socklen = sizeof(rpc->udp_src);
                char *buf = NULL;

		buf = malloc(MAX_UDP_SIZE);
		if (buf == NULL) {
			rpc_set_error(rpc, "Failed to malloc buffer for "
                                      "recvfrom");
			return -1;
		}
#ifdef __linux__
                struct sockaddr_in *sin;
                struct sockaddr_in6 *sin6;
                char cmbuf[0x100];
                struct iovec iov = { buf, MAX_UDP_SIZE };
                struct msghdr mh = {
                        .msg_iov = &iov,
                        .msg_iovlen = 1,
                        .msg_name = &rpc->udp_src,
                        .msg_namelen = socklen,
                        .msg_control = cmbuf,
                        .msg_controllen = sizeof(cmbuf),
                };
                count = recvmsg(rpc->fd, &mh, 0);
                for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh);
                     cmsg != NULL;
                     cmsg = CMSG_NXTHDR(&mh, cmsg)) {
                        if (cmsg->cmsg_type != IP_PKTINFO) {
                                continue;
                        }
                        switch (cmsg->cmsg_level) {
                        case IPPROTO_IP:
                                sin = (struct sockaddr_in *)&rpc->udp_dst;
                                sin->sin_family = AF_INET;
                                sin->sin_addr.s_addr = ((struct in_pktinfo *)CMSG_DATA(cmsg))->ipi_addr.s_addr;
                                break;
                        case IPPROTO_IPV6:
                                sin6 = (struct sockaddr_in6 *)&rpc->udp_dst;
                                sin6->sin6_family = AF_INET6;
                                memcpy(&sin6->sin6_addr.s6_addr[0], &((struct in6_pktinfo *)CMSG_DATA(cmsg))->ipi6_addr.s6_addr[0], 16);
                                break;
                        }
                        break;
                }
#else /* __linux__ */
		count = recvfrom(rpc->fd, buf, MAX_UDP_SIZE, MSG_DONTWAIT,
                                 (struct sockaddr *)&rpc->udp_src, &socklen);
#endif /* __linux__ */
		if (count == -1) {
			free(buf);
			if (errno == EINTR || errno == EAGAIN) {
				return 0;
			}
			rpc_set_error(rpc, "Failed recvfrom: %s",
                                      strerror(errno));
			return -1;
		}
		if (!rpc->is_server_context) {
			rpc->rm_xid[0] = count;
			rpc->rm_xid[1] = ntohl(*(uint32_t *)(void *)&buf[0]);
			rpc->pdu = rpc_find_pdu(rpc, ntohl(*(uint32_t *)(void *)&buf[0]));
			if (rpc->pdu == NULL) {
				rpc_set_error(rpc, "Failed to match incoming PDU/XID."
						" Ignoring PDU");
				free(buf);
				return 0;
			}
		}
		if (rpc_process_pdu(rpc, buf, count) != 0) {
			rpc_set_error(rpc, "Invalid/garbage pdu received from "
                                      "server. Ignoring PDU");
			free(buf);
			return -1;
		}
		free(buf);
		return 0;
	}

        while (1){
                if (rpc->inpos == 0) {
                        switch (rpc->state) {
                        case READ_RM:
                                /*
                                 * Read record marker,
                                 * And if this is a cleint context read the next 4 bytes
                                 * i.e. the XID on a client
                                 */
                                rpc->pdu_size = 8;
                                rpc->buf = (char *)&rpc->rm_xid[0];
                                rpc->pdu = NULL;
                                break;
                        case READ_PAYLOAD:
                                /* we already read 4 bytes into the buffer */
                                rpc->inpos = 4;
                                rpc->pdu_size = rpc->rm_xid[0];
                                rpc->buf = rpc->inbuf + rpc->inpos;

                                /*
                                 * If it is a READ pdu, just read part of the data
                                 * to the buffer and read the remainder directly into
                                 * the application iovec. ZCRP is big enough to
                                 * "guarantee" that we get the whole onc-rpc as well
                                 * as the read3res header into the buffer.
                                 * I don't want to have to deal with reading too
                                 * little here and having to increase the limit and
                                 * restart unmarshalling from scratch.
                                 */
                                /* We do not have rpc->pdu for server context */
#ifdef HAVE_LIBKRB5
                                /*
                                 * KRB5P can not use zero-copy reads
                                 */
                                if (rpc->sec != RPC_SEC_KRB5P)
#endif /* HAVE_LIBKRB5 */
                                        if (rpc->pdu && rpc->pdu->in.base && rpc->pdu_size > ZCRP) {
                                                rpc->pdu_size = ZCRP;
                                        }
                                break;
                        case READ_UNKNOWN:
                        case READ_FRAGMENT:
                                /* we already read 4 bytes into the buffer */
                                rpc->inpos = 4;
                                rpc->pdu_size = rpc->rm_xid[0];
                                rpc->buf = rpc->inbuf + rpc->inpos;
                                assert(rpc->pdu_size <= rpc->inbuf_size);
                                break;
                        case READ_IOVEC:
                                /*
                                 * Set rpc->buf to NULL to convey to the following
                                 * code that data must be read into the vector buffer
                                 * rpc->pdu->in.iov instead.
                                 */
                                rpc->buf = NULL;
                                rpc->pdu_size = rpc->pdu->read_count;
                                break;
                        case READ_PADDING:
                                /* rm_xid[0] is clamped to be the remaining
                                 * amount of data after we have processed
                                 * all payload and all iovecs
                                 */
                                rpc->pdu_size = rpc->rm_xid[0];
                                rpc->buf = rpc->inbuf;
                                break;
                        }
                }

                count = rpc->pdu_size - rpc->inpos;
                /*
                 * When reading padding, clamp this so we do not overwrite
                 * rpc->inbuf/rpc->inbuf_size which we use as the garbage buffer
                 */
                if (rpc->state == READ_PADDING) {
                        rpc->buf = rpc->inbuf;
                        if (count > rpc->inbuf_size) {
                                count = rpc->inbuf_size;
                        }
                }

                if (rpc->buf) {
                        count = recv(rpc->fd, rpc->buf, count, MSG_DONTWAIT);
                } else {
                        assert(rpc->pdu->in.iovcnt > 0);
                        assert(count <= rpc->pdu->in.remaining_size);
                        count = readv(rpc->fd, rpc->pdu->in.iov, rpc->pdu->in.iovcnt);
                }

                if (count < 0) {
                        /*
                         * No more data to read so we can break out of
                         * the loop and return.
                         */
			if (errno == EINTR || errno == EAGAIN) {
				break;
			}
			rpc_set_error(rpc, "Read from socket(%d) failed, errno:%d (%s). "
                                      "Closing socket.", rpc->fd, errno, strerror(errno));
			RPC_LOG(rpc, 2, "Read from socket(%d) failed, errno:%d (%s). "
				"Closing socket.", rpc->fd, errno, strerror(errno));
			return -1;
		}
		if (count == 0) {
			rpc_set_error(rpc, "Remote side closed connection for socket fd %d",
				      rpc->fd);
			RPC_LOG(rpc, 2, "Remote side closed connection for socket fd %d",
				rpc->fd);
			/* remote side has closed the socket. Reconnect. */
			return -1;
		}
		rpc->inpos += count;

		if (rpc->buf) {
			rpc->buf += count;
                } else {
                        rpc_advance_cursor(rpc, &rpc->pdu->in, count);
                }
                
                if (rpc->inpos == rpc->pdu_size) {
                        switch (rpc->state) {
                        case READ_RM:
                                /* We have just read the record marker */
                                rpc->rm_xid[0] = ntohl(rpc->rm_xid[0]);
                                if (rpc->rm_xid[0] & 0x80000000) {
                                        rpc->state = READ_PAYLOAD;
                                } else {
                                        rpc_set_error(rpc, "Fragment support not yet working");
                                        rpc->state = READ_FRAGMENT;
                                        return -1;
                                }
                                rpc->rm_xid[0] &= 0x7fffffff;
                                if (rpc->rm_xid[0] < 8 || rpc->rm_xid[0] > MAX_FRAGMENT_SIZE) {
                                        rpc_set_error(rpc, "Invalid recordmarker size");
                                        return -1;
                                }

                                /*
                                 * When performing zero-copy read we read just ZCRP bytes
                                 * into rpc->inbuf and read rest of the data directly into
                                 * user provided buffers, so we just need to allocate
                                 * inbuf large enough to hold ZCRP bytes of data, plus 4
                                 * bytes for the XID, i.e., ZCRP+4 bytes.
                                 * RPC fragments are directly read into rpc->inbuf, no
                                 * zero copy, so we need to allocate space equal to the
                                 * fragment size. For non zero-copy reads also we need to
                                 * allocate the entire PDU size.
                                 */
                                inbuf_size = rpc->rm_xid[0];

                                rpc->rm_xid[1] = ntohl(rpc->rm_xid[1]);
                                if (!rpc->is_server_context) {
                                        rpc->pdu = rpc_find_pdu(rpc, rpc->rm_xid[1]);

#ifdef HAVE_LIBKRB5
                                        if (rpc->sec != RPC_SEC_KRB5P)
#endif /* HAVE_LIBKRB5 */
                                                if (rpc->state != READ_FRAGMENT && rpc->pdu && rpc->pdu->in.base) {
                                                        inbuf_size = ZCRP;
                                                }
                                }

                                if (adjust_inbuf(rpc, inbuf_size) != 0) {
                                        if (!rpc->is_server_context && rpc->pdu) {
                                                #ifdef HAVE_MULTITHREADING
                                                if (rpc->multithreading_enabled) {
                                                        nfs_mt_mutex_lock(&rpc->rpc_mutex);
                                                }
                                                #endif /* HAVE_MULTITHREADING */

                                                /*
                                                 * queue it back to outqueue for retransmit.
                                                 * Note that we don't need to queue it back to
                                                 * waitpdu[] queue as returning failure from
                                                 * here will force a reconnect, which will anyways
                                                 * re-queue everything from waitpdu[] to outqueue.
                                                 */
                                                rpc_return_to_outqueue(rpc, rpc->pdu);
                                                rpc->pdu = NULL;

                                                #ifdef HAVE_MULTITHREADING
                                                if (rpc->multithreading_enabled) {
                                                        nfs_mt_mutex_unlock(&rpc->rpc_mutex);
                                                }
                                                #endif /* HAVE_MULTITHREADING */
                                        }

                                        rpc_set_error(rpc, "adjust_inbuf failed for socket fd %d",
                                                      rpc->fd);
                                        RPC_LOG(rpc, 2, "adjust_inbuf failed for socket fd %d",
                                                rpc->fd);
                                        return -1;
                                }

                                /* Copy the next 4 bytes into inbuf */
                                *((uint32_t *)(void *)rpc->inbuf) = htonl(rpc->rm_xid[1]);

                                /* but set inpos to 0, we will update it above
                                 * that we have already read these 4 bytes in
                                 * PAYLOAD and FRAGMENT
                                 */
                                rpc->inpos = 0;   

                                if (!rpc->is_server_context) {
                                        /* Unknown xid, either unsolicited
                                         * or an xid we have cancelled
                                         */
                                        if (rpc->pdu == NULL) {
                                                rpc->state = READ_UNKNOWN;
                                                continue;
                                        }
                                }
                                continue;
                        case READ_FRAGMENT:
                                if (rpc_add_fragment(rpc, rpc->inbuf, rpc->inpos) != 0) {
                                        rpc_set_error(rpc, "Failed to queue fragment for reassembly.");
                                        return -1;
                                }
                                rpc->state = READ_RM;
                                rpc->inpos  = 0;
                                continue;
                        case READ_UNKNOWN:
                                rpc->state = READ_RM;
                                rpc->inpos  = 0;
                                continue;
                        case READ_PAYLOAD:
                                if (rpc->fragments) {
                                        rpc->buf = rpc_reassemble_pdu(rpc, &rpc->pdu_size);
                                        if (rpc->buf == NULL) {
                                                return -1;
                                        }
                                } else {
                                        rpc->buf = rpc->inbuf;
                                }
                                if (rpc_process_pdu(rpc, rpc->buf, rpc->pdu_size) != 0) {
                                        rpc_set_error(rpc, "Invalid/garbage pdu"
                                                      " received from server. "
                                                      "Closing socket");
                                        return -1;
                                }
#ifdef HAVE_LIBKRB5
                                /*
                                 * Since we don't do zero-copy reads for
                                 * KRB5P we are basically finished processing
                                 * the reply at this point.
                                 */
                                if (rpc->sec == RPC_SEC_KRB5P) {
                                        goto payload_finished;
                                }
#endif /* HAVE_LIBKRB5 */
                                /* We do not have rpc->pdu for server context */
                                if (rpc->pdu && rpc->pdu->free_zdr) {
                                        if (rpc->program == NFS_PROGRAM && rpc->version == NFS_V3) {
                                                /*
                                                 * If the READ failed, bail out here as there is no
                                                 * data.
                                                 */
                                                const READ3res *res = (READ3res *)(void *) rpc->pdu->zdr_decode_buf;
                                                if (res->status != NFS3_OK) {
                                                        goto payload_finished;
                                                }
                                        }

                                        /*
                                         * We are doing zero-copy read.
                                         * pdu->read_count is the amount of read data returned by
                                         * the server in this RPC response.
                                         */
                                        if (!zdr_uint32_t(&rpc->pdu->zdr, &rpc->pdu->read_count))
                                                return -1;

                                        /*
                                         * Now pos is pointing at the start of data, while rpc->inpos
                                         * is the total bytes we have read for this RPC response,
                                         * including the RPC header, so "rpc->inpos - pos" is the
                                         * number of data bytes read. This will be less than ZCRP,
                                         * since we clamped the read size to ZCRP above.
                                         */
                                        pos = zdr_getpos(&rpc->pdu->zdr);
                                        count = rpc->inpos - pos;
                                        assert(count <= ZCRP);
                                        /*
                                         * No sane server will return more read data than we asked for.
                                         * If the server is buggy and does send more, we discard the extra
                                         * data.
                                         */
                                        if (rpc->pdu->read_count > rpc->pdu->requested_read_count) {
                                                rpc->pdu->read_count = rpc->pdu->requested_read_count;
                                        }

                                        /*
                                         * Clamp count to the actual data read, minus any padding.
                                         */
                                        if (count > rpc->pdu->read_count) {
                                                count = rpc->pdu->read_count;
                                        }
                                        if (rpc->pdu->in.remaining_size > rpc->pdu->read_count) {
                                                /* we got a short read */
                                                rpc_shrink_cursor(rpc, &rpc->pdu->in, rpc->pdu->read_count);
                                                assert(rpc->pdu->in.remaining_size == rpc->pdu->read_count);
                                        }

                                        /* XXX With the above two clamps, can this still happen ? */
                                        if (rpc->pdu->in.remaining_size < count) {
                                                count = rpc->pdu->in.remaining_size;
                                        }
                                        rpc_memcpy_cursor(rpc, &rpc->pdu->in, &rpc->inbuf[pos], count);

                                        if (rpc->pdu->in.remaining_size == 0) {
                                                // handle padding?
                                        } else {
                                                rpc->pdu->read_count -= count;
                                                rpc->state = READ_IOVEC;
                                                rpc->inpos  = 0;
                                                rpc->rm_xid[0] -= pos + count;
                                                continue;
                                        }
                                }
                        payload_finished:
                                if (rpc->fragments) {
                                        free(rpc->buf);
                                        rpc->buf = NULL;
                                        rpc_free_all_fragments(rpc);
                                }
                                rpc_finished_pdu(rpc);
                                break;
                        case READ_IOVEC:
                                rpc->pdu->read_count -= rpc->pdu_size;
                                if (rpc->rm_xid[0] < rpc->pdu_size) {
                                        return -1;
                                }
                                rpc->rm_xid[0] -= rpc->pdu_size;
                                if (!rpc->rm_xid[0]) {
                                        rpc_finished_pdu(rpc);
                                        break;
                                }
                                rpc->state = READ_PADDING;
                                rpc->inpos  = 0;
                                continue;
                        case READ_PADDING:
                                rpc_finished_pdu(rpc);
                                break;
                        }
                }
	}

	return 0;
}

static void
maybe_call_connect_cb(struct rpc_context *rpc, int status)
{
	rpc_cb tmp_cb = rpc->connect_cb;

	if (rpc->connect_cb == NULL) {
		return;
	}

	rpc->connect_cb = NULL;
	tmp_cb(rpc, status, rpc->error_string, rpc->connect_data);
}

/*
 * Return value of -1 indicates that the timeout scan discovered one or more
 * RPCs with major timeout and caller MUST terminate the connection to try fix
 * things.
 */
static int
rpc_timeout_scan(struct rpc_context *rpc)
{
	struct rpc_pdu *pdu;
	struct rpc_pdu *next_pdu;
	uint64_t t = rpc_current_time();
	unsigned int i;
	/* Milliseconds since last successful RPC response on this transport */
	const int last_rpc_msecs =
		((rpc->last_successful_rpc_response == 0) ? -1 :
		 (t - rpc->last_successful_rpc_response));
	bool_t need_reconnect = FALSE;

        /*
         * Only scan once per second.
         */
        if (t <= rpc->last_timeout_scan + 1000) {
                return 0;
        }
        rpc->last_timeout_scan = t;

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

        /*
         * First check requests that have timed out while sitting in outqueue.
         * These have not been sent to the server so do not indicate any issue
         * with server or connection, hence we do not take any corrective
         * action based on these request timeouts.
         */
	for (pdu = rpc->outqueue.head; pdu; pdu = next_pdu) {
		next_pdu = pdu->next;

		if (pdu->timeout == 0) {
			/* no timeout for this pdu */
			continue;
		}
		if (t < pdu->timeout) {
			/* not expired yet */
			continue;
		}

		/* Timed out w/o being sent */
		INC_STATS(rpc, num_timedout_in_outqueue);

                /*
                 * rpc->retrans > 0 implies that user wants us to retransmit
                 * timed out RPCs. Note that we treat non-zero rpc->retrans
                 * as hard mount, so we just advance the timeout values for
                 * this RPC and leave it in the outqueue.
                 * Since these have not been sent to the server, they don't
                 * signify any issue with the server or the connection and
                 * hence major timeout has no special significance for such
                 * requests.
                 */
                if (!pdu->do_not_retry && rpc->retrans > 0) {
                        /*
                         * Ask pdu_set_timeout() to advance pdu->timeout and
                         * pdu->major_timeout. Note that major_timeout has no
                         * special significance for requests timing out in
                         * outqueue.
                         */
                        pdu->timeout = 0;
                        pdu->major_timeout = 0;
                        pdu_set_timeout(rpc, pdu, t);

                        RPC_LOG(rpc, 2, "[pdu %p] Request timed out in outqueue, "
                                "will send when connection allows!", pdu);
                } else {
		        rpc_remove_pdu_from_queue(&rpc->outqueue, pdu);
			rpc_set_error_locked(rpc, "command timed out");
                        if (pdu->cb) {
                                pdu->cb(rpc, RPC_STATUS_TIMEOUT,
                                        NULL, pdu->private_data);
                        }
			rpc_free_pdu(rpc, pdu);
		}
	}

	/*
	 * Now look for requests in waitpdu. These are requests which have
	 * been sent to server and we are awaiting response from the server.
	 * These may indicate an unresponsive server and/or bad connection.
	 * We log a message on major_timeout and try recovery by dropping
	 * existing connection and creting a new one.
	 */
	for (i = 0; i < rpc->num_hashes; i++) {
		struct rpc_queue *q;

                q = &rpc->waitpdu[i];
		for (pdu = q->head; pdu; pdu = next_pdu) {
			next_pdu = pdu->next;

			if (pdu->timeout == 0) {
				/* no timeout for this pdu */
				continue;
			}
			if (t < pdu->timeout && t < pdu->major_timeout) {
				/* not expired yet */
				continue;
			}

			/* Timed out waiting for response */
                        if (t >= pdu->timeout) {
                                INC_STATS(rpc, num_timedout);
                        }

                        rpc_remove_pdu_from_queue(q, pdu);
			rpc->waitpdu_len--;

			/*
			 * rpc->retrans > 0 implies that user wants us to
			 * retransmit timed out RPCs. We update the timeout
			 * values for these RPCs and move them to outqueue for
			 * retransmit.
			 */
			if (!pdu->do_not_retry && rpc->retrans > 0) {
				/* Ask pdu_set_timeout() to set pdu->timeout */
				pdu->timeout = 0;

				if (t >= pdu->major_timeout) {
					/* Timed out waiting for response */
					INC_STATS(rpc, num_major_timedout);

					/* Ask pdu_set_timeout() to set pdu->major_timeout */
					pdu->major_timeout = 0;
					if (!pdu->snr_logged) {
						/* Log only once for an RPC */
						pdu->snr_logged = TRUE;
						RPC_LOG(rpc, 1, "[pdu %p] Server %s "
							"not responding, still trying",
							pdu, rpc->server);
					}
					if (!need_reconnect) {
						need_reconnect = (last_rpc_msecs > rpc->timeout);
					}
				}
				/* Reset the RPC timeout values as appropriate */
				pdu_set_timeout(rpc, pdu, t);

				/* queue it back to outqueue for retransmit */
				rpc_return_to_outqueue(rpc, pdu);
			} else {
				// qqq move to a temporary queue and process after
				// we drop the mutex
				rpc_set_error_locked(rpc, "command timed out");
                                if (pdu->cb) {
                                        pdu->cb(rpc, RPC_STATUS_TIMEOUT,
                                                NULL, pdu->private_data);
                                }
				rpc_free_pdu(rpc, pdu);
			}
		}
	}
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

	if (need_reconnect) {
		RPC_LOG(rpc, 2, "rpc_timeout_scan: Recovery action needed for fd %d",
			rpc->fd);
	}

	return (need_reconnect ? -1 : 0);
}

int
rpc_service(struct rpc_context *rpc, int revents)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/*
	 * rpc_timeout_scan() will return -1 to indicate that we need to perform
	 * recovery action by reconnecting and queueing all RPCs on the new
	 * connection. Schedule reconnect and requeue and return. Once the new
	 * connection is ready, events will be processed for that.
	 */
	if (rpc_timeout_scan(rpc) != 0) {
		return rpc_reconnect_requeue(rpc);
	}

	if (revents == -1 || revents & (POLLERR|POLLHUP)) {
		if (revents != -1 && revents & POLLERR) {

#ifdef WIN32
			char err = 0;
#else
			int err = 0;
#endif
			socklen_t err_size = sizeof(err);

			if (getsockopt(rpc->fd, SOL_SOCKET, SO_ERROR,
				(char *)&err, &err_size) != 0 || err != 0) {
				if (err == 0) {
					err = errno;
				}
				rpc_set_error(rpc, "rpc_service: socket error "
						    "%s(%d).",
						    strerror(err), err);
			} else {
				rpc_set_error(rpc, "rpc_service: POLLERR, "
						   "Unknown socket error.");
			}
		} else if (revents != -1 && revents & POLLHUP) {
			rpc_set_error(rpc, "Socket failed with POLLHUP");
		}
		if (rpc->auto_reconnect) {
			return rpc_reconnect_requeue(rpc);
		}
		maybe_call_connect_cb(rpc, RPC_STATUS_ERROR);
		return -1;

	}

	if (rpc->is_connected == 0 && rpc->fd != -1 && (revents & POLLOUT)) {
		int err = 0;
		socklen_t err_size = sizeof(err);

		if (getsockopt(rpc->fd, SOL_SOCKET, SO_ERROR,
				(char *)&err, &err_size) != 0 || err != 0) {
			if (err == 0) {
				err = errno;
			}
			rpc_set_error(rpc, "rpc_service: socket error "
				  	"%s(%d) while connecting.",
					strerror(err), err);
			maybe_call_connect_cb(rpc, RPC_STATUS_ERROR);
			return -1;
		}

		rpc->is_connected = 1;
		RPC_LOG(rpc, 2, "connection established on fd %d", rpc->fd);
		maybe_call_connect_cb(rpc, RPC_STATUS_SUCCESS);
		return 0;
	}

#ifdef HAVE_TLS
	/*
	 * We perform TLS handshake in a nonblocking fashion, i.e., we don't
	 * block on recv() and send(), so if we get a POLLIN or POLLOUT event
	 * during TLS handshake we must advance the TLS handshake process by
	 * calling do_tls_handshake() again. do_tls_handshake() will return
	 * TLS_HANDSHAKE_IN_PROGRESS if it needs to wait for network IO, o/w
	 * it'll complete the handshake process.
	 * Note that do_tls_handshake() can correctly handle multiple calls and
	 * it can advance the handshake process till it either completes successfully
	 * or fails.
	 */
	if (rpc->tls_context.state == TLS_HANDSHAKE_IN_PROGRESS &&
			(revents & (POLLOUT | POLLIN))) {
		struct tls_cb_data *data = &rpc->tls_context.data;

		/* Should be only doing this for secure transport */
		assert(rpc->use_tls);

		rpc->tls_context.state = do_tls_handshake(rpc);

		switch (rpc->tls_context.state) {
			case TLS_HANDSHAKE_IN_PROGRESS:
				RPC_LOG(rpc, 2, "do_tls_handshake() returned "
						"TLS_HANDSHAKE_IN_PROGRESS on fd %d",
					rpc->fd);
				break;
			case TLS_HANDSHAKE_COMPLETED:
				RPC_LOG(rpc, 2, "do_tls_handshake() returned "
						"TLS_HANDSHAKE_COMPLETED on fd %d",
					rpc->fd);
				data->cb(rpc, RPC_STATUS_SUCCESS, NULL, data->private_data);
				break;
			case TLS_HANDSHAKE_FAILED:
				RPC_LOG(rpc, 1, "do_tls_handshake() returned "
						"TLS_HANDSHAKE_FAILED on fd %d",
					rpc->fd);
				data->cb(rpc, RPC_STATUS_ERROR, "TLS_HANDSHAKE_FAILED",
					 data->private_data);
				break;
			default:
				/* Should not return any other status */
				assert(0);
		}

		return 0;
	}
#endif /* HAVE_TLS */

	if (revents & POLLIN) {
		if (rpc_read_from_socket(rpc) != 0) {
                        if (rpc->is_server_context) {
                                return -1;
                        } else {
#ifdef HAVE_TLS
				/*
				 * TODO: read from ktls sockets will fail with EIO
				 *       if TLS records of type other than data
				 *       (e.g., alert or handshake) are received.
				 *       We will need to issue a recvmsg() call
				 *       with enough cmsg space to fetch the
				 *       record type and data correctly.
				 *       We can then log that here to help the
				 *       user. In any case the only valid course
				 *       of action is to terminate the connection
				 *       and reconnect so that we can correctly
				 *       re-auth.
				 */
#endif /* HAVE_TLS */
                                return rpc_reconnect_requeue(rpc);
                        }
		}
	}

#ifdef HAVE_TLS
	/*
	 * For secure NFS connections we should never write to the socket w/o
	 * properly completing the TLS handshake. Note that we do allow reads
	 * from the socket as we would want to read response to the AUTH_TLS
	 * NULL RPC.
	 */
	if (rpc->use_tls && (rpc->tls_context.state != TLS_HANDSHAKE_COMPLETED)) {
                RPC_LOG(rpc, 2, "TLS handshake state %d on fd %d, skipping socket write!",
			rpc->tls_context.state, rpc->fd);
                return 0;
        }
#endif

	if (revents & POLLOUT && rpc_has_queue(&rpc->outqueue)) {
		if (rpc_write_to_socket(rpc) != 0) {
                        if (rpc->is_server_context) {
                                return -1;
                        } else {
                                return rpc_reconnect_requeue(rpc);
                        }
		}
	}

	return 0;
}

/*
 * Set resiliency related paramters for the RPC context.
 * Following are the resiliency parameters for RPC transport:
 * 1. num_tcp_reconnect:
 *    Number of times TCP reconnection is allowed before giving up.
 *    -1 indicates retry indefinitely.
 * 2. timeout:
 *    How long we wait for an RPC response before retrying the RPC?
 *    0 or -1 indicates infinite timeout.
 * 3. retrans:
 *    Number of times an RPC is retried before we consider it a "major timeout"
 *    and take further recovery actions which might involve reconnection.
 */
void
rpc_set_resiliency(struct rpc_context *rpc,
		   int num_tcp_reconnect,
		   int timeout_msecs,
		   int retrans)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

        /* we can not connect and not reconnect on a server context. */
        if (rpc->is_server_context) {
                return;
        }

	rpc->auto_reconnect = num_tcp_reconnect;
	rpc->timeout = timeout_msecs;
	rpc->retrans = retrans;
}


void
rpc_set_tcp_syncnt(struct rpc_context *rpc, int v)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->tcp_syncnt = v;
}

#ifndef TCP_SYNCNT
#define TCP_SYNCNT        7
#endif

static int
rpc_connect_sockaddr_async(struct rpc_context *rpc)
{
        struct sockaddr_storage *s = &rpc->s;
        socklen_t socksize;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	switch (s->ss_family) {
	case AF_INET:
		socksize = sizeof(struct sockaddr_in);
		rpc->fd = create_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (set_bind_device(rpc->fd, rpc->ifname) != 0) {
			rpc_set_error (rpc, "Failed to bind to interface");
			return -1;
		}

#ifdef HAVE_NETINET_TCP_H
		if (rpc->tcp_syncnt != RPC_PARAM_UNDEFINED) {
			set_tcp_sockopt(rpc->fd, TCP_SYNCNT, rpc->tcp_syncnt);
		}
#endif
		break;
	case AF_INET6:
		socksize = sizeof(struct sockaddr_in6);
		rpc->fd = create_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		if (set_bind_device(rpc->fd, rpc->ifname) != 0) {
			rpc_set_error (rpc, "Failed to bind to interface");
			return -1;
		}

#ifdef HAVE_NETINET_TCP_H
		if (rpc->tcp_syncnt != RPC_PARAM_UNDEFINED) {
			set_tcp_sockopt(rpc->fd, TCP_SYNCNT, rpc->tcp_syncnt);
		}
#endif
		break;
	default:
		rpc_set_error(rpc, "Can not handle AF_FAMILY:%d", s->ss_family);
		return -1;
	}

	if (rpc->fd == -1) {
		rpc_set_error(rpc, "Failed to open socket");
		return -1;
	}

#if defined(HAVE_NETINET_TCP_H) && defined(TCP_NODELAY)
        set_tcp_sockopt(rpc->fd, TCP_NODELAY, 1);
#endif

	if (rpc->old_fd) {
#if !defined(WIN32) && !defined(PS3_PPU) && !defined(PS2_EE)
		if (dup2(rpc->fd, rpc->old_fd) == -1) {
			rpc_set_error(rpc, "dup2() failed: %s", strerror(errno));
			return -1;
		}
		close(rpc->fd);
		rpc->fd = rpc->old_fd;
#else
		/* On Windows dup2 does not work on sockets
		 * instead just close the old socket */
		close(rpc->old_fd);
		rpc->old_fd = 0;
#endif
	}

	/* Some systems allow you to set capabilities on an executable
	 * to allow the file to be executed with privilege to bind to
	 * privileged system ports, even if the user is not root.
	 *
	 * Opportunistically try to bind the socket to a low numbered
	 * system port in the hope that the user is either root or the
	 * executable has the CAP_NET_BIND_SERVICE.
	 *
	 * As soon as we fail the bind() with EACCES we know we will never
	 * be able to bind to a system port so we terminate the loop.
	 *
	 * On linux, use
	 *    sudo setcap 'cap_net_bind_service=+ep' /path/executable
	 * to make the executable able to bind to a system port.
	 *
	 * On Windows, there is no concept of privileged ports. Thus
	 * binding will usually succeed.
	 */
	{
		struct sockaddr_storage ss;
		struct sockaddr_in *sin;
#if !defined(PS3_PPU) && !defined(PS2_EE)		
		struct sockaddr_in6 *sin6;
#endif
		static int portOfs = 0;
		const int firstPort = 512; /* >= 512 according to Sun docs */
		const int portCount = IPPORT_RESERVED - firstPort;
		int startOfs, port, rc;

		sin  = (struct sockaddr_in *)&ss;
#if !defined(PS3_PPU) && !defined(PS2_EE)        
		sin6 = (struct sockaddr_in6 *)&ss;
#endif
		if (portOfs == 0) {
			portOfs = rpc_current_time() % 400;
		}
		startOfs = portOfs;
		do {
			rc = -1;
			port = htons(firstPort + portOfs);
			portOfs = (portOfs + 1) % portCount;

			/* skip well-known ports */
			if (!getservbyport(port, "tcp")) {
				memset(&ss, 0, sizeof(ss));

				switch (s->ss_family) {
				case AF_INET:
					sin->sin_port = port;
					sin->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_LEN
					sin->sin_len =
                                                sizeof(struct sockaddr_in);
#endif
					break;
#if !defined(PS3_PPU) && !defined(PS2_EE)
				case AF_INET6:
					sin6->sin6_port = port;
					sin6->sin6_family = AF_INET6;
#ifdef HAVE_SOCKADDR_LEN
					sin6->sin6_len =
                                                sizeof(struct sockaddr_in6);
#endif
					break;
#endif
				}

				rc = bind(rpc->fd, (struct sockaddr *)&ss,
                                          socksize);
#if !defined(WIN32)
				/* we got EACCES, so don't try again */
				if (rc != 0 && errno == EACCES)
					break;
#endif
			}
		} while (rc != 0 && portOfs != startOfs);
	}

	rpc->is_nonblocking = !set_nonblocking(rpc->fd);
	set_nolinger(rpc->fd);

#ifdef HAVE_NETINET_TCP_H
	/*
	 * Enable keepalive to detect and terminate dead connections when server
	 * TCP stops responding.
	 */
	if (set_keepalive(rpc, rpc->fd) != 0) {
		rpc_set_error(rpc, "Cannot enable keepalive for fd %d: %s",
                              rpc->fd, strerror(errno));
		return -1;
	}
#endif

	if (connect(rpc->fd, (struct sockaddr *)s, socksize) != 0 &&
            errno != EINPROGRESS) {
		rpc_set_error(rpc, "connect() to server failed. %s(%d)",
                              strerror(errno), errno);
		return -1;
	}

	return 0;
}

static int
rpc_set_sockaddr(struct rpc_context *rpc, const char *server, int port)
{
	struct addrinfo *ai = NULL;

	if (getaddrinfo(server, NULL, NULL, &ai) != 0) {
		rpc_set_error(rpc, "Invalid address:%s. "
			      "Can not resolv into IPv4/v6 structure.", server);
		return -1;
 	}

	switch (ai->ai_family) {
	case AF_INET:
		((struct sockaddr_in *)&rpc->s)->sin_family = ai->ai_family;
		((struct sockaddr_in *)&rpc->s)->sin_port = htons(port);
		((struct sockaddr_in *)&rpc->s)->sin_addr =
                        ((struct sockaddr_in *)(void *)(ai->ai_addr))->sin_addr;
#ifdef HAVE_SOCKADDR_LEN
		((struct sockaddr_in *)&rpc->s)->sin_len =
                        sizeof(struct sockaddr_in);
#endif
		break;
#if !defined(PS3_PPU) && !defined(PS2_EE)
	case AF_INET6:
		((struct sockaddr_in6 *)&rpc->s)->sin6_family = ai->ai_family;
		((struct sockaddr_in6 *)&rpc->s)->sin6_port = htons(port);
		((struct sockaddr_in6 *)&rpc->s)->sin6_addr =
                        ((struct sockaddr_in6 *)(void *)(ai->ai_addr))->sin6_addr;
#ifdef HAVE_SOCKADDR_LEN
		((struct sockaddr_in6 *)&rpc->s)->sin6_len =
                        sizeof(struct sockaddr_in6);
#endif
		break;
#endif
	}
	freeaddrinfo(ai);

        return 0;
}

int
rpc_connect_async(struct rpc_context *rpc, const char *server, int port,
                  rpc_cb cb, void *private_data)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

        if (rpc->is_server_context) {
		rpc_set_error(rpc, "Can not connect on a server context");
                return -1;
        }

	if (rpc->fd != -1) {
		rpc_set_error(rpc, "Trying to connect while already connected");
		return -1;
	}

	if (rpc->is_udp != 0) {
		rpc_set_error(rpc, "Trying to connect on UDP socket");
		return -1;
	}

	rpc->auto_reconnect = 0;

        if (rpc_set_sockaddr(rpc, server, port) != 0) {
                return -1;
        }

	rpc->connect_cb  = cb;
	rpc->connect_data = private_data;

	if (rpc_connect_sockaddr_async(rpc) != 0) {
		return -1;
	}

	return 0;
}

int
rpc_disconnect(struct rpc_context *rpc, const char *error)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->fd != -1) {
		close(rpc->fd);
		rpc->fd  = -1;
	}

	/* Do not re-disconnect if we are already disconnected */
	if (!rpc->is_connected) {
		return 0;
	}

	/* Turn off resiliency */
	rpc_set_resiliency(rpc, 0, rpc->timeout, 0);

	rpc->is_connected = 0;

        if (!rpc->is_server_context) {
                rpc_error_all_pdus(rpc, error);
        }

        maybe_call_connect_cb(rpc, RPC_STATUS_CANCEL);
	return 0;
}

#ifdef HAVE_TLS
/*
 * During TCP reconnection (either server or client closes connection) for secure
 * transport we need to perform the TLS handshake. This is the callback function
 * called when a TLS handshake performed during reconnection completes.
 */
static void
reconnect_cb_tls(struct rpc_context *rpc, int status,
		 void *command_data, void *private_data)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Must be called only for TLS transport */
	assert(rpc->use_tls);

	/* Must be called only after TLS handshake completes/fails */
	assert(rpc->tls_context.state == TLS_HANDSHAKE_COMPLETED ||
	       rpc->tls_context.state == TLS_HANDSHAKE_FAILED);

	/*
	 * If handshake failed, restart the entire TCP connection not just the handshake.
	 * This will create a new connection and perform the handshake.
	 */
	if (rpc->tls_context.state == TLS_HANDSHAKE_FAILED) {
		RPC_LOG(rpc, 1, "reconnect_cb_tls: TLS handshake failed, restarting connection!");

		if (rpc->fd != -1) {
			close(rpc->fd);
			rpc->fd  = -1;
		}
		rpc->is_connected = 0;
		rpc_reconnect_requeue(rpc);
		return;
	}

	RPC_LOG(rpc, 2, "reconnect_cb_tls: TLS handshake completed successfully!");
}
#endif

static void
reconnect_cb(struct rpc_context *rpc, int status, void *data,
             void *private_data)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		rpc_set_error(rpc, "Failed to reconnect async");
		rpc_reconnect_requeue(rpc);
		return;
	}

	rpc->is_connected = 1;
	rpc->connect_cb   = NULL;
	rpc->old_fd = 0;

#ifdef HAVE_TLS
	/*
	 * For secure NFS connections, we need to setup TLS session now.
	 */
	RPC_LOG(rpc, 2, "reconnect_cb called with status %d", status);
	if (rpc->use_tls) {
		if (rpc_null_task_authtls(rpc, rpc->nfs_version,
					  reconnect_cb_tls, NULL) == NULL) {
			RPC_LOG(rpc, 1, "reconnect_cb: rpc_null_task_authtls() failed, "
				"restarting connection!");
			/*
			 * Force reconnect so that we can time the retries using
			 * the existing rpc->num_retries. Forcing reconnect also
			 * has the advantage that it sets up a fresh TCP connection
			 * in case the older connection had some issues preventing
			 * successful TLS handshake.
			 */
			if (rpc->fd != -1) {
				close(rpc->fd);
				rpc->fd  = -1;
			}
			rpc->is_connected = 0;
			rpc_reconnect_requeue(rpc);
			return;
		}
	}
#endif /* HAVE_TLS */
}

/* Disconnect but do not error all PDUs, just move pdus in-flight back to the
 * outqueue and reconnect.
 */
static int
rpc_reconnect_requeue(struct rpc_context *rpc)
{
	struct rpc_pdu *pdu, *next;
	unsigned int i;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->auto_reconnect == 0) {
		RPC_LOG(rpc, 1, "reconnect is disabled");
		rpc_error_all_pdus(rpc, "RPC ERROR: Failed to reconnect async");
		return -1;
	}

	if (rpc->is_connected) {
		rpc->num_retries = rpc->auto_reconnect;
	}

	if (rpc->fd != -1) {
		rpc->old_fd = rpc->fd;
	}
	rpc->fd  = -1;
	rpc->is_connected = 0;

	if (rpc->outqueue.head) {
		rpc->outqueue.head->out.num_done = 0;
	}

	rpc->inpos = 0;
	rpc->state = READ_RM;

        /*
         * Drop all fragments on reconnect
         */
        rpc_free_all_fragments(rpc);
        
	/* Socket is closed so we will not get any replies to any commands
	 * in flight. Move them all over from the waitpdu queue back to the
         * out queue.
	 */
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	for (i = 0; i < rpc->num_hashes; i++) {
		struct rpc_queue *q = &rpc->waitpdu[i];
		for (pdu = q->head; pdu; pdu = next) {
			next = pdu->next;
			rpc_return_to_outqueue(rpc, pdu);
		}
		rpc_reset_queue(q);
	}
	rpc->waitpdu_len = 0;

       /*
        * If there's any half-read PDU, that needs to be restarted too.
        */
        if (rpc->pdu) {
                rpc_return_to_outqueue(rpc, rpc->pdu);
                rpc->pdu = NULL;
        }

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

	if (rpc->auto_reconnect < 0 || rpc->num_retries > 0) {
		rpc->num_retries--;
		rpc->connect_cb  = reconnect_cb;
		RPC_LOG(rpc, 1, "reconnect initiated");
		if (rpc_connect_sockaddr_async(rpc) != 0) {
			rpc_error_all_pdus(rpc, "RPC ERROR: Failed to "
                                           "reconnect async");
			return -1;
		}
		INC_STATS(rpc, num_reconnects);
		return 0;
	}

	RPC_LOG(rpc, 1, "reconnect: all attempts failed.");
	rpc_error_all_pdus(rpc, "RPC ERROR: All attempts to reconnect failed.");
	return -1;
}


int
rpc_bind_udp(struct rpc_context *rpc, char *addr, int port)
{
	struct addrinfo *ai = NULL;
	char service[6];

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->is_udp == 0) {
		rpc_set_error(rpc, "Cant not bind UDP. Not UDP context");
		return -1;
	}

	sprintf(service, "%d", port);
	if (getaddrinfo(addr, service, NULL, &ai) != 0) {
		rpc_set_error(rpc, "Invalid address:%s. "
			      "Can not resolv into IPv4/v6 structure.", addr);
		return -1;
 	}

	switch(ai->ai_family) {
	case AF_INET:
		rpc->fd = create_socket(ai->ai_family, SOCK_DGRAM, 0);
		if (rpc->fd == -1) {
			rpc_set_error(rpc, "Failed to create UDP socket: %s",
                                      strerror(errno));
			freeaddrinfo(ai);
			return -1;
		}

		if (bind(rpc->fd, (struct sockaddr *)ai->ai_addr,
                         sizeof(struct sockaddr_in)) != 0) {
			rpc_set_error(rpc, "Failed to bind to UDP socket: %s",
                                      strerror(errno));
			freeaddrinfo(ai);
			return -1;
		}
		break;
	default:
		rpc_set_error(rpc, "Can not handle UPD sockets of family %d "
                              "yet", ai->ai_family);
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	return 0;
}

int
rpc_set_udp_destination(struct rpc_context *rpc, char *addr, int port,
                        int is_broadcast)
{
	struct addrinfo *ai = NULL;
	char service[6];

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (rpc->is_udp == 0) {
		rpc_set_error(rpc, "Can not set destination sockaddr. Not UDP "
                              "context");
		return -1;
	}

	sprintf(service, "%d", port);
	if (getaddrinfo(addr, service, NULL, &ai) != 0) {
		rpc_set_error(rpc, "Invalid address:%s. "
			      "Can not resolv into IPv4/v6 structure.", addr);
		return -1;
 	}

	rpc->is_broadcast = is_broadcast;
	setsockopt(rpc->fd, SOL_SOCKET, SO_BROADCAST, (char *)&is_broadcast, sizeof(is_broadcast));

	memcpy(&rpc->udp_dest, ai->ai_addr, ai->ai_addrlen);
	freeaddrinfo(ai);

        if (!is_broadcast) {
                if (connect(rpc->fd, (struct sockaddr *)&rpc->udp_dest, sizeof(rpc->udp_dest)) != 0 && errno != EINPROGRESS) {
                        rpc_set_error(rpc, "connect() to UDP address failed. %s(%d)", strerror(errno), errno);
                        return -1;
                }
        }

	return 0;
}

struct sockaddr *
rpc_get_udp_src_sockaddr(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return (struct sockaddr *)&rpc->udp_src;
}

#ifdef __linux__
struct sockaddr *
rpc_get_udp_dst_sockaddr(struct rpc_context *rpc)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	return (struct sockaddr *)&rpc->udp_dst;
}
#endif

int
rpc_queue_length(struct rpc_context *rpc)
{
	int i = 0;
	struct rpc_pdu *pdu;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	for(pdu = rpc->outqueue.head; pdu; pdu = pdu->next) {
		i++;
	}

#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	i += rpc->waitpdu_len;
#ifdef HAVE_MULTITHREADING
        if (rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */

	return i;
}

int rpc_get_num_awaiting(struct rpc_context *rpc)
{
	return rpc->waitpdu_len;
}

void rpc_set_awaiting_limit(struct rpc_context *rpc, int limit)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->max_waitpdu_len = limit;
}

void
rpc_set_fd(struct rpc_context *rpc, int fd)
{
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	rpc->fd = fd;
}

int
rpc_is_udp_socket(struct rpc_context *rpc)
{
#ifdef WIN32
        char type = 0;
#else
        int type = 0;
#endif
        socklen_t len = sizeof(type);

        getsockopt(rpc->fd, SOL_SOCKET, SO_TYPE, &type, &len);
        return type == SOCK_DGRAM;
}
