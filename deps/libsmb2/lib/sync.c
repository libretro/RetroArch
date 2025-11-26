/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "compat.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdio.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

static int wait_for_reply(struct smb2_context *smb2,
                          struct sync_cb_data *cb_data)
{
        time_t t = time(NULL);

        while (!cb_data->is_finished) {
		struct pollfd pfd;
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

		if (poll(&pfd, 1, 1000) < 0) {
			smb2_set_error(smb2, "Poll failed");
			return -1;
		}
                if (smb2->timeout) {
                        smb2_timeout_pdus(smb2);
                }
		if (!SMB2_VALID_SOCKET(smb2->fd) && ((time(NULL) - t) > (smb2->timeout)))
		{
			smb2_set_error(smb2, "Timeout expired and no connection exists\n");
			return -1;
		}                
                if (pfd.revents == 0) {
                        continue;
                }
		if (smb2_service(smb2, pfd.revents) < 0) {
			smb2_set_error(smb2, "smb2_service failed with : "
                                        "%s\n", smb2_get_error(smb2));
                        return -1;
		}
	}

        return 0;
}

static void connect_cb(struct smb2_context *smb2, int status,
                       void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                if (cb_data != &smb2->connect_cb_data) {
                        free(cb_data);
                }
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
}

/*
 * Connect to the server and mount the share.
 */
int smb2_connect_share(struct smb2_context *smb2,
                       const char *server,
                       const char *share,                      
                       const char *user)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = &smb2->connect_cb_data;
	rc = smb2_connect_share_async(smb2, server, share, user, connect_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:

	return rc;
}

/*
 * Disconnect from share
 */
int smb2_disconnect_share(struct smb2_context *smb2)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = &smb2->connect_cb_data;

	rc = smb2_disconnect_share_async(smb2, connect_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:

	return rc;
}

/*
 * opendir()
 */
static void opendir_cb(struct smb2_context *smb2, int status,
                       void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (status == SMB2_STATUS_SHUTDOWN) {
                return;
        }
        if (status) {
                cb_data->status = status;
        }
        cb_data->is_finished = 1;
        cb_data->ptr = command_data;
}

struct smb2dir *smb2_opendir(struct smb2_context *smb2, const char *path)
{
        struct smb2_pdu *pdu;
        struct sync_cb_data *cb_data;
        struct smb2dir *dir;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return NULL;
        }

	pdu = smb2_opendir_async_pdu(smb2, path, opendir_cb, cb_data, NULL);
        if (pdu == NULL) {
		smb2_set_error(smb2, "smb2_opendir_async failed");
                free(cb_data);
		return NULL;
	}

	if (wait_for_reply(smb2, cb_data) < 0) {
                free(cb_data);
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

	dir = cb_data->ptr;
        if (dir) {
                /* Give ownership of cb_data to dir. It will be freed when dir is freed */
                dir->free_cb_data = free;
        } else {
                free(cb_data);
        }
        smb2_free_pdu(smb2, pdu);
        return dir;
}

/*
 * open()
 */
static void open_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->ptr = command_data;
}

struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags)
{
        struct smb2_pdu *pdu;
        struct sync_cb_data *cb_data;
        void *ptr;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return NULL;
        }

        /* pdu takes ownership of cb_data and will free it when the pdu is freed */
	pdu = smb2_open_async_pdu(smb2, path, flags, open_cb, cb_data, free);
        if (pdu == NULL) {
		smb2_set_error(smb2, "smb2_open_async failed");
                free(cb_data);
		return NULL;
	}

	if (wait_for_reply(smb2, cb_data) < 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

	ptr = cb_data->ptr;
        cb_data->ptr = NULL;
        smb2_free_pdu(smb2, pdu);
        return ptr;
}

/*
 * close()
 */
