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
/*
 * This is the lowlevel interface to access NFS resources.
 * Through this interface you have access to the full gamut of nfs and nfs
 * related protocol as well as the XDR encoded/decoded structures.
 */
#ifndef _LIBNFS_RAW_H_
#define _LIBNFS_RAW_H_

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

#include <stdint.h>

#if defined(HAVE_SYS_UIO_H) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/uio.h>
#endif

#include <nfsc/libnfs-zdr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpc_data {
       int size;
       char *data;
};

/*
 * Stats maintained per RPC transport.
 * User can query these using get_rpc_stats().
 *
 * Note: If you add more counters, make sure they are updated atomically.
 *
 * TODO: These are currently updated only for the client.
 */
struct rpc_stats {
        /*
         * RPC requests sent out.
         * Retransmitted requests are counted multiple times in this.
         */
        uint64_t num_req_sent;

        /*
         * RPC responses received.
         * (num_req_sent - num_resp_rcvd) could be one of the following:
         * - requests in flight, whose responses are awaited.
         * - requests timed out, whose responses were never received.
         *   If retransmits are enabled, we would have retransmited these.
         */
        uint64_t num_resp_rcvd;

        /*
         * RPC requests which didn't get a response for timeo period.
         * See mount option 'timeo'.
         * These indicate some issue with the server and/or connection.
         */
        uint64_t num_timedout;

        /*
         * RPC requests that timed out while sitting in outqueue.
         * Unlike num_timedout, these are requests which were not sent to
         * server. If this number is high it indicates a slow or unresponsive
         * server and/or slow connection. Application should slow down issuing
         * new RPC requests.
         */
        uint64_t num_timedout_in_outqueue;

        /*
         * RPC requests which didn't get a response even after retrans
         * retries. These are counted in num_timedout as well.
         * See mount option 'retrans'.
         */
        uint64_t num_major_timedout;

        /*
         * RPC requests retransmited due to reconnect or timeout.
         */
        uint64_t num_retransmitted;

        /*
         * Number of times we had to reconnect, for one of the following
         * reasons:
         * - Peer closed connection.
         * - Major timeout was observed.
         */
        uint64_t num_reconnects;
};

struct rpc_context;
EXTERN struct rpc_context *rpc_init_context(void);
EXTERN void rpc_destroy_context(struct rpc_context *rpc);

/*
 * Stats callback for all ASYNC rpc functions.
 * When the stats callback is provided it will geterate a callback
 * every time a PDU is queued for sending as well as when it has received
 * on the socket.
 */
struct rpc_pdu;
struct rpc_pdu_stats {
        uint32_t size;
        uint32_t xid;
        uint32_t direction;
        uint32_t status;         /* only valid in replies */
        uint32_t prog;
        uint32_t vers;
        uint32_t proc;
        uint64_t enqueue_timestamp;   /* us, when the pdu was enqueued */
        uint64_t send_timestamp;      /* us, when the pdu was sent */
        uint64_t response_time;       /* us, only valid in replies */
};
typedef void (*rpc_stats_cb)(struct rpc_context *rpc,
                             struct rpc_pdu_stats *data,
                             void *private_data);
/*
 * Function to query the pdu stats for the current PDU.
 * Only valid if called from an RPC callback function.
 */
struct rpc_pdu_stats *rpc_get_pdu_stats(struct rpc_context *rpc);
        
/*
 * The callback executes in the context of the event-loop so it is vital
 * that the callback will never block and will return as fast as possible.
 */
EXTERN void rpc_set_stats_cb(struct rpc_context *rpc, rpc_stats_cb cb,
                             void *private_data);

/*
 * Set debug level for logging.
 */
void rpc_set_debug(struct rpc_context *rpc, int level);
/*
 * Logging is done via a callback.
 * Log level is set via rpc_set_debug()/nfs_set_debug()
 */
typedef void (*rpc_log_cb)(struct rpc_context *rpc,
                           int level, char *msg, void *private_data);
/*
 * The callback executes in the context of the event-loop so it is vital
 * that the callback will never block and will return as fast as possible.
 */
EXTERN void rpc_set_log_cb(struct rpc_context *rpc, rpc_log_cb cb,
                           void *private_data);

        
/*
 * Commands that are in flight are kept on linked lists and keyed by
 * XID so that responses received can be matched with a request.
 * For performance reasons, this would not scale well for applications
 * that use many concurrent async requests concurrently.
 * The default setting is to hash the requests into a small number of
 * lists which should work well for single threaded syncrhonous and
 * async applications with a moderate number of concurrent requests in flight
 * at any one time.
 * If you application uses a significant number of concurrent requests
 * as in thousands or more, then the default might not be sufficient.
 * In that case you can change the number of lists that requests will
 * be hashed into with this function.
 * NOTE: you can only call this function and modify the number of hashes
 * before you connect to the remote service.
 */
EXTERN int rpc_set_hash_size(struct rpc_context *rpc, int hashes);

EXTERN void rpc_set_auth(struct rpc_context *rpc, struct AUTH *auth);

