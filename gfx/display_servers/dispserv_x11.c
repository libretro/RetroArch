/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* We are targeting XRandR 1.2 here. */
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/Xrender.h>
#endif

#include "../video_display_server.h"
#include "../common/x11_common.h"
#ifdef HAVE_XINERAMA
#include "../common/xinerama_common.h"
#endif
#include "../../retroarch.h"
#include "../../verbosity.h"

enum dispserv_x11_flags
{
   DISPSERV_X11_FLAG_USING_GLOBAL_DPY  = (1 << 0),
   DISPSERV_X11_FLAG_DECORATIONS       = (1 << 2)
};

#ifdef HAVE_XRANDR
/* XRandR modeline application state. One output is managed; its
 * desktop mode and crtc placement are kept so close() can put the
 * desktop back. sp_desktop_crtc mirrors every crtc's position at open
 * time for screen restore and reordering. */
typedef struct
{
   Display *dpy;
   XRRCrtcInfo *desktop_crtc;   /* one per crtc, from open */
   Window root;
   XRRModeInfo desktop_mode;
   XRRCrtcInfo last_crtc;
   int desktop_output;          /* index into resources->outputs, -1 none */
   int screen;
   int crtc_flags;              /* MODELINE_ROTATED when the desktop is */
   int ncrtc;
   unsigned min_width, max_width, min_height, max_height;
   unsigned xerrors;
   unsigned xerrors_flag;
   Rotation desktop_rotation;
   bool enable_screen_reordering;
   bool enable_screen_compositing;
   bool keep_changes;
   bool opened;
} x11_modeline_t;
#endif

typedef struct
{
#ifdef HAVE_XRANDR
   x11_modeline_t ml;
#endif
   unsigned opacity;
   uint8_t flags;
} dispserv_x11_t;

#ifdef HAVE_XRANDR
static void x11_display_server_modeline_close(void *data);
#endif

#ifdef HAVE_XRANDR
static Display* x11_display_server_open_display(dispserv_x11_t *dispserv)
{
   Display *dpy        = g_x11_dpy;
   if (!dispserv)
      return NULL;
   if (dpy)
   {
      dispserv->flags |= DISPSERV_X11_FLAG_USING_GLOBAL_DPY;
      return dpy;
   }
   /* SDL might use X11 but doesn't use g_x11_dpy, so open it manually */
   return XOpenDisplay(0);
}

static void x11_display_server_close_display(dispserv_x11_t *dispserv,
      Display *dpy)
{
   if (     !dpy
         || !dispserv
         || (dispserv->flags & DISPSERV_X11_FLAG_USING_GLOBAL_DPY)
         || dpy == g_x11_dpy)
      return;

   XCloseDisplay(dpy);
}

static void x11_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   int i, j;
   XRRScreenResources *screen     = NULL;
   /* switched to using XOpenDisplay() due to deinit order issue with g_x11_dpy when restoring original rotation on exit */
   Display                   *dpy = XOpenDisplay(0);
   XRRScreenConfiguration *config = XRRGetScreenInfo(dpy, DefaultRootWindow(dpy));
   double dpi = (25.4 * DisplayHeight(dpy, DefaultScreen(dpy))) / DisplayHeightMM(dpy, DefaultScreen(dpy));

   XGrabServer(dpy);

   screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

   if (!screen)
   {
      XUngrabServer(dpy);
      if (config)
         XRRFreeScreenConfigInfo(config);
      XCloseDisplay(dpy);
      return;
   }

   for (i = 0; i < screen->noutput; i++)
   {
      XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

      /* See x11_display_server_get_screen_orientation(). */
      if (!info)
         continue;

      if (info->connection != RR_Connected)
      {
         XRRFreeOutputInfo(info);
         continue;
      }

      for (j = 0; j < info->ncrtc; j++)
      {
         XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtcs[j]);
         Rotation new_rotation = RR_Rotate_0;

         if (!crtc)
            continue;

         if (crtc->width == 0 || crtc->height == 0)
         {
            XRRFreeCrtcInfo(crtc);
            continue;
         }

         switch (rotation)
         {
            case ORIENTATION_NORMAL:
            default:
               if (crtc->rotations & RR_Rotate_0)
                  new_rotation = RR_Rotate_0;
               break;
            case ORIENTATION_VERTICAL:
               if (crtc->rotations & RR_Rotate_270)
                  new_rotation = RR_Rotate_270;
               break;
            case ORIENTATION_FLIPPED:
               if (crtc->rotations & RR_Rotate_180)
                  new_rotation = RR_Rotate_180;
               break;
            case ORIENTATION_FLIPPED_ROTATED:
               if (crtc->rotations & RR_Rotate_90)
                  new_rotation = RR_Rotate_90;
               break;
         }

         XRRSetCrtcConfig(dpy, screen, info->crtcs[j], CurrentTime,
               0, 0, None, RR_Rotate_0, NULL, 0);

         if ((crtc->rotation & RR_Rotate_0 || crtc->rotation & RR_Rotate_180) && (rotation == ORIENTATION_VERTICAL || rotation == ORIENTATION_FLIPPED_ROTATED))
         {
            unsigned width = crtc->width;
            crtc->width = crtc->height;
            crtc->height = width;
         }
         else if ((crtc->rotation & RR_Rotate_90 || crtc->rotation & RR_Rotate_270) && (rotation == ORIENTATION_NORMAL || rotation == ORIENTATION_FLIPPED))
         {
            unsigned width = crtc->width;
            crtc->width    = crtc->height;
            crtc->height   = width;
         }

         crtc->rotation = new_rotation;

         XRRSetScreenSize(dpy, DefaultRootWindow(dpy), crtc->width, crtc->height, (25.4 * crtc->width) / dpi, (25.4 * crtc->height) / dpi);

         XRRSetCrtcConfig(dpy, screen, info->crtcs[j], CurrentTime, crtc->x, crtc->y, crtc->mode, crtc->rotation, crtc->outputs, crtc->noutput);

         XRRFreeCrtcInfo(crtc);
      }

      XRRFreeOutputInfo(info);
   }

   XRRFreeScreenResources(screen);

   XUngrabServer(dpy);
   XSync(dpy, False);
   if (config)
      XRRFreeScreenConfigInfo(config);
   XCloseDisplay(dpy);
}

static enum rotation x11_display_server_get_screen_orientation(void *data)
{
   int i, j;
   XRRScreenConfiguration *config = NULL;
   enum rotation     rotation     = ORIENTATION_NORMAL;
   dispserv_x11_t *dispserv       = (dispserv_x11_t*)data;
   Display               *dpy     = x11_display_server_open_display(dispserv);
   XRRScreenResources *screen     = NULL;

   /* x11_display_server_open_display() returns NULL when there is no
    * global display and XOpenDisplay() fails; DefaultRootWindow()
    * dereferences its argument. */
   if (!dpy)
      return ORIENTATION_NORMAL;

