/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_x11_live_test.c).
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

/* End-to-end mode switch on a live X server: the video modeline engine
 * generates a 15 kHz timing for a core resolution, the X11 display
 * server puts it on the wire through XRandR, and the test reads the
 * server back to check that the crtc is scanning out exactly the
 * timing the engine produced. Then a second switch, then restore and
 * close, and the server must be back on its desktop mode with the
 * generated modes gone.
 *
 * Needs an X server whose RandR implementation honours XRRCreateMode
 * on an output: an Xorg 'dummy' driver server does, Xvfb does not (it
 * returns success and lists nothing). With no DISPLAY the test skips
 * rather than fails so it can sit in the default sample build. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/modeline/modeline_list.h"

/* RetroArch-side symbols the driver refers to */
Display *g_x11_dpy = NULL;
Window   g_x11_win = 0;
int      g_x11_screen = 0;

static int s_verbose;

void live_log(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

/* verbosity.h entry points for the driver object */
void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   if (!s_verbose)
      return;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
}

void video_monitor_set_refresh_rate(float hz) { (void)hz; }

/* Which connected output's crtc holds a root-relative point */
static int output_under(Display *dpy, int x, int y, char *name, size_t len)
{
   int o, found = -1;
   Window root = DefaultRootWindow(dpy);
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
   if (!res)
      return -1;
   for (o = 0; o < res->noutput && found < 0; o++)
   {
      XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, res->outputs[o]);
      XRRCrtcInfo *ci;
      if (!oi)
         continue;
      if (oi->connection == RR_Connected && oi->crtc
            && (ci = XRRGetCrtcInfo(dpy, res, oi->crtc)))
      {
         if (x >= ci->x && x < ci->x + (int)ci->width
               && y >= ci->y && y < ci->y + (int)ci->height)
         {
            found = o;
            strncpy(name, oi->name, len - 1);
            name[len - 1] = '\0';
         }
         XRRFreeCrtcInfo(ci);
      }
      XRRFreeOutputInfo(oi);
   }
   XRRFreeScreenResources(res);
   return found;
}

/* Head selection: with a RetroArch window on the display, "auto"
 * must bind the output under it, and a monitor index must bind the
 * output under the Xinerama screen the context driver would place
 * the window on. Both are checked through what enum then reports as
 * the desktop mode of the bound head. */
static int test_head_selection(Display *dpy, video_modeline_ops_t *ops,
      void *data)
{
   video_modeline_disp_t ds;
   video_modeline_t modes[64];
   char under[64];
   int i, n;
   int wx = 40, wy = 30;
   Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), wx, wy,
         200, 100, 0, 0, 0);
   XMapWindow(dpy, win);
   XSync(dpy, False);
   g_x11_win = win;

   if (output_under(dpy, wx + 100, wy + 50, under, sizeof(under)) < 0)
   {
      fprintf(stderr, "FAIL: no output under the test window\n");
      return 1;
   }

   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "auto");
   if (!ops->open(data, &ds))
   {
      fprintf(stderr, "FAIL: open(auto) with a window on the display\n");
      return 1;
   }
   n = ops->enum_modes(data, modes, 64);
   for (i = 0; i < n; i++)
      if (modes[i].type & MODELINE_DESKTOP)
         break;
   if (n < 1 || i == n)
   {
      fprintf(stderr, "FAIL: open(auto) bound a head with no desktop mode\n");
      return 1;
   }
   printf("[pass] open(auto): head under the window (%s), desktop %dx%d\n",
         under, (int)VIDEO_SCALE_W(modes[i].dims),
         (int)VIDEO_SCALE_H(modes[i].dims));
   ops->close(data);

   /* Monitor index 1 -> Xinerama screen 0 */
   strcpy(ds.screen, "0");
   if (!ops->open(data, &ds))
   {
      fprintf(stderr, "FAIL: open(0) through the Xinerama screen\n");
      return 1;
   }
   n = ops->enum_modes(data, modes, 64);
   if (n < 1)
   {
      fprintf(stderr, "FAIL: open(0) bound no head\n");
      return 1;
   }
   printf("[pass] open(0): head under Xinerama screen 0, %d modes\n", n);
   ops->close(data);

   /* A connector name binds directly */
   strcpy(ds.screen, under);
   if (!ops->open(data, &ds))
   {
      fprintf(stderr, "FAIL: open(%s) by connector name\n", under);
      return 1;
   }
   printf("[pass] open(%s): head by connector name\n", under);
   ops->close(data);

   XDestroyWindow(dpy, win);
   g_x11_win = 0;
   return 0;
}

