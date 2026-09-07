/**
 * \file int_util.h
 *
 * \brief Byte-order-explicit integer load/store helpers shared by the
 *        mbedtls sources.
 *
 * These used to be open-coded once per .c file as GET_UINT32_BE and
 * friends, each copy wrapped in an #ifndef guard. That is unsafe in a
 * unity build: the guard makes whichever definition is seen first win,
 * so a file silently compiles against a foreign definition instead of
 * its own. Keeping a single definition here removes both the guard and
 * the duplication, and the MBEDTLS_ prefix keeps the names out of the
 * way of the rest of the translation unit.
 */
/*
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
#ifndef MBEDTLS_INT_UTIL_H
#define MBEDTLS_INT_UTIL_H

#include <stdint.h>

/*
 * 32-bit integer manipulation macros (big endian)
 */
#define MBEDTLS_GET_UINT32_BE(n,b,i)                        \
do {                                                        \
    (n) = ( (uint32_t) (b)[(i)    ] << 24 )                 \
        | ( (uint32_t) (b)[(i) + 1] << 16 )                 \
        | ( (uint32_t) (b)[(i) + 2] <<  8 )                 \
        | ( (uint32_t) (b)[(i) + 3]       );                \
} while( 0 )

#define MBEDTLS_PUT_UINT32_BE(n,b,i)                        \
do {                                                        \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );           \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );           \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );           \
    (b)[(i) + 3] = (unsigned char) ( (n)       );           \
} while( 0 )

/*
 * 32-bit integer manipulation macros (little endian)
 */
#define MBEDTLS_GET_UINT32_LE(n,b,i)                        \
do {                                                        \
    (n) = ( (uint32_t) (b)[(i)    ]       )                 \
        | ( (uint32_t) (b)[(i) + 1] <<  8 )                 \
        | ( (uint32_t) (b)[(i) + 2] << 16 )                 \
        | ( (uint32_t) (b)[(i) + 3] << 24 );                \
} while( 0 )

#define MBEDTLS_PUT_UINT32_LE(n,b,i)                        \
do {                                                        \
    (b)[(i)    ] = (unsigned char) ( ( (n)       ) & 0xFF );\
    (b)[(i) + 1] = (unsigned char) ( ( (n) >>  8 ) & 0xFF );\
    (b)[(i) + 2] = (unsigned char) ( ( (n) >> 16 ) & 0xFF );\
    (b)[(i) + 3] = (unsigned char) ( ( (n) >> 24 ) & 0xFF );\
} while( 0 )

/*
 * 64-bit integer manipulation macros (big endian)
 */
#define MBEDTLS_GET_UINT64_BE(n,b,i)                        \
do {                                                        \
    (n) = ( (uint64_t) (b)[(i)    ] << 56 )                 \
        | ( (uint64_t) (b)[(i) + 1] << 48 )                 \
        | ( (uint64_t) (b)[(i) + 2] << 40 )                 \
        | ( (uint64_t) (b)[(i) + 3] << 32 )                 \
        | ( (uint64_t) (b)[(i) + 4] << 24 )                 \
        | ( (uint64_t) (b)[(i) + 5] << 16 )                 \
        | ( (uint64_t) (b)[(i) + 6] <<  8 )                 \
        | ( (uint64_t) (b)[(i) + 7]       );                \
} while( 0 )

#define MBEDTLS_PUT_UINT64_BE(n,b,i)                        \
do {                                                        \
    (b)[(i)    ] = (unsigned char) ( (n) >> 56 );           \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 48 );           \
    (b)[(i) + 2] = (unsigned char) ( (n) >> 40 );           \
    (b)[(i) + 3] = (unsigned char) ( (n) >> 32 );           \
    (b)[(i) + 4] = (unsigned char) ( (n) >> 24 );           \
    (b)[(i) + 5] = (unsigned char) ( (n) >> 16 );           \
    (b)[(i) + 6] = (unsigned char) ( (n) >>  8 );           \
    (b)[(i) + 7] = (unsigned char) ( (n)       );           \
} while( 0 )

#endif /* MBEDTLS_INT_UTIL_H */
