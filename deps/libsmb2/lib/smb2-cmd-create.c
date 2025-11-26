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

#define CCX_OFFSET()    \
        PAD_TO_64BIT(SMB2_HEADER_SIZE + (SMB2_CREATE_REQUEST_SIZE & 0xfffe)     \
                + (req->name_length ? req->name_length : 1));

static int
smb2_encode_create_request(struct smb2_context *smb2,
                           struct smb2_pdu *pdu,
                           struct smb2_create_request *req)
{
        int i, len;
        uint8_t *buf;
        uint16_t ch;
        struct smb2_utf16 *name = NULL;
        uint32_t name_byte_len = 0;
        struct smb2_iovec *iov;

        len = SMB2_CREATE_REQUEST_SIZE & 0xfffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate create buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for create request");
                return -1;
        }

        /* Name */
        if (req->name && req->name[0]) {
                name = smb2_utf8_to_utf16(req->name);
                if (name == NULL) {
                        smb2_set_error(smb2, "Could not convert name into UTF-16");
                        return -1;
                }
                name_byte_len = 2 * name->len;
                /* name length */
                req->name_length = name_byte_len;
                smb2_set_uint16(iov, 46, req->name_length);
        }

        smb2_set_uint16(iov, 0, SMB2_CREATE_REQUEST_SIZE);
        smb2_set_uint8(iov, 2, req->security_flags);
        smb2_set_uint8(iov, 3, req->requested_oplock_level);
        smb2_set_uint32(iov, 4, req->impersonation_level);
        smb2_set_uint64(iov, 8, req->smb_create_flags);
        smb2_set_uint32(iov, 24, req->desired_access);
        smb2_set_uint32(iov, 28, req->file_attributes);
        smb2_set_uint32(iov, 32, req->share_access);
        smb2_set_uint32(iov, 36, req->create_disposition);
        smb2_set_uint32(iov, 40, req->create_options);
        /* name offset */
        req->name_offset = PAD_TO_32BIT(SMB2_HEADER_SIZE + (SMB2_CREATE_REQUEST_SIZE & 0xfffe));
        smb2_set_uint16(iov, 44, req->name_offset);
        /* context offset */
        if (req->create_context_length) {
                if (req->name_length == 0) {
                        req->create_context_offset = PAD_TO_64BIT(4 + req->name_offset);
                }
                else {
                        req->create_context_offset = PAD_TO_64BIT(req->name_length + req->name_offset);
                }
        }
        else {
                req->create_context_offset = 0;
        }
        smb2_set_uint32(iov, 48, req->create_context_offset);
        smb2_set_uint32(iov, 52, req->create_context_length);

        /* Name */
        if (name) {
                len = PAD_TO_64BIT(name_byte_len);
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate create name");
                        free(name);
                        return -1;
                }
                memcpy(buf, &name->val[0], name_byte_len);
                memset(buf + name_byte_len, 0, len - name_byte_len);
                iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        len,
                                        free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for create name");
                        return -1;
                }
                /* Convert '/' to '\' */
                for (i = 0; i < name->len; i++) {
                        smb2_get_uint16(iov, i * 2, &ch);
                        if (ch == 0x002f) {
                                smb2_set_uint16(iov, i * 2, 0x005c);
                        }
                }
                free(name);
        }
        else {
                /* have to have at least one byte for name even if len is 0
                 * this code pads out to a 64 bit boundary to place the
                 * create contexts on which they require */
                static uint8_t zero[8];
                iov = smb2_add_iovector(smb2, &pdu->out,
                                                zero, 8, NULL);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for create empty name padding");
                        return -1;
                }
        }
        /* Create Context: note there is no encoding, we just pass along */
        if (req->create_context_length) {
                len = PAD_TO_64BIT(req->create_context_length);
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate create context");
                        return -1;
                }
                memcpy(buf, req->create_context, req->create_context_length);
                memset(buf + req->create_context_length, 0, len - req->create_context_length);
                iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        len,
                                        free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for create context");
                        return -1;
                }
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_create_async(struct smb2_context *smb2,
                      struct smb2_create_request *req,
                      smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CREATE, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_create_request(smb2, pdu, req)) {
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
smb2_encode_create_reply(struct smb2_context *smb2,
                           struct smb2_pdu *pdu,
                           struct smb2_create_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_CREATE_REPLY_SIZE & 0xfffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate create buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for create reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_CREATE_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->oplock_level);
        smb2_set_uint8(iov, 3, rep->flags);
        smb2_set_uint32(iov, 4, rep->create_action);
        smb2_set_uint64(iov, 8, rep->creation_time);
        smb2_set_uint64(iov, 16, rep->last_access_time);
        smb2_set_uint64(iov, 24, rep->last_write_time);
        smb2_set_uint64(iov, 32, rep->change_time);
        smb2_set_uint64(iov, 40, rep->allocation_size);
        smb2_set_uint64(iov, 48, rep->end_of_file);
        smb2_set_uint32(iov, 56, rep->file_attributes);
        memcpy(&iov->buf[64], rep->file_id, SMB2_FD_SIZE);
        rep->create_context_offset = PAD_TO_64BIT((SMB2_CREATE_REPLY_SIZE & 0xfffe) + SMB2_HEADER_SIZE);
        smb2_set_uint32(iov, 80, rep->create_context_offset);
        smb2_set_uint32(iov, 84, rep->create_context_length);

        /* Create Context */
        if (rep->create_context_length) {
                len = PAD_TO_64BIT(rep->create_context_length);
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate create context");
                        return -1;
                }
                memcpy(buf, rep->create_context, rep->create_context_length);
                memset(buf + rep->create_context_length, 0, len - rep->create_context_length);
                iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        len,
                                        free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for create reply context");
                        return -1;
                }
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_create_reply_async(struct smb2_context *smb2,
                      struct smb2_create_reply *rep,
                      smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CREATE, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_create_reply(smb2, pdu, rep)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

#define IOV_OFFSET (rep->create_context_offset - SMB2_HEADER_SIZE - \
                    (SMB2_CREATE_REPLY_SIZE & 0xfffe))

