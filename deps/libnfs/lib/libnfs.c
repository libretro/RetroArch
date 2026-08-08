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
 * High level api to nfs filesystems
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#if defined(__ANDROID__) && !defined(HAVE_SYS_STATVFS_H)
#define statvfs statfs
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef WIN32
#include <win32/win32_compat.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "slist.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-raw-mount.h"
#include "libnfs-raw-portmap.h"
#include "libnfs-private.h"

#ifdef HAVE_LIBKRB5
#include "krb5-wrapper.h"
#endif

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

static const char *oom = "out of memory";

void
nfs_free_nfsdir(struct nfsdir *nfsdir)
{
	while (nfsdir->entries) {
		struct nfsdirent *dirent = nfsdir->entries->next;
		if (nfsdir->entries->name != NULL) {
			free(nfsdir->entries->name);
		}
		free(nfsdir->entries);
		nfsdir->entries = dirent;
	}
	free(nfsdir->fh.val);
	free(nfsdir);
}

void
nfs_dircache_add(struct nfs_context *nfs, struct nfsdir *nfsdir)
{
	int i = 0;
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->rpc->rpc_mutex);
        }
#endif
	LIBNFS_LIST_ADD(&nfs->nfsi->dircache, nfsdir);

	for (nfsdir = nfs->nfsi->dircache; nfsdir; nfsdir = nfsdir->next, i++) {
		if (i > MAX_DIR_CACHE) {
			LIBNFS_LIST_REMOVE(&nfs->nfsi->dircache, nfsdir);
			nfs_free_nfsdir(nfsdir);
			break;
		}
	}
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->rpc->rpc_mutex);
        }
#endif
}

struct nfsdir *
nfs_dircache_find(struct nfs_context *nfs, struct nfs_fh *fh)
{
	struct nfsdir *nfsdir;

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->rpc->rpc_mutex);
        }
#endif
	for (nfsdir = nfs->nfsi->dircache; nfsdir; nfsdir = nfsdir->next) {
		if (nfsdir->fh.len == fh->len &&
		    !memcmp(nfsdir->fh.val, fh->val, fh->len)) {
			LIBNFS_LIST_REMOVE(&nfs->nfsi->dircache, nfsdir);
                        break;
		}
	}

#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->rpc->rpc_mutex);
        }
#endif
	return nfsdir;
}

void
nfs_dircache_drop(struct nfs_context *nfs, struct nfs_fh *fh)
{
	struct nfsdir *cached;

	cached = nfs_dircache_find(nfs, fh);
	if (cached) {
		nfs_free_nfsdir(cached);
	}
}

void
nfs_set_auth(struct nfs_context *nfs, struct AUTH *auth)
{
	rpc_set_auth(nfs->rpc, auth);
}

void
nfs_set_security(struct nfs_context *nfs, enum rpc_sec sec)
{
#ifdef HAVE_LIBKRB5
        nfs->rpc->wanted_sec = sec;
#endif
}

#ifdef HAVE_TLS
void
nfs_set_xprtsecurity(struct nfs_context *nfs, enum rpc_xprtsec xprtsec)
{
	/* Ensure only permissible values are being set */
	assert(xprtsec == RPC_XPRTSEC_NONE ||
	       xprtsec == RPC_XPRTSEC_TLS ||
	       xprtsec == RPC_XPRTSEC_MTLS);

	nfs->rpc->wanted_xprtsec = xprtsec;
}
#endif /* HAVE_TLS */

int
nfs_get_fd(struct nfs_context *nfs)
{
	return rpc_get_fd(nfs->rpc);
}

int
nfs_queue_length(struct nfs_context *nfs)
{
	return rpc_queue_length(nfs->rpc);
}

int
nfs_which_events(struct nfs_context *nfs)
{
	return rpc_which_events(nfs->rpc);
}

int
nfs_service(struct nfs_context *nfs, int revents)
{
	return rpc_service(nfs->rpc, revents);
}

char *
nfs_get_error(struct nfs_context *nfs)
{
#ifdef HAVE_MULTITHREADING
        if (nfs && nfs->rpc->multithreading_enabled) {
                struct nfs_thread_context *ntc;

                for(ntc = nfs->nfsi->thread_ctx; ntc; ntc = ntc->next) {
                        if (nfs_mt_get_tid() == ntc->tid) {
                                nfs = &ntc->nfs;
                                break;
                        }
                }
        }
#endif
	return nfs->error_string ? nfs->error_string : "";
};

#ifdef HAVE_SO_BINDTODEVICE
void
nfs_set_interface(struct nfs_context *nfs, const char *ifname)
{
	rpc_set_interface(nfs_get_rpc_context(nfs), ifname);
}
#endif

static int
nfs_set_context_args_no_val(struct nfs_context *nfs, const char *arg)
{
	if (!strcmp(arg, "readonly")) {
		nfs_set_readonly(nfs, 1);
	} else {
                nfs_set_error(nfs, "Unknown url argument : %s",
                              arg);
                return -1;
        }
	return 0;
}

static int
nfs_set_context_args(struct nfs_context *nfs, const char *arg, const char *val)
{
	if (!strcmp(arg, "tcp-syncnt")) {
		rpc_set_tcp_syncnt(nfs_get_rpc_context(nfs), atoi(val));
	} else if (!strcmp(arg, "uid")) {
		rpc_set_uid(nfs_get_rpc_context(nfs), atoi(val));
	} else if (!strcmp(arg, "gid")) {
		rpc_set_gid(nfs_get_rpc_context(nfs), atoi(val));
	} else if (!strcmp(arg, "timeo")) {
		/* val is in deci-seconds */
		const int timeout_msecs = atoi(val) * 100;
		if (timeout_msecs < (10 * 1000)) {
			nfs_set_error(nfs, "timeo cannot be less than 100: %s", val);
			return -1;
		}
		nfs_set_timeout(nfs, timeout_msecs);
	} else if (!strcmp(arg, "retrans")) {
		const int retrans = atoi(val);
		if (retrans < 0) {
			nfs_set_error(nfs, "retrans cannot be less than 0: %s", val);
			return -1;
		}
		nfs_set_retrans(nfs, retrans);
	} else if (!strcmp(arg, "debug")) {
		rpc_set_debug(nfs_get_rpc_context(nfs), atoi(val));
	} else if (!strcmp(arg, "auto-traverse-mounts")) {
		nfs_set_auto_traverse_mounts(nfs, atoi(val));
	} else if (!strcmp(arg, "dircache")) {
		nfs_set_dircache(nfs, atoi(val));
	} else if (!strcmp(arg, "autoreconnect")) {
		nfs_set_autoreconnect(nfs, atoi(val));
#ifdef HAVE_SO_BINDTODEVICE
	} else if (!strcmp(arg, "if")) {
		nfs_set_interface(nfs, val);
#endif
	} else if (!strcmp(arg, "version")) {
		if (nfs_set_version(nfs, atoi(val)) < 0) {
			nfs_set_error(nfs, "NFS version %d is not supported",
				      atoi(val));
			return -1;
		}
	} else if (!strcmp(arg, "nfsport")) {
		nfs_set_nfsport(nfs, atoi(val));
	} else if (!strcmp(arg, "mountport")) {
		nfs_set_mountport(nfs, atoi(val));
	} else if (!strcmp(arg, "rsize")) {
		nfs_set_readmax(nfs, atoi(val));
	} else if (!strcmp(arg, "wsize")) {
		nfs_set_writemax(nfs, atoi(val));
	} else if (!strcmp(arg, "readdir-buffer")) {
		const char *strp = strchr(val, ',');
		if (strp) {
			strp++;
			nfs_set_readdir_max_buffer_size(nfs, atoi(val), atoi(strp));
		} else {
			nfs_set_readdir_max_buffer_size(nfs, atoi(val), atoi(val));
		}
#ifdef HAVE_TLS
	} else if (nfs->rpc && !strcmp(arg, "xprtsec")) {
		if (!strcmp(val, "none")) {
			nfs_set_xprtsecurity(nfs, RPC_XPRTSEC_NONE);
		} else if (!strcmp(val, "tls")) {
			nfs_set_xprtsecurity(nfs, RPC_XPRTSEC_TLS);
		} else  if (!strcmp(val, "mtls")) {
			nfs_set_xprtsecurity(nfs, RPC_XPRTSEC_MTLS);
		} else {
			nfs_set_error(nfs, "Unknown/unsupported xprtsec type : %s", val);
			return -1;
		}
#endif /* HAVE_TLS */
#ifdef HAVE_LIBKRB5
	} else if (nfs->rpc && !strcmp(arg, "sec")) {
                /*
                 * We switch to AUTH_GSS after the first call to NFS/NULL call.
                 */
                if (!strcmp(val, "krb5p")) {
                        nfs_set_security(nfs, RPC_SEC_KRB5P);
                } else if (!strcmp(val, "krb5i")) {
                        nfs_set_security(nfs, RPC_SEC_KRB5I);
                } else  if (!strcmp(val, "krb5")) {
                        nfs_set_security(nfs, RPC_SEC_KRB5);
                } else {
			nfs_set_error(nfs, "Unknown/unsupported sec type : %s",
				      val);
			return -1;
                }
#endif
	} else {
                nfs_set_error(nfs, "Unknown url argument : %s",
                              arg);
                return -1;
        }
	return 0;
}

