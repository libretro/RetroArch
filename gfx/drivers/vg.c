/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <math.h>
#include <string.h>

#include <VG/openvg.h>
#include <VG/vgext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <retro_inline.h>
#include <retro_assert.h>
#include <gfx/math/matrix_3x3.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../retroarch.h"
#include "../../driver.h"
#include "../../content.h"
#include "../../verbosity.h"
#include "../../configuration.h"

typedef struct
{
   bool should_resize;
   bool keep_aspect;
   bool mEglImageBuf;
   bool mFontsOn;

   float mScreenAspect;

   unsigned mTextureWidth;
   unsigned mTextureHeight;
   unsigned mRenderWidth;
   unsigned mRenderHeight;
   unsigned x1, y1, x2, y2;
   uint32_t mFontHeight;

   char *mLastMsg;

   VGint scissor[4];
   VGImageFormat mTexType;
   VGImage mImage;
   math_matrix_3x3 mTransformMatrix;
   EGLImageKHR last_egl_image;

   VGFont mFont;
   void *mFontRenderer;
   const font_renderer_driver_t *font_driver;
   VGuint mMsgLength;
   VGuint mGlyphIndices[1024];
   VGPaint mPaintFg;
   VGPaint mPaintBg;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
} vg_t;

static PFNVGCREATEEGLIMAGETARGETKHRPROC pvgCreateEGLImageTargetKHR;

static void vg_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   vg_t *vg     = (vg_t*)data;
   int interval = state ? 0 : 1;

   if (vg->ctx_driver && vg->ctx_driver->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      vg->ctx_driver->swap_interval(vg->ctx_data, interval);
   }
}

