/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>

#include <encodings/utf.h>
#include <lists/string_list.h>
#include <retro_miscellaneous.h>
#include <formats/image.h>
#include <string/stdstring.h>

#include <windows.h>
#include <wingdi.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"
#include "../gfx_display.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gdi_defines.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

struct bitmap_info
{
   BITMAPINFOHEADER header;
   union
   {
      RGBQUAD          colors;
      DWORD            masks[3];
   } u;
};

HDC          win32_gdi_hdc;
static void *dinput_gdi;

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_gdi_get_default_vertices(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static const float *gfx_display_gdi_get_default_tex_coords(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static void gfx_display_gdi_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   struct gdi_texture *texture = NULL;
   gdi_t *gdi                  = (gdi_t*)data;
   BITMAPINFO info             = {{0}};

   if (!gdi || !draw || draw->x < 0 || draw->y < 0 || draw->width <= 1 || draw->height <= 1)
      return;

   texture = (struct gdi_texture*)draw->texture;

   if (!texture || texture->width <= 1 || texture->height <= 1)
      return;

   info.bmiHeader.biBitCount    = 32;
   info.bmiHeader.biWidth       = texture->width;
   info.bmiHeader.biHeight      = -texture->height;
   info.bmiHeader.biPlanes      = 1;
   info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   info.bmiHeader.biSizeImage   = 0;
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
      blend.SourceConstantAlpha = 255;
#if 0
      clamp_8bit(draw->coords->color[3] * 255.0f);
#endif
      blend.AlphaFormat = AC_SRC_ALPHA;

      /* AlphaBlend() is only available since Win98 */
      AlphaBlend(gdi->memDC, draw->x, video_height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, blend);
#if 0
      TransparentBlt(gdi->memDC, draw->x, video_height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, 0);
#endif
#else
      /* Just draw without the blending */
      StretchBlt(gdi->memDC, draw->x, video_height - draw->height - draw->y, draw->width, draw->height, gdi->texDC, 0, 0, draw->width, draw->height, SRCCOPY);

#endif

      SelectObject(gdi->memDC, gdi->bmp_old);
      SelectObject(gdi->texDC, texture->bmp_old);
   }
}

gfx_display_ctx_driver_t gfx_display_ctx_gdi = {
   gfx_display_gdi_draw,
   NULL,                                     /* draw_pipeline   */
   NULL,                                     /* blend_begin     */
   NULL,                                     /* blend_end       */
   NULL,                                     /* get_default_mvp */
   gfx_display_gdi_get_default_vertices,
   gfx_display_gdi_get_default_tex_coords,
   FONT_DRIVER_RENDER_GDI,
   GFX_VIDEO_DRIVER_GDI,
   "gdi",
   false,
   NULL,                                     /* scissor_begin */
   NULL                                      /* scissor_end   */
};

/*
 * FONT DRIVER
 */

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void *font_data;
   gdi_t *gdi;
} gdi_raster_t;

