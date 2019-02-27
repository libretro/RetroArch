/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Graphics library for VG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vgfont.h"
#include "graphics_x_private.h"

/******************************************************************************
Defines.
******************************************************************************/
#define ATEXT_FONT_SIZE 12 /*< Default font size (font size can be set with *_ext functions). */

/******************************************************************************
Local data
******************************************************************************/
static GX_DISPLAY_T display; /*< Our one and only EGL display. */

/**
 * We create one eglContext for each of the possible graphics_x resource types
 * that are supported.
 ***********************************************************/
static EGLContext gx_contexts[GRAPHICS_RESOURCE_HANDLE_TYPE_MAX];

/** Note: we have to share all our contexts, because otherwise it seems
 * to be not valid to blit from one image to another if the images
 * have different contexts.
 *
 * That means we have to use a single global lock to serialise all accesses
 * to any contexts.
 ***********************************************************/
static VCOS_MUTEX_T lock;

static EGLConfig gx_configs[GRAPHICS_RESOURCE_HANDLE_TYPE_MAX];

static int inited;

/******************************************************************************
Local Functions
******************************************************************************/

/** Convert graphics_x colour formats into EGL format. */
static int gx_egl_attrib_colours(EGLint *attribs, GRAPHICS_RESOURCE_TYPE_T res_type)
{
   int i, n;
   static EGLint rgba[] = {EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE};
   static uint8_t rgb565[] = {5,6,5,0};
   static uint8_t rgb888[] = {8,8,8,0};
   static uint8_t rgb32a[] = {8,8,8,8};

   uint8_t *sizes = NULL;

   switch (res_type)
   {
      case GRAPHICS_RESOURCE_RGB565:
         sizes = rgb565;
         break;
      case GRAPHICS_RESOURCE_RGB888:
         sizes = rgb888;
         break;
      case GRAPHICS_RESOURCE_RGBA32:
         sizes = rgb32a;
         break;
      default:
         vcos_assert(0);
         return -1;
   }
   for (n=0, i=0; i<countof(rgba); i++)
   {
      attribs[n++] = rgba[i];
      attribs[n++] = sizes[i];
   }
   return n;
}

/* Create an EGLContext for a given GRAPHICS_RESOURCE_TYPE */
static VCOS_STATUS_T create_context(EGLDisplay disp,
                                    GRAPHICS_RESOURCE_TYPE_T image_type,
                                    EGLContext *shared_with)
{
   int n;
   EGLConfig configs[1];
   EGLint nconfigs, attribs[32];
   n = gx_egl_attrib_colours(attribs, image_type);

   // we want to be able to do OpenVG on this surface...
   attribs[n++] = EGL_RENDERABLE_TYPE; attribs[n++] = EGL_OPENVG_BIT;
   attribs[n++] = EGL_SURFACE_TYPE;    attribs[n++] = EGL_WINDOW_BIT;

   attribs[n] = EGL_NONE;

   EGLBoolean egl_ret = eglChooseConfig(disp,
                                        attribs, configs,
                                        countof(configs), &nconfigs);

   if (!egl_ret || !nconfigs)
   {
      GX_LOG("%s: no suitable configurations for res type %d",
             __FUNCTION__, image_type);
      return VCOS_EINVAL;
   }

   EGLContext cxt = eglCreateContext(disp, configs[0], *shared_with, 0);
   if (!cxt)
   {
      GX_LOG("Could not create context for image type %d: 0x%x",
             image_type, eglGetError());
      return VCOS_ENOSPC;
   }

   gx_contexts[image_type] = cxt;
   gx_configs[image_type] = configs[0];
   *shared_with = cxt;

   return VCOS_SUCCESS;
}

/******************************************************************************
Functions private to code inside GraphicsX
******************************************************************************/

static VCOS_STATUS_T gx_priv_initialise( void )
{
   int i;
   EGLDisplay disp;
   EGLint egl_maj, egl_min;
   int32_t ret = VCOS_EINVAL;
   EGLBoolean result;

   vcos_demand(inited == 0);

   vcos_log_set_level(&gx_log_cat, VCOS_LOG_WARN);
   vcos_log_register("graphics", &gx_log_cat);

   memset(&display,0,sizeof(display));

   gx_priv_init();

   disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);

   if (disp == EGL_NO_DISPLAY)
   {
      GX_LOG("Could not open display: 0x%x", eglGetError());
      vcos_assert(0);
      goto fail_disp;
   }

   result = eglInitialize(disp, &egl_maj, &egl_min);
   if (!result)
   {
      GX_LOG("Could not init display :0x%x", eglGetError());
      vcos_assert(0); // really can't continue
      goto fail_egl_init;
   }

   result = eglBindAPI(EGL_OPENVG_API);
   vcos_assert(result); // really should succeed

   display.disp = disp;

   GX_TRACE("Supported client APIS: %s", eglQueryString(disp, EGL_CLIENT_APIS));

   // create the available contexts
   EGLContext shared_context = EGL_NO_CONTEXT;
   ret =  create_context(disp,GRAPHICS_RESOURCE_RGB565, &shared_context);
   ret |= create_context(disp,GRAPHICS_RESOURCE_RGB888, &shared_context);
   ret |= create_context(disp,GRAPHICS_RESOURCE_RGBA32, &shared_context);

   if (ret != VCOS_SUCCESS)
      goto fail_cxt;

   eglSwapInterval(disp, 1);

   inited = 1;

   return ret;

fail_cxt:
   for (i=0; i<GRAPHICS_RESOURCE_HANDLE_TYPE_MAX; i++)
   {
      if (gx_contexts[i])
      {
         eglDestroyContext(display.disp,gx_contexts[i]);
         vcos_mutex_delete(&lock);
      }
   }
   eglTerminate(display.disp);
fail_egl_init:
fail_disp:
   return ret;
}