static int
tohex(char ch)
{
        if (ch >= '0' && ch <= '9') {
                return ch - '0';
        }
        ch &= 0xDF;
        if (ch >= 'A' && ch <= 'F') {
                return ch - 'A' + 10;
        }
        return -1;
}

static struct nfs_url *
nfs_parse_url(struct nfs_context *nfs, const char *url, int dir, int incomplete)
{
	struct nfs_url *urls;
	char *strp, *flagsp, *strp2, ch, *original_server, *port_str, *slash_pos, *end_ptr;
        int tmp;
        size_t port_len;

	if (strncmp(url, "nfs://", 6)) {
		nfs_set_error(nfs, "Invalid URL specified");
		return NULL;
	}

	urls = calloc(1, sizeof(struct nfs_url));
	if (urls == NULL) {
		nfs_set_error(nfs, "Out of memory");
		return NULL;
	}

	urls->server = strdup(url + 6);
	if (urls->server == NULL) {
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Out of memory");
		return NULL;
	}

        /* unescape all % hex hex characters */
        strp = urls->server;
        while (strp && *strp) {
                strp = strchr(strp, '%');
                if (strp == NULL) {
                        break;
                }
                tmp = tohex(strp[1]);
                if (tmp < 0) {
                        strp++;
                        continue;
                }
                ch = (tmp & 0x0f) << 4;
                tmp = tohex(strp[2]);
                if (tmp < 0) {
                        strp++;
                        continue;
                }
                ch |= tmp & 0x0f;
                *strp = ch;
                memmove(strp + 1, strp + 3, strlen(strp + 3) + 1);
                strp++;
        }

	if (urls->server[0] == '/' || urls->server[0] == '\0' ||
		urls->server[0] == '?') {
		if (incomplete) {
			flagsp = strchr(urls->server, '?');
			goto flags;
		}
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Invalid server string");
		return NULL;
	}

	strp = strchr(urls->server, ':');
        if (strp) {
                strp++;
                slash_pos = strchr(strp, '/');
                port_len = slash_pos ? (size_t)(slash_pos - strp) : strlen(strp);
                port_str = (char*)malloc(port_len + 1);
                if (!port_str)
                {
                        nfs_destroy_url(urls);
                        nfs_set_error(nfs, "Out of memory");
                        return NULL;
                }
                strncpy(port_str, strp, port_len);
                port_str[port_len] = '\0';
                int port_num = strtol(port_str, &end_ptr, 10);
                if (end_ptr == port_str || port_num < 0 || port_num > 65535)
                {
                        free(port_str);
                        nfs_destroy_url(urls);
                        nfs_set_error(nfs, "Invalid port number");
                        return NULL;
                }
		nfs->nfsi->nfsport =  port_num;
                if (slash_pos == NULL)
                {
                        *strp = '\0';
                }
                else
                {
                        memmove(strp - 1, slash_pos, strlen(slash_pos) + 1);
                }

                free(port_str);
        }

	if (strp == NULL)
		strp = urls->server;

	strp = strchr(urls->server, '/');
	if (strp == NULL) {
		if (incomplete) {
			flagsp = strchr(urls->server, '?');
			goto flags;
		}
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Incomplete or invalid URL specified.");
		return NULL;
	}

	urls->path = strdup(strp);
	if (urls->path == NULL) {
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Out of memory");
		return NULL;
	}
	*strp = 0;

	if (dir) {
		flagsp = strchr(urls->path, '?');
		goto flags;
	}

	strp = strrchr(urls->path, '/');
	if (strp == NULL) {
		if (incomplete) {
			flagsp = strchr(urls->path, '?');
			goto flags;
		}
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Incomplete or invalid URL specified.");
		return NULL;
	}
	urls->file = strdup(strp);
	if (urls->path == NULL) {
		nfs_destroy_url(urls);
		nfs_set_error(nfs, "Out of memory");
		return NULL;
	}
	*strp = 0;
	flagsp = strchr(urls->file, '?');

flags:
	if (flagsp) {
		*flagsp = 0;
	}

	if (urls->file && !strlen(urls->file)) {
		free(urls->file);
		urls->file = NULL;
		if (!incomplete) {
			nfs_destroy_url(urls);
			nfs_set_error(nfs, "Incomplete or invalid URL "
                                      "specified.");
			return NULL;
		}
	}

	while (flagsp != NULL && *(flagsp+1) != 0) {
		strp = flagsp + 1;
		flagsp = strchr(strp, '&');
		if (flagsp) {
			*flagsp = 0;
		}
		strp2 = strchr(strp, '=');
		if (strp2) {
			*strp2 = 0;
			strp2++;
			if (nfs_set_context_args(nfs, strp, strp2) != 0) {
                                nfs_destroy_url(urls);
				return NULL;
                        }
		} else {
			if (nfs_set_context_args_no_val(nfs, strp) != 0) {
                                nfs_destroy_url(urls);
				return NULL;
                        }
                }
	}

        strp =strchr(urls->server, '@');
        if (strp && nfs->rpc) {
                *strp++ = '\0';
                if (rpc_set_username(nfs->rpc, urls->server) != 0) {
                        nfs_destroy_url(urls);
                        return NULL;
                }
                original_server = urls->server;
                urls->server = strdup(strp);
		if (urls->server == NULL) {
                        urls->server = original_server;
			nfs_destroy_url(urls);
			rpc_set_error(nfs->rpc,
				      "Out of memory: Failed to allocate "
				      "server name");
			return NULL;
		}
                free(original_server);
        }
	if (urls->server && strlen(urls->server) <= 1) {
		free(urls->server);
		urls->server = NULL;
	}

	urls->port = nfs->nfsi->nfsport;
	if (nfs->nfsi->mountport)
		urls->port = nfs->nfsi->mountport;

#ifdef HAVE_TLS
	/*
	 * Call this in the end after all options are processed, as it uses
	 * rpc->debug.
	 */ 
	if (nfs->rpc->wanted_xprtsec == RPC_XPRTSEC_TLS ||
            nfs->rpc->wanted_xprtsec == RPC_XPRTSEC_MTLS) {
		/* tls_global_init() MUST succeed for us to use TLS security */
		if (tls_global_init(nfs->rpc) != 0) {
                        nfs_set_error(nfs, "tls_global_init() failed!");
                        nfs_destroy_url(urls);
                        return NULL;
		}
	}
#endif

	return urls;
}

struct nfs_url *
nfs_parse_url_full(struct nfs_context *nfs, const char *url)
{
	return nfs_parse_url(nfs, url, 0, 0);
}

struct nfs_url *
nfs_parse_url_dir(struct nfs_context *nfs, const char *url)
{
	return nfs_parse_url(nfs, url, 1, 0);
}

struct nfs_url *
nfs_parse_url_incomplete(struct nfs_context *nfs, const char *url)
{
	return nfs_parse_url(nfs, url, 0, 1);
}


void
nfs_destroy_url(struct nfs_url *url)
{
	if (url) {
		free(url->server);
		free(url->path);
		free(url->file);
	}
	free(url);
}

#define MAX_CLIENT_NAME 64

