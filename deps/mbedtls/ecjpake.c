/*
 *  Elliptic curve J-PAKE
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
 * References in the code are to the Thread v1.0 Specification,
 * available to members of the Thread Group http://threadgroup.org/
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_ECJPAKE_C)

#include "mbedtls/ecjpake.h"

#include <string.h>

/*
 * Convert a mbedtls_ecjpake_role to identifier string
 */
static const char * const ecjpake_id[] = {
    "client",
    "server"
};

#define ID_MINE     ( ecjpake_id[ ctx->role ] )
#define ID_PEER     ( ecjpake_id[ 1 - ctx->role ] )

/*
 * Initialize context
 */
void mbedtls_ecjpake_init( mbedtls_ecjpake_context *ctx )
{
    if( ctx == NULL )
        return;

    ctx->md_info = NULL;
    mbedtls_ecp_group_init( &ctx->grp );
    ctx->point_format = MBEDTLS_ECP_PF_UNCOMPRESSED;

    mbedtls_ecp_point_init( &ctx->Xm1 );
    mbedtls_ecp_point_init( &ctx->Xm2 );
    mbedtls_ecp_point_init( &ctx->Xp1 );
    mbedtls_ecp_point_init( &ctx->Xp2 );
    mbedtls_ecp_point_init( &ctx->Xp  );

    mbedtls_mpi_init( &ctx->xm1 );
    mbedtls_mpi_init( &ctx->xm2 );
    mbedtls_mpi_init( &ctx->s   );
}

/*
 * Free context
 */
void mbedtls_ecjpake_free( mbedtls_ecjpake_context *ctx )
{
    if( ctx == NULL )
        return;

    ctx->md_info = NULL;
    mbedtls_ecp_group_free( &ctx->grp );

    mbedtls_ecp_point_free( &ctx->Xm1 );
    mbedtls_ecp_point_free( &ctx->Xm2 );
    mbedtls_ecp_point_free( &ctx->Xp1 );
    mbedtls_ecp_point_free( &ctx->Xp2 );
    mbedtls_ecp_point_free( &ctx->Xp  );

    mbedtls_mpi_free( &ctx->xm1 );
    mbedtls_mpi_free( &ctx->xm2 );
    mbedtls_mpi_free( &ctx->s   );
}

/*
 * Setup context
 */
int mbedtls_ecjpake_setup( mbedtls_ecjpake_context *ctx,
                           mbedtls_ecjpake_role role,
                           mbedtls_md_type_t hash,
                           mbedtls_ecp_group_id curve,
                           const unsigned char *secret,
                           size_t len )
{
    int ret;

    ctx->role = role;

    if( ( ctx->md_info = mbedtls_md_info_from_type( hash ) ) == NULL )
        return( MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE );

    MBEDTLS_MPI_CHK( mbedtls_ecp_group_load( &ctx->grp, curve ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->s, secret, len ) );

cleanup:
    if( ret != 0 )
        mbedtls_ecjpake_free( ctx );

    return( ret );
}

/*
 * Check if context is ready for use
 */
int mbedtls_ecjpake_check( const mbedtls_ecjpake_context *ctx )
{
    if( ctx->md_info == NULL ||
        ctx->grp.id == MBEDTLS_ECP_DP_NONE ||
        ctx->s.p == NULL )
    {
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );
    }

    return( 0 );
}

/*
 * Write a point plus its length to a buffer
 */
static int ecjpake_write_len_point( unsigned char **p,
                                    const unsigned char *end,
                                    const mbedtls_ecp_group *grp,
                                    const int pf,
                                    const mbedtls_ecp_point *P )
{
    int ret;
    size_t len;

    /* Need at least 4 for length plus 1 for point */
    if( end < *p || end - *p < 5 )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    ret = mbedtls_ecp_point_write_binary( grp, P, pf,
                                          &len, *p + 4, end - ( *p + 4 ) );
    if( ret != 0 )
        return( ret );

    (*p)[0] = (unsigned char)( ( len >> 24 ) & 0xFF );
    (*p)[1] = (unsigned char)( ( len >> 16 ) & 0xFF );
    (*p)[2] = (unsigned char)( ( len >>  8 ) & 0xFF );
    (*p)[3] = (unsigned char)( ( len       ) & 0xFF );

    *p += 4 + len;

    return( 0 );
}