/*****************************************************************************/
void gx_priv_save(GX_CLIENT_STATE_T *state, GRAPHICS_RESOURCE_HANDLE res)
{
   EGLBoolean egl_result;
   vcos_assert(res == NULL || (res->magic == RES_MAGIC));
   vcos_assert(res == NULL || !res->context_bound);

   state->context      = eglGetCurrentContext();
   state->api          = eglQueryAPI();
   state->read_surface = eglGetCurrentSurface(EGL_READ);
   state->draw_surface = eglGetCurrentSurface(EGL_DRAW);
   state->res = res;

   vcos_assert(state->api); // should never be anything other than VG or GL

   vcos_mutex_lock(&lock);

   egl_result = eglBindAPI(EGL_OPENVG_API);
   vcos_assert(egl_result);

   if (res)
   {
      GX_TRACE("gx_priv_save: eglMakeCurrent: %s, res %x surface %x, cxt %x", vcos_thread_get_name(vcos_thread_current()),
         (uint32_t)res, (uint32_t)res->surface, (uint32_t)res->context);

      egl_result = eglMakeCurrent(display.disp, res->surface,
                                  res->surface, res->context);
      vcos_assert(egl_result);

      res->context_bound = 1;
   }
}

/*****************************************************************************/
void gx_priv_restore(GX_CLIENT_STATE_T *state)
{
   EGLBoolean egl_result;

   GX_TRACE("gx_priv_restore: eglMakeCurrent: %s, res %x draw_surface %x, surface %x, cxt %x", vcos_thread_get_name(vcos_thread_current()),
      (uint32_t)state->res, (uint32_t)state->draw_surface, (uint32_t)state->read_surface, (uint32_t)state->context);

   // disconnect our thread from this context, so we other threads can use it via
   // this API
   egl_result = eglMakeCurrent(display.disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   vcos_assert(egl_result);

   // now return to the client's API binding
   egl_result = eglBindAPI(state->api);
   vcos_assert(egl_result);

   egl_result = eglMakeCurrent(display.disp, state->draw_surface, state->read_surface, state->context);
   vcos_assert(egl_result);

   if (state->res) state->res->context_bound = 0;

   vcos_mutex_unlock(&lock);
}

/******************************************************************************
Functions and data exported as part of the public GraphicsX API
******************************************************************************/

VCOS_LOG_CAT_T gx_log_cat; /*< Logging category for GraphicsX. */

int32_t graphics_initialise( void )
{
   // dummy initialisation function. This is typically called
   // early in the day before VLLs are available, and so cannot
   // do anything useful.
   return 0;
}

/*****************************************************************************/
int32_t graphics_uninitialise( void )
{
   int i;
   vcos_assert(inited);

   gx_priv_font_term();

   for (i=0; i<GRAPHICS_RESOURCE_HANDLE_TYPE_MAX; i++)
      if (gx_contexts[i])
         eglDestroyContext(display.disp,gx_contexts[i]);

   eglTerminate(display.disp);
   gx_priv_destroy();
   vcos_log_unregister(&gx_log_cat);
   inited = 0;
   return 0;
}

/*****************************************************************************/
VCOS_STATUS_T gx_create_window( uint32_t screen_id,
                                uint32_t width,
                                uint32_t height,
                                GRAPHICS_RESOURCE_TYPE_T image_type,
                                GRAPHICS_RESOURCE_HANDLE *resource_handle )
{
   int rc;
   VCOS_STATUS_T status = VCOS_SUCCESS;
   GRAPHICS_RESOURCE_HANDLE h;
   EGLBoolean egl_result;
   void *cookie;
   GX_CLIENT_STATE_T save;

   if (!gx_contexts[image_type])
   {
      GX_LOG("Invalid image type %d", image_type);
      return VCOS_EINVAL;
   }

   h = vcos_calloc(1,sizeof(*h), "graphics_x_resource");
   if (!h)
   {
      GX_LOG("%s: no memory for resource", __FUNCTION__);
      return VCOS_ENOMEM;
   }

   // now need to get the native window
   rc = gx_priv_create_native_window(screen_id,
                                     width, height, image_type,
                                     &h->u.native_window,
                                     &cookie);
   if (rc < 0)
   {
      GX_LOG("%s: could not create native window", __FUNCTION__);
      status = VCOS_ENOMEM;
      goto fail_create_native_win;
   }

   h->magic = RES_MAGIC;
   h->type  = GX_WINDOW;
   h->alpha = 1.0;

   h->surface = eglCreateWindowSurface(display.disp, gx_configs[image_type], &h->u.native_window.egl_win, NULL);
   if (!h->surface)
   {
      GX_LOG("Could not create window surface: 0x%x", eglGetError());
      status = VCOS_ENOMEM;
      goto fail_win;
   }

   egl_result = eglSurfaceAttrib(display.disp, h->surface,
      EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED);
   vcos_assert(egl_result);

   h->context   = gx_contexts[image_type];
   h->screen_id = screen_id;
   h->width     = width;
   h->height    = height;
   h->restype   = image_type;

   gx_priv_save(&save, h);

   // fill it with black
   status = gx_priv_resource_fill(h, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0xff));
   vcos_assert(status == VCOS_SUCCESS);

   gx_priv_finish_native_window(h, cookie);
   gx_priv_flush(h);

   *resource_handle = h;
   gx_priv_restore(&save);
   return status;

fail_win:
   gx_priv_destroy_native_window(h);
fail_create_native_win:
   vcos_free(h);
   return status;
}