int
smb2_process_create_fixed(struct smb2_context *smb2,
                          struct smb2_pdu *pdu)
{
        struct smb2_create_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CREATE_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Create. "
                               "Expected %d, got %d",
                               SMB2_CREATE_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate create reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint8(iov, 2, &rep->oplock_level);
        smb2_get_uint8(iov, 3, &rep->flags);
        smb2_get_uint32(iov, 4, &rep->create_action);
        smb2_get_uint64(iov, 8, &rep->creation_time);
        smb2_get_uint64(iov, 16, &rep->last_access_time);
        smb2_get_uint64(iov, 24, &rep->last_write_time);
        smb2_get_uint64(iov, 32, &rep->change_time);
        smb2_get_uint64(iov, 40, &rep->allocation_size);
        smb2_get_uint64(iov, 48, &rep->end_of_file);
        smb2_get_uint32(iov, 56, &rep->file_attributes);
        memcpy(rep->file_id, iov->buf + 64, SMB2_FD_SIZE);
        smb2_get_uint32(iov, 80, &rep->create_context_offset);
        smb2_get_uint32(iov, 84, &rep->create_context_length);

        if (rep->create_context_length == 0) {
                return 0;
        }

        if (rep->create_context_offset < SMB2_HEADER_SIZE +
            (SMB2_CREATE_REPLY_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Create context overlaps with "
                               "reply header");
                pdu->payload = NULL;
                free(rep);
                return -1;
        }

        /* Return the amount of data that the security buffer will take up.
         * Including any padding before the security buffer itself.
         */
        return IOV_OFFSET + rep->create_context_length;
}

int
smb2_process_create_variable(struct smb2_context *smb2,
                             struct smb2_pdu *pdu)
{
        struct smb2_create_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        struct smb2_iovec vec;

