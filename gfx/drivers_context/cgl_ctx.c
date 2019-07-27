/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

/* Apple CGL context.
   Based on http://fernlightning.com/doku.php?id=randd:xopengl.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <string/stdstring.h>

#include <ApplicationServices/ApplicationServices.h>

#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include "../../retroarch.h"

typedef int CGSConnectionID;
typedef int CGSWindowID;
typedef int CGSSurfaceID;

typedef uint32_t _CGWindowID;

#ifdef __cplusplus
extern "C" {
#endif

/* Undocumented CGS */
extern CGSConnectionID CGSMainConnectionID(void);
extern CGError CGSAddSurface(CGSConnectionID cid, _CGWindowID wid, CGSSurfaceID *sid);
extern CGError CGSSetSurfaceBounds(CGSConnectionID cid, _CGWindowID wid, CGSSurfaceID sid, CGRect rect);
extern CGError CGSOrderSurface(CGSConnectionID cid, _CGWindowID wid, CGSSurfaceID sid, int a, int b);

/* Undocumented CGL */
extern CGLError CGLSetSurface(CGLContextObj gl, CGSConnectionID cid, CGSWindowID wid, CGSSurfaceID sid);

#ifdef __cplusplus
}
#endif

static enum gfx_ctx_api cgl_api = GFX_CTX_NONE;

typedef struct gfx_ctx_cgl_data
{
   CGLContextObj glCtx;
   CGDirectDisplayID displayID;
   int width, height;
} gfx_ctx_cgl_data_t;

static void gfx_ctx_cgl_swap_interval(void *data, int interval)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;
   GLint params            = interval;

   CGLSetParameter(cgl->glCtx, kCGLCPSwapInterval, &params);
}

static void gfx_ctx_cgl_get_video_size(void *data, unsigned *width, unsigned *height)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;
   *width  = cgl->width;
   *height = cgl->height;
}

static void gfx_ctx_cgl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width  = 0;
   unsigned new_height = 0;

   *quit = false;

   gfx_ctx_cgl_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void gfx_ctx_cgl_swap_buffers(void *data, void *data2)
{
   gfx_ctx_cgl_data_t *cgl = (gfx_ctx_cgl_data_t*)data;

   CGLFlushDrawable(cgl->glCtx);
}

static bool gfx_ctx_cgl_set_video_mode(void *data,
      video_frame_info_t *video_info,
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

static void gfx_ctx_cgl_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   (void)data;
   (void)input;
   (void)input_data;
}

static gfx_ctx_proc_t gfx_ctx_cgl_get_proc_address(const char *symbol_name)
{
   CFURLRef bundle_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
         CFSTR
         ("/System/Library/Frameworks/OpenGL.framework"),
         kCFURLPOSIXPathStyle, true);
   CFBundleRef opengl_bundle_ref  = CFBundleCreate(kCFAllocatorDefault, bundle_url);
   CFStringRef function =  CFStringCreateWithCString(kCFAllocatorDefault, symbol_name,
         kCFStringEncodingASCII);
   gfx_ctx_proc_t ret = (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(
         opengl_bundle_ref, function);

   CFRelease(bundle_url);
   CFRelease(function);
   CFRelease(opengl_bundle_ref);

   return ret;
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

static enum gfx_ctx_api gfx_ctx_cgl_get_api(void *data)
{
   return cgl_api;
}

static bool gfx_ctx_cgl_bind_api(void *data, enum gfx_ctx_api api,
   unsigned major, unsigned minor)
{
   (void)data;
   (void)api;
   (void)major;
   (void)minor;

   if (api == GFX_CTX_OPENGL_API)
   {
      cgl_api = api;
      return true;
   }

   return false;
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
   GLint num, params = 1;
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

   CGLSetParameter(glCtx, kCGLCPSwapInterval, &params);

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
    /* FIXME/TODO - CGWindowListCopyWindowInfo was introduced on OSX 10.5,
     * find alternative for lower versions. */
    wins = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, wid); /* expect one result only */
    win = (CFDictionaryRef)CFArrayGetValueAtIndex(wins, 0);
    bnd = (CFDictionaryRef)CFDictionaryGetValue(win, kCGWindowBounds);
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)bnd, CFSTR("Width")),
       kCFNumberFloat64Type, &w);
    CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(bnd, CFSTR("Height")),
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

static void *gfx_ctx_cgl_init(video_frame_info_t *video_info, void *video_driver)
{
   CGError err;
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

   printf("size:%dx%d\n", cgl->width, cgl->height);

   CGLSetCurrentContext(cgl->glCtx);

   return cgl;

error:
   gfx_ctx_cgl_destroy(cgl);

   return NULL;
}

static uint32_t gfx_ctx_cgl_get_flags(void *data)
{
   uint32_t flags = 0;

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else
   {
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   }

   return flags;
}

static void gfx_ctx_cgl_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_cgl = {
   gfx_ctx_cgl_init,
   gfx_ctx_cgl_destroy,
   gfx_ctx_cgl_get_api,
   gfx_ctx_cgl_bind_api,
   gfx_ctx_cgl_swap_interval,
   gfx_ctx_cgl_set_video_mode,
   gfx_ctx_cgl_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_cgl_check_window,
   NULL, /* set_resize */
   gfx_ctx_cgl_has_focus,
   gfx_ctx_cgl_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_cgl_swap_buffers,
   gfx_ctx_cgl_input_driver,
   gfx_ctx_cgl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_cgl_show_mouse,
   "cgl",
   gfx_ctx_cgl_get_flags,
   gfx_ctx_cgl_set_flags,
   gfx_ctx_cgl_bind_hw_render,
   NULL,
   NULL
};
