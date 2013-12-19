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
#include "libretro.h"
#include <stdint.h>

retro_perf_tick_t rarch_get_perf_counter(void);
retro_time_t rarch_get_time_usec(void);
void rarch_perf_register(struct retro_perf_counter *perf);
void retro_perf_register(struct retro_perf_counter *perf); // Same as rarch_perf_register, just for libretro cores.
void retro_perf_clear(void);
void rarch_perf_log(void);
void retro_perf_log(void);

static inline void rarch_perf_start(struct retro_perf_counter *perf)
{
   perf->call_cnt++;
   perf->start = rarch_get_perf_counter();
}

static inline void rarch_perf_stop(struct retro_perf_counter *perf)
{
   perf->total += rarch_get_perf_counter() - perf->start;
}

uint64_t rarch_get_cpu_features(void);

// Used internally by RetroArch.
#if defined(PERF_TEST) || !defined(RARCH_INTERNAL)
#define RARCH_PERFORMANCE_INIT(X) \
   static struct retro_perf_counter X = {#X}; \
   do { \
      if (!(X).registered) \
         rarch_perf_register(&(X)); \
   } while(0)
#define RARCH_PERFORMANCE_START(X) rarch_perf_start(&(X))
#define RARCH_PERFORMANCE_STOP(X) rarch_perf_stop(&(X))
#else
#define RARCH_PERFORMANCE_INIT(X)
#define RARCH_PERFORMANCE_START(X)
#define RARCH_PERFORMANCE_STOP(X)
#endif

#ifdef __cplusplus
}
#endif

#endif

