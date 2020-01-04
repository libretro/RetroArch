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

#include <retro_miscellaneous.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../driver.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/gdi_common.h"

#if defined(_WIN32) && !defined(_XBOX)
#include "../common/win32_common.h"
#endif

static unsigned char *gdi_menu_frame = NULL;
static unsigned gdi_menu_width       = 0;
static unsigned gdi_menu_height      = 0;
static unsigned gdi_menu_pitch       = 0;
static unsigned gdi_video_width      = 0;
static unsigned gdi_video_height     = 0;
static unsigned gdi_video_pitch      = 0;
static unsigned gdi_video_bits       = 0;
static unsigned gdi_menu_bits        = 0;
static bool gdi_rgb32                = false;
static bool gdi_menu_rgb32           = false;
static int gdi_win_major             = 0;
static int gdi_win_minor             = 0;
static bool gdi_lte_win98            = false;
static unsigned short *gdi_temp_buf  = NULL;

static void gdi_gfx_create(void)
{
   char os[64] = {0};

   frontend_ctx_driver_t *ctx = frontend_get_ptr();

   if (!ctx || !ctx->get_os)
   {
      RARCH_ERR("[GDI] No frontend driver found.\n");
      return;
   }

   ctx->get_os(os, sizeof(os), &gdi_win_major, &gdi_win_minor);

   /* Are we running on Windows 98 or below? */
   if (gdi_win_major < 4 || (gdi_win_major == 4 && gdi_win_minor <= 10))
   {
      RARCH_LOG("[GDI] Win98 or lower detected, using slow frame conversion method for RGB444.\n");
      gdi_lte_win98 = true;
   }
}

static void *gdi_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned full_x, full_y;
   gfx_ctx_input_t inp;
   gfx_ctx_mode_t mode;
   void *ctx_data                       = NULL;
   const gfx_ctx_driver_t *ctx_driver   = NULL;
   unsigned win_width = 0, win_height   = 0;
   unsigned temp_width = 0, temp_height = 0;
   settings_t *settings                 = config_get_ptr();
   gdi_t *gdi                           = (gdi_t*)calloc(1, sizeof(*gdi));

   if (!gdi)
      return NULL;

   *input                               = NULL;
   *input_data                          = NULL;

   gdi_video_width                      = video->width;
   gdi_video_height                     = video->height;
   gdi_rgb32                            = video->rgb32;

   gdi_video_bits                       = video->rgb32 ? 32 : 16;

   if (video->rgb32)
      gdi_video_pitch = video->width * 4;
   else
      gdi_video_pitch = video->width * 2;

   gdi_gfx_create();

   ctx_driver = video_context_driver_init_first(gdi,
         settings->arrays.video_context_driver,
         GFX_CTX_GDI_API, 1, 0, false, &ctx_data);
   if (!ctx_driver)
      goto error;

   if (ctx_data)
      gdi->ctx_data = ctx_data;

   gdi->ctx_driver  = ctx_driver;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[GDI]: Found GDI context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);

   full_x      = mode.width;
   full_y      = mode.height;
   mode.width  = 0;
   mode.height = 0;

   RARCH_LOG("[GDI]: Detecting screen resolution %ux%u.\n", full_x, full_y);

   win_width   = video->width;
   win_height  = video->height;

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

   RARCH_LOG("[GDI]: Using resolution %ux%u\n", temp_width, temp_height);

   inp.input      = input;
   inp.input_data = input_data;

   video_context_driver_input_driver(&inp);

   if (settings->bools.video_font_enable)
      font_driver_init_osd(gdi, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_GDI);

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
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   BITMAPINFO *info;
   gfx_ctx_mode_t mode;
   const void *frame_to_copy = frame;
   unsigned width            = 0;
   unsigned height           = 0;
   unsigned bits             = gdi_video_bits;
   bool draw                 = true;
   gdi_t *gdi                = (gdi_t*)data;
   HWND hwnd                 = win32_get_window();

   /* FIXME: Force these settings off as they interfere with the rendering */
   video_info->xmb_shadows_enable   = false;
   video_info->menu_shader_pipeline = 0;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (  gdi_video_width  != frame_width  ||
         gdi_video_height != frame_height ||
         gdi_video_pitch  != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         gdi_video_width  = frame_width;
         gdi_video_height = frame_height;
         gdi_video_pitch  = pitch;
      }
   }

   if (gdi_menu_frame && video_info->menu_is_alive)
   {
      frame_to_copy = gdi_menu_frame;
      width         = gdi_menu_width;
      height        = gdi_menu_height;
      pitch         = gdi_menu_pitch;
      bits          = gdi_menu_bits;
   }
   else
   {
      width         = gdi_video_width;
      height        = gdi_video_height;
      pitch         = gdi_video_pitch;

      if (  frame_width  == 4 &&
            frame_height == 4 &&
            (frame_width < width && frame_height < height)
         )
         draw = false;

      if (video_info->menu_is_alive)
         draw = false;
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

      if (gdi_lte_win98)
      {
         unsigned short *tmp = NULL;

         if (gdi_temp_buf)
            free(gdi_temp_buf);

         tmp = (unsigned short*)malloc(width * height
               * sizeof(unsigned short));

         if (tmp)
            gdi_temp_buf = tmp;
      }
   }

   video_context_driver_get_video_size(&mode);

   gdi->screen_width           = mode.width;
   gdi->screen_height          = mode.height;

   info                        = (BITMAPINFO*)
      calloc(1, sizeof(*info) + (3 * sizeof(RGBQUAD)));

   info->bmiHeader.biBitCount  = bits;
   info->bmiHeader.biWidth     = pitch / (bits / 8);
   info->bmiHeader.biHeight    = -height;
   info->bmiHeader.biPlanes    = 1;
   info->bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
   info->bmiHeader.biSizeImage = 0;

   if (bits == 16)
   {
      if (gdi_lte_win98 && gdi_temp_buf)
      {
         /* Win98 and below cannot use BI_BITFIELDS with RGB444,
          * so convert it to RGB555 first. */
         unsigned x, y;

         for (y = 0; y < height; y++)
         {
            for (x = 0; x < width; x++)
            {
               unsigned short pixel = ((unsigned short*)frame_to_copy)[width * y + x];
               gdi_temp_buf[width * y + x] = (pixel & 0xF000) >> 1 | (pixel & 0x0F00) >> 2 | (pixel & 0x00F0) >> 3;
            }
         }

         frame_to_copy = gdi_temp_buf;
         info->bmiHeader.biCompression = BI_RGB;
      }
      else
      {
         unsigned *masks = (unsigned*)info->bmiColors;

         info->bmiHeader.biCompression = BI_BITFIELDS;

         /* default 16-bit format on Windows is XRGB1555 */
         if (frame_to_copy == gdi_menu_frame)
         {
            /* map RGB444 color bits for RGUI */
            masks[0] = 0xF000;
            masks[1] = 0x0F00;
            masks[2] = 0x00F0;
         }
         else
         {
            /* map RGB565 color bits for core */
            masks[0] = 0xF800;
            masks[1] = 0x07E0;
            masks[2] = 0x001F;
         }
      }
   }
   else
      info->bmiHeader.biCompression = BI_RGB;

   if (draw)
      StretchDIBits(gdi->memDC, 0, 0, width, height, 0, 0, width, height,
            frame_to_copy, info, DIB_RGB_COLORS, SRCCOPY);

   SelectObject(gdi->memDC, gdi->bmp_old);

   free(info);

   if (msg)
      font_driver_render_msg(gdi, video_info, msg, NULL, NULL);

   InvalidateRect(hwnd, NULL, false);

   video_info->cb_update_window_title(
         video_info->context_data, video_info);

   return true;
}

