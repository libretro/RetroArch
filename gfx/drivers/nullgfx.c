/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../../general.h"
#include "../../driver.h"

static void *null_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   RARCH_ERR("Using the null video driver. RetroArch will not be visible.");

   *input = NULL;
   *input_data = NULL;
   (void)video;

   return (void*)-1;
}

static bool null_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   (void)data;
   (void)frame;
   (void)width;
   (void)height;
   (void)pitch;
   (void)msg;

   return true;
}

static void null_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool null_gfx_alive(void *data)
{
   (void)data;
   return true;
}

static bool null_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool null_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool null_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void null_gfx_free(void *data)
{
   (void)data;
}

static bool null_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false; 
}

static void null_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void null_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool null_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void null_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   (void)iface;
}

video_driver_t video_null = {
   null_gfx_init,
   null_gfx_frame,
   null_gfx_set_nonblock_state,
   null_gfx_alive,
   null_gfx_focus,
   null_gfx_suppress_screensaver,
   null_gfx_has_windowed,
   null_gfx_set_shader,
   null_gfx_free,
   "null",
   NULL, /* set_viewport */
   null_gfx_set_rotation,
   null_gfx_viewport_info,
   null_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  null_gfx_get_poke_interface,
};
