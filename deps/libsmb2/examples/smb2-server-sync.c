/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2024 by Brian Dodge <bdodge09@gmail.com>

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
#include <errno.h>

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"

#define PAD_TO_32BIT(len) ((len + 0x03) & 0xfffffffc)
#define PAD_TO_64BIT(len) ((len + 0x07) & 0xfffffff8)

static int
fill_file_info(struct smb2_context *smb2, uint8_t info_type, uint8_t file_info_class, void **out_info)
{
        uint8_t *info = NULL;
        int len = 0;

        switch (info_type) {
        case SMB2_0_INFO_FILE:
                switch (file_info_class) {
                case SMB2_FILE_BASIC_INFORMATION:
                {
                        struct smb2_file_basic_info *fs;

                        len = sizeof(struct smb2_file_basic_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        memset(fs, 0, len);
                        fs->file_attributes = 0;
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_STANDARD_INFORMATION:
                {
                        struct smb2_file_standard_info *fs;

                        len = sizeof(struct smb2_file_standard_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        memset(fs, 0, len);
                        fs->allocation_size = 32;
                        fs->end_of_file = 32;
                        fs->number_of_links = 0;
                        fs->delete_pending = 0;
                        fs->directory = 0;
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_RENAME_INFORMATION:
                        break;
                case SMB2_FILE_ALL_INFORMATION:
                {
                        struct smb2_file_all_info *fs;

                        len = sizeof(struct smb2_file_all_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        memset(fs, 0, len);
                        fs->basic.file_attributes = 0;
                        fs->standard.allocation_size = 32;
                        fs->standard.end_of_file = 32;
                        fs->standard.number_of_links = 0;
                        fs->standard.delete_pending = 0;
                        fs->standard.directory = 0;
                        fs->index_number = 1;
                        fs->ea_size = 0;
                        fs->access_flags = 0xFFFFFFFF;
                        fs->current_byte_offset = 0;
                        fs->mode = 0;
                        fs->alignment_requirement = 0;
                        fs->name = (uint8_t*)"junk.txt";
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_NETWORK_OPEN_INFORMATION:
                {
                        struct smb2_file_network_open_info *fs;

                        len = sizeof(struct smb2_file_network_open_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        memset(fs, 0, len);
                        fs->file_attributes = 0;
                        fs->allocation_size = 32;
                        fs->end_of_file = 32;
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_END_OF_FILE_INFORMATION:
                        break;
                default:
                        break;
                }
                break;
        case SMB2_0_INFO_FILESYSTEM:
                switch (file_info_class) {
                case SMB2_FILE_FS_VOLUME_INFORMATION:
                        break;
                case SMB2_FILE_FS_SIZE_INFORMATION:
                {
                        struct smb2_file_fs_size_info *fs;

                        len = sizeof(struct smb2_file_fs_size_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        fs->total_allocation_units = 0x100000;
                        fs->available_allocation_units = 0x10000;
                        fs->sectors_per_allocation_unit = 1;
                        fs->bytes_per_sector = 512;
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_FS_DEVICE_INFORMATION:
                {
                        struct smb2_file_fs_device_info *fs;

                        len = sizeof(struct smb2_file_fs_device_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        fs->device_type = FILE_DEVICE_DISK;
                        fs->characteristics = 0;
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_FS_ATTRIBUTE_INFORMATION:
                {
                        struct smb2_file_fs_attribute_info *fs;

                        len = sizeof(struct smb2_file_fs_attribute_info);
                        fs = malloc(len);
                        if (!fs) {
                                return -1;
                        }
                        fs->filesystem_attributes = 0x2; //FILE_CASE_PRESERVED_NAMES;
                        fs->maximum_component_name_length = 0x100;
                        fs->filesystem_name = (uint8_t*)"Sotero";
                        fs->filesystem_name_length = strlen((char*)fs->filesystem_name);
                        info = (uint8_t*)fs;
                        break;
                }
                case SMB2_FILE_FS_CONTROL_INFORMATION:
                        break;
                case SMB2_FILE_FS_FULL_SIZE_INFORMATION:
                        break;
                case SMB2_FILE_FS_SECTOR_SIZE_INFORMATION:
                        break;
                default:
                        break;
                }
                break;
        case SMB2_0_INFO_SECURITY:
                break;
        case SMB2_0_INFO_QUOTA:
                break;
        default:
                return 0;
        }

        *out_info = info;
        return len;
}

static int fill_dir_info(struct smb2_context *smb2, uint8_t **out_info)
{
        static const char *files[] = {
                "junk.txt",
                "crap.bin"
        };
        struct smb2_fileidbothdirectoryinformation *fsb;
        struct smb2_utf16 *fname = NULL;
        uint8_t *info;
        uint32_t fname_len;
        int len;
        int nfiles = sizeof(files)/sizeof(files[0]);
        int n;

        len = 0;
        for (n = 0; n < nfiles; n++) {
                len += PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation) + 2 * strlen(files[n]));
        }

        info = malloc(len);
        if (!info) {
                //smb2_set_error(smb2, "can not alloc dir info", smb2_get_error(smb2));
                return -ENOMEM;
        }
        memset(info, 0, len);

        len = 0;
        for (n = 0; n < nfiles; n++) {
                fname_len = 0;
                fname = smb2_utf8_to_utf16(files[n]);
                if (fname == NULL) {
                        //smb2_set_error(smb2, "Could not convert name into UTF-16");
                        return -EINVAL;
                }
                fname_len = 2 * fname->len;

                fsb = (struct smb2_fileidbothdirectoryinformation *)(info + len);
                fsb->file_index = n;
                fsb->file_name_length = fname_len;
                fsb->short_name_length = sizeof(fsb->short_name);
                if (fsb->short_name_length > fname_len) {
                        fsb->short_name_length = fname_len;
                }
                memcpy(fsb->short_name, fname->val, fsb->short_name_length);
                fsb->name = files[n];
                len += PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation));
                free(fname);
        }
        *out_info = info;
        return len;
}

static int session_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
        printf("Selected dialect %04x\n", smb2_get_dialect(smb2));
        return 0;
}

static int authorize_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                                const char *user,
                                const char *domain,
                                const char *workstation)
{
        if (user) {
                smb2_set_user(smb2, user);
                smb2_set_password_from_file(smb2);
                return 0;
        }
        return -1;
}

static int logoff_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
        return 0;
}

static int tree_connect_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_tree_connect_request *req,
                    struct smb2_tree_connect_reply *rep)
{
        rep->share_type = SMB2_SHARE_TYPE_DISK;
        rep->maximal_access = 0x101f01ff;

        if (req->path && req->path_length) {
                int ei = (req->path_length / 2) - 4;
                if (ei >= 0) {
                        if (req->path[ei] == 'I' && req->path[ei + 3] == '$') {
                                rep->share_type = SMB2_SHARE_TYPE_PIPE;
                                rep->maximal_access = 0x1f00a9;
                        }
                }
        }
        rep->share_flags = 0;
        rep->capabilities = 0;

        return 0;
}

static int tree_disconnect_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                            const uint32_t tree_id)
{
        return 0;
}

static int create_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_create_request *req,
                    struct smb2_create_reply *rep)
{
        rep->file_attributes = SMB2_FILE_ATTRIBUTE_NORMAL;
        return 0;
}

static int close_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_close_request *req,
                    struct smb2_close_reply *rep)
{
        memset(rep, 0, sizeof(*rep));
        return 0;
}

static int flush_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_flush_request *req)
{
        return 0;
}