static void gdi_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool gdi_gfx_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool quit            = false;
   bool resize          = false;
   bool ret             = false;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
   gdi_t *gdi           = (gdi_t*)data;

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   gdi->ctx_driver->check_window(gdi->ctx_data,
            &quit, &resize, &temp_width, &temp_height, is_shutdown);

   ret = !quit;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
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
   HWND hwnd  = win32_get_window();

   if (gdi_menu_frame)
   {
      free(gdi_menu_frame);
      gdi_menu_frame = NULL;
   }

   if (gdi_temp_buf)
   {
      free(gdi_temp_buf);
      gdi_temp_buf = NULL;
   }

   if (!gdi)
      return;

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

   if ( !gdi_menu_frame            ||
         gdi_menu_width != width   ||
         gdi_menu_height != height ||
         gdi_menu_pitch != pitch)
   {
      if (pitch && height)
      {
         unsigned char *tmp = (unsigned char*)malloc(pitch * height);

         if (tmp)
            gdi_menu_frame = tmp;
      }
   }

   if (gdi_menu_frame && frame && pitch && height)
   {
      memcpy(gdi_menu_frame, frame, pitch * height);
      gdi_menu_width  = width;
      gdi_menu_height = height;
      gdi_menu_pitch  = pitch;
      gdi_menu_bits   = rgb32 ? 32 : 16;
   }
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

static void gdi_unload_texture(void *data, uintptr_t handle)
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

static uint32_t gdi_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
}

static const video_poke_interface_t gdi_poke_interface = {
   gdi_get_flags,
   gdi_load_texture,
   gdi_unload_texture,
   gdi_set_video_mode,
   win32_get_refresh_rate,
   NULL,
   gdi_get_video_output_size,
   gdi_get_video_output_prev,
   gdi_get_video_output_next,
   NULL,
   NULL,
   NULL,
   NULL,
   gdi_set_texture_frame,
   NULL,
   font_driver_render_msg,
   NULL,
   NULL,                         /* grab_mouse_toggle */
   NULL,                         /* get_current_shader */
   NULL,                         /* get_current_software_framebuffer */
   NULL                          /* get_hw_render_interface */
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

bool gdi_has_menu_frame(void)
{
   return (gdi_menu_frame != NULL);
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
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
  gdi_gfx_get_poke_interface,
};
