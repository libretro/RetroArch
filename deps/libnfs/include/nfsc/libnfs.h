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
 * This is the high-level interface to access NFS resources using posix-like
 * functions.
 */

#ifndef _LIBNFS_H_
#define _LIBNFS_H_

#ifdef PS2_EE
#include "ps2_compat.h"
#endif

#include <stddef.h>
#include <stdint.h>
#if !defined(_WIN32)
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LIBNFS_API_V2
#define LIBNFS_FEATURE_DEBUG
#define LIBNFS_FEATURE_UMOUNT
#define LIBNFS_FEATURE_MULTITHREADING

#define NFS_BLKSIZE 4096

struct nfs_context;
struct rpc_context;

struct nfs_url {
	char *server;
	int  port;
	char *path;
	char *file;
};

#if defined(WIN32) && defined(libnfs_EXPORTS)
#define EXTERN __declspec( dllexport )
#else
#ifndef EXTERN
#define EXTERN
#endif
#endif

#ifdef WIN32
#include <winsock2.h>
#ifdef HAVE_FUSE_H
#include <fuse.h>
#else
struct statvfs {
	uint32_t	f_bsize;
	uint32_t	f_frsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint32_t	f_files;
	uint32_t	f_ffree;
	uint32_t	f_favail;
	uint32_t	f_fsid;
	uint32_t	f_flag;
	uint32_t	f_namemax;
};
#endif
#if !defined(__MINGW32__)
struct utimbuf {
	time_t actime;
	time_t modtime;
};
#endif
#define R_OK	4
#define W_OK	2
#define X_OK	1
#endif

/*
 * Used for interfacing the async version of the api into an external
 * eventsystem.
 *
 * nfs_get_fd() returns the file descriptor for the context we need to
 * listen for events from.
 *
 * nfs_which_events() returns which events that we need to poll for.
 * This is a combination of the POLLIN and POLLOUT flags.
 *
 * nfs_service() This function should be called once there are events triggered
 * for the filedescriptor. This function takes POLLIN/POLLOUT/POLLHUP/POLLERR
 * as arguments.
 * This function returns 0 on success or -1 on error. If it returns -1 it
 * means that the socket is in an unrecoverable error state (disconnected?)
 * and that no further commands can be used.
 * When this happens the application should destroy the now errored context
 * re-create a new context and reconnect.
 *
 *
 * If using the async interface and nfs timeouts, i.e. nfs_set_timeout(),
 * you will need to ensure to call nfs_service() on a regular basis as
 * the timeout handling is done as part of that function.
 * For example calling nfs_service() with revents == 0 once every 100ms
 * or so from your event loop.
 * You only need this for the async interface. The sync interface already
 * do this in their built-in event loops.
 */
EXTERN int nfs_get_fd(struct nfs_context *nfs);
EXTERN int nfs_which_events(struct nfs_context *nfs);
EXTERN int nfs_service(struct nfs_context *nfs, int revents);

/*
 * Returns the number of commands in-flight. Can be used by the application
 * to check if there are any more responses we are awaiting for the server
 * or if the connection is completely idle.
 */
EXTERN int nfs_queue_length(struct nfs_context *nfs);

/*
 * Used if you need different credentials than the default for the current user.
 */
struct AUTH;
EXTERN void nfs_set_auth(struct nfs_context *nfs, struct AUTH *auth);

/*
 * Used to set which security to use.
 */
enum rpc_sec {
        RPC_SEC_UNDEFINED = 0,
        RPC_SEC_KRB5,
        RPC_SEC_KRB5I,
        RPC_SEC_KRB5P,
};
EXTERN void nfs_set_security(struct nfs_context *nfs, enum rpc_sec sec);

/*
 * Set readonly parameter of the mount.
 */
EXTERN void nfs_set_readonly(struct nfs_context *nfs, int readonly);
        
#ifdef HAVE_TLS
/*
 * Various transport level security values that map to the mount option
 * xprtsec=[none,tls,mtls].
 */
enum rpc_xprtsec {
	RPC_XPRTSEC_UNDEFINED = 0,
	RPC_XPRTSEC_NONE,	/* No transport security */
	RPC_XPRTSEC_TLS,	/* TLS, with server authentication */
	RPC_XPRTSEC_MTLS,	/* Mutual TLS, with both server and client authentication */
};

EXTERN void nfs_set_xprtsecurity(struct nfs_context *nfs, enum rpc_xprtsec xprtsec);
#endif /* HAVE_TLS */

/*
 * Used if you need to bind to a specific interface.
 * Only available on platforms that support SO_BINDTODEVICE.
 */
EXTERN void nfs_set_interface(struct nfs_context *nfs, const char *ifname);

/*
 * When an operation failed, this function can extract a detailed error string.
 */
EXTERN char *nfs_get_error(struct nfs_context *nfs);


/*
 * Callback for all ASYNC nfs functions
 */
typedef void (*nfs_cb)(int err, struct nfs_context *nfs, void *data,
                       void *private_data);

/*
 * Callback for all ASYNC rpc functions
 */