/*
 * Used for interfacing the api into an external eventsystem.
 *
 * rpc_get_fd() returns the file descriptor to poll.
 *
 * rpc_which_events() returns which events that we need to poll for.
 * This is a combination of the POLLIN and POLLOUT flags.
 *
 * rpc_service() is called everytime there are events that needs to be
 * processed.
 * revents is a combination of POLLIN/POLLOUT/POLLHUP/POLLERR
 *
 * This function returns 0 on success or -1 on error. If it returns -1 it
 * means that the socket is in an unrecoverable error state (disconnected?)
 * and that no further commands can be used.
 * When this happens the application should destroy the now errored context
 * re-create a new context and reconnect.
 *
 *
 * rpc_service() will both process the events indicated by revents and also
 * check for and terminate any RPCs that have timed out.
 * Thus, if using rpc timeouts, you will need to ensure that rpc_service()
 * is invoked on a regular basis so that the timeout processing can take place.
 * The easiest way to do this is to call rpc_service() once every 100ms from
 * your event system and passing revents as 0.
 *
 * rpc_disable_socket() will disable libnfs reading/writing to the socket.
 * Useful if you want to manage the socket from an external event loop and
 * want to ensure that libnfs will never do i/o to the socket.
 */
EXTERN int rpc_get_fd(struct rpc_context *rpc);
EXTERN int rpc_which_events(struct rpc_context *rpc);
EXTERN int rpc_service(struct rpc_context *rpc, int revents);
EXTERN void rpc_disable_socket(struct rpc_context *rpc, int val);


/*
 * Returns the number of commands in-flight. Can be used by the application
 * to check if there are any more responses we are awaiting from the server
 * or if the connection is completely idle.
 * The number returned includes the commands on the output queue and the
 * commands waiting from a response from the server.
 */
EXTERN int rpc_queue_length(struct rpc_context *rpc);

/*
 * Returns the number of commands awaiting from the server.
 * Can be used by the application to check if there are any
 * more responses we are awaiting from the server
 * or if the connection is completely idle.
 */
EXTERN int rpc_get_num_awaiting(struct rpc_context *rpc);

/*
 * Used to limit the total number of commands awaiting from the server.
 * By default there is no limit, all commands will be sent as soon as possible.
 * If a limit is set and it is reached then new commands will be kept on
 * the output queue until the total number of commands in-flight goes below
 * the limit again.
 */
EXTERN void rpc_set_awaiting_limit(struct rpc_context *rpc, int limit);

/*
 * Set which UID/GIDs to use in the authenticator.
 * By default libnfs will use getuid()/getgid() where available
 * and 65534/65534 where not, with no auxiliary GIDs.
 */
EXTERN void rpc_set_uid(struct rpc_context *rpc, int uid);
EXTERN void rpc_set_gid(struct rpc_context *rpc, int gid);
EXTERN void rpc_set_auxiliary_gids(struct rpc_context *rpc, uint32_t len, uint32_t* gids);

/*
 * Used in GSSAPI mode
 */
EXTERN int rpc_set_username(struct rpc_context *rpc, const char *username);


/*
 * sync rpc_set_timeout()
 * This function sets the timeout used for this rpc context.
 *
 * Function returns nothing.
 *
 * int milliseconds : timeout to be applied in milliseconds (-1 no timeout)
 *                    timeouts must currently be set in whole seconds,
 *                    i.e. units of 1000
 */
EXTERN void rpc_set_timeout(struct rpc_context *rpc, int timeout);
/*
 * sync rpc_get_timeout()
 * This function gets the timeout used for rpc context.
 *
 * Function returns
 *    -1 : No timeout applied
 *   > 0 : Timeout in milliseconds
 */
EXTERN int rpc_get_timeout(struct rpc_context *rpc);
        
/*
 * Create a server context.
 */
EXTERN struct rpc_context *rpc_init_server_context(int s);

/* This is the callback functions for server contexts.
 * These are invoked from the library when a CALL has been received and a
 * service procedure has been found that matches the rpc
 * program/version/procedure.
 *
 * The rpc arguments are stored in call->body.cbody.args;
 * Example:
 *  static int pmap2_getport_proc(struct rpc_context *rpc, struct rpc_msg *call)
 *  {
 *       pmap2_mapping *args = call->body.cbody.args;
 *  ...
 *
 *  struct service_proc pmap2_pt[] = {
 *         {PMAP2_GETPORT, pmap2_getport_proc,
 *           (zdrproc_t)zdr_pmap2_mapping, sizeof(pmap2_mapping)},
 *  ...
 *
 *
 * The call argument is only valid for the duration of the callback.
 * If your application needs to call rpc_send_reply() not from the callback
 * but from a different context at a later time you will need to make a temporary
 * copy of the call structure using rpc_copy_deferred_call()/rpc_free_deferred_call().
 * See for example examples/rpcbind.c where during the callit processing
 * we create a copy so that we can send the reply at a later point in time
 * when the rpc created in callit has completed.
 *
 * The return value is:
 *  0:  Procedure was completed normally.
 * !0:  An abnormal error has occured. It is unrecoverable and the only
 *      meaningful action is to tear down the connection to the server.
 */
typedef int (*service_fn)(struct rpc_context *rpc, struct rpc_msg *call, void *opaque);

struct service_proc {
        int proc;
        service_fn func;
        zdrproc_t decode_fn;
        int decode_buf_size;
        void *opaque;
};

/*
 * Register a service callback table for program/version.
 * Can only be used with contexts created with rpc_init_server_context()
 */
EXTERN int rpc_register_service(struct rpc_context *rpc, int program,
                                int version, struct service_proc *procs,
                                int num_procs);

EXTERN int rpc_send_reply(struct rpc_context *rpc, struct rpc_msg *call,
                          void *reply, zdrproc_t encode_fn,
                          int alloc_hint);

EXTERN struct rpc_msg *rpc_copy_deferred_call(struct rpc_context *rpc,
                                              struct rpc_msg *call);

EXTERN void rpc_free_deferred_call(struct rpc_context *rpc,
                                   struct rpc_msg *call);

