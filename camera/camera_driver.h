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

#include <retro_miscellaneous.h>

#include "../gfx/video_defines.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

RETRO_BEGIN_DECLS

struct string_list;

typedef struct camera_driver
{
   /* FIXME: params for initialization - queries for resolution,
    * framerate, color format which might or might not be honored. */
   /* @dims is the requested frame size, packed with VIDEO_SCALE_PACK;
    * an axis of 0 leaves it to the device. */
   void *(*init)(const char *device, uint64_t buffer_types,
         unsigned dims);

   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   /* Polls the camera driver.
    * Will call the appropriate callback if a new frame is ready: the
    * one the driver was initialised for, among those the core gave.
    * Returns true if a new frame was handled. */
   bool (*poll)(void *data,
         retro_camera_frame_raw_framebuffer_t frame_raw_cb,
         retro_camera_frame_opengl_texture_t frame_gl_cb,
         retro_camera_frame_planar_t frame_planar_cb);

   const char *ident;
} camera_driver_t;

/* A frontend consumer of camera frames beside the core: the menu's
 * live preview. Frames reach it on the main thread, inside a poll, in
 * the mode the camera was opened for; planes are upright, a rotated
 * camera's turned for it. */
typedef struct camera_preview_sink
{
   void (*planar)(void *user, const struct retro_camera_planar_frame *f);
   void (*raw)(void *user, const uint32_t *buf, unsigned width,
         unsigned height, size_t pitch);
   void *user;
} camera_preview_sink_t;

enum camera_preview_state
{
   CAMERA_PREVIEW_OFF = 0,     /* no preview open */
   CAMERA_PREVIEW_DISALLOWED,  /* camera_allow is off */
   CAMERA_PREVIEW_OWN,         /* the preview's own camera */
   CAMERA_PREVIEW_SHARED,      /* the core's camera, started */
   CAMERA_PREVIEW_CORE_IDLE,   /* the core's camera, not started */
   CAMERA_PREVIEW_FAILED       /* the driver would not open or start */
};

/* camera_pip_corner: where the gameplay picture-in-picture goes */
enum camera_pip_corner
{
   CAMERA_PIP_TOP_LEFT = 0,
   CAMERA_PIP_TOP_RIGHT,
   CAMERA_PIP_BOTTOM_LEFT,
   CAMERA_PIP_BOTTOM_RIGHT
};

typedef struct
{
   struct retro_camera_callback cb;    /* uint64_t alignment */
   camera_preview_sink_t preview;
   const camera_driver_t *driver;
   void *data;
   /* The preview's own camera, open only while no core holds one. */
   const camera_driver_t *preview_driver;
   void *preview_data;
   /* NV12 a rotated frame is turned upright into for the preview */
   uint8_t *preview_upright;
   size_t preview_upright_size;
   char preview_device[NAME_MAX_LENGTH];
   bool active;
   bool preview_on;
   bool preview_failed;
   bool core_polled;    /* the runloop polled since the preview did */
   bool forward_core;   /* the poll in progress delivers to the core */
} camera_driver_state_t;

extern const camera_driver_t *camera_drivers[];


extern camera_driver_t camera_v4l2;
extern camera_driver_t camera_pipewire;
extern camera_driver_t camera_android;
extern camera_driver_t camera_rwebcam;
extern camera_driver_t camera_avfoundation;
extern camera_driver_t camera_ffmpeg;

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

/* The runloop's per-frame poll of the core's camera; an open preview
 * is handed the same frames. */
void camera_driver_poll(void);

/* Opens the preview: @sink is copied, and frames reach it from
 * camera_driver_preview_poll() and from the core's polls. Nothing is
 * opened until the first preview poll. */
void camera_driver_preview_open(const camera_preview_sink_t *sink);
/* Closes the preview and its own camera, if it has one. */
void camera_driver_preview_close(void);
/* Once a frame while the preview shows. Opens the preview's own camera
 * when no core holds one, and polls whichever camera it watches unless
 * the runloop already did this frame. */
enum camera_preview_state camera_driver_preview_poll(void);
/* Frees the preview's own camera, keeping the preview open: before a
 * core's camera is created, and when the camera may no longer run. */
void camera_driver_preview_release(void);

void driver_camera_stop(void);

bool camera_driver_find_driver(const char *prefix,
      bool verbosity_enabled);

camera_driver_state_t *camera_state_get_ptr(void);

RETRO_END_DECLS

#endif
