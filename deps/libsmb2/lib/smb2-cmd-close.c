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
smb2_encode_close_request(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_close_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_CLOSE_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate close buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for close request");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_CLOSE_REQUEST_SIZE);
        smb2_set_uint16(iov, 2, req->flags);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);

        return 0;
}

struct smb2_pdu *
smb2_cmd_close_async(struct smb2_context *smb2,
                     struct smb2_close_request *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CLOSE, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_close_request(smb2, pdu, req)) {
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
smb2_encode_close_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_close_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_CLOSE_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate close reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for close reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_CLOSE_REPLY_SIZE);
        smb2_set_uint16(iov, 2, rep->flags);
        /* 4 bytes reserved at offset 4 */
        smb2_set_uint64(iov, 8, rep->creation_time);
        smb2_set_uint64(iov, 16, rep->last_access_time);
        smb2_set_uint64(iov, 24, rep->last_write_time);
        smb2_set_uint64(iov, 32, rep->change_time);
        smb2_set_uint64(iov, 40, rep->allocation_size);
        smb2_set_uint64(iov, 48, rep->end_of_file);
        smb2_set_uint32(iov, 56, rep->file_attributes);

        return 0;
}

struct smb2_pdu *
smb2_cmd_close_reply_async(struct smb2_context *smb2,
                     struct smb2_close_reply *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_CLOSE, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_close_reply(smb2, pdu, rep)) {
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
smb2_process_close_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_close_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CLOSE_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Close "
                               "reply. Expected %d, got %d",
                               SMB2_CLOSE_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate close reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint16(iov, 2, &rep->flags);
        smb2_get_uint64(iov, 8, &rep->creation_time);
        smb2_get_uint64(iov, 16, &rep->last_access_time);
        smb2_get_uint64(iov, 24, &rep->last_write_time);
        smb2_get_uint64(iov, 32, &rep->change_time);
        smb2_get_uint64(iov, 40, &rep->allocation_size);
        smb2_get_uint64(iov, 48, &rep->end_of_file);
        smb2_get_uint32(iov, 56, &rep->file_attributes);

        return 0;
}

int
smb2_process_close_request_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_close_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_CLOSE_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Close "
                               "request. Expected %d, got %d",
                               SMB2_CLOSE_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate close request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint16(iov, 2, &req->flags);
        memcpy(req->file_id, iov->buf + 8, SMB2_FD_SIZE);

        return 0;
}