struct nfs_context *
nfs_init_context(void)
{
	struct nfs_context *nfs;
	struct nfs_context_internal *nfsi;
#ifdef HAVE_LIBKRB5
	char *login;
#if defined(HAVE_PWD_H) && defined(HAVE_UNISTD_H)
	struct passwd *euid_passwd;
#endif
#endif
        int i;
        uint64_t v;
        verifier4 verifier;
        char client_name[MAX_CLIENT_NAME];

	nfsi = calloc(1, sizeof(struct nfs_context_internal));
	if (nfsi == NULL) {
		return NULL;
	}

	nfs = calloc(1, sizeof(struct nfs_context));
	if (nfs == NULL) {
                free(nfsi);
		return NULL;
	}

        nfs->nfsi = nfsi;
	nfs->rpc = rpc_init_context();
	if (nfs->rpc == NULL) {
		free(nfs->nfsi);
		free(nfs);
		return NULL;
	}
#ifdef HAVE_LIBKRB5
	login = getenv("USER");
#if defined(HAVE_PWD_H) && defined(HAVE_UNISTD_H)
	if (login == NULL) {
		euid_passwd = getpwuid(geteuid());
		if (euid_passwd) {
			login = euid_passwd->pw_name;
		}
	}
#endif
	rpc_set_username(nfs->rpc, login ? login : "");
#endif
	nfs->nfsi->cwd = strdup("/");
	nfs->nfsi->mask = 022;
	nfs->nfsi->auto_traverse_mounts = 1;
	nfs->nfsi->dircache_enabled = 1;

	/*
	 * Default resiliency parameters are chosen with safe values that
	 * emulate "hard" mount, which means on any error (RPC or TCP) keep
	 * retrying indefinitely.
	 *
	 * TCP reconnect is indefinitely tried, RPC requests time out after
	 * 60 secs and we retry an RPC request 2 times before declaring it as
	 * "major timeout" and running the major timeout recovery workflow,
	 * after which the whole RPC retransmit cycle restarts and this continues
	 * indefinitely.
	 */
	nfs->nfsi->auto_reconnect = -1;
	nfs->nfsi->timeout = 60*1000;
	nfs->nfsi->retrans = 2;

	nfs->nfsi->default_version = NFS_V3;
	nfs->nfsi->version = NFS_V3;
	nfs->nfsi->readmax = NFS_DEF_XFER_SIZE;
	nfs->nfsi->writemax = NFS_DEF_XFER_SIZE;
	nfs->nfsi->readdir_dircount = 8192;
	nfs->nfsi->readdir_maxcount = 8192;

        /* NFSv4 parameters */
        /* We need a "random" initial verifier */
        v = rpc_current_time() << 32 | getpid();
        for (i = 0; i < NFS4_VERIFIER_SIZE; i++) {
                verifier[i] = v & 0xff;
                v >>= 8;
        }
        nfs4_set_verifier(nfs, verifier);

        snprintf(client_name, MAX_CLIENT_NAME, "Libnfs pid:%d %d", getpid(),
                 (int)time(NULL));
        nfs4_set_client_name(nfs, client_name);

#ifdef HAVE_MULTITHREADING
        nfs_mt_mutex_init(&nfs->nfsi->nfs_mutex);
        nfs_mt_mutex_init(&nfs->nfsi->nfs4_open_counter_mutex);
        nfs_mt_mutex_init(&nfs->nfsi->nfs4_open_call_mutex);
#endif /* HAVE_MULTITHREADING */

#ifdef HAVE_SIGNAL_H
#if !defined(WIN32)
	/*
	 * Ignore SIGPIPE when writing to sockets where peer decides to close
	 * the door on us, rather write()/dup2() should fail with EPIPE which
	 * we can gracefully handle.
	 */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		nfs_destroy_context(nfs);
		return NULL;
	}
#endif /* WIN32 */
#endif /* HAVE_SIGNAL_H */
	return nfs;
}

void
nfs4_set_client_name(struct nfs_context *nfs, const char *client_name)
{
        free(nfs->nfsi->client_name);
        nfs->nfsi->client_name = strdup(client_name);
}

void
nfs4_set_verifier(struct nfs_context *nfs, const char *verifier)
{
        memcpy(nfs->nfsi->verifier, verifier, NFS4_VERIFIER_SIZE);
}

void
nfs_destroy_context(struct nfs_context *nfs)
{
	while (nfs->nfsi->nested_mounts) {
		struct nested_mounts *mnt = nfs->nfsi->nested_mounts;

		LIBNFS_LIST_REMOVE(&nfs->nfsi->nested_mounts, mnt);
		free(mnt->path);
		free(mnt->fh.val);
                free(mnt);
	}

	rpc_destroy_context(nfs->rpc);
	nfs->rpc = NULL;

	if (nfs->error_string && nfs->error_string != oom) {
		free(nfs->error_string);
		nfs->error_string = NULL;
	}

        free(nfs->nfsi->server);
        free(nfs->nfsi->export);
        free(nfs->nfsi->cwd);
        free(nfs->nfsi->rootfh.val);
        free(nfs->nfsi->client_name);
	while (nfs->nfsi->dircache) {
		struct nfsdir *nfsdir = nfs->nfsi->dircache;
		LIBNFS_LIST_REMOVE(&nfs->nfsi->dircache, nfsdir);
		nfs_free_nfsdir(nfsdir);
	}

#ifdef HAVE_MULTITHREADING
        nfs_mt_mutex_destroy(&nfs->nfsi->nfs4_open_call_mutex);
        nfs_mt_mutex_destroy(&nfs->nfsi->nfs4_open_counter_mutex);
        nfs_mt_mutex_destroy(&nfs->nfsi->nfs_mutex);
        while (nfs->nfsi->thread_ctx) {
                struct nfs_thread_context *tmp = nfs->nfsi->thread_ctx->next;
                free(nfs->nfsi->thread_ctx->nfs.error_string);
                free(nfs->nfsi->thread_ctx);
                nfs->nfsi->thread_ctx = tmp;
        }
#endif /* HAVE_MULTITHREADING */
	free(nfs->nfsi);
	free(nfs);
}

struct rpc_cb_data {
       char *server;
       uint32_t program;
       uint32_t version;

       rpc_cb cb;
       void *private_data;
};

void free_rpc_cb_data(struct rpc_cb_data *data)
{
	free(data->server);
	data->server = NULL;
	free(data);
}

static int
rpc_connect_port_internal(struct rpc_context *rpc, int port, struct rpc_cb_data *data);

#ifdef HAVE_LIBKRB5
struct rpc_pdu *
rpc_null_task_gss(struct rpc_context *rpc, int program, int version,
                  rpc_gss_init_arg *arg,
                  rpc_cb cb, void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, program, version, 0, cb, private_data,
                               (zdrproc_t)zdr_rpc_gss_init_res, sizeof(struct rpc_gss_init_res));
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu "
                              "for NULL call");
		return NULL;
	}

        /* add the krb5 blob */
	if (zdr_rpc_gss_init_arg(&pdu->zdr, arg) == 0) {
		rpc_set_error(rpc, "ZDR error: Failed to encode blob");
		rpc_free_pdu(rpc, pdu);
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu "
                              "for NULL call");
		return NULL;
	}

	return pdu;
}

static void
rpc_connect_program_6_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

	data->cb(rpc, status, NULL, data->private_data);
	free_rpc_cb_data(data);
}
#endif /* HAVE_LIBKRB5 */

#ifdef HAVE_TLS
void free_tls_cb_data(struct tls_cb_data *data)
{
	free(data);
}

/*
 * Callback function called when we get a response for an AUTH_TLS NULL RPC
 * that we sent to the server.
 * On a successful response confirming server support for TLS, this will
 * initiate an async TLS handshake process.
 */
static void
rpc_connect_program_4_1_cb(struct rpc_context *rpc, int status,
			   void *command_data, void *private_data)
{
	struct tls_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	RPC_LOG(rpc, 2, "Got AUTH_TLS response, status=%d", status);

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_tls_cb_data(data);
		return;
	}

	/*
	 * Ok, server supports RPC-with-TLS, start handshake.
	 */
	rpc->tls_context.data = *data;
	free_tls_cb_data(data);
	data = &rpc->tls_context.data;

	rpc->tls_context.state = do_tls_handshake(rpc);

	switch (rpc->tls_context.state) {
		case TLS_HANDSHAKE_IN_PROGRESS:
			/*
			 * We will continue this asynchronously in rpc_service(), as we
			 * hear from the peer.
			 */
			return;
		case TLS_HANDSHAKE_COMPLETED:
			RPC_LOG(rpc, 2, "do_tls_handshake: TLS handshake completed "
					"synchronously on fd %d", rpc->fd);
			data->cb(rpc, RPC_STATUS_SUCCESS, NULL, data->private_data);
			break;
		case TLS_HANDSHAKE_FAILED:
			RPC_LOG(rpc, 2, "do_tls_handshake: Failed to start TLS handshake, or "
					"TLS handshake failed synchronously on fd %d", rpc->fd);
			data->cb(rpc, RPC_STATUS_ERROR, rpc_get_error(rpc), data->private_data);
			break;
		default:
			/* Should not return any other status */
			assert(0);
	}
}
#endif /* HAVE_TLS */