   if (!(screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy))))
   {
      x11_display_server_close_display(dispserv, dpy);
      return ORIENTATION_NORMAL;
   }

   config                         = XRRGetScreenInfo(dpy, DefaultRootWindow(dpy));

   for (i = 0; i < screen->noutput; i++)
   {
      XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

      /* XRRGetOutputInfo() returns NULL for an output the server
       * cannot describe, which is the normal case under Xvfb and any
       * other server with RandR present but no configured output. */
      if (!info)
         continue;

      if (info->connection != RR_Connected)
      {
         XRRFreeOutputInfo(info);
         continue;
      }

      /* The crtcs to walk are this output's, not the screen's: ncrtc
       * bounds info->crtcs, and there is no guarantee the screen has
       * that many. */
      for (j = 0; j < info->ncrtc; j++)
      {
         XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtcs[j]);

         if (!crtc)
            continue;

         if (crtc->width == 0 || crtc->height == 0)
         {
            XRRFreeCrtcInfo(crtc);
            continue;
         }

         switch (crtc->rotation)
         {
            case RR_Rotate_0:
            default:
               rotation = ORIENTATION_NORMAL;
               break;
            case RR_Rotate_270:
               rotation = ORIENTATION_VERTICAL;
               break;
            case RR_Rotate_180:
               rotation = ORIENTATION_FLIPPED;
               break;
            case RR_Rotate_90:
               rotation = ORIENTATION_FLIPPED_ROTATED;
               break;
         }

         XRRFreeCrtcInfo(crtc);
      }

      XRRFreeOutputInfo(info);
   }

   XRRFreeScreenResources(screen);
   /* XRRGetScreenInfo() can fail; the free is not NULL-tolerant. */
   if (config)
      XRRFreeScreenConfigInfo(config);

   x11_display_server_close_display(dispserv, dpy);

   return rotation;
}
#endif

#ifdef HAVE_XRANDR
/* Set-timing flags */
#define XRANDR_DISABLE_CRTC_RELOCATION  0x00000001
#define XRANDR_ENABLE_SCREEN_REORDERING 0x00000002

/* Per-crtc work flags, carried in XRRCrtcInfo.timestamp while a set
 * is in progress */
#define XRANDR_SETMODE_IS_DESKTOP          0x00000001
#define XRANDR_SETMODE_RESTORE_DESKTOP     0x00000002
#define XRANDR_SETMODE_UPDATE_DESKTOP_CRTC 0x00000010
#define XRANDR_SETMODE_UPDATE_OTHER_CRTC   0x00000020
#define XRANDR_SETMODE_UPDATE_REORDERING   0x00000040
#define XRANDR_SETMODE_INFO_MASK           0x0000000F
#define XRANDR_SETMODE_UPDATE_MASK         0x000000F0

/* Super resolution placement, vertical stacking, reserved height */
#define XRANDR_REORDERING_MAXIMUM_HEIGHT 1024

/* Xlib's error handler is process-global; the backend is a single
 * instance, so the errors land here. */
static x11_modeline_t *x11_ml_current = NULL;
static int (*x11_ml_old_error_handler)(Display *, XErrorEvent *) = NULL;

static int x11_ml_error_handler(Display *dpy, XErrorEvent *err)
{
   if (x11_ml_current)
      x11_ml_current->xerrors |= x11_ml_current->xerrors_flag;
   if (x11_ml_old_error_handler)
      x11_ml_old_error_handler(dpy, err);
   RARCH_ERR("[XRandR] Error code %d flags %02x\n", err->error_code,
         x11_ml_current ? x11_ml_current->xerrors : 0);
   return 0;
}

static void x11_ml_trap(x11_modeline_t *ml, unsigned flag)
{
   XSync(ml->dpy, False);
   ml->xerrors_flag         = flag;
   x11_ml_current           = ml;
   x11_ml_old_error_handler = XSetErrorHandler(x11_ml_error_handler);
}

static void x11_ml_untrap(x11_modeline_t *ml)
{
   XSync(ml->dpy, False);
   XSetErrorHandler(x11_ml_old_error_handler);
   x11_ml_current = NULL;
}

static bool x11_ml_find_mode(x11_modeline_t *ml, uint64_t xid,
      XRRModeInfo *out)
{
   int m;
   bool found = false;
   XRRScreenResources *resources = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return false;
   for (m = 0; m < resources->nmode; m++)
   {
      if (xid == resources->modes[m].id)
      {
         *out  = resources->modes[m];
         found = true;
         break;
      }
   }
   XRRFreeScreenResources(resources);
   return found;
}

static bool x11_ml_find_mode_by_name(x11_modeline_t *ml, const char *name,
      XRRModeInfo *out)
{
   int m;
   bool found = false;
   XRRScreenResources *resources = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return false;
   for (m = 0; m < resources->nmode; m++)
   {
      if (strcmp(resources->modes[m].name, name) == 0)
      {
         *out  = resources->modes[m];
         found = true;
         break;
      }
   }
   XRRFreeScreenResources(resources);
   return found;
}

/* Switch the managed output to pxmode, relocating the other crtcs
 * and resizing the framebuffer as needed. flags selects crtc
 * relocation and the one-time desktop reordering pass. */
