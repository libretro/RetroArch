/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include <sys/types.h>
#include <unistd.h>

#include "../video_display_server.h"
#include "../common/x11_common.h"
#include "../../configuration.h"
#include "../video_driver.h" /* needed to set refresh rate in set resolution */
#include "../video_crt_switch.h" /* needed to set aspect for low res in linux */

static unsigned orig_width      = 0;
static unsigned orig_height     = 0;
static char old_mode[250]       = {0};
static char new_mode[250]       = {0};
static char xrandr[250]         = {0};
static char fbset[150]          = {0};
static char output[250]         = {0};
static bool crt_en              = false;

typedef struct
{
   unsigned opacity;
   bool decorations;
} dispserv_x11_t;

static void* x11_display_server_init(void)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)calloc(1, sizeof(*dispserv));

   if (!dispserv)
      return NULL;

   return dispserv;
}

static void x11_display_server_destroy(void *data)
{
   dispserv_x11_t *dispserv = (dispserv_x11_t*)data;
   int i          = 0;

   if (crt_en)
   {
      sprintf(output, "xrandr -s %dx%d", orig_width, orig_height);
      system(output);

      for (i = 0; i < 3; i++)
      {
         sprintf(output, "xrandr --delmode %s%d %s", "VGA", i, old_mode);
         system(output);
         sprintf(output, "xrandr --delmode %s-%d %s", "VGA", i, old_mode);
         system(output);

         sprintf(output, "xrandr --delmode %s%d %s", "DVI", i, old_mode);
         system(output);
         sprintf(output, "xrandr --delmode %s-%d %s", "DVI", i, old_mode);
         system(output);
      }

      sprintf(output, "xrandr --rmmode %s", old_mode);
      system(output);
   }

   if (dispserv)
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
      XChangeProperty(g_x11_dpy, g_x11_win, net_wm_opacity, cardinal, 32, PropModeReplace, (const unsigned char*)&opacity, 1);

   return true;
}

static bool x11_display_server_set_window_decorations(void *data, bool on)
{
   dispserv_x11_t *serv = (dispserv_x11_t*)data;

   serv->decorations = on;

   /* menu_setting performs a reinit instead to properly apply decoration changes */

   return true;
}

