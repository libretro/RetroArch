/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2024 by Brian Dodge <bdodge09@gmail.com>
   Copyright (C) 2024 by André Guilherme <andregui17@outlook.com>

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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"
#include "asn1-ber.h"

#include "spnego-wrapper.h"

static const struct asn1ber_oid_value oid_gss_mech_spnego = {
        7, { 1, 3, 6, 1, 5, 5, 2 }
};

static const struct asn1ber_oid_value oid_spnego_mech_krb5 = {
        7, { 1, 2, 840, 113554,  1, 2, 2 }
};

/* hack for older microsoft systems, note its the same as krb5 oid if only 16-bit elements */
static const struct asn1ber_oid_value oid_spnego_mech_ms_krb5 = {
        7, { 1, 2, 840, 48018,  1, 2, 2 }
};

static const struct asn1ber_oid_value oid_spnego_mech_ntlmssp = {
        10, { 1, 3, 6, 1, 4, 1, 311, 2, 2, 10 }
};

static int
oid_compare(const struct asn1ber_oid_value *a, const struct asn1ber_oid_value *b)
{
        int i;

        if (!a || !b || !a->length || !b->length || a->length != b->length) {
                return -1;
        }
        for (i = 0; i < a->length; i++) {
                if (a->elements[i] < b->elements[i]) {
                        return -1;
                }
                if (a->elements[i] > b->elements[i]) {
                        return 1;
                }
        }
        return 0;
}

int
smb2_spnego_create_negotiate_reply_blob(struct smb2_context *smb2, int allow_ntlmssp, void **neg_init_token)
{
        struct asn1ber_context asn_encoder;
        uint8_t *neg_init;
        int alloc_len;
        int pos[6];

        alloc_len = 5 * sizeof oid_gss_mech_spnego;
        neg_init = calloc(1, alloc_len);
        if (neg_init == NULL) {
                smb2_set_error(smb2, "Failed to allocate negotiate token init");
                return 0;
        }

        memset(&asn_encoder, 0, sizeof(asn_encoder));
        asn_encoder.dst = neg_init;
        asn_encoder.dst_size = alloc_len;
        asn_encoder.dst_head = 0;

        asn1ber_ber_from_typecode(&asn_encoder, asnCONSTRUCTOR | asnAPPLICATION);
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[0]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* insert top level oid */
        asn1ber_ber_from_oid(&asn_encoder, &oid_gss_mech_spnego);

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));
        /* save location of length of sub mechanisms */
        asn1ber_save_out_state(&asn_encoder, &pos[1]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, asnSTRUCT);
        /* save location of length of mechanism struct */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* constructed */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));
        /* save location of length of mechanism sequence */
        asn1ber_save_out_state(&asn_encoder, &pos[3]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));
        /* save location of length of mechanism struct */
        asn1ber_save_out_state(&asn_encoder, &pos[4]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* for each negotiable mechanism */

#ifdef HAVE_LIBKRB5
        /* insert mechanism oids */
        asn1ber_ber_from_oid(&asn_encoder, &oid_spnego_mech_krb5);
#endif
        if (allow_ntlmssp) {
                /* insert mechanism oids */
                asn1ber_ber_from_oid(&asn_encoder, &oid_spnego_mech_ntlmssp);
        }
        asn1ber_annotate_length(&asn_encoder, pos[4], 5);
        asn1ber_annotate_length(&asn_encoder, pos[3], 5);
        asn1ber_annotate_length(&asn_encoder, pos[2], 5);
        asn1ber_annotate_length(&asn_encoder, pos[1], 5);
        asn1ber_annotate_length(&asn_encoder, pos[0], 5);

        *neg_init_token = neg_init;
        return asn_encoder.dst_head;
}

