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

#ifndef __GFX_CONTEXT_H
#define __GFX_CONTEXT_H

#include "../boolean.h"
#include "../driver.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(RARCH_CONSOLE) || defined(HAVE_RMENU)
#include "../console/rmenu/rmenu.h"
#endif

#define MAX_EGLIMAGE_TEXTURES 32

enum gfx_ctx_api
{
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_OPENVG_API
};

typedef struct rarch_position
{
   float x;
   float y;
   float width;
   float height;
} rarch_position_t;

typedef void (*gfx_ctx_proc_t)(void);

typedef struct gfx_ctx_driver
{
   bool (*init)(void);
   void (*destroy)(void);

   bool (*bind_api)(enum gfx_ctx_api); // Which API to bind to.

   // Sets the swap interval.
   void (*swap_interval)(unsigned);

   // Sets video mode. Creates a window, etc.
   bool (*set_video_mode)(unsigned, unsigned, bool);

   // Gets current window size.
   // If not initialized yet, it returns current screen size.
   void (*get_video_size)(unsigned*, unsigned*);

   // Translates a window size to an aspect ratio.
   // In most cases this will be just width / height, but
   // some contexts will better know which actual aspect ratio is used.
   // This can be NULL to assume the default behavior.
   float (*translate_aspect)(unsigned, unsigned);

   // Asks driver to update window title (FPS, etc).
   void (*update_window_title)(bool);

   // Queries for resize and quit events.
   // Also processes events.
   void (*check_window)(bool*, bool*, unsigned*, unsigned*, unsigned);

   // Acknowledge a resize event. This is needed for some APIs. Most backends will ignore this.
   void (*set_resize)(unsigned, unsigned);

   // Checks if window has input focus.
   bool (*has_focus)(void);

   // Swaps buffers. VBlank sync depends on earlier calls to swap_interval.
   void (*swap_buffers)(void);

   // Most video backends will want to use a certain input driver.
   // Checks for it here.
   void (*input_driver)(const input_driver_t**, void**);

   // Wraps whatever gl_proc_address() there is.
   gfx_ctx_proc_t (*get_proc_address)(const char*);

   // Returns true if this context supports EGLImage buffers for screen drawing and was initalized correctly.
   bool (*init_egl_image_buffer)(const video_info_t*);

   // Writes the frame to the EGLImage and sets image_handle to it. Returns true if a new image handle is created.
   // Always returns true the first time it's called for a new index. The graphics core must handle a change in the handle correctly.
   bool (*write_egl_image)(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle);

   // Called after frame is rendered, but before swap.
   // Can be used to render context-specific overlays and stuff.
   bool (*post_render)(void *gl);

   // Human readable string.
   const char *ident;

#if defined(HAVE_RMENU) || defined(_XBOX360)
   void (*set_blend)(bool enable);
   void (*set_filtering)(unsigned index, bool set_smooth);
   void (*get_available_resolutions)(void);
   int  (*check_resolution)(unsigned resolution_id);
   void (*set_fbo)(unsigned);
   bool (*rmenu_init)(void);
   void (*rmenu_frame)(void *data);
   void (*rmenu_free)(void);
   void (*rmenu_draw_bg)(rarch_position_t *position);
   void (*rmenu_draw_panel)(rarch_position_t *position);
   void (*rmenu_set_default_pos)(rmenu_default_positions_t *position);
   void (*rmenu_screenshot_enable)(bool enable);
   void (*rmenu_screenshot_dump)(void *data);
   const char *(*drive_mapping_previous)(void);
   const char *(*drive_mapping_next)(void);
#endif
} gfx_ctx_driver_t;

extern const gfx_ctx_driver_t gfx_ctx_sdl_gl;
extern const gfx_ctx_driver_t gfx_ctx_x_egl;
extern const gfx_ctx_driver_t gfx_ctx_glx;
extern const gfx_ctx_driver_t gfx_ctx_drm_egl;
extern const gfx_ctx_driver_t gfx_ctx_android;
extern const gfx_ctx_driver_t gfx_ctx_ps3;
extern const gfx_ctx_driver_t gfx_ctx_xdk;
extern const gfx_ctx_driver_t gfx_ctx_wgl;
extern const gfx_ctx_driver_t gfx_ctx_videocore;

const gfx_ctx_driver_t *gfx_ctx_find_driver(const char *ident); // Finds driver with ident. Does not initialize.
const gfx_ctx_driver_t *gfx_ctx_init_first(enum gfx_ctx_api api); // Finds first suitable driver and initializes.

#endif

