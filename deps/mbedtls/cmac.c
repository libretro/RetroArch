/**
 * \file cmac.c
 *
 * \brief NIST SP800-38B compliant CMAC implementation for AES and 3DES
 *
 *  Copyright (C) 2006-2016, ARM Limited, All Rights Reserved
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
 * References:
 *
 * - NIST SP 800-38B Recommendation for Block Cipher Modes of Operation: The
 *      CMAC Mode for Authentication
 *   http://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38b.pdf
 *
 * - RFC 4493 - The AES-CMAC Algorithm
 *   https://tools.ietf.org/html/rfc4493
 *
 * - RFC 4615 - The Advanced Encryption Standard-Cipher-based Message
 *      Authentication Code-Pseudo-Random Function-128 (AES-CMAC-PRF-128)
 *      Algorithm for the Internet Key Exchange Protocol (IKE)
 *   https://tools.ietf.org/html/rfc4615
 *
 *   Additional test vectors: ISO/IEC 9797-1
 *
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_CMAC_C)

#include "mbedtls/cmac.h"

#include <string.h>


#include <stdlib.h>

#include "arc4_alt.h"

/*
 * Multiplication by u in the Galois field of GF(2^n)
 *
 * As explained in NIST SP 800-38B, this can be computed:
 *
 *   If MSB(p) = 0, then p = (p << 1)
 *   If MSB(p) = 1, then p = (p << 1) ^ R_n
 *   with R_64 = 0x1B and  R_128 = 0x87
 *
 * Input and output MUST NOT point to the same buffer
 * Block size must be 8 bytes or 16 bytes - the block sizes for DES and AES.
 */
static int cmac_multiply_by_u( unsigned char *output,
                               const unsigned char *input,
                               size_t blocksize )
{
    const unsigned char R_128 = 0x87;
    const unsigned char R_64 = 0x1B;
    unsigned char R_n, mask;
    unsigned char overflow = 0x00;
    int i;

    if( blocksize == MBEDTLS_AES_BLOCK_SIZE )
    {
        R_n = R_128;
    }
    else if( blocksize == MBEDTLS_DES3_BLOCK_SIZE )
    {
        R_n = R_64;
    }
    else
    {
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
    }

    for( i = (int)blocksize - 1; i >= 0; i-- )
    {
        output[i] = input[i] << 1 | overflow;
        overflow = input[i] >> 7;
    }

    /* mask = ( input[0] >> 7 ) ? 0xff : 0x00
     * using bit operations to avoid branches */

    /* MSVC has a warning about unary minus on unsigned, but this is
     * well-defined and precisely what we want to do here */
#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4146 )
#endif
    mask = - ( input[0] >> 7 );
#if defined(_MSC_VER)
#pragma warning( pop )
#endif

    output[ blocksize - 1 ] ^= R_n & mask;

    return( 0 );
}

/*
 * Generate subkeys
 *
 * - as specified by RFC 4493, section 2.3 Subkey Generation Algorithm
 */
static int cmac_generate_subkeys( mbedtls_cipher_context_t *ctx,
                                  unsigned char* K1, unsigned char* K2 )
{
    int ret;
    unsigned char L[MBEDTLS_CIPHER_BLKSIZE_MAX];
    size_t olen, block_size;

    mbedtls_zeroize( L, sizeof( L ) );

    block_size = ctx->cipher_info->block_size;

    /* Calculate Ek(0) */
    if( ( ret = mbedtls_cipher_update( ctx, L, block_size, L, &olen ) ) != 0 )
        goto exit;

    /*
     * Generate K1 and K2
     */
    if( ( ret = cmac_multiply_by_u( K1, L , block_size ) ) != 0 )
        goto exit;

    if( ( ret = cmac_multiply_by_u( K2, K1 , block_size ) ) != 0 )
        goto exit;

exit:
    mbedtls_zeroize( L, sizeof( L ) );

    return( ret );
}

static void cmac_xor_block( unsigned char *output, const unsigned char *input1,
                            const unsigned char *input2,
                            const size_t block_size )
{
    size_t index;

    for( index = 0; index < block_size; index++ )
        output[ index ] = input1[ index ] ^ input2[ index ];
}

/*
 * Create padded last block from (partial) last block.
 *
 * We can't use the padding option from the cipher layer, as it only works for
 * CBC and we use ECB mode, and anyway we need to XOR K1 or K2 in addition.
 */