/* What the server is scanning out on the first connected output */
typedef struct
{
   RRMode mode;
   unsigned width, height;
   unsigned hsync_start, hsync_end, htotal;
   unsigned vsync_start, vsync_end, vtotal;
   unsigned long dot_clock;
   unsigned long flags;
   char name[64];
   int nmode_total;
   int ra_modes; /* modes named RA-* in the resources */
} live_state_t;

static int live_read(Display *dpy, live_state_t *st)
{
   int o, m;
   Window root = DefaultRootWindow(dpy);
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
   memset(st, 0, sizeof(*st));
   if (!res)
      return -1;
   st->nmode_total = res->nmode;
   for (m = 0; m < res->nmode; m++)
      if (!strncmp(res->modes[m].name, "RA-", 3))
         st->ra_modes++;
   for (o = 0; o < res->noutput; o++)
   {
      XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, res->outputs[o]);
      XRRCrtcInfo *ci;
      if (!oi)
         continue;
      if (oi->connection != RR_Connected || !oi->crtc)
      {
         XRRFreeOutputInfo(oi);
         continue;
      }
      ci = XRRGetCrtcInfo(dpy, res, oi->crtc);
      if (ci)
      {
         st->mode = ci->mode;
         for (m = 0; m < res->nmode; m++)
         {
            if (res->modes[m].id == ci->mode)
            {
               XRRModeInfo *mi  = &res->modes[m];
               st->width        = mi->width;
               st->height       = mi->height;
               st->hsync_start  = mi->hSyncStart;
               st->hsync_end    = mi->hSyncEnd;
               st->htotal       = mi->hTotal;
               st->vsync_start  = mi->vSyncStart;
               st->vsync_end    = mi->vSyncEnd;
               st->vtotal       = mi->vTotal;
               st->dot_clock    = mi->dotClock;
               st->flags        = mi->modeFlags;
               strncpy(st->name, mi->name, sizeof(st->name) - 1);
            }
         }
         XRRFreeCrtcInfo(ci);
      }
      XRRFreeOutputInfo(oi);
      break;
   }
   XRRFreeScreenResources(res);
   return 0;
}

static int check_on_wire(Display *dpy, const video_modeline_t *mode,
      const char *what)
{
   live_state_t st;
   if (live_read(dpy, &st) < 0)
   {
      fprintf(stderr, "FAIL: %s: could not read the server back\n", what);
      return 1;
   }
   if (st.mode != (RRMode)mode->platform_data)
   {
      fprintf(stderr, "FAIL: %s: crtc scans out mode %lx, expected %llx (%s)\n",
            what, st.mode, (unsigned long long)mode->platform_data, st.name);
      return 1;
   }
   if (st.width != (unsigned)mode->hactive || st.height != (unsigned)mode->vactive
         || st.hsync_start != (unsigned)mode->hbegin || st.hsync_end != (unsigned)mode->hend
         || st.htotal != (unsigned)mode->htotal
         || st.vsync_start != (unsigned)mode->vbegin || st.vsync_end != (unsigned)mode->vend
         || st.vtotal != (unsigned)mode->vtotal
         || st.dot_clock != (unsigned long)mode->pclock)
   {
      fprintf(stderr,
            "FAIL: %s: server timing %lu %u %u %u %u %u %u %u %u differs from generated %llu %d %d %d %d %d %d %d %d\n",
            what, st.dot_clock, st.width, st.hsync_start, st.hsync_end, st.htotal,
            st.height, st.vsync_start, st.vsync_end, st.vtotal,
            (unsigned long long)mode->pclock, mode->hactive, mode->hbegin, mode->hend,
            mode->htotal, mode->vactive, mode->vbegin, mode->vend, mode->vtotal);
      return 1;
   }
   if (!!(st.flags & RR_Interlace) != !!mode->interlace
         || !!(st.flags & RR_DoubleScan) != !!mode->doublescan
         || !!(st.flags & RR_HSyncPositive) != !!mode->hsync
         || !!(st.flags & RR_VSyncPositive) != !!mode->vsync)
   {
      fprintf(stderr, "FAIL: %s: server flags %lx do not match i%d d%d h%d v%d\n",
            what, st.flags, mode->interlace, mode->doublescan, mode->hsync, mode->vsync);
      return 1;
   }
   printf("[pass] %s: server scans out %s %ux%u clock %lu, the generated timing\n",
         what, st.name, st.width, st.height, st.dot_clock);
   return 0;
}