static void *gdi_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   gdi_raster_t *font  = (gdi_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->gdi = (gdi_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
      return NULL;

   return font;
}

static void gdi_font_free(void *data, bool is_threaded)
{
  gdi_raster_t *font = (gdi_raster_t*)data;

  if (!font)
     return;

  if (font->font_driver && font->font_data)
     font->font_driver->free(font->font_data);

  free(font);
}

static void gdi_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   size_t i;
   char* msg_local;
   size_t msg_len;
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   unsigned new_x, new_y, new_drop_x, new_drop_y;
   unsigned align;
   unsigned red, green, blue;
   gdi_t *gdi                       = (gdi_t*)userdata;
   gdi_raster_t *font               = (gdi_raster_t*)data;
   unsigned width                   = gdi->video_width;
   unsigned height                  = gdi->video_height;
   SIZE text_size                   = {0};
   struct string_list msg_list      = {0};

   if (!font || string_is_empty(msg) || !gdi)
      return;

   if (params)
   {
      x                       = params->x;
      y                       = params->y;
      drop_x                  = params->drop_x;
      drop_y                  = params->drop_y;
      drop_mod                = params->drop_mod;
      drop_alpha              = params->drop_alpha;
      scale                   = params->scale;
      align                   = params->text_align;

      red                     = FONT_COLOR_GET_RED(params->color);
      green                   = FONT_COLOR_GET_GREEN(params->color);
      blue                    = FONT_COLOR_GET_BLUE(params->color);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
      scale                   = 1.0f;
      align                   = TEXT_ALIGN_LEFT;
      red                     = video_msg_color_r * 255.0f;
      green                   = video_msg_color_g * 255.0f;
      blue                    = video_msg_color_b * 255.0f;
   }

   msg_local                  = utf8_to_local_string_alloc(msg);
   msg_len                    = strlen(msg_local);

   GetTextExtentPoint32(gdi->memDC, msg_local, (int)msg_len, &text_size);

   switch (align)
   {
      case TEXT_ALIGN_LEFT:
         new_x        = x * width * scale;
         new_drop_x   = drop_x * width * scale;
         break;
      case TEXT_ALIGN_RIGHT:
         new_x        = (x * width * scale) - text_size.cx;
         new_drop_x   = (drop_x * width * scale) - text_size.cx;
         break;
      case TEXT_ALIGN_CENTER:
         new_x        = (x * width * scale) - (text_size.cx / 2);
         new_drop_x   = (drop_x * width * scale) - (text_size.cx / 2);
         break;
      default:
         new_x        = 0;
         new_drop_x   = 0;
         break;
   }

   new_y        = height - (y * height * scale)      - text_size.cy;
   new_drop_y   = height - (drop_y * height * scale) - text_size.cy;

   gdi->bmp_old = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

   SetBkMode(gdi->memDC, TRANSPARENT);

   string_list_initialize(&msg_list);
   string_split_noalloc(&msg_list, msg_local, "\n");

   if (drop_x || drop_y)
   {
      float    dark_alpha = drop_alpha;
      unsigned drop_red   = red * drop_mod * dark_alpha;
      unsigned drop_green = green * drop_mod * dark_alpha;
      unsigned drop_blue  = blue * drop_mod * dark_alpha;

      SetTextColor(gdi->memDC, RGB(drop_red, drop_green, drop_blue));

      for (i = 0; i < msg_list.size; i++)
         TextOut(gdi->memDC, new_drop_x,
               new_drop_y + (text_size.cy * i),
               msg_list.elems[i].data,
               strlen(msg_list.elems[i].data));
   }

   SetTextColor(gdi->memDC, RGB(red, green, blue));

   for (i = 0; i < msg_list.size; i++)
      TextOut(gdi->memDC, new_x, new_y + (text_size.cy * i),
            msg_list.elems[i].data,
            strlen(msg_list.elems[i].data));

   string_list_deinitialize(&msg_list);
   free(msg_local);

   SelectObject(gdi->memDC, gdi->bmp_old);
}

font_renderer_t gdi_font = {
   gdi_font_init,
   gdi_font_free,
   gdi_font_render_msg,
   "gdi",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL,                      /* get_message_width */
   NULL                       /* get_line_metrics */
};

/*
 * VIDEO DRIVER
 */

static void gfx_ctx_gdi_get_video_size(
      unsigned *width, unsigned *height)
{
   HWND window                  = win32_get_window();

   if (window)
   {
      *width                    = g_win32_resize_width;
      *height                   = g_win32_resize_height;
   }
   else
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id           = 0;
      HMONITOR hm_to_use        = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect = current_mon.rcMonitor;
      *width   = mon_rect.right - mon_rect.left;
      *height  = mon_rect.bottom - mon_rect.top;
   }
}

static bool gfx_ctx_gdi_init(void)
{
   WNDCLASSEX wndclass      = {0};
   settings_t *settings     = config_get_ptr();
   uint8_t win32_flags      = win32_get_flags();

   if (win32_flags & WIN32_CMN_FLAG_INITED)
      return true;

   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = wnd_proc_gdi_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      wndclass.lpfnWndProc   = wnd_proc_gdi_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      wndclass.lpfnWndProc   = wnd_proc_gdi_winraw;
#endif
   if (!win32_window_init(&wndclass, true, NULL))
      return false;
   return true;
}

static void gfx_ctx_gdi_destroy(void)
{
   HWND     window         = win32_get_window();

   if (window && win32_gdi_hdc)
   {
      ReleaseDC(window, win32_gdi_hdc);
      win32_gdi_hdc = NULL;
   }

   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

   if (g_win32_flags & WIN32_CMN_FLAG_RESTORE_DESKTOP)
   {
      win32_monitor_get_info();
      g_win32_flags &= ~WIN32_CMN_FLAG_RESTORE_DESKTOP;
   }

   g_win32_flags &= ~WIN32_CMN_FLAG_INITED;
}

static bool gfx_ctx_gdi_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      gfx_ctx_gdi_destroy();
      return false;
   }

   return true;
}

