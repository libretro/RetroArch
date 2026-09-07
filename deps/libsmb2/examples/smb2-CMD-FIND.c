/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2022 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

int is_finished;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-CMD-FIND <smb2-url>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>>[:<port>]/<share>\n");
        exit(1);
}

struct app_data {
        smb2_file_id file_id;
        uint32_t index_1, index_2;
        const char *name_1, *name_2;
};

void qd_3_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2_query_directory_reply *rep = command_data;
        struct app_data *data = private_data;
        struct smb2_iovec vec;
        struct smb2_fileidfulldirectoryinformation fs;

        if (status) {
                printf("Restarting scan of directory using SMB2_INDEX_SPECIFIED failed (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                printf("ERROR: server does not support SMB2_INDEX_SPECIFIED\n");
                exit(10);
        }

        printf("Third scan of directory done.\n");
        vec.buf = rep->output_buffer;
        vec.len = rep->output_buffer_length;

        if (smb2_decode_fileidfulldirectoryinformation(smb2, &fs, &vec)) {
                printf("Failed to decode directory entry\n");
                printf("ERROR: restarting the scan using SMB2_RESTART_SCAN broken\n");
                exit(10);
        }
        printf("Index of first entry after restarting the scan at the second index: 0x%08x\n", fs.file_index);
        printf("Name of first entry after restarting the scan at the second index %s\n", fs.name);
        if (strcmp(fs.name, data->name_2)) {
                 printf("ERROR: restarting the scan using SMB2_INDEX_SPECIFIED did not return the expected second entry.\n");
                 printf("ERROR: server does not support SMB2_INDEX_SPECIFIED.\n");
                exit(10);
        }
                
}

void qd_2_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2_query_directory_reply *rep = command_data;
        struct smb2_query_directory_request req;
        struct smb2_pdu *pdu;
        struct app_data *data = private_data;
        struct smb2_iovec vec;
        struct smb2_fileidfulldirectoryinformation fs;

        if (status) {
                printf("Restarting scan of directory using SMB2_RESTART_SCAN failed (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                printf("ERROR: server does not support SMB2_RESTART_SCAN\n");
                exit(10);
        }

        printf("Second scan of directory done.\n");
        vec.buf = rep->output_buffer;
        vec.len = rep->output_buffer_length;

        if (smb2_decode_fileidfulldirectoryinformation(smb2, &fs, &vec)) {
                printf("Failed to decode directory entry\n");
                printf("ERROR: restarting the scan using SMB2_RESTART_SCAN broken\n");
                exit(10);
        }
        printf("Index of first entry after restarting the scan: 0x%08x\n", fs.file_index);
        printf("Name of first entry after restarting the scan %s\n", fs.name);
        if (strcmp(fs.name, data->name_1)) {
                 printf("ERROR: restarting the scan using SMB2_RESTART_SCAN did not return the name of the first entry.\n");
                exit(10);
        }
                
        printf("Try restarting scan at the second entry using SMB2_INDEX_SPECIFIED\n");
        memset(&req, 0, sizeof(struct smb2_query_directory_request));
        req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
        req.flags = SMB2_INDEX_SPECIFIED;
        req.file_index = data->index_2;
        memcpy(req.file_id, data->file_id, SMB2_FD_SIZE);
        req.output_buffer_length = 8 * 1024;
        req.name = "*";

        pdu = smb2_cmd_query_directory_async(smb2, &req, qd_2_cb, data);
        if (pdu == NULL) {
                printf("Failed to queue QUERY_DIRECTORY\n");
                exit(10);
        }
        smb2_queue_pdu(smb2, pdu);
}

