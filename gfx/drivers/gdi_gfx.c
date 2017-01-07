/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
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

#include <retro_miscellaneous.h>

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../menu/menu_driver.h"
#include "../common/gdi_common.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

static unsigned char *gdi_menu_frame = NULL;
static unsigned gdi_menu_width = 0;
static unsigned gdi_menu_height = 0;
static unsigned gdi_menu_pitch = 0;
static unsigned gdi_video_width = 0;
static unsigned gdi_video_height = 0;
static unsigned gdi_video_pitch = 0;
static unsigned gdi_video_bits = 0;
static bool gdi_rgb32 = 0;

static void gdi_gfx_free(void *data);

static void gdi_gfx_create()
{
   if(!gdi_video_width || !gdi_video_height)
   {
      printf("***** GDI: no width or height!\n");
   }

   //video_driver_set_size(&gdi_video_width, &gdi_video_height);
}

static void *gdi_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   settings_t *settings = config_get_ptr();
   gdi_t *gdi = (gdi_t*)calloc(1, sizeof(*gdi));
   const gfx_ctx_driver_t *ctx_driver = NULL;
   gfx_ctx_input_t inp;
   gfx_ctx_mode_t mode;
   unsigned win_width = 0, win_height = 0;
   unsigned temp_width = 0, temp_height = 0;

   *input = NULL;
   *input_data = NULL;

   gdi_video_width = video->width;
   gdi_video_height = video->height;
   gdi_rgb32 = video->rgb32;

   gdi_video_bits = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      gdi_video_pitch = video->width * 4;
   else
      gdi_video_pitch = video->width * 2;

   gdi_gfx_create();

   ctx_driver = video_context_driver_init_first(gdi,
         settings->video.context_driver,
         GFX_CTX_GDI_API, 1, 0, false);
   if (!ctx_driver)
      goto error;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("Found GDI context: %s\n", ctx_driver->ident);

/*#ifdef HAVE_WINDOW
   win32_window_init(&gdi->wndclass, true, NULL);
#endif

#ifdef HAVE_MONITOR
   bool windowed_full;
   RECT mon_rect;
   MONITORINFOEX current_mon;
   HMONITOR hm_to_use;

   win32_monitor_info(&current_mon, &hm_to_use, &d3d->cur_mon_id);
   mon_rect = current_mon.rcMonitor;
   g_resize_width  = video->width;
   g_resize_height = video->height;

   windowed_full = settings->video.windowed_fullscreen;

   full_x = (windowed_full || video->width  == 0) ?
      (mon_rect.right  - mon_rect.left) : video->width;
   full_y = (windowed_full || video->height == 0) ?
      (mon_rect.bottom - mon_rect.top)  : video->height;
   RARCH_LOG("[GDI]: Monitor size: %dx%d.\n",
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   {
      video_context_driver_get_video_size(&mode);

      full_x   = mode.width;
      full_y   = mode.height;
   }
#endif
   {
      unsigned new_width  = video->fullscreen ? full_x : video->width;
      unsigned new_height = video->fullscreen ? full_y : video->height;
      mode.width = new_width;
      mode.height = new_height;
      mode.fullscreen = video->fullscreen;

      video_context_driver_set_video_mode(&mode);
      video_driver_set_size(&new_width, &new_height);
   }

#ifdef HAVE_WINDOW
   DWORD style;
   unsigned win_width, win_height;
   RECT rect            = {0};

   video_driver_get_size(&win_width, &win_height);

   win32_set_style(&current_mon, &hm_to_use, &win_width, &win_height,
         video->fullscreen, windowed_full, &rect, &mon_rect, &style);

   win32_window_create(gdi, style, &mon_rect, win_width,
         win_height, video->fullscreen);

   win32_set_window(&win_width, &win_height, video->fullscreen,
      windowed_full, &rect);
#endif
*/

   video_context_driver_get_video_size(&mode);

   full_x  = mode.width;
   full_y  = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("Detecting screen resolution %ux%u.\n", full_x, full_y);

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);

   temp_width     = mode.width;
   temp_height    = mode.height;
   mode.width     = 0;
   mode.height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("GDI: Using resolution %ux%u\n", temp_width, temp_height);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->video.font_enable)
      font_driver_init_osd(NULL, false, FONT_DRIVER_RENDER_GDI);

   RARCH_LOG("[GDI]: Init complete.\n");

   return gdi;

error:
   video_context_driver_destroy();
   if (gdi)
      free(gdi);
   return NULL;
}

