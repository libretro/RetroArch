/*
 *  RIPE MD-160 implementation
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
 *  The RIPEMD-160 algorithm was designed by RIPE in 1996
 *  http://homes.esat.kuleuven.be/~bosselae/mbedtls_ripemd160.html
 *  http://ehash.iaik.tugraz.at/wiki/RIPEMD-160
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_RIPEMD160_C)

#include "mbedtls/ripemd160.h"

#include <string.h>

#include "mbedtls/int_util.h"

#include "arc4_alt.h"

void mbedtls_ripemd160_init( mbedtls_ripemd160_context *ctx )
{
    memset( ctx, 0, sizeof( mbedtls_ripemd160_context ) );
}

void mbedtls_ripemd160_free( mbedtls_ripemd160_context *ctx )
{
    if( ctx == NULL )
        return;

    mbedtls_zeroize( ctx, sizeof( mbedtls_ripemd160_context ) );
}

void mbedtls_ripemd160_clone( mbedtls_ripemd160_context *dst,
                        const mbedtls_ripemd160_context *src )
{
    *dst = *src;
}

/*
 * RIPEMD-160 context setup
 */
void mbedtls_ripemd160_starts( mbedtls_ripemd160_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
}

#if !defined(MBEDTLS_RIPEMD160_PROCESS_ALT)
/*
 * Process one block
 */