typedef void (*rpc_cb)(struct rpc_context *rpc, int status, void *data,
                       void *private_data);


/*
 * NFS CONTEXT.
 */
/*
 * Create an NFS context.
 * Function returns
 *  NULL : Failed to create a context.
 *  *nfs : A pointer to an nfs context.
 */
EXTERN struct nfs_context *nfs_init_context(void);
/*
 * Destroy an nfs context.
 */
EXTERN void nfs_destroy_context(struct nfs_context *nfs);

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
 * before you mount the share.
 */
EXTERN int nfs_set_hash_size(struct nfs_context *nfs, int hashes);


/*
 * URL parsing functions.
 * These functions all parse a URL of the form
 * nfs://server/path/file?argv=val[&arg=val]*
 * and returns a nfs_url.
 *
 * Apart from parsing the URL the functions will also update
 * the nfs context to reflect settings controlled via url arguments.
 *
 * Current URL arguments are :
 * tcp-syncnt=<int>  : Number of SYNs to send during the session establish
 *                     before failing setting up the TCP connection to the
 *                     server.
 * uid=<int>         : UID value to use when talking to the server.
 *                     default is 65534 on Windows and getuid() on unixen.
 * gid=<int>         : GID value to use when talking to the server.
 *                     default is 65534 on Windows and getgid() on unixen.
 * sec=<krb5|krb5i|krb5p>
 *                   : Specify the security mode.
 * xprtsec=<none|tls|mtls>
 *                   : Specify the transport security mode.
 *                     none : No TLS security.
 *                     tls  : TLS with server authentication only.
 *                     mtls : Mutual TLS, both server and client authentication.
 * auto-traverse-mounts=<0|1>
 *                   : Should libnfs try to traverse across nested mounts
 *                     automatically or not. Default is 1 == enabled.
 * readonly	     : Set the mount to readonly.
 * dircache=<0|1>    : Disable/enable directory caching. Enabled by default.
 * autoreconnect=<-1|0|>=1>
 *                   : Control the auto-reconnect behaviour to the NFS session.
 *                    -1 : Try to reconnect forever on session failures.
 *                         Just like normal NFS clients do.
 *                     0 : Disable auto-reconnect completely and immediately
 *                         return a failure to the application.
 *                   >=1 : Retry to connect back to the server this many
 *                         times before failing and returing an error back
 *                         to the application.
 * version=<3|4>     : NFS version. Default is 3.
 * nfsport=<port>    : Use this port for NFS instead of using the portmapper.
 * mountport=<port>  : Use this port for the MOUNT protocol instead of
 *                     using portmapper. This argument is ignored for NFSv4
 *                     as it does not use the MOUNT protocol.
 * readdir-buffer=<count> | readdir-buffer=<dircount>,<maxcount>
 *                   : Set the buffer size for READDIRPLUS, where dircount is
 *                     the maximum amount of bytes the server should use to
 *                     retrieve the entry names and maxcount is the maximum
 *                     size of the response buffer (including attributes).
 *                     If only one <count> is given it will be used for both.
 *                     Default is 8192 for both.
 */
/*
 * Parse a complete NFS URL including, server, path and
 * filename. Fail if any component is missing.
 */
EXTERN struct nfs_url *nfs_parse_url_full(struct nfs_context *nfs,
                                          const char *url);

/*
 * Parse an NFS URL, but do not split path and file. File
 * in the resulting struct remains NULL.
 */
EXTERN struct nfs_url *nfs_parse_url_dir(struct nfs_context *nfs,
                                         const char *url);

/*
 * Parse an NFS URL, but do not fail if file, path or even server is missing.
 * Check elements of the resulting struct for NULL.
 */
EXTERN struct nfs_url *nfs_parse_url_incomplete(struct nfs_context *nfs,
                                                const char *url);


/*
 * Free the URL struct returned by the nfs_parse_url_* functions.
 */
EXTERN void nfs_destroy_url(struct nfs_url *url);


struct nfsfh;

/*
 * Get the NFS server address we are currently connected to.
 */
EXTERN const struct sockaddr_storage *nfs_get_server_address(struct nfs_context *nfs);

/*
 * Get the maximum supported READ size by the server
 */
EXTERN size_t nfs_get_readmax(struct nfs_context *nfs);

/*
 * Get the maximum supported WRITE size by the server
 */
EXTERN size_t nfs_get_writemax(struct nfs_context *nfs);

/*
 * Set the maximum supported READ size by the server
 */
EXTERN void nfs_set_readmax(struct nfs_context *nfs, size_t readmax);

/*
 * Set the maximum supported WRITE size by the server
 */
EXTERN void nfs_set_writemax(struct nfs_context *nfs, size_t writemax);
        
/*
 *  MODIFY CONNECT PARAMETERS
 */

