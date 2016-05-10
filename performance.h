/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
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