        vec.buf = iov->buf + IOV_OFFSET;
        vec.len = iov->len - IOV_OFFSET;

        rep->create_context = NULL;
        if (rep->create_context_length) {
                rep->create_context = vec.buf;
        }

        return 0;
}

#define IOVREQ_OFFSET (req->name_offset - SMB2_HEADER_SIZE - \
                    (SMB2_CREATE_REQUEST_SIZE & 0xfffe))

int
smb2_process_create_request_fixed(struct smb2_context *smb2,
                          struct smb2_pdu *pdu)
{
        struct smb2_create_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;
        int remaining;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CREATE_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Create "
                               "Request. Expected %d, got %d",
                               SMB2_CREATE_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req== NULL) {
                smb2_set_error(smb2, "Failed to allocate create request");
                return -1;
        }
        pdu->payload = req;

        req->name = ""; /* avoid seg faults if accessed before set */
        smb2_get_uint8(iov, 2, &req->security_flags);
        smb2_get_uint8(iov, 3, &req->requested_oplock_level);
        smb2_get_uint32(iov, 4, &req->impersonation_level);
        smb2_get_uint64(iov, 8, &req->smb_create_flags);
        /* reserved 64bits at 16 */
        smb2_get_uint32(iov, 24, &req->desired_access);
        smb2_get_uint32(iov, 28, &req->file_attributes);
        smb2_get_uint32(iov, 32, &req->share_access);
        smb2_get_uint32(iov, 36, &req->create_disposition);
        smb2_get_uint32(iov, 40, &req->create_options);
        smb2_get_uint16(iov, 44, &req->name_offset);
        smb2_get_uint16(iov, 46, &req->name_length);
        smb2_get_uint32(iov, 48, &req->create_context_offset);
        smb2_get_uint32(iov, 52, &req->create_context_length);
        req->name = NULL;

        if (req->create_context_length == 0 && req->name_length == 0) {
                return 0;
        }

        if (req->name_length > 0) {
                if (req->name_offset < SMB2_HEADER_SIZE +
                    (SMB2_CREATE_REQUEST_SIZE & 0xfffe)) {
                        smb2_set_error(smb2, "name overlaps with "
                                       "request header");
                        pdu->payload = NULL;
                        free(req);
                        return -1;
                }
        }

        if (req->create_context_length > 0) {
                if (req->create_context_offset < SMB2_HEADER_SIZE +
                    (SMB2_CREATE_REQUEST_SIZE & 0xfffe)) {
                        smb2_set_error(smb2, "Create context overlaps with "
                                       "request header");
                        pdu->payload = NULL;
                        free(req);
                        return -1;
                }
        }

        /* Return the amount of data that the name will take up.
         * Including any padding before the name itself, and between name and create contexts
         */
        remaining = IOVREQ_OFFSET;
        if (req->create_context_offset > req->name_offset) {
                remaining += PAD_TO_64BIT(req->create_context_offset - req->name_offset);
        } else {
                remaining += req->name_length;
        }
        remaining += req->create_context_length;
        return remaining;
}

int
smb2_process_create_request_variable(struct smb2_context *smb2,
                          struct smb2_pdu *pdu)
{
        struct smb2_create_request *req = (struct smb2_create_request*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint32_t offset;
        void *ptr;
        int name_byte_len;

        req->name = NULL;
        if (req->name_length > 0) {
                req->name = smb2_utf16_to_utf8((const uint16_t *)(void *)iov->buf, req->name_length / 2);
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

        /* we dont parse the create contexts but we tack them on in case the
         * the caller wants to pass them along
         */
        req->create_context = NULL;
        if (req->create_context_length && req->create_context_offset) {
                offset = req->create_context_offset - SMB2_HEADER_SIZE -
                        (SMB2_CREATE_REQUEST_SIZE & 0xfffe);
                req->create_context = iov->buf + offset;
        }
        return 0;
}

