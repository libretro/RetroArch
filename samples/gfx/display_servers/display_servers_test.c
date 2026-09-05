/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (display_servers_test.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the XRandR query handling in
 * gfx/display_servers/dispserv_x11.c.
 *
 * Unlike most samples here, this one links the real
 * dispserv_x11.c rather than mirroring its shape: the file
 * refers to only four RetroArch-side symbols (g_x11_dpy,
 * g_x11_win, strlcpy_retro__, video_monitor_set_refresh_rate),
 * which makes it cheap to stand up on its own, and linking it
 * means the test actually regresses if the code changes.
 *
 * THE CRASH
 *
 * Every XRandR query in that file was treated as infallible.
 * On any X server that advertises RandR without a usable
 * output, x11_display_server_get_screen_orientation() took the
 * process down inside video_display_server_init(), before
 * RetroArch had drawn anything:
 *
 *   #0 x11_display_server_get_screen_orientation ()
 *   #1 video_display_server_init ()
 *   #2 rarch_main ()
 *
 * XRRGetOutputInfo() returns NULL for an output the server
 * cannot describe, and info->connection was read straight off
 * it.  Same for XRRGetCrtcInfo() and crtc->width.
 * XRRGetScreenResources() can fail.
 * x11_display_server_open_display() can return NULL, and
 * DefaultRootWindow() dereferences its argument.  And
 * XRRGetScreenInfo() can fail while XRRFreeScreenConfigInfo()
 * is not NULL-tolerant, so the cleanup path faulted on the way
 * out.  x11_display_server_destroy() and both branches of
 * set_resolution() had the same pattern.
 *
 * Reachability: Xvfb is the cheapest way to hit it -- this was
 * found by running RetroArch headless on llvmpipe -- but a real
 * display with everything disconnected reaches the same code,
 * as does a server where RandR is present but the output is
 * being reconfigured underneath the query.
 *
 * THE OUT-OF-BOUNDS WALK
 *
 * Both orientation walks iterated
 *
 *   for (j = 0; j < info->ncrtc; j++)
 *      XRRGetCrtcInfo(dpy, screen, screen->crtcs[j]);
 *
 * ncrtc bounds info->crtcs, not screen->crtcs.  Reading the
 * screen's array with the output's count is out of bounds
 * whenever an output has more crtcs than the screen has, and
 * where it happens to stay in bounds it still describes the
 * wrong crtc.  Nine XRRSetCrtcConfig() calls then went on to
 * configure screen->crtcs[j] / res->crtcs[i] after describing
 * outputs->crtc -- configuring one crtc from another's
 * geometry.
 *
 * WHY A STUB X SERVER
 *
 * The behaviour under test is what happens when the RandR
 * queries *fail*, and a working display is precisely the
 * environment that cannot produce that.  The stub below stands
 * in for libXrandr and the handful of Xlib entry points
 * dispserv_x11.c reaches, so this needs no X server, no display
 * and no RandR extension -- only the headers.
 *
 * Two things it does beyond returning NULL on demand.  It
 * numbers the screen's crtcs from 100 and the output's from
 * 200, so which array the code walked is readable straight off
 * the call log rather than inferred.  And it tracks every
 * pointer it hands out, so a double free, a free of something
 * it never allocated, or a leak is a failure in the case that
 * caused it instead of something the sanitizer reports against
 * whichever case happens to run last.
 *
 * Six checks covering eleven scenarios.  Against the fixed tree
 * they pass.  Point the Makefile's dispserv_x11.c at the tree
 * before any of this was fixed and the very first one takes the
 * process down with SIGSEGV, which is the whole point.
 *
 * Build and run:
 *
 *     make && ./display_servers_test
 *     make clean && make SANITIZER=address,undefined && ./display_servers_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlibint.h>
#include <X11/extensions/Xrandr.h>

#include "../../../gfx/video_display_server.h"

extern const video_display_server_t dispserv_x11;

#define SCREEN_CRTC_BASE 100
#define OUTPUT_CRTC_BASE 200
#define MAX_ALLOCS        64
#define MAX_LOGGED        16

/* ------------------------------------------------------------------
 * Stub knobs and observations
 * ------------------------------------------------------------------ */

typedef struct
{
   int fail_open_display;
   int fail_screen_resources;
   int fail_screen_info;
   int fail_output_info;
   int fail_crtc_info;

   /* Making output_ncrtc larger than screen_ncrtc is what
    * distinguishes indexing info->crtcs from screen->crtcs: the
    * latter runs off the end of the screen's array. */
   int screen_ncrtc;
   int output_ncrtc;

   int output_connection; /* RR_Connected or RR_Disconnected */

   /* A real server lists the output's crtc among the screen's; the
    * orientation cases deliberately keep them disjoint to catch
    * indexing one array with the other, so the modeline cases opt
    * in to the realistic layout. */
   int output_crtc_in_screen;
} stub_cfg_t;

typedef struct
{
   int    get_crtc_info_calls;
   int    set_crtc_config_calls;
   RRCrtc queried_crtcs[MAX_LOGGED];
   RRCrtc configured_crtcs[MAX_LOGGED];
   int    create_mode_calls;
   int    destroy_mode_calls;
   int    add_output_mode_calls;
   int    delete_output_mode_calls;
   int    bad_free;
} stub_log_t;

static stub_cfg_t  s_cfg;
static stub_log_t  s_log;

static void       *s_allocs[MAX_ALLOCS];
static const char *s_alloc_tag[MAX_ALLOCS];
static int         s_alloc_freed[MAX_ALLOCS];
static int         s_alloc_count;
static const char *s_tag = "?";

static Display *s_dpy;
static Screen   s_screen;
static char     s_mode_name[64];

static void *stub_alloc(size_t len)
{
   void *p;
   if (s_alloc_count >= MAX_ALLOCS)
      return NULL;
   if (!(p = calloc(1, len)))
      return NULL;
   s_allocs[s_alloc_count]     = p;
   s_alloc_tag[s_alloc_count]  = s_tag;
   s_alloc_freed[s_alloc_count] = 0;
   s_alloc_count++;
   return p;
}

static void stub_free(void *p)
{
   int i;

   /* The real XRRFree* entry points are not NULL-tolerant, so
    * handing them NULL is itself the defect. */
   if (!p)
   {
      s_log.bad_free++;
      return;
   }

   /* Newest first: the allocator reuses addresses, so the same
    * value can appear more than once and only the most recent
    * entry is live. */
   for (i = s_alloc_count - 1; i >= 0; i--)
   {
      if (s_allocs[i] != p || s_alloc_freed[i])
         continue;
      s_alloc_freed[i] = 1;
      free(p);
      return;
   }
   s_log.bad_free++;
}

static void stub_reset(const stub_cfg_t *cfg)
{
   int i;
   for (i = 0; i < s_alloc_count; i++)
      if (!s_alloc_freed[i])
         free(s_allocs[i]);
   s_alloc_count  = 0;
   s_mode_name[0] = '\0';

   memset(&s_log, 0, sizeof(s_log));
   s_cfg = *cfg;

   if (!s_dpy)
   {
      s_dpy = (Display*)calloc(1, sizeof(Display));
      memset(&s_screen, 0, sizeof(s_screen));
      s_screen.root    = 1;
      s_screen.width   = 1280;
      s_screen.height  = 720;
      s_screen.mwidth  = 340;
      s_screen.mheight = 190;
      ((_XPrivDisplay)s_dpy)->nscreens       = 1;
      ((_XPrivDisplay)s_dpy)->screens        = &s_screen;
      ((_XPrivDisplay)s_dpy)->default_screen = 0;
   }
}

static int stub_leaks(const char *what)
{
   int i, leaked = 0;
   for (i = 0; i < s_alloc_count; i++)
      if (!s_alloc_freed[i])
      {
         fprintf(stderr, "FAIL: %s leaked a %s allocation\n",
               what, s_alloc_tag[i]);
         leaked++;
      }
   return leaked;
}

/* Defaults: everything succeeds, one connected output, screen
 * and output agreeing on a single crtc. */
static void cfg_default(stub_cfg_t *cfg)
{
   memset(cfg, 0, sizeof(*cfg));
   cfg->screen_ncrtc      = 1;
   cfg->output_ncrtc      = 1;
   cfg->output_connection = RR_Connected;
}

/* ------------------------------------------------------------------
 * Xlib
 * ------------------------------------------------------------------ */

Display *XOpenDisplay(const char *name)
{
   (void)name;
   return s_cfg.fail_open_display ? NULL : s_dpy;
}

int XCloseDisplay(Display *dpy)       { (void)dpy; return 0; }
int XGrabServer(Display *dpy)         { (void)dpy; return 0; }
int XUngrabServer(Display *dpy)       { (void)dpy; return 0; }
int XSync(Display *dpy, Bool discard) { (void)dpy; (void)discard; return 0; }

Atom XInternAtom(Display *dpy, const char *name, Bool only_if_exists)
{
   (void)dpy; (void)name; (void)only_if_exists;
   return 1;
}

int XChangeProperty(Display *dpy, Window w, Atom prop, Atom type, int fmt,
      int mode, const unsigned char *data, int nelements)
{
   (void)dpy; (void)w; (void)prop; (void)type; (void)fmt; (void)mode;
   (void)data; (void)nelements;
   return 0;
}

int XDeleteProperty(Display *dpy, Window w, Atom prop)
{
   (void)dpy; (void)w; (void)prop;
   return 0;
}

/* The modeline path traps X errors around each RandR call; the
 * stub never raises one, so the handler is stored and returned. */
static XErrorHandler s_error_handler;
XErrorHandler XSetErrorHandler(XErrorHandler handler)
{
   XErrorHandler old = s_error_handler;
   s_error_handler   = handler;
   return old;
}

int XClearWindow(Display *dpy, Window w) { (void)dpy; (void)w; return 0; }

/* The modeline path locates the head under the RetroArch window;
 * g_x11_win is 0 here, so neither is reached, but the driver links
 * them. */
Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *attr)
{
   (void)dpy; (void)w;
   memset(attr, 0, sizeof(*attr));
   return 0;
}

