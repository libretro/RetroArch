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
#include <math.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xatom.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "x11_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/common/input_x11_common.h"
#include "../../verbosity.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
static DBusConnection* dbus_connection      = NULL;
static unsigned int dbus_screensaver_cookie = 0;
#endif

static bool xdg_screensaver_available       = true;
bool g_x11_entered                          = false;
static bool g_x11_has_focus                 = false;
static bool g_x11_true_full                 = false;
Display *g_x11_dpy                          = NULL;

unsigned g_x11_screen                       = 0;

Colormap g_x11_cmap;
Window   g_x11_win;

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_MOVERESIZE_WINDOW;

static Atom g_x11_quit_atom;
static XIM g_x11_xim;
static XIC g_x11_xic;

#define XA_INIT(x) XA##x = XInternAtom(dpy, #x, False)
#define _NET_WM_STATE_ADD 1
#define MOVERESIZE_GRAVITY_CENTER 5
#define MOVERESIZE_X_SHIFT 8
#define MOVERESIZE_Y_SHIFT 9

#ifdef HAVE_DBUS
static void dbus_ensure_connection(void)
{
    DBusError err;
    int ret;
    
    dbus_error_init(&err);

    dbus_connection = dbus_bus_get_private(DBUS_BUS_SESSION, &err);

    if (dbus_error_is_set(&err))
    {
        RARCH_LOG("[DBus]: Failed to get DBus connection. Screensaver will not be suspended via DBus.\n");
        dbus_error_free(&err);
    }

    if (dbus_connection)
        dbus_connection_set_exit_on_disconnect(dbus_connection, true);
}

static void dbus_close_connection(void)
{
   if (!dbus_connection)
      return;

   dbus_connection_close(dbus_connection);
   dbus_connection_unref(dbus_connection);
   dbus_connection = NULL;
}

static bool dbus_screensaver_inhibit(void)
{
   const char *app    = "RetroArch";
   const char *reason = "Playing a game";
   DBusMessage   *msg = NULL;
   DBusMessage *reply = NULL;
   bool ret           = false;

   if (!dbus_connection)
      return false; /* DBus connection was not obtained */

   if (dbus_screensaver_cookie > 0)
      return true; /* Already inhibited */

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "Inhibit");

   if (!msg)
      return false;

   if (!dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &app,
            DBUS_TYPE_STRING, &reason,
            DBUS_TYPE_INVALID))
   {
      dbus_message_unref(msg);
      return false;
   }

   reply = dbus_connection_send_with_reply_and_block(dbus_connection,
         msg, 300, NULL);

   if (reply != NULL)
   {
      if (!dbus_message_get_args(reply, NULL,
               DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
               DBUS_TYPE_INVALID))
         dbus_screensaver_cookie = 0;
      else
         ret = true;

      dbus_message_unref(reply);
   }

   dbus_message_unref(msg);

   if (dbus_screensaver_cookie == 0)
   {
      RARCH_ERR("[DBus]: Failed to suspend screensaver via DBus.\n");
   }
   else
   {
      RARCH_LOG("[DBus]: Suspended screensaver via DBus.\n");
   }

   return ret;
}

static void dbus_screensaver_uninhibit(void)
{
   DBusMessage *msg = NULL;

   if (!dbus_connection)
      return;

   if (dbus_screensaver_cookie == 0)
      return;

   msg = dbus_message_new_method_call("org.freedesktop.ScreenSaver",
         "/org/freedesktop/ScreenSaver",
         "org.freedesktop.ScreenSaver",
         "UnInhibit");
   if (!msg)
       return;

   dbus_message_append_args(msg,
         DBUS_TYPE_UINT32, &dbus_screensaver_cookie,
         DBUS_TYPE_INVALID);

   if (dbus_connection_send(dbus_connection, msg, NULL))
      dbus_connection_flush(dbus_connection);
   dbus_message_unref(msg);

   dbus_screensaver_cookie = 0;
}

/* Returns false when fallback should be attempted */
bool x11_suspend_screensaver_dbus(bool enable)
{
   if (enable) return dbus_screensaver_inhibit();
   dbus_screensaver_uninhibit();
   return false;
}
#endif

