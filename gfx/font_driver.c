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

#include "font_driver.h"
#include "video_thread_wrapper.h"

#include "../configuration.h"
#include "../verbosity.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdlib.h>

static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
   &coretext_font_renderer,
#endif
#ifdef HAVE_STB_FONT
#if defined(VITA) || defined(WIIU) || defined(ANDROID) || defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) || defined(_WIN32) && !defined(_XBOX) && defined(_MSC_VER) && _MSC_VER > 1400
   &stb_unicode_font_renderer,
#else
   &stb_font_renderer,
#endif
#endif
   &bitmap_font_renderer,
   NULL
};

static void *video_font_driver = NULL;

int font_renderer_create_default(const void **data, void **handle,
      const char *font_path, unsigned font_size)
{

   unsigned i;
   const font_renderer_driver_t **drv = 
      (const font_renderer_driver_t**)data;

   for (i = 0; font_backends[i]; i++)
   {
      const char *path = font_path;

      if (!path)
         path = font_backends[i]->get_default_font();
      if (!path)
         continue;

      *handle = font_backends[i]->init(path, font_size);
      if (*handle)
      {
         RARCH_LOG("[Font]: Using font rendering backend: %s.\n",
               font_backends[i]->ident);
         *drv = font_backends[i];
         return 1;
      }
      else
         RARCH_ERR("Failed to create rendering backend: %s.\n",
               font_backends[i]->ident);
   }

   *drv = NULL;
   *handle = NULL;

   return 0;
}

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
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d_font_backends); i++)
   {
      void *data = d3d_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

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
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gl_font_backends[i]; i++)
   {
      void *data = gl_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gl_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_CACA
static const font_renderer_t *caca_font_backends[] = {
   &caca_font,
   NULL,
};

static bool caca_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; caca_font_backends[i]; i++)
   {
      void *data = caca_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = caca_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef DJGPP
static const font_renderer_t *vga_font_backends[] = {
   &vga_font,
   NULL,
};

static bool vga_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vga_font_backends[i]; i++)
   {
      void *data = vga_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vga_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#if defined(_WIN32) && !defined(_XBOX)
static const font_renderer_t *gdi_font_backends[] = {
   &gdi_font,
   NULL,
};

static bool gdi_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; gdi_font_backends[i]; i++)
   {
      void *data = gdi_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = gdi_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_VULKAN
static const font_renderer_t *vulkan_font_backends[] = {
   &vulkan_raster_font,
   NULL,
};

static bool vulkan_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vulkan_font_backends[i]; i++)
   {
      void *data = vulkan_font_backends[i]->init(video_data,
            font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vulkan_font_backends[i];
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
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; vita2d_font_backends[i]; i++)
   {
      void *data = vita2d_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = vita2d_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef _3DS
static const font_renderer_t *ctr_font_backends[] = {
   &ctr_font
};

static bool ctr_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; ctr_font_backends[i]; i++)
   {
      void *data = ctr_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = ctr_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef WIIU
static const font_renderer_t *wiiu_font_backends[] = {
   &wiiu_font,
   NULL
};

static bool wiiu_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path,
      float font_size, bool is_threaded)
{
   unsigned i;

   for (i = 0; wiiu_font_backends[i]; i++)
   {
      void *data = wiiu_font_backends[i]->init(
            video_data, font_path, font_size,
            is_threaded);

      if (!data)
         continue;

      *font_driver = wiiu_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

static bool font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const char *font_path, float font_size,
      enum font_driver_render_api api, bool is_threaded)
{
   if (font_path && !font_path[0])
      font_path = NULL;

   switch (api)
   {
#ifdef HAVE_D3D
      case FONT_DRIVER_RENDER_DIRECT3D_API:
         return d3d_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_OPENGL
      case FONT_DRIVER_RENDER_OPENGL_API:
         return gl_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_VULKAN
      case FONT_DRIVER_RENDER_VULKAN_API:
         return vulkan_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_VITA2D
      case FONT_DRIVER_RENDER_VITA2D:
         return vita2d_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef _3DS
      case FONT_DRIVER_RENDER_CTR:
         return ctr_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef WIIU
      case FONT_DRIVER_RENDER_WIIU:
         return wiiu_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef HAVE_CACA
      case FONT_DRIVER_RENDER_CACA:
         return caca_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#if defined(_WIN32) && !defined(_XBOX)
      case FONT_DRIVER_RENDER_GDI:
         return gdi_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
#ifdef DJGPP
      case FONT_DRIVER_RENDER_VGA:
         return vga_font_init_first(font_driver, font_handle,
               video_data, font_path, font_size, is_threaded);
#endif
      case FONT_DRIVER_RENDER_DONT_CARE:
         /* TODO/FIXME - lookup graphics driver's 'API' */
         break;
      default:
         break;
   }

   return false;
}

void font_driver_render_msg(
      video_frame_info_t *video_info,
      void *font_data,
      const char *msg, const void *params)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (font && font->renderer && font->renderer->render_msg)
      font->renderer->render_msg(video_info, font->renderer_data, msg, params);
}

void font_driver_bind_block(void *font_data, void *block)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);

   if (font && font->renderer && font->renderer->bind_block)
      font->renderer->bind_block(font->renderer_data, block);
}

