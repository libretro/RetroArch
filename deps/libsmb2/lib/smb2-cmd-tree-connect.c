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
smb2_encode_tree_connect_request(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu,
                                 struct smb2_tree_connect_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_TREE_CONNECT_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate tree connect setup "
                               "buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for tree connect request");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_TREE_CONNECT_REQUEST_SIZE);
        smb2_set_uint16(iov, 2, req->flags);
        /* path offset */
        smb2_set_uint16(iov, 4, SMB2_HEADER_SIZE + len);
        smb2_set_uint16(iov, 6, req->path_length);


        /* Path */
        buf = malloc(req->path_length);
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate tcon path");
                return -1;
        }
        memcpy(buf, req->path, req->path_length);
        iov = smb2_add_iovector(smb2, &pdu->out,
                                buf,
                                req->path_length,
                                free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for tree connect path");
                return -1;
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_tree_connect_async(struct smb2_context *smb2,
                            struct smb2_tree_connect_request *req,
                            smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_TREE_CONNECT, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_tree_connect_request(smb2, pdu, req)) {
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
smb2_encode_tree_connect_reply(struct smb2_context *smb2,
                                 struct smb2_pdu *pdu,
                                 struct smb2_tree_connect_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_TREE_CONNECT_REPLY_SIZE;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate tree connect reply "
                               "buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for tree connect reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_TREE_CONNECT_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->share_type);
        smb2_set_uint8(iov, 3, 0);
        smb2_set_uint32(iov, 4, rep->share_flags);
        smb2_set_uint32(iov, 8, rep->capabilities);
        smb2_set_uint32(iov, 12, rep->maximal_access);

        return 0;
}
struct smb2_pdu *
smb2_cmd_tree_connect_reply_async(struct smb2_context *smb2,
                            struct smb2_tree_connect_reply *rep,
                            uint32_t tree_id,
                            smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;
        static uint32_t s_tree_id = 0xfeedface;

        pdu = smb2_allocate_pdu(smb2, SMB2_TREE_CONNECT, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (!tree_id) {
                /* invent a tree-id and use it while tree connected */
                tree_id = s_tree_id++;
        }
        smb2_connect_tree_id(smb2, tree_id);
        pdu->header.sync.tree_id = smb2_tree_id(smb2);

        if (smb2_encode_tree_connect_reply(smb2, pdu, rep)) {
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
smb2_process_tree_connect_fixed(struct smb2_context *smb2,
                                struct smb2_pdu *pdu)
{
        struct smb2_tree_connect_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_TREE_CONNECT_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Tree Connect "
                               "reply. Expected %d, got %d",
                               SMB2_TREE_CONNECT_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate tcon reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_connect_tree_id(smb2, smb2->hdr.sync.tree_id);

        smb2_get_uint8(iov, 2, &rep->share_type);
        smb2_get_uint32(iov, 4, &rep->share_flags);
        smb2_get_uint32(iov, 8, &rep->capabilities);
        smb2_get_uint32(iov, 12, &rep->maximal_access);

        if (!smb2->seal)
                smb2->seal = !!(rep->share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA);

        return 0;
}

int
smb2_process_tree_connect_request_fixed(struct smb2_context *smb2,
                                struct smb2_pdu *pdu)
{
        struct smb2_tree_connect_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_TREE_CONNECT_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Tree Connect "
                               "request. Expected %d, got %d",
                               SMB2_TREE_CONNECT_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate tcon request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint16(iov, 2, &req->flags);
        smb2_get_uint16(iov, 4, &req->path_offset);
        smb2_get_uint16(iov, 6, &req->path_length);

        return req->path_length;
}

int
smb2_process_tree_connect_request_variable(struct smb2_context *smb2,
                               struct smb2_pdu *pdu)
{
        struct smb2_tree_connect_request *req = (struct smb2_tree_connect_request*)pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        req->path = (uint16_t*)(void *)iov->buf;
        return 0;
}

