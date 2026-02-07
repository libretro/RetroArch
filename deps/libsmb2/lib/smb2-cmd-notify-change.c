/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Brian Dodge <bdodge09g@gmail.com>

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
smb2_encode_change_notify_request(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_change_notify_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_CHANGE_NOTIFY_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate "
                                "change-notify buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for change-notify request");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_CHANGE_NOTIFY_REQUEST_SIZE);
        smb2_set_uint16(iov, 2, req->flags);
        smb2_set_uint32(iov, 4, req->output_buffer_length);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);
        smb2_set_uint32(iov, 24, req->completion_filter);

        return 0;
}

struct smb2_pdu *
smb2_cmd_change_notify_async(struct smb2_context *smb2,
                     struct smb2_change_notify_request *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CHANGE_NOTIFY, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_change_notify_request(smb2, pdu, req)) {
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
smb2_encode_change_notify_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_change_notify_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;
        len = SMB2_CHANGE_NOTIFY_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate "
                                "change-notify reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for change-notify reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_CHANGE_NOTIFY_REPLY_SIZE);
        rep->output_buffer_offset = SMB2_HEADER_SIZE + SMB2_CHANGE_NOTIFY_REQUEST_SIZE;
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
                smb2_set_error(smb2, "Failed to add iovector for change-notify output buffer");
                return -1;
        }

        if (smb2->passthrough) {
                memcpy(buf, rep->output, rep->output_buffer_length);
                memset(buf + rep->output_buffer_length, 0, len - rep->output_buffer_length);
                iov->len = rep->output_buffer_length;
        }
        else {
                smb2_set_error(smb2, "Change-notify buffer "
                                "packing not implemented");
                return -1;
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_change_notify_reply_async(struct smb2_context *smb2,
                     struct smb2_change_notify_reply *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CHANGE_NOTIFY, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_change_notify_reply(smb2, pdu, rep)) {
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
smb2_process_change_notify_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_change_notify_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CHANGE_NOTIFY_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of change "
                               "notify reply. Expected %d, got %d",
                               SMB2_CHANGE_NOTIFY_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate "
                               "change-notify reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint16(iov, 2, &rep->output_buffer_offset);
        smb2_get_uint32(iov, 4, &rep->output_buffer_length);

        return rep->output_buffer_length;
}

int
smb2_process_change_notify_variable(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu)
{
        struct smb2_change_notify_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        rep->output = (uint8_t *)iov->buf;
        return 0;
}

int
smb2_process_change_notify_request_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_change_notify_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CHANGE_NOTIFY_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of change "
                               "notify request. Expected %d, got %d",
                               SMB2_CHANGE_NOTIFY_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate "
                                "change-notify request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint16(iov, 2, &req->flags);
        memcpy(req->file_id, iov->buf + 8, SMB2_FD_SIZE);
        smb2_get_uint32(iov, 24, &req->completion_filter);

        return 0;
}


