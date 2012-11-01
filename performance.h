/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "general.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
typedef unsigned long long rarch_perf_tick_t;

typedef struct rarch_perf_counter
{
   const char *ident;
   rarch_perf_tick_t start;
   rarch_perf_tick_t total;
   rarch_perf_tick_t call_cnt;

   bool registered;
} rarch_perf_counter_t;

rarch_perf_tick_t rarch_get_perf_counter(void);
void rarch_perf_register(struct rarch_perf_counter *perf);
void rarch_perf_log(void);

#ifdef PERF_TEST

#define RARCH_PERFORMANCE_INIT(X)  static rarch_perf_counter_t X = {#X}; \
   do { \
      if (!(X).registered) \
         rarch_perf_register(&(X)); \
   } while(0)

#define RARCH_PERFORMANCE_START(X) do { \
   (X).call_cnt++; \
   (X).start  = rarch_get_perf_counter(); \
} while(0)

#define RARCH_PERFORMANCE_STOP(X) do { \
   (X).total += rarch_get_perf_counter() - (X).start; \
} while(0)

#ifdef _WIN32
#define RARCH_PERFORMANCE_LOG(functionname, X) RARCH_LOG("[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n", \
      functionname, \
      (X).total / (X).call_cnt, \
      (X).call_cnt)
#else
#define RARCH_PERFORMANCE_LOG(functionname, X) RARCH_LOG("[PERF]: Avg (%s): %llu ticks, %llu runs.\n", \
      functionname, \
      (X).total / (X).call_cnt, \
      (X).call_cnt)
#endif

#else

#define RARCH_PERFORMANCE_INIT(X)
#define RARCH_PERFORMANCE_START(X)
#define RARCH_PERFORMANCE_STOP(X)
#define RARCH_PERFORMANCE_LOG(functionname, X)

#endif

#endif