EXTERN void nfs_set_tcp_syncnt(struct nfs_context *nfs, int v);
EXTERN void nfs_set_uid(struct nfs_context *nfs, int uid);
EXTERN void nfs_set_gid(struct nfs_context *nfs, int gid);
EXTERN void nfs_set_auxiliary_gids(struct nfs_context *nfs, uint32_t len, uint32_t* gids);
EXTERN void nfs_set_debug(struct nfs_context *nfs, int level);
EXTERN void nfs_set_auto_traverse_mounts(struct nfs_context *nfs, int enabled);
EXTERN void nfs_set_dircache(struct nfs_context *nfs, int enabled);
EXTERN void nfs_set_autoreconnect(struct nfs_context *nfs, int num_retries);
EXTERN void nfs_set_retrans(struct nfs_context *nfs, int retrans);
EXTERN void nfs_set_nfsport(struct nfs_context *nfs, int port);
EXTERN void nfs_set_mountport(struct nfs_context *nfs, int port);
EXTERN size_t nfs_get_readdir_maxcount(struct nfs_context *nfs);
EXTERN void nfs_set_readdir_max_buffer_size(struct nfs_context *nfs, uint32_t dircount, uint32_t maxcount);

/*
 * Set NFS version. Supported versions are
 * NFS_V3 (default)
 * NFS_V4
 */
EXTERN int nfs_set_version(struct nfs_context *nfs, int version);
/*
 * Get NFS version of a connected share. Supported versions are
 * NFS_V3
 * NFS_V4
 */
EXTERN int nfs_get_version(struct nfs_context *nfs);

/*
 * MOUNT THE EXPORT
 */
/*
 * Async nfs mount.
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_mount_async(struct nfs_context *nfs, const char *server,
                           const char *exportname, nfs_cb cb,
                           void *private_data);
/*
 * Sync nfs mount.
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_mount(struct nfs_context *nfs, const char *server,
                     const char *exportname);


/*
 * UNMOUNT THE EXPORT
 */
/*
 * Async nfs umount.
 * For NFSv4 this is a NO-OP.
 * For NFSv3 this function returns unregisters the mount from the MOUNT Daemon.
 *
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_umount_async(struct nfs_context *nfs, nfs_cb cb,
                           void *private_data);
/*
 * Sync nfs umount.
 * For NFSv4 this is a NO-OP.
 * For NFSv3 this function returns unregisters the mount from the MOUNT Daemon.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_umount(struct nfs_context *nfs);




/*
 * STAT()
 */
/*
 * Async stat(<filename>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct stat *
 * -errno : An error occured.
 *          data is the error string.
 */
/* This function is deprecated. Use nfs_stat64_async() instead */
struct stat;
EXTERN int nfs_stat_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                          void *private_data);
/*
 * Sync stat(<filename>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
/* This function is deprecated. Use nfs_stat64() instead */
#ifdef WIN32
EXTERN int nfs_stat(struct nfs_context *nfs, const char *path,
                    struct __stat64 *st);
#else
EXTERN int nfs_stat(struct nfs_context *nfs, const char *path, struct stat *st);
#endif


/* nfs_stat64
 * 64 bit version if stat. All fields are always 64bit.
 * Use these functions instead of nfs_stat[_async](), especially if you
 * have weird stat structures.
 */
/*
 * STAT()
 */
struct nfs_stat_64 {
	uint64_t nfs_dev;
	uint64_t nfs_ino;
	uint64_t nfs_mode;
	uint64_t nfs_nlink;
	uint64_t nfs_uid;
	uint64_t nfs_gid;
	uint64_t nfs_rdev;
	uint64_t nfs_size;
	uint64_t nfs_blksize;
	uint64_t nfs_blocks;
	uint64_t nfs_atime;
	uint64_t nfs_mtime;
	uint64_t nfs_ctime;
	uint64_t nfs_atime_nsec;
	uint64_t nfs_mtime_nsec;
	uint64_t nfs_ctime_nsec;
	uint64_t nfs_used;
};

/*
 * Async stat64filename>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct nfs_stat_64 *
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_stat64_async(struct nfs_context *nfs, const char *path,
                            nfs_cb cb, void *private_data);
/*
 * Sync stat64filename>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_stat64(struct nfs_context *nfs, const char *path,
                      struct nfs_stat_64 *st);

/*
 * Async stat(<filename>)
 *
 * Like stat except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct nfs_stat_64 *
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_lstat64_async(struct nfs_context *nfs, const char *path,
                             nfs_cb cb, void *private_data);
/*
 * Sync stat(<filename>)
 *
 * Like stat except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_lstat64(struct nfs_context *nfs, const char *path,
                       struct nfs_stat_64 *st);

/*
 * FSTAT()
 */
/*
 * Async fstat(nfsfh *)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct stat *
 * -errno : An error occured.
 *          data is the error string.
 */
/* This function is deprecated. Use nfs_fstat64_async() instead */
EXTERN int nfs_fstat_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           nfs_cb cb, void *private_data);
/*
 * Sync fstat(nfsfh *)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
#ifdef WIN32
EXTERN int nfs_fstat(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     struct __stat64 *st);
#else
EXTERN int nfs_fstat(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     struct stat *st);
#endif

/* nfs_fstat64
 * 64 bit version of fstat. All fields are always 64bit.
 * Use these functions instead of nfs_fstat[_async](), especially if you
 * have weird stat structures.
 */
