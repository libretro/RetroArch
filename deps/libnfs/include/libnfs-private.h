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

#ifndef _LIBNFS_PRIVATE_H_
#define _LIBNFS_PRIVATE_H_

#ifdef HAVE_CONFIG_H
#include "config.h"  /* HAVE_SOCKADDR_STORAGE ? */
#endif

#if !defined(WIN32) && !defined(PS2_EE)
#include <sys/socket.h>  /* struct sockaddr_storage */
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_LIBKRB5
#include "lib/krb5-wrapper.h"
#endif

#ifdef HAVE_TLS
#include <gnutls/gnutls.h>
#endif

#if defined(WIN32) && !defined(IFNAMSIZ)
#define IFNAMSIZ 255
#endif

#if defined(PS3_PPU) && !defined(IFNAMSIZ)
#define IFNAMSIZ 16
#endif

#if defined(PS2_EE) && !defined(IFNAMSIZ)
#define IFNAMSIZ 16
#endif

#ifdef HAVE_MULTITHREADING
#ifdef HAVE_STDATOMIC_H
#include <stdatomic.h>
#define ATOMIC_INC(rpc, x) \
        atomic_fetch_add_explicit(&x, 1, memory_order_relaxed)
#define ATOMIC_DEC(rpc, x) \
        atomic_fetch_sub_explicit(&x, 1, memory_order_relaxed)
#else /* HAVE_STDATOMIC_H */
#define ATOMIC_INC(rpc, x)                              \
        if (rpc->multithreading_enabled) {              \
                nfs_mt_mutex_lock(&rpc->atomic_int_mutex);     \
        }                                               \
	x++;                                            \
        if (rpc->multithreading_enabled) {              \
                nfs_mt_mutex_unlock(&rpc->atomic_int_mutex);   \
        }
#define ATOMIC_DEC(rpc, x)                              \
        if (rpc->multithreading_enabled) {              \
                nfs_mt_mutex_lock(&rpc->atomic_int_mutex);     \
        }                                               \
	x--;                                            \
        if (rpc->multithreading_enabled) {              \
                nfs_mt_mutex_unlock(&rpc->atomic_int_mutex);   \
        }
#endif /* HAVE_STDATOMIC_H */
#else /* HAVE_MULTITHREADING */
/* no multithreading support, no need to protect the increment */
#define ATOMIC_INC(rpc, x) x++
#define ATOMIC_DEC(rpc, x) x--
#endif /* HAVE_MULTITHREADING */

#include "libnfs-multithreading.h"
#include "libnfs-zdr.h"
#include "../nfs/libnfs-raw-nfs.h"
#include "../nfs4/libnfs-raw-nfs4.h"

#ifdef __cplusplus
extern "C" {
#endif

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)((char *)__mptr - offsetof(type,member));})
        
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#if !defined(HAVE_SOCKADDR_STORAGE) && !defined(WIN32) && !defined(PS2_EE)
/*
 * RFC 2553: protocol-independent placeholder for socket addresses
 */
#define _SS_MAXSIZE	128
#define _SS_ALIGNSIZE	(sizeof(double))
#define _SS_PAD1SIZE	(_SS_ALIGNSIZE - sizeof(unsigned char) * 2)
#define _SS_PAD2SIZE	(_SS_MAXSIZE - sizeof(unsigned char) * 2 - \
				_SS_PAD1SIZE - _SS_ALIGNSIZE)

struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_LEN
	unsigned char ss_len;		/* address length */
	unsigned char ss_family;	/* address family */
#else
	unsigned short ss_family;
#endif
	char	__ss_pad1[_SS_PAD1SIZE];
	double	__ss_align;	/* force desired structure storage alignment */
	char	__ss_pad2[_SS_PAD2SIZE];
};
#endif


struct rpc_fragment {
	struct rpc_fragment *next;
	uint32_t size;
	char *data;
};

#define RPC_CONTEXT_MAGIC 0xc6e46435
#define RPC_PARAM_UNDEFINED -1

/*
 * Queue is singly-linked but we hold on to the tail
 */
struct rpc_queue {
	struct rpc_pdu *head, *tail;
};

#define DEFAULT_HASHES 4
#define NFS_RA_TIMEOUT 5
#define NFS_MIN_XFER_SIZE NFSMAXDATA2
#define NFS_MAX_XFER_SIZE (4 * 1024 * 1024)
#define NFS_DEF_XFER_SIZE (1 * 1024 * 1024)
#define ZDR_ENCODE_OVERHEAD 1024
#define ZDR_ENCODEBUF_MINSIZE 4096