static INLINE bool vg_query_extension(const char *ext)
{
   const char *str = (const char*)vgGetString(VG_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("[VG]: Querying VG extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static void *vg_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned win_width, win_height;
   VGfloat clearColor[4]           = {0, 0, 0, 1};
   int interval                    = 0;
   unsigned mode_width             = 0;
   unsigned mode_height            = 0;
   unsigned temp_width             = 0;
   unsigned temp_height            = 0;
   void *ctx_data                  = NULL;
   settings_t        *settings     = config_get_ptr();
   const char *path_font           = settings->paths.path_font;
   float video_font_size           = settings->floats.video_font_size;
   float video_msg_color_r         = settings->floats.video_msg_color_r;
   float video_msg_color_g         = settings->floats.video_msg_color_g;
   float video_msg_color_b         = settings->floats.video_msg_color_b;
   vg_t                    *vg     = (vg_t*)calloc(1, sizeof(vg_t));
   const gfx_ctx_driver_t *ctx     = video_context_driver_init_first(
         vg, settings->arrays.video_context_driver,
         GFX_CTX_OPENVG_API, 0, 0, false, &ctx_data);
   bool adaptive_vsync_enabled     = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;

   if (!vg || !ctx)
      goto error;

   if (ctx_data)
      vg->ctx_data = ctx_data;

   vg->ctx_driver = ctx;
   video_context_driver_set((void*)ctx);

   if (vg->ctx_driver->get_video_size)
      vg->ctx_driver->get_video_size(vg->ctx_data,
               &mode_width, &mode_height);

   temp_width  = mode_width;
   temp_height = mode_height;

   RARCH_LOG("[VG]: Detecting screen resolution %ux%u.\n", temp_width, temp_height);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   interval = video->vsync ? 1 : 0;

   if (ctx->swap_interval)
   {
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      ctx->swap_interval(vg->ctx_data, interval);
   }

   vg->mTexType    = video->rgb32 ? VG_sXRGB_8888 : VG_sRGB_565;
   vg->keep_aspect = video->force_aspect;

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      video_driver_get_size(&temp_width, &temp_height);

      win_width  = temp_width;
      win_height = temp_height;
   }

   if (     !vg->ctx_driver->set_video_mode
         || !vg->ctx_driver->set_video_mode(vg->ctx_data,
            win_width, win_height, video->fullscreen))
      goto error;

   video_driver_get_size(&temp_width, &temp_height);

   temp_width        = 0;
   temp_height       = 0;
   mode_width        = 0;
   mode_height       = 0;

   if (vg->ctx_driver->get_video_size)
      vg->ctx_driver->get_video_size(vg->ctx_data,
               &mode_width, &mode_height);

   temp_width        = mode_width;
   temp_height       = mode_height;

   vg->should_resize = true;

   if (temp_width != 0 && temp_height != 0)
   {
      RARCH_LOG("[VG]: Verified window resolution %ux%u.\n",
            temp_width, temp_height);
      video_driver_set_size(temp_width, temp_height);
   }

   video_driver_get_size(&temp_width, &temp_height);

   vg->mScreenAspect = (float)temp_width / temp_height;

   if (vg->ctx_driver->translate_aspect)
      vg->mScreenAspect = vg->ctx_driver->translate_aspect(
            vg->ctx_data, temp_width, temp_height);

   vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

   vg->mTextureWidth = vg->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
   vg->mImage        = vgCreateImage(
         vg->mTexType,
         vg->mTextureWidth,
         vg->mTextureHeight,
         video->smooth
         ? VG_IMAGE_QUALITY_BETTER
         : VG_IMAGE_QUALITY_NONANTIALIASED);
   vg_set_nonblock_state(vg, !video->vsync, adaptive_vsync_enabled, interval);

   if (vg->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      vg->ctx_driver->input_driver(
            vg->ctx_data, joypad_name,
            input, input_data);
   }

   if (     video->font_enable
         && font_renderer_create_default(
            &vg->font_driver, &vg->mFontRenderer,
            *path_font ? path_font : NULL,
            video_font_size))
   {
      vg->mFont            = vgCreateFont(0);

      if (vg->mFont != VG_INVALID_HANDLE)
      {
         VGfloat paintFg[4];
         VGfloat paintBg[4];

         vg->mFontsOn      = true;
         vg->mFontHeight   = video_font_size;
         vg->mPaintFg      = vgCreatePaint();
         vg->mPaintBg      = vgCreatePaint();

         paintFg[0]        = video_msg_color_r;
         paintFg[1]        = video_msg_color_g;
         paintFg[2]        = video_msg_color_b;
         paintFg[3]        = 1.0f;

         paintBg[0]        = video_msg_color_r / 2.0f;
         paintBg[1]        = video_msg_color_g / 2.0f;
         paintBg[2]        = video_msg_color_b / 2.0f;
         paintBg[3]        = 0.5f;

         vgSetParameteri(vg->mPaintFg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(vg->mPaintFg, VG_PAINT_COLOR, 4, paintFg);

         vgSetParameteri(vg->mPaintBg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(vg->mPaintBg, VG_PAINT_COLOR, 4, paintBg);
      }
   }

   if (vg_query_extension("KHR_EGL_image")
         && vg->ctx_driver->image_buffer_init
         && vg->ctx_driver->image_buffer_init(vg->ctx_data, (void*)video))
   {
      if (vg->ctx_driver->get_proc_address)
         pvgCreateEGLImageTargetKHR = (PFNVGCREATEEGLIMAGETARGETKHRPROC)vg->ctx_driver->get_proc_address("vgCreateEGLImageTargetKHR");

      if (pvgCreateEGLImageTargetKHR)
      {
         RARCH_LOG("[VG] Using EGLImage buffer\n");
         vg->mEglImageBuf = true;
      }
   }

#if 0
   const char *ext = (const char*)vgGetString(VG_EXTENSIONS);
   if (ext)
      RARCH_LOG("[VG] Supported extensions: %s\n", ext);
#endif

   return vg;

error:
   video_context_driver_free();
   if (vg)
      free(vg);
   return NULL;
}

static void vg_free(void *data)
{
   vg_t                    *vg = (vg_t*)data;

   if (!vg)
      return;

   vgDestroyImage(vg->mImage);

   if (vg->mFontsOn)
   {
      vgDestroyFont(vg->mFont);
      vg->font_driver->free(vg->mFontRenderer);
      vgDestroyPaint(vg->mPaintFg);
      vgDestroyPaint(vg->mPaintBg);
   }

   if (vg->ctx_driver && vg->ctx_driver->destroy)
      vg->ctx_driver->destroy(vg->ctx_data);
   video_context_driver_free();

   free(vg);
}

static void vg_calculate_quad(vg_t *vg,
      unsigned width, unsigned height)
{
   /* set viewport for aspect ratio, taken from the OpenGL driver. */
   if (vg->keep_aspect)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

      /* If the aspect ratios of screen and desired aspect ratio
       * are sufficiently equal (floating point stuff),
       * assume they are actually equal. */
      if (fabs(vg->mScreenAspect - desired_aspect) < 0.0001)
      {
         vg->x1 = 0;
         vg->y1 = 0;
         vg->x2 = width;
         vg->y2 = height;
      }
      else if (vg->mScreenAspect > desired_aspect)
      {
         float delta = (desired_aspect / vg->mScreenAspect - 1.0) / 2.0 + 0.5;
         vg->x1 = width * (0.5 - delta);
         vg->y1 = 0;
         vg->x2 = 2.0 * width * delta + vg->x1;
         vg->y2 = height + vg->y1;
      }
      else
      {
         float delta = (vg->mScreenAspect / desired_aspect - 1.0) / 2.0 + 0.5;
         vg->x1 = 0;
         vg->y1 = height * (0.5 - delta);
         vg->x2 = width + vg->x1;
         vg->y2 = 2.0 * height * delta + vg->y1;
      }
   }
   else
   {
      vg->x1 = 0;
      vg->y1 = 0;
      vg->x2 = width;
      vg->y2 = height;
   }

   vg->scissor[0] = vg->x1;
   vg->scissor[1] = vg->y1;
   vg->scissor[2] = vg->x2 - vg->x1;
   vg->scissor[3] = vg->y2 - vg->y1;

   vgSetiv(VG_SCISSOR_RECTS, 4, vg->scissor);
}

static void vg_copy_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   vg_t *vg = (vg_t*)data;

   if (vg->mEglImageBuf)
   {
      EGLImageKHR img = 0;
      bool new_egl    = false;

      if (vg->ctx_driver->image_buffer_write)
         new_egl      = vg->ctx_driver->image_buffer_write(
               vg->ctx_data,
               frame, width, height, pitch,
               (vg->mTexType == VG_sXRGB_8888),
               0,
               &img);

      retro_assert(img != EGL_NO_IMAGE_KHR);

      if (new_egl)
      {
         vgDestroyImage(vg->mImage);
         vg->mImage = pvgCreateEGLImageTargetKHR((VGeglImageKHR) img);
         if (!vg->mImage)
         {
            RARCH_ERR(
                  "[VG:EGLImage] Error creating image: %08x\n",
                  vgGetError());
            exit(2);
         }
         vg->last_egl_image = img;
      }
   }
   else
      vgImageSubData(vg->mImage, frame, pitch, vg->mTexType, 0, 0, width, height);
}

static bool vg_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count, unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   vg_t                           *vg = (vg_t*)data;
   unsigned width                     = video_info->width;
   unsigned height                    = video_info->height;
#ifdef HAVE_MENU
   bool menu_is_alive                 = video_info->menu_is_alive;
#endif

   if (     frame_width != vg->mRenderWidth
         || frame_height != vg->mRenderHeight
         || vg->should_resize)
   {
      vg->mRenderWidth  = frame_width;
      vg->mRenderHeight = frame_height;
      vg_calculate_quad(vg, width, height);
      matrix_3x3_quad_to_quad(
         vg->x1, vg->y1, vg->x2, vg->y1, vg->x2, vg->y2, vg->x1, vg->y2,
         /* needs to be flipped, Khronos loves their bottom-left origin */
         0, frame_height, frame_width, frame_height, frame_width, 0, 0, 0,
         &vg->mTransformMatrix);
      vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
      vgLoadMatrix(vg->mTransformMatrix.data);

      vg->should_resize = false;
   }

   vgSeti(VG_SCISSORING, VG_FALSE);
   vgClear(0, 0, width, height);
   vgSeti(VG_SCISSORING, VG_TRUE);

   vg_copy_frame(vg, frame, frame_width, frame_height, pitch);

#ifdef HAVE_MENU
   menu_driver_frame(menu_is_alive, video_info);
#endif

   vgDrawImage(vg->mImage);

#if 0
   if (msg && vg->mFontsOn)
      vg_draw_message(vg, msg);
#endif

   if (vg->ctx_driver->update_window_title)
      vg->ctx_driver->update_window_title(vg->ctx_data);

   if (vg->ctx_driver->swap_buffers)
      vg->ctx_driver->swap_buffers(vg->ctx_data);

   return true;
}

