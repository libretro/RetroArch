/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __GL_OPENXR_H
#define __GL_OPENXR_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "openxr_frame.h"
#include "../video_driver.h"
#include "../video_xr.h"

RETRO_BEGIN_DECLS

/* Headset output for the desktop GL drivers. The XR thread holds a
 * context in the window context's share group for its whole life and
 * makes every session, swapchain and frame call. The video thread
 * draws each slot into the textures of a triple buffer and publishes
 * them with a fence; the XR thread waits on it, copies the newest into
 * the slot's swapchain image, and shows the quads and projection that
 * came with it. Each fence is deleted by the thread that made it. */
typedef struct gl_openxr gl_openxr_t;

/* Video thread, the window's context current: a context the XR thread
 * will hold, the runtime's instance and headset, the runtime asked of
 * that context's version. NULL, after one log line and a notification,
 * where any of them is missing. */
gl_openxr_t *gl_openxr_new(const gfx_ctx_driver_t *ctx_driver,
      void *ctx_data);

/* Starts the XR thread, which makes the session; false when it could
 * not, the window as before. The host is registered once it runs. */
bool gl_openxr_start(gl_openxr_t *xr);

/* Stops the XR thread, which destroys the session and its swapchains
 * as it ends, before a core's context_destroy. NULL is fine. */
void gl_openxr_stop_thread(gl_openxr_t *xr);

/* Video thread, the window's context current. NULL is fine. */
void gl_openxr_free(gl_openxr_t *xr);

/* Video thread: the textures to draw slot's next image into, layer 0
 * and, for a stereo slot, 1; RGBA8, dims in VIDEO_SCALE_PACK. They stay
 * the video thread's until published. False: none. */
bool gl_openxr_slot_target(gl_openxr_t *xr, unsigned slot,
      unsigned dims, unsigned layers, uint32_t textures[2]);

/* Video thread, after drawing the target: that image with quads' quads
 * for slot and, for OPENXR_PROJECTION_SLOT alone, the projection drawn
 * into it (gl_openxr_projection). The XR thread shows them together,
 * once copied, never one without the other. */
void gl_openxr_slot_publish(gl_openxr_t *xr, unsigned slot,
      const video_xr_quad_set_t *quads,
      const openxr_projection_t *projection);

/* Video thread: slot shows nothing from its next image on, in order
 * with the images published before; a slot shown again shows only an
 * image drawn after this. */
void gl_openxr_slot_forget(gl_openxr_t *xr, unsigned slot);

/* Video thread: as forget, for the screen the tracked eyes are on: it
 * keeps showing until the projection published since does, so the
 * headset never shows neither. A projection cleared with a screen
 * published again likewise waits for that screen's image. */
void gl_openxr_slot_yield(gl_openxr_t *xr, unsigned slot);

/* Video thread: the size of slot's images the runtime refused, 0 for
 * none; that slot's quads are left out until it is drawn at another. */
unsigned gl_openxr_slot_refused(gl_openxr_t *xr, unsigned slot);

/* Video thread: the projection of a frame drawn from the views the core
 * was last given, each eye width x height from its image's origin. */
void gl_openxr_projection(gl_openxr_t *xr, unsigned width,
      unsigned height, openxr_projection_t *out);

/* As vulkan_openxr_*. */
bool gl_openxr_alive(gl_openxr_t *xr);
bool gl_openxr_should_draw(gl_openxr_t *xr);
float gl_openxr_pixels_per_radian(gl_openxr_t *xr);
float gl_openxr_refresh_rate(gl_openxr_t *xr);
void gl_openxr_set_pacing(gl_openxr_t *xr, unsigned interval);
void gl_openxr_pace_wait(gl_openxr_t *xr);
/* Once a frame: the runtime's events while the XR thread is parked. */
void gl_openxr_host_poll(gl_openxr_t *xr);
void gl_openxr_pace_skip(gl_openxr_t *xr);
unsigned gl_openxr_refresh_rates(const gl_openxr_t *xr, float *rates,
      unsigned cap);
void gl_openxr_request_rate(gl_openxr_t *xr, float hz);
unsigned gl_openxr_max_dim(const gl_openxr_t *xr);
void gl_openxr_set_hidden(gl_openxr_t *xr, bool hidden);
bool gl_openxr_has_given(const gl_openxr_t *xr);
void gl_openxr_place_kept(gl_openxr_t *xr);
void gl_openxr_new_frame(gl_openxr_t *xr);
unsigned gl_openxr_eye_dims(const gl_openxr_t *xr);
void gl_openxr_request_recenter(gl_openxr_t *xr);
void gl_openxr_set_placement(gl_openxr_t *xr, unsigned placement);

RETRO_END_DECLS

#endif