/*****************************************************************************/
int32_t graphics_delete_resource( GRAPHICS_RESOURCE_HANDLE res )
{
   EGLBoolean result;

   if (!res)
   {
      // let it slide - mimics old behaviour
      return 0;
   }
   GX_TRACE("delete resource @%p", res);

   vcos_assert(res->magic == RES_MAGIC);

   if (res->type == GX_PBUFFER)
   {
      GX_CLIENT_STATE_T save;
      gx_priv_save(&save, res);
      vgDestroyImage(res->u.pixmap);
      vcos_assert(vgGetError() == 0);
      gx_priv_restore(&save);
   }

   GX_TRACE("graphics_delete_resource: calling eglDestroySurface...");
   result = eglDestroySurface(display.disp, res->surface);
   vcos_assert(result);

   GX_TRACE("graphics_delete_resource: calling eglWaitClient...");
   eglWaitClient(); // wait for EGL to finish sorting out its surfaces

   if (res->type == GX_WINDOW)
   {
      GX_TRACE("graphics_delete_resource: calling gx_priv_destroy_native_window...");
      gx_priv_destroy_native_window(res);
   }

   res->magic = ~RES_MAGIC;
   vcos_free(res);
   GX_TRACE("graphics_delete_resource: done");

   return 0;
}

/*****************************************************************************/
int32_t graphics_update_displayed_resource(GRAPHICS_RESOURCE_HANDLE res,
                                           const uint32_t x_offset,
                                           const uint32_t y_offset,
                                           const uint32_t width,
                                           const uint32_t height )
{
   GX_CLIENT_STATE_T save;
   gx_priv_save(&save, res);

   gx_priv_flush(res);

   gx_priv_restore(&save);

   return 0;
}

/*****************************************************************************/
int32_t graphics_resource_fill(GRAPHICS_RESOURCE_HANDLE res,
                               uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height,
                               uint32_t fill_colour )
{
   GX_CLIENT_STATE_T save;
   gx_priv_save(&save, res);

   VCOS_STATUS_T st = gx_priv_resource_fill(
      res,
      x, res->height-y-height,
      width, height,
      fill_colour);

   gx_priv_restore(&save);

   return st == VCOS_SUCCESS ? 0 : -1;
}

/*****************************************************************************/
int32_t graphics_resource_render_text_ext( GRAPHICS_RESOURCE_HANDLE res,
                                           const int32_t x,
                                           const int32_t y,
                                           const uint32_t width,
                                           const uint32_t height,
                                           const uint32_t fg_colour,
                                           const uint32_t bg_colour,
                                           const char *text,
                                           const uint32_t text_length,
                                           const uint32_t text_size )
{

   /*
   * FIXME: Not at all optimal - re-renders each time.
   * FIXME: Not UTF-8 safe
   * FIXME: much better caching (or any caching)
   */
   VCOS_STATUS_T rc = gx_priv_render_text(
      &display, res,
      x, res->height-y-text_size, width, height, fg_colour, bg_colour,
      text, text_length, text_size);

   return (rc == VCOS_SUCCESS) ? 0 : -1;
}

/*****************************************************************************/
int32_t graphics_resource_render_text(  GRAPHICS_RESOURCE_HANDLE res,
                                        const int32_t x,
                                        const int32_t y,
                                        const uint32_t width, /* this can be GRAPHICS_RESOURCE_WIDTH for no clipping */
                                        const uint32_t height, /* this can be GRAPHICS_RESOURCE_HEIGHT for no clipping */
                                        const uint32_t fg_colour,
                                        const uint32_t bg_colour,
                                        const char *text,
                                        const uint32_t text_length)
{
   return graphics_resource_render_text_ext(res, x, y, width, height,
                                            fg_colour, bg_colour,
                                            text, text_length,
                                            ATEXT_FONT_SIZE);
}

/*****************************************************************************/
int32_t graphics_get_resource_size(
   const GRAPHICS_RESOURCE_HANDLE res,
   uint32_t *w,
   uint32_t *h)
{
   if (w) *w = res->width;
   if (h) *h = res->height;
   return 0;
}

/*****************************************************************************/
int32_t graphics_get_resource_type(const GRAPHICS_RESOURCE_HANDLE res, GRAPHICS_RESOURCE_TYPE_T *type)
{
   if (type) *type = res->restype;
   return 0;
}

/*****************************************************************************/
int32_t graphics_bitblt( const GRAPHICS_RESOURCE_HANDLE src,
                         const uint32_t x, // offset within source
                         const uint32_t y, // offset within source
                         const uint32_t width,
                         const uint32_t height,
                         GRAPHICS_RESOURCE_HANDLE dest,
                         const uint32_t x_pos,
                         const uint32_t y_pos )
{
   int rc = -1;
   VGfloat old[9];
   uint32_t w, h;
   VGPaint paint = VG_INVALID_HANDLE;
   GX_CLIENT_STATE_T save;
   int is_child = 0;
   VGImage img = VG_INVALID_HANDLE;

   gx_priv_save(&save, dest);

   if (src->type != GX_PBUFFER)
   {
      vcos_assert(0);
      goto finish;
   }

   // create a child image that contains just the part wanted
   w = width == GRAPHICS_RESOURCE_WIDTH ? src->width : width;
   h = height == GRAPHICS_RESOURCE_HEIGHT ? src->height : height;

   if (x==0 && y==0 &&
       w == src->width &&
       h == src->height)
   {
      img = src->u.pixmap;
   }
   else
   {
      is_child = 1;
      img = vgChildImage(src->u.pixmap, x, y, w, h);
      if (img == VG_INVALID_HANDLE)
      {
         vcos_assert(0);
         goto finish;
      }
   }

   vcos_assert(vgGetError()==0);

   vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
   vgGetMatrix(old);
   vgLoadIdentity();
   vgTranslate((VGfloat)x_pos, (VGfloat)(dest->height-y_pos));
   vgScale(1.0, -1.0);

   // Do we have a translucency going on?
   if (src->alpha != 1.0)
   {
      VGfloat colour[4] = {1.0,1.0,1.0,src->alpha};
      paint = vgCreatePaint();

      vgSetParameterfv(paint, VG_PAINT_COLOR, 4, colour);
      vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
      vgSetPaint(paint, VG_STROKE_PATH | VG_FILL_PATH);
   }
   vcos_assert(vgGetError()==0);

   vgDrawImage(img);
   vcos_assert(vgGetError()==0);
   vgLoadMatrix(old);

   int err = vgGetError();

   if (err)
   {
      GX_LOG("vg error %x blitting area", err);
      vcos_assert(0);
      rc = -1;
   }
   else
   {
      rc = 0;
   }
finish:
   if (paint != VG_INVALID_HANDLE)
      vgDestroyPaint(paint);

   if (is_child)
      vgDestroyImage(img);

   gx_priv_restore(&save);
   return rc;
}

