/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "x11_monitors.h"

#ifdef RARCH_HAVE_X11_MONITORS

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xinerama.h>
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif

#include <compat/strl.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <rthreads/rthreads.h>

/* What the worker and its readers share, without a lock: the worker
 * publishes a snapshot by swapping the pointer, a reader enters and
 * leaves around it, and the one replaced is freed once no reader is
 * in, by whichever is last out (retro_retire.h). */
typedef struct x11_monitors_ctl
{
   retro_atomic_ptr_t snap;    /* x11_monitors_t, NULL until the first */
   retro_retire_t     retire;
   retro_atomic_int_t serial;  /* bumped after each swap */
   retro_atomic_int_t want;    /* X11_MONITORS_* */
   retro_atomic_int_t win;     /* the main window's XID, 0 none */
   retro_atomic_int_t state;   /* 0 starting, 1 running, -1 gave up */
   retro_atomic_int_t piped;   /* wake[] made */
   int                wake[2];
   char               name[256];
} x11_monitors_ctl_t;

/* Zero-filled: the retire block needs no setup. */
static x11_monitors_ctl_t x11_monitors_ctl;

static void x11_monitors_free_node(mpsc_stack_node_t *node)
{
   free((x11_monitors_t*)node);
}

const x11_monitors_t *x11_monitors_get(void)
{
   retro_retire_enter(&x11_monitors_ctl.retire);
   return (const x11_monitors_t*)
      retro_atomic_load_acquire_ptr(&x11_monitors_ctl.snap);
}

void x11_monitors_put(void)
{
   retro_retire_leave(&x11_monitors_ctl.retire, x11_monitors_free_node);
}

unsigned x11_monitors_serial(void)
{
   return (unsigned)retro_atomic_load_acquire_int(&x11_monitors_ctl.serial);
}

int x11_monitors_state(void)
{
   return retro_atomic_load_acquire_int(&x11_monitors_ctl.state);
}

void x11_monitors_wake(int what)
{
   x11_monitors_ctl_t *ctl = &x11_monitors_ctl;
   /* A bit already set has its wake on the way: one write per pass. */
   if (     !(retro_atomic_fetch_or_int(&ctl->want, what) & what)
         && retro_atomic_load_acquire_int(&ctl->piped))
   {
      char c = 1;
      if (write(ctl->wake[1], &c, 1) < 0) { }
   }
}

void x11_monitors_window(unsigned long win)
{
   /* XIDs are 29 bits */
   retro_atomic_store_release_int(&x11_monitors_ctl.win, (int)(unsigned)win);
   x11_monitors_wake(X11_MONITORS_LOCATE);
}

/* The worker: readers find @snap from here on; the one it replaces is
 * freed once no reader is still inside it. */
static void x11_monitors_publish(x11_monitors_ctl_t *ctl,
      x11_monitors_t *snap)
{
   x11_monitors_t *old;
   snap->serial = (unsigned)retro_atomic_load_acquire_int(&ctl->serial) + 1;
   old          = (x11_monitors_t*)
      retro_atomic_exchange_ptr(&ctl->snap, snap);
   retro_retire(&ctl->retire, old ? &old->node : NULL,
         x11_monitors_free_node);
   retro_atomic_store_release_int(&ctl->serial, (int)snap->serial);
}

#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
/* Each rect's RandR 1.5 monitor name, by the same geometry. */
static void x11_monitors_names(Display *dpy, x11_monitor_t *mon,
      unsigned count)
{
   int j, nm = 0;
   unsigned i, k, n = 0;
   int taken[X11_MONITORS_MAX];
   unsigned slot[X11_MONITORS_MAX];
   Atom atoms[X11_MONITORS_MAX];
   char *got[X11_MONITORS_MAX];
   XRRMonitorInfo *info = XRRGetMonitors(dpy, DefaultRootWindow(dpy),
         True, &nm);

   if (!info)
      return;
   for (i = 0; i < count; i++)
   {
      for (j = 0; j < nm; j++)
      {
         bool used = false;
         if (     info[j].x != mon[i].x || info[j].y != mon[i].y
               || (unsigned)info[j].width  != mon[i].w
               || (unsigned)info[j].height != mon[i].h)
            continue;
         for (k = 0; k < n; k++)
            if (taken[k] == j)
               used = true;
         if (used)
            continue;
         taken[n]   = j;
         slot[n]    = i;
         atoms[n]   = info[j].name;
         got[n]     = NULL;
         n++;
         break;
      }
   }
   XRRFreeMonitors(info);

   if (!n)
      return;
   XGetAtomNames(dpy, atoms, (int)n, got);
   for (k = 0; k < n; k++)
   {
      if (!got[k])
         continue;
      strlcpy(mon[slot[k]].name, got[k], sizeof(mon[slot[k]].name));
      XFree(got[k]);
   }
}
#endif

