/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2017 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-raw-getsd-async <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>>[:<port>]/<share>/<path>\n");
        exit(1);
}

struct sync_cb_data {
	int is_finished;
	int status;
	void *ptr;
};

static int wait_for_reply(struct smb2_context *smb2,
                          struct sync_cb_data *cb_data)
{
        while (!cb_data->is_finished) {
                struct pollfd pfd;

		pfd.fd = smb2_get_fd(smb2);
		pfd.events = smb2_which_events(smb2);

		if (poll(&pfd, 1, 1000) < 0) {
			fprintf(stderr, "Poll failed");
			return -1;
		}
                if (pfd.revents == 0) {
                        continue;
                }
		if (smb2_service(smb2, pfd.revents) < 0) {
			fprintf(stderr, "smb2_service failed with : "
                                "%s\n", smb2_get_error(smb2));
                        return -1;
		}
	}

        return 0;
}

static void generic_status_cb(struct smb2_context *smb2, int status,
                    void *command_data, void *private_data)
{
        struct sync_cb_data *cb_data = private_data;

        cb_data->is_finished = 1;
        cb_data->status = status;
        cb_data->ptr = command_data;
}

struct stat_cb_data {
        smb2_command_cb cb;
        void *cb_data;

        uint32_t status;
        struct smb2_security_descriptor *sd;
};

static void
stat_cb_3(struct smb2_context *smb2, int status,
          void *command_data _U_, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }

        stat_data->cb(smb2, -nterror_to_errno(stat_data->status),
                      stat_data->sd, stat_data->cb_data);
        free(stat_data);
}

static void
stat_cb_2(struct smb2_context *smb2, int status,
          void *command_data, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;
        struct smb2_query_info_reply *rep = command_data;
        struct smb2_security_descriptor *sd = rep->output_buffer;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
        if (stat_data->status != SMB2_STATUS_SUCCESS) {
                return;
        }

        /* Remember the sd structure so we can pass it back to the caller */
        stat_data->sd = sd;
}

static void
stat_cb_1(struct smb2_context *smb2, int status,
          void *command_data _U_, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
}

