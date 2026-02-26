/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include <errno.h>
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

uint8_t buf[256 * 1024];

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-put-async <file> <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>>[:<port>]/<share>/<path>\n");
        exit(1);
}

struct cb_data {
        int is_finished;
        int fd;
        struct smb2fh *fh;
        uint32_t pos;
};

void write_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct cb_data *data = private_data;
        int count;

        if (status < 0) {
                printf("failed to write to share (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        if (status == 0) {
                data->is_finished = 1;
                return;
        }
        
        printf("write_cb:%d\n", status);
        data->pos += status;
        
        count = read(data->fd, buf, 1024);
        if (count <= 0) {
                exit(10);
        }
        if (smb2_pwrite_async(smb2, data->fh, buf, count, data->pos, write_cb,
                              data) < 0) {
		printf("smb2_write_async failed. %s\n", smb2_get_error(smb2));
		exit(10);
        };
}

        
int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
        struct cb_data data;
	struct pollfd pfd;
        int count;

        data.is_finished = 0;
        data.pos = 0;

        if (argc < 2) {
                usage();
        }

        data.fd = open(argv[1], O_RDONLY);
        if (data.fd == -1) {
                printf("Failed to open local file %s (%s)\n", argv[1],
                       strerror(errno));
                exit(10);
        }

	smb2 = smb2_init_context();
        if (smb2 == NULL) {
                fprintf(stderr, "Failed to init context\n");
                exit(0);
        }

        url = smb2_parse_url(smb2, argv[2]);
        if (url == NULL) {
                fprintf(stderr, "Failed to parse url: %s\n",
                        smb2_get_error(smb2));
                exit(0);
        }

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);
	if (smb2_connect_share(smb2, url->server, url->share, url->user) != 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        data.fh = smb2_open(smb2, url->path, O_WRONLY|O_CREAT);
        if (data.fh == NULL) {
		printf("smb2_open failed. %s\n", smb2_get_error(smb2));
		exit(10);
        }

        count = read(data.fd, buf, 1024);
        if (count <= 0) {
                exit(10);
        }
        if (smb2_pwrite_async(smb2, data.fh, buf, count, data.pos, write_cb,
                              &data) < 0) {
		printf("smb2_write_async failed. %s\n", smb2_get_error(smb2));
		exit(10);
        };

        while (!data.is_finished) {
		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

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
        
        close(data.fd);
        smb2_close(smb2, data.fh);
        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);

	return 0;
}
