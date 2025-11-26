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
smb2_encode_read_request(struct smb2_context *smb2,
                         struct smb2_pdu *pdu,
                         struct smb2_read_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_READ_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate read buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                return -1;
        }

        if (!smb2->supports_multi_credit && req->length > 64 * 1024) {
                req->length = 64 * 1024;
                req->minimum_count = 0;
        }
        smb2_set_uint16(iov, 0, SMB2_READ_REQUEST_SIZE);
        smb2_set_uint8(iov, 3, req->flags);
        smb2_set_uint32(iov, 4, req->length);
        smb2_set_uint64(iov, 8, req->offset);
        memcpy(iov->buf + 16, req->file_id, SMB2_FD_SIZE);
        smb2_set_uint32(iov, 32, req->minimum_count);
        smb2_set_uint32(iov, 36, req->channel);
        smb2_set_uint32(iov, 40, req->remaining_bytes);
        smb2_set_uint16(iov, 46, req->read_channel_info_length);

        if (req->read_channel_info_length > 0 &&
            req->read_channel_info != NULL) {
                if (smb2->passthrough) {
                        req->read_channel_info_offset =
                                (SMB2_READ_REQUEST_SIZE & 0xfffffffe) + SMB2_HEADER_SIZE;
                        smb2_set_uint16(iov, 44, req->read_channel_info_offset);

                        len = PAD_TO_64BIT(req->read_channel_info_length);
                        buf = malloc(len);
                        if (buf == NULL) {
                                smb2_set_error(smb2, "Failed to allocate read channel context");
                                return -1;
                        }
                        memcpy(buf, req->read_channel_info, req->read_channel_info_length);
                        memset(buf + req->read_channel_info_length, 0,
                                       len - req->read_channel_info_length);
                        iov = smb2_add_iovector(smb2, &pdu->out,
                                                        buf,
                                                        len,
                                                        free);
                        if (iov == NULL) {
                                return -1;
                        }
                }
                else {
                        smb2_set_error(smb2, "ChannelInfo not yet implemented");
                        return -1;
                }
        }

        /* The buffer must contain at least one byte, even if we do not
         * have any read channel info.
         */
        if (req->read_channel_info_length == 0) {
                static uint8_t zero;

                if (smb2_add_iovector(smb2, &pdu->out, &zero, 1, NULL) == NULL) {
                        return -1;
                }
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_read_async(struct smb2_context *smb2,
                    struct smb2_read_request *req,
                    smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_READ, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_read_request(smb2, pdu, req)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        /* Add a vector for the reply buffer that the application gave us */
        if (req->length) {
                if (!req->buf) {
                        /* need a place to put read data, so fail if app doesn't supply one */
                        smb2_set_error(smb2, "No buffer for read reply data");
                        smb2_free_pdu(smb2, pdu);
                        return NULL;
                }

                if (smb2_add_iovector(smb2, &pdu->in, req->buf, req->length, NULL) == NULL) {
                        smb2_free_pdu(smb2, pdu);
                        return NULL;
                }
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        /* Adjust credit charge for large payloads */
        if (smb2->supports_multi_credit) {
                pdu->header.credit_charge = (req->length - 1) / 65536 + 1; /* 3.1.5.2 of [MS-SMB2] */
        }

        return pdu;
}

static int
smb2_encode_read_reply(struct smb2_context *smb2,
                         struct smb2_pdu *pdu,
                         struct smb2_read_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_READ_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate read reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                return -1;
        }

        rep->data_offset = 0;
        if (rep->data_length && rep->data) {
                rep->data_offset = (SMB2_READ_REPLY_SIZE & 0xfffffffe) + SMB2_HEADER_SIZE;
        }
        smb2_set_uint16(iov, 0, SMB2_READ_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->data_offset);
        smb2_set_uint32(iov, 4, rep->data_length);
        smb2_set_uint32(iov, 8, rep->data_remaining);

        if (rep->data_length > 0 && rep->data) {
                if (smb2_add_iovector(smb2, &pdu->out, rep->data, rep->data_length, free) == NULL) {
                        return -1;
                }
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_read_reply_async(struct smb2_context *smb2,
                    struct smb2_read_reply *rep,
                    smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_READ, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_read_reply(smb2, pdu, rep)) {
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
smb2_process_read_fixed(struct smb2_context *smb2,
                        struct smb2_pdu *pdu)
{
        struct smb2_read_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size > SMB2_READ_REPLY_SIZE) {
                smb2_set_error(smb2, "Unexpected size of Read "
                               "reply. Expected %d, got %d",
                               SMB2_READ_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate read reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint8(iov, 2, &rep->data_offset);
        smb2_get_uint32(iov, 4, &rep->data_length);
        smb2_get_uint32(iov, 8, &rep->data_remaining);

        rep->data = NULL;
        if (rep->data_length == 0) {
                return 0;
        }

        if (rep->data_offset != SMB2_HEADER_SIZE + 16) {
                smb2_set_error(smb2, "Unexpected data offset in Read reply. "
                               "Expected %d, got %d",
                               SMB2_HEADER_SIZE + 16, rep->data_offset);
                pdu->payload = NULL;
                free(rep);
                return -1;
        }

        return rep->data_length;
}

static void free_read_reply(struct smb2_context *smb2, void * payload) {
    struct smb2_read_reply *rep;
    if (payload == NULL) {
        return;
    }

    rep = (struct smb2_read_reply*)payload;
    if (rep->data_length != 0 && rep->data != NULL) {
        free(rep->data);
    }
}

int
smb2_process_read_variable(struct smb2_context *smb2,
                        struct smb2_pdu *pdu)
{
        struct smb2_read_reply *rep = (struct smb2_read_reply*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        if (rep->data) {
                pdu->free_payload = free_read_reply;
                memcpy(rep->data, iov->buf, rep->data_length);
        }
        else {
                /* allow app to get data direct from iov to avoid copy */
                rep->data = iov->buf;
        }
        return 0;
}

#define IOVREQ_OFFSET ((req->read_channel_info_offset)?(req->read_channel_info_offset - SMB2_HEADER_SIZE - \
        (SMB2_READ_REQUEST_SIZE & 0xfffe)):0)
int
smb2_process_read_request_fixed(struct smb2_context *smb2,
                        struct smb2_pdu *pdu)
{
        struct smb2_read_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size > SMB2_READ_REQUEST_SIZE) {
                smb2_set_error(smb2, "Unexpected size of Read "
                               "request. Expected %d, got %d",
                               SMB2_READ_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = calloc(1, sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate read request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint8(iov, 3, &req->flags);
        smb2_get_uint32(iov, 4, &req->length);
        smb2_get_uint64(iov, 8, &req->offset);
        memcpy(req->file_id, iov->buf + 16, SMB2_FD_SIZE);
        smb2_get_uint32(iov, 32, &req->minimum_count);
        smb2_get_uint32(iov, 36, &req->channel);
        smb2_get_uint32(iov, 40, &req->remaining_bytes);
        if (req->read_channel_info_length) {
                req->read_channel_info_offset = (SMB2_READ_REQUEST_SIZE & 0xfffffffe) + SMB2_HEADER_SIZE;
                smb2_get_uint16(iov, 44, &req->read_channel_info_offset);
        }
        smb2_get_uint16(iov, 46, &req->read_channel_info_length);

        if (req->length > smb2->max_read_size) {
                smb2_set_error(smb2, "can not read more than %d bytes", smb2->max_read_size);
                pdu->payload = NULL;
                free(req);
                return -1;
        }

        if (req->read_channel_info_length == 0) {
                return 0;
        }

        if (req->read_channel_info_offset < SMB2_HEADER_SIZE + (SMB2_READ_REQUEST_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "channel info overlaps request", "");
                pdu->payload = NULL;
                free(req);
                return -1;
        }

        return IOVREQ_OFFSET + req->read_channel_info_length;
}

int
smb2_process_read_request_variable(struct smb2_context *smb2,
                        struct smb2_pdu *pdu)
{
        struct smb2_read_request *req = (struct smb2_read_request*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        req->read_channel_info = (uint8_t *)iov->buf;
        return 0;
}

