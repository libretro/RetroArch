/*
 *  RFC 1186/1320 compliant MD4 implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
/*
 *  The MD4 algorithm was designed by Ron Rivest in 1990.
 *
 *  http://www.ietf.org/rfc/rfc1186.txt
 *  http://www.ietf.org/rfc/rfc1320.txt
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_MD4_C)

#include "mbedtls/md4.h"

#include <string.h>

#if !defined(MBEDTLS_MD4_ALT)

#include "arc4_alt.h"

/*
 * 32-bit integer manipulation macros (little endian)
 */
#ifndef GET_UINT32_LE
#define GET_UINT32_LE(n,b,i)                            \
{                                                       \
    (n) = ( (uint32_t) (b)[(i)    ]       )             \
        | ( (uint32_t) (b)[(i) + 1] <<  8 )             \
        | ( (uint32_t) (b)[(i) + 2] << 16 )             \
        | ( (uint32_t) (b)[(i) + 3] << 24 );            \
}
#endif

#ifndef PUT_UINT32_LE
#define PUT_UINT32_LE(n,b,i)                                    \
{                                                               \
    (b)[(i)    ] = (unsigned char) ( ( (n)       ) & 0xFF );    \
    (b)[(i) + 1] = (unsigned char) ( ( (n) >>  8 ) & 0xFF );    \
    (b)[(i) + 2] = (unsigned char) ( ( (n) >> 16 ) & 0xFF );    \
    (b)[(i) + 3] = (unsigned char) ( ( (n) >> 24 ) & 0xFF );    \
}
#endif

void mbedtls_md4_init( mbedtls_md4_context *ctx )
{
    memset( ctx, 0, sizeof( mbedtls_md4_context ) );
}

void mbedtls_md4_free( mbedtls_md4_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_zeroize( ctx, sizeof( mbedtls_md4_context ) );
}

void mbedtls_md4_clone( mbedtls_md4_context *dst,
                        const mbedtls_md4_context *src )
{
    *dst = *src;
}

/*
 * MD4 context setup
 */
