/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x11_refresh_rate_test.c).
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
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The X display server's refresh rate is asked every frame by a core
 * polling the throttle state. Once the event pump drains g_x11_dpy the
 * rate must be answered with no request to the server, and a mode
 * change made by any client must reach it at the next pump. A second
 * thread asks for the rate throughout, so a sanitizer sees the reader
 * race the pump and the changes. Needs an Xorg dummy-driver server. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../../gfx/common/x11_common.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/video_display_server.h"
#include "../../../input/input_keymaps.h"

/* Stubs for what x11_common.c and dispserv_x11.c reach outside the
 * pump and the rate. */
static int stub_signal_state;
const struct rarch_key_map rarch_key_map_x11[] = { { 0, RETROK_UNKNOWN } };
void live_log(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}
int frontend_driver_get_signal_handler_state(void) { return stub_signal_state; }
void frontend_driver_set_signal_handler_state(int v) { stub_signal_state = v; }
void frontend_driver_destroy_signal_handler_state(void) { stub_signal_state = 0; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
void x_input_poll_wheel(XButtonEvent *event, bool latch)
{ (void)event; (void)latch; }
enum rarch_display_type video_driver_display_type_get(void)
{ return RARCH_DISPLAY_X11; }
uintptr_t video_driver_window_get(void) { return (uintptr_t)g_x11_win; }
void video_driver_display_type_set(enum rarch_display_type type) { (void)type; }
void video_driver_display_set(uintptr_t idx) { (void)idx; }
void video_driver_window_set(uintptr_t idx) { (void)idx; }
size_t video_driver_get_window_title(char *s, size_t len)
{ (void)len; s[0] = '\0'; return 0; }
void video_monitor_set_refresh_rate(float hz) { (void)hz; }

static int failures;

#define CHECK(cond, what) do { \
   if (!(cond)) { \
      fprintf(stderr, "[FAIL] %s (line %d)\n", what, __LINE__); \
      failures++; \
   } else \
      printf("[ok] %s\n", what); \
} while (0)

static void          *dispserv;
static retro_atomic_int_t reader_stop;
static retro_atomic_int_t reader_done;
static unsigned long  reader_calls;

/* What a core polling the throttle state does from the runloop while
 * the video thread pumps events. */
static void reader_thread(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_acquire_int(&reader_stop))
   {
      (void)dispserv_x11.get_refresh_rate(dispserv);
      reader_calls++;
      retro_atomic_inc_int(&reader_done);
   }
}

/* The reader's asks that began before the last pump may still be
 * reading the server, and one that raced the pump withdraws what it
 * kept when it finishes. Both land on g_x11_dpy, which is where the
 * requests are counted, so a count starts only once an ask that began
 * after the pump has finished: every ask before it is then done. */
static void reader_settle(void)
{
   int seen = retro_atomic_load_acquire_int(&reader_done);
   while (retro_atomic_load_acquire_int(&reader_done) - seen < 2)
      sthread_yield();
}

static void pump(void)
{
   bool quit     = false;
   bool resize   = false;
   unsigned dims = 0;
   /* Everything the server sent before this reply is queued. */
   XSync(g_x11_dpy, False);
   x11_check_window(NULL, &quit, &resize, &dims);
}

static float rate(void)
{
   return dispserv_x11.get_refresh_rate(dispserv);
}

static unsigned long requests_over(unsigned calls)
{
   unsigned i;
   unsigned long before;
   XSync(g_x11_dpy, False);
   before = NextRequest(g_x11_dpy);
   for (i = 0; i < calls; i++)
      (void)rate();
   XSync(g_x11_dpy, False);
   /* Less the XSync that reads the count. */
   return NextRequest(g_x11_dpy) - before - 1;
}

/* The first connected output and its crtc, on the given connection. */
static bool first_output(Display *dpy, XRRScreenResources *res,
      RROutput *out, RRCrtc *crtc_id, XRRCrtcInfo **crtc)
{
   int o;
   for (o = 0; o < res->noutput; o++)
   {
      XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, res->outputs[o]);
      if (!oi)
         continue;
      if (oi->connection == RR_Connected && oi->crtc)
      {
         *out     = res->outputs[o];
         *crtc_id = oi->crtc;
         *crtc    = XRRGetCrtcInfo(dpy, res, oi->crtc);
         XRRFreeOutputInfo(oi);
         return *crtc != NULL;
      }
      XRRFreeOutputInfo(oi);
   }
   return false;
}

/* Another client switches the output: what xrandr or a desktop
 * settings panel does. */
static bool other_client_sets(Display *dpy, RRMode mode)
{
   bool ok;
   RROutput out;
   RRCrtc crtc_id;
   XRRCrtcInfo *crtc      = NULL;
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy,
         DefaultRootWindow(dpy));
   if (!res)
      return false;
   if (!first_output(dpy, res, &out, &crtc_id, &crtc))
   {
      XRRFreeScreenResources(res);
      return false;
   }
   ok = XRRSetCrtcConfig(dpy, res, crtc_id,
         CurrentTime, crtc->x, crtc->y, mode, crtc->rotation,
         crtc->outputs, crtc->noutput) == RRSetConfigSuccess;
   XSync(dpy, False);
   XRRFreeCrtcInfo(crtc);
   XRRFreeScreenResources(res);
   return ok;
}

