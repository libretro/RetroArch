/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Brian Dodge <bdodge09@gmail.com>

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
smb2_encode_lock_request(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_lock_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;
        struct smb2_lock_element *elements;
        uint32_t u32;
        int i;
        uint32_t offset;

        len = SMB2_LOCK_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate lock buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for lock request");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_LOCK_REQUEST_SIZE);
        smb2_set_uint16(iov, 2, req->lock_count);
        u32 = (req->lock_sequence_number << 28) | req->lock_sequence_index;
        smb2_set_uint32(iov, 4, u32);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);

        elements = req->locks;

        if (req->lock_count && req->locks) {
                smb2_set_uint64(iov, 24, elements->offset);
                smb2_set_uint64(iov, 24 + 8, elements->length);
                smb2_set_uint32(iov, 24 + 16, elements->flags);
                elements++;
        }

        if ((req->lock_count > 1) && req->locks) {
                len = PAD_TO_64BIT(SMB2_LOCK_ELEMENT_SIZE * req->lock_count);
                buf = calloc(len, sizeof(uint8_t));
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate locks buffer");
                        return -1;
                }
                iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
                if (iov == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for lock elements");
                        return -1;
                }

                for (i = 0, offset = 0; i < req->lock_count - 1; i++) {
                        smb2_set_uint64(iov, offset, elements->offset);
                        smb2_set_uint64(iov, offset + 8, elements->length);
                        smb2_set_uint32(iov, offset + 16, elements->flags);
                        offset += SMB2_LOCK_ELEMENT_SIZE;
                        elements++;
                }
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_lock_async(struct smb2_context *smb2,
                     struct smb2_lock_request *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_LOCK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_lock_request(smb2, pdu, req)) {
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
smb2_encode_lock_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_LOCK_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate lock buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for lock reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_LOCK_REPLY_SIZE);
        return 0;
}

struct smb2_pdu *
smb2_cmd_lock_reply_async(struct smb2_context *smb2,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_LOCK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_lock_reply(smb2, pdu)) {
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
smb2_process_lock_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_LOCK_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of lock "
                               "reply. Expected %d, got %d",
                               SMB2_LOCK_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        return 0;
}

static int
smb2_parse_locks(struct smb2_context *smb2,
                struct smb2_iovec *iov, uint32_t offset, int count, void *locks)
{
        struct smb2_lock_element *elements;

        if (!iov || !locks) {
                return -1;
        }
        elements = (struct smb2_lock_element *)locks;
        while (count-- > 0) {
                smb2_get_uint64(iov, offset + 0, &elements->offset);
                smb2_get_uint64(iov, offset + 8, &elements->length);
                smb2_get_uint32(iov, offset + 16, &elements->flags);
                offset += SMB2_LOCK_ELEMENT_SIZE;
                elements++;
        }
        return 0;
}

int
smb2_process_lock_request_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_lock_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;
        void *ptr;
        uint32_t u32;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_LOCK_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of lock "
                               "request. Expected %d, got %d",
                               SMB2_LOCK_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate lock request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint16(iov, 2, &req->lock_count);
        smb2_get_uint32(iov, 4, &u32);
        req->lock_sequence_number = u32 >> 28;
        req->lock_sequence_index = u32 & 0x0FFFFFFF;
        memcpy(req->file_id, iov->buf + 8, SMB2_FD_SIZE);

        if (req->lock_count < 1) {
                smb2_set_error(smb2, "Lock request must have at least one lock.");
                pdu->payload = NULL;
                free(req);
                return -1;
        }

        ptr = smb2_alloc_init(smb2,
                        req->lock_count * sizeof(struct smb2_lock_element));
        if (!ptr) {
                smb2_set_error(smb2, "can not alloc lock buffer.");
                pdu->payload = NULL;
                free(req);
                return -1;
        }
        req->locks = ptr;

        /* the fixed size includes 1 lock (the most common use) so
         * there is no more data in that case
         */
        if (req->lock_count > 0) {
                struct smb2_iovec vec;

                vec.buf = iov->buf + SMB2_LOCK_ELEMENT_SIZE;
                vec.len = iov->len - SMB2_LOCK_ELEMENT_SIZE;
                smb2_parse_locks(smb2, &vec, 0, 1, ptr);
        }
        return SMB2_LOCK_ELEMENT_SIZE * (req->lock_count - 1);
}

int
smb2_process_lock_request_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_lock_request *req = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        /* parse remaining locks, there is already one parsed */
        return smb2_parse_locks(smb2, iov, 0, req->lock_count - 1,
               (uint8_t*)(req->locks + 1));
}

