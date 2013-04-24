/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "../general.h"
#include "../driver.h"

static void *null_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
   (void)video;

   return (void*)-1;
}

static bool null_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
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

static void null_gfx_free(void *data)
{
   (void)data;
}

#ifdef RARCH_CONSOLE
static void null_gfx_start(void) {}
static void null_gfx_restart(void) {}
#endif

const video_driver_t video_null = {
   null_gfx_init,
   null_gfx_frame,
   null_gfx_set_nonblock_state,
   null_gfx_alive,
   null_gfx_focus,
   NULL,
   null_gfx_free,
   "null",

#ifdef RARCH_CONSOLE
   null_gfx_start,
   null_gfx_restart,
#endif
};

