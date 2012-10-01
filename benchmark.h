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

#ifndef _RARCH_BENCHMARK_H
#define _RARCH_BENCHMARK_H

typedef struct performance_counter_t
{
   unsigned long long start;
   unsigned long long stop;
} performance_counter_t;

unsigned long long rarch_get_performance_counter(void);

#define RARCH_PERFORMANCE_INIT(X)  performance_counter_t (X)
#define RARCH_PERFORMANCE_START(X) ((X).start = rarch_get_performance_counter())
#define RARCH_PERFORMANCE_STOP(X)  ((X).stop  = rarch_get_performance_counter() - (X).start)

#ifdef _WIN32
#define RARCH_PERFORMANCE_LOG(functionname, X)   RARCH_LOG("Time taken (%s): %I64u.\n", functionname, (X).stop)
#else
#define RARCH_PERFORMANCE_LOG(functionname, X)   RARCH_LOG("Time taken (%s): %llu.\n", functionname, (X).stop)
#endif

#endif