int smb2_spnego_wrap_gssapi(struct smb2_context *smb2,
                const uint8_t *ntlmssp_token,
                const int token_len, void **blob)
{
        struct asn1ber_context asn_encoder;
        uint8_t *neg_init;
        int alloc_len;
        int pos[8];

        alloc_len = 256 + 4 * token_len;
        neg_init = calloc(1, alloc_len);
        if (neg_init == NULL) {
                smb2_set_error(smb2, "Failed to allocate spnego wrapper");
                return 0;
        }

        memset(&asn_encoder, 0, sizeof(asn_encoder));
        asn_encoder.dst = neg_init;
        asn_encoder.dst_size = alloc_len;
        asn_encoder.dst_head = 0;

        /*  blob */
        asn1ber_ber_from_typecode(&asn_encoder, asnCONSTRUCTOR | asnAPPLICATION); /* 60 ZZ */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[0]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* gss-spnego mech  oid */
        asn1ber_ber_from_oid(&asn_encoder, &oid_gss_mech_spnego);

        /* context 0 */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));               /* A0 XX */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[1]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* sequence 0 */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));               /* 30 YY */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* context 0 mech types */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));               /* A0 zz */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[3]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* sequence 0 of mech types */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));               /* 30 xx */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[4]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* ntlmssp mech oid */
        asn1ber_ber_from_oid(&asn_encoder, &oid_spnego_mech_ntlmssp);

        /* end sequenuce of mech types */
        asn1ber_annotate_length(&asn_encoder, pos[4], 5);

        /* end mech types */
        asn1ber_annotate_length(&asn_encoder, pos[3], 5);

        if (ntlmssp_token && token_len) {
                /* context 2 mech token */
                asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(2));               /* A2 yy */
                /* save location of total length */
                asn1ber_save_out_state(&asn_encoder, &pos[3]);
                asn1ber_ber_reserve_length(&asn_encoder, 5);

                /* ntlmssp token */
                asn1ber_ber_from_typecode(&asn_encoder, asnOCTET_STRING);               /* 04 nn */
                /* save location of total length */
                asn1ber_save_out_state(&asn_encoder, &pos[4]);
                asn1ber_ber_reserve_length(&asn_encoder, 5);

                memcpy(asn_encoder.dst + asn_encoder.dst_head, ntlmssp_token, token_len);
                asn_encoder.dst_head += token_len;

                asn1ber_annotate_length(&asn_encoder, pos[4], 5);
        }
        asn1ber_annotate_length(&asn_encoder, pos[3], 5);
        asn1ber_annotate_length(&asn_encoder, pos[2], 5);
        asn1ber_annotate_length(&asn_encoder, pos[1], 5);
        asn1ber_annotate_length(&asn_encoder, pos[0], 5);

        *blob = neg_init;
        return asn_encoder.dst_head;
}

int
smb2_spnego_wrap_ntlmssp_challenge(struct smb2_context *smb2, const uint8_t *ntlmssp_token,
               const int token_len, void **neg_init_token)
{
        struct asn1ber_context asn_encoder;
        uint8_t *neg_init;
        int alloc_len;
        int pos[6];
        uint8_t neg_result = 1;

        alloc_len = 64 + 2 * token_len;
        neg_init = calloc(1, alloc_len);
        if (neg_init == NULL) {
                smb2_set_error(smb2, "Failed to allocate spnego wrapper");
                return 0;
        }

        memset(&asn_encoder, 0, sizeof(asn_encoder));
        asn_encoder.dst = neg_init;
        asn_encoder.dst_size = alloc_len;
        asn_encoder.dst_head = 0;

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(1));               /* A1 XX */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[0]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));              /* 30 YY */
        /* save location of sub length */
        asn1ber_save_out_state(&asn_encoder, &pos[1]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* negTokenTarg */
        /*   negResult: accept-incomplete */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));               /* A0 ZZ */
        /* save location of length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 1);

        asn1ber_ber_from_bytes(&asn_encoder, asnENUMERATED, &neg_result, sizeof(neg_result));      /* 0A 01 01 */
        asn1ber_annotate_length(&asn_encoder, pos[2], 1);

        /*   supportedMech */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(1));               /* A1 ZZ */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 1);

        asn1ber_ber_from_oid(&asn_encoder, &oid_spnego_mech_ntlmssp);
        asn1ber_annotate_length(&asn_encoder, pos[2], 1);

        /*   ntlm service provider */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(2));               /* A2 ZZ */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, asnOCTET_STRING);               /* 04 zz */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[3]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        memcpy(asn_encoder.dst + asn_encoder.dst_head, ntlmssp_token, token_len);
        asn_encoder.dst_head += token_len;

        asn1ber_annotate_length(&asn_encoder, pos[3], 5);
        asn1ber_annotate_length(&asn_encoder, pos[2], 5);
        asn1ber_annotate_length(&asn_encoder, pos[1], 5);
        asn1ber_annotate_length(&asn_encoder, pos[0], 5);

        *neg_init_token = neg_init;
        return asn_encoder.dst_head;
}

