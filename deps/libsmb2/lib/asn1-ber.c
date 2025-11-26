/* ber_type:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
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

#include <errno.h>
#include "compat.h"

#include "asn1-ber.h"

int asn1ber_next_byte(struct asn1ber_context *actx, uint8_t *outb)
{
    if (!actx || !actx->src || actx->src_tail >= actx->src_count) {
        return -1;
    }

    actx->src_tail++;
    *outb = actx->src[actx->src_tail - 1];
    return 0;
}

int asn1ber_out_byte(struct asn1ber_context *actx, uint8_t inb)
{
    if (!actx || !actx->dst || actx->dst_head >= actx->dst_size) {
        return -1;
    }

    actx->dst[actx->dst_head++] = inb;
    return 0;
}

int asn1ber_save_out_state(struct asn1ber_context *actx, int *out_pos)
{
    if (!out_pos || !actx || !actx->dst || actx->dst_head >= actx->dst_size) {
        return -1;
    }

    *out_pos = actx->dst_head;
    return 0;
}

int asn1ber_annotate_length(struct asn1ber_context *actx, int out_pos, int reserved)
{
    uint32_t bytes_made;
    uint32_t lenbytes;
    int result;

    if (!actx || !actx->dst || actx->dst_head >= actx->dst_size) {
        return -1;
    }

    /* bytes added since out_pos snap-shot */
    bytes_made = actx->dst_head - out_pos;
    bytes_made -= reserved;

    actx->dst_head = out_pos;

    /* encode length in bytes as ber at out pos */
    result = asn1ber_ber_from_length(actx, bytes_made, &lenbytes);
    if (result)
    {
        return result;
    }
    /* if reserved more than needed, move new contents left over unused reserved */
    if (reserved > (int)lenbytes)
    {
        memmove(actx->dst + actx->dst_head, actx->dst + out_pos + reserved, bytes_made);
    }
    /* restore head to new end */
    actx->dst_head+= bytes_made;
    return 0;
}

int asn1ber_length_from_ber(struct asn1ber_context *actx, uint32_t *len)
{
    int result;
    uint8_t b = 0;
    uint32_t val;

    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if (b & 0x80)
    {
        uint32_t vallen;

        val = 0;

        /* length is number of bytes of length, not actual length */
        vallen = b & 0x7F;
        if (vallen > 4)
        {
            actx->last_error = -E2BIG;
            return -1;
        }
        while (vallen > 0)
        {
            result = asn1ber_next_byte(actx, &b);
            if (result)
            {
                return result;
            }
            val <<= 8;
            val |= (uint32_t)b;
            vallen--;
        }
    }
    else
    {
        /* simple small len */
        val = (uint32_t)b;
    }
    *len = val;
    return 0;
}

int ber_typecode_from_ber(struct asn1ber_context *actx, ber_type_t *typecode)
{
    int result;
    uint8_t b = 0;

    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if ((b & 0x1F) != 0x1F) /* not [IMPLICIT] */
    {
        *typecode = b;
        return 0;
    }
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if (b >= 0x30)
    {
        b -= 0x30;
    }
    *typecode = (ber_type_t)b;
    return 0;
}

int ber_typelen_from_ber(struct asn1ber_context *actx, ber_type_t *typecode, uint32_t *len)
{
    int result;

    result = ber_typecode_from_ber(actx, typecode);
    if (result)
    {
        return result;
    }
    return asn1ber_length_from_ber(actx, len);
}

int asn1ber_request_from_ber(struct asn1ber_context *actx, ber_type_t *opcode, uint32_t *len)
{
    int result;

    result = ber_typelen_from_ber(actx, opcode, len);
    if (result)
    {
        return result;
    }

    return 0;
}

int asn1ber_struct_from_ber(struct asn1ber_context *actx, uint32_t *len)
{
    int result;
    ber_type_t typecode;

    result = ber_typelen_from_ber(actx, &typecode, len);
    if (result)
    {
        return result;
    }
    if (typecode != asnSTRUCT)
    {
        actx->last_error = -EINVAL;
        return -1;
    }
    return 0;
}