void gx_priv_flush(GRAPHICS_RESOURCE_HANDLE res)
{
   EGLBoolean result;
   result = eglSwapBuffers(display.disp, res->surface);
   vcos_assert(result);
}

/** Map a colour, which the client will have supplied in RGB888.
 */

void gx_priv_colour_to_paint(uint32_t col, VGfloat *rgba)
{
   // with OpenVG we use RGB order.
   rgba[0] = ((VGfloat)((col & R_888_MASK) >> 16 )) / 0xff;
   rgba[1] = ((VGfloat)((col & G_888_MASK) >> 8 )) / 0xff;
   rgba[2] = ((VGfloat)((col & B_888_MASK) >> 0 )) / 0xff;
   rgba[3] = ((VGfloat)((col & ALPHA_888_MASK) >> 24)) / 0xff;
}

/** Fill an area of a surface with a fixed colour.
  */
VCOS_STATUS_T gx_priv_resource_fill(GRAPHICS_RESOURCE_HANDLE res,
                               uint32_t x,
                               uint32_t y,
                               uint32_t width,
                               uint32_t height,
                               uint32_t fill_colour )
{
   VGfloat vg_clear_colour[4];

   gx_priv_colour_to_paint(fill_colour, vg_clear_colour);
   vgSeti(VG_SCISSORING, VG_FALSE);

   vgSetfv(VG_CLEAR_COLOR, 4, vg_clear_colour);
   vgClear(x, y, width, height);

   int err = vgGetError();
   if (err)
   {
      GX_LOG("vg error %x filling area", err);
      vcos_assert(0);
   }

   return VCOS_SUCCESS;
}

VCOS_STATUS_T gx_priv_get_pixels(const GRAPHICS_RESOURCE_HANDLE res, void **p_pixels, GX_RASTER_ORDER_T raster_order)
{
   VCOS_STATUS_T status = VCOS_SUCCESS;
   void *pixels, *dest;
   uint32_t width, height;
   int data_size, pitch;
   VGImageFormat image_format;

   if (!p_pixels)
   {
      status = VCOS_EINVAL;
      goto finish;
   }

   GX_TRACE("%s: res %p", __FUNCTION__, res);

   graphics_get_resource_size(res, &width, &height);

   /* FIXME: implement e.g. gx_get_pitch */
   switch (res->restype)
   {
      case GRAPHICS_RESOURCE_RGB565:
         pitch = ((width + 31)&(~31)) << 1;
         break;
      case GRAPHICS_RESOURCE_RGB888:
      case GRAPHICS_RESOURCE_RGBA32:
         pitch = ((width + 31)&(~31)) << 2;
         break;
      default:
      {
         GX_LOG("Unsupported pixel format");
         status = VCOS_EINVAL;
         goto finish;
      }
   }

   data_size = pitch * height;

   /* NB: vgReadPixels requires that the data pointer is aligned, but does not
      require the stride to be aligned. Most implementations probably will
      require that as well though... */
   pixels = vcos_malloc(data_size, "gx_get_pixels data");
   if (!pixels)
   {
      GX_LOG("Could not allocate %d bytes for vgReadPixels", data_size);
      status = VCOS_ENOMEM;
      goto finish;
   }
   /* FIXME: introduce e.g. GX_COLOR_FORMAT and mapping to VGImageFormat... */

   /* Hand out image data formatted to match OpenGL RGBA format.
    */
   switch (res->restype)
   {
      case GRAPHICS_RESOURCE_RGB565:
         image_format = VG_sBGR_565;
         break;
      case GRAPHICS_RESOURCE_RGB888:
         image_format = VG_sXBGR_8888;
         break;
      case GRAPHICS_RESOURCE_RGBA32:
         image_format = VG_sABGR_8888;
         break;
      default:
      {
         GX_LOG("Unsupported pixel format");
         status = VCOS_EINVAL;
         goto finish;
      }
   }

   /* VG raster order is bottom-to-top */
   if (raster_order == GX_TOP_BOTTOM)
   {
      dest = ((uint8_t*)pixels)+(pitch*(height-1));
      pitch = -pitch;
   }
   else
   {
      dest = pixels;
   }

   vgReadPixels(dest, pitch, image_format, 0, 0, width, height);

   vcos_assert(vgGetError() == 0);

   *p_pixels = pixels;

finish:
   return status;
}

static VCOS_STATUS_T convert_image_type(GRAPHICS_RESOURCE_TYPE_T image_type,
                                        VGImageFormat *vg_image_type,
                                        int *pbytes_per_pixel)
{
   int bytes_per_pixel;

   switch (image_type)
   {
   case GRAPHICS_RESOURCE_RGB565:
      *vg_image_type = VG_sRGB_565;
      bytes_per_pixel = 2;
      break;
   case GRAPHICS_RESOURCE_RGB888:
      *vg_image_type = VG_sRGBX_8888;
      bytes_per_pixel = 3; // 24 bpp
      break;
   case GRAPHICS_RESOURCE_RGBA32:
      *vg_image_type = VG_sARGB_8888;
      bytes_per_pixel = 4;
      break;
   default:
      vcos_assert(0);
      *vg_image_type = 0;
      return VCOS_EINVAL;
   }
   if (pbytes_per_pixel)
      *pbytes_per_pixel = bytes_per_pixel;

   return VCOS_SUCCESS;
}

