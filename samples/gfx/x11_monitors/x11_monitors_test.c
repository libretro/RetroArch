/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (x11_monitors_test.c).
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

/* The X11 monitor follower, gfx/common/x11_monitors.c: a worker thread
 * on an X connection of its own lists the monitors and finds the one
 * the main window is on, so no thread RetroArch draws from waits on
 * the server for them.
 *
 * Run by run.sh, one case per invocation, each on a fresh Xvfb:
 *
 *   nodisplay  no server: start returns at once, the worker gives up,
 *              nothing is published
 *   one        one monitor, its RandR name; a mode switch alone (no
 *              wake) is seen through the worker's own RandR selection;
 *              the wake pipe is not inherited by a child (system())
 *   two        two RandR 1.5 monitors, by name, in Xinerama order
 *   own        the main window's monitor follows the window; a move
 *              within a monitor publishes nothing, a new window there
 *              does
 *   gone       the main window destroyed while handed over: own goes
 *              to -1, the worker's errors never reach the process's
 *              handler, and it carries on
 *   threads    four readers while the monitors flip 60 times: each
 *              snapshot whole, freed only once no reader is in it
 *              (run under ASan and TSan)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>

#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>

#include "../../../gfx/common/x11_monitors.h"

#ifndef RARCH_HAVE_X11_MONITORS
#error "x11_monitors.h gate not met: HAVE_VIDEO_WINDOWS, HAVE_THREADS, HAVE_XINERAMA, lock-free pointer atomics"
#endif

#define FAIL(...) do { fprintf(stderr, "[FAIL] " __VA_ARGS__); \
   fprintf(stderr, " (line %d)\n", __LINE__); return 1; } while (0)

/* The worker answers a moment after it is asked; the test waits for
 * it, RetroArch never does. */
#define WAIT_MS 5000

#define FD_SCAN 1024

/* Per fd: 0 not a pipe, 1 a pipe, 2 a pipe closed on exec */
static void fd_pipes(unsigned char *out)
{
   int fd;
   for (fd = 0; fd < FD_SCAN; fd++)
   {
      struct stat st;
      int flags = fcntl(fd, F_GETFD);
      out[fd]   = 0;
      if (flags >= 0 && !fstat(fd, &st) && S_ISFIFO(st.st_mode))
         out[fd] = (flags & FD_CLOEXEC) ? 2 : 1;
   }
}

/* The process's handler, as RetroArch's are: any error from any
 * connection, the worker's included, is a failure. */
static retro_atomic_int_t errors;

static int test_error(Display *dpy, XErrorEvent *ev)
{
   (void)dpy;
   fprintf(stderr, "X error %d (request %d)\n",
         ev->error_code, ev->request_code);
   retro_atomic_fetch_add_int(&errors, 1);
   return 0;
}

static int errors_clean(void)
{
   return !retro_atomic_load_acquire_int(&errors);
}

static unsigned wait_serial(unsigned after)
{
   unsigned i, s = 0;
   for (i = 0; i < WAIT_MS; i++)
   {
      if ((s = x11_monitors_serial()) > after)
         return s;
      retro_sleep(1);
   }
   return 0;
}

static int snap_own(void)
{
   int own = -2;
   const x11_monitors_t *s = x11_monitors_get();
   if (s)
      own = s->own;
   x11_monitors_put();
   return own;
}

static int wait_own(int want)
{
   unsigned i;
   for (i = 0; i < WAIT_MS; i++)
   {
      if (snap_own() == want)
         return 1;
      retro_sleep(1);
   }
   return 0;
}

static Display *test_open(void)
{
   Display *dpy = XOpenDisplay(NULL);
   if (dpy)
      XSetErrorHandler(test_error);
   return dpy;
}

/* Starts the worker on the test's server and waits for its first
 * snapshot. */
static int test_start(Display *dpy)
{
   if (!x11_monitors_start(DisplayString(dpy)))
      return 0;
   return wait_serial(0) == 1;
}

/* The name the test's own RandR gives the monitor at a rect; "" when
 * the server has no RandR 1.5 or no monitor is there. */
