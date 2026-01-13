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

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "compat.h"

#include "portable-endian.h"

#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"
#include "smb3-seal.h"
#include "smb2-signing.h"

int
smb2_pad_to_64bit(struct smb2_context *smb2, struct smb2_io_vectors *v)
{
        static uint8_t zero_bytes[7];
        int i, len = 0;

        for (i = 0; i < v->niov; i++) {
                len += (int)v->iov[i].len;
        }
        if ((len & 0x07) == 0) {
                return 0;
        }
        if (smb2_add_iovector(smb2, v, 
                        &zero_bytes[0], 
                        8 - (len & 0x07), NULL)
                        == NULL) {
                return -1;
        }

        return 0;
}

#include <stdio.h>

struct smb2_pdu *
smb2_allocate_pdu(struct smb2_context *smb2, enum smb2_command command,
                  smb2_command_cb cb, void *cb_data)
{
	struct smb2_pdu *pdu;
        struct smb2_header *hdr;
        char magic[4] = {0xFE, 'S', 'M', 'B'};

        pdu = calloc(1, sizeof(struct smb2_pdu));
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to allocate pdu");
                return NULL;
        }

        hdr = &pdu->header;

        memcpy(hdr->protocol_id, magic, 4);

        /* ZERO out the signature
         * Signature calculation happens by zeroing out
         */
        memset(hdr->signature, 0, 16);

        hdr->struct_size = SMB2_HEADER_SIZE;
        hdr->command = command;
        hdr->flags = 0;
        hdr->sync.process_id = 0xFEFF;

        if (smb2->dialect == SMB2_VERSION_0202) {
                hdr->credit_charge = 0;
        } else if (hdr->command == SMB2_NEGOTIATE) {
                /* We don't have any credits yet during negprot by
                 * looking at traces.
                 */
                hdr->credit_charge = 0;
        } else {
                /* Assume the credits for this PDU will be 1.
                 * READ/WRITE/IOCTL/QUERYDIR that consumes more than
                 * 1 credit will adjusted this after it has marshalled the
                 * fixed part of the PDU.
                 */
                hdr->credit_charge = 1;
        }
        hdr->credit_request_response = MAX_CREDITS - smb2->credits;

        switch (command) {
        case SMB2_NEGOTIATE:
        case SMB2_SESSION_SETUP:
        case SMB2_LOGOFF:
        case SMB2_ECHO:
        /* case SMB2_CANCEL: */
                hdr->sync.tree_id = 0;
                break;
        case SMB2_TREE_CONNECT:
                /* [MS-SMB2] 2.2.1.2 */
                hdr->sync.tree_id = 0;
                break;
        default:
                hdr->sync.tree_id = smb2_tree_id(smb2);
                break;
        }

        switch (command) {
        case SMB2_NEGOTIATE:
                break;
        default:
               hdr->session_id = smb2->session_id;
        }

        pdu->cb = cb;
        pdu->cb_data = cb_data;
        pdu->out.niov = 0;

        if (smb2_add_iovector(smb2, &pdu->out, pdu->hdr, SMB2_HEADER_SIZE, NULL) == NULL) {
                free(pdu);
                smb2_set_error(smb2, "Too many I/O vectors when adding SMB2 header");
                return NULL;
        }

        switch (command) {
        case SMB2_NEGOTIATE:
        case SMB2_SESSION_SETUP:
                break;
        default:
                if (smb2->seal) {
                        pdu->seal = 1;
                }
        }

        if (smb2->timeout) {
                pdu->timeout = time(NULL) + smb2->timeout;
        }

        return pdu;
}

int
smb2_select_tree_id(struct smb2_context *smb2, uint32_t tree_id)
{
        int i;

        for (
                i = 1;
                i <= smb2->tree_id_top && i <= SMB2_MAX_TREE_NESTING;
                i++
        ) {
                if (smb2->tree_id[i] == tree_id) {
                        break;
                }
        }
        if (i <= smb2->tree_id_top) {
                smb2->tree_id_cur = i;
        }
        else {
                smb2_set_error(smb2, "No connected tree-id %08X to select", tree_id);
                return -1;
        }
        return 0;
}

int
smb2_get_tree_id_for_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu, uint32_t *tree_id)
{
        if (pdu) {
                switch (pdu->header.command) {
                case SMB2_NEGOTIATE:
                case SMB2_SESSION_SETUP:
                case SMB2_LOGOFF:
                case SMB2_ECHO:
                /* case SMB2_CANCEL: */
                case SMB2_TREE_CONNECT:
                        *tree_id  = 0;
                        return 0;
                default:
                        break;
                }
        }
        if (smb2->tree_id_top > 0) {
                *tree_id = smb2->tree_id[smb2->tree_id_cur];
        }
        else {
                smb2_set_error(smb2, "No tree-id connected");
                *tree_id = 0xdeadbeef;
                return -1;
        }
        return 0;
}