static void cmac_pad( unsigned char padded_block[MBEDTLS_CIPHER_BLKSIZE_MAX],
                      size_t padded_block_len,
                      const unsigned char *last_block,
                      size_t last_block_len )
{
    size_t j;

    for( j = 0; j < padded_block_len; j++ )
    {
        if( j < last_block_len )
            padded_block[j] = last_block[j];
        else if( j == last_block_len )
            padded_block[j] = 0x80;
        else
            padded_block[j] = 0x00;
    }
}

int mbedtls_cipher_cmac_starts( mbedtls_cipher_context_t *ctx,
                                const unsigned char *key, size_t keybits )
{
    mbedtls_cipher_type_t type;
    mbedtls_cmac_context_t *cmac_ctx;
    int retval;

    if( ctx == NULL || ctx->cipher_info == NULL || key == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    if( ( retval = mbedtls_cipher_setkey( ctx, key, (int)keybits,
                                          MBEDTLS_ENCRYPT ) ) != 0 )
        return( retval );

    type = ctx->cipher_info->type;

    switch( type )
    {
        case MBEDTLS_CIPHER_AES_128_ECB:
        case MBEDTLS_CIPHER_AES_192_ECB:
        case MBEDTLS_CIPHER_AES_256_ECB:
        case MBEDTLS_CIPHER_DES_EDE3_ECB:
            break;
        default:
            return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );
    }

    /* Allocated and initialise in the cipher context memory for the CMAC
     * context */
    cmac_ctx = calloc( 1, sizeof( mbedtls_cmac_context_t ) );
    if( cmac_ctx == NULL )
        return( MBEDTLS_ERR_CIPHER_ALLOC_FAILED );

    ctx->cmac_ctx = cmac_ctx;

    mbedtls_zeroize( cmac_ctx->state, sizeof( cmac_ctx->state ) );

    return 0;
}

