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

#include <stdio.h>
#include <string.h>

#include "performance_counters.h"

#include "general.h"
#include "compat/strl.h"
#include "verbosity.h"

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

#if !defined(_WIN32) && !defined(RARCH_CONSOLE)
#include <unistd.h>
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
            !runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL)
         || perf->registered
         || perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   perf_counters_rarch[perf_ptr_rarch++] = perf;
   perf->registered = true;
}

void retro_perf_register(struct retro_perf_counter *perf)
{
   if (perf->registered || perf_ptr_libretro >= MAX_COUNTERS)
      return;

   perf_counters_libretro[perf_ptr_libretro++] = perf;
   perf->registered = true;
}

void retro_perf_clear(void)
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
               (unsigned long long)counters[i]->total /
               (unsigned long long)counters[i]->call_cnt,
               (unsigned long long)counters[i]->call_cnt);
      }
   }
}

void rarch_perf_log(void)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL))
      return;

   RARCH_LOG("[PERF]: Performance counters (RetroArch):\n");
   log_counters(perf_counters_rarch, perf_ptr_rarch);
}

void retro_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   log_counters(perf_counters_libretro, perf_ptr_libretro);
}

int rarch_perf_init(struct retro_perf_counter *perf, const char *name)
{
   perf->ident = name;

   if (!perf->registered)
      rarch_perf_register(perf);

   return 0;
}

void retro_perf_start(struct retro_perf_counter *perf)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL) || !perf)
      return;

   perf->call_cnt++;
   perf->start = retro_get_perf_counter();
}

void retro_perf_stop(struct retro_perf_counter *perf)
{
   if (!runloop_ctl(RUNLOOP_CTL_IS_PERFCNT_ENABLE, NULL) || !perf)
      return;

   perf->total += retro_get_perf_counter() - perf->start;
}