int
smb2_set_tree_id_for_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu, uint32_t tree_id)
{
        if (pdu) {
                if (pdu->header.flags & SMB2_FLAGS_ASYNC_COMMAND) {
                        smb2_set_error(smb2, "no tree id for async pdu");
                        return 0;
                }
                switch (pdu->header.command) {
                case SMB2_NEGOTIATE:
                case SMB2_SESSION_SETUP:
                case SMB2_LOGOFF:
                case SMB2_ECHO:
                /* case SMB2_CANCEL: */
                        break;
                case SMB2_TREE_CONNECT:
                        break;
                default:
                        pdu->header.sync.tree_id = tree_id;
                }
                return 0;
        }
        return -1;
}

int smb2_get_session_id(struct smb2_context* smb2, uint64_t *session_id)
{
        *session_id = smb2->session_id;

        return 0;
}

int
smb2_connect_tree_id(struct smb2_context *smb2, uint32_t tree_id)
{
        if (smb2->tree_id_top < (SMB2_MAX_TREE_NESTING - 1)) {
                smb2->tree_id[++smb2->tree_id_top] = tree_id;
                smb2->tree_id_cur = smb2->tree_id_top;
        }
        else {
                smb2_set_error(smb2, "Tree nesting too deep");
                return -1;
        }
        return 0;
}

int
smb2_disconnect_tree_id(struct smb2_context *smb2, uint32_t tree_id)
{
        int i, j;

        if (smb2->tree_id_top > 0) {
                for (
                        i = 1;
                        i <= smb2->tree_id_top && i <= SMB2_MAX_TREE_NESTING;
                        i++
                ) {
                        if (smb2->tree_id[i] == tree_id) {
                                break;
                        }
                }
                if (i <= smb2->tree_id_top) {
                       for (j = i; j < smb2->tree_id_top; j++) {
                               smb2->tree_id[j] = smb2->tree_id[j + 1];
                       }
                       smb2->tree_id_top--;
                        /* not sure what tree id should be after a disconnect but
                         * this makes sure its not invalid */
                        if (smb2->tree_id_cur > smb2->tree_id_top) {
                                smb2->tree_id_cur = smb2->tree_id_top;
                        }
                        return 0;
                }
        }

        smb2_set_error(smb2, "No tree-id %08X to remove", tree_id);
        return -1;
}
int
smb2_pdu_is_compound(struct smb2_context *smb2)
{
        return (smb2) ?
                (smb2->hdr.next_command != 0) : 0;
}

void
smb2_add_compound_pdu(struct smb2_context *smb2,
                      struct smb2_pdu *pdu, struct smb2_pdu *next_pdu)
{
        int i, offset;

        /* find the last pdu in the chain */
        while (pdu->next_compound) {
                pdu = pdu->next_compound;
        }
        pdu->next_compound = next_pdu;

        /* Fixup the next offset in the header */
        for (i = 0, offset = 0; i < pdu->out.niov; i++) {
                offset += (int)pdu->out.iov[i].len;
        }

        pdu->header.next_command = offset;
        smb2_set_uint32(&pdu->out.iov[0], 20, pdu->header.next_command);

        /* Fixup flags */
        next_pdu->header.flags |= SMB2_FLAGS_RELATED_OPERATIONS;
        smb2_set_uint32(&next_pdu->out.iov[0], 16, next_pdu->header.flags);
}

void
smb2_free_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        SMB2_LIST_REMOVE(&smb2->outqueue, pdu);
        SMB2_LIST_REMOVE(&smb2->waitqueue, pdu);

        if (pdu->next_compound) {
                smb2_free_pdu(smb2, pdu->next_compound);
        }

        smb2_free_iovector(smb2, &pdu->out);
        smb2_free_iovector(smb2, &pdu->in);

        if (pdu->free_cb != NULL) {
            pdu->free_cb(pdu->cb_data);
        }
        
        if (pdu->free_payload != NULL) {
            pdu->free_payload(smb2, pdu->payload);
        }

        free(pdu->payload);
        free(pdu->crypt);
        free(pdu);
}

int
smb2_set_uint8(struct smb2_iovec *iov, int offset, uint8_t value)
{
        if (offset + sizeof(uint8_t) > iov->len) {
                return -1;
        }
        iov->buf[offset] = value;
        return 0;
}

int
smb2_set_uint16(struct smb2_iovec *iov, int offset, uint16_t value)
{
        if (offset + sizeof(uint16_t) > iov->len) {
                return -1;
        }
        *(uint16_t *)(void *)(iov->buf + offset) = htole16(value);
        return 0;
}