Bool XTranslateCoordinates(Display *dpy, Window src, Window dst,
      int sx, int sy, int *dx, int *dy, Window *child)
{
   (void)dpy; (void)src; (void)dst; (void)sx; (void)sy;
   *dx = *dy = 0;
   *child = 0;
   return False;
}

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *v)
{
   (void)dpy; (void)d; (void)mask; (void)v;
   return (GC)stub_alloc(16);
}

int XFreeGC(Display *dpy, GC gc) { (void)dpy; stub_free(gc); return 0; }

int XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
      unsigned int w, unsigned int h)
{
   (void)dpy; (void)d; (void)gc; (void)x; (void)y; (void)w; (void)h;
   return 0;
}

/* ------------------------------------------------------------------
 * XRandR
 * ------------------------------------------------------------------ */

XRRScreenResources *XRRGetScreenResources(Display *dpy, Window w)
{
   XRRScreenResources *r;
   int i;

   (void)dpy; (void)w;
   s_tag = "screen_resources";

   if (s_cfg.fail_screen_resources)
      return NULL;
   if (!(r = (XRRScreenResources*)stub_alloc(sizeof(*r))))
      return NULL;

   r->noutput = 1;
   if (!(r->outputs = (RROutput*)stub_alloc(sizeof(RROutput))))
      return r;
   r->outputs[0] = 1;

   r->ncrtc = s_cfg.screen_ncrtc;
   if (r->ncrtc > 0)
   {
      if (!(r->crtcs = (RRCrtc*)stub_alloc(sizeof(RRCrtc) * r->ncrtc)))
         return r;
      for (i = 0; i < r->ncrtc; i++)
         r->crtcs[i] = SCREEN_CRTC_BASE + i;
   }

   /* Report the mode XRRCreateMode() was last asked for, so a
    * caller that creates a mode and looks it up again finds it,
    * as it would against a real server. */
   if (s_mode_name[0])
   {
      if ((r->modes = (XRRModeInfo*)stub_alloc(sizeof(XRRModeInfo))))
      {
         r->nmode               = 1;
         r->modes[0].id         = 42;
         r->modes[0].name       = s_mode_name;
         r->modes[0].nameLength = (int)strlen(s_mode_name);
         r->modes[0].width      = 1280;
         r->modes[0].height     = 720;
      }
   }
   return r;
}

