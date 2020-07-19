/*
 *  HMAC_DRBG implementation (NIST SP 800-90)
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
 *  The NIST SP 800-90A DRBGs are described in the following publication.
 *  http://csrc.nist.gov/publications/nistpubs/800-90A/SP800-90A.pdf
 *  References below are based on rev. 1 (January 2012).
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_HMAC_DRBG_C)

#include "mbedtls/hmac_drbg.h"

#include <string.h>

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#endif

#include "arc4_alt.h"

/*
 * HMAC_DRBG context initialization
 */
void mbedtls_hmac_drbg_init( mbedtls_hmac_drbg_context *ctx )
{
    memset( ctx, 0, sizeof( mbedtls_hmac_drbg_context ) );

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_init( &ctx->mutex );
#endif
}

/*
 * HMAC_DRBG update, using optional additional data (10.1.2.2)
 */
void mbedtls_hmac_drbg_update( mbedtls_hmac_drbg_context *ctx,
                       const unsigned char *additional, size_t add_len )
{
    size_t md_len = mbedtls_md_get_size( ctx->md_ctx.md_info );
    unsigned char rounds = ( additional != NULL && add_len != 0 ) ? 2 : 1;
    unsigned char sep[1];
    unsigned char K[MBEDTLS_MD_MAX_SIZE];

    for( sep[0] = 0; sep[0] < rounds; sep[0]++ )
    {
        /* Step 1 or 4 */
        mbedtls_md_hmac_reset( &ctx->md_ctx );
        mbedtls_md_hmac_update( &ctx->md_ctx, ctx->V, md_len );
        mbedtls_md_hmac_update( &ctx->md_ctx, sep, 1 );
        if( rounds == 2 )
            mbedtls_md_hmac_update( &ctx->md_ctx, additional, add_len );
        mbedtls_md_hmac_finish( &ctx->md_ctx, K );

        /* Step 2 or 5 */
        mbedtls_md_hmac_starts( &ctx->md_ctx, K, md_len );
        mbedtls_md_hmac_update( &ctx->md_ctx, ctx->V, md_len );
        mbedtls_md_hmac_finish( &ctx->md_ctx, ctx->V );
    }
}

/*
 * Simplified HMAC_DRBG initialisation (for use with deterministic ECDSA)
 */
int mbedtls_hmac_drbg_seed_buf( mbedtls_hmac_drbg_context *ctx,
                        const mbedtls_md_info_t * md_info,
                        const unsigned char *data, size_t data_len )
{
    int ret;

    if( ( ret = mbedtls_md_setup( &ctx->md_ctx, md_info, 1 ) ) != 0 )
        return( ret );

    /*
     * Set initial working state.
     * Use the V memory location, which is currently all 0, to initialize the
     * MD context with an all-zero key. Then set V to its initial value.
     */
    mbedtls_md_hmac_starts( &ctx->md_ctx, ctx->V, mbedtls_md_get_size( md_info ) );
    memset( ctx->V, 0x01, mbedtls_md_get_size( md_info ) );

    mbedtls_hmac_drbg_update( ctx, data, data_len );

    return( 0 );
}

/*
 * HMAC_DRBG reseeding: 10.1.2.4 (arabic) + 9.2 (Roman)
 */
int mbedtls_hmac_drbg_reseed( mbedtls_hmac_drbg_context *ctx,
                      const unsigned char *additional, size_t len )
{
    unsigned char seed[MBEDTLS_HMAC_DRBG_MAX_SEED_INPUT];
    size_t seedlen;

    /* III. Check input length */
    if( len > MBEDTLS_HMAC_DRBG_MAX_INPUT ||
        ctx->entropy_len + len > MBEDTLS_HMAC_DRBG_MAX_SEED_INPUT )
    {
        return( MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG );
    }

    memset( seed, 0, MBEDTLS_HMAC_DRBG_MAX_SEED_INPUT );

    /* IV. Gather entropy_len bytes of entropy for the seed */
    if( ctx->f_entropy( ctx->p_entropy, seed, ctx->entropy_len ) != 0 )
        return( MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED );

    seedlen = ctx->entropy_len;

    /* 1. Concatenate entropy and additional data if any */
    if( additional != NULL && len != 0 )
    {
        memcpy( seed + seedlen, additional, len );
        seedlen += len;
    }

    /* 2. Update state */
    mbedtls_hmac_drbg_update( ctx, seed, seedlen );

    /* 3. Reset reseed_counter */
    ctx->reseed_counter = 1;

    /* 4. Done */
    return( 0 );
}