int asn1ber_null_from_ber(struct asn1ber_context *actx, uint32_t *len)
{
    int result;
    ber_type_t typecode;

    result = ber_typelen_from_ber(actx, &typecode, len);
    if (result)
    {
        return result;
    }
    if (typecode != asnNULL)
    {
        actx->last_error = -EINVAL;
        return -1;
    }
    return 0;
}

int asn1ber_int32_from_ber(struct asn1ber_context *actx, int32_t *val)
{
    int result;
    int ival;
    uint8_t vallen;
    uint8_t b = 0;

    *val = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    switch ((ber_type_t)b)
    {
    case BER_INTEGER:
    case BER_COUNTER:
        break;
    default:
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    vallen = b;
    if (vallen > 4)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    /* VALUE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if (b & 0x80)
    {
        ival = -1;
    }
    else
    {
        ival = 0;
    }
    ival <<= 8;
    ival |= (int32_t)(uint32_t)b;
    vallen--;

    while (vallen > 0)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        ival <<= 8;
        ival |= (int32_t)(uint32_t)b;
        vallen--;
    }
    *val = ival;
    return 0;
}

int asn1ber_uint32_from_ber(struct asn1ber_context *actx, uint32_t *val)
{
    int result;
    uint32_t uval;
    uint8_t vallen;
    uint8_t b = 0;

    *val = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    switch ((ber_type_t)b)
    {
    case BER_BOOLEAN:
    case BER_IPADDRESS:
    case BER_COUNTER:
    case BER_UNSIGNED:
    case BER_TIMETICKS:
    case BER_NSAPADDRESS:
    case BER_UNSIGNED32:
    case BER_ENUMERATED:
        break;
    default:
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    vallen = b;
    if (vallen > 4)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    /* VALUE */
    uval = 0;
    while (vallen > 0)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        uval <<= 8;
        uval |= (uint32_t)b;
        vallen--;
    }
    *val = uval;
    return 0;
}

int asn1ber_int64_from_ber(struct asn1ber_context *actx, int64_t *val)
{
    int result;
    int64_t ival;
    uint8_t vallen;
    uint8_t b = 0;

    *val = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    switch ((ber_type_t)b)
    {
    case BER_INTEGER64:
        break;
    default:
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    vallen = b;
    if (vallen > 8)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    /* VALUE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if (b & 0x80)
    {
        ival = -1;
    }
    else
    {
        ival = 0;
    }
    ival <<= 8;
    ival |= (int32_t)(uint32_t)b;
    vallen--;

    while (vallen > 0)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        ival <<= 8;
        ival |= (int64_t)(uint32_t)b;
        vallen--;
    }
    *val = ival;
    return 0;
}

int asn1ber_uint64_from_ber(struct asn1ber_context *actx, uint64_t *val)
{
    int result;
    uint64_t uval;
    uint8_t vallen;
    uint8_t b = 0;

    *val = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    switch ((ber_type_t)b)
    {
    case BER_UNSIGNED64:
    case BER_COUNTER64:
        break;
    default:
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    vallen = b;
    if (vallen > 8)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        *val = 0;
        actx->last_error = -E2BIG;
        return -1;
    }
    /* VALUE */
    uval = 0;
    while (vallen > 0)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        uval <<= 8;
        uval |= (uint64_t)b;
        vallen--;
    }
    *val = uval;
    return 0;
}

