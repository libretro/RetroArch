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

#include <errno.h>
#include <unistd.h>

#include <X11/Xatom.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"

#include <X11/extensions/xf86vmode.h>

#include <encodings/utf.h>
#include <compat/strl.h>

#ifdef HAVE_DBUS
#include "dbus_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../input/common/input_x11_common.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#define _NET_WM_STATE_ADD                    1
#define MOVERESIZE_GRAVITY_CENTER            5
#define MOVERESIZE_X_SHIFT                   8
#define MOVERESIZE_Y_SHIFT                   9

#define V_DBLSCAN 0x20

static XF86VidModeModeInfo desktop_mode;
static bool xdg_screensaver_available       = true;
bool g_x11_entered                          = false;
static bool g_x11_has_focus                 = false;
static bool g_x11_true_full                 = false;
Display *g_x11_dpy                          = NULL;

unsigned g_x11_screen                       = 0;

Colormap g_x11_cmap;
Window   g_x11_win = None;

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_MOVERESIZE_WINDOW;

static Atom g_x11_quit_atom;
static XIM g_x11_xim;
static XIC g_x11_xic;

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

void x11_set_net_wm_fullscreen(Display *dpy, Window win)
{
   XEvent xev                 = {0};

   XA_NET_WM_STATE            = XInternAtom(dpy, "_NET_WM_STATE", False);
   XA_NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

   xev.xclient.type           = ClientMessage;
   xev.xclient.send_event     = True;
   xev.xclient.message_type   = XA_NET_WM_STATE;
   xev.xclient.window         = win;
   xev.xclient.format         = 32;
   xev.xclient.data.l[0]      = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1]      = XA_NET_WM_STATE_FULLSCREEN;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

/* Try to be nice to tiling WMs if possible. */

void x11_move_window(Display *dpy, Window win, int x, int y,
      unsigned width, unsigned height)
{
   XEvent xev               = {0};

   XA_NET_MOVERESIZE_WINDOW = XInternAtom(dpy,
		   "_NET_MOVERESIZE_WINDOW", False);

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

static void x11_set_window_pid(Display *dpy, Window win)
{
    long scret     = 0;
    char *hostname = NULL;
    pid_t pid      = getpid();

    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_PID", False),
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);

    errno = 0;
    if ((scret = sysconf(_SC_HOST_NAME_MAX)) == -1 && errno)
        return;
    if ((hostname = (char*)malloc(scret + 1)) == NULL)
        return;

    if (gethostname(hostname, scret + 1) == -1)
        RARCH_WARN("Failed to get hostname.\n");
    else
    {
        XChangeProperty(dpy, win, XA_WM_CLIENT_MACHINE, XA_STRING, 8,
            PropModeReplace, (unsigned char *)hostname, strlen(hostname));
    }
    free(hostname);
}

void x11_set_window_attr(Display *dpy, Window win)
{
   x11_set_window_class(dpy, win);
   x11_set_window_pid(dpy, win);
}

static void xdg_screensaver_inhibit(Window wnd)
{
   int  ret;
   char cmd[64];

   cmd[0] = '\0';

   RARCH_LOG("[X11]: Suspending screensaver (X11, xdg-screensaver).\n");

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
    if (dbus_suspend_screensaver(enable))
       return;
#endif
    x11_suspend_screensaver_xdg_screensaver(wnd, enable);
}

float x11_get_refresh_rate(void *data)
{
   XWindowAttributes attr;
   XF86VidModeModeLine modeline;
   Screen *screen;
   int screenid;
   int dotclock;
   float refresh;

   if (!g_x11_dpy || g_x11_win == None)
      return 0.0f;

   if (!XGetWindowAttributes(g_x11_dpy, g_x11_win, &attr))
      return 0.0f;

   screen = attr.screen;
   screenid = XScreenNumberOfScreen(screen);

   XF86VidModeGetModeLine(g_x11_dpy, screenid, &dotclock, &modeline);

   /* non-native modes like 1080p on a 4K display might use DoubleScan */
   if (modeline.flags & V_DBLSCAN)
      dotclock /= 2;

   refresh = (float)dotclock * 1000.0f / modeline.htotal / modeline.vtotal;

   return refresh;
}

