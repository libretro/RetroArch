/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <errno.h>

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"

static int
smb2_encode_set_info_request(struct smb2_context *smb2,
                             struct smb2_pdu *pdu,
                             struct smb2_set_info_request *req)
{
        int i, len;
        uint8_t *buf;
        struct smb2_iovec *iov;
        struct smb2_file_end_of_file_info *eofi;
        struct smb2_file_disposition_info *fdi;
        struct smb2_file_rename_info *rni;
        struct smb2_utf16 *name;

        len = SMB2_SET_INFO_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate set info buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for set-info request header");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_SET_INFO_REQUEST_SIZE);
        smb2_set_uint8(iov, 2, req->info_type);
        smb2_set_uint8(iov, 3, req->file_info_class);
        smb2_set_uint16(iov,8, SMB2_HEADER_SIZE + 32); /* buffer offset */
        smb2_set_uint32(iov,12, req->additional_information);
        memcpy(iov->buf + 16, req->file_id, SMB2_FD_SIZE);

        if (smb2->passthrough) {
                if (req->buffer_length) {
                        buf = malloc(PAD_TO_32BIT(req->buffer_length));
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate set "
                                                        "info data buffer");
                                return -1;
                        }
                        memcpy(buf, req->input_data, req->buffer_length);
                        iov = smb2_add_iovector(smb2, &pdu->out, buf, req->buffer_length, free);
                        if (iov == NULL) {
                                smb2_set_error(smb2, "Failed to add iovector for set-info passthrough buffer");
                                return -1;
                        }
                }
                smb2_set_uint32(iov, 4, req->buffer_length);
                smb2_set_uint16(iov, 8, req->buffer_offset);
                return 0;
        }

        switch (req->info_type) {
        case SMB2_0_INFO_FILE:
                switch (req->file_info_class) {
                case SMB2_FILE_BASIC_INFORMATION:
                        len = 40;
                        smb2_set_uint32(iov, 4, len); /* buffer length */

                        buf = calloc(len, sizeof(uint8_t));
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate set "
                                               "info data buffer");
                                return -1;
                        }
                        iov = smb2_add_iovector(smb2, &pdu->out, buf, len,
                                                free);
                        if (iov == NULL) {
                                smb2_set_error(smb2, "Failed to add iovector for set-info basic data");
                                return -1;
                        }
                        smb2_encode_file_basic_info(smb2, req->input_data, iov);
                        break;
                case SMB2_FILE_END_OF_FILE_INFORMATION:
                        len = 8;
                        smb2_set_uint32(iov, 4, len); /* buffer length */

                        buf = calloc(len, sizeof(uint8_t));
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate set "
                                               "info data buffer");
                                return -1;
                        }
                        iov = smb2_add_iovector(smb2, &pdu->out, buf, len,
                                                free);
                        if (iov == NULL) {
                                smb2_set_error(smb2, "Failed to add iovector for set-info EOF data");
                                return -1;
                        }

                        eofi = req->input_data;
                        smb2_set_uint64(iov, 0, eofi->end_of_file);
                        break;
                case SMB2_FILE_RENAME_INFORMATION:
                        rni = req->input_data;

                        name = smb2_utf8_to_utf16((char *)(rni->file_name));
                        if (name == NULL) {
                                smb2_set_error(smb2, "Could not convert name into UTF-16");
                                return -1;
                        }
                        /* Convert '/' to '\' in-place before copying into the iovec */
                        for (i = 0; i < name->len; i++) {
                                if (name->val[i] == 0x002f) {
                                        name->val[i] = 0x005c;
                                }
                        }

                        len = 28 + name->len * 2;
                        smb2_set_uint32(iov, 4, len); /* buffer length */

                        buf = calloc(len, sizeof(uint8_t));
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate set "
                                               "info data buffer");
                                free(name);
                                return -1;
                        }
                        iov = smb2_add_iovector(smb2, &pdu->out, buf, len,
                                                free);
                        if (iov == NULL) {
                                smb2_set_error(smb2, "Failed to add iovector for set-info rename data");
                                free(name);
                                return -1;
                        }

                        smb2_set_uint8(iov, 0, rni->replace_if_exist);
                        smb2_set_uint64(iov, 8, 0u);
                        smb2_set_uint32(iov, 16, name->len * 2);
                        memcpy(iov->buf + 20, name->val, name->len * 2);
                        free(name);

                        break;
                case SMB2_FILE_DISPOSITION_INFORMATION:
                        len = 1;
                        smb2_set_uint32(iov, 4, len); /* buffer length */

                        buf = calloc(len, sizeof(uint8_t));
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate set "
                                               "info data buffer");
                                return -1;
                        }
                        iov = smb2_add_iovector(smb2, &pdu->out, buf, len,
                                                free);
                        if (iov == NULL) {
                                smb2_set_error(smb2, "Failed to add iovector for set-info disposition data");
                                return -1;
                        }

                        fdi = req->input_data;
                        smb2_set_uint8(iov, 0, fdi->delete_pending);
                        break;
                default:
                        smb2_set_error(smb2, "Can not enccode info_type/"
                                       "info_class %d/%d yet",
                                       req->info_type,
                                       req->file_info_class);
                        return -1;
                }
                break;

        default:
                smb2_set_error(smb2, "Can not encode file info_type %d yet",
                               req->info_type);
                return -1;
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_set_info_async(struct smb2_context *smb2,
                        struct smb2_set_info_request *req,
                        smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_SET_INFO, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_set_info_request(smb2, pdu, req)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

static int
smb2_encode_set_info_reply(struct smb2_context *smb2,
                             struct smb2_pdu *pdu,
                             struct smb2_set_info_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_SET_INFO_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate set info buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for set-info reply header");
                return -1;
        }
        smb2_set_uint16(iov, 0, SMB2_SET_INFO_REPLY_SIZE);
        return 0;
}

struct smb2_pdu *
smb2_cmd_set_info_reply_async(struct smb2_context *smb2,
                        struct smb2_set_info_request *req,
                        smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_SET_INFO, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_set_info_reply(smb2, pdu, req)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

int
smb2_process_set_info_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        return 0;
}

int
smb2_process_set_info_request_fixed(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_set_info_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_SET_INFO_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of set "
                               "info request. Expected %d, got %d",
                               SMB2_SET_INFO_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate set-info request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint8(iov, 2, &req->info_type);
        smb2_get_uint8(iov, 3, &req->file_info_class);
        smb2_get_uint32(iov, 4, &req->buffer_length);
        smb2_get_uint16(iov, 8, &req->buffer_offset);
        smb2_get_uint32(iov, 12, &req->additional_information);
        memcpy(req->file_id, iov->buf + 16, SMB2_FD_SIZE);

        return req->buffer_length;
}

int
smb2_process_set_info_request_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_set_info_request *req = (struct smb2_set_info_request*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        if (!smb2->passthrough) {
                smb2_set_error(smb2, "can not interpret set-info buffers yet");
                return -1;
        }
        req->input_data = iov->buf;
        return 0;
}