int
smb2_spnego_wrap_ntlmssp_auth(struct smb2_context *smb2, const uint8_t *ntlmssp_token,
               const int token_len, void **neg_targ_token)
{
        struct asn1ber_context asn_encoder;
        uint8_t *neg_token;
        int alloc_len;
        int pos[6];

        alloc_len = 64 + 2 * token_len;
        neg_token = calloc(1, alloc_len);
        if (neg_token == NULL) {
                smb2_set_error(smb2, "Failed to allocate spnego wrapper");
                return 0;
        }

        memset(&asn_encoder, 0, sizeof(asn_encoder));
        asn_encoder.dst = neg_token;
        asn_encoder.dst_size = alloc_len;
        asn_encoder.dst_head = 0;

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(1));               /* A1 XX */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[0]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));              /* 30 YY */
        /* save location of sub length */
        asn1ber_save_out_state(&asn_encoder, &pos[1]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* negTokenTarg */
        /*   ntlm service provider */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(2));               /* A2 ZZ */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, asnOCTET_STRING);               /* 04 zz */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[3]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        memcpy(asn_encoder.dst + asn_encoder.dst_head, ntlmssp_token, token_len);
        asn_encoder.dst_head += token_len;

        asn1ber_annotate_length(&asn_encoder, pos[3], 5);
        asn1ber_annotate_length(&asn_encoder, pos[2], 5);
        asn1ber_annotate_length(&asn_encoder, pos[1], 5);
        asn1ber_annotate_length(&asn_encoder, pos[0], 5);

        *neg_targ_token = neg_token;
        return asn_encoder.dst_head;
}

int
smb2_spnego_wrap_authenticate_result(struct smb2_context *smb2, const int authorized_ok, void **blob)
{
        struct asn1ber_context asn_encoder;
        uint8_t *neg_targ;
        int alloc_len;
        int pos[6];
        uint8_t result_code = 3; /* accept-fail */

        alloc_len = 128;
        neg_targ = calloc(1, alloc_len);
        if (neg_targ == NULL) {
                smb2_set_error(smb2, "Failed to allocate spnego wrapper");
                return -ENOMEM;
        }

        if (authorized_ok) {
                result_code = 0 /* accept-completed */;
        }

        memset(&asn_encoder, 0, sizeof(asn_encoder));
        asn_encoder.dst = neg_targ;
        asn_encoder.dst_size = alloc_len;
        asn_encoder.dst_head = 0;

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(1));               /* A1 XX */
        /* save location of total length */
        asn1ber_save_out_state(&asn_encoder, &pos[0]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        asn1ber_ber_from_typecode(&asn_encoder, ASN1_SEQUENCE(0));              /* 30 YY */
        /* save location of sub length */
        asn1ber_save_out_state(&asn_encoder, &pos[1]);
        asn1ber_ber_reserve_length(&asn_encoder, 5);

        /* negTokenTarg */
        /*   negResult: accept-incomplete */
        asn1ber_ber_from_typecode(&asn_encoder, ASN1_CONTEXT(0));               /* A0 ZZ */
        /* save location of length */
        asn1ber_save_out_state(&asn_encoder, &pos[2]);
        asn1ber_ber_reserve_length(&asn_encoder, 1);

        asn1ber_ber_from_bytes(&asn_encoder, asnENUMERATED, &result_code, 1);   /* 0A 01 Value */
        asn1ber_annotate_length(&asn_encoder, pos[2], 1);

        asn1ber_annotate_length(&asn_encoder, pos[1], 5);
        asn1ber_annotate_length(&asn_encoder, pos[0], 5);

        *blob = neg_targ;
        return asn_encoder.dst_head;
}