static bool get_video_mode(video_frame_info_t *video_info,
      Display *dpy, unsigned width, unsigned height,
      XF86VidModeModeInfo *mode, XF86VidModeModeInfo *desktop_mode)
{
   int i, num_modes            = 0;
   bool ret                    = false;
   float refresh_mod           = 0.0f;
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
      unsigned height)
{
   XF86VidModeModeInfo mode;

   if (!get_video_mode(video_info, dpy, width, height, &mode, &desktop_mode))
      return false;

   if (!XF86VidModeSwitchToMode(dpy, DefaultScreen(dpy), &mode))
      return false;

   XF86VidModeSetViewPort(dpy, DefaultScreen(dpy), 0, 0);
   return true;
}

void x11_exit_fullscreen(Display *dpy)
{
   XF86VidModeSwitchToMode(dpy, DefaultScreen(dpy), &desktop_mode);
   XF86VidModeSetViewPort(dpy, DefaultScreen(dpy), 0, 0);
}

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
      case DISPLAY_METRIC_PIXEL_WIDTH:
         *value = (float)pixels_x;
         break;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         *value = (float)pixels_y;
         break;
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

static void x11_handle_key_event(unsigned keycode, XEvent *event, XIC ic, bool filter)
{
   int i;
   Status status;
   uint32_t chars[32];
   unsigned key   = 0;
   uint16_t mod   = 0;
   unsigned state = event->xkey.state;
   bool down      = event->type == KeyPress;
   int num        = 0;
   KeySym keysym  = 0;

   chars[0]       = '\0';

   /* this code generates the localized chars using keysyms */
   if (!filter)
   {
      if (down)
      {
         char keybuf[32];

         keybuf[0] = '\0';
#ifdef X_HAVE_UTF8_STRING
         status = 0;

         /* XwcLookupString doesn't seem to work. */
         num = Xutf8LookupString(ic, &event->xkey, keybuf,
               ARRAY_SIZE(keybuf), &keysym, &status);

         /* libc functions need UTF-8 locale to work properly,
          * which makes mbrtowc a bit impractical.
          *
          * Use custom UTF8 -> UTF-32 conversion. */
         num = utf8_conv_utf32(chars, ARRAY_SIZE(chars), keybuf, num);
#else
         (void)ic;
         num = XLookupString(&event->xkey, keybuf,
               sizeof(keybuf), &keysym, NULL); /* ASCII only. */
         for (i = 0; i < num; i++)
            chars[i] = keybuf[i] & 0x7f;
#endif
      }
      else
         keysym = XLookupKeysym(&event->xkey, (state & ShiftMask) || (state & LockMask));
   }

   /* We can't feed uppercase letters to the keycode translator. Seems like a bad idea
    * to feed it keysyms anyway, so here is a little hack... */
   if (keysym >= XK_A && keysym <= XK_Z)
       keysym += XK_z - XK_Z;

   /* Get the real keycode,
      that correctly ignores international layouts as windows code does. */
   key     = input_keymaps_translate_keysym_to_rk(keycode);

   if (state & ShiftMask)
      mod |= RETROKMOD_SHIFT;
   if (state & LockMask)
      mod |= RETROKMOD_CAPSLOCK;
   if (state & ControlMask)
      mod |= RETROKMOD_CTRL;
   if (state & Mod1Mask)
      mod |= RETROKMOD_ALT;
   if (state & Mod2Mask)
      mod |= RETROKMOD_NUMLOCK;
   if (state & Mod4Mask)
      mod |= RETROKMOD_META;

   input_keyboard_event(down, key, chars[0], mod, RETRO_DEVICE_KEYBOARD);

   for (i = 1; i < num; i++)
      input_keyboard_event(down, RETROK_UNKNOWN,
            chars[i], mod, RETRO_DEVICE_KEYBOARD);
}

