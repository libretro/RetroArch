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

int is_finished;
int level;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-share-enum [-l level] <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/\n");
        exit(1);
}

void se_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct srvsvc_NetrShareEnum_rep *rep = command_data;
        int i;

        if (status) {
                printf("failed to enumerate shares (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        /* We always only use Level1 for netshare enum */
        switch (level) {
        case SHARE_INFO_0:
                printf("Number of shares:%d\n", rep->ses.ShareInfo.Level0.EntriesRead);
                for (i = 0; i < rep->ses.ShareInfo.Level0.EntriesRead; i++) {
                        printf("%-20s\n", rep->ses.ShareInfo.Level0.Buffer->share_info_0[i].netname.utf8);
                }
                break;
        case SHARE_INFO_1:
                printf("Number of shares:%d\n", rep->ses.ShareInfo.Level1.EntriesRead);
                for (i = 0; i < rep->ses.ShareInfo.Level1.EntriesRead; i++) {
                        printf("%-20s %-20s", rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].netname.utf8,
                               rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].remark.utf8);
                        if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_DISKTREE) {
                                printf(" DISKTREE");
                        }
                        if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_PRINTQ) {
                                printf(" PRINTQ");
                        }
                        if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_DEVICE) {
                                printf(" DEVICE");
                        }
                        if ((rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & 3) == SHARE_TYPE_IPC) {
                                printf(" IPC");
                        }
                        if (rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & SHARE_TYPE_TEMPORARY) {
                                printf(" TEMPORARY");
                        }
                        if (rep->ses.ShareInfo.Level1.Buffer->share_info_1[i].type & SHARE_TYPE_HIDDEN) {
                                printf(" HIDDEN");
                        }
                        printf("\n");
                }
                break;
        }

        smb2_free_data(smb2, rep);

        is_finished = 1;
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
	struct pollfd pfd;
        int opt;

        while ((opt = getopt(argc, argv, "l:")) != -1) {
                switch (opt) {
                case 'l':
                        level = atoi(optarg);
                        break;
                default: /* '?' */
                        usage();
                }
        }

        if (optind >= argc) {
                usage();
        }

	smb2 = smb2_init_context();
        if (smb2 == NULL) {
                fprintf(stderr, "Failed to init context\n");
                exit(0);
        }

        switch (level) {
        case SHARE_INFO_0:
        case SHARE_INFO_1:
                break;
        default:
                fprintf(stderr, "level must be 0/1\n");
                exit(0);
        }

        url = smb2_parse_url(smb2, argv[optind]);
        if (url == NULL) {
                fprintf(stderr, "Failed to parse url: %s\n",
                        smb2_get_error(smb2));
                exit(0);
        }
        if (url->user) {
                smb2_set_user(smb2, url->user);
        }

        smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

        if (smb2_connect_share(smb2, url->server, "IPC$", NULL) < 0) {
		printf("Failed to connect to IPC$. %s\n",
                       smb2_get_error(smb2));
		exit(10);
        }

	if (smb2_share_enum_async(smb2, level, se_cb, NULL) != 0) {
		printf("smb2_share_enum failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        while (!is_finished) {
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

        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