static void rr_name(Display *dpy, int x, int y, unsigned w, unsigned h,
      char *out, size_t len)
{
   int i, n = 0, major = 0, minor = 0, eb, erb;
   XRRMonitorInfo *mon;
   out[0] = '\0';
   if (     !XRRQueryExtension(dpy, &eb, &erb)
         || !XRRQueryVersion(dpy, &major, &minor)
         || (major == 1 && minor < 5))
      return;
   if (!(mon = XRRGetMonitors(dpy, DefaultRootWindow(dpy), True, &n)))
      return;
   for (i = 0; i < n; i++)
      if (     mon[i].x == x && mon[i].y == y
            && (unsigned)mon[i].width == w && (unsigned)mon[i].height == h)
      {
         char *s = XGetAtomName(dpy, mon[i].name);
         if (s)
         {
            snprintf(out, len, "%s", s);
            XFree(s);
         }
         break;
      }
   XRRFreeMonitors(mon);
}

static void rr_set(Display *dpy, const char *name, int x, int y,
      int w, int h, RROutput output)
{
   XRRMonitorInfo *mon = XRRAllocateMonitor(dpy, output ? 1 : 0);
   mon->name    = XInternAtom(dpy, name, False);
   mon->x       = x;
   mon->y       = y;
   mon->width   = w;
   mon->height  = h;
   mon->mwidth  = w * 254 / 960;
   mon->mheight = h * 286 / 1080;
   if (output)
      mon->outputs[0] = output;
   XRRSetMonitor(dpy, DefaultRootWindow(dpy), mon);
   XRRFreeMonitors(mon);
}

static void rr_del(Display *dpy, const char *name)
{
   XRRDeleteMonitor(dpy, DefaultRootWindow(dpy),
         XInternAtom(dpy, name, False));
}

static RROutput rr_output(Display *dpy)
{
   RROutput out = 0;
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy,
         DefaultRootWindow(dpy));
   if (res)
   {
      if (res->noutput > 0)
         out = res->outputs[0];
      XRRFreeScreenResources(res);
   }
   return out;
}

/* RA-A on the left half of the 1920x1080 screen, taking its output (so
 * RandR's automatic monitor goes), RA-B on the right with none, as
 * `xrandr --setmonitor ... screen` and `... none` do. */
static void rr_two(Display *dpy)
{
   rr_set(dpy, "RA-A", 0, 0, 960, 1080, rr_output(dpy));
   rr_set(dpy, "RA-B", 960, 0, 960, 1080, 0);
   XSync(dpy, False);
}

static int xinerama_count(Display *dpy)
{
   int n = 0;
   XineramaScreenInfo *info = XineramaQueryScreens(dpy, &n);
   if (!info)
      return 0;
   XFree(info);
   return n;
}

static int index_of(const x11_monitors_t *s, const char *name)
{
   unsigned i;
   for (i = 0; i < s->count; i++)
      if (!strcmp(s->mon[i].name, name))
         return (int)i;
   return -1;
}

/* Two monitors enumerated, after rr_two(); 0 with SKIP printed when
 * Xinerama does not show them. */
static int enumerate_two(Display *dpy, int *skip)
{
   unsigned prev = x11_monitors_serial();
   *skip = 0;
   rr_two(dpy);
   if (xinerama_count(dpy) < 2)
   {
      *skip = 1;
      return 1;
   }
   x11_monitors_wake(X11_MONITORS_ENUMERATE);
   return wait_serial(prev) != 0;
}

static int case_nodisplay(void)
{
   time_t t0 = time(NULL);
   unsigned i;

   if (!x11_monitors_start(":250"))
      FAIL("start refused");
   if (time(NULL) - t0 > 1)
      FAIL("start waited for the connection");
   for (i = 0; i < WAIT_MS && x11_monitors_state() != -1; i++)
      retro_sleep(1);
   if (x11_monitors_state() != -1)
      FAIL("state %d, not -1", x11_monitors_state());
   if (x11_monitors_get())
   {
      x11_monitors_put();
      FAIL("a snapshot without a server");
   }
   x11_monitors_put();
   if (x11_monitors_serial())
      FAIL("serial %u without a server", x11_monitors_serial());
   printf("[pass] nodisplay: gave up, nothing published\n");
   return 0;
}

