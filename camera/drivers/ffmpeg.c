/*  RetroArch - A frontend for libretro.
*  Copyright (C) 2010-2023 - Hans-Kristian Arntzen
*  Copyright (C) 2023 - Jesse Talavera-Greenberg
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

#include <libretro.h>

#include "../camera_driver.h"
#include "verbosity.h"

typedef struct ffmpeg_camera
{
} ffmpeg_camera_t;

void *ffmpeg_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   ffmpeg_camera_t *ffmpeg = NULL;

   if ((caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)) == 0)
   { /* If the core didn't ask for raw framebuffers... */
      RARCH_ERR("[FFMPEG]: Camera driver only supports raw framebuffer output.\n");
      return NULL;
   }

   ffmpeg = (ffmpeg_camera_t*)calloc(1, sizeof(*ffmpeg));
   if (!ffmpeg)
   {
      RARCH_ERR("[FFMPEG]: Failed to allocate memory for camera driver.\n");
      return NULL;
   }

   return ffmpeg;
}

void ffmpeg_camera_free(void *data)
{
   ffmpeg_camera_t *ffmpeg = (ffmpeg_camera_t*)data;

   if (!ffmpeg)
      return;

   free(ffmpeg);
}

bool ffmpeg_camera_start(void *data)
{
   ffmpeg_camera_t *ffmpeg = (ffmpeg_camera_t*)data;

   return false;
}

void ffmpeg_camera_stop(void *data)
{
   ffmpeg_camera_t *ffmpeg = (ffmpeg_camera_t*)data;

}

bool ffmpeg_camera_poll(
   void *data,
   retro_camera_frame_raw_framebuffer_t frame_raw_cb,
   retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   ffmpeg_camera_t *ffmpeg = (ffmpeg_camera_t*)data;

   return false;
}

camera_driver_t camera_ffmpeg = {
   ffmpeg_camera_init,
   ffmpeg_camera_free,
   ffmpeg_camera_start,
   ffmpeg_camera_stop,
   ffmpeg_camera_poll,
   "ffmpeg",
};