int asn1ber_oid_from_ber(struct asn1ber_context *actx, struct asn1ber_oid_value *oid)
{
    int result;
    int i;
    beroid_type_t oval;
    uint32_t vallen;
    uint8_t b = 0;

    oid->length = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if ((ber_type_t)b != BER_OBJECT_ID)
    {
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_length_from_ber(actx, &vallen);
    if (result)
    {
        return result;
    }
    if (vallen > BER_MAX_OID_ELEMENTS)
    {
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        actx->last_error = -E2BIG;
        return -1;
    }
    /* VALUE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    /*
    ** first byte is specially encoded
    */
    oval = (beroid_type_t)b;
    ((beroid_type_t*)(oid->elements))[0] = oval / 40;
    ((beroid_type_t*)(oid->elements))[1] = oval - (((beroid_type_t *)(oid->elements))[0] * 40);
    vallen--;

    /*
    ** next bytes are oid types ber encoded
    */
    i = 2;
    while (vallen > 0 && i < BER_MAX_OID_ELEMENTS)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        oval = (beroid_type_t)(b & 0x7F);
        vallen--;

        while ((b & 0x80) && (vallen > 0))
        {
            result = asn1ber_next_byte(actx, &b);
            if (result)
            {
                return result;
            }
            oval <<= 7;
            oval |= (beroid_type_t)(b & 0x7F);
            vallen--;
        }
        ((beroid_type_t*)(oid->elements))[i++] = oval;
    }
    if (vallen != 0)
    {
        actx->last_error = -E2BIG;
        return -1;
    }
    oid->length = i;
    return 0;
}

int asn1ber_bytes_from_ber(struct asn1ber_context *actx, uint8_t *val, uint32_t maxlen, uint32_t *lenout)
{
    int result;
    size_t i;
    uint32_t vallen;
    uint8_t b = 0;

    /* TYPE */
    result = asn1ber_next_byte(actx, &b);
    if (result)
    {
        return result;
    }
    if ((ber_type_t)b != asnOCTET_STRING)
    {
        actx->last_error = -EINVAL;
        return -1;
    }
    /* LENGTH */
    result = asn1ber_length_from_ber(actx, &vallen);
    if (result)
    {
        return result;
    }
    if (vallen > maxlen)
    {
        actx->last_error = -E2BIG;
        return -1;
    }
    if (vallen == 0)
    {
        *val = 0;
        *lenout = 0;
        return 0;
    }
    /* VALUE */
    for (i = 0; vallen > 0; vallen--)
    {
        result = asn1ber_next_byte(actx, &b);
        if (result)
        {
            return result;
        }
        val[i++] = b;
    }
    /* convenience 0 terminate */
    if (i < maxlen)
    {
        val[i] = 0;
    }
    *lenout = i;
    return 0;
}

int asn1ber_string_from_ber(struct asn1ber_context *actx, char *val, uint32_t maxlen, uint32_t *lenout)
{
    return asn1ber_bytes_from_ber(actx, (uint8_t *)val, maxlen, lenout);
}

int asn1ber_ber_from_length(struct asn1ber_context *actx, uint32_t lenin, uint32_t *lenout)
{
    int result;
    int outlen = 0;

    if (lenin < 128)
    {
        result = asn1ber_out_byte(actx, (const uint8_t)lenin);
        if (result)
        {
            return result;
        }
        outlen++;
    }
    else
    {
        uint32_t lenbytesneeded;
        uint32_t lenbytes;

        for (lenbytesneeded = 0, lenbytes = lenin; lenbytes; lenbytesneeded++)
        {
            lenbytes >>= 8;
        }
        result = asn1ber_out_byte(actx, (const uint8_t)(0x80 | lenbytesneeded));
        if (result)
        {
            return result;
        }
        outlen++;

        while (lenbytesneeded > 0)
        {
            result = asn1ber_out_byte(actx, (const uint8_t)(lenin >> (8 * (lenbytesneeded - 1))));
            if (result)
            {
                return result;
            }
            lenbytesneeded--;
            outlen++;
        }
    }
    *lenout = outlen;
    return 0;
}

int asn1ber_ber_reserve_length(struct asn1ber_context *actx, uint32_t len)
{
    int result;

    while (len-- > 0)
    {
        result = asn1ber_out_byte(actx, 0);
        if (result)
        {
            return result;
        }
    }
    return 0;
}

int asn1ber_ber_from_typecode(struct asn1ber_context *actx, const ber_type_t typecode)
{
    return asn1ber_out_byte(actx, (const uint8_t)typecode);
}