static void x11_hide_mouse(Display *dpy, Window win)
{
   static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
   Cursor no_ptr;
   Pixmap bm_no;
   XColor black, dummy;
   Colormap colormap = DefaultColormap(dpy, DefaultScreen(dpy));

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

void x11_show_mouse(Display *dpy, Window win, bool state)
{
   if (state)
      XUndefineCursor(dpy, win);
   else
      x11_hide_mouse(dpy, win);
}

void x11_windowed_fullscreen(Display *dpy, Window win)
{
   XEvent xev = {0};

   XA_INIT(_NET_WM_STATE);
   XA_INIT(_NET_WM_STATE_FULLSCREEN);

   xev.xclient.type         = ClientMessage;
   xev.xclient.send_event   = True;
   xev.xclient.message_type = XA_NET_WM_STATE;
   xev.xclient.window       = win;
   xev.xclient.format       = 32;
   xev.xclient.data.l[0]    = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1]    = XA_NET_WM_STATE_FULLSCREEN;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

/* Try to be nice to tiling WMs if possible. */

void x11_move_window(Display *dpy, Window win, int x, int y,
      unsigned width, unsigned height)
{
   XEvent xev = {0};

   XA_INIT(_NET_MOVERESIZE_WINDOW);

   xev.xclient.type         = ClientMessage;
   xev.xclient.send_event   = True;
   xev.xclient.message_type = XA_NET_MOVERESIZE_WINDOW;
   xev.xclient.window       = win;
   xev.xclient.format       = 32;
   xev.xclient.data.l[0]    = (1 << MOVERESIZE_X_SHIFT) 
      | (1 << MOVERESIZE_Y_SHIFT);
   xev.xclient.data.l[1]    = x;
   xev.xclient.data.l[2]    = y;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

static void x11_set_window_class(Display *dpy, Window win)
{
   XClassHint hint;

   hint.res_name   = (char*)"retroarch"; /* Broken header. */
   hint.res_class  = (char*)"retroarch";
   XSetClassHint(dpy, win, &hint);
}

void x11_set_window_attr(Display *dpy, Window win)
{
   x11_set_window_class(dpy, win);
}

static void xdg_screensaver_inhibit(Window wnd)
{
   int  ret;
   char cmd[64];

   cmd[0] = '\0';

   RARCH_LOG("Suspending screensaver (X11, xdg-screensaver).\n");

   snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend 0x%x", (int)wnd);

   ret = system(cmd);
   if (ret == -1)
   {
      xdg_screensaver_available = false;
      RARCH_WARN("Failed to launch xdg-screensaver.\n");
   }
   else if (WEXITSTATUS(ret))
   {
      xdg_screensaver_available = false;
      RARCH_WARN("Could not suspend screen saver.\n");
   }
}

void x11_suspend_screensaver_xdg_screensaver(Window wnd, bool enable)
{
   /* Check if screensaver suspend is enabled in config */
   if (!enable)
      return;

   if (xdg_screensaver_available)
      xdg_screensaver_inhibit(wnd);
}

void x11_suspend_screensaver(Window wnd, bool enable)
{
#ifdef HAVE_DBUS
    if (x11_suspend_screensaver_dbus(enable))
       return;
#endif
    x11_suspend_screensaver_xdg_screensaver(wnd, enable);
}

static bool get_video_mode(video_frame_info_t *video_info,
      Display *dpy, unsigned width, unsigned height,
      XF86VidModeModeInfo *mode, XF86VidModeModeInfo *desktop_mode)
{
   float refresh_mod;
   int i, num_modes            = 0;
   bool ret                    = false;
   float minimum_fps_diff      = 0.0f;
   XF86VidModeModeInfo **modes = NULL;

   XF86VidModeGetAllModeLines(dpy, DefaultScreen(dpy), &num_modes, &modes);

   if (!num_modes)
   {
      XFree(modes);
      return false;
   }

   *desktop_mode = *modes[0];

   /* If we use black frame insertion, we fake a 60 Hz monitor 
    * for 120 Hz one, etc, so try to match that. */
   refresh_mod = video_info->black_frame_insertion ? 0.5f : 1.0f;

   for (i = 0; i < num_modes; i++)
   {
      float refresh, diff;
      const XF86VidModeModeInfo *m = modes[i];

      if (!m)
         continue;

      if (m->hdisplay != width)
         continue;
      if (m->vdisplay != height)
         continue;

      refresh = refresh_mod * m->dotclock * 1000.0f / (m->htotal * m->vtotal);
      diff    = fabsf(refresh - video_info->refresh_rate);

      if (!ret || diff < minimum_fps_diff)
      {
         *mode = *m;
         minimum_fps_diff = diff;
      }
      ret = true;
   }

   XFree(modes);
   return ret;
}

bool x11_enter_fullscreen(video_frame_info_t *video_info,
      Display *dpy, unsigned width,
      unsigned height, XF86VidModeModeInfo *desktop_mode)
{
   XF86VidModeModeInfo mode;

   if (!get_video_mode(video_info, dpy, width, height, &mode, desktop_mode))
      return false;

   if (!XF86VidModeSwitchToMode(dpy, DefaultScreen(dpy), &mode))
      return false;

   XF86VidModeSetViewPort(dpy, DefaultScreen(dpy), 0, 0);
   return true;
}

void x11_exit_fullscreen(Display *dpy, XF86VidModeModeInfo *desktop_mode)
{
   XF86VidModeSwitchToMode(dpy, DefaultScreen(dpy), desktop_mode);
   XF86VidModeSetViewPort(dpy, DefaultScreen(dpy), 0, 0);
}

#ifdef HAVE_XINERAMA
static XineramaScreenInfo *x11_query_screens(Display *dpy, int *num_screens)
{
   int major, minor;

   if (!XineramaQueryExtension(dpy, &major, &minor))
      return NULL;

   XineramaQueryVersion(dpy, &major, &minor);
   RARCH_LOG("[X11]: Xinerama version: %d.%d.\n", major, minor);

   if (!XineramaIsActive(dpy))
      return NULL;

   return XineramaQueryScreens(dpy, num_screens);
}

bool x11_get_xinerama_coord(Display *dpy, int screen,
      int *x, int *y, unsigned *w, unsigned *h)
{
   int i, num_screens       = 0;
   bool                 ret = false;
   XineramaScreenInfo *info = x11_query_screens(dpy, &num_screens);

   RARCH_LOG("[X11]: Xinerama screens: %d.\n", num_screens);

   for (i = 0; i < num_screens; i++)
   {
      if (info[i].screen_number != screen)
         continue;

      *x = info[i].x_org;
      *y = info[i].y_org;
      *w = info[i].width;
      *h = info[i].height;
      ret = true;
      break;
   }

   XFree(info);
   return ret;
}

unsigned x11_get_xinerama_monitor(Display *dpy, int x, int y,
      int w, int h)
{
   int       i, num_screens = 0;
   unsigned       monitor   = 0;
   int       largest_area   = 0;
   XineramaScreenInfo *info = x11_query_screens(dpy, &num_screens);

   RARCH_LOG("[X11]: Xinerama screens: %d.\n", num_screens);

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
         monitor = i;
         largest_area = area;
      }
   }

   XFree(info);
   return monitor;
}
#endif