/* What the managed head scans out now: size, field rate, and the
 * framebuffer size from the root window geometry */
typedef struct
{
   unsigned w, h, fb_w, fb_h;
   float rate;
   RRMode id;
} res_state_t;

static int res_read(Display *dpy, res_state_t *st)
{
   int o;
   Window root = DefaultRootWindow(dpy), groot;
   int gx, gy;
   unsigned gb, gd;
   XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
   memset(st, 0, sizeof(*st));
   if (!res)
      return -1;
   XGetGeometry(dpy, root, &groot, &gx, &gy, &st->fb_w, &st->fb_h, &gb, &gd);
   for (o = 0; o < res->noutput && !st->id; o++)
   {
      XRROutputInfo *oi = XRRGetOutputInfo(dpy, res, res->outputs[o]);
      XRRCrtcInfo *ci;
      if (!oi)
         continue;
      if (oi->connection == RR_Connected && oi->crtc
            && (ci = XRRGetCrtcInfo(dpy, res, oi->crtc)))
      {
         int m;
         for (m = 0; m < res->nmode; m++)
         {
            XRRModeInfo *mi = &res->modes[m];
            if (mi->id != ci->mode)
               continue;
            st->id = mi->id;
            st->w  = mi->width;
            st->h  = mi->height;
            if (mi->hTotal && mi->vTotal)
               st->rate = (float)((double)mi->dotClock
                     / ((double)mi->hTotal * (double)mi->vTotal));
            break;
         }
         XRRFreeCrtcInfo(ci);
      }
      XRRFreeOutputInfo(oi);
   }
   XRRFreeScreenResources(res);
   return st->id ? 0 : -1;
}

static int res_expect(Display *dpy, unsigned w, unsigned h, float hz,
      const char *what)
{
   res_state_t st;
   if (res_read(dpy, &st) < 0)
   {
      fprintf(stderr, "FAIL: %s: no lit head to read back\n", what);
      return 1;
   }
   if (st.w != w || st.h != h || fabs(st.rate - hz) > 0.01)
   {
      fprintf(stderr, "FAIL: %s: head scans out %ux%u %.3f Hz, wanted %ux%u %.3f Hz\n",
            what, st.w, st.h, st.rate, w, h, hz);
      return 1;
   }
   /* One head: the framebuffer follows it, growing and shrinking */
   if (st.fb_w != w || st.fb_h != h)
   {
      fprintf(stderr, "FAIL: %s: framebuffer is %ux%u, head is %ux%u\n",
            what, st.fb_w, st.fb_h, w, h);
      return 1;
   }
   printf("[pass] %s: %ux%u %.3f Hz, framebuffer %ux%u\n",
         what, st.w, st.h, st.rate, st.fb_w, st.fb_h);
   return 0;
}

/* Settings > Video > Output > Screen Resolution, and the refresh rate
 * autoswitch that rides on the same two callbacks. Without them the
 * menu hides the entry on X11 (video_display_server_has_resolution_list)
 * and the autoswitch never runs. The list must describe the lit head,
 * mark exactly its mode current, and every switch - smaller, larger,
 * rate only, back - must land on the wire with the framebuffer
 * following. A size the head does not list must fail and change
 * nothing. */
