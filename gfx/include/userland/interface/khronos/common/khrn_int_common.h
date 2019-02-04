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

#ifndef KHRN_INT_COMMON_H
#define KHRN_INT_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif\

#include "helpers/v3d/v3d_ver.h"

#define VC_KHRN_VERSION 1
//#define KHRN_NOT_REALLY_DUALCORE   // Use dual core codebase but switch master thread to vpu1
//#define KHRN_SIMPLE_MULTISAMPLE
//#define USE_CTRL_FOR_DATA

//#define GLXX_FORCE_MULTISAMPLE

//#define KHRN_COMMAND_MODE_DISPLAY    /* Platforms where we need to submit updates even for single-buffered surfaces */

/* As BCG runs from cached memory, all allocations have to be the max of the CPU cache (calculated in the platform layer) and
   the size of the cache line on the L3 */
#define BCG_GCACHE_LINE_SIZE        256

#ifdef _VIDEOCORE
   #define KHRN_VECTOR_CORE            /* have a vector core for image processing operations */
   #define KHRN_HW_KICK_POWERMAN       /* khrn_hw_kick() kicks the clock using powerman */
#endif

#if defined(SIMPENROSE) || defined(KHRN_CARBON)
   /* for simplicity and determinism, the driver is single threaded when running
    * on simpenrose and carbon */
   #define KHRN_SINGLE_THREADED
#endif

#ifdef KHRN_SINGLE_THREADED
   #define KHRN_LLAT_NO_THREAD
   #define KHRN_WORKER_USE_LLAT
   #define EGL_DISP_USE_LLAT
#else
   #if !VCOS_HAVE_RTOS
      #define KHRN_WORKER_USE_LLAT
   #endif
   #define EGL_DISP_USE_LLAT
#endif

#define KHRN_LLAT_OTHER_CORE
#ifndef V3D_LEAN
   #define KHRN_WORKER_OTHER_CORE
#endif

#if defined(ANDROID)
#define GL_GET_ERROR_ASYNC /* enabled with property brcm.graphics.async_errors "true" */
#endif

#if defined(ANDROID)
#define KHDISPATCH_WORKSPACE_READAHEAD_BUFFERS 15 /* only VCHIQ */
#else
#define KHDISPATCH_WORKSPACE_READAHEAD_BUFFERS 0
#endif
#define KHDISPATCH_WORKSPACE_BUFFERS (KHDISPATCH_WORKSPACE_READAHEAD_BUFFERS + 1)
#if defined(RPC_DIRECT) || defined(ANDROID)
#include <limits.h>
#define KHDISPATCH_WORKSPACE_SIZE (0x200000 / KHDISPATCH_WORKSPACE_BUFFERS)
#else
#define KHDISPATCH_WORKSPACE_SIZE ((1024 * 1024) / KHDISPATCH_WORKSPACE_BUFFERS) /* should be a multiple of 16, todo: how big does this need to be? (vg needs 8kB) */
#endif

#define KHDISPATCH_CTRL_THRESHOLD 2032
/* todo: use v3d_ver.h stuff... */

#ifdef __BCM2708A0__
#define WORKAROUND_HW1297
#define WORKAROUND_HW1451
#define WORKAROUND_HW1632
#define WORKAROUND_HW1637
#define WORKAROUND_HW2038
#define WORKAROUND_HW2136
#define WORKAROUND_HW2187
#define WORKAROUND_HW2366
#define WORKAROUND_HW2384
#define WORKAROUND_HW2422
#define WORKAROUND_HW2479
#define WORKAROUND_HW2487
#define WORKAROUND_HW2488
#define WORKAROUND_HW2522
#define WORKAROUND_HW2781
#define KHRN_HW_SINGLE_TEXTURE_UNIT /* Only single texture unit available */
#endif
#define WORKAROUND_HW2116
#define WORKAROUND_HW2806
#define WORKAROUND_HW2885
#define WORKAROUND_HW2903
#define WORKAROUND_HW2905
#define WORKAROUND_HW2924
#if !defined(KHRN_CARBON) /* hw-2959 fixed in latest rtl... */
#define WORKAROUND_HW2959
#define WORKAROUND_HW2989
#endif

#if !V3D_VER_AT_LEAST(3,0)
#define WORKAROUND_GFXH30
#endif

#ifndef NULL
# ifdef __cplusplus
#  define NULL 0
# else
#  define NULL ((void *)0)
# endif
#endif

#include "interface/vcos/vcos_assert.h"
#include <string.h> /* size_t */

#ifdef _MSC_VER
#define INLINE __inline
typedef unsigned long long uint64_t;
#else
#ifdef __GNUC__
/* Just using inline doesn't work for gcc (at least on MIPS), so use the gcc attribute.
   This gives a pretty decent performance boost */
#define INLINE inline __attribute__((always_inline))
#else
#define INLINE inline
#endif
#endif

#include "interface/vcos/vcos_stdbool.h"

#ifdef NDEBUG
   #define verify(X) X
#else
   #define verify(X) vcos_assert(X)
#endif
#define UNREACHABLE() vcos_assert(0)

#ifdef _MSC_VER
   #define UNUSED(X) X
#else
   #define UNUSED(X) (void)(X)
#endif

#define UNUSED_NDEBUG(X) UNUSED(X)

#define KHRN_NO_SEMAPHORE 0xffffffff

#ifdef __cplusplus
 }
#endif

#endif
