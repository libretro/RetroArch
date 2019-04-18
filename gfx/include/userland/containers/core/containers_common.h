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
#ifndef VC_CONTAINERS_COMMON_H
#define VC_CONTAINERS_COMMON_H

/** \file containers_common.h
 * Common definitions for containers infrastructure
 */

#ifndef ENABLE_CONTAINERS_STANDALONE
# include "vcos.h"
# define vc_container_assert(a) vcos_assert(a)
#else
# include "assert.h"
# define vc_container_assert(a) assert(a)
#endif /* ENABLE_CONTAINERS_STANDALONE */

#ifndef countof
# define countof(a) (sizeof(a) / sizeof(a[0]))
#endif

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifdef _MSC_VER
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif

#define STATIC_INLINE static __inline
#define VC_CONTAINER_PARAM_UNUSED(a) (void)(a)

#if defined(__HIGHC__) && !defined(strcasecmp)
# define strcasecmp(a,b) _stricmp(a,b)
#endif

#if defined(__GNUC__) && (__GNUC__ > 2)
# define VC_CONTAINER_CONSTRUCTOR(func) void __attribute__((constructor,used)) func(void)
# define VC_CONTAINER_DESTRUCTOR(func) void __attribute__((destructor,used)) func(void)
#else
# define VC_CONTAINER_CONSTRUCTOR(func) void func(void)
# define VC_CONTAINER_DESTRUCTOR(func) void func(void)
#endif

#endif /* VC_CONTAINERS_COMMON_H */
