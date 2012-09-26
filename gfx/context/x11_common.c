/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "x11_common.h"
#include <stdlib.h>
#include <string.h>
#include "../../general.h"

void x11_hide_mouse(Display *dpy, Window win)
{
   Cursor no_ptr;
   Pixmap bm_no;
   XColor black, dummy;
   Colormap colormap;

   static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};

   colormap = DefaultColormap(dpy, DefaultScreen(dpy));
   if (!XAllocNamedColor(dpy, colormap, "black", &black, &dummy))
      return;

   bm_no  = XCreateBitmapFromData(dpy, win, bm_no_data, 8, 8);
   no_ptr = XCreatePixmapCursor(dpy, bm_no, bm_no, &black, &black, 0, 0);

   XDefineCursor(dpy, win, no_ptr);
   XFreeCursor(dpy, no_ptr);

   if (bm_no != None)
      XFreePixmap(dpy, bm_no);

   XFreeColors(dpy, colormap, &black.pixel, 1, 0);
}

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
#define XA_INIT(x) XA##x = XInternAtom(dpy, #x, False)
#define _NET_WM_STATE_ADD 1
void x11_windowed_fullscreen(Display *dpy, Window win)
{
   XA_INIT(_NET_WM_STATE);
   XA_INIT(_NET_WM_STATE_FULLSCREEN);

   if (!XA_NET_WM_STATE || !XA_NET_WM_STATE_FULLSCREEN)
   {
      RARCH_ERR("[X/EGL]: Cannot set windowed fullscreen.\n");
      return;
   }

   XEvent xev;

   xev.xclient.type = ClientMessage;
   xev.xclient.serial = 0;
   xev.xclient.send_event = True;
   xev.xclient.message_type = XA_NET_WM_STATE;
   xev.xclient.window = win;
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;
   xev.xclient.data.l[2] = 0;
   xev.xclient.data.l[3] = 0;
   xev.xclient.data.l[4] = 0;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

void x11_suspend_screensaver(Window wnd)
{
   char cmd[64];
   snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend %d", (int)wnd);

   int ret = system(cmd);

   if (ret != 0)
      RARCH_WARN("Could not suspend screen saver.\n");
}