static int test_screen_resolution(Display *dpy, void *data)
{
   unsigned i, j, n = 0, ncur = 0;
   int small = -1, big = -1, multi_a = -1, multi_b = -1;
   res_state_t orig, now;
   video_display_config_t *list;

   if (!dispserv_x11.get_resolution_list || !dispserv_x11.set_resolution)
   {
      fprintf(stderr, "FAIL: dispserv_x11 has no get_resolution_list/set_resolution;"
            " the Screen Resolution entry is hidden on X11\n");
      return 1;
   }
   if (res_read(dpy, &orig) < 0)
      return 1;

   list = (video_display_config_t*)dispserv_x11.get_resolution_list(data, &n);
   if (!list || n < 2)
   {
      fprintf(stderr, "FAIL: get_resolution_list returned %u entries\n", n);
      free(list);
      return 1;
   }
   for (i = 0; i < n; i++)
   {
      unsigned w = VIDEO_SCALE_W(list[i].dims), h = VIDEO_SCALE_H(list[i].dims);
      if (list[i].current)
      {
         ncur++;
         if (w != orig.w || h != orig.h
               || fabs(list[i].refreshrate_float - orig.rate) > 0.01)
         {
            fprintf(stderr, "FAIL: entry %u %ux%u %.3f marked current, head is %ux%u %.3f\n",
                  i, w, h, list[i].refreshrate_float, orig.w, orig.h, orig.rate);
            free(list);
            return 1;
         }
      }
      if (list[i].idx != i
            || (i && list[i - 1].dims > list[i].dims)
            || list[i].refreshrate != (unsigned)floor(list[i].refreshrate_float + 0.001f))
      {
         fprintf(stderr, "FAIL: entry %u out of order or mislabelled\n", i);
         free(list);
         return 1;
      }
      for (j = 0; j < i; j++)
         if (list[j].dims == list[i].dims
               && list[j].interlaced == list[i].interlaced
               && fabs(list[j].refreshrate_float - list[i].refreshrate_float) < 0.005)
         {
            fprintf(stderr, "FAIL: %ux%u %.3f listed twice\n",
                  w, h, list[i].refreshrate_float);
            free(list);
            return 1;
         }
      if (list[i].interlaced || list[i].dblscan)
         continue;
      if (w < orig.w && h < orig.h)
      {
         /* The smallest size with two progressive rates doubles as
          * the rate-only case */
         if (small < 0)
            small = (int)i;
         if (multi_a < 0 && i + 1 < n && list[i + 1].dims == list[i].dims
               && !list[i + 1].interlaced && !list[i + 1].dblscan)
         {
            multi_a = (int)i;
            multi_b = (int)i + 1;
         }
      }
      if (w > orig.w && h > orig.h && big < 0)
         big = (int)i;
   }
   if (ncur != 1 || small < 0 || big < 0 || multi_a < 0)
   {
      fprintf(stderr, "FAIL: %u current entries, smaller %d larger %d multi-rate %d\n",
            ncur, small, big, multi_a);
      free(list);
      return 1;
   }
   printf("[pass] get_resolution_list: %u entries, current %ux%u %.3f Hz\n",
         n, orig.w, orig.h, orig.rate);

   /* Smaller: crtc first, then the framebuffer shrinks */
   if (!dispserv_x11.set_resolution(data, list[small].dims,
            (int)list[small].refreshrate, list[small].refreshrate_float, 0, 0, 0, 0)
         || res_expect(dpy, VIDEO_SCALE_W(list[small].dims),
            VIDEO_SCALE_H(list[small].dims), list[small].refreshrate_float,
            "switch to a smaller mode"))
      goto fail;

   /* Larger: the framebuffer grows first */
   if (!dispserv_x11.set_resolution(data, list[big].dims,
            (int)list[big].refreshrate, list[big].refreshrate_float, 0, 0, 0, 0)
         || res_expect(dpy, VIDEO_SCALE_W(list[big].dims),
            VIDEO_SCALE_H(list[big].dims), list[big].refreshrate_float,
            "switch to a larger mode"))
      goto fail;

   /* Rate only, the way video_display_server_set_refresh_rate() asks:
    * no size, the size stays */
   if (!dispserv_x11.set_resolution(data, list[multi_a].dims,
            (int)list[multi_a].refreshrate, list[multi_a].refreshrate_float, 0, 0, 0, 0)
         || !dispserv_x11.set_resolution(data, 0,
            (int)list[multi_b].refreshrate, list[multi_b].refreshrate_float, 0, 0, 0, 0)
         || res_expect(dpy, VIDEO_SCALE_W(list[multi_b].dims),
            VIDEO_SCALE_H(list[multi_b].dims), list[multi_b].refreshrate_float,
            "refresh rate only"))
      goto fail;

   /* Whole hertz from the menu label alone still finds the mode */
   if (!dispserv_x11.set_resolution(data, list[multi_a].dims,
            (int)list[multi_a].refreshrate, 0.0f, 0, 0, 0, 0)
         || res_expect(dpy, VIDEO_SCALE_W(list[multi_a].dims),
            VIDEO_SCALE_H(list[multi_a].dims), list[multi_a].refreshrate_float,
            "whole-hertz rate"))
      goto fail;

   /* A size the head does not list: refused, nothing moves */
   res_read(dpy, &now);
   if (dispserv_x11.set_resolution(data, VIDEO_SCALE_PACK(1234, 567),
            60, 60.0f, 0, 0, 0, 0))
   {
      fprintf(stderr, "FAIL: an unlisted 1234x567 was accepted\n");
      goto fail;
   }
   if (res_expect(dpy, now.w, now.h, now.rate, "unlisted size refused"))
      goto fail;

   /* Back to where the head started */
   if (!dispserv_x11.set_resolution(data, VIDEO_SCALE_PACK(orig.w, orig.h),
            (int)floor(orig.rate + 0.001f), orig.rate, 0, 0, 0, 0)
         || res_expect(dpy, orig.w, orig.h, orig.rate, "restore"))
      goto fail;

   free(list);
   return 0;

fail:
   free(list);
   return 1;
}