/*
 * When an operation failed, this function can extract a detailed error string.
 */
EXTERN char *rpc_get_error(struct rpc_context *rpc);

/*
 * Return the current snapshot of stats for this transport.
 */
EXTERN void rpc_get_stats(struct rpc_context *rpc, struct rpc_stats *stats);

/* Utility function to get an RPC context from a NFS context. Useful for doing
 * low level NFSACL calls on a NFS context.
 */
EXTERN struct rpc_context *nfs_get_rpc_context(struct nfs_context *nfs);

/* This function returns the nfs_fh structure from a nfsfh structure.
   This allows to use a file opened with nfs_open() together with low-level
   rpc functions that thake a nfs filehandle
*/
EXTERN struct nfs_fh *nfs_get_fh(struct nfsfh *nfsfh);

/* Control what the next XID value to be used on the context will be.
   This can be used when multiple contexts are used to the same server
   to avoid that the two contexts have xid collissions.
 */
EXTERN void rpc_set_next_xid(struct rpc_context *rpc, uint32_t xid);

/* This function can be used to set the file descriptor used for
 * the RPC context. It is primarily useful when emulating dup2()
 * and similar or where you want full control of the filedescriptor numbers
 * used by the rpc socket.
 *
 * ...
 * oldfd = rpc_get_fd(rpc);
 * dup2(oldfd, newfd);
 * rpc_set_fd(rpc, newfd);
 * close(oldfd);
 * ...
 */
EXTERN void rpc_set_fd(struct rpc_context *rpc, int fd);

#define RPC_STATUS_SUCCESS	   	0
#define RPC_STATUS_ERROR		1
#define RPC_STATUS_CANCEL		2
#define RPC_STATUS_TIMEOUT		3

/*
 * Async connection to the tcp port at server:port.
 * Function returns
 *  0 : The connection was initiated. The callback will be invoked once the
 *      connection establish finishes.
 * <0 : An error occured when trying to set up the connection.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : The tcp connection was successfully established.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The connection failed to establish.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The connection attempt was aborted before it could
 *                      complete.
 *                    : data is NULL.
 */
EXTERN int rpc_connect_async(struct rpc_context *rpc, const char *server,
                             int port, rpc_cb cb, void *private_data);

/*
 * Async function to connect to a specific RPC program/version
 * Function returns
 *  0 : The connection was initiated. The callback will be invoked once the
 *      connection establish finishes.
 * <0 : An error occured when trying to set up the connection.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : The tcp connection was successfully established.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The connection failed to establish.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The connection attempt was aborted before it could
 *                      complete.
 *                    : data is NULL.
 */
EXTERN int rpc_connect_program_async(struct rpc_context *rpc,
                                     const char *server,
                                     int program, int version,
                                     rpc_cb cb, void *private_data);

/*
 * Async function to connect to a specific RPC program/version.
 * This connects directly to the specified port without using portmapper.
 *
 * Function returns
 *  0 : The connection was initiated. The callback will be invoked once the
 *      connection establish finishes.
 * <0 : An error occured when trying to set up the connection.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : The tcp connection was successfully established.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The connection failed to establish.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The connection attempt was aborted before it could
 *                      complete.
 *                    : data is NULL.
 */
EXTERN int rpc_connect_port_async(struct rpc_context *rpc, const char *server,
                                  int port,
                                  int program, int version,
                                  rpc_cb cb, void *private_data);

/*
 * When disconnecting a connection all commands in flight will be
 * called with a callback status RPC_STATUS_ERROR. Data will be the
 * error string for the disconnection.
 */
EXTERN int rpc_disconnect(struct rpc_context *rpc, const char *error);


/*
 * All rpc_<protocol>_ functions return a struct rpc_pdu *
 * This is to allow to cancel a pdu in flight. Beware, the pdu pointer
 * is only valid until the callback function has completed.
 * After the callback function has finished the pdu structure will no longer
 * be valid.
 * It is the responsibility of the application to make sure
 * that the pdu pointer is not used after the callback has returned.
 *
 * A PDU can not be cancelled once we has started to receive it on the
 * socket.
 */
/*
 * rpc_cancel_pdu()
 *
 * Function returns
 *  0 : PDU was successfully cancelled.
 * <0 : PDU could not be cancelled.
 *      This can happen for example if we have started to receive this
 *      pdu on the socket but have not yet completed the callback
 *      function.
 */
struct rpc_pdu;
int rpc_cancel_pdu(struct rpc_context *rpc, struct rpc_pdu *pdu);

/*
 * PORTMAP v2 FUNCTIONS
 */

/*
 * Call PORTMAPPER2/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_null_task(struct rpc_context *rpc,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER2/GETPORT.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a (uint32_t *), containing the port returned.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_getport_task(struct rpc_context *rpc, int program,
                        int version, int protocol,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER2/SET
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a (uint32_t *), containing status
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_set_task(struct rpc_context *rpc, int program,
                    int version, int protocol, int port,
                    rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER2/UNSET
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a (uint32_t *), containing status
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_unset_task(struct rpc_context *rpc, int program,
                      int version, int protocol, int port,
                      rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER2/DUMP.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is struct pmap2_dump_result.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_dump_task(struct rpc_context *rpc, rpc_cb cb,
                     void *private_data);

/*
 * Call PORTMAPPER2/CALLIT.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap2_call_result' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap2_callit_task(struct rpc_context *rpc, int program,
                       int version, int procedure,
                       char *data, int datalen,
                       rpc_cb cb, void *private_data);

/*
 * PORTMAP v3 FUNCTIONS
 */

