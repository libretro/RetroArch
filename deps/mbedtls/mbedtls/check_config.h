/**
 * \file check_config.h
 *
 * \brief Consistency checks for configuration options
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
 * It is recommended to include this file from your config.h
 * in order to catch dependency issues early.
 */

#ifndef MBEDTLS_CHECK_CONFIG_H
#define MBEDTLS_CHECK_CONFIG_H

/*
 * We assume CHAR_BIT is 8 in many places. In practice, this is true on our
 * target platforms, so not an issue, but let's just be extra sure.
 */
#include <limits.h>
#if CHAR_BIT != 8
#error "mbed TLS requires a platform with 8-bit chars"
#endif

#if defined(TARGET_LIKE_MBED) && \
    ( defined(MBEDTLS_NET_C) || defined(MBEDTLS_TIMING_C) )
#error "The NET and TIMING modules are not available for mbed OS - please use the network and timing functions provided by mbed OS"
#endif

#if defined(MBEDTLS_AESNI_C) && !defined(MBEDTLS_HAVE_ASM)
#error "MBEDTLS_AESNI_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_CTR_DRBG_C) && !defined(MBEDTLS_AES_C)
#error "MBEDTLS_CTR_DRBG_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_DHM_C) && !defined(MBEDTLS_BIGNUM_C)
#error "MBEDTLS_DHM_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECDH_C) && !defined(MBEDTLS_ECP_C)
#error "MBEDTLS_ECDH_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECDSA_C) &&            \
    ( !defined(MBEDTLS_ECP_C) ||           \
      !defined(MBEDTLS_ASN1_PARSE_C) ||    \
      !defined(MBEDTLS_ASN1_WRITE_C) )
#error "MBEDTLS_ECDSA_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECDSA_DETERMINISTIC) && !defined(MBEDTLS_HMAC_DRBG_C)
#error "MBEDTLS_ECDSA_DETERMINISTIC defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_C) && ( !defined(MBEDTLS_BIGNUM_C) || (   \
    !defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)   &&                  \
    !defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED) &&                  \
    !defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED) ) )
#error "MBEDTLS_ECP_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ENTROPY_C) && (!defined(MBEDTLS_SHA512_C) &&      \
                                    !defined(MBEDTLS_SHA256_C))
#error "MBEDTLS_ENTROPY_C defined, but not all prerequisites"
#endif
#if defined(MBEDTLS_ENTROPY_C) && defined(MBEDTLS_SHA512_C) &&         \
    defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN) && (MBEDTLS_CTR_DRBG_ENTROPY_LEN > 64)
#error "MBEDTLS_CTR_DRBG_ENTROPY_LEN value too high"
#endif
#if defined(MBEDTLS_ENTROPY_C) &&                                            \
    ( !defined(MBEDTLS_SHA512_C) || defined(MBEDTLS_ENTROPY_FORCE_SHA256) ) \
    && defined(MBEDTLS_CTR_DRBG_ENTROPY_LEN) && (MBEDTLS_CTR_DRBG_ENTROPY_LEN > 32)
#error "MBEDTLS_CTR_DRBG_ENTROPY_LEN value too high"
#endif
#if defined(MBEDTLS_ENTROPY_C) && \
    defined(MBEDTLS_ENTROPY_FORCE_SHA256) && !defined(MBEDTLS_SHA256_C)
#error "MBEDTLS_ENTROPY_FORCE_SHA256 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_GCM_C) && (                                        \
        !defined(MBEDTLS_AES_C) && !defined(MBEDTLS_CAMELLIA_C) )
#error "MBEDTLS_GCM_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_RANDOMIZE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_RANDOMIZE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_ADD_MIXED_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_ADD_MIXED_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_DOUBLE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_JAC_MANY_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_JAC_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_DOUBLE_ADD_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_RANDOMIZE_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_RANDOMIZE_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_ECP_NORMALIZE_MXZ_ALT) && !defined(MBEDTLS_ECP_INTERNAL_ALT)
#error "MBEDTLS_ECP_NORMALIZE_MXZ_ALT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_HMAC_DRBG_C) && !defined(MBEDTLS_MD_C)
#error "MBEDTLS_HMAC_DRBG_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_ECDH_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDH_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_ECDH_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDH_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED) && !defined(MBEDTLS_DHM_C)
#error "MBEDTLS_KEY_EXCHANGE_DHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED) &&                     \
    !defined(MBEDTLS_ECDH_C)
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED) &&                   \
    ( !defined(MBEDTLS_DHM_C) || !defined(MBEDTLS_RSA_C) ||           \
      !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_DHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_ECDH_C) || !defined(MBEDTLS_RSA_C) ||          \
      !defined(MBEDTLS_X509_CRT_PARSE_C) || !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) &&                 \
    ( !defined(MBEDTLS_ECDH_C) || !defined(MBEDTLS_ECDSA_C) ||          \
      !defined(MBEDTLS_X509_CRT_PARSE_C) )
