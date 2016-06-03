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

#ifndef _PERFORMANCE_COUNTERS_H
#define _PERFORMANCE_COUNTERS_H

#include <stdint.h>

#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

struct retro_perf_counter **retro_get_perf_counter_rarch(void);

struct retro_perf_counter **retro_get_perf_counter_libretro(void);

unsigned retro_get_perf_count_rarch(void);

unsigned retro_get_perf_count_libretro(void);

void performance_counter_register(struct retro_perf_counter *perf);

void performance_counters_clear(void);

void retro_perf_log(void);

void rarch_perf_log(void);

int performance_counter_init(struct retro_perf_counter *perf, const char *name);

/**
 * performance_counter_start:
 * @perf               : pointer to performance counter
 *
 * Start performance counter. 
 **/
void performance_counter_start(struct retro_perf_counter *perf);

/**
 * performance_counter_stop:
 * @perf               : pointer to performance counter
 *
 * Stop performance counter. 
 **/
void performance_counter_stop(struct retro_perf_counter *perf);

RETRO_END_DECLS

#endif

