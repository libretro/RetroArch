/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cpu_features.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _LIBRETRO_SDK_CPU_INFO_H
#define _LIBRETRO_SDK_CPU_INFO_H

#include <stdint.h>

#ifdef RARCH_INTERNAL
#include "libretro.h"
#else
typedef uint64_t retro_perf_tick_t;
typedef uint64_t retro_time_t;
#endif

/* ID values for CPU features */
#define CPU_FEATURE_SSE      (1 << 0)
#define CPU_FEATURE_SSE2     (1 << 1)
#define CPU_FEATURE_VMX      (1 << 2)
#define CPU_FEATURE_VMX128   (1 << 3)
#define CPU_FEATURE_AVX      (1 << 4)
#define CPU_FEATURE_NEON     (1 << 5)
#define CPU_FEATURE_SSE3     (1 << 6)
#define CPU_FEATURE_SSSE3    (1 << 7)
#define CPU_FEATURE_MMX      (1 << 8)
#define CPU_FEATURE_MMXEXT   (1 << 9)
#define CPU_FEATURE_SSE4     (1 << 10)
#define CPU_FEATURE_SSE42    (1 << 11)
#define CPU_FEATURE_AVX2     (1 << 12)
#define CPU_FEATURE_VFPU     (1 << 13)
#define CPU_FEATURE_PS       (1 << 14)
#define CPU_FEATURE_AES      (1 << 15)
#define CPU_FEATURE_VFPV3    (1 << 16)
#define CPU_FEATURE_VFPV4    (1 << 17)
#define CPU_FEATURE_POPCNT   (1 << 18)
#define CPU_FEATURE_MOVBE    (1 << 19)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cpu_features_get_perf_counter:
 *
 * Gets performance counter.
 *
 * Returns: performance counter.
 **/
retro_perf_tick_t cpu_features_get_perf_counter(void);

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.  *
 * Returns: time in microseconds.
 **/
retro_time_t cpu_features_get_time_usec(void);

/**
 * cpu_features_get:
 *
 * Gets CPU features..
 *
 * Returns: bitmask of all CPU features available.
 **/
uint64_t cpu_features_get(void);

/**
 * cpu_features_get_core_amount:
 *
 * Gets the amount of available CPU cores.
 *
 * Returns: amount of CPU cores available.
 **/
unsigned cpu_features_get_core_amount(void);


#ifdef __cplusplus
}
#endif

#endif

