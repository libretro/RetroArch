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

#ifndef VCOS_STDINT_H
#define VCOS_STDINT_H

/** \file
 * Attempt to provide the types defined in stdint.h.
 *
 * Except for use with lcc, this simply includes stdint.h, which should find
 * the system/toolchain version if present, otherwise falling back to the
 * version in interface/vcos/<platform>.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined (VCMODS_LCC)

#include <limits.h>

typedef signed   char      int8_t;
typedef unsigned char      uint8_t;

typedef signed   short     int16_t;
typedef unsigned short     uint16_t;

typedef signed   long      int32_t;
typedef unsigned long      uint32_t;

typedef int32_t            intptr_t;
typedef uint32_t           uintptr_t;

typedef int32_t            intmax_t;
typedef uint32_t           uintmax_t;

typedef int8_t             int_least8_t;
typedef int16_t            int_least16_t;
typedef int32_t            int_least32_t;
typedef uint8_t            uint_least8_t;
typedef uint16_t           uint_least16_t;
typedef uint32_t           uint_least32_t;

#define INT8_MIN SCHAR_MIN
#define INT8_MAX SCHAR_MAX
#define UINT8_MAX UCHAR_MAX

#define INT16_MIN SHRT_MIN
#define INT16_MAX SHRT_MAX
#define UINT16_MAX USHRT_MAX

#define INT32_MIN LONG_MIN
#define INT32_MAX LONG_MAX
#define UINT32_MAX ULONG_MAX

#define INTPTR_MIN INT32_MIN
#define INTPTR_MAX INT32_MAX
#define UINTPTR_MAX UINT32_MAX

#define INTMAX_MIN INT32_MIN
#define INTMAX_MAX INT32_MAX
#define UINTMAX_MAX UINT32_MAX

/* N.B. 64-bit integer types are not currently supported by lcc.
 * However, these symbols are referenced in header files included by files
 * compiled by lcc for VCE, so removing them would break the build.
 * The solution here then is to define them, as the correct size, but in a
 * way that should make them unusable in normal arithmetic operations.
 */
typedef struct { uint32_t a; uint32_t b; }  int64_t;
typedef struct { uint32_t a; uint32_t b; } uint64_t;

#else

#include <stdint.h>

#endif

#ifdef __cplusplus
}
#endif
#endif /* VCOS_STDINT_H */