struct rpc_endpoint {
        struct rpc_endpoint *next;
        int program;
        int version;
        struct service_proc *procs;
        int num_procs;
};

#define RPC_FAST_VECTORS 8 /* Same as UIO_FASTIOV used by the Linux kernel */

/*
 * Maximum io vectors supported by rpc_io_vectors.
 * This must not be greater than the POSIX UIO_MAXIOV value as we use writev()
 * to write the io vectors over the socket.
 */
#define RPC_MAX_VECTORS  1024

struct rpc_iovec {
        char *buf;
        size_t len;
        void (*free)(void *);
};

/**
 * Vectored buffer for holding zero-copy user data to be sent over the socket.
 */
struct rpc_io_vectors {
        /* How many bytes from iov[] already written out over the network */
        size_t num_done;
        /* Cumulative size of all rpc_iovecs in iov[] */
        size_t total_size;
        /* iov[] has space for these many rpc_iovecs */
        int iov_capacity;
        /* These many are currently filled */
        int niov;
        /*
         * For small vectors this will point to fast_iov, else it'll be
         * allocated dynamically and must be freed using free().
         */
        struct rpc_iovec *iov;
        /* Inline vector, for saving allocation in the common case */
        struct rpc_iovec fast_iov[RPC_FAST_VECTORS];
};

/**
 * Vectored buffer for holding zero-copy user data to be read over the socket.
 */
struct rpc_iovec_cursor {
        /*
         * Fixed base of the allocated iovec array.
         * Once allocated this doesn't change, and should be used for freeing
         * the iovec array.
         */
        struct iovec *base;

        /*
         * Current iovec we should be reading into, updated as we finish
         * reading whole iovecs. iovcnt holds the count of iovecs remaining
         * to be read into and is decremented as we read whole iovecs or if
         * the cursor is shrinked. We also update the iov_base and iov_len as
         * we read data into iov[], so at any point iov and iovcnt can be
         * passed to readv() to read remaining data.
         */
        struct iovec *iov;
        int iovcnt;

        /*
         * Total to-be-read bytes. This is initialized to the total size of
         * all the individual buffers and later updated as we read data or if
         * the cursor length is reduced due to short read.
         * At any point these many new bytes need to be read into this cursor.
         */
        size_t remaining_size;

        /*
         * Following ref are used to reset iov[] in case we need to resend
         * this request, (possibly) after a reconnect.
         */
        struct iovec *iov_ref;
        int iovcnt_ref;
};

enum input_state {
        READ_RM = 0,
        READ_PAYLOAD = 1,
        READ_FRAGMENT = 2,
        READ_IOVEC = 3,
        READ_PADDING = 4,
        READ_UNKNOWN = 5,
};

#ifdef HAVE_TLS
struct tls_cb_data {
	rpc_cb cb;
	void *private_data;
};

typedef enum tls_handshake_state {
	TLS_HANDSHAKE_UNDEFINED = 0,
	TLS_HANDSHAKE_WAITING_FOR_STARTTLS,
	TLS_HANDSHAKE_IN_PROGRESS,
	TLS_HANDSHAKE_COMPLETED,
	TLS_HANDSHAKE_FAILED,
} tls_handshake_state_t;

/*
 * TLS handshake context information.
 */
struct tls_context {
	/* Current TLS handshake state */
	enum tls_handshake_state state;

	/* Callback to be called on handshake completion (or failure) */
	struct tls_cb_data data;

	/* gnutls session used for the handshake */
	gnutls_session_t session;
};
#endif /* HAVE_TLS */

#define INC_STATS(rpc, stat) ++((rpc)->stats.stat)

struct gss_ctx_id_struct;
struct rpc_context {
	uint32_t magic;
	int fd;
        int socket_disabled; /* Do not allow read/write to the socket */
	int old_fd;
	int is_connected;
	int is_nonblocking;

	char *error_string;

        uint32_t program;
        uint32_t version;

	rpc_cb connect_cb;
	void *connect_data;

	struct AUTH *auth;
	uint32_t xid;