/*
 * Call PORTMAPPER3/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_null_task(struct rpc_context *rpc,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/SET.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct pmap3_mapping;
EXTERN struct rpc_pdu *
rpc_pmap3_set_task(struct rpc_context *rpc,
                    struct pmap3_mapping *map,
                    rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/UNSET.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_unset_task(struct rpc_context *rpc,
                      struct pmap3_mapping *map,
                      rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/GETADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is struct pmap3_string_result.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_getaddr_task(struct rpc_context *rpc,
                        struct pmap3_mapping *map,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/DUMP.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is struct pmap3_dump_result.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_dump_task(struct rpc_context *rpc,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/CALLIT.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap3_call_result' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_callit_task(struct rpc_context *rpc, int program,
                       int version, int procedure,
                       char *data, int datalen,
                       rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/GETTIME.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_gettime_task(struct rpc_context *rpc,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/UADDR2TADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a struct pmap3_netbuf *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap3_uaddr2taddr_task(struct rpc_context *rpc, char *uaddr,
                            rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER3/TADDR2UADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a struct pmap3_string_result *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct pmap3_netbuf;
EXTERN struct rpc_pdu *
rpc_pmap3_taddr2uaddr_task(struct rpc_context *rpc,
                            struct pmap3_netbuf *netbuf,
                            rpc_cb cb, void *private_data);

/*
 * PORTMAP v4 FUNCTIONS
 */

/*
 * Call PORTMAPPER4/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_null_task(struct rpc_context *rpc,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/SET.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct pmap4_mapping;
EXTERN struct rpc_pdu *
rpc_pmap4_set_task(struct rpc_context *rpc,
                    struct pmap4_mapping *map,
                    rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/UNSET.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_unset_task(struct rpc_context *rpc,
                      struct pmap4_mapping *map,
                      rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/GETADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is struct pmap4_string_result.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_getaddr_task(struct rpc_context *rpc,
                        struct pmap4_mapping *map,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/DUMP.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is struct pmap4_dump_result.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_dump_task(struct rpc_context *rpc,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/BCAST.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap4_bcast_result' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_bcast_task(struct rpc_context *rpc, int program,
                     int version, int procedure,
                     char *data, int datalen,
                     rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/GETTIME.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a uint32_t * containing status.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_gettime_task(struct rpc_context *rpc,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/UADDR2TADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a struct pmap4_netbuf *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_uaddr2taddr_task(struct rpc_context *rpc, char *uaddr,
                            rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/TADDR2UADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a struct pmap4_string_result *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct pmap4_netbuf;
EXTERN struct rpc_pdu *
rpc_pmap4_taddr2uaddr_task(struct rpc_context *rpc,
                            struct pmap4_netbuf *netbuf,
                            rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/GETVERSADDR.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a struct pmap4_string_result *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_getversaddr_task(struct rpc_context *rpc, struct pmap4_mapping *map, rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/INDIRECT.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap4_indirect_result' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_indirect_task(struct rpc_context *rpc, int program,
                        int version, int procedure,
                        char *data, int datalen,
                        rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/GETADDRLIST.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap4_entry_list_ptr' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_getaddrlist_task(struct rpc_context *rpc, struct pmap4_mapping *map, rpc_cb cb, void *private_data);

/*
 * Call PORTMAPPER4/GETSTAT.
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a 'pmap4_stat_byvers' pointer.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_pmap4_getstat_task(struct rpc_context *rpc, rpc_cb cb, void *private_data);



/*
 * MOUNT v3 FUNCTIONS
 */
EXTERN char *mountstat3_to_str(int stat);
EXTERN int mountstat3_to_errno(int error);

/*
 * Call MOUNT3/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_null_task(struct rpc_context *rpc,
                      rpc_cb cb, void *private_data);

/*
 * Call MOUNT3/MNT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is union mountres3.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_mnt_task(struct rpc_context *rpc, rpc_cb cb,
                     char *exportname, void *private_data);

/*
 * Call MOUNT3/DUMP
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a mountlist.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_dump_task(struct rpc_context *rpc,
                      rpc_cb cb, void *private_data);

/*
 * Call MOUNT3/UMNT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_umnt_task(struct rpc_context *rpc, rpc_cb cb,
                      char *exportname,
                      void *private_data);

/*
 * Call MOUNT3/UMNTALL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_umntall_task(struct rpc_context *rpc,
                         rpc_cb cb, void *private_data);

/*
 * Call MOUNT3/EXPORT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is exports *:
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount3_export_task(struct rpc_context *rpc,
                        rpc_cb cb, void *private_data);

/*
 * MOUNT v1 FUNCTIONS (Used with NFSv2)
 */
/*
 * Call MOUNT1/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_null_task(struct rpc_context *rpc,
                      rpc_cb cb, void *private_data);

/*
 * Call MOUNT1/MNT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is union mountres1.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_mnt_task(struct rpc_context *rpc, rpc_cb cb,
                     char *exportname,
                     void *private_data);

/*
 * Call MOUNT1/DUMP
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is a mountlist.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_dump_task(struct rpc_context *rpc,
                      rpc_cb cb, void *private_data);

/*
 * Call MOUNT1/UMNT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_umnt_task(struct rpc_context *rpc, rpc_cb cb,
                      char *exportname,
                      void *private_data);

/*
 * Call MOUNT1/UMNTALL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_umntall_task(struct rpc_context *rpc,
                         rpc_cb cb, void *private_data);

/*
 * Call MOUNT1/EXPORT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is exports *:
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_mount1_export_task(struct rpc_context *rpc,
                        rpc_cb cb, void *private_data);


/*
 * NFS v3 FUNCTIONS
 */