#define require_typecode(ctx, expected, label)                          \
        ret = ber_typecode_from_ber(ctx, (ber_type_t*)&typecode);       \
        if (ret || (typecode != (expected))) {                          \
                goto label;                                             \
        }

#define require_typeandlen(ctx, expected, minimum, label)                       \
        ret = ber_typelen_from_ber(ctx, (ber_type_t*)&typecode, &typelen);      \
        if (ret || (typecode != (expected)) || (typelen < (minimum))) {         \
                fail_line = __LINE__;                                           \
                goto label;                                                     \
        }

#define require_noerr(errcode, label)                                   \
        if (errcode) {                                                  \
                fail_line = __LINE__;                                   \
                goto label;                                             \
        }

int
smb2_spnego_unwrap_targ(struct smb2_context *smb2, const uint8_t *spnego,
               const int spnego_len, uint8_t **token, uint32_t *mechanisms)
{
        struct asn1ber_context asn_decoder;
        struct asn1ber_oid_value oid;
        uint32_t typecode;
        uint32_t typelen;
        int objpos;
        int token_len = 0;
        int sequence_len = 0;
        int fail_line = 0;
        int ret;

        memset(&asn_decoder, 0, sizeof(asn_decoder));
        asn_decoder.src = discard_const(spnego);
        asn_decoder.src_count = spnego_len;
        asn_decoder.src_tail = 0;

        require_typeandlen(&asn_decoder, ASN1_CONTEXT(1), 14, fail);
        require_typeandlen(&asn_decoder, ASN1_SEQUENCE(0), 12, fail);
        sequence_len = typelen;
        while (sequence_len > 2) {
                objpos = asn_decoder.src_tail;
                ret = ber_typelen_from_ber(&asn_decoder, (ber_type_t*)&typecode, &typelen);
                require_noerr(ret, fail);
                switch (typecode) {
                case ASN1_CONTEXT(0):
                        /* num mechs, or neg-result */
                        ret = asn1ber_uint32_from_ber(&asn_decoder, mechanisms);
                        require_noerr(ret, fail);
                        break;
                case ASN1_CONTEXT(1):
                        /* a supported mechanism */
                        ret = asn1ber_oid_from_ber(&asn_decoder, &oid);
                        require_noerr(ret, fail);
                        break;
                case ASN1_CONTEXT(2):
                        /* response token */
                        require_typeandlen(&asn_decoder, asnOCTET_STRING, 8, fail);
                        *token  = asn_decoder.src + asn_decoder.src_tail;
                        token_len = typelen;
                        break;
                }
                sequence_len -= (asn_decoder.src_tail - objpos);
                if (token_len) {
                        break;
                }
        }
        return token_len;

fail:
        smb2_set_error(smb2, "bad spnego at line %d, spengo offset %d", fail_line, asn_decoder.src_tail);
        return -EINVAL;
}