        /*
         * Queue of to-be-transmitted PDUs.
         * Note: The PDU at the head of this queue will be the next one to be
         *       written to the socket. This can be a half-sent PDU for which
         *       (out.num_done < out.total_size). This implies that it's never
         *       safe to add anything to the head of this queue as that might
         *       cause the next rpc_write_to_socket() to incorrectly pick data
         *       from this new PDU while the previous one is half written.
         *       Only rpc_reconnect_requeue() can safely add to the head of
         *       this queue as it resets the connection and also the read and
         *       write cursors. Always use rpc_return_to_outqueue() to safely
         *       return a pdu to outqueue for retransmit.
         */
	struct rpc_queue outqueue;
	struct sockaddr_storage udp_src;
#ifdef __linux__        
	struct sockaddr_storage udp_dst; /* Which ip we received a UDP packet on */
#endif
        uint32_t num_hashes;
        /*
         * Queue of transmitted-and-awaiting-response PDUs.
         */
	struct rpc_queue *waitpdu;
	uint32_t waitpdu_len;
	uint32_t max_waitpdu_len;

#ifdef HAVE_MULTITHREADING
        int multithreading_enabled;
        libnfs_mutex_t rpc_mutex;
#ifndef HAVE_STDATOMIC_H
        libnfs_mutex_t atomic_int_mutex;
#endif /* HAVE_STDATOMIC_H */
#endif /* HAVE_MULTITHREADING */

	uint32_t inpos;
	uint32_t inbuf_size;
	char *inbuf;
        enum input_state state;
        uint32_t rm_xid[2]; /* array holding the record marker and the next 4 bytes */
	uint32_t pdu_size;  /* used in rpc_read_from_socket() */
	char *buf;          /* used in rpc_read_from_socket() */
        struct rpc_pdu *pdu;

	/* special fields for UDP, which can sometimes be BROADCASTed */
	int is_udp;
	struct sockaddr_storage udp_dest;
	int is_broadcast;

	/* track the address we connect to so we can auto-reconnect on session failure */
	struct sockaddr_storage s;
	int auto_reconnect;
	int num_retries;

	/*
	 * NFS server name or IP address. It has the following uses:
	 * - Used for certificate verification, in case of xprtsec=[tls,mtls]
	 *   mount option.
	 *   Note: In this case it must be a DNS name and not an IP address.
	 * - For logging along with the "server not responding" message.
	 *
	 * Since at RPC layer we don't have access to struct nfs_context, we instead
	 * save a copy here from nfs_get_server().
	 */
	char *server;

	int mountport;

	/* fragment reassembly */
	struct rpc_fragment *fragments;

	/* parameters passable via URL */
	int tcp_syncnt;
	int uid;
	int gid;
	int debug;
	uint64_t last_timeout_scan;

	/*
	 * Absolute time in milliseconds when the last successful RPC response
	 * was received over this RPC transport/connection. We use it to see if
	 * some RPC transport could be stuck and if yes we terminate and reconnect
	 * as recovery action.
	 * Note that this is to check activity at the RPC level and not at the
	 * TCP level, latter is checked by using TCP keepalives.
	 */
	uint64_t last_successful_rpc_response;

        /*
         * RPC timeout in milliseconds. This is set from the timeo=<int> mount
         * option. This is also called the "minor timeout", in contrast to the
         * "major timeout" that happens after retrans*timeout milliseconds.
         * It cannot have a value less than 10000, i.e., 10 seconds.
         */
	int timeout;

	/*
	 * Number of times an RPC request is retried before taking further
	 * recovery action. This is set from the retrans=<int> mount option.
	 * If 'retrans' is 0 then RPC requests are not retried and they fail
	 * (with RPC_STATUS_TIMEOUT) after 'timeout' milliseconds. This roughly
	 * mimics the "soft" mount option of NFS clients. Note that it's almost
	 * never a good idea for NFS clients to let RPC requests fail, so 'retrans'
	 * must not be set to 0.
	 * If 'retrans' is non-zero then that's the number of times an RPC
	 * request is retried before declaring a "major timeout", which prompts
	 * more stricter recovery actions, f.e., reconnection.
	 *
	 * Note: This is set to non-zero only after successful mount as we want
	 *       a resilient RPC transport only after mount.
	 *       Ref rpc_set_resiliency().
	 */
	int retrans;

	char ifname[IFNAMSIZ];
	int poll_timeout;

#ifdef HAVE_TLS
	/*
	 * Transport level security as selected by the xprtsec=[none,tls,mtls]
	 * mount option.
	 */
	enum rpc_xprtsec wanted_xprtsec;

	/*
	 * Do we need to send AUTH_TLS NULL RPC on connect/reconnect?
	 * Note that we need this even with wanted_xprtsec as we use TLS only
	 * for connections to NFS program and not for MOUNT or PORTMAP programs.
	 * Once set, this remains set for the life of the rpc_context as we need
	 * it for reconnect also. Note that this is fine for NFSv4 clients as they
	 * only ever make RPC calls to NFS_PROGRAM. This is fine for NFSv3 clients
	 * as we turn it on after we are done talking to the PORTMAP and MOUNT
	 * programs and after that we only talk to NFS_PROGRAM using this rpc_context.
	 *
	 * Note: If the rpc_context is again used for talking to PORTMAP/MOUNT
	 *       programs, use_tls must be cleared.
	 */
	bool_t use_tls;

