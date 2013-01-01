/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef X11_COMMON_H__
#define X11_COMMON_H__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "../../boolean.h"

void x11_hide_mouse(Display *dpy, Window win);
void x11_windowed_fullscreen(Display *dpy, Window win);
void x11_suspend_screensaver(Window win);
bool x11_enter_fullscreen(Display *dpy, unsigned width, unsigned height, XF86VidModeModeInfo *desktop_mode);
void x11_exit_fullscreen(Display *dpy, XF86VidModeModeInfo *desktop_mode);
void x11_move_window(Display *dpy, Window win, int x, int y, unsigned width, unsigned height);

// Set icon, class, default stuff.
void x11_set_window_attr(Display *dpy, Window win);

#ifdef HAVE_XINERAMA
bool x11_get_xinerama_coord(Display *dpy, int screen,
      int *x, int *y, unsigned *w, unsigned *h);

unsigned x11_get_xinerama_monitor(Display *dpy,
      int x, int y, int w, int h);
#endif

void x11_handle_key_event(XEvent *event);

#endif

