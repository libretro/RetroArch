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

#include <ctype.h>
#include "portable-endian.h"
#include <stdio.h>

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "compat.h"

#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"
#include "spnego-wrapper.h"

#include "md4.h"
#include "md5.h"
#include "hmac-md5.h"
#include "ntlmssp.h"

struct auth_data {
        unsigned char *buf;
        size_t len;
        size_t allocated;

        int neg_result;
        unsigned char *ntlm_buf;
        size_t ntlm_len;

        char *user;
        char *domain;
        char *password;
        char *workstation;
        char *target_name;
        uint8_t *client_challenge;
        uint8_t server_challenge[8];
        uint8_t *target_info;
        int target_info_len;

        int spnego_wrap;
        int is_authenticated;
        uint64_t wintime;
        uint8_t exported_session_key[SMB2_KEY_SIZE];
};

#define NTLMSSP_NEGOTIATE_56                               0x80000000
#define NTLMSSP_NEGOTIATE_KEY_EXCH                         0x40000000
#define NTLMSSP_NEGOTIATE_128                              0x20000000
#define NTLMSSP_NEGOTIATE_VERSION                          0x02000000
#define NTLMSSP_NEGOTIATE_TARGET_INFO                      0x00800000
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY         0x00080000
#define NTLMSSP_TARGET_TYPE_SERVER                         0x00020000
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN                      0x00008000
#define NTLMSSP_NEGOTIATE_ANONYMOUS                        0x00000800
#define NTLMSSP_NEGOTIATE_NTLM                             0x00000200
#define NTLMSSP_NEGOTIATE_SEAL                             0x00000020
#define NTLMSSP_NEGOTIATE_SIGN                             0x00000010
#define NTLMSSP_REQUEST_TARGET                             0x00000004
#define NTLMSSP_NEGOTIATE_OEM                              0x00000002
#define NTLMSSP_NEGOTIATE_UNICODE                          0x00000001

void
ntlmssp_destroy_context(struct auth_data *auth)
{
        free(auth->ntlm_buf);
        free(auth->buf);
        free(auth->user);
        free(auth->password);
        free(auth->domain);
        free(auth->workstation);
        free(auth->target_name);
        free(auth->client_challenge);
        free(auth->target_info);
        free(auth);
}

static int
auth_data_set_password(struct auth_data *auth_data, const char *password)
{
        free(auth_data->password);
        auth_data->password = NULL;

        if (password == NULL) {
                return 0;
        }

        auth_data->password = strdup(password);
        if (auth_data->password == NULL) {
                return -ENOMEM;
        }
        return 0;
}

static int
auth_data_set_domain(struct auth_data *auth_data, const char *domain)
{
        free(auth_data->domain);
        auth_data->domain = NULL;

        if (domain == NULL) {
                return 0;
        }

        auth_data->domain = strdup(domain);
        if (auth_data->domain == NULL) {
                return -ENOMEM;
        }
        return 0;
}

struct auth_data *
ntlmssp_init_context(const char *user,
                     const char *password,
                     const char *domain,
                     const char *workstation,
                     const char *client_challenge)
{
        struct auth_data *auth_data = NULL;
        struct smb2_timeval tv;

        auth_data = calloc(1, sizeof(struct auth_data));
        if (auth_data == NULL) {
                return NULL;
        }

        if (user) {
                auth_data->user = strdup(user);
                if (auth_data->user == NULL) {
                        goto failed;
                }
        }
        if (auth_data_set_password(auth_data, password) < 0) {
                goto failed;
        }
        if (auth_data_set_domain(auth_data, domain) < 0) {
                goto failed;
        }
        if (workstation) {
                auth_data->workstation = strdup(workstation);
                if (auth_data->workstation == NULL) {
                        goto failed;
                }
        }
        auth_data->client_challenge = malloc(8);
        if (auth_data->client_challenge == NULL) {
                goto failed;
        }
        memcpy(auth_data->client_challenge, client_challenge, 8);
        auth_data->is_authenticated = 0;
        memset(auth_data->exported_session_key, 0, SMB2_KEY_SIZE);
        tv.tv_sec = time(NULL);
        tv.tv_usec = 0;
        auth_data->wintime = smb2_timeval_to_win(&tv);

        return auth_data;
 failed:
        free(auth_data->user);
        free(auth_data->password);
        free(auth_data->domain);
        free(auth_data->workstation);
        free(auth_data->client_challenge);
        return NULL;
}

void
ntlmssp_set_spnego_wrapping(struct auth_data *auth, int wrap)
{
        auth->spnego_wrap = wrap;
}

int
ntlmssp_get_spnego_wrapping(struct auth_data *auth)
{
        return auth->spnego_wrap;
}

int
ntlmssp_get_authenticated(struct auth_data *auth)
{
        return auth ? auth->is_authenticated : 0;
}

static int
encoder(const void *buffer, size_t size, void *ptr)
{
        struct auth_data *auth_data = ptr;

        if (size + auth_data->len > auth_data->allocated) {
                unsigned char *tmp = auth_data->buf;

                auth_data->allocated = 2 * ((size + auth_data->allocated + 256) & ~0xff);
                auth_data->buf = malloc(auth_data->allocated);
                if (auth_data->buf == NULL) {
                        free(tmp);
                        return -1;
                }
                memcpy(auth_data->buf, tmp, auth_data->len);
                free(tmp);
        }

        if (auth_data->buf == NULL) {
                return -1;
        }
        memcpy(auth_data->buf + auth_data->len, buffer, size);
        auth_data->len += size;

        return 0;
}