static bool x11_ml_set_timing(x11_modeline_t *ml,
      const video_modeline_t *mode, int flags)
{
   int c;
   XRRModeInfo xmode;
   XRRModeInfo *pxmode;
   XRRScreenResources *resources;
   XRROutputInfo *output_info;
   XRRCrtcInfo *crtc_info;
   XRRCrtcInfo *global_crtc;
   XRRCrtcInfo *original_crtc;
   unsigned width, height, active_crtc, reordering_last_y;
   bool ok;

   if (ml->desktop_output == -1)
   {
      RARCH_ERR("[XRandR] No screen detected\n");
      return false;
   }

   if (mode->type & MODELINE_DESKTOP)
      pxmode = &ml->desktop_mode;
   else
   {
      if (!x11_ml_find_mode(ml, mode->platform_data, &xmode))
      {
         RARCH_ERR("[XRandR] Mode not found\n");
         return false;
      }
      pxmode = &xmode;
   }

   resources   = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return false;
   output_info = XRRGetOutputInfo(ml->dpy, resources,
         resources->outputs[ml->desktop_output]);
   if (!output_info)
   {
      XRRFreeScreenResources(resources);
      return false;
   }
   crtc_info   = XRRGetCrtcInfo(ml->dpy, resources, output_info->crtc);
   if (!crtc_info)
   {
      XRRFreeOutputInfo(output_info);
      XRRFreeScreenResources(resources);
      return false;
   }

   if (flags & XRANDR_DISABLE_CRTC_RELOCATION)
      RARCH_DBG("[XRandR] Crtc relocation disabled\n");

   if (flags & XRANDR_ENABLE_SCREEN_REORDERING)
      RARCH_DBG("[XRandR] Global desktop screen preparation\n");
   else if (ml->last_crtc.mode == crtc_info->mode
         && ml->last_crtc.x == crtc_info->x && ml->last_crtc.y == crtc_info->y
         && pxmode->id == crtc_info->mode)
      RARCH_DBG("[XRandR] Requested mode is already active [%04lx] %ux%u+%d+%d\n",
            crtc_info->mode, crtc_info->width, crtc_info->height,
            crtc_info->x, crtc_info->y);
   else if (ml->last_crtc.mode != crtc_info->mode)
   {
      RARCH_DBG("[XRandR] Unexpected active modeline (last:[%04lx] now:[%04lx] %ux%u+%d+%d want:[%04lx])\n",
            ml->last_crtc.mode, crtc_info->mode, crtc_info->width,
            crtc_info->height, crtc_info->x, crtc_info->y, pxmode->id);
      *crtc_info = ml->last_crtc;
   }

   global_crtc   = (XRRCrtcInfo*)calloc(resources->ncrtc, sizeof(XRRCrtcInfo));
   original_crtc = (XRRCrtcInfo*)calloc(resources->ncrtc, sizeof(XRRCrtcInfo));
   if (!global_crtc || !original_crtc)
   {
      free(global_crtc);
      free(original_crtc);
      XRRFreeCrtcInfo(crtc_info);
      XRRFreeOutputInfo(output_info);
      XRRFreeScreenResources(resources);
      return false;
   }

   /* Keep the window manager out while crtcs are shuffled */
   XGrabServer(ml->dpy);

   width             = ml->min_width;
   height            = ml->min_height;
   active_crtc       = 0;
   reordering_last_y = 0;
   ml->xerrors       = 0;

   /* Compute the new placement of every crtc */
   for (c = 0; c < resources->ncrtc; c++)
   {
      XRRCrtcInfo *info = XRRGetCrtcInfo(ml->dpy, resources, resources->crtcs[c]);
      XRRCrtcInfo *crtc_info0 = &original_crtc[c];
      XRRCrtcInfo *crtc_info1 = &global_crtc[c];
      if (!info)
         continue;
      *crtc_info0 = *info;
      *crtc_info1 = *info;
      XRRFreeCrtcInfo(info);

      crtc_info1->timestamp = 0;

      if (output_info->crtc == 0 || crtc_info0->mode == 0)
         continue;

      if (flags & XRANDR_ENABLE_SCREEN_REORDERING)
      {
         /* Stack every crtc vertically */
         crtc_info1->x = 0;
         crtc_info1->y = reordering_last_y;
         if (crtc_info1->height > XRANDR_REORDERING_MAXIMUM_HEIGHT)
            reordering_last_y += crtc_info1->height;
         else
            reordering_last_y += XRANDR_REORDERING_MAXIMUM_HEIGHT;
         crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_REORDERING;
         active_crtc++;
      }
      else if (resources->crtcs[c] == output_info->crtc)
      {
         crtc_info1->timestamp |= XRANDR_SETMODE_IS_DESKTOP;
         crtc_info1->mode   = pxmode->id;
         crtc_info1->width  = pxmode->width;
         crtc_info1->height = pxmode->height;
         if (mode->type & MODELINE_DESKTOP)
         {
            if (!ml->enable_screen_compositing && ml->desktop_crtc
                  && (crtc_info1->x != ml->desktop_crtc[c].x
                     || crtc_info1->y != ml->desktop_crtc[c].y))
            {
               crtc_info1->x = ml->desktop_crtc[c].x;
               crtc_info1->y = ml->desktop_crtc[c].y;
               crtc_info1->timestamp |= XRANDR_SETMODE_RESTORE_DESKTOP;
            }
         }
         else
         {
            crtc_info1->x = crtc_info->x;
            crtc_info1->y = crtc_info->y;
         }
         if (crtc_info0->mode != crtc_info1->mode
               || crtc_info0->width != crtc_info1->width
               || crtc_info0->height != crtc_info1->height
               || crtc_info0->x != crtc_info1->x
               || crtc_info0->y != crtc_info1->y)
            crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_DESKTOP_CRTC;
      }
      else if ((mode->type & MODELINE_DESKTOP) && ml->enable_screen_reordering
            && ml->desktop_crtc
            && (crtc_info1->x != ml->desktop_crtc[c].x
               || crtc_info1->y != ml->desktop_crtc[c].y))
      {
         crtc_info1->x = ml->desktop_crtc[c].x;
         crtc_info1->y = ml->desktop_crtc[c].y;
         crtc_info1->timestamp |= (XRANDR_SETMODE_RESTORE_DESKTOP
               | XRANDR_SETMODE_UPDATE_REORDERING);
      }
   }

   for (c = 0; c < resources->ncrtc; c++)
   {
      XRRCrtcInfo *crtc_info0 = &original_crtc[c];
      XRRCrtcInfo *crtc_info1 = &global_crtc[c];

      if (output_info->crtc == 0 || crtc_info0->mode == 0)
         continue;

      if ((flags & XRANDR_DISABLE_CRTC_RELOCATION) == 0
            && (crtc_info1->timestamp & XRANDR_SETMODE_IS_DESKTOP) == 0)
      {
         /* Neighbours move with the new width and height */
         if (crtc_info1->x >= crtc_info->x + (int)crtc_info->width)
         {
            crtc_info1->x += pxmode->width - crtc_info->width;
            crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_OTHER_CRTC;
         }
         if (crtc_info1->y >= crtc_info->y + (int)crtc_info->height)
         {
            crtc_info1->y += pxmode->height - crtc_info->height;
            crtc_info1->timestamp |= XRANDR_SETMODE_UPDATE_OTHER_CRTC;
         }
      }

      /* Framebuffer size from the crtc placement */
      if (crtc_info1->x + crtc_info1->width > width)
         width = crtc_info1->x + crtc_info1->width;
      if (crtc_info1->y + crtc_info1->height > height)
         height = crtc_info1->y + crtc_info1->height;
      if (width > ml->max_width)
      {
         RARCH_ERR("[XRandR] Width is above allowed maximum (%u > %u)\n",
               width, ml->max_width);
         width = ml->max_width;
      }
      if (height > ml->max_height)
      {
         RARCH_ERR("[XRandR] Height is above allowed maximum (%u > %u)\n",
               height, ml->max_height);
         height = ml->max_height;
      }

      if (crtc_info1->timestamp & XRANDR_SETMODE_UPDATE_MASK)
         RARCH_DBG("[XRandR] crtc %d%s [%04lx] %ux%u+%d+%d --> [%04lx] %ux%u+%d+%d flags [%02lx]\n",
               c, (crtc_info1->timestamp & 1) ? "*" : " ",
               crtc_info0->mode, crtc_info0->width, crtc_info0->height,
               crtc_info0->x, crtc_info0->y,
               crtc_info1->mode, crtc_info1->width, crtc_info1->height,
               crtc_info1->x, crtc_info1->y, crtc_info1->timestamp);
   }

   /* Disable every crtc that changes */
   for (c = 0; c < resources->ncrtc; c++)
   {
      if (global_crtc[c].timestamp & XRANDR_SETMODE_UPDATE_MASK)
      {
         if (XRRSetCrtcConfig(ml->dpy, resources, resources->crtcs[c],
                  CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0)
               != RRSetConfigSuccess)
         {
            RARCH_ERR("[XRandR] Error disabling crtc %d\n", c);
            ml->xerrors_flag = 0x01;
            ml->xerrors     |= ml->xerrors_flag;
         }
      }
   }

   /* Framebuffer size for the new placement */
   if (ml->xerrors == 0)
   {
      RARCH_DBG("[XRandR] Setting screen size to %u x %u\n", width, height);
      x11_ml_trap(ml, 0x02);
      XRRSetScreenSize(ml->dpy, ml->root, width, height,
            (int)((25.4 * width) / 96.0), (int)((25.4 * height) / 96.0));
      x11_ml_untrap(ml);
      if (ml->xerrors & ml->xerrors_flag)
         RARCH_ERR("[XRandR] Error in XRRSetScreenSize\n");
   }

   /* Re-enable with the new mode and placement */
   for (c = 0; c < resources->ncrtc; c++)
   {
      XRRCrtcInfo *crtc_info1 = &global_crtc[c];
      if (crtc_info1->timestamp & XRANDR_SETMODE_UPDATE_MASK)
      {
         if (crtc_info1->timestamp & XRANDR_SETMODE_IS_DESKTOP)
         {
            GC gc = XCreateGC(ml->dpy, ml->root, 0, 0);
            XFillRectangle(ml->dpy, ml->root, gc, crtc_info1->x, crtc_info1->y,
                  crtc_info1->width, crtc_info1->height);
            XFreeGC(ml->dpy, gc);
         }
         x11_ml_trap(ml, 0x14);
         XRRSetCrtcConfig(ml->dpy, resources, resources->crtcs[c], CurrentTime,
               crtc_info1->x, crtc_info1->y, crtc_info1->mode,
               crtc_info1->rotation, crtc_info1->outputs, crtc_info1->noutput);
         x11_ml_untrap(ml);
         if (ml->xerrors & 0x10)
         {
            RARCH_ERR("[XRandR] Error in XRRSetCrtcConfig crtc %d set modeline %04lx\n",
                  c, crtc_info1->mode);
            ml->xerrors &= 0xEF;
         }
      }
   }

   free(original_crtc);
   free(global_crtc);

   XUngrabServer(ml->dpy);

   if (ml->xerrors & ml->xerrors_flag)
      RARCH_ERR("[XRandR] Error in XRRSetCrtcConfig\n");

   /* Read the managed crtc back to settle */
   XRRFreeCrtcInfo(crtc_info);
   crtc_info = XRRGetCrtcInfo(ml->dpy, resources, output_info->crtc);
   ok        = crtc_info && crtc_info->mode != 0;
   if (!ok)
      RARCH_ERR("[XRandR] Switching resolution failed, no modeline is set\n");
   else
      ml->last_crtc = *crtc_info;

   if (crtc_info)
      XRRFreeCrtcInfo(crtc_info);
   XRRFreeOutputInfo(output_info);
   XRRFreeScreenResources(resources);

   return (ml->xerrors == 0 && ok);
}