static int case_one(void)
{
   static unsigned char pipes_before[FD_SCAN], pipes_after[FD_SCAN];
   char want[64];
   int fd;
   unsigned serial;
   unsigned wake_fds = 0;
   const x11_monitors_t *s;
   int ok;
   Display *dpy = test_open();

   if (!dpy)
      FAIL("no X server");
   fd_pipes(pipes_before);
   if (!test_start(dpy))
      FAIL("first snapshot (serial %u)", x11_monitors_serial());
   if (x11_monitors_state() != 1)
      FAIL("state %d", x11_monitors_state());
   fd_pipes(pipes_after);
   for (fd = 0; fd < FD_SCAN; fd++)
   {
      if (pipes_before[fd] || !pipes_after[fd])
         continue;
      if (pipes_after[fd] != 2)
         FAIL("wake pipe fd %d not closed on exec", fd);
      wake_fds++;
   }
   if (wake_fds != 2)
      FAIL("%u new pipe fd(s), wanted the wake pipe's 2", wake_fds);
   rr_name(dpy, 0, 0, 1920, 1080, want, sizeof(want));
   s  = x11_monitors_get();
   ok =     s && s->serial == 1 && s->count == 1 && s->own == -1
         && s->mon[0].x == 0 && s->mon[0].y == 0
         && s->mon[0].w == 1920 && s->mon[0].h == 1080
         && s->mon[0].ordinal == 1 && !strcmp(s->mon[0].name, want);
   if (s)
      printf("one: %u monitor(s), \"%s\" %ux%u+%d+%d ordinal %u own %d\n",
            s->count, s->mon[0].name, s->mon[0].w, s->mon[0].h,
            s->mon[0].x, s->mon[0].y, s->mon[0].ordinal, s->own);
   x11_monitors_put();
   if (!ok)
      FAIL("one monitor (RandR name \"%s\")", want);

   /* Another client switches the mode: no wake, the worker sees the
    * RandR events on its own connection. */
   {
      XRRModeInfo mi;
      RRMode mode;
      XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy,
            DefaultRootWindow(dpy));
      if (!res || res->ncrtc < 1 || res->noutput < 1)
         FAIL("no CRTC to switch");
      memset(&mi, 0, sizeof(mi));
      mi.width      = 1280;
      mi.height     = 720;
      mi.hTotal     = 1280;
      mi.vTotal     = 720;
      mi.dotClock   = 1280UL * 720UL * 60UL;
      mi.name       = (char*)"1280x720";
      mi.nameLength = 8;
      mode          = XRRCreateMode(dpy, DefaultRootWindow(dpy), &mi);
      XRRAddOutputMode(dpy, res->outputs[0], mode);
      if (XRRSetCrtcConfig(dpy, res, res->crtcs[0], CurrentTime, 0, 0,
               mode, RR_Rotate_0, res->outputs, 1) != RRSetConfigSuccess)
      {
         XRRFreeScreenResources(res);
         FAIL("mode switch refused");
      }
      XRRFreeScreenResources(res);
      XSync(dpy, False);
   }
   if (!(serial = wait_serial(1)))
      FAIL("mode switch not seen without a wake");
   s  = x11_monitors_get();
   ok =     s && s->count == 1
         && s->mon[0].w == 1280 && s->mon[0].h == 720;
   x11_monitors_put();
   if (!ok)
      FAIL("monitor after the switch");
   if (!errors_clean())
      FAIL("X errors");
   printf("[pass] one: \"%s\", mode switch seen by the worker (serial %u)\n",
         want, serial);
   return 0;
}

static int case_two(void)
{
   unsigned i;
   int skip, ok;
   int n = 0;
   XineramaScreenInfo *info;
   const x11_monitors_t *s;
   Display *dpy = test_open();

   if (!dpy)
      FAIL("no X server");
   if (!test_start(dpy))
      FAIL("first snapshot");
   if (!enumerate_two(dpy, &skip))
      FAIL("two monitors not published");
   if (skip)
   {
      printf("SKIP two: Xinerama shows %d screen(s)\n", xinerama_count(dpy));
      return 0;
   }
   if (!(info = XineramaQueryScreens(dpy, &n)))
      FAIL("Xinerama");
   s  = x11_monitors_get();
   ok = s && s->count == 2 && n == 2 && s->own == -1;
   for (i = 0; ok && i < 2; i++)
   {
      char want[64];
      rr_name(dpy, info[i].x_org, info[i].y_org, info[i].width,
            info[i].height, want, sizeof(want));
      printf("two: %u \"%s\" %ux%u+%d+%d (RandR \"%s\")\n",
            s->mon[i].ordinal, s->mon[i].name, s->mon[i].w, s->mon[i].h,
            s->mon[i].x, s->mon[i].y, want);
      ok =     s->mon[i].ordinal == i + 1
            && s->mon[i].x == info[i].x_org && s->mon[i].y == info[i].y_org
            && s->mon[i].w == (unsigned)info[i].width
            && s->mon[i].h == (unsigned)info[i].height
            && !strcmp(s->mon[i].name, want);
   }
   ok = ok && index_of(s, "RA-A") >= 0 && index_of(s, "RA-B") >= 0;
   x11_monitors_put();
   XFree(info);
   if (!ok)
      FAIL("two monitors");
   if (!errors_clean())
      FAIL("X errors");
   printf("[pass] two: RA-A and RA-B in Xinerama order\n");
   return 0;
}