void qd_1_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2_query_directory_reply *rep = command_data;
        struct smb2_query_directory_request req;
        struct smb2_pdu *pdu;
        struct app_data *data = private_data;
        struct smb2_iovec vec, tmp_vec;
        struct smb2_fileidfulldirectoryinformation fs;
        uint32_t offset = 0;

        if (status) {
                printf("failed to query directory (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        printf("Initial scan of directory done.\n");
        vec.buf = rep->output_buffer;
        vec.len = rep->output_buffer_length;

        tmp_vec.buf = &vec.buf[offset];
        tmp_vec.len = vec.len - offset;
        if (smb2_decode_fileidfulldirectoryinformation(smb2, &fs, &tmp_vec)) {
                printf("Failed to decode directory entry\n");
                exit(10);
        }
        data->name_1 = strdup(fs.name);
        data->index_1 = fs.file_index;
        printf("First file in directory: %s\n", data->name_1);
        printf("Index of first file in directory: 0x%08x\n", fs.file_index);

        offset += fs.next_entry_offset;
        tmp_vec.buf = &vec.buf[offset];
        tmp_vec.len = vec.len - offset;
        if (smb2_decode_fileidfulldirectoryinformation(smb2, &fs, &tmp_vec)) {
                printf("Failed to decode directory entry\n");
                exit(10);
        }
        data->name_2 = strdup(fs.name);
        data->index_2 = fs.file_index;
        printf("Second file in directory: %s\n", data->name_2);
        printf("Index of second file in directory: 0x%08x\n", fs.file_index);
        if (data->index_1 == data->index_2) {
                printf("ERROR: broken server returns same file_index for first two entries in the directory\n");
                printf("ERROR: This server does not support SMB2_INDEX_SPECIFIED queries\n");
        }
        
        printf("Try restarting scan of directory using SMB2_RESTART_SCAN\n");
        memset(&req, 0, sizeof(struct smb2_query_directory_request));
        req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
        req.flags = SMB2_RESTART_SCANS;
        memcpy(req.file_id, data->file_id, SMB2_FD_SIZE);
        req.output_buffer_length = 8 * 1024;
        req.name = "*";

        pdu = smb2_cmd_query_directory_async(smb2, &req, qd_2_cb, data);
        if (pdu == NULL) {
                printf("Failed to queue QUERY_DIRECTORY\n");
                exit(10);
        }
        smb2_queue_pdu(smb2, pdu);
}

void cr_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2_create_reply *rep = command_data;
        struct smb2_query_directory_request req;
        struct smb2_pdu *pdu;
        struct app_data *data = private_data;

        if (status) {
                printf("failed to open root of share (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        printf("Opened root of the share\n");
        memcpy(data->file_id, rep->file_id, SMB2_FD_SIZE);

        memset(&req, 0, sizeof(struct smb2_query_directory_request));
        req.file_information_class = SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION;
        req.flags = 0;
        memcpy(req.file_id, data->file_id, SMB2_FD_SIZE);
        req.output_buffer_length = 8 * 1024;
        req.name = "*";

        printf("Performing initial scan of directory\n");
        pdu = smb2_cmd_query_directory_async(smb2, &req, qd_1_cb, data);
        if (pdu == NULL) {
                printf("Failed to queue QUERY_DIRECTORY\n");
                exit(10);
        }
        smb2_queue_pdu(smb2, pdu);
}


void cf_cb(struct smb2_context *smb2, int status,
                void *command_data, void *private_data)
{
        struct smb2_create_request cr_req;
        struct smb2_pdu *pdu;
        struct add_data *data = private_data;

        if (status) {
                printf("failed to connect share (%s) %s\n",
                       strerror(-status), smb2_get_error(smb2));
                exit(10);
        }

        /* CREATE command */
        memset(&cr_req, 0, sizeof(struct smb2_create_request));
        cr_req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        cr_req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        cr_req.file_attributes = SMB2_FILE_ATTRIBUTE_DIRECTORY;
        cr_req.desired_access = SMB2_FILE_LIST_DIRECTORY | SMB2_FILE_READ_ATTRIBUTES;
        cr_req.share_access = SMB2_FILE_SHARE_READ | SMB2_FILE_SHARE_WRITE;
        cr_req.create_disposition = SMB2_FILE_OPEN;
        cr_req.create_options = SMB2_FILE_DIRECTORY_FILE;
        cr_req.name = "";

        pdu = smb2_cmd_create_async(smb2, &cr_req, cr_cb, data);
        if (pdu == NULL) {
                fprintf(stderr, "Failed to create create command\n");
                exit(10);
        }
        smb2_queue_pdu(smb2, pdu);
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
        struct app_data data;
        
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

	if (smb2_connect_share_async(smb2, url->server, url->share, url->user,
                                     cf_cb, &data) != 0) {
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