static bool vg_alive(void *data)
{
   bool quit            = false;
   bool resize          = false;
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   vg_t            *vg  = (vg_t*)data;

   vg->ctx_driver->check_window(vg->ctx_data,
            &quit, &resize, &temp_width, &temp_height);

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return !quit;
}

static bool vg_suppress_screensaver(void *data, bool enable)
{
   bool enabled         = enable;
   vg_t            *vg  = (vg_t*)data;
   if (vg->ctx_data && vg->ctx_driver->suppress_screensaver)
      return vg->ctx_driver->suppress_screensaver(vg->ctx_data, enabled);
   return false;
}

static bool vg_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }
static void vg_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { }

static bool vg_has_windowed(void *data)
{
   vg_t            *vg  = (vg_t*)data;
   if (vg && vg->ctx_driver)
      return vg->ctx_driver->has_windowed;
   return false;
}

static bool vg_focus(void *data)
{
   vg_t            *vg  = (vg_t*)data;
   if (vg && vg->ctx_driver && vg->ctx_driver->has_focus)
      return vg->ctx_driver->has_focus(vg->ctx_data);
   return true;
}

video_driver_t video_vg = {
   vg_init,
   vg_frame,
   vg_set_nonblock_state,
   vg_alive,
   vg_focus,
   vg_suppress_screensaver,
   vg_has_windowed,
   vg_set_shader,
   vg_free,
   "vg",
   NULL,                      /* set_viewport */
   NULL,                      /* set_rotation */
   NULL,                      /* viewport_info */
   NULL,                      /* read_viewport */
   NULL,                      /* read_frame_raw */
#ifdef HAVE_OVERLAY
  NULL,                       /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  vg_get_poke_interface
};
