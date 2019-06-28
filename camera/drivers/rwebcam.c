/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>

#include <boolean.h>

#include "../../retroarch.h"

/* forward declarations */
void *RWebCamInit(uint64_t caps, unsigned width, unsigned height);
void RWebCamFree(void *data);
bool RWebCamStart(void *data);
void RWebCamStop(void *data);
bool RWebCamPoll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb);

static void *rwebcam_init(const char *device, uint64_t caps,
      unsigned width, unsigned height)
{
   (void)device;
   return RWebCamInit(caps, width, height);
}

static void rwebcam_free(void *data)
{
   RWebCamFree(data);
}

static bool rwebcam_start(void *data)
{
   return RWebCamStart(data);
}

static void rwebcam_stop(void *data)
{
   RWebCamStop(data);
}

static bool rwebcam_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   return RWebCamPoll(data, frame_raw_cb, frame_gl_cb);
}

camera_driver_t camera_rwebcam = {
   rwebcam_init,
   rwebcam_free,
   rwebcam_start,
   rwebcam_stop,
   rwebcam_poll,
   "rwebcam",
};
