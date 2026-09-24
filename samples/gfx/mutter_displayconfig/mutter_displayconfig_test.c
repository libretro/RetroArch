/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (mutter_displayconfig_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Screen Resolution under GNOME, through Mutter's DisplayConfig D-Bus
 * interface, and proof that nothing changes anywhere else.
 *
 * On GNOME's Wayland session RetroArch's X11 driver runs on XWayland,
 * which lists every mode at the desktop's current rate (a 240 Hz panel
 * shows no 60 Hz mode) and "switches" by scaling the window; the
 * Wayland driver had no resolution list at all. Mutter's D-Bus
 * interface is the only way to the real modes, and gfx/common/
 * mutter_displayconfig.c now uses it from both drivers.
 *
 * Run by run.sh, which starts a private session bus and, per case,
 * mock_mutter.py (Mutter's signatures and its ApplyMonitorsConfig
 * checks), a real Xorg dummy server, or Xwayland under a headless
 * Weston. One case per invocation:
 *
 *   nobus          no session bus: everything unavailable, nothing
 *                  autolaunched
 *   nomutter       a bus without Mutter: unavailable
 *   mutter         the list, switches (rate, size with the neighbour
 *                  moved, rate only, whole hertz, another head by
 *                  connector), refusals, and what went on the wire
 *   x11-xorg       dispserv_x11 on a real X server with Mutter on the
 *                  bus: the XRandR list, and Mutter never asked
 *   x11-xwayland   dispserv_x11 on XWayland with Mutter: Mutter's list
 *                  and Mutter's switch
 *   x11-xwayland-nomutter  XWayland without Mutter: XWayland's list,
 *                  exactly as before
 *   wl             dispserv_wl with Mutter: the list, the switch, and
 *                  the resolution list not flagged away
 *   wl-nomutter    dispserv_wl without Mutter: flagged away, as before
 *   x11-real-mutter  dispserv_x11 on the XWayland of a real headless
 *                  Mutter, whose outputs are named after connectors:
 *                  Mutter's modes, not XWayland's
 */

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <retro_miscellaneous.h>

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/common/mutter_displayconfig.h"

#ifdef TEST_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
Display *g_x11_dpy    = NULL;
Window   g_x11_win    = 0;
int      g_x11_screen = 0;
extern const video_display_server_t dispserv_x11;
#endif

#ifdef TEST_WL
struct wl_display;
void wayland_drm_lease_report(struct wl_display *dpy) { (void)dpy; }
extern const video_display_server_t dispserv_wl;
#endif

static int s_verbose;

static void vlog(const char *fmt, va_list ap)
{
   if (s_verbose)
      vfprintf(stderr, fmt, ap);
}
void RARCH_DBG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }
void RARCH_LOG(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)  { va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap); }
void video_monitor_set_refresh_rate(float hz) { (void)hz; }

/* ------------------------------------------------------------------
 * The mock's test-only methods
 * ------------------------------------------------------------------ */

static DBusMessage *mock_call(const char *method)
{
   DBusError err;
   DBusMessage *msg, *reply;
   DBusConnection *c;

   dbus_error_init(&err);
   if (!(c = dbus_bus_get_private(DBUS_BUS_SESSION, &err)))
   {
      dbus_error_free(&err);
      return NULL;
   }
   msg   = dbus_message_new_method_call("org.gnome.Mutter.DisplayConfig",
         "/org/gnome/Mutter/DisplayConfig", "org.gnome.Mutter.DisplayConfig",
         method);
   reply = dbus_connection_send_with_reply_and_block(c, msg, 2000, &err);
   dbus_message_unref(msg);
   if (dbus_error_is_set(&err))
      dbus_error_free(&err);
   dbus_connection_close(c);
   dbus_connection_unref(c);
   return reply;
}

static void mock_last(char *s, size_t len)
{
   const char *v    = "";
   DBusMessage *r   = mock_call("LastApply");
   if (r)
      dbus_message_get_args(r, NULL, DBUS_TYPE_STRING, &v, DBUS_TYPE_INVALID);
   snprintf(s, len, "%s", v);
   if (r)
      dbus_message_unref(r);
}

static void mock_calls(unsigned *get, unsigned *apply)
{
   dbus_uint32_t g = 0, a = 0;
   DBusMessage *r  = mock_call("Calls");
   if (r)
   {
      dbus_message_get_args(r, NULL, DBUS_TYPE_UINT32, &g,
            DBUS_TYPE_UINT32, &a, DBUS_TYPE_INVALID);
      dbus_message_unref(r);
   }
   *get   = g;
   *apply = a;
}