static int read_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_read_request *req,
                    struct smb2_read_reply *rep)
{
        if (req->offset > 32) {
                rep->data = NULL;
                rep->data_length = 0;
                rep->data_remaining = 0;
                return 0;
        }
        rep->data_offset = req->offset;
        rep->data_length = 32;
        rep->data_remaining = 0;

        rep->data = malloc(rep->data_length);
        if (!rep->data) {
                return -ENOMEM;
        }
        for (uint32_t i = 0; i < rep->data_length; i++) {
                rep->data[i] = ('A' + i) & 0xff;
        }
        return 0;
}

static int write_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_write_request *req,
                    struct smb2_write_reply *rep)
{
        rep->count = req->length;
        rep->remaining = 0;

        return 0;
}

static int lock_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_lock_request *req)
{
        return 0;
}

int ioctl_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_ioctl_request *req,
                    struct smb2_ioctl_reply *rep)
{
        memset(rep, 0, sizeof(*rep));
        rep->ctl_code = req->ctl_code;
        memcpy(rep->file_id, req->file_id, SMB2_FD_SIZE);

        switch(rep->ctl_code) {
        case SMB2_FSCTL_VALIDATE_NEGOTIATE_INFO:
                break;
        default:
                return 1;
        }
        return 0;
}

static int cancel_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
        return 0;
}