struct nfs_fh3;
EXTERN char *nfsstat3_to_str(int error);
EXTERN int nfsstat3_to_errno(int error);

/*
 * Call NFS3/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nfs3_null_task(struct rpc_context *rpc,
                    rpc_cb cb, void *private_data);

/*
 * Call NFS3/GETATTR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETATTR3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct GETATTR3args;
EXTERN struct rpc_pdu *
rpc_nfs3_getattr_task(struct rpc_context *rpc, rpc_cb cb,
                       struct GETATTR3args *args,
                       void *private_data);

/*
 * Call NFS3/PATHCONF
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is PATHCONF3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct PATHCONF3args;
EXTERN struct rpc_pdu *
rpc_nfs3_pathconf_task(struct rpc_context *rpc, rpc_cb cb,
                        struct PATHCONF3args *args,
                        void *private_data);

/*
 * Call NFS3/LOOKUP
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is LOOKUP3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct LOOKUP3args;
EXTERN struct rpc_pdu *
rpc_nfs3_lookup_task(struct rpc_context *rpc, rpc_cb cb,
                      struct LOOKUP3args *args,
                      void *private_data);

/*
 * Call NFS3/ACCESS
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is ACCESS3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct ACCESS3args;
EXTERN struct rpc_pdu *
rpc_nfs3_access_task(struct rpc_context *rpc, rpc_cb cb,
                      struct ACCESS3args *args,
                      void *private_data);

/*
 * Call NFS3/READ
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READ3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READ3args;
EXTERN struct rpc_pdu *
rpc_nfs3_read_task(struct rpc_context *rpc, rpc_cb cb,
                    void *buf, size_t count,
                    struct READ3args *args,
                    void *private_data);

/*
 * Same as rpc_nfs3_read_task() but can be used to receive READ data into
 * an iovec. Useful for callers who do not have a single contiguous read
 * buffer but instead want the READ data to be gathered into multiple
 * non-contiguous buffers.
 */
EXTERN struct rpc_pdu *
rpc_nfs3_readv_task(struct rpc_context *rpc, rpc_cb cb,
                    const struct iovec *iov, int iovcnt,
                    struct READ3args *args,
                    void *private_data);

/*
 * Call NFS3/WRITE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is WRITE3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct WRITE3args;
EXTERN struct rpc_pdu *
rpc_nfs3_write_task(struct rpc_context *rpc, rpc_cb cb,
                     struct WRITE3args *args,
                     void *private_data);

/*
 * Same as rpc_nfs3_write_task() but can be used to send WRITE data from
 * an iovec. Useful for callers who do not have the WRITE data in a single
 * contiguous buffer but instead the WRITE data needs to be gathered from
 * multiple non-contiguous buffers.
 */
EXTERN struct rpc_pdu *
rpc_nfs3_writev_task(struct rpc_context *rpc, rpc_cb cb,
                     struct WRITE3args *args,
                     const struct iovec *iov, int iovcnt,
                     void *private_data);

/*
 * Call NFS3/COMMIT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is COMMIT3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct COMMIT3args;
EXTERN struct rpc_pdu *
rpc_nfs3_commit_task(struct rpc_context *rpc, rpc_cb cb,
                      struct COMMIT3args *args,
                      void *private_data);

/*
 * Call NFS3/SETATTR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is SETATTR3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct SETATTR3args;
EXTERN struct rpc_pdu *
rpc_nfs3_setattr_task(struct rpc_context *rpc, rpc_cb cb,
                       struct SETATTR3args *args,
                       void *private_data);

/*
 * Call NFS3/MKDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is MKDIR3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct MKDIR3args;
EXTERN struct rpc_pdu *
rpc_nfs3_mkdir_task(struct rpc_context *rpc, rpc_cb cb,
                     struct MKDIR3args *args,
                     void *private_data);

/*
 * Call NFS3/RMDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is RMDIR3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct RMDIR3args;
EXTERN struct rpc_pdu *
rpc_nfs3_rmdir_task(struct rpc_context *rpc, rpc_cb cb,
                     struct RMDIR3args *args,
                     void *private_data);

/*
 * Call NFS3/CREATE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is CREATE3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct CREATE3args;
EXTERN struct rpc_pdu *
rpc_nfs3_create_task(struct rpc_context *rpc, rpc_cb cb,
                      struct CREATE3args *args,
                      void *private_data);

/*
 * Call NFS3/MKNOD
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is MKNOD3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct MKNOD3args;
EXTERN struct rpc_pdu *
rpc_nfs3_mknod_task(struct rpc_context *rpc, rpc_cb cb,
                     struct MKNOD3args *args,
                     void *private_data);

/*
 * Call NFS3/REMOVE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is REMOVE3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct REMOVE3args;
EXTERN struct rpc_pdu *
rpc_nfs3_remove_task(struct rpc_context *rpc, rpc_cb cb,
                      struct REMOVE3args *args,
                      void *private_data);

/*
 * Call NFS3/READDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READDIR3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READDIR3args;
EXTERN struct rpc_pdu *
rpc_nfs3_readdir_task(struct rpc_context *rpc, rpc_cb cb,
                       struct READDIR3args *args,
                       void *private_data);

/*
 * Call NFS3/READDIRPLUS
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READDIRPLUS3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READDIRPLUS3args;
EXTERN struct rpc_pdu *
rpc_nfs3_readdirplus_task(struct rpc_context *rpc, rpc_cb cb,
                           struct READDIRPLUS3args *args,
                           void *private_data);

/*
 * Call NFS3/FSSTAT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is FSSTAT3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct FSSTAT3args;
EXTERN struct rpc_pdu *
rpc_nfs3_fsstat_task(struct rpc_context *rpc, rpc_cb cb,
                      struct FSSTAT3args *args,
                      void *private_data);

/*
 * Call NFS3/FSINFO
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is FSINFO3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct FSINFO3args;
EXTERN struct rpc_pdu *
rpc_nfs3_fsinfo_task(struct rpc_context *rpc, rpc_cb cb,
                      struct FSINFO3args *args,
                      void *private_data);

/*
 * Call NFS3/READLINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READLINK3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READLINK3args;
EXTERN struct rpc_pdu *
rpc_nfs3_readlink_task(struct rpc_context *rpc, rpc_cb cb,
                        struct READLINK3args *args,
                        void *private_data);

/*
 * Call NFS3/SYMLINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is SYMLINK3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct SYMLINK3args;
EXTERN struct rpc_pdu *
rpc_nfs3_symlink_task(struct rpc_context *rpc, rpc_cb cb,
                       struct SYMLINK3args *args,
                       void *private_data);

/*
 * Call NFS3/RENAME
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is RENAME3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct RENAME3args;
EXTERN struct rpc_pdu *
rpc_nfs3_rename_task(struct rpc_context *rpc, rpc_cb cb,
                      struct RENAME3args *args,
                      void *private_data);

/*
 * Call NFS3/LINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is LINK3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct LINK3args;
EXTERN struct rpc_pdu *
rpc_nfs3_link_task(struct rpc_context *rpc, rpc_cb cb,
                    struct LINK3args *args,
                    void *private_data);

/*
 * NFS v2 FUNCTIONS
 */

