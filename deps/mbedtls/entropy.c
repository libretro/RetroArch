/*
 *  Entropy accumulator implementation
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

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ENTROPY_C)

#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"

#include <string.h>

#if defined(MBEDTLS_FS_IO)
#include <stdio.h>
#endif

#include "arc4_alt.h"

#define ENTROPY_MAX_LOOP    256     /**< Maximum amount to loop before error */

void mbedtls_entropy_init( mbedtls_entropy_context *ctx )
{
    memset( ctx, 0, sizeof(mbedtls_entropy_context) );

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_init( &ctx->mutex );
#endif

#if defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR)
    mbedtls_sha512_starts( &ctx->accumulator, 0 );
#else
    mbedtls_sha256_starts( &ctx->accumulator, 0 );
#endif

    mbedtls_entropy_add_source( ctx, mbedtls_platform_entropy_poll, NULL,
                                MBEDTLS_ENTROPY_MIN_PLATFORM,
                                MBEDTLS_ENTROPY_SOURCE_STRONG );
#if defined(MBEDTLS_TIMING_C)
    mbedtls_entropy_add_source( ctx, mbedtls_hardclock_poll, NULL,
                                MBEDTLS_ENTROPY_MIN_HARDCLOCK,
                                MBEDTLS_ENTROPY_SOURCE_WEAK );
#endif
#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
    mbedtls_entropy_add_source( ctx, mbedtls_hardware_poll, NULL,
                                MBEDTLS_ENTROPY_MIN_HARDWARE,
                                MBEDTLS_ENTROPY_SOURCE_STRONG );
#endif
}

void mbedtls_entropy_free( mbedtls_entropy_context *ctx )
{
#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_free( &ctx->mutex );
#endif
    mbedtls_zeroize( ctx, sizeof( mbedtls_entropy_context ) );
}

