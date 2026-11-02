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

/* The X11 monitors and the one the main window is on, for the screens'
 * Display setting. A worker thread on an X connection of its own asks
 * the server and publishes what it finds; nothing here waits on the
 * server, so the context thread never makes a round trip for them. */

#ifndef __X11_MONITORS_H
#define __X11_MONITORS_H

#include <boolean.h>
#include <retro_common_api.h>
#include <queues/retro_retire.h>

#if defined(HAVE_VIDEO_WINDOWS) && defined(HAVE_THREADS) \
      && defined(HAVE_XINERAMA) && defined(RETRO_RETIRE_LOCK_FREE)
#define RARCH_HAVE_X11_MONITORS 1
#endif

#ifdef RARCH_HAVE_X11_MONITORS

RETRO_BEGIN_DECLS

#define X11_MONITORS_MAX       8   /* = VIDEO_WINDOW_DISPLAYS_MAX */
#define X11_MONITORS_ENUMERATE 1
#define X11_MONITORS_LOCATE    2

typedef struct x11_monitor
{
   int      x, y;
   unsigned w, h;
   /* Xinerama's screen_number + 1: what x11_video_window_place() matches */
   unsigned ordinal;
   /* The RandR 1.5 monitor's name (its output's, "DP-2"); "" unknown */
   char     name[64];
} x11_monitor_t;

typedef struct x11_monitors
{
   mpsc_stack_node_t node;   /* replaced, waiting for its readers */
   unsigned          serial;
   unsigned          count;
   /* Index into mon[] of the monitor the main window is on; -1 unknown */
   int               own;
   /* The window @own was looked for, 0 none */
   unsigned long     win;
   x11_monitor_t     mon[X11_MONITORS_MAX];
} x11_monitors_t;

/* Context thread: starts the worker on a connection of its own to
 * @display (DisplayString(g_x11_dpy)) the first time, or again after it
 * gave up. Returns at once; the connection is made on the worker. */
bool x11_monitors_start(const char *display);
/* Context thread: the main window to locate, 0 for none; wakes the
 * worker to locate it again (a move, or another window). */
void x11_monitors_window(unsigned long win);
/* Any thread: X11_MONITORS_ENUMERATE and/or X11_MONITORS_LOCATE. */
void x11_monitors_wake(int what);
/* Any thread: one acquire load; 0 until the first snapshot. */
unsigned x11_monitors_serial(void);
/* Reader: the newest snapshot until x11_monitors_put(), NULL until
 * the first; put() after every get(), NULL or not. */
const x11_monitors_t *x11_monitors_get(void);
void x11_monitors_put(void);
/* 0 starting, 1 running, -1 gave up (no connection). */
int x11_monitors_state(void);

RETRO_END_DECLS

#endif

#endif