static void mock_reset(void)
{
   DBusMessage *r = mock_call("Reset");
   if (r)
      dbus_message_unref(r);
}

/* ------------------------------------------------------------------
 * Checks
 * ------------------------------------------------------------------ */

#define FAIL(...) do { fprintf(stderr, "FAIL: " __VA_ARGS__); fputc('\n', stderr); return 1; } while (0)

/* The module answers from the state its worker publishes and applies
 * switches on that worker, so what Mutter was asked, and the state it
 * reports back, arrive a moment after the call returns. The harness
 * waits for them; RetroArch never does. */
static void settle(void)
{
   usleep(10000);
}

static int wait_mutter(void)
{
   int i;
   for (i = 0; i < 500 && !mutter_displayconfig_available(); i++)
      settle();
   return mutter_displayconfig_available() ? 0 : 1;
}

static void wait_current(const mutter_dc_target_t *t, unsigned w,
      unsigned h, float hz)
{
   int i;
   for (i = 0; i < 500; i++)
   {
      unsigned k, n = 0;
      int done = 0;
      video_display_config_t *l = NULL;
      if (mutter_displayconfig_get_resolution_list(t, &l, &n) == MUTTER_DC_OK)
         for (k = 0; k < n; k++)
            if (     l[k].current
                  && VIDEO_SCALE_W(l[k].dims) == w
                  && VIDEO_SCALE_H(l[k].dims) == h
                  && l[k].refreshrate_float - hz < 0.01f
                  && hz - l[k].refreshrate_float < 0.01f)
               done = 1;
      free(l);
      if (done)
         return;
      settle();
   }
}

static int expect_last(const char *want, const char *what)
{
   int i;
   char got[2048];
   for (i = 0; i < 500; i++)
   {
      mock_last(got, sizeof(got));
      if (!strcmp(got, want))
         break;
      settle();
   }
   if (strcmp(got, want))
      FAIL("%s: Mutter was asked\n  %s\nwanted\n  %s", what, got, want);
   printf("[pass] %s\n", what);
   return 0;
}

static int find(const video_display_config_t *l, unsigned n,
      unsigned w, unsigned h, float hz)
{
   unsigned i;
   for (i = 0; i < n; i++)
   {
      float d = l[i].refreshrate_float - hz;
      if (VIDEO_SCALE_W(l[i].dims) == w && VIDEO_SCALE_H(l[i].dims) == h
            && d < 0.01f && d > -0.01f)
         return (int)i;
   }
   return -1;
}

/* DP-1's modes, as the mock lists them: the 240 Hz mode twice (fixed
 * and variable rate) must come out once */
static int check_dp1_list(const video_display_config_t *l, unsigned n,
      float current_hz, const char *what)
{
   unsigned i;
   int c;
   if (n != 4)
      FAIL("%s: %u entries, want 4", what, n);
   if (find(l, n, 1920, 1080, 60.0f) != 0
         || find(l, n, 2560, 1440, 59.951f) != 1
         || find(l, n, 2560, 1440, 119.877f) != 2
         || find(l, n, 2560, 1440, 239.913f) != 3)
      FAIL("%s: entries missing or out of order", what);
   c = find(l, n, 2560, 1440, current_hz);
   for (i = 0; i < n; i++)
      if (l[i].current != ((int)i == c) || l[i].idx != i
            || l[i].refreshrate != (unsigned)(l[i].refreshrate_float + 0.001f))
         FAIL("%s: entry %u mislabelled (current %d idx %u rate %u)", what, i,
               l[i].current, l[i].idx, l[i].refreshrate);
   printf("[pass] %s: 4 modes, 60/120/240 Hz, current %.3f Hz\n", what, current_hz);
   return 0;
}

static int case_unavailable(const char *what)
{
   video_display_config_t *l = (video_display_config_t*)1;
   unsigned n = 99;
   if (mutter_displayconfig_available())
      FAIL("%s: reported available", what);
   if (mutter_displayconfig_get_resolution_list(NULL, &l, &n) != MUTTER_DC_UNAVAILABLE
         || l || n)
      FAIL("%s: list not unavailable", what);
   if (mutter_displayconfig_set_resolution(NULL, 0, 60, 60.0f) != MUTTER_DC_UNAVAILABLE)
      FAIL("%s: set not unavailable", what);
   printf("[pass] %s: unavailable, caller keeps its old behaviour\n", what);
   return 0;
}

#define LAYOUT "props={layout-mode:1}"