void mbedtls_ripemd160_process( mbedtls_ripemd160_context *ctx, const unsigned char data[64] )
{
    uint32_t A, B, C, D, E, Ap, Bp, Cp, Dp, Ep, X[16];

    MBEDTLS_GET_UINT32_LE( X[ 0], data,  0 );
    MBEDTLS_GET_UINT32_LE( X[ 1], data,  4 );
    MBEDTLS_GET_UINT32_LE( X[ 2], data,  8 );
    MBEDTLS_GET_UINT32_LE( X[ 3], data, 12 );
    MBEDTLS_GET_UINT32_LE( X[ 4], data, 16 );
    MBEDTLS_GET_UINT32_LE( X[ 5], data, 20 );
    MBEDTLS_GET_UINT32_LE( X[ 6], data, 24 );
    MBEDTLS_GET_UINT32_LE( X[ 7], data, 28 );
    MBEDTLS_GET_UINT32_LE( X[ 8], data, 32 );
    MBEDTLS_GET_UINT32_LE( X[ 9], data, 36 );
    MBEDTLS_GET_UINT32_LE( X[10], data, 40 );
    MBEDTLS_GET_UINT32_LE( X[11], data, 44 );
    MBEDTLS_GET_UINT32_LE( X[12], data, 48 );
    MBEDTLS_GET_UINT32_LE( X[13], data, 52 );
    MBEDTLS_GET_UINT32_LE( X[14], data, 56 );
    MBEDTLS_GET_UINT32_LE( X[15], data, 60 );

    A = Ap = ctx->state[0];
    B = Bp = ctx->state[1];
    C = Cp = ctx->state[2];
    D = Dp = ctx->state[3];
    E = Ep = ctx->state[4];
#define RIPEMD160_F1( x, y, z )   ( x ^ y ^ z )
#define RIPEMD160_F2( x, y, z )   ( ( x & y ) | ( ~x & z ) )
#define RIPEMD160_F3( x, y, z )   ( ( x | ~y ) ^ z )
#define RIPEMD160_F4( x, y, z )   ( ( x & z ) | ( y & ~z ) )
#define RIPEMD160_F5( x, y, z )   ( x ^ ( y | ~z ) )
#define RIPEMD160_S( x, n ) ( ( x << n ) | ( x >> (32 - n) ) )
#define RIPEMD160_P( a, b, c, d, e, r, s, f, k )                    \
    a += f( b, c, d ) + X[r] + k;                                   \
    a = RIPEMD160_S( a, s ) + e;                                    \
    c = RIPEMD160_S( c, 10 );

#define RIPEMD160_P2( a, b, c, d, e, r, s, rp, sp )                 \
    RIPEMD160_P( a, b, c, d, e, r, s, RIPEMD160_F, RIPEMD160_K );   \
    RIPEMD160_P( a ## p, b ## p, c ## p, d ## p, e ## p, rp, sp,    \
                 RIPEMD160_FP, RIPEMD160_KP );

#define RIPEMD160_F   RIPEMD160_F1
#define RIPEMD160_K   0x00000000
#define RIPEMD160_FP  RIPEMD160_F5
#define RIPEMD160_KP  0x50A28BE6
    RIPEMD160_P2( A, B, C, D, E,  0, 11,  5,  8 );
    RIPEMD160_P2( E, A, B, C, D,  1, 14, 14,  9 );
    RIPEMD160_P2( D, E, A, B, C,  2, 15,  7,  9 );
    RIPEMD160_P2( C, D, E, A, B,  3, 12,  0, 11 );
    RIPEMD160_P2( B, C, D, E, A,  4,  5,  9, 13 );
    RIPEMD160_P2( A, B, C, D, E,  5,  8,  2, 15 );
    RIPEMD160_P2( E, A, B, C, D,  6,  7, 11, 15 );
    RIPEMD160_P2( D, E, A, B, C,  7,  9,  4,  5 );
    RIPEMD160_P2( C, D, E, A, B,  8, 11, 13,  7 );
    RIPEMD160_P2( B, C, D, E, A,  9, 13,  6,  7 );
    RIPEMD160_P2( A, B, C, D, E, 10, 14, 15,  8 );
    RIPEMD160_P2( E, A, B, C, D, 11, 15,  8, 11 );
    RIPEMD160_P2( D, E, A, B, C, 12,  6,  1, 14 );
    RIPEMD160_P2( C, D, E, A, B, 13,  7, 10, 14 );
    RIPEMD160_P2( B, C, D, E, A, 14,  9,  3, 12 );
    RIPEMD160_P2( A, B, C, D, E, 15,  8, 12,  6 );
#undef RIPEMD160_F
#undef RIPEMD160_K
#undef RIPEMD160_FP
#undef RIPEMD160_KP

#define RIPEMD160_F   RIPEMD160_F2
#define RIPEMD160_K   0x5A827999
#define RIPEMD160_FP  RIPEMD160_F4
#define RIPEMD160_KP  0x5C4DD124
    RIPEMD160_P2( E, A, B, C, D,  7,  7,  6,  9 );
    RIPEMD160_P2( D, E, A, B, C,  4,  6, 11, 13 );
    RIPEMD160_P2( C, D, E, A, B, 13,  8,  3, 15 );
    RIPEMD160_P2( B, C, D, E, A,  1, 13,  7,  7 );
    RIPEMD160_P2( A, B, C, D, E, 10, 11,  0, 12 );
    RIPEMD160_P2( E, A, B, C, D,  6,  9, 13,  8 );
    RIPEMD160_P2( D, E, A, B, C, 15,  7,  5,  9 );
    RIPEMD160_P2( C, D, E, A, B,  3, 15, 10, 11 );
    RIPEMD160_P2( B, C, D, E, A, 12,  7, 14,  7 );
    RIPEMD160_P2( A, B, C, D, E,  0, 12, 15,  7 );
    RIPEMD160_P2( E, A, B, C, D,  9, 15,  8, 12 );
    RIPEMD160_P2( D, E, A, B, C,  5,  9, 12,  7 );
    RIPEMD160_P2( C, D, E, A, B,  2, 11,  4,  6 );
    RIPEMD160_P2( B, C, D, E, A, 14,  7,  9, 15 );
    RIPEMD160_P2( A, B, C, D, E, 11, 13,  1, 13 );
    RIPEMD160_P2( E, A, B, C, D,  8, 12,  2, 11 );
#undef RIPEMD160_F
#undef RIPEMD160_K
#undef RIPEMD160_FP
#undef RIPEMD160_KP

#define RIPEMD160_F   RIPEMD160_F3
#define RIPEMD160_K   0x6ED9EBA1
#define RIPEMD160_FP  RIPEMD160_F3
#define RIPEMD160_KP  0x6D703EF3
    RIPEMD160_P2( D, E, A, B, C,  3, 11, 15,  9 );
    RIPEMD160_P2( C, D, E, A, B, 10, 13,  5,  7 );
    RIPEMD160_P2( B, C, D, E, A, 14,  6,  1, 15 );
    RIPEMD160_P2( A, B, C, D, E,  4,  7,  3, 11 );
    RIPEMD160_P2( E, A, B, C, D,  9, 14,  7,  8 );
    RIPEMD160_P2( D, E, A, B, C, 15,  9, 14,  6 );
    RIPEMD160_P2( C, D, E, A, B,  8, 13,  6,  6 );
    RIPEMD160_P2( B, C, D, E, A,  1, 15,  9, 14 );
    RIPEMD160_P2( A, B, C, D, E,  2, 14, 11, 12 );
    RIPEMD160_P2( E, A, B, C, D,  7,  8,  8, 13 );
    RIPEMD160_P2( D, E, A, B, C,  0, 13, 12,  5 );
    RIPEMD160_P2( C, D, E, A, B,  6,  6,  2, 14 );
    RIPEMD160_P2( B, C, D, E, A, 13,  5, 10, 13 );
    RIPEMD160_P2( A, B, C, D, E, 11, 12,  0, 13 );
    RIPEMD160_P2( E, A, B, C, D,  5,  7,  4,  7 );
    RIPEMD160_P2( D, E, A, B, C, 12,  5, 13,  5 );
#undef RIPEMD160_F
#undef RIPEMD160_K
#undef RIPEMD160_FP
#undef RIPEMD160_KP

#define RIPEMD160_F   RIPEMD160_F4
#define RIPEMD160_K   0x8F1BBCDC
#define RIPEMD160_FP  RIPEMD160_F2
#define RIPEMD160_KP  0x7A6D76E9
    RIPEMD160_P2( C, D, E, A, B,  1, 11,  8, 15 );
    RIPEMD160_P2( B, C, D, E, A,  9, 12,  6,  5 );
    RIPEMD160_P2( A, B, C, D, E, 11, 14,  4,  8 );
    RIPEMD160_P2( E, A, B, C, D, 10, 15,  1, 11 );
    RIPEMD160_P2( D, E, A, B, C,  0, 14,  3, 14 );
    RIPEMD160_P2( C, D, E, A, B,  8, 15, 11, 14 );
    RIPEMD160_P2( B, C, D, E, A, 12,  9, 15,  6 );
    RIPEMD160_P2( A, B, C, D, E,  4,  8,  0, 14 );
    RIPEMD160_P2( E, A, B, C, D, 13,  9,  5,  6 );
    RIPEMD160_P2( D, E, A, B, C,  3, 14, 12,  9 );
    RIPEMD160_P2( C, D, E, A, B,  7,  5,  2, 12 );
    RIPEMD160_P2( B, C, D, E, A, 15,  6, 13,  9 );
    RIPEMD160_P2( A, B, C, D, E, 14,  8,  9, 12 );
    RIPEMD160_P2( E, A, B, C, D,  5,  6,  7,  5 );
    RIPEMD160_P2( D, E, A, B, C,  6,  5, 10, 15 );
    RIPEMD160_P2( C, D, E, A, B,  2, 12, 14,  8 );
#undef RIPEMD160_F
#undef RIPEMD160_K
#undef RIPEMD160_FP
#undef RIPEMD160_KP

#define RIPEMD160_F   RIPEMD160_F5
#define RIPEMD160_K   0xA953FD4E
#define RIPEMD160_FP  RIPEMD160_F1
#define RIPEMD160_KP  0x00000000
    RIPEMD160_P2( B, C, D, E, A,  4,  9, 12,  8 );
    RIPEMD160_P2( A, B, C, D, E,  0, 15, 15,  5 );
    RIPEMD160_P2( E, A, B, C, D,  5,  5, 10, 12 );
    RIPEMD160_P2( D, E, A, B, C,  9, 11,  4,  9 );
    RIPEMD160_P2( C, D, E, A, B,  7,  6,  1, 12 );
    RIPEMD160_P2( B, C, D, E, A, 12,  8,  5,  5 );
    RIPEMD160_P2( A, B, C, D, E,  2, 13,  8, 14 );
    RIPEMD160_P2( E, A, B, C, D, 10, 12,  7,  6 );
    RIPEMD160_P2( D, E, A, B, C, 14,  5,  6,  8 );
    RIPEMD160_P2( C, D, E, A, B,  1, 12,  2, 13 );
    RIPEMD160_P2( B, C, D, E, A,  3, 13, 13,  6 );
    RIPEMD160_P2( A, B, C, D, E,  8, 14, 14,  5 );
    RIPEMD160_P2( E, A, B, C, D, 11, 11,  0, 15 );
    RIPEMD160_P2( D, E, A, B, C,  6,  8,  3, 13 );
    RIPEMD160_P2( C, D, E, A, B, 15,  5,  9, 11 );
    RIPEMD160_P2( B, C, D, E, A, 13,  6, 11, 11 );
#undef RIPEMD160_F
#undef RIPEMD160_K
#undef RIPEMD160_FP
#undef RIPEMD160_KP

    C             = ctx->state[1] + C + Dp;
    ctx->state[1] = ctx->state[2] + D + Ep;
    ctx->state[2] = ctx->state[3] + E + Ap;
    ctx->state[3] = ctx->state[4] + A + Bp;
    ctx->state[4] = ctx->state[0] + B + Cp;
    ctx->state[0] = C;
}
#endif /* !MBEDTLS_RIPEMD160_PROCESS_ALT */

