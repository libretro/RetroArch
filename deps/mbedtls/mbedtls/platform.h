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

#include <time.h>

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

/* For size_t */
#include <stddef.h>

/*
 * The default exit values
 */
#define MBEDTLS_EXIT_SUCCESS 0
#define MBEDTLS_EXIT_FAILURE 1

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
