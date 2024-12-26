/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2018 - Natinusala
 *  Copyright (C) 2018-2018 - M4xw
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

#ifndef __SWITCH_PERFORMANCE_PROFILES_H
#define __SWITCH_PERFORMANCE_PROFILES_H

#if defined(HAVE_LIBNX)

static char *SWITCH_CPU_PROFILES[] = {
    "Maximum Performance",
    "High Performance",
    "Boost Performance",
    "Stock Performance",
    "Powersaving Mode 1",
    "Powersaving Mode 2",
    "Powersaving Mode 3",
};

#define SWITCH_DEFAULT_CPU_PROFILE 3 /* Stock Performance */
#define LIBNX_MAX_CPU_PROFILE 0 /* Max Performance */

static char *SWITCH_CPU_SPEEDS[] = {
    "1785 MHz",
    "1581 MHz",
    "1224 MHz",
    "1020 MHz",
    "918 MHz",
    "816 MHz",
    "714 MHz"
};

static unsigned SWITCH_CPU_SPEEDS_VALUES[] = {
    1785000000,
    1581000000,
    1224000000,
    1020000000,
    918000000,
    816000000,
    714000000
};

#endif

#endif
