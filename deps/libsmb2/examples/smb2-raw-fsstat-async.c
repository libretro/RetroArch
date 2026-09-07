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

int info_level;

int usage(void)
{
        fprintf(stderr, "Usage:\n"
                "smb2-raw-fsstat-async <smb2-url> <infolevel>\n\n"
                "URL format: "
                "smb://[<domain;][<username>@]<host>[:<port>]/<share>/<path>\n");
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
                      0, stat_data->cb_data);
        free(stat_data);
}

static void
stat_cb_2(struct smb2_context *smb2, int status,
          void *command_data, void *private_data)
{
        struct stat_cb_data *stat_data = private_data;
        struct smb2_query_info_reply *rep = command_data;
        struct smb2_file_fs_size_info *fs_size;
        struct smb2_file_fs_device_info *fs_dev;
        struct smb2_file_fs_control_info *fs_ct;
        struct smb2_file_fs_full_size_info *fs_full_size;
	struct smb2_file_fs_volume_info *fs_vol;

        if (stat_data->status == SMB2_STATUS_SUCCESS) {
                stat_data->status = status;
        }
        if (stat_data->status != SMB2_STATUS_SUCCESS) {
                return;
        }
        switch (info_level) {
	case SMB2_FILE_FS_VOLUME_INFORMATION:
		fs_vol = rep->output_buffer;
		printf("VolumeSerialNumber: 0x%0x\n",
		       fs_vol->volume_serial_number);
		printf("VolumeLabel: %s\n",
		       fs_vol->volume_label);
		break;
        case SMB2_FILE_FS_SIZE_INFORMATION:
                fs_size = rep->output_buffer;
                printf("TotalAllocationUnits: %" PRIu64 "\n",
                       fs_size->total_allocation_units);
                printf("AvailableAllocationUnits: %" PRIu64 "\n",
                       fs_size->available_allocation_units);
                printf("SectorsPerAllocationUnit: %u\n",
                       fs_size->sectors_per_allocation_unit);
                printf("BytesPerSector: %u\n",
                       fs_size->bytes_per_sector);
                break;
        case SMB2_FILE_FS_DEVICE_INFORMATION:
                fs_dev = rep->output_buffer;
                printf("DeviceType: %u\n",
                       fs_dev->device_type);
                printf("Characteristics: 0x%08x\n",
                       fs_dev->characteristics);
                break;
        case SMB2_FILE_FS_CONTROL_INFORMATION:
                fs_ct = rep->output_buffer;
                printf("FreeSpaceStartFiltering: %" PRIu64 "\n",
                       fs_ct->free_space_start_filtering);
                printf("FreeSpaceThreshold: %" PRIu64 "\n",
                       fs_ct->free_space_threshold);
                printf("FreeSpaceStopFiltering: %" PRIu64 "\n",
                       fs_ct->free_space_stop_filtering);
                printf("DefaultQuotaThreshold: %" PRIu64 "\n",
                       fs_ct->default_quota_threshold);
                printf("DefaultQuotaLimit: %" PRIu64 "\n",
                       fs_ct->default_quota_limit);
                printf("Characteristics: 0x%08x\n",
                       fs_ct->file_system_control_flags);
                break;
        case SMB2_FILE_FS_FULL_SIZE_INFORMATION:
                fs_full_size = rep->output_buffer;
                printf("TotalAllocationUnits: %" PRIu64 "\n",
                       fs_full_size->total_allocation_units);
                printf("CallerAvailableAllocationUnits: %" PRIu64 "\n",
                       fs_full_size->caller_available_allocation_units);
                printf("ActualAvailableAllocationUnits: %" PRIu64 "\n",
                       fs_full_size->actual_available_allocation_units);
                printf("SectorsPerAllocationUnit: %u\n",
                       fs_full_size->sectors_per_allocation_unit);
                printf("BytesPerSector: %u\n",
                       fs_full_size->bytes_per_sector);
                break;
        }
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
        qi_req.info_type = SMB2_0_INFO_FILESYSTEM;
        qi_req.file_info_class = info_level;
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

        if (argc < 3) {
                usage();
        }

        if (!strncmp(argv[2], "0x", 2)) {
                info_level = strtol(argv[2], NULL, 16);
        } else {
                info_level = strtol(argv[2], NULL, 10);
        }
        switch (info_level) {
	case SMB2_FILE_FS_VOLUME_INFORMATION:
		printf("InfoLevel:%d FileFsVolmeInformation\n", info_level);
		break;
        case SMB2_FILE_FS_SIZE_INFORMATION:
                printf("InfoLevel:%d FileFsSizeInformation\n", info_level);
                break;
        case SMB2_FILE_FS_DEVICE_INFORMATION:
                printf("InfoLevel:%d FileFsDeviceInformation\n", info_level);
                break;
        case SMB2_FILE_FS_CONTROL_INFORMATION:
                printf("InfoLevel:%d FileFsControlInformation\n", info_level);
                break;
        case SMB2_FILE_FS_FULL_SIZE_INFORMATION:
                printf("InfoLevel:%d FileFsFullSizeInformation\n", info_level);
                break;
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

        smb2_free_data(smb2, fs);

        smb2_disconnect_share(smb2);
        smb2_destroy_url(url);
        smb2_destroy_context(smb2);
        
	return 0;
}