static bool x11_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz, int center)
{
   int i              = 0;
   int hfp            = 0;
   int hsp            = 0;
   int hbp            = 0;
   int vfp            = 0;
   int vsp            = 0;
   int vbp            = 0;
   int hmax           = 0;
   int vmax           = 0;
   int pdefault       = 8;
   int pwidth         = 0;
   float roundw     = 0.0f;
   float roundh     = 0.0f;
   float pixel_clock  = 0;

   crt_en = true;

   /* set core refresh from hz */
   video_monitor_set_refresh_rate(hz);

   /* following code is the mode line genorator */

   hsp = width * 1.140;
   hfp = width * 1.055;

   pwidth = width;

   if (height < 400 && width > 400)
      pwidth = width / 2;

   roundw = roundf((float)pwidth / (float)height * 100) / 100;

   if (height > width)
   {
      roundw = roundf((float)height / (float)width * 100) / 100;
   }

   if (roundw > 1.35)
      roundw = 1.25;

   if (roundw < 1.20)
      roundw = 1.34;

   hbp = width * roundw - 8;
   hmax = hbp;

   if (height < 241)
   {
      vmax = 261;
   }
   if (height < 241 && hz > 56 && hz < 58)
   {
      vmax = 280;
   }
   if (height < 241 && hz < 55)
   {
      vmax = 313;
   }
   if (height > 250 && height < 260 && hz > 54)
   {
      vmax = 296;
   }
   if (height > 250 && height < 260 && hz > 52 && hz < 54)
   {
      vmax = 285;
   }
   if (height > 250 && height < 260 && hz < 52)
   {
      vmax = 313;
   }
   if (height > 260 && height < 300)
   {
      vmax = 318;
   }

   if (height > 400 && hz > 56)
   {
      vmax = 533;
   }
   if (height > 520 && hz < 57)
   {
      vmax = 580;
   }

   if (height > 300 && hz < 56)
   {
      vmax = 615;
   }
   if (height > 500 && hz < 56)
   {
      vmax = 624;
   }
   if (height > 300)
   {
      pdefault = pdefault * 2;
   }

   vfp = height + ((vmax - height) / 2) - pdefault;

   if (height < 300)
   {
      vsp = vfp + 3; /* needs to me 3 for progressive */
   }
   if (height > 300)
   {
      vsp = vfp + 6; /* needs to me 6 for interlaced */
   }

   vbp = vmax;

   if (height < 300)
   {
      pixel_clock = (hmax * vmax * hz) / 1000000;
   }

   if (height > 300)
   {
      pixel_clock = ((hmax * vmax * hz) / 1000000) / 2;
   }
   /* above code is the modeline genorator */

   /* create interlaced newmode from modline variables */
   if (height < 300)
   {
      snprintf(xrandr, sizeof(xrandr), "xrandr --newmode \"%dx%d_%0.2f\" %lf %d %d %d %d %d %d %d %d -hsync -vsync", width, height, hz, pixel_clock, width, hfp, hsp, hbp, height, vfp, vsp, vbp);
      system(xrandr);
   }
   /* create interlaced newmode from modline variables */
   if (height > 300)
   {
      snprintf(xrandr, sizeof(xrandr), "xrandr --newmode \"%dx%d_%0.2f\" %lf %d %d %d %d %d %d %d %d interlace -hsync -vsync", width, height, hz, pixel_clock, width, hfp, hsp, hbp, height, vfp, vsp, vbp);
      system(xrandr);
   }

   /* variable for new mode */
   snprintf(new_mode, sizeof(new_mode), "%dx%d_%0.2f", width, height, hz);

   /* need to run loops for DVI0 - DVI-2 and VGA0 - VGA-2 outputs to add and delete modes */
   for (i = 0; i < 3; i++)
   {
      snprintf(output, sizeof(output), "xrandr --addmode %s%d %s", "DVI", i, new_mode);
      system(output);
      snprintf(output, sizeof(output), "xrandr --delmode %s%d %s", "DVI", i, old_mode);
      system(output);
   }
   for (i = 0; i < 3; i++)
   {
      snprintf(output, sizeof(output), "xrandr --addmode %s-%d %s", "DVI", i, new_mode);
      system(output);
      snprintf(output, sizeof(output), "xrandr --delmode %s-%d %s", "DVI", i, old_mode);
      system(output);
   }
   for (i = 0; i < 3; i++)
   {
      snprintf(output, sizeof(output), "xrandr --addmode %s%d %s", "VGA", i, new_mode);
      system(output);
      snprintf(output, sizeof(output), "xrandr --delmode %s%d %s", "VGA", i, old_mode);
      system(output);
   }
   for (i = 0; i < 3; i++)
   {
      snprintf(output, sizeof(output), "xrandr --addmode %s-%d %s", "VGA", i, new_mode);
      system(output);
      snprintf(output, sizeof(output), "xrandr --delmode %s-%d %s", "VGA", i, old_mode);
      system(output);
   }

   snprintf(output, sizeof(output), "xrandr -s %s", new_mode);
   system(output);

   /* remove old mode */
   snprintf(output, sizeof(output), "xrandr --rmmode %s", old_mode);
   system(output);

   system("xdotool windowactivate $(xdotool search --class RetroArch)");   /* needs xdotool installed. needed to recapture window. */

   /* variable for old mode */
   snprintf(old_mode, sizeof(old_mode), "%s", new_mode);

   system("xdotool windowactivate $(xdotool search --class RetroArch)");      /* needs xdotool installed. needed to recapture window. */
   /* Second run needed as some times it runs to fast to capture first time */

   return true;
}

const char *x11_display_server_get_output_options(void)
{
   /* TODO/FIXME - hardcoded for now; list should be built up dynamically later */
   return "HDMI-0|HDMI-1|HDMI-2|HDMI-3|DVI-0|DVI-1|DVI-2|DVI-3|VGA-0|VGA-1|VGA-2|VGA-3|Config";
}

const video_display_server_t dispserv_x11 = {
   x11_display_server_init,
   x11_display_server_destroy,
   x11_display_server_set_window_opacity,
   NULL,
   x11_display_server_set_window_decorations,
   x11_display_server_set_resolution,
   x11_display_server_get_output_options,
   "x11"
};
