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

/** \file mmal_common.h
 * Multi-Media Abstraction Layer - Common definitions
 */

#ifndef MMAL_COMMON_H
#define MMAL_COMMON_H

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <interface/vcos/vcos.h>

/* C99 64bits integers */
#ifndef INT64_C
# define INT64_C(value) value##LL
# define UINT64_C(value) value##ULL
#endif

#define MMAL_TSRING(s) #s
#define MMAL_TO_STRING(s) MMAL_TSRING(s)

#define MMAL_COUNTOF(x) (sizeof((x))/sizeof((x)[0]))
#define MMAL_MIN(a,b) ((a)<(b)?(a):(b))
#define MMAL_MAX(a,b) ((a)<(b)?(b):(a))

/* FIXME: should be different for big endian */
#define MMAL_FOURCC(a,b,c,d) ((a) | (b << 8) | (c << 16) | (d << 24))
#define MMAL_PARAM_UNUSED(a) (void)(a)
#define MMAL_MAGIC MMAL_FOURCC('m','m','a','l')

typedef int32_t MMAL_BOOL_T;
#define MMAL_FALSE   0
#define MMAL_TRUE    1

typedef struct MMAL_CORE_STATISTICS_T
{
   uint32_t buffer_count;        /**< Total buffer count on this port */
   uint32_t first_buffer_time;   /**< Time (us) of first buffer seen on this port */
   uint32_t last_buffer_time;    /**< Time (us) of most recently buffer on this port */
   uint32_t max_delay;           /**< Max delay (us) between buffers, ignoring first few frames */
} MMAL_CORE_STATISTICS_T;

/** Statistics collected by the core on all ports, if enabled in the build.
 */
typedef struct MMAL_CORE_PORT_STATISTICS_T
{
   MMAL_CORE_STATISTICS_T rx;
   MMAL_CORE_STATISTICS_T tx;
} MMAL_CORE_PORT_STATISTICS_T;

/** Unsigned 16.16 fixed point value, also known as Q16.16 */
typedef uint32_t MMAL_FIXED_16_16_T;

#endif /* MMAL_COMMON_H */