int mbedtls_entropy_add_source( mbedtls_entropy_context *ctx,
                        mbedtls_entropy_f_source_ptr f_source, void *p_source,
                        size_t threshold, int strong )
{
    int idx, ret = 0;

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    idx = ctx->source_count;
    if( idx >= MBEDTLS_ENTROPY_MAX_SOURCES )
    {
        ret = MBEDTLS_ERR_ENTROPY_MAX_SOURCES;
        goto exit;
    }

    ctx->source[idx].f_source  = f_source;
    ctx->source[idx].p_source  = p_source;
    ctx->source[idx].threshold = threshold;
    ctx->source[idx].strong    = strong;

    ctx->source_count++;

exit:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

/*
 * Entropy accumulator update
 */
static int entropy_update( mbedtls_entropy_context *ctx, unsigned char source_id,
                           const unsigned char *data, size_t len )
{
    unsigned char header[2];
    unsigned char tmp[MBEDTLS_ENTROPY_BLOCK_SIZE];
    size_t use_len = len;
    const unsigned char *p = data;

    if( use_len > MBEDTLS_ENTROPY_BLOCK_SIZE )
    {
#if defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR)
        mbedtls_sha512( data, len, tmp, 0 );
#else
        mbedtls_sha256( data, len, tmp, 0 );
#endif
        p = tmp;
        use_len = MBEDTLS_ENTROPY_BLOCK_SIZE;
    }

    header[0] = source_id;
    header[1] = use_len & 0xFF;

#if defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR)
    mbedtls_sha512_update( &ctx->accumulator, header, 2 );
    mbedtls_sha512_update( &ctx->accumulator, p, use_len );
#else
    mbedtls_sha256_update( &ctx->accumulator, header, 2 );
    mbedtls_sha256_update( &ctx->accumulator, p, use_len );
#endif

    return( 0 );
}

int mbedtls_entropy_update_manual( mbedtls_entropy_context *ctx,
                           const unsigned char *data, size_t len )
{
    int ret;

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    ret = entropy_update( ctx, MBEDTLS_ENTROPY_SOURCE_MANUAL, data, len );

#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

/*
 * Run through the different sources to add entropy to our accumulator
 */
static int entropy_gather_internal( mbedtls_entropy_context *ctx )
{
    int ret, i, have_one_strong = 0;
    unsigned char buf[MBEDTLS_ENTROPY_MAX_GATHER];
    size_t olen;

    if( ctx->source_count == 0 )
        return( MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED );

    /*
     * Run through our entropy sources
     */
    for( i = 0; i < ctx->source_count; i++ )
    {
        if( ctx->source[i].strong == MBEDTLS_ENTROPY_SOURCE_STRONG )
            have_one_strong = 1;

        olen = 0;
        if( ( ret = ctx->source[i].f_source( ctx->source[i].p_source,
                        buf, MBEDTLS_ENTROPY_MAX_GATHER, &olen ) ) != 0 )
        {
            return( ret );
        }

        /*
         * Add if we actually gathered something
         */
        if( olen > 0 )
        {
            entropy_update( ctx, (unsigned char) i, buf, olen );
            ctx->source[i].size += olen;
        }
    }

    if( have_one_strong == 0 )
        return( MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE );

    return( 0 );
}

/*
 * Thread-safe wrapper for entropy_gather_internal()
 */
int mbedtls_entropy_gather( mbedtls_entropy_context *ctx )
{
    int ret;

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    ret = entropy_gather_internal( ctx );

#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

int mbedtls_entropy_func( void *data, unsigned char *output, size_t len )
{
    int ret, count = 0, i, done;
    mbedtls_entropy_context *ctx = (mbedtls_entropy_context *) data;
    unsigned char buf[MBEDTLS_ENTROPY_BLOCK_SIZE];

    if( len > MBEDTLS_ENTROPY_BLOCK_SIZE )
        return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    /*
     * Always gather extra entropy before a call
     */
    do
    {
        if( count++ > ENTROPY_MAX_LOOP )
        {
            ret = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            goto exit;
        }

        if( ( ret = entropy_gather_internal( ctx ) ) != 0 )
            goto exit;

        done = 1;
        for( i = 0; i < ctx->source_count; i++ )
            if( ctx->source[i].size < ctx->source[i].threshold )
                done = 0;
    }
    while( ! done );

    memset( buf, 0, MBEDTLS_ENTROPY_BLOCK_SIZE );

#if defined(MBEDTLS_ENTROPY_SHA512_ACCUMULATOR)
    mbedtls_sha512_finish( &ctx->accumulator, buf );

    /*
     * Reset accumulator and counters and recycle existing entropy
     */
    memset( &ctx->accumulator, 0, sizeof( mbedtls_sha512_context ) );
    mbedtls_sha512_starts( &ctx->accumulator, 0 );
    mbedtls_sha512_update( &ctx->accumulator, buf, MBEDTLS_ENTROPY_BLOCK_SIZE );

    /*
     * Perform second SHA-512 on entropy
     */
    mbedtls_sha512( buf, MBEDTLS_ENTROPY_BLOCK_SIZE, buf, 0 );
#else /* MBEDTLS_ENTROPY_SHA512_ACCUMULATOR */
    mbedtls_sha256_finish( &ctx->accumulator, buf );

    /*
     * Reset accumulator and counters and recycle existing entropy
     */
    memset( &ctx->accumulator, 0, sizeof( mbedtls_sha256_context ) );
    mbedtls_sha256_starts( &ctx->accumulator, 0 );
    mbedtls_sha256_update( &ctx->accumulator, buf, MBEDTLS_ENTROPY_BLOCK_SIZE );

    /*
     * Perform second SHA-256 on entropy
     */
    mbedtls_sha256( buf, MBEDTLS_ENTROPY_BLOCK_SIZE, buf, 0 );
#endif /* MBEDTLS_ENTROPY_SHA512_ACCUMULATOR */

    for( i = 0; i < ctx->source_count; i++ )
        ctx->source[i].size = 0;

    memcpy( output, buf, len );

    ret = 0;

exit:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

#if defined(MBEDTLS_FS_IO)
int mbedtls_entropy_write_seed_file( mbedtls_entropy_context *ctx, const char *path )
{
    int ret = MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR;
    FILE *f;
    unsigned char buf[MBEDTLS_ENTROPY_BLOCK_SIZE];

    if( ( f = fopen( path, "wb" ) ) == NULL )
        return( MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR );

    if( ( ret = mbedtls_entropy_func( ctx, buf, MBEDTLS_ENTROPY_BLOCK_SIZE ) ) != 0 )
        goto exit;

    if( fwrite( buf, 1, MBEDTLS_ENTROPY_BLOCK_SIZE, f ) != MBEDTLS_ENTROPY_BLOCK_SIZE )
    {
        ret = MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR;
        goto exit;
    }

    ret = 0;

exit:
    fclose( f );
    return( ret );
}

int mbedtls_entropy_update_seed_file( mbedtls_entropy_context *ctx, const char *path )
{
    FILE *f;
    size_t n;
    unsigned char buf[ MBEDTLS_ENTROPY_MAX_SEED_SIZE ];

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR );

    fseek( f, 0, SEEK_END );
    n = (size_t) ftell( f );
    fseek( f, 0, SEEK_SET );

    if( n > MBEDTLS_ENTROPY_MAX_SEED_SIZE )
        n = MBEDTLS_ENTROPY_MAX_SEED_SIZE;

    if( fread( buf, 1, n, f ) != n )
    {
        fclose( f );
        return( MBEDTLS_ERR_ENTROPY_FILE_IO_ERROR );
    }

    fclose( f );

    mbedtls_entropy_update_manual( ctx, buf, n );

    return( mbedtls_entropy_write_seed_file( ctx, path ) );
}
#endif /* MBEDTLS_FS_IO */

#endif /* MBEDTLS_ENTROPY_C */