static int case_own(void)
{
   int a, b, skip, ok;
   unsigned serial;
   Window win, win2;
   const x11_monitors_t *s;
   Display *dpy = test_open();

   if (!dpy)
      FAIL("no X server");
   if (!test_start(dpy))
      FAIL("first snapshot");
   if (!enumerate_two(dpy, &skip) || skip)
      FAIL("two monitors not published");
   s = x11_monitors_get();
   a = s ? index_of(s, "RA-A") : -1;
   b = s ? index_of(s, "RA-B") : -1;
   x11_monitors_put();
   if (a < 0 || b < 0)
      FAIL("RA-A %d RA-B %d", a, b);

   win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
         960 + (960 - 200) / 2, (1080 - 150) / 2, 200, 150, 0, 0, 0);
   XMapWindow(dpy, win);
   XSync(dpy, False);
   x11_monitors_window(win);
   if (!wait_own(b))
      FAIL("own %d, not RA-B's %d", snap_own(), b);

   XMoveWindow(dpy, win, (960 - 200) / 2, (1080 - 150) / 2);
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_LOCATE);
   if (!wait_own(a))
      FAIL("own %d after the move, not RA-A's %d", snap_own(), a);

   /* Within RA-A: nothing changed, nothing published */
   serial = x11_monitors_serial();
   XMoveWindow(dpy, win, (960 - 200) / 2 + 40, (1080 - 150) / 2 + 30);
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_LOCATE);
   retro_sleep(200);
   if (x11_monitors_serial() != serial)
      FAIL("a move within RA-A published (serial %u -> %u)",
            serial, x11_monitors_serial());

   /* Another main window where the last was: published, for it */
   win2 = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
         (960 - 200) / 2, (1080 - 150) / 2, 200, 150, 0, 0, 0);
   XMapWindow(dpy, win2);
   XSync(dpy, False);
   x11_monitors_window(win2);
   if (!wait_serial(serial))
      FAIL("a new window on RA-A not published");
   s  = x11_monitors_get();
   ok = s && s->own == a && s->win == win2;
   x11_monitors_put();
   if (!ok)
      FAIL("snapshot after a new window on RA-A");
   XDestroyWindow(dpy, win2);

   x11_monitors_window(0);
   if (!wait_own(-1))
      FAIL("own %d with no window", snap_own());
   XDestroyWindow(dpy, win);
   XSync(dpy, False);
   if (!errors_clean())
      FAIL("X errors");
   printf("[pass] own: RA-B, then RA-A, unchanged within RA-A, none\n");
   return 0;
}

static int case_gone(void)
{
   int b, skip, ok;
   unsigned serial;
   Window win;
   const x11_monitors_t *s;
   Display *dpy = test_open();

   if (!dpy)
      FAIL("no X server");
   if (!test_start(dpy))
      FAIL("first snapshot");
   if (!enumerate_two(dpy, &skip))
      FAIL("two monitors not published");
   if (skip)
   {
      printf("SKIP gone: Xinerama shows %d screen(s)\n", xinerama_count(dpy));
      return 0;
   }
   s = x11_monitors_get();
   b = s ? index_of(s, "RA-B") : -1;
   x11_monitors_put();
   if (b < 0)
      FAIL("RA-B %d", b);

   win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
         960 + (960 - 200) / 2, (1080 - 150) / 2, 200, 150, 0, 0, 0);
   XMapWindow(dpy, win);
   XSync(dpy, False);
   x11_monitors_window(win);
   if (!wait_own(b))
      FAIL("own %d, not RA-B's %d", snap_own(), b);

   /* Gone while still handed over: only a failed locate gives -1 */
   XDestroyWindow(dpy, win);
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_LOCATE);
   if (!wait_own(-1))
      FAIL("own %d for a window that is gone", snap_own());
   s  = x11_monitors_get();
   ok = s && s->win == win;
   x11_monitors_put();
   if (!ok)
      FAIL("the gone window not named as the one looked for");

   /* Still alive: a change is still seen */
   serial = x11_monitors_serial();
   rr_del(dpy, "RA-B");
   XSync(dpy, False);
   x11_monitors_wake(X11_MONITORS_ENUMERATE);
   if (!wait_serial(serial))
      FAIL("the worker stopped after the error");
   s  = x11_monitors_get();
   ok = s && s->count == 1 && s->own == -1;
   x11_monitors_put();
   if (!ok)
      FAIL("snapshot after RA-B went");
   if (!errors_clean())
      FAIL("an X error reached the process's handler");
   printf("[pass] gone: own -1, no error left the worker; worker carries on\n");
   return 0;
}