	/* NFS version to use when sending the AUTH_TLS NULL RPC */
	int nfs_version;

	/* Context used for performing TLS handshake with the server */
	struct tls_context tls_context;
#endif /* HAVE_TLS */

#ifdef HAVE_LIBKRB5
        const char *username;
        enum rpc_sec wanted_sec;
        enum rpc_sec sec;
        uint32_t gss_seqno;
        int context_len;
        char *context;

        void *auth_data; /* for krb5 */
        struct gss_ctx_id_struct *gss_context;
#endif /* HAVE_LIBKRB5 */

        /* Is a server context ? */
        int is_server_context;
        struct rpc_endpoint *endpoints;

        /* Per-transport RPC stats */
        struct rpc_stats stats;

        /* Stats callback */
        rpc_stats_cb stats_cb;
        void *stats_private_data;

        /* Logging callback */
        rpc_log_cb log_cb;
        void *log_private_data;
};
        
struct rpc_pdu {
	struct rpc_pdu *next;

	uint32_t xid;

        ZDR zdr;
        ZDR zdr_in;
        int free_zdr;
        int free_pdu;

	struct rpc_data outdata;

        /* For sending/receiving
         * out contains at least three vectors:
         * [0]  4 bytes for the stream protocol length
         * [1]  Varying size for the rpc header (including cred & verf)
         * [2+] command and and extra parameters
         */
        struct rpc_io_vectors out;

        /*
         * vector for zero-copy READ3 receive.
         * This is updated as data is read from the socket into the user's
         * zero-copy buffers directly.
         */
        struct rpc_iovec_cursor in;

        /*
         * How much more data remains to be read into 'in'. It's initialized
         * with the returned count in READ response and is reduced as we
         * read data from the socket into the user's zero-copy buffers.
         */
        uint32_t read_count;
        uint32_t requested_read_count; /* The amount requested by the
                                        * application.
                                        * Used to clamp long reads.
                                        */

	rpc_cb cb;
	void *private_data;

	/* function to decode the zdr reply data and buffer to decode into */
	zdrproc_t zdr_decode_fn;
	void *zdr_decode_buf;
	uint32_t zdr_decode_bufsize;

        uint32_t discard_after_sending:1;
        uint32_t zero_copy_iov:1;

	/*
	 * If TRUE, this RPC would not be retried. If no response is received
	 * it'll fail with RPC_STATUS_TIMEOUT after 'timeout' msecs.
	 * 'major_timeout' and 'snr_logged' fields are ignored for an RPC which
	 * has do_not_retry set.
	 * Non-NFS RPCs are not retried as they are mostly sent before or during
	 * the mount process and it's desirable to fail them so that the mount
	 * program can fail with appropriate error to the user who is waiting for
	 * the mount to complete.
	 *
	 * Note that there are two ways to ensure that RPCs are not retried:
	 * 1. Set rpc->retrans to 0.
	 * 2. Set pd->do_not_retry to TRUE.
	 *
	 * Since we set rpc->retrans to non-zero value only after successful
	 * mount completion all RPCs sent during the mount process are not
	 * retried. For any other PDU if we don't want it to be retried we need
	 * to set pdu->do_not_retry to TRUE. One such example is the AUTH_TLS
	 * NULL RPC sent on reconnect which needs to be sent inline and hence
	 * cannot be safely retried.
	 */
	bool_t do_not_retry;

	/*
	 * Absolute (minor) timeout in milliseconds for this RPC request.
	 * This is set to current time in milliseconds (when the RPC is
	 * queued) plus rpc->timeout.
	 * If we do not get a response for an RPC request till timeout
	 * milliseconds we retry the RPC request and reset pdu->timeout to
	 * the next rpc->timeout milliseconds.
	 */
	uint64_t timeout;

	/*
	 * Absolute major timeout in milliseconds for this RPC request.
	 * A major timeout happens after every rpc->retrans retries, i.e.,
	 * after rpc->retrans*rpc->timeout milliseconds.
	 * If we do not get a response for an RPC request till 'major_timeout'
	 * milliseconds we log the "server not responding" message and take
	 * further recovery action like reconnecting to the server and retrying
	 * the RPC over the new connection. 'major_timeout' is also reset to
	 * the next rpc->retrans*rpc->timeout milliseconds.
	 */
	uint64_t major_timeout;