#error "MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED) &&                   \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) || \
      !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_RSA_PSK_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_KEY_EXCHANGE_RSA_ENABLED) &&                       \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_X509_CRT_PARSE_C) || \
      !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_KEY_EXCHANGE_RSA_ENABLED defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PADLOCK_C) && !defined(MBEDTLS_HAVE_ASM)
#error "MBEDTLS_PADLOCK_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PEM_PARSE_C) && !defined(MBEDTLS_BASE64_C)
#error "MBEDTLS_PEM_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PEM_WRITE_C) && !defined(MBEDTLS_BASE64_C)
#error "MBEDTLS_PEM_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_C) && \
    ( !defined(MBEDTLS_RSA_C) && !defined(MBEDTLS_ECP_C) )
#error "MBEDTLS_PK_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_PARSE_C) && !defined(MBEDTLS_PK_C)
#error "MBEDTLS_PK_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_PK_WRITE_C) && !defined(MBEDTLS_PK_C)
#error "MBEDTLS_PK_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_RSA_C) && ( !defined(MBEDTLS_BIGNUM_C) ||         \
    !defined(MBEDTLS_OID_C) )
#error "MBEDTLS_RSA_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_RSA_C) && ( !defined(MBEDTLS_PKCS1_V21) &&         \
    !defined(MBEDTLS_PKCS1_V15) )
#error "MBEDTLS_RSA_C defined, but none of the PKCS1 versions enabled"
#endif

#if defined(MBEDTLS_X509_RSASSA_PSS_SUPPORT) &&                        \
    ( !defined(MBEDTLS_RSA_C) || !defined(MBEDTLS_PKCS1_V21) )
#error "MBEDTLS_X509_RSASSA_PSS_SUPPORT defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_PROTO_SSL3) && ( !defined(MBEDTLS_MD5_C) ||     \
    !defined(MBEDTLS_SHA1_C) )
#error "MBEDTLS_SSL_PROTO_SSL3 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1) && ( !defined(MBEDTLS_MD5_C) ||     \
    !defined(MBEDTLS_SHA1_C) )
#error "MBEDTLS_SSL_PROTO_TLS1 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_1) && ( !defined(MBEDTLS_MD5_C) ||     \
    !defined(MBEDTLS_SHA1_C) )
#error "MBEDTLS_SSL_PROTO_TLS1_1 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_PROTO_TLS1_2) && ( !defined(MBEDTLS_SHA1_C) &&     \
    !defined(MBEDTLS_SHA256_C) && !defined(MBEDTLS_SHA512_C) )
#error "MBEDTLS_SSL_PROTO_TLS1_2 defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_PROTO_DTLS)     && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_1)  && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_PROTO_DTLS defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CLI_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_CLI_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && ( !defined(MBEDTLS_CIPHER_C) ||     \
    !defined(MBEDTLS_MD_C) )
#error "MBEDTLS_SSL_TLS_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_SRV_C) && !defined(MBEDTLS_SSL_TLS_C)
#error "MBEDTLS_SSL_SRV_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && (!defined(MBEDTLS_SSL_PROTO_SSL3) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1) && !defined(MBEDTLS_SSL_PROTO_TLS1_1) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2))
#error "MBEDTLS_SSL_TLS_C defined, but no protocols are active"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && (defined(MBEDTLS_SSL_PROTO_SSL3) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_1) && !defined(MBEDTLS_SSL_PROTO_TLS1))
#error "Illegal protocol selection"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && (defined(MBEDTLS_SSL_PROTO_TLS1) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_2) && !defined(MBEDTLS_SSL_PROTO_TLS1_1))
#error "Illegal protocol selection"
#endif

#if defined(MBEDTLS_SSL_TLS_C) && (defined(MBEDTLS_SSL_PROTO_SSL3) && \
    defined(MBEDTLS_SSL_PROTO_TLS1_2) && (!defined(MBEDTLS_SSL_PROTO_TLS1) || \
    !defined(MBEDTLS_SSL_PROTO_TLS1_1)))
#error "Illegal protocol selection"
#endif

#if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY) && !defined(MBEDTLS_SSL_PROTO_DTLS)
#error "MBEDTLS_SSL_DTLS_HELLO_VERIFY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE) && \
    !defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
