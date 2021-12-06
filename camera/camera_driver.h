/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#ifndef _CAMERA_DRIVER_H
#define _CAMERA_DRIVER_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

RETRO_BEGIN_DECLS

typedef struct camera_driver
{
   /* FIXME: params for initialization - queries for resolution,
    * framerate, color format which might or might not be honored. */
   void *(*init)(const char *device, uint64_t buffer_types,
         unsigned width, unsigned height);

   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   /* Polls the camera driver.
    * Will call the appropriate callback if a new frame is ready.
    * Returns true if a new frame was handled. */
   bool (*poll)(void *data,
         retro_camera_frame_raw_framebuffer_t frame_raw_cb,
         retro_camera_frame_opengl_texture_t frame_gl_cb);

   const char *ident;
} camera_driver_t;

typedef struct
{
   struct retro_camera_callback cb;    /* uint64_t alignment */
   const camera_driver_t *driver;
   void *data;
   bool active;
} camera_driver_state_t;

extern const camera_driver_t *camera_drivers[];


extern camera_driver_t camera_v4l2;
extern camera_driver_t camera_android;
extern camera_driver_t camera_rwebcam;
extern camera_driver_t camera_avfoundation;

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char* config_get_camera_driver_options(void);

bool driver_camera_start(void);

void driver_camera_stop(void);

bool camera_driver_find_driver(const char *prefix,
      bool verbosity_enabled);

camera_driver_state_t *camera_state_get_ptr(void);

RETRO_END_DECLS

#endif