static int case_mutter(void)
{
   mutter_dc_target_t hdmi;
   video_display_config_t *l = NULL;
   unsigned n = 0, g, a;

   if (wait_mutter())
      FAIL("mock Mutter not seen on the bus");
   if (mutter_displayconfig_get_resolution_list(NULL, &l, &n) != MUTTER_DC_OK)
      FAIL("list failed");
   if (check_dp1_list(l, n, 239.913f, "primary head list"))
      return 1;
   free(l);

   /* 240 -> 60 Hz: HDR colour mode kept on DP-1, underscanning kept on
    * HDMI-1, HDMI-1 where it was, temporary, current serial */
   if (mutter_displayconfig_set_resolution(NULL,
            VIDEO_SCALE_PACK(2560, 1440), 59, 59.951f) != MUTTER_DC_OK)
      FAIL("switch to 60 Hz refused");
   if (expect_last("method=1 serial=7 lm=0,0,1.00,0,p[DP-1=2560x1440@59.951{color-mode:1}]"
            " 2560,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
            "60 Hz: one mode changed, every other setting carried over"))
      return 1;
   wait_current(NULL, 2560, 1440, 59.951f);
   if (mutter_displayconfig_get_resolution_list(NULL, &l, &n) != MUTTER_DC_OK
         || check_dp1_list(l, n, 59.951f, "list after the switch"))
      return 1;
   free(l);

   /* Rate only (size 0), as the refresh rate autoswitch asks */
   if (mutter_displayconfig_set_resolution(NULL, 0, 119, 119.877f) != MUTTER_DC_OK)
      FAIL("rate-only switch refused");
   if (expect_last("method=1 serial=8 lm=0,0,1.00,0,p[DP-1=2560x1440@119.877{color-mode:1}]"
            " 2560,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
            "rate only: the size stays"))
      return 1;

   /* Whole hertz from the menu label alone */
   if (mutter_displayconfig_set_resolution(NULL,
            VIDEO_SCALE_PACK(2560, 1440), 239, 0.0f) != MUTTER_DC_OK)
      FAIL("whole-hertz switch refused");
   if (expect_last("method=1 serial=9 lm=0,0,1.00,0,p[DP-1=2560x1440@239.913{color-mode:1}]"
            " 2560,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
            "whole hertz: 239 finds 239.913, not the variable-rate twin"))
      return 1;

   /* Smaller: the head to the right moves in to stay adjacent */
   if (mutter_displayconfig_set_resolution(NULL,
            VIDEO_SCALE_PACK(1920, 1080), 60, 60.0f) != MUTTER_DC_OK)
      FAIL("switch to 1920x1080 refused");
   if (expect_last("method=1 serial=10 lm=0,0,1.00,0,p[DP-1=1920x1080@60.000{color-mode:1}]"
            " 1920,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
            "smaller mode: the neighbour follows"))
      return 1;

   /* The other head, by connector: its own list and switch */
   memset(&hdmi, 0, sizeof(hdmi));
   hdmi.connector = "HDMI-1";
   if (mutter_displayconfig_get_resolution_list(&hdmi, &l, &n) != MUTTER_DC_OK
         || n != 2 || find(l, n, 1280, 720, 60.0f) != 0 || !l[1].current)
      FAIL("HDMI-1 list wrong (%u entries)", n);
   free(l);
   if (mutter_displayconfig_set_resolution(&hdmi,
            VIDEO_SCALE_PACK(1280, 720), 60, 60.0f) != MUTTER_DC_OK)
      FAIL("HDMI-1 switch refused");
   if (expect_last("method=1 serial=11 lm=0,0,1.00,0,p[DP-1=1920x1080@60.000{color-mode:1}]"
            " 1920,0,1.00,0,-[HDMI-1=1280x720@60.000{enable_underscanning:true}] " LAYOUT,
            "head chosen by connector"))
      return 1;

   /* Refusals: a size no head lists, and a rate that one does not */
   mock_calls(&g, &a);
   if (mutter_displayconfig_set_resolution(NULL,
            VIDEO_SCALE_PACK(1234, 567), 60, 60.0f) != MUTTER_DC_FAILED)
      FAIL("an unlisted size was not refused");
   if (mutter_displayconfig_set_resolution(NULL,
            VIDEO_SCALE_PACK(1920, 1080), 144, 144.0f) != MUTTER_DC_FAILED)
      FAIL("an unlisted rate was not refused");
   {
      unsigned g2, a2;
      mock_calls(&g2, &a2);
      if (a2 != a)
         FAIL("a refused switch still sent ApplyMonitorsConfig");
   }
   printf("[pass] unlisted size and rate refused without asking Mutter\n");

   mock_reset();
   return 0;
}