/*
 * RIPEMD-160 process buffer
 */
void mbedtls_ripemd160_update( mbedtls_ripemd160_context *ctx,
                       const unsigned char *input, size_t ilen )
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
        memcpy( (void *) (ctx->buffer + left), input, fill );
        mbedtls_ripemd160_process( ctx, ctx->buffer );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        mbedtls_ripemd160_process( ctx, input );
        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
    {
        memcpy( (void *) (ctx->buffer + left), input, ilen );
    }
}

static const unsigned char ripemd160_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * RIPEMD-160 final digest
 */
void mbedtls_ripemd160_finish( mbedtls_ripemd160_context *ctx, unsigned char output[20] )
{
    uint32_t last, padn;
    uint32_t high, low;
    unsigned char msglen[8];

    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    MBEDTLS_PUT_UINT32_LE( low,  msglen, 0 );
    MBEDTLS_PUT_UINT32_LE( high, msglen, 4 );

    last = ctx->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    mbedtls_ripemd160_update( ctx, ripemd160_padding, padn );
    mbedtls_ripemd160_update( ctx, msglen, 8 );

    MBEDTLS_PUT_UINT32_LE( ctx->state[0], output,  0 );
    MBEDTLS_PUT_UINT32_LE( ctx->state[1], output,  4 );
    MBEDTLS_PUT_UINT32_LE( ctx->state[2], output,  8 );
    MBEDTLS_PUT_UINT32_LE( ctx->state[3], output, 12 );
    MBEDTLS_PUT_UINT32_LE( ctx->state[4], output, 16 );
}

/*
 * output = RIPEMD-160( input buffer )
 */
void mbedtls_ripemd160( const unsigned char *input, size_t ilen,
                unsigned char output[20] )
{
    mbedtls_ripemd160_context ctx;

    mbedtls_ripemd160_init( &ctx );
    mbedtls_ripemd160_starts( &ctx );
    mbedtls_ripemd160_update( &ctx, input, ilen );
    mbedtls_ripemd160_finish( &ctx, output );
    mbedtls_ripemd160_free( &ctx );
}

#endif /* MBEDTLS_RIPEMD160_C */
