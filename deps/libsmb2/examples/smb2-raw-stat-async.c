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
                "smb2-raw-stat-async <smb2-url>\n\n"
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
        struct smb2_file_all_info *fs;
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
                      stat_data->fs, stat_data->cb_data);
        free(stat_data);
}

static void
stat_cb_2(struct smb2_context *smb2, int status,
          void *command_data, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;
        struct smb2_query_info_reply *rep = command_data;
        struct smb2_file_all_info *fs = rep->output_buffer;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
        if (stat_data->status != SMB2_STATUS_SUCCESS) {
                return;
        }

        /* Remember the fs structure so we can pass it back to the caller */
        stat_data->fs = fs;
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

int send_compound_stat(struct smb2_context *smb2, const char *path,
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
        cr_req.desired_access = SMB2_FILE_READ_ATTRIBUTES | SMB2_FILE_READ_EA;
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
        qi_req.info_type = SMB2_0_INFO_FILE;
        qi_req.file_info_class = SMB2_FILE_ALL_INFORMATION;
        qi_req.output_buffer_length = 65535;
        qi_req.additional_information = 0;
        qi_req.flags = 0;
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
                fprintf(stderr, "Failed to create CLOSE command\n");
                free(stat_data);
                smb2_free_pdu(smb2, pdu);
                return -1;
        }
        smb2_add_compound_pdu(smb2, pdu, next_pdu);

        smb2_queue_pdu(smb2, pdu);

        return 0;
}