/*
 * Call NFS2/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nfs2_null_task(struct rpc_context *rpc,
                    rpc_cb cb, void *private_data);

/*
 * Call NFS2/GETATTR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETATTR2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct GETATTR2args;
EXTERN struct rpc_pdu *
rpc_nfs2_getattr_task(struct rpc_context *rpc, rpc_cb cb,
                       struct GETATTR2args *args,
                       void *private_data);

/*
 * Call NFS2/SETATTR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is SETATTR2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct SETATTR2args;
EXTERN struct rpc_pdu *
rpc_nfs2_setattr_task(struct rpc_context *rpc, rpc_cb cb,
                       struct SETATTR2args *args,
                       void *private_data);

/*
 * Call NFS2/LOOKUP
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is LOOKUP2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct LOOKUP2args;
EXTERN struct rpc_pdu *
rpc_nfs2_lookup_task(struct rpc_context *rpc, rpc_cb cb,
                      struct LOOKUP2args *args,
                      void *private_data);

/*
 * Call NFS2/READLINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READLINK2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READLINK2args;
EXTERN struct rpc_pdu *
rpc_nfs2_readlink_task(struct rpc_context *rpc, rpc_cb cb,
                        struct READLINK2args *args,
                        void *private_data);

/*
 * Call NFS2/READ
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READ2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READ2args;
EXTERN struct rpc_pdu *
rpc_nfs2_read_task(struct rpc_context *rpc, rpc_cb cb,
                    struct READ2args *args,
                    void *private_data);

/*
 * Call NFS2/WRITE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is WRITE2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct WRITE2args;
EXTERN struct rpc_pdu *
rpc_nfs2_write_task(struct rpc_context *rpc, rpc_cb cb,
                     struct WRITE2args *args,
                     void *private_data);

/*
 * Call NFS2/CREATE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is CREATE2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct CREATE2args;
EXTERN struct rpc_pdu *
rpc_nfs2_create_task(struct rpc_context *rpc, rpc_cb cb,
                      struct CREATE2args *args,
                      void *private_data);

/*
 * Call NFS2/REMOVE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is REMOVE2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct REMOVE2args;
EXTERN struct rpc_pdu *
rpc_nfs2_remove_task(struct rpc_context *rpc, rpc_cb cb,
                      struct REMOVE2args *args,
                      void *private_data);

/*
 * Call NFS2/RENAME
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is RENAME2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct RENAME2args;
EXTERN struct rpc_pdu *
rpc_nfs2_rename_task(struct rpc_context *rpc, rpc_cb cb,
                      struct RENAME2args *args,
                      void *private_data);

/*
 * Call NFS2/LINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is LINK2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct LINK2args;
EXTERN struct rpc_pdu *
rpc_nfs2_link_task(struct rpc_context *rpc, rpc_cb cb,
                    struct LINK2args *args,
                    void *private_data);

/*
 * Call NFS2/SYMLINK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is SYMLINK2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct SYMLINK2args;
EXTERN struct rpc_pdu *
rpc_nfs2_symlink_task(struct rpc_context *rpc, rpc_cb cb,
                       struct SYMLINK2args *args,
                       void *private_data);

/*
 * Call NFS2/MKDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is MKDIR2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct MKDIR2args;
EXTERN struct rpc_pdu *
rpc_nfs2_mkdir_task(struct rpc_context *rpc, rpc_cb cb,
                     struct MKDIR2args *args,
                     void *private_data);

/*
 * Call NFS2/RMDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is RMDIR2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct RMDIR2args;
EXTERN struct rpc_pdu *
rpc_nfs2_rmdir_task(struct rpc_context *rpc, rpc_cb cb,
                     struct RMDIR2args *args,
                     void *private_data);

/*
 * Call NFS2/READDIR
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is READDIR2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct READDIR2args;
EXTERN struct rpc_pdu *
rpc_nfs2_readdir_task(struct rpc_context *rpc, rpc_cb cb,
                       struct READDIR2args *args,
                       void *private_data);

/*
 * Call NFS2/STATFS
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is STATFS2res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct STATFS2args;
EXTERN struct rpc_pdu *
rpc_nfs2_statfs_task(struct rpc_context *rpc, rpc_cb cb,
                      struct STATFS2args *args,
                      void *private_data);

/*
 * RQUOTA FUNCTIONS
 */