int asn1ber_ber_from_typelen(struct asn1ber_context *actx, const ber_type_t typecode, const uint32_t lenin, uint32_t *lenout)
{
    int result;
    uint32_t outlen;

    result = asn1ber_out_byte(actx, (const uint8_t)typecode);
    if (result)
    {
        return result;
    }
    result = asn1ber_ber_from_length(actx, lenin, &outlen);
    if (result)
    {
        return result;
    }
    *lenout = outlen + 1;
    return 0;
}

int asn1ber_ber_from_int32(struct asn1ber_context *actx, const ber_type_t type, const int32_t val)
{
    uint32_t bytesneeded;
    uint32_t bytes;
    int result;

    if (val == 0)
    {
        bytesneeded = 1;
    }
    else if (val < 0)
    {
        for (bytes = (uint32_t)val, bytesneeded = 4; bytes; bytesneeded--)
        {
            if ((bytes & 0xFF800000) != 0xFF800000)
            {
                break;
            }
        }
    }
    else
    {
        for (bytes = (uint32_t)val, bytesneeded = 4; bytes; bytesneeded--)
        {
            if ((bytes & 0xFF000000) != 0)
            {
                break;
            }
            bytes <<= 8;
        }
    }
    result = asn1ber_ber_from_typelen(actx, type, bytesneeded, &bytes);
    if (result)
    {
        return result;
    }
    while (bytesneeded > 0)
    {
        result = asn1ber_out_byte(actx, (const uint8_t)(val >> (8 * (bytesneeded - 1))));
        if (result)
        {
            return result;
        }
        bytesneeded--;
    }
    return result;
}

int asn1ber_ber_from_uint32(struct asn1ber_context *actx, const ber_type_t type, const uint32_t val)
{
    uint32_t bytesneeded;
    uint32_t bytes;
    int result;

    for (bytes = val, bytesneeded = 4; bytes; bytesneeded--)
    {
        if ((bytes & 0xFF000000) != 0)
        {
            break;
        }
        bytes <<= 8;
    }
    result = asn1ber_ber_from_typelen(actx, type, bytesneeded, &bytes);
    if (result)
    {
        return result;
    }
    while (bytesneeded > 0)
    {
        result = asn1ber_out_byte(actx, (const uint8_t)(val >> (8 * (bytesneeded - 1))));
        if (result)
        {
            return result;
        }
        bytesneeded--;
    }
    return result;
}

int asn1ber_ber_from_int64(struct asn1ber_context *actx, const ber_type_t type, const int64_t val)
{
    uint32_t bytesneeded;
    uint32_t bytesout;
    uint64_t bytes;
    int result;

    if (val == 0)
    {
        bytesneeded = 1;
    }
    else if (val < 0)
    {
        for (bytes = (uint64_t)val, bytesneeded = 8; bytes; bytesneeded--)
        {
            if ((bytes & 0xFF80000000000000) != 0xFF80000000000000)
            {
                break;
            }
        }
    }
    else
    {
        for (bytes = (uint64_t)val, bytesneeded = 8; bytes; bytesneeded--)
        {
            if ((bytes & 0xFF00000000000000) != 0)
            {
                break;
            }
            bytes <<= 8;
        }
    }
    result = asn1ber_ber_from_typelen(actx, type, bytesneeded, &bytesout);
    if (result)
    {
        return result;
    }
    while (bytesneeded > 0)
    {
        result = asn1ber_out_byte(actx, (const uint8_t)(val >> (8 * (bytesneeded - 1))));
        if (result)
        {
            return result;
        }
        bytesneeded--;
    }
    return result;
}

int asn1ber_ber_from_uint64(struct asn1ber_context *actx, const ber_type_t type, const uint64_t val)
{
    uint32_t bytesneeded;
    uint32_t bytesout;
    uint64_t bytes;
    int result;

    for (bytes = val, bytesneeded = 4; bytes; bytesneeded--)
    {
        if ((bytes & 0xFF00000000000000) != 0)
        {
            break;
        }
        bytes <<= 8;
    }
    result = asn1ber_ber_from_typelen(actx, type, bytesneeded, &bytesout);
    if (result)
    {
        return result;
    }
    while (bytesneeded > 0)
    {
        result = asn1ber_out_byte(actx, (const uint8_t)(val >> (8 * (bytesneeded - 1))));
        if (result)
        {
            return result;
        }
        bytesneeded--;
    }
    return result;
}