static unsigned x11_monitors_enumerate(Display *dpy, bool names,
      x11_monitor_t *mon)
{
   int i, n               = 0;
   unsigned count         = 0;
   XineramaScreenInfo *xi = XineramaQueryScreens(dpy, &n);

   if (!xi)
      return 0;
   for (i = 0; i < n && count < X11_MONITORS_MAX; i++, count++)
   {
      mon[count].x       = xi[i].x_org;
      mon[count].y       = xi[i].y_org;
      mon[count].w       = (unsigned)xi[i].width;
      mon[count].h       = (unsigned)xi[i].height;
      mon[count].ordinal = (unsigned)xi[i].screen_number + 1;
   }
   XFree(xi);
#if defined(HAVE_XRANDR) && (RANDR_MAJOR > 1 || RANDR_MINOR >= 5)
   if (names && count)
      x11_monitors_names(dpy, mon, count);
#else
   (void)names;
#endif
   return count;
}

static unsigned x11_monitors_overlap(const x11_monitor_t *m,
      int x, int y, unsigned w, unsigned h)
{
   int r = MIN(m->x + (int)m->w, x + (int)w) - MAX(m->x, x);
   int b = MIN(m->y + (int)m->h, y + (int)h) - MAX(m->y, y);
   if (r <= 0 || b <= 0)
      return 0;
   return (unsigned)r * (unsigned)b;
}

/* The monitor holding @win's centre, else the one it overlaps most */
static int x11_monitors_locate(Display *dpy, Window win,
      const x11_monitor_t *mon, unsigned count)
{
   Window root, child;
   int x, y, cx, cy;
   unsigned i, w, h, border, depth;
   unsigned most = 0;
   int own       = -1;

   if (     !win
         || !XGetGeometry(dpy, win, &root, &x, &y, &w, &h, &border, &depth)
         || !XTranslateCoordinates(dpy, win, root, 0, 0, &x, &y, &child))
      return -1;
   cx = x + (int)(w / 2);
   cy = y + (int)(h / 2);
   for (i = 0; i < count; i++)
      if (     cx >= mon[i].x && cx < mon[i].x + (int)mon[i].w
            && cy >= mon[i].y && cy < mon[i].y + (int)mon[i].h)
         return (int)i;
   for (i = 0; i < count; i++)
   {
      unsigned area = x11_monitors_overlap(&mon[i], x, y, w, h);
      if (area > most)
      {
         most = area;
         own  = (int)i;
      }
   }
   return own;
}

/* One pass for @what into @next: published, or freed when nothing
 * changed. */
static bool x11_monitors_pass(x11_monitors_ctl_t *ctl, Display *dpy,
      bool names, const x11_monitors_t *last, int what,
      x11_monitors_t *next)
{
   Window win;

   if ((what & X11_MONITORS_ENUMERATE) || !last)
      next->count = x11_monitors_enumerate(dpy, names, next->mon);
   else
   {
      next->count = last->count;
      memcpy(next->mon, last->mon, sizeof(next->mon));
   }
   /* A window handed over during the round trips is located instead */
   do
   {
      win       = (Window)(unsigned)
         retro_atomic_load_acquire_int(&ctl->win);
      next->own = x11_monitors_locate(dpy, win, next->mon, next->count);
   } while (win != (Window)(unsigned)
         retro_atomic_load_acquire_int(&ctl->win));
   next->win = win;

   if (     last
         && next->count == last->count
         && next->own   == last->own
         && next->win   == last->win
         && !memcmp(next->mon, last->mon, sizeof(next->mon)))
   {
      free(next);
      return false;
   }
   x11_monitors_publish(ctl, next);
   return true;
}

/* Keeps a gone window's locate errors from the process's handler;
 * any other error still reaches it. */
static Bool x11_monitors_wire_error(Display *dpy, XErrorEvent *error,
      xError *wire)
{
   (void)dpy;
   (void)wire;
   return error->request_code != X_GetGeometry
       && error->request_code != X_TranslateCoords;
}