EXTERN char *rquotastat_to_str(int error);
EXTERN int rquotastat_to_errno(int error);

/*
 * Call RQUOTA1/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota1_null_task(struct rpc_context *rpc,
                       rpc_cb cb, void *private_data);

/*
 * Call RQUOTA1/GETQUOTA
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETQUOTA1res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota1_getquota_task(struct rpc_context *rpc, rpc_cb cb,
                           char *exportname, int uid,
                           void *private_data);

/*
 * Call RQUOTA1/GETACTIVEQUOTA
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETQUOTA1res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota1_getactivequota_task(struct rpc_context *rpc, rpc_cb cb,
                                 char *exportname, int uid,
                                 void *private_data);


/*
 * Call RQUOTA2/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota2_null_task(struct rpc_context *rpc,
                       rpc_cb cb, void *private_data);

/*
 * Call RQUOTA2/GETQUOTA
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETQUOTA1res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota2_getquota_task(struct rpc_context *rpc, rpc_cb cb,
                           char *exportname, int type, int uid,
                           void *private_data);

/*
 * Call RQUOTA2/GETACTIVEQUOTA
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETQUOTA1res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_rquota2_getactivequota_task(struct rpc_context *rpc, rpc_cb cb,
                                 char *exportname, int type, int uid,
                                 void *private_data);


/*
 * NFSACL functions
 */

/*
 * Call NFSACL3/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nfsacl3_null_task(struct rpc_context *rpc, rpc_cb cb,
                      void *private_data);

/*
 * Call NFSACL3/GETACL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is GETACL3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct GETACL3args;
EXTERN struct rpc_pdu *
rpc_nfsacl3_getacl_task(struct rpc_context *rpc, rpc_cb cb,
                        struct GETACL3args *args,
                        void *private_data);

/*
 * Call NFSACL3/SETACL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is SETACL3res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct SETACL3args;
EXTERN struct rpc_pdu *
rpc_nfsacl3_setacl_task(struct rpc_context *rpc, rpc_cb cb,
                        struct SETACL3args *args,
                        void *private_data);




/*
 * NLM functions
 */
EXTERN char *nlmstat4_to_str(int stat);

/*
 * Call NLM/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nlm4_null_task(struct rpc_context *rpc, rpc_cb cb,
                    void *private_data);

/*
 * Call NLM/TEST
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_TESTres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NLM4_TESTargs;
EXTERN struct rpc_pdu *
rpc_nlm4_test_task(struct rpc_context *rpc, rpc_cb cb,
                    struct NLM4_TESTargs *args,
                    void *private_data);

/*
 * Call NLM/LOCK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_LOCKres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NLM4_LOCKargs;
EXTERN struct rpc_pdu *
rpc_nlm4_lock_task(struct rpc_context *rpc, rpc_cb cb,
                    struct NLM4_LOCKargs *args,
                    void *private_data);

/*
 * Call NLM/CANCEL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_CANCres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NLM4_CANCargs;
EXTERN struct rpc_pdu *
rpc_nlm4_cancel_task(struct rpc_context *rpc, rpc_cb cb,
                      struct NLM4_CANCargs *args,
                      void *private_data);

/*
 * Call NLM/UNLOCK
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_UNLOCKres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NLM4_UNLOCKargs;
EXTERN struct rpc_pdu *
rpc_nlm4_unlock_task(struct rpc_context *rpc, rpc_cb cb,
                      struct NLM4_UNLOCKargs *args,
                      void *private_data);

/*
 * Call NLM/SHARE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_LOCKres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NLM4_SHAREargs;
EXTERN struct rpc_pdu *
rpc_nlm4_share_task(struct rpc_context *rpc, rpc_cb cb,
                     struct NLM4_SHAREargs *args,
                     void *private_data);

/*
 * Call NLM/UNSHARE
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NLM4_UNLOCKres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nlm4_unshare_task(struct rpc_context *rpc, rpc_cb cb,
                       struct NLM4_SHAREargs *args,
                       void *private_data);

/*
 * NSM functions
 */
EXTERN char *nsmstat1_to_str(int stat);

