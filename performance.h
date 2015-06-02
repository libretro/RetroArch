/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _RARCH_PERF_H
#define _RARCH_PERF_H

#include <stdint.h>

#include <retro_inline.h>

#include "libretro.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

/* Used internally by RetroArch. */
#define RARCH_PERFORMANCE_INIT(X) \
   static struct retro_perf_counter X = {#X}; \
   do { \
      if (!(X).registered) \
         rarch_perf_register(&(X)); \
   } while(0)

#define RARCH_PERFORMANCE_START(X) rarch_perf_start(&(X))
#define RARCH_PERFORMANCE_STOP(X) rarch_perf_stop(&(X))

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

extern const struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];
extern const struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];
extern unsigned perf_ptr_rarch;
extern unsigned perf_ptr_libretro;

/**
 * rarch_get_perf_counter:
 *
 * Gets performance counter.
 *
 * Returns: performance counter.
 **/
retro_perf_tick_t rarch_get_perf_counter(void);

/**
 * rarch_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
retro_time_t rarch_get_time_usec(void);

void rarch_perf_register(struct retro_perf_counter *perf);

/* Same as rarch_perf_register, just for libretro cores. */
void retro_perf_register(struct retro_perf_counter *perf);

void retro_perf_clear(void);

void rarch_perf_log(void);

void retro_perf_log(void);

/**
 * rarch_perf_start:
 * @perf               : pointer to performance counter
 *
 * Start performance counter. 
 **/
void rarch_perf_start(struct retro_perf_counter *perf);

/**
 * rarch_perf_stop:
 * @perf               : pointer to performance counter
 *
 * Stop performance counter. 
 **/
void rarch_perf_stop(struct retro_perf_counter *perf);

/**
 * rarch_get_cpu_features:
 *
 * Gets CPU features..
 *
 * Returns: bitmask of all CPU features available.
 **/
uint64_t rarch_get_cpu_features(void);

/**
 * rarch_get_cpu_cores:
 *
 * Gets the amount of available CPU cores.
 *
 * Returns: amount of CPU cores available.
 **/
unsigned rarch_get_cpu_cores(void);


#ifdef __cplusplus
}
#endif

#endif