XRRScreenResources *XRRGetScreenResourcesCurrent(Display *dpy, Window w)
{
   return XRRGetScreenResources(dpy, w);
}

void XRRFreeScreenResources(XRRScreenResources *r)
{
   if (r)
   {
      if (r->modes)
         stub_free(r->modes);
      if (r->crtcs)
         stub_free(r->crtcs);
      if (r->outputs)
         stub_free(r->outputs);
   }
   stub_free(r);
}

Status XRRQueryVersion(Display *dpy, int *major, int *minor)
{
   (void)dpy;
   *major = 1;
   *minor = 5;
   return 1;
}

Rotation XRRConfigCurrentConfiguration(XRRScreenConfiguration *c,
      Rotation *rotation)
{
   (void)c;
   *rotation = RR_Rotate_0;
   return RR_Rotate_0;
}

Status XRRGetScreenSizeRange(Display *dpy, Window w, int *min_w,
      int *min_h, int *max_w, int *max_h)
{
   (void)dpy; (void)w;
   *min_w = 320;
   *min_h = 200;
   *max_w = 16384;
   *max_h = 16384;
   return 1;
}

XRRScreenConfiguration *XRRGetScreenInfo(Display *dpy, Window w)
{
   (void)dpy; (void)w;
   s_tag = "screen_info";
   if (s_cfg.fail_screen_info)
      return NULL;
   /* Opaque to the caller; only ever passed back to the free. */
   return (XRRScreenConfiguration*)stub_alloc(sizeof(void*));
}