	/*
	 * Have we logged the "server not responding" message for this RPC.
	 * Note that for any RPC the "server not responding" message is logged
	 * just once, when the first major_timeout occurs. After a major timeout
	 * if we get the response to the RPC request we log the "server OK"
	 * message.
	 */
	bool_t snr_logged;

#ifdef HAVE_LIBKRB5
        uint32_t gss_seqno;
        char creds[64];
        int start_of_payload;
        gss_buffer_desc output_buffer;
#endif

#ifdef HAVE_TLS
	/* Set by rpc_allocate_pdu2() when we use AUTH_TLS for a NULL RPC request */
	bool_t expect_starttls;
#endif
        /* ONC-RPC header for outgoing PDUs */
        struct rpc_msg msg;
        struct rpc_pdu_stats pdu_stats;
};

void rpc_reset_queue(struct rpc_queue *q);
void rpc_enqueue(struct rpc_queue *q, struct rpc_pdu *pdu);
void rpc_return_to_outqueue(struct rpc_context *rpc, struct rpc_pdu *pdu);
int rpc_remove_pdu_from_queue(struct rpc_queue *q, struct rpc_pdu *remove_pdu);
unsigned int rpc_hash_xid(struct rpc_context *rpc, uint32_t xid);
struct rpc_pdu *rpc_allocate_pdu(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, zdrproc_t zdr_decode_fn, int zdr_bufsize);
struct rpc_pdu *rpc_allocate_pdu2(struct rpc_context *rpc, int program, int version, int procedure, rpc_cb cb, void *private_data, zdrproc_t zdr_decode_fn, int zdr_bufsize, size_t alloc_hint, int iovcnt_hint);
void pdu_set_timeout(struct rpc_context *rpc, struct rpc_pdu *pdu, uint64_t now_msecs);

void rpc_free_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu);
int rpc_queue_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu);
int rpc_process_pdu(struct rpc_context *rpc, char *buf, int size);
struct rpc_pdu *rpc_find_pdu(struct rpc_context *rpc, uint32_t xid);
void rpc_error_all_pdus(struct rpc_context *rpc, const char *error);

/*
 * XXX This holds rpc->rpc_mutex, so if the caller is already holding
 *     rpc->rpc_mutex, use the nolock version below.
 */
void rpc_set_error(struct rpc_context *rpc, const char *error_string, ...)
#ifdef __GNUC__
 __attribute__((format(printf, 2, 3)))
#endif
;

/*
 * rpc_set_error() is a common error path function which is called from many
 * functions that hold rpc->rpc_mutex. The following nolock version must be
 * used by callers who hold the rpc->rpc_mutex.
 */
void rpc_set_error_locked(struct rpc_context *rpc, const char *error_string, ...)
#ifdef __GNUC__
 __attribute__((format(printf, 2, 3)))
#endif
;

void nfs_set_error(struct nfs_context *nfs, char *error_string, ...)
#ifdef __GNUC__
 __attribute__((format(printf, 2, 3)))
#endif
;

void nfs_set_error_locked(struct nfs_context *nfs, char *error_string, ...)
#ifdef __GNUC__
 __attribute__((format(printf, 2, 3)))
#endif
;