bool x11_alive(void *data)
{
   while (XPending(g_x11_dpy))
   {
      XEvent event;
      bool filter = false;
      unsigned keycode = 0;

      /* Can get events from older windows. Check this. */
      XNextEvent(g_x11_dpy, &event);

      /* IMPORTANT - Get keycode before XFilterEvent
         because the event is localizated after the call */
      keycode = event.xkey.keycode;
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
               case 6: /* Scroll wheel left */
               case 7: /* Scroll wheel right */
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

         case KeyRelease:
            /*  When you receive a key release and the next event is a key press
               of the same key combination, then it's auto-repeat and the
               key wasn't actually released. */
            if(XEventsQueued(g_x11_dpy, QueuedAfterReading))
            {
               XEvent next_event;
               XPeekEvent(g_x11_dpy, &next_event);
               if (next_event.type == KeyPress &&
                   next_event.xkey.time == event.xkey.time &&
                   next_event.xkey.keycode == event.xkey.keycode)
               {
                  break; /* Key wasn't actually released */
               }
            }
         case KeyPress:
            if (event.xkey.window == g_x11_win)
               x11_handle_key_event(keycode, &event, g_x11_xic, filter);
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
      *width  = new_width;
      *height = new_height;
      *resize = true;
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

void x11_update_title(void *data, void *data2)
{
   video_frame_info_t *video_info = (video_frame_info_t*)data2;
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
      XChangeProperty(g_x11_dpy, g_x11_win, XA_WM_NAME, XA_STRING,
            8, PropModeReplace, (const unsigned char*)title,
            strlen(title));
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

static bool x11_check_atom_supported(Display *dpy, Atom atom)
{
   Atom XA_NET_SUPPORTED = XInternAtom(dpy, "_NET_SUPPORTED", True);
   Atom type;
   int format;
   unsigned long nitems;
   unsigned long bytes_after;
   Atom *prop;
   int i;

   if (XA_NET_SUPPORTED == None)
      return false;

   XGetWindowProperty(dpy, DefaultRootWindow(dpy), XA_NET_SUPPORTED,
         0, UINT_MAX, False, XA_ATOM, &type, &format,&nitems,
         &bytes_after, (unsigned char **) &prop);

   if (!prop || type != XA_ATOM)
      return false;

   for (i = 0; i < nitems; i++)
   {
      if (prop[i] == atom)
      {
         XFree(prop);
         return true;
      }
   }

   XFree(prop);

   return false;
}

bool x11_has_net_wm_fullscreen(Display *dpy)
{
   XA_NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

   return x11_check_atom_supported(dpy, XA_NET_WM_STATE_FULLSCREEN);
}

char *x11_get_wm_name(Display *dpy)
{
   Atom type;
   int  format;
   Window window;
   Atom XA_NET_SUPPORTING_WM_CHECK = XInternAtom(g_x11_dpy, "_NET_SUPPORTING_WM_CHECK", False);
   Atom XA_NET_WM_NAME             = XInternAtom(g_x11_dpy, "_NET_WM_NAME", False);
   Atom XA_UTF8_STRING             = XInternAtom(g_x11_dpy, "UTF8_STRING", False);
   unsigned long nitems            = 0;
   unsigned long bytes_after       = 0;
   char *title                     = NULL;
   unsigned char *propdata         = NULL;

   if (!XA_NET_SUPPORTING_WM_CHECK || !XA_NET_WM_NAME)
      return NULL;

   if (!(XGetWindowProperty(dpy,
                               DefaultRootWindow(dpy),
                               XA_NET_SUPPORTING_WM_CHECK,
                               0,
                               1,
                               False,
                               XA_WINDOW,
                               &type,
                               &format,
                               &nitems,
                               &bytes_after,
                               &propdata) == Success &&
		   propdata))
	   return NULL;

   window = ((Window *) propdata)[0];

   XFree(propdata);

   if (!(XGetWindowProperty(dpy,
                               window,
                               XA_NET_WM_NAME,
                               0,
                               8192,
                               False,
                               XA_UTF8_STRING,
                               &type,
                               &format,
                               &nitems,
                               &bytes_after,
                               &propdata) == Success
		   && propdata))
	   return NULL;

   title = strdup((char *) propdata);
   XFree(propdata);

   return title;
}