/*
 * HMAC_DRBG initialisation (10.1.2.3 + 9.1)
 */
int mbedtls_hmac_drbg_seed( mbedtls_hmac_drbg_context *ctx,
                    const mbedtls_md_info_t * md_info,
                    int (*f_entropy)(void *, unsigned char *, size_t),
                    void *p_entropy,
                    const unsigned char *custom,
                    size_t len )
{
    int ret;
    size_t entropy_len, md_size;

    if( ( ret = mbedtls_md_setup( &ctx->md_ctx, md_info, 1 ) ) != 0 )
        return( ret );

    md_size = mbedtls_md_get_size( md_info );

    /*
     * Set initial working state.
     * Use the V memory location, which is currently all 0, to initialize the
     * MD context with an all-zero key. Then set V to its initial value.
     */
    mbedtls_md_hmac_starts( &ctx->md_ctx, ctx->V, md_size );
    memset( ctx->V, 0x01, md_size );

    ctx->f_entropy = f_entropy;
    ctx->p_entropy = p_entropy;

    ctx->reseed_interval = MBEDTLS_HMAC_DRBG_RESEED_INTERVAL;

    /*
     * See SP800-57 5.6.1 (p. 65-66) for the security strength provided by
     * each hash function, then according to SP800-90A rev1 10.1 table 2,
     * min_entropy_len (in bits) is security_strength.
     *
     * (This also matches the sizes used in the NIST test vectors.)
     */
    entropy_len = md_size <= 20 ? 16 : /* 160-bits hash -> 128 bits */
                  md_size <= 28 ? 24 : /* 224-bits hash -> 192 bits */
                                  32;  /* better (256+) -> 256 bits */

    /*
     * For initialisation, use more entropy to emulate a nonce
     * (Again, matches test vectors.)
     */
    ctx->entropy_len = entropy_len * 3 / 2;

    if( ( ret = mbedtls_hmac_drbg_reseed( ctx, custom, len ) ) != 0 )
        return( ret );

    ctx->entropy_len = entropy_len;

    return( 0 );
}

/*
 * Set prediction resistance
 */
void mbedtls_hmac_drbg_set_prediction_resistance( mbedtls_hmac_drbg_context *ctx,
                                          int resistance )
{
    ctx->prediction_resistance = resistance;
}

/*
 * Set entropy length grabbed for reseeds
 */
void mbedtls_hmac_drbg_set_entropy_len( mbedtls_hmac_drbg_context *ctx, size_t len )
{
    ctx->entropy_len = len;
}

/*
 * Set reseed interval
 */
void mbedtls_hmac_drbg_set_reseed_interval( mbedtls_hmac_drbg_context *ctx, int interval )
{
    ctx->reseed_interval = interval;
}

/*
 * HMAC_DRBG random function with optional additional data:
 * 10.1.2.5 (arabic) + 9.3 (Roman)
 */
