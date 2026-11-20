/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <time/rtime.h>

#include "calendar_time.h"

typedef struct calendar_time_snapshot
{
   struct retro_calendar_time start;
   struct retro_calendar_zone zone;
   bool valid;
} calendar_time_snapshot_t;

/* Written only before retro_load_game and after retro_unload_game,
 * when no core may call the interface; read freely in between. */
static calendar_time_snapshot_t calendar_time_snapshot;
static retro_atomic_int_t calendar_time_held = RETRO_ATOMIC_INT_INITIALIZER(0);

static bool RETRO_CALLCONV calendar_time_get_start_time(
      struct retro_calendar_time *out)
{
   if (!calendar_time_snapshot.valid)
      return false;
   *out = calendar_time_snapshot.start;
   return true;
}

static bool RETRO_CALLCONV calendar_time_get_current_time(
      struct retro_calendar_time *out)
{
   int64_t sec;
   int32_t nsec;
   if (     !calendar_time_snapshot.valid
         || retro_atomic_load_acquire_int(&calendar_time_held)
         || !rtime_calendar_now(&sec, &nsec))
      return false;
   out->sec        = sec;
   out->nsec       = nsec;
   out->utc_offset = calendar_time_snapshot.start.utc_offset;
   return true;
}

static bool RETRO_CALLCONV calendar_time_get_zone(
      struct retro_calendar_zone *out)
{
   if (!calendar_time_snapshot.valid)
      return false;
   *out = calendar_time_snapshot.zone;
   return true;
}

void calendar_time_publish(const struct retro_calendar_time *start,
      const struct retro_calendar_zone *zone)
{
   calendar_time_snapshot_t *snap = &calendar_time_snapshot;
   memset(snap, 0, sizeof(*snap));
   if (!start)
      return;
   snap->start = *start;
   if (zone)
   {
      strlcpy(snap->zone.name, zone->name, sizeof(snap->zone.name));
      snap->zone.standard_offset = zone->standard_offset;
   }
   snap->valid = true;
}

void calendar_time_clear(void)
{
   calendar_time_publish(NULL, NULL);
}

void calendar_time_capture(void)
{
   struct retro_calendar_time start;
   struct retro_calendar_zone zone;
   if (!rtime_calendar_now(&start.sec, &start.nsec))
   {
      calendar_time_clear();
      return;
   }
   start.utc_offset     = rtime_utc_offset(start.sec);
   zone.standard_offset = rtime_standard_offset(start.sec);
   rtime_zone_name(zone.name, sizeof(zone.name));
   calendar_time_publish(&start, &zone);
}

void calendar_time_set_held(bool held)
{
   int value = held ? 1 : 0;
   /* Called every frame: no store unless it changes */
   if (retro_atomic_load_relaxed_int(&calendar_time_held) != value)
      retro_atomic_store_release_int(&calendar_time_held, value);
}

bool calendar_time_get_interface(struct retro_calendar_time_interface *iface)
{
   if (!iface || !iface->interface_version)
      return false;
   iface->interface_version = RETRO_CALENDAR_TIME_INTERFACE_VERSION;
   iface->get_start_time    = calendar_time_get_start_time;
   iface->get_current_time  = calendar_time_get_current_time;
   iface->get_zone          = calendar_time_get_zone;
   return true;
}
