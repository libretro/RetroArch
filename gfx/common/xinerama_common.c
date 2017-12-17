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

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_miscellaneous.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>

#include "xinerama_common.h"

#include "../../verbosity.h"

static XineramaScreenInfo *xinerama_query_screens(Display *dpy, int *num_screens)
{
   int major, minor;

   if (!XineramaQueryExtension(dpy, &major, &minor))
      return NULL;

   XineramaQueryVersion(dpy, &major, &minor);
   RARCH_LOG("[XINERAMA]: Xinerama version: %d.%d.\n", major, minor);

   if (XineramaIsActive(dpy))
      return XineramaQueryScreens(dpy, num_screens);

   return NULL;
}

bool xinerama_get_coord(Display *dpy, int screen,
      int *x, int *y, unsigned *w, unsigned *h)
{
   int i, num_screens       = 0;
   XineramaScreenInfo *info = xinerama_query_screens(dpy, &num_screens);

   RARCH_LOG("[XINERAMA]: Xinerama screens: %d.\n", num_screens);

   for (i = 0; i < num_screens; i++)
   {
      if (info[i].screen_number != screen)
         continue;

      *x = info[i].x_org;
      *y = info[i].y_org;
      *w = info[i].width;
      *h = info[i].height;
      XFree(info);
      return true;
   }

   XFree(info);

   return false;
}

unsigned xinerama_get_monitor(Display *dpy, int x, int y,
      int w, int h)
{
   int       i, num_screens = 0;
   unsigned       monitor   = 0;
   int       largest_area   = 0;
   XineramaScreenInfo *info = xinerama_query_screens(dpy, &num_screens);

   RARCH_LOG("[XINERAMA]: Xinerama screens: %d.\n", num_screens);

   for (i = 0; i < num_screens; i++)
   {
      int area;
      int max_lx = MAX(x, info[i].x_org);
      int min_rx = MIN(x + w, info[i].x_org + info[i].width);
      int max_ty = MAX(y, info[i].y_org);
      int min_by = MIN(y + h, info[i].y_org + info[i].height);

      int len_x  = min_rx - max_lx;
      int len_y  = min_by - max_ty;

      /* The whole window is outside the screen. */
      if (len_x < 0 || len_y < 0)
         continue;

      area = len_x * len_y;

      if (area > largest_area)
      {
         monitor      = i;
         largest_area = area;
      }
   }

   XFree(info);

   if (monitor > 0)
      return monitor;

   return 0;
}

void xinerama_save_last_used_monitor(Window win)
{
   XWindowAttributes target;
   Window child;
   int x = 0, y = 0;

   XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);
   XTranslateCoordinates(g_x11_dpy, g_x11_win,
         DefaultRootWindow(g_x11_dpy),
         target.x, target.y, &x, &y, &child);

   g_x11_screen = xinerama_get_monitor(g_x11_dpy, x, y,
         target.width, target.height);

   RARCH_LOG("[XINERAMA]: Saved monitor #%u.\n", g_x11_screen);
}

#endif