static void close_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (status == SMB2_STATUS_SHUTDOWN) {
                return;
        }
        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                free(cb_data);
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_close(struct smb2_context *smb2, struct smb2fh *fh)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_close_async(smb2, fh, close_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                goto out;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

/*
 * fsync()
 */
static void fsync_cb(struct smb2_context *smb2, int status,
                     void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                free(cb_data);
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_fsync(struct smb2_context *smb2, struct smb2fh *fh)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_fsync_async(smb2, fh, fsync_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

/*
 * pread()
 */
static void generic_status_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                free(cb_data);
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
}

int smb2_pread(struct smb2_context *smb2, struct smb2fh *fh,
               uint8_t *buf, uint32_t count, uint64_t offset)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }
        
	rc = smb2_pread_async(smb2, fh, buf, count, offset,
                              generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_pwrite(struct smb2_context *smb2, struct smb2fh *fh,
                const uint8_t *buf, uint32_t count, uint64_t offset)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_pwrite_async(smb2, fh, buf, count, offset,
                               generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

        rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_read(struct smb2_context *smb2, struct smb2fh *fh,
              uint8_t *buf, uint32_t count)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_read_async(smb2, fh, buf, count,
                             generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

        rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_write(struct smb2_context *smb2, struct smb2fh *fh,
               const uint8_t *buf, uint32_t count)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }
        
	rc = smb2_write_async(smb2, fh, buf, count,
                              generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_unlink(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_unlink_async(smb2, path,
                               generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_rmdir(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }
        
	rc = smb2_rmdir_async(smb2, path,
                              generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_mkdir(struct smb2_context *smb2, const char *path)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_mkdir_async(smb2, path,
                              generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_fstat(struct smb2_context *smb2, struct smb2fh *fh,
               struct smb2_stat_64 *st)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_fstat_async(smb2, fh, st,
                              generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_stat(struct smb2_context *smb2, const char *path,
              struct smb2_stat_64 *st)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_stat_async(smb2, path, st,
                             generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_rename(struct smb2_context *smb2, const char *oldpath,
                const char *newpath)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_rename_async(smb2, oldpath, newpath,
                               generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_statvfs(struct smb2_context *smb2, const char *path,
                 struct smb2_statvfs *st)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_statvfs_async(smb2, path, st,
                                generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_truncate(struct smb2_context *smb2, const char *path,
                  uint64_t length)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_truncate_async(smb2, path, length,
                                 generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

int smb2_ftruncate(struct smb2_context *smb2, struct smb2fh *fh,
                   uint64_t length)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

	rc = smb2_ftruncate_async(smb2, fh, length,
                                  generic_status_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

struct readlink_cb_data {
	char *buf;
        int len;
};

static void readlink_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;
        struct readlink_cb_data *rl_data = cb_data->ptr;
        
        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                free(cb_data);
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
        strncpy(rl_data->buf, command_data, rl_data->len);
}

int smb2_readlink(struct smb2_context *smb2, const char *path,
                  char *buf, uint32_t len)
{
        struct sync_cb_data *cb_data;
        struct readlink_cb_data rl_data _U_;
        int rc = 0;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

        rl_data.buf = buf;
        rl_data.len = len;

        cb_data->ptr = &rl_data;

	rc = smb2_readlink_async(smb2, path, readlink_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

static void echo_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                free(cb_data);
                return;
        }

        cb_data->is_finished = 1;
        cb_data->status = status;
}

/*
 * Send SMB2_ECHO command to the server
 */
int smb2_echo(struct smb2_context *smb2)
{
        struct sync_cb_data *cb_data;
        int rc = 0;

        if (!SMB2_VALID_SOCKET(smb2->fd)) {
                smb2_set_error(smb2, "Not Connected to Server");
                return -ENOMEM;
        }

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return -ENOMEM;
        }

        rc = smb2_echo_async(smb2, echo_cb, cb_data);
        if (rc < 0) {
                goto out;
	}

	rc = wait_for_reply(smb2, cb_data);
        if (rc < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                return rc;
	}

        rc = cb_data->status;
 out:
        free(cb_data);

	return rc;
}

static void sync_notify_change_cb(struct smb2_context *smb2, int status,
                       void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        if (cb_data->status == SMB2_STATUS_CANCELLED) {
                return;
        }

        cb_data->is_finished = 1;
        cb_data->ptr = command_data;
}


/**
 * One-off sync command for getting notify change response
 */
struct smb2_file_notify_change_information *smb2_notify_change(struct smb2_context *smb2, const char *path, uint16_t flags, uint32_t filter)
{
        struct sync_cb_data *cb_data;
        void *ptr;

        cb_data = calloc(1, sizeof(struct sync_cb_data));
        if (cb_data == NULL) {
                smb2_set_error(smb2, "Failed to allocate sync_cb_data");
                return NULL;
        }

	if (smb2_notify_change_async(smb2, path, flags, filter, 0,
                               sync_notify_change_cb, cb_data) != 0) {
		smb2_set_error(smb2, "smb2_notify_change failed");
                free(cb_data);
		return NULL;
	}

	if (wait_for_reply(smb2, cb_data) < 0) {
                cb_data->status = SMB2_STATUS_CANCELLED;
                free(cb_data);
                return NULL;
        }

	ptr = cb_data->ptr;
        free(cb_data);
        return ptr;
}