int
smb2_set_uint32(struct smb2_iovec *iov, int offset, uint32_t value)
{
        if (offset + sizeof(uint32_t) > iov->len) {
                return -1;
        }
        *(uint32_t *)(void *)(iov->buf + offset) = htole32(value);
        return 0;
}

int
smb2_set_uint64(struct smb2_iovec *iov, int offset, uint64_t value)
{
        if (offset + sizeof(uint64_t) > iov->len) {
                return -1;
        }
        value = htole64(value);
        memcpy(iov->buf + offset, &value, 8);
        return 0;
}

int
smb2_get_uint8(struct smb2_iovec *iov, int offset, uint8_t *value)
{
        if (offset + sizeof(uint8_t) > iov->len) {
                return -1;
        }
        *value = iov->buf[offset];
        return 0;
}

int
smb2_get_uint16(struct smb2_iovec *iov, int offset, uint16_t *value)
{
        uint16_t tmp;

        if (offset + sizeof(uint16_t) > iov->len) {
                return -1;
        }
        memcpy(&tmp, iov->buf + offset, sizeof(uint16_t));
        *value = le16toh(tmp);
        return 0;
}

int
smb2_get_uint32(struct smb2_iovec *iov, int offset, uint32_t *value)
{
        uint32_t tmp;

        if (offset + sizeof(uint32_t) > iov->len) {
                return -1;
        }
        memcpy(&tmp, iov->buf + offset, sizeof(uint32_t));
        *value = le32toh(tmp);
        return 0;
}

int
smb2_get_uint64(struct smb2_iovec *iov, int offset, uint64_t *value)
{
        uint64_t tmp;

        if (offset + sizeof(uint64_t) > iov->len) {
                return -1;
        }
        memcpy(&tmp, iov->buf + offset, sizeof(uint64_t));
        *value = le64toh(tmp);
        return 0;
}

static void
smb2_encode_header(struct smb2_context *smb2, struct smb2_iovec *iov,
                   struct smb2_header *hdr)
{
        if (!smb2_is_server(smb2)) {
                hdr->message_id = smb2->message_id++;
                if (hdr->credit_charge > 1) {
                        smb2->message_id += (hdr->credit_charge - 1);
                }
        }

        memcpy(iov->buf, hdr->protocol_id, 4);
        smb2_set_uint16(iov, 4, hdr->struct_size);
        smb2_set_uint16(iov, 6, hdr->credit_charge);
        smb2_set_uint32(iov, 8, hdr->status);
        smb2_set_uint16(iov, 12, hdr->command);
        smb2_set_uint16(iov, 14, hdr->credit_request_response);
        smb2_set_uint32(iov, 16, hdr->flags);
        smb2_set_uint32(iov, 20, hdr->next_command);
        smb2_set_uint64(iov, 24, hdr->message_id);

        if (hdr->flags & SMB2_FLAGS_ASYNC_COMMAND) {
                smb2_set_uint64(iov, 32, hdr->async.async_id);
        } else {
                /*
                printf(">>>>>>>>>> %p %s %d treeid=%08x    %08X\n", smb2,
                                (hdr->flags & SMB2_FLAGS_SERVER_TO_REDIR) ? "rep" : "cmd",
                                hdr->command, hdr->sync.tree_id, smb2_tree_id(smb2));
                */
                smb2_set_uint32(iov, 32, hdr->sync.process_id);
                smb2_set_uint32(iov, 36, hdr->sync.tree_id);
        }

        smb2_set_uint64(iov, 40, hdr->session_id);
        memcpy(iov->buf + 48, hdr->signature, 16);
}

int
smb2_decode_header(struct smb2_context *smb2, struct smb2_iovec *iov,
                   struct smb2_header *hdr)
{
        static char smb1sign[4] = {0xFF, 'S', 'M', 'B'};
        static char smb2sign[4] = {0xFE, 'S', 'M', 'B'};