/*
 * Size of the temporary buffer for ecjpake_hash:
 * 3 EC points plus their length, plus ID and its length (4 + 6 bytes)
 */
#define ECJPAKE_HASH_BUF_LEN    ( 3 * ( 4 + MBEDTLS_ECP_MAX_PT_LEN ) + 4 + 6 )

/*
 * Compute hash for ZKP (7.4.2.2.2.1)
 */
static int ecjpake_hash( const mbedtls_md_info_t *md_info,
                         const mbedtls_ecp_group *grp,
                         const int pf,
                         const mbedtls_ecp_point *G,
                         const mbedtls_ecp_point *V,
                         const mbedtls_ecp_point *X,
                         const char *id,
                         mbedtls_mpi *h )
{
    int ret;
    unsigned char buf[ECJPAKE_HASH_BUF_LEN];
    unsigned char *p = buf;
    const unsigned char *end = buf + sizeof( buf );
    const size_t id_len = strlen( id );
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];

    /* Write things to temporary buffer */
    MBEDTLS_MPI_CHK( ecjpake_write_len_point( &p, end, grp, pf, G ) );
    MBEDTLS_MPI_CHK( ecjpake_write_len_point( &p, end, grp, pf, V ) );
    MBEDTLS_MPI_CHK( ecjpake_write_len_point( &p, end, grp, pf, X ) );

    if( end - p < 4 )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    *p++ = (unsigned char)( ( id_len >> 24 ) & 0xFF );
    *p++ = (unsigned char)( ( id_len >> 16 ) & 0xFF );
    *p++ = (unsigned char)( ( id_len >>  8 ) & 0xFF );
    *p++ = (unsigned char)( ( id_len       ) & 0xFF );

    if( end < p || (size_t)( end - p ) < id_len )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    memcpy( p, id, id_len );
    p += id_len;

    /* Compute hash */
    mbedtls_md( md_info, buf, p - buf, hash );

    /* Turn it into an integer mod n */
    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( h, hash,
                                        mbedtls_md_get_size( md_info ) ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( h, h, &grp->N ) );

cleanup:
    return( ret );
}

/*
 * Parse a ECShnorrZKP (7.4.2.2.2) and verify it (7.4.2.3.3)
 */
