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
smb2_encode_error_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_error_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_ERROR_REPLY_SIZE;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate error buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        if (iov == NULL) {
                smb2_set_error(smb2, "Failed to add iovector for error reply");
                return -1;
        }

        smb2_set_uint16(iov, 0, SMB2_ERROR_REPLY_SIZE);
        smb2_set_uint8(iov, 2, rep->error_context_count);
        smb2_set_uint32(iov, 4, rep->byte_count);

        /* TODO - handle error data? */
        return 0;
}

struct smb2_pdu *
smb2_cmd_error_reply_async(struct smb2_context *smb2,
                     struct smb2_error_reply *rep,
                     uint8_t causing_command,
                     int status,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, causing_command, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        pdu->header.status = status;

        if (smb2_encode_error_reply(smb2, pdu, rep)) {
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
smb2_process_error_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_error_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_ERROR_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Error "
                               "reply. Expected %d, got %d",
                               SMB2_ERROR_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate error reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint8(iov, 2, &rep->error_context_count);
        smb2_get_uint32(iov, 4, &rep->byte_count);

        return rep->byte_count;
}

int
smb2_process_error_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_error_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        rep->error_data = &iov->buf[0];

        return 0;
}