static int x11_display_server_modeline_list_outputs(void *data,
      video_output_info_t *out, int max)
{
   int o;
   int n = 0;
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   Display *dpy             = x11_display_server_open_display(dispserv);
   XRRScreenResources *resources;
   Window root;

   if (!dpy)
      return -1;
   root      = RootWindow(dpy, DefaultScreen(dpy));
   resources = XRRGetScreenResourcesCurrent(dpy, root);
   if (!resources)
   {
      x11_display_server_close_display(dispserv, dpy);
      return -1;
   }

   for (o = 0; o < resources->noutput && n < max; o++)
   {
      XRRCrtcInfo *crtc;
      XRROutputInfo *info = XRRGetOutputInfo(dpy, resources, resources->outputs[o]);
      if (!info)
         continue;
      if (info->connection == RR_Connected && info->crtc)
      {
         memset(&out[n], 0, sizeof(out[n]));
         out[n].id = o;
         strlcpy(out[n].name, info->name, sizeof(out[n].name));
         crtc = XRRGetCrtcInfo(dpy, resources, info->crtc);
         if (crtc)
         {
            out[n].x      = crtc->x;
            out[n].y      = crtc->y;
            out[n].width  = crtc->width;
            out[n].height = crtc->height;
            XRRFreeCrtcInfo(crtc);
         }
         out[n].primary = (n == 0);
         n++;
      }
      XRRFreeOutputInfo(info);
   }
   XRRFreeScreenResources(resources);
   x11_display_server_close_display(dispserv, dpy);
   return n;
}

/* The head a request is for, as a root-relative point: the centre of
 * the RetroArch window when the screen is "auto", the centre of the
 * Xinerama screen the window was placed on when the screen is an
 * index (the context driver maps video_monitor_index to Xinerama
 * screen index-1, so the same mapping here lands the timing on the
 * head the window sits on). false when neither is known, in which
 * case the caller falls back to the output count. */
static bool x11_ml_target_point(Display *dpy, int screen_pos, int *px, int *py)
{
#ifdef HAVE_XINERAMA
   if (screen_pos >= 0)
   {
      int x, y;
      unsigned w, h;
      if (xinerama_get_coord(dpy, screen_pos, &x, &y, &w, &h))
      {
         *px = x + (int)w / 2;
         *py = y + (int)h / 2;
         return true;
      }
   }
#endif
   if (screen_pos < 0 && g_x11_win)
   {
      XWindowAttributes attr;
      Window child;
      int rx = 0, ry = 0;
      if (XGetWindowAttributes(dpy, g_x11_win, &attr)
            && XTranslateCoordinates(dpy, g_x11_win, attr.root, 0, 0, &rx, &ry, &child))
      {
         *px = rx + attr.width / 2;
         *py = ry + attr.height / 2;
         return true;
      }
   }
   return false;
}