static int ecjpake_zkp_read( const mbedtls_md_info_t *md_info,
                             const mbedtls_ecp_group *grp,
                             const int pf,
                             const mbedtls_ecp_point *G,
                             const mbedtls_ecp_point *X,
                             const char *id,
                             const unsigned char **p,
                             const unsigned char *end )
{
    int ret;
    mbedtls_ecp_point V, VV;
    mbedtls_mpi r, h;
    size_t r_len;

    mbedtls_ecp_point_init( &V );
    mbedtls_ecp_point_init( &VV );
    mbedtls_mpi_init( &r );
    mbedtls_mpi_init( &h );

    /*
     * struct {
     *     ECPoint V;
     *     opaque r<1..2^8-1>;
     * } ECSchnorrZKP;
     */
    if( end < *p )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    MBEDTLS_MPI_CHK( mbedtls_ecp_tls_read_point( grp, &V, p, end - *p ) );

    if( end < *p || (size_t)( end - *p ) < 1 )
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    r_len = *(*p)++;

    if( end < *p || (size_t)( end - *p ) < r_len )
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &r, *p, r_len ) );
    *p += r_len;

    /*
     * Verification
     */
    MBEDTLS_MPI_CHK( ecjpake_hash( md_info, grp, pf, G, &V, X, id, &h ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_muladd( (mbedtls_ecp_group *) grp,
                     &VV, &h, X, &r, G ) );

    if( mbedtls_ecp_point_cmp( &VV, &V ) != 0 )
    {
        ret = MBEDTLS_ERR_ECP_VERIFY_FAILED;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_point_free( &V );
    mbedtls_ecp_point_free( &VV );
    mbedtls_mpi_free( &r );
    mbedtls_mpi_free( &h );

    return( ret );
}

/*
 * Generate ZKP (7.4.2.3.2) and write it as ECSchnorrZKP (7.4.2.2.2)
 */
static int ecjpake_zkp_write( const mbedtls_md_info_t *md_info,
                              const mbedtls_ecp_group *grp,
                              const int pf,
                              const mbedtls_ecp_point *G,
                              const mbedtls_mpi *x,
                              const mbedtls_ecp_point *X,
                              const char *id,
                              unsigned char **p,
                              const unsigned char *end,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng )
{
    int ret;
    mbedtls_ecp_point V;
    mbedtls_mpi v;
    mbedtls_mpi h; /* later recycled to hold r */
    size_t len;

    if( end < *p )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    mbedtls_ecp_point_init( &V );
    mbedtls_mpi_init( &v );
    mbedtls_mpi_init( &h );

    /* Compute signature */
    MBEDTLS_MPI_CHK( mbedtls_ecp_gen_keypair_base( (mbedtls_ecp_group *) grp,
                                                   G, &v, &V, f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( ecjpake_hash( md_info, grp, pf, G, &V, X, id, &h ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &h, &h, x ) ); /* x*h */
    MBEDTLS_MPI_CHK( mbedtls_mpi_sub_mpi( &h, &v, &h ) ); /* v - x*h */
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &h, &h, &grp->N ) ); /* r */

    /* Write it out */
    MBEDTLS_MPI_CHK( mbedtls_ecp_tls_write_point( grp, &V,
                pf, &len, *p, end - *p ) );
    *p += len;

    len = mbedtls_mpi_size( &h ); /* actually r */
    if( end < *p || (size_t)( end - *p ) < 1 + len || len > 255 )
    {
        ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    *(*p)++ = (unsigned char)( len & 0xFF );
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &h, *p, len ) ); /* r */
    *p += len;

cleanup:
    mbedtls_ecp_point_free( &V );
    mbedtls_mpi_free( &v );
    mbedtls_mpi_free( &h );

    return( ret );
}

/*
 * Parse a ECJPAKEKeyKP (7.4.2.2.1) and check proof
 * Output: verified public key X
 */
static int ecjpake_kkp_read( const mbedtls_md_info_t *md_info,
                             const mbedtls_ecp_group *grp,
                             const int pf,
                             const mbedtls_ecp_point *G,
                             mbedtls_ecp_point *X,
                             const char *id,
                             const unsigned char **p,
                             const unsigned char *end )
{
    int ret;

    if( end < *p )
        return( MBEDTLS_ERR_ECP_BAD_INPUT_DATA );

    /*
     * struct {
     *     ECPoint X;
     *     ECSchnorrZKP zkp;
     * } ECJPAKEKeyKP;
     */
    MBEDTLS_MPI_CHK( mbedtls_ecp_tls_read_point( grp, X, p, end - *p ) );
    if( mbedtls_ecp_is_zero( X ) )
    {
        ret = MBEDTLS_ERR_ECP_INVALID_KEY;
        goto cleanup;
    }

    MBEDTLS_MPI_CHK( ecjpake_zkp_read( md_info, grp, pf, G, X, id, p, end ) );

cleanup:
    return( ret );
}

/*
 * Generate an ECJPAKEKeyKP
 * Output: the serialized structure, plus private/public key pair
 */
