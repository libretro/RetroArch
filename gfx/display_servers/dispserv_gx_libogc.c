/*  RetroArch - A frontend for libretro.
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

/* Display server for the GameCube and Wii. The VI has a fixed set of
 * framebuffer sizes RetroArch offers (dispserv_gx_modes.c) and a TV
 * standard the console is set to; this server lists the sizes the
 * standard can show, with the refresh rate and scan type each one
 * really runs at, and switches between them. The VI itself is still
 * programmed by the gx video driver (gx_set_video_mode), which owns
 * the framebuffers a switch has to clear and the menu surface it has
 * to resize; set_resolution records the choice in
 * current_resolution_id, which is what the driver comes back up in,
 * and hands the mode to the driver.
 *
 * Only whole modes can be switched: there is no separate refresh
 * rate to change, so a set_resolution without dims (the refresh rate
 * autoswitch) fails. */

#include <stdlib.h>

#include <gccore.h>
#if defined(HW_RVL)
#include <ogc/conf.h>
#endif

#include "dispserv_gx.h"
#include "../video_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/* Nothing to keep; any non-NULL pointer marks the server as up */
static char gx_display_server_state;

void gx_display_server_query(gx_vi_standard_t *std, unsigned *tvmode)
{
   GXRModeObj pref;
   unsigned mode;
#if defined(HW_RVL)
   std->progressive = CONF_GetProgressiveScan() > 0
      && VIDEO_HaveComponentCable();

   switch (CONF_GetVideo())
   {
      case CONF_VIDEO_PAL:
         mode = (CONF_GetEuRGB60() > 0) ? VI_EURGB60 : VI_PAL;
         break;
      case CONF_VIDEO_MPAL:
         mode = VI_MPAL;
         break;
      default:
         mode = VI_NTSC;
         break;
   }
#else
   std->progressive = VIDEO_HaveComponentCable() ? true : false;
   mode             = VIDEO_GetCurrentTvMode();
#endif

   switch (mode)
   {
      case VI_PAL:
         std->max_width  = VI_MAX_WIDTH_PAL;
         std->max_height = VI_MAX_HEIGHT_PAL;
         break;
      case VI_MPAL:
         std->max_width  = VI_MAX_WIDTH_MPAL;
         std->max_height = VI_MAX_HEIGHT_MPAL;
         break;
      case VI_EURGB60:
         std->max_width  = VI_MAX_WIDTH_EURGB60;
         std->max_height = VI_MAX_HEIGHT_EURGB60;
         break;
      default:
         mode            = VI_NTSC;
         std->max_width  = VI_MAX_WIDTH_NTSC;
         std->max_height = VI_MAX_HEIGHT_NTSC;
         break;
   }

   std->fifty_hz = (mode == VI_PAL);

   VIDEO_GetPreferredMode(&pref);
   std->pref_width  = pref.fbWidth;
   std->pref_height = pref.xfbHeight;

   if (tvmode)
      *tvmode = mode;
}

static unsigned gx_display_server_current_id(void)
{
   global_t *global = global_get_ptr();
   unsigned id      = gx_modes_clamp_id(
         global->console.screen.resolutions.current.id);
   /* A config naming a mode that does not exist starts over at
    * the default, as the driver would run it */
   global->console.screen.resolutions.current.id = id;
   return id;
}

static void *gx_display_server_init(void)
{
   return &gx_display_server_state;
}

static void gx_display_server_destroy(void *data) { }

static void *gx_display_server_get_resolution_list(void *data,
      unsigned *len)
{
   gx_vi_standard_t std;
   unsigned count;
   unsigned current = gx_display_server_current_id();
   video_display_config_t *list;

   gx_display_server_query(&std, NULL);

   count = gx_modes_list(&std, current, NULL, 0);
   list  = (video_display_config_t*)malloc(count * sizeof(*list));
   if (!list)
   {
      *len = 0;
      return NULL;
   }

   *len  = gx_modes_list(&std, current, list, count);
   return list;
}

static bool gx_display_server_set_resolution(void *data,
      unsigned dims, int int_hz, float hz, int center,
      int monitor_index, int xoffset, int padjust)
{
   gx_vi_standard_t std;
   int id;
   global_t *global = global_get_ptr();

   /* No dims is a refresh rate change on its own, which the VI
    * cannot make without changing the mode */
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;

   gx_display_server_query(&std, NULL);

   if ((id = gx_modes_find(&std, dims)) < 0)
   {
      RARCH_WARN("[GX] No %ux%u mode on this TV standard.\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      return false;
   }

   global->console.screen.resolutions.current.id = (unsigned)id;
   return video_driver_set_video_mode(gx_modes_dims((unsigned)id), true);
}

static float gx_display_server_get_refresh_rate(void *data)
{
   gx_vi_standard_t std;
   gx_vi_mode_t mode;

   gx_display_server_query(&std, NULL);
   gx_modes_resolve(&std,
         gx_modes_dims(gx_display_server_current_id()), &mode);
   return mode.hz;
}

static void gx_display_server_get_video_output_size(void *data,
      unsigned *dims, char *s, size_t len)
{
   /* 0x0 for the default: the menu shows it as such, and it is what
    * resetting the setting hands back to the driver */
   *dims = gx_modes_dims(gx_display_server_current_id());
}

const video_display_server_t dispserv_gx = {
   gx_display_server_init,
   gx_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   gx_display_server_set_resolution,
   gx_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   gx_display_server_get_refresh_rate,
   gx_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* get_flags */
   NULL, /* get_scanline */
   NULL, /* wait_vblank */
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
   NULL, /* get_edid */
   NULL, /* idle_wait */
   "gx"
};