void XRRFreeScreenConfigInfo(XRRScreenConfiguration *c)
{
   stub_free(c);
}

XRROutputInfo *XRRGetOutputInfo(Display *dpy, XRRScreenResources *res,
      RROutput out)
{
   XRROutputInfo *o;
   int i;

   (void)dpy; (void)res; (void)out;
   s_tag = "output_info";

   if (s_cfg.fail_output_info)
      return NULL;
   if (!(o = (XRROutputInfo*)stub_alloc(sizeof(*o))))
      return NULL;

   o->connection = (unsigned short)s_cfg.output_connection;
   o->name       = (char*)"STUB-1";
   o->nameLen    = 6;
   o->crtc       = s_cfg.output_crtc_in_screen ? SCREEN_CRTC_BASE : OUTPUT_CRTC_BASE;
   o->ncrtc      = s_cfg.output_ncrtc;

   if (o->ncrtc > 0)
   {
      if (!(o->crtcs = (RRCrtc*)stub_alloc(sizeof(RRCrtc) * o->ncrtc)))
         return o;
      for (i = 0; i < o->ncrtc; i++)
         o->crtcs[i] = OUTPUT_CRTC_BASE + i;
   }
   return o;
}

void XRRFreeOutputInfo(XRROutputInfo *o)
{
   if (o && o->crtcs)
      stub_free(o->crtcs);
   stub_free(o);
}