static int
encode_ntlm_negotiate_message(struct smb2_context *smb2, struct auth_data *auth_data)
{
        unsigned char ntlm[40];
        uint32_t flags;
        uint32_t u32;
        int ntlm_len = 32;

        memset(ntlm, 0, sizeof(ntlm));
        memcpy(ntlm, "NTLMSSP", 8);

        u32 = htole32(NEGOTIATE_MESSAGE);
        memcpy(&ntlm[8], &u32, 4);

        flags = NTLMSSP_NEGOTIATE_128|
                NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY|
              /*  NTLMSSP_NEGOTIATE_KEY_EXCH| */
              /*  NTLMSSP_NEGOTIATE_VERSION| */
              /*  NTLMSSP_NEGOTIATE_TARGET_INFO| */
              /*  NTLMSSP_NEGOTIATE_ALWAYS_SIGN| */
                NTLMSSP_NEGOTIATE_NTLM| /* Azure netapp server needs this */
                NTLMSSP_NEGOTIATE_SEAL|
              /*  NTLMSSP_NEGOTIATE_SIGN| */
                NTLMSSP_REQUEST_TARGET|
                NTLMSSP_NEGOTIATE_OEM|
                NTLMSSP_NEGOTIATE_UNICODE;

        u32 = htole32(flags);
        memcpy(&ntlm[12], &u32, 4);

        if (flags & NTLMSSP_NEGOTIATE_VERSION) {
                u32 = 0x1db00106;
                u32 = htole32(u32);
                memcpy(&ntlm[32], &u32, 4);
                u32 = 0x0f000000;
                u32 = htole32(u32);
                memcpy(&ntlm[36], &u32, 4);
                ntlm_len = 40;
        }
        if (encoder(&ntlm[0], ntlm_len, auth_data) < 0) {
                return -1;
        }

        return 0;
}

static int
ntlm_decode_challenge_message(struct smb2_context *smb2, struct auth_data *auth_data,
                        unsigned char *buf, size_t len)
{
        if (buf && len > 0) {
                int alloc_len;
                uint32_t inoff;
                uint16_t inlen;
                uint32_t outoff;
                uint16_t u16;
                uint32_t u32;
                uint16_t infolen;
                uint16_t attr_len;
                uint16_t attr_code;
                struct smb2_utf16 *utf16_spn = NULL;
                const uint32_t challenge_header_len = 56;

                /* form destination SPN in case server is checking */
                free(auth_data->target_info);
                alloc_len = 32 + strlen(smb2->server);
                auth_data->target_info = malloc(alloc_len);
                if (!auth_data->target_info) {
                        return -1;
                }
                auth_data->target_info_len = snprintf((char*)auth_data->target_info,
                        alloc_len, "cifs/%s", smb2->server);

                free(auth_data->ntlm_buf);
                auth_data->ntlm_len = len;
                /* alloc enough to add a target-name attribute */
                alloc_len = auth_data->ntlm_len + 400;
                auth_data->ntlm_buf = malloc(alloc_len);
                if (auth_data->ntlm_buf == NULL) {
                        return -1;
                }
                /* copy challenge message verbatim except payload */
                memcpy(auth_data->ntlm_buf, buf, challenge_header_len);

                /* payload pointer */
                outoff = challenge_header_len;

                /* copy target-name-fields payload from source to dest */
                memcpy(&u16, &buf[12], 2);
                inlen = htole16(u16);
                memcpy(&u32, &buf[16], 4);
                inoff = htole32(u32);

                /* and update offset to where we put it (probably the same offset) */
                u32 = htole32(outoff);
                memcpy(&auth_data->ntlm_buf[16], &u32, 4);

                if (inlen > 0 && inlen < len && (outoff + inlen) < alloc_len) {
                        auth_data->target_name = discard_const(smb2_utf16_to_utf8((const uint16_t *)(void *)&buf[inoff], inlen / 2));
                        memcpy(&auth_data->ntlm_buf[outoff], &buf[inoff], inlen);
                        outoff += inlen;
                }

                memcpy(&u16, &buf[40], 2);
                inlen = htole16(u16);
                memcpy(&u32, &buf[44], 4);
                inoff = htole32(u32);

                infolen = 0;

                if (inlen > 0 && inlen < len && (outoff + inlen) < alloc_len) {
                        /* back annotate target info field offset */
                        u32 = htole32(outoff);
                        memcpy(&auth_data->ntlm_buf[44], &u32, 4);

                        /* transcode target info fields, appending our target-name */
                        while (inlen > 0) {
                                memcpy(&u16, &buf[inoff], 2);
                                attr_code = htole16(u16);
                                memcpy(&u16, &buf[inoff + 2], 2);
                                attr_len = htole16(u16);
                                if (attr_len > inlen || (outoff + attr_len) > alloc_len) {
                                        /* invalid, must be out of parse? */
                                        break;
                                }

                                if (attr_code == 0) { /* end of list */
                                        /*  insert target-name */
                                        if (auth_data->target_info && auth_data->target_info_len) {
                                                utf16_spn = smb2_utf8_to_utf16((char*)auth_data->target_info);
                                                if (utf16_spn != NULL) {
                                                        attr_code = 0x9; /* target-name code */
                                                        attr_len = utf16_spn->len * 2;
                                                        u16 = htole16(attr_code);
                                                        memcpy(&auth_data->ntlm_buf[outoff], &u16, 2);
                                                        u16 = htole16(attr_len);
                                                        memcpy(&auth_data->ntlm_buf[outoff + 2], &u16, 2);
                                                        outoff += 4;
                                                        memcpy(&auth_data->ntlm_buf[outoff],
                                                                (uint8_t*)utf16_spn->val, attr_len);
                                                        outoff += attr_len;
                                                        infolen += 4 + attr_len;
                                                        free(utf16_spn);
                                                }
                                        }
                                        /* insert original end of list attr */
                                        u16 = 0;
                                        memcpy(&auth_data->ntlm_buf[outoff], &u16, 2);
                                        memcpy(&auth_data->ntlm_buf[outoff + 2], &u16, 2);
                                        outoff += 4;
                                        attr_code = 0;
                                        attr_len = 0;
                                } else {
                                        u16 = htole16(attr_code);
                                        memcpy(&auth_data->ntlm_buf[outoff], &u16, 2);
                                        u16 = htole16(attr_len);
                                        memcpy(&auth_data->ntlm_buf[outoff + 2], &u16, 2);
                                        outoff += 4;
                                        memcpy(&auth_data->ntlm_buf[outoff], &buf[inoff + 4], attr_len);
                                        outoff += attr_len;
                                }

                                inoff += 4 + attr_len;
                                inlen -= 4 + attr_len;
                                infolen += 4 + attr_len;
                        }

                        /* back annotate target info field len */
                        u16 = htole16(infolen);
                        memcpy(&auth_data->ntlm_buf[40], &u16, 2);
                        memcpy(&auth_data->ntlm_buf[42], &u16, 2);

                        /* set the actual length of total message */
                        auth_data->ntlm_len = outoff;
                }
                return 0;
        }

        return -1;
}

