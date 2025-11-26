/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2019 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_SYS__IOVEC_H
#include <sys/_iovec.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <stdio.h>

#include "compat.h"

#include "portable-endian.h"

#include "aes128ccm.h"
#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"
#include "smb3-seal.h"

static const char xfer[4] = {0xFD, 'S', 'M', 'B'};

int
smb3_encrypt_pdu(struct smb2_context *smb2,
                 struct smb2_pdu *pdu)
{
        struct smb2_pdu *tmp_pdu;
        uint32_t spl, u32;
        int i;
        uint16_t u16;

        if (!smb2->seal) {
                return 0;
        }
        if (!pdu->seal) {
                return 0;
        }

        spl = 52;  /* transform header */
        for (tmp_pdu = pdu; tmp_pdu; tmp_pdu = tmp_pdu->next_compound) {
                for (i = 0; i < tmp_pdu->out.niov; i++) {
                        spl += (uint32_t)tmp_pdu->out.iov[i].len;
                }
        }
        pdu->crypt = calloc(spl, sizeof(uint8_t));
        if (pdu->crypt == NULL) {
                pdu->seal = 0;
                return -1;
        }

        memcpy(&pdu->crypt[0], xfer, 4);
        for (i = 20; i < 31; i++) {
                pdu->crypt[i] = random()&0xff;
        }
        u32 = htole32(spl - 52);
        memcpy(&pdu->crypt[36], &u32, 4);
        u16 = htole16(SMB_ENCRYPTION_AES128_CCM);
        memcpy(&pdu->crypt[42], &u16, 2);
        memcpy(&pdu->crypt[44], &smb2->session_id, 8);

        spl = 52;  /* transform header */
        for (tmp_pdu = pdu; tmp_pdu; tmp_pdu = tmp_pdu->next_compound) {
                for (i = 0; i < tmp_pdu->out.niov; i++) {
                        memcpy(&pdu->crypt[spl], tmp_pdu->out.iov[i].buf,
                               tmp_pdu->out.iov[i].len);
                        spl += (uint32_t)tmp_pdu->out.iov[i].len;
                }
        }

        aes128ccm_encrypt(smb2->serverin_key,
                          &pdu->crypt[20], 11,
                          &pdu->crypt[20], 32,
                          &pdu->crypt[52], spl - 52,
                          &pdu->crypt[4], 16);
        pdu->crypt_len = spl;

        return 0;
}

int
smb3_decrypt_pdu(struct smb2_context *smb2)
{
        int rc;

        if (aes128ccm_decrypt(smb2->serverout_key,
                              &smb2->in.iov[smb2->in.niov - 2].buf[20], 11,
                              &smb2->in.iov[smb2->in.niov - 2].buf[20], 32,
                              &smb2->in.iov[smb2->in.niov - 1].buf[0],
                              smb2->in.iov[smb2->in.niov - 1].len,
                              &smb2->in.iov[smb2->in.niov - 2].buf[4], 16)) {
                smb2_set_error(smb2, "Failed to decrypt PDU");
                return -1;
        }

        if (smb2->in.num_done == 0) {
                smb2->enc = smb2->in.iov[smb2->in.niov - 1].buf;
                smb2->enc_len = smb2->in.iov[smb2->in.niov - 1].len;
                smb2->enc_pos = 0;
                smb2->in.iov[smb2->in.niov - 1].free = NULL;
                smb2_free_iovector(smb2, &smb2->in);

                smb2->spl = (uint32_t)smb2->enc_len;
                smb2->recv_state = SMB2_RECV_HEADER;
                if (smb2_add_iovector(smb2, &smb2->in, &smb2->header[0],
                                       SMB2_HEADER_SIZE, NULL) == NULL) {
                        smb2_set_error(smb2, "Failed to add iovector for decrypted header");
                        free(smb2->enc);
                        smb2->enc = NULL;
                        return -1;
                }
        }

        rc = smb2_read_from_buf(smb2);
        free(smb2->enc);
        smb2->enc = NULL;

        return rc;
}