static int ecjpake_kkp_write( const mbedtls_md_info_t *md_info,
                              const mbedtls_ecp_group *grp,
                              const int pf,
                              const mbedtls_ecp_point *G,
                              mbedtls_mpi *x,
                              mbedtls_ecp_point *X,
                              const char *id,
                              unsigned char **p,
                              const unsigned char *end,
                              int (*f_rng)(void *, unsigned char *, size_t),
                              void *p_rng )
{
    int ret;
    size_t len;

    if( end < *p )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    /* Generate key (7.4.2.3.1) and write it out */
    MBEDTLS_MPI_CHK( mbedtls_ecp_gen_keypair_base( (mbedtls_ecp_group *) grp, G, x, X,
                                                   f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_tls_write_point( grp, X,
                pf, &len, *p, end - *p ) );
    *p += len;

    /* Generate and write proof */
    MBEDTLS_MPI_CHK( ecjpake_zkp_write( md_info, grp, pf, G, x, X, id,
                                        p, end, f_rng, p_rng ) );

cleanup:
    return( ret );
}

/*
 * Read a ECJPAKEKeyKPPairList (7.4.2.3) and check proofs
 * Ouputs: verified peer public keys Xa, Xb
 */
static int ecjpake_kkpp_read( const mbedtls_md_info_t *md_info,
                              const mbedtls_ecp_group *grp,
                              const int pf,
                              const mbedtls_ecp_point *G,
                              mbedtls_ecp_point *Xa,
                              mbedtls_ecp_point *Xb,
                              const char *id,
                              const unsigned char *buf,
                              size_t len )
{
    int ret;
    const unsigned char *p = buf;
    const unsigned char *end = buf + len;

    /*
     * struct {
     *     ECJPAKEKeyKP ecjpake_key_kp_pair_list[2];
     * } ECJPAKEKeyKPPairList;
     */
    MBEDTLS_MPI_CHK( ecjpake_kkp_read( md_info, grp, pf, G, Xa, id, &p, end ) );
    MBEDTLS_MPI_CHK( ecjpake_kkp_read( md_info, grp, pf, G, Xb, id, &p, end ) );

    if( p != end )
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

cleanup:
    return( ret );
}

/*
 * Generate a ECJPAKEKeyKPPairList
 * Outputs: the serialized structure, plus two private/public key pairs
 */
static int ecjpake_kkpp_write( const mbedtls_md_info_t *md_info,
                               const mbedtls_ecp_group *grp,
                               const int pf,
                               const mbedtls_ecp_point *G,
                               mbedtls_mpi *xm1,
                               mbedtls_ecp_point *Xa,
                               mbedtls_mpi *xm2,
                               mbedtls_ecp_point *Xb,
                               const char *id,
                               unsigned char *buf,
                               size_t len,
                               size_t *olen,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng )
{
    int ret;
    unsigned char *p = buf;
    const unsigned char *end = buf + len;

    MBEDTLS_MPI_CHK( ecjpake_kkp_write( md_info, grp, pf, G, xm1, Xa, id,
                &p, end, f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( ecjpake_kkp_write( md_info, grp, pf, G, xm2, Xb, id,
                &p, end, f_rng, p_rng ) );

    *olen = p - buf;

cleanup:
    return( ret );
}

/*
 * Read and process the first round message
 */
int mbedtls_ecjpake_read_round_one( mbedtls_ecjpake_context *ctx,
                                    const unsigned char *buf,
                                    size_t len )
{
    return( ecjpake_kkpp_read( ctx->md_info, &ctx->grp, ctx->point_format,
                               &ctx->grp.G,
                               &ctx->Xp1, &ctx->Xp2, ID_PEER,
                               buf, len ) );
}

/*
 * Generate and write the first round message
 */
int mbedtls_ecjpake_write_round_one( mbedtls_ecjpake_context *ctx,
                            unsigned char *buf, size_t len, size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng )
{
    return( ecjpake_kkpp_write( ctx->md_info, &ctx->grp, ctx->point_format,
                                &ctx->grp.G,
                                &ctx->xm1, &ctx->Xm1, &ctx->xm2, &ctx->Xm2,
                                ID_MINE, buf, len, olen, f_rng, p_rng ) );
}

/*
 * Compute the sum of three points R = A + B + C
 */
static int ecjpake_ecp_add3( mbedtls_ecp_group *grp, mbedtls_ecp_point *R,
                             const mbedtls_ecp_point *A,
                             const mbedtls_ecp_point *B,
                             const mbedtls_ecp_point *C )
{
    int ret;
    mbedtls_mpi one;

    mbedtls_mpi_init( &one );

    MBEDTLS_MPI_CHK( mbedtls_mpi_lset( &one, 1 ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_muladd( grp, R, &one, A, &one, B ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_muladd( grp, R, &one, R, &one, C ) );

cleanup:
    mbedtls_mpi_free( &one );

    return( ret );
}

/*
 * Read and process second round message (C: 7.4.2.5, S: 7.4.2.6)
 */
int mbedtls_ecjpake_read_round_two( mbedtls_ecjpake_context *ctx,
                                            const unsigned char *buf,
                                            size_t len )
{
    int ret;
    const unsigned char *p = buf;
    const unsigned char *end = buf + len;
    mbedtls_ecp_group grp;
    mbedtls_ecp_point G;    /* C: GB, S: GA */

    mbedtls_ecp_group_init( &grp );
    mbedtls_ecp_point_init( &G );

    /*
     * Server: GA = X3  + X4  + X1      (7.4.2.6.1)
     * Client: GB = X1  + X2  + X3      (7.4.2.5.1)
     * Unified: G = Xm1 + Xm2 + Xp1
     * We need that before parsing in order to check Xp as we read it
     */
    MBEDTLS_MPI_CHK( ecjpake_ecp_add3( &ctx->grp, &G,
                                       &ctx->Xm1, &ctx->Xm2, &ctx->Xp1 ) );

    /*
     * struct {
     *     ECParameters curve_params;   // only client reading server msg
     *     ECJPAKEKeyKP ecjpake_key_kp;
     * } Client/ServerECJPAKEParams;
     */
    if( ctx->role == MBEDTLS_ECJPAKE_CLIENT )
    {
        MBEDTLS_MPI_CHK( mbedtls_ecp_tls_read_group( &grp, &p, len ) );
        if( grp.id != ctx->grp.id )
        {
            ret = MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE;
            goto cleanup;
        }
    }

    MBEDTLS_MPI_CHK( ecjpake_kkp_read( ctx->md_info, &ctx->grp,
                            ctx->point_format,
                            &G, &ctx->Xp, ID_PEER, &p, end ) );

    if( p != end )
    {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto cleanup;
    }

cleanup:
    mbedtls_ecp_group_free( &grp );
    mbedtls_ecp_point_free( &G );

    return( ret );
}

/*
 * Compute R = +/- X * S mod N, taking care not to leak S
 */
static int ecjpake_mul_secret( mbedtls_mpi *R, int sign,
                               const mbedtls_mpi *X,
                               const mbedtls_mpi *S,
                               const mbedtls_mpi *N,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng )
{
    int ret;
    mbedtls_mpi b; /* Blinding value, then s + N * blinding */

    mbedtls_mpi_init( &b );

    /* b = s + rnd-128-bit * N */
    MBEDTLS_MPI_CHK( mbedtls_mpi_fill_random( &b, 16, f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &b, &b, N ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_add_mpi( &b, &b, S ) );

    /* R = sign * X * b mod N */
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( R, X, &b ) );
    R->s *= sign;
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( R, R, N ) );

cleanup:
    mbedtls_mpi_free( &b );

    return( ret );
}

/*
 * Generate and write the second round message (S: 7.4.2.5, C: 7.4.2.6)
 */
int mbedtls_ecjpake_write_round_two( mbedtls_ecjpake_context *ctx,
                            unsigned char *buf, size_t len, size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng )
{
    int ret;
    mbedtls_ecp_point G;    /* C: GA, S: GB */
    mbedtls_ecp_point Xm;   /* C: Xc, S: Xs */
    mbedtls_mpi xm;         /* C: xc, S: xs */
    unsigned char *p = buf;
    const unsigned char *end = buf + len;
    size_t ec_len;

    mbedtls_ecp_point_init( &G );
    mbedtls_ecp_point_init( &Xm );
    mbedtls_mpi_init( &xm );

    /*
     * First generate private/public key pair (S: 7.4.2.5.1, C: 7.4.2.6.1)
     *
     * Client:  GA = X1  + X3  + X4  | xs = x2  * s | Xc = xc * GA
     * Server:  GB = X3  + X1  + X2  | xs = x4  * s | Xs = xs * GB
     * Unified: G  = Xm1 + Xp1 + Xp2 | xm = xm2 * s | Xm = xm * G
     */
    MBEDTLS_MPI_CHK( ecjpake_ecp_add3( &ctx->grp, &G,
                                       &ctx->Xp1, &ctx->Xp2, &ctx->Xm1 ) );
    MBEDTLS_MPI_CHK( ecjpake_mul_secret( &xm, 1, &ctx->xm2, &ctx->s,
                                         &ctx->grp.N, f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_mul( &ctx->grp, &Xm, &xm, &G, f_rng, p_rng ) );

    /*
     * Now write things out
     *
     * struct {
     *     ECParameters curve_params;   // only server writing its message
     *     ECJPAKEKeyKP ecjpake_key_kp;
     * } Client/ServerECJPAKEParams;
     */
    if( ctx->role == MBEDTLS_ECJPAKE_SERVER )
    {
        if( end < p )
        {
            ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
            goto cleanup;
        }
        MBEDTLS_MPI_CHK( mbedtls_ecp_tls_write_group( &ctx->grp, &ec_len,
                                                      p, end - p ) );
        p += ec_len;
    }

    if( end < p )
    {
        ret = MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    MBEDTLS_MPI_CHK( mbedtls_ecp_tls_write_point( &ctx->grp, &Xm,
                     ctx->point_format, &ec_len, p, end - p ) );
    p += ec_len;

    MBEDTLS_MPI_CHK( ecjpake_zkp_write( ctx->md_info, &ctx->grp,
                                        ctx->point_format,
                                        &G, &xm, &Xm, ID_MINE,
                                        &p, end, f_rng, p_rng ) );

    *olen = p - buf;

cleanup:
    mbedtls_ecp_point_free( &G );
    mbedtls_ecp_point_free( &Xm );
    mbedtls_mpi_free( &xm );

    return( ret );
}

/*
 * Derive PMS (7.4.2.7 / 7.4.2.8)
 */
int mbedtls_ecjpake_derive_secret( mbedtls_ecjpake_context *ctx,
                            unsigned char *buf, size_t len, size_t *olen,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng )
{
    int ret;
    mbedtls_ecp_point K;
    mbedtls_mpi m_xm2_s, one;
    unsigned char kx[MBEDTLS_ECP_MAX_BYTES];
    size_t x_bytes;

    *olen = mbedtls_md_get_size( ctx->md_info );
    if( len < *olen )
        return( MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL );

    mbedtls_ecp_point_init( &K );
    mbedtls_mpi_init( &m_xm2_s );
    mbedtls_mpi_init( &one );

    MBEDTLS_MPI_CHK( mbedtls_mpi_lset( &one, 1 ) );

    /*
     * Client:  K = ( Xs - X4  * x2  * s ) * x2
     * Server:  K = ( Xc - X2  * x4  * s ) * x4
     * Unified: K = ( Xp - Xp2 * xm2 * s ) * xm2
     */
    MBEDTLS_MPI_CHK( ecjpake_mul_secret( &m_xm2_s, -1, &ctx->xm2, &ctx->s,
                                         &ctx->grp.N, f_rng, p_rng ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_muladd( &ctx->grp, &K,
                                         &one, &ctx->Xp,
                                         &m_xm2_s, &ctx->Xp2 ) );
    MBEDTLS_MPI_CHK( mbedtls_ecp_mul( &ctx->grp, &K, &ctx->xm2, &K,
                                      f_rng, p_rng ) );

    /* PMS = SHA-256( K.X ) */
    x_bytes = ( ctx->grp.pbits + 7 ) / 8;
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &K.X, kx, x_bytes ) );
    MBEDTLS_MPI_CHK( mbedtls_md( ctx->md_info, kx, x_bytes, buf ) );

cleanup:
    mbedtls_ecp_point_free( &K );
    mbedtls_mpi_free( &m_xm2_s );
    mbedtls_mpi_free( &one );

    return( ret );
}

#undef ID_MINE
#undef ID_PEER

#endif /* MBEDTLS_ECJPAKE_C */