/*
 * Call NSM/NULL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nsm1_null_task(struct rpc_context *rpc, rpc_cb cb,
                    void *private_data);

/*
 * Call NSM/STAT
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NSM1_STATres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NSM1_STATargs;
EXTERN struct rpc_pdu *
rpc_nsm1_stat_task(struct rpc_context *rpc, rpc_cb cb,
                    struct NSM1_STATargs *args,
                    void *private_data);

/*
 * Call NSM/MON
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NSM1_MONres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NSM1_MONargs;
EXTERN struct rpc_pdu *
rpc_nsm1_mon_task(struct rpc_context *rpc, rpc_cb cb,
                   struct NSM1_MONargs *args,
                   void *private_data);

/*
 * Call NSM/UNMON
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NSM1_UNMONres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NSM1_UNMONargs;
EXTERN struct rpc_pdu *
rpc_nsm1_unmon_task(struct rpc_context *rpc, rpc_cb cb,
                     struct NSM1_UNMONargs *args,
                     void *private_data);

/*
 * Call NSM/UNMONALL
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NSM1_UNMONALLres *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NSM1_UNMONALLargs;
EXTERN struct rpc_pdu *
rpc_nsm1_unmonall_task(struct rpc_context *rpc, rpc_cb cb,
                        struct NSM1_UNMONALLargs *args,
                        void *private_data);

/*
 * Call NSM/SIMUCRASH
 *
 * Function returns
 * pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nsm1_simucrash_task(struct rpc_context *rpc, rpc_cb cb,
                         void *private_data);

/*
 * Call NSM/NOTIFY
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
struct NSM1_NOTIFYargs;
EXTERN struct rpc_pdu *
rpc_nsm1_notify_task(struct rpc_context *rpc, rpc_cb cb,
                      struct NSM1_NOTIFYargs *args,
                      void *private_data);

/*
 * NFS v4 FUNCTIONS
 */
EXTERN char *nfsstat4_to_str(int error);
EXTERN int nfsstat4_to_errno(int error);

/*
 * Call NFS4/NULL
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_nfs4_null_task(struct rpc_context *rpc, rpc_cb cb,
                    void *private_data);

/*
 * Call NFS4/COMPOUND
 *
 * Function returns
 *  pdu : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is COMPOUND4res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 * This function can NOT be used for compounds that contain OP_READ or OP_WRITE.
 */
struct COMPOUND4args;
EXTERN struct rpc_pdu *
rpc_nfs4_compound_task(struct rpc_context *rpc, rpc_cb cb,
                        struct COMPOUND4args *args,
                        void *private_data);
/*
 * Call NFS4/COMPOUND with extra allocation.

 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is COMPOUND4res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 * This function can NOT be used for compounds that contain OP_READ or OP_WRITE.
 */
struct COMPOUND4args;
EXTERN struct rpc_pdu *
rpc_nfs4_compound_task2(struct rpc_context *rpc, rpc_cb cb,
                         struct COMPOUND4args *args,
                         void *private_data,
                         size_t alloc_hint);

/*
 * Call NFS4/COMPOUND for read operations

 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is COMPOUND4res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 * If the compound contains OP_READ you must use this function and not
 * rpc_nfs4_compound_task()
 * The OP_READ must be the last operation in the compound.
 */
EXTERN struct rpc_pdu *
rpc_nfs4_read_task(struct rpc_context *rpc, rpc_cb cb,
                    void *buf, size_t count,
                    struct COMPOUND4args *args,
                    void *private_data);

/*
 * Same as rpc_nfs4_read_task() but can be used to receive READ data into
 * an iovec. Useful for callers who do not have a single contiguous read
 * buffer but instead want the READ data to be gathered into multiple
 * non-contiguous buffers.
 */
EXTERN struct rpc_pdu *
rpc_nfs4_readv_task(struct rpc_context *rpc, rpc_cb cb,
                    const struct iovec *iov, int iovcnt,
                    struct COMPOUND4args *args,
                    void *private_data);

/*
 * Call NFS4/COMPOUND for write operations
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is COMPOUND4res *.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 * If the compound contains OP_WRITE you must use this function and not
 * rpc_nfs4_compound_async()
 * The OP_WRITE must be the last operation in the compound.
 */
EXTERN struct rpc_pdu *
rpc_nfs4_write_task(struct rpc_context *rpc, rpc_cb cb,
                     const void *buf, size_t count,
                     struct COMPOUND4args *args,
                     void *private_data);

/*
 * Same as rpc_nfs3_write_task() but can be used to send WRITE data from
 * an iovec. Useful for callers who do not have the WRITE data in a single
 * contiguous buffer but instead the WRITE data needs to be gathered from
 * multiple non-contiguous buffers.
 */
EXTERN struct rpc_pdu *
rpc_nfs4_writev_task(struct rpc_context *rpc, rpc_cb cb,
                     const struct iovec *iov, int iovcnt,
                     struct COMPOUND4args *args,
                     void *private_data);

/*
 * Call <generic>/NULL
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_null_task(struct rpc_context *rpc, int program, int version,
               rpc_cb cb, void *private_data);

#ifdef HAVE_TLS
/*
 * Call <generic>/NULL RPC with AUTH_TLS in order to probe RPC-with-TLS
 * support from the server, and if server supports RPC-with-TLS, initiate a TLS
 * handshake. Callback will be called after TLS handshake completes (success or
 * failure) and not just after we get a response for this NULL RPC.
 * Function returns
 * pdu  : The command was queued successfully. The callback will be invoked once
 *        the command completes.
 * NULL : An error occured when trying to queue the command.
 *        The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 * RPC_STATUS_SUCCESS : We got a successful response from the server.
 *                      data is NULL.
 * RPC_STATUS_ERROR   : The command failed with an error, either server doesn't
 *                      support TLS or the TLS handshake failed.
 *                      data is the error string.
 * RPC_STATUS_CANCEL  : The command was cancelled.
 *                      data is NULL.
 */
EXTERN struct rpc_pdu *
rpc_null_task_authtls(struct rpc_context *rpc, int nfs_version, rpc_cb cb,
		      void *private_data);
#endif

#ifdef __cplusplus
}
#endif

#endif