static bool x11_display_server_modeline_open(void *data,
      const video_modeline_disp_t *ds)
{
   int screen, major_version, minor_version;
   int screen_pos = -1;
   int target_x = 0, target_y = 0;
   bool have_target;
   bool detected  = false;
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;

   if (ml->opened)
      return true;

   memset(ml, 0, sizeof(*ml));
   ml->desktop_output            = -1;
   ml->enable_screen_reordering  = ds->screen_reordering;
   ml->enable_screen_compositing = !ds->screen_reordering && ds->screen_compositing;
   ml->keep_changes              = ds->keep_changes;

   ml->dpy = x11_display_server_open_display(dispserv);
   if (!ml->dpy)
   {
      RARCH_ERR("[XRandR] Failed to connect to the X server\n");
      return false;
   }

   XRRQueryVersion(ml->dpy, &major_version, &minor_version);
   RARCH_DBG("[XRandR] Version %d.%d\n", major_version, minor_version);
   if (major_version < 1 || (major_version == 1 && minor_version < 2))
   {
      RARCH_ERR("[XRandR] Xrandr version 1.2 or above is required\n");
      x11_display_server_close_display(dispserv, ml->dpy);
      ml->dpy = NULL;
      return false;
   }

   /* Screen selection: "auto", "screenN", "N" or an output name */
   if (strlen(ds->screen) == 7 && !strncmp(ds->screen, "screen", 6)
         && ds->screen[6] >= '0' && ds->screen[6] <= '9')
      screen_pos = ds->screen[6] - '0';
   else if (strlen(ds->screen) == 1 && ds->screen[0] >= '0' && ds->screen[0] <= '9')
      screen_pos = ds->screen[0] - '0';

   if (ScreenCount(ml->dpy) > 1)
      RARCH_WARN("[XRandR] Screen count is %d, unpredictable behavior to be expected\n",
            ScreenCount(ml->dpy));

   have_target = x11_ml_target_point(ml->dpy, screen_pos, &target_x, &target_y);

   for (screen = 0; !detected && screen < ScreenCount(ml->dpy); screen++)
   {
      int o, c;
      int output_position       = 0;
      Rotation current_rotation = 0;
      XRRScreenConfiguration *sc;
      XRRScreenResources *resources;

      RARCH_DBG("[XRandR] Check screen number %d\n", screen);
      ml->screen = screen;
      ml->root   = RootWindow(ml->dpy, screen);
      resources  = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
      if (!resources)
         continue;

      /* Every crtc's placement, for restore and reordering */
      free(ml->desktop_crtc);
      ml->desktop_crtc = (XRRCrtcInfo*)calloc(resources->ncrtc, sizeof(XRRCrtcInfo));
      ml->ncrtc        = resources->ncrtc;
      if (ml->desktop_crtc)
      {
         for (c = 0; c < resources->ncrtc; c++)
         {
            XRRCrtcInfo *info = XRRGetCrtcInfo(ml->dpy, resources, resources->crtcs[c]);
            if (!info)
               continue;
            ml->desktop_crtc[c] = *info;
            XRRFreeCrtcInfo(info);
         }
      }

      sc = XRRGetScreenInfo(ml->dpy, ml->root);
      if (sc)
      {
         XRRConfigCurrentConfiguration(sc, &ml->desktop_rotation);
         XRRFreeScreenConfigInfo(sc);
      }

      for (o = 0; o < resources->noutput; o++)
      {
         XRROutputInfo *output_info = XRRGetOutputInfo(ml->dpy, resources, resources->outputs[o]);
         if (!output_info)
         {
            RARCH_ERR("[XRandR] Could not get output 0x%x information\n",
                  (unsigned)resources->outputs[o]);
            continue;
         }

         if (ml->desktop_output == -1 && output_info->connection == RR_Connected
               && output_info->crtc)
         {
            bool take = false;
            if (!strcmp(ds->screen, output_info->name))
               take = true;
            else if (have_target)
            {
               /* The head under the target point */
               XRRCrtcInfo *ci = XRRGetCrtcInfo(ml->dpy, resources, output_info->crtc);
               if (ci)
               {
                  take = target_x >= ci->x && target_x < ci->x + (int)ci->width
                     && target_y >= ci->y && target_y < ci->y + (int)ci->height;
                  XRRFreeCrtcInfo(ci);
               }
            }
            else if (!strcmp(ds->screen, "auto") || output_position == screen_pos)
               take = true;

            if (take)
            {
               int m;
               int min_width, max_width, min_height, max_height;
               XRRCrtcInfo *crtc_info;

               ml->desktop_output = o;

               XRRGetScreenSizeRange(ml->dpy, ml->root, &min_width, &min_height,
                     &max_width, &max_height);
               ml->min_width  = min_width;
               ml->max_width  = max_width;
               ml->min_height = min_height;
               ml->max_height = max_height;

               crtc_info = XRRGetCrtcInfo(ml->dpy, resources, output_info->crtc);
               if (crtc_info)
               {
                  current_rotation = crtc_info->rotation;
                  for (m = 0; m < resources->nmode && ml->desktop_mode.id == 0; m++)
                  {
                     if (crtc_info->mode == resources->modes[m].id)
                     {
                        ml->desktop_mode = resources->modes[m];
                        ml->last_crtc    = *crtc_info;
                     }
                  }
                  XRRFreeCrtcInfo(crtc_info);
               }

               if (current_rotation & 0xe)
               {
                  ml->crtc_flags = MODELINE_ROTATED;
                  RARCH_DBG("[XRandR] Desktop rotation is %s\n",
                        (current_rotation & 0x2) ? "left"
                        : ((current_rotation & 0x8) ? "right" : "inverted"));
               }
            }
            output_position++;
         }
         RARCH_DBG("[XRandR] Check output connector '%s' active %d crtc %d %s\n",
               output_info->name, output_info->connection == RR_Connected ? 1 : 0,
               output_info->crtc ? 1 : 0, ml->desktop_output == o
               ? (have_target ? "[SELECTED: under the window]" : "[SELECTED]") : "");
         XRRFreeOutputInfo(output_info);
      }
      XRRFreeScreenResources(resources);

      detected = ml->desktop_output != -1;
      if (!detected && have_target)
      {
         /* Nothing under the point: the count rule on the same screen */
         have_target = false;
         screen--;
      }
   }

   if (!detected)
   {
      RARCH_ERR("[XRandR] No screen detected\n");
      x11_display_server_close_display(dispserv, ml->dpy);
      ml->dpy = NULL;
      free(ml->desktop_crtc);
      ml->desktop_crtc = NULL;
      return false;
   }

   if (ml->enable_screen_reordering)
   {
      video_modeline_t mode;
      memset(&mode, 0, sizeof(mode));
      mode.type = MODELINE_DESKTOP;
      x11_ml_set_timing(ml, &mode, XRANDR_ENABLE_SCREEN_REORDERING);
   }

   ml->opened = true;
   return true;
}

static void x11_display_server_modeline_close(void *data)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;
   video_modeline_t mode;

   if (!ml->opened)
      return;

   /* Desktop timing back, unless the user asked to keep changes */
   if (!ml->keep_changes && ml->desktop_output != -1)
   {
      memset(&mode, 0, sizeof(mode));
      mode.type = MODELINE_DESKTOP;
      x11_ml_set_timing(ml, &mode, ml->enable_screen_compositing
            ? 0 : XRANDR_DISABLE_CRTC_RELOCATION);
   }

   /* Default background back */
   XClearWindow(ml->dpy, ml->root);
   XSync(ml->dpy, False);

   free(ml->desktop_crtc);
   ml->desktop_crtc = NULL;
   x11_display_server_close_display(dispserv, ml->dpy);
   ml->dpy    = NULL;
   ml->opened = false;
}

static unsigned x11_display_server_modeline_caps(void *data)
{
   return MODELINE_CAPS_ADD;
}

