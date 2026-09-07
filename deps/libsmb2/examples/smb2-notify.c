/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2024 by Tryggvi Larusson <tryggvi.larusson@netapp.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

int info_level;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-notify <smb2-url> <sync|async>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/<share>/<path>\n");
        exit(1);
}

static int wait_for_reply(struct smb2_context *smb2,
                          struct sync_cb_data *cb_data)
{
        while (!cb_data->is_finished) {
		struct pollfd pfd;
		memset(&pfd, 0, sizeof(struct pollfd));
		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

		if (poll(&pfd, 1, 1000) < 0) {
			smb2_set_error(smb2, "Poll failed");
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

static void notify_async_test_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->status = status;

        cb_data->ptr = command_data;

        struct smb2_file_notify_change_information *fnc = cb_data->ptr;
        if (fnc) {
                printf("async notify info: %s %d\n", fnc->name, fnc->action);
                free_smb2_file_notify_change_information(smb2, fnc);
        } else {
                printf("async notify info is null \n");
        }
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
        int is_sync = 1;

        if (argc < 2) {
                usage();
        } else if (argc == 3) {
                char *sync_mode = argv[2];
                if (strcmp(sync_mode, "sync") == 0) {
                        is_sync = 1;
                } else if (strcmp(sync_mode, "async") == 0) {
                        is_sync = 0;
                }
        }

	smb2 = smb2_init_context();
        if (smb2 == NULL) {
                fprintf(stderr, "Failed to init context\n");
                exit(0);
        }

        url = smb2_parse_url(smb2, argv[1]);
        if (url == NULL) {
                fprintf(stderr, "Failed to parse url: %s\n",
                        smb2_get_error(smb2));
                exit(0);
        }

        if (url->user) {
                smb2_set_user(smb2, url->user);
        }

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);
	if (smb2_connect_share(smb2, url->server, url->share, url->user) != 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        uint16_t flags = SMB2_CHANGE_NOTIFY_WATCH_TREE;
        uint32_t completion_filter = SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_FILE_NAME 
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_DIR_NAME
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_ATTRIBUTES
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_SIZE
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_LAST_WRITE
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_LAST_ACCESS
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_CREATION
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_EA
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_SECURITY
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_NAME
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_SIZE
                | SMB2_CHANGE_NOTIFY_FILE_NOTIFY_CHANGE_STREAM_WRITE;


        // sync one-off version
        if (is_sync) {
                struct smb2_file_notify_change_information *fnc = smb2_notify_change(smb2, url->path, flags, completion_filter);
                if (fnc) {
                        printf("sync got notify info: %s %d\n", fnc->name, fnc->action);
                        free_smb2_file_notify_change_information(smb2, fnc);
                } else {
                        printf("notify info is null \n");
                }
        // async version - sends a new notify request after each is finished
        } else {
                struct sync_cb_data cb_data;
                memset(&cb_data, 0, sizeof(cb_data));
                // if execute_in_loop is 1 a new new notify change request is sent on completion of each previous call
                int execute_in_loop = 1;
                if (smb2_notify_change_async(smb2, url->path, flags, completion_filter, execute_in_loop, 
                                notify_async_test_cb, &cb_data) != 0) {
                        printf("sending notify change failed. %s\n",
                        smb2_get_error(smb2));
                        exit(10);
                }

                if (wait_for_reply(smb2, &cb_data) < 0) {
                        printf("failed waiting for a reply. %s\n",
                        smb2_get_error(smb2));
                        exit(10);
                }

                if (cb_data.status != 0) {
                        printf("notify command sequence returned error 0x%08x\n",
                        cb_data.status);
                        exit(10);
                }
        }

        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
