/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2017 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
 * High level api to nfsv3 filesystems
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#define PRIu64 "llu"
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
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

#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libnfs-zdr.h"
#include "slist.h"
#include "libnfs.h"
#include "libnfs-raw.h"
#include "libnfs-raw-mount.h"
#include "libnfs-private.h"

static uint64_t
specdata3_to_rdev(struct specdata3 *specdata)
{
        uint64_t rdev = specdata->specdata1;
        return (rdev << 32) | specdata->specdata2;
}

struct mount_attr_cb {
	int wait_count;
	struct nfs_cb_data *data;
};

struct mount_attr_item_cb {
	struct mount_attr_cb *ma;
	struct nested_mounts *mnt;
};

struct nfs_mcb_data {
       struct nfs_cb_data *data;
       uint64_t offset;
       size_t count;
};

static int
check_nfs3_error(struct nfs_context *nfs, int status,
                 struct nfs_cb_data *data, void *command_data)
{
	if (status == RPC_STATUS_ERROR) {
		data->cb(-EFAULT, nfs, command_data, data->private_data);
		return 1;
	}
	if (status == RPC_STATUS_CANCEL) {
		data->cb(-EINTR, nfs, "Command was cancelled",
			 data->private_data);
		return 1;
	}
	if (status == RPC_STATUS_TIMEOUT) {
		data->cb(-EINTR, nfs, "Command timed out",
			 data->private_data);
		return 1;
	}

	return 0;
}

static int nfs3_lookup_path_async_internal(struct nfs_context *nfs,
                                           struct nfs_attr *attr,
                                           struct nfs_cb_data *data,
                                           struct nfs_fh *fh);

/*
 * Functions to first look up a path, component by component, and then finally
 * call a specific function once the filehandle for the final component is
 * found.
 */
static void
nfs3_lookup_path_2_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	READLINK3res *res;
	char *path, *newpath;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: READLINK of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	path = res->READLINK3res_u.resok.data;

	/* Handle absolute paths, ensuring that the path lies within the
	 * export. */
	if (path[0] == '/') {
		if (strstr(path, nfs_get_export(nfs)) == path) {
			char *ptr = path + strlen(nfs_get_export(nfs));
			if (*ptr == '/') {
				newpath = strdup(ptr);
			} else if (*ptr == '\0') {
				newpath = strdup("/");
			} else {
				data->cb(-ENOENT, nfs, "Symbolic link points "
                                         "outside export", data->private_data);
				free_nfs_cb_data(data);
				return;
			}
		} else {
			data->cb(-ENOENT, nfs, "Symbolic link points outside "
                                 "export", data->private_data);
			free_nfs_cb_data(data);
			return;
		}

		if (!newpath)
			goto nomem;
	} else {
		/* Handle relative paths, both the case where the current
		 * component is an intermediate component and when it is the
		 * final component. */
		if (data->path[0]) {
			/* Since path points to a component and saved_path
			 * always starts with '/', path[-1] is valid. */
			data->path[-1] = '\0';
			newpath = malloc(strlen(data->saved_path) +
                                         strlen(path) + strlen(data->path) + 6);
			if (!newpath)
				goto nomem;

			sprintf(newpath, "%s/../%s/%s", data->saved_path, path,
                                data->path);
		} else {
			newpath = malloc(strlen(data->saved_path) +
                                         strlen(path) + 5);
			if (!newpath)
				goto nomem;

			sprintf(newpath, "%s/../%s", data->saved_path, path);
		}
	}
	free(data->saved_path);
	data->saved_path = newpath;

	if (nfs_normalize_path(nfs, data->saved_path) != 0) {
		data->cb(-ENOENT, nfs, "Symbolic link resolves to invalid "
                         "path", data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->path = data->saved_path;
	nfs3_lookup_path_async_internal(nfs, NULL, data, &nfs->nfsi->rootfh);
	return;

nomem:
	data->cb(-ENOMEM, nfs, "Failed to allocate memory for path",
                 data->private_data);
	free_nfs_cb_data(data);
}

static void
fattr3_to_nfs_attr(struct nfs_attr *attr, fattr3 *fa3)
{
        attr->type  = fa3->type;
        attr->mode  = fa3->mode;
        attr->uid   = fa3->uid;
        attr->gid   = fa3->gid;
        attr->nlink = fa3->nlink;
        attr->size  = fa3->size;
        attr->used  = fa3->used;
        attr->fsid  = fa3->fsid;
        attr->rdev.specdata1 = fa3->rdev.specdata1;
        attr->rdev.specdata2 = fa3->rdev.specdata2;
        attr->atime.seconds  = fa3->atime.seconds;
        attr->atime.nseconds = fa3->atime.nseconds;
        attr->mtime.seconds  = fa3->mtime.seconds;
        attr->mtime.nseconds = fa3->mtime.nseconds;
        attr->ctime.seconds  = fa3->ctime.seconds;
        attr->ctime.nseconds = fa3->ctime.nseconds;
}

static void
nfs3_lookup_path_1_cb(struct rpc_context *rpc, int status, void *command_data,
                      void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	LOOKUP3res *res;
	struct nfs_attr attr;
        struct nfs_fh fh;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: Lookup of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

        memset(&attr, 0, sizeof(attr));
	if (res->LOOKUP3res_u.resok.obj_attributes.attributes_follow) {
                fattr3_to_nfs_attr(&attr, &res->LOOKUP3res_u.resok.obj_attributes.post_op_attr_u.attributes);
        }
                
	/* This function will always invoke the callback and cleanup
	 * for failures. So no need to check the return value.
	 */
        fh.val = res->LOOKUP3res_u.resok.object.data.data_val;
        fh.len = res->LOOKUP3res_u.resok.object.data.data_len;
	nfs3_lookup_path_async_internal(nfs, &attr, data, &fh);
}

static int
nfs3_lookup_path_async_internal(struct nfs_context *nfs, struct nfs_attr *attr,
                                struct nfs_cb_data *data, struct nfs_fh *fh)
{
	char *path, *slash;
	LOOKUP3args args;

	while (*data->path == '/') {
	      data->path++;
	}

	path = data->path;
	slash = strchr(path, '/');

	if (attr && attr->type == NF3LNK) {
		if (data->continue_int & O_NOFOLLOW) {
			data->cb(-ELOOP, nfs, "Symbolic link encountered",
                                 data->private_data);
			free_nfs_cb_data(data);
			return -1;
		}
		if (!data->no_follow || *path != '\0') {
			READLINK3args rl_args;

			if (data->link_count++ >= MAX_LINK_COUNT) {
				data->cb(-ELOOP, nfs, "Too many levels of "
                                         "symbolic links", data->private_data);
				free_nfs_cb_data(data);
				return -1;
                        }

			rl_args.symlink.data.data_len = fh->len;
			rl_args.symlink.data.data_val = fh->val;

			if (rpc_nfs3_readlink_task(nfs->rpc,
                                                   nfs3_lookup_path_2_cb,
                                                   &rl_args, data) == NULL) {
				nfs_set_error(nfs, "RPC error: Failed to "
                                              "send READLINK call for %s",
                                              data->path);
				data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                         data->private_data);
				free_nfs_cb_data(data);
				return -1;
			}

			if (slash != NULL) {
				*slash = '/';
			}
			return 0;
		}
	}

	if (slash != NULL) {
		/* Clear slash so that path is a zero terminated string for
		 * the current path component. Set it back to '/' again later
		 * when we are finished referencing this component so that
		 * data->saved_path will still point to the full
		 * normalized path.
		 */
		*slash = 0;
		data->path = slash+1;
	} else {
		while (*data->path != 0) {
		      data->path++;
		}
	}
	if (*path == 0) {
		data->fh.len = fh->len;
		data->fh.val = malloc(data->fh.len);
		if (data->fh.val == NULL) {
			nfs_set_error(nfs, "Out of memory: Failed to "
                                      "allocate fh for %s", data->path);
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			free_nfs_cb_data(data);
			return -1;
		}
		memcpy(data->fh.val, fh->val, data->fh.len);
		if (slash != NULL) {
			*slash = '/';
		}
		data->continue_cb(nfs, attr, data);
		return 0;
	}

	memset(&args, 0, sizeof(LOOKUP3args));
	args.what.dir.data.data_len = fh->len;
	args.what.dir.data.data_val = fh->val;
	args.what.name = path;

	if (rpc_nfs3_lookup_task(nfs->rpc, nfs3_lookup_path_1_cb,
                                 &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send lookup "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	if (slash != NULL) {
		*slash = '/';
	}
	return 0;
}

static void
nfs3_lookup_path_getattr_cb(struct rpc_context *rpc, int status,
                            void *command_data, void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	GETATTR3res *res;
	struct nfs_attr attr;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETATTR of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	fattr3_to_nfs_attr(&attr, &res->GETATTR3res_u.resok.obj_attributes);
	/* This function will always invoke the callback and cleanup
	 * for failures. So no need to check the return value.
	 */
	nfs3_lookup_path_async_internal(nfs, &attr, data, &nfs->nfsi->rootfh);
}

/* This function will free continue_data on error */
static int
nfs3_lookuppath_async(struct nfs_context *nfs, const char *path, int no_follow,
                      nfs_cb cb, void *private_data,
                      continue_func continue_cb, void *continue_data,
                      void (*free_continue_data)(void *),
                      uint64_t continue_int)
{
	struct nfs_cb_data *data;
	struct GETATTR3args args;
	struct nfs_fh *fh;

	if (path == NULL || path[0] == '\0') {
		path = ".";
	}

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "Out of memory: failed to allocate "
			"nfs_cb_data structure");
                if (continue_data) {
                        free_continue_data(continue_data);
                }
		return -1;
	}
	data->nfs                = nfs;
	data->cb                 = cb;
	data->continue_cb        = continue_cb;
	data->continue_data      = continue_data;
	data->free_continue_data = free_continue_data;
	data->continue_int       = continue_int;
	data->private_data       = private_data;
	data->no_follow          = no_follow;
	if (path[0] == '/') {
		data->saved_path = strdup(path);
	} else {
		data->saved_path = malloc(strlen(path) + strlen(nfs->nfsi->cwd) + 2);
		if (data->saved_path == NULL) {
			nfs_set_error(nfs, "Out of memory: failed to "
				"allocate path string");
			free_nfs_cb_data(data);
			return -1;
		}
		sprintf(data->saved_path, "%s/%s", nfs->nfsi->cwd, path);
	}

	if (data->saved_path == NULL) {
		nfs_set_error(nfs, "Out of memory: failed to copy path "
                              "string");
		free_nfs_cb_data(data);
		return -1;
	}
	if (nfs_normalize_path(nfs, data->saved_path) != 0) {
		free_nfs_cb_data(data);
		return -1;
	}

	data->path = data->saved_path;
	fh = &nfs->nfsi->rootfh;

	if (data->path[0]) {
		struct nested_mounts *mnt;
		/* Make sure we match on longest nested export.
		 * TODO: If we make sure the list is sorted we can skip this
		 * check and end the loop on first match.
		 */
		size_t max_match_len = 0;

		/* Do we need to switch to a different nested export ? */
		for (mnt = nfs->nfsi->nested_mounts; mnt; mnt = mnt->next) {
			if (strlen(mnt->path) < max_match_len)
				continue;
			if (strncmp(mnt->path, data->saved_path,
				     strlen(mnt->path)))
				continue;
			if (data->saved_path[strlen(mnt->path)] != '\0'
			    && data->saved_path[strlen(mnt->path)] != '/')
				continue;

			data->saved_path = strdup(data->path
						  + strlen(mnt->path));
			free(data->path);
			data->path = data->saved_path;
			fh = &mnt->fh;
			max_match_len = strlen(mnt->path);
		}

		/* This function will always invoke the callback and cleanup
		 * for failures. So no need to check the return value.
		 */
		nfs3_lookup_path_async_internal(nfs, NULL, data, fh);
		return 0;
	}

	/* We have a request for "", so just perform a GETATTR3 so we can
	 * return the attributes to the caller.
	 */
	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = fh->len;
	args.object.data.data_val = fh->val;
	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_lookup_path_getattr_cb,
                                  &args, data) == NULL) {
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static void
nfs3_mount_8_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct mount_attr_item_cb *ma_item = private_data;
	struct mount_attr_cb *ma = ma_item->ma;
	struct nfs_cb_data *data = ma->data;
	struct nfs_context *nfs = data->nfs;
	GETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status != RPC_STATUS_SUCCESS) {
		goto finished;
	}

	res = command_data;
	if (res->status != NFS3_OK)
		goto finished;

	fattr3_to_nfs_attr(&ma_item->mnt->attr,
                           &res->GETATTR3res_u.resok.obj_attributes);

finished:
	free(ma_item);
	ma->wait_count--;
	if (ma->wait_count > 0)
		return;

	free(ma);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static void
nfs3_mount_7_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct mount_attr_cb *ma = NULL;
	struct nested_mounts *mnt;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	/*
	 * Now the entire mount process (including the NFS FSINFO and GETATTR)
	 * has completed. Any RPC failure till now would have caused the mount
	 * process to fail. Note that it's desirable for the mount process to
	 * fail upfront if it encounters any errors (TCP or RPC), rather than
	 * keep trying indefinitely causing the mount process to "hang", but
	 * from now on the RPC transport will be used to carry NFS RPCs issued
	 * by the application which may not be equipped to handle TCP connection
	 * failure and RPC timeouts, so we set the resiliency parameters of the
	 * rpc_context as selected by the user using the mount options. The
	 * default resiliency parameters emulate the common "hard" mount and
	 * we are resilient to any TCP or RPC connectivity issues.
         */
	rpc_set_resiliency(rpc,
			   nfs->nfsi->auto_reconnect,
			   nfs->nfsi->timeout,
			   nfs->nfsi->retrans);

	if (!nfs->nfsi->nested_mounts)
		goto finished;

	/* nested mount traversals are best-effort only, so any
	 * failures just means that we don't get traversal for that
	 * particular mount. We do not fail the call from the application.
	 */
	ma = malloc(sizeof(struct mount_attr_cb));
	if (ma == NULL)
		goto finished;
	memset(ma, 0, sizeof(struct mount_attr_cb));
	ma->data = data;

	for(mnt = nfs->nfsi->nested_mounts; mnt; mnt = mnt->next) {
		struct mount_attr_item_cb *ma_item;
		struct GETATTR3args args;

		ma_item = malloc(sizeof(struct mount_attr_item_cb));
		if (ma_item == NULL)
			goto finished;
		ma_item->mnt = mnt;
		ma_item->ma = ma;

		memset(&args, 0, sizeof(GETATTR3args));
		args.object.data.data_len = mnt->fh.len;
		args.object.data.data_val = mnt->fh.val;

		if (rpc_nfs3_getattr_task(rpc, nfs3_mount_8_cb, &args,
                                          ma_item) == NULL) {
                        nfs_set_error(nfs, "%s: %s", __FUNCTION__,
                                      nfs_get_error(nfs));
			free(ma_item);
			continue;
		}

		ma->wait_count++;
	}

