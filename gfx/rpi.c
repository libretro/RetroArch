/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Michael Lelli
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

#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <bcm_host.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "../libretro.h"
#include "../general.h"
//#include "../input/linuxraw_input.h"
// SDL include messing with some defines
typedef struct linuxraw_input linuxraw_input_t;
#include "../driver.h"

#ifdef HAVE_FREETYPE
#include "fonts/fonts.h"
#include "../file.h"
#endif


typedef struct {
   EGLDisplay mDisplay;
   EGLSurface mSurface;
   EGLContext mContext;
   uint32_t mScreenWidth;
   uint32_t mScreenHeight;
   float mScreenAspect;
   bool mKeepAspect;
   unsigned mTextureWidth;
   unsigned mTextureHeight;
   unsigned mRenderWidth;
   unsigned mRenderHeight;
   unsigned x1, y1, x2, y2;
   VGImageFormat mTexType;
   VGImage mImage;
   VGfloat mTransformMatrix[9];
   VGint scissor[4];

#ifdef HAVE_FREETYPE
   char *mLastMsg;
   uint32_t mFontHeight;
   VGFont mFont;
   font_renderer_t *mFontRenderer;
   bool mFontsOn;
   VGuint mMsgLength;
   VGuint mGlyphIndices[1024];
   VGPaint mPaintFg;
   VGPaint mPaintBg;
#endif
} rpi_t;

static void rpi_set_nonblock_state(void *data, bool state)
{
   rpi_t *rpi = (rpi_t*)data;
   eglSwapInterval(rpi->mDisplay, state ? 0 : 1);
}