XRRCrtcInfo *XRRGetCrtcInfo(Display *dpy, XRRScreenResources *res, RRCrtc crtc)
{
   XRRCrtcInfo *c;

   (void)dpy; (void)res;
   s_tag = "crtc_info";

   if (s_log.get_crtc_info_calls < MAX_LOGGED)
      s_log.queried_crtcs[s_log.get_crtc_info_calls] = crtc;
   s_log.get_crtc_info_calls++;

   if (s_cfg.fail_crtc_info)
      return NULL;
   if (!(c = (XRRCrtcInfo*)stub_alloc(sizeof(*c))))
      return NULL;

   c->width    = 1280;
   c->height   = 720;
   c->rotation = RR_Rotate_0;
   c->mode     = 1;
   c->noutput  = 0;
   return c;
}

void XRRFreeCrtcInfo(XRRCrtcInfo *c) { stub_free(c); }

Status XRRSetCrtcConfig(Display *dpy, XRRScreenResources *res, RRCrtc crtc,
      Time ts, int x, int y, RRMode mode, Rotation rot,
      RROutput *outputs, int noutputs)
{
   (void)dpy; (void)res; (void)ts; (void)x; (void)y; (void)mode; (void)rot;
   (void)outputs; (void)noutputs;

   if (s_log.set_crtc_config_calls < MAX_LOGGED)
      s_log.configured_crtcs[s_log.set_crtc_config_calls] = crtc;
   s_log.set_crtc_config_calls++;
   return 0;
}

void XRRSetScreenSize(Display *dpy, Window w, int width, int height,
      int mmWidth, int mmHeight)
{
   (void)dpy; (void)w; (void)width; (void)height;
   (void)mmWidth; (void)mmHeight;
}

RRMode XRRCreateMode(Display *dpy, Window w, XRRModeInfo *info)
{
   (void)dpy; (void)w;
   if (info && info->name)
   {
      size_t n = sizeof(s_mode_name) - 1;
      strncpy(s_mode_name, info->name, n);
      s_mode_name[n] = '\0';
   }
   s_log.create_mode_calls++;
   return 42;
}

void XRRDestroyMode(Display *dpy, RRMode mode)
{
   (void)dpy; (void)mode;
   s_log.destroy_mode_calls++;
   s_mode_name[0] = '\0';
}

void XRRAddOutputMode(Display *dpy, RROutput out, RRMode mode)
{
   (void)dpy; (void)out; (void)mode;
   s_log.add_output_mode_calls++;
}

void XRRDeleteOutputMode(Display *dpy, RROutput out, RRMode mode)
{
   (void)dpy; (void)out; (void)mode;
   s_log.delete_output_mode_calls++;
}

/* ------------------------------------------------------------------
 * RetroArch-side symbols dispserv_x11.c refers to.  Keeping this
 * list short is what makes the file testable in isolation; if it
 * grows, that is worth noticing.
 * ------------------------------------------------------------------ */

Display *g_x11_dpy = NULL;
Window   g_x11_win = 0;

size_t strlcpy_retro__(char *dest, const char *source, size_t size)
{
   size_t src_size = strlen(source);
   if (size)
   {
      size_t n = src_size < size - 1 ? src_size : size - 1;
      memcpy(dest, source, n);
      dest[n] = '\0';
   }
   return src_size;
}

void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* ------------------------------------------------------------------
 * Cases
 * ------------------------------------------------------------------ */

/* Each failing query, in turn, must yield the default
 * orientation rather than a signal. */