/*
 * FSTAT()
 */
/*
 * Async fstat(nfsfh *)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct stat *
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                             nfs_cb cb, void *private_data);
/*
 * Sync fstat(nfsfh *)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_fstat64(struct nfs_context *nfs, struct nfsfh *nfsfh,
                       struct nfs_stat_64 *st);

/*
 * UMASK() never blocks, so no special aync/async versions are available
 */
/*
 * Sync umask(<mask>)
 * Function returns the old mask.
 */
EXTERN uint16_t nfs_umask(struct nfs_context *nfs, uint16_t mask);

/*
 * OPEN()
 */
/*
 * Async open(<filename>)
 *
 * mode is a combination of the flags :
 * O_RDONLY, O_WRONLY, O_RDWR , O_SYNC, O_APPEND, O_TRUNC, O_NOFOLLOW,
 * O_CREAT
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * Supported flags are
 * O_NOFOLLOW
 * O_APPEND
 * O_CREAT
 * O_RDONLY
 * O_WRONLY
 * O_RDWR
 * O_SYNC
 * O_TRUNC (Only valid with O_RDWR or O_WRONLY. Ignored otherwise.)
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is a struct *nfsfh;
 *          The nfsfh is close using nfs_close().
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_open_async(struct nfs_context *nfs, const char *path, int flags,
                          nfs_cb cb, void *private_data);
EXTERN int nfs_open2_async(struct nfs_context *nfs, const char *path, int flags,
                           int mode, nfs_cb cb, void *private_data);
/*
 * Sync open(<filename>)
 * Function returns
 *      0 : The operation was successful. *nfsfh is filled in.
 * -errno : The command failed.
 */
EXTERN int nfs_open(struct nfs_context *nfs, const char *path, int flags,
                    struct nfsfh **nfsfh);
EXTERN int nfs_open2(struct nfs_context *nfs, const char *path, int flags,
                     int mode, struct nfsfh **nfsfh);




/*
 * CLOSE
 */
/*
 * Async close(nfsfh)
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           nfs_cb cb, void *private_data);
/*
 * Sync close(nfsfh)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_close(struct nfs_context *nfs, struct nfsfh *nfsfh);


struct iovec;
/*
 * PREAD()
 */
/*
 * Async pread()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes read.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_pread_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           void *buf, size_t count, uint64_t offset,
                           nfs_cb cb, void *private_data);
/*
 * Sync pread()
 * Function returns
 *    >=0 : numer of bytes read.
 * -errno : An error occured.
 */
EXTERN int nfs_pread(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     void *buf, size_t count, uint64_t offset);

/*
 * PREADV()
 */
/*
 * Async preadv()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes read.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_preadv_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const struct iovec *iov, int iovcnt, uint64_t offset,
                           nfs_cb cb, void *private_data);
/*
 * Sync preadv()
 * Function returns
 *    >=0 : numer of bytes read.
 * -errno : An error occured.
 */
EXTERN int nfs_preadv(struct nfs_context *nfs, struct nfsfh *nfsfh,
                      const struct iovec *iov, int iovcnt, uint64_t offset);
        


/*
 * READ()
 */
/*
 * Async read()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes read.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_read_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                          void *buf, size_t count,
                          nfs_cb cb, void *private_data);
/*
 * Sync read()
 * Function returns
 *    >=0 : numer of bytes read.
 * -errno : An error occured.
 */
EXTERN int nfs_read(struct nfs_context *nfs, struct nfsfh *nfsfh,
                    void *buf, size_t count);

/*
 * READV()
 */
/*
 * Async readv()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes read.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_readv_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const struct iovec *iov, int iovcnt,
                           nfs_cb cb, void *private_data);
/*
 * Sync readv()
 * Function returns
 *    >=0 : numer of bytes read.
 * -errno : An error occured.
 */
EXTERN int nfs_readv(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     const struct iovec *iov, int iovcnt);

/*
 * PWRITE()
 */
/*
 * Async pwrite()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes written.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_pwrite_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                            const void *buf, size_t count, uint64_t offset,
                            nfs_cb cb, void *private_data);
/*
 * Sync pwrite()
 * Function returns
 *    >=0 : numer of bytes written.
 * -errno : An error occured.
 */
EXTERN int nfs_pwrite(struct nfs_context *nfs, struct nfsfh *nfsfh,
                      const void *buf, size_t count, uint64_t offset);


/*
 * WRITE()
 */
/*
 * Async write()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          status is numer of bytes written.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const void *buf, size_t count,
                           nfs_cb cb, void *private_data);
/*
 * Sync write()
 * Function returns
 *    >=0 : numer of bytes written.
 * -errno : An error occured.
 */
EXTERN int nfs_write(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     const void *buf, uint64_t count);


/*
 * LSEEK()
 */
/*
 * Async lseek()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *    >=0 : Success.
 *          data is uint64_t * for the current position.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_lseek_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           int64_t offset, int whence, nfs_cb cb,
                           void *private_data);
/*
 * Sync lseek()
 * Function returns
 *    >=0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_lseek(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     int64_t offset, int whence, uint64_t *current_offset);


/*
 * LOCKF()
 */