bool x11_create_input_context(Display *dpy, Window win, XIM *xim, XIC *xic)
{
   x11_destroy_input_context(xim, xic);

   g_x11_has_focus = true;
   *xim            = XOpenIM(dpy, NULL, NULL, NULL);

   if (!*xim)
   {
      RARCH_ERR("[X11]: Failed to open input method.\n");
      return false;
   }

   *xic = XCreateIC(*xim, XNInputStyle,
         XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL);

   if (!*xic)
   {
      RARCH_ERR("[X11]: Failed to create input context.\n");
      return false;
   }

   XSetICFocus(*xic);
   return true;
}

void x11_destroy_input_context(XIM *xim, XIC *xic)
{
   if (*xic)
   {
      XDestroyIC(*xic);
      *xic = NULL;
   }

   if (*xim)
   {
      XCloseIM(*xim);
      *xim = NULL;
   }
}

bool x11_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   unsigned     screen_no  = 0;
   Display           *dpy  = (Display*)XOpenDisplay(NULL);
   int pixels_x            = DisplayWidth(dpy, screen_no);
   int pixels_y            = DisplayHeight(dpy, screen_no);
   int physical_width      = DisplayWidthMM(dpy, screen_no);
   int physical_height     = DisplayHeightMM(dpy, screen_no);

   (void)pixels_y;

   XCloseDisplay(dpy);

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = (float)physical_width;
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         *value = (float)physical_height;
         break;
      case DISPLAY_METRIC_DPI:
         *value = ((((float)pixels_x) * 25.4) / ((float)physical_width));
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
}

bool x11_alive(void *data)
{
   while (XPending(g_x11_dpy))
   {
      XEvent event;
      bool filter = false;

      /* Can get events from older windows. Check this. */
      XNextEvent(g_x11_dpy, &event);
      filter = XFilterEvent(&event, g_x11_win);

      switch (event.type)
      {
         case ClientMessage:
            if (event.xclient.window == g_x11_win && 
                  (Atom)event.xclient.data.l[0] == g_x11_quit_atom)
               frontend_driver_set_signal_handler_state(1);
            break;

         case DestroyNotify:
            if (event.xdestroywindow.window == g_x11_win)
               frontend_driver_set_signal_handler_state(1);
            break;

         case MapNotify:
            if (event.xmap.window == g_x11_win)
               g_x11_has_focus = true;
            break;

         case UnmapNotify:
            if (event.xunmap.window == g_x11_win)
               g_x11_has_focus = false;
            break;

         case ButtonPress:
            switch (event.xbutton.button)
            {
               case 1: /* Left click */
#if 0
                  RARCH_LOG("Click occurred : [%d, %d]\n",
                        event.xbutton.x_root,
                        event.xbutton.y_root);
#endif
                  break;
               case 2: /* Grabbed  */
                       /* Middle click */
                  break;
               case 3: /* Right click */
                  break;
               case 4: /* Grabbed  */
                       /* Scroll up */
               case 5: /* Scroll down */
                  x_input_poll_wheel(&event.xbutton, true);
                  break;
            }
            break;

         case EnterNotify:
            g_x11_entered = true;
            break;

         case LeaveNotify:
            g_x11_entered = false;
            break;

         case ButtonRelease:
            break;

         case KeyPress:
         case KeyRelease:
            if (event.xkey.window == g_x11_win)
               x11_handle_key_event(&event, g_x11_xic, filter);
            break;
      }
   }

   return !((bool)frontend_driver_get_signal_handler_state());
}