void font_driver_flush(unsigned width, unsigned height, void *font_data)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (font && font->renderer && font->renderer->flush)
      font->renderer->flush(width, height, font->renderer_data);
}

int font_driver_get_message_width(void *font_data,
      const char *msg, unsigned len, float scale)
{
   font_data_t *font = (font_data_t*)(font_data ? font_data : video_font_driver);
   if (font && font->renderer && font->renderer->get_message_width)
      return font->renderer->get_message_width(font->renderer_data, msg, len, scale);
   return -1;
}

void font_driver_free(void *font_data)
{
   font_data_t *font = (font_data_t*)font_data;

   if (font)
   {
      bool is_threaded        = false;
#ifdef HAVE_THREADS
      bool *is_threaded_tmp   = video_driver_get_threaded();
      is_threaded             = *is_threaded_tmp;
#endif

      if (font->renderer && font->renderer->free)
         font->renderer->free(font->renderer_data, is_threaded);

      font->renderer      = NULL;
      font->renderer_data = NULL;

      free(font);
   }
}

font_data_t *font_driver_init_first(
      void *video_data, const char *font_path, float font_size,
      bool threading_hint, bool is_threaded,
      enum font_driver_render_api api)
{
   const void *font_driver = NULL;
   void *font_handle       = NULL;
   bool ok                 = false;
#ifdef HAVE_THREADS

   if (     threading_hint 
         && is_threaded 
         && !video_driver_is_hw_context())
      ok = video_thread_font_init(&font_driver, &font_handle,
            video_data, font_path, font_size, api, font_init_first,
            is_threaded);
   else
#endif
   ok = font_init_first(&font_driver, &font_handle,
         video_data, font_path, font_size, api, is_threaded);

   if (ok)
   {
      font_data_t *font   = (font_data_t*)calloc(1, sizeof(*font));
      font->renderer      = (const font_renderer_t*)font_driver;
      font->renderer_data = font_handle;
      font->size          = font_size;
      return font;
   }

   return NULL;
}


void font_driver_init_osd(
      void *video_data,
      bool threading_hint, 
      bool is_threaded,
      enum font_driver_render_api api)
{
   settings_t *settings = config_get_ptr();
   if (video_font_driver)
      return;

   video_font_driver = font_driver_init_first(video_data,
         *settings->paths.path_font ? settings->paths.path_font : NULL,
         settings->floats.video_font_size, threading_hint, is_threaded, api);

   if (!video_font_driver)
      RARCH_ERR("[font]: Failed to initialize OSD font.\n");
}

void font_driver_free_osd(void)
{
   if (video_font_driver)
      font_driver_free(video_font_driver);

   video_font_driver = NULL;
}
