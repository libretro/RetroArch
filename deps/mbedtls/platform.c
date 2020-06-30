/*
 *  Platform abstraction layer
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

#if defined(MBEDTLS_PLATFORM_C)

#include "mbedtls/platform.h"

#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
#if !defined(MBEDTLS_PLATFORM_STD_PRINTF)
/*
 * Make dummy function to prevent NULL pointer dereferences
 */
static int platform_printf_uninit( const char *format, ... )
{
    ((void) format);
    return( 0 );
}

#define MBEDTLS_PLATFORM_STD_PRINTF    platform_printf_uninit
#endif /* !MBEDTLS_PLATFORM_STD_PRINTF */

int (*mbedtls_printf)( const char *, ... ) = MBEDTLS_PLATFORM_STD_PRINTF;

int mbedtls_platform_set_printf( int (*printf_func)( const char *, ... ) )
{
    mbedtls_printf = printf_func;
    return( 0 );
}
#endif /* MBEDTLS_PLATFORM_PRINTF_ALT */

#if defined(MBEDTLS_PLATFORM_FPRINTF_ALT)
#if !defined(MBEDTLS_PLATFORM_STD_FPRINTF)
/*
 * Make dummy function to prevent NULL pointer dereferences
 */
static int platform_fprintf_uninit( FILE *stream, const char *format, ... )
{
    ((void) stream);
    ((void) format);
    return( 0 );
}

#define MBEDTLS_PLATFORM_STD_FPRINTF   platform_fprintf_uninit
#endif /* !MBEDTLS_PLATFORM_STD_FPRINTF */

int (*mbedtls_fprintf)( FILE *, const char *, ... ) =
                                        MBEDTLS_PLATFORM_STD_FPRINTF;

int mbedtls_platform_set_fprintf( int (*fprintf_func)( FILE *, const char *, ... ) )
{
    mbedtls_fprintf = fprintf_func;
    return( 0 );
}
#endif /* MBEDTLS_PLATFORM_FPRINTF_ALT */

#if defined(MBEDTLS_PLATFORM_EXIT_ALT)
#if !defined(MBEDTLS_PLATFORM_STD_EXIT)
/*
 * Make dummy function to prevent NULL pointer dereferences
 */
static void platform_exit_uninit( int status )
{
    ((void) status);
}

#define MBEDTLS_PLATFORM_STD_EXIT   platform_exit_uninit
#endif /* !MBEDTLS_PLATFORM_STD_EXIT */

void (*mbedtls_exit)( int status ) = MBEDTLS_PLATFORM_STD_EXIT;

int mbedtls_platform_set_exit( void (*exit_func)( int status ) )
{
    mbedtls_exit = exit_func;
    return( 0 );
}
#endif /* MBEDTLS_PLATFORM_EXIT_ALT */

#if defined(MBEDTLS_HAVE_TIME)

#if defined(MBEDTLS_PLATFORM_TIME_ALT)
#if !defined(MBEDTLS_PLATFORM_STD_TIME)
/*
 * Make dummy function to prevent NULL pointer dereferences
 */
static mbedtls_time_t platform_time_uninit( mbedtls_time_t* timer )
{
    ((void) timer);
    return( 0 );
}

#define MBEDTLS_PLATFORM_STD_TIME   platform_time_uninit
#endif /* !MBEDTLS_PLATFORM_STD_TIME */

mbedtls_time_t (*mbedtls_time)( mbedtls_time_t* timer ) = MBEDTLS_PLATFORM_STD_TIME;

int mbedtls_platform_set_time( mbedtls_time_t (*time_func)( mbedtls_time_t* timer ) )
{
    mbedtls_time = time_func;
    return( 0 );
}
#endif /* MBEDTLS_PLATFORM_TIME_ALT */

#endif /* MBEDTLS_HAVE_TIME */

#if !defined(MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT)
/*
 * Placeholder platform setup that does nothing by default
 */
int mbedtls_platform_setup( mbedtls_platform_context *ctx )
{
    (void)ctx;

    return( 0 );
}

/*
 * Placeholder platform teardown that does nothing by default
 */
void mbedtls_platform_teardown( mbedtls_platform_context *ctx )
{
    (void)ctx;
}
#endif /* MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT */

#endif /* MBEDTLS_PLATFORM_C */