static int test_orientation_query_failures(void)
{
   int i;

   for (i = 0; i < 4; i++)
   {
      stub_cfg_t cfg;
      void *data;
      enum rotation rot;
      const char *what;

      cfg_default(&cfg);
      switch (i)
      {
         case 0: cfg.fail_output_info      = 1; what = "output_info";      break;
         case 1: cfg.fail_crtc_info        = 1; what = "crtc_info";        break;
         case 2: cfg.fail_screen_resources = 1; what = "screen_resources"; break;
         default: cfg.fail_screen_info     = 1; what = "screen_info";      break;
      }
      stub_reset(&cfg);

      data = dispserv_x11.init();
      rot  = dispserv_x11.get_screen_orientation(data);
      dispserv_x11.destroy(data);

      if (rot != ORIENTATION_NORMAL)
      {
         fprintf(stderr, "FAIL: %s failure gave orientation %d, want %d\n",
               what, (int)rot, (int)ORIENTATION_NORMAL);
         return 1;
      }
      if (s_log.bad_free)
      {
         fprintf(stderr, "FAIL: %s failure produced %d bad free(s)\n",
               what, s_log.bad_free);
         return 1;
      }
      if (stub_leaks(what))
         return 1;
   }

   printf("[pass] get_screen_orientation survives every failing query\n");
   return 0;
}

/* A disconnected output is the ordinary shape of "RandR
 * present, no usable output" and must simply yield the default
 * without describing any crtc. */
static int test_orientation_output_disconnected(void)
{
   stub_cfg_t cfg;
   void *data;
   enum rotation rot;

   cfg_default(&cfg);
   cfg.output_connection = RR_Disconnected;
   stub_reset(&cfg);

   data = dispserv_x11.init();
   rot  = dispserv_x11.get_screen_orientation(data);
   dispserv_x11.destroy(data);

   if (rot != ORIENTATION_NORMAL)
   {
      fprintf(stderr, "FAIL: disconnected output gave orientation %d\n",
            (int)rot);
      return 1;
   }
   if (s_log.get_crtc_info_calls != 0)
   {
      fprintf(stderr, "FAIL: described %d crtc(s) of a disconnected output\n",
            s_log.get_crtc_info_calls);
      return 1;
   }
   if (s_log.bad_free || stub_leaks("disconnected output"))
      return 1;

   printf("[pass] a disconnected output describes no crtc\n");
   return 0;
}

/* info->ncrtc bounds info->crtcs, not screen->crtcs.  Give the
 * output four crtcs and the screen one: walking the screen's
 * array reads three past its end, and every id it comes back
 * with is the wrong one. */
static int test_orientation_walks_output_crtcs(void)
{
   stub_cfg_t cfg;
   void *data;
   int i;

   cfg_default(&cfg);
   cfg.screen_ncrtc = 1;
   cfg.output_ncrtc = 4;
   stub_reset(&cfg);

   data = dispserv_x11.init();
   dispserv_x11.get_screen_orientation(data);
   dispserv_x11.destroy(data);

   if (s_log.get_crtc_info_calls != 4)
   {
      fprintf(stderr, "FAIL: described %d crtc(s), want 4\n",
            s_log.get_crtc_info_calls);
      return 1;
   }
   for (i = 0; i < s_log.get_crtc_info_calls; i++)
   {
      if ((int)s_log.queried_crtcs[i] == OUTPUT_CRTC_BASE + i)
         continue;
      fprintf(stderr,
            "FAIL: crtc %d of the walk is id %d, want %d"
            " (the screen's array was indexed with the output's count)\n",
            i, (int)s_log.queried_crtcs[i], OUTPUT_CRTC_BASE + i);
      return 1;
   }
   if (s_log.bad_free || stub_leaks("orientation walk"))
      return 1;

   printf("[pass] the orientation walk indexes the output's crtcs\n");
   return 0;
}

/* Whatever geometry it read off a crtc, it must apply to that
 * same crtc -- not to whichever one sits at the same index in
 * the screen's array. */
