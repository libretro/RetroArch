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
#include <rthreads/retro_cas_record.h>
#include <time/rtime.h>

#include "calendar_time.h"

typedef struct calendar_time_snapshot
{
   struct retro_calendar_time start;
   struct retro_calendar_zone zone;
   bool valid;
} calendar_time_snapshot_t;

#ifdef RETRO_CAS_RECORD_LOCK_FREE
static calendar_time_snapshot_t calendar_time_slots[RETRO_CAS_RECORD_SLOTS];
static retro_cas_record_t calendar_time_record;
static bool calendar_time_inited = false;
#else
static calendar_time_snapshot_t calendar_time_snapshot;
#endif
static retro_atomic_int_t calendar_time_held = RETRO_ATOMIC_INT_INITIALIZER(0);

static void calendar_time_init(void)
{
#ifdef RETRO_CAS_RECORD_LOCK_FREE
   if (calendar_time_inited)
      return;
   retro_cas_record_init(&calendar_time_record, calendar_time_slots,
         sizeof(calendar_time_slots[0]));
   calendar_time_inited = true;
#endif
}

static void calendar_time_read(calendar_time_snapshot_t *snap)
{
#ifdef RETRO_CAS_RECORD_LOCK_FREE
   retro_cas_record_read(&calendar_time_record, snap);
#else
   *snap = calendar_time_snapshot;
#endif
}

static void calendar_time_fill(calendar_time_snapshot_t *snap,
      const struct retro_calendar_time *start,
      const struct retro_calendar_zone *zone)
{
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

static bool RETRO_CALLCONV calendar_time_get_start_time(
      struct retro_calendar_time *out)
{
   calendar_time_snapshot_t snap;
   calendar_time_read(&snap);
   if (!snap.valid)
      return false;
   *out = snap.start;
   return true;
}

static bool RETRO_CALLCONV calendar_time_get_current_time(
      struct retro_calendar_time *out)
{
   calendar_time_snapshot_t snap;
   int64_t sec;
   int32_t nsec;
   calendar_time_read(&snap);
   /* After the copy: a copy newer than a hold sees that hold */
   if (     !snap.valid
         || retro_atomic_load_acquire_int(&calendar_time_held)
         || !rtime_calendar_now(&sec, &nsec))
      return false;
   out->sec        = sec;
   out->nsec       = nsec;
   out->utc_offset = snap.start.utc_offset;
   return true;
}

static bool RETRO_CALLCONV calendar_time_get_zone(
      struct retro_calendar_zone *out)
{
   calendar_time_snapshot_t snap;
   calendar_time_read(&snap);
   if (!snap.valid)
      return false;
   *out = snap.zone;
   return true;
}

void calendar_time_publish(const struct retro_calendar_time *start,
      const struct retro_calendar_zone *zone)
{
#ifdef RETRO_CAS_RECORD_LOCK_FREE
   retro_cas_record_txn_t txn;
   calendar_time_init();
   /* The only writer: the first commit lands */
   do
   {
      calendar_time_snapshot_t *snap = (calendar_time_snapshot_t*)
            retro_cas_record_begin(&calendar_time_record, &txn);
      calendar_time_fill(snap, start, zone);
   } while (!retro_cas_record_commit(&calendar_time_record, &txn));
#else
   /* Filled aside, but a reader can still see the copy half done */
   calendar_time_snapshot_t next;
   calendar_time_fill(&next, start, zone);
   calendar_time_snapshot = next;
#endif
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
   calendar_time_init();
   iface->interface_version = RETRO_CALENDAR_TIME_INTERFACE_VERSION;
   iface->get_start_time    = calendar_time_get_start_time;
   iface->get_current_time  = calendar_time_get_current_time;
   iface->get_zone          = calendar_time_get_zone;
   return true;
}