#ifdef TEST_X11
static int x11_open(void)
{
   if (!(g_x11_dpy = XOpenDisplay(NULL)))
      FAIL("no X display");
   return 0;
}

static int case_x11_xorg(void)
{
   video_display_config_t *l;
   unsigned n = 0, g, a;
   void *data;
   if (x11_open())
      return 1;
   data = dispserv_x11.init();
   l    = (video_display_config_t*)dispserv_x11.get_resolution_list(data, &n);
   if (!l || n < 10)
      FAIL("real X server: %u entries", n);
   free(l);
   dispserv_x11.destroy(data);
   mock_calls(&g, &a);
   if (g || a)
      FAIL("real X server: Mutter was asked (%u state, %u apply)", g, a);
   printf("[pass] real X server: %u XRandR modes, Mutter never asked\n", n);
   return 0;
}

static int case_x11_xwayland(bool mutter)
{
   video_display_config_t *l;
   unsigned n = 0;
   void *data;
   if (x11_open())
      return 1;
   data = dispserv_x11.init();
   if (mutter && wait_mutter())
      FAIL("XWayland: mock Mutter not seen on the bus");
   l    = (video_display_config_t*)dispserv_x11.get_resolution_list(data, &n);
   if (mutter)
   {
      if (!l || check_dp1_list(l, n, 239.913f, "XWayland + Mutter list"))
         return 1;
      free(l);
      if (!dispserv_x11.set_resolution(data, VIDEO_SCALE_PACK(2560, 1440),
               59, 59.951f, 0, 0, 0, 0))
         FAIL("XWayland + Mutter switch refused");
      if (expect_last("method=1 serial=7 lm=0,0,1.00,0,p[DP-1=2560x1440@59.951{color-mode:1}]"
               " 2560,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
               "XWayland + Mutter switch goes to Mutter"))
         return 1;
   }
   else
   {
      /* XWayland's own list, as before: its emulated sizes, each a
       * CVT timing at the one rate the compositor runs */
      unsigned i;
      float lo, hi;
      if (!l || n < 2)
         FAIL("XWayland without Mutter: %u entries", n);
      lo = hi = l[0].refreshrate_float;
      for (i = 1; i < n; i++)
      {
         if (l[i].refreshrate_float < lo)
            lo = l[i].refreshrate_float;
         if (l[i].refreshrate_float > hi)
            hi = l[i].refreshrate_float;
      }
      if (hi - lo > 2.0f)
         FAIL("XWayland without Mutter: %.3f..%.3f Hz, not XWayland's list", lo, hi);
      free(l);
      printf("[pass] XWayland without Mutter: XWayland's %u modes, as before\n", n);
   }
   dispserv_x11.destroy(data);
   return 0;
}

/* The real thing: dispserv_x11 on the XWayland a real (headless)
 * Mutter starts, with that Mutter on the bus. Mutter names its outputs,
 * so XWayland's is Meta-0 - not XWAYLAND0 - and only the XWAYLAND
 * extension says what the server is. run.sh starts Mutter with one
 * 2560x1440@60 virtual monitor: Mutter lists that one mode at exactly
 * 60.000 Hz, while XWayland's own list has several sizes, each a CVT
 * timing at 59.9x Hz. */
static int case_x11_real_mutter(void)
{
   int o;
   bool named = true;
   video_display_config_t *l;
   unsigned n = 0;
   void *data;
   XRRScreenResources *res;

   if (x11_open())
      return 1;
   if ((res = XRRGetScreenResourcesCurrent(g_x11_dpy, DefaultRootWindow(g_x11_dpy))))
   {
      for (o = 0; o < res->noutput; o++)
      {
         XRROutputInfo *oi = XRRGetOutputInfo(g_x11_dpy, res, res->outputs[o]);
         if (!oi)
            continue;
         if (oi->connection == RR_Connected && !strncmp(oi->name, "XWAYLAND", 8))
            named = false;
         XRRFreeOutputInfo(oi);
      }
      XRRFreeScreenResources(res);
   }
   if (!named)
      FAIL("Mutter's XWayland output is named XWAYLAND<n>; this case needs a named one");

   data = dispserv_x11.init();
   /* Real Mutter, on the bus by the time this runs */
   wait_mutter();
   l    = (video_display_config_t*)dispserv_x11.get_resolution_list(data, &n);
   if (!l || n != 1 || VIDEO_SCALE_W(l[0].dims) != 2560
         || VIDEO_SCALE_H(l[0].dims) != 1440
         || l[0].refreshrate_float < 59.999f || l[0].refreshrate_float > 60.001f
         || !l[0].current)
      FAIL("real Mutter's XWayland: %u entries, first %ux%u %.3f Hz - XWayland's list, not Mutter's",
            n, l ? VIDEO_SCALE_W(l[0].dims) : 0, l ? VIDEO_SCALE_H(l[0].dims) : 0,
            l ? l[0].refreshrate_float : 0.0f);
   free(l);
   if (!dispserv_x11.set_resolution(data, VIDEO_SCALE_PACK(2560, 1440),
            60, 60.0f, 0, 0, 0, 0))
      FAIL("real Mutter's XWayland: switch to the listed mode refused");
   dispserv_x11.destroy(data);
   printf("[pass] real Mutter's XWayland (named output): Mutter's modes, not XWayland's\n");
   return 0;
}
#endif