int main(int argc, char *argv[])
{
        struct smb2_context *smb2;
        struct smb2_url *url;
        struct sync_cb_data cb_data;
        struct smb2_file_all_info *fs;
        time_t t;

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

        fs = cb_data.ptr;

        /* Print the file_all_info structure */
        printf("Attributes: ");
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_READONLY) {
                printf("READONLY ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_HIDDEN) {
                printf("HIDDEN ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_SYSTEM) {
                printf("SYSTEM ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_DIRECTORY) {
                printf("DIRECTORY ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_ARCHIVE) {
                printf("ARCHIVE ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_NORMAL) {
                printf("NORMAL ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_TEMPORARY) {
                printf("TMP ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_SPARSE_FILE) {
                printf("SPARSE ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_REPARSE_POINT) {
                printf("REPARSE ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_COMPRESSED) {
                printf("COMPRESSED ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_OFFLINE) {
                printf("OFFLINE ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) {
                printf("NOT_CONTENT_INDEXED ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_ENCRYPTED) {
                printf("ENCRYPTED ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_INTEGRITY_STREAM) {
                printf("INTEGRITY_STREAM ");
        }
        if (fs->basic.file_attributes & SMB2_FILE_ATTRIBUTE_NO_SCRUB_DATA) {
                printf("NO_SCRUB_DATA ");
        }
        printf("\n");


        t = fs->basic.creation_time.tv_sec;
        printf("Creation Time:    %s", asctime(localtime(&t)));
        t = fs->basic.last_access_time.tv_sec;
        printf("Last Access Time: %s", asctime(localtime(&t)));
        t = fs->basic.last_write_time.tv_sec;
        printf("Last Write Time:  %s", asctime(localtime(&t)));
        t = fs->basic.change_time.tv_sec;
        printf("Change Time:      %s", asctime(localtime(&t)));

        printf("Allocation Size: %" PRIu64 "\n", fs->standard.allocation_size);
        printf("End Of File:     %" PRIu64 "\n", fs->standard.end_of_file);
        printf("Number Of Links: %d\n", fs->standard.number_of_links);
        printf("Delete Pending:  %s\n", fs->standard.delete_pending ?
               "YES" : "NO");
        printf("Directory:       %s\n", fs->standard.directory ?
               "YES" : "NO");

        printf("Index Number: 0x%016" PRIx64 "\n", fs->index_number);
        printf("EA Size : %d\n", fs->ea_size);

        printf("Access Flags: ");
        if (fs->standard.directory) {
                if (fs->access_flags & SMB2_FILE_LIST_DIRECTORY) {
                        printf("LIST_DIRECTORY ");
                }
                if (fs->access_flags & SMB2_FILE_ADD_FILE) {
                        printf("ADD_FILE ");
                }
                if (fs->access_flags & SMB2_FILE_ADD_SUBDIRECTORY) {
                        printf("ADD_SUBDIRECTORY ");
                }
                if (fs->access_flags & SMB2_FILE_TRAVERSE) {
                        printf("TRAVERSE ");
                }
        } else {
                if (fs->access_flags & SMB2_FILE_READ_DATA) {
                        printf("READ_DATA ");
                }
                if (fs->access_flags & SMB2_FILE_WRITE_DATA) {
                        printf("WRITE_DATA ");
                }
                if (fs->access_flags & SMB2_FILE_APPEND_DATA) {
                        printf("APPEND_DATA ");
                }
                if (fs->access_flags & SMB2_FILE_EXECUTE) {
                        printf("FILE_EXECUTE ");
                }
        }
        if (fs->access_flags & SMB2_FILE_READ_EA) {
                printf("READ_EA ");
        }
        if (fs->access_flags & SMB2_FILE_WRITE_EA) {
                printf("WRITE_EA ");
        }
        if (fs->access_flags & SMB2_FILE_READ_ATTRIBUTES) {
                printf("READ_ATTRIBUTES ");
        }
        if (fs->access_flags & SMB2_FILE_WRITE_ATTRIBUTES) {
                printf("WRITE_ATTRIBUTES ");
        }
        if (fs->access_flags & SMB2_FILE_DELETE_CHILD) {
                printf("DELETE_CHILD ");
        }
        if (fs->access_flags & SMB2_DELETE) {
                printf("DELETE ");
        }
        if (fs->access_flags & SMB2_READ_CONTROL) {
                printf("READ_CONTROL ");
        }
        if (fs->access_flags & SMB2_WRITE_DACL) {
                printf("WRITE_DACL ");
        }
        if (fs->access_flags & SMB2_WRITE_OWNER) {
                printf("WRITE_OWNER ");
        }
        if (fs->access_flags & SMB2_SYNCHRONIZE) {
                printf("SYNCHRONIZE ");
        }
        if (fs->access_flags & SMB2_ACCESS_SYSTEM_SECURITY) {
                printf("ACCESS_SYSTEM_SECURITY ");
        }
        if (fs->access_flags & SMB2_MAXIMUM_ALLOWED) {
                printf("MAXIMUM_ALLOWED ");
        }
        if (fs->access_flags & SMB2_GENERIC_ALL) {
                printf("GENERIC_ALL ");
        }
        if (fs->access_flags & SMB2_GENERIC_EXECUTE) {
                printf("GENERIC_EXECUTE ");
        }
        if (fs->access_flags & SMB2_GENERIC_WRITE) {
                printf("GENERIC_WRITE ");
        }
        if (fs->access_flags & SMB2_GENERIC_READ) {
                printf("GENERIC_READ ");
        }
        printf("\n");

        printf("Current Byte Offset: %" PRIu64 "\n", fs->current_byte_offset);

        printf("Mode: ");
        if (fs->access_flags & SMB2_FILE_WRITE_THROUGH) {
                printf("WRITE_THROUGH ");
        }
        if (fs->access_flags & SMB2_FILE_SEQUENTIAL_ONLY) {
                printf("SEQUENTIAL_ONLY ");
        }
        if (fs->access_flags & SMB2_FILE_NO_INTERMEDIATE_BUFFERING) {
                printf("NO_INTERMEDIATE_BUFFERING ");
        }
        if (fs->access_flags & SMB2_FILE_SYNCHRONOUS_IO_ALERT) {
                printf("SYNCHRONOUS_IO_ALERT ");
        }
        if (fs->access_flags & SMB2_FILE_SYNCHRONOUS_IO_NONALERT) {
                printf("SYNCHRONOUS_IO_NONALERT ");
        }
        if (fs->access_flags & SMB2_FILE_DELETE_ON_CLOSE) {
                printf("DELETE_ON_CLOSE ");
        }
        printf("\n");

        printf("Alignment Requirement: %d\n", fs->alignment_requirement);
        printf("Name: %s\n", fs->name);

        smb2_free_data(smb2, fs);

        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