#if defined(PS2_EE)
#define RPC_LOG(rpc, level, format, ...) ;
#else
#define RPC_LOG(rpc, level, format, ...) \
	do { \
		if (rpc->log_cb && level <= rpc->debug) { \
			char xxlogbuf[256];                    \
			snprintf(xxlogbuf, 255, "libnfs:%d rpc %p " format, level, rpc, ## __VA_ARGS__); \
                        rpc->log_cb(rpc, level,  xxlogbuf, rpc->log_private_data);                 \
		} \
	} while (0)
#endif

const char *nfs_get_server(struct nfs_context *nfs);
const char *nfs_get_export(struct nfs_context *nfs);

void rpc_set_interface(struct rpc_context *rpc, const char *ifname);

void rpc_set_tcp_syncnt(struct rpc_context *rpc, int v);
void rpc_set_mountport(struct rpc_context *rpc, int port);
int rpc_get_mountport(struct rpc_context *rpc);
void rpc_set_poll_timeout(struct rpc_context *rpc, int poll_timeout);
int rpc_get_poll_timeout(struct rpc_context *rpc);
int rpc_add_fragment(struct rpc_context *rpc, char *data, uint32_t size);
void rpc_free_all_fragments(struct rpc_context *rpc);
uint64_t rpc_current_time(void);
uint64_t rpc_current_time_us(void);

void *zdr_malloc(ZDR *zdrs, uint32_t size);


struct nfs_cb_data;
void free_nfs_cb_data(struct nfs_cb_data *data);

struct nfs_specdata {
        uint32_t specdata1;
        uint32_t specdata2;
};
struct nfs_time {
        uint32_t seconds;
        uint32_t nseconds;
};
struct nfs_attr {
        uint32_t type;
        uint32_t mode;
        uint32_t uid;
        uint32_t gid;
        uint32_t nlink;
        uint64_t size;
        uint64_t used;
        uint64_t fsid;
        struct nfs_specdata rdev;
        struct nfs_time atime;
        struct nfs_time mtime;
        struct nfs_time ctime;
};

struct nfs_fh {
        int len;
        char *val;
};

struct nfs_context_internal {
       char *server;
       char *export;
       char *cwd;
       struct nfs_fh rootfh;
       size_t readmax;
       size_t writemax;
       
       /*
	* Resilency parameters, taken from mount parameters and saved here.
	* Later these are pushed to the RPC layer by rpc_set_resiliency().
	*/
       int auto_reconnect;
       int timeout;
       int retrans;

       int dircache_enabled;
       struct nfsdir *dircache;
       uint16_t	mask;
       int auto_traverse_mounts;
       struct nested_mounts *nested_mounts;
       int default_version; /* if 0 it means no default version and only use the
                             * selected version.
                             */
       int version;
       int nfsport;
       int mountport;
       uint32_t readdir_dircount;
       uint32_t readdir_maxcount;

       /* NFSv4 specific fields */
       verifier4 verifier;
       char *client_name;
       uint64_t clientid;
       verifier4 setclientid_confirm;
       uint32_t open_counter;
       int has_lock_owner;
#ifdef HAVE_MULTITHREADING
       libnfs_thread_t service_thread;
       libnfs_mutex_t nfs_mutex;
       libnfs_mutex_t nfs4_open_counter_mutex;
       libnfs_mutex_t nfs4_open_call_mutex;
       struct nfs_thread_context *thread_ctx;
#endif /* HAVE_MULTITHREADING */
       int readonly;
};

struct nfs_context {
       struct rpc_context *rpc;
       struct nfs_context_internal *nfsi;
       char *error_string;

#ifdef HAVE_MULTITHREADING
       struct nfs_context *master_ctx;
#endif /* HAVE_MULTITHREADING */
};

#ifdef HAVE_MULTITHREADING
struct nfs_thread_context {
        struct nfs_thread_context *next;
        nfs_tid_t tid;
        struct nfs_context nfs;
};
#endif /* HAVE_MULTITHREADING */

typedef int (*continue_func)(struct nfs_context *nfs, struct nfs_attr *attr,
			     struct nfs_cb_data *data);

struct nfs_cb_data {
       struct nfs_context *nfs;
       struct nfsfh *nfsfh;
       char *saved_path, *path;
       int link_count, no_follow;

       nfs_cb cb;
       void *private_data;

       continue_func continue_cb;
       void *continue_data;
       void (*free_continue_data)(void *);
       uint64_t continue_int;

       struct nfs_fh fh;

       /* for multi-read/write calls. */
       int error;
       int cancel;
       int oom;
#if defined(HAVE_MULTITHREADING) && defined(HAVE_STDATOMIC_H)
       atomic_int num_calls;
#else
       int num_calls;
#endif
       size_t count, org_count;
       uint64_t offset, max_offset, org_offset;
       char *buffer;
       int not_my_buffer;
       const char *usrbuf;
       int update_pos;
};

struct nested_mounts {
       struct nested_mounts *next;
       char *path;
       struct nfs_fh fh;
       struct nfs_attr attr;
};

#define MAX_DIR_CACHE 128
#define MAX_LINK_COUNT 40

struct nfsdir {
       struct nfs_fh fh;
       struct nfs_attr attr;
       struct nfsdir *next;

       struct nfsdirent *entries;
       struct nfsdirent *current;
};

struct stateid {
        uint32_t seqid;
        char other[12];
};

struct nfsfh {
        struct nfs_fh fh;
        int is_sync;
        int is_append;
        int is_readonly;
        int is_dirty;
        uint64_t offset;

        /* NFSv4 */
        struct stateid stateid;
        uint32_t open_owner;
        /* locking */
        uint32_t open_seqid;
        uint32_t lock_seqid;
        struct stateid lock_stateid;
};

void rpc_free_iovector(struct rpc_context *rpc, struct rpc_io_vectors *v);
int rpc_add_iovector(struct rpc_context *rpc, struct rpc_io_vectors *v,
                     char *buf, int len, void (*free)(void *));
void rpc_advance_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
                        size_t len);
