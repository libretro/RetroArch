/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

/* Apple CGL context. */

#include <CoreGraphics/CoreGraphics.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <stdio.h>
#include <stdlib.h>

#include "../../driver.h"
#include "../video_context_driver.h"
#include "../video_monitor.h"

typedef int CGSConnectionID;
typedef int CGSWindowID;
typedef int CGSSurfaceID;

/* Undocumented CGS */
extern CGSConnectionID CGSMainConnectionID(void);
extern CGError CGSAddSurface(CGSConnectionID cid, CGWindowID wid, CGSSurfaceID *sid);
extern CGError CGSSetSurfaceBounds(CGSConnectionID cid, CGWindowID wid, CGSSurfaceID sid, CGRect rect);
extern CGError CGSOrderSurface(CGSConnectionID cid, CGWindowID wid, CGSSurface sid, int a, int b);

/* Undocumented CGL */
extern CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID, cid, CGSWindowID wid, CGSSurfaceID sid);

typedef struct gfx_ctx_cgl_data
{
   CGLContextObj glCtx;
   CGDirectDisplayID displayID;
   int width, height;
} gfx_ctx_cgl_data_t;

static void gfx_ctx_cgl_swap_interval(void *data, unsigned interval)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;
   GLint params            = interval;

   CGLSetParameter(cgl->glCtx, kCGLCPSwapInterval, &params);
}

static void gfx_ctx_cgl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;
   (void)data;
   (void)quit;
   (void)width;
   (void)height;
   (void)resize;
}

static void gfx_ctx_cgl_swap_buffers(void *data)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;

   CGLFlushDrawable(cgl->glCtx);
}

static void gfx_ctx_cgl_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_cgl_update_window_title(void *data)
{
   (void)data;
}

static void gfx_ctx_cgl_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
   *width  = 320;
   *height = 240;
}

static bool gfx_ctx_cgl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;

   return true;
}

static void gfx_ctx_cgl_destroy(void *data)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;
   
   if (cgl->glCtx)
   {
      CGLSetCurrentContext(NULL);
      CGLDestroyContext(cgl->glCtx);
   }

   if (cgl->displayID)
      CGDisplayRelease(cgl->displayID);

   if (cgl)
      free(cgl);
}

static void gfx_ctx_cgl_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   (void)input;
   (void)input_data;
}

static bool gfx_ctx_cgl_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_cgl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_cgl_has_windowed(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_cgl_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)api;
   (void)major;
   (void)minor;

   return true;
}

static void gfx_ctx_cgl_show_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static void gfx_ctx_cgl_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;

   (void)enable;

   CGLSetCurrentContext(cgl->glCtx);

   /* TODO - needs to handle HW render context too */
}

static CGLContextObj gfx_ctx_cgl_init_create(void)
{
   CGLint num;
   CGLPixelFormatObj pix;
   CGLContextObj glCtx = NULL;
   CGLPixelFormatAttribute attributes[] = {
      kCGLPFAAccelerated,
      kCGLPFADoublebuffer,
      (CGLPixelFormatAttribute)0
   };

   CGLChoosePixelFormat(attributes, &pix, &num);
   CGLCreateContext(pix, NULL, &glCtx);
   CGLDestroyPixelFormat(pix);

   return glCtx;
}

static bool gfx_ctx_cgl_init(void *data)
{
   CGError err;
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)calloc(1, sizeof(gfx_ctx_cgl_data_t));

   if (!cgl)
      goto false;

   cgl->displayID = CGMainDisplayID();

   err = CGDisplayCapture(cgl->displayID);

   if (err != kCGErrorSuccess)
      goto error;

   cgl->glCtx = gfx_ctx_cgl_init_create();

   if (!cgl->glCtx)
      goto error;

   driver->video_context_data = cgl;

   return true;

error:
   gfx_ctx_cgl_destroy(cgl);

   return false;
}

const gfx_ctx_driver_t gfx_ctx_cgl = {
   gfx_ctx_cgl_init,
   gfx_ctx_cgl_destroy,
   gfx_ctx_cgl_bind_api,
   gfx_ctx_cgl_swap_interval,
   gfx_ctx_cgl_set_video_mode,
   gfx_ctx_cgl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_cgl_update_window_title,
   gfx_ctx_cgl_check_window,
   gfx_ctx_cgl_set_resize,
   gfx_ctx_cgl_has_focus,
   gfx_ctx_cgl_suppress_screensaver,
   gfx_ctx_cgl_has_windowed,
   gfx_ctx_cgl_swap_buffers,
   gfx_ctx_cgl_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_cgl_show_mouse,
   "cgl",
   gfx_ctx_cgl_bind_hw_render,
};