finished:
	if (ma && ma->wait_count)
		return;

	free(ma);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static void
nfs3_mount_6_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	FSINFO3res *res = command_data;
	struct GETATTR3args args;
	size_t readmax, writemax, dircount, maxcount;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: FSINFO of %s failed with %s(%d)",
                              nfs_get_export(nfs), nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
        }

	readmax = MIN(nfs->nfsi->readmax, res->FSINFO3res_u.resok.rtmax);
	writemax = MIN(nfs->nfsi->writemax, res->FSINFO3res_u.resok.wtmax);

	/* The server supports sizes up to rtmax and wtmax, so it is legal
	 * to use smaller transfers sizes.
	 */
	if (nfs->nfsi->readmax < NFSMAXDATA2) {
		nfs_set_error(nfs, "server max rsize of %d",
                              (int)nfs->nfsi->readmax);
		data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	} else {
		nfs_set_readmax(nfs, readmax);
	}

	if (nfs->nfsi->writemax < NFSMAXDATA2) {
		nfs_set_error(nfs, "server max wsize of %d",
                              (int)nfs->nfsi->writemax);
		data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	} else {
		nfs_set_writemax(nfs, writemax);
	}

	dircount = MIN(nfs->nfsi->readdir_dircount, res->FSINFO3res_u.resok.dtpref);
	maxcount = MIN(nfs->nfsi->readdir_maxcount, res->FSINFO3res_u.resok.dtpref);

	if (nfs->nfsi->readdir_dircount < NFSMAXDATA2) {
		nfs_set_error(nfs, "server dtpref of %d",
                              (int)nfs->nfsi->readdir_dircount);
		data->cb(-EINVAL, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	} else {
		nfs_set_readdir_max_buffer_size(nfs, dircount, maxcount);
	}

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = nfs->nfsi->rootfh.len;
	args.object.data.data_val = nfs->nfsi->rootfh.val;

	if (rpc_nfs3_getattr_task(rpc, nfs3_mount_7_cb, &args, data) == NULL) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}

static void
nfs3_mount_5_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct FSINFO3args args;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	args.fsroot.data.data_len = nfs->nfsi->rootfh.len;
	args.fsroot.data.data_val = nfs->nfsi->rootfh.val;
	if (rpc_nfs3_fsinfo_task(rpc, nfs3_mount_6_cb, &args, data) == NULL) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}

struct mount_discovery_cb {
	int wait_count;
	int error;
	int status;
	struct nfs_cb_data *data;
};

struct mount_discovery_item_cb {
	struct mount_discovery_cb *md_cb;
	char *path;
};

static void
nfs3_mount_4_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct mount_discovery_item_cb *md_item_cb = private_data;
	struct mount_discovery_cb *md_cb = md_item_cb->md_cb;
	struct nfs_cb_data *data = md_cb->data;
	struct nfs_context *nfs = data->nfs;
	mountres3 *res;
	struct nested_mounts *mnt;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (status == RPC_STATUS_ERROR) {
		nfs_set_error(nfs, "MOUNT failed with RPC_STATUS_ERROR");
		md_cb->error = -EFAULT;
		goto finished;
	}
	if (status == RPC_STATUS_CANCEL) {
		nfs_set_error(nfs, "MOUNT failed with RPC_STATUS_CANCEL");
		md_cb->status = RPC_STATUS_CANCEL;
		goto finished;
	}
	if (status == RPC_STATUS_TIMEOUT) {
		nfs_set_error(nfs, "MOUNT timed out");
		md_cb->status = RPC_STATUS_TIMEOUT;
		goto finished;
	}

	res = command_data;
	if (res->fhs_status != MNT3_OK) {
		nfs_set_error(nfs, "RPC error: Mount failed with error "
                              "%s(%d) %s(%d)",
                              mountstat3_to_str(res->fhs_status),
                              res->fhs_status,
                              strerror(-mountstat3_to_errno(res->fhs_status)),
                              -mountstat3_to_errno(res->fhs_status));
		md_cb->error = mountstat3_to_errno(res->fhs_status);
		goto finished;
	}

	mnt = malloc(sizeof(*mnt));
	if (mnt == NULL) {
		nfs_set_error(nfs, "Out of memory. Could not allocate memory "
                              "to store mount handle");
		md_cb->error = -ENOMEM;
		goto finished;
	}
	memset(mnt, 0, sizeof(*mnt));

	mnt->fh.len = res->mountres3_u.mountinfo.fhandle.fhandle3_len;
	mnt->fh.val = malloc(mnt->fh.len);
	if (mnt->fh.val == NULL) {
		free(mnt);
		goto finished;
	}
	memcpy(mnt->fh.val,
	       res->mountres3_u.mountinfo.fhandle.fhandle3_val,
	       mnt->fh.len);

	mnt->path = md_item_cb->path;
	md_item_cb->path = NULL;

	LIBNFS_LIST_ADD(&nfs->nfsi->nested_mounts, mnt);

finished:
	free(md_item_cb->path);
	free(md_item_cb);
	md_cb->wait_count--;
	if (md_cb->wait_count > 0)
		return;

	rpc_disconnect(rpc, "normal disconnect");

	if (md_cb->status == RPC_STATUS_CANCEL) {
		data->cb(-EINTR, nfs, "Command was cancelled",
                         data->private_data);
		free(md_cb);
		free_nfs_cb_data(data);
		return;
	}
	if (md_cb->error) {
		data->cb(md_cb->error, nfs, command_data, data->private_data);
		free(md_cb);
		free_nfs_cb_data(data);
		return;
	}

        if (nfs->nfsi->nfsport) {
                if (rpc_connect_port_async(nfs->rpc, nfs_get_server(nfs),
                                           nfs->nfsi->nfsport,
                                           NFS_PROGRAM, NFS_V3,
                                           nfs3_mount_5_cb, data) != 0) {
                        nfs_set_error(nfs, "%s: %s", __FUNCTION__,
                                      nfs_get_error(nfs));
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free(md_cb);
                        free_nfs_cb_data(data);
                        return;
                }
                return;
        }

	if (rpc_connect_program_async(nfs->rpc, nfs_get_server(nfs),
                                      NFS_PROGRAM,
                                      NFS_V3, nfs3_mount_5_cb, data) != 0) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free(md_cb);
		free_nfs_cb_data(data);
		return;
	}
	free(md_cb);
}

static void
nfs3_mount_3_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	exports res;
	int len;
	struct mount_discovery_cb *md_cb = NULL;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	/* Iterate over all exports and check if there are any mounts nested
	 * below the current mount.
	 */
	len = strlen(nfs_get_export(nfs));
	if (!len) {
		data->cb(-EFAULT, nfs, "Export is empty", data->private_data);
		free_nfs_cb_data(data);
		return;
	}
	res = *(exports *)command_data;
	while (res) {
		struct mount_discovery_item_cb *md_item_cb;

		if (strncmp(nfs_get_export(nfs), res->ex_dir, len)) {
			res = res->ex_next;
			continue;
		}
		if (res->ex_dir[len - 1] != '/' && res->ex_dir[len] != '/') {
			res = res->ex_next;
			continue;
		}

		/* There is no need to fail the whole mount if anything
		 * below fails. Just clean up and continue. At worst it
		 * just mean that we might not be able to access any nested
		 * mounts.
		 */
		md_item_cb = malloc(sizeof(*md_item_cb));
		if (md_item_cb == NULL)
			continue;

		memset(md_item_cb, 0, sizeof(*md_item_cb));

		md_item_cb->path = strdup(res->ex_dir + len
					  - (nfs_get_export(nfs)[len -1] == '/'));
		if (md_item_cb->path == NULL) {
			free(md_item_cb);
			continue;
		}

		if (md_cb == NULL) {
			md_cb = malloc(sizeof(*md_cb));
			if (md_cb == NULL) {
				free(md_item_cb->path);
				free(md_item_cb);
				continue;
			}
			memset(md_cb, 0, sizeof(*md_cb));
			md_cb->data = data;
			md_cb->status = RPC_STATUS_SUCCESS;
			md_cb->error = 0;
		}
		md_item_cb->md_cb = md_cb;

		if (rpc_mount3_mnt_task(rpc, nfs3_mount_4_cb,
                                        res->ex_dir, md_item_cb) == NULL) {
                        nfs_set_error(nfs, "%s: %s",
                                      __FUNCTION__, nfs_get_error(nfs));
			if (md_cb->wait_count == 0) {
				free(md_cb);
				md_cb = NULL;
			}
			free(md_item_cb->path);
			free(md_item_cb);
			continue;
		}
		md_cb->wait_count++;

		res = res->ex_next;
	}

	if (md_cb)
		return;

	/* We did not have any nested mounts to check so we can proceed straight
	 * to reconnecting to NFSd.
	 */
	rpc_disconnect(rpc, "normal disconnect");

        if (nfs->nfsi->nfsport) {
                if (rpc_connect_port_async(nfs->rpc, nfs_get_server(nfs),
                                           nfs->nfsi->nfsport,
                                           NFS_PROGRAM, NFS_V3,
                                           nfs3_mount_5_cb, data) != 0) {
                        nfs_set_error(nfs, "%s: %s", __FUNCTION__,
                                      nfs_get_error(nfs));
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs_cb_data(data);
                        return;
                }
                return;
        }

	if (rpc_connect_program_async(nfs->rpc, nfs_get_server(nfs),
                                      NFS_PROGRAM,
                                      NFS_V3, nfs3_mount_5_cb, data) != 0) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}