int main(void)
{
   Display *dpy;
   live_state_t desktop, after;
   void *data;
   video_modeline_ops_t ops;
   video_modeline_disp_t ds;
   video_modeline_gen_t *gen;
   video_modeline_t *mode;
   video_modeline_t first;
   int event_base, error_base;

   s_verbose = getenv("LIVE_VERBOSE") != NULL;

   if (!getenv("DISPLAY") || !(dpy = XOpenDisplay(NULL)))
   {
      puts("[skip] no X display; run against an Xorg dummy-driver server");
      return 0;
   }
   if (!XRRQueryExtension(dpy, &event_base, &error_base))
   {
      puts("[skip] no RandR on this display");
      XCloseDisplay(dpy);
      return 0;
   }
   g_x11_dpy = dpy;

   if (live_read(dpy, &desktop) < 0 || desktop.mode == 0)
   {
      fprintf(stderr, "FAIL: no connected output with a mode on this display\n");
      return 1;
   }
   if (desktop.ra_modes)
   {
      fprintf(stderr, "FAIL: %d RA-* mode(s) already on the server; a previous run did not restore\n",
            desktop.ra_modes);
      return 1;
   }
   printf("desktop: %s %ux%u (%d modes listed)\n", desktop.name,
         desktop.width, desktop.height, desktop.nmode_total);

   /* Engine + display server, the way the CRT consumer wires them */
   gen = modeline_gen_new();
   if (!gen)
      return 1;
   modeline_set_monitor(gen, "arcade_15");

   data = dispserv_x11.init();
   memset(&ops, 0, sizeof(ops));
   ops.data       = data;
   ops.open       = dispserv_x11.modeline_open;
   ops.close      = dispserv_x11.modeline_close;
   ops.caps       = dispserv_x11.modeline_caps;
   ops.enum_modes = dispserv_x11.modeline_enum;
   ops.add        = dispserv_x11.modeline_add;
   ops.update     = dispserv_x11.modeline_update;
   ops.del        = dispserv_x11.modeline_delete;
   ops.set        = dispserv_x11.modeline_set;
   ops.flush      = dispserv_x11.modeline_flush;
   ops.name       = "x11";

   if (test_head_selection(dpy, &ops, data))
      return 1;

   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "auto");
   if (!ops.open(data, &ds))
   {
      fprintf(stderr, "FAIL: modeline_open failed on the live server\n");
      return 1;
   }
   if (!modeline_list_init(gen, &ops))
   {
      fprintf(stderr, "FAIL: modeline_list_init failed\n");
      return 1;
   }
   if (gen->num_modes < 1 || !(gen->desktop_mode.type & MODELINE_DESKTOP))
   {
      fprintf(stderr, "FAIL: enum listed %d modes, desktop flagged %d\n",
            gen->num_modes, !!(gen->desktop_mode.type & MODELINE_DESKTOP));
      return 1;
   }
   printf("[pass] open + enum: %d listed modes, desktop %dx%d@%d\n",
         gen->num_modes, (int)VIDEO_SCALE_W(gen->desktop_mode.dims),
         (int)VIDEO_SCALE_H(gen->desktop_mode.dims),
         gen->desktop_mode.refresh);

   /* A 320x240@60 core: generated, added, set, verified on the wire */
   mode = modeline_get(gen, &ops, VIDEO_SCALE_PACK(320, 240), 60.0, 0);
   if (!mode || !(mode->type & MODELINE_ADD))
   {
      fprintf(stderr, "FAIL: get did not produce a new mode for 320x240@60\n");
      return 1;
   }
   if (!modeline_flush(gen, &ops) || mode->platform_data == 0)
   {
      fprintf(stderr, "FAIL: flush did not create the mode on the server\n");
      return 1;
   }
   if (!modeline_set(gen, &ops, mode))
   {
      fprintf(stderr, "FAIL: set failed\n");
      return 1;
   }
   if (check_on_wire(dpy, mode, "320x240@60"))
      return 1;
   first = *mode;

   /* A second core resolution: a second generated mode, then the
    * switch, with the first one still in the server's list */
   mode = modeline_get(gen, &ops, VIDEO_SCALE_PACK(256, 224), 60.0988, 0);
   if (!mode || !modeline_flush(gen, &ops) || !modeline_set(gen, &ops, mode))
   {
      fprintf(stderr, "FAIL: second switch failed\n");
      return 1;
   }
   if (check_on_wire(dpy, mode, "256x224@60.0988"))
      return 1;
   if (live_read(dpy, &after) < 0 || after.ra_modes != 2)
   {
      fprintf(stderr, "FAIL: expected both generated modes listed, found %d\n",
            after.ra_modes);
      return 1;
   }

   /* Restore: generated modes deleted, desktop mode scanning out */
   if (!modeline_restore(gen, &ops))
   {
      fprintf(stderr, "FAIL: restore reported an error\n");
      return 1;
   }
   ops.close(data);
   if (live_read(dpy, &after) < 0)
      return 1;
   if (after.mode != desktop.mode)
   {
      fprintf(stderr, "FAIL: after close the crtc scans out %s, not the desktop %s\n",
            after.name, desktop.name);
      return 1;
   }
   if (after.ra_modes != 0 || after.nmode_total != desktop.nmode_total)
   {
      fprintf(stderr, "FAIL: after close %d RA-* modes remain, %d listed vs %d before\n",
            after.ra_modes, after.nmode_total, desktop.nmode_total);
      return 1;
   }
   printf("[pass] restore + close: desktop %s back, generated modes gone\n",
         after.name);

   /* The first mode's id must not resolve to anything any more */
   {
      Window root = DefaultRootWindow(dpy);
      XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
      int m;
      for (m = 0; res && m < res->nmode; m++)
      {
         if (res->modes[m].id == (RRMode)first.platform_data && first.platform_data)
         {
            fprintf(stderr, "FAIL: deleted mode id still listed\n");
            return 1;
         }
      }
      if (res)
         XRRFreeScreenResources(res);
   }

   if (test_screen_resolution(dpy, data))
      return 1;

   dispserv_x11.destroy(data);
   modeline_gen_free(gen);
   XCloseDisplay(dpy);
   puts("ALL OK");
   return 0;
}
