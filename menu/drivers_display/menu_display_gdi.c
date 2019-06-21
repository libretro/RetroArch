/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <time.h>

#include <clamping.h>
#include <queues/message_queue.h>
#include <retro_miscellaneous.h>

#include "../../config.def.h"
#include "../../gfx/font_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../menu_driver.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../../gfx/common/win32_common.h"
#include "../../gfx/common/gdi_common.h"
#endif

static void *menu_display_gdi_get_default_mvp(video_frame_info_t *video_info)
{
   return NULL;
}

static void menu_display_gdi_blend_begin(video_frame_info_t *video_info)
{
}

static void menu_display_gdi_blend_end(video_frame_info_t *video_info)
{
}

static void menu_display_gdi_draw(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
   struct gdi_texture *texture = NULL;
   gdi_t *gdi                  = (gdi_t*)video_info->userdata;
   BITMAPINFO info             = {{0}};

   if (!gdi || !draw || draw->x < 0 || draw->y < 0 || draw->width <= 1 || draw->height <= 1)
      return;

   texture = (struct gdi_texture*)draw->texture;

   if (!texture || texture->width <= 1 || texture->height <= 1)
      return;

   info.bmiHeader.biBitCount  = 32;
   info.bmiHeader.biWidth     = texture->width;
   info.bmiHeader.biHeight    = -texture->height;
   info.bmiHeader.biPlanes    = 1;
   info.bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
   info.bmiHeader.biSizeImage = 0;
   info.bmiHeader.biCompression = BI_RGB;

   if (gdi->memDC)
   {
#if _WIN32_WINNT >= 0x0410 /* Win98 */
      BLENDFUNCTION blend = {0};
#endif

      if (!gdi->texDC)
         gdi->texDC        = CreateCompatibleDC(gdi->winDC);

      if (texture->bmp)
         texture->bmp_old  = (HBITMAP)SelectObject(gdi->texDC, texture->bmp);
      else
      {
         /* scale texture data into a bitmap we can easily blit later */
         texture->bmp     = CreateCompatibleBitmap(gdi->winDC, draw->width, draw->height);
         texture->bmp_old = (HBITMAP)SelectObject(gdi->texDC, texture->bmp);

         StretchDIBits(gdi->texDC, 0, 0, draw->width, draw->height, 0, 0, texture->width, texture->height, texture->data, &info, DIB_RGB_COLORS, SRCCOPY);
      }

      gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

#if _WIN32_WINNT >= 0x0410 /* Win98 */
      blend.BlendOp = AC_SRC_OVER;
      blend.BlendFlags = 0;
      blend.SourceConstantAlpha = 255;/*clamp_8bit(draw->coords->color[3] * 255.0f);*/
      blend.AlphaFormat = AC_SRC_ALPHA;

      /* AlphaBlend() is only available since Win98 */
      AlphaBlend(gdi->memDC, draw->x, video_info->height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, blend);
      /*TransparentBlt(gdi->memDC, draw->x, video_info->height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, 0);*/
#else
      /* Just draw without the blending */
      StretchBlt(gdi->memDC, draw->x, video_info->height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, SRCCOPY);

#endif

      SelectObject(gdi->memDC, gdi->bmp_old);
      SelectObject(gdi->texDC, texture->bmp_old);
   }
}

static void menu_display_gdi_draw_pipeline(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_gdi_viewport(menu_display_ctx_draw_t *draw,
      video_frame_info_t *video_info)
{
}

static void menu_display_gdi_restore_clear_color(void)
{
}

static void menu_display_gdi_clear_color(
      menu_display_ctx_clearcolor_t *clearcolor,
      video_frame_info_t *video_info)
{
   (void)clearcolor;

   menu_display_gdi_restore_clear_color();
}

static bool menu_display_gdi_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float gdi_font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;
   if (!(*handle = font_driver_init_first(video_data,
         font_path, gdi_font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_GDI)))
      return false;
   return true;
}

static const float *menu_display_gdi_get_default_vertices(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static const float *menu_display_gdi_get_default_tex_coords(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

menu_display_ctx_driver_t menu_display_ctx_gdi = {
   menu_display_gdi_draw,
   menu_display_gdi_draw_pipeline,
   menu_display_gdi_viewport,
   menu_display_gdi_blend_begin,
   menu_display_gdi_blend_end,
   menu_display_gdi_restore_clear_color,
   menu_display_gdi_clear_color,
   menu_display_gdi_get_default_mvp,
   menu_display_gdi_get_default_vertices,
   menu_display_gdi_get_default_tex_coords,
   menu_display_gdi_font_init_first,
   MENU_VIDEO_DRIVER_GDI,
   "gdi",
   false,
   NULL,
   NULL
};