        if (iov->len < SMB2_HEADER_SIZE) {
                smb2_set_error(smb2, "io vector for header is too small");
                return -1;
        }
        if (!memcmp(iov->buf, smb1sign, 4)) {
                /* an SMBv1 request. if it is a negotiate request
                 * allow it through, else ingore */
                if (iov->buf[4] == SMB1_NEGOTIATE) {
                        /*printf("Handling SMBv1 Negotiate\n");*/
                        memset(hdr, 0, sizeof *hdr);
                        hdr->command = SMB1_NEGOTIATE;
                        return 0;
                }
                smb2_set_error(smb2, "not handling SMBv1 request");
                return -1;
        }
        if (memcmp(iov->buf, smb2sign, 4)) {
                smb2_set_error(smb2, "bad SMB signature in header");
                return -1;
        }
        memcpy(&hdr->protocol_id, iov->buf, 4);
        smb2_get_uint16(iov, 4, &hdr->struct_size);
        smb2_get_uint16(iov, 6, &hdr->credit_charge);
        smb2_get_uint32(iov, 8, &hdr->status);
        smb2_get_uint16(iov, 12, &hdr->command);
        smb2_get_uint16(iov, 14, &hdr->credit_request_response);
        smb2_get_uint32(iov, 16, &hdr->flags);
        smb2_get_uint32(iov, 20, &hdr->next_command);
        smb2_get_uint64(iov, 24, &hdr->message_id);

        if (hdr->flags & SMB2_FLAGS_ASYNC_COMMAND) {
                smb2_get_uint64(iov, 32, &hdr->async.async_id);
        } else {
                smb2_get_uint32(iov, 32, &hdr->sync.process_id);
                smb2_get_uint32(iov, 36, &hdr->sync.tree_id);
                /*
                printf("<<<<<<<<<<< %p %s %d treeid=%08x  %08X\n", smb2,
                                (hdr->flags & SMB2_FLAGS_SERVER_TO_REDIR) ? "rep" : "cmd",
                                hdr->command, hdr->sync.tree_id, smb2_tree_id(smb2));
                */
                /* for requests, set the context tree id to the header value
                */
                if (!(hdr->flags & SMB2_FLAGS_SERVER_TO_REDIR)) {
                        switch (hdr->command) {
                        case SMB2_NEGOTIATE:
                        case SMB2_SESSION_SETUP:
                        case SMB2_LOGOFF:
                        case SMB2_ECHO:
                        /* case SMB2_CANCEL: */
                                break;
                        case SMB2_TREE_CONNECT:
                                break;
                        default:
                                /* TODO - care about not having this already connected */
                                smb2_select_tree_id(smb2, hdr->sync.tree_id);
                                break;
                        }
                }
        }
        if (smb2_is_server(smb2)) {
                /* remember message id to format a reply for this
                 * request (or ack this notification) */
                smb2->message_id = hdr->message_id;
        }
        smb2_get_uint64(iov, 40, &hdr->session_id);
        memcpy(&hdr->signature, iov->buf + 48, 16);

        return 0;
}

static void
smb2_add_to_outqueue(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        SMB2_LIST_ADD_END(&smb2->outqueue, pdu);

        /* opportunistically try to write it to the socket right away */
        if (smb2->outqueue == pdu) {
                smb2_write_to_socket(smb2);
        }

        smb2_change_events(smb2, smb2->fd, smb2_which_events(smb2));
}

static int
smb2_correlate_reply(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        struct smb2_pdu *req_pdu;
        int ret = 0;

        req_pdu = smb2_find_pdu(smb2, pdu->header.message_id);
        if (req_pdu == NULL) {
                if (pdu->header.command != SMB2_OPLOCK_BREAK) {
                        smb2_set_error(smb2, "no matching request PDU "
                                       "found for reply to cmd %d id %llu",
                                        pdu->header.command, pdu->header.message_id);
                        return -1;
                } else {
                        /* sending an unsolicited break */
                        pdu->header.message_id = 0xffffffffffffffffULL;
                        pdu->header.sync.tree_id = 0;
                        pdu->header.session_id = 0;
                }
        }  else {
                uint16_t credit_grant = req_pdu->header.credit_request_response;

                /* some clients (lookin at you nautilus) break if their credits
                 * increment too far (and probably wrap to 0) so limit credits
                 * to what the client requests only and if the clients charge
                 * is greater than ours, charge the larger amount so they cant
                 * accumulate on the client
                 */
                if (credit_grant > 0xf000) {
                        pdu->header.credit_request_response = 0xffff;
                } else {
                        pdu->header.credit_request_response = credit_grant;
                }

                if (req_pdu->header.credit_charge > pdu->header.credit_charge) {
                        pdu->header.credit_charge = req_pdu->header.credit_charge;
                }

                /* replies always have to have the same message-id and tree-id as
                 * the request we sent, so use the request from the wait queue
                 * to make sure that is the case. (exception is tree-connect where
                 * the reply has the new tree-id and request was 0)
                 */
                pdu->header.message_id = req_pdu->header.message_id;
                if (pdu->header.command != SMB2_TREE_CONNECT) {
                        pdu->header.sync.tree_id = req_pdu->header.sync.tree_id;
                }

                /* remove the request from the waitqueue when we get its reply
                 * unless this is an async status-pending reply
                */
                if ((pdu->header.flags & SMB2_FLAGS_ASYNC_COMMAND) &&
                               (pdu->header.status == SMB2_STATUS_PENDING)) {
                        /* the real reply will be queued later so we leave the request
                         * in the list to be correlated with that. we will also mark the
                         * request as async as well so when the real reply gets here
                         * we will know to set the async-id and flag
                         */
                        pdu->header.async.async_id = ++smb2->async_id;
                        req_pdu->header.async.async_id = pdu->header.async.async_id;
                        req_pdu->header.flags |= SMB2_FLAGS_ASYNC_COMMAND;
               } else {
                       /* if this is the real-reply to an async operation, also
                        * set the async flag and id (MS-SMB2 3.2.5.1.5)
                        */
                       if (req_pdu->header.flags & SMB2_FLAGS_ASYNC_COMMAND) {
                               pdu->header.flags |= SMB2_FLAGS_ASYNC_COMMAND;
                               pdu->header.async.async_id = req_pdu->header.async.async_id;
                       }
                       SMB2_LIST_REMOVE(&smb2->waitqueue, req_pdu);
                       smb2_free_pdu(smb2, req_pdu);
                }
        }
        return ret;
}

