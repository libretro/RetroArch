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

#ifndef __MENU_TIMEDATE_H
#define __MENU_TIMEDATE_H

#include <stddef.h>
#include <time.h>

#include <retro_common_api.h>

#include "menu_defines.h"

RETRO_BEGIN_DECLS

/* The date order used where the IANA zone @tz is: a convention lookup,
 * since a zone only stands in for a locale. A zone tied to no place
 * (UTC, Etc/...) is UNKNOWN; any other zone not listed is DMY. */
enum menu_timedate_date_order menu_timedate_zone_order(const char *tz);

/* The date order of the host's zone. Main thread only. */
enum menu_timedate_date_order menu_timedate_host_order(void);

/* Takes the host's AM/PM designators once per process, converted to
 * UTF-8, "AM"/"PM" where the locale has none. Main thread only, before
 * the first clock is drawn. */
void menu_timedate_capture_ampm(void);

/* Writes the clock for @tm in @style with @separator into @s, the
 * AM/PM styles with the captured designator. Never allocates and
 * never touches the locale, so any thread may draw it.
 * Returns the length, or 0 with @s empty for an unknown style. */
size_t menu_timedate_format(char *s, size_t len, unsigned style,
      unsigned separator, const struct tm *tm);

RETRO_END_DECLS

#endif