static void *rpi_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   int32_t success;
   EGLBoolean result;
   EGLint num_config;
   rpi_t *rpi = (rpi_t*)calloc(1, sizeof(rpi_t));
   *input = NULL;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   DISPMANX_MODEINFO_T dispman_modeinfo;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };

   EGLConfig config;

   bcm_host_init();

   // get an EGL display connection
   rpi->mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(rpi->mDisplay != EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(rpi->mDisplay, NULL, NULL);
   assert(result != EGL_FALSE);
   eglBindAPI(EGL_OPENVG_API);

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(rpi->mDisplay, attribute_list, &config, 1, &num_config);
   assert(result != EGL_FALSE);

   // create an EGL rendering context
   rpi->mContext = eglCreateContext(rpi->mDisplay, config, EGL_NO_CONTEXT, NULL);
   assert(rpi->mContext != EGL_NO_CONTEXT);

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &rpi->mScreenWidth, &rpi->mScreenHeight);
   assert(success >= 0);

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = rpi->mScreenWidth;
   dst_rect.height = rpi->mScreenHeight;

   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = rpi->mScreenWidth << 16;
   src_rect.height = rpi->mScreenHeight << 16;

   dispman_display = vc_dispmanx_display_open(0 /* LCD */);
   vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);
   dispman_update = vc_dispmanx_update_start(0);

   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, DISPMANX_NO_ROTATE);

   nativewindow.element = dispman_element;
   nativewindow.width = rpi->mScreenWidth;
   nativewindow.height = rpi->mScreenHeight;
   vc_dispmanx_update_submit_sync(dispman_update);

   rpi->mSurface = eglCreateWindowSurface(rpi->mDisplay, config, &nativewindow, NULL);
   assert(rpi->mSurface != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(rpi->mDisplay, rpi->mSurface, rpi->mSurface, rpi->mContext);
   assert(result != EGL_FALSE);

   rpi->mTexType = video->rgb32 ? VG_sABGR_8888 : VG_sARGB_1555;
   rpi->mKeepAspect = video->force_aspect;

   // check for SD televisions: they should always be 4:3
   if (dispman_modeinfo.width == 720 && (dispman_modeinfo.height == 480 || dispman_modeinfo.height == 576))
      rpi->mScreenAspect = 4.0f / 3.0f;
   else
      rpi->mScreenAspect = (float) dispman_modeinfo.width / dispman_modeinfo.height;

   VGfloat clearColor[4] = {0, 0, 0, 1};
   vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

   rpi->mTextureWidth = rpi->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
   // We can't use the native format because there's no sXRGB_1555 type and
   // emulation cores can send 0 in the top bit. We lose some speed on
   // conversion but I doubt it has any real affect, since we are only drawing
   // one image at the end of the day. Still keep the alpha channel for ABGR.
   rpi->mImage = vgCreateImage(video->rgb32 ? VG_sABGR_8888 : VG_sXBGR_8888, rpi->mTextureWidth, rpi->mTextureHeight, video->smooth ? VG_IMAGE_QUALITY_BETTER : VG_IMAGE_QUALITY_NONANTIALIASED);
   rpi_set_nonblock_state(rpi, !video->vsync);

   linuxraw_input_t *linuxraw_input = (linuxraw_input_t *)input_linuxraw.init();
   if (linuxraw_input)
   {
      *input = (const input_driver_t *)&input_linuxraw;
      *input_data = linuxraw_input;
   }

#ifdef HAVE_FREETYPE
   if (g_settings.video.font_enable)
   {
      rpi->mFont = vgCreateFont(0);
      rpi->mFontHeight = g_settings.video.font_size * (g_settings.video.font_scale ? (float) rpi->mScreenWidth / 1280.0f : 1.0f);

      const char *path = g_settings.video.font_path;
      if (!*path || !path_file_exists(path))
         path = font_renderer_get_default_font();

      rpi->mFontRenderer = font_renderer_new(path, rpi->mFontHeight);

      if (rpi->mFont != VG_INVALID_HANDLE && rpi->mFontRenderer)
      {
         rpi->mFontsOn = true;

         rpi->mPaintFg = vgCreatePaint();
         rpi->mPaintBg = vgCreatePaint();
         VGfloat paintFg[] = { g_settings.video.msg_color_r, g_settings.video.msg_color_g, g_settings.video.msg_color_b, 1.0f };
         VGfloat paintBg[] = { g_settings.video.msg_color_r / 2.0f, g_settings.video.msg_color_g / 2.0f, g_settings.video.msg_color_b / 2.0f, 0.5f };

         vgSetParameteri(rpi->mPaintFg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(rpi->mPaintFg, VG_PAINT_COLOR, 4, paintFg);

         vgSetParameteri(rpi->mPaintBg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
         vgSetParameterfv(rpi->mPaintBg, VG_PAINT_COLOR, 4, paintBg);
      }
   }
#endif

   return rpi;
}

static void rpi_free(void *data)
{
   rpi_t *rpi = (rpi_t*)data;

   vgDestroyImage(rpi->mImage);

#ifdef HAVE_FREETYPE
   if (rpi->mFontsOn)
   {
      vgDestroyFont(rpi->mFont);
      font_renderer_free(rpi->mFontRenderer);
      vgDestroyPaint(rpi->mPaintFg);
      vgDestroyPaint(rpi->mPaintBg);
   }
#endif

   // Release EGL resources
   eglMakeCurrent(rpi->mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroySurface(rpi->mDisplay, rpi->mSurface);
   eglDestroyContext(rpi->mDisplay, rpi->mContext);
   eglTerminate(rpi->mDisplay);

   free(rpi);
}

#ifdef HAVE_FREETYPE

static void rpi_render_message(rpi_t *rpi, const char *msg)
{
   free(rpi->mLastMsg);
   rpi->mLastMsg = strdup(msg);

   if(rpi->mMsgLength)
   {
      while (--rpi->mMsgLength)
         vgClearGlyph(rpi->mFont, rpi->mMsgLength);

      vgClearGlyph(rpi->mFont, 0);
   }

   struct font_output_list out;
   font_renderer_msg(rpi->mFontRenderer, msg, &out);
   struct font_output *head = out.head;

   while (head)
   {
      if (rpi->mMsgLength >= 1024)
         break;

      VGfloat origin[2], escapement[2];
      VGImage img;

      escapement[0] = (VGfloat) (head->advance_x);
      escapement[1] = (VGfloat) (head->advance_y);
      origin[0] = (VGfloat) (-head->char_off_x);
      origin[1] = (VGfloat) (head->char_off_y);

      img = vgCreateImage(VG_A_8, head->width, head->height, VG_IMAGE_QUALITY_NONANTIALIASED);

      // flip it
      for (unsigned i = 0; i < head->height; i++)
         vgImageSubData(img, head->output + head->pitch * i, head->pitch, VG_A_8, 0, head->height - i - 1, head->width, 1);

      vgSetGlyphToImage(rpi->mFont, rpi->mMsgLength, img, origin, escapement);
      vgDestroyImage(img);

      rpi->mMsgLength++;
      head = head->next;
   }

   font_renderer_free_output(&out);

   for (unsigned i = 0; i < rpi->mMsgLength; i++)
      rpi->mGlyphIndices[i] = i;
}

static void rpi_draw_message(rpi_t *rpi, const char *msg)
{
   if (!rpi->mLastMsg || strcmp(rpi->mLastMsg, msg))
      rpi_render_message(rpi, msg);

   vgSeti(VG_SCISSORING, VG_FALSE);
   vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_STENCIL);

   VGfloat origins[] = { rpi->mScreenWidth * g_settings.video.msg_pos_x - 2.0f, rpi->mScreenHeight * g_settings.video.msg_pos_y - 2.0f };
   vgSetfv(VG_GLYPH_ORIGIN, 2, origins);
   vgSetPaint(rpi->mPaintBg, VG_FILL_PATH);
   vgDrawGlyphs(rpi->mFont, rpi->mMsgLength, rpi->mGlyphIndices, NULL, NULL, VG_FILL_PATH, VG_TRUE);
   origins[0] += 2.0f;
   origins[1] += 2.0f;
   vgSetfv(VG_GLYPH_ORIGIN, 2, origins);
   vgSetPaint(rpi->mPaintFg, VG_FILL_PATH);
   vgDrawGlyphs(rpi->mFont, rpi->mMsgLength, rpi->mGlyphIndices, NULL, NULL, VG_FILL_PATH, VG_TRUE);

   vgSeti(VG_SCISSORING, VG_TRUE);
   vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
}

#endif

static void rpi_calculate_quad(rpi_t *rpi)
{
   // set viewport for aspect ratio, taken from the OpenGL driver
   if (rpi->mKeepAspect)
   {
      float desired_aspect = g_settings.video.aspect_ratio;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff),
      // assume they are actually equal.
      if (fabs(rpi->mScreenAspect - desired_aspect) < 0.0001)
      {
         rpi->x1 = 0;
         rpi->y1 = 0;
         rpi->x2 = rpi->mScreenWidth;
         rpi->y2 = rpi->mScreenHeight;
      }
      else if (rpi->mScreenAspect > desired_aspect)
      {
         float delta = (desired_aspect / rpi->mScreenAspect - 1.0) / 2.0 + 0.5;
         rpi->x1 = rpi->mScreenWidth * (0.5 - delta);
         rpi->y1 = 0;
         rpi->x2 = 2.0 * rpi->mScreenWidth * delta + rpi->x1;
         rpi->y2 = rpi->mScreenHeight + rpi->y1;
      }
      else
      {
         float delta = (rpi->mScreenAspect / desired_aspect - 1.0) / 2.0 + 0.5;
         rpi->x1 = 0;
         rpi->y1 = rpi->mScreenHeight * (0.5 - delta);
         rpi->x2 = rpi->mScreenWidth + rpi->x1;
         rpi->y2 = 2.0 * rpi->mScreenHeight * delta + rpi->y1;
      }
   }
   else
   {
      rpi->x1 = 0;
      rpi->y1 = 0;
      rpi->x2 = rpi->mScreenWidth;
      rpi->y2 = rpi->mScreenHeight;
   }

   rpi->scissor[0] = rpi->x1;
   rpi->scissor[1] = rpi->y1;
   rpi->scissor[2] = rpi->x2 - rpi->x1;
   rpi->scissor[3] = rpi->y2 - rpi->y1;

   vgSetiv(VG_SCISSOR_RECTS, 4, rpi->scissor);
}

static bool rpi_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   rpi_t *rpi = (rpi_t*)data;

   if (width != rpi->mRenderWidth || height != rpi->mRenderHeight)
   {
      rpi->mRenderWidth = width;
      rpi->mRenderHeight = height;
      rpi_calculate_quad(rpi);
      vguComputeWarpQuadToQuad(
         rpi->x1, rpi->y1, rpi->x2, rpi->y1, rpi->x2, rpi->y2, rpi->x1, rpi->y2,
         // needs to be flipped, Khronos loves their bottom-left origin
         0, height, width, height, width, 0, 0, 0,
         rpi->mTransformMatrix);
      vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
      vgLoadMatrix(rpi->mTransformMatrix);
   }
   vgSeti(VG_SCISSORING, VG_FALSE);
   vgClear(0, 0, rpi->mScreenWidth, rpi->mScreenHeight);
   vgSeti(VG_SCISSORING, VG_TRUE);

   vgImageSubData(rpi->mImage, frame, pitch, rpi->mTexType, 0, 0, width, height);
   vgDrawImage(rpi->mImage);

#ifdef HAVE_FREETYPE
   if (msg && rpi->mFontsOn)
      rpi_draw_message(rpi, msg);
#else
   (void)msg;
#endif

   eglSwapBuffers(rpi->mDisplay, rpi->mSurface);

   return true;
}

static bool rpi_alive(void *data)
{
   (void)data;
   return true;
}

static bool rpi_focus(void *data)
{
   (void)data;
   return true;
}

static void rpi_set_rotation(void *data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

const video_driver_t video_rpi = {
   rpi_init,
   rpi_frame,
   rpi_set_nonblock_state,
   rpi_alive,
   rpi_focus,
   NULL,
   rpi_free,
   "rpi",
   rpi_set_rotation,
};
