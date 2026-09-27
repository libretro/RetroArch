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

#ifdef HAVE_XRANDR
#include <X11/extensions/randr.h>
#endif

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#include <encodings/utf.h>
#include <retro_atomic.h>
#include <compat/strl.h>

#include "dbus_common.h"

#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../input/common/input_x11_common.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define _NET_WM_STATE_ADD                    1
#define MOVERESIZE_GRAVITY_CENTER            5
#define MOVERESIZE_X_SHIFT                   8
#define MOVERESIZE_Y_SHIFT                   9

#define V_DBLSCAN                            0x20

/* TODO/FIXME - globals */
/* Whether the pointer is inside the window. Written by the event pump
 * in x11_alive(), which runs on the video thread under the threaded
 * wrapper, and read by the input driver's poll on the runloop thread. */
retro_atomic_int_t g_x11_entered;
/* Keyboard focus on the window itself, from FocusIn/FocusOut. */
static retro_atomic_int_t g_x11_focused;
/* The window's size as VIDEO_SCALE_PACK, 0 while unknown. Written by
 * the event pump and read by the input driver's poll on the runloop
 * thread, so it is one word. */
retro_atomic_int_t g_x11_size;
Display *g_x11_dpy                          = NULL;
unsigned g_x11_screen                       = 0;
Window   g_x11_win                          = None;
Colormap g_x11_cmap;

/* TODO/FIXME - static globals */
#ifdef HAVE_XF86VM
static XF86VidModeModeInfo desktop_mode;
#endif
static bool xdg_screensaver_available       = true;
/* Whether the window is mapped - the compositor or WM has it on
 * screen. Written only by MapNotify and UnmapNotify below, and once at
 * input-context creation; nothing to do with keyboard focus, which
 * is g_x11_focused. */
static bool g_x11_mapped                    = false;
static bool g_x11_true_full                 = false;
static Atom XA_NET_WM_STATE;
static Atom XA_NET_WM_STATE_FULLSCREEN;
static Atom XA_NET_MOVERESIZE_WINDOW;
static Atom g_x11_quit_atom;
static XIM g_x11_xim;
static XIC g_x11_xic;

static enum retro_key x11_keysym_lut[RETROK_LAST];
static unsigned *x11_keysym_rlut            = NULL;
static unsigned x11_keysym_rlut_size        = 0;

static void x11_hide_mouse(Display *dpy, Window win)
{
   Cursor no_ptr;
   Pixmap bm_no;
   XColor black, dummy;
   static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};
   Colormap colormap        = DefaultColormap(dpy, DefaultScreen(dpy));

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

