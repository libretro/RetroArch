/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef DRIVER_H_
#define DRIVER_H_

#include "vcinclude/common.h"

/******************************************************************************
 Global Macros
 *****************************************************************************/

// build the driver function table names through macro magic
#define DRIVER_CONCATENATE(a,b) a##b

//concat the driver name and the _get_func_table extension
#define DRIVER_NAME(x) DRIVER_CONCATENATE( x, _get_func_table )

/******************************************************************************
 Global Defines
 *****************************************************************************/

typedef enum
{
   DRIVER_FLAGS_DUMMY

   //   DRIVER_FLAGS_SUPPORTS_CORE_FREQ_CHANGE = 0x1,
   //   DRIVER_FLAGS_SUPPORTS_RUN_DOMAIN_CHANGE = 0x2,
   //   DRIVER_FLAGS_SUPPORTS_SUSPEND_RESUME = 0x4

} DRIVER_FLAGS_T;

/******************************************************************************
 Function defines
 *****************************************************************************/

//Generic handle passed used by all drivers
typedef struct opaque_driver_handle_t *DRIVER_HANDLE_T;

//Routine to initialise a driver
typedef int32_t (*DRIVER_INIT_T)( void );

//Routine to shutdown a driver
typedef int32_t (*DRIVER_EXIT_T)( void );

//Routine to return a drivers info (name, version etc..)
typedef int32_t (*DRIVER_INFO_T)(const char **driver_name,
                                 uint32_t *version_major,
                                 uint32_t *version_minor,
                                 DRIVER_FLAGS_T *flags );

//Routine to open a driver.
typedef int32_t (*DRIVER_OPEN_T)(const void *params,
                                 DRIVER_HANDLE_T *handle );

//Routine to close a driver
typedef int32_t (*DRIVER_CLOSE_T)( const DRIVER_HANDLE_T handle );

//Test routine to open a test driver
typedef int32_t (*DRIVER_TEST_INIT_T)( DRIVER_HANDLE_T *handle );

//Test routine to close a test driver
typedef int32_t (*DRIVER_TEST_EXIT_T)( const DRIVER_HANDLE_T handle );

/******************************************************************************
 Driver struct definition
 *****************************************************************************/

/* Basic driver structure, as implemented by all device drivers */
#define COMMON_DRIVER_API(param1) \
   /*Used to be param1... but no drivers were using multiple params and MSVC doesn't like vararg macros*/ \
   /*Function to initialize the driver. This is used at the start of day to //initialize the driver*/ \
   DRIVER_INIT_T   init; \
   \
   /*Routine to shutdown a driver*/ \
   DRIVER_EXIT_T   exit; \
   \
   /*Function to get the driver name + version*/ \
   DRIVER_INFO_T   info; \
   \
   /*Function to open an instance of the driver. Takes in option parameters, //defined per driver.*/ \
   /*Returns a handle to the open driver and a success code*/ \
   int32_t (*open)(param1, \
                   DRIVER_HANDLE_T *handle ); \
   \
   /*Function to close a driver instance*/ \
   /*Returns success code*/ \
   DRIVER_CLOSE_T   close;

typedef struct
{
   //just include the basic driver api
   COMMON_DRIVER_API(void const *unused)

} DRIVER_T;

/******************************************************************************
 Test Driver struct definition
 *****************************************************************************/

/* Test driver structure, implemented by all (optional) test drivers */

#define COMMON_DRIVER_TEST_API \
   /*Function used to tell a driver that the core freq is about to change*/ \
   /*Returns success code (0 if its ok to change the clock)*/ \
   DRIVER_TEST_INIT_T   test_init;\
   \
   /*Function used to tell a driver if a power domain is about to change*/ \
   /*Returns success code (0 if its ok to change the power domains)*/ \
   DRIVER_TEST_EXIT_T   test_exit;

typedef struct
{
   //just include the test driver api
   COMMON_DRIVER_TEST_API

} DRIVER_TEST_T;

#define VCFW_AUTO_REGISTER_DRIVER(type, name)      \
   pragma Data(LIT, ".drivers");                   \
   static const type * const name##_ptr = &name;   \
   pragma Data;

#define VCFW_AUTO_REGISTER_BASE_DRIVER(type, name) \
   pragma Data(LIT, ".drivers_base");              \
   static const type * const name##_ptr = &name;   \
   pragma Data;

#endif // DRIVER_H_
