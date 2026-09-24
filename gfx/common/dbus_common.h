/*  RetroArch - A frontend for libretro.
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

#ifndef DBUS_COMMON_H__
#define DBUS_COMMON_H__

#include <boolean.h>

void dbus_ensure_connection(void);

void dbus_close_connection(void);

bool dbus_screensaver_inhibit(void);

void dbus_screensaver_uninhibit(void);

bool dbus_suspend_screensaver(bool enable);

#if defined(HAVE_DBUS) && defined(HAVE_THREADS) && defined(__linux__)
#define HAVE_DBUS_RTKIT 1
/**
 * Hands the calling thread to RealtimeKit on the system bus: real-time
 * round-robin up to rtkit's MaxRealtimePriority, else a nice value down
 * to its MinNiceLevel. The exchange runs on a detached thread of its
 * own, so nothing here or after it waits on the bus; the outcome is
 * only logged. Returns false when no request was started - one is
 * already in flight, or the helper thread could not be created.
 */
bool dbus_rtkit_raise_current_thread(void);
#endif

#endif