static int asn1ber_ber_from_single_oid(struct asn1ber_context *actx, beroid_type_t oidb)
{
    int result;
    uint8_t a, b, c, d, e;

    a = (unsigned char)(oidb & 0x7F);
    oidb >>= 7;
    b = (unsigned char)(oidb & 0x7F);
    oidb >>= 7;
    c = (unsigned char)(oidb & 0x7F);
    oidb >>= 7;
    d = (unsigned char)(oidb & 0x7F);
    oidb >>= 7;
    e = (unsigned char)(oidb & 0x7F);

    if (e)
    {
        result = asn1ber_out_byte(actx, (e | 0x80));
        if (result)
        {
            return result;
        }
    }
    if (d || e)
    {
        result = asn1ber_out_byte(actx, (d | 0x80));
        if (result)
        {
            return result;
        }
    }
    if (c || d || e)
    {
        result = asn1ber_out_byte(actx, (c | 0x80));
        if (result)
        {
            return result;
        }
    }
    if (b || c || d || e)
    {
        result = asn1ber_out_byte(actx, (b | 0x80));
        if (result)
        {
            return result;
        }
    }
    result = asn1ber_out_byte(actx, a);
    return result;
}

int asn1ber_ber_from_oid(struct asn1ber_context *actx, const struct asn1ber_oid_value *oid)
{
    int lenpos;
    int reserve;
    int oiddex;
    int result;

    if (! oid)
    {
        actx->last_error = -EINVAL;
        return 1;
    }
    if (oid->length >= BER_MAX_OID_ELEMENTS)
    {
        actx->last_error = -E2BIG;
        return 1;
    }
    result = asn1ber_ber_from_typecode(actx, BER_OBJECT_ID);
    if (result)
    {
        return result;
    }
    if (BER_MAX_OID_ELEMENTS > 127)
    {
        reserve = 5;
    }
    else
    {
        reserve = 1;
    }
    /* save position for back annotating */
    asn1ber_save_out_state(actx, &lenpos);

    /* put in expected length bytes */
    result = asn1ber_ber_reserve_length(actx, reserve);
    if (result)
    {
        return result;
    }
    if (oid->length > 1)
    {
        if (((beroid_type_t*)(oid->elements))[0] < 40)
        {
            result = asn1ber_ber_from_single_oid(actx, ((beroid_type_t *)(oid->elements))[0] * 40 + ((beroid_type_t *)(oid->elements))[1]);
            if (result)
            {
                return result;
            }
            oiddex = 2;
        }
        else
        {
            oiddex = 0;
        }
    }
    else
    {
        oiddex = 0;
    }
    while (oiddex < oid->length)
    {
        result = asn1ber_ber_from_single_oid(actx, ((beroid_type_t *)(oid->elements))[oiddex]);
        if (result)
        {
            return result;
        }
        oiddex++;
    }
    /* back annotate length byte(s) */
    result = asn1ber_annotate_length(actx, lenpos, reserve);
    return result;
}

int asn1ber_ber_from_bytes(struct asn1ber_context *actx, const ber_type_t type, const uint8_t *val, uint32_t len)
{
    uint32_t outlen;
    uint32_t outdex;
    int result;

    result = asn1ber_ber_from_typelen(actx, type, len, &outlen);
    if (result)
    {
        return result;
    }
    for (outdex = 0; outdex <  len; outdex++)
    {
        result = asn1ber_out_byte(actx, val[outdex]);
        if (result)
        {
            return result;
        }
    }
    return result;
}

int asn1ber_ber_from_string(struct asn1ber_context *actx, const char *val, uint32_t len)
{
    return asn1ber_ber_from_bytes(actx, BER_OCTET_STRING, (uint8_t*)val, len);
}

