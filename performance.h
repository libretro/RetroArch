/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "boolean.h"
#include <stdint.h>

retro_perf_tick_t rarch_get_perf_counter(void);
retro_time_t rarch_get_time_usec(void);
void rarch_perf_register(struct retro_perf_counter *perf);
void rarch_perf_log(void);

void rarch_get_cpu_features(unsigned *cpu);

#if defined(PERF_TEST) || !defined(RARCH_INTERNAL)
#ifdef _WIN32
#define RETRO_PERFORMANCE_LOG(functionname, X) RARCH_LOG("[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n", \
      functionname, \
      (X).total / (X).call_cnt, \
      (X).call_cnt)
#else
#define RETRO_PERFORMANCE_LOG(functionname, X) RARCH_LOG("[PERF]: Avg (%s): %llu ticks, %llu runs.\n", \
      functionname, \
      (X).total / (X).call_cnt, \
      (X).call_cnt)
#endif
#else
#define RETRO_PERFORMANCE_LOG(functionname, X)
#endif

#ifdef __cplusplus
}
#endif

#endif