#ifdef TEST_WL
static int case_wl(bool mutter)
{
   video_display_config_t *l;
   unsigned n = 0;
   uint32_t flags;
   void *data = dispserv_wl.init();
   if (!data)
      FAIL("no Wayland display");
   if (!dispserv_wl.get_resolution_list || !dispserv_wl.set_resolution
         || !dispserv_wl.get_flags)
      FAIL("dispserv_wl has no resolution callbacks in a HAVE_DBUS build");
   /* dispserv_wl's init makes no roundtrip: the output arrives on its
    * own queue over the calls that follow, each taking only what the
    * compositor has sent. */
   {
      int i;
      float hz = 0.0f;
      for (i = 0; i < 500 && (hz = dispserv_wl.get_refresh_rate(data)) <= 0.0f; i++)
         settle();
      if (hz <= 0.0f)
         FAIL("Wayland: the output was never described");
      if (!mutter)
         printf("[pass] Wayland: output described without a roundtrip, %.3f Hz\n", hz);
   }
   if (mutter && wait_mutter())
      FAIL("Wayland: mock Mutter not seen on the bus");
   flags = dispserv_wl.get_flags(data);
   l     = (video_display_config_t*)dispserv_wl.get_resolution_list(data, &n);
   if (mutter)
   {
      if (BIT32_GET(flags, DISPSERV_CTX_NO_RESOLUTION_LIST))
         FAIL("Wayland + Mutter: resolution list flagged away");
      if (!l || check_dp1_list(l, n, 239.913f, "Wayland + Mutter list"))
         return 1;
      free(l);
      if (!dispserv_wl.set_resolution(data, VIDEO_SCALE_PACK(2560, 1440),
               119, 119.877f, 0, 0, 0, 0))
         FAIL("Wayland + Mutter switch refused");
      if (expect_last("method=1 serial=7 lm=0,0,1.00,0,p[DP-1=2560x1440@119.877{color-mode:1}]"
               " 2560,0,1.00,0,-[HDMI-1=1920x1080@60.000{enable_underscanning:true}] " LAYOUT,
               "Wayland + Mutter switch"))
         return 1;
   }
   else
   {
      /* What video_display_server_has_resolution_list() reads: the
       * menu entry and the autoswitch stay off, as before */
      if (!BIT32_GET(flags, DISPSERV_CTX_NO_RESOLUTION_LIST) || l || n
            || dispserv_wl.set_resolution(data, 0, 60, 60.0f, 0, 0, 0, 0))
         FAIL("Wayland without Mutter: resolution list offered");
      printf("[pass] Wayland without Mutter: no resolution list, as before\n");
   }
   dispserv_wl.destroy(data);
   return 0;
}
#endif

int main(int argc, char **argv)
{
   const char *c = argc > 1 ? argv[1] : "";
   s_verbose     = getenv("MDC_VERBOSE") != NULL;

   if (!strcmp(c, "nobus"))
      return case_unavailable("no session bus");
   if (!strcmp(c, "nomutter"))
      return case_unavailable("session bus without Mutter");
   if (!strcmp(c, "mutter"))
      return case_mutter();
#ifdef TEST_X11
   if (!strcmp(c, "x11-xorg"))
      return case_x11_xorg();
   if (!strcmp(c, "x11-xwayland"))
      return case_x11_xwayland(true);
   if (!strcmp(c, "x11-xwayland-nomutter"))
      return case_x11_xwayland(false);
   if (!strcmp(c, "x11-real-mutter"))
      return case_x11_real_mutter();
#endif
#ifdef TEST_WL
   if (!strcmp(c, "wl"))
      return case_wl(true);
   if (!strcmp(c, "wl-nomutter"))
      return case_wl(false);
#endif
   fprintf(stderr, "unknown case '%s'\n", c);
   return 2;
}
