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

/* Apple CGL context.
   Based on http://fernlightning.com/doku.php?id=randd:xopengl.
 */

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
extern CGError CGSOrderSurface(CGSConnectionID cid, CGWindowID wid, CGSSurfaceID sid, int a, int b);

/* Undocumented CGL */
extern CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);

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

static bool gfx_ctx_cgl_bind_api(void *data, enum gfx_ctx_api api,
   unsigned major, unsigned minor)
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
   GLint num;
   CGLPixelFormatObj pix;
   CGLContextObj glCtx = NULL;
   CGLPixelFormatAttribute attributes[] = {
      kCGLPFAAccelerated,
      kCGLPFADoubleBuffer,
      (CGLPixelFormatAttribute)0
   };

   CGLChoosePixelFormat(attributes, &pix, &num);
   CGLCreateContext(pix, NULL, &glCtx);
   CGLDestroyPixelFormat(pix);

   return glCtx;
}

static CGSSurfaceID attach_gl_context_to_window(CGLContextObj glCtx,
   CGSWindowID wid, int *width, int *height)
{
    CFArrayRef wins;
    CFDictionaryRef win, bnd;
    GLint params = 0;
    Float64 w = 0, h = 0;
    CGSSurfaceID sid = 0;
    CGSConnectionID cid = CGSMainConnectionID();

    printf("cid:%d wid:%d\n", cid, wid);
 
    /* determine window size */
    wins = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, wid); /* expect one result only */
    win = CFArrayGetValueAtIndex(wins, 0);
    bnd = CFDictionaryGetValue(win, kCGWindowBounds);
    CFNumberGetValue(CFDictionaryGetValue(bnd, CFSTR("Width")),
       kCFNumberFloat64Type, &w);
    CFNumberGetValue(CFDictionaryGetValue(bnd, CFSTR("Height")),
       kCFNumberFloat64Type, &h);
    CFRelease(wins);
 
    /* create a surface. */
    if(CGSAddSurface(cid, wid, &sid) != kCGErrorSuccess)
    {
       printf("ERR: no surface\n");
    }
    printf("sid:%d\n", sid);
 
    /* set surface size, and order it frontmost */
    if(CGSSetSurfaceBounds(cid, wid, sid, CGRectMake(0, 0, w, h)) != kCGErrorSuccess)
       printf("ERR: cant set bounds\n");
    if(CGSOrderSurface(cid, wid, sid, 1, 0) != kCGErrorSuccess)
       printf("ERR: cant order front\n");
 
    /* attach context to the surface */
    if(CGLSetSurface(glCtx, cid, wid, sid) != kCGErrorSuccess)
    {
       printf("ERR: cant set surface\n");
    }
 
    /* check drawable */
    CGLGetParameter(glCtx, kCGLCPHasDrawable, &params);
    if(params != 1)
    {
       printf("ERR: no drawable\n");
    }
 
    *width  = (int)w;
    *height = (int)h;

    return sid;
}

static bool gfx_ctx_cgl_init(void *data)
{
   CGError err;
   driver_t *driver = driver_get_ptr();
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)calloc(1, sizeof(gfx_ctx_cgl_data_t));

   if (!cgl)
      goto error;

   cgl->displayID = CGMainDisplayID();

   err = CGDisplayCapture(cgl->displayID);

   if (err != kCGErrorSuccess)
      goto error;

   cgl->glCtx = gfx_ctx_cgl_init_create();

   if (!cgl->glCtx)
      goto error;

   attach_gl_context_to_window(cgl->glCtx,
   CGShieldingWindowID(cgl->displayID), &cgl->width, &cgl->height);

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