/*
 * Async lockf()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
enum nfs4_lock_op {
        NFS4_F_LOCK  = 0,
        NFS4_F_TLOCK = 1,
        NFS4_F_ULOCK = 2,
        NFS4_F_TEST  = 3,
};
EXTERN int nfs_lockf_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           enum nfs4_lock_op op, uint64_t count,
                           nfs_cb cb, void *private_data);
/*
 * Sync lockf()
 * Function returns
 *      0 : Success.
 * -errno : An error occured.
 */
EXTERN int nfs_lockf(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     enum nfs4_lock_op op, uint64_t count);

/*
 * FCNTL()
 */
/*
 * Async fcntl()
 * Supported commands are :
 *       NFS4_F_SETLK
 *       NFS4_F_SETLKW
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
enum nfs4_fcntl_op {
        NFS4_F_SETLK  = 0,
        NFS4_F_SETLKW,
};
struct nfs4_flock {
        int l_type;        /* F_RDLCK, F_WRLCK or F_UNLCK */
        int l_whence;      /* SEEK_SET, SEEK_CUR or SEEK_END */
        uint32_t l_pid;
        uint64_t l_start;
        uint64_t l_len;
};

EXTERN int nfs_fcntl_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           enum nfs4_fcntl_op cmd, void *arg,
                           nfs_cb cb, void *private_data);
/*
 * Sync lockf()
 * Function returns
 *      0 : Success.
 * -errno : An error occured.
 */
EXTERN int nfs_fcntl(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     enum nfs4_fcntl_op cmd, void *arg);

/*
 * FSYNC()
 */
/*
 * Async fsync()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_fsync_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           nfs_cb cb, void *private_data);
/*
 * Sync fsync()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_fsync(struct nfs_context *nfs, struct nfsfh *nfsfh);



/*
 * TRUNCATE()
 */
/*
 * Async truncate()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_truncate_async(struct nfs_context *nfs, const char *path,
                              uint64_t length, nfs_cb cb, void *private_data);
/*
 * Sync truncate()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_truncate(struct nfs_context *nfs, const char *path,
                        uint64_t length);



/*
 * FTRUNCATE()
 */
/*
 * Async ftruncate()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_ftruncate_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                               uint64_t length, nfs_cb cb, void *private_data);
/*
 * Sync ftruncate()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_ftruncate(struct nfs_context *nfs, struct nfsfh *nfsfh,
                         uint64_t length);






/*
 * MKDIR()
 */
/*
 * Async mkdir()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_mkdir_async(struct nfs_context *nfs, const char *path,
                           nfs_cb cb, void *private_data);
/*
 * Sync mkdir()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_mkdir(struct nfs_context *nfs, const char *path);

/*
 * Async mkdir2()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_mkdir2_async(struct nfs_context *nfs, const char *path,
                            int mode, nfs_cb cb, void *private_data);
/*
 * Sync mkdir2()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_mkdir2(struct nfs_context *nfs, const char *path, int mode);



/*
 * RMDIR()
 */
/*
 * Async rmdir()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_rmdir_async(struct nfs_context *nfs, const char *path,
                           nfs_cb cb, void *private_data);
/*
 * Sync rmdir()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_rmdir(struct nfs_context *nfs, const char *path);




/*
 * CREAT()
 */
/*
 * Async creat()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is a struct *nfsfh;
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_creat_async(struct nfs_context *nfs, const char *path, int mode,
                           nfs_cb cb, void *private_data);
/*
 * Sync creat()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_creat(struct nfs_context *nfs, const char *path, int mode,
                     struct nfsfh **nfsfh);


/*
 * MKNOD()
 */
/*
 * Async mknod()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_mknod_async(struct nfs_context *nfs, const char *path, int mode,
                           int dev, nfs_cb cb, void *private_data);
/*
 * Sync mknod()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_mknod(struct nfs_context *nfs, const char *path, int mode,
                     int dev);



/*
 * UNLINK()
 */
/*
 * Async unlink()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_unlink_async(struct nfs_context *nfs, const char *path,
                            nfs_cb cb, void *private_data);
/*
 * Sync unlink()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_unlink(struct nfs_context *nfs, const char *path);




/*
 * OPENDIR()
 */
struct nfsdir;
/*
 * Async opendir()
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When struct nfsdir * is returned, this resource is closed/freed by calling nfs_closedir()
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct nfsdir *
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_opendir_async(struct nfs_context *nfs, const char *path,
                             nfs_cb cb, void *private_data);
/*
 * Sync opendir()
 * Function returns
 *      0 : Success
 * -errno : An error occured.
 */
EXTERN int nfs_opendir(struct nfs_context *nfs, const char *path,
                       struct nfsdir **nfsdir);



/*
 * READDIR()
 */
struct nfsdirent  {
       struct nfsdirent *next;
       char *name;
       uint64_t inode;