static int
ntlm_convert_password_hash(const char *password, unsigned char password_hash[16])
{
        int i, hn, ln;
        struct smb2_utf16 *utf16_password = NULL;

        utf16_password = smb2_utf8_to_utf16(password);
        if (utf16_password == NULL) {
                return -1;
        }

        for (i = 0; i < 32; i++) {
                utf16_password->val[i] = le16toh(utf16_password->val[i]);
                if (islower((unsigned int) utf16_password->val[i])) {
                        utf16_password->val[i] = toupper((unsigned int) utf16_password->val[i]);
                }
        }

        /* FreeRDP: winpr/libwinpr/sspi/NTLM/ntlm_compute.c */
        for (i = 0; i < 32; i += 2)
        {
                hn = utf16_password->val[i] > '9' ? utf16_password->val[i] - 'A' + 10 : utf16_password->val[i] - '0';
                ln = utf16_password->val[i + 1] > '9' ? utf16_password->val[i + 1] - 'A' + 10 : utf16_password->val[i + 1] - '0';
                password_hash[i / 2] = (hn << 4) | ln;
        }

        return 0;
}

static int
NTOWFv1(const char *password, unsigned char password_hash[16])
{
        MD4_CTX ctx;
        struct smb2_utf16 *utf16_password = NULL;

        utf16_password = smb2_utf8_to_utf16(password);
        if (utf16_password == NULL) {
                return -1;
        }
        MD4Init(&ctx);
        MD4Update(&ctx, (unsigned char *)utf16_password->val, utf16_password->len * 2);
        MD4Final(password_hash, &ctx);
        free(utf16_password);

        return 0;
}

static int
NTOWFv2(const char *user, const char *password, const char *domain,
        unsigned char ntlmv2_hash[16])
{
        int64_t i;
        size_t len;
        char *userdomain;
        struct smb2_utf16 *utf16_userdomain = NULL;
        unsigned char ntlm_hash[16];

        if (user == NULL || password == NULL) {
                return -1;
        }

        /* ntlm:F638EDF864C4805DC65D9BF2BB77E4C0 */
        if ((strlen(password) == 37) && (strncmp(password, "ntlm:", 5) == 0)) {
                if (ntlm_convert_password_hash(password + 5, ntlm_hash) < 0) {
                        return -1;
                }
        } else {
                if (NTOWFv1(password, ntlm_hash) < 0) {
                        return -1;
                }
        }

        len = strlen(user) + 1;
        if (domain) {
                len += strlen(domain);
        }
        userdomain = malloc(len);
        if (userdomain == NULL) {
                return -1;
        }

        strcpy(userdomain, user);
        for (i = strlen(userdomain) - 1; i >= 0; i--) {
                if (islower((unsigned int) userdomain[i])) {
                        userdomain[i] = toupper((unsigned int) userdomain[i]);
                }
        }
        if (domain) {
                strcat(userdomain, domain);
        }

        utf16_userdomain = smb2_utf8_to_utf16(userdomain);
        if (utf16_userdomain == NULL) {
                free(userdomain);
                return -1;
        }

        smb2_hmac_md5((unsigned char *)utf16_userdomain->val,
                 utf16_userdomain->len * 2,
                 ntlm_hash, 16, ntlmv2_hash);
        free(userdomain);
        free(utf16_userdomain);

        return 0;
}

/* This is not the same temp as in MS-NLMP. This temp has an additional
 * 16 bytes at the start of the buffer.
 * Use &auth_data->val[16] if you want the temp from MS-NLMP
 */