int
smb2_spnego_unwrap_gssapi(struct smb2_context *smb2, const uint8_t *spnego,
                        const int spnego_len, const int suppress_errors,
                        uint8_t **token, uint32_t *mechanisms)
{
        struct asn1ber_context asn_decoder;
        struct asn1ber_oid_value oid;
        uint32_t typecode;
        uint32_t typelen;
        int decode_pos;
        int mech_bytes;
        uint32_t mechs = 0;
        int fail_line;
        int ret;

        memset(&asn_decoder, 0, sizeof(asn_decoder));
        asn_decoder.src = discard_const(spnego);
        asn_decoder.src_count = spnego_len;
        asn_decoder.src_tail = 0;

        require_typeandlen(&asn_decoder, asnCONSTRUCTOR | asnAPPLICATION, 32, fail);
        /* gss-spnego mech oid */
        ret = asn1ber_oid_from_ber(&asn_decoder, &oid);
        require_noerr(ret, fail);
        require_noerr(oid.length != oid_gss_mech_spnego.length, fail);
        require_noerr(oid_compare(&oid, &oid_gss_mech_spnego), fail);
        /* context 0 */
        require_typeandlen(&asn_decoder, ASN1_CONTEXT(0), 10, fail);
        require_typeandlen(&asn_decoder, ASN1_SEQUENCE(0), 8, fail);
        require_typeandlen(&asn_decoder, ASN1_CONTEXT(0), 10, fail);
        require_typeandlen(&asn_decoder, ASN1_SEQUENCE(0), 8, fail);
        mech_bytes = typelen;
        /* sequence of mechanism types */
        while (mech_bytes > 0) {
                decode_pos = asn_decoder.src_tail;
                ret = asn1ber_oid_from_ber(&asn_decoder, &oid);
                require_noerr(ret, fail);
                mech_bytes -= (asn_decoder.src_tail - decode_pos);
                if (!oid_compare(&oid, &oid_spnego_mech_krb5)) {
                        mechs |= SPNEGO_MECHANISM_KRB5;
                } else if (!oid_compare(&oid, &oid_spnego_mech_ms_krb5)) {
                        mechs |= SPNEGO_MECHANISM_KRB5;
                } else if (!oid_compare(&oid, &oid_spnego_mech_ntlmssp)) {
                        mechs |= SPNEGO_MECHANISM_NTLMSSP;
                }
        }
        if (mechanisms) {
                *mechanisms = mechs;
        }
        /* mech token, if present, follows */
        if (token) {
                *token = NULL;
                typelen = 0;
                if (asn_decoder.src_count > 2 &&
                                asn_decoder.src[asn_decoder.src_tail] == ASN1_CONTEXT(2)) {
                        /* mech token, note we expect NTLMSSP (7 bytes) at least here */
                        require_typeandlen(&asn_decoder, ASN1_CONTEXT(2), 10, fail);
                        require_typeandlen(&asn_decoder, asnOCTET_STRING, 7, fail);
                        *token  = asn_decoder.src + asn_decoder.src_tail;
                }
                return typelen;
        } else {
                return 0;
        }

fail:
        if (!suppress_errors) {
                smb2_set_error(smb2, "bad spnego at line %d, spengo offset %d", fail_line, asn_decoder.src_tail);
        }
        return -EINVAL;
}

int
smb2_spnego_unwrap_blob(struct smb2_context *smb2,
                const uint8_t *spnego,
                const int spnego_len,
                const int suppress_errors,
                uint8_t **token,  uint32_t *mechanisms)
{
        uint8_t typecode;

        if (token) {
                *token = NULL;
        }
        if (!spnego || !token || spnego_len < 7) {
                return -EINVAL;
        }
        /* if not wrapped, just return raw token */
        if (spnego_len > 7 && !memcmp(spnego, "NTLMSSP", 8)) {
                *token = discard_const(spnego);
                return spnego_len;
        }

        /* peek at first byte of spnego and dispatch */
        typecode = spnego[0];
        if (typecode == (asnCONSTRUCTOR | asnAPPLICATION)) {
                /* 0x60 - a GSS-API blob */
                return smb2_spnego_unwrap_gssapi(smb2,
                                spnego, spnego_len,
                                suppress_errors, token, mechanisms);
        }
        else if (typecode == ASN1_CONTEXT(0) ||
                        typecode == ASN1_CONTEXT(1) ||
                        typecode == ASN1_CONTEXT(2)) {
                /* 0xAx - a raw spnego blob */
                return smb2_spnego_unwrap_targ(smb2,
                                spnego, spnego_len, token, mechanisms);
        }
        return -EINVAL;
}