static void gfx_ctx_gdi_input_driver(
      input_driver_t **input, void **input_data)
{
   settings_t *settings = config_get_ptr();
#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   if (string_is_equal(settings->arrays.input_driver, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, settings->arrays.input_driver);
      if (*input_data)
      {
         *input     = &input_winraw;
         dinput_gdi = NULL;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   dinput_gdi  = input_driver_init_wrap(&input_dinput, settings->arrays.input_driver);
   *input      = dinput_gdi ? &input_dinput : NULL;
#else
   dinput_gdi  = NULL;
   *input      = NULL;
#endif
   *input_data = dinput_gdi;
}

static void gdi_create(gdi_t *gdi)
{
   char os[64] = {0};

   frontend_ctx_driver_t *ctx = frontend_get_ptr();

   if (!ctx || !ctx->get_os)
   {
      RARCH_ERR("[GDI] No frontend driver found.\n");
      return;
   }

   ctx->get_os(os, sizeof(os), &gdi->win_major, &gdi->win_minor);

   /* Are we running on Windows 98 or below? */
   if (gdi->win_major < 4 || (gdi->win_major == 4 && gdi->win_minor <= 10))
   {
      RARCH_LOG("[GDI] Win98 or lower detected, using slow frame conversion method for RGB444.\n");
      gdi->lte_win98 = true;
   }
}

static void *gdi_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   void *ctx_data                       = NULL;
   unsigned mode_width = 0, mode_height = 0;
   unsigned win_width  = 0, win_height  = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   bool video_font_enable               = settings->bools.video_font_enable;
   gdi_t *gdi                           = (gdi_t*)calloc(1, sizeof(*gdi));

   if (!gdi)
      return NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   gdi->video_width                     = video->width;
   gdi->video_height                    = video->height;
   gdi->rgb32                           = video->rgb32;

   gdi->video_bits                      = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      gdi->video_pitch                  = video->width * 4;
   else
      gdi->video_pitch                  = video->width * 2;

   gdi_create(gdi);
   if (!gfx_ctx_gdi_init())
      goto error;

   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);

   full_x      = mode_width;
   full_y      = mode_height;
   mode_width  = 0;
   mode_height = 0;

   RARCH_LOG("[GDI]: Detecting screen resolution: %ux%u.\n", full_x, full_y);

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode_width      = win_width;
   mode_height     = win_height;

   if (!gfx_ctx_gdi_set_video_mode(mode_width,
            mode_height, video->fullscreen))
      goto error;

   mode_width     = 0;
   mode_height    = 0;

   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);

   temp_width     = mode_width;
   temp_height    = mode_height;
   mode_width     = 0;
   mode_height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("[GDI]: Using resolution %ux%u\n", temp_width, temp_height);

   gfx_ctx_gdi_input_driver(input, input_data);

   if (video_font_enable)
      font_driver_init_osd(gdi,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_GDI);

   RARCH_LOG("[GDI]: Init complete.\n");

   return gdi;

error:
   gfx_ctx_gdi_destroy();
   if (gdi)
      free(gdi);
   return NULL;
}

static bool gdi_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   struct bitmap_info info;
   unsigned mode_width              = 0;
   unsigned mode_height             = 0;
   const void *frame_to_copy        = frame;
   unsigned width                   = 0;
   unsigned height                  = 0;
   bool draw                        = true;
   gdi_t *gdi                       = (gdi_t*)data;
   unsigned bits                    = gdi->video_bits;
   HWND hwnd                        = win32_get_window();
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif

   /* FIXME: Force these settings off as they interfere with the rendering */
   video_info->xmb_shadows_enable   = false;
   video_info->menu_shader_pipeline = 0;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   if (gdi->menu_enable)
      menu_driver_frame(menu_is_alive, video_info);
#endif

   if (     (gdi->video_width  != frame_width)
         || (gdi->video_height != frame_height)
         || (gdi->video_pitch  != pitch))
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gdi->video_width  = frame_width;
         gdi->video_height = frame_height;
         gdi->video_pitch  = pitch;
      }
   }

#ifdef HAVE_MENU
   if (gdi->menu_frame && menu_is_alive)
   {
      frame_to_copy = gdi->menu_frame;
      width         = gdi->menu_width;
      height        = gdi->menu_height;
      pitch         = gdi->menu_pitch;
      bits          = gdi->menu_bits;
   }
   else