void x11_show_mouse(void *data, bool state)
{
   Display *dpy = g_x11_dpy;
   Window   win = g_x11_win;
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

/* Set the fullscreen state on the window before it is shown.
 * On GNOME + X11 (Mutter), fullscreen works properly when the
 * window carries the fullscreen hint as it is being mapped */
void x11_set_net_wm_fullscreen_hint(Display *dpy, Window win)
{
   Atom states[1]             = {0};

   XA_NET_WM_STATE            = XInternAtom(dpy, "_NET_WM_STATE", False);
   XA_NET_WM_STATE_FULLSCREEN = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
   states[0]                  = XA_NET_WM_STATE_FULLSCREEN;

   XChangeProperty(dpy, win, XA_NET_WM_STATE, XA_ATOM, 32,
         PropModeReplace, (const unsigned char*)states,
         sizeof(states) / sizeof(*states));
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
    if (!(hostname = (char*)malloc(scret + 1)))
        return;

    if (gethostname(hostname, scret + 1) == -1)
        RARCH_WARN("[X11] Failed to get hostname.\n");
    else
        XChangeProperty(dpy, win, XA_WM_CLIENT_MACHINE, XA_STRING, 8,
            PropModeReplace, (unsigned char *)hostname, strlen(hostname));
    free(hostname);
}

void x11_set_window_attr(Display *dpy, Window win)
{
   x11_set_window_class(dpy, win);
   x11_set_window_pid(dpy, win);
}

#ifdef HAVE_XSCRNSAVER
#include <X11/extensions/scrnsaver.h>
static bool xss_screensaver_inhibit(Display *dpy, bool enable)
{
    int dummy, min, maj;
    /* Guard against being called with a NULL Display.  This
     * happens with the SDL2 video driver under HAVE_X11 +
     * HAVE_XSCRNSAVER builds without HAVE_DBUS: SDL2's
     * sdl2_set_handles() in gfx/common/sdl2_common.c sets the
     * display *type* to RARCH_DISPLAY_X11 (so the gate in
     * x11_suspend_screensaver passes) and routes the SDL-owned
     * X Display through video_driver_display_set(), but the
     * file-scope g_x11_dpy here stays at its initial NULL --
     * it's only assigned in the xvideo / GL / X11-direct init
     * paths.  libX11's XQueryExtension() then SEGVs at a tiny
     * offset off the NULL display pointer.  Surfaced by the
     * ASan+UBSan CI workflow's headless SDL2 smoke (b9777c8 +
     * d967813), where dbus-1 isn't apt-installed but libXss is. */
    if (!dpy)
       return false;
    if (       !XScreenSaverQueryExtension(dpy, &dummy, &dummy)
            || !XScreenSaverQueryVersion(dpy, &maj, &min)
            || (maj < 1)
            || (maj == 1 && min < 1))
            return false;
    XScreenSaverSuspend(dpy, enable);
    XResetScreenSaver(dpy);
    return true;
}
#else
static bool xss_screensaver_inhibit(Display *dpy, bool enable) { return false; }
#endif

enum xdg_screensaver_de
{
   XDG_SCREENSAVER_DE_OTHER = 0,
   XDG_SCREENSAVER_DE_KDE,
   XDG_SCREENSAVER_DE_GNOME
};

/* The desktop xdg-screensaver will pick, read from the environment the
 * way it reads it: the first XDG_CURRENT_DESKTOP entry it knows, then
 * the classic session variables. Anything it would settle by asking a
 * session bus, or an override, reads as OTHER. */
static enum xdg_screensaver_de xdg_screensaver_desktop(void)
{
   static const struct
   {
      const char *name;
      enum xdg_screensaver_de de;
   } known[] =
   {
      { "KDE",           XDG_SCREENSAVER_DE_KDE   },
      { "GNOME",         XDG_SCREENSAVER_DE_GNOME },
      { "Cinnamon",      XDG_SCREENSAVER_DE_OTHER },
      { "X-Cinnamon",    XDG_SCREENSAVER_DE_OTHER },
      { "ENLIGHTENMENT", XDG_SCREENSAVER_DE_OTHER },
      { "DEEPIN",        XDG_SCREENSAVER_DE_OTHER },
      { "Deepin",        XDG_SCREENSAVER_DE_OTHER },
      { "deepin",        XDG_SCREENSAVER_DE_OTHER },
      { "DDE",           XDG_SCREENSAVER_DE_OTHER },
      { "LXDE",          XDG_SCREENSAVER_DE_OTHER },
      { "LXQt",          XDG_SCREENSAVER_DE_OTHER },
      { "MATE",          XDG_SCREENSAVER_DE_OTHER },
      { "XFCE",          XDG_SCREENSAVER_DE_OTHER },
      { "Budgie",        XDG_SCREENSAVER_DE_OTHER },
      { "X-Generic",     XDG_SCREENSAVER_DE_OTHER }
   };
   const char *env;
   const char *cur;

   if (     ((env = getenv("XDG_UTILS_OVERRIDE_DE")) && *env)
         || ((env = getenv("XDG_UTILS_SCREENSAVER_OVERRIDE_DE")) && *env)
         || access("/run/.toolboxenv", F_OK) == 0)
      return XDG_SCREENSAVER_DE_OTHER;

   if ((cur = getenv("XDG_CURRENT_DESKTOP")))
   {
      while (*cur)
      {
         size_t i;
         size_t _len = strcspn(cur, ":");
         for (i = 0; i < sizeof(known) / sizeof(known[0]); i++)
            if (     strlen(known[i].name) == _len
                  && !strncmp(cur, known[i].name, _len))
               return known[i].de;
         cur += _len;
         if (*cur == ':')
            cur++;
      }
   }

   if ((env = getenv("KDE_FULL_SESSION")) && *env)
      return XDG_SCREENSAVER_DE_KDE;
   if ((env = getenv("GNOME_DESKTOP_SESSION_ID")) && *env)
      return XDG_SCREENSAVER_DE_GNOME;
   return XDG_SCREENSAVER_DE_OTHER;
}

/* Probe once for xdg-screensaver and its xset backend dependency.
 * xdg-screensaver's "X11" backend shells out to xset; if xset is missing
 * (common on minimal installs / containers / some WMs without
 * x11-xserver-utils), invoking xdg-screensaver spams stderr with
 * "xset: not found" and "Illegal number" without us ever knowing why.
 * Check up front so we can silently no-op instead. */
static bool xdg_screensaver_probe(void)
{
   const char *env;
   int ret;
   /* Both are needed: xdg-screensaver itself, and xset which it execs.
    * `command -v` is a POSIX shell builtin so this works under /bin/sh
    * on every platform that has system(). Redirecting both streams
    * keeps the probe silent. */
   ret = system("command -v xdg-screensaver >/dev/null 2>&1 && "
                "command -v xset >/dev/null 2>&1");
   if (ret == -1 || WEXITSTATUS(ret) != 0)
   {
      RARCH_LOG("[X11] xdg-screensaver or xset not available; screensaver suspension disabled.\n");
      return false;
   }

   /* On KDE 4 and later and on GNOME 3, "suspend" hands the inhibit to
    * a Perl helper that talks D-Bus through Net::DBus and X11::Protocol.
    * It runs detached, so a missing module fails it on stderr while
    * xdg-screensaver itself still exits 0. */
   switch (xdg_screensaver_desktop())
   {
      case XDG_SCREENSAVER_DE_KDE:
         if (!(env = getenv("KDE_SESSION_VERSION")) || !*env)
            return true;
         ret = system("perl -MNet::DBus -MX11::Protocol -e 1 "
                      ">/dev/null 2>&1");
         break;
      case XDG_SCREENSAVER_DE_GNOME:
         /* GNOME 2 is told apart by this tool and has its own backend. */
         ret = system("command -v gnome-default-applications-properties "
                      ">/dev/null 2>&1 || "
                      "perl -MNet::DBus -MX11::Protocol -e 1 "
                      ">/dev/null 2>&1");
         break;
      default:
         return true;
   }
   if (ret == -1 || WEXITSTATUS(ret) != 0)
   {
      RARCH_LOG("[X11] xdg-screensaver needs Perl's Net::DBus and X11::Protocol on this desktop; screensaver suspension disabled.\n");
      return false;
   }
   return true;
}

static void xdg_screensaver_inhibit(Window wnd)
{
   int  ret;
   size_t _len;
   char cmd[64];
   char title[128];

   title[0] = '\0';

   RARCH_LOG("[X11] Suspending screensaver (X11, xdg-screensaver).\n");

   if (g_x11_dpy && g_x11_win)
   {
      /* Make sure the window has a title, even if it's a bogus one, otherwise
       * xdg-screensaver will fail and report to stderr, framing RA for its bug.
       * A single space character is used so that the title bar stays visibly
       * the same, as if there's no title at all. */
      size_t title_len = video_driver_get_window_title(title, sizeof(title));
      if (title_len == 0)
         title_len = strlcpy_lit(title, " ", sizeof(title));
      XChangeProperty(g_x11_dpy, g_x11_win, XA_WM_NAME, XA_STRING,
            8, PropModeReplace, (const unsigned char*) title, title_len);

#ifdef X_HAVE_UTF8_STRING
      /* Also set the EWMH _NET_WM_NAME (UTF8_STRING). Without this, a
       * title containing non-Latin-1 characters set via the legacy
       * WM_NAME above is rendered garbled by EWMH-aware window managers,
       * and this code path (which runs on screensaver inhibit, i.e. on
       * window creation and game open/close transitions) would otherwise
       * leave a stale legacy-only title behind. Purely additive: if the
       * atoms are unavailable, WM_NAME remains the sole fallback. */
      {
         Atom XA_NET_WM_NAME      = XInternAtom(g_x11_dpy, "_NET_WM_NAME",      False);
         Atom XA_NET_WM_ICON_NAME = XInternAtom(g_x11_dpy, "_NET_WM_ICON_NAME", False);
         Atom XA_UTF8_STRING      = XInternAtom(g_x11_dpy, "UTF8_STRING",       False);

         if (XA_UTF8_STRING)
         {
            if (XA_NET_WM_NAME)
               XChangeProperty(g_x11_dpy, g_x11_win, XA_NET_WM_NAME,
                     XA_UTF8_STRING, 8, PropModeReplace,
                     (const unsigned char*)title, title_len);
            if (XA_NET_WM_ICON_NAME)
               XChangeProperty(g_x11_dpy, g_x11_win, XA_NET_WM_ICON_NAME,
                     XA_UTF8_STRING, 8, PropModeReplace,
                     (const unsigned char*)title, title_len);
         }
      }
#endif
   }

   _len = strlcpy_lit(cmd, "xdg-screensaver suspend 0x", sizeof(cmd));
   snprintf(cmd + _len, sizeof(cmd) - _len, "%x", (int)wnd);

   if ((ret = system(cmd)) == -1)
   {
      xdg_screensaver_available = false;
      RARCH_WARN("[X11] Failed to launch xdg-screensaver.\n");
   }
   else if (WEXITSTATUS(ret))
   {
      xdg_screensaver_available = false;
      RARCH_WARN("[X11] Could not suspend screensaver.\n");
   }
}

/* xdg-screensaver, the last resort: its suspend lasts as long as the
 * window, so it is only started when nothing else holds the
 * screensaver. */
static void x11_xdg_screensaver_fallback(Window wnd)
{
   static bool probed = false;
   if (!xdg_screensaver_available)
      return;
   if (!probed)
   {
      xdg_screensaver_available = xdg_screensaver_probe();
      probed = true;
   }
   if (xdg_screensaver_available)
      xdg_screensaver_inhibit(wnd);
}

#ifdef RARCH_HAVE_DBUS_SCREENSAVER
/* Set while the D-Bus worker has not yet said whether it inhibited the
 * screensaver and nothing else holds it; x11_check_window() then starts
 * the xdg-screensaver fallback once D-Bus is known to have failed. */
static bool g_x11_xdg_deferred = false;
#endif

bool x11_suspend_screensaver(void *data, bool enable)
{
   Window wnd;
   bool dbus_asked = false;
   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
      return false;
   wnd = video_driver_window_get();
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
   /* D-Bus answers on its own worker; XScreenSaver is a request on this
    * thread's display, so it is asked alongside rather than after. */
   dbus_asked         = dbus_suspend_screensaver(enable);
   g_x11_xdg_deferred = false;
#endif
   if (!xss_screensaver_inhibit(g_x11_dpy, enable) && enable)
   {
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
      if (dbus_asked)
      {
         if (dbus_screensaver_state() == DBUS_SCREENSAVER_FAILED)
            x11_xdg_screensaver_fallback(wnd);
         else if (dbus_screensaver_state() == DBUS_SCREENSAVER_PENDING)
            g_x11_xdg_deferred = true;
         return true;
      }
#endif
      x11_xdg_screensaver_fallback(wnd);
   }
   return true;
}

#ifdef HAVE_XF86VM
float x11_get_refresh_rate(void *data)
{
   XWindowAttributes attr;
   XF86VidModeModeLine modeline;
   Screen *screen;
   int screenid;
   int dotclock;

   if (!g_x11_dpy || g_x11_win == None)
      return 0.0f;

   if (!XGetWindowAttributes(g_x11_dpy, g_x11_win, &attr))
      return 0.0f;

   screen = attr.screen;
   screenid = XScreenNumberOfScreen(screen);

   /* A server without the extension (Xvfb, some nested and remote
    * servers) leaves the modeline unset, and a zero total made this
    * a NaN or an infinity that the callers then paced by. Unknown is
    * 0, as the other paths here report it. */
   dotclock = 0;
   memset(&modeline, 0, sizeof(modeline));
   if (!XF86VidModeGetModeLine(g_x11_dpy, screenid, &dotclock, &modeline)
         || !modeline.htotal || !modeline.vtotal || !dotclock)
      return 0.0f;

   /* non-native modes like 1080p on a 4K display might use DoubleScan */
   if (modeline.flags & V_DBLSCAN)
      dotclock /= 2;

   return (float)dotclock * 1000.0f / modeline.htotal / modeline.vtotal;
}

static bool get_video_mode(
      Display *dpy, unsigned width, unsigned height,
      XF86VidModeModeInfo *mode, XF86VidModeModeInfo *x11_desktop_mode)
{
   int i, num_modes                = 0;
   bool ret                        = false;
   float refresh_mod               = 0.0f;
   float minimum_fps_diff          = 0.0f;
   XF86VidModeModeInfo **modes     = NULL;
   settings_t *settings            = config_get_ptr();
   unsigned black_frame_insertion  = settings->uints.video_black_frame_insertion;
   float video_refresh_rate        = settings->floats.video_refresh_rate;

   XF86VidModeGetAllModeLines(dpy, DefaultScreen(dpy), &num_modes, &modes);

   if (!num_modes)
   {
      XFree(modes);
      return false;
   }

   *x11_desktop_mode = *modes[0];

   /* If we use black frame insertion, we fake a 60 Hz monitor
    * for 120 Hz one, etc, so try to match that. */
   refresh_mod = 1.0f / (black_frame_insertion + 1.0f);

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
      diff    = fabsf(refresh - video_refresh_rate);

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

bool x11_enter_fullscreen(
      Display *dpy, unsigned width,
      unsigned height)
{
   XF86VidModeModeInfo mode;

   if (!get_video_mode(dpy, width, height, &mode, &desktop_mode))
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
#endif

static void x11_init_keyboard_lut(void)
{
   const struct rarch_key_map *map       = rarch_key_map_x11;
   const struct rarch_key_map *map_start = rarch_key_map_x11;

   memset(x11_keysym_lut, 0, sizeof(x11_keysym_lut));
   x11_keysym_rlut_size = 0;

   for (; map->rk != RETROK_UNKNOWN; map++)
   {
      x11_keysym_lut[map->rk] = (enum retro_key)map->sym;
      if (map->sym > x11_keysym_rlut_size)
         x11_keysym_rlut_size = map->sym;
   }

   if (x11_keysym_rlut_size < 65536)
   {
      if (x11_keysym_rlut)
         free(x11_keysym_rlut);

      /* NULL-check the calloc: the populate loop below dereferences
       * x11_keysym_rlut[map->sym] unconditionally, so an OOM here
       * would segfault.  The reader (x11_keysym_lookup, line ~493)
       * guards with 'if (x11_keysym_rlut && sym < size)', so leaving
       * the pointer NULL on failure cleanly disables the rlut path
       * without further damage. */
      if (!(x11_keysym_rlut = (unsigned*)calloc(++x11_keysym_rlut_size, sizeof(unsigned))))
         x11_keysym_rlut_size = 0;
      else
         for (map = map_start; map->rk != RETROK_UNKNOWN; map++)
            x11_keysym_rlut[map->sym] = (enum retro_key)map->rk;
   }
   else
      x11_keysym_rlut_size = 0;
}

static void x11_destroy_input_context(XIM *xim, XIC *xic)
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

   memset(x11_keysym_lut, 0, sizeof(x11_keysym_lut));
   if (x11_keysym_rlut)
   {
      free(x11_keysym_rlut);
      x11_keysym_rlut = NULL;
   }
   x11_keysym_rlut_size = 0;
}


static bool x11_create_input_context(Display *dpy,
      Window win, XIM *xim, XIC *xic)
{
   x11_destroy_input_context(xim, xic);
   x11_init_keyboard_lut();

   g_x11_mapped = true;

   if (!(*xim = XOpenIM(dpy, NULL, NULL, NULL)))
   {
      RARCH_ERR("[X11] Failed to open input method.\n");
      return false;
   }

   if (!(*xic = XCreateIC(*xim, XNInputStyle,
         XIMPreeditNothing | XIMStatusNothing, XNClientWindow, win, NULL)))
   {
      RARCH_ERR("[X11] Failed to create input context.\n");
      return false;
   }

   XSetICFocus(*xic);
   return true;
}


static enum retro_key x11_translate_keysym_to_rk(unsigned sym)
{
   size_t i;

   /* Fast path */
   if (x11_keysym_rlut && sym < x11_keysym_rlut_size)
      return (enum retro_key)x11_keysym_rlut[sym];

   /* Slow path */
   for (i = 0; i < ARRAY_SIZE(x11_keysym_lut); i++)
   {
      if (x11_keysym_lut[i] != sym)
         continue;

      return (enum retro_key)i;
   }

   return RETROK_UNKNOWN;
}

static void x11_handle_key_event(unsigned keycode, XEvent *event,
      XIC ic, bool filter)
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
         num = XLookupString(&event->xkey, keybuf,
               sizeof(keybuf), &keysym, NULL); /* ASCII only. */
         for (i = 0; i < num; i++)
            chars[i] = keybuf[i] & 0x7f;
#endif
      }
      else
         keysym = XLookupKeysym(&event->xkey,
               (state & ShiftMask) || (state & LockMask));
   }

   /* We can't feed uppercase letters to the keycode translator.
    * Seems like a bad idea to feed it keysyms anyway, so here
    * is a little hack...
    **/
   if (keysym >= XK_A && keysym <= XK_Z)
       keysym += XK_z - XK_Z;

   /* Get the real keycode, that correctly ignores international layouts
    * as windows code does. */
   key     = x11_translate_keysym_to_rk(keycode);

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
   if (state & Mod3Mask)
      mod |= RETROKMOD_SCROLLOCK;
   if (state & Mod4Mask)
      mod |= RETROKMOD_META;

   input_keyboard_event(down, key, chars[0], mod, RETRO_DEVICE_KEYBOARD);

   for (i = 1; i < num; i++)
      input_keyboard_event(down, RETROK_UNKNOWN,
            chars[i], mod, RETRO_DEVICE_KEYBOARD);
}

