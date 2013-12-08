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

#include "x11_common.h"
#include <stdlib.h>
#include <string.h>
#include <X11/Xatom.h>
#include "../image.h"
#include "../../general.h"
#include "../../input/input_common.h"

static void x11_hide_mouse(Display *dpy, Window win)
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

void x11_show_mouse(Display *dpy, Window win, bool state)
{
   if (state)
      XUndefineCursor(dpy, win);
   else
      x11_hide_mouse(dpy, win);
}

static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_MOVERESIZE_WINDOW;
#define XA_INIT(x) XA##x = XInternAtom(dpy, #x, False)
#define _NET_WM_STATE_ADD 1
#define MOVERESIZE_GRAVITY_CENTER 5
#define MOVERESIZE_X_SHIFT 8
#define MOVERESIZE_Y_SHIFT 9
void x11_windowed_fullscreen(Display *dpy, Window win)
{
   XA_INIT(_NET_WM_STATE);
   XA_INIT(_NET_WM_STATE_FULLSCREEN);

   XEvent xev = {0};

   xev.xclient.type = ClientMessage;
   xev.xclient.send_event = True;
   xev.xclient.message_type = XA_NET_WM_STATE;
   xev.xclient.window = win;
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
   xev.xclient.data.l[1] = XA_NET_WM_STATE_FULLSCREEN;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

// Try to be nice to tiling WMs if possible.
void x11_move_window(Display *dpy, Window win, int x, int y,
      unsigned width, unsigned height)
{
   XA_INIT(_NET_MOVERESIZE_WINDOW);

   XEvent xev = {0};

   xev.xclient.type = ClientMessage;
   xev.xclient.send_event = True;
   xev.xclient.message_type = XA_NET_MOVERESIZE_WINDOW;
   xev.xclient.window = win;
   xev.xclient.format = 32;
   xev.xclient.data.l[0] = (1 << MOVERESIZE_X_SHIFT) | (1 << MOVERESIZE_Y_SHIFT);
   xev.xclient.data.l[1] = x;
   xev.xclient.data.l[2] = y;

   XSendEvent(dpy, DefaultRootWindow(dpy), False,
         SubstructureRedirectMask | SubstructureNotifyMask,
         &xev);
}

static void x11_set_window_class(Display *dpy, Window win)
{
   XClassHint hint = {0};
   hint.res_name   = (char*)"retroarch"; // Broken header.
   hint.res_class  = (char*)"retroarch";
   XSetClassHint(dpy, win, &hint);
}

void x11_set_window_attr(Display *dpy, Window win)
{
   x11_set_window_class(dpy, win);
}

void x11_suspend_screensaver(Window wnd)
{
   char cmd[64];
   snprintf(cmd, sizeof(cmd), "xdg-screensaver suspend %d", (int)wnd);

   int ret = system(cmd);

   if (ret != 0)
      RARCH_WARN("Could not suspend screen saver.\n");
}

static bool get_video_mode(Display *dpy, unsigned width, unsigned height, XF86VidModeModeInfo *mode, XF86VidModeModeInfo *desktop_mode)
{
   int i;
   int num_modes = 0;
   XF86VidModeModeInfo **modes = NULL;
   XF86VidModeGetAllModeLines(dpy, DefaultScreen(dpy), &num_modes, &modes);

   if (!num_modes)
   {
      XFree(modes);
      return false;
   }

   *desktop_mode = *modes[0];

   bool ret = false;
   for (i = 0; i < num_modes; i++)
   {
      if (modes[i]->hdisplay == width && modes[i]->vdisplay == height)
      {
         *mode = *modes[i];
         ret = true;
         break;
      }
   }

   XFree(modes);
   return ret;
}

bool x11_enter_fullscreen(Display *dpy, unsigned width, unsigned height, XF86VidModeModeInfo *desktop_mode)
{
   XF86VidModeModeInfo mode;
   if (get_video_mode(dpy, width, height, &mode, desktop_mode))
   {
      if (XF86VidModeSwitchToMode(dpy, DefaultScreen(dpy), &mode))
      {
         XF86VidModeSetViewPort(dpy, DefaultScreen(dpy), 0, 0);
         return true;
      }
      else
         return false;
   }
   else
      return false;
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
   int i;
   bool ret = false;

   int num_screens = 0;
   XineramaScreenInfo *info = x11_query_screens(dpy, &num_screens);
   RARCH_LOG("[X11]: Xinerama screens: %d.\n", num_screens);

   for (i = 0; i < num_screens; i++)
   {
      if (info[i].screen_number == screen)
      {
         *x = info[i].x_org;
         *y = info[i].y_org;
         *w = info[i].width;
         *h = info[i].height;
         ret = true;
         break;
      }
   }

   XFree(info);
   return ret;
}

unsigned x11_get_xinerama_monitor(Display *dpy, int x, int y,
      int w, int h)
{
   int i;
   unsigned monitor = 0;
   int largest_area = 0;

   int num_screens = 0;
   XineramaScreenInfo *info = x11_query_screens(dpy, &num_screens);
   RARCH_LOG("[X11]: Xinerama screens: %d.\n", num_screens);

   for (i = 0; i < num_screens; i++)
   {
      int max_lx = max(x, info[i].x_org);
      int min_rx = min(x + w, info[i].x_org + info[i].width);
      int max_ty = max(y, info[i].y_org);
      int min_by = min(y + h, info[i].y_org + info[i].height);

      int len_x = min_rx - max_lx;
      int len_y = min_by - max_ty;
      if (len_x < 0 || len_y < 0) // The whole window is outside the screen.
         continue;

      int area = len_x * len_y;
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
   *xim = XOpenIM(dpy, NULL, NULL, NULL);
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

static inline unsigned leading_ones(uint8_t c)
{
   unsigned ones = 0;
   while (c & 0x80)
   {
      ones++;
      c <<= 1;
   }

   return ones;
}

// Simple implementation. Assumes the sequence is properly synchronized and terminated.
static size_t conv_utf8_utf32(uint32_t *out, size_t out_chars, const char *in, size_t in_size)
{
   unsigned i;
   size_t ret = 0;
   while (in_size && out_chars)
   {
      uint8_t first = *in++;

      unsigned ones = leading_ones(first);
      if (ones > 6 || ones == 1) // Invalid or desync
         break;

      unsigned extra = ones ? ones - 1 : ones;
      if (1 + extra > in_size) // Overflow
         break;

      unsigned shift = (extra - 1) * 6;
      uint32_t c = (first & ((1 << (7 - ones)) - 1)) << (6 * extra);

      for (i = 0; i < extra; i++, in++, shift -= 6)
         c |= (*in & 0x3f) << shift;

      *out++ = c;
      in_size -= 1 + extra;
      out_chars--;
      ret++;
   }

   return ret;
}

void x11_handle_key_event(XEvent *event, XIC ic, bool filter)
{
   if (!g_extern.system.key_event)
      return;

   int i;
   char keybuf[32] = {0};
   uint32_t chars[32] = {0};

   bool down    = event->type == KeyPress;
   unsigned key = input_translate_keysym_to_rk(XLookupKeysym(&event->xkey, 0));
   int num      = 0;

   if (down && !filter)
   {
      KeySym keysym = 0;

#ifdef X_HAVE_UTF8_STRING
      Status status = 0;

      // XwcLookupString doesn't seem to work.
      num = Xutf8LookupString(ic, &event->xkey, keybuf, ARRAY_SIZE(keybuf), &keysym, &status);

      // libc functions need UTF-8 locale to work properly, which makes mbrtowc a bit impractical.
      // Use custom utf8 -> UTF-32 conversion.
      num = conv_utf8_utf32(chars, ARRAY_SIZE(chars), keybuf, num);
#else
      (void)ic;
      num = XLookupString(&event->xkey, keybuf, sizeof(keybuf), &keysym, NULL); // ASCII only.
      for (i = 0; i < num; i++)
         chars[i] = keybuf[i] & 0x7f;
#endif
   }

   unsigned state = event->xkey.state;
   uint16_t mod = 0;
   mod |= (state & ShiftMask) ? RETROKMOD_SHIFT : 0;
   mod |= (state & LockMask) ? RETROKMOD_CAPSLOCK : 0;
   mod |= (state & ControlMask) ? RETROKMOD_CTRL : 0;
   mod |= (state & Mod1Mask) ? RETROKMOD_ALT : 0;
   mod |= (state & Mod4Mask) ? RETROKMOD_META : 0;

   g_extern.system.key_event(down, key, chars[0], mod);
   for (i = 1; i < num; i++)
      g_extern.system.key_event(down, RETROK_UNKNOWN, chars[i], mod);
}

