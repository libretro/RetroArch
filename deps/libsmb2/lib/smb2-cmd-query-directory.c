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

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"

int
smb2_decode_fileidfulldirectoryinformation(
    struct smb2_context *smb2,
    struct smb2_fileidfulldirectoryinformation *fs,
    struct smb2_iovec *vec)
{
        uint32_t name_len;
        uint64_t t;

        /* Make sure the name fits before end of vector.
         * As the name is the final part of this blob this guarantees
         * that all other fields also fit within the remainder of the
         * vector.
         */
        smb2_get_uint32(vec, 60, &name_len);
        if (name_len > 80 + name_len ||
            80 + name_len > vec->len) {
                smb2_set_error(smb2, "Malformed name in query.\n");
                return -1;
        }

        smb2_get_uint32(vec, 0, &fs->next_entry_offset);
        smb2_get_uint32(vec, 4, &fs->file_index);
        smb2_get_uint64(vec, 40, &fs->end_of_file);
        smb2_get_uint64(vec, 48, &fs->allocation_size);
        smb2_get_uint32(vec, 56, &fs->file_attributes);
        smb2_get_uint32(vec, 64, &fs->ea_size);
        smb2_get_uint64(vec, 72, &fs->file_id);

        fs->name = smb2_utf16_to_utf8((uint16_t *)(void *)&vec->buf[80], name_len / 2);

        smb2_get_uint64(vec, 8, &t);
        smb2_win_to_timeval(t, &fs->creation_time);

        smb2_get_uint64(vec, 16, &t);
        smb2_win_to_timeval(t, &fs->last_access_time);

        smb2_get_uint64(vec, 24, &t);
        smb2_win_to_timeval(t, &fs->last_write_time);

        smb2_get_uint64(vec, 32, &t);
        smb2_win_to_timeval(t, &fs->change_time);

        return 0;
}

static int
smb2_encode_query_directory_request(struct smb2_context *smb2,
                                    struct smb2_pdu *pdu,
                                    struct smb2_query_directory_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_utf16 *name = NULL;
        struct smb2_iovec *iov;

        len = SMB2_QUERY_DIRECTORY_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate query buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for query-directory request");
                free(name);
                return -1;
        }

        /* Name */
        if (req->name && req->name[0]) {
                name = smb2_utf8_to_utf16(req->name);
                if (name == NULL) {
                        smb2_set_error(smb2, "Could not convert name into UTF-16");
                        return -1;
                }
                smb2_set_uint16(iov, 26, 2 * name->len);
        }

        smb2_set_uint16(iov, 0, SMB2_QUERY_DIRECTORY_REQUEST_SIZE);
        smb2_set_uint8(iov, 2, req->file_information_class);
        smb2_set_uint8(iov, 3, req->flags);
        smb2_set_uint32(iov, 4, req->file_index);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);
        smb2_set_uint16(iov, 24, SMB2_HEADER_SIZE + 32);
        smb2_set_uint32(iov, 28, req->output_buffer_length);

        /* Name */
        if (name) {
                buf = malloc(2 * name->len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate qdir name");
                        free(name);
                        return -1;
                }
                memcpy(buf, &name->val[0], 2 * name->len);
                iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        2 * name->len,
                                        free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for query-directory name");
                        return -1;
                }
        }
        free(name);

        return 0;
}

struct smb2_pdu *
smb2_cmd_query_directory_async(struct smb2_context *smb2,
                               struct smb2_query_directory_request *req,
                               smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_QUERY_DIRECTORY, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_query_directory_request(smb2, pdu, req)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        /* Adjust credit charge for large payloads */
        if (smb2->supports_multi_credit) {
                pdu->header.credit_charge =
                        (req->output_buffer_length - 1) / 65536 + 1; /* 3.1.5.2 of [MS-SMB2] */
        }

        return pdu;
}

