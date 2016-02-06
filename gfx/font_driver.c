/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include "../verbosity.h"

static const font_renderer_driver_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &freetype_font_renderer,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
   &coretext_font_renderer,
#endif
#ifdef HAVE_STB_FONT
   &stb_font_renderer,
#endif
   &bitmap_font_renderer,
   NULL
};

static const struct font_renderer *font_osd_driver;

static void *font_osd_data;

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
         RARCH_LOG("Using font rendering backend: %s.\n",
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
      void *video_data, const char *font_path, float font_size)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d_font_backends); i++)
   {
      void *data = d3d_font_backends[i]->init(
            video_data, font_path, font_size);

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
      void *data = gl_font_backends[i]->init(
            video_data, font_path, font_size);

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
      void *data = vita2d_font_backends[i]->init(
            video_data, font_path, font_size);

      if (!data)
         continue;

      *font_driver = vita2d_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

static bool font_init_first(
      const void **font_driver, void **font_handle,
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
   const font_renderer_t *font_ctx = font_osd_driver;
   if (!font_ctx || !font_ctx->render_msg)
      return false;
   return true;
}

void font_driver_render_msg(void *font_data,
      const char *msg, const struct font_params *params)
{
   const font_renderer_t *font_ctx = font_osd_driver;

   if (font_ctx->render_msg)
      font_ctx->render_msg(font_data 
            ? font_data : font_osd_data, msg, params);
}

void font_driver_bind_block(void *font_data, void *block)
{
   const font_renderer_t *font_ctx = font_osd_driver;
   void             *new_font_data = font_data 
      ? font_data : font_osd_data;

   if (font_ctx->bind_block)
      font_ctx->bind_block(new_font_data, block);
}

void font_driver_flush(void *data)
{
   const font_renderer_t *font_ctx = font_osd_driver;

   if (font_ctx->flush)
      font_ctx->flush(data);
}

int font_driver_get_message_width(void *data,
      const char *msg, unsigned len, float scale)
{
   const font_renderer_t *font_ctx = font_osd_driver;

   if (!font_ctx || !font_ctx->get_message_width)
      return -1;
   return font_ctx->get_message_width(data, msg, len, scale);
}

void font_driver_free(void *data)
{
   const font_renderer_t *font_ctx = 
      (const font_renderer_t*)font_osd_driver;

   if (font_ctx->free)
      font_ctx->free(data ? data : font_osd_data);

   if (data)
      return;

   font_osd_data   = NULL;
   font_osd_driver = NULL;
}

bool font_driver_init_first(
      const void **font_driver, void **font_handle,
      void *data, const char *font_path, float font_size,
      bool threading_hint,
      enum font_driver_render_api api)
{
   const void **new_font_driver = font_driver ? font_driver 
      : (const void**)&font_osd_driver;
   void **new_font_handle        = font_handle ? font_handle 
      : (void**)&font_osd_data;
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();

   if (threading_hint && settings->video.threaded && !hw_render->context_type)
      return rarch_threaded_video_font_init(new_font_driver, new_font_handle,
            data, font_path, font_size, api, font_init_first);
#endif

   return font_init_first(new_font_driver, new_font_handle,
         data, font_path, font_size, api);
}