/*****************************************************************************/
VCOS_STATUS_T gx_create_pbuffer( uint32_t width,
                                uint32_t height,
                                GRAPHICS_RESOURCE_TYPE_T image_type,
                                GRAPHICS_RESOURCE_HANDLE *resource_handle )
{
   VCOS_STATUS_T status = VCOS_SUCCESS;
   GRAPHICS_RESOURCE_HANDLE h;
   VGImage image;
   VGImageFormat vg_image_type;
   GX_CLIENT_STATE_T save;

   h = vcos_calloc(1,sizeof(*h), "graphics_x_resource");
   if (!h)
   {
      GX_LOG("%s: no memory for resource", __FUNCTION__);
      return VCOS_ENOMEM;
   }

   status = convert_image_type(image_type, &vg_image_type, NULL);
   if (status != VCOS_SUCCESS)
   {
      vcos_free(h);
      return status;
   }

   h->magic     = RES_MAGIC;
   h->context   = gx_contexts[image_type];
   h->config    = gx_configs[image_type];
   h->alpha     = 1.0;
   h->type      = GX_PBUFFER;
   h->width     = width;
   h->height    = height;
   h->restype   = image_type;

   GX_TRACE("Creating pbuffer surface");

   EGLint attribs[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
   h->surface = eglCreatePbufferSurface(display.disp, h->config,
                                        attribs);
   if (!h->surface)
   {
      GX_LOG("Could not create EGL pbuffer surface: 0x%x", eglGetError());
      vcos_free(h);
      return VCOS_EINVAL;
   }

   gx_priv_save(&save, h);

   image = vgCreateImage(vg_image_type, width, height, VG_IMAGE_QUALITY_BETTER);
   if (image == VG_INVALID_HANDLE)
   {
      GX_LOG("Could not create vg image type %d: vg error 0x%x",
             vg_image_type, vgGetError());
      eglDestroySurface(display.disp, h->surface);
      vcos_free(h);
      status = VCOS_ENOMEM;
      goto finish;
   }

   h->u.pixmap  = image;

   // fill it with black
   status = gx_priv_resource_fill(h, 0, 0, width, height, GRAPHICS_RGBA32(0,0,0,0xff));
   vcos_assert(status == VCOS_SUCCESS);

   *resource_handle = h;
finish:
   gx_priv_restore(&save);
   return status;
}

/*****************************************************************************/
GX_PAINT_T *gx_create_gradient(GRAPHICS_RESOURCE_HANDLE res,
                               uint32_t start_colour,
                               uint32_t end_colour)
{
   // holds  the two colour stops (offset,r,g,b,a).
   VGfloat fill_stops[10];
   GX_CLIENT_STATE_T save;
   VGPaint paint = VG_INVALID_HANDLE;

   gx_priv_save(&save, res);

   paint = vgCreatePaint();
   if (!paint)
   {
      gx_priv_restore(&save);
      vcos_log("Could not create paint: vg %d\n", vgGetError());
      vcos_assert(0);
      goto finish;
   }

   fill_stops[0] = 0.0;
   gx_priv_colour_to_paint(start_colour, fill_stops+1);

   fill_stops[5] = 1.0;
   gx_priv_colour_to_paint(end_colour, fill_stops+6);

   vgSetParameteri(paint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
   vgSetParameterfv(paint, VG_PAINT_COLOR_RAMP_STOPS, 5*2, fill_stops);

finish:
   gx_priv_restore(&save);
   return (GX_PAINT_T*)paint;
}

/*****************************************************************************/
void gx_destroy_paint(GRAPHICS_RESOURCE_HANDLE res, GX_PAINT_T *p)
{
   GX_CLIENT_STATE_T save;
   VGPaint paint = (VGPaint)p;
   gx_priv_save(&save, res);
   vgDestroyPaint(paint);
   gx_priv_restore(&save);
}

/*****************************************************************************/
VCOS_STATUS_T gx_fill_gradient(GRAPHICS_RESOURCE_HANDLE dest,
                               uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               uint32_t radius,
                               GX_PAINT_T *p)
{
   /* Define start and end points of gradient, see OpenVG specification,
      section 9.3.3. */
   VGfloat gradient[4] = {0.0, 0.0, 0.0, 0.0};
   VGPaint paint = (VGPaint)p;
   VGPath path;
   GX_CLIENT_STATE_T save;
   VCOS_STATUS_T status = VCOS_SUCCESS;

   if (!paint)
      return VCOS_EINVAL;

   gx_priv_save(&save, dest);

   if (width == GRAPHICS_RESOURCE_WIDTH)
      width = dest->width;

   if (height == GRAPHICS_RESOURCE_HEIGHT)
      height = dest->height;

   gradient[2] = width;

   vgSetParameterfv(paint, VG_PAINT_LINEAR_GRADIENT, 4, gradient);
   vgSetPaint(paint, VG_FILL_PATH);

   path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_S_32,
                       1.0, 0.0, 8, 8, VG_PATH_CAPABILITY_ALL);
   if (!path)
   {
      status = VCOS_ENOMEM;
      goto finish;
   }

   vguRoundRect(path, (VGfloat)x, (VGfloat)y, (VGfloat)width, (VGfloat)height,
                (VGfloat)radius, (VGfloat)radius);
   vgDrawPath(path, VG_FILL_PATH);
   vgDestroyPath(path);

   vcos_assert(vgGetError() == 0);

finish:
   gx_priv_restore(&save);

   return status;
}

/*****************************************************************************/
VCOS_STATUS_T gx_graphics_init(const char *font_dir)
{
   GX_CLIENT_STATE_T save;
   VCOS_STATUS_T rc;

   gx_priv_save(&save, NULL);

   rc = gx_priv_initialise();
   if (rc == VCOS_SUCCESS)
      rc = gx_priv_font_init(font_dir);

   gx_priv_restore(&save);

   return rc;
}

/*****************************************************************************/
int gx_is_double_buffered(void)
{
   return 1;
}

/*****************************************************************************/
int32_t graphics_userblt(GRAPHICS_RESOURCE_TYPE_T src_type,
                         const void *src_data,
                         const uint32_t src_x,
                         const uint32_t src_y,
                         const uint32_t width,
                         const uint32_t height,
                         const uint32_t pitch,
                         GRAPHICS_RESOURCE_HANDLE dest,
                         const uint32_t x_pos,
                         const uint32_t y_pos )
{
   VCOS_STATUS_T status;
   VGImageFormat vg_src_type;
   int bytes_per_pixel;
   GX_CLIENT_STATE_T save;

   status = convert_image_type(src_type, &vg_src_type, &bytes_per_pixel);
   if (status != VCOS_SUCCESS)
      return status;

   gx_priv_save(&save, dest);

   if (dest->type == GX_PBUFFER)
   {
      vgImageSubData(dest->u.pixmap,
                     src_data,
                     pitch,
                     vg_src_type,
                     x_pos, y_pos, width, height);
   }
   else if (dest->type == GX_WINDOW)
   {
      // need to invert this as VG thinks zero is at the bottom
      // while graphics_x thinks it is at the top.
      vgWritePixels((uint8_t*)src_data + pitch*(height-1),
                    -pitch,
                    vg_src_type,
                    x_pos, dest->height-y_pos-height, width, height);
   }
   else
   {
      vcos_assert(0);
   }

   if (vgGetError() == 0)
      status = VCOS_SUCCESS;
   else
   {
      vcos_assert(0);
      status = VCOS_EINVAL;
   }

   gx_priv_restore(&save);
   return status;
}

/*****************************************************************************/
int32_t graphics_resource_text_dimensions( GRAPHICS_RESOURCE_HANDLE resource_handle,
                                           const char *text,
                                           const uint32_t text_length,
                                           uint32_t *width,
                                           uint32_t *height )
{
   return graphics_resource_text_dimensions_ext(resource_handle, text, text_length, width, height, ATEXT_FONT_SIZE);
}

/*****************************************************************************/
VCOS_STATUS_T gx_render_arrowhead(GRAPHICS_RESOURCE_HANDLE res,
                                  uint32_t tip_x, uint32_t tip_y,
                                  int32_t w, int32_t h,
                                  GX_PAINT_T *p)
{
   VGfloat gradient[4];
   VGPaint paint = (VGPaint)p;
   VGPath path;
   VCOS_STATUS_T status = VCOS_SUCCESS;

   GX_CLIENT_STATE_T save;
   gx_priv_save(&save, res);

   if (!paint)
   {
      vcos_assert(0);
      status = VCOS_EINVAL;
      goto finish;
   }

   gradient[0] = 0.0; gradient[1] = 0.0;
   gradient[2] = w; gradient[2] = 0.0;

   vgSetParameterfv(paint, VG_PAINT_LINEAR_GRADIENT, 4, gradient);
   vgSetPaint(paint, VG_FILL_PATH);

   path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_S_32,
                       1.0, 0.0, 8, 8, VG_PATH_CAPABILITY_ALL);
   if (!path)
   {
      status = VCOS_ENOMEM;
      goto finish;
   }
   VGfloat points[] = {
      (VGfloat)tip_x, (VGfloat)tip_y,
      (VGfloat)tip_x + w, (VGfloat)tip_y + h/2,
      (VGfloat)tip_x + w, (VGfloat)tip_y - h/2,
   };

   vguPolygon(path, points, 3, 1);

   vgDrawPath(path, VG_FILL_PATH);
   vgDestroyPath(path);

   vcos_assert(vgGetError()==0);

finish:
   gx_priv_restore(&save);
   return status;
}

