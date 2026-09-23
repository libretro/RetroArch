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
#include <retro_atomic.h>
#include <retro_inline.h>

#include "../../retroarch.h"

extern Window   g_x11_win;
extern Display *g_x11_dpy;
extern Colormap g_x11_cmap;
extern unsigned g_x11_screen;

/* The refresh rate the X display server read through g_x11_dpy, kept
 * until the display changes. The display server selects RandR screen,
 * crtc and output changes on the root window and publishes the
 * extension's event base in g_x11_randr_state; the event pump marks it
 * X11_RANDR_PUMPED the first time it drains that connection, and only
 * from then on is the rate kept, since only then does a change reach
 * it. The rate is the bits of a float, or X11_REFRESH_NONE. Stored in
 * dispserv_x11.c. */
#define X11_RANDR_BASE_MASK   0xff
#define X11_RANDR_PUMPED      0x100
#define X11_RANDR_UNAVAILABLE 0x200
#define X11_REFRESH_NONE      (-1)

extern retro_atomic_int_t g_x11_randr_state;
extern retro_atomic_int_t g_x11_refresh_serial;
extern retro_atomic_int_t g_x11_refresh_bits;

/* The change is counted before the kept rate is dropped: a reader that
 * read the old mode keeps its rate only if the count did not move. */
static INLINE void x11_refresh_invalidate(void)
{
   retro_atomic_inc_int(&g_x11_refresh_serial);
   retro_atomic_store_release_int(&g_x11_refresh_bits, X11_REFRESH_NONE);
}

void x11_show_mouse(void *data, bool state);
void x11_set_net_wm_fullscreen(Display *dpy, Window win);
void x11_set_net_wm_fullscreen_hint(Display *dpy, Window win);
bool x11_suspend_screensaver(void *data, bool enable);

void x11_move_window(Display *dpy, Window win,
      int x, int y, unsigned width, unsigned height);

/* Set icon, class, default stuff. */
void x11_set_window_attr(Display *dpy, Window win);


#ifdef HAVE_XF86VM
float x11_get_refresh_rate(void *data);

bool x11_enter_fullscreen(Display *dpy, unsigned width, unsigned height);

void x11_exit_fullscreen(Display *dpy);
#endif

void x11_check_window(void *data, bool *quit,
   bool *resize, unsigned *dims);

void x11_get_video_size(void *data, unsigned *dims);

bool x11_has_focus(void *data);

bool x11_has_focus_internal(void *data);

/* False while the window is unmapped; see gfx_ctx_driver_t::presentable. */
bool x11_presentable(void *data);

bool x11_alive(void *data);

bool x11_connect(void);

void x11_update_title(void *data);

bool x11_input_ctx_new(bool true_full);

void x11_input_ctx_destroy(void);

void x11_window_destroy(bool fullscreen);

void x11_colormap_destroy(void);

void x11_install_quit_atom(void);

void x11_event_queue_check(XEvent *event);

char *x11_get_wm_name(Display *dpy);

bool x11_has_net_wm_fullscreen(Display *dpy);

#endif