void
smb2_queue_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        struct smb2_pdu *p;
        uint64_t prev_compound_mid = 0;

        /* Update all the PDU headers in this chain */
        for (p = pdu; p; p = p->next_compound) {
                if (smb2_is_server(smb2)) {
                        /* set reply flag, servers will only reply */
                        pdu->header.flags |= SMB2_FLAGS_SERVER_TO_REDIR;

                        /* set async flag for status==pending */
                        if (pdu->header.status == SMB2_STATUS_PENDING) {
                                pdu->header.flags |= SMB2_FLAGS_ASYNC_COMMAND;
                        }

                        /* the server handler functions must set message id unless this
                         * is a negotiate request, in which case it should be 0
                         */
                        if (!pdu->header.message_id && pdu->header.command != SMB2_NEGOTIATE) {
                                smb2_set_error(smb2, "Queued pdu has no message id");
                                smb2_free_pdu(smb2, pdu);
                                return;
                        }

                        smb2_correlate_reply(smb2, p);
                        /* TODO - care about check reply failures? */
                }
                smb2_encode_header(smb2, &p->out.iov[0], &p->header);
                if (!smb2_is_server(smb2)) {
                        /*
                         * Track the mid of the previous command in the chain
                         * so we can enforce ordering on receive.
                         */
                        p->prev_compound_mid = prev_compound_mid;
                        prev_compound_mid = p->header.message_id;
                }

                if (smb2->sign ||
                    (p->header.command == SMB2_TREE_CONNECT && smb2->dialect == SMB2_VERSION_0311 && !smb2->seal)) {
                        if (smb2_pdu_add_signature(smb2, p) < 0) {
                                smb2_set_error(smb2, "Failure to add "
                                               "signature. %s",
                                               smb2_get_error(smb2));
                        }
                }
        }

        smb3_encrypt_pdu(smb2, pdu);

        smb2_add_to_outqueue(smb2, pdu);
}

struct smb2_pdu *
smb2_get_compound_pdu(struct smb2_context *smb2,
                      struct smb2_pdu *pdu)
{
        if (pdu && pdu->next_compound) {
                return pdu->next_compound;
        }

        return NULL;
}

void
smb2_set_pdu_status(struct smb2_context *smb2, struct smb2_pdu *pdu, int status)
{
        pdu->header.status = status;
}

void
smb2_set_pdu_message_id(struct smb2_context *smb2, struct smb2_pdu *pdu, uint64_t message_id)
{
        pdu->header.message_id = message_id;
}

uint64_t
smb2_get_pdu_message_id(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (pdu) {
                return pdu->header.message_id;
        }
        return 0;
}

uint64_t
smb2_get_last_request_message_id(struct smb2_context *smb2)
{
        if (smb2) {
                return smb2->message_id;
        }
        return 0;
}

uint64_t
smb2_get_last_reply_message_id(struct smb2_context *smb2)
{
        if (smb2) {
                return smb2->hdr.message_id;
        }
        return 0;
}

struct smb2_pdu *
smb2_find_pdu(struct smb2_context *smb2,
              uint64_t message_id) {
        struct smb2_pdu *pdu;

        for (pdu = smb2->waitqueue; pdu; pdu = pdu->next) {
                if (pdu->header.message_id == message_id) {
                        break;
                }
        }
        return pdu;
}

