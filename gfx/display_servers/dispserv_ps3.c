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

/* Display server for the PS3. It lists the video output modes the
 * connected display takes (dispserv_ps3_modes.c), marks the one in
 * use, and switches between them. The output itself is configured by
 * whoever owns the frame buffers - the RSX driver on PSL1GHT, the PSGL
 * context on the Cell SDK - at init, from
 * ps3_display_server_resolution(); so a switch stores the choice in
 * current_resolution_id and reinitialises the video driver, which
 * comes back up in it.
 *
 * Only whole modes can be switched: a set_resolution without dims
 * (the refresh rate autoswitch) fails. */

#include <stdlib.h>

#ifdef __PSL1GHT__
#include <sysutil/video.h>
typedef videoState ps3_video_state_t;
#define PS3_VIDEO_GET_STATE(s) videoGetState(0, 0, (s))
#define PS3_VIDEO_AVAILABLE(id) videoGetResolutionAvailability( \
      VIDEO_PRIMARY, (id), VIDEO_ASPECT_AUTO, 0)
#else
#include <sysutil/sysutil_sysparam.h>
typedef CellVideoOutState ps3_video_state_t;
#define PS3_VIDEO_GET_STATE(s) cellVideoOutGetState( \
      CELL_VIDEO_OUT_PRIMARY, 0, (s))
#define PS3_VIDEO_AVAILABLE(id) cellVideoOutGetResolutionAvailability( \
      CELL_VIDEO_OUT_PRIMARY, (id), CELL_VIDEO_OUT_ASPECT_AUTO, 0)
#endif

#include "dispserv_ps3.h"
#include "../../command.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/* Nothing to keep; any non-NULL pointer marks the server as up */
static char ps3_display_server_state;

static bool ps3_display_server_available(void *user, unsigned id)
{
   return PS3_VIDEO_AVAILABLE(id) ? true : false;
}

/* The mode the system menu has the output in, 0 when unreadable */
static unsigned ps3_display_server_system_id(void)
{
   ps3_video_state_t state;
   if (PS3_VIDEO_GET_STATE(&state) != 0)
      return PS3_MODE_ID_SYSTEM;
   return state.displayMode.resolution;
}

unsigned ps3_display_server_resolution(unsigned system_id)
{
   global_t *global = global_get_ptr();
   return ps3_modes_effective(ps3_display_server_available, NULL,
         global->console.screen.resolutions.current.id, system_id);
}

static unsigned ps3_display_server_in_use(void)
{
   return ps3_display_server_resolution(ps3_display_server_system_id());
}

static void *ps3_display_server_init(void)
{
   return &ps3_display_server_state;
}

static void ps3_display_server_destroy(void *data) { }

static void *ps3_display_server_get_resolution_list(void *data,
      unsigned *len)
{
   unsigned count;
   video_display_config_t *list;
   global_t *global = global_get_ptr();
   unsigned current = global->console.screen.resolutions.current.id;
   unsigned system  = ps3_display_server_system_id();

   count = ps3_modes_list(ps3_display_server_available, NULL,
         current, system, NULL, 0);
   if (!count || !(list = (video_display_config_t*)
            malloc(count * sizeof(*list))))
   {
      *len = 0;
      return NULL;
   }

   *len = ps3_modes_list(ps3_display_server_available, NULL,
         current, system, list, count);
   return list;
}

static bool ps3_display_server_set_resolution(void *data,
      unsigned dims, int int_hz, float hz, int center,
      int monitor_index, int xoffset, int padjust)
{
   int id;
   global_t *global = global_get_ptr();

   /* No dims is a refresh rate change on its own, which the video
    * output cannot make without changing the mode */
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;

   if ((id = ps3_modes_find(ps3_display_server_available, NULL, dims)) < 0)
   {
      RARCH_WARN("[PS3] The display does not take %ux%u.\n",
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      return false;
   }

   if ((unsigned)id == ps3_display_server_in_use())
   {
      /* Already there; store it so the choice outlives a change to
       * the system menu's mode */
      global->console.screen.resolutions.current.id = (unsigned)id;
      return true;
   }

   global->console.screen.resolutions.current.id = (unsigned)id;
   /* PAL60 temporal conversion only applies to 576 (PSGL) */
   if (dims != VIDEO_SCALE_PACK(720, 576))
   {
      global->console.screen.pal_enable   = false;
      global->console.screen.pal60_enable = false;
   }

   /* The output is configured at video init, with the frame buffers
    * sized for it */
   command_event(CMD_EVENT_REINIT, NULL);
   return true;
}

static float ps3_display_server_get_refresh_rate(void *data)
{
   return ps3_modes_hz(ps3_display_server_in_use());
}

static void ps3_display_server_get_video_output_size(void *data,
      unsigned *dims, char *s, size_t len)
{
   *dims = ps3_modes_dims(ps3_display_server_in_use());
}

const video_display_server_t dispserv_ps3 = {
   ps3_display_server_init,
   ps3_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   ps3_display_server_set_resolution,
   ps3_display_server_get_resolution_list,
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   ps3_display_server_get_refresh_rate,
   ps3_display_server_get_video_output_size,
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
   "ps3"
};
