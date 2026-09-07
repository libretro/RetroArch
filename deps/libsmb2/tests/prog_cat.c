/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include <fcntl.h>
#if !defined(__amigaos4__) && !defined(__AMIGA__) && !defined(__AROS__)
#include <poll.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
struct pollfd {
        int fd;
        short events;
        short revents;
};

int poll(struct pollfd *fds, unsigned int nfds, int timo);
#endif

int is_finished = 0;
uint8_t buf[256 * 1024];
uint32_t pos;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-cat-async <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/<share>/<path>\n");
        exit(1);
}

void dc_cb(struct smb2_context *smb2, int status,
                void *command_data _U_, void *private_data)
{
        is_finished = 1;
}

void cl_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        smb2_disconnect_share_async(smb2, dc_cb, NULL);
}

void pr_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2fh *fh = private_data;

        if (status < 0) {
                printf("failed to read file (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                is_finished = 1;
                return;
        }

        if (status == 0) {
                if (smb2_close_async(smb2, fh, cl_cb, NULL) < 0) {
                        printf("Failed to call smb2_close_async()\n");
                        is_finished = 1;
                }
                return;
        }

        write(STDOUT_FILENO, buf, status);

        pos += status;
        if (smb2_pread_async(smb2, fh, buf, 102400, pos, pr_cb, fh) < 0) {
                printf("Failed to call smb2_pread_async()\n");
                is_finished = 1;
                return;
        }
}

void of_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2fh *fh = command_data;

        if (status) {
                printf("failed to open file (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                is_finished = 1;
                return;
        }

        if (smb2_pread_async(smb2, fh, buf, 102400, 0, pr_cb, fh) < 0) {
                printf("Failed to call smb2_pread_async()\n");
                is_finished = 1;
                return;
        }
}

void cf_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        if (status) {
                printf("failed to connect share (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                is_finished = 1;
                return;
        }

        if (smb2_open_async(smb2, private_data, O_RDONLY,
                            of_cb, NULL) < 0) {
                printf("Failed to call smb2_open_async()\n");
                is_finished = 1;
                return;
        }
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
	struct pollfd pfd;
        int rc = 0;

        if (argc < 2) {
                usage();
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

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);
	if (smb2_connect_share_async(smb2, url->server, url->share, url->user,
                                     cf_cb, (void *)url->path) != 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
                goto finished;
	}

        while (!is_finished) {
		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

		if (poll(&pfd, 1, 1000) < 0) {
			printf("Poll failed");
                        goto finished;
		}
                if (pfd.revents == 0) {
                        continue;
                }
		if (smb2_service(smb2, pfd.revents) < 0) {
			printf("smb2_service failed with : %s\n",
                               smb2_get_error(smb2));
			goto finished;
		}
	}

 finished:
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);

	return rc;
}
