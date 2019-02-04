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

#ifndef __VC_INCLUDE_COMMON_H__
#define __VC_INCLUDE_COMMON_H__

#include "interface/vcos/vcos_stdint.h"
#include "interface/vctypes/vc_image_types.h"

#if defined(__HIGHC__) && defined(_VIDEOCORE) && !defined(_I386)
// __HIGHC__ is only available with MW
// The scvc plugins are compiled (bizarrely) on an x86 with _VIDEOCORE set!
#include <vc/intrinsics.h>
#endif

#ifdef __COVERITY__
#ifndef _Rarely
#define _Rarely(x) (x)
#endif
#ifndef _Usually
#define _Usually(x) (x)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __SYMBIAN32__
# ifndef INLINE
#  define INLINE __inline
# endif

/* Align a pointer/integer by rounding up/down */
#define ALIGN_DOWN(p, n)   ((uint32_t)(p) - ( (uint32_t)(p) % (uint32_t)(n) ))
#define ALIGN_UP(p, n)     ALIGN_DOWN((uint32_t)(p) + (uint32_t)(n) - 1, (n))

#elif defined (VCMODS_LCC)
#include <limits.h>

#elif !defined(__KERNEL__)
#include <limits.h>

#endif

/*}}}*/

/* Fixed-point types */
typedef unsigned short uint8p8_t;
typedef signed short sint8p8_t;
typedef unsigned short uint4p12_t;
typedef signed short sint4p12_t;
typedef signed short sint0p16_t;
typedef signed char sint8p0_t;
typedef unsigned char uint0p8_t;
typedef signed long int24p8_t;

/*{{{ Common typedefs */

typedef enum bool_e
{
   VC_FALSE = 0,
   VC_TRUE = 1,
} VC_BOOL_T;

#ifndef bool_t
#define bool_t VC_BOOL_T
#endif

/*}}}*/

/*{{{ Common macros */

/* Align a pointer/integer by rounding up/down */
#define ALIGN_DOWN(p, n)   ((uintptr_t)(p) - ( (uintptr_t)(p) % (uintptr_t)(n) ))
#define ALIGN_UP(p, n)     ALIGN_DOWN((uintptr_t)(p) + (uintptr_t)(n) - 1, (n))

#define CLIP(lower, n, upper) _min((upper), _max((lower), (n)))

/*}}}*/

/*{{{ Debugging and profiling macros */

#if 0
/* There's already an assert_once in <logging/logging.h> */
#ifdef DEBUG
#define assert_once(x) \
   { \
      static uint8_t ignore = 0; \
      if(!ignore) \
      { \
         assert(x); \
         ignore++; \
      } \
   }
#else
#define assert_once(x) (void)0
#endif
#endif /* 0 */

#if defined(__HIGHC__) && !defined(NDEBUG)
/* HighC lacks a __FUNCTION__ preproc symbol... :( */
#define profile_rename(name) _ASM(".global " name "\n" name ":\n")
#else
#define profile_rename(name) (void)0
#endif

/*}}}*/
#ifdef __cplusplus
 }
#endif
#endif /* __VCINCLUDE_COMMON_H__ */