static int
smb2_is_error_response(struct smb2_context *smb2,
                       struct smb2_pdu *pdu) {
        if ((smb2->hdr.status & SMB2_STATUS_SEVERITY_MASK) ==
            SMB2_STATUS_SEVERITY_ERROR) {
                switch (smb2->hdr.status) {
                case SMB2_STATUS_MORE_PROCESSING_REQUIRED:
                        return 0;
                default:
                        return 1;
                }
        } else if ((smb2->hdr.status & SMB2_STATUS_SEVERITY_MASK) ==
                 SMB2_STATUS_SEVERITY_WARNING) {
                switch(smb2->hdr.status) {
                case SMB2_STATUS_STOPPED_ON_SYMLINK:
                        return 1;
                default:
                        return 0;
                }
        }
        return 0;
}

int
smb2_get_fixed_reply_size(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_error_response(smb2, pdu)) {
                return SMB2_ERROR_REPLY_SIZE & 0xfffe;
        }

        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return SMB2_NEGOTIATE_REPLY_SIZE;
        case SMB2_SESSION_SETUP:
                return SMB2_SESSION_SETUP_REPLY_SIZE;
        case SMB2_LOGOFF:
                return SMB2_LOGOFF_REPLY_SIZE;
        case SMB2_TREE_CONNECT:
                return SMB2_TREE_CONNECT_REPLY_SIZE;
        case SMB2_TREE_DISCONNECT:
                return SMB2_TREE_DISCONNECT_REPLY_SIZE;
        case SMB2_CREATE:
                return SMB2_CREATE_REPLY_SIZE;
        case SMB2_CLOSE:
                return SMB2_CLOSE_REPLY_SIZE;
        case SMB2_FLUSH:
                return SMB2_FLUSH_REPLY_SIZE;
        case SMB2_READ:
                return SMB2_READ_REPLY_SIZE;
        case SMB2_WRITE:
                return SMB2_WRITE_REPLY_SIZE;
        case SMB2_LOCK:
                return SMB2_LOCK_REPLY_SIZE;
        case SMB2_ECHO:
                return SMB2_ECHO_REPLY_SIZE;
        case SMB2_QUERY_DIRECTORY:
                return SMB2_QUERY_DIRECTORY_REPLY_SIZE;
        case SMB2_CHANGE_NOTIFY:
                return SMB2_CHANGE_NOTIFY_REPLY_SIZE;
        case SMB2_QUERY_INFO:
                return SMB2_QUERY_INFO_REPLY_SIZE;
        case SMB2_SET_INFO:
                return SMB2_SET_INFO_REPLY_SIZE;
        case SMB2_IOCTL:
                return SMB2_IOCTL_REPLY_SIZE;
        case SMB2_OPLOCK_BREAK:
                /* need to read the struct size to see what
                 * type (oplock or lease) the pdu is */
                return sizeof(uint16_t);
        }
        return -1;
}

int
smb2_get_fixed_request_size(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return SMB2_NEGOTIATE_REQUEST_SIZE;
        case SMB2_SESSION_SETUP:
                return SMB2_SESSION_SETUP_REQUEST_SIZE;
        case SMB2_LOGOFF:
                return SMB2_LOGOFF_REQUEST_SIZE;
        case SMB2_TREE_CONNECT:
                return SMB2_TREE_CONNECT_REQUEST_SIZE;
        case SMB2_TREE_DISCONNECT:
                return SMB2_TREE_DISCONNECT_REQUEST_SIZE;
        case SMB2_CREATE:
                return SMB2_CREATE_REQUEST_SIZE;
        case SMB2_CLOSE:
                return SMB2_CLOSE_REQUEST_SIZE;
        case SMB2_FLUSH:
                return SMB2_FLUSH_REQUEST_SIZE;
        case SMB2_READ:
                return SMB2_READ_REQUEST_SIZE;
        case SMB2_WRITE:
                return SMB2_WRITE_REQUEST_SIZE;
        case SMB2_LOCK:
                return SMB2_LOCK_REQUEST_SIZE;
        case SMB2_CANCEL:
                return SMB2_CANCEL_REQUEST_SIZE;
        case SMB2_ECHO:
                return SMB2_ECHO_REQUEST_SIZE;
        case SMB2_QUERY_DIRECTORY:
                return SMB2_QUERY_DIRECTORY_REQUEST_SIZE;
        case SMB2_CHANGE_NOTIFY:
                return SMB2_CHANGE_NOTIFY_REQUEST_SIZE;
        case SMB2_QUERY_INFO:
                return SMB2_QUERY_INFO_REQUEST_SIZE;
        case SMB2_SET_INFO:
                return SMB2_SET_INFO_REQUEST_SIZE;
        case SMB2_IOCTL:
                return SMB2_IOCTL_REQUEST_SIZE;
        case SMB2_OPLOCK_BREAK:
                /* need to read the struct size to see what
                 * type (oplock or lease) the pdu is */
                return sizeof(uint16_t);
        }
        return -1;
}