       /* Some extra fields we get for free through the READDIRPLUS3 call.
	  You need libnfs-raw-nfs.h for type/mode constants */
       uint32_t type; /* NF3REG, NF3DIR, NF3BLK, ... */
       uint32_t mode;
       uint64_t size;
       struct timeval atime;
       struct timeval mtime;
       struct timeval ctime;
       uint32_t uid;
       uint32_t gid;
       uint32_t nlink;
       uint64_t dev;
       uint64_t rdev;
       uint64_t blksize;
       uint64_t blocks;
       uint64_t used;
       uint32_t atime_nsec;
       uint32_t mtime_nsec;
       uint32_t ctime_nsec;
};
/*
 * nfs_readdir() never blocks, so no special sync/async versions are available
 */
EXTERN struct nfsdirent *nfs_readdir(struct nfs_context *nfs,
                                     struct nfsdir *nfsdir);


/*
 * SEEKDIR()
 */
/*
 * This function will never block so there is no need for an async version.
 */
EXTERN void nfs_seekdir(struct nfs_context *nfs, struct nfsdir *nfsdir,
                        long loc);

/*
 * TELLDIR()
 */
/*
 * On success, nfs_telldir() will return a location as a value >= 0.
 * On failure, nfs_telldir() will return -1.
 *
 * This function will never block so there is no need for an async version.
 */
EXTERN long nfs_telldir(struct nfs_context *nfs, struct nfsdir *nfsdir);


/*
 * REWINDDIR()
 */
/*
 * nfs_rewinddir() cancel all previous nfs_readdir() side effects.
 * This function will never block so there is no need for an async version.
 */
EXTERN void nfs_rewinddir(struct nfs_context *nfs, struct nfsdir *nfsdir);


/*
 * CLOSEDIR()
 */
/*
 * nfs_closedir() never blocks, so no special sync/async versions are available
 */
EXTERN void nfs_closedir(struct nfs_context *nfs, struct nfsdir *nfsdir);


/*
 * CHDIR()
 */
/*
 * Async chdir(<path>)
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL;
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_chdir_async(struct nfs_context *nfs, const char *path,
                           nfs_cb cb, void *private_data);
/*
 * Sync chdir(<path>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_chdir(struct nfs_context *nfs, const char *path);

/*
 * GETCWD()
 */
/*
 * nfs_getcwd() never blocks, so no special sync/async versions are available
 */
/*
 * Sync getcwd()
 * This function returns a pointer to the current working directory.
 * This pointer is only stable until the next [f]chdir or when the
 * context is destroyed.
 *
 * Function returns
 *      0 : The operation was successful and *cwd is filled in.
 * -errno : The command failed.
 */
EXTERN void nfs_getcwd(struct nfs_context *nfs, const char **cwd);


/*
 * STATVFS()
 */
struct nfs_statvfs_64 {
	uint64_t	f_bsize;
	uint64_t	f_frsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint64_t	f_files;
	uint64_t	f_ffree;
	uint64_t	f_favail;
	uint64_t	f_fsid;
	uint64_t	f_flag;
	uint64_t	f_namemax;
};
/*
 * Async statvfs(<dirname>)
 * This function is deprecated. Use nfs_statvfs64_async() instead.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct statvfs *
 * -errno : An error occured.
 *          data is the error string.
 */
struct statvfs;
EXTERN int nfs_statvfs_async(struct nfs_context *nfs, const char *path,
                             nfs_cb cb, void *private_data);
/*
 * Sync statvfs(<dirname>)
 * This function is deprecated. Use nfs_statvfs64() instead.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_statvfs(struct nfs_context *nfs, const char *path,
                       struct statvfs *svfs);
/*
 * Async statvfs64(<dirname>)
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is struct nfs_statvfs_64 *
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_statvfs64_async(struct nfs_context *nfs, const char *path,
                               nfs_cb cb, void *private_data);
/*
 * Sync statvfs64(<dirname>)
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_statvfs64(struct nfs_context *nfs, const char *path,
                         struct nfs_statvfs_64 *svfs);

/*
 * READLINK()
 */
/*
 * Async readlink(<name>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is a char *
 *          data is only valid during the callback and is automatically freed
 *          when the callback returns.
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_readlink_async(struct nfs_context *nfs, const char *path,
                              nfs_cb cb, void *private_data);
/*
 * Sync readlink(<name>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_readlink(struct nfs_context *nfs, const char *path, char *buf,
                        int bufsize);

/*
 * Sync readlink2(<name>)
 * Function returns
 *       0 : The operation was successful.
 *  -errno : The command failed.
 * *bufptr : NULL if the command failed, otherwise the contents of the symlink.
 *           The caller must free the buffer.
 */
EXTERN int nfs_readlink2(struct nfs_context *nfs, const char *path,
                         char **bufptr);



/*
 * CHMOD()
 */
/*
 * Async chmod(<name>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_chmod_async(struct nfs_context *nfs, const char *path, int mode,
                           nfs_cb cb, void *private_data);
/*
 * Sync chmod(<name>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_chmod(struct nfs_context *nfs, const char *path, int mode);
/*
 * Async chmod(<name>)
 *
 * Like chmod except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_lchmod_async(struct nfs_context *nfs, const char *path,
                            int mode, nfs_cb cb, void *private_data);
/*
 * Sync chmod(<name>)
 *
 * Like chmod except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_lchmod(struct nfs_context *nfs, const char *path, int mode);



/*
 * FCHMOD()
 */