static int echo_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
        return 0;
}

static int query_directory_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_query_directory_request *req,
                    struct smb2_query_directory_reply *rep)
{
        static int rets;

        if (rets++ == 0) {
                rep->output_buffer_length = fill_dir_info(smb2, &rep->output_buffer);
        }
        else
        {
                rep->output_buffer_length = 0;
                rep->output_buffer = NULL;
                rets = 0;
        }
        return 0;
}

static int query_info_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                    struct smb2_query_info_request *req,
                    struct smb2_query_info_reply *rep)
{
        rep->output_buffer_length = fill_file_info(smb2, req->info_type, req->file_info_class, &rep->output_buffer);
        return rep->output_buffer_length > 0 ? 0 : -1;
}

struct smb2_server_request_handlers test_handlers = {
        NULL,
        authorize_handler,
        session_handler,
        logoff_handler,
        tree_connect_handler,
        tree_disconnect_handler,
        create_handler,
        close_handler,
        flush_handler,
        read_handler,
        write_handler,
        NULL,
        NULL,
        lock_handler,
        ioctl_handler,
        cancel_handler,
        echo_handler,
        query_directory_handler,
        NULL,
        query_info_handler,
        NULL
};

struct smb2_server server;

int usage(void)
{
    fprintf(stderr, "Usage:\n"
            "smbsever [port]\n\n");
    exit(1);
}

void on_smb2_error(struct smb2_context *smb2, const char *error_string)
{
        if (error_string) {
                fprintf(stderr, "%p: %s\n", smb2, error_string);
        }
}

void on_new_client(struct smb2_context *smb2, void *cb_data)
{
    struct smb2_context **pctx = (struct smb2_context **)cb_data;

    printf("New connection: %p\n", smb2);

    /* setup client context */
    smb2_set_version(smb2, SMB2_VERSION_ANY);
//  smb2_set_version(smb2, SMB2_VERSION_0210);

    smb2_register_error_callback(smb2, on_smb2_error);
    *pctx = smb2;
}

int main(int argc, char **argv)
{
    struct smb2_context *smb2 = NULL;
    int err;

    if (argc < 2) {
        usage();
    }

    memset(&server, 0, sizeof(server));
    server.handlers = &test_handlers;

    server.signing_enabled = 1;
    server.allow_anonymous = 1;

    server.port = strtoul(argv[1], NULL, 0);

    err = smb2_serve_port(&server, 1, on_new_client, (void *)&smb2);
    if (err) {
        fprintf(stderr, "smb2_serv_port failed %d\n", err);
        exit(err);
    }

    if (smb2) {
        fprintf(stderr, "client error %s\n", smb2_get_error(smb2));
    }

    return 0;
}