/*****************************************************************************/
int32_t gx_apply_alpha( GRAPHICS_RESOURCE_HANDLE resource_handle,
                        const uint8_t alpha )
{
   vcos_assert(resource_handle);
   if (resource_handle->type != GX_PBUFFER)
   {
      vcos_assert(0);
      return -1;
   }
   resource_handle->alpha = 1.0*alpha/255;
   return 0;
}

/*****************************************************************************/
int32_t graphics_resource_set_alpha_per_colour( GRAPHICS_RESOURCE_HANDLE res,
                                                const uint32_t colour,
                                                const uint8_t alpha )
{
   GX_ERROR("Not implemented yet!");
   return 0;
}

/*****************************************************************************/
VCOS_STATUS_T gx_get_pixels(const GRAPHICS_RESOURCE_HANDLE res, void **pixels)
{
   VCOS_STATUS_T status = VCOS_SUCCESS;
   GX_CLIENT_STATE_T save;
   gx_priv_save(&save, res);

   /* Default to top-top-bottom raster scan order */
   status = gx_priv_get_pixels(res, pixels, GX_TOP_BOTTOM);

   gx_priv_restore(&save);
   return status;
}

/*****************************************************************************/
VCOS_STATUS_T gx_get_pixels_in_raster_order(const GRAPHICS_RESOURCE_HANDLE res,
                                            void **pixels,
                                            GX_RASTER_ORDER_T raster_order)
{
   VCOS_STATUS_T status = VCOS_SUCCESS;
   GX_CLIENT_STATE_T save;
   gx_priv_save(&save, res);

   status = gx_priv_get_pixels(res, pixels, raster_order);

   gx_priv_restore(&save);
   return status;
}

/*****************************************************************************/
void gx_free_pixels(const GRAPHICS_RESOURCE_HANDLE res, void *pixels)
{
   vcos_free(pixels);
}

VCOS_STATUS_T gx_bind_vg( GX_CLIENT_STATE_T *save, GRAPHICS_RESOURCE_HANDLE res )
{
   gx_priv_save(save, res);
   vcos_assert(vgGetError()==0);
   return VCOS_SUCCESS;
}

