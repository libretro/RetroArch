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
smb2_encode_session_setup_request(struct smb2_context *smb2,
                                  struct smb2_pdu *pdu,
                                  struct smb2_session_setup_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_SESSION_SETUP_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate session "
                               "setup buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for session setup request");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_SESSION_SETUP_REQUEST_SIZE);
        smb2_set_uint8(iov, 2, req->flags);
        smb2_set_uint8(iov, 3, req->security_mode);
        smb2_set_uint32(iov, 4, req->capabilities);
        smb2_set_uint32(iov, 8, req->channel);
        smb2_set_uint16(iov, 12, SMB2_HEADER_SIZE + 24);
        smb2_set_uint16(iov, 14, req->security_buffer_length);
        smb2_set_uint64(iov, 16, req->previous_session_id);


        /* Security buffer */
        buf = malloc(req->security_buffer_length);
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate secbuf");
                return -1;
        }
        memcpy(buf, req->security_buffer, req->security_buffer_length);
        iov = smb2_add_iovector(smb2, &pdu->out,
                                buf,
                                req->security_buffer_length,
                                free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for session setup security buffer");
                return -1;
        }
        return 0;
}

struct smb2_pdu *
smb2_cmd_session_setup_async(struct smb2_context *smb2,
                             struct smb2_session_setup_request *req,
                             smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_session_setup_request(smb2, pdu, req)) {
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
smb2_encode_session_setup_reply(struct smb2_context *smb2,
                              struct smb2_pdu *pdu,
                              struct smb2_session_setup_reply *rep)
{
        uint8_t *buf;
        int len;
        struct smb2_iovec *iov;

        len = SMB2_SESSION_SETUP_REPLY_SIZE & 0xfffe;
        len = PAD_TO_32BIT(len);

        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate session_setup buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for session setup reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_SESSION_SETUP_REPLY_SIZE);
        smb2_set_uint16(iov, 2, rep->session_flags);
        rep->security_buffer_offset = len + SMB2_HEADER_SIZE;
        smb2_set_uint16(iov, 4, rep->security_buffer_offset);
        smb2_set_uint16(iov, 6, rep->security_buffer_length);

        if (rep->security_buffer_length) {
                len = rep->security_buffer_length;
                len = PAD_TO_32BIT(len);
                /* Security buffer */
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate secbuf");
                        return -1;
                }
                memcpy(buf, rep->security_buffer, rep->security_buffer_length);
                memset(buf + rep->security_buffer_length, 0, len - rep->security_buffer_length);
                iov = smb2_add_iovector(smb2, &pdu->out,
                                        buf,
                                        len,
                                        free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for session setup reply buffer");
                        return -1;
                }
        }
        /* TODO append neg contexts? */
        return 0;
}

struct smb2_pdu *
smb2_cmd_session_setup_reply_async(struct smb2_context *smb2,
                         struct smb2_session_setup_reply *rep,
                         smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_SESSION_SETUP, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_session_setup_reply(smb2, pdu, rep)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

#define IOV_OFFSET (rep->security_buffer_offset - SMB2_HEADER_SIZE - \
                    (SMB2_SESSION_SETUP_REPLY_SIZE & 0xfffe))

int
smb2_process_session_setup_fixed(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu)
{
        struct smb2_session_setup_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_SESSION_SETUP_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Session Setup "
                               "reply. Expected %d, got %d",
                               SMB2_SESSION_SETUP_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate session setup reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint16(iov, 2, &rep->session_flags);
        smb2_get_uint16(iov, 4, &rep->security_buffer_offset);
        smb2_get_uint16(iov, 6, &rep->security_buffer_length);
        if (rep->security_buffer_length &&
            (rep->security_buffer_offset + rep->security_buffer_length > (uint16_t)smb2->spl)) {
                smb2_set_error(smb2, "Security buffer extends beyond end of "
                               "PDU");
                pdu->payload = NULL;
                free(rep);
                return -1;
        }
        /* Update session ID to use for future PDUs */
        smb2->session_id = smb2->hdr.session_id;

        if (rep->security_buffer_length == 0) {
                return 0;
        }
        if (rep->security_buffer_offset < SMB2_HEADER_SIZE +
            (SMB2_SESSION_SETUP_REPLY_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Security buffer overlaps with "
                               "Session Setup reply header");
                pdu->payload = NULL;
                free(rep);
                return -1;
        }

        /* Return the amount of data that the security buffer will take up.
         * Including any padding before the security buffer itself.
         */
        return IOV_OFFSET + rep->security_buffer_length;
}

int
smb2_process_session_setup_variable(struct smb2_context *smb2,
                                    struct smb2_pdu *pdu)
{
        struct smb2_session_setup_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        rep->security_buffer = &iov->buf[IOV_OFFSET];

        return 0;
}

int
smb2_process_session_setup_request_fixed(struct smb2_context *smb2,
                               struct smb2_pdu *pdu)
{
        struct smb2_session_setup_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_SESSION_SETUP_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Session setup "
                               "request. Expected %d, got %d",
                               SMB2_SESSION_SETUP_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate session setup request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint8(iov, 2, &req->flags);
        smb2_get_uint8(iov, 3, &req->security_mode);
        smb2_get_uint32(iov, 4, &req->capabilities);
        smb2_get_uint32(iov, 8, &req->channel);
/*        smb2_get_uint16(iov, 12, &req->security_buffer_offset); */
        smb2_get_uint16(iov, 14, &req->security_buffer_length);
        smb2_get_uint64(iov, 18, &req->previous_session_id);

        return req->security_buffer_length;
}

int
smb2_process_session_setup_request_variable(struct smb2_context *smb2,
                               struct smb2_pdu *pdu)
{
        struct smb2_session_setup_request *req = (struct smb2_session_setup_request*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        req->security_buffer = iov->buf;
        return 0;
}