static void
rpc_connect_program_5_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Dont want any more callbacks even if the socket is closed */
	rpc->connect_cb = NULL;

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

#ifdef HAVE_LIBKRB5
        if (data->program == 100003 && rpc->wanted_sec != RPC_SEC_UNDEFINED && rpc->username) {
                rpc_gss_init_arg gia;

                rpc->sec = rpc->wanted_sec;

                libnfs_authgss_init(rpc);
                rpc->auth_data = krb5_auth_init(rpc,
                                                data->server,
                                                rpc->username,
                                                rpc->wanted_sec);
                if (rpc->auth_data == NULL) {
                        data->cb(rpc, RPC_STATUS_ERROR, rpc_get_error(rpc),
                                 data->private_data);
                        free_rpc_cb_data(data);
                        return;
                }

                if (krb5_auth_request(rpc, rpc->auth_data,
                                      NULL, 0) < 0) {
                        data->cb(rpc, RPC_STATUS_ERROR, rpc_get_error(rpc),
                                 data->private_data);
                        free_rpc_cb_data(data);
                        return;
                }


                gia.gss_token.gss_token_len = krb5_get_output_token_length(rpc->auth_data);
                gia.gss_token.gss_token_val = (char *)krb5_get_output_token_buffer(rpc->auth_data);
                if (rpc_null_task_gss(rpc, data->program, data->version,
                                  &gia,
                                  rpc_connect_program_6_cb, data) == NULL) {
                        data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
                        free_rpc_cb_data(data);
                        return;
                }
                return;
        }
#endif
	data->cb(rpc, status, NULL, data->private_data);
	free_rpc_cb_data(data);
}

static void
rpc_connect_program_4_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Dont want any more callbacks even if the socket is closed */
	rpc->connect_cb = NULL;

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

#ifdef HAVE_TLS
	/*
	 * Connected to RPC endpoint, for NFS connections see if we need to secure them.
	 * If yes, we query the server TLS support by sending a NULL RPC with auth flavor
	 * AUTH_TLS and if server supports RPC-with-TLS we initiate the TLS handshake.
	 */
	rpc->use_tls = (data->program == NFS_PROGRAM) &&
				(rpc->wanted_xprtsec == RPC_XPRTSEC_TLS ||
				 rpc->wanted_xprtsec == RPC_XPRTSEC_MTLS);
	if (rpc->use_tls) {
		/* We should not use TLS for anything other than NFS */
		assert(data->program == NFS_PROGRAM);

		if (rpc_null_task_authtls(rpc, data->version,
					  rpc_connect_program_5_cb, data) == NULL) {
			data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
			free_rpc_cb_data(data);
			return;
		}
	} else
#endif
        if (rpc_null_task(rpc, data->program, data->version,
                          rpc_connect_program_5_cb, data) == NULL) {
                data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
                free_rpc_cb_data(data);
                return;
        }
}

static void
rpc_connect_program_3_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;
	struct pmap3_string_result *gar;
	uint32_t rpc_port = 0;
	char *ptr;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

	switch (rpc->s.ss_family) {
	case AF_INET:
		rpc_port = *(uint32_t *)(void *)command_data;
		break;
	case AF_INET6:
		/* ouch. portmapper and ipv6 are not great */
		gar = command_data;
		if (gar->addr == NULL) {
			break;
		}
		ptr = strrchr(gar->addr, '.');
		if (ptr == NULL) {
			break;
		}
		rpc_port = atoi(ptr + 1);
		*ptr = 0;
		ptr = strrchr(gar->addr, '.');
		if (ptr == NULL) {
			break;
		}
		rpc_port += 256 * atoi(ptr + 1);
		break;
	}
	if (rpc_port == 0) {
		rpc_set_error(rpc, "RPC error. Program is not available on %s",
			      data->server);
		data->cb(rpc, RPC_STATUS_ERROR, rpc_get_error(rpc),
			 data->private_data);
		free_rpc_cb_data(data);
		return;
	}

	rpc_disconnect(rpc, "normal disconnect");
        rpc->program = data->program;
        rpc->version = data->version;

        if (rpc_connect_port_internal(rpc, rpc_port, data)) {
		data->cb(rpc, RPC_STATUS_ERROR, command_data,
                         data->private_data);
		free_rpc_cb_data(data);
                return;
        }
}

static void
rpc_connect_program_2_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;
	struct pmap3_mapping map;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

	switch (rpc->s.ss_family) {
	case AF_INET:
		if (rpc_pmap2_getport_task(rpc, data->program, data->version,
                                           IPPROTO_TCP,
                                           rpc_connect_program_3_cb,
                                           private_data) == NULL) {
			data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
			free_rpc_cb_data(data);
			return;
		}
		break;
	case AF_INET6:
		map.prog=data->program;
		map.vers=data->version;
		map.netid="";
		map.addr="";
		map.owner="";
		if (rpc_pmap3_getaddr_task(rpc, &map,
                                           rpc_connect_program_3_cb,
                                           private_data) == NULL) {
			data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
			free_rpc_cb_data(data);
			return;
		}
		break;
	}
}

static void
rpc_connect_program_1_cb(struct rpc_context *rpc, int status,
                         void *command_data, void *private_data)
{
	struct rpc_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Dont want any more callbacks even if the socket is closed */
	rpc->connect_cb = NULL;

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, status, command_data, data->private_data);
		free_rpc_cb_data(data);
		return;
	}

	switch (rpc->s.ss_family) {
	case AF_INET:
		if (rpc_pmap2_null_task(rpc, rpc_connect_program_2_cb,
                                         data) == NULL) {
			data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
			free_rpc_cb_data(data);
			return;
		}
		break;
	case AF_INET6:
		if (rpc_pmap3_null_task(rpc, rpc_connect_program_2_cb,
                                        data) == NULL) {
			data->cb(rpc, RPC_STATUS_ERROR, command_data, data->private_data);
			free_rpc_cb_data(data);
			return;
		}
		break;
	}
}

static int
rpc_connect_port_internal(struct rpc_context *rpc, int port, struct rpc_cb_data *data)
{
        if (rpc_connect_async(rpc, data->server, port,
                              rpc_connect_program_4_cb, data) != 0) {
		return -1;
	}

        return 0;
}

int
rpc_connect_port_async(struct rpc_context *rpc, const char *server,
                       int port,
                       int program, int version,
                       rpc_cb cb, void *private_data)
{
	struct rpc_cb_data *data;

        rpc->program = program;
        rpc->version = version;

	data = calloc(1, sizeof(struct rpc_cb_data));
	if (data == NULL) {
		return -1;
	}
	data->server       = strdup(server);
	data->program      = program;
	data->version      = version;

	data->cb           = cb;
	data->private_data = private_data;

        if (rpc_connect_port_internal(rpc, port, data)) {
		rpc_set_error(rpc, "Failed to start connection. %s",
                              rpc_get_error(rpc));
		free_rpc_cb_data(data);
                return -1;
        }
        return 0;
}

int
rpc_connect_program_async(struct rpc_context *rpc, const char *server,
                          int program, int version,
                          rpc_cb cb, void *private_data)
{
	struct rpc_cb_data *data;

	data = calloc(1, sizeof(struct rpc_cb_data));
	if (data == NULL) {
		return -1;
	}
	data->server       = strdup(server);
	data->program      = program;
	data->version      = version;

	data->cb           = cb;
	data->private_data = private_data;

        rpc->program = 100001;
        rpc->version = 2;
        
	if (rpc_connect_async(rpc, server, 111, rpc_connect_program_1_cb,
                              data) != 0) {
		rpc_set_error(rpc, "Failed to start connection. %s",
                              rpc_get_error(rpc));
		free_rpc_cb_data(data);
		return -1;
	}
	return 0;
}

void
free_nfs_cb_data(struct nfs_cb_data *data)
{
	if (data->continue_data && data->free_continue_data) {
		data->free_continue_data(data->continue_data);
	}

	free(data->saved_path);
	free(data->fh.val);
	if (!data->not_my_buffer) {
		free(data->buffer);
	}

	free(data);
}