int
smb2_get_fixed_size(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_server(smb2)) {
                return smb2_get_fixed_request_size(smb2, pdu);
        }
        else {
                return smb2_get_fixed_reply_size(smb2, pdu);
        }
}

int
smb2_process_reply_payload_fixed(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_error_response(smb2, pdu)) {
                return smb2_process_error_fixed(smb2, pdu);
        }

        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return smb2_process_negotiate_fixed(smb2, pdu);
        case SMB2_SESSION_SETUP:
                return smb2_process_session_setup_fixed(smb2, pdu);
        case SMB2_LOGOFF:
                return smb2_process_logoff_fixed(smb2, pdu);
        case SMB2_TREE_CONNECT:
                return smb2_process_tree_connect_fixed(smb2, pdu);
        case SMB2_TREE_DISCONNECT:
                return smb2_process_tree_disconnect_fixed(smb2, pdu);
        case SMB2_CREATE:
                return smb2_process_create_fixed(smb2, pdu);
        case SMB2_CLOSE:
                return smb2_process_close_fixed(smb2, pdu);
        case SMB2_FLUSH:
                return smb2_process_flush_fixed(smb2, pdu);
        case SMB2_READ:
                return smb2_process_read_fixed(smb2, pdu);
        case SMB2_WRITE:
                return smb2_process_write_fixed(smb2, pdu);
        case SMB2_ECHO:
                return smb2_process_echo_fixed(smb2, pdu);
        case SMB2_LOCK:
                return smb2_process_lock_fixed(smb2, pdu);
        case SMB2_QUERY_DIRECTORY:
                return smb2_process_query_directory_fixed(smb2, pdu);
        case SMB2_CHANGE_NOTIFY:
                return smb2_process_change_notify_fixed(smb2, pdu);
        case SMB2_QUERY_INFO:
                return smb2_process_query_info_fixed(smb2, pdu);
        case SMB2_SET_INFO:
                return smb2_process_set_info_fixed(smb2, pdu);
        case SMB2_IOCTL:
                return smb2_process_ioctl_fixed(smb2, pdu);
        case SMB2_OPLOCK_BREAK:
                /* notice that op/lease lock breaks can be notification or response here */
                return smb2_process_oplock_break_fixed(smb2, pdu);
        }
        return 0;
}

int
smb2_process_reply_payload_variable(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_error_response(smb2, pdu)) {
                return smb2_process_error_variable(smb2, pdu);
        }

        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return smb2_process_negotiate_variable(smb2, pdu);
        case SMB2_SESSION_SETUP:
                return smb2_process_session_setup_variable(smb2, pdu);
        case SMB2_LOGOFF:
                return 0;
        case SMB2_TREE_CONNECT:
                return 0;
        case SMB2_TREE_DISCONNECT:
                return 0;
        case SMB2_CREATE:
                return smb2_process_create_variable(smb2, pdu);
        case SMB2_CLOSE:
                return 0;
        case SMB2_FLUSH:
                return 0;
        case SMB2_READ:
                return smb2_process_read_variable(smb2, pdu);
        case SMB2_WRITE:
                return 0;
        case SMB2_ECHO:
                return 0;
        case SMB2_LOCK:
                return 0;
        case SMB2_CANCEL:
                return 0;
        case SMB2_QUERY_DIRECTORY:
                return smb2_process_query_directory_variable(smb2, pdu);
        case SMB2_CHANGE_NOTIFY:
                return smb2_process_change_notify_variable(smb2, pdu);
        case SMB2_QUERY_INFO:
                return smb2_process_query_info_variable(smb2, pdu);
        case SMB2_SET_INFO:
                return 0;
        case SMB2_IOCTL:
                return smb2_process_ioctl_variable(smb2, pdu);
        case SMB2_OPLOCK_BREAK:
                return smb2_process_oplock_break_variable(smb2, pdu);
        }
        return 0;
}