static void x11_monitors_worker(void *data)
{
   x11_monitors_ctl_t *ctl = (x11_monitors_ctl_t*)data;
   const x11_monitors_t *last;
   Display *dpy;
   bool names              = false;
   int events              = 0;
#ifdef HAVE_XRANDR
   int rr_base             = 0;
#endif

   sthread_setname("ra-x11-monitors");

   if (!(dpy = XOpenDisplay(ctl->name[0] ? ctl->name : NULL)))
   {
      retro_atomic_store_release_int(&ctl->state, -1);
      return;
   }
   XESetWireToError(dpy, BadWindow,   x11_monitors_wire_error);
   XESetWireToError(dpy, BadDrawable, x11_monitors_wire_error);

#ifdef HAVE_XRANDR
   {
      int event_base, error_base;
      int major = 0;
      int minor = 0;
      if (     XRRQueryExtension(dpy, &event_base, &error_base)
            && XRRQueryVersion(dpy, &major, &minor))
      {
         /* On this connection: a hotplug is seen whichever display
          * server RetroArch runs. */
         XRRSelectInput(dpy, DefaultRootWindow(dpy),
               (major > 1 || minor >= 2)
               ? RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask
                  | RROutputChangeNotifyMask
               : RRScreenChangeNotifyMask);
         rr_base = event_base;
         names   = major > 1 || minor >= 5;
      }
   }
#endif

   /* Only this thread replaces it, so it reads it without entering */
   last = (const x11_monitors_t*)retro_atomic_load_acquire_ptr(&ctl->snap);
   retro_atomic_store_release_int(&ctl->state, 1);

   for (;;)
   {
      int what;
      struct pollfd pfd[2];

      while (XPending(dpy))
      {
         XEvent ev;
         XNextEvent(dpy, &ev);
#ifdef HAVE_XRANDR
         if (     rr_base
               && ev.type >= rr_base + RRScreenChangeNotify
               && ev.type <= rr_base + RRNotify)
            events |= X11_MONITORS_ENUMERATE;
#endif
      }

      what   = retro_atomic_exchange_int(&ctl->want, 0) | events;
      events = 0;
      if (what)
      {
         x11_monitors_t *next = (x11_monitors_t*)calloc(1, sizeof(*next));
         if (next)
         {
            if (x11_monitors_pass(ctl, dpy, names, last, what, next))
               last = next;
            continue;
         }
         /* Out of memory: kept for the pass after the next wake */
         events = what;
      }

      pfd[0].fd      = ConnectionNumber(dpy);
      pfd[0].events  = POLLIN;
      pfd[0].revents = 0;
      pfd[1].fd      = ctl->wake[0];
      pfd[1].events  = POLLIN;
      pfd[1].revents = 0;
      if (poll(pfd, 2, -1) < 0)
         continue;
      if (pfd[1].revents & POLLIN)
      {
         char buf[64];
         while (read(ctl->wake[0], buf, sizeof(buf)) > 0) { }
      }
   }
}

bool x11_monitors_start(const char *display)
{
   sthread_t *thread;
   x11_monitors_ctl_t *ctl = &x11_monitors_ctl;

   if (!retro_atomic_load_acquire_int(&ctl->piped))
   {
      if (pipe(ctl->wake) != 0)
         return false;
      if (     fcntl(ctl->wake[0], F_SETFL, O_NONBLOCK) != 0
            || fcntl(ctl->wake[1], F_SETFL, O_NONBLOCK) != 0)
      {
         close(ctl->wake[0]);
         close(ctl->wake[1]);
         return false;
      }
      /* Not inherited by system()'s children */
      fcntl(ctl->wake[0], F_SETFD, FD_CLOEXEC);
      fcntl(ctl->wake[1], F_SETFD, FD_CLOEXEC);
      retro_atomic_store_release_int(&ctl->piped, 1);
   }
   else if (retro_atomic_load_acquire_int(&ctl->state) >= 0)
      return true;

   strlcpy(ctl->name, display ? display : "", sizeof(ctl->name));
   retro_atomic_store_release_int(&ctl->state, 0);
   retro_atomic_fetch_or_int(&ctl->want,
         X11_MONITORS_ENUMERATE | X11_MONITORS_LOCATE);
   if (!(thread = sthread_create(x11_monitors_worker, ctl)))
   {
      retro_atomic_store_release_int(&ctl->state, -1);
      return false;
   }
   /* Lives as long as the process, as g_x11_dpy does */
   sthread_detach(thread);
   return true;
}

#endif