void rpc_shrink_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
                       size_t new_len);
void rpc_memcpy_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v,
                       const void *src, size_t len);
void rpc_free_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v);
void rpc_reset_cursor(struct rpc_context *rpc, struct rpc_iovec_cursor *v);
const struct nfs_fh *nfs_get_rootfh(struct nfs_context *nfs);

int nfs_normalize_path(struct nfs_context *nfs, char *path);
void nfs_free_nfsdir(struct nfsdir *nfsdir);
void nfs_free_nfsfh(struct nfsfh *nfsfh);

void nfs_dircache_add(struct nfs_context *nfs, struct nfsdir *nfsdir);
struct nfsdir *nfs_dircache_find(struct nfs_context *nfs, struct nfs_fh *fh);
void nfs_dircache_drop(struct nfs_context *nfs, struct nfs_fh *fh);

int nfs3_access_async(struct nfs_context *nfs, const char *path, int mode,
                      nfs_cb cb, void *private_data);
int nfs3_access2_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs3_chdir_async(struct nfs_context *nfs, const char *path,
                     nfs_cb cb, void *private_data);
int nfs3_chmod_async_internal(struct nfs_context *nfs, const char *path,
                              int no_follow, int mode, nfs_cb cb,
                              void *private_data);
int nfs3_chown_async_internal(struct nfs_context *nfs, const char *path,
                              int no_follow, int uid, int gid,
                              nfs_cb cb, void *private_data);
int nfs3_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                     void *private_data);
int nfs3_creat_async(struct nfs_context *nfs, const char *path,
                     int mode, nfs_cb cb, void *private_data);
int nfs3_fchmod_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode,
                      nfs_cb cb, void *private_data);
int nfs3_fchown_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid,
                      int gid, nfs_cb cb, void *private_data);
int nfs3_fstat_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                     void *private_data);
int nfs3_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                       void *private_data);
int nfs3_fsync_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                     void *private_data);
int nfs3_ftruncate_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                         uint64_t length, nfs_cb cb, void *private_data);
int nfs3_link_async(struct nfs_context *nfs, const char *oldpath,
		    const char *newpath, nfs_cb cb, void *private_data);
int nfs3_lseek_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     int64_t offset, int whence, nfs_cb cb, void *private_data);
int nfs3_mkdir2_async(struct nfs_context *nfs, const char *path, int mode,
                      nfs_cb cb, void *private_data);
int nfs3_mknod_async(struct nfs_context *nfs, const char *path, int mode,
                     int dev, nfs_cb cb, void *private_data);
int nfs3_mount_async(struct nfs_context *nfs, const char *server,
		     const char *export, nfs_cb cb, void *private_data);
int nfs3_open_async(struct nfs_context *nfs, const char *path, int flags,
                    int mode, nfs_cb cb, void *private_data);
int nfs3_opendir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs3_pread_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                              void *buf, size_t count, uint64_t offset,
                              nfs_cb cb, void *private_data, int update_pos);
int nfs3_preadv_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                               const struct iovec *iov, int iovcnt, uint64_t offset,
                               nfs_cb cb, void *private_data, int update_pos);
int nfs3_pwrite_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                               const char *buf, size_t count, uint64_t offset,
                               nfs_cb cb, void *private_data, int update_pos);
int nfs3_readlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                        void *private_data);
int nfs3_rename_async(struct nfs_context *nfs, const char *oldpath,
		      const char *newpath, nfs_cb cb, void *private_data);
int nfs3_rmdir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                     void *private_data);
int nfs3_stat_async(struct nfs_context *nfs, const char *path,
                    nfs_cb cb, void *private_data);
int nfs3_stat64_async(struct nfs_context *nfs, const char *path,
                      int no_follow, nfs_cb cb, void *private_data);
int nfs3_statvfs_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs3_statvfs64_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                         void *private_data);
int nfs3_symlink_async(struct nfs_context *nfs, const char *oldpath,
                       const char *newpath, nfs_cb cb, void *private_data);
int nfs3_truncate_async(struct nfs_context *nfs, const char *path,
                        uint64_t length, nfs_cb cb, void *private_data);
int nfs3_umount_async(struct nfs_context *nfs, nfs_cb cb, void *private_data);
int nfs3_unlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                      void *private_data);
int nfs3_utime_async(struct nfs_context *nfs, const char *path,
                     struct utimbuf *times, nfs_cb cb, void *private_data);