int
smb2_process_request_payload_fixed(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return smb2_process_negotiate_request_fixed(smb2, pdu);
        case SMB2_SESSION_SETUP:
                return smb2_process_session_setup_request_fixed(smb2, pdu);
        case SMB2_LOGOFF:
                return smb2_process_logoff_request_fixed(smb2, pdu);
        case SMB2_TREE_CONNECT:
                return smb2_process_tree_connect_request_fixed(smb2, pdu);
        case SMB2_TREE_DISCONNECT:
                return 0;
        case SMB2_CREATE:
                return smb2_process_create_request_fixed(smb2, pdu);
        case SMB2_CLOSE:
                return smb2_process_close_request_fixed(smb2, pdu);
        case SMB2_FLUSH:
                return smb2_process_flush_request_fixed(smb2, pdu);
        case SMB2_READ:
                return smb2_process_read_request_fixed(smb2, pdu);
        case SMB2_WRITE:
                return smb2_process_write_request_fixed(smb2, pdu);
        case SMB2_ECHO:
                return smb2_process_echo_request_fixed(smb2, pdu);
        case SMB2_LOCK:
                return smb2_process_lock_request_fixed(smb2, pdu);
        case SMB2_CANCEL:
                return 0;
        case SMB2_QUERY_DIRECTORY:
                return smb2_process_query_directory_request_fixed(smb2, pdu);
        case SMB2_CHANGE_NOTIFY:
                return smb2_process_change_notify_request_fixed(smb2, pdu);
        case SMB2_QUERY_INFO:
                return smb2_process_query_info_request_fixed(smb2, pdu);
        case SMB2_SET_INFO:
                return smb2_process_set_info_request_fixed(smb2, pdu);
        case SMB2_IOCTL:
                return smb2_process_ioctl_request_fixed(smb2, pdu);
        case SMB2_OPLOCK_BREAK:
                /* note oplock/lease break from a client is an acknowlegement here */
                return smb2_process_oplock_break_request_fixed(smb2, pdu);
        default:
                smb2_set_error(smb2, "No handler for fixed request");
                return -1;
        }
        return 0;
}

int
smb2_process_request_payload_variable(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        switch (pdu->header.command) {
        case SMB2_NEGOTIATE:
                return smb2_process_negotiate_request_variable(smb2, pdu);
        case SMB2_SESSION_SETUP:
                return smb2_process_session_setup_request_variable(smb2, pdu);
        case SMB2_LOGOFF:
                return 0;
        case SMB2_TREE_CONNECT:
                return smb2_process_tree_connect_request_variable(smb2, pdu);
        case SMB2_TREE_DISCONNECT:
                return 0;
        case SMB2_CREATE:
                return smb2_process_create_request_variable(smb2, pdu);
        case SMB2_CLOSE:
                return 0;
        case SMB2_FLUSH:
                return 0;
        case SMB2_READ:
                return smb2_process_read_request_variable(smb2, pdu);
        case SMB2_WRITE:
                return smb2_process_write_request_variable(smb2, pdu);
        case SMB2_LOCK:
                return smb2_process_lock_request_variable(smb2, pdu);
        case SMB2_CANCEL:
                return 0;
        case SMB2_ECHO:
                return 0;
        case SMB2_QUERY_DIRECTORY:
                return smb2_process_query_directory_request_variable(smb2, pdu);
        case SMB2_CHANGE_NOTIFY:
                return 0;
        case SMB2_QUERY_INFO:
                return smb2_process_query_info_request_variable(smb2, pdu);
        case SMB2_SET_INFO:
                return smb2_process_set_info_request_variable(smb2, pdu);
        case SMB2_IOCTL:
                return smb2_process_ioctl_request_variable(smb2, pdu);
        case SMB2_OPLOCK_BREAK:
                return smb2_process_oplock_break_request_variable(smb2, pdu);
        default:
                smb2_set_error(smb2, "No handler for var request");
        }
        return -1;
}

int
smb2_process_payload_fixed(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_server(smb2)) {
                return smb2_process_request_payload_fixed(smb2, pdu);
        }
        else {
                return smb2_process_reply_payload_fixed(smb2, pdu);
        }
}

int
smb2_process_payload_variable(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        if (smb2_is_server(smb2)) {
                return smb2_process_request_payload_variable(smb2, pdu);
        }
        else {
                return smb2_process_reply_payload_variable(smb2, pdu);
        }
}

void smb2_timeout_pdus(struct smb2_context *smb2)
{
        struct smb2_pdu *pdu, *next;
        time_t t = time(NULL);

        pdu = smb2->outqueue;
        while (pdu) {
                next = pdu->next;
                if (pdu->timeout && pdu->timeout < t) {
                        SMB2_LIST_REMOVE(&smb2->outqueue, pdu);
                        pdu->cb(smb2, SMB2_STATUS_IO_TIMEOUT, NULL,
                                pdu->cb_data);
                        smb2_free_pdu(smb2, pdu);
                }
                pdu = next;
        }

        pdu = smb2->waitqueue;
        while (pdu) {
                next = pdu->next;
                if (pdu->timeout && pdu->timeout < t) {
                        SMB2_LIST_REMOVE(&smb2->waitqueue, pdu);
                        pdu->cb(smb2, SMB2_STATUS_IO_TIMEOUT, NULL,
                                pdu->cb_data);
                        smb2_free_pdu(smb2, pdu);
                }
                pdu = next;
        }
}

