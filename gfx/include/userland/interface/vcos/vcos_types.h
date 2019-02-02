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

/*=============================================================================
VideoCore OS Abstraction Layer - basic types
=============================================================================*/

#ifndef VCOS_TYPES_H
#define VCOS_TYPES_H

#define VCOS_VERSION   1

#include <stddef.h>
#if defined(__unix__) && !defined(__ANDROID__)
#include "interface/vcos/pthreads/vcos_platform_types.h"
#else
#include "vcos_platform_types.h"
#endif
#include "interface/vcos/vcos_attr.h"

#if !defined(VCOSPRE_) || !defined(VCOSPOST_)
#error VCOSPRE_ or VCOSPOST_ not defined!
#endif

/* Redefine these here; this means that existing header files can carry on
 * using the VCHPOST/VCHPRE macros rather than having huge changes, which
 * could cause nasty merge problems.
 */
#ifndef VCHPOST_
#define VCHPOST_ VCOSPOST_
#endif
#ifndef VCHPRE_
#define VCHPRE_  VCOSPRE_
#endif

/** Entry function for a lowlevel thread.
  *
  * Returns void for consistency with Nucleus/ThreadX.
  */
typedef void (*VCOS_LLTHREAD_ENTRY_FN_T)(void *);

/** Thread entry point. Returns a void* for consistency
  * with pthreads.
  */
typedef void *(*VCOS_THREAD_ENTRY_FN_T)(void*);


/* Error return codes - chosen to be similar to errno values */
typedef enum
{
   VCOS_SUCCESS,
   VCOS_EAGAIN,
   VCOS_ENOENT,
   VCOS_ENOSPC,
   VCOS_EINVAL,
   VCOS_EACCESS,
   VCOS_ENOMEM,
   VCOS_ENOSYS,
   VCOS_EEXIST,
   VCOS_ENXIO,
   VCOS_EINTR
} VCOS_STATUS_T;

/* Some compilers (MetaWare) won't inline with -g turned on, which then results
 * in a lot of code bloat. To overcome this, inline functions are forward declared
 * with the prefix VCOS_INLINE_DECL, and implemented with the prefix VCOS_INLINE_IMPL.
 *
 * That then means that in a release build, "static inline" can be used in the obvious
 * way, but in a debug build the implementations can be skipped in all but one file,
 * by using VCOS_INLINE_BODIES.
 *
 * VCOS_INLINE_DECL - put this at the start of an inline forward declaration of a VCOS
 * function.
 *
 * VCOS_INLINE_IMPL - put this at the start of an inlined implementation of a VCOS
 * function.
 *
 */

/* VCOS_EXPORT - it turns out that in some circumstances we need the implementation of
 * a function even if it is usually inlined.
 *
 * In particular, if we have a codec that is usually provided in object form, if it
 * was built for a debug build it will be full of calls to vcos_XXX(). If this is used
 * in a *release* build, then there won't be any of these calls around in the main image
 * as they will all have been inlined. The problem also exists for vcos functions called
 * from assembler.
 *
 * VCOS_EXPORT ensures that the named function will be emitted as a regular (not static-inline)
 * function inside vcos_<platform>.c so that it can be linked against. Doing this for every
 * VCOS function would be a bit code-bloat-tastic, so it is only done for those that need it.
 *
 */

#ifdef __cplusplus
#define _VCOS_INLINE inline
#else
#define _VCOS_INLINE __inline
#endif

#if defined(NDEBUG)

#ifdef __GNUC__
# define VCOS_INLINE_DECL extern __inline__
# define VCOS_INLINE_IMPL static __inline__
#else
# define VCOS_INLINE_DECL static _VCOS_INLINE   /* declare a func */
# define VCOS_INLINE_IMPL static _VCOS_INLINE   /* implement a func inline */
#endif

# if defined(VCOS_WANT_IMPL)
#  define VCOS_EXPORT
# else
#  define VCOS_EXPORT VCOS_INLINE_IMPL
# endif /* VCOS_WANT_IMPL */

#define VCOS_INLINE_BODIES

#else /* NDEBUG */

#if !defined(VCOS_INLINE_DECL)
   #define VCOS_INLINE_DECL extern
#endif
#if !defined(VCOS_INLINE_IMPL)
   #define VCOS_INLINE_IMPL
#endif
#define VCOS_EXPORT VCOS_INLINE_IMPL
#endif

#define VCOS_STATIC_INLINE static _VCOS_INLINE

#if defined(__HIGHC__) || defined(__HIGHC_ANSI__)
#define _VCOS_METAWARE
#endif

/** It seems that __FUNCTION__ isn't standard!
  */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2 || defined(__VIDEOCORE__)