int nfs3_utimes_async_internal(struct nfs_context *nfs, const char *path,
                               int no_follow, struct timeval *times,
                               nfs_cb cb, void *private_data);
int nfs3_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     const void *buf, size_t count, nfs_cb cb,
                     void *private_data);

int nfs4_access_async(struct nfs_context *nfs, const char *path, int mode,
                      nfs_cb cb, void *private_data);
int nfs4_access2_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs4_chdir_async(struct nfs_context *nfs, const char *path,
                     nfs_cb cb, void *private_data);
int nfs4_chmod_async_internal(struct nfs_context *nfs, const char *path,
                              int no_follow, int mode, nfs_cb cb,
                              void *private_data);
int nfs4_chown_async_internal(struct nfs_context *nfs, const char *path,
                              int no_follow, int uid, int gid,
                              nfs_cb cb, void *private_data);
int nfs4_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                     void *private_data);
int nfs4_creat_async(struct nfs_context *nfs, const char *path,
                     int mode, nfs_cb cb, void *private_data);
int nfs4_fchmod_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode,
                      nfs_cb cb, void *private_data);
int nfs4_fchown_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid,
                      int gid, nfs_cb cb, void *private_data);
int nfs4_fcntl_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     enum nfs4_fcntl_op cmd, void *arg,
                     nfs_cb cb, void *private_data);
int nfs4_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                       void *private_data);
int nfs4_fsync_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                     void *private_data);
int nfs4_ftruncate_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                         uint64_t length, nfs_cb cb, void *private_data);
int nfs4_link_async(struct nfs_context *nfs, const char *oldpath,
		    const char *newpath, nfs_cb cb, void *private_data);
int nfs4_lseek_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     int64_t offset, int whence, nfs_cb cb, void *private_data);
int nfs4_lockf_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     enum nfs4_lock_op op, uint64_t count,
                     nfs_cb cb, void *private_data);
int nfs4_mkdir2_async(struct nfs_context *nfs, const char *path, int mode,
                      nfs_cb cb, void *private_data);
int nfs4_mknod_async(struct nfs_context *nfs, const char *path, int mode,
                     int dev, nfs_cb cb, void *private_data);
int nfs4_mount_async(struct nfs_context *nfs, const char *server,
		     const char *export, nfs_cb cb, void *private_data);
int nfs4_open_async(struct nfs_context *nfs, const char *path, int flags,
                    int mode, nfs_cb cb, void *private_data);
int nfs4_opendir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs4_pread_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                              void *buf, size_t count, uint64_t offset,
                              nfs_cb cb, void *private_data, int update_pos);
int nfs4_preadv_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                               const struct iovec *iov, int iovcnt, uint64_t offset,
                               nfs_cb cb, void *private_data, int update_pos);
int nfs4_pwrite_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                               const void *buf, size_t count, uint64_t offset,
                               nfs_cb cb, void *private_data, int update_pos);
int nfs4_readlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                        void *private_data);
int nfs4_rename_async(struct nfs_context *nfs, const char *oldpath,
		      const char *newpath, nfs_cb cb, void *private_data);
int nfs4_rmdir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                     void *private_data);
int nfs4_stat64_async(struct nfs_context *nfs, const char *path,
                      int no_follow, nfs_cb cb, void *private_data);
int nfs4_statvfs_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                       void *private_data);
int nfs4_statvfs64_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                         void *private_data);
int nfs4_symlink_async(struct nfs_context *nfs, const char *oldpath,
                       const char *newpath, nfs_cb cb, void *private_data);
int nfs4_truncate_async(struct nfs_context *nfs, const char *path,
                        uint64_t length, nfs_cb cb, void *private_data);
int nfs4_unlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                      void *private_data);
int nfs4_utime_async(struct nfs_context *nfs, const char *path,
                     struct utimbuf *times, nfs_cb cb, void *private_data);
int nfs4_utimes_async_internal(struct nfs_context *nfs, const char *path,
                               int no_follow, struct timeval *times,
                               nfs_cb cb, void *private_data);
int nfs4_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     uint64_t count, const void *buf, nfs_cb cb,
                     void *private_data);

int rpc_write_to_socket(struct rpc_context *rpc);
int _nfs_mount_async(struct nfs_context *nfs, const char *server,
                     const char *exportname, nfs_cb cb,
                     void *private_data);

#ifdef HAVE_TLS
int tls_global_init(struct rpc_context *rpc);
enum tls_handshake_state do_tls_handshake(struct rpc_context *rpc);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !_LIBNFS_PRIVATE_H_ */