#error "MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_ANTI_REPLAY  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_DTLS_BADMAC_LIMIT) &&                              \
    ( !defined(MBEDTLS_SSL_TLS_C) || !defined(MBEDTLS_SSL_PROTO_DTLS) )
#error "MBEDTLS_SSL_DTLS_BADMAC_LIMIT  defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC) &&   \
    !defined(MBEDTLS_SSL_PROTO_TLS1)   &&      \
    !defined(MBEDTLS_SSL_PROTO_TLS1_1) &&      \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_ENCRYPT_THEN_MAC defined, but not all prerequsites"
#endif

#if defined(MBEDTLS_SSL_EXTENDED_MASTER_SECRET) && \
    !defined(MBEDTLS_SSL_PROTO_TLS1)   &&          \
    !defined(MBEDTLS_SSL_PROTO_TLS1_1) &&          \
    !defined(MBEDTLS_SSL_PROTO_TLS1_2)
#error "MBEDTLS_SSL_EXTENDED_MASTER_SECRET defined, but not all prerequsites"
#endif

#if defined(MBEDTLS_SSL_TICKET_C) && !defined(MBEDTLS_CIPHER_C)
#error "MBEDTLS_SSL_TICKET_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_CBC_RECORD_SPLITTING) && \
    !defined(MBEDTLS_SSL_PROTO_SSL3) && !defined(MBEDTLS_SSL_PROTO_TLS1)
#error "MBEDTLS_SSL_CBC_RECORD_SPLITTING defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_SSL_SERVER_NAME_INDICATION) && \
        !defined(MBEDTLS_X509_CRT_PARSE_C)
#error "MBEDTLS_SSL_SERVER_NAME_INDICATION defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_THREADING_PTHREAD)
#if !defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_PTHREAD defined, but not all prerequisites"
#endif
#define MBEDTLS_THREADING_IMPL
#endif

#if defined(MBEDTLS_THREADING_ALT)
#if !defined(MBEDTLS_THREADING_C) || defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_ALT defined, but not all prerequisites"
#endif
#define MBEDTLS_THREADING_IMPL
#endif

#if defined(MBEDTLS_THREADING_C) && !defined(MBEDTLS_THREADING_IMPL)
#error "MBEDTLS_THREADING_C defined, single threading implementation required"
#endif
#undef MBEDTLS_THREADING_IMPL

#if defined(MBEDTLS_X509_USE_C) && ( !defined(MBEDTLS_BIGNUM_C) ||  \
    !defined(MBEDTLS_OID_C) || !defined(MBEDTLS_ASN1_PARSE_C) ||      \
    !defined(MBEDTLS_PK_PARSE_C) )
#error "MBEDTLS_X509_USE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CREATE_C) && ( !defined(MBEDTLS_BIGNUM_C) ||  \
    !defined(MBEDTLS_OID_C) || !defined(MBEDTLS_ASN1_WRITE_C) ||       \
    !defined(MBEDTLS_PK_WRITE_C) )
#error "MBEDTLS_X509_CREATE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRT_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRL_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CRL_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_PARSE_C) && ( !defined(MBEDTLS_X509_USE_C) )
#error "MBEDTLS_X509_CSR_PARSE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CRT_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CRT_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_X509_CSR_WRITE_C) && ( !defined(MBEDTLS_X509_CREATE_C) )
#error "MBEDTLS_X509_CSR_WRITE_C defined, but not all prerequisites"
#endif

#if defined(MBEDTLS_HAVE_INT32) && defined(MBEDTLS_HAVE_INT64)
#error "MBEDTLS_HAVE_INT32 and MBEDTLS_HAVE_INT64 cannot be defined simultaneously"
#endif /* MBEDTLS_HAVE_INT32 && MBEDTLS_HAVE_INT64 */

#if ( defined(MBEDTLS_HAVE_INT32) || defined(MBEDTLS_HAVE_INT64) ) && \
    defined(MBEDTLS_HAVE_ASM)
#error "MBEDTLS_HAVE_INT32/MBEDTLS_HAVE_INT64 and MBEDTLS_HAVE_ASM cannot be defined simultaneously"
#endif /* (MBEDTLS_HAVE_INT32 || MBEDTLS_HAVE_INT64) && MBEDTLS_HAVE_ASM */

/*
 * Avoid warning from -pedantic. This is a convenient place for this
 * workaround since this is included by every single file before the
 * #if defined(MBEDTLS_xxx_C) that results in emtpy translation units.
 */
typedef int mbedtls_iso_c_forbids_empty_translation_units;

#endif /* MBEDTLS_CHECK_CONFIG_H */
