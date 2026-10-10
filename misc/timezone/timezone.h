/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - gavin_darkglider
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

#ifndef _MISC_TIMEZONE_H
#define _MISC_TIMEZONE_H

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Order in which day, month and year are written. */
enum timezone_date_order
{
   TIMEZONE_DATE_ORDER_DMY = 0, /* most of the world */
   TIMEZONE_DATE_ORDER_MDY,     /* United States and a few others */
   TIMEZONE_DATE_ORDER_YMD,     /* East Asia, Hungary, Lithuania, ... */
   TIMEZONE_DATE_ORDER_UNKNOWN  /* UTC and other zones with no place */
};

/**
 * timezone_get_date_order:
 * @tz : an IANA time zone name, e.g. "Europe/Berlin".
 *
 * Best guess at the date order used where @tz is. A time zone is
 * only a proxy for a locale, so this is a convention lookup, not
 * a rule: a zone tied to no place (UTC, Etc/...) is UNKNOWN, and any
 * other zone not listed is DMY.
 **/
enum timezone_date_order timezone_get_date_order(const char *tz);

/**
 * timezone_get_system:
 * @s   : receives the name of the system time zone.
 * @len : size of @s.
 *
 * Name of the time zone the system is set to, where it can be found:
 * the TZ variable, else the /etc/localtime link (Linux, BSD, macOS),
 * persist.sys.timezone (Android) or the time zone key (Windows; a
 * Windows name such as "Pacific Standard Time", which
 * timezone_get_date_order() also knows).
 *
 * Returns: false when the platform gives no way to tell.
 **/
bool timezone_get_system(char *s, size_t len);

RETRO_END_DECLS

#endif