#define THR_READERS 4
#define THR_FLIPS   60

static retro_atomic_int_t thr_stop;
static retro_atomic_int_t thr_bad;
static retro_atomic_int_t thr_reads;

static void thr_reader(void *data)
{
   unsigned seen = 0;
   (void)data;
   while (!retro_atomic_load_acquire_int(&thr_stop))
   {
      const x11_monitors_t *s = x11_monitors_get();
      if (s)
      {
         unsigned i;
         int bad = s->count < 1 || s->count > 2 || s->serial < seen;
         for (i = 0; !bad && i < s->count; i++)
            bad =    !memchr(s->mon[i].name, '\0', sizeof(s->mon[i].name))
                  || !s->mon[i].w || !s->mon[i].h;
         seen = s->serial;
         if (bad)
            retro_atomic_fetch_add_int(&thr_bad, 1);
         retro_atomic_fetch_add_int(&thr_reads, 1);
      }
      x11_monitors_put();
   }
}

static int case_threads(void)
{
   int i, n;
   sthread_t *thr[THR_READERS];
   Display *dpy = test_open();

   if (!dpy)
      FAIL("no X server");
   if (!test_start(dpy))
      FAIL("first snapshot");
   rr_set(dpy, "RA-B", 960, 0, 960, 1080, 0);
   XSync(dpy, False);
   n = xinerama_count(dpy);
   rr_del(dpy, "RA-B");
   XSync(dpy, False);
   if (n < 2)
   {
      printf("SKIP threads: Xinerama shows %d screen(s)\n", n);
      return 0;
   }
   retro_atomic_int_init(&thr_stop, 0);
   retro_atomic_int_init(&thr_bad, 0);
   retro_atomic_int_init(&thr_reads, 0);
   for (i = 0; i < THR_READERS; i++)
      if (!(thr[i] = sthread_create(thr_reader, NULL)))
         FAIL("reader thread");

   for (i = 0; i < THR_FLIPS; i++)
   {
      unsigned prev = x11_monitors_serial();
      if (i & 1)
         rr_del(dpy, "RA-B");
      else
         rr_set(dpy, "RA-B", 960, 0, 960, 1080, 0);
      XSync(dpy, False);
      x11_monitors_wake(X11_MONITORS_ENUMERATE);
      if (!wait_serial(prev))
      {
         retro_atomic_store_release_int(&thr_stop, 1);
         FAIL("flip %d not published", i);
      }
   }

   retro_atomic_store_release_int(&thr_stop, 1);
   for (i = 0; i < THR_READERS; i++)
      sthread_join(thr[i]);

   if (retro_atomic_load_acquire_int(&thr_bad))
      FAIL("%d bad reads while the snapshot was replaced",
            retro_atomic_load_acquire_int(&thr_bad));
   if (!retro_atomic_load_acquire_int(&thr_reads))
      FAIL("the readers read nothing");
   if (!errors_clean())
      FAIL("X errors");
   printf("[pass] threads: %d flips under %d readers, %d reads, every one whole\n",
         THR_FLIPS, THR_READERS, retro_atomic_load_acquire_int(&thr_reads));
   return 0;
}

int main(int argc, char **argv)
{
   const char *c = argc > 1 ? argv[1] : "";

   XInitThreads();
   retro_atomic_int_init(&errors, 0);

   if (!strcmp(c, "nodisplay"))
      return case_nodisplay();
   if (!strcmp(c, "one"))
      return case_one();
   if (!strcmp(c, "two"))
      return case_two();
   if (!strcmp(c, "own"))
      return case_own();
   if (!strcmp(c, "gone"))
      return case_gone();
   if (!strcmp(c, "threads"))
      return case_threads();
   fprintf(stderr, "unknown case '%s'\n", c);
   return 2;
}