bool x11_alive(void *data)
{
#ifdef HAVE_XRANDR
   int randr   = retro_atomic_load_acquire_int(&g_x11_randr_state);
   int rr_base = randr & X11_RANDR_BASE_MASK;
   /* From here on every RandR change reaches the kept refresh rate
    * through this pump. */
   if (rr_base && !(randr & X11_RANDR_PUMPED))
   {
      retro_atomic_fetch_or_int(&g_x11_randr_state, X11_RANDR_PUMPED);
      x11_refresh_invalidate();
   }
#endif

   while (XPending(g_x11_dpy))
   {
      XEvent event;
      bool filter = false;
      unsigned keycode = 0;

      /* Can get events from older windows. Check this. */
      XNextEvent(g_x11_dpy, &event);

#ifdef HAVE_XRANDR
      /* Screen, crtc or output change, selected on the root window by
       * the X display server. */
      if (     rr_base
            && event.type >= rr_base + RRScreenChangeNotify
            && event.type <= rr_base + RRNotify)
      {
         x11_refresh_invalidate();
         continue;
      }
#endif

      /* IMPORTANT - Get keycode before XFilterEvent
         because the event is localizated after the call */
      keycode = event.xkey.keycode;
      filter  = XFilterEvent(&event, g_x11_win);

      switch (event.type)
      {
         case ClientMessage:
            if (        event.xclient.window    == g_x11_win &&
                  (Atom)event.xclient.data.l[0] == g_x11_quit_atom)
               frontend_driver_set_signal_handler_state(1);
            break;

         case DestroyNotify:
            if (event.xdestroywindow.window == g_x11_win)
               frontend_driver_set_signal_handler_state(1);
            break;

         case MapNotify:
            if (event.xmap.window == g_x11_win)
               g_x11_mapped = true;
            break;

         case UnmapNotify:
            if (event.xunmap.window == g_x11_win)
               g_x11_mapped = false;
            break;

         case ConfigureNotify:
            if (event.xconfigure.window == g_x11_win)
               retro_atomic_store_relaxed_int(&g_x11_size,
                     (int)VIDEO_SCALE_PACK(event.xconfigure.width,
                        event.xconfigure.height));
            break;

         /* Grabs leave the focus where it was. */
         case FocusIn:
            if (     event.xfocus.window == g_x11_win
                  && event.xfocus.mode   != NotifyGrab
                  && event.xfocus.mode   != NotifyUngrab
                  && (   event.xfocus.detail == NotifyAncestor
                      || event.xfocus.detail == NotifyInferior
                      || event.xfocus.detail == NotifyNonlinear))
               retro_atomic_store_relaxed_int(&g_x11_focused, 1);
            break;

         case FocusOut:
            if (     event.xfocus.window == g_x11_win
                  && event.xfocus.mode   != NotifyGrab
                  && event.xfocus.mode   != NotifyUngrab
                  && event.xfocus.detail != NotifyPointer)
               retro_atomic_store_relaxed_int(&g_x11_focused, 0);
            break;

         case ButtonPress:
            switch (event.xbutton.button)
            {
               case 1: /* Left click */
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
               case 8: /* Mouse button 4 */
               case 9: /* Mouse button 5 */
                  x_input_poll_wheel(&event.xbutton, true);
                  break;
            }
            break;

         case EnterNotify:
            retro_atomic_store_relaxed_int(&g_x11_entered, 1);
            break;

         case LeaveNotify:
            retro_atomic_store_relaxed_int(&g_x11_entered, 0);
            break;

         case ButtonRelease:
            switch (event.xbutton.button)
            {
               case 8: /* Mouse button 4 - not handled as click */
               case 9: /* Mouse button 5 - not handled as click */
                  x_input_poll_wheel(&event.xbutton, true);
                  break;
            }
            break;

         case KeyRelease:
            /*  When you receive a key release and the next event
             * is a key press of the same key combination,
             * then it's auto-repeat and the key wasn't
             * actually released. */
            if (XEventsQueued(g_x11_dpy, QueuedAfterReading))
            {
               XEvent next_event;
               XPeekEvent(g_x11_dpy, &next_event);
               if (   next_event.type         == KeyPress
                   && next_event.xkey.time    == event.xkey.time
                   && next_event.xkey.keycode == event.xkey.keycode)
                  break; /* Key wasn't actually released */
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
   bool *resize, unsigned *dims)
{
   unsigned new_dims  = *dims;
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
   if (g_x11_xdg_deferred)
   {
      enum dbus_screensaver_state st = dbus_screensaver_state();
      if (st != DBUS_SCREENSAVER_PENDING)
      {
         g_x11_xdg_deferred = false;
         if (st == DBUS_SCREENSAVER_FAILED)
            x11_xdg_screensaver_fallback(video_driver_window_get());
      }
   }
#endif
   x11_get_video_size(data, &new_dims);

   if (new_dims != *dims)
   {
      *dims  = new_dims;
      *resize = true;
   }

   x11_alive(data);

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

void x11_get_video_size(void *data, unsigned *dims)
{
   if (!g_x11_dpy || g_x11_win == None)
   {
      Display *dpy = (Display*)XOpenDisplay(NULL);
      *dims = VIDEO_SCALE_PACK(0, 0);

      if (dpy)
      {
         int screen = DefaultScreen(dpy);
         *dims = VIDEO_SCALE_PACK(DisplayWidth(dpy, screen),
               DisplayHeight(dpy, screen));
         XCloseDisplay(dpy);
      }
   }
   else
   {
      unsigned size = (unsigned)retro_atomic_load_relaxed_int(&g_x11_size);
      if (VIDEO_SCALE_W(size) && VIDEO_SCALE_H(size))
         *dims = size;
      else
      {
         XWindowAttributes target;
         XGetWindowAttributes(g_x11_dpy, g_x11_win, &target);

         *dims = VIDEO_SCALE_PACK(target.width, target.height);
      }
   }
}

bool x11_has_focus_internal(void *data)
{
   return g_x11_mapped;
}

/* Nothing to present to while the window is unmapped - minimised, on
 * another workspace, or withdrawn. The X server discards the drawing
 * and neither glXSwapBuffers nor a Vulkan present blocks, so with the
 * display as the only pacing the loop would spin. Unfocused is not
 * unmapped and is deliberately not tested here: a visible window that
 * happens not to have the keyboard must keep running at full rate. */
bool x11_presentable(void *data)
{
   return g_x11_mapped;
}

bool x11_has_focus(void *data)
{
   return (   retro_atomic_load_relaxed_int(&g_x11_focused)
           && g_x11_mapped)
      || g_x11_true_full;
}

bool x11_connect(void)
{
   frontend_driver_destroy_signal_handler_state();

   /* Keep one g_x11_dpy alive the entire process lifetime.
    * This is necessary for nVidia's EGL implementation for now. */
   if (!g_x11_dpy)
      if (!(g_x11_dpy = XOpenDisplay(NULL)))
         return false;

#ifdef RARCH_HAVE_DBUS_SCREENSAVER
   dbus_ensure_connection();
#endif

   retro_atomic_store_relaxed_int(&g_x11_size, 0);
   retro_atomic_store_relaxed_int(&g_x11_focused, 0);

   return true;
}

void x11_update_title(void *data)
{
   size_t _len;
   char title[128];
   title[0]  = '\0';
   _len      = video_driver_get_window_title(title, sizeof(title));
   if (title[0])
   {
      /* Legacy ICCCM property. Kept for window managers that do not
       * understand EWMH. The encoding of WM_NAME is nominally STRING
       * (Latin-1), but RetroArch's title may contain UTF-8; this is
       * preserved as-is to avoid changing long-standing behaviour for
       * old clients, which is exactly what they receive today. */
      XChangeProperty(g_x11_dpy, g_x11_win, XA_WM_NAME, XA_STRING,
            8, PropModeReplace, (const unsigned char*)title, _len);

#ifdef X_HAVE_UTF8_STRING
      /* EWMH properties. Window managers that implement EWMH prefer
       * _NET_WM_NAME (UTF8_STRING) over WM_NAME, so non-Latin-1 titles
       * (e.g. Japanese ROM names) render correctly. This is purely
       * additive: the atoms are interned at runtime and, if either is
       * unavailable, the legacy WM_NAME above is the sole fallback, so
       * older clients are never broken. */
      {
         Atom XA_NET_WM_NAME      = XInternAtom(g_x11_dpy, "_NET_WM_NAME",      False);
         Atom XA_NET_WM_ICON_NAME = XInternAtom(g_x11_dpy, "_NET_WM_ICON_NAME", False);
         Atom XA_UTF8_STRING      = XInternAtom(g_x11_dpy, "UTF8_STRING",       False);

         if (XA_UTF8_STRING)
         {
            if (XA_NET_WM_NAME)
               XChangeProperty(g_x11_dpy, g_x11_win, XA_NET_WM_NAME,
                     XA_UTF8_STRING, 8, PropModeReplace,
                     (const unsigned char*)title, _len);
            if (XA_NET_WM_ICON_NAME)
               XChangeProperty(g_x11_dpy, g_x11_win, XA_NET_WM_ICON_NAME,
                     XA_UTF8_STRING, 8, PropModeReplace,
                     (const unsigned char*)title, _len);
         }
      }
#endif
   }
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
   retro_atomic_store_relaxed_int(&g_x11_size, 0);
   retro_atomic_store_relaxed_int(&g_x11_focused, 0);

#ifdef RARCH_HAVE_DBUS_SCREENSAVER
   g_x11_xdg_deferred = false;
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

/* Without a window manager no ConfigureNotify follows the map. */
void x11_event_queue_check(XEvent *event)
{
   XWindowAttributes target;
   XIfEvent(g_x11_dpy, event, x11_wait_notify, NULL);
   if (XGetWindowAttributes(g_x11_dpy, g_x11_win, &target))
      retro_atomic_store_relaxed_int(&g_x11_size,
            (int)VIDEO_SCALE_PACK(target.width, target.height));
}

static bool x11_check_atom_supported(Display *dpy, Atom atom)
{
   Atom XA_NET_SUPPORTED = XInternAtom(dpy, "_NET_SUPPORTED", True);
   Atom type;
   int format;
   unsigned long nitems      = 0;
   unsigned long bytes_after = 0;
   Atom *prop                = NULL;
   int i;

   if (XA_NET_SUPPORTED == None)
      return false;

   /* On failure XGetWindowProperty() leaves the return parameters
    * undefined, so prop has to start NULL and the status has to be
    * tested -- reading an uninitialised pointer is the one outcome
    * the NULL test below cannot catch.  x11_get_wm_name() a few lines
    * down already does both. */
   if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), XA_NET_SUPPORTED,
         0, UINT_MAX, False, XA_ATOM, &type, &format, &nitems,
         &bytes_after, (unsigned char **)&prop) != Success)
      return false;

   if (!prop)
      return false;

   if (type != XA_ATOM)
   {
      XFree(prop);
      return false;
   }

   for (i = 0; i < (int)nitems; i++)
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

   /* A _NET_SUPPORTING_WM_CHECK that exists but carries nothing still
    * yields a non-NULL propdata; reading element zero of it is out of
    * bounds. */
   if (nitems < 1)
   {
      XFree(propdata);
      return NULL;
   }

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