/*
 * Async fchmod(<handle>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_fchmod_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                            int mode, nfs_cb cb, void *private_data);
/*
 * Sync fchmod(<handle>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_fchmod(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode);



/*
 * CHOWN()
 */
/*
 * Async chown(<name>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_chown_async(struct nfs_context *nfs, const char *path, int uid,
                           int gid, nfs_cb cb, void *private_data);
/*
 * Sync chown(<name>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_chown(struct nfs_context *nfs, const char *path, int uid,
                     int gid);
/*
 * Async chown(<name>)
 *
 * Like chown except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_lchown_async(struct nfs_context *nfs, const char *path, int uid,
                            int gid, nfs_cb cb, void *private_data);
/*
 * Sync chown(<name>)
 *
 * Like chown except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_lchown(struct nfs_context *nfs, const char *path, int uid,
                      int gid);



/*
 * FCHOWN()
 */
/*
 * Async fchown(<handle>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_fchown_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                            int uid, int gid, nfs_cb cb, void *private_data);
/*
 * Sync fchown(<handle>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_fchown(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid,
                      int gid);




/*
 * UTIMES()
 */
/*
 * Async utimes(<path>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_utimes_async(struct nfs_context *nfs, const char *path,
                            struct timeval *times, nfs_cb cb,
                            void *private_data);
/*
 * Sync utimes(<path>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_utimes(struct nfs_context *nfs, const char *path,
                      struct timeval *times);
/*
 * Async utimes(<path>)
 *
 * Like utimes except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_lutimes_async(struct nfs_context *nfs, const char *path,
                             struct timeval *times, nfs_cb cb,
                             void *private_data);
/*
 * Sync utimes(<path>)
 *
 * Like utimes except if the destination is a symbolic link, it acts on the
 * symbolic link itself.
 *
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_lutimes(struct nfs_context *nfs, const char *path,
                       struct timeval *times);


/*
 * UTIME()
 */
/*
 * Async utime(<path>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
struct utimbuf;
EXTERN int nfs_utime_async(struct nfs_context *nfs, const char *path,
                           struct utimbuf *times, nfs_cb cb,
                           void *private_data);
/*
 * Sync utime(<path>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_utime(struct nfs_context *nfs, const char *path,
                     struct utimbuf *times);




/*
 * ACCESS()
 */
/*
 * Async access(<path>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_access_async(struct nfs_context *nfs, const char *path,
                            int mode, nfs_cb cb, void *private_data);
/*
 * Sync access(<path>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_access(struct nfs_context *nfs, const char *path, int mode);





/*
 * ACCESS2()
 */
/*
 * Async access2(<path>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      >= 0 : A mask of R_OK, W_OK and X_OK indicating which permissions are
 *             available.
 *             data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_access2_async(struct nfs_context *nfs, const char *path,
                             nfs_cb cb, void *private_data);
/*
 * Sync access(<path>)
 * Function returns
 *      >= 0 : A mask of R_OK, W_OK and X_OK indicating which permissions are
 *             available.
 * -errno : The command failed.
 */
EXTERN int nfs_access2(struct nfs_context *nfs, const char *path);




/*
 * SYMLINK()
 */
/*
 * Async symlink(<path>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_symlink_async(struct nfs_context *nfs, const char *target,
                             const char *linkname, nfs_cb cb,
                             void *private_data);
/*
 * Sync symlink(<path>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_symlink(struct nfs_context *nfs, const char *target,
                       const char *linkname);


/*
 * RENAME()
 */
/*
 * Async rename(<oldpath>, <newpath>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_rename_async(struct nfs_context *nfs, const char *oldpath,
                            const char *newpath, nfs_cb cb, void *private_data);
/*
 * Sync rename(<oldpath>, <newpath>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_rename(struct nfs_context *nfs, const char *oldpath,
                      const char *newpath);



/*
 * LINK()
 */
/*
 * Async link(<oldpath>, <newpath>)
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is NULL
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int nfs_link_async(struct nfs_context *nfs, const char *oldpath,
                          const char *newpath, nfs_cb cb, void *private_data);
/*
 * Sync link(<oldpath>, <newpath>)
 * Function returns
 *      0 : The operation was successful.
 * -errno : The command failed.
 */
EXTERN int nfs_link(struct nfs_context *nfs, const char *oldpath,
                    const char *newpath);


/*
 * GETEXPORTS()
 */
/*
 * Async getexports()
 * NOTE: You must include 'libnfs-raw-mount.h' to get the definitions of the
 * returned structures.
 *
 * This function will return the list of exports from an NFS server.
 *
 * Function returns
 *  0 : The command was queued successfully. The callback will be invoked once
 *      the command completes.
 * <0 : An error occured when trying to queue the command.
 *      The callback will not be invoked.
 *
 * When the callback is invoked, status indicates the result:
 *      0 : Success.
 *          data is a pointer to an exports pointer:
 *          exports export = *(exports *)data;
 * -errno : An error occured.
 *          data is the error string.
 */
