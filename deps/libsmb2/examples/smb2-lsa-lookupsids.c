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
#include "libsmb2-dcerpc-lsa.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

int is_finished;
struct ndr_context_handle PolicyHandle;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-lsa-lookupsids <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/share\n");
        exit(1);
}

void print_sid(RPC_SID *sid)
{
        int i;
        uint64_t ia = 0;

        printf("S-%d-", sid->Revision);
        for (i = 0; i < 6; i++) {
                ia <<= 8;
                ia |= sid->IdentifierAuthority[i];
        }
        printf("%"PRIu64, ia);
        for (i = 0; i < sid->SubAuthorityCount; i++) {
                printf("-%d", sid->SubAuthority[i]);
        }
}

void cl_cb(struct dcerpc_context *dce, int status,
           void *command_data, void *cb_data)
{
        struct lsa_close_rep *rep = command_data;
        
        if (status) {
                dcerpc_free_data(dce, rep);
                printf("failed to close policy handle (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }
        dcerpc_free_data(dce, rep);
        is_finished = 1;
}

void ls_cb(struct dcerpc_context *dce, int status,
                void *command_data, void *cb_data)
{
        struct lsa_lookupsids2_rep *rep = command_data;
        struct lsa_close_req cl_req;
        int i;

        if (status) {
                dcerpc_free_data(dce, rep);
                printf("failed to lookup sids (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }

        printf("ReferencedDomains\n");
        printf("   Entries:%d\n", rep->ReferencedDomains.Entries);
        printf("   MaxEntries:%d\n", rep->ReferencedDomains.MaxEntries);
        for(i = 0; i < rep->ReferencedDomains.Entries; i++) {
                printf("   Name:%s SID:", rep->ReferencedDomains.Domains[i].Name);
                print_sid(&rep->ReferencedDomains.Domains[i].Sid);
                printf("\n");
        }
        printf("TranslatedNames\n");
        printf("   Entries:%d\n", rep->TranslatedNames.Entries);
        for(i = 0; i < rep->TranslatedNames.Entries; i++) {
                printf("   Name:%s DomainIndex:%d\n", rep->TranslatedNames.Names[i].Name, rep->TranslatedNames.Names[i].DomainIndex);
        }

        memcpy(&cl_req.PolicyHandle, &PolicyHandle,
               sizeof(struct ndr_context_handle));
        dcerpc_free_data(dce, rep);
        if (dcerpc_call_async(dce,
                              LSA_CLOSE,
                              lsa_Close_req_coder, &cl_req,
                              lsa_Close_rep_coder,
                              sizeof(struct lsa_close_rep),
                              cl_cb, NULL) != 0) {
                printf("dcerpc_call_async failed with %s\n",
                       dcerpc_get_error(dce));
                exit(10);
        }
}

void op_cb(struct dcerpc_context *dce, int status,
                void *command_data, void *cb_data)
{
        struct lsa_openpolicy2_rep *rep = command_data;
        struct lsa_lookupsids2_req ls_req;
        PRPC_SID sid, *sids;
        int num_sids;
        uint32_t sa[2];

        if (status) {
                dcerpc_free_data(dce, rep);
                printf("failed to get policy handle (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }

        memcpy(&PolicyHandle, &rep->PolicyHandle,
               sizeof(struct ndr_context_handle));
        memcpy(&ls_req.PolicyHandle, &PolicyHandle,
               sizeof(struct ndr_context_handle));

        sid = malloc(sizeof(*sid) + 2 * sizeof(uint32_t));
        if (sid == NULL) {
                printf("failed to allocate SID\n");
                exit(10);
        }
        sid->Revision = 1;
        sid->SubAuthorityCount = 2;
        memcpy(sid->IdentifierAuthority, NT_SID_AUTHORITY, 6);
        sid->SubAuthority = &sa[0];
        sid->SubAuthority[0] = 32;
        sid->SubAuthority[1] = 544;

        num_sids = 2;
        sids = malloc(num_sids * sizeof(PRPC_SID));
        if (sids == NULL) {
                printf("failed to allocate SIDs\n");
                exit(10);
        }
        ls_req.SidEnumBuffer.Entries = num_sids;
        ls_req.SidEnumBuffer.SidInfo = sids;
        ls_req.SidEnumBuffer.SidInfo[0] = sid;
        ls_req.SidEnumBuffer.SidInfo[1] = sid;

        ls_req.TranslatedNames.Entries = 0;
        ls_req.TranslatedNames.Names = NULL;
        ls_req.LookupLevel = LsapLookupWksta;

        dcerpc_free_data(dce, rep);
        if (dcerpc_call_async(dce,
                              LSA_LOOKUPSIDS2,
                              lsa_LookupSids2_req_coder, &ls_req,
                              lsa_LookupSids2_rep_coder,
                              sizeof(struct lsa_lookupsids2_rep),
                              ls_cb, NULL) != 0) {
                printf("dcerpc_call_async failed with %s\n",
                       dcerpc_get_error(dce));
                exit(10);
        }
        free(sid);
        free(sids);
}

void co_cb(struct dcerpc_context *dce, int status,
           void *command_data, void *cb_data)
{
        struct lsa_openpolicy2_req op_req;
        struct smb2_url *url = cb_data;

        if (status != SMB2_STATUS_SUCCESS) {
                printf("failed to connect to LSA (%s) %s\n",
                       strerror(-status), dcerpc_get_error(dce));
                exit(10);
        }

        op_req.SystemName = malloc(strlen(url->server) + 3);
        if (op_req.SystemName == NULL) {
                printf("failed to allocate SystemName\n");
                exit(10);
        }
        sprintf(op_req.SystemName, "\\\\%s", url->server);

        op_req.ObjectAttributes.Length = 24;
        op_req.DesiredAccess = POLICY_LOOKUP_NAMES |
                POLICY_VIEW_LOCAL_INFORMATION;

        if (dcerpc_call_async(dce,
                              LSA_OPENPOLICY2,
                              lsa_OpenPolicy2_req_coder, &op_req,
                              lsa_OpenPolicy2_rep_coder,
                              sizeof(struct lsa_openpolicy2_rep),
                              op_cb, NULL) != 0) {
                printf("dcerpc_call_async failed with %s\n",
                       dcerpc_get_error(dce));
                exit(10);
        }
        free(op_req.SystemName);
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

        if (dcerpc_connect_context_async(dce, "lsarpc", &lsa_interface,
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
