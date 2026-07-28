/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (xrandr_stub.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlibint.h>

#include "xrandr_stub.h"

#define STUB_MAX_ALLOCS 64

static xrandr_stub_cfg_t s_cfg;
static xrandr_stub_log_t s_log;

/* Every pointer handed to the code under test, so a double free or a
 * free of something never allocated is caught rather than left to
 * ASan to find in whichever test happens to run last. */
static void *s_allocs[STUB_MAX_ALLOCS];
static int   s_alloc_freed[STUB_MAX_ALLOCS];
static int   s_alloc_count;

/* A Display that DefaultRootWindow(), DefaultScreen(), DisplayWidth()
 * and friends can be expanded against.  Those are macros reaching
 * straight into the struct, so there is no intercepting them; the
 * struct has to be real enough to index. */
static Display *s_dpy;
static Screen   s_screen;

/* Name of the mode most recently passed to XRRCreateMode(). */
static char s_mode_name[64];

static const char *s_alloc_tag[STUB_MAX_ALLOCS];
static const char *s_tag = "?";

static void *stub_alloc(size_t len)
{
   void *p;
   if (s_alloc_count >= STUB_MAX_ALLOCS)
      return NULL;
   if (!(p = calloc(1, len)))
      return NULL;
   s_allocs[s_alloc_count]      = p;
   s_alloc_tag[s_alloc_count]   = s_tag;
   s_alloc_freed[s_alloc_count] = 0;
   s_alloc_count++;
   return p;
}

static void stub_free(void *p)
{
   int i;
   if (!p)
   {
      /* Freeing NULL is what the fixed code must not do, because the
       * real XRRFree* entry points do not tolerate it. */
      s_log.bad_free++;
      return;
   }
   /* Search from the newest entry: the allocator reuses addresses, so
    * the same pointer value can appear more than once in the table and
    * only the most recent one is live. */
   for (i = s_alloc_count - 1; i >= 0; i--)
   {
      if (s_allocs[i] != p || s_alloc_freed[i])
         continue;
      s_alloc_freed[i] = 1;
      free(p);
      return;
   }
   /* Either never handed out, or handed out and already freed. */
   s_log.bad_free++;
}

void xrandr_stub_reset(const xrandr_stub_cfg_t *cfg)
{
   int i;
   for (i = 0; i < s_alloc_count; i++)
      if (!s_alloc_freed[i])
         free(s_allocs[i]);
   s_alloc_count = 0;

   memset(&s_log, 0, sizeof(s_log));
   s_mode_name[0] = '\0';
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

const xrandr_stub_log_t *xrandr_stub_log(void) { return &s_log; }

int xrandr_stub_all_freed(void)
{
   int i, leaked = 0;
   for (i = 0; i < s_alloc_count; i++)
      if (!s_alloc_freed[i])
      {
         fprintf(stderr, "  stub: leaked allocation #%d (%s)\n",
               i, s_alloc_tag[i]);
         leaked++;
      }
   return leaked == 0;
}

/* --- Xlib --- */

Display *XOpenDisplay(const char *name)
{
   (void)name;
   if (s_cfg.fail_open_display)
      return NULL;
   return s_dpy;
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

/* --- XRandR --- */

XRRScreenResources *XRRGetScreenResources(Display *dpy, Window w)
{
   s_tag = "screen_resources";
   XRRScreenResources *r;
   int i;

   (void)dpy; (void)w;

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
         r->crtcs[i] = XRANDR_STUB_SCREEN_CRTC_BASE + i;
   }

   /* Report the mode XRRCreateMode() was last asked for, so callers
    * that create a mode and then look it up again find it, as they
    * would against a real server. */
   if (s_mode_name[0])
   {
      if ((r->modes = (XRRModeInfo*)stub_alloc(sizeof(XRRModeInfo))))
      {
         r->nmode           = 1;
         r->modes[0].id     = 42;
         r->modes[0].name   = s_mode_name;
         r->modes[0].nameLength = (int)strlen(s_mode_name);
         r->modes[0].width  = 1280;
         r->modes[0].height = 720;
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
   s_log.free_screen_resources_calls++;
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

XRRScreenConfiguration *XRRGetScreenInfo(Display *dpy, Window w)
{
   s_tag = "screen_info";
   (void)dpy; (void)w;
   if (s_cfg.fail_screen_info)
      return NULL;
   /* Opaque to the caller; only ever passed back to the free. */
   return (XRRScreenConfiguration*)stub_alloc(sizeof(void*));
}

void XRRFreeScreenConfigInfo(XRRScreenConfiguration *c)
{
   s_log.free_screen_config_calls++;
   stub_free(c);
}

XRROutputInfo *XRRGetOutputInfo(Display *dpy, XRRScreenResources *res,
      RROutput out)
{
   s_tag = "output_info";
   XRROutputInfo *o;
   int i;

   (void)dpy; (void)res; (void)out;

   if (s_cfg.fail_output_info)
      return NULL;

   if (!(o = (XRROutputInfo*)stub_alloc(sizeof(*o))))
      return NULL;

   o->connection = (unsigned short)s_cfg.output_connection;
   o->name       = (char*)"STUB-1";
   o->nameLen    = 6;
   o->crtc       = XRANDR_STUB_OUTPUT_CRTC_BASE;
   o->ncrtc      = s_cfg.output_ncrtc;

   if (o->ncrtc > 0)
   {
      if (!(o->crtcs = (RRCrtc*)stub_alloc(sizeof(RRCrtc) * o->ncrtc)))
         return o;
      for (i = 0; i < o->ncrtc; i++)
         o->crtcs[i] = XRANDR_STUB_OUTPUT_CRTC_BASE + i;
   }

   o->nmode = 0;
   return o;
}

void XRRFreeOutputInfo(XRROutputInfo *o)
{
   s_log.free_output_info_calls++;
   if (o && o->crtcs)
      stub_free(o->crtcs);
   stub_free(o);
}

XRRCrtcInfo *XRRGetCrtcInfo(Display *dpy, XRRScreenResources *res, RRCrtc crtc)
{
   s_tag = "crtc_info";
   XRRCrtcInfo *c;

   (void)dpy; (void)res;

   if (s_log.get_crtc_info_calls
         < (int)(sizeof(s_log.queried_crtcs) / sizeof(s_log.queried_crtcs[0])))
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

void XRRFreeCrtcInfo(XRRCrtcInfo *c)
{
   s_log.free_crtc_info_calls++;
   stub_free(c);
}

Status XRRSetCrtcConfig(Display *dpy, XRRScreenResources *res, RRCrtc crtc,
      Time ts, int x, int y, RRMode mode, Rotation rot,
      RROutput *outputs, int noutputs)
{
   (void)dpy; (void)res; (void)ts; (void)x; (void)y; (void)mode; (void)rot;
   (void)outputs; (void)noutputs;

   if (s_log.set_crtc_config_calls
         < (int)(sizeof(s_log.configured_crtcs) / sizeof(s_log.configured_crtcs[0])))
      s_log.configured_crtcs[s_log.set_crtc_config_calls] = crtc;
   s_log.set_crtc_config_calls++;
   return 0;
}

void XRRSetScreenSize(Display *dpy, Window w, int width, int height,
      int mmWidth, int mmHeight)
{
   (void)dpy; (void)w; (void)width; (void)height; (void)mmWidth; (void)mmHeight;
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
   return 42;
}

void XRRDestroyMode(Display *dpy, RRMode mode)   { (void)dpy; (void)mode; }

void XRRAddOutputMode(Display *dpy, RROutput out, RRMode mode)
{
   (void)dpy; (void)out; (void)mode;
}

void XRRDeleteOutputMode(Display *dpy, RROutput out, RRMode mode)
{
   (void)dpy; (void)out; (void)mode;
}
