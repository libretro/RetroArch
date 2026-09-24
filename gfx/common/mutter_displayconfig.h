/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
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

/* GNOME's display modes through Mutter's org.gnome.Mutter.DisplayConfig
 * D-Bus interface - the one GNOME Settings uses. Under GNOME neither
 * XWayland's RandR (every mode at the desktop's current rate, and a
 * "switch" that only scales the window) nor any Wayland protocol can
 * list or change the monitor's real modes; this can.
 *
 * Built on every threaded Linux and BSD build, over libdbus loaded at
 * run time. None of these calls waits on the bus: a worker thread
 * follows Mutter's state and applies switches, and these answer from
 * the last state it published. Until it has answered once, or where
 * Mutter is not there, they report MUTTER_DC_UNAVAILABLE. */

#ifndef __MUTTER_DISPLAYCONFIG_H
#define __MUTTER_DISPLAYCONFIG_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../video_display_server.h"
#include "dbus_runtime.h"

#if defined(RARCH_HAVE_DBUS_RUNTIME) && defined(HAVE_THREADS)
#define RARCH_HAVE_MUTTER_DC 1
#endif

#ifdef RARCH_HAVE_MUTTER_DC

RETRO_BEGIN_DECLS

/* Which head a call is for, most specific first: a connector name
 * ("DP-1", from wl_output v4), a point in the logical layout (the
 * centre of the RetroArch window on XWayland), a 1-based logical
 * monitor index. Anything unset or unmatched falls through to the
 * primary logical monitor. */
typedef struct mutter_dc_target
{
   const char *connector;
   int x, y;
   int monitor_index;
   bool have_point;
} mutter_dc_target_t;

enum mutter_dc_result
{
   /* Mutter is not on the session bus, or no session bus: the caller
    * does what it did before */
   MUTTER_DC_UNAVAILABLE = -1,
   /* Mutter answered and refused (no such mode, invalid layout) */
   MUTTER_DC_FAILED      =  0,
   MUTTER_DC_OK          =  1
};

/* true once the worker has found org.gnome.Mutter.DisplayConfig on the
 * session bus and read its state. Never autolaunches a bus. */
bool mutter_displayconfig_available(void);

/* The modes of the target head, as the menu's resolution list: sorted
 * by size then rate, one entry per size and rate, the scanned-out one
 * current. *list is malloc'd; the caller frees it. */
enum mutter_dc_result mutter_displayconfig_get_resolution_list(
      const mutter_dc_target_t *target,
      video_display_config_t **list, unsigned *len);

/* Switch the target head to the listed mode of that size nearest the
 * rate; a zero width or height keeps the current one. Checked against
 * the published state at once - MUTTER_DC_FAILED for a mode the head
 * does not list, without asking Mutter - then applied by the worker;
 * MUTTER_DC_OK means asked, and the state Mutter reports afterwards
 * carries the outcome. Applied as a
 * temporary configuration: not saved, and no "keep these settings?"
 * prompt. Every other monitor keeps its mode, position, scale,
 * transform and colour settings. */
enum mutter_dc_result mutter_displayconfig_set_resolution(
      const mutter_dc_target_t *target,
      unsigned dims, int int_hz, float hz);

RETRO_END_DECLS

#endif

#endif
