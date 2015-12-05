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

#include "font_driver.h"
#include "video_thread_wrapper.h"
#include "../general.h"

#ifdef HAVE_D3D
static const font_renderer_t *d3d_font_backends[] = {
#if defined(_XBOX1)
   &d3d_xdk1_font,
#elif defined(_XBOX360)
   &d3d_xbox360_font,
#elif defined(_WIN32)
   &d3d_win32_font,
#endif
};

static bool d3d_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d_font_backends); i++)
   {
      void *data = d3d_font_backends[i]->init(video_data, font_path, font_size);

      if (!data)
         continue;

      *font_driver = d3d_font_backends[i];
      *font_handle = data;

      return true;
   }

   return false;
}
#endif

#ifdef HAVE_OPENGL
static const font_renderer_t *gl_font_backends[] = {
   &gl_raster_font,
#if defined(HAVE_LIBDBGFONT)
   &libdbg_font,
#endif
   NULL,
};

static bool gl_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size)
{
   unsigned i;

   for (i = 0; gl_font_backends[i]; i++)
   {
      void *data = gl_font_backends[i]->init(video_data, font_path, font_size);

      if (!data)
         continue;

      *font_driver = gl_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_VITA2D
static const font_renderer_t *vita2d_font_backends[] = {
   &vita2d_vita_font
};

static bool vita2d_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size)
{
   unsigned i;

   for (i = 0; vita2d_font_backends[i]; i++)
   {
      void *data = vita2d_font_backends[i]->init(video_data, font_path, font_size);

      if (!data)
         continue;

      *font_driver = vita2d_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

bool font_init_first(const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size,
      enum font_driver_render_api api)
{
   if (font_path && !font_path[0])
      font_path = NULL;

   switch (api)
   {
#ifdef HAVE_D3D
      case FONT_DRIVER_RENDER_DIRECT3D_API:
         return d3d_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size);
#endif
#ifdef HAVE_OPENGL
      case FONT_DRIVER_RENDER_OPENGL_API:
         return gl_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size);
#endif
#ifdef HAVE_VITA2D
      case FONT_DRIVER_RENDER_VITA2D:
         return vita2d_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size);
#endif
      case FONT_DRIVER_RENDER_DONT_CARE:
         /* TODO/FIXME - lookup graphics driver's 'API' */
         break;
      default:
         break;
   }

   return false;
}

bool font_driver_has_render_msg(void)
{
   driver_t          *driver = driver_get_ptr();
   const font_renderer_t *font_ctx = driver->font_osd_driver;
   if (!font_ctx || !font_ctx->render_msg)
      return false;
   return true;
}

void font_driver_render_msg(void *font_data, const char *msg, const struct font_params *params)
{
   driver_t          *driver = driver_get_ptr();
   const font_renderer_t *font_ctx = driver->font_osd_driver;

   if (font_ctx->render_msg)
      font_ctx->render_msg(font_data ? font_data : driver->font_osd_data, msg, params);
}

void font_driver_bind_block(void *block)
{
   driver_t          *driver = driver_get_ptr();
   const font_renderer_t *font_ctx = driver->font_osd_driver;

   if (font_ctx->bind_block)
      font_ctx->bind_block(driver->font_osd_data, block);
}

void font_driver_free(void *data)
{
   driver_t *driver = driver_get_ptr();
   const font_renderer_t *font_ctx = (const font_renderer_t*)driver->font_osd_driver;

   if (font_ctx->free)
      font_ctx->free(data ? data : driver->font_osd_data);

   if (data)
      return;

   driver->font_osd_data   = NULL;
   driver->font_osd_driver = NULL;
}

bool font_driver_init_first(const void **font_driver, void *font_handle,
      void *data, const char *font_path, float font_size,
      bool threading_hint,
      enum font_driver_render_api api)
{
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();
   driver_t *driver             = driver_get_ptr();
   const void **new_font_driver = font_driver ? font_driver 
      : (const void**)&driver->font_osd_driver;
   void *new_font_handle        = font_handle ? font_handle 
      : &driver->font_osd_data;

   if (threading_hint && settings->video.threaded && !hw_render->context_type)
   {
      thread_packet_t pkt;
      thread_video_t *thr = (thread_video_t*)video_driver_get_ptr(true);

      if (!thr)
         return false;

      pkt.type                       = CMD_FONT_INIT;
      pkt.data.font_init.method      = font_init_first;
      pkt.data.font_init.font_driver = new_font_driver;
      pkt.data.font_init.font_handle = new_font_handle;
      pkt.data.font_init.video_data  = data;
      pkt.data.font_init.font_path   = font_path;
      pkt.data.font_init.font_size   = font_size;
      pkt.data.font_init.api         = api;

      thr->send_and_wait(thr, &pkt);

      return pkt.data.font_init.return_value;
   }

   return font_init_first(new_font_driver, new_font_handle,
         data, font_path, font_size, api);
}