static int x11_display_server_modeline_enum(void *data,
      video_modeline_t *modes, int max)
{
   int i, m;
   int n = 0;
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;
   XRRScreenResources *resources;
   XRROutputInfo *output_info;

   if (!ml->opened || ml->desktop_output == -1)
      return -1;

   resources = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return -1;
   output_info = XRRGetOutputInfo(ml->dpy, resources, resources->outputs[ml->desktop_output]);
   if (!output_info)
   {
      XRRFreeScreenResources(resources);
      return -1;
   }

   for (i = 0; i < output_info->nmode && n < max; i++)
   {
      for (m = 0; m < resources->nmode; m++)
      {
         XRRModeInfo *pxmode    = &resources->modes[m];
         video_modeline_t *mode = &modes[n];
         if (pxmode->id != output_info->modes[i])
            continue;

         memset(mode, 0, sizeof(*mode));
         mode->platform_data = pxmode->id;
         mode->pclock     = pxmode->dotClock;
         mode->hactive    = pxmode->width;
         mode->hbegin     = pxmode->hSyncStart;
         mode->hend       = pxmode->hSyncEnd;
         mode->htotal     = pxmode->hTotal;
         mode->vactive    = pxmode->height;
         mode->vbegin     = pxmode->vSyncStart;
         mode->vend       = pxmode->vSyncEnd;
         mode->vtotal     = pxmode->vTotal;
         mode->interlace  = (pxmode->modeFlags & RR_Interlace) ? 1 : 0;
         mode->doublescan = (pxmode->modeFlags & RR_DoubleScan) ? 1 : 0;
         mode->hsync      = (pxmode->modeFlags & RR_HSyncPositive) ? 1 : 0;
         mode->vsync      = (pxmode->modeFlags & RR_VSyncPositive) ? 1 : 0;
         /* Whole hertz for the line rate, the label the list uses */
         mode->hfreq      = (double)(mode->pclock / (uint64_t)mode->htotal);
         mode->vfreq      = mode->hfreq / mode->vtotal * (mode->interlace ? 2 : 1);
         mode->refresh    = (int)mode->vfreq;
         mode->width      = pxmode->width;
         mode->height     = pxmode->height;
         mode->type      |= ml->crtc_flags;
         mode->type      |= MODELINE_TIMING_XRANDR;
         if (strncmp(pxmode->name, "SR-", 3) == 0 || strncmp(pxmode->name, "RA-", 3) == 0)
            RARCH_DBG("[XRandR] Leftover generated modeline %s detected\n", pxmode->name);
         if (ml->desktop_mode.id == pxmode->id)
            mode->type |= MODELINE_DESKTOP;
         RARCH_DBG("[XRandR] Mode %04lx %dx%d refresh %.6f listed\n",
               pxmode->id, pxmode->width, pxmode->height, mode->vfreq);
         n++;
         break;
      }
   }

   XRRFreeOutputInfo(output_info);
   XRRFreeScreenResources(resources);
   return n;
}

static bool x11_display_server_modeline_delete(void *data,
      video_modeline_t *mode)
{
   int m;
   int total_xerrors = 0;
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;
   XRRScreenResources *resources;

   if (!ml->opened || ml->desktop_output == -1)
   {
      RARCH_ERR("[XRandR] No screen detected\n");
      return false;
   }
   if (!mode)
      return false;

   resources = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return false;

   for (m = 0; m < resources->nmode && mode->platform_data != 0; m++)
   {
      XRROutputInfo *output_info;
      XRRCrtcInfo *crtc_info;

      if (mode->platform_data != resources->modes[m].id)
         continue;

      output_info = XRRGetOutputInfo(ml->dpy, resources, resources->outputs[ml->desktop_output]);
      crtc_info   = output_info ? XRRGetCrtcInfo(ml->dpy, resources, output_info->crtc) : NULL;
      if (crtc_info && resources->modes[m].id == crtc_info->mode)
      {
         video_modeline_t desktop_mode;
         RARCH_DBG("[XRandR] Modeline [%04lx] is active, restoring desktop mode first\n",
               resources->modes[m].id);
         memset(&desktop_mode, 0, sizeof(desktop_mode));
         desktop_mode.type |= MODELINE_DESKTOP;
         if (!x11_ml_set_timing(ml, &desktop_mode, 0))
         {
            RARCH_ERR("[XRandR] Could not restore desktop mode\n");
            XRRFreeCrtcInfo(crtc_info);
            XRRFreeOutputInfo(output_info);
            XRRFreeScreenResources(resources);
            return false;
         }
      }
      if (crtc_info)
         XRRFreeCrtcInfo(crtc_info);
      if (output_info)
         XRRFreeOutputInfo(output_info);

      RARCH_DBG("[XRandR] Remove mode %s\n", resources->modes[m].name);
      ml->xerrors = 0;
      x11_ml_trap(ml, 0x01);
      XRRDeleteOutputMode(ml->dpy, resources->outputs[ml->desktop_output],
            resources->modes[m].id);
      XSync(ml->dpy, False);
      if (ml->xerrors & ml->xerrors_flag)
      {
         RARCH_ERR("[XRandR] Error in XRRDeleteOutputMode\n");
         total_xerrors++;
      }
      ml->xerrors_flag = 0x02;
      XRRDestroyMode(ml->dpy, resources->modes[m].id);
      x11_ml_untrap(ml);
      if (ml->xerrors & ml->xerrors_flag)
      {
         RARCH_ERR("[XRandR] Error in XRRDestroyMode\n");
         total_xerrors++;
      }
      mode->platform_data = 0;
   }

   XRRFreeScreenResources(resources);
   return total_xerrors == 0;
}

static bool x11_display_server_modeline_add(void *data,
      video_modeline_t *mode)
{
   char name[48];
   XRRModeInfo xmode;
   XRRModeInfo found;
   RRMode gmid;
   XRRScreenResources *resources;
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;

   if (!mode)
      return false;
   if (!ml->opened || ml->desktop_output == -1)
   {
      RARCH_ERR("[XRandR] No screen detected\n");
      return false;
   }

   if (x11_ml_find_mode(ml, mode->platform_data, &found))
   {
      RARCH_DBG("[XRandR] Mode already exists\n");
      return true;
   }

   snprintf(name, sizeof(name), "RA-%dx%d@%.02f%s", mode->hactive,
         mode->vactive, mode->vfreq, mode->interlace ? "i" : "");

   if (x11_ml_find_mode_by_name(ml, name, &found))
   {
      RARCH_DBG("[XRandR] Mode already exists (duplicate request)\n");
      mode->platform_data = found.id;
      return true;
   }

   RARCH_DBG("[XRandR] Create mode %s\n", name);

   memset(&xmode, 0, sizeof(xmode));
   xmode.name       = name;
   xmode.nameLength = strlen(name);
   xmode.dotClock   = mode->pclock;
   xmode.width      = mode->hactive;
   xmode.hSyncStart = mode->hbegin;
   xmode.hSyncEnd   = mode->hend;
   xmode.hTotal     = mode->htotal;
   xmode.height     = mode->vactive;
   xmode.vSyncStart = mode->vbegin;
   xmode.vSyncEnd   = mode->vend;
   xmode.vTotal     = mode->vtotal;
   xmode.modeFlags  = (mode->interlace ? RR_Interlace : 0)
      | (mode->doublescan ? RR_DoubleScan : 0)
      | (mode->hsync ? RR_HSyncPositive : RR_HSyncNegative)
      | (mode->vsync ? RR_VSyncPositive : RR_VSyncNegative);
   xmode.hSkew      = 0;
   mode->type      |= MODELINE_TIMING_XRANDR;

   ml->xerrors = 0;
   x11_ml_trap(ml, 0x01);
   gmid = XRRCreateMode(ml->dpy, ml->root, &xmode);
   x11_ml_untrap(ml);
   if (ml->xerrors & ml->xerrors_flag)
   {
      RARCH_ERR("[XRandR] Error in XRRCreateMode\n");
      return false;
   }
   mode->platform_data = gmid;

   resources = XRRGetScreenResourcesCurrent(ml->dpy, ml->root);
   if (!resources)
      return false;
   x11_ml_trap(ml, 0x02);
   XRRAddOutputMode(ml->dpy, resources->outputs[ml->desktop_output],
         (RRMode)mode->platform_data);
   x11_ml_untrap(ml);
   XRRFreeScreenResources(resources);

   if (ml->xerrors & ml->xerrors_flag)
   {
      RARCH_ERR("[XRandR] Error in XRRAddOutputMode\n");
      if (mode->platform_data)
      {
         RARCH_ERR("[XRandR] Remove mode [%04llx]\n",
               (unsigned long long)mode->platform_data);
         XRRDestroyMode(ml->dpy, (RRMode)mode->platform_data);
         mode->platform_data = 0;
      }
   }
   else
      RARCH_DBG("[XRandR] Mode %04llx %dx%d refresh %.6f added\n",
            (unsigned long long)mode->platform_data, mode->hactive,
            mode->vactive, mode->vfreq);

   return ml->xerrors == 0;
}

