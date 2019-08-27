/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>

#include "performance_counters.h"

#include "retroarch.h"
#include "verbosity.h"

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

static struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];
static struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];
static unsigned perf_ptr_rarch;
static unsigned perf_ptr_libretro;

struct retro_perf_counter **retro_get_perf_counter_rarch(void)
{
   return perf_counters_rarch;
}

struct retro_perf_counter **retro_get_perf_counter_libretro(void)
{
   return perf_counters_libretro;
}

unsigned retro_get_perf_count_rarch(void)
{
   return perf_ptr_rarch;
}

unsigned retro_get_perf_count_libretro(void)
{
   return perf_ptr_libretro;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   if (
            !rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL)
         || perf->registered
         || perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   perf_counters_rarch[perf_ptr_rarch++] = perf;
   perf->registered = true;
}

void performance_counter_register(struct retro_perf_counter *perf)
{
   if (perf->registered || perf_ptr_libretro >= MAX_COUNTERS)
      return;

   perf_counters_libretro[perf_ptr_libretro++] = perf;
   perf->registered = true;
}

void performance_counters_clear(void)
{
   perf_ptr_libretro = 0;
   memset(perf_counters_libretro, 0, sizeof(perf_counters_libretro));
}

static void log_counters(struct retro_perf_counter **counters, unsigned num)
{
   unsigned i;
   for (i = 0; i < num; i++)
   {
      if (counters[i]->call_cnt)
      {
         RARCH_LOG(PERF_LOG_FMT,
               counters[i]->ident,
               (uint64_t)counters[i]->total /
               (uint64_t)counters[i]->call_cnt,
               (uint64_t)counters[i]->call_cnt);
      }
   }
}

void rarch_perf_log(void)
{
   if (!rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL))
      return;

   RARCH_LOG("[PERF]: Performance counters (RetroArch):\n");
   log_counters(perf_counters_rarch, perf_ptr_rarch);
}

void retro_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   log_counters(perf_counters_libretro, perf_ptr_libretro);
}

void rarch_timer_tick(rarch_timer_t *timer)
{
   if (!timer)
      return;
   timer->current = cpu_features_get_time_usec();
   timer->timeout_us = (timer->timeout_end - timer->current);
}

int rarch_timer_get_timeout(rarch_timer_t *timer)
{
   if (!timer)
      return 0;
   return (int)(timer->timeout_us / 1000000);
}

bool rarch_timer_is_running(rarch_timer_t *timer)
{
   if (!timer)
      return false;
   return timer->timer_begin;
}

bool rarch_timer_has_expired(rarch_timer_t *timer)
{
   if (!timer || timer->timeout_us <= 0)
      return true;
   return false;
}

void rarch_timer_end(rarch_timer_t *timer)
{
   if (!timer)
      return;
   timer->timer_end   = true;
   timer->timer_begin = false;
   timer->timeout_end = 0;
}

void rarch_timer_begin_new_time(rarch_timer_t *timer, uint64_t sec)
{
   if (!timer)
      return;
   timer->timeout_us = sec * 1000000;
   timer->current = cpu_features_get_time_usec();
   timer->timeout_end = timer->current + timer->timeout_us;
}

void rarch_timer_begin_new_time_us(rarch_timer_t *timer, uint64_t usec)
{
   if (!timer)
      return;
   timer->timeout_us = usec;
   timer->current = cpu_features_get_time_usec();
   timer->timeout_end = timer->current + timer->timeout_us;
}

void rarch_timer_begin(rarch_timer_t *timer, uint64_t sec)
{
   if (!timer)
      return;
   rarch_timer_begin_new_time(timer, sec);
   timer->timer_begin = true;
   timer->timer_end   = false;
}

void rarch_timer_begin_us(rarch_timer_t *timer, uint64_t usec)
{
   if (!timer)
      return;
   rarch_timer_begin_new_time_us(timer, usec);
   timer->timer_begin = true;
   timer->timer_end   = false;
}