#  define VCOS_FUNCTION __FUNCTION__
# else
#  define VCOS_FUNCTION "<unknown>"
# endif
#else
# define VCOS_FUNCTION __func__
#endif

#define _VCOS_MS_PER_TICK (1000/VCOS_TICKS_PER_SECOND)

/* Convert a number of milliseconds to a tick count. Internal use only - fails to
 * convert VCOS_SUSPEND correctly.
 */
#define _VCOS_MS_TO_TICKS(ms) (((ms)+_VCOS_MS_PER_TICK-1)/_VCOS_MS_PER_TICK)

#define VCOS_TICKS_TO_MS(ticks) ((ticks) * _VCOS_MS_PER_TICK)

/** VCOS version of DATESTR, from pcdisk.h. Used by the hostreq service.
 */
typedef struct vcos_datestr
{
   uint8_t       cmsec;              /**< Centesimal mili second */
   uint16_t      date;               /**< Date */
   uint16_t      time;               /**< Time */

} VCOS_DATESTR;

/* Compile-time assert - declares invalid array length if condition
 * not met, or array of length one if OK.
 */
#define VCOS_CASSERT(e) extern char vcos_compile_time_check[1/(e)]

#define vcos_min(x,y) ((x) < (y) ? (x) : (y))
#define vcos_max(x,y) ((x) > (y) ? (x) : (y))

/** Return the count of an array. FIXME: under gcc we could make
 * this report an error for pointers using __builtin_types_compatible().
 */
#define vcos_countof(x) (sizeof((x)) / sizeof((x)[0]))

/* for backward compatibility */
#define countof(x) (sizeof((x)) / sizeof((x)[0]))

#define VCOS_ALIGN_DOWN(p,n) (((ptrdiff_t)(p)) & ~((n)-1))
#define VCOS_ALIGN_UP(p,n) VCOS_ALIGN_DOWN((ptrdiff_t)(p)+(n)-1,(n))

#ifdef _MSC_VER
   #define vcos_alignof(T) __alignof(T)
#elif defined(__GNUC__)
   #define vcos_alignof(T) __alignof__(T)
#else
   #define vcos_alignof(T) (sizeof(struct { T t; char ch; }) - sizeof(T))
#endif

/** bool_t is not a POSIX type so cannot rely on it. Define it here.
  * It's not even defined in stdbool.h.
  */
typedef int32_t vcos_bool_t;
typedef int32_t vcos_fourcc_t;

#define VCOS_FALSE   0
#define VCOS_TRUE    (!VCOS_FALSE)

/** Mark unused arguments to keep compilers quiet */
#define vcos_unused(x) (void)(x)

/** For backward compatibility */
typedef vcos_fourcc_t fourcc_t;
typedef vcos_fourcc_t FOURCC_T;

#ifdef __cplusplus
#define VCOS_EXTERN_C_BEGIN extern "C" {
#define VCOS_EXTERN_C_END }
#else
#define VCOS_EXTERN_C_BEGIN
#define VCOS_EXTERN_C_END
#endif

/** Define a function as a weak alias to another function.
 * @param ret_type     Function return type.
 * @param alias_name   Name of the alias.
 * @param param_list   Function parameter list, including the parentheses.
 * @param target_name  Target function (bare function name, not a string).
 */
#if defined(__GNUC__) || defined(_VCOS_METAWARE)
  /* N.B. gcc allows __attribute__ after parameter list, but hcvc seems to silently ignore it. */
# define VCOS_WEAK_ALIAS(ret_type, alias_name, param_list, target_name) \
   __attribute__ ((weak, alias(#target_name))) ret_type alias_name param_list
#else
# define VCOS_WEAK_ALIAS(ret_type, alias, params, target)  VCOS_CASSERT(0)
#endif

/** Define a function as a weak alias to another function, specified as a string.
 * @param ret_type     Function return type.
 * @param alias_name   Name of the alias.
 * @param param_list   Function parameter list, including the parentheses.
 * @param target_name  Target function name as a string.
 * @note Prefer the use of VCOS_WEAK_ALIAS - it is likely to be more portable.
 *       Only use VCOS_WEAK_ALIAS_STR if you need to do pre-processor mangling of the target
 *       symbol.
 */
#if defined(__GNUC__) || defined(_VCOS_METAWARE)
  /* N.B. gcc allows __attribute__ after parameter list, but hcvc seems to silently ignore it. */
# define VCOS_WEAK_ALIAS_STR(ret_type, alias_name, param_list, target_name) \
   __attribute__ ((weak, alias(target_name))) ret_type alias_name param_list
#else
# define VCOS_WEAK_ALIAS_STR(ret_type, alias, params, target)  VCOS_CASSERT(0)
#endif

#if defined(__GNUC__)
#define VCOS_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define VCOS_DEPRECATED(msg)
#endif

#endif