int mbedtls_cipher_cmac_update( mbedtls_cipher_context_t *ctx,
                                const unsigned char *input, size_t ilen )
{
    mbedtls_cmac_context_t* cmac_ctx;
    unsigned char *state;
    int ret = 0;
    size_t n, j, olen, block_size;

    if( ctx == NULL || ctx->cipher_info == NULL || input == NULL ||
        ctx->cmac_ctx == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    block_size = ctx->cipher_info->block_size;
    state = ctx->cmac_ctx->state;

    /* Is there data still to process from the last call, that's greater in
     * size than a block? */
    if( cmac_ctx->unprocessed_len > 0 &&
        ilen > block_size - cmac_ctx->unprocessed_len )
    {
        memcpy( &cmac_ctx->unprocessed_block[cmac_ctx->unprocessed_len],
                input,
                block_size - cmac_ctx->unprocessed_len );

        cmac_xor_block( state, cmac_ctx->unprocessed_block, state, block_size );

        if( ( ret = mbedtls_cipher_update( ctx, state, block_size, state,
                                           &olen ) ) != 0 )
        {
           goto exit;
        }

        input += block_size - cmac_ctx->unprocessed_len;
        ilen -= block_size - cmac_ctx->unprocessed_len;
        cmac_ctx->unprocessed_len = 0;
    }

    /* n is the number of blocks including any final partial block */
    n = ( ilen + block_size - 1 ) / block_size;

    /* Iterate across the input data in block sized chunks, excluding any
     * final partial or complete block */
    for( j = 1; j < n; j++ )
    {
        cmac_xor_block( state, input, state, block_size );

        if( ( ret = mbedtls_cipher_update( ctx, state, block_size, state,
                                           &olen ) ) != 0 )
           goto exit;

        ilen -= block_size;
        input += block_size;
    }

    /* If there is data left over that wasn't aligned to a block */
    if( ilen > 0 )
    {
        memcpy( &cmac_ctx->unprocessed_block[cmac_ctx->unprocessed_len],
                input,
                ilen );
        cmac_ctx->unprocessed_len += ilen;
    }

exit:
    return( ret );
}

int mbedtls_cipher_cmac_finish( mbedtls_cipher_context_t *ctx,
                                unsigned char *output )
{
    mbedtls_cmac_context_t* cmac_ctx;
    unsigned char *state, *last_block;
    unsigned char K1[MBEDTLS_CIPHER_BLKSIZE_MAX];
    unsigned char K2[MBEDTLS_CIPHER_BLKSIZE_MAX];
    unsigned char M_last[MBEDTLS_CIPHER_BLKSIZE_MAX];
    int ret;
    size_t olen, block_size;

    if( ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL ||
        output == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;
    block_size = ctx->cipher_info->block_size;
    state = cmac_ctx->state;

    mbedtls_zeroize( K1, sizeof( K1 ) );
    mbedtls_zeroize( K2, sizeof( K2 ) );
    cmac_generate_subkeys( ctx, K1, K2 );

    last_block = cmac_ctx->unprocessed_block;

    /* Calculate last block */
    if( cmac_ctx->unprocessed_len < block_size )
    {
        cmac_pad( M_last, block_size, last_block, cmac_ctx->unprocessed_len );
        cmac_xor_block( M_last, M_last, K2, block_size );
    }
    else
    {
        /* Last block is complete block */
        cmac_xor_block( M_last, last_block, K1, block_size );
    }


    cmac_xor_block( state, M_last, state, block_size );
    if( ( ret = mbedtls_cipher_update( ctx, state, block_size, state,
                                       &olen ) ) != 0 )
    {
        goto exit;
    }

    memcpy( output, state, block_size );

exit:
    /* Wipe the generated keys on the stack, and any other transients to avoid
     * side channel leakage */
    mbedtls_zeroize( K1, sizeof( K1 ) );
    mbedtls_zeroize( K2, sizeof( K2 ) );

    cmac_ctx->unprocessed_len = 0;
    mbedtls_zeroize( cmac_ctx->unprocessed_block,
                     sizeof( cmac_ctx->unprocessed_block ) );

    mbedtls_zeroize( state, MBEDTLS_CIPHER_BLKSIZE_MAX );
    return( ret );
}

int mbedtls_cipher_cmac_reset( mbedtls_cipher_context_t *ctx )
{
    mbedtls_cmac_context_t* cmac_ctx;

    if( ctx == NULL || ctx->cipher_info == NULL || ctx->cmac_ctx == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cmac_ctx = ctx->cmac_ctx;

    /* Reset the internal state */
    cmac_ctx->unprocessed_len = 0;
    mbedtls_zeroize( cmac_ctx->unprocessed_block,
                     sizeof( cmac_ctx->unprocessed_block ) );
    mbedtls_zeroize( cmac_ctx->state,
                     sizeof( cmac_ctx->state ) );

    return( 0 );
}

int mbedtls_cipher_cmac( const mbedtls_cipher_info_t *cipher_info,
                         const unsigned char *key, size_t keylen,
                         const unsigned char *input, size_t ilen,
                         unsigned char *output )
{
    mbedtls_cipher_context_t ctx;
    int ret;

    if( cipher_info == NULL || key == NULL || input == NULL || output == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    mbedtls_cipher_init( &ctx );

    if( ( ret = mbedtls_cipher_setup( &ctx, cipher_info ) ) != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_starts( &ctx, key, keylen );
    if( ret != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_update( &ctx, input, ilen );
    if( ret != 0 )
        goto exit;

    ret = mbedtls_cipher_cmac_finish( &ctx, output );

exit:
    mbedtls_cipher_free( &ctx );

    return( ret );
}

#if defined(MBEDTLS_AES_C)
/*
 * Implementation of AES-CMAC-PRF-128 defined in RFC 4615
 */
int mbedtls_aes_cmac_prf_128( const unsigned char *key, size_t key_length,
                              const unsigned char *input, size_t in_len,
                              unsigned char *output )
{
    int ret;
    const mbedtls_cipher_info_t *cipher_info;
    unsigned char zero_key[MBEDTLS_AES_BLOCK_SIZE];
    unsigned char int_key[MBEDTLS_AES_BLOCK_SIZE];

    if( key == NULL || input == NULL || output == NULL )
        return( MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA );

    cipher_info = mbedtls_cipher_info_from_type( MBEDTLS_CIPHER_AES_128_ECB );
    if( cipher_info == NULL )
    {
        /* Failing at this point must be due to a build issue */
        ret = MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE;
        goto exit;
    }

    if( key_length == MBEDTLS_AES_BLOCK_SIZE )
    {
        /* Use key as is */
        memcpy( int_key, key, MBEDTLS_AES_BLOCK_SIZE );
    }
    else
    {
        memset( zero_key, 0, MBEDTLS_AES_BLOCK_SIZE );

        ret = mbedtls_cipher_cmac( cipher_info, zero_key, 128, key,
                                   key_length, int_key );
        if( ret != 0 )
            goto exit;
    }

    ret = mbedtls_cipher_cmac( cipher_info, int_key, 128, input, in_len,
                               output );

exit:
    mbedtls_zeroize( int_key, sizeof( int_key ) );

    return( ret );
}
#endif /* MBEDTLS_AES_C */

#endif /* MBEDTLS_CMAC_C */