int mbedtls_hmac_drbg_random_with_add( void *p_rng,
                               unsigned char *output, size_t out_len,
                               const unsigned char *additional, size_t add_len )
{
    int ret;
    mbedtls_hmac_drbg_context *ctx = (mbedtls_hmac_drbg_context *) p_rng;
    size_t md_len = mbedtls_md_get_size( ctx->md_ctx.md_info );
    size_t left = out_len;
    unsigned char *out = output;

    /* II. Check request length */
    if( out_len > MBEDTLS_HMAC_DRBG_MAX_REQUEST )
        return( MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG );

    /* III. Check input length */
    if( add_len > MBEDTLS_HMAC_DRBG_MAX_INPUT )
        return( MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG );

    /* 1. (aka VII and IX) Check reseed counter and PR */
    if( ctx->f_entropy != NULL && /* For no-reseeding instances */
        ( ctx->prediction_resistance == MBEDTLS_HMAC_DRBG_PR_ON ||
          ctx->reseed_counter > ctx->reseed_interval ) )
    {
        if( ( ret = mbedtls_hmac_drbg_reseed( ctx, additional, add_len ) ) != 0 )
            return( ret );

        add_len = 0; /* VII.4 */
    }

    /* 2. Use additional data if any */
    if( additional != NULL && add_len != 0 )
        mbedtls_hmac_drbg_update( ctx, additional, add_len );

    /* 3, 4, 5. Generate bytes */
    while( left != 0 )
    {
        size_t use_len = left > md_len ? md_len : left;

        mbedtls_md_hmac_reset( &ctx->md_ctx );
        mbedtls_md_hmac_update( &ctx->md_ctx, ctx->V, md_len );
        mbedtls_md_hmac_finish( &ctx->md_ctx, ctx->V );

        memcpy( out, ctx->V, use_len );
        out += use_len;
        left -= use_len;
    }

    /* 6. Update */
    mbedtls_hmac_drbg_update( ctx, additional, add_len );

    /* 7. Update reseed counter */
    ctx->reseed_counter++;

    /* 8. Done */
    return( 0 );
}

/*
 * HMAC_DRBG random function
 */
int mbedtls_hmac_drbg_random( void *p_rng, unsigned char *output, size_t out_len )
{
    int ret;
    mbedtls_hmac_drbg_context *ctx = (mbedtls_hmac_drbg_context *) p_rng;

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    ret = mbedtls_hmac_drbg_random_with_add( ctx, output, out_len, NULL, 0 );

#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

/*
 * Free an HMAC_DRBG context
 */
void mbedtls_hmac_drbg_free( mbedtls_hmac_drbg_context *ctx )
{
    if( ctx == NULL )
        return;

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_free( &ctx->mutex );
#endif
    mbedtls_md_free( &ctx->md_ctx );
    mbedtls_zeroize( ctx, sizeof( mbedtls_hmac_drbg_context ) );
}

#if defined(MBEDTLS_FS_IO)
int mbedtls_hmac_drbg_write_seed_file( mbedtls_hmac_drbg_context *ctx, const char *path )
{
    int ret;
    FILE *f;
    unsigned char buf[ MBEDTLS_HMAC_DRBG_MAX_INPUT ];

    if( ( f = fopen( path, "wb" ) ) == NULL )
        return( MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR );

    if( ( ret = mbedtls_hmac_drbg_random( ctx, buf, sizeof( buf ) ) ) != 0 )
        goto exit;

    if( fwrite( buf, 1, sizeof( buf ), f ) != sizeof( buf ) )
    {
        ret = MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR;
        goto exit;
    }

    ret = 0;

exit:
    fclose( f );
    return( ret );
}

int mbedtls_hmac_drbg_update_seed_file( mbedtls_hmac_drbg_context *ctx, const char *path )
{
    FILE *f;
    size_t n;
    unsigned char buf[ MBEDTLS_HMAC_DRBG_MAX_INPUT ];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR );

    fseek( f, 0, SEEK_END );
    n = (size_t) ftell( f );
    fseek( f, 0, SEEK_SET );

    if( n > MBEDTLS_HMAC_DRBG_MAX_INPUT )
    {
        fclose( f );
        return( MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG );
    }

    if( fread( buf, 1, n, f ) != n )
    {
        fclose( f );
        return( MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR );
    }

    fclose( f );

    mbedtls_hmac_drbg_update( ctx, buf, n );

    return( mbedtls_hmac_drbg_write_seed_file( ctx, path ) );
}
#endif /* MBEDTLS_FS_IO */

#endif /* MBEDTLS_HMAC_DRBG_C */
