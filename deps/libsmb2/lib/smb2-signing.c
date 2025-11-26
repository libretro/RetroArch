/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#include "compat.h"

#include "smb2-signing.h"

#define EBC 1
#define CBC 1

#include "aes.h"
#include "sha.h"
#include "sha-private.h"

#define AES128_KEY_LEN     16
#define AES_BLOCK_SIZE     16

static
int aes_cmac_shift_left(uint8_t data[AES128_KEY_LEN])
{
        int i = 0;
        int cin = 0;
        int cout = 0;

        for (i = AES128_KEY_LEN - 1; i >= 0; i--) {
            cout = ((int) data[i] & 0x80) >> 7;
            data[i] = (data[i] << 1) | cin;
            cin = cout;
        }

        return cout;
}

static
void aes_cmac_xor(
    uint8_t data[AES128_KEY_LEN],
    const uint8_t value[AES128_KEY_LEN]
    )
{
        int i = 0;

        for (i = 0; i < AES128_KEY_LEN; i++) {
            data[i] ^= value[i];
        }
}

static
void aes_cmac_sub_keys(
    uint8_t key[AES128_KEY_LEN],
    uint8_t sub_key1[AES128_KEY_LEN],
    uint8_t sub_key2[AES128_KEY_LEN]
    )
{
        uint8_t zero[AES128_KEY_LEN] = {0};
        static const uint8_t rb[AES128_KEY_LEN] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x87};

        AES128_ECB_encrypt(zero, key, sub_key1);
        if (aes_cmac_shift_left(sub_key1)) {
                aes_cmac_xor(sub_key1, rb);
        }

        memcpy(sub_key2, sub_key1, AES128_KEY_LEN);

        if (aes_cmac_shift_left(sub_key2)) {
                aes_cmac_xor(sub_key2, rb);
        }
}

void smb3_aes_cmac_128(uint8_t key[AES128_KEY_LEN],
                   uint8_t * msg,
                   uint64_t msg_len,
                   uint8_t mac[AES128_KEY_LEN]
                  )
{
        uint8_t sub_key1[AES128_KEY_LEN] = {0};
        uint8_t sub_key2[AES128_KEY_LEN] = {0};
        uint8_t scratch[AES128_KEY_LEN] = {0};
        uint64_t n = (msg_len + AES128_KEY_LEN - 1) / AES128_KEY_LEN;
        uint64_t rem = msg_len % AES128_KEY_LEN;
        uint64_t i = 0;
        int is_last_block_complete = n != 0 && rem == 0;

        if (n == 0) {
                n = 1;
        }

        aes_cmac_sub_keys(key, sub_key1, sub_key2);

        memset(mac, 0, AES128_KEY_LEN);

        for (i = 0; i < n - 1; i++) {
                aes_cmac_xor(mac, &msg[i*AES128_KEY_LEN]);
                AES128_ECB_encrypt(mac, key, scratch);
                memcpy(mac, scratch, AES128_KEY_LEN);
        }

        if (is_last_block_complete) {
                memcpy(scratch, &msg[i*AES128_KEY_LEN], AES128_KEY_LEN);
                aes_cmac_xor(scratch, sub_key1);
        } else {
                memcpy(scratch, &msg[i*AES128_KEY_LEN], (size_t)rem);
                scratch[rem] = 0x80;
                memset(&scratch[rem + 1], 0, AES128_KEY_LEN - ((size_t)rem + 1));
                aes_cmac_xor(scratch, sub_key2);
        }

        aes_cmac_xor(mac, scratch);
        AES128_ECB_encrypt(mac, key, scratch);
        memcpy(mac, scratch, AES128_KEY_LEN);
}

int
smb2_calc_signature(struct smb2_context *smb2, uint8_t *signature,
                    struct smb2_iovec *iov, size_t niov)

{
        /* Clear the smb2 header signature field field */
        memset(iov[0].buf + 48, 0, 16);

        if (smb2->dialect > SMB2_VERSION_0210) {
                size_t i = 0;
                size_t len = 0, offset = 0;
                uint8_t aes_mac[AES_BLOCK_SIZE];
                /* combine the buffers into one */
                uint8_t *msg = NULL;

                for (i=0; i < niov; i++) {
                        len += iov[i].len;
                }
                msg = (uint8_t *) malloc(len);
                if (msg == NULL) {
                        smb2_set_error(smb2, "Failed to allocate buffer for "
                                       "signature calculation");
                        return -1;
                }

                for (i=0; i < niov; i++) {
                        memcpy(msg + offset, iov[i].buf, iov[i].len);
                        offset += iov[i].len;
                }
                smb3_aes_cmac_128(smb2->signing_key, msg, offset, aes_mac);
                free(msg);
                memcpy(&signature[0], aes_mac, SMB2_SIGNATURE_SIZE);
        } else {
                HMACContext ctx;
                uint8_t digest[USHAMaxHashSize];
                size_t i;

                hmacReset(&ctx, SHA256, &smb2->signing_key[0], SMB2_KEY_SIZE);
                for (i=0; i < niov; i++) {
                        hmacInput(&ctx, iov[i].buf, iov[i].len);
                }
                hmacResult(&ctx, digest);
                memcpy(&signature[0], digest, SMB2_SIGNATURE_SIZE);
        }

        return 0;
}

int
smb2_pdu_add_signature(struct smb2_context *smb2,
                       struct smb2_pdu *pdu
                       )
{
        struct smb2_header *hdr = NULL;
        uint8_t signature[16] = {0};
        struct smb2_iovec *iov = NULL;
        int niov;

        if (pdu->header.command == SMB2_SESSION_SETUP) {
                /* the first session setup response with ok status
                 * is the first signed message
                 */
                if (pdu->header.status != 0 ||
                                !(pdu->header.flags & SMB2_FLAGS_SERVER_TO_REDIR)) {
                        return 0;
                }
        }
        if (pdu->out.niov < 2) {
                smb2_set_error(smb2, "Too few vectors to sign");
                return -1;
        }
        if (pdu->out.iov[0].len != SMB2_HEADER_SIZE) {
                smb2_set_error(smb2, "First vector is not same size as smb2 "
                               "header");
                return -1;
        }
        if (smb2->session_id == 0) {
                return 0; /* DO NOT sign the PDU if session id is 0 */
        }
        if (smb2->session_key_size == 0) {
                return -1;
        }

        hdr = &pdu->header;

        /* Set the flag before calculating signature */
        iov = &pdu->out.iov[0];
        niov = pdu->out.niov;
        hdr->flags |= SMB2_FLAGS_SIGNED;
        smb2_set_uint32(iov, 16, hdr->flags);

        /* sign the pdu and store the signature in pdu->header.signature
         * if pdu is signed then add SMB2_FLAGS_SIGNED to pdu->header.flags
         */
        if (smb2_calc_signature(smb2, signature, iov, niov) < 0) {
                return -1;
        }

        memcpy(&(hdr->signature[0]), signature, SMB2_SIGNATURE_SIZE);
        memcpy(iov->buf + 48, hdr->signature, 16);

        return 0;
}

int
smb2_pdu_check_signature(struct smb2_context *smb2,
                         struct smb2_pdu *pdu
                         )
{
        return 0;
}
