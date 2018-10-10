/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2016 - Jean-André Santoni
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

#ifndef __RARCH_LAKKA_H
#define __RARCH_LAKKA_H

#define LAKKA_SSH_PATH       "/storage/.cache/services/sshd.conf"
#define LAKKA_SAMBA_PATH     "/storage/.cache/services/samba.conf"
#define LAKKA_BLUETOOTH_PATH "/storage/.cache/services/bluez.conf"
#define LAKKA_UPDATE_DIR     "/storage/.update/"
#define LAKKA_CONNMAN_DIR    "/storage/.cache/connman/"

#ifdef HAVE_LAKKA_SWITCH
static char* SWITCH_GPU_PROFILES[] = {
   "docked-overclock-3",
   "docked-overclock-2",
   "docked-overclock-1",
   "docked",
   "non-docked-overclock-5",
   "non-docked-overclock-4",
   "non-docked-overclock-3",
   "non-docked-overclock-2",
   "non-docked-overclock-1",
   "non-docked",
   "non-docked-underclock-1",
   "non-docked-underclock-2",
   "non-docked-underclock-3",
};
   
static char* SWITCH_GPU_SPEEDS[] = {
   "998 Mhz",
   "921 Mhz",
   "844 Mhz",
   "768 Mhz",
   "691 Mhz",
   "614 Mhz",
   "537 Mhz",
   "460 Mhz",
   "384 Mhz",
   "307 Mhz",
   "230 Mhz",
   "153 Mhz",
   "76 Mhz"   
};

static int SWITCH_BRIGHTNESS[] = {
   10,
   20,
   30,
   40,
   50,
   60,
   70,
   80,
   90,
   100
};

static char* SWITCH_CPU_PROFILES[] = {
   "overclock-4",
   "overclock-3",
   "overclock-2",
   "overclock-1",
   "default",
};
   
static char* SWITCH_CPU_SPEEDS[] = {
   "1912 MHz",
   "1734 MHz",
   "1530 MHz",
   "1224 MHz",
   "1020 MHz"
};
#endif

#endif