static int
send_compound_stat(struct smb2_context *smb2, const char *path,
                   smb2_command_cb cb, void *cb_data)
{
        struct stat_cb_data *stat_data;
        struct smb2_create_request cr_req;
        struct smb2_query_info_request qi_req;
        struct smb2_close_request cl_req;
        struct smb2_pdu *pdu, *next_pdu;

        stat_data = malloc(sizeof(struct stat_cb_data));
        if (stat_data == NULL) {
                fprintf(stderr, "Failed to allocate create_data");
                return -1;
        }
        memset(stat_data, 0, sizeof(struct stat_cb_data));

        stat_data->cb = cb;
        stat_data->cb_data = cb_data;

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.desired_access = SMB2_READ_CONTROL;
        cr_req.file_attributes = 0;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = 0;
        cr_req.name = path;

        pdu = smb2_cmd_create_async(smb2, &cr_req, stat_cb_1, stat_data);
        if (pdu == NULL) {
                fprintf(stderr, "Failed to create create command\n");
                free(stat_data);
                return -1;
        }

        /* QUERY INFO command */
        memset(&qi_req, 0, sizeof(struct smb2_query_info_request));
        qi_req.info_type = SMB2_0_INFO_SECURITY;
        qi_req.output_buffer_length = 65535;
        qi_req.additional_information =
                SMB2_OWNER_SECURITY_INFORMATION |
                SMB2_GROUP_SECURITY_INFORMATION |
                SMB2_DACL_SECURITY_INFORMATION;
        memcpy(qi_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_query_info_async(smb2, &qi_req,
                                             stat_cb_2, stat_data);
        if (next_pdu == NULL) {
                fprintf(stderr, "Failed to create query command\n");
                free(stat_data);
                smb2_free_pdu(smb2, pdu);
                return -1;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        /* CLOSE command */
        memset(&cl_req, 0, sizeof(struct smb2_close_request));
        cl_req.flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
        memcpy(cl_req.file_id, compound_file_id, SMB2_FD_SIZE);

        next_pdu = smb2_cmd_close_async(smb2, &cl_req, stat_cb_3, stat_data);
        if (next_pdu == NULL) {
                fprintf(stderr, "Failed to create close command\n");
                free(stat_data);
                smb2_free_pdu(smb2, pdu);
                return -1;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

static void
print_sid(struct smb2_sid *sid)
{
        int i;
        uint64_t ia = 0;

        if (sid == NULL) {
                printf("No SID");
                return;
        }

        printf("S-1");
        for(i = 0; i < SID_ID_AUTH_LEN; i++) {
                ia <<= 8;
                ia |= sid->id_auth[i];
        }
        if (ia <= 0xffffffff) {
                printf("-%" PRIu64, ia);
        } else {
                printf("-0x%012" PRIx64, ia);
        }
        for (i = 0; i < sid->sub_auth_count; i++) {
                printf("-%u", sid->sub_auth[i]);
        }
}

static void
print_ace(struct smb2_ace *ace)
{
        printf("ACE: ");
        printf("Type:%d ", ace->ace_type);
        printf("Flags:0x%02x ", ace->ace_flags);
        switch (ace->ace_type) {
        case SMB2_ACCESS_ALLOWED_ACE_TYPE:
        case SMB2_ACCESS_DENIED_ACE_TYPE:
        case SMB2_SYSTEM_AUDIT_ACE_TYPE:
        case SMB2_SYSTEM_MANDATORY_LABEL_ACE_TYPE:
                printf("Mask:0x%08x ", ace->mask);
                print_sid(ace->sid);
                break;
        default:
                printf("can't print this type");
        }
        printf("\n");
}

static void
print_acl(struct smb2_acl *acl)
{
        struct smb2_ace *ace;

        printf("Revision: %d\n", acl->revision);
        printf("Ace count: %d\n", acl->ace_count);
        for (ace = acl->aces; ace; ace = ace->next) {
                print_ace(ace);
        }
};

static void
print_security_descriptor(struct smb2_security_descriptor *sd)
{
        printf("Revision: %d\n", sd->revision);
        printf("Control: (0x%08x) ", sd->control);
        if (sd->control & SMB2_SD_CONTROL_SR) {
                printf("SR ");
        }
        if (sd->control & SMB2_SD_CONTROL_RM) {
                printf("RM ");
        }
        if (sd->control & SMB2_SD_CONTROL_PS) {
                printf("PS ");
        }
        if (sd->control & SMB2_SD_CONTROL_PD) {
                printf("PD ");
        }
        if (sd->control & SMB2_SD_CONTROL_SI) {
                printf("SI ");
        }
        if (sd->control & SMB2_SD_CONTROL_DI) {
                printf("DI ");
        }
        if (sd->control & SMB2_SD_CONTROL_SC) {
                printf("SC ");
        }
        if (sd->control & SMB2_SD_CONTROL_DC) {
                printf("DC ");
        }
        if (sd->control & SMB2_SD_CONTROL_DT) {
                printf("DT ");
        }
        if (sd->control & SMB2_SD_CONTROL_SS) {
                printf("SS ");
        }
        if (sd->control & SMB2_SD_CONTROL_SD) {
                printf("SD ");
        }
        if (sd->control & SMB2_SD_CONTROL_SP) {
                printf("SP ");
        }
        if (sd->control & SMB2_SD_CONTROL_DD) {
                printf("DD ");
        }
        if (sd->control & SMB2_SD_CONTROL_DP) {
                printf("DP ");
        }
        if (sd->control & SMB2_SD_CONTROL_GD) {
                printf("GD ");
        }
        if (sd->control & SMB2_SD_CONTROL_OD) {
                printf("OD ");
        }
        printf("\n");

        if (sd->owner) {
                printf("Owner SID: ");
                print_sid(sd->owner);
                printf("\n");
        }
        if (sd->group) {
                printf("Group SID: ");
                print_sid(sd->group);
                printf("\n");
        }
        if (sd->dacl) {
                printf("DACL:\n");
                print_acl(sd->dacl);
        }
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
        struct sync_cb_data cb_data;
        struct smb2_security_descriptor *sd;

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

	if (smb2_connect_share(smb2, url->server, url->share, url->user) != 0) {
		printf("smb2_connect_share failed. %s\n", smb2_get_error(smb2));
		exit(10);
	}

        memset(&cb_data, 0, sizeof(cb_data));
        if (send_compound_stat(smb2, url->path,
                               generic_status_cb, &cb_data) != 0) {
		printf("sending compound stat failed. %s\n",
                       smb2_get_error(smb2));
		exit(10);
        }

	if (wait_for_reply(smb2, &cb_data) < 0) {
		printf("failed waiting for a reply. %s\n",
                       smb2_get_error(smb2));
                exit(10);
        }

        if (cb_data.status != 0) {
                printf("Compound command sequence returned error 0x%08x\n",
                       cb_data.status);
                exit(10);
        }

        sd = cb_data.ptr;
        print_security_descriptor(sd);
        smb2_free_data(smb2, sd);

        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);

	return 0;
}