static bool x11_display_server_modeline_update(void *data,
      video_modeline_t *mode)
{
   if (!mode)
      return false;
   if (!x11_display_server_modeline_delete(data, mode))
   {
      RARCH_ERR("[XRandR] Delete operation not successful\n");
      return false;
   }
   if (!x11_display_server_modeline_add(data, mode))
   {
      RARCH_ERR("[XRandR] Add operation not successful\n");
      return false;
   }
   return true;
}

static bool x11_display_server_modeline_set(void *data,
      video_modeline_t *mode)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   x11_modeline_t *ml       = &dispserv->ml;
   if (!ml->opened || !mode)
      return false;
   return x11_ml_set_timing(ml, mode, ml->enable_screen_compositing
         ? 0 : XRANDR_DISABLE_CRTC_RELOCATION);
}

static bool x11_display_server_modeline_flush(void *data)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   if (dispserv->ml.dpy)
      XSync(dispserv->ml.dpy, False);
   return true;
}
#endif /* HAVE_XRANDR */

static void* x11_display_server_init(void)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)calloc(1, sizeof(*dispserv));

   if (dispserv)
      return dispserv;
   return NULL;
}

static void x11_display_server_destroy(void *data)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;

   if (!dispserv)
      return;
#ifdef HAVE_XRANDR
   x11_display_server_modeline_close(dispserv);
#endif
   free(dispserv);
}

static bool x11_display_server_set_window_opacity(void *data, unsigned opacity)
{
   dispserv_x11_t *serv = (dispserv_x11_t*)data;
   Atom net_wm_opacity  = XInternAtom(g_x11_dpy, "_NET_WM_WINDOW_OPACITY", False);
   Atom cardinal        = XInternAtom(g_x11_dpy, "CARDINAL", False);

   serv->opacity        = opacity;

   opacity              = opacity * ((unsigned)-1 / 100.0);

   if (opacity == (unsigned)-1)
      XDeleteProperty(g_x11_dpy, g_x11_win, net_wm_opacity);
   else
      XChangeProperty(g_x11_dpy, g_x11_win, net_wm_opacity, cardinal,
            32, PropModeReplace, (const unsigned char*)&opacity, 1);

   return true;
}

static bool x11_display_server_set_window_decorations(void *data, bool on)
{
   dispserv_x11_t *serv = (dispserv_x11_t*)data;
   if (serv)
      serv->flags |= DISPSERV_X11_FLAG_DECORATIONS;
   /* menu_setting performs a reinit instead to properly apply
    * decoration changes */
   return true;
}


const char *x11_display_server_get_output_options(void *data)
{
#ifdef HAVE_XRANDR
   int i;
   Display *dpy;
   XRRScreenResources *res;
   XRROutputInfo *info;
   Window root;
   static char s[PATH_MAX_LENGTH];

   if (!(dpy = XOpenDisplay(0)))
      return NULL;

   root = RootWindow(dpy, DefaultScreen(dpy));

   if (!(res = XRRGetScreenResources(dpy, root)))
      return NULL;

   /* Build "out1|out2|out3" via offset tracking; the prior
    * strlcat-in-loop form re-scanned `s` from the start on every
    * append, giving O(outputs^2) total cost.  Also resets `s` at
    * the start of the loop — without that, subsequent calls would
    * append to leftover content in the static buffer. */
   {
      size_t buf_len = 0;
      size_t avail   = sizeof(s);
      s[0] = '\0';
      for (i = 0; i < res->noutput && buf_len + 1 < avail; i++)
      {
         size_t nlen;
         if (!(info = XRRGetOutputInfo(dpy, res, res->outputs[i])))
            return NULL;
         nlen = strlen(info->name);
         if (i > 0)
            s[buf_len++] = '|';
         if (nlen >= avail - buf_len)
            nlen = avail - buf_len - 1;
         memcpy(s + buf_len, info->name, nlen);
         buf_len += nlen;
      }
      s[buf_len] = '\0';
   }

   return s;
#else
   /* TODO/FIXME - hardcoded for now; list should be built up dynamically later */
   return "HDMI-0|HDMI-1|HDMI-2|HDMI-3|DVI-0|DVI-1|DVI-2|DVI-3|VGA-0|VGA-1|VGA-2|VGA-3|Config";
#endif
}

static uint32_t x11_display_server_get_flags(void *data)
{
   uint32_t             flags   = 0;

#ifdef HAVE_XRANDR
   BIT32_SET(flags, DISPSERV_CTX_MODELINE);
#endif

   return flags;
}

#ifdef HAVE_XRANDR
static float x11_display_server_get_refresh_rate(void *data)
{
   float refresh_rate             = 0.0f;
   dispserv_x11_t *dispserv       = (dispserv_x11_t*)data;
   Display *dpy                   = x11_display_server_open_display(dispserv);
   XRRScreenResources *screen     = NULL;

   if (!dpy)
      return 0.0f;

   screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

   if (screen)
   {
      int i;
      for (i = 0; i < screen->noutput; i++)
      {
         XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

         if (info->connection == RR_Connected && info->crtc)
         {
            int j;
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtc);

            if (crtc && crtc->mode)
            {
               for (j = 0; j < screen->nmode; j++)
               {
                  if (screen->modes[j].id == crtc->mode)
                  {
                     XRRModeInfo *mode = &screen->modes[j];
                     if (mode->hTotal && mode->vTotal)
                        refresh_rate = (float)mode->dotClock
                           / ((float)mode->hTotal * (float)mode->vTotal);
                     break;
                  }
               }
            }

            if (crtc)
               XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(info);
            break;
         }

         XRRFreeOutputInfo(info);
      }

      XRRFreeScreenResources(screen);
   }

   x11_display_server_close_display(dispserv, dpy);
   return refresh_rate;
}