static int test_set_orientation_configures_queried_crtcs(void)
{
   stub_cfg_t cfg;
   void *data;
   int i, j;

   cfg_default(&cfg);
   cfg.screen_ncrtc = 1;
   cfg.output_ncrtc = 4;
   stub_reset(&cfg);

   data = dispserv_x11.init();
   dispserv_x11.set_screen_orientation(data, ORIENTATION_VERTICAL);
   dispserv_x11.destroy(data);

   if (s_log.set_crtc_config_calls <= 0)
   {
      fputs("FAIL: set_screen_orientation configured no crtc\n", stderr);
      return 1;
   }

   /* The walk disables a crtc and then reconfigures it, so
    * there are two set calls per crtc described; what matters
    * is that every id configured is one that was described. */
   for (i = 0; i < s_log.set_crtc_config_calls && i < MAX_LOGGED; i++)
   {
      int found = 0;
      for (j = 0; j < s_log.get_crtc_info_calls && j < MAX_LOGGED; j++)
         if (s_log.configured_crtcs[i] == s_log.queried_crtcs[j])
            found = 1;
      if (!found)
      {
         fprintf(stderr,
               "FAIL: configured crtc %d was never described\n",
               (int)s_log.configured_crtcs[i]);
         return 1;
      }
   }
   if (s_log.bad_free || stub_leaks("set_screen_orientation"))
      return 1;

   printf("[pass] set_screen_orientation configures what it described\n");
   return 0;
}

static int test_set_orientation_query_failures(void)
{
   int i;

   for (i = 0; i < 2; i++)
   {
      stub_cfg_t cfg;
      void *data;
      const char *what;

      cfg_default(&cfg);
      if (i == 0)
      {
         cfg.fail_output_info = 1;
         what                 = "output_info";
      }
      else
      {
         cfg.fail_screen_resources = 1;
         what                      = "screen_resources";
      }
      stub_reset(&cfg);

      data = dispserv_x11.init();
      dispserv_x11.set_screen_orientation(data, ORIENTATION_VERTICAL);
      dispserv_x11.destroy(data);

      if (s_log.set_crtc_config_calls != 0)
      {
         fprintf(stderr,
               "FAIL: %s failure still configured %d crtc(s)\n",
               what, s_log.set_crtc_config_calls);
         return 1;
      }
      if (s_log.bad_free)
      {
         fprintf(stderr, "FAIL: %s failure produced %d bad free(s)\n",
               what, s_log.bad_free);
         return 1;
      }
      if (stub_leaks(what))
         return 1;
   }

   printf("[pass] set_screen_orientation survives its failing queries\n");
   return 0;
}

/* The modeline path: open binds the connected output, add creates a
 * RandR mode and attaches it, set reconfigures the crtc, delete
 * detaches and destroys, close puts the desktop back. Each failing
 * query must fail the op cleanly, never signal or leak. */
