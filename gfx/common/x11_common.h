/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef X11_COMMON_H__
#define X11_COMMON_H__

#include <X11/Xutil.h>

#include <boolean.h>

#include "../../retroarch.h"

extern Window   g_x11_win;
extern Display *g_x11_dpy;
extern Colormap g_x11_cmap;
extern unsigned g_x11_screen;

void x11_show_mouse(Display *dpy, Window win, bool state);
void x11_set_net_wm_fullscreen(Display *dpy, Window win);
void x11_suspend_screensaver(Window win, bool enable);
bool x11_enter_fullscreen(video_frame_info_t *video_info,
      Display *dpy, unsigned width,
      unsigned height);

void x11_exit_fullscreen(Display *dpy);
void x11_move_window(Display *dpy, Window win,
      int x, int y, unsigned width, unsigned height);

/* Set icon, class, default stuff. */
void x11_set_window_attr(Display *dpy, Window win);

bool x11_create_input_context(Display *dpy, Window win, XIM *xim, XIC *xic);
void x11_destroy_input_context(XIM *xim, XIC *xic);

bool x11_get_metrics(void *data,
      enum display_metric_types type, float *value);

float x11_get_refresh_rate(void *data);

void x11_check_window(void *data, bool *quit,
   bool *resize, unsigned *width, unsigned *height, bool is_shutdown);

void x11_get_video_size(void *data, unsigned *width, unsigned *height);

bool x11_has_focus(void *data);

bool x11_has_focus_internal(void *data);

bool x11_alive(void *data);

bool x11_connect(void);

void x11_update_title(void *data, void *data2);

bool x11_input_ctx_new(bool true_full);

void x11_input_ctx_destroy(void);

void x11_window_destroy(bool fullscreen);

void x11_colormap_destroy(void);

void x11_install_quit_atom(void);

void x11_event_queue_check(XEvent *event);

char *x11_get_wm_name(Display *dpy);

bool x11_has_net_wm_fullscreen(Display *dpy);

#endif
