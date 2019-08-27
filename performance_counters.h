/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <boolean.h>

#include <retro_common_api.h>
#include <libretro.h>
#include <features/features_cpu.h>

RETRO_BEGIN_DECLS

#ifndef MAX_COUNTERS
#define MAX_COUNTERS 64
#endif

typedef struct rarch_timer
{
   int64_t current;
   int64_t timeout_us;
   int64_t timeout_end;
   bool timer_begin;
   bool timer_end;
} rarch_timer_t;

struct retro_perf_counter **retro_get_perf_counter_rarch(void);

struct retro_perf_counter **retro_get_perf_counter_libretro(void);

unsigned retro_get_perf_count_rarch(void);

unsigned retro_get_perf_count_libretro(void);

void performance_counter_register(struct retro_perf_counter *perf);

void performance_counters_clear(void);

void retro_perf_log(void);

void rarch_perf_log(void);

void rarch_perf_register(struct retro_perf_counter *perf);

#define performance_counter_init(perf, name) \
   perf.ident = name; \
   if (!perf.registered) \
      rarch_perf_register(&perf)

#define performance_counter_start_internal(is_perfcnt_enable, perf) \
   if ((is_perfcnt_enable)) \
   { \
      perf.call_cnt++; \
      perf.start = cpu_features_get_perf_counter(); \
   }

#define performance_counter_stop_internal(is_perfcnt_enable, perf) \
   if ((is_perfcnt_enable)) \
      perf.total += cpu_features_get_perf_counter() - perf.start

/**
 * performance_counter_start:
 * @perf               : pointer to performance counter
 *
 * Start performance counter.
 **/
#define performance_counter_start_plus(is_perfcnt_enable, perf) performance_counter_start_internal(is_perfcnt_enable, perf)

/**
 * performance_counter_stop:
 * @perf               : pointer to performance counter
 *
 * Stop performance counter.
 **/
#define performance_counter_stop_plus(is_perfcnt_enable, perf) performance_counter_stop_internal(is_perfcnt_enable, perf)

void rarch_timer_tick(rarch_timer_t *timer);

bool rarch_timer_is_running(rarch_timer_t *timer);

bool rarch_timer_has_expired(rarch_timer_t *timer);

void rarch_timer_begin(rarch_timer_t *timer, uint64_t sec);

void rarch_timer_begin_us(rarch_timer_t *timer, uint64_t usec);

void rarch_timer_begin_new_time(rarch_timer_t *timer, uint64_t sec);

void rarch_timer_begin_new_time_us(rarch_timer_t *timer, uint64_t usec);

void rarch_timer_end(rarch_timer_t *timer);

int rarch_timer_get_timeout(rarch_timer_t *timer);

RETRO_END_DECLS

#endif