static int
encode_temp(struct auth_data *auth_data, uint64_t t,
                uint8_t *client_challenge, size_t client_challenge_len,
                uint8_t *server_challenge,
                uint8_t *server_name, size_t server_name_len)
{
        unsigned char sign[8] = {0x01, 0x01, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        unsigned char zero[8] = {0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00};
        uint64_t u64;

        if (encoder(&zero, 8, auth_data) < 0) {
                return -1;
        }
        if (encoder(server_challenge, 8, auth_data) < 0) {
                return -1;
        }
        if (encoder(sign, 8, auth_data) < 0) {
                return -1;
        }
        u64 = htole64(t);
        if (encoder(&u64, 8, auth_data) < 0) {
                return -1;
        }
        if (encoder(client_challenge, client_challenge_len, auth_data) < 0) {
                return -1;
        }
        if (encoder(&zero, 4, auth_data) < 0) {
                return -1;
        }
        if (encoder(server_name, server_name_len, auth_data) < 0) {
                return -1;
        }
        if (encoder(&zero, 4, auth_data) < 0) {
                return -1;
        }
        return 0;
}

static int
encode_ntlm_auth(struct smb2_context *smb2, time_t ti,
                 struct auth_data *auth_data, char *server_challenge)
{
        int ret = -1;
        unsigned char lm_buf[16] _U_;
        unsigned char *NTChallengeResponse_buf = NULL;
        unsigned char ResponseKeyNT[16];
        struct smb2_utf16 *utf16_domain = NULL;
        struct smb2_utf16 *utf16_user = NULL;
        struct smb2_utf16 *utf16_workstation = NULL;
        unsigned int NTChallengeResponse_len = 0;
        unsigned char NTProofStr[16];
        uint64_t t;
        struct smb2_timeval tv _U_;
        char *server_name_buf;
        uint32_t server_name_len;
        uint32_t u32;
        uint32_t server_neg_flags;
        unsigned char key_exch[SMB2_KEY_SIZE];
        uint8_t anonymous = 0;

        tv.tv_sec = ti;
        tv.tv_usec = 0;
        t = smb2_timeval_to_win(&tv);

        /*
         * If we discovered the domain (and a new associated password in NTLM_USER_FILE)
         * on receiving the challenge message we need to update auth_data with the
         * new domain/password.
         */
        if (auth_data_set_password(auth_data, smb2->password) < 0) {
                goto finished;
        }
        if (auth_data_set_domain(auth_data, smb2->domain) < 0) {
                goto finished;
        }

        if (auth_data->password == NULL) {
                anonymous = 1;
                goto encode;
        }
        /*
         * Generate Concatenation of(NTProofStr, temp)
         */
        if (NTOWFv2(auth_data->user, auth_data->password,
                    auth_data->domain, ResponseKeyNT) < 0) {
                goto finished;
        }

        /* Must have at least enough bytes for server name offset */
        if (auth_data->ntlm_len < 47) {
                goto finished;
        }
        /* get the server neg flags */
        memcpy(&server_neg_flags, &auth_data->ntlm_buf[20], 4);
        server_neg_flags = le32toh(server_neg_flags);

        memcpy(&u32, &auth_data->ntlm_buf[40], 4);
        u32 = le32toh(u32);
        server_name_len = u32 >> 16;

        memcpy(&u32, &auth_data->ntlm_buf[44], 4);
        u32 = le32toh(u32);
        /* Server name must fit in the buffer */
        if (u32 >= auth_data->ntlm_len ||
            (u32 + server_name_len) > auth_data->ntlm_len) {
                goto finished;
        }

        /* note - this is the target-info sent in the challenge perhaps
         * modified to add a target-name attribute
         */
        server_name_buf = (char *)&auth_data->ntlm_buf[u32];

        if (encode_temp(auth_data, t,
                        auth_data->client_challenge, 8,
                        (uint8_t*)server_challenge,
                        (uint8_t*)server_name_buf, server_name_len) < 0) {
                return -1;
        }

        smb2_hmac_md5(&auth_data->buf[8], (unsigned int)auth_data->len-8,
                 ResponseKeyNT, 16, NTProofStr);
        memcpy(auth_data->buf, NTProofStr, 16);

        NTChallengeResponse_buf = auth_data->buf;
        NTChallengeResponse_len = (unsigned int)auth_data->len;

        auth_data->buf = NULL;
        auth_data->len = 0;
        auth_data->allocated = 0;

        /* get the NTLMv2 Key-Exchange Key
           For NTLMv2 - Key Exchange Key is the Session Base Key
         */
        smb2_hmac_md5(NTProofStr, 16, ResponseKeyNT, 16, key_exch);
        memcpy(auth_data->exported_session_key, key_exch, 16);

 encode:
        /*
         * Generate AUTHENTICATE_MESSAGE
         */
        encoder("NTLMSSP", 8, auth_data);

        /* message type */
        u32 = htole32(AUTHENTICATION_MESSAGE);
        encoder(&u32, 4, auth_data);

        /* lm challenge response fields */
        u32 = 0;
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* nt challenge response fields */
        u32 = htole32((NTChallengeResponse_len<<16)|
                      NTChallengeResponse_len);
        encoder(&u32, 4, auth_data);
        u32 = 0;
        encoder(&u32, 4, auth_data);

        /* domain name fields */
        if (!anonymous && auth_data->domain) {
                utf16_domain = smb2_utf8_to_utf16(auth_data->domain);
                if (utf16_domain == NULL) {
                        goto finished;
                }
                u32 = utf16_domain->len * 2;
                u32 = htole32((u32 << 16) | u32);
                encoder(&u32, 4, auth_data);
                u32 = 0;
                encoder(&u32, 4, auth_data);
        } else {
                u32 = 0;
                encoder(&u32, 4, auth_data);
                encoder(&u32, 4, auth_data);
        }

        /* user name fields */
        if (!anonymous) {
                utf16_user = smb2_utf8_to_utf16(auth_data->user);
                if (utf16_user == NULL) {
                        goto finished;
                }
                u32 = utf16_user->len * 2;
                u32 = htole32((u32 << 16) | u32);
                encoder(&u32, 4, auth_data);
                u32 = 0;
                encoder(&u32, 4, auth_data);
        } else {
                u32 = 0;
                encoder(&u32, 4, auth_data);
                encoder(&u32, 4, auth_data);
        }

        /* workstation name fields */
        if (!anonymous && auth_data->workstation) {
                utf16_workstation = smb2_utf8_to_utf16(auth_data->workstation);
                if (utf16_workstation == NULL) {
                        goto finished;
                }
                u32 = utf16_workstation->len * 2;
                u32 = htole32((u32 << 16) | u32);
                encoder(&u32, 4, auth_data);
                u32 = 0;
                encoder(&u32, 4, auth_data);
        } else {
                u32 = 0;
                encoder(&u32, 4, auth_data);
                encoder(&u32, 4, auth_data);
        }

        /* encrypted random session key */
        u32 = 0;
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* negotiate flags */
        u32 = NTLMSSP_NEGOTIATE_128|
                NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY|
                NTLMSSP_NEGOTIATE_ALWAYS_SIGN|
                NTLMSSP_NEGOTIATE_SIGN|
                NTLMSSP_REQUEST_TARGET|NTLMSSP_NEGOTIATE_OEM|
                NTLMSSP_NEGOTIATE_UNICODE;
        if (anonymous)
                u32 |= NTLMSSP_NEGOTIATE_ANONYMOUS;
        else
                u32 |= NTLMSSP_NEGOTIATE_SEAL;

        u32 = htole32(u32);
        encoder(&u32, 4, auth_data);

        /* version - 8 bytes of 0 */
        u32 = 0;
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* MIC - 16 byte message integrity code (after windows 2003) */

        if (!anonymous) {
                /* append domain */
                u32 = htole32((uint32_t)auth_data->len);
                memcpy(&auth_data->buf[32], &u32, 4);
                if (utf16_domain) {
                        encoder(utf16_domain->val, utf16_domain->len * 2,
                                auth_data);
                }

                /* append user */
                u32 = htole32((uint32_t)auth_data->len);
                memcpy(&auth_data->buf[40], &u32, 4);
                encoder(utf16_user->val, utf16_user->len * 2, auth_data);

                /* append workstation */
                u32 = htole32((uint32_t)auth_data->len);
                memcpy(&auth_data->buf[48], &u32, 4);
                if (utf16_workstation) {
                        encoder(utf16_workstation->val,
                                utf16_workstation->len * 2, auth_data);
                }

                /* append NTChallengeResponse */
                u32 = htole32((uint32_t)auth_data->len);
                memcpy(&auth_data->buf[24], &u32, 4);
                encoder(NTChallengeResponse_buf, NTChallengeResponse_len,
                        auth_data);
        }

        ret = 0;
finished:
        free(utf16_domain);
        free(utf16_user);
        free(utf16_workstation);
        free(NTChallengeResponse_buf);

        return ret;
}

static int
encode_ntlm_challenge(struct smb2_context *smb2, struct auth_data *auth_data)
{
        int ret = -1;
        struct smb2_utf16 *utf16_workstation = NULL;
        struct smb2_utf16 *utf16_workstation_upper = NULL;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        uint8_t anonymous = 0;
        int target_info_pos;
        int namelen;
     int cc;
        char *upper = NULL;

        /* Generate CHALLENGE_MESSAGE  */
        encoder("NTLMSSP", 8, auth_data);

        /* message type */
        u32 = htole32(CHALLENGE_MESSAGE);
        encoder(&u32, 4, auth_data);

        /* target name fields */
        u32 = 0;
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* negotiate flags */
        u32 = NTLMSSP_NEGOTIATE_128|
                NTLMSSP_NEGOTIATE_TARGET_INFO|
                NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY|
                NTLMSSP_NEGOTIATE_ALWAYS_SIGN|
                NTLMSSP_NEGOTIATE_SIGN|
         /*       NTLMSSP_NEGOTIATE_KEY_EXCH| */
                NTLMSSP_REQUEST_TARGET|NTLMSSP_NEGOTIATE_OEM|
                NTLMSSP_NEGOTIATE_VERSION|
                NTLMSSP_NEGOTIATE_UNICODE;
        if (anonymous)
                u32 |= NTLMSSP_NEGOTIATE_ANONYMOUS;
        else
                u32 |= NTLMSSP_NEGOTIATE_SEAL;

        u32 = htole32(u32);
        encoder(&u32, 4, auth_data);

        /* server challenge */
        for (cc = 0; cc < 8; cc++) {
                auth_data->server_challenge[cc] = cc + 1;
        }
        encoder(auth_data->server_challenge, 8, auth_data);

        /* reserved */
        u32 = 0;
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* target into fields */
        encoder(&u32, 4, auth_data);
        encoder(&u32, 4, auth_data);

        /* version (if we set negotiate version flag */
        u32 = htole32(0x00000106);
        encoder(&u32, 4, auth_data);
        u32 = htole32(0x0F000000);
        encoder(&u32, 4, auth_data);

        /* target name  */
        if (auth_data->workstation) {
             int i;
                namelen = strlen(auth_data->workstation);
                upper = malloc(namelen + 1);
                if (!upper) {
                        return -1;
                }
                for (i = 0; i < namelen; i++) {
                        upper[i] = toupper(auth_data->workstation[i]);
                }
                upper[namelen] = 0;
                utf16_workstation = smb2_utf8_to_utf16(auth_data->workstation);
                if (utf16_workstation == NULL) {
                        goto finished;
                }
                utf16_workstation_upper = smb2_utf8_to_utf16(upper);
                if (utf16_workstation_upper == NULL) {
                        goto finished;
                }
                u32 = htole32(auth_data->len);
                memcpy(&auth_data->buf[16], &u32, 4);
                u32 = utf16_workstation->len * 2;
                u32 = htole32((u32 << 16) | u32);
                memcpy(&auth_data->buf[12], &u32, 4);
                encoder(utf16_workstation_upper->val,
                                utf16_workstation_upper->len * 2, auth_data);
        }
        /* target info fields */
        target_info_pos = auth_data->len;
        if (utf16_workstation) {
                u16 = 0x0002; /* netbios domain */
                encoder(&u16, 2, auth_data);
                u16 = utf16_workstation_upper->len * 2;
                encoder(&u16, 2, auth_data);
                encoder(utf16_workstation_upper->val,
                               utf16_workstation_upper->len * 2, auth_data);
                u16 = 0x0001;  /* netbios computer name */
                encoder(&u16, 2, auth_data);
                u16 = utf16_workstation->len * 2;
                encoder(&u16, 2, auth_data);
                encoder(utf16_workstation_upper->val,
                               utf16_workstation_upper->len * 2, auth_data);
                u16 = 0x0004;  /* dns domain name */
                encoder(&u16, 2, auth_data);
                u16 = 0;
                encoder(&u16, 2, auth_data);
                u16 = 0x0003;  /* dns computer name */
                encoder(&u16, 2, auth_data);
                u16 = utf16_workstation->len * 2;
                encoder(&u16, 2, auth_data);
                encoder(utf16_workstation->val,
                               utf16_workstation->len * 2, auth_data);
        }

        /*  target info timestamp */
        u16 = 0x0007;
        encoder(&u16, 2, auth_data);
        u16 = 8;
        encoder(&u16, 2, auth_data);
        u64 =  auth_data->wintime;
        encoder(&u64, 8, auth_data);

        /* end of info */
        u32 = 0;
        encoder(&u32, 4, auth_data);

        /* save the target info in auth-data for later */
        auth_data->target_info_len = auth_data->len - target_info_pos;
        auth_data->target_info = malloc(auth_data->target_info_len);
        memcpy(auth_data->target_info,
                        auth_data->buf + target_info_pos,
                        auth_data->target_info_len);

        /* back annotate length of target info  */
        u16 = htole16(auth_data->len - target_info_pos);
        memcpy(&auth_data->buf[40], &u16, 2);
        memcpy(&auth_data->buf[42], &u16, 2);
        u16 = htole16(target_info_pos);
        memcpy(&auth_data->buf[44], &u16, 2);

        ret = 0;
finished:
        if (upper) {
                free(upper);
        }
        if (utf16_workstation) {
                free(utf16_workstation);
        }
        if (utf16_workstation_upper) {
                free(utf16_workstation_upper);
        }
        return ret;
}

int
ntlmssp_generate_blob(struct smb2_server *server, struct smb2_context *smb2, time_t t,
                      struct auth_data *auth_data,
                      unsigned char *input_buf, int input_len,
                      unsigned char **output_buf, uint16_t *output_len)
{
        uint32_t cmd;
        uint8_t *ntlmssp;
        int ntlmssp_len;
        uint8_t *spnego_buf;
        int spnego_len;
        int is_wrapped;

        free(auth_data->buf);
        auth_data->buf = NULL;
        auth_data->len = 0;
        auth_data->allocated = 0;

        if (input_buf == NULL) {
                if (smb2_is_server(smb2)) {
                        return -1;
                }
                encode_ntlm_negotiate_message(smb2, auth_data);

                if (auth_data->spnego_wrap) {
                        spnego_len = smb2_spnego_wrap_gssapi(smb2, auth_data->buf,
                                       auth_data->len, (void*)&spnego_buf);
                        if (spnego_len < 0) {
                                smb2_set_error(smb2, "can not wrap negotiate");
                                return -1;
                        }
                        free(auth_data->buf);
                        auth_data->buf = spnego_buf;
                        auth_data->len = spnego_len;
                }
        }
        else {
                if(ntlmssp_get_message_type(smb2, input_buf,
                                input_len,
                                0,
                                &cmd,
                                &ntlmssp, &ntlmssp_len,
                                &is_wrapped) < 0) {
                        ntlmssp_len = 0;
                }
                if (ntlmssp_len < 12) {
                        smb2_set_error(smb2, "no message type in NTLMSSP blob");
                        return -1;
                }
                if (is_wrapped) {
                        auth_data->spnego_wrap = 1;
                }
                if (smb2_is_server(smb2)) {
                        if (cmd == NEGOTIATE_MESSAGE) {
                                if (encode_ntlm_challenge(smb2, auth_data)) {
                                        smb2_set_error(smb2, "can not encode challenge");
                                        return -1;
                                }
                                if (auth_data->spnego_wrap) {
                                        spnego_len = smb2_spnego_wrap_ntlmssp_challenge(smb2,
                                                        auth_data->buf,
                                                         auth_data->len, (void*)&spnego_buf);
                                        if (spnego_len < 0) {
                                                smb2_set_error(smb2, "can not wrap challenge");
                                                return -1;
                                        }
                                        free(auth_data->buf);
                                        auth_data->buf = spnego_buf;
                                        auth_data->len = spnego_len;
                                }
                        }
                        else if (cmd == AUTHENTICATION_MESSAGE) {
                                auth_data->is_authenticated = !ntlmssp_authenticate_blob(server,
                                                        smb2, auth_data,
                                                        ntlmssp, ntlmssp_len);
                                if (auth_data->spnego_wrap) {
                                        spnego_len = smb2_spnego_wrap_authenticate_result(smb2,
                                                                auth_data->is_authenticated, (void*)&spnego_buf);
                                        if (spnego_len < 0) {
                                                smb2_set_error(smb2, "can not wrap auth result");
                                                return -1;
                                        }
                                        free(auth_data->buf);
                                        auth_data->buf = spnego_buf;
                                        auth_data->len = spnego_len;
                                }
                        }
                        else {
                                return -1;
                        }
                }
                else {
                        if (cmd == CHALLENGE_MESSAGE) {
                                if (ntlm_decode_challenge_message(smb2, auth_data,
                                                ntlmssp, ntlmssp_len) < 0) {
                                        smb2_set_error(smb2, "can not decode challenge");
                                        return -1;
                                }
                                if (auth_data->domain == NULL && auth_data->target_name) {
                                        smb2_set_domain(smb2, auth_data->target_name);
                                        auth_data->domain = strdup(auth_data->target_name);
                                        if (auth_data->domain == NULL) {
                                                return -1;
                                        }
                                        /* Update the password now that we know the domain */
                                        smb2_set_password_from_file(smb2);
                                }
                                if (encode_ntlm_auth(smb2, t, auth_data,
                                                     (char *)&auth_data->ntlm_buf[24]) < 0) {
                                        smb2_set_error(smb2, "can not encode auth data");
                                        return -1;
                                }
                                if (auth_data->spnego_wrap) {
                                        spnego_len = smb2_spnego_wrap_ntlmssp_auth(smb2,
                                                                auth_data->buf, auth_data->len,
                                                                (void*)&spnego_buf);
                                        if (spnego_len < 0) {
                                                smb2_set_error(smb2, "can not wrap auth result");
                                                return -1;
                                        }
                                        free(auth_data->buf);
                                        auth_data->buf = spnego_buf;
                                        auth_data->len = spnego_len;
                                }
                        }
                        else {
                                smb2_set_error(smb2, "Unexpected NTLMSSP message %08X, wanted challenge", cmd);
                                return -1;
                        }
                }
        }

        *output_buf = auth_data->buf;
        *output_len = (uint16_t)auth_data->len;

        return 0;
}

void
ntlmssp_get_utf16_field(uint8_t *input_buf, int input_len, int offset, char **result)
{
        uint32_t field_len;
        uint32_t field_off;
        uint32_t u32;

        *result = NULL;

        if (offset > (input_len - 8)) {
                return;
        }
        memcpy(&u32, &input_buf[offset], 4);
        field_len = le32toh(u32) >> 16;
        memcpy(&u32, &input_buf[offset + 4], 4);
        field_off = le32toh(u32);
        if (field_len && field_off) {
                *result = (char*)smb2_utf16_to_utf8((uint16_t *)(void *)(input_buf + field_off), field_len / 2);
        }
}

int
ntlmssp_authenticate_blob(struct smb2_server *server, struct smb2_context *smb2,
                      struct auth_data *auth_data,
                      unsigned char *input_buf, int input_len)
{
        unsigned char ResponseKeyNT[16];
        unsigned char NTProofStr[16];
        unsigned char key_exch[SMB2_KEY_SIZE];
        uint32_t field_len;
        uint32_t field_off;
        uint32_t challenge_len;
        uint8_t *response;
        uint8_t *temp;
        uint32_t temp_len;
        int ret = -1;
        /* uint32_t negotiate_flags; */
        uint32_t u32;

        if (!input_buf || (input_len < 8) || memcmp(input_buf, "NTLMSSP", 8)) {
                return -1;
        }
        memcpy(&u32, &input_buf[4*2], 4);
        u32 = le32toh(u32);
        if (u32 != AUTHENTICATION_MESSAGE) {
                return -1;
        }
        if (auth_data->domain) {
                free(auth_data->domain);
                auth_data->domain = NULL;
        }
        if (auth_data->user) {
                free(auth_data->user);
                auth_data->user = NULL;
        }
        if (auth_data->workstation) {
                free(auth_data->workstation);
                auth_data->workstation = NULL;
        }
        ntlmssp_get_utf16_field(input_buf, input_len, 4*7, &auth_data->domain);
        ntlmssp_get_utf16_field(input_buf, input_len, 4*9, &auth_data->user);
        ntlmssp_get_utf16_field(input_buf, input_len, 4*11, &auth_data->workstation);
        memcpy(&u32, &input_buf[4*15], 4);

        smb2_set_user(smb2, auth_data->user);
        smb2_set_domain(smb2, auth_data->domain);
        smb2_set_workstation(smb2, auth_data->workstation);

        /* call server handler to get pw for this user */
        if (server && server->handlers) {
                if(server->handlers->authorize_user(server, smb2,
                                auth_data->user,
                                auth_data->domain,
                                auth_data->workstation)) {
                        smb2_set_error(smb2, "server can not authorize %s",
                                auth_data->user);
                        return -1;
                }
                if (!smb2->password && !server->allow_anonymous) {
                        smb2_set_error(smb2, "server has no passwd for %s",
                                auth_data->user);
                        return -1;
                }
        }
        /* if no user/pw, and anonymous allowed, do anonymous */
        if (!auth_data->user || (auth_data->user[0] == '\0') ||
                        !smb2->password || (smb2->password[0] == '\0')) {
                if (server->allow_anonymous) {
                        return 0;
                }
                return -1;
        }

        /* negotiate_flags = le32toh(u32); */

        /* Lan Man response (we dont even look at, its obsolete) */

        /* NTLM response */
        memcpy(&u32, &input_buf[4*5], 4);
        field_len = le32toh(u32) >> 16;
        memcpy(&u32, &input_buf[4*6], 4);
        field_off = le32toh(u32);
        if (field_len == 0 || field_off == 0) {
                return -1;
        }
        if (field_off > (uint32_t)input_len) {
                return -1;
        }
        /* 16 byte NTLMv2 response */
        response = input_buf + field_off;
        challenge_len = field_len - 16;
        if (challenge_len > 9*4) {
                temp = input_buf + field_off + 16;
                temp_len = field_len - 16;
                if (auth_data->client_challenge) {
                        free(auth_data->client_challenge);
                }
                auth_data->client_challenge = malloc(8);
                memcpy(auth_data->client_challenge, input_buf + field_off + 32, 8);
        }
        else {
                smb2_set_error(smb2, "bad NTLMSSP challenge len %d",
                       challenge_len);
                return -1;
        }
        if (NTOWFv2(auth_data->user, smb2->password,
                    auth_data->domain, ResponseKeyNT) < 0) {
                return -1;
        }
        /* wipe pw out now that its been used */
        smb2_set_password(smb2, "");
        auth_data->len = 0;
        if (encoder(auth_data->server_challenge, 8, auth_data)) {
                return -1;
        }
        if (encoder(temp, temp_len, auth_data)) {
                return -1;
        }
        temp = auth_data->buf;
        temp_len = auth_data->len;

        smb2_hmac_md5(temp, temp_len, ResponseKeyNT, 16, NTProofStr);
        memcpy(auth_data->buf, NTProofStr, 16);

        /* verify ntproof */
        if (memcmp(NTProofStr, response, 16)) {
                smb2_set_error(smb2, "NTLMSSP NTProof != response. Auth failed");
                goto fail;
        }
        smb2_hmac_md5(NTProofStr, 16, ResponseKeyNT, 16, key_exch);
        memcpy(auth_data->exported_session_key, key_exch, 16);
        ret = 0;
fail:
        free(auth_data->buf);
        auth_data->buf = NULL;
        auth_data->len = 0;
        return ret;
}


int
ntlmssp_get_session_key(struct auth_data *auth,
                        uint8_t **key,
                        uint8_t *key_size)
{
        uint8_t *mkey = NULL;

        if (auth == NULL || key == NULL || key_size == NULL) {
                return -1;
        }

        mkey = (uint8_t *) malloc(SMB2_KEY_SIZE);
        if (mkey == NULL) {
                return -1;
        }
        memcpy(mkey, auth->exported_session_key, SMB2_KEY_SIZE);

        *key = mkey;
        *key_size = SMB2_KEY_SIZE;

        return 0;
}

int
ntlmssp_get_message_type(struct smb2_context *smb2,
                        uint8_t *buffer, int len,
                        int suppress_errors,
                        uint32_t *message_type,
                        uint8_t **ntlmssp_ptr, int *ntlmssp_len,
                        int *is_wrapped)
{
        uint8_t *ntlmssp = NULL;
        uint32_t u32;
        uint32_t mechanisms;
        int ntlm_len;

        if (message_type) {
                *message_type = 0xFFFFFFFF;
        }
        if (ntlmssp_ptr) {
                *ntlmssp_ptr = NULL;
        }
        if (ntlmssp_len) {
                *ntlmssp_len = 0;
        }
        if (!buffer || len < 12) {
                return -1;
        }

        ntlm_len = smb2_spnego_unwrap_blob(smb2, buffer, len,
                       suppress_errors, &ntlmssp, &mechanisms);
        if (ntlm_len < 12 || !ntlmssp) {
                return -1;
        }
        if (ntlmssp != buffer) {
                if (is_wrapped) {
                        *is_wrapped = 1;
                }
        }
        else {
                if (is_wrapped) {
                        *is_wrapped = 0;
                }
        }
        if (ntlmssp_ptr) {
                *ntlmssp_ptr = ntlmssp;
        }
        if (ntlmssp_len) {
                *ntlmssp_len = ntlm_len;
        }
        if (!memcmp(ntlmssp, "NTLMSSP", 7)) {
                memcpy(&u32, ntlmssp + 8, sizeof(uint32_t));
                if (message_type) {
                        *message_type = le32toh(u32);
                }
                return 0;
        }
        return -1;
}

