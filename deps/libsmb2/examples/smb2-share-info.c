/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2020 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-dcerpc.h"
#include "libsmb2-dcerpc-srvsvc.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

int is_finished;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-share-info <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/share\n");
        exit(1);
}

void si_cb(struct dcerpc_context *dce, int status,
                void *command_data, void *cb_data)
{
        struct srvsvc_NetrShareGetInfo_rep *rep = command_data;

        free(cb_data);
        if (status) {
                printf("failed to get info for share (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }
        printf("%-20s %-20s", rep->InfoStruct.ShareInfo1.netname.utf8,
               rep->InfoStruct.ShareInfo1.remark.utf8);
        if ((rep->InfoStruct.ShareInfo1.type & 3) == SHARE_TYPE_DISKTREE) {
                        printf(" DISKTREE");
        }
        if ((rep->InfoStruct.ShareInfo1.type & 3) == SHARE_TYPE_PRINTQ) {
                printf(" PRINTQ");
        }
        if ((rep->InfoStruct.ShareInfo1.type & 3) == SHARE_TYPE_DEVICE) {
                printf(" DEVICE");
        }
        if ((rep->InfoStruct.ShareInfo1.type & 3) == SHARE_TYPE_IPC) {
                printf(" IPC");
        }
        if (rep->InfoStruct.ShareInfo1.type & SHARE_TYPE_TEMPORARY) {
                printf(" TEMPORARY");
        }
        if (rep->InfoStruct.ShareInfo1.type & SHARE_TYPE_HIDDEN) {
                printf(" HIDDEN");
        }

        printf("\n");
        dcerpc_free_data(dce, rep);

        is_finished = 1;
}

void co_cb(struct dcerpc_context *dce, int status,
           void *command_data, void *cb_data)
{
        struct srvsvc_NetrShareGetInfo_req *si_req;
        struct smb2_url *url = cb_data;
        char *server;

        if (status != SMB2_STATUS_SUCCESS) {
                printf("failed to connect to SRVSVC (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }

        si_req = calloc(1, sizeof(struct srvsvc_NetrShareGetInfo_req));
        if (si_req == NULL) {
                printf("failed to allocate srvsvc_NetrShareGetInfo_req\n");
                exit(10);
        }

        server = malloc(strlen(url->server) + 3);
        if (server == NULL) {
                printf("failed to allocate ServerName\n");
                exit(10);
        }
        sprintf(server, "\\\\%s", url->server);
        si_req->ServerName.utf8 = server;
        si_req->NetName.utf8 = url->share;
        si_req->Level = 1;

        if (dcerpc_call_async(dce,
                              SRVSVC_NETRSHAREGETINFO,
                              srvsvc_NetrShareGetInfo_req_coder, si_req,
                              srvsvc_NetrShareGetInfo_rep_coder,
                              sizeof(struct srvsvc_NetrShareGetInfo_rep),
                              si_cb, si_req) != 0) {
                printf("dcerpc_call_async failed with %s\n",
                       dcerpc_get_error(dce));
                free(si_req);
                exit(10);
        }
        free(server);
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct dcerpc_context *dce;
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

        if (smb2_connect_share(smb2, url->server, "IPC$", NULL) < 0) {
		printf("Failed to connect to IPC$. %s\n",
                       smb2_get_error(smb2));
		exit(10);
        }

        dce = dcerpc_create_context(smb2);
        if (dce == NULL) {
		printf("Failed to create dce context. %s\n",
                       smb2_get_error(smb2));
		exit(10);
        }

        if (dcerpc_connect_context_async(dce, "srvsvc", &srvsvc_interface,
                       co_cb, url) != 0) {
		printf("Failed to connect dce context. %s\n",
                       smb2_get_error(smb2));
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

        dcerpc_destroy_context(dce);
        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
