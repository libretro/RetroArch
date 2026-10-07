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

/* Display server for the GameCube and Wii on os/gekko.  The modes the
 * VI offers are dispserv_gx_modes.c's, shared with the libogc build;
 * the standard the console is set to comes from the VI and (Wii) the
 * system settings.  The video driver programs the VI: set_resolution
 * records the choice as the fullscreen size and hands the mode to
 * it.  Only whole modes switch, so a refresh rate change alone fails. */

#include <stdlib.h>

#include <gekko/video.h>

#include "dispserv_gx.h"
#include "../video_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

/* Nothing to keep; any non-NULL pointer marks the server as up */
static char gx_display_server_state;

void gx_display_server_query(gx_vi_standard_t *std, unsigned *tvmode)
{
   gk_vi_mode_t pref;

   gk_vi_preferred(&pref);
   std->progressive = pref.scan == GK_VI_PROGRESSIVE;
   std->max_width   = 720;
   std->max_height  = gk_vi_max_lines(pref.std);
   std->fifty_hz    = pref.std == GK_VI_PAL;
   std->pref_width  = pref.fb_width;
   std->pref_height = pref.fb_lines;

   if (tvmode)
      *tvmode = pref.std;
}

unsigned gx_display_server_current_id(void)
{
   gx_vi_standard_t std;
   const settings_t *settings = config_get_ptr();
   gx_display_server_query(&std, NULL);
   return gx_modes_id(&std, VIDEO_SCALE_PACK(
         settings->uints.video_fullscreen_x,
         settings->uints.video_fullscreen_y));
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
   settings_t *settings = config_get_ptr();

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

   settings->uints.video_fullscreen_x = VIDEO_SCALE_W(dims);
   settings->uints.video_fullscreen_y = VIDEO_SCALE_H(dims);
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
