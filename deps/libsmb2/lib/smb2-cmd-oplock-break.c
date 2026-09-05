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

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"

static int
smb2_encode_oplock_break_acknowledgement(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_oplock_break_acknowledgement *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate oplock request buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for oplock break ack");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE);
        smb2_set_uint8(iov, 2, req->oplock_level);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);

        return 0;
}

struct smb2_pdu *
smb2_cmd_oplock_break_async(struct smb2_context *smb2,
                     struct smb2_oplock_break_acknowledgement *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_oplock_break_acknowledgement(smb2, pdu, req)) {
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
smb2_encode_oplock_break_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_oplock_break_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        /* this encodes both notifications and responses */
        len = SMB2_OPLOCK_BREAK_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate oplock reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for oplock break reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_OPLOCK_BREAK_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->oplock_level);
        memcpy(iov->buf + 8, rep->file_id, SMB2_FD_SIZE);

        return 0;
}

struct smb2_pdu *
smb2_cmd_oplock_break_reply_async(struct smb2_context *smb2,
                     struct smb2_oplock_break_reply *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_oplock_break_reply(smb2, pdu, rep)) {
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
smb2_encode_oplock_break_notification(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_oplock_break_notification *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        /* this encodes both notifications and responses */
        len = SMB2_OPLOCK_BREAK_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate oplock reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for oplock break notification");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_OPLOCK_BREAK_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->oplock_level);
        memcpy(iov->buf + 8, rep->file_id, SMB2_FD_SIZE);

        return 0;
}

struct smb2_pdu *
smb2_cmd_oplock_break_notification_async(struct smb2_context *smb2,
                     struct smb2_oplock_break_notification *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_oplock_break_notification(smb2, pdu, rep)) {
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
smb2_encode_lease_break_acknowledgement(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_lease_break_acknowledgement *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate lease notification buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for lease break ack");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE);
        smb2_set_uint32(iov, 4, req->flags);
        memcpy(iov->buf + 8, req->lease_key, SMB2_LEASE_KEY_SIZE);
        smb2_set_uint32(iov, 4, req->lease_state);
        smb2_set_uint32(iov, 4, req->lease_duration);

        return 0;
}

struct smb2_pdu *
smb2_cmd_lease_break_async(struct smb2_context *smb2,
                     struct smb2_lease_break_acknowledgement *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_lease_break_acknowledgement(smb2, pdu, req)) {
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
smb2_encode_lease_break_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_lease_break_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_LEASE_BREAK_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate "
                                "lease-break reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for lease break reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_LEASE_BREAK_REPLY_SIZE);
        smb2_set_uint32(iov, 4, rep->flags);
        memcpy(iov->buf + 8, rep->lease_key, SMB2_LEASE_KEY_SIZE);
        smb2_set_uint32(iov, 24, rep->lease_state);
        smb2_set_uint32(iov, 28, rep->lease_duration);

        return 0;
}

struct smb2_pdu *
smb2_cmd_lease_break_reply_async(struct smb2_context *smb2,
                     struct smb2_lease_break_reply *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_lease_break_reply(smb2, pdu, rep)) {
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
smb2_encode_lease_break_notification(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_lease_break_notification *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_LEASE_BREAK_NOTIFICATION_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate lease notification buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for lease break notification");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_LEASE_BREAK_NOTIFICATION_SIZE);
        smb2_set_uint16(iov, 2, req->new_epoch);
        smb2_set_uint32(iov, 4, req->flags);
        memcpy(iov->buf + 8, req->lease_key, SMB2_LEASE_KEY_SIZE);
        smb2_set_uint32(iov, 4, req->current_lease_state);
        smb2_set_uint32(iov, 4, req->new_lease_state);
        smb2_set_uint32(iov, 4, req->break_reason);
        smb2_set_uint32(iov, 4, req->access_mask_hint);
        smb2_set_uint32(iov, 4, req->share_mask_hint);

        return 0;
}

struct smb2_pdu *
smb2_cmd_lease_break_notification_async(struct smb2_context *smb2,
                     struct smb2_lease_break_notification *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_lease_break_notification(smb2, pdu, req)) {
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
smb2_process_oplock_break_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_oplock_or_lease_break_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate oplock-"
                                "or lease break reply");
                return -1;
        }
        rep->struct_size = struct_size;
        pdu->payload = rep;

        if (struct_size == SMB2_OPLOCK_BREAK_REPLY_SIZE) {
                /* same size as notification too */
                return struct_size - sizeof(uint16_t);
        }
        else if (struct_size == SMB2_LEASE_BREAK_NOTIFICATION_SIZE) {
                return struct_size - sizeof(uint16_t);
        }
        else if (struct_size == SMB2_LEASE_BREAK_REPLY_SIZE) {
                return struct_size - sizeof(uint16_t);
        }
        else {
                smb2_set_error(smb2, "Unexpected size of oplock or "
                               "lease break noiify/reply. Expected "
                                "%d or %d, got %d",
                               SMB2_OPLOCK_BREAK_REPLY_SIZE,
                               SMB2_LEASE_BREAK_REPLY_SIZE,
                               struct_size);
                pdu->payload = NULL;
                free(rep);
                return -1;
        }

        return 0;
}

int
smb2_process_oplock_break_variable(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_oplock_or_lease_break_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        /* note the stuct-size has already been read, so offsets are -2 */
        if (rep->struct_size == SMB2_OPLOCK_BREAK_NOTIFICATION_SIZE) {
                /* the oplock notify and response are both the same structure */
                if (smb2->hdr.message_id == 0xffffffffffffffffULL) {
                        rep->break_type = SMB2_BREAK_TYPE_OPLOCK_NOTIFICATION;
                } else {
                        rep->break_type = SMB2_BREAK_TYPE_OPLOCK_RESPONSE;
                }
                smb2_get_uint8(iov, 0, &rep->lock.oplock.oplock_level);
                memcpy(rep->lock.oplock.file_id, iov->buf + 6, SMB2_FD_SIZE);
        }
        else if (rep->struct_size == SMB2_LEASE_BREAK_NOTIFICATION_SIZE) {
                rep->break_type = SMB2_BREAK_TYPE_LEASE_NOTIFICATION;
                smb2_get_uint16(iov, 0, &rep->lock.lease.new_epoch);
                smb2_get_uint32(iov, 2, &rep->lock.lease.flags);
                memcpy(rep->lock.lease.lease_key, iov->buf + 6, SMB2_LEASE_KEY_SIZE);
                smb2_get_uint32(iov, 22, &rep->lock.lease.current_lease_state);
                smb2_get_uint32(iov, 26, &rep->lock.lease.new_lease_state);
                smb2_get_uint32(iov, 30, &rep->lock.lease.break_reason);
                smb2_get_uint32(iov, 34, &rep->lock.lease.access_mask_hint);
                smb2_get_uint32(iov, 38, &rep->lock.lease.share_mask_hint);
        }
        else if (rep->struct_size == SMB2_LEASE_BREAK_REPLY_SIZE) {
                rep->break_type = SMB2_BREAK_TYPE_LEASE_RESPONSE;
                smb2_get_uint32(iov, 2, &rep->lock.leaserep.flags);
                memcpy(rep->lock.lease.lease_key, iov->buf + 6, SMB2_LEASE_KEY_SIZE);
                smb2_get_uint32(iov, 22, &rep->lock.leaserep.lease_state);
                smb2_get_uint64(iov, 26, &rep->lock.leaserep.lease_duration);
        }
        else {
                return -1;
        }
        return 0;
}

int
smb2_process_oplock_break_request_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_oplock_or_lease_break_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate oplock-"
                               "lease-break request");
                return -1;
        }
        req->struct_size = struct_size;
        pdu->payload = req;

        if (struct_size == SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE) {
                return struct_size - sizeof(uint16_t);
        }
        else if (struct_size == SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE) {
                return struct_size - sizeof(uint16_t);
        }
        else {
                smb2_set_error(smb2, "Unexpected size of oplock or "
                               "lease break acknowledge. Expected "
                                "%d or %d, got %d",
                               SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE,
                               SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE,
                               (int)struct_size);
                pdu->payload = NULL;
                free(req);
                return -1;
        }
        return 0;
}

int
smb2_process_oplock_break_request_variable(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_oplock_or_lease_break_request *req = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        /* note the stuct-size has already been read, so offsets are -2 */
        if (req->struct_size == SMB2_OPLOCK_BREAK_ACKNOWLEDGE_SIZE) {
                req->break_type = SMB2_BREAK_TYPE_OPLOCK_ACKNOWLEDGE;
                smb2_get_uint8(iov, 0, &req->lock.oplock.oplock_level);
                memcpy(req->lock.oplock.file_id, iov->buf + 6, SMB2_FD_SIZE);
        }
        else if (req->struct_size == SMB2_LEASE_BREAK_ACKNOWLEDGE_SIZE) {
                req->break_type = SMB2_BREAK_TYPE_LEASE_ACKNOWLEDGE;
                smb2_get_uint32(iov, 2, &req->lock.lease.flags);
                memcpy(req->lock.lease.lease_key, iov->buf + 6, SMB2_LEASE_KEY_SIZE);
                smb2_get_uint32(iov, 22, &req->lock.lease.lease_state);
                smb2_get_uint64(iov, 26, &req->lock.lease.lease_duration);
        }
        else {
                return -1;
        }
        return 0;
}