void x11_check_window(void *data, bool *quit,
   bool *resize, unsigned *width, unsigned *height,
   bool is_shutdown)
{
   unsigned new_width  = *width;
   unsigned new_height = *height;

   x11_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *resize = true;
      *width  = new_width;
      *height = new_height;
   }

   x11_alive(data);

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

void x11_get_video_size(void *data, unsigned *width, unsigned *height)
{
   if (!g_x11_dpy || g_x11_win == None)
   {
      Display *dpy = (Display*)XOpenDisplay(NULL);
      *width       = 0;
      *height      = 0;

      if (dpy)
      {
         int screen = DefaultScreen(dpy);
         *width     = DisplayWidth(dpy, screen);
         *height    = DisplayHeight(dpy, screen);
         XCloseDisplay(dpy);
      }
   }
   else
   {
      XWindowAttributes target;
      XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);

      *width  = target.width;
      *height = target.height;
   }
}

bool x11_has_focus_internal(void *data)
{
   return g_x11_has_focus;
}

bool x11_has_focus(void *data)
{
   Window win;
   int rev;

   XGetInputFocus(g_x11_dpy, &win, &rev);

   return (win == g_x11_win && g_x11_has_focus) || g_x11_true_full;
}

bool x11_connect(void)
{
   frontend_driver_destroy_signal_handler_state();

   /* Keep one g_x11_dpy alive the entire process lifetime.
    * This is necessary for nVidia's EGL implementation for now. */
   if (!g_x11_dpy)
   {
      g_x11_dpy = XOpenDisplay(NULL);
      if (!g_x11_dpy)
         return false;
   }

#ifdef HAVE_DBUS
   dbus_ensure_connection();
#endif


   return true;
}

void x11_update_title(void *data, video_frame_info_t *video_info)
{
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      XStoreName(g_x11_dpy, g_x11_win, title);
}

bool x11_input_ctx_new(bool true_full)
{
   if (!x11_create_input_context(g_x11_dpy, g_x11_win,
            &g_x11_xim, &g_x11_xic))
      return false;

   video_driver_display_type_set(RARCH_DISPLAY_X11);
   video_driver_display_set((uintptr_t)g_x11_dpy);
   video_driver_window_set((uintptr_t)g_x11_win);
   g_x11_true_full       = true_full;
   return true;
}

void x11_input_ctx_destroy(void)
{
   x11_destroy_input_context(&g_x11_xim, &g_x11_xic);
}

void x11_window_destroy(bool fullscreen)
{
   if (g_x11_win)
      XUnmapWindow(g_x11_dpy, g_x11_win);
   if (!fullscreen)
      XDestroyWindow(g_x11_dpy, g_x11_win);
   g_x11_win = None;

#ifdef HAVE_DBUS
    dbus_screensaver_uninhibit();
    dbus_close_connection();
#endif
}

void x11_colormap_destroy(void)
{
   if (!g_x11_cmap)
      return;

   XFreeColormap(g_x11_dpy, g_x11_cmap);
   g_x11_cmap = None;
}

void x11_install_quit_atom(void)
{
   g_x11_quit_atom = XInternAtom(g_x11_dpy,
         "WM_DELETE_WINDOW", False);
   if (g_x11_quit_atom)
      XSetWMProtocols(g_x11_dpy, g_x11_win, &g_x11_quit_atom, 1);
}

static Bool x11_wait_notify(Display *d, XEvent *e, char *arg)
{
   return e->type == MapNotify && e->xmap.window == g_x11_win;
}

void x11_event_queue_check(XEvent *event)
{
   XIfEvent(g_x11_dpy, event, x11_wait_notify, NULL);
}

void x11_save_last_used_monitor(Window win)
{
#ifdef HAVE_XINERAMA
   XWindowAttributes target;
   Window child;
   int x = 0, y = 0;

   XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);
   XTranslateCoordinates(g_x11_dpy, g_x11_win,
         DefaultRootWindow(g_x11_dpy),
         target.x, target.y, &x, &y, &child);

   g_x11_screen = x11_get_xinerama_monitor(g_x11_dpy, x, y,
         target.width, target.height);

   RARCH_LOG("[X11]: Saved monitor #%u.\n", g_x11_screen);
#endif
}