/** Unbind VG */
void gx_unbind_vg(GX_CLIENT_STATE_T *restore)
{
   gx_priv_restore(restore);
}

GX_CLIENT_STATE_T *gx_alloc_context(void)
{
   GX_CLIENT_STATE_T *ret = vcos_calloc(1,sizeof(*ret), "gx_client_state");
   return ret;
}

void gx_free_context(GX_CLIENT_STATE_T *state)
{
   vcos_free(state);
}

void gx_convert_colour(uint32_t colour, float *dest)
{
   gx_priv_colour_to_paint(colour, dest);
}

#define MAX_DISPLAY_HANDLES  4

#define CHANGE_LAYER    (1<<0)
#define CHANGE_OPACITY  (1<<1)
#define CHANGE_DEST     (1<<2)
#define CHANGE_SRC      (1<<3)
#define CHANGE_MASK     (1<<4)
#define CHANGE_XFORM    (1<<5)

typedef struct
{
   /** Keep a display handle going for each connected screen (LCD, HDMI). */
   DISPMANX_DISPLAY_HANDLE_T screens[MAX_DISPLAY_HANDLES];
   int refcounts[MAX_DISPLAY_HANDLES];

   //a flag to count the number of dispman starts that have been invoked

   uint32_t dispman_start_count;
   // maintain the single global handle to the update in progress
   DISPMANX_UPDATE_HANDLE_T current_update;

   VCOS_MUTEX_T lock;
} gx_priv_state_t;

static gx_priv_state_t gx;

void gx_priv_init(void)
{
   vcos_mutex_create(&gx.lock,NULL);
}

void gx_priv_destroy(void)
{
   vcos_mutex_delete(&gx.lock);
}

static
int32_t gx_priv_open_screen(uint32_t index, DISPMANX_DISPLAY_HANDLE_T *pscreen)
{
   int ret = -1;
   vcos_mutex_lock(&gx.lock);

   if (gx.refcounts[index] != 0)
   {
      *pscreen = gx.screens[index];
      gx.refcounts[index]++;
      ret = 0;
   }
   else
   {
      DISPMANX_DISPLAY_HANDLE_T h = vc_dispmanx_display_open(index);
      if (h == DISPMANX_NO_HANDLE)
      {
         GX_LOG("Could not open dispmanx display %d", index);
         ret = -1;
         goto finish;
      }
      gx.screens[index] = h;
      gx.refcounts[index] = 1;
      *pscreen = h;
      ret = 0;
   }
finish:
   vcos_mutex_unlock(&gx.lock);
   return ret;
}

static
int32_t gx_priv_release_screen(uint32_t index)
{
   vcos_mutex_lock(&gx.lock);
   gx.refcounts[index]--;
   if (gx.refcounts[index] == 0)
   {
      vc_dispmanx_display_close(gx.screens[index]);
      gx.screens[index] = DISPMANX_NO_HANDLE;
   }
   vcos_mutex_unlock(&gx.lock);
   return 0;
}

int gx_priv_create_native_window(uint32_t screen_id,
                                 uint32_t w, uint32_t h,
                                 GRAPHICS_RESOURCE_TYPE_T type,
                                 GX_NATIVE_WINDOW_T *win,
                                 void **cookie)
{
   int rc;
   DISPMANX_DISPLAY_HANDLE_T dispmanx_display;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;
   DISPMANX_UPDATE_HANDLE_T current_update;
   *cookie = NULL;

   rc = gx_priv_open_screen(screen_id, &dispmanx_display);
   if (rc < 0)
   {
      GX_LOG("Could not open display %d", screen_id);
      goto fail_screen;
   }

   current_update = vc_dispmanx_update_start(0);
   if (!current_update)
   {
      GX_LOG("Could not start update on screen %d", screen_id);
      goto fail_update;
   }

   src_rect.x = src_rect.y = 0;
   src_rect.width = w << 16;
   src_rect.height = h << 16;

   dst_rect.x = dst_rect.y = 0;
   dst_rect.width = dst_rect.height = 1;

   win->egl_win.width = w;
   win->egl_win.height = h;
   VC_DISPMANX_ALPHA_T alpha;
   memset(&alpha, 0x0, sizeof(VC_DISPMANX_ALPHA_T));
   alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;

   DISPMANX_CLAMP_T clamp;
   memset(&clamp, 0x0, sizeof(DISPMANX_CLAMP_T));

   win->egl_win.element = vc_dispmanx_element_add(current_update, dispmanx_display,
      0 /* layer */, &dst_rect,
      0 /* src */, &src_rect,
      DISPMANX_PROTECTION_NONE,
      &alpha /* alpha */,
      &clamp /* clamp */,
      0 /* transform */);

   if ( !win->egl_win.element )
   {
      GX_LOG("Could not add element %dx%d",w,h);
      vc_dispmanx_update_submit_sync(current_update);
      rc = -1;
   }

   // have to pass back the update so it can be completed *After* the
   // window has been initialised (filled with background colour).
   *cookie = (void*)current_update;

   return 0;

fail_update:
   gx_priv_release_screen(screen_id);
fail_screen:
   return rc;
}

void gx_priv_finish_native_window(GRAPHICS_RESOURCE_HANDLE_TABLE_T *res,
                                  void *current_update)
{
   vc_dispmanx_update_submit_sync((DISPMANX_UPDATE_HANDLE_T)current_update);
}

void
gx_priv_destroy_native_window(GRAPHICS_RESOURCE_HANDLE_TABLE_T *res)
{
   DISPMANX_UPDATE_HANDLE_T current_update;

   if((current_update = vc_dispmanx_update_start(0)) != 0)
   {
      int ret = vc_dispmanx_element_remove(current_update, res->u.native_window.egl_win.element);
      vcos_assert(ret == 0);
      ret = vc_dispmanx_update_submit_sync(current_update);
      vcos_assert(ret == 0);
   }

   gx_priv_release_screen(res->screen_id);
}