static void
nfs3_mount_2_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	mountres3 *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->fhs_status != MNT3_OK) {
		nfs_set_error(nfs, "RPC error: Mount failed with error "
                              "%s(%d) %s(%d)",
                              mountstat3_to_str(res->fhs_status),
                              res->fhs_status,
                              strerror(-mountstat3_to_errno(res->fhs_status)),
                              -mountstat3_to_errno(res->fhs_status));
		data->cb(mountstat3_to_errno(res->fhs_status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs->nfsi->rootfh.len = res->mountres3_u.mountinfo.fhandle.fhandle3_len;
	nfs->nfsi->rootfh.val = malloc(nfs->nfsi->rootfh.len);
	if (nfs->nfsi->rootfh.val == NULL) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
	memcpy(nfs->nfsi->rootfh.val,
               res->mountres3_u.mountinfo.fhandle.fhandle3_val,
               nfs->nfsi->rootfh.len);

	if (nfs->nfsi->auto_traverse_mounts) {
		if (rpc_mount3_export_task(rpc, nfs3_mount_3_cb, data) == NULL) {
                        nfs_set_error(nfs, "%s: %s", __FUNCTION__,
                                      nfs_get_error(nfs));
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			free_nfs_cb_data(data);
			return;
		}
		return;
	}

	rpc_disconnect(rpc, "normal disconnect");

        if (nfs->nfsi->nfsport) {
                if (rpc_connect_port_async(nfs->rpc, nfs_get_server(nfs),
                                           nfs->nfsi->nfsport,
                                           NFS_PROGRAM, NFS_V3,
                                           nfs3_mount_5_cb, data) != 0) {
                        nfs_set_error(nfs, "%s: %s", __FUNCTION__,
                                      nfs_get_error(nfs));
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
                        free_nfs_cb_data(data);
                        return;
                }
                return;
        }

	if (rpc_connect_program_async(nfs->rpc, nfs_get_server(nfs),
                                      NFS_PROGRAM,
                                      NFS_V3, nfs3_mount_5_cb, data) != 0) {
                nfs_set_error(nfs, "%s: %s", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}


static void
nfs3_mount_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	if (rpc_mount3_mnt_task(rpc, nfs3_mount_2_cb, nfs->nfsi->export,
                                data) == NULL) {
                nfs_set_error(nfs, "%s: %s.", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}

int
nfs3_mount_async(struct nfs_context *nfs, const char *server,
                 const char *export, nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;
	char *new_server, *new_export;

	new_server = strdup(server);
	if (new_server == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs server string");
		return -1;
	}
        free(nfs->nfsi->server);
	nfs->nfsi->server = new_server;
        
	free(nfs->rpc->server);
	nfs->rpc->server = strdup(nfs->nfsi->server);

	new_export = strdup(export);
	if (new_export == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs export string");
		return -1;
	}
        free(nfs->nfsi->export);
	nfs->nfsi->export  = new_export;

        data = calloc(1, sizeof(*data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs mount data");
		return -1;
	}
        
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

        if (nfs->nfsi->mountport) {
                if (rpc_connect_port_async(nfs->rpc, server, nfs->nfsi->mountport,
                                           MOUNT_PROGRAM, MOUNT_V3,
                                           nfs3_mount_1_cb, data) != 0) {
                        nfs_set_error(nfs, "Failed to start connection. %s",
                                      nfs_get_error(nfs));
                        free_nfs_cb_data(data);
                        return -1;
                }
                return 0;
        }

	if (rpc_connect_program_async(nfs->rpc, server,
				      MOUNT_PROGRAM, MOUNT_V3,
				      nfs3_mount_1_cb, data) != 0) {
		nfs_set_error(nfs, "Failed to start connection. %s",
                              nfs_get_error(nfs));
		free_nfs_cb_data(data);
		return -1;
	}

	return 0;
}

static void
nfs3_umount_2_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	rpc_disconnect(rpc, "umount");
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static void
nfs3_umount_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	if (rpc_mount3_umnt_task(rpc, nfs3_umount_2_cb, nfs->nfsi->export,
                                 data) == NULL) {
                nfs_set_error(nfs, "%s: %s.", __FUNCTION__, nfs_get_error(nfs));
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
}

int
nfs3_umount_async(struct nfs_context *nfs, nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;

	data = malloc(sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
			      "memory for nfs mount data");
		return -1;
	}
	memset(data, 0, sizeof(struct nfs_cb_data));

	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	rpc_disconnect(nfs->rpc, "umount");

        if (nfs->nfsi->mountport) {
                if (rpc_connect_port_async(nfs->rpc, nfs_get_server(nfs),
                                           nfs->nfsi->mountport,
                                           MOUNT_PROGRAM, MOUNT_V3,
                                           nfs3_umount_1_cb, data) != 0) {
                        nfs_set_error(nfs, "Failed to start connection. %s",
                                      nfs_get_error(nfs));
                        free_nfs_cb_data(data);
                        return -1;
                }
                return 0;
        }

	if (rpc_connect_program_async(nfs->rpc, nfs_get_server(nfs),
				      MOUNT_PROGRAM, MOUNT_V3,
				      nfs3_umount_1_cb, data) != 0) {
		nfs_set_error(nfs, "Failed to start connection. %s",
                              nfs_get_error(nfs));
		free_nfs_cb_data(data);
		return -1;
	}

	return 0;
}


struct nfs_link_data {
       char *oldpath;
       struct nfs_fh oldfh;
       char *newparent;
       char *newobject;
       struct nfs_fh newdir;
};

static void
free_nfs_link_data(void *mem)
{
	struct nfs_link_data *data = mem;

        free(data->oldpath);
        free(data->oldfh.val);
        free(data->newparent);
        free(data->newobject);
        free(data->newdir.val);
	free(data);
}

static void
nfs3_link_cb(struct rpc_context *rpc, int status, void *command_data,
             void *private_data)
{
	LINK3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfs_link_data *link_data = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: LINK %s -> %s/%s failed with "
                              "%s(%d)", link_data->oldpath,
                              link_data->newparent,
                              link_data->newobject,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &link_data->newdir);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_link_continue_2_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	struct nfs_link_data *link_data = data->continue_data;
	LINK3args args;

	/* steal the filehandle */
	link_data->newdir = data->fh;
	data->fh.val = NULL;

	memset(&args, 0, sizeof(LINK3args));
	args.file.data.data_len = link_data->oldfh.len;
	args.file.data.data_val = link_data->oldfh.val;
	args.link.dir.data.data_len = link_data->newdir.len;
        args.link.dir.data.data_val = link_data->newdir.val;
	args.link.name = link_data->newobject;
	if (rpc_nfs3_link_task(nfs->rpc, nfs3_link_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send LINK "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static int
nfs3_link_continue_1_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	struct nfs_link_data *link_data = data->continue_data;

	/* steal the filehandle */
	link_data->oldfh = data->fh;
	data->fh.val = NULL;

	if (nfs3_lookuppath_async(nfs, link_data->newparent, 0,
                                  data->cb, data->private_data,
                                  nfs3_link_continue_2_internal,
                                  link_data, free_nfs_link_data, 0) != 0) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                data->continue_data = NULL;
		free_nfs_cb_data(data);
		return -1;
	}
	data->continue_data = NULL;
	free_nfs_cb_data(data);

	return 0;
}

int
nfs3_link_async(struct nfs_context *nfs, const char *oldpath,
                const char *newpath, nfs_cb cb, void *private_data)
{
	char *ptr;
	struct nfs_link_data *link_data;

	link_data = calloc(1, sizeof(struct nfs_link_data));
	if (link_data == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for link data");
		return -1;
	}

	link_data->oldpath = strdup(oldpath);
	if (link_data->oldpath == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for oldpath");
		free_nfs_link_data(link_data);
		return -1;
	}

	link_data->newobject = strdup(newpath);
	if (link_data->newobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to strdup "
                              "newpath");
		free_nfs_link_data(link_data);
		return -1;
	}
	ptr = strrchr(link_data->newobject, '/');
	if (ptr == NULL) {
                link_data->newparent = NULL;
        } else {
                *ptr = 0;
                link_data->newparent = link_data->newobject;

                ptr++;
                link_data->newobject = strdup(ptr);
        }
	if (link_data->newobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for newobject");
		free_nfs_link_data(link_data);
		return -1;
	}

	if (nfs3_lookuppath_async(nfs, link_data->oldpath, 0,
                                  cb, private_data,
                                  nfs3_link_continue_1_internal,
                                  link_data, free_nfs_link_data, 0) != 0) {
		return -1;
	}

	return 0;
}

struct nfs_rename_data {
       char *oldparent;
       char *oldobject;
       struct nfs_fh olddir;
       char *newparent;
       char *newobject;
       struct nfs_fh newdir;
};

static void
free_nfs_rename_data(void *mem)
{
	struct nfs_rename_data *data = mem;

        free(data->oldparent);
        free(data->oldobject);
        free(data->olddir.val);
        free(data->newparent);
        free(data->newobject);
        free(data->newdir.val);
	free(data);
}

static void
nfs3_rename_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	RENAME3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfs_rename_data *rename_data = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: RENAME %s/%s -> %s/%s failed "
                              "with %s(%d)", rename_data->oldparent,
                              rename_data->oldobject, rename_data->newparent,
                              rename_data->newobject,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_rename_continue_2_internal(struct nfs_context *nfs,
                                struct nfs_attr *attr _U_,
                                struct nfs_cb_data *data)
{
	struct nfs_rename_data *rename_data = data->continue_data;
	RENAME3args args;

	/* Drop the destination directory from the cache */
	nfs_dircache_drop(nfs, &data->fh);

	/* steal the filehandle */
	rename_data->newdir = data->fh;
	data->fh.val = NULL;

	args.from.dir.data.data_len = rename_data->olddir.len;
	args.from.dir.data.data_val = rename_data->olddir.val;
	args.from.name = rename_data->oldobject;
	args.to.dir.data.data_len = rename_data->newdir.len;
	args.to.dir.data.data_val = rename_data->newdir.val;
	args.to.name = rename_data->newobject;
	if (rpc_nfs3_rename_task(nfs->rpc, nfs3_rename_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send RENAME "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static int
nfs3_rename_continue_1_internal(struct nfs_context *nfs,
                                struct nfs_attr *attr _U_,
                                struct nfs_cb_data *data)
{
	struct nfs_rename_data *rename_data = data->continue_data;

	/* Drop the source directory from the cache */
	nfs_dircache_drop(nfs, &data->fh);

	/* steal the filehandle */
	rename_data->olddir = data->fh;
	data->fh.val = NULL;

	if (nfs3_lookuppath_async(nfs, rename_data->newparent, 0,
                                  data->cb, data->private_data,
                                  nfs3_rename_continue_2_internal,
                                  rename_data, free_nfs_rename_data, 0) != 0) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
                data->continue_data = NULL;
		free_nfs_cb_data(data);
		return -1;
	}
	data->continue_data = NULL;
	free_nfs_cb_data(data);

	return 0;
}

int
nfs3_rename_async(struct nfs_context *nfs, const char *oldpath,
                  const char *newpath, nfs_cb cb, void *private_data)
{
	char *ptr;
	struct nfs_rename_data *rename_data;

	rename_data = calloc(1, sizeof(struct nfs_rename_data));
	if (rename_data == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for rename data");
		return -1;
	}

	rename_data->oldobject = strdup(oldpath);
	if (rename_data->oldobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to strdup "
                              "oldpath");
		free_nfs_rename_data(rename_data);
		return -1;
	}
	ptr = strrchr(rename_data->oldobject, '/');
	if (ptr == NULL) {
                rename_data->oldparent = NULL;
        } else {
                *ptr = 0;
                rename_data->oldparent = rename_data->oldobject;

                ptr++;
                rename_data->oldobject = strdup(ptr);
        }
	if (rename_data->oldobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for oldobject");
		free_nfs_rename_data(rename_data);
		return -1;
	}

	rename_data->newobject = strdup(newpath);
	if (rename_data->newobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to strdup "
                              "newpath");
		free_nfs_rename_data(rename_data);
		return -1;
	}
	ptr = strrchr(rename_data->newobject, '/');
	if (ptr == NULL) {
                rename_data->newparent = NULL;
        } else {
                *ptr = 0;
                rename_data->newparent = rename_data->newobject;

                ptr++;
                rename_data->newobject = strdup(ptr);
        }
	if (rename_data->newobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for newobject");
		free_nfs_rename_data(rename_data);
		return -1;
	}

	if (nfs3_lookuppath_async(nfs, rename_data->oldparent, 0,
                                  cb, private_data,
                                  nfs3_rename_continue_1_internal,
                                  rename_data, free_nfs_rename_data, 0) != 0) {
		return -1;
	}

	return 0;
}


struct nfs_symlink_data {
        char *target;
        char *linkparent;
        char *linkobject;
};

static void
free_nfs_symlink_data(void *mem)
{
	struct nfs_symlink_data *data = mem;

        free(data->target);
        free(data->linkparent);
        free(data->linkobject);
	free(data);
}

static void
nfs3_symlink_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	SYMLINK3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfs_symlink_data *symlink_data = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: SYMLINK %s/%s -> %s failed with "
                              "%s(%d)", symlink_data->linkparent,
                              symlink_data->linkobject,
                              symlink_data->target,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_symlink_continue_internal(struct nfs_context *nfs,
                               struct nfs_attr *attr _U_,
                               struct nfs_cb_data *data)
{
	struct nfs_symlink_data *symlink_data = data->continue_data;
	SYMLINK3args args;

	memset(&args, 0, sizeof(SYMLINK3args));
	args.where.dir.data.data_len = data->fh.len;
	args.where.dir.data.data_val = data->fh.val;
	args.where.name = symlink_data->linkobject;
	args.symlink.symlink_attributes.mode.set_it = 1;
	args.symlink.symlink_attributes.mode.set_mode3_u.mode = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH;
	args.symlink.symlink_data = symlink_data->target;

	if (rpc_nfs3_symlink_task(nfs->rpc, nfs3_symlink_cb,
                                  &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send SYMLINK "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_symlink_async(struct nfs_context *nfs, const char *target,
                   const char *linkname, nfs_cb cb, void *private_data)
{
	char *ptr;
	struct nfs_symlink_data *symlink_data;

	symlink_data = calloc(1, sizeof(struct nfs_symlink_data));
	if (symlink_data == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for symlink data");
		return -1;
	}

	symlink_data->target = strdup(target);
	if (symlink_data->target == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "buffer for target");
		free_nfs_symlink_data(symlink_data);
		return -1;
	}

        symlink_data->linkobject = strdup(linkname);
	if (symlink_data->linkobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to strdup "
                              "linkname");
		free_nfs_symlink_data(symlink_data);
		return -1;
	}
	ptr = strrchr(symlink_data->linkobject, '/');
	if (ptr == NULL) {
                symlink_data->linkparent = NULL;
        } else {
                *ptr = 0;
                symlink_data->linkparent = symlink_data->linkobject;

                ptr++;
                symlink_data->linkobject = strdup(ptr);
        }

	if (symlink_data->linkobject == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "mode buffer for new path");
		free_nfs_symlink_data(symlink_data);
		return -1;
	}

	if (nfs3_lookuppath_async(nfs, symlink_data->linkparent, 0,
                                  cb, private_data,
                                  nfs3_symlink_continue_internal,
                                  symlink_data, free_nfs_symlink_data, 0)
            != 0) {
		return -1;
	}

	return 0;
}


static void
nfs3_access2_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	ACCESS3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	unsigned int result = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: ACCESS of %s failed with %s(%d)",
                              data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (res->ACCESS3res_u.resok.access & ACCESS3_READ) {
		result |= R_OK;
	}
	if (res->ACCESS3res_u.resok.access & (ACCESS3_MODIFY | ACCESS3_EXTEND | ACCESS3_DELETE)) {
		result |= W_OK;
	}
	if (res->ACCESS3res_u.resok.access & (ACCESS3_LOOKUP | ACCESS3_EXECUTE)) {
		result |= X_OK;
	}

	data->cb(result, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_access2_continue_internal(struct nfs_context *nfs,
                               struct nfs_attr *attr _U_,
                               struct nfs_cb_data *data)
{
	ACCESS3args args;

	memset(&args, 0, sizeof(ACCESS3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	args.access = ACCESS3_READ | ACCESS3_LOOKUP | ACCESS3_MODIFY | ACCESS3_EXTEND | ACCESS3_DELETE | ACCESS3_EXECUTE;

	if (rpc_nfs3_access_task(nfs->rpc, nfs3_access2_cb,
                                 &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_access2_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_access2_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}


static void
nfs3_access_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	ACCESS3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	unsigned int mode = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: ACCESS of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if ((data->continue_int & R_OK) && (res->ACCESS3res_u.resok.access & ACCESS3_READ)) {
		mode |= R_OK;
	}
	if ((data->continue_int & W_OK) && (res->ACCESS3res_u.resok.access & (ACCESS3_MODIFY | ACCESS3_EXTEND | ACCESS3_DELETE))) {
		mode |= W_OK;
	}
	if ((data->continue_int & X_OK) && (res->ACCESS3res_u.resok.access & (ACCESS3_LOOKUP | ACCESS3_EXECUTE))) {
		mode |= X_OK;
	}

	if (data->continue_int != mode) {
		nfs_set_error(nfs, "NFS: ACCESS denied. Required access "
                              "%c%c%c. Allowed access %c%c%c",
					data->continue_int&R_OK?'r':'-',
					data->continue_int&W_OK?'w':'-',
					data->continue_int&X_OK?'x':'-',
					mode&R_OK?'r':'-',
					mode&W_OK?'w':'-',
					mode&X_OK?'x':'-');
		data->cb(-EACCES, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_access_continue_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	int nfsmode = 0;
	ACCESS3args args;

	if (data->continue_int & R_OK) {
		nfsmode |= ACCESS3_READ;
	}
	if (data->continue_int & W_OK) {
		nfsmode |= ACCESS3_MODIFY | ACCESS3_EXTEND | ACCESS3_DELETE;
	}
	if (data->continue_int & X_OK) {
		nfsmode |= ACCESS3_LOOKUP | ACCESS3_EXECUTE;
	}

	memset(&args, 0, sizeof(ACCESS3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	args.access = nfsmode;

	if (rpc_nfs3_access_task(nfs->rpc, nfs3_access_cb, &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_access_async(struct nfs_context *nfs, const char *path, int mode,
                  nfs_cb cb, void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_access_continue_internal,
                                  NULL, NULL,
                                  mode & (R_OK | W_OK | X_OK)) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_utimes_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	SETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: SETATTR failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_utimes_continue_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	SETATTR3args args;
	struct timeval *utimes_data = data->continue_data;

	memset(&args, 0, sizeof(SETATTR3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	if (utimes_data != NULL) {
		args.new_attributes.atime.set_it = SET_TO_CLIENT_TIME;
		args.new_attributes.atime.set_atime_u.atime.seconds  = utimes_data[0].tv_sec;
		args.new_attributes.atime.set_atime_u.atime.nseconds = utimes_data[0].tv_usec * 1000;
		args.new_attributes.mtime.set_it = SET_TO_CLIENT_TIME;
		args.new_attributes.mtime.set_mtime_u.mtime.seconds  = utimes_data[1].tv_sec;
		args.new_attributes.mtime.set_mtime_u.mtime.nseconds = utimes_data[1].tv_usec * 1000;
	} else {
		args.new_attributes.atime.set_it = SET_TO_SERVER_TIME;
		args.new_attributes.mtime.set_it = SET_TO_SERVER_TIME;
	}

	if (rpc_nfs3_setattr_task(nfs->rpc, nfs3_utimes_cb,
                                  &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send SETATTR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_utimes_async_internal(struct nfs_context *nfs, const char *path,
                           int no_follow, struct timeval *times,
                           nfs_cb cb, void *private_data)
{
	struct timeval *new_times = NULL;

	if (times != NULL) {
		new_times = malloc(sizeof(struct timeval)*2);
		if (new_times == NULL) {
			nfs_set_error(nfs, "Failed to allocate memory "
                                      "for timeval structure");
			return -1;
		}

		memcpy(new_times, times, sizeof(struct timeval)*2);
	}

	if (nfs3_lookuppath_async(nfs, path, no_follow, cb, private_data,
                                  nfs3_utimes_continue_internal,
                                  new_times, free, 0) != 0) {
		return -1;
	}

	return 0;
}

int
nfs3_utime_async(struct nfs_context *nfs, const char *path,
                 struct utimbuf *times, nfs_cb cb, void *private_data)
{
	struct timeval *new_times = NULL;

	if (times != NULL) {
		new_times = malloc(sizeof(struct timeval)*2);
		if (new_times == NULL) {
			nfs_set_error(nfs, "Failed to allocate memory "
                                      "for timeval structure");
			return -1;
		}

		new_times[0].tv_sec  = times->actime;
		new_times[0].tv_usec = 0;
		new_times[1].tv_sec  = times->modtime;
		new_times[1].tv_usec = 0;
	}

	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_utimes_continue_internal,
                                  new_times, free, 0) != 0) {
		return -1;
	}

	return 0;
}

        
static void
nfs3_chown_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	SETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: SETATTR failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

struct nfs_chown_data {
       uid_t uid;
       gid_t gid;
};

static int
nfs3_chown_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	SETATTR3args args;
	struct nfs_chown_data *chown_data = data->continue_data;

	memset(&args, 0, sizeof(SETATTR3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	if (chown_data->uid != (uid_t)-1) {
		args.new_attributes.uid.set_it = 1;
		args.new_attributes.uid.set_uid3_u.uid = chown_data->uid;
	}
	if (chown_data->gid != (gid_t)-1) {
		args.new_attributes.gid.set_it = 1;
		args.new_attributes.gid.set_gid3_u.gid = chown_data->gid;
	}

	if (rpc_nfs3_setattr_task(nfs->rpc, nfs3_chown_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send SETATTR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_chown_async_internal(struct nfs_context *nfs, const char *path,
                          int no_follow, int uid, int gid,
                          nfs_cb cb, void *private_data)
{
	struct nfs_chown_data *chown_data;

	chown_data = malloc(sizeof(struct nfs_chown_data));
	if (chown_data == NULL) {
		nfs_set_error(nfs, "Failed to allocate memory for "
                              "chown data structure");
		return -1;
	}

	chown_data->uid = uid;
	chown_data->gid = gid;

	if (nfs3_lookuppath_async(nfs, path, no_follow, cb, private_data,
                                  nfs3_chown_continue_internal,
                                  chown_data, free, 0) != 0) {
		return -1;
	}

	return 0;
}

int
nfs3_fchown_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int uid,
                  int gid, nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;
	struct nfs_chown_data *chown_data;

	chown_data = malloc(sizeof(struct nfs_chown_data));
	if (chown_data == NULL) {
		nfs_set_error(nfs, "Failed to allocate memory for "
                              "fchown data structure");
		return -1;
	}

	chown_data->uid = uid;
	chown_data->gid = gid;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
                              "memory for fchown data");
		free(chown_data);
		return -1;
	}
	data->nfs           = nfs;
	data->cb            = cb;
	data->private_data  = private_data;
	data->continue_data = chown_data;
	data->free_continue_data = free;
	data->fh.len = nfsfh->fh.len;
	data->fh.val = malloc(data->fh.len);
	if (data->fh.val == NULL) {
		nfs_set_error(nfs, "Out of memory: Failed to allocate fh");
		free_nfs_cb_data(data);
		return -1;
	}
	memcpy(data->fh.val, nfsfh->fh.val, data->fh.len);

	if (nfs3_chown_continue_internal(nfs, NULL, data) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_chmod_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	SETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: SETATTR failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_chmod_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	SETATTR3args args;

	memset(&args, 0, sizeof(SETATTR3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	args.new_attributes.mode.set_it = 1;
	args.new_attributes.mode.set_mode3_u.mode = (mode3)data->continue_int;

	if (rpc_nfs3_setattr_task(nfs->rpc, nfs3_chmod_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send SETATTR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_chmod_async_internal(struct nfs_context *nfs, const char *path,
                          int no_follow, int mode, nfs_cb cb,
                          void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, no_follow, cb, private_data,
                                  nfs3_chmod_continue_internal,
                                  NULL, NULL, mode) != 0) {
		return -1;
	}

	return 0;
}

int
nfs3_fchmod_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int mode,
                  nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory. failed to allocate "
                              "memory for fchmod data");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;
	data->continue_int = mode;
	data->fh.len = nfsfh->fh.len;
	data->fh.val = malloc(data->fh.len);
	if (data->fh.val == NULL) {
		nfs_set_error(nfs, "Out of memory: Failed to allocate fh");
		free_nfs_cb_data(data);
		return -1;
	}
	memcpy(data->fh.val, nfsfh->fh.val, data->fh.len);

	if (nfs3_chmod_continue_internal(nfs, NULL, data) != 0) {
		return -1;
	}

	return 0;
}


static void
nfs3_readlink_1_cb(struct rpc_context *rpc, int status, void *command_data,
                   void *private_data)
{
	READLINK3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: READLINK of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->cb(0, nfs, res->READLINK3res_u.resok.data, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_readlink_continue_internal(struct nfs_context *nfs,
                                struct nfs_attr *attr _U_,
                                struct nfs_cb_data *data)
{
	READLINK3args args;

	args.symlink.data.data_val = data->fh.val;
	args.symlink.data.data_len = data->fh.len;

	if (rpc_nfs3_readlink_task(nfs->rpc, nfs3_readlink_1_cb,
                                   &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send READLINK "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_readlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                    void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 1, cb, private_data,
                                  nfs3_readlink_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_statvfs_1_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	FSSTAT3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct statvfs svfs;
	struct nfs_statvfs_64 svfs64;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: FSSTAT of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

        if (data->continue_int == 0) {
                /* statvfs */
                svfs.f_bsize   = NFS_BLKSIZE;
                svfs.f_frsize  = NFS_BLKSIZE;
                svfs.f_blocks  = res->FSSTAT3res_u.resok.tbytes/NFS_BLKSIZE;
                svfs.f_bfree   = res->FSSTAT3res_u.resok.fbytes/NFS_BLKSIZE;
                svfs.f_bavail  = res->FSSTAT3res_u.resok.abytes/NFS_BLKSIZE;
                svfs.f_files   = (uint32_t)res->FSSTAT3res_u.resok.tfiles;
                svfs.f_ffree   = (uint32_t)res->FSSTAT3res_u.resok.ffiles;
#if !defined(__ANDROID__)
                svfs.f_favail  = (uint32_t)res->FSSTAT3res_u.resok.afiles;
                svfs.f_fsid    = 0;
                svfs.f_flag    = 0;
                svfs.f_namemax = 256;
#endif
                data->cb(0, nfs, &svfs, data->private_data);
        } else {
                /* statvfs64 */
                svfs64.f_bsize   = NFS_BLKSIZE;
                svfs64.f_frsize  = NFS_BLKSIZE;
                svfs64.f_blocks  = res->FSSTAT3res_u.resok.tbytes/NFS_BLKSIZE;
                svfs64.f_bfree   = res->FSSTAT3res_u.resok.fbytes/NFS_BLKSIZE;
                svfs64.f_bavail  = res->FSSTAT3res_u.resok.abytes/NFS_BLKSIZE;
                svfs64.f_files   = res->FSSTAT3res_u.resok.tfiles;
                svfs64.f_ffree   = res->FSSTAT3res_u.resok.ffiles;
                svfs64.f_favail  = res->FSSTAT3res_u.resok.afiles;
                svfs64.f_fsid    = 0;
                svfs64.f_flag    = 0;
                svfs64.f_namemax = 256;

                data->cb(0, nfs, &svfs64, data->private_data);
        }

	free_nfs_cb_data(data);
}

static int
nfs3_statvfs_continue_internal(struct nfs_context *nfs,
                               struct nfs_attr *attr _U_,
                               struct nfs_cb_data *data)
{
	FSSTAT3args args;

	args.fsroot.data.data_len = data->fh.len;
	args.fsroot.data.data_val = data->fh.val;
	if (rpc_nfs3_fsstat_task(nfs->rpc, nfs3_statvfs_1_cb,
                                 &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send FSSTAT "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_statvfs_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_statvfs_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}

int
nfs3_statvfs64_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                     void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_statvfs_continue_internal,
                                  NULL, NULL, 1) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_lseek_1_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	GETATTR3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	int64_t size = 0;
        int64_t offset = (int64_t) data->offset;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETATTR failed with "
                              "%s(%d)", nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free(data);
		return;
	}

	size = (int64_t)res->GETATTR3res_u.resok.obj_attributes.size;

	if (offset < 0 &&
	    -offset > (int64_t)size) {
		data->cb(-EINVAL, nfs, &data->nfsfh->offset,
                         data->private_data);
	} else {
		data->nfsfh->offset = data->offset + size;
		data->cb(0, nfs, &data->nfsfh->offset, data->private_data);
	}

	free(data);
}

int
nfs3_lseek_async(struct nfs_context *nfs, struct nfsfh *nfsfh, int64_t offset,
                 int whence, nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;
	struct GETATTR3args args;

	if (whence == SEEK_SET) {
		if (offset < 0) {
			cb(-EINVAL, nfs, &nfsfh->offset, private_data);
		} else {
			nfsfh->offset = offset;
			cb(0, nfs, &nfsfh->offset, private_data);
		}
		return 0;
	}
	if (whence == SEEK_CUR) {
		if (offset < 0 &&
		    nfsfh->offset < (uint64_t)(-offset)) {
			cb(-EINVAL, nfs, &nfsfh->offset, private_data);
		} else {
			nfsfh->offset += offset;
			cb(0, nfs, &nfsfh->offset, private_data);
		}
		return 0;
	}

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "Out Of Memory: Failed to malloc nfs "
			      "cb data");
		return -1;
	}

	data->nfs          = nfs;
	data->nfsfh        = nfsfh;
	data->offset       = offset;
	data->cb           = cb;
	data->private_data = private_data;

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = nfsfh->fh.len;
	args.object.data.data_val = nfsfh->fh.val;

	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_lseek_1_cb,
                                  &args, data) == NULL) {
		free(data);
		return -1;
	}
	return 0;
}


/* ReadDirPlus Emulation Callback data */
struct rdpe_cb_data {
	int getattrcount;
	int status;
	struct nfs_cb_data *data;
};

/* ReadDirPlus Emulation LOOKUP Callback data */
struct rdpe_lookup_cb_data {
	struct rdpe_cb_data *rdpe_cb_data;
	struct nfsdirent *nfsdirent;
};

/* Workaround for servers lacking READDIRPLUS.
 * Use READDIR instead and a GETATTR-loop */
static void
nfs3_opendir_3_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	LOOKUP3res *res = command_data;
	struct rdpe_lookup_cb_data *rdpe_lookup_cb_data = private_data;
	struct rdpe_cb_data *rdpe_cb_data = rdpe_lookup_cb_data->rdpe_cb_data;
	struct nfs_cb_data *data = rdpe_cb_data->data;
	struct nfsdir *nfsdir = data->continue_data;
	struct nfs_context *nfs = data->nfs;
	struct nfsdirent *nfsdirent = rdpe_lookup_cb_data->nfsdirent;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	free(rdpe_lookup_cb_data);

	rdpe_cb_data->getattrcount--;

	if (status == RPC_STATUS_ERROR) {

		nfs_set_error(nfs, "LOOKUP during READDIRPLUS emulation "
			      "failed with RPC_STATUS_ERROR");
		rdpe_cb_data->status = RPC_STATUS_ERROR;
	}
	if (status == RPC_STATUS_CANCEL) {
		nfs_set_error(nfs, "LOOKUP during READDIRPLUS emulation "
			      "failed with RPC_STATUS_CANCEL");
		rdpe_cb_data->status = RPC_STATUS_CANCEL;
	}
	if (status == RPC_STATUS_TIMEOUT) {
		nfs_set_error(nfs, "LOOKUP during READDIRPLUS emulation "
			      "timed out");
		rdpe_cb_data->status = RPC_STATUS_CANCEL;
	}
	if (status == RPC_STATUS_SUCCESS && res->status == NFS3_OK) {
		if (res->LOOKUP3res_u.resok.obj_attributes.attributes_follow) {
			fattr3 *attributes = &res->LOOKUP3res_u.resok.obj_attributes.post_op_attr_u.attributes;

			nfsdirent->type = attributes->type;
			nfsdirent->mode = attributes->mode;
			switch (nfsdirent->type) {
			case NF3REG:  nfsdirent->mode |= S_IFREG; break;
			case NF3DIR:  nfsdirent->mode |= S_IFDIR; break;
			case NF3BLK:  nfsdirent->mode |= S_IFBLK; break;
			case NF3CHR:  nfsdirent->mode |= S_IFCHR; break;
			case NF3LNK:  nfsdirent->mode |= S_IFLNK; break;
			case NF3SOCK: nfsdirent->mode |= S_IFSOCK; break;
			case NF3FIFO: nfsdirent->mode |= S_IFIFO; break;
			};
			nfsdirent->size = attributes->size;

			nfsdirent->atime.tv_sec  = attributes->atime.seconds;
			nfsdirent->atime.tv_usec = attributes->atime.nseconds/1000;
			nfsdirent->atime_nsec = attributes->atime.nseconds;
			nfsdirent->mtime.tv_sec  = attributes->mtime.seconds;
			nfsdirent->mtime.tv_usec = attributes->mtime.nseconds/1000;
			nfsdirent->mtime_nsec = attributes->mtime.nseconds;
			nfsdirent->ctime.tv_sec  = attributes->ctime.seconds;
			nfsdirent->ctime.tv_usec = attributes->ctime.nseconds/1000;
			nfsdirent->ctime_nsec = attributes->ctime.nseconds;
			nfsdirent->uid = attributes->uid;
			nfsdirent->gid = attributes->gid;
			nfsdirent->nlink = attributes->nlink;
			nfsdirent->dev = attributes->fsid;
			nfsdirent->rdev = specdata3_to_rdev(&attributes->rdev);
			nfsdirent->blksize = NFS_BLKSIZE;
			nfsdirent->blocks = (attributes->used + 512 - 1) / 512;
			nfsdirent->used = attributes->used;
		}
	}

	if (rdpe_cb_data->getattrcount == 0) {
		if (rdpe_cb_data->status != RPC_STATUS_SUCCESS) {
			nfs_set_error(nfs, "READDIRPLUS emulation "
			      "failed: %s", rpc_get_error(rpc));
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
				data->private_data);
			nfs_free_nfsdir(nfsdir);
		} else {
			data->cb(0, nfs, nfsdir, data->private_data);
		}
		free(rdpe_cb_data);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
	}
}

static int
lookup_missing_attributes(struct nfs_context *nfs,
                          struct nfsdir *nfsdir,
                          struct nfs_cb_data *data)
{
	struct rdpe_cb_data *rdpe_cb_data = NULL;
	struct nfsdirent *nfsdirent;

	for (nfsdirent = nfsdir->entries;
	     nfsdirent;
	     nfsdirent = nfsdirent->next) {
		struct rdpe_lookup_cb_data *rdpe_lookup_cb_data;
		LOOKUP3args args;

		/* If type == 0 we assume it is a case of the server not
		 * giving us the attributes for this entry during READIR[PLUS]
		 * so we fallback to LOOKUP3
		 */
		if (nfsdirent->type != 0) {
			continue;
		}

		if (rdpe_cb_data == NULL) {
			rdpe_cb_data = malloc(sizeof(struct rdpe_cb_data));
			rdpe_cb_data->getattrcount = 0;
			rdpe_cb_data->status = RPC_STATUS_SUCCESS;
			rdpe_cb_data->data = data;
		}
		rdpe_lookup_cb_data = malloc(sizeof(struct rdpe_lookup_cb_data));
		rdpe_lookup_cb_data->rdpe_cb_data = rdpe_cb_data;
		rdpe_lookup_cb_data->nfsdirent = nfsdirent;

		memset(&args, 0, sizeof(LOOKUP3args));
		args.what.dir.data.data_len = data->fh.len;
		args.what.dir.data.data_val = data->fh.val;
		args.what.name = nfsdirent->name;

		if (rpc_nfs3_lookup_task(nfs->rpc, nfs3_opendir_3_cb, &args,
                                         rdpe_lookup_cb_data) == NULL) {
			nfs_set_error(nfs, "RPC error: Failed to send "
				      "READDIR LOOKUP call");

			/* if we have already commands in flight, we cant just
			 * stop, we have to wait for the commands in flight to
			 * complete
			 */
			continue;
		}
		rdpe_cb_data->getattrcount++;
	}
	if (rdpe_cb_data != NULL) {
		return rdpe_cb_data->getattrcount;
	}
	return 0;
}

static void
nfs3_opendir_2_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	READDIR3res *res = command_data;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfsdir *nfsdir = data->continue_data;
	struct nfsdirent *nfsdirent;
	struct entry3 *entry;
	uint64_t cookie = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		nfs_free_nfsdir(nfsdir);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}

	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: READDIR of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		nfs_free_nfsdir(nfsdir);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}

	entry =res->READDIR3res_u.resok.reply.entries;
	while (entry != NULL) {
		nfsdirent = calloc(1, sizeof(struct nfsdirent));
		if (nfsdirent == NULL) {
			data->cb(-ENOMEM, nfs, "Failed to allocate dirent",
                                 data->private_data);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		nfsdirent->name = strdup(entry->name);
		if (nfsdirent->name == NULL) {
			data->cb(-ENOMEM, nfs, "Failed to allocate "
                                 "dirent->name", data->private_data);
			free(nfsdirent);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		nfsdirent->inode = entry->fileid;

		nfsdirent->next  = nfsdir->entries;
		nfsdir->entries  = nfsdirent;

		cookie = entry->cookie;
		entry  = entry->nextentry;
	}

	if (res->READDIR3res_u.resok.reply.eof == 0) {
		READDIR3args args;

		args.dir.data.data_len = data->fh.len;
		args.dir.data.data_val = data->fh.val;
		args.cookie = cookie;
		memcpy(&args.cookieverf, res->READDIR3res_u.resok.cookieverf,
                       sizeof(cookieverf3));
		args.count = nfs->nfsi->readdir_dircount;

	     	if (rpc_nfs3_readdir_task(nfs->rpc, nfs3_opendir_2_cb,
                                          &args, data) == NULL) {
			nfs_set_error(nfs, "RPC error: Failed to send "
                                      "READDIR call for %s", data->path);
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		return;
	}

	if (res->READDIR3res_u.resok.dir_attributes.attributes_follow)
		fattr3_to_nfs_attr(&nfsdir->attr, &res->READDIR3res_u.resok.dir_attributes.post_op_attr_u.attributes);

	/* steal the dirhandle */
	nfsdir->current = nfsdir->entries;

	if (lookup_missing_attributes(nfs, nfsdir, data) == 0) {
		data->cb(0, nfs, nfsdir, data->private_data);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}
}

static void
nfs3_opendir_cb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	READDIRPLUS3res *res = command_data;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfsdir *nfsdir = data->continue_data;
	struct entryplus3 *entry;
	uint64_t cookie = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

    if (check_nfs3_error(nfs, status, data, command_data)) {
		nfs_free_nfsdir(nfsdir);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}

	if (status == RPC_STATUS_SUCCESS && res->status == NFS3ERR_NOTSUPP) {
		READDIR3args args;

		args.dir.data.data_len = data->fh.len;
		args.dir.data.data_val = data->fh.val;
		args.cookie = cookie;
		memset(&args.cookieverf, 0, sizeof(cookieverf3));
		args.count = nfs->nfsi->readdir_dircount;

		if (rpc_nfs3_readdir_task(nfs->rpc, nfs3_opendir_2_cb,
                                          &args, data) == NULL) {
			nfs_set_error(nfs, "RPC error: Failed to send "
                                      "READDIR call for %s", data->path);
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		return;
	}

	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: READDIRPLUS of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		nfs_free_nfsdir(nfsdir);
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}

	entry =res->READDIRPLUS3res_u.resok.reply.entries;
	while (entry != NULL) {
		struct nfsdirent *nfsdirent;
		struct nfs_attr attr;
                int has_attr = 0;

                memset(&attr, 0, sizeof(attr));

		nfsdirent = calloc(1, sizeof(struct nfsdirent));
		if (nfsdirent == NULL) {
			data->cb(-ENOMEM, nfs, "Failed to allocate dirent",
                                 data->private_data);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		nfsdirent->name = strdup(entry->name);
		if (nfsdirent->name == NULL) {
			data->cb(-ENOMEM, nfs, "Failed to allocate "
                                 "dirent->name", data->private_data);
			free(nfsdirent);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		nfsdirent->inode = entry->fileid;

		if (entry->name_attributes.attributes_follow) {
			fattr3_to_nfs_attr(&attr, &entry->name_attributes.post_op_attr_u.attributes);
                        has_attr = 1;
                }

		if (!has_attr) {
			struct nested_mounts *mnt;
			int splen = strlen(data->saved_path);

			/* A single '/' is a special case, treat it as
			 * zero-length below. */
			if (splen == 1)
				splen = 0;

			/* No name attributes. Is it a nested mount then?*/
			for(mnt = nfs->nfsi->nested_mounts; mnt; mnt = mnt->next) {
				if (strncmp(data->saved_path, mnt->path, splen))
					continue;
				if (mnt->path[splen] != '/')
					continue;
				if (strcmp(mnt->path + splen + 1, entry->name))
					continue;
				attr = mnt->attr;
                                has_attr = 1;
				break;
			}
		}
		if (has_attr) {
                        struct specdata3 sd3 = { attr.rdev.specdata1,
                                                 attr.rdev.specdata2 };

			nfsdirent->type = attr.type;
			nfsdirent->mode = attr.mode;
			switch (nfsdirent->type) {
			case NF3REG:  nfsdirent->mode |= S_IFREG; break;
			case NF3DIR:  nfsdirent->mode |= S_IFDIR; break;
			case NF3BLK:  nfsdirent->mode |= S_IFBLK; break;
			case NF3CHR:  nfsdirent->mode |= S_IFCHR; break;
			case NF3LNK:  nfsdirent->mode |= S_IFLNK; break;
			case NF3SOCK: nfsdirent->mode |= S_IFSOCK; break;
			case NF3FIFO: nfsdirent->mode |= S_IFIFO; break;
			};
			nfsdirent->size = attr.size;

			nfsdirent->atime.tv_sec  = attr.atime.seconds;
			nfsdirent->atime.tv_usec = attr.atime.nseconds/1000;
			nfsdirent->atime_nsec = attr.atime.nseconds;
			nfsdirent->mtime.tv_sec  = attr.mtime.seconds;
			nfsdirent->mtime.tv_usec = attr.mtime.nseconds/1000;
			nfsdirent->mtime_nsec = attr.mtime.nseconds;
			nfsdirent->ctime.tv_sec  = attr.ctime.seconds;
			nfsdirent->ctime.tv_usec = attr.ctime.nseconds/1000;
			nfsdirent->ctime_nsec = attr.ctime.nseconds;
			nfsdirent->uid = attr.uid;
			nfsdirent->gid = attr.gid;
			nfsdirent->nlink = attr.nlink;
			nfsdirent->dev = attr.fsid;
			nfsdirent->rdev = specdata3_to_rdev(&sd3);
			nfsdirent->blksize = NFS_BLKSIZE;
			nfsdirent->blocks = (attr.used + 512 - 1) / 512;
			nfsdirent->used = attr.used;
		}

		nfsdirent->next  = nfsdir->entries;
		nfsdir->entries  = nfsdirent;

		cookie = entry->cookie;
		entry  = entry->nextentry;
	}

	if (res->READDIRPLUS3res_u.resok.reply.eof == 0) {
		READDIRPLUS3args args;

		args.dir.data.data_len = data->fh.len;
		args.dir.data.data_val = data->fh.val;
		args.cookie = cookie;
		memcpy(&args.cookieverf,
                       res->READDIRPLUS3res_u.resok.cookieverf,
                       sizeof(cookieverf3));
		args.dircount = nfs->nfsi->readdir_dircount;
		args.maxcount = nfs->nfsi->readdir_maxcount;

	     	if (rpc_nfs3_readdirplus_task(nfs->rpc, nfs3_opendir_cb,
                                              &args, data) == NULL) {
			nfs_set_error(nfs, "RPC error: Failed to send "
                                      "READDIRPLUS call for %s", data->path);
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			nfs_free_nfsdir(nfsdir);
			data->continue_data = NULL;
			free_nfs_cb_data(data);
			return;
		}
		return;
	}

	if (res->READDIRPLUS3res_u.resok.dir_attributes.attributes_follow) {
		fattr3_to_nfs_attr(&nfsdir->attr, &res->READDIRPLUS3res_u.resok.dir_attributes.post_op_attr_u.attributes);
        }

	/* steal the dirhandle */
	nfsdir->current = nfsdir->entries;

	if (lookup_missing_attributes(nfs, nfsdir, data) == 0) {
		data->cb(0, nfs, nfsdir, data->private_data);
		/* We can not free data->continue_data here */
		data->continue_data = NULL;
		free_nfs_cb_data(data);
		return;
	}
}

static int
nfs3_opendir_continue_internal(struct nfs_context *nfs,
                               struct nfs_attr *attr,
                               struct nfs_cb_data *data)
{
	READDIRPLUS3args args;
	struct nfsdir *nfsdir = data->continue_data;
	struct nfsdir *cached;

	cached = nfs_dircache_find(nfs, &data->fh);
	if (cached) {
		if (attr && attr->mtime.seconds == cached->attr.mtime.seconds
		    && attr->mtime.nseconds == cached->attr.mtime.nseconds) {
			cached->current = cached->entries;
			data->cb(0, nfs, cached, data->private_data);
			free_nfs_cb_data(data);
			return 0;
		} else {
			/* cache must be stale */
			nfs_free_nfsdir(cached);
		}
	}

	nfsdir->fh.len  = data->fh.len;
	nfsdir->fh.val = malloc(nfsdir->fh.len);
	if (nfsdir->fh.val == NULL) {
		nfs_set_error(nfs, "OOM when allocating fh for nfsdir");
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	memcpy(nfsdir->fh.val, data->fh.val, data->fh.len);

	args.dir.data.data_len = data->fh.len;
	args.dir.data.data_val = data->fh.val;
	args.cookie = 0;
	memset(&args.cookieverf, 0, sizeof(cookieverf3));
	args.dircount = nfs->nfsi->readdir_dircount;
	args.maxcount = nfs->nfsi->readdir_maxcount;
	if (rpc_nfs3_readdirplus_task(nfs->rpc, nfs3_opendir_cb,
                                      &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send "
                              "READDIRPLUS call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_opendir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                   void *private_data)
{
	struct nfsdir *nfsdir;

	nfsdir = calloc(1, sizeof(struct nfsdir));
	if (nfsdir == NULL) {
		nfs_set_error(nfs, "failed to allocate buffer for nfsdir");
		return -1;
	}

	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_opendir_continue_internal,
                                  nfsdir, free, 0) != 0) {
		return -1;
	}

	return 0;
}

struct mknod_cb_data {
       char *path;
       int mode;
       int major;
       int minor;
};

static void
free_mknod_cb_data(void *ptr)
{
	struct mknod_cb_data *data = ptr;

	free(data->path);
	free(data);
}

static void
nfs3_mknod_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	MKNOD3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	char *str = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	str = &str[strlen(str) + 1];

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: MKNOD of %s/%s failed with "
                              "%s(%d)", data->saved_path, str,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_mknod_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	struct mknod_cb_data *cb_data = data->continue_data;
	char *str = cb_data->path;
	MKNOD3args args;

        memset(&args, 0, sizeof(args));

	str = &str[strlen(str) + 1];

	args.where.dir.data.data_len = data->fh.len;
	args.where.dir.data.data_val = data->fh.val;
	args.where.name = str;
	switch (cb_data->mode & S_IFMT) {
	case S_IFCHR:
		args.what.type = NF3CHR;
		args.what.mknoddata3_u.chr_device.dev_attributes.mode.set_it = 1;
		args.what.mknoddata3_u.chr_device.dev_attributes.mode.set_mode3_u.mode = cb_data->mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		args.what.mknoddata3_u.chr_device.spec.specdata1 = cb_data->major;
		args.what.mknoddata3_u.chr_device.spec.specdata2 = cb_data->minor;
		break;
	case S_IFBLK:
		args.what.type = NF3BLK;
		args.what.mknoddata3_u.blk_device.dev_attributes.mode.set_it = 1;
		args.what.mknoddata3_u.blk_device.dev_attributes.mode.set_mode3_u.mode = cb_data->mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		args.what.mknoddata3_u.blk_device.spec.specdata1 = cb_data->major;
		args.what.mknoddata3_u.blk_device.spec.specdata2 = cb_data->minor;
                break;
	case S_IFSOCK:
		args.what.type = NF3SOCK;
		args.what.mknoddata3_u.sock_attributes.mode.set_it = 1;
		args.what.mknoddata3_u.sock_attributes.mode.set_mode3_u.mode = cb_data->mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		break;
	case S_IFIFO:
		args.what.type = NF3FIFO;
		args.what.mknoddata3_u.pipe_attributes.mode.set_it = 1;
		args.what.mknoddata3_u.pipe_attributes.mode.set_mode3_u.mode = cb_data->mode & (S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
		break;
	default:
		nfs_set_error(nfs, "Invalid file type for "
                              "NFS3/MKNOD call");
		data->cb(-EINVAL, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}

	if (rpc_nfs3_mknod_task(nfs->rpc, nfs3_mknod_cb, &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_mknod_async(struct nfs_context *nfs, const char *path, int mode, int dev,
                 nfs_cb cb, void *private_data)
{
	const char *ptr;
	struct mknod_cb_data *cb_data;

	cb_data = malloc(sizeof(struct mknod_cb_data));
	if (cb_data == NULL) {
		nfs_set_error(nfs, "Out of memory, failed to allocate "
                              "mode buffer for cb data");
		return -1;
	}

        ptr = strrchr(path, '/');
        if (ptr) {
                cb_data->path = strdup(path);
                if (cb_data->path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for mknod path");
                        return -1;
                }
                char *ptr2 = strrchr(cb_data->path, '/');
                *ptr2 = 0;
        } else {
                cb_data->path = malloc(strlen(path) + 2);
                if (cb_data->path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for mknod path");
                        return -1;
                }
                sprintf(cb_data->path, "%c%s", '\0', path);
        }

	cb_data->mode = mode;
	cb_data->major = major(dev);
	cb_data->minor = minor(dev);

	/* data->path now points to the parent directory and beyond the
         * null terminator is the new node to create */
	if (nfs3_lookuppath_async(nfs, cb_data->path, 0, cb, private_data,
                                  nfs3_mknod_continue_internal,
                                  cb_data, free_mknod_cb_data, 0) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_unlink_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	REMOVE3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	char *str = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	str = &str[strlen(str) + 1];

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: REMOVE of %s/%s failed with "
                              "%s(%d)", data->saved_path, str,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_unlink_continue_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	char *str = data->continue_data;
	struct REMOVE3args args;

	str = &str[strlen(str) + 1];

	args.object.dir.data.data_len = data->fh.len;
        args.object.dir.data.data_val = data->fh.val;
	args.object.name = str;
	if (rpc_nfs3_remove_task(nfs->rpc, nfs3_unlink_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send REMOVE "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_unlink_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                  void *private_data)
{
	char *new_path;
	const char *ptr;

        ptr = strrchr(path, '/');
        if (ptr) {
                new_path = strdup(path);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for unlink path");
                        return -1;
                }
                char *ptr2 = strrchr(new_path, '/');
                *ptr2 = 0;
        } else {
                new_path = malloc(strlen(path) + 2);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for unlink path");
                        return -1;
                }
                sprintf(new_path, "%c%s", '\0', path);
        }

	/* new_path now points to the parent directory and beyond the
         * null terminator is the object to unlink */
	if (nfs3_lookuppath_async(nfs, new_path, 0, cb, private_data,
                                  nfs3_unlink_continue_internal,
                                  new_path, free, 0) != 0) {
		return -1;
	}

	return 0;
}

struct create_cb_data {
       char *path;
       int flags;
       int mode;
};

int
nfs3_creat_async(struct nfs_context *nfs, const char *path,
                 int mode, nfs_cb cb, void *private_data)
{
        return nfs3_open_async(nfs, path, O_CREAT|O_WRONLY|O_TRUNC,
                               mode, cb, private_data);
}

static void
nfs3_rmdir_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	RMDIR3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	char *str = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	str = &str[strlen(str) + 1];

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: RMDIR of %s/%s failed with "
                              "%s(%d)", data->saved_path, str,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_rmdir_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	char *str = data->continue_data;
	RMDIR3args args;

	str = &str[strlen(str) + 1];

	args.object.dir.data.data_len = data->fh.len;
	args.object.dir.data.data_val = data->fh.val;
	args.object.name = str;
	if (rpc_nfs3_rmdir_task(nfs->rpc, nfs3_rmdir_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send RMDIR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_rmdir_async(struct nfs_context *nfs, const char *path, nfs_cb cb,
                 void *private_data)
{
	char *new_path;
	const char *ptr;

        ptr = strrchr(path, '/');
        if (ptr) {
                new_path = strdup(path);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for rmdir path");
                        return -1;
                }
                char *ptr2 = strrchr(new_path, '/');
                *ptr2 = 0;
        } else {
                new_path = malloc(strlen(path) + 2);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for rmdir path");
                        return -1;
                }
                sprintf(new_path, "%c%s", '\0', path);
        }

	/* new_path now points to the parent directory and beyond the
         * null terminator is the directory to remove */
	if (nfs3_lookuppath_async(nfs, new_path, 0, cb, private_data,
                                  nfs3_rmdir_continue_internal,
                                  new_path, free, 0) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_mkdir_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	MKDIR3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	char *str = data->continue_data;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	str = &str[strlen(str) + 1];

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: MKDIR of %s/%s failed "
                              "with %s(%d)", data->saved_path, str,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_mkdir_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	char *str = data->continue_data;
	int mode = (int)data->continue_int;
	MKDIR3args args;

	str = &str[strlen(str) + 1];

	memset(&args, 0, sizeof(MKDIR3args));
	args.where.dir.data.data_len = data->fh.len;
	args.where.dir.data.data_val = data->fh.val;
	args.where.name = str;
	args.attributes.mode.set_it = 1;
	args.attributes.mode.set_mode3_u.mode = mode;

	if (rpc_nfs3_mkdir_task(nfs->rpc, nfs3_mkdir_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send MKDIR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_mkdir2_async(struct nfs_context *nfs, const char *path, int mode,
                 nfs_cb cb, void *private_data)
{
	char *new_path;
	const char *ptr;

        ptr = strrchr(path, '/');
        if (ptr) {
                new_path = strdup(path);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for mkdir path");
                        return -1;
                }
                char *ptr2 = strrchr(new_path, '/');
                *ptr2 = 0;
        } else {
                new_path = malloc(strlen(path) + 2);
                if (new_path == NULL) {
                        nfs_set_error(nfs, "Out of memory, failed to allocate "
                                      "buffer for mkdir path");
                        return -1;
                }
                sprintf(new_path, "%c%s", '\0', path);
        }

	/* new_path now points to the parent directory and beyond the 
         * null terminator is the new directory to create */
	if (nfs3_lookuppath_async(nfs, new_path, 0, cb, private_data,
                                  nfs3_mkdir_continue_internal,
                                  new_path, free, mode) != 0) {
		return -1;
	}

	return 0;
}

static int
nfs3_truncate_continue_internal(struct nfs_context *nfs,
                                struct nfs_attr *attr _U_,
                                struct nfs_cb_data *data)
{
	uint64_t offset = data->continue_int;
	struct nfsfh nfsfh;

        memset(&nfsfh, 0, sizeof(struct nfsfh));
	nfsfh.fh = data->fh;

	if (nfs_ftruncate_async(nfs, &nfsfh, offset, data->cb,
                                data->private_data) != 0) {
		nfs_set_error(nfs, "RPC error: Failed to send SETATTR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	free_nfs_cb_data(data);
	return 0;
}

int
nfs3_truncate_async(struct nfs_context *nfs, const char *path, uint64_t length,
                    nfs_cb cb, void *private_data)
{
	uint64_t offset;

	offset = length;

	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_truncate_continue_internal,
                                  NULL, NULL, offset) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_ftruncate_cb(struct rpc_context *rpc, int status, void *command_data,
                  void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	SETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: Setattr failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

int
nfs3_ftruncate_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                     uint64_t length, nfs_cb cb, void *private_data)
{
	struct nfs_cb_data *data;
	SETATTR3args args;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	memset(&args, 0, sizeof(SETATTR3args));
	args.object.data.data_len = nfsfh->fh.len;
	args.object.data.data_val = nfsfh->fh.val;
	args.new_attributes.size.set_it = 1;
	args.new_attributes.size.set_size3_u.size = length;

	if (rpc_nfs3_setattr_task(nfs->rpc, nfs3_ftruncate_cb,
                                  &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send SETATTR "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static void
nfs3_fsync_cb(struct rpc_context *rpc, int status, void *command_data,
              void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	COMMIT3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: Commit failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
}

int
nfs3_fsync_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
	struct nfs_cb_data *data;
	struct COMMIT3args args;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	args.file.data.data_len = nfsfh->fh.len;
	args.file.data.data_val = nfsfh->fh.val;
	args.offset = 0;
	args.count = 0;
	if (rpc_nfs3_commit_task(nfs->rpc, nfs3_fsync_cb, &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send COMMIT "
                              "call for %s", data->path);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static void
nfs3_getacl_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	GETACL3res *res;
	GETACL3resok *resok;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
        fattr3_acl acl;
        int i;
        
	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETACL of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}
        memset(&acl, 0, sizeof(acl));
        resok = &res->GETACL3res_u.resok;
        acl.ace_count = resok->ace_count;
        if (acl.ace_count) {
                acl.ace = calloc(acl.ace_count, sizeof(struct nfsacl_ace));
                if (acl.ace == NULL) {
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                        free_nfs_cb_data(data);
                        return;
                }
                for (i = 0; i < acl.ace_count; i++) {
                        acl.ace[i] = resok->ace.ace_val[i];
                }
        }
        acl.default_ace_count = resok->default_ace_count;
        if (acl.default_ace_count) {
                acl.default_ace = calloc(acl.default_ace_count, sizeof(struct nfsacl_ace));
                if (acl.default_ace == NULL) {
                        data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
                        free_nfs_cb_data(data);
                        return;
                }
                for (i = 0; i < acl.default_ace_count; i++) {
                        acl.default_ace[i] = resok->default_ace.default_ace_val[i];
                }
        }
	data->cb(0, nfs, &acl, data->private_data);
	free_nfs_cb_data(data);
}


int
nfs3_getacl_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                  void *private_data)
{
	struct nfs_cb_data *data;
        GETACL3args args;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	memset(&args, 0, sizeof(GETACL3args));
        args.dir.data.data_len = nfsfh->fh.len;
        args.dir.data.data_val = nfsfh->fh.val;
	args.mask = NFSACL_MASK_ACL_ENTRY|NFSACL_MASK_ACL_COUNT|NFSACL_MASK_ACL_DEFAULT_ENTRY|NFSACL_MASK_ACL_DEFAULT_COUNT;
	if (rpc_nfsacl3_getacl_task(nfs->rpc, nfs3_getacl_cb, &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}
        
static void
nfs3_stat_1_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	GETATTR3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
#ifdef WIN32
  struct __stat64 st;
#else
	struct stat st;
#endif

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETATTR of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	st.st_dev     = (dev_t)res->GETATTR3res_u.resok.obj_attributes.fsid;
        st.st_ino     = (ino_t)res->GETATTR3res_u.resok.obj_attributes.fileid;
        st.st_mode    = res->GETATTR3res_u.resok.obj_attributes.mode;
	switch (res->GETATTR3res_u.resok.obj_attributes.type) {
	case NF3REG:
		st.st_mode |= S_IFREG;
		break;
	case NF3DIR:
		st.st_mode |= S_IFDIR;
		break;
	case NF3BLK:
		st.st_mode |= S_IFBLK;
		break;
	case NF3CHR:
		st.st_mode |= S_IFCHR;
		break;
	case NF3LNK:
		st.st_mode |= S_IFLNK;
		break;
	case NF3SOCK:
		st.st_mode |= S_IFSOCK;
		break;
	case NF3FIFO:
		st.st_mode |= S_IFIFO;
		break;
	}
        st.st_nlink   = res->GETATTR3res_u.resok.obj_attributes.nlink;
        st.st_uid     = res->GETATTR3res_u.resok.obj_attributes.uid;
        st.st_gid     = res->GETATTR3res_u.resok.obj_attributes.gid;
	st.st_rdev    = specdata3_to_rdev(&res->GETATTR3res_u.resok.obj_attributes.rdev);
        st.st_size    = res->GETATTR3res_u.resok.obj_attributes.size;
#ifndef WIN32
        st.st_blksize = NFS_BLKSIZE;
	st.st_blocks  = (res->GETATTR3res_u.resok.obj_attributes.used + 512 - 1) / 512;
#endif//WIN32
        st.st_atime   = res->GETATTR3res_u.resok.obj_attributes.atime.seconds;
        st.st_mtime   = res->GETATTR3res_u.resok.obj_attributes.mtime.seconds;
        st.st_ctime   = res->GETATTR3res_u.resok.obj_attributes.ctime.seconds;
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
	st.st_atim.tv_nsec = res->GETATTR3res_u.resok.obj_attributes.atime.nseconds;
	st.st_mtim.tv_nsec = res->GETATTR3res_u.resok.obj_attributes.mtime.nseconds;
	st.st_ctim.tv_nsec = res->GETATTR3res_u.resok.obj_attributes.ctime.nseconds;
#endif

	data->cb(0, nfs, &st, data->private_data);
	free_nfs_cb_data(data);
}

int
nfs3_fstat_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
	struct nfs_cb_data *data;
	struct GETATTR3args args;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = nfsfh->fh.len;
	args.object.data.data_val = nfsfh->fh.val;

	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_stat_1_cb, &args,
                                  data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static void
nfs3_stat64_1_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
	GETATTR3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfs_stat_64 st;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETATTR of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	st.nfs_dev     = res->GETATTR3res_u.resok.obj_attributes.fsid;
        st.nfs_ino     = res->GETATTR3res_u.resok.obj_attributes.fileid;
        st.nfs_mode    = res->GETATTR3res_u.resok.obj_attributes.mode;
	switch (res->GETATTR3res_u.resok.obj_attributes.type) {
	case NF3REG:
		st.nfs_mode |= S_IFREG;
		break;
	case NF3DIR:
		st.nfs_mode |= S_IFDIR;
		break;
	case NF3BLK:
		st.nfs_mode |= S_IFBLK;
		break;
	case NF3CHR:
		st.nfs_mode |= S_IFCHR;
		break;
	case NF3LNK:
		st.nfs_mode |= S_IFLNK;
		break;
	case NF3SOCK:
		st.nfs_mode |= S_IFSOCK;
		break;
	case NF3FIFO:
		st.nfs_mode |= S_IFIFO;
		break;
	}
        st.nfs_nlink   = res->GETATTR3res_u.resok.obj_attributes.nlink;
        st.nfs_uid     = res->GETATTR3res_u.resok.obj_attributes.uid;
        st.nfs_gid     = res->GETATTR3res_u.resok.obj_attributes.gid;
	st.nfs_rdev    = specdata3_to_rdev(&res->GETATTR3res_u.resok.obj_attributes.rdev);
        st.nfs_size    = res->GETATTR3res_u.resok.obj_attributes.size;
	st.nfs_blksize = NFS_BLKSIZE;
	st.nfs_blocks  = (res->GETATTR3res_u.resok.obj_attributes.used + 512 - 1) / 512;
        st.nfs_atime   = res->GETATTR3res_u.resok.obj_attributes.atime.seconds;
        st.nfs_mtime   = res->GETATTR3res_u.resok.obj_attributes.mtime.seconds;
        st.nfs_ctime   = res->GETATTR3res_u.resok.obj_attributes.ctime.seconds;
	st.nfs_atime_nsec = res->GETATTR3res_u.resok.obj_attributes.atime.nseconds;
	st.nfs_mtime_nsec = res->GETATTR3res_u.resok.obj_attributes.mtime.nseconds;
	st.nfs_ctime_nsec = res->GETATTR3res_u.resok.obj_attributes.ctime.nseconds;
	st.nfs_used    = res->GETATTR3res_u.resok.obj_attributes.used;

	data->cb(0, nfs, &st, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_stat64_continue_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	struct GETATTR3args args;

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;

	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_stat64_1_cb,
                                  &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_stat64_async(struct nfs_context *nfs, const char *path,
                  int no_follow, nfs_cb cb, void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, no_follow, cb, private_data,
                                  nfs3_stat64_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}

int
nfs3_fstat64_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                   void *private_data)
{
	struct nfs_cb_data *data;
	struct GETATTR3args args;

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = nfsfh->fh.len;
	args.object.data.data_val = nfsfh->fh.val;

	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_stat64_1_cb, &args,
                                  data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

static int
nfs3_stat_continue_internal(struct nfs_context *nfs,
                            struct nfs_attr *attr _U_,
                            struct nfs_cb_data *data)
{
	struct GETATTR3args args;

	memset(&args, 0, sizeof(GETATTR3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;

	if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_stat_1_cb, &args,
                                  data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

int
nfs3_stat_async(struct nfs_context *nfs, const char *path,
               nfs_cb cb, void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_stat_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_close_cb(int err, struct nfs_context *nfs, void *ret_data,
              void *private_data) {
        struct nfs_cb_data *data = private_data;
        nfs_free_nfsfh(data->nfsfh);
        data->cb(err, nfs, ret_data, data->private_data);
        free_nfs_cb_data(data);
}

int
nfs3_close_async(struct nfs_context *nfs, struct nfsfh *nfsfh, nfs_cb cb,
                 void *private_data)
{
        struct nfs_cb_data *data;

        if (!nfsfh->is_dirty) {
                nfs_free_nfsfh(nfsfh);
                cb(0, nfs, NULL, private_data);
                return 0;
        }

        data = calloc(1, sizeof(struct nfs_cb_data));
        if (data == NULL) {
                nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
                return -1;
        }

        data->nfsfh = nfsfh;
        data->cb = cb;
        data->private_data = private_data;

        return nfs_fsync_async(nfs, nfsfh, nfs3_close_cb, data);
}

static void
nfs3_write_append_cb(struct rpc_context *rpc, int status, void *command_data,
                     void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	GETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: GETATTR failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (nfs3_pwrite_async_internal(nfs, data->nfsfh,
                                       data->usrbuf, data->count, res->GETATTR3res_u.resok.obj_attributes.size, 
                                       data->cb, data->private_data, 1) != 0) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return;
	}
	free_nfs_cb_data(data);
}

static void
nfs3_fill_WRITE3args (WRITE3args *args, struct nfsfh *fh, uint64_t offset,
                      uint64_t count, const void *buf)
{
	memset(args, 0, sizeof(WRITE3args));
	args->file.data.data_len = fh->fh.len;
	args->file.data.data_val = fh->fh.val;
	args->offset = offset;
	args->count  = (count3)count;
	args->stable = fh->is_sync ? FILE_SYNC : UNSTABLE;
	args->data.data_len = (count3)count;
	args->data.data_val = (char *)buf;
}

static void
nfs3_pwrite_mcb(struct rpc_context *rpc, int status, void *command_data,
                void *private_data)
{
	struct nfs_mcb_data *mdata = private_data;
	struct nfs_cb_data *data = mdata->data;
	struct nfs_context *nfs = data->nfs;
	WRITE3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	ATOMIC_DEC(nfs->rpc, data->num_calls);

	/* Flag the failure but do not invoke callback until we have
	 * received all responses.
	 */
	if (status == RPC_STATUS_ERROR) {
                data->error = -EFAULT;
	}
	if (status == RPC_STATUS_CANCEL) {
		data->cancel = 1;
	}
	if (status == RPC_STATUS_TIMEOUT) {
		data->cancel = 1;
	}

	if (status == RPC_STATUS_SUCCESS) {
		res = command_data;
		if (res->status != NFS3_OK) {
			nfs_set_error(nfs, "NFS: Write failed with %s(%d)", nfsstat3_to_str(res->status), nfsstat3_to_errno(res->status));
                        data->error = nfsstat3_to_errno(res->status);
		} else  {
			size_t count = res->WRITE3res_u.resok.count;

			if (count < mdata->count) {
				if (count == 0) {
					nfs_set_error(nfs, "NFS: Write failed. No bytes written!");
                                        data->error = -EFAULT;
				} else {
					/* reissue reminder of this write request */
					WRITE3args args;
					mdata->offset += count;
					mdata->count -= count;

					nfs3_fill_WRITE3args(&args,
                                                             data->nfsfh,
                                                             mdata->offset,
                                                             mdata->count,
                                                             &data->usrbuf[mdata->offset - data->offset]);
					ATOMIC_INC(nfs->rpc, data->num_calls);
					if (rpc_nfs3_write_task(nfs->rpc,
                                                                nfs3_pwrite_mcb,
                                                                &args, mdata)) {
						return;
					} else {
                                                ATOMIC_DEC(nfs->rpc, data->num_calls);
						nfs_set_error(nfs, "RPC error: Failed to send WRITE call for %s", data->path);
						data->oom = 1;
					}
				}
			}
			if (count > 0) {
				if (data->max_offset < mdata->offset + count) {
					data->max_offset = mdata->offset + count;
				}
			}
		}
	}

	free(mdata);

	if (data->num_calls > 0) {
		/* still waiting for more replies */
		return;
	}
	if (data->oom != 0) {
		data->cb(-ENOMEM, nfs, command_data, data->private_data);
		free_nfs_cb_data(data);
		return;
	}
	if (data->error != 0) {
		data->cb(data->error, nfs, command_data, data->private_data);
		free_nfs_cb_data(data);
		return;
	}
	if (data->cancel != 0) {
		data->cb(-EINTR, nfs, "Command was cancelled", data->private_data);
		free_nfs_cb_data(data);
		return;
	}


	if (data->update_pos) {
		data->nfsfh->offset = data->max_offset;
	}

	data->cb((int)(data->max_offset - data->offset), nfs, NULL, data->private_data);

	free_nfs_cb_data(data);
}

int
nfs3_pwrite_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const char *buf, size_t count, uint64_t offset,
                           nfs_cb cb, void *private_data, int update_pos)
{
	struct nfs_cb_data *data;

        if (count > nfs_get_writemax(nfs)) {
                count = nfs_get_writemax(nfs);
        }
        
        nfsfh->is_dirty = 1;
	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;
	data->nfsfh        = nfsfh;
	data->usrbuf       = buf;
	data->update_pos   = update_pos;

	/* hello, clang-analyzer */
	assert(data->num_calls == 0);

	/* chop requests into chunks of at most WRITEMAX bytes if necessary.
	 * we send all writes in parallel so that performance is still good.
	 */
	data->max_offset = offset;
	data->offset = offset;
	data->count = count;

	do {
		size_t writecount = count;
		struct nfs_mcb_data *mdata;
		WRITE3args args;

		if (writecount > nfs_get_writemax(nfs)) {
		  writecount = nfs_get_writemax(nfs);
		}

		mdata = calloc(1, sizeof(struct nfs_mcb_data));
		if (mdata == NULL) {
			nfs_set_error(nfs, "out of memory: failed to allocate "
                                      "nfs_mcb_data structure");
			if (data->num_calls == 0) {
				free_nfs_cb_data(data);
				return -1;
			}
			data->oom = 1;
			break;
		}
		mdata->data   = data;
		mdata->offset = offset;
		mdata->count  = writecount;

		nfs3_fill_WRITE3args(&args, nfsfh, offset, writecount,
                                     &buf[offset - data->offset]);
                ATOMIC_INC(nfs->rpc, data->num_calls);
		if (rpc_nfs3_write_task(nfs->rpc, nfs3_pwrite_mcb,
                                        &args, mdata) == NULL) {
                        ATOMIC_DEC(nfs->rpc, data->num_calls);
			nfs_set_error(nfs, "RPC error: Failed to send WRITE "
                                      "call for %s", data->path);
			free(mdata);
			if (data->num_calls == 0) {
				free_nfs_cb_data(data);
				return -1;
			}
			data->oom = 1;
			break;
		}

		count               -= writecount;
		offset              += writecount;
	} while (count > 0);

	return 0;
}

int
nfs3_write_async(struct nfs_context *nfs, struct nfsfh *nfsfh,
                 const void *buf, size_t count,
                 nfs_cb cb, void *private_data)
{
	if (nfsfh->is_append) {
		struct GETATTR3args args;
		struct nfs_cb_data *data;

		data = calloc(1, sizeof(struct nfs_cb_data));
		if (data == NULL) {
			nfs_set_error(nfs, "Out of memory.");
			return -1;
		}
		data->nfs           = nfs;
		data->cb            = cb;
		data->private_data  = private_data;
		data->nfsfh         = nfsfh;
		data->usrbuf	    = buf;
		data->count         = (size_t)count;

		memset(&args, 0, sizeof(GETATTR3args));
		args.object.data.data_len = nfsfh->fh.len;
		args.object.data.data_val = nfsfh->fh.val;

		if (rpc_nfs3_getattr_task(nfs->rpc, nfs3_write_append_cb,
                                          &args, data) == NULL) {
			free_nfs_cb_data(data);
			return -1;
		}
		return 0;
	}
	return nfs3_pwrite_async_internal(nfs, nfsfh,
                                          buf, count, nfsfh->offset,
                                          cb, private_data, 1);
}

static void
nfs3_fill_READ3args(READ3args *args, struct nfsfh *fh, uint64_t offset,
                    uint64_t count)
{
	memset(args, 0, sizeof(READ3args));
	args->file.data.data_len = fh->fh.len;
	args->file.data.data_val = fh->fh.val;
	args->offset = offset;
	args->count = (count3)count;
}

static void
nfs3_pread_cb(struct rpc_context *rpc, int status, void *command_data,
               void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	READ3res *res;
	int count;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
        if (res->status != NFS3_OK) {
                nfs_set_error(nfs, "NFS: Read failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs, command_data, data->private_data);
		free_nfs_cb_data(data);
		return;
        }

        count = (int)res->READ3res_u.resok.count;
        if (data->update_pos) {
                data->nfsfh->offset = data->offset + count;
        }

        if (count > rpc->pdu->requested_read_count) {
                count = rpc->pdu->requested_read_count;
        }

	data->cb(count, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);
	return;
}

int
nfs3_pread_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                          void *buf, size_t count, uint64_t offset,
                          nfs_cb cb, void *private_data, int update_pos)
{
	struct nfs_cb_data *data;
        READ3args args;
        struct rpc_pdu *pdu;

        if (count > nfs_get_readmax(nfs)) {
                count = nfs_get_readmax(nfs);
        }
        
	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;
	data->nfsfh        = nfsfh;
	data->org_offset   = offset;
	data->org_count    = count;
	data->update_pos   = update_pos;

	assert(data->num_calls == 0);

	data->offset = offset;
	data->count = (count3)count;
	data->max_offset = data->offset;

        nfs3_fill_READ3args(&args, nfsfh, offset, count);
        pdu = rpc_nfs3_read_task(nfs->rpc, nfs3_pread_cb, buf, count, &args, data);
        if (pdu == NULL) {
                nfs_set_error(nfs, "RPC error: Failed to send READ "
                              "call for %s", data->path);
                free_nfs_cb_data(data);
                return -1;
        }

        return 0;
}

int
nfs3_preadv_async_internal(struct nfs_context *nfs, struct nfsfh *nfsfh,
                           const struct iovec *iov, int iovcnt, uint64_t offset,
                           nfs_cb cb, void *private_data, int update_pos)
{
	struct nfs_cb_data *data;
        READ3args args;
        struct rpc_pdu *pdu;
        size_t count = 0;
        int i;

        for (i = 0; i < iovcnt; i++) {
                count += iov[i].iov_len;
        }
        if (count > nfs_get_readmax(nfs)) {
                count = nfs_get_readmax(nfs);
        }

	data = calloc(1, sizeof(struct nfs_cb_data));
	if (data == NULL) {
		nfs_set_error(nfs, "out of memory: failed to allocate "
                              "nfs_cb_data structure");
		return -1;
	}
	data->nfs          = nfs;
	data->cb           = cb;
	data->private_data = private_data;
	data->nfsfh        = nfsfh;
	data->org_offset   = offset;
	data->org_count    = count;
	data->update_pos   = update_pos;

	assert(data->num_calls == 0);

	data->offset = offset;
	data->count = (count3)count;
	data->max_offset = data->offset;

        nfs3_fill_READ3args(&args, nfsfh, offset, count);
        pdu = rpc_nfs3_readv_task(nfs->rpc, nfs3_pread_cb, iov, iovcnt, &args, data);
        if (pdu == NULL) {
                nfs_set_error(nfs, "RPC error: Failed to send READ "
                              "call for %s", data->path);
                free_nfs_cb_data(data);
                return -1;
        }

        return 0;
}

static int
nfs3_chdir_continue_internal(struct nfs_context *nfs,
                             struct nfs_attr *attr _U_,
                             struct nfs_cb_data *data)
{
	/* steal saved_path */
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_lock(&nfs->rpc->rpc_mutex);
        }
#endif
	free(nfs->nfsi->cwd);
        nfs->nfsi->cwd = data->saved_path;
#ifdef HAVE_MULTITHREADING
        if (nfs->rpc->multithreading_enabled) {
                nfs_mt_mutex_unlock(&nfs->rpc->rpc_mutex);
        }
#endif

	data->saved_path = NULL;

	data->cb(0, nfs, NULL, data->private_data);
	free_nfs_cb_data(data);

	return 0;
}

int
nfs3_chdir_async(struct nfs_context *nfs, const char *path,
                 nfs_cb cb, void *private_data)
{
	if (nfs3_lookuppath_async(nfs, path, 0, cb, private_data,
                                  nfs3_chdir_continue_internal,
                                  NULL, NULL, 0) != 0) {
		return -1;
	}

	return 0;
}

static void
nfs3_open_trunc_cb(struct rpc_context *rpc, int status, void *command_data,
                   void *private_data)
{
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfsfh *nfsfh;
	SETATTR3res *res;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: Setattr failed with %s(%d)",
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfsfh = calloc(1, sizeof(struct nfsfh));
	if (nfsfh == NULL) {
		nfs_set_error(nfs, "NFS: Failed to allocate nfsfh "
                              "structure");
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (data->continue_int & O_SYNC) {
		nfsfh->is_sync = 1;
	}
	if (data->continue_int & O_APPEND) {
		nfsfh->is_append = 1;
	}

	/* steal the filehandle */
	nfsfh->fh = data->fh;
	data->fh.val = NULL;

	data->cb(0, nfs, nfsfh, data->private_data);
	free_nfs_cb_data(data);
}

static void
nfs3_open_cb(struct rpc_context *rpc, int status, void *command_data,
             void *private_data)
{
	ACCESS3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct nfsfh *nfsfh;
	unsigned int nfsmode = 0;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	res = command_data;
	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: ACCESS of %s failed with %s(%d)",
                              data->saved_path, nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (data->continue_int & O_WRONLY) {
		nfsmode |= ACCESS3_MODIFY;
	}
	if (data->continue_int & O_RDWR) {
		nfsmode |= ACCESS3_READ|ACCESS3_MODIFY;
	}
	if (!(data->continue_int & (O_WRONLY|O_RDWR))) {
		nfsmode |= ACCESS3_READ;
	}


	if (res->ACCESS3res_u.resok.access != nfsmode) {
		nfs_set_error(nfs, "NFS: ACCESS denied. Required "
                              "access %c%c%c. Allowed access %c%c%c",
                              nfsmode&ACCESS3_READ?'r':'-',
                              nfsmode&ACCESS3_MODIFY?'w':'-',
                              nfsmode&ACCESS3_EXECUTE?'x':'-',
                              res->ACCESS3res_u.resok.access&ACCESS3_READ ? 'r':'-',
                              res->ACCESS3res_u.resok.access&ACCESS3_MODIFY ?'w':'-',
                              res->ACCESS3res_u.resok.access&ACCESS3_EXECUTE ?'x':'-');
		data->cb(-EACCES, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	/* Try to truncate it if we were requested to */
	if ((data->continue_int & O_TRUNC) &&
	    (data->continue_int & (O_RDWR|O_WRONLY))) {
		SETATTR3args args;

		memset(&args, 0, sizeof(SETATTR3args));
		args.object.data.data_len = data->fh.len;
		args.object.data.data_val = data->fh.val;
		args.new_attributes.size.set_it = 1;
		args.new_attributes.size.set_size3_u.size = 0;

		if (rpc_nfs3_setattr_task(nfs->rpc, nfs3_open_trunc_cb, &args,
                                          data) == NULL) {
			nfs_set_error(nfs, "RPC error: Failed to send "
                                      "SETATTR call for %s", data->path);
			data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                                 data->private_data);
			free_nfs_cb_data(data);
			return;
		}
		return;
	}

	nfsfh = calloc(1, sizeof(struct nfsfh));
	if (nfsfh == NULL) {
		nfs_set_error(nfs, "NFS: Failed to allocate nfsfh structure");
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (data->continue_int & O_SYNC) {
		nfsfh->is_sync = 1;
	}
	if (data->continue_int & O_APPEND) {
		nfsfh->is_append = 1;
	}
	if (!(data->continue_int & (O_WRONLY|O_RDWR))) {
		nfsfh->is_readonly = 1;
	}

	/* steal the filehandle */
	nfsfh->fh = data->fh;
	data->fh.val = NULL;

	data->cb(0, nfs, nfsfh, data->private_data);
	free_nfs_cb_data(data);
}

static int
nfs3_open_continue_internal(struct nfs_context *nfs,
                            struct nfs_attr *attr _U_,
                            struct nfs_cb_data *data)
{
	int nfsmode = 0;
	ACCESS3args args;

        if ((data->continue_int & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL)) {
                nfs_set_error(nfs, "File allready exist when opening with O_CREAT|O_EXCL");
		data->cb(-EEXIST, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return -1;
        }
	if (data->continue_int & O_WRONLY) {
		nfsmode |= ACCESS3_MODIFY;
	}
	if (data->continue_int & O_RDWR) {
		nfsmode |= ACCESS3_READ|ACCESS3_MODIFY;
	}
	if (!(data->continue_int & (O_WRONLY|O_RDWR))) {
		nfsmode |= ACCESS3_READ;
	}

	memset(&args, 0, sizeof(ACCESS3args));
	args.object.data.data_len = data->fh.len;
	args.object.data.data_val = data->fh.val;
	args.access = nfsmode;

	if (rpc_nfs3_access_task(nfs->rpc, nfs3_open_cb, &args, data) == NULL) {
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
				data->private_data);
		free_nfs_cb_data(data);
		return -1;
	}
	return 0;
}

struct open_cb_data {
        nfs_cb cb;
        void *private_data;
        char *path;
        int flags;
        int mode;
};

static void
free_open_cb_data(void *ptr)
{
	struct open_cb_data *data = ptr;

	free(data->path);
	free(data);
}



static void nfs3_open_create_cb(int err, struct nfs_context *nfs, void *ret_data,
                         void *private_data)
{
	struct open_cb_data *cb_data = private_data;

        if (err) {
		cb_data->cb(nfsstat3_to_errno(err), nfs,
                         nfs_get_error(nfs), cb_data->private_data);
                free_open_cb_data(cb_data);
                return;
        }
        
        cb_data->cb(0, nfs, ret_data, cb_data->private_data);
        free_open_cb_data(cb_data);
}

static void
nfs3_create_1_cb(struct rpc_context *rpc, int status, void *command_data,
                 void *private_data)
{
	CREATE3res *res;
	struct nfs_cb_data *data = private_data;
	struct nfs_context *nfs = data->nfs;
	struct open_cb_data *cb_data = data->continue_data;
	struct nfsfh *nfsfh;

	assert(rpc->magic == RPC_CONTEXT_MAGIC);

	res = command_data;
	if (check_nfs3_error(nfs, status, data, command_data)) {
		free_nfs_cb_data(data);
		return;
	}

	if (res->status != NFS3_OK) {
		nfs_set_error(nfs, "NFS: CREATE3 of %s failed with "
                              "%s(%d)", data->saved_path,
                              nfsstat3_to_str(res->status),
                              nfsstat3_to_errno(res->status));
		data->cb(nfsstat3_to_errno(res->status), nfs,
                         nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	nfsfh = calloc(1, sizeof(struct nfsfh));
	if (nfsfh == NULL) {
		nfs_set_error(nfs, "NFS: Failed to allocate nfsfh structure");
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_nfs_cb_data(data);
		return;
	}

	if (cb_data->flags & O_SYNC) {
		nfsfh->is_sync = 1;
	}
	if (cb_data->flags & O_APPEND) {
		nfsfh->is_append = 1;
	}

	/* copy the filehandle */
	nfsfh->fh.len = res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_len;
	nfsfh->fh.val = malloc(nfsfh->fh.len);
	if (nfsfh->fh.val == NULL) {
		nfs_set_error(nfs, "Out of memory: Failed to allocate "
                              "fh structure");
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs), data->private_data);
		free_nfs_cb_data(data);
		free(nfsfh);
		return;
	}
	memcpy(nfsfh->fh.val,
               res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_val,
               nfsfh->fh.len);

        
	nfs_dircache_drop(nfs, &data->fh);
	data->cb(0, nfs, nfsfh, data->private_data);
        free_nfs_cb_data(data);
        return;
}

static int
nfs3_create_continue_internal(struct nfs_context *nfs,
                              struct nfs_attr *attr _U_,
                              struct nfs_cb_data *data)
{
	struct open_cb_data *cb_data = data->continue_data;
	char *str = cb_data->path;
	CREATE3args args;

	str = &str[strlen(str) + 1];

	memset(&args, 0, sizeof(CREATE3args));
	args.where.dir.data.data_len = data->fh.len;
	args.where.dir.data.data_val = data->fh.val;
	args.where.name = str;
	args.how.mode = (cb_data->flags & O_EXCL) ? GUARDED : UNCHECKED;
	args.how.createhow3_u.obj_attributes.mode.set_it = 1;
	args.how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = cb_data->mode;

	if (rpc_nfs3_create_task(nfs->rpc, nfs3_create_1_cb,
                                 &args, data) == NULL) {
		nfs_set_error(nfs, "RPC error: Failed to send CREATE "
                              "call for %s/%s", data->path, str);
		data->cb(-ENOMEM, nfs, nfs_get_error(nfs),
                         data->private_data);
		free_open_cb_data(data);
		return -1;
	}
	return 0;
}

static void nfs3_initial_open_cb(int err, struct nfs_context *nfs, void *ret_data,
                         void *private_data)
{
	struct open_cb_data *cb_data = private_data;
        char *ptr;

        if (err == -EEXIST && (cb_data->flags & (O_CREAT|O_EXCL)) == (O_CREAT|O_EXCL)) {
		cb_data->cb(-EEXIST, nfs,
                         nfs_get_error(nfs), cb_data->private_data);
                free_open_cb_data(cb_data);
                return;
        }
        if (err == -NFS3ERR_NOENT && (cb_data->flags & O_CREAT)) {
                ptr = strrchr(cb_data->path, '/');
                if (ptr) {
                        *ptr++ = 0;
                } else {
                        /*
                         * We have a simple path to a top level name and no
                         * leading slashes. Make room for an extra character so
                         * we can create a path that is '\0' and then followed
                         * by the object we wish to create.
                         */
                        ptr = calloc(1, strlen(cb_data->path) + 2);
                        if (ptr == NULL) {
                                cb_data->cb(-ENOMEM, nfs,
                                            nfs_get_error(nfs),
                                            cb_data->private_data);
                                free_open_cb_data(cb_data);
                                return;
                        }
                        ptr[0] = 0;
                        memcpy(&ptr[1], cb_data->path, strlen(cb_data->path));
                        free(cb_data->path);
                        cb_data->path = ptr;
                }
                /*
                 * New_path now points to the parent directory and beyond the
                 * null terminator is the new object to create
                 */
                if (nfs3_lookuppath_async(nfs, cb_data->path, 0,
                                          nfs3_open_create_cb, cb_data,
                                          nfs3_create_continue_internal, cb_data,
                                          NULL, 0) != 0) {
                        cb_data->cb(-ENOMEM, nfs,
                                    nfs_get_error(nfs), cb_data->private_data);
                        free_open_cb_data(cb_data);
                        return;
                }
                return;
        }
        if (err) {
                cb_data->cb(nfsstat3_to_errno(err), nfs,
                            nfs_get_error(nfs), cb_data->private_data);
                free_open_cb_data(cb_data);
                return;
        }
        cb_data->cb(0, nfs, ret_data, cb_data->private_data);
        free_open_cb_data(cb_data);
}

/* TODO add the plumbing for mode */
int
nfs3_open_async(struct nfs_context *nfs, const char *path, int flags,
                int mode, nfs_cb cb, void *private_data)
{
	struct open_cb_data *cb_data;

	cb_data = calloc(1, sizeof(struct open_cb_data));
	if (cb_data == NULL) {
		nfs_set_error(nfs, "Out of memory: failed to allocate "
			"nfs_cb_data structure");
		return -ENOMEM;
	}
        cb_data->path               = strdup(path);
        if (cb_data->path == NULL) {
		nfs_set_error(nfs, "Out of memory: failed to strup path");
                free_open_cb_data(cb_data);
		return -ENOMEM;
        }
	cb_data->cb                 = cb;
	cb_data->private_data       = private_data;
        cb_data->flags              = flags;
        cb_data->mode               = mode;

	if (nfs3_lookuppath_async(nfs, path, 0, nfs3_initial_open_cb, cb_data,
                                  nfs3_open_continue_internal, NULL,
                                  NULL, flags) != 0) {
                free_open_cb_data(cb_data);
		return -1;
	}
	return 0;
}

