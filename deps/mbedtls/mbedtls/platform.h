/**
 * \file platform.h
 *
 * \brief mbed TLS Platform abstraction layer
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
#ifndef MBEDTLS_PLATFORM_H
#define MBEDTLS_PLATFORM_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_HAVE_TIME)
#include "platform_time.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \name SECTION: Module settings
 *
 * The configuration options you can set for this module are in this section.
 * Either change them in config.h or define them on the compiler command line.
 * \{
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if !defined(MBEDTLS_PLATFORM_STD_PRINTF)
#define MBEDTLS_PLATFORM_STD_PRINTF   printf /**< Default printf to use  */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_FPRINTF)
#define MBEDTLS_PLATFORM_STD_FPRINTF fprintf /**< Default fprintf to use */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT)
#define MBEDTLS_PLATFORM_STD_EXIT      exit /**< Default exit to use */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_TIME)
#define MBEDTLS_PLATFORM_STD_TIME       time    /**< Default time to use */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT_SUCCESS)
#define MBEDTLS_PLATFORM_STD_EXIT_SUCCESS  EXIT_SUCCESS /**< Default exit value to use */
#endif
#if !defined(MBEDTLS_PLATFORM_STD_EXIT_FAILURE)
#define MBEDTLS_PLATFORM_STD_EXIT_FAILURE  EXIT_FAILURE /**< Default exit value to use */
#endif

/* \} name SECTION: Module settings */

/*
 * The function pointers for calloc and free
 */
#if defined(MBEDTLS_PLATFORM_MEMORY)
#if defined(MBEDTLS_PLATFORM_FREE_MACRO) && \
    defined(MBEDTLS_PLATFORM_CALLOC_MACRO)
#define mbedtls_free       MBEDTLS_PLATFORM_FREE_MACRO
#define mbedtls_calloc     MBEDTLS_PLATFORM_CALLOC_MACRO
#else
/* For size_t */
#include <stddef.h>
extern void * (*mbedtls_calloc)( size_t n, size_t size );
extern void (*mbedtls_free)( void *ptr );

/**
 * \brief   Set your own memory implementation function pointers
 *
 * \param calloc_func   the calloc function implementation
 * \param free_func     the free function implementation
 *
 * \return              0 if successful
 */
int mbedtls_platform_set_calloc_free( void * (*calloc_func)( size_t, size_t ),
                              void (*free_func)( void * ) );
#endif /* MBEDTLS_PLATFORM_FREE_MACRO && MBEDTLS_PLATFORM_CALLOC_MACRO */
#else /* !MBEDTLS_PLATFORM_MEMORY */
#define mbedtls_free       free
#define mbedtls_calloc     calloc
#endif /* MBEDTLS_PLATFORM_MEMORY && !MBEDTLS_PLATFORM_{FREE,CALLOC}_MACRO */

/*
 * The function pointers for fprintf
 */
#if defined(MBEDTLS_PLATFORM_FPRINTF_ALT)
/* We need FILE * */
#include <stdio.h>
extern int (*mbedtls_fprintf)( FILE *stream, const char *format, ... );

/**
 * \brief   Set your own fprintf function pointer
 *
 * \param fprintf_func   the fprintf function implementation
 *
 * \return              0
 */
int mbedtls_platform_set_fprintf( int (*fprintf_func)( FILE *stream, const char *,
                                               ... ) );
#else
#if defined(MBEDTLS_PLATFORM_FPRINTF_MACRO)
#define mbedtls_fprintf    MBEDTLS_PLATFORM_FPRINTF_MACRO
#else
#define mbedtls_fprintf    fprintf
#endif /* MBEDTLS_PLATFORM_FPRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_FPRINTF_ALT */

/*
 * The function pointers for printf
 */
#if defined(MBEDTLS_PLATFORM_PRINTF_ALT)
extern int (*mbedtls_printf)( const char *format, ... );

/**
 * \brief   Set your own printf function pointer
 *
 * \param printf_func   the printf function implementation
 *
 * \return              0
 */
int mbedtls_platform_set_printf( int (*printf_func)( const char *, ... ) );
#else /* !MBEDTLS_PLATFORM_PRINTF_ALT */
#if defined(MBEDTLS_PLATFORM_PRINTF_MACRO)
#define mbedtls_printf     MBEDTLS_PLATFORM_PRINTF_MACRO
#else
#define mbedtls_printf     printf
#endif /* MBEDTLS_PLATFORM_PRINTF_MACRO */
#endif /* MBEDTLS_PLATFORM_PRINTF_ALT */

/*
 * The function pointers for exit
 */
#if defined(MBEDTLS_PLATFORM_EXIT_ALT)
extern void (*mbedtls_exit)( int status );

/**
 * \brief   Set your own exit function pointer
 *
 * \param exit_func   the exit function implementation
 *
 * \return              0
 */
int mbedtls_platform_set_exit( void (*exit_func)( int status ) );
#else
#if defined(MBEDTLS_PLATFORM_EXIT_MACRO)
#define mbedtls_exit   MBEDTLS_PLATFORM_EXIT_MACRO
#else
#define mbedtls_exit   exit
#endif /* MBEDTLS_PLATFORM_EXIT_MACRO */
#endif /* MBEDTLS_PLATFORM_EXIT_ALT */

/*
 * The default exit values
 */
#if defined(MBEDTLS_PLATFORM_STD_EXIT_SUCCESS)
#define MBEDTLS_EXIT_SUCCESS MBEDTLS_PLATFORM_STD_EXIT_SUCCESS
#else
#define MBEDTLS_EXIT_SUCCESS 0
#endif
#if defined(MBEDTLS_PLATFORM_STD_EXIT_FAILURE)
#define MBEDTLS_EXIT_FAILURE MBEDTLS_PLATFORM_STD_EXIT_FAILURE
#else
#define MBEDTLS_EXIT_FAILURE 1
#endif

#if !defined(MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT)

/**
 * \brief   Platform context structure
 *
 * \note    This structure may be used to assist platform-specific
 *          setup/teardown operations.
 */
typedef struct {
    char dummy; /**< Placeholder member as empty structs are not portable */
}
mbedtls_platform_context;

#else
#include "platform_alt.h"
#endif /* !MBEDTLS_PLATFORM_SETUP_TEARDOWN_ALT */

/**
 * \brief   Perform any platform initialisation operations
 *
 * \param   ctx     mbed TLS context
 *
 * \return  0 if successful
 *
 * \note    This function is intended to allow platform specific initialisation,
 *          and should be called before any other library functions. Its
 *          implementation is platform specific, and by default, unless platform
 *          specific code is provided, it does nothing.
 *
 *          Its use and whether its necessary to be called is dependent on the
 *          platform.
 */
int mbedtls_platform_setup( mbedtls_platform_context *ctx );
/**
 * \brief   Perform any platform teardown operations
 *
 * \param   ctx     mbed TLS context
 *
 * \note    This function should be called after every other mbed TLS module has
 *          been correctly freed using the appropriate free function.
 *          Its implementation is platform specific, and by default, unless
 *          platform specific code is provided, it does nothing.
 *
 *          Its use and whether its necessary to be called is dependent on the
 *          platform.
 */
void mbedtls_platform_teardown( mbedtls_platform_context *ctx );

#ifdef __cplusplus
}
#endif

#endif /* platform.h */