/***********************************************************
 * Name: graphics_get_display_size
 *
 * Arguments:
 *       void
 *
 * Description: Return size of display
 *
 * Returns: int32_t:
 *               >=0 if it succeeded
 *
 ***********************************************************/
int32_t graphics_get_display_size( const uint16_t display_number,
                                   uint32_t *width,
                                   uint32_t *height)
{
   DISPMANX_MODEINFO_T mode_info;
   int32_t success = -1;
   DISPMANX_DISPLAY_HANDLE_T disp;
   vcos_assert(width && height);
   *width = *height = 0;

   if(vcos_verify(display_number < MAX_DISPLAY_HANDLES))
   {
      // TODO Shouldn't this close the display if it wasn't previously open?
      if (gx_priv_open_screen(display_number, &disp) < 0)
      {
         vcos_assert(0);
         return -1;
      }
      success = vc_dispmanx_display_get_info(disp, &mode_info);

      if( success >= 0 )
      {
         *width = mode_info.width;
         *height = mode_info.height;
         vcos_assert(*height > 64);
      }
      else
      {
         vcos_assert(0);
      }
   }

   return success;
}

static inline uint16_t auto_size(uint16_t arg, uint16_t actual_size)
{
   return arg == GRAPHICS_RESOURCE_WIDTH ? actual_size : arg;
}

int32_t graphics_display_resource( GRAPHICS_RESOURCE_HANDLE res,
                                   const uint16_t screen_number,
                                   const int16_t z_order,
                                   const uint16_t offset_x,
                                   const uint16_t offset_y,
                                   const uint16_t dest_width,
                                   const uint16_t dest_height,
                                   const VC_DISPMAN_TRANSFORM_T transform,
                                   const uint8_t display )
{
   DISPMANX_UPDATE_HANDLE_T update;
   int32_t rc;
   int xform_changed;

   if (!res)
   {
      // mimics old behaviour.
      (void)vcos_verify(0);
      return 0;
   }
   vcos_assert(res->magic == RES_MAGIC);

   xform_changed = transform != res->transform;
   res->transform = transform;

   rc = graphics_update_start();
   update = gx.current_update;
   vcos_assert(rc == 0);

   if (display)
   {
      VC_RECT_T src_rect, dest_rect;

      int32_t src_width = res->width;
      int32_t src_height = res->height;

      uint32_t change_flags = CHANGE_LAYER;

      // has the destination position changed?
      uint32_t w = auto_size(dest_width, res->width);
      uint32_t h = auto_size(dest_height, res->height);

      vcos_assert(screen_number == res->screen_id);

      if (gx.screens[screen_number] == 0)
      {
         vcos_assert(0);
         DISPMANX_DISPLAY_HANDLE_T display_handle;
         gx_priv_open_screen(screen_number, &display_handle);
      }

      if ((offset_x != res->dest.x) ||
          (offset_y != res->dest.y) ||
          (h != res->dest.height) ||
          (w != res->dest.width))
      {
         change_flags |= CHANGE_DEST;
         res->dest.x      = offset_x;
         res->dest.y      = offset_y;
         res->dest.height = h;
         res->dest.width  = w;
      }

      if (xform_changed)
         change_flags |= CHANGE_XFORM;

      vc_dispmanx_rect_set( &src_rect, 0, 0, ((uint32_t)src_width)<<16, ((uint32_t)src_height)<<16 );
      vc_dispmanx_rect_set( &dest_rect, offset_x, offset_y, w, h);

      rc = vc_dispmanx_element_change_attributes(update,
         res->u.native_window.egl_win.element,
         change_flags,
         z_order, /* layer */
         0xff, /* opacity */
         &dest_rect,
         &src_rect,
         0, transform);

      vcos_assert(rc==0);
      gx_priv_flush(res);

   }
   else
   {
      vgFinish();
      eglWaitClient();
      rc = vc_dispmanx_element_change_source(update, res->u.native_window.egl_win.element, 0);
      vcos_assert(rc==0);
   }

   rc = graphics_update_end();
   vcos_assert(rc==0);

   return rc;
}

/***********************************************************
 * Name: graphics_update_start
 *
 * Arguments:
 *       void
 *
 * Description: Starts an update UNLESS and update is already in progress
 *
 * Returns: int32_t:
 *               >=0 if it succeeded
 *
 ***********************************************************/
int32_t graphics_update_start(void)
{
   int32_t success = 0;

   //check we are not already in an update
   if ( 0 == gx.dispman_start_count )
   {
      gx.current_update = vc_dispmanx_update_start( 10 );
      if( gx.current_update == DISPMANX_NO_HANDLE )
      {
         //error
         success = -1;
         vc_assert( 0 );
      }
   }

   if( success == 0 )
   {
      //inc the counter
      gx.dispman_start_count++;
   }

   return success;
}

/***********************************************************
 * Name: graphics_update_end
 *
 * Arguments:
 *       void
 *
 * Description: Ends an update UNLESS more than one update is in progress
 *
 * Returns: int32_t:
 *               >=0 if it succeeded
 *
 ***********************************************************/
int32_t graphics_update_end( void )
{
   int32_t success = -1;

   // make sure you are checking the return value of graphics_update_start
   if(vcos_verify(gx.current_update != DISPMANX_NO_HANDLE))
   {
      //check we are in an update
      if(vcos_verify(gx.dispman_start_count > 0))
      {
         //dec the counter
         gx.dispman_start_count--;

         success = 0;

         //is the counter now 0?
         if( 0 == gx.dispman_start_count )
         {
            eglWaitClient();
            if( vc_dispmanx_update_submit_sync( gx.current_update ) != 0 )
            {
               //error
               success = -1;
               vc_assert( 0 );
            }
         }
      }
   }

   return success;
}
