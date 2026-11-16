/* The X display server's vblank clock (wait_vblank), against a real
 * server's Present extension. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

#include <boolean.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../../../gfx/video_display_server.h"

extern const video_display_server_t dispserv_x11;

Display *g_x11_dpy;
Window   g_x11_win;

static int failures;
#define CHECK(cond, msg) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); } } while (0)

static Window make_window(void)
{
   Window w = XCreateSimpleWindow(g_x11_dpy, DefaultRootWindow(g_x11_dpy),
         0, 0, 64, 64, 0, 0, 0);
   XMapWindow(g_x11_dpy, w);
   XSync(g_x11_dpy, False);
   return w;
}

/* @frames vblanks one after the other; the count reached */
static unsigned run(void *ds, unsigned seen, unsigned frames)
{
   unsigned i;
   retro_time_t last = 0, at = 0;
   retro_time_t start = cpu_features_get_time_usec();
   for (i = 0; i < frames; i++)
   {
      unsigned n = dispserv_x11.wait_vblank(ds, seen, 500000, &at);
      CHECK(n > seen, "a wait did not return a later vblank");
      CHECK(at > last, "a vblank is not after the one before it");
      CHECK(at <= cpu_features_get_time_usec(), "a vblank from the future");
      if (!n)
         return 0;
      last = at;
      seen = n;
   }
   /* waited for, not handed back at once: no display runs at 500 Hz */
   CHECK(cpu_features_get_time_usec() - start >= (retro_time_t)frames * 2000,
         "the vblanks were not waited for");
   return seen;
}

int main(void)
{
   void *ds;
   unsigned n, m;
   retro_time_t at = 0, now;
   Window first;

   if (!(g_x11_dpy = XOpenDisplay(NULL)))
   {
      fprintf(stderr, "no X display\n");
      return 1;
   }
   ds = dispserv_x11.init();
   CHECK(dispserv_x11.wait_vblank != NULL, "no vblank clock");
   if (!ds || !dispserv_x11.wait_vblank)
      return 1;

   /* no window, no clock */
   CHECK(!dispserv_x11.wait_vblank(ds, 0, 100000, &at), "a clock with no window");

   g_x11_win = first = make_window();
   n   = dispserv_x11.wait_vblank(ds, 0, 500000, &at);
   now = cpu_features_get_time_usec();
   CHECK(n, "no vblank on a mapped window");
   CHECK(at <= now && now - at < 1000000, "the stamp is not of this clock");

   /* asked without a wait: what is known, at once */
   m = dispserv_x11.wait_vblank(ds, n, 0, &at);
   CHECK(m >= n, "the count went back");
   n = run(ds, m, 30);

   /* nobody waiting for a while: the first read may be the vblank that
    * came just after the last wait, the one after it is current */
   retro_sleep(600);
   m   = dispserv_x11.wait_vblank(ds, n, 500000, &at);
   CHECK(m > n, "no vblank after a pause");
   m   = dispserv_x11.wait_vblank(ds, m, 500000, &at);
   now = cpu_features_get_time_usec();
   CHECK(m && now - at < 500000, "a stale vblank after a pause");
   n   = run(ds, m, 10);

   /* the window gone: no clock, and none asked for again... */
   XDestroyWindow(g_x11_dpy, first);
   XSync(g_x11_dpy, False);
   {
      unsigned i;
      m = n;
      for (i = 0; i < 100 && m; i++)
         m = dispserv_x11.wait_vblank(ds, m, 100000, &at);
      CHECK(!m, "vblanks of a destroyed window");
      CHECK(!dispserv_x11.wait_vblank(ds, 0, 100000, &at),
            "a destroyed window asked about again");
   }
   /* ...until there is another */
   g_x11_win = make_window();
   n = dispserv_x11.wait_vblank(ds, 0, 500000, &at);
   CHECK(n, "no vblank on the next window");
   run(ds, n, 10);

   dispserv_x11.destroy(ds);
   XDestroyWindow(g_x11_dpy, g_x11_win);
   XCloseDisplay(g_x11_dpy);
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   puts("[pass] x11 vblank clock");
   return 0;
}