#endif
   {
      width         = gdi->video_width;
      height        = gdi->video_height;
      pitch         = gdi->video_pitch;

      if (  frame_width  == 4 &&
            frame_height == 4 &&
            (frame_width < width && frame_height < height)
         )
         draw = false;

#ifdef HAVE_MENU
      if (menu_is_alive)
         draw = false;
#endif
   }

   if (hwnd && !gdi->winDC)
   {
      gdi->winDC        = GetDC(hwnd);
      gdi->memDC        = CreateCompatibleDC(gdi->winDC);
      gdi->video_width  = width;
      gdi->video_height = height;
      gdi->bmp          = CreateCompatibleBitmap(
            gdi->winDC, gdi->video_width, gdi->video_height);
   }

   gdi->bmp_old  = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

   if (gdi->video_width != width || gdi->video_height != height)
   {
      SelectObject(gdi->memDC, gdi->bmp_old);
      DeleteObject(gdi->bmp);

      gdi->video_width  = width;
      gdi->video_height = height;
      gdi->bmp          = CreateCompatibleBitmap(
            gdi->winDC, gdi->video_width, gdi->video_height);
      gdi->bmp_old      = (HBITMAP)SelectObject(gdi->memDC, gdi->bmp);

      if (gdi->lte_win98)
      {
         unsigned short *tmp = NULL;

         if (gdi->temp_buf)
            free(gdi->temp_buf);

         tmp = (unsigned short*)malloc(width * height
               * sizeof(unsigned short));

         if (tmp)
            gdi->temp_buf = tmp;
      }
   }

   gfx_ctx_gdi_get_video_size(&mode_width, &mode_height);

   gdi->screen_width             = mode_width;
   gdi->screen_height            = mode_height;

   info.u.colors.rgbBlue     = 0;
   info.u.colors.rgbGreen    = 0;
   info.u.colors.rgbRed      = 0;
   info.u.colors.rgbReserved = 0;

   info.header.biSize         = sizeof(BITMAPINFOHEADER);
   info.header.biWidth        = pitch / (bits / 8);
   info.header.biHeight       = -height;
   info.header.biPlanes       = 1;
   info.header.biBitCount     = bits;
   info.header.biCompression  = 0;
   info.header.biSizeImage    = 0;
   info.header.biXPelsPerMeter= 0;
   info.header.biYPelsPerMeter= 0;
   info.header.biClrUsed      = 0;
   info.header.biClrImportant = 0;

   if (bits == 16)
   {
      if (gdi->lte_win98 && gdi->temp_buf)
      {
         /* Win98 and below cannot use BI_BITFIELDS with RGB444,
          * so convert it to RGB555 first. */
         unsigned x, y;

         for (y = 0; y < height; y++)
         {
            for (x = 0; x < width; x++)
            {
               unsigned short pixel = ((unsigned short*)frame_to_copy)[width * y + x];
               gdi->temp_buf[width * y + x] = (pixel & 0xF000) >> 1 | (pixel & 0x0F00) >> 2 | (pixel & 0x00F0) >> 3;
            }
         }

         frame_to_copy = gdi->temp_buf;
         info.header.biCompression = BI_RGB;
      }
      else
      {
         info.header.biCompression = BI_BITFIELDS;

         /* default 16-bit format on Windows is XRGB1555 */
         if (frame_to_copy == gdi->menu_frame)
         {
            /* map RGB444 color bits for RGUI */
            info.u.masks[0] = 0xF000;
            info.u.masks[1] = 0x0F00;
            info.u.masks[2] = 0x00F0;
         }
         else
         {
            /* map RGB565 color bits for core */
            info.u.masks[0] = 0xF800;
            info.u.masks[1] = 0x07E0;
            info.u.masks[2] = 0x001F;
         }
      }
   }
   else
      info.header.biCompression = BI_RGB;

   if (draw)
      StretchDIBits(gdi->memDC, 0, 0, width, height, 0, 0, width, height,
            frame_to_copy, (BITMAPINFO*)&info, DIB_RGB_COLORS, SRCCOPY);

   SelectObject(gdi->memDC, gdi->bmp_old);

   if (msg)
      font_driver_render_msg(gdi, msg, NULL, NULL);

   InvalidateRect(hwnd, NULL, false);

   video_driver_update_title(NULL);

   return true;
}

static bool gdi_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool ret             = false;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   win32_check_window(NULL,
            &quit, &resize, &temp_width, &temp_height);

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);

   return ret;
}