static void x11_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   dispserv_x11_t *dispserv       = (dispserv_x11_t*)data;
   Display *dpy                   = x11_display_server_open_display(dispserv);
   XRRScreenResources *screen     = NULL;

   if (!dpy)
      return;

   screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

   if (screen)
   {
      int i;
      for (i = 0; i < screen->noutput; i++)
      {
         XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

         if (info->connection == RR_Connected && info->crtc)
         {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtc);

            if (crtc)
            {
               if (width)
                  *width  = crtc->width;
               if (height)
                  *height = crtc->height;
               XRRFreeCrtcInfo(crtc);
            }

            XRRFreeOutputInfo(info);
            break;
         }

         XRRFreeOutputInfo(info);
      }

      XRRFreeScreenResources(screen);
   }

   x11_display_server_close_display(dispserv, dpy);
}

static void x11_display_server_get_video_output_prev(void *data)
{
   dispserv_x11_t *dispserv       = (dispserv_x11_t*)data;
   Display *dpy                   = x11_display_server_open_display(dispserv);
   XRRScreenResources *screen     = NULL;

   if (!dpy)
      return;

   screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

   if (screen)
   {
      int i;
      for (i = 0; i < screen->noutput; i++)
      {
         XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

         if (info->connection == RR_Connected && info->crtc)
         {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtc);

            if (crtc && crtc->mode)
            {
               int j;
               int cur_idx = -1;

               /* Find the current mode index in the output's mode list */
               for (j = 0; j < info->nmode; j++)
               {
                  if (info->modes[j] == crtc->mode)
                  {
                     cur_idx = j;
                     break;
                  }
               }

               /* Select previous mode */
               if (cur_idx > 0)
               {
                  int k;
                  RRMode prev_mode = info->modes[cur_idx - 1];
                  for (k = 0; k < screen->nmode; k++)
                  {
                     if (screen->modes[k].id == prev_mode)
                     {
                        XRRSetCrtcConfig(dpy, screen, info->crtc,
                              CurrentTime, crtc->x, crtc->y,
                              prev_mode, crtc->rotation,
                              crtc->outputs, crtc->noutput);
                        break;
                     }
                  }
               }
            }

            if (crtc)
               XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(info);
            break;
         }

         XRRFreeOutputInfo(info);
      }

      XRRFreeScreenResources(screen);
   }

   x11_display_server_close_display(dispserv, dpy);
}

static void x11_display_server_get_video_output_next(void *data)
{
   dispserv_x11_t *dispserv       = (dispserv_x11_t*)data;
   Display *dpy                   = x11_display_server_open_display(dispserv);
   XRRScreenResources *screen     = NULL;

   if (!dpy)
      return;

   screen = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));

   if (screen)
   {
      int i;
      for (i = 0; i < screen->noutput; i++)
      {
         XRROutputInfo *info = XRRGetOutputInfo(dpy, screen, screen->outputs[i]);

         if (info->connection == RR_Connected && info->crtc)
         {
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, screen, info->crtc);

            if (crtc && crtc->mode)
            {
               int j;
               int cur_idx = -1;

               /* Find the current mode index in the output's mode list */
               for (j = 0; j < info->nmode; j++)
               {
                  if (info->modes[j] == crtc->mode)
                  {
                     cur_idx = j;
                     break;
                  }
               }

               /* Select next mode */
               if (cur_idx >= 0 && cur_idx + 1 < info->nmode)
               {
                  int k;
                  RRMode next_mode = info->modes[cur_idx + 1];
                  for (k = 0; k < screen->nmode; k++)
                  {
                     if (screen->modes[k].id == next_mode)
                     {
                        XRRSetCrtcConfig(dpy, screen, info->crtc,
                              CurrentTime, crtc->x, crtc->y,
                              next_mode, crtc->rotation,
                              crtc->outputs, crtc->noutput);
                        break;
                     }
                  }
               }
            }

            if (crtc)
               XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(info);
            break;
         }

         XRRFreeOutputInfo(info);
      }

      XRRFreeScreenResources(screen);
   }

   x11_display_server_close_display(dispserv, dpy);
}
#endif

#ifndef HAVE_XRANDR
static void x11_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   Display *dpy = XOpenDisplay(NULL);
   if (!dpy)
      return;
   if (width)
      *width  = DisplayWidth(dpy, DefaultScreen(dpy));
   if (height)
      *height = DisplayHeight(dpy, DefaultScreen(dpy));
   XCloseDisplay(dpy);
}
#endif

static bool x11_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   unsigned screen_no      = 0;
   Display *dpy            = NULL;

   switch (type)
   {
      case DISPLAY_METRIC_PIXEL_WIDTH:
         dpy    = (Display*)XOpenDisplay(NULL);
         *value = (float)DisplayWidth(dpy, screen_no);
         XCloseDisplay(dpy);
         break;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         dpy    = (Display*)XOpenDisplay(NULL);
         *value = (float)DisplayHeight(dpy, screen_no);
         XCloseDisplay(dpy);
         break;
      case DISPLAY_METRIC_MM_WIDTH:
         dpy    = (Display*)XOpenDisplay(NULL);
         *value = (float)DisplayWidthMM(dpy, screen_no);
         XCloseDisplay(dpy);
         break;
      case DISPLAY_METRIC_MM_HEIGHT:
         dpy    = (Display*)XOpenDisplay(NULL);
         *value = (float)DisplayHeightMM(dpy, screen_no);
         XCloseDisplay(dpy);
         break;
      case DISPLAY_METRIC_DPI:
         dpy    = (Display*)XOpenDisplay(NULL);
         *value = ((((float)DisplayWidth  (dpy, screen_no)) * 25.4)
               /  (  (float)DisplayWidthMM(dpy, screen_no)));
         XCloseDisplay(dpy);
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;
}

const video_display_server_t dispserv_x11 = {
   x11_display_server_init,
   x11_display_server_destroy,
   x11_display_server_set_window_opacity,
   NULL, /* set_window_progress */
   x11_display_server_set_window_decorations,
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   x11_display_server_get_output_options,
#ifdef HAVE_XRANDR
   x11_display_server_set_screen_orientation,
   x11_display_server_get_screen_orientation,
#else
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
#endif
#ifdef HAVE_XRANDR
   x11_display_server_get_refresh_rate,
   x11_display_server_get_video_output_size,
#else
   NULL, /* get_refresh_rate — no standard Xlib API */
   x11_display_server_get_video_output_size,
#endif
#ifdef HAVE_XRANDR
   x11_display_server_get_video_output_prev,
   x11_display_server_get_video_output_next,
#else
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
#endif
   x11_get_metrics,
   x11_display_server_get_flags,
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
#ifdef HAVE_XRANDR
   x11_display_server_modeline_list_outputs,
   x11_display_server_modeline_open,
   x11_display_server_modeline_close,
   x11_display_server_modeline_caps,
   x11_display_server_modeline_enum,
   x11_display_server_modeline_add,
   x11_display_server_modeline_update,
   x11_display_server_modeline_delete,
   x11_display_server_modeline_set,
   x11_display_server_modeline_flush,
#else
   NULL, /* modeline_list_outputs */
   NULL, /* modeline_open */
   NULL, /* modeline_close */
   NULL, /* modeline_caps */
   NULL, /* modeline_enum */
   NULL, /* modeline_add */
   NULL, /* modeline_update */
   NULL, /* modeline_delete */
   NULL, /* modeline_set */
   NULL, /* modeline_flush */
#endif
   "x11"
};
