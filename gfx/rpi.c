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

#include <math.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <EGL/egl.h>
#include "gfx_context.h"
#include "../libretro.h"
#include "../general.h"
#include "../driver.h"

#ifdef HAVE_FREETYPE
#include "fonts/fonts.h"
#include "../file.h"
#endif

typedef struct
{
   uint32_t mScreenWidth;
   uint32_t mScreenHeight;
   float mScreenAspect;
   bool mKeepAspect;
   unsigned mTextureWidth;
   unsigned mTextureHeight;
   unsigned mRenderWidth;
   unsigned mRenderHeight;
   unsigned x1, y1, x2, y2;
   unsigned frame_count;
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
   (void)data;
   gfx_ctx_set_swap_interval(state ? 0 : 1, true);
}

static void *rpi_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   rpi_t *rpi = (rpi_t*)calloc(1, sizeof(rpi_t));
   if (!rpi)
      return NULL;

   if (!eglBindAPI(EGL_OPENVG_API))
      return NULL;

   if (!gfx_ctx_init())
   {
      free(rpi);
      return NULL;
   }

   gfx_ctx_get_video_size(&rpi->mScreenWidth, &rpi->mScreenHeight);
   RARCH_LOG("Detecting screen resolution %ux%u.\n", rpi->mScreenWidth, rpi->mScreenHeight);

   gfx_ctx_set_swap_interval(video->vsync ? 1 : 0, false);

   rpi->mTexType = video->rgb32 ? VG_sABGR_8888 : VG_sARGB_1555;
   rpi->mKeepAspect = video->force_aspect;

   // check for SD televisions: they should always be 4:3
   if (rpi->mScreenWidth == 720 && (rpi->mScreenHeight == 480 || rpi->mScreenHeight == 576))
      rpi->mScreenAspect = 4.0f / 3.0f;
   else
      rpi->mScreenAspect = (float)rpi->mScreenWidth / rpi->mScreenHeight;

   VGfloat clearColor[4] = {0, 0, 0, 1};
   vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

   rpi->mTextureWidth = rpi->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
   // We can't use the native format because there's no sXRGB_1555 type and
   // emulation cores can send 0 in the top bit. We lose some speed on
   // conversion but I doubt it has any real affect, since we are only drawing
   // one image at the end of the day. Still keep the alpha channel for ABGR.
   rpi->mImage = vgCreateImage(video->rgb32 ? VG_sABGR_8888 : VG_sXBGR_8888,
         rpi->mTextureWidth, rpi->mTextureHeight,
         video->smooth ? VG_IMAGE_QUALITY_BETTER : VG_IMAGE_QUALITY_NONANTIALIASED);
   rpi_set_nonblock_state(rpi, !video->vsync);

   gfx_ctx_input_driver(input, input_data);

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

   gfx_ctx_destroy();

   free(rpi);
}

#ifdef HAVE_FREETYPE

static void rpi_render_message(rpi_t *rpi, const char *msg)
{
   free(rpi->mLastMsg);
   rpi->mLastMsg = strdup(msg);

   if (rpi->mMsgLength)
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

      escapement[0] = head->advance_x;
      escapement[1] = head->advance_y;
      origin[0] = -head->char_off_x;
      origin[1] = -head->char_off_y;

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

   VGfloat origins[] = {
      rpi->mScreenWidth * g_settings.video.msg_pos_x - 2.0f,
      rpi->mScreenHeight * g_settings.video.msg_pos_y - 2.0f,
   };

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
   rpi->frame_count++;

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
   //if (msg && rpi->mFontsOn)
   //   rpi_draw_message(rpi, msg);
   static char temp[4096];
   gfx_window_title(temp, 4096);
   rpi_draw_message(rpi, temp);
#else
   (void)msg;
#endif

   gfx_ctx_swap_buffers();

   return true;
}

static bool rpi_alive(void *data)
{
   rpi_t *rpi = (rpi_t*)data;
   bool quit, resize;

   gfx_ctx_check_window(&quit,
         &resize, &rpi->mScreenWidth, &rpi->mScreenHeight,
         rpi->frame_count);
   return !quit;
}

static bool rpi_focus(void *data)
{
   (void)data;
   return gfx_ctx_window_has_focus();
}

const video_driver_t video_rpi = {
   rpi_init,
   rpi_frame,
   rpi_set_nonblock_state,
   rpi_alive,
   rpi_focus,
   NULL,
   rpi_free,
   "rpi"
};
