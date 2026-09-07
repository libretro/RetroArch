/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
#ifndef _ASN1_BER_H_
#define _ASN1_BER_H_

/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   and 2024 by Brian Dodge <bdodge09@gmail.com>
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
   along with this program; if not, see <http:/www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BER_MAX_OID_ELEMENTS     (32)
typedef uint32_t  beroid_type_t;
        
/* Notes: BER = ASN.1 Basic Encoding Rules defined in  ITU-TX.690
 *              https://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf
*/

/** ASN.1 Type Tags in BER per X.208
*/
#define asnUNIVERSAL        (0x0)
#define asnBOOLEAN          (0x1)
#define asnINTEGER          (0x2)
#define asnBIT_STRING       (0x3)
#define asnOCTET_STRING     (0x4)
#define asnNULL             (0x5)
#define asnOBJECT_ID        (0x6)
#define asnENUMERATED       (0xA)
#define asnSEQUENCE         (0x10)
#define asnSET              (0x11)
#define asnPRINTABLE_STR    (0x13)
#define asnUTC_TIME         (0x17)

/** ASN.1 Type Class in BER per X.690
*/
#define asnEXTENSION_ID     (0x1F)
#define asnCONSTRUCTOR      (0x20)
#define asnSTRUCT           (0x30)
#define asnAPPLICATION      (0x40)
#define asnCONTEXT_SPECIFIC (0x80)
#define asnPRIVATE          (0xC0)

typedef enum
{
        /** SNMP Basic Types per ASN.1 BER
        */
        BER_BOOLEAN        = (asnBOOLEAN),
        BER_INTEGER        = (asnINTEGER),
        BER_BIT_STRING     = (asnBIT_STRING),
        BER_OCTET_STRING   = (asnOCTET_STRING),
        BER_NULL           = (asnNULL),
        BER_OBJECT_ID      = (asnOBJECT_ID),
        BER_ENUMERATED     = (asnENUMERATED),
        BER_SEQUENCE       = (asnSEQUENCE),
        BER_SETOF          = (asnSET),
        BER_PRINTABLE_STR  = (asnPRINTABLE_STR),
        BER_UTC_TIME       = (asnUTC_TIME),
        
        /** BER Types as per RFC-1442, pre-formatted per BER
        */
        BER_IPADDRESS      = (asnAPPLICATION | 0x0),
        BER_COUNTER        = (asnAPPLICATION | 0x1),
        BER_GAUGE          = (asnAPPLICATION | 0x2),
        BER_UNSIGNED       = (asnAPPLICATION | 0x2),
        BER_TIMETICKS      = (asnAPPLICATION | 0x3),
        BER_OPAQUE         = (asnAPPLICATION | 0x4),
        BER_NSAPADDRESS    = (asnAPPLICATION | 0x5),
        BER_COUNTER64      = (asnAPPLICATION | 0x6),
        BER_UNSIGNED32     = (asnAPPLICATION | 0x7),
        BER_FLOAT          = (asnAPPLICATION | 0x8),
        BER_DOUBLE         = (asnAPPLICATION | 0x9),
        
        /** BER Types per RFC3781 = (Next gen SMI)
        */
        BER_INTEGER64      = (asnAPPLICATION | 0xA),
        BER_UNSIGNED64     = (asnAPPLICATION | 0xB),
        BER_FLOAT32        = (asnAPPLICATION | 0xC),
        BER_FLOAT64        = (asnAPPLICATION | 0xD),
        BER_FLOAT128       = (asnAPPLICATION | 0xE)
}
ber_type_t;

#define ASN1_SEQUENCE(n)        (asnSTRUCT | (n))
#define ASN1_CONTEXT(n)         (asnCONTEXT_SPECIFIC | asnCONSTRUCTOR | (n))
#define ASN1_CONTEXT_SIMPLE(n)  (asnCONTEXT_SPECIFIC | (n))
#define ASN1_PRIVATE            (asnPRIVATE)

struct asn1ber_context
{
        uint8_t *src;
        int src_count;
        int src_tail;
        
        uint8_t *dst;
        int dst_size;
        int dst_head;
        
        int last_error;
};

struct asn1ber_oid_value {
        int length;
        beroid_type_t elements[BER_MAX_OID_ELEMENTS];
};

int asn1ber_save_out_state(struct asn1ber_context *actx, int *out_pos);
int asn1ber_annotate_length(struct asn1ber_context *actx, int out_pos, int reserved);
int asn1ber_length_from_ber(struct asn1ber_context *actx, uint32_t *len);
int ber_typecode_from_ber(struct asn1ber_context *actx, ber_type_t *typecode);
int ber_typelen_from_ber(struct asn1ber_context *actx, ber_type_t *typecode, uint32_t *len);
int asn1ber_request_from_ber(struct asn1ber_context *actx, ber_type_t *opcode, uint32_t *len);
int asn1ber_struct_from_ber(struct asn1ber_context *actx, uint32_t *len);
int asn1ber_null_from_ber(struct asn1ber_context *actx, uint32_t *len);
int asn1ber_int32_from_ber(struct asn1ber_context *actx, int32_t *val);
int asn1ber_uint32_from_ber(struct asn1ber_context *actx, uint32_t *val);
int asn1ber_int64_from_ber(struct asn1ber_context *actx, int64_t *val);
int asn1ber_uint64_from_ber(struct asn1ber_context *actx, uint64_t *val);
int asn1ber_oid_from_ber(struct asn1ber_context *actx, struct asn1ber_oid_value *oid);
int asn1ber_bytes_from_ber(struct asn1ber_context *actx, uint8_t *val, uint32_t maxlen, uint32_t *lenout);
int asn1ber_string_from_ber(struct asn1ber_context *actx, char *val, uint32_t maxlen, uint32_t *lenout);
int asn1ber_ber_from_length(struct asn1ber_context *actx, uint32_t lenin, uint32_t *lenout);
int asn1ber_ber_reserve_length(struct asn1ber_context *actx, uint32_t len);
int asn1ber_ber_from_typecode(struct asn1ber_context *actx, const ber_type_t typecode);
int asn1ber_ber_from_typelen(struct asn1ber_context *actx, const ber_type_t typecode, const uint32_t lenin, uint32_t *lenout);
int asn1ber_ber_from_int32(struct asn1ber_context *actx, const ber_type_t type, const int32_t val);
int asn1ber_ber_from_uint32(struct asn1ber_context *actx, const ber_type_t type, const uint32_t val);
int asn1ber_ber_from_int64(struct asn1ber_context *actx, const ber_type_t type, const int64_t val);
int asn1ber_ber_from_uint64(struct asn1ber_context *actx, const ber_type_t type, const uint64_t val);
int asn1ber_ber_from_oid(struct asn1ber_context *actx, const struct asn1ber_oid_value *oid);
int asn1ber_ber_from_bytes(struct asn1ber_context *actx, const ber_type_t type, const uint8_t *val, uint32_t len);
int asn1ber_ber_from_string(struct asn1ber_context *actx, const char *val, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* ASN1_BER_H_ */