static bool gdi_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg)
{
   const void *frame_to_copy = frame;
   unsigned width = 0;
   unsigned height = 0;
   bool draw = true;
   gdi_t *gdi = (gdi_t*)data;

   if (!frame || !frame_width || !frame_height)
      return true;

   if (gdi_video_width != frame_width || gdi_video_height != frame_height || gdi_video_pitch != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gdi_video_width = frame_width;
         gdi_video_height = frame_height;
         gdi_video_pitch = pitch;
         gdi_gfx_free(NULL);
         gdi_gfx_create();
      }
   }

   if (gdi_menu_frame)
      frame_to_copy = gdi_menu_frame;

   //width = gdi_get_canvas_width(gdi_cv);
   //height = gdi_get_canvas_height(gdi_cv);
   width = frame_width;
   height = frame_height;

   if (frame_to_copy == frame && frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
      draw = false;

#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_FRAME, NULL);
#endif

   if (msg)
      font_driver_render_msg(NULL, msg, NULL);

   if (draw)
   {
      unsigned win_width, win_height;
      HWND hwnd = win32_get_window();
      HDC dc = GetDC(hwnd);
      BITMAPINFO info;
      gfx_ctx_mode_t mode;

      video_context_driver_get_video_size(&mode);

      ZeroMemory(&info, sizeof(BITMAPINFO));
      info.bmiHeader.biBitCount = gdi_video_bits;
      info.bmiHeader.biWidth = width;
      info.bmiHeader.biHeight = height;
      info.bmiHeader.biPlanes = 1;
      info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      info.bmiHeader.biSizeImage = pitch * height;
      info.bmiHeader.biCompression = BI_RGB;
/*
      if (gdi_rgb32)
      {
         info.bmiColors[0].rgbBlue = 0xFF;
         info.bmiColors[0].rgbGreen = 0x00;
         info.bmiColors[0].rgbRed = 0x00;
         info.bmiColors[0].rgbReserved = 0x00;

         info.bmiColors[1].rgbBlue = 0x00;
         info.bmiColors[1].rgbGreen = 0xFF;
         info.bmiColors[1].rgbRed = 0x00;
         info.bmiColors[1].rgbReserved = 0x00;

         info.bmiColors[2].rgbBlue = 0x00;
         info.bmiColors[2].rgbGreen = 0x00;
         info.bmiColors[2].rgbRed = 0xFF;
         info.bmiColors[2].rgbReserved = 0x00;
      }
      else
      {
         info.bmiColors[0].rgbBlue = 0x1F;
         info.bmiColors[0].rgbGreen = 0x00;
         info.bmiColors[0].rgbRed = 0x00;
         info.bmiColors[0].rgbReserved = 0x00;

         info.bmiColors[1].rgbBlue = 0x00;
         info.bmiColors[1].rgbGreen = 0x1F;
         info.bmiColors[1].rgbRed = 0x00;
         info.bmiColors[1].rgbReserved = 0x00;

         info.bmiColors[2].rgbBlue = 0x00;
         info.bmiColors[2].rgbGreen = 0x00;
         info.bmiColors[2].rgbRed = 0x1F;
         info.bmiColors[2].rgbReserved = 0x00;
      }
*/
      StretchDIBits(dc, 0, 0, mode.width, mode.height, 0, 0, width, height,
            frame_to_copy, &info, DIB_RGB_COLORS, SRCCOPY);
      ReleaseDC(hwnd, dc);
   }

   video_context_driver_update_window_title();

   video_context_driver_swap_buffers();

   //UpdateWindow(win32_get_window());

   return true;
}

static void gdi_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool gdi_gfx_alive(void *data)
{
   (void)data;
   video_driver_set_size(&gdi_video_width, &gdi_video_height);
   return true;
}

static bool gdi_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool gdi_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gdi_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void gdi_gfx_free(void *data)
{
   gdi_t *gdi = (gdi_t*)data;

   if (gdi_menu_frame)
   {
      free(gdi_menu_frame);
      gdi_menu_frame = NULL;
   }

   if (!gdi)
      return;

   font_driver_free_osd();
   video_context_driver_free();
   free(gdi);
}

static bool gdi_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void gdi_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void gdi_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool gdi_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void gdi_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (gdi_menu_frame)
   {
      free(gdi_menu_frame);
      gdi_menu_frame = NULL;
   }

   if (!gdi_menu_frame || gdi_menu_width != width || gdi_menu_height != height || gdi_menu_pitch != pitch)
      if (pitch && height)
         gdi_menu_frame = (unsigned char*)malloc(pitch * height);

   if (gdi_menu_frame && frame && pitch && height)
      memcpy(gdi_menu_frame, frame, pitch * height);
}

static void gdi_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   font_driver_render_msg(font, msg, params);
}

static void gdi_get_video_output_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_size_t size_data;
   size_data.width  = width;
   size_data.height = height;
   video_context_driver_get_video_output_size(&size_data);
}

static void gdi_get_video_output_prev(void *data)
{
   video_context_driver_get_video_output_prev();
}

static void gdi_get_video_output_next(void *data)
{
   video_context_driver_get_video_output_next();
}

static void gdi_set_video_mode(void *data, unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static const video_poke_interface_t gdi_poke_interface = {
   NULL,
   NULL,
   gdi_set_video_mode,
   NULL,
   gdi_get_video_output_size,
   gdi_get_video_output_prev,
   gdi_get_video_output_next,
#ifdef HAVE_FBO
   NULL,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
#if defined(HAVE_MENU)
   gdi_set_texture_frame,
   NULL,
   gdi_set_osd_msg,
   NULL,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif

   NULL,
#ifdef HAVE_MENU
   NULL,
#endif
};

static void gdi_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gdi_poke_interface;
}

static void gdi_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
}

video_driver_t video_gdi = {
   gdi_gfx_init,
   gdi_gfx_frame,
   gdi_gfx_set_nonblock_state,
   gdi_gfx_alive,
   gdi_gfx_focus,
   gdi_gfx_suppress_screensaver,
   gdi_gfx_has_windowed,
   gdi_gfx_set_shader,
   gdi_gfx_free,
   "gdi",
   gdi_gfx_set_viewport,
   gdi_gfx_set_rotation,
   gdi_gfx_viewport_info,
   gdi_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  gdi_gfx_get_poke_interface,
};