void mbedtls_md4_starts( mbedtls_md4_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

#if !defined(MBEDTLS_MD4_PROCESS_ALT)
void mbedtls_md4_process( mbedtls_md4_context *ctx, const unsigned char data[64] )
{
    uint32_t X[16], A, B, C, D;

    GET_UINT32_LE( X[ 0], data,  0 );
    GET_UINT32_LE( X[ 1], data,  4 );
    GET_UINT32_LE( X[ 2], data,  8 );
    GET_UINT32_LE( X[ 3], data, 12 );
    GET_UINT32_LE( X[ 4], data, 16 );
    GET_UINT32_LE( X[ 5], data, 20 );
    GET_UINT32_LE( X[ 6], data, 24 );
    GET_UINT32_LE( X[ 7], data, 28 );
    GET_UINT32_LE( X[ 8], data, 32 );
    GET_UINT32_LE( X[ 9], data, 36 );
    GET_UINT32_LE( X[10], data, 40 );
    GET_UINT32_LE( X[11], data, 44 );
    GET_UINT32_LE( X[12], data, 48 );
    GET_UINT32_LE( X[13], data, 52 );
    GET_UINT32_LE( X[14], data, 56 );
    GET_UINT32_LE( X[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x, y, z) ((x & y) | ((~x) & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x; a = S(a,s); }

    P( A, B, C, D, X[ 0],  3 );
    P( D, A, B, C, X[ 1],  7 );
    P( C, D, A, B, X[ 2], 11 );
    P( B, C, D, A, X[ 3], 19 );
    P( A, B, C, D, X[ 4],  3 );
    P( D, A, B, C, X[ 5],  7 );
    P( C, D, A, B, X[ 6], 11 );
    P( B, C, D, A, X[ 7], 19 );
    P( A, B, C, D, X[ 8],  3 );
    P( D, A, B, C, X[ 9],  7 );
    P( C, D, A, B, X[10], 11 );
    P( B, C, D, A, X[11], 19 );
    P( A, B, C, D, X[12],  3 );
    P( D, A, B, C, X[13],  7 );
    P( C, D, A, B, X[14], 11 );
    P( B, C, D, A, X[15], 19 );

#undef P
#undef F

#define F(x,y,z) ((x & y) | (x & z) | (y & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x5A827999; a = S(a,s); }

    P( A, B, C, D, X[ 0],  3 );
    P( D, A, B, C, X[ 4],  5 );
    P( C, D, A, B, X[ 8],  9 );
    P( B, C, D, A, X[12], 13 );
    P( A, B, C, D, X[ 1],  3 );
    P( D, A, B, C, X[ 5],  5 );
    P( C, D, A, B, X[ 9],  9 );
    P( B, C, D, A, X[13], 13 );
    P( A, B, C, D, X[ 2],  3 );
    P( D, A, B, C, X[ 6],  5 );
    P( C, D, A, B, X[10],  9 );
    P( B, C, D, A, X[14], 13 );
    P( A, B, C, D, X[ 3],  3 );
    P( D, A, B, C, X[ 7],  5 );
    P( C, D, A, B, X[11],  9 );
    P( B, C, D, A, X[15], 13 );

#undef P
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x6ED9EBA1; a = S(a,s); }

    P( A, B, C, D, X[ 0],  3 );
    P( D, A, B, C, X[ 8],  9 );
    P( C, D, A, B, X[ 4], 11 );
    P( B, C, D, A, X[12], 15 );
    P( A, B, C, D, X[ 2],  3 );
    P( D, A, B, C, X[10],  9 );
    P( C, D, A, B, X[ 6], 11 );
    P( B, C, D, A, X[14], 15 );
    P( A, B, C, D, X[ 1],  3 );
    P( D, A, B, C, X[ 9],  9 );
    P( C, D, A, B, X[ 5], 11 );
    P( B, C, D, A, X[13], 15 );
    P( A, B, C, D, X[ 3],  3 );
    P( D, A, B, C, X[11],  9 );
    P( C, D, A, B, X[ 7], 11 );
    P( B, C, D, A, X[15], 15 );

#undef F
#undef P

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}
#endif /* !MBEDTLS_MD4_PROCESS_ALT */

/*
 * MD4 process buffer
 */
void mbedtls_md4_update( mbedtls_md4_context *ctx, const unsigned char *input, size_t ilen )
{
    size_t fill;
    uint32_t left;

    if( ilen == 0 )
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (uint32_t) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (uint32_t) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, fill );
        mbedtls_md4_process( ctx, ctx->buffer );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        mbedtls_md4_process( ctx, input );
        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, ilen );
    }
}

static const unsigned char md4_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * MD4 final digest
 */
void mbedtls_md4_finish( mbedtls_md4_context *ctx, unsigned char output[16] )
{
    uint32_t last, padn;
    uint32_t high, low;
    unsigned char msglen[8];

    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_UINT32_LE( low,  msglen, 0 );
    PUT_UINT32_LE( high, msglen, 4 );

    last = ctx->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    mbedtls_md4_update( ctx, (unsigned char *) md4_padding, padn );
    mbedtls_md4_update( ctx, msglen, 8 );

    PUT_UINT32_LE( ctx->state[0], output,  0 );
    PUT_UINT32_LE( ctx->state[1], output,  4 );
    PUT_UINT32_LE( ctx->state[2], output,  8 );
    PUT_UINT32_LE( ctx->state[3], output, 12 );
}

#endif /* !MBEDTLS_MD4_ALT */

/*
 * output = MD4( input buffer )
 */
void mbedtls_md4( const unsigned char *input, size_t ilen, unsigned char output[16] )
{
    mbedtls_md4_context ctx;

    mbedtls_md4_init( &ctx );
    mbedtls_md4_starts( &ctx );
    mbedtls_md4_update( &ctx, input, ilen );
    mbedtls_md4_finish( &ctx, output );
    mbedtls_md4_free( &ctx );
}

#endif /* MBEDTLS_MD4_C */
