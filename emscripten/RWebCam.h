/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Michael Lelli
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
#include <stdbool.h>
#include "../driver.h"

void *RWebCamInit(uint64_t caps, unsigned width, unsigned height);
void RWebCamFree(void *data);
bool RWebCamStart(void *data);
void RWebCamStop(void *data);
bool RWebCamPoll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb, retro_camera_frame_opengl_texture_t frame_gl_cb);
