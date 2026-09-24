/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_DBUS_COMMON_H
#define __RARCH_DBUS_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>

#include "dbus_runtime.h"

/* Screensaver inhibit over the session bus's org.freedesktop.ScreenSaver,
 * through libdbus loaded at run time. Everything that can wait - loading
 * the library, connecting, Inhibit and UnInhibit - happens on a worker
 * thread of the connection's own; the caller only says what it wants and
 * never waits for an answer. */
#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS)
#define RARCH_HAVE_DBUS_SCREENSAVER 1
#endif

#ifdef RARCH_HAVE_DBUS_SCREENSAVER

RETRO_BEGIN_DECLS

enum dbus_screensaver_state
{
   /* Nothing known yet: the worker has not answered. */
   DBUS_SCREENSAVER_PENDING = 0,
   /* The session's screensaver is inhibited through D-Bus. */
   DBUS_SCREENSAVER_INHIBITED,
   /* D-Bus cannot inhibit it: no libdbus, no session bus, or no
    * screensaver service answering there. */
   DBUS_SCREENSAVER_FAILED
};

/**
 * Opens a session-bus connection on the worker, starting the worker the
 * first time. Returns at once.
 */
void dbus_ensure_connection(void);

/**
 * Tells the worker to release any inhibit it holds and close the
 * connection. Returns at once.
 */
void dbus_close_connection(void);

/**
 * Asks for the screensaver to be inhibited or released. Returns at once:
 * false when no connection is open, so the caller's own fallbacks apply;
 * true when the worker has the request, whose outcome
 * dbus_screensaver_state() reports once it is known.
 */
bool dbus_suspend_screensaver(bool enable);

/**
 * How the latest request ended, as far as it is known.
 */
enum dbus_screensaver_state dbus_screensaver_state(void);

RETRO_END_DECLS

#endif

#endif