static int test_modeline_lifecycle(void)
{
   stub_cfg_t cfg;
   void *data;
   video_modeline_disp_t ds;
   video_modeline_t mode;
   video_modeline_t listed[8];
   int n;

   cfg_default(&cfg);
   cfg.output_crtc_in_screen = 1;
   stub_reset(&cfg);
   memset(&ds, 0, sizeof(ds));
   strcpy(ds.screen, "auto");

   memset(&mode, 0, sizeof(mode));
   mode.pclock  = 6700000;
   mode.width   = mode.hactive = 320;
   mode.hbegin  = 336;
   mode.hend    = 367;
   mode.htotal  = 426;
   mode.height  = mode.vactive = 240;
   mode.vbegin  = 244;
   mode.vend    = 247;
   mode.vtotal  = 262;
   mode.vfreq   = 60.0;
   mode.refresh = 60;

   data = dispserv_x11.init();
   if (!dispserv_x11.modeline_open(data, &ds))
   {
      fprintf(stderr, "FAIL: modeline_open failed on a connected output\n");
      return 1;
   }
   if (dispserv_x11.modeline_caps(data) != MODELINE_CAPS_ADD)
   {
      fprintf(stderr, "FAIL: XRandR caps should be ADD only\n");
      return 1;
   }
   n = dispserv_x11.modeline_enum(data, listed, 8);
   if (n < 0)
   {
      fprintf(stderr, "FAIL: modeline_enum failed\n");
      return 1;
   }
   if (!dispserv_x11.modeline_add(data, &mode) || mode.platform_data != 42)
   {
      fprintf(stderr, "FAIL: modeline_add did not create and attach the mode\n");
      return 1;
   }
   if (s_log.create_mode_calls != 1 || s_log.add_output_mode_calls != 1)
   {
      fprintf(stderr, "FAIL: add made %d create / %d attach calls\n",
            s_log.create_mode_calls, s_log.add_output_mode_calls);
      return 1;
   }
   if (!dispserv_x11.modeline_set(data, &mode))
   {
      fprintf(stderr, "FAIL: modeline_set failed on the added mode\n");
      return 1;
   }
   if (s_log.set_crtc_config_calls == 0)
   {
      fprintf(stderr, "FAIL: set configured no crtc\n");
      return 1;
   }
   if (!dispserv_x11.modeline_delete(data, &mode) || mode.platform_data != 0)
   {
      fprintf(stderr, "FAIL: modeline_delete did not remove the mode\n");
      return 1;
   }
   if (s_log.delete_output_mode_calls != 1 || s_log.destroy_mode_calls != 1)
   {
      fprintf(stderr, "FAIL: delete made %d detach / %d destroy calls\n",
            s_log.delete_output_mode_calls, s_log.destroy_mode_calls);
      return 1;
   }
   dispserv_x11.modeline_close(data);
   dispserv_x11.destroy(data);

   if (s_log.bad_free)
   {
      fprintf(stderr, "FAIL: modeline lifecycle produced %d bad free(s)\n",
            s_log.bad_free);
      return 1;
   }
   if (stub_leaks("modeline lifecycle"))
      return 1;

   printf("[pass] modeline open/add/set/delete/close on XRandR\n");
   return 0;
}

static int test_modeline_query_failures(void)
{
   int i;

   for (i = 0; i < 4; i++)
   {
      stub_cfg_t cfg;
      void *data;
      video_modeline_disp_t ds;
      const char *what;
      bool opened;

      cfg_default(&cfg);
      switch (i)
      {
         case 0: cfg.fail_open_display     = 1; what = "open_display";     break;
         case 1: cfg.fail_screen_resources = 1; what = "screen_resources"; break;
         case 2: cfg.fail_output_info      = 1; what = "output_info";      break;
         default: cfg.output_connection = RR_Disconnected; what = "disconnected"; break;
      }
      stub_reset(&cfg);
      memset(&ds, 0, sizeof(ds));
      strcpy(ds.screen, "auto");

      data   = dispserv_x11.init();
      opened = dispserv_x11.modeline_open(data, &ds);
      if (opened)
      {
         fprintf(stderr, "FAIL: %s failure still opened the modeline path\n", what);
         return 1;
      }
      /* Ops after a failed open must refuse rather than dereference */
      if (dispserv_x11.modeline_enum(data, NULL, 0) != -1)
      {
         fprintf(stderr, "FAIL: %s: enum after failed open did not refuse\n", what);
         return 1;
      }
      dispserv_x11.modeline_close(data);
      dispserv_x11.destroy(data);

      if (s_log.bad_free)
      {
         fprintf(stderr, "FAIL: %s failure produced %d bad free(s)\n",
               what, s_log.bad_free);
         return 1;
      }
      if (stub_leaks(what))
         return 1;
   }

   printf("[pass] modeline_open survives its failing queries\n");
   return 0;
}

int main(void)
{
   if (test_orientation_query_failures())
      return 1;
   if (test_orientation_output_disconnected())
      return 1;
   if (test_orientation_walks_output_crtcs())
      return 1;
   if (test_set_orientation_configures_queried_crtcs())
      return 1;
   if (test_set_orientation_query_failures())
      return 1;
   if (test_modeline_lifecycle())
      return 1;
   if (test_modeline_query_failures())
      return 1;

   puts("ALL OK");
   return 0;
}