static int
smb2_encode_query_directory_reply(struct smb2_context *smb2,
                                    uint8_t info_class,
                                    uint8_t flags,
                                    uint32_t room,
                                    struct smb2_pdu *pdu,
                                    struct smb2_query_directory_reply *rep)
{
        int len;
        int fslen;
        uint8_t *buf;
        struct smb2_utf16 *name = NULL;
        struct smb2_fileidbothdirectoryinformation *fs;
        struct smb2_iovec *iov;
        uint32_t fs_size;
        uint32_t fname_len;
        int offset;
        int in_offset;
        int in_remain;

        len = SMB2_QUERY_DIRECTORY_REPLY_SIZE & 0xfffe;
        len = PAD_TO_32BIT(len);

        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate query reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);

        fslen = rep->output_buffer_length;
        rep->output_buffer_offset = len + SMB2_HEADER_SIZE;

        if (rep->output_buffer) {
                if (!smb2->passthrough) {
                        rep->output_buffer_length = 0;
                        in_offset = 0;
                        in_remain = fslen;
                        do {
                                fs = (struct smb2_fileidbothdirectoryinformation*)(void *)(rep->output_buffer + in_offset);
                                fname_len = 0;
                                if (fs->name && fs->name[0]) {
                                        name = smb2_utf8_to_utf16(fs->name);
                                        if (name == NULL) {
                                                smb2_set_error(smb2, "Could not convert name into UTF-16");
                                                return -1;
                                        }
                                        fname_len = 2 * name->len;
                                        free(name);
                                }
                                switch (info_class)
                                {
                                case SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION:
                                        fs_size = PAD_TO_32BIT(SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE + fname_len);
                                        break;
                                case SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION:
                                        fs_size = PAD_TO_32BIT(SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE + fname_len);
                                        break;
                                default:
                                        fs_size = 0;
                                        break;
                                }
                                rep->output_buffer_length += fs_size;
                                in_offset += PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation));
                                in_remain -= PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation));
                        }
                        while (in_remain >= sizeof(struct smb2_fileidbothdirectoryinformation));
                }
        }

        smb2_set_uint16(iov, 0, SMB2_QUERY_DIRECTORY_REPLY_SIZE);
        smb2_set_uint16(iov, 2, rep->output_buffer_offset);
        smb2_set_uint32(iov, 4, rep->output_buffer_length);

        if (rep->output_buffer_length == 0) {
                return 0;
        }

        len = rep->output_buffer_length;
        len = PAD_TO_32BIT(len);
        buf = malloc(len);
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate output buf");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        len,
                                        free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for query-directory output buffer");
                return -1;
        }

        in_offset = 0;
        in_remain = fslen;
        offset = 0;

        if (smb2->passthrough) {
                memcpy(buf, rep->output_buffer, rep->output_buffer_length);
                memset(buf + rep->output_buffer_length, 0, len - rep->output_buffer_length);
                iov->len = rep->output_buffer_length;
        }
        else {

                do {
                        fs = (struct smb2_fileidbothdirectoryinformation*)(void *)(rep->output_buffer + in_offset);
                        fname_len = 0;
                        if (fs->name && fs->name[0]) {
                                name = smb2_utf8_to_utf16(fs->name);
                                if (name == NULL) {
                                        smb2_set_error(smb2, "Could not convert name into UTF-16");
                                        return -1;
                                }
                                fname_len = 2 * name->len;
                        }
                        switch (info_class)
                        {
                        case SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION:
                                fs_size = PAD_TO_32BIT(SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE + fname_len);
                                break;
                        case SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION:
                                fs_size = PAD_TO_32BIT(SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE + fname_len);
                                break;
                        default:
                                fs_size = 0;
                                break;
                        }
                        in_offset += PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation));
                        in_remain -= PAD_TO_64BIT(sizeof(struct smb2_fileidbothdirectoryinformation));
                        if (in_remain >= SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE) {
                                smb2_set_uint32(iov, offset + 0, offset + fs_size);
                        }
                        else {
                                smb2_set_uint32(iov, offset + 0, 0);
                        }

                        switch (info_class)
                        {
                        case SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION:
                                fs_size = PAD_TO_32BIT(SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE + fname_len);
                                smb2_set_uint32(iov, offset + 4, fs->file_index);
                                smb2_set_uint64(iov, offset + 8, smb2_timeval_to_win(&fs->creation_time));
                                smb2_set_uint64(iov, offset + 16, smb2_timeval_to_win(&fs->last_access_time));
                                smb2_set_uint64(iov, offset + 24, smb2_timeval_to_win(&fs->last_write_time));
                                smb2_set_uint64(iov, offset + 32, smb2_timeval_to_win(&fs->change_time));
                                smb2_set_uint64(iov, offset + 40, fs->end_of_file);
                                smb2_set_uint64(iov, offset + 48, fs->allocation_size);
                                smb2_set_uint32(iov, offset + 56, fs->file_attributes);
                                smb2_set_uint32(iov, offset + 60, fname_len);
                                smb2_set_uint32(iov, offset + 64, fs->ea_size);
                                smb2_set_uint32(iov, offset + 68, 0); /* reserved */
                                smb2_set_uint64(iov, offset + 72, fs->file_id);
                                if (name && fname_len > 0) {
                                        memcpy(buf + offset + SMB2_FILEID_FULL_DIRECTORY_INFORMATION_SIZE, &name->val[0], fname_len);
                                }
                                break;
                        case SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION:
                                fs_size = PAD_TO_32BIT(SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE + fname_len);
                                smb2_set_uint32(iov, offset + 4, fs->file_index);
                                smb2_set_uint64(iov, offset + 8, smb2_timeval_to_win(&fs->creation_time));
                                smb2_set_uint64(iov, offset + 16, smb2_timeval_to_win(&fs->last_access_time));
                                smb2_set_uint64(iov, offset + 24, smb2_timeval_to_win(&fs->last_write_time));
                                smb2_set_uint64(iov, offset + 32, smb2_timeval_to_win(&fs->change_time));
                                smb2_set_uint64(iov, offset + 40, fs->end_of_file);
                                smb2_set_uint64(iov, offset + 48, fs->allocation_size);
                                smb2_set_uint32(iov, offset + 56, fs->file_attributes);
                                smb2_set_uint32(iov, offset + 60, fname_len);
                                smb2_set_uint32(iov, offset + 64, fs->ea_size);
                                smb2_set_uint8(iov, offset + 68, fs->short_name_length);
                                smb2_set_uint8(iov, offset + 69, 0); /* reserved */
                                memcpy(iov->buf + offset + 70, fs->short_name, sizeof(fs->short_name));
                                smb2_set_uint16(iov, offset + 94, 0); /* reserved */
                                smb2_set_uint64(iov, offset + 96, fs->file_id);
                                if (name && fname_len > 0) {
                                        memcpy(buf + offset + SMB2_FILEID_BOTH_DIRECTORY_INFORMATION_SIZE, &name->val[0], fname_len);
                                }
                                break;
                        default:
                                break;
                        }

                        if (name) {
                                free(name);
                        }

                        offset += fs_size;
                }
                while (in_remain >= sizeof(struct smb2_fileidbothdirectoryinformation));
        }
        return 0;
}