EXTERN int mount_getexports_async(struct rpc_context *rpc, const char *server,
                                  rpc_cb cb, void *private_data);
/*
 * Sync getexports(<server>)
 * Function returns
 *            NULL : something failed
 *  exports export : a linked list of exported directories
 *
 * returned data must be freed by calling mount_free_export_list(exportnode);
 */
EXTERN struct exportnode *mount_getexports(const char *server);

/*
 * Sync getexports_mountport(<server>, <mountport>)
 * Function returns
 *            NULL : something failed
 *  exports export : a linked list of exported directories
 *
 * returned data must be freed by calling mount_free_export_list(exportnode);
 */
EXTERN struct exportnode *mount_getexports_mountport(const char *server, int mountport);
/*
 * Sync getexports_timeout(<server>, <timeout>)
 * Function returns
 *            NULL : something failed
 *  exports export : a linked list of exported directories
 *
 * returned data must be freed by calling mount_free_export_list(exportnode);
 */
EXTERN struct exportnode *mount_getexports_timeout(const char *server, int timeout);

EXTERN void mount_free_export_list(struct exportnode *exports);


struct nfs_server_list {
       struct nfs_server_list *next;
       char *addr;
};

/*
 * Sync find_local_servers(<server>)
 * This function will probe all local networks for NFS server.
 * This function will block for one second while awaiting for all nfs servers
 * to respond.
 *
 * Function returns
 * NULL : something failed
 *
 * struct nfs_server_list : a linked list of all discovered servers
 *
 * returned data must be freed by nfs_free_srvr_list(srv);
 */
struct nfs_server_list *nfs_find_local_servers(void);
void free_nfs_srvr_list(struct nfs_server_list *srv);

/*
 * nfs_set_poll_timeout()
 * This function sets the polling timeout used for nfs rpc calls.
 *
 * Function returns nothing.
 *
 * int milliseconds : timeout that is passed to poll(2)
 *                    to be applied in milliseconds (-1 no timeout)
 */
EXTERN void nfs_set_poll_timeout(struct nfs_context *nfs, int milliseconds);

/*
 * nfs_get_poll_timeout()
 * This function gets the polling timeout used for nfs rpc calls.
 *
 * Function returns:
 * int milliseconds : timeout that is passed to poll(2)
 *                    to be applied in milliseconds (-1 no timeout)
 */
EXTERN int nfs_get_poll_timeout(struct nfs_context *nfs);

/*
 * sync nfs_set_timeout()
 * This function sets the timeout used for nfs rpc calls.
 *
 * Function returns nothing.
 *
 * int milliseconds : timeout to be applied in milliseconds (-1 no timeout)
 *                    timeouts must currently be set in whole seconds,
 *                    i.e. units of 1000
 *
 * Note: Prefer the mount option timeo=<int> to set the timeout over directly
 *       calling nfs_set_timeout().
 */
EXTERN void nfs_set_timeout(struct nfs_context *nfs, int milliseconds);

/*
 * sync nfs_get_timeout()
 * This function gets the timeout used for nfs rpc calls.
 *
 * Function returns
 *    -1 : No timeout applied
 *   > 0 : Timeout in milliseconds
 */
EXTERN int nfs_get_timeout(struct nfs_context *nfs);

/*
 * Set the client name for NFSv4.
 */
EXTERN void nfs4_set_client_name(struct nfs_context *nfs, const char *id);

/*
 * Set the client verifier for NFSv4.
 * This an 8 byte array of random data.
 */
EXTERN void nfs4_set_verifier(struct nfs_context *nfs, const char *verifier);

/*
 * MULTITHREADING
 */        
/*
 * This function starts a separate service thread for multithreading support.
 * When multithreading is enabled the eventdriven async API is no longer
 * supported and you can only use the synchronous API.
 */
EXTERN int nfs_mt_service_thread_start(struct nfs_context *nfs);
/*
 * Shutdown multithreading support.
 */
EXTERN void nfs_mt_service_thread_stop(struct nfs_context *nfs);
        
#ifdef __cplusplus
}
#endif


/*
 * UDP contexts for UDP servers
 */
EXTERN int rpc_bind_udp(struct rpc_context *rpc, char *addr, int port);
EXTERN int rpc_set_udp_destination(struct rpc_context *rpc, char *addr, int port, int is_broadcast);
EXTERN struct rpc_context *rpc_init_udp_context(void);
EXTERN int rpc_is_udp_socket(struct rpc_context *rpc);

/* UDP server sockets */
/* The ip address of the udp client */
EXTERN struct sockaddr *rpc_get_udp_src_sockaddr(struct rpc_context *rpc);
#ifdef __linux__
/* The ip address where we received the udp packet from the client */
EXTERN struct sockaddr *rpc_get_udp_dst_sockaddr(struct rpc_context *rpc);
#endif

void rpc_set_resiliency(struct rpc_context *rpc,
			int num_tcp_reconnect,
			int timeout,
			int retrans);


#endif /* !_LIBNFS_H_ */
