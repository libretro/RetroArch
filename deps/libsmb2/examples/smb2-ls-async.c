/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include <inttypes.h>
#if !defined(__amigaos4__) && !defined(__AMIGA__) && !defined(__AROS__)
#include <poll.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

#ifdef __AROS__
#include "asprintf.h"
#endif

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
struct pollfd {
        int fd;
        short events;
        short revents;
};

int poll(struct pollfd *fds, unsigned int nfds, int timo);
#endif

int is_finished;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-ls-async <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>>[:<port>]/<share>/<path>\n");	
        exit(1);
}

void dc_cb(struct smb2_context *smb2, int status,
                void *command_data _U_, void *private_data)
{
        is_finished = 1;
}

void od_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2dir *dir = command_data;
        struct smb2dirent *ent;

        if (status) {
                printf("failed to create/open directory (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        while ((ent = smb2_readdir(smb2, dir))) {
                char *type;
                time_t t;

                switch (ent->st.smb2_type) {
                case SMB2_TYPE_LINK:
                        type = "LINK";
                        break;
                case SMB2_TYPE_FILE:
                        type = "FILE";
                        break;
                case SMB2_TYPE_DIRECTORY:
                        type = "DIRECTORY";
                        break;
                default:
                        type = "unknown";
                        break;
                }
                t = (time_t)ent->st.smb2_mtime;
	        printf("%-20s %-9s %15"PRIu64" %s\n", ent->name, type, ent->st.smb2_size, asctime(localtime(&t)));
        }

        smb2_closedir(smb2, dir);
        smb2_disconnect_share_async(smb2, dc_cb, NULL);
}

void cf_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        if (status) {
                printf("failed to connect share (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        if (smb2_opendir_async(smb2, private_data, od_cb, NULL) < 0) {
                printf("Failed to call opendir_async()\n");
                exit(10);
        }
}

static int cfd = -1;

void fd_cb(struct smb2_context *smb2, int fd, int cmd)
{
        if (cmd == SMB2_ADD_FD) {
                cfd = fd;
        }
        if (cmd == SMB2_DEL_FD) {
                cfd = -1;
        }
}

static int cevents = 0;

void events_cb(struct smb2_context *smb2, int fd, int events)
{
        cevents = events;
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
	struct pollfd pfd;

        if (argc < 2) {
                usage();
        }

	smb2 = smb2_init_context();
        if (smb2 == NULL) {
                fprintf(stderr, "Failed to init context\n");
                exit(0);
        }
        smb2_fd_event_callbacks(smb2, fd_cb, events_cb);

        url = smb2_parse_url(smb2, argv[1]);
        if (url == NULL) {
                fprintf(stderr, "Failed to parse url: %s\n",
                        smb2_get_error(smb2));
                exit(0);
        }

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);
	if (smb2_connect_share_async(smb2, url->server, url->share, url->user, cf_cb, (void *)url->path) != 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        while (!is_finished) {
		pfd.fd = cfd;
		pfd.events = cevents;

		if (poll(&pfd, 1, 1000) < 0) {
			printf("Poll failed");
			exit(10);
		}
                if (pfd.revents == 0) {
                        continue;
                }
		if (smb2_service(smb2, pfd.revents) < 0) {
			printf("smb2_service failed with : %s\n",
                               smb2_get_error(smb2));
			break;
		}
	}

        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