struct smb2_pdu *
smb2_cmd_query_directory_reply_async(struct smb2_context *smb2,
                                struct smb2_query_directory_request *req,
                                struct smb2_query_directory_reply *rep,
                                smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;
        pdu = smb2_allocate_pdu(smb2, SMB2_QUERY_DIRECTORY, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_query_directory_reply(smb2, req->file_information_class,
                                req->flags, req->output_buffer_length, pdu, rep)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

#define IOV_OFFSET (rep->output_buffer_offset - SMB2_HEADER_SIZE - \
                    (SMB2_QUERY_DIRECTORY_REPLY_SIZE & 0xfffe))

int
smb2_process_query_directory_fixed(struct smb2_context *smb2,
                                   struct smb2_pdu *pdu)
{
        struct smb2_query_directory_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_QUERY_DIRECTORY_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Query Dir "
                               "reply. Expected %d, got %d",
                               SMB2_QUERY_DIRECTORY_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate query dir reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint16(iov, 2, &rep->output_buffer_offset);
        smb2_get_uint32(iov, 4, &rep->output_buffer_length);
        if (rep->output_buffer_length &&
            (rep->output_buffer_offset + rep->output_buffer_length > smb2->spl)) {
                smb2_set_error(smb2, "Output buffer extends beyond end of "
                               "PDU");
                free(rep);
                return -1;
        }

        if (rep->output_buffer_length == 0) {
                return 0;
        }

        if (rep->output_buffer_offset < SMB2_HEADER_SIZE +
            (SMB2_QUERY_INFO_REPLY_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Output buffer overlaps with "
                               "Query Dir reply header");
                free(rep);
                return -1;
        }

        /* Return the amount of data that the output buffer will take up.
         * Including any padding before the output buffer itself.
         */
        return IOV_OFFSET + rep->output_buffer_length;
}

int
smb2_process_query_directory_variable(struct smb2_context *smb2,
                                      struct smb2_pdu *pdu)
{
        struct smb2_query_directory_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        rep->output_buffer = &iov->buf[IOV_OFFSET];

        return 0;
}

#define IOVREQ_OFFSET (req->file_name_offset - SMB2_HEADER_SIZE - \
                    (SMB2_QUERY_DIRECTORY_REQUEST_SIZE & 0xfffe))