int main(void)
{
   XEvent ev;
   XSetWindowAttributes swa;
   XRRModeInfo mi;
   XRRScreenResources *res;
   XRRCrtcInfo *crtc = NULL;
   RROutput out;
   RRCrtc crtc_id;
   RRMode desktop_mode, test_mode;
   Display *other;
   sthread_t *reader;
   float r0, r;
   unsigned long sent;
   int eb, erb;
   char name[] = "RA-REFRESH-50";

   XInitThreads();
   if (!getenv("DISPLAY") || !(g_x11_dpy = XOpenDisplay(NULL)))
   {
      puts("[skip] no X display; run against an Xorg dummy-driver server");
      return 0;
   }
   if (!XRRQueryExtension(g_x11_dpy, &eb, &erb)
         || !(other = XOpenDisplay(NULL)))
   {
      puts("[skip] no RandR on this display");
      return 0;
   }

   /* The window and event mask the X contexts make. */
   memset(&swa, 0, sizeof(swa));
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask
      | LeaveWindowMask | EnterWindowMask | ButtonReleaseMask
      | ButtonPressMask | FocusChangeMask;
   g_x11_win = XCreateWindow(g_x11_dpy, DefaultRootWindow(g_x11_dpy),
         0, 0, 320, 240, 0, CopyFromParent, InputOutput, CopyFromParent,
         CWEventMask, &swa);
   XMapWindow(g_x11_dpy, g_x11_win);
   x11_event_queue_check(&ev);

   /* A run stopped halfway leaves its mode behind, and the output on it. */
   res = XRRGetScreenResourcesCurrent(other, DefaultRootWindow(other));
   if (res)
   {
      int m;
      for (m = 0; m < res->nmode; m++)
         if (!strcmp(res->modes[m].name, name))
         {
            fprintf(stderr, "[FAIL] %s is already on the server; a previous run did not restore\n",
                  name);
            return 1;
         }
      XRRFreeScreenResources(res);
   }

   dispserv = dispserv_x11.init();

   /* Nothing pumps g_x11_dpy yet, as under a driver that never drains
    * it: every ask reads the server. */
   r0 = rate();
   printf("desktop rate %.3f Hz\n", r0);
   CHECK(fabs(r0 - 60.0f) < 0.1f, "desktop rate read from the server");
   CHECK(requests_over(4) > 0, "unpumped: the rate is read from the server");

   /* The pump runs: from the next read on, memory answers. */
   pump();
   (void)rate();
   sent = requests_over(1000);
   printf("pumped: %lu requests over 1000 asks\n", sent);
   CHECK(sent == 0, "pumped: no request over 1000 asks");
   CHECK(fabs(rate() - r0) < 0.001f, "pumped: same rate as the server's");

   /* A 50 Hz mode at the desktop size, from another client. */
   res = XRRGetScreenResourcesCurrent(other, DefaultRootWindow(other));
   if (!res || !first_output(other, res, &out, &crtc_id, &crtc))
   {
      fprintf(stderr, "[FAIL] no connected output\n");
      return 1;
   }
   desktop_mode = crtc->mode;
   memset(&mi, 0, sizeof(mi));
   mi.width      = crtc->width;
   mi.height     = crtc->height;
   mi.hSyncStart = crtc->width + 110;
   mi.hSyncEnd   = crtc->width + 150;
   mi.hTotal     = 1980;
   mi.vSyncStart = crtc->height + 5;
   mi.vSyncEnd   = crtc->height + 10;
   mi.vTotal     = 750;
   mi.dotClock   = 74250000;
   mi.name       = name;
   mi.nameLength = strlen(name);
   test_mode     = XRRCreateMode(other, DefaultRootWindow(other), &mi);
   XRRAddOutputMode(other, out, test_mode);
   XSync(other, False);
   XRRFreeCrtcInfo(crtc);
   XRRFreeScreenResources(res);

   retro_atomic_int_init(&reader_stop, 0);
   retro_atomic_int_init(&reader_done, 0);
   reader = sthread_create(reader_thread, NULL);

   CHECK(other_client_sets(other, test_mode), "other client sets 50 Hz");
   pump();
   r = rate();
   printf("after the switch %.3f Hz\n", r);
   CHECK(fabs(r - 50.0f) < 0.1f, "the switch reaches the rate at the next pump");
   reader_settle();
   (void)rate();
   CHECK(requests_over(100) == 0, "50 Hz: answered from memory again");

   CHECK(other_client_sets(other, desktop_mode), "other client restores the desktop");
   pump();
   r = rate();
   CHECK(fabs(r - r0) < 0.001f, "the restore reaches the rate at the next pump");

   /* Switch back and forth under the reader; after the last pump the
    * kept rate is the server's, whatever the reader kept on the way. */
   {
      int i;
      for (i = 0; i < 10; i++)
      {
         other_client_sets(other, (i & 1) ? desktop_mode : test_mode);
         pump();
      }
   }
   pump();
   r = rate();
   retro_atomic_store_release_int(&reader_stop, 1);
   sthread_join(reader);
   printf("after 10 switches %.3f Hz, reader asked %lu times\n", r, reader_calls);
   CHECK(fabs(r - 60.0f) < 0.1f, "no stale rate kept across racing switches");

   XRRDeleteOutputMode(other, out, test_mode);
   XRRDestroyMode(other, test_mode);
   XSync(other, False);
   XCloseDisplay(other);

   dispserv_x11.destroy(dispserv);
   x11_window_destroy(false);
   XCloseDisplay(g_x11_dpy);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   puts("all refresh rate checks passed");
   return 0;
}