static void gdi_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool gdi_focus(void *data) { return true; }
static bool gdi_suppress_screensaver(void *a, bool b) { return false; }
static bool gdi_has_windowed(void *data) { return true; }

static void gdi_free(void *data)
{
   gdi_t *gdi = (gdi_t*)data;
   HWND hwnd  = win32_get_window();

   if (!gdi)
      return;

   if (gdi->menu_frame)
      free(gdi->menu_frame);
   gdi->menu_frame = NULL;

   if (gdi->temp_buf)
      free(gdi->temp_buf);
   gdi->temp_buf = NULL;

   if (gdi->bmp)
      DeleteObject(gdi->bmp);

   if (gdi->texDC)
   {
      DeleteDC(gdi->texDC);
      gdi->texDC = 0;
   }
   if (gdi->memDC)
   {
      DeleteDC(gdi->memDC);
      gdi->memDC = 0;
   }

   if (hwnd && gdi->winDC)
   {
      ReleaseDC(hwnd, gdi->winDC);
      gdi->winDC = 0;
   }

   font_driver_free_osd();
   gfx_ctx_gdi_destroy();
   free(gdi);
}

static bool gdi_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void gdi_set_texture_enable(
      void *data, bool state, bool full_screen)
{
   gdi_t *gdi     = (gdi_t*)data;
   if (!gdi)
      return;

   gdi->menu_enable      = state;
   gdi->menu_full_screen = full_screen;
}

static void gdi_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   gdi_t *gdi     = (gdi_t*)data;
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (gdi->menu_frame)
      free(gdi->menu_frame);
   gdi->menu_frame = NULL;

   if (     !gdi->menu_frame
         || (gdi->menu_width  != width)
         || (gdi->menu_height != height)
         || (gdi->menu_pitch  != pitch))
   {
      if (pitch && height)
      {
         unsigned char *tmp = (unsigned char*)malloc(pitch * height);

         if (tmp)
            gdi->menu_frame = tmp;
      }
   }

   if (gdi->menu_frame && frame && pitch && height)
   {
      memcpy(gdi->menu_frame, frame, pitch * height);
      gdi->menu_width  = width;
      gdi->menu_height = height;
      gdi->menu_pitch  = pitch;
      gdi->menu_bits   = rgb32 ? 32 : 16;
   }
}

static void gdi_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_gdi_set_video_mode(width, height, fullscreen);
}

static uintptr_t gdi_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   void *tmpdata               = NULL;
   gdi_texture_t *texture      = NULL;
   struct texture_image *image = (struct texture_image*)data;

   if (!image || image->width > 2048 || image->height > 2048)
      return 0;

   texture                     = (gdi_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   texture->width              = image->width;
   texture->height             = image->height;
   texture->active_width       = image->width;
   texture->active_height      = image->height;
   texture->data               = calloc(1,
         texture->width * texture->height * sizeof(uint32_t));
   texture->type               = filter_type;

   if (!texture->data)
   {
      free(texture);
      return 0;
   }

   memcpy(texture->data, image->pixels,
         texture->width * texture->height * sizeof(uint32_t));

   return (uintptr_t)texture;
}

static void gdi_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   struct gdi_texture *texture = (struct gdi_texture*)handle;

   if (!texture)
      return;

   if (texture->data)
      free(texture->data);

   if (texture->bmp)
   {
      DeleteObject(texture->bmp);
      texture->bmp = NULL;
   }

   free(texture);
}

static uint32_t gdi_get_flags(void *data) { return 0; }

static void gdi_get_video_output_prev(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_prev(&width, &height);
}

static void gdi_get_video_output_next(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_next(&width, &height);
}

static const video_poke_interface_t gdi_poke_interface = {
   gdi_get_flags,
   gdi_load_texture,
   gdi_unload_texture,
   gdi_set_video_mode,
   win32_get_refresh_rate,
   NULL, /* set_filtering */
   win32_get_video_output_size,
   gdi_get_video_output_prev,
   gdi_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   gdi_set_texture_frame,
   gdi_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void gdi_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &gdi_poke_interface; }
static void gdi_set_viewport(void *data, unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate) { }

video_driver_t video_gdi = {
   gdi_init,
   gdi_frame,
   gdi_set_nonblock_state,
   gdi_alive,
   gdi_focus,
   gdi_suppress_screensaver,
   gdi_has_windowed,
   gdi_set_shader,
   gdi_free,
   "gdi",
   gdi_set_viewport,
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* get_overlay_interface */
#endif
   gdi_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};