void
nfs_free_nfsfh(struct nfsfh *nfsfh)
{
	if (nfsfh->fh.val != NULL) {
		free(nfsfh->fh.val);
		nfsfh->fh.len = 0;
		nfsfh->fh.val = NULL;
	}
	free(nfsfh);
}

/*
 * Async call for mounting an nfs share and geting the root filehandle
 */
int
_nfs_mount_async(struct nfs_context *nfs, const char *server,
                const char *export, nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_mount_async(nfs, server, export, cb, private_data);
        case NFS_V4:
                return nfs4_mount_async(nfs, server, export, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}
int
nfs_mount_async(struct nfs_context *nfs, const char *server,
                const char *export, nfs_cb cb, void *private_data)
{
        return _nfs_mount_async(nfs, server,
                                export, cb, private_data);
}

/*
 * Async call for umounting an nfs share
 */
int
nfs_umount_async(struct nfs_context *nfs, nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_umount_async(nfs, cb, private_data);
        case NFS_V4:
                /* umount is a no-op in v4 */
                (*cb)(0, nfs, NULL, private_data);
                return 0;
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_normalize_path(struct nfs_context *nfs, char *path)
{
	char *str;
	size_t len;

	/* // -> / */
	while ((str = strstr(path, "//"))) {
		while(*str) {
			*str = *(str + 1);
			str++;
		}
	}

	/* /./ -> / */
	while ((str = strstr(path, "/./"))) {
		while(*(str + 1)) {
			*str = *(str + 2);
			str++;
		}
	}

	/* ^/../ -> error */
	if (!strncmp(path, "/../", 4)) {
		nfs_set_error(nfs,
			"Absolute path starts with '/../' "
			"during normalization");
		return -1;
	}

	/* ^[^/] -> error */
	if (path[0] != '/') {
		nfs_set_error(nfs,
			"Absolute path does not start with '/'");
		return -1;
	}

	/* /string/../ -> / */
	while ((str = strstr(path, "/../"))) {
		char *tmp;

		if (!strncmp(path, "/../", 4)) {
			nfs_set_error(nfs,
				"Absolute path starts with '/../' "
				"during normalization");
			return -1;
		}

		tmp = str - 1;
		while (*tmp != '/') {
			tmp--;
		}
		str += 3;
		while((*(tmp++) = *(str++)) != '\0')
			;
	}

	/* /$ -> \0 */
	len = strlen(path);
	if (len > 1) {
		if (path[len - 1] == '/') {
			path[len - 1] = '\0';
			len--;
		}
	}
	if (path[0] == '\0') {
		nfs_set_error(nfs,
			"Absolute path became '' "
			"during normalization");
		return -1;
	}

	/* /.$ -> \0 */
	if (len >= 2) {
		if (!strcmp(&path[len - 2], "/.")) {
			path[len - 2] = '\0';
			len -= 2;
		}
	}

	/* ^/..$ -> error */
	if (!strcmp(path, "/..")) {
		nfs_set_error(nfs,
			"Absolute path is '/..' "
			"during normalization");
		return -1;
	}

	/* /string/..$ -> / */
	if (len >= 3) {
		if (!strcmp(&path[len - 3], "/..")) {
			char *tmp = &path[len - 3];
			while (*--tmp != '/')
				;
			*tmp = '\0';
		}
	}

	return 0;
}

int
nfs_stat_async(struct nfs_context *nfs, const char *path,
               nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_stat_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv4",
                              __FUNCTION__);
                return -1;
        }
}