int
smb2_process_query_directory_request_fixed(struct smb2_context *smb2,
                                   struct smb2_pdu *pdu)
{
        struct smb2_query_directory_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_QUERY_DIRECTORY_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Query Dir "
                               "request. Expected %d, got %d",
                               SMB2_QUERY_DIRECTORY_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate query dir request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint8(iov, 2, &req->file_information_class);
        smb2_get_uint8(iov, 3, &req->flags);
        smb2_get_uint32(iov, 4, &req->file_index);
        memcpy(req->file_id, &iov->buf[8], SMB2_FD_SIZE);
        smb2_get_uint16(iov, 24, &req->file_name_offset);
        smb2_get_uint16(iov, 26, &req->file_name_length);
        smb2_get_uint32(iov, 28, &req->output_buffer_length);

        if (req->file_name_length &&
            (req->file_name_offset + req->file_name_length > (uint16_t)smb2->spl)) {
                smb2_set_error(smb2, "Filename extends beyond end of "
                               "PDU");
                free(req);
                return -1;
        }

        if (req->file_name_length == 0) {
                return 0;
        }

        if (req->file_name_offset < SMB2_HEADER_SIZE +
            (SMB2_QUERY_DIRECTORY_REQUEST_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Name buffer overlaps with "
                               "Query Dir request header");
                free(req);
                return -1;
        }

        /* Return the amount of data that the name will take up.
         * Including any padding before the name itself.
         */
        return IOVREQ_OFFSET + req->file_name_length;
}

int
smb2_process_query_directory_request_variable(struct smb2_context *smb2,
                                      struct smb2_pdu *pdu)
{
        struct smb2_query_directory_request *req = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        void *ptr;
        int name_byte_len;

        if (req->file_name_length > 0) {
                req->name = smb2_utf16_to_utf8((uint16_t*)(void *)&iov->buf[IOVREQ_OFFSET], req->file_name_length / 2);
                if (req->name) {
                        name_byte_len = strlen(req->name) + 1;
                        ptr = smb2_alloc_init(smb2, name_byte_len);
                        if (ptr) {
                                memcpy(ptr, req->name, name_byte_len);
                        }
                        free(discard_const(req->name));
                        req->name = ptr;
                        if (!ptr) {
                                smb2_set_error(smb2, "can not alloc name buffer");
                                return -1;
                        }
                }
                else {
                        smb2_set_error(smb2, "can not convert name to utf8");
                        return -1;
                }
        }
        return 0;
}