int
nfs_stat64_async(struct nfs_context *nfs, const char *path,
                 nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_stat64_async(nfs, path, 0,
                                         cb, private_data);
        case NFS_V4:
                return nfs4_stat64_async(nfs, path, 0,
                                         cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_lstat64_async(struct nfs_context *nfs, const char *path,
                  nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_stat64_async(nfs, path, 1,
                                         cb, private_data);
        case NFS_V4:
                return nfs4_stat64_async(nfs, path, 1,
                                         cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_open2_async(struct nfs_context *nfs, const char *path, int flags,
                int mode, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly && (flags & (O_WRONLY|O_RDWR|O_APPEND|O_CREAT|O_TRUNC))) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_open_async(nfs, path, flags, mode,
                                       cb, private_data);
        case NFS_V4:
                return nfs4_open_async(nfs, path, flags, mode,
                                       cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_open_async(struct nfs_context *nfs, const char *path, int flags,
               nfs_cb cb, void *private_data)
{
        return nfs_open2_async(nfs, path, flags, 0666 & ~nfs->nfsi->mask,
                               cb, private_data);
}

int
nfs_chdir_async(struct nfs_context *nfs, const char *path,
                nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_chdir_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_chdir_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

static int
__nfs_pread_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                  void *buf, size_t count, uint64_t offset,
                  nfs_cb cb, void *private_data, int update_pos)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_pread_async_internal(nfs, nfsfh,
                                                 buf, count, offset,
                                                 cb, private_data, update_pos);
        case NFS_V4:
                return nfs4_pread_async_internal(nfs, nfsfh,
                                                 buf, count, offset,
                                                 cb, private_data, update_pos);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

struct rw_data {
        struct nfsfh *nfsfh;
        int update_pos;
        uint8_t *buf;
        size_t count;
        size_t remaining;
        uint64_t offset;
        nfs_cb cb;
        void *private_data;
};

static void r_cb(int status, struct nfs_context *nfs,
                   void *data, void *private_data)
{
        struct rw_data *rw_data = private_data;
        size_t cnt;

        if (status < 0) {
                nfs_set_error(nfs, "%s multi pread failed with %d",
                              __FUNCTION__, status);
                rw_data->cb(status, nfs, NULL, rw_data->private_data);
                free(rw_data);
                return;
        }

        if (status > rw_data->remaining) {
                status = rw_data->remaining;
        }
        rw_data->buf += status;
        rw_data->offset += status;
        rw_data->remaining -= status;
        /*
         * Read until we have all the data or the server retruned a short read (eof?)
         */
        if (rw_data->remaining == 0 || status < nfs_get_readmax(nfs)) {
                rw_data->cb(rw_data->count - rw_data->remaining, nfs, NULL, rw_data->private_data);
                free(rw_data);
                return;
        }
        cnt = rw_data->remaining;
        if (nfs_get_readmax(nfs) && cnt > nfs_get_readmax(nfs)) {
                cnt = nfs_get_readmax(nfs);
        }
        if (__nfs_pread_async(nfs, rw_data->nfsfh, rw_data->buf, cnt, rw_data->offset, r_cb, rw_data, rw_data->update_pos)) {
                nfs_set_error(nfs, "%s multi pread failed with ENOMEM",
                              __FUNCTION__);
                rw_data->cb(-ENOMEM, nfs, NULL, rw_data->private_data);
                free(rw_data);
                return;
        }
}

        
static int
_nfs_pread_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
               void *buf, size_t count,  uint64_t offset,
                 nfs_cb cb, void *private_data, int update_pos)
{
        struct rw_data *rw_data;
        size_t cnt;


        if (count < nfs_get_readmax(nfs)) {
                return __nfs_pread_async(nfs, nfsfh, buf, count, offset, cb, private_data, update_pos);
        }

        rw_data = malloc(sizeof(struct rw_data));
        if (rw_data == NULL) {
                return -ENOMEM;
        }
        rw_data->update_pos = 0;
        rw_data->nfsfh = nfsfh;
        rw_data->buf = buf;
        rw_data->count = count;
        rw_data->remaining = count;
        rw_data->offset = offset;
        rw_data->cb = cb;
        rw_data->private_data = private_data;

        cnt = count;
        if (nfs_get_readmax(nfs) && cnt > nfs_get_readmax(nfs)) {
                cnt = nfs_get_readmax(nfs);
        }
        return __nfs_pread_async(nfs, rw_data->nfsfh, rw_data->buf, cnt, rw_data->offset, r_cb, rw_data, rw_data->update_pos);
}
        
int
nfs_pread_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
               void *buf, size_t count,  uint64_t offset,
               nfs_cb cb, void *private_data)
{
        return _nfs_pread_async(nfs, nfsfh, buf, count, offset, cb, private_data, 0);
}

int
nfs_read_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
               void *buf, size_t count,
               nfs_cb cb, void *private_data)
{
        return _nfs_pread_async(nfs, nfsfh, buf, count, nfsfh->offset, cb, private_data, 1);
}

int
nfs_preadv_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                 const struct iovec *iov, int iovcnt, uint64_t offset,
                 nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_preadv_async_internal(nfs, nfsfh,
                                                  iov, iovcnt, offset,
                                                  cb, private_data, 0);
        case NFS_V4:
                return nfs4_preadv_async_internal(nfs, nfsfh,
                                                  iov, iovcnt, offset,
                                                  cb, private_data, 0);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_readv_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                const struct iovec *iov, int iovcnt,
                nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_preadv_async_internal(nfs, nfsfh,
                                                  iov, iovcnt, nfsfh->offset,
                                                  cb, private_data, 1);
        case NFS_V4:
                return nfs4_preadv_async_internal(nfs, nfsfh,
                                                  iov, iovcnt, nfsfh->offset,
                                                  cb, private_data, 1);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_pwrite_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                 const void *buf, size_t count, uint64_t offset,
                 nfs_cb cb, void *private_data)
{
        if (nfsfh->is_readonly) {
                nfs_set_error(nfs, "Trying to write to read-only file");
                return -1;
        }
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_pwrite_async_internal(nfs, nfsfh,
                                                  buf, count, offset,
                                                  cb, private_data, 0);
        case NFS_V4:
                return nfs4_pwrite_async_internal(nfs, nfsfh,
                                                  buf, count, offset,
                                                  cb, private_data, 0);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d.",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                const void *buf, size_t count,
                nfs_cb cb, void *private_data)
{
        if (nfsfh->is_readonly) {
                nfs_set_error(nfs, "Trying to write to read-only file");
                return -1;
        }
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_write_async(nfs, nfsfh,
                                        buf, count,
                                        cb, private_data);
        case NFS_V4:
                //qqq
                return nfs4_write_async(nfs, nfsfh, count, buf,
                                        cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_close_async(nfs, nfsfh, cb, private_data);
        case NFS_V4:
                return nfs4_close_async(nfs, nfsfh, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_fstat_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_fstat_async(nfs, nfsfh, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv4",
                              __FUNCTION__);
                return -1;
        }
}

int
nfs_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                   void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_fstat64_async(nfs, nfsfh, cb, private_data);
        case NFS_V4:
                return nfs4_fstat64_async(nfs, nfsfh, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_fsync_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_fsync_async(nfs, nfsfh, cb, private_data);
        case NFS_V4:
                return nfs4_fsync_async(nfs, nfsfh, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_ftruncate_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                    uint64_t length, nfs_cb cb, void *private_data)
{
        if (nfsfh->is_readonly) {
                nfs_set_error(nfs, "Trying to truncate to read-only file");
                return -1;
        }
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_ftruncate_async(nfs, nfsfh, length,
                                            cb, private_data);
        case NFS_V4:
                return nfs4_ftruncate_async(nfs, nfsfh, length,
                                            cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_truncate_async(struct nfs_context *nfs, const char *path, uint64_t length,
                   nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_truncate_async(nfs, path, length, cb, private_data);
        case NFS_V4:
                return nfs4_truncate_async(nfs, path, length, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_mkdir2_async(struct nfs_context *nfs, const char *path, int mode,
                 nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_mkdir2_async(nfs, path, mode, cb, private_data);
        case NFS_V4:
                return nfs4_mkdir2_async(nfs, path, mode, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_mkdir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                void *private_data)
{
	return nfs_mkdir2_async(nfs, path, 0755, cb, private_data);
}

int
nfs_rmdir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                 void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_rmdir_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_rmdir_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_creat_async(struct nfs_context *nfs, const char *path,
                int mode, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_creat_async(nfs, path, mode,
                                         cb, private_data);
        case NFS_V4:
                return nfs4_creat_async(nfs, path, mode,
                                         cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_unlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                  void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_unlink_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_unlink_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_mknod_async(struct nfs_context *nfs, const char *path, int mode, int dev,
                 nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_mknod_async(nfs, path, mode, dev, cb, private_data);
        case NFS_V4:
                return nfs4_mknod_async(nfs, path, mode, dev, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_opendir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_opendir_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_opendir_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv4",
                              __FUNCTION__);
                return -1;
        }
}

struct nfsdirent *
nfs_readdir(struct nfs_context *nfs _U_, struct nfsdir *nfsdir)
{
	struct nfsdirent *nfsdirent = nfsdir->current;

	if (nfsdir->current != NULL) {
		nfsdir->current = nfsdir->current->next;
	}
	return nfsdirent;
}

long
nfs_telldir(struct nfs_context *nfs _U_, struct nfsdir *nfsdir)
{
        long i;
        struct nfsdirent *tmp;

        for (i = 0, tmp = nfsdir->entries; tmp; i++, tmp = tmp->next) {
                if (tmp == nfsdir->current) {
                        return i;
                }
        }
        return -1;
}

void
nfs_seekdir(struct nfs_context *nfs _U_, struct nfsdir *nfsdir, long loc)
{
        if (loc < 0) {
                return;
        }
        for (nfsdir->current = nfsdir->entries;
             nfsdir->current && loc--;
             nfsdir->current = nfsdir->current->next) {
        }
}

void
nfs_rewinddir(struct nfs_context *nfs _U_, struct nfsdir *nfsdir)
{
	nfsdir->current = nfsdir->entries;
}

void
nfs_closedir(struct nfs_context *nfs, struct nfsdir *nfsdir)
{
	if (nfs && nfs->nfsi->dircache_enabled) {
		nfs_dircache_add(nfs, nfsdir);
	} else {
		nfs_free_nfsdir(nfsdir);
	}
}

void
nfs_getcwd(struct nfs_context *nfs, const char **cwd)
{
	if (cwd) {
		*cwd = nfs->nfsi->cwd;
	}
}

int
nfs_lseek_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int64_t offset,
                 int whence, nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_lseek_async(nfs, nfsfh, offset, whence,
                                        cb, private_data);
        case NFS_V4:
                return nfs4_lseek_async(nfs, nfsfh, offset, whence,
                                        cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_lockf_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                enum nfs4_lock_op op, uint64_t count,
                nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V4:
                return nfs4_lockf_async(nfs, nfsfh, op, count,
                                        cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_fcntl_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                enum nfs4_fcntl_op cmd, void *arg,
                nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V4:
                return nfs4_fcntl_async(nfs, nfsfh, cmd, arg,
                                        cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_statvfs_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_statvfs_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_statvfs_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_statvfs64_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                    void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_statvfs64_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_statvfs64_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_readlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                    void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_readlink_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_readlink_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_chmod_async(struct nfs_context *nfs, const char *path, int mode,
                nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_chmod_async_internal(nfs, path, 0, mode,
                                                 cb, private_data);
        case NFS_V4:
                return nfs4_chmod_async_internal(nfs, path, 0, mode,
                                                 cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_lchmod_async(struct nfs_context *nfs, const char *path, int mode,
                 nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_chmod_async_internal(nfs, path, 1, mode,
                                                 cb, private_data);
        case NFS_V4:
                return nfs4_chmod_async_internal(nfs, path, 1, mode,
                                                 cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_fchmod_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode,
                  nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_fchmod_async(nfs, nfsfh, mode, cb, private_data);
        case NFS_V4:
                return nfs4_fchmod_async(nfs, nfsfh, mode, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_chown_async(struct nfs_context *nfs, const char *path, int uid, int gid,
                nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_chown_async_internal(nfs, path, 0, uid, gid,
                                                 cb, private_data);
        case NFS_V4:
                return nfs4_chown_async_internal(nfs, path, 0, uid, gid,
                                                 cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_lchown_async(struct nfs_context *nfs, const char *path, int uid, int gid,
                 nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_chown_async_internal(nfs, path, 1, uid, gid,
                                                 cb, private_data);
        case NFS_V4:
                return nfs4_chown_async_internal(nfs, path, 1, uid, gid,
                                                 cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_fchown_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid,
                 int gid, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_fchown_async(nfs, nfsfh, uid, gid,
                                         cb, private_data);
        case NFS_V4:
                return nfs4_fchown_async(nfs, nfsfh, uid, gid,
                                         cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_utimes_async(struct nfs_context *nfs, const char *path,
                 struct timeval *times, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_utimes_async_internal(nfs, path, 0, times,
                                                  cb, private_data);
        case NFS_V4:
                return nfs4_utimes_async_internal(nfs, path, 0, times,
                                                  cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_lutimes_async(struct nfs_context *nfs, const char *path,
                  struct timeval *times, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_utimes_async_internal(nfs, path, 1, times,
                                                  cb, private_data);
        case NFS_V4:
                return nfs4_utimes_async_internal(nfs, path, 1, times,
                                                  cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_utime_async(struct nfs_context *nfs, const char *path,
                struct utimbuf *times, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_utime_async(nfs, path, times, cb, private_data);
        case NFS_V4:
                return nfs4_utime_async(nfs, path, times, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv4",
                              __FUNCTION__);
                return -1;
        }
}

int
nfs_access_async(struct nfs_context *nfs, const char *path, int mode,
                 nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_access_async(nfs, path, mode, cb, private_data);
        case NFS_V4:
                return nfs4_access_async(nfs, path, mode, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_access2_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_access2_async(nfs, path, cb, private_data);
        case NFS_V4:
                return nfs4_access2_async(nfs, path, cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv4",
                              __FUNCTION__);
                return -1;
        }
}

int
nfs_symlink_async(struct nfs_context *nfs, const char *target,
                   const char *newpath, nfs_cb cb, void *private_data)
{
	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_symlink_async(nfs, target, newpath,
                                          cb, private_data);
        case NFS_V4:
                return nfs4_symlink_async(nfs, target, newpath,
                                          cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_rename_async(struct nfs_context *nfs, const char *oldpath,
                  const char *newpath, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_rename_async(nfs, oldpath, newpath,
                                         cb, private_data);
        case NFS_V4:
                return nfs4_rename_async(nfs, oldpath, newpath,
                                         cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

int
nfs_link_async(struct nfs_context *nfs, const char *oldpath,
               const char *newpath, nfs_cb cb, void *private_data)
{
        if (nfs->nfsi->readonly) {
                nfs_set_error(nfs, "EROFS. Readonly mount");
                cb(-EROFS, nfs, NULL, private_data);
                return 0;
        }

	switch (nfs->nfsi->version) {
        case NFS_V3:
                return nfs3_link_async(nfs, oldpath, newpath,
                                       cb, private_data);
        case NFS_V4:
                return nfs4_link_async(nfs, oldpath, newpath,
                                       cb, private_data);
        default:
                nfs_set_error(nfs, "%s does not support NFSv%d",
                              __FUNCTION__, nfs->nfsi->version);
                return -1;
        }
}

/*
 * Get/Set the maximum supported READ size by the server
 */
size_t
nfs_get_readmax(struct nfs_context *nfs)
{
	return nfs->nfsi->readmax;
}
void
nfs_set_readmax(struct nfs_context *nfs, size_t readmax)
{
        size_t readmax_adjusted = readmax;

        readmax_adjusted = MIN(readmax_adjusted, NFS_MAX_XFER_SIZE);
        readmax_adjusted = MAX(readmax_adjusted, NFS_MIN_XFER_SIZE);
        readmax_adjusted = (readmax_adjusted / 4096) * 4096;

        nfs->nfsi->readmax = readmax_adjusted;
}

/*
 * Get/Set the maximum supported WRITE size by the server
 */
size_t
nfs_get_writemax(struct nfs_context *nfs)
{
	return nfs->nfsi->writemax;
}
void
nfs_set_writemax(struct nfs_context *nfs, size_t writemax)
{
        size_t writemax_adjusted = writemax;

        writemax_adjusted = MIN(writemax_adjusted, NFS_MAX_XFER_SIZE);
        writemax_adjusted = MAX(writemax_adjusted, NFS_MIN_XFER_SIZE);
        writemax_adjusted = (writemax_adjusted / 4096) * 4096;

        nfs->nfsi->writemax = writemax_adjusted;
}

void
nfs_set_tcp_syncnt(struct nfs_context *nfs, int v) {
	rpc_set_tcp_syncnt(nfs->rpc, v);
}

void
nfs_set_uid(struct nfs_context *nfs, int uid) {
	rpc_set_uid(nfs->rpc, uid);
}

void
nfs_set_gid(struct nfs_context *nfs, int gid) {
	rpc_set_gid(nfs->rpc, gid);
}

void
nfs_set_auxiliary_gids(struct nfs_context *nfs, uint32_t len, uint32_t* gids) {
	rpc_set_auxiliary_gids(nfs->rpc, len, gids);
}

void
nfs_set_debug(struct nfs_context *nfs, int level) {
	rpc_set_debug(nfs->rpc, level);
}

void
nfs_set_auto_traverse_mounts(struct nfs_context *nfs, int enabled) {
	nfs->nfsi->auto_traverse_mounts = enabled;
}

void
nfs_set_readonly(struct nfs_context *nfs, int readonly) {
	nfs->nfsi->readonly = readonly;
}

void
nfs_set_dircache(struct nfs_context *nfs, int enabled) {
	nfs->nfsi->dircache_enabled = enabled;
}

void
nfs_set_autoreconnect(struct nfs_context *nfs, int num_retries) {
	/*
	 * Save the user provided value in nfs_context_internal.
	 * This will later be set in rpc_context using rpc_set_resiliency()
	 * once the mount process completes.
	 */
	nfs->nfsi->auto_reconnect = num_retries;
}

void
nfs_set_retrans(struct nfs_context *nfs, int retrans) {
	/*
	 * Save the user provided value in nfs_context_internal.
	 * This will later be set in rpc_context using rpc_set_resiliency()
	 * once the mount process completes.
	 */
	assert(retrans >= 0);
	nfs->nfsi->retrans = retrans;
}

int
nfs_set_version(struct nfs_context *nfs, int version) {
	switch (version) {
	case NFS_V3:
	case NFS_V4:
#ifdef HAVE_TLS
		nfs->rpc->nfs_version = version;
#endif
		nfs->nfsi->version = version;
		nfs->nfsi->default_version = 0;
		break;
	default:
		nfs_set_error(nfs, "NFS version %d is not supported", version);
		return -1;
	}
	return 0;
}

int
nfs_get_version(struct nfs_context *nfs) {
        return nfs->nfsi->version;
}

void
nfs_set_nfsport(struct nfs_context *nfs, int port) {
	nfs->nfsi->nfsport = port;
}

void
nfs_set_mountport(struct nfs_context *nfs, int port) {
	nfs->nfsi->mountport = port;
	rpc_set_mountport(nfs->rpc, port);
}

size_t
nfs_get_readdir_maxcount(struct nfs_context *nfs)
{
        return nfs->nfsi->readdir_maxcount;
}

void
nfs_set_readdir_max_buffer_size(struct nfs_context *nfs,
                                uint32_t dircount,
                                uint32_t maxcount) {
        size_t dircount_adjusted = dircount;
        size_t maxcount_adjusted = maxcount;

        dircount_adjusted = MIN(dircount_adjusted, NFS_MAX_XFER_SIZE);
        dircount_adjusted = MAX(dircount_adjusted, NFS_MIN_XFER_SIZE);
        dircount_adjusted = (dircount_adjusted / 4096) * 4096;

        maxcount_adjusted = MIN(maxcount_adjusted, NFS_MAX_XFER_SIZE);
        maxcount_adjusted = MAX(maxcount_adjusted, NFS_MIN_XFER_SIZE);
        maxcount_adjusted = (maxcount_adjusted / 4096) * 4096;

        nfs->nfsi->readdir_dircount = dircount_adjusted;
        nfs->nfsi->readdir_maxcount = maxcount_adjusted;
}

void
nfs_set_error(struct nfs_context *nfs, char *error_string, ...)
{
        va_list ap;
	char *old_error_string;

#ifdef HAVE_MULTITHREADING
        /* All thread contexts share the same rpc_context so
         * use the mutex from the rpc_context.
         */
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
	old_error_string = nfs->error_string;
        va_start(ap, error_string);
	nfs->error_string = malloc(1024);
        if (nfs->error_string == NULL) {
                nfs->error_string = discard_const(oom);
                goto finished;
        }
	vsnprintf(nfs->error_string, 1024, error_string, ap);
        va_end(ap);

 finished:
	/*
	 * Free old_error_string after vsnprintf() above to support calls like
	 * nfs_set_error(nfs, "Failed to perform xxx: %s", nfs_get_error(nfs));
	 */
        if (old_error_string && old_error_string != oom) {
                free(old_error_string);
        }
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->rpc->rpc_mutex);
        }
#endif /* HAVE_MULTITHREADING */
}

void
nfs_set_error_locked(struct nfs_context *nfs, char *error_string, ...)
{
        va_list ap;
	char *old_error_string = nfs->error_string;

        va_start(ap, error_string);
        nfs->error_string = malloc(1024);
        if (nfs->error_string == NULL) {
                free(old_error_string);
                nfs->error_string = discard_const(oom);
                return;
        }
        vsnprintf(nfs->error_string, 1024, error_string, ap);
        va_end(ap);

        if (old_error_string && old_error_string != oom) {
                free(old_error_string);
        }
}

struct mount_cb_data {
       rpc_cb cb;
       void *private_data;
       char *server;
};

static void
free_mount_cb_data(struct mount_cb_data *data)
{
	if (data->server != NULL) {
		free(data->server);
		data->server = NULL;
	}

	free(data);
}

static void
mount_export_5_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	struct mount_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, -EFAULT, command_data, data->private_data);
		free_mount_cb_data(data);
		return;
	}

	data->cb(rpc, 0, command_data, data->private_data);
	if (rpc_disconnect(rpc, "normal disconnect") != 0) {
		rpc_set_error(rpc, "Failed to disconnect\n");
	}
	free_mount_cb_data(data);
}

static void
mount_export_4_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	struct mount_cb_data *data = private_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	/* Dont want any more callbacks even if the socket is closed */
	rpc->connect_cb = NULL;

	if (status != RPC_STATUS_SUCCESS) {
		data->cb(rpc, -EFAULT, command_data, data->private_data);
		free_mount_cb_data(data);
		return;
	}

	if (rpc_mount3_export_task(rpc, mount_export_5_cb, data) == NULL) {
		data->cb(rpc, -ENOMEM, command_data, data->private_data);
		free_mount_cb_data(data);
		return;
	}
}

int
mount_getexports_async(struct rpc_context *rpc, const char *server, rpc_cb cb,
                       void *private_data)
{
	struct mount_cb_data *data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	data = calloc(1, sizeof(struct mount_cb_data));
	if (data == NULL) {
		return -1;
	}
	data->cb           = cb;
	data->private_data = private_data;
	data->server       = strdup(server);
	if (data->server == NULL) {
		free_mount_cb_data(data);
		return -1;
	}
	if (rpc->mountport) {
		if (rpc_connect_port_async(rpc, data->server, rpc->mountport, MOUNT_PROGRAM,
		                                              MOUNT_V3, mount_export_4_cb, data) != 0) {
				rpc_set_error(rpc, "Failed to start connection. %s",
				              rpc_get_error(rpc));
				free_mount_cb_data(data);
			return -1;
		}
		return 0;
	}
	if (rpc_connect_program_async(rpc, data->server, MOUNT_PROGRAM,
                                      MOUNT_V3, mount_export_4_cb, data) != 0) {
		rpc_set_error(rpc, "Failed to start connection. %s",
                              rpc_get_error(rpc));
		free_mount_cb_data(data);
		return -1;
	}

	return 0;
}

struct rpc_context *
nfs_get_rpc_context(struct nfs_context *nfs)
{
	assert(nfs->rpc->magic == RPC_CONTEXT_MAGIC);
	return nfs->rpc;
}

/*
 * Get the NFS server address we are currently connected to.
 */
const struct sockaddr_storage *
nfs_get_server_address(struct nfs_context *nfs)
{
	return &nfs->rpc->s;
}

const char *
nfs_get_server(struct nfs_context *nfs) {
	return nfs->nfsi->server;
}

const char *
nfs_get_export(struct nfs_context *nfs) {
	return nfs->nfsi->export;
}

const struct nfs_fh *
nfs_get_rootfh(struct nfs_context *nfs) {
      return &nfs->nfsi->rootfh;
}

struct nfs_fh *
nfs_get_fh(struct nfsfh *nfsfh) {
       return &nfsfh->fh;
}

uint16_t
nfs_umask(struct nfs_context *nfs, uint16_t mask) {
	 uint16_t tmp = nfs->nfsi->mask;
	 nfs->nfsi->mask = mask;
	 return tmp;
}

/*
* Sets polling timeout for nfs apis
*/
void
nfs_set_poll_timeout(struct nfs_context *nfs, int poll_timeout)
{
	rpc_set_poll_timeout(nfs->rpc, poll_timeout);
}

/*
* Gets polling timeout for nfs apis
*/
int
nfs_get_poll_timeout(struct nfs_context *nfs)
{
	return rpc_get_poll_timeout(nfs->rpc);
}

/*
* Sets timeout for nfs apis
*/
void
nfs_set_timeout(struct nfs_context *nfs, int timeout_msecs)
{
	/*
	 * Save the timeout in nfs_context_internal and also set it in
	 * rpc_context. Contrast this with nfs_set_retrans() which only saves
	 * the user provided value in nfs_context_internal but does not set it
	 * in rpc_context. Note that it's ok (and needed) to set timeout in the
	 * rpc_context as timeout is used by all RPC requests while retrans adds
	 * resiliency to RPC transport and is used only after the mount completes.
	 */
	nfs->nfsi->timeout = timeout_msecs;

	rpc_set_timeout(nfs->rpc, timeout_msecs);
}

/*
* Gets timeout for nfs apis
*/
int
nfs_get_timeout(struct nfs_context *nfs)
{
	return rpc_get_timeout(nfs->rpc);
}

struct rpc_pdu *
rpc_null_task(struct rpc_context *rpc, int program, int version, rpc_cb cb,
              void *private_data)
{
	struct rpc_pdu *pdu;

	pdu = rpc_allocate_pdu(rpc, program, version, 0, cb, private_data,
                               (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu "
                              "for NULL call");
		return NULL;
	}

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu "
                              "for NULL call");
		return NULL;
	}

	return pdu;
}

#ifdef HAVE_TLS
/*
 * Call this in place of rpc_null_task() if user wants TLS security for the RPC
 * connection. Since we only ever send AUTH_TLS NULL RPC for NFS_PROGRAM it does
 * not need the program parameter.
 * Callback 'cb' is called not after we get the reply for this AUTH_TLS NULL RPC,
 * but after the TLS handshake completes (success or failure).
 */
struct rpc_pdu *
rpc_null_task_authtls(struct rpc_context *rpc, int nfs_version, rpc_cb cb,
		      void *private_data)
{
	struct rpc_pdu *pdu;
	struct tls_cb_data *data;

	/* Must be called only for secure connections to NFS_PROGRAM version 3 or 4 */
	assert(rpc->use_tls);
	assert(nfs_version == NFS_V3 || nfs_version == NFS_V4);

	data = calloc(1, sizeof(*data));
	if (data == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate tls_cb_data "
			      "for AUTH_TLS NULL call");
		return NULL;
	}
	data->cb 	   = cb;
	data->private_data = private_data;

	/*
	 * Set MSbit in procedure number to convey use of AUTH_TLS
	 * This should not interfere with valid procedure numbers as they are all
	 * small numbers.
	 */
	pdu = rpc_allocate_pdu(rpc, NFS_PROGRAM, nfs_version, (0 | 0x80000000U),
			       rpc_connect_program_4_1_cb, data,
			       (zdrproc_t)zdr_void, 0);
	if (pdu == NULL) {
		rpc_set_error(rpc, "Out of memory. Failed to allocate pdu "
			      "for AUTH_TLS NULL call");
		free_tls_cb_data(data);
		return NULL;
	}

	rpc->tls_context.state = TLS_HANDSHAKE_WAITING_FOR_STARTTLS;

	if (rpc_queue_pdu(rpc, pdu) != 0) {
		rpc_set_error(rpc, "Out of memory. Failed to queue pdu "
				"for AUTH_TLS NULL call");
		free_tls_cb_data(data);
		return NULL;
	}

	return pdu;
}
#endif /* HAVE_TLS */

void rpc_set_stats_cb(struct rpc_context *rpc, rpc_stats_cb cb,
                      void *private_data)
{
        rpc->stats_cb = cb;
        rpc->stats_private_data = private_data;
}

void rpc_set_log_cb(struct rpc_context *rpc, rpc_log_cb cb,
                           void *private_data)
{
        rpc->log_cb = cb;
        rpc->log_private_data = private_data;
}

struct rpc_pdu_stats *rpc_get_pdu_stats(struct rpc_context *rpc)
{
        return &rpc->pdu->pdu_stats;
}
