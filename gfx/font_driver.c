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

#include "../configuration.h"
#include "../verbosity.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <rhash.h>

static const font_backend_t *font_backends[] = {
#ifdef HAVE_FREETYPE
   &freetype_font_backend,
#endif
#if defined(__APPLE__) && defined(HAVE_CORETEXT)
   &coretext_font_backend,
#endif
#ifdef HAVE_STB_FONT
   &stb_font_backend,
#endif
   &bitmap_font_backend,
   NULL
};

static const font_renderer_t *font_osd_driver = NULL;
static void *font_osd_data = NULL;

int font_renderer_create_default(const void **data, void **handle,
      const char *font_path, unsigned font_size)
{

   unsigned i;
   const font_backend_t **drv = 
      (const font_backend_t**)data;

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
      void *video_data, const font_t *font)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(d3d_font_backends); i++)
   {
      void *data = d3d_font_backends[i]->init(video_data, font);

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
   &gl_font_renderer,
#if defined(HAVE_LIBDBGFONT)
   &libdbg_font,
#endif
   NULL,
};

static bool gl_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const font_t *font)
{
   unsigned i;

   for (i = 0; gl_font_backends[i]; i++)
   {
      void *data = gl_font_backends[i]->init(video_data, font);

      if (!data)
         continue;

      *font_driver = gl_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

#ifdef HAVE_VULKAN
static const font_renderer_t *vulkan_font_backends[] = {
   &vulkan_font_renderer,
   NULL,
};

static bool vulkan_font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const font_t *font)
{
   unsigned i;

   for (i = 0; vulkan_font_backends[i]; i++)
   {
      void *data = vulkan_font_backends[i]->init(video_data, font);

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
      void *video_data, const font_t *font)
{
   unsigned i;

   for (i = 0; vita2d_font_backends[i]; i++)
   {
      void *data = vita2d_font_backends[i]->init(
            video_data, font);

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
      void *video_data, const font_t *font)
{
   unsigned i;

   for (i = 0; ctr_font_backends[i]; i++)
   {
      void *data = ctr_font_backends[i]->init(video_data, font);

      if (!data)
         continue;

      *font_driver = ctr_font_backends[i];
      *font_handle = data;
      return true;
   }

   return false;
}
#endif

static bool font_init_first(
      const void **font_driver, void **font_handle,
      void *video_data, const font_t *font,
      enum font_driver_render_api api)
{

   if (font_osd_driver)
   {
      void *data = font_osd_driver->init(video_data, font);

      if (data)
      {
         *font_driver = font_osd_driver;
         *font_handle = data;
         return true;
      }

      RARCH_WARN("[font] Failed to reuse renderer.\n");
   }

   switch (api)
   {
#ifdef HAVE_D3D
      case FONT_DRIVER_RENDER_DIRECT3D_API:
         return d3d_font_init_first(font_driver, font_handle,
               video_data, font);
#endif
#ifdef HAVE_OPENGL
      case FONT_DRIVER_RENDER_OPENGL_API:
         return gl_font_init_first(font_driver, font_handle,
               video_data, font);
#endif
#ifdef HAVE_VULKAN
      case FONT_DRIVER_RENDER_VULKAN_API:
         return vulkan_font_init_first(font_driver, font_handle,
               video_data, font);
#endif
#ifdef HAVE_VITA2D
      case FONT_DRIVER_RENDER_VITA2D:
         return vita2d_font_init_first(font_driver, font_handle,
               video_data, font);
#endif
#ifdef _3DS
      case FONT_DRIVER_RENDER_CTR:
         return ctr_font_init_first(font_driver, font_handle,
               video_data, font);
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
   if (!font_osd_driver || !font_osd_driver->render_msg)
      return false;
   return true;
}

void font_driver_render_msg(void *font_data,
      const char *msg, const struct font_params *params)
{

   if (font_osd_driver && font_osd_driver->render_msg)
      font_osd_driver->render_msg(font_data 
            ? font_data : font_osd_data, msg, params);
}

void font_driver_bind_block(void *font_data, void *block)
{
   void             *new_font_data = font_data 
      ? font_data : font_osd_data;

   if (font_osd_driver && font_osd_driver->bind_block)
      font_osd_driver->bind_block(new_font_data, block);
}

void font_driver_flush(void *data)
{
   if (font_osd_driver && font_osd_driver->flush)
      font_osd_driver->flush(data);
}

int font_driver_get_message_width(void *data,
      const char *msg, unsigned len, float scale)
{

   if (!font_osd_driver || !font_osd_driver->get_message_width)
      return -1;
   return font_osd_driver->get_message_width(data, msg, len, scale);
}

void font_driver_free(void *data)
{
   if (font_osd_driver && font_osd_driver->free)
      font_osd_driver->free(data ? data : font_osd_data);

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

   /* FIXME: check PSP, PS3(LIBDBG), Vita and D3D. */
   const font_t *font;
   bool result = false;

   /* TODO: font_load() and font_unref() should be handled by the caller */
   font = font_load(font_path, font_size);

   if (!font)
      return false;

#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (threading_hint 
         && settings->video.threaded 
         && !video_driver_is_hw_context())
      result = video_thread_font_init(new_font_driver, new_font_handle,
            data, font, api, font_init_first);
#endif

   result = font_init_first(new_font_driver, new_font_handle,
         data, font, api);

   font_unref(font);

   return result;
}


struct font_t {
   font_t *next;
   const font_backend_t *backend;
   void                 *backend_data;

   char *filename;
   unsigned hash; /* filename hash */
   float    size;

   int ref;
};

#if 0 /* disabled until all relevant files are updated */
static font_t *g_fonts = NULL;

static font_t *find_font_instance(unsigned hash, float size)
{
   font_t *font = g_fonts;

   while (font) {
      if (font->hash == hash && fabs(font->size - size) < 0.0001)
         return font;
   }

   return NULL;
}
#endif

static font_t *create_font_instance(const char *filename, float size)
{
   font_t *font = (font_t*)calloc(sizeof(*font), 1);
   const font_backend_t **backend;

   if (!font)
   {
      RARCH_ERR("[font] Failed to allocate the font structure.\n");
      return NULL;
   }

   font->filename = strdup(filename ? filename : "");
   font->hash = djb2_calculate(filename ? filename : "");
   font->size = size;

   for (backend = font_backends; *backend; ++backend)
   {
      const char *path = filename;
      void *data;

      if (!path)
         path = (*backend)->get_default_font();

      if (!path)
         continue;

      data = (*backend)->init(path, size);

      if (data)
      {
         font->backend      = *backend;
         font->backend_data = data;

         break;
      }
   }

   if (font->backend)
   {
      RARCH_LOG("[font] Loaded '%s' (size=%.2f backend=%s)\n",
                filename, size, font->backend->ident);
#if 0 /* disabled until all relevant files are updated */
      font->next = g_fonts;
      g_fonts = font;
#endif
      font->ref = 1;
   }
   else
   {
      RARCH_ERR("[font] Failed to load %s (size=%.2f): no working backend\n",
                filename, size);
      free(font->filename);
      free(font);
      font = NULL;
   }

   return font;
}

const font_t *font_load(const char *filename, float size)
{
   font_t *font;

#if 0 /* disabled until all relevant files are updated */
   font = find_font_instance(djb2_calculate(filename ? filename : ""), size);

   if (font)
      font_ref(font);
   else
#endif
      font = create_font_instance(filename, size);

   return font;
}

/* XXX: ref and unref might have concurrency issues */
const font_t *font_ref(const font_t *font)
{
   font_t *f = (font_t*)font; /* yes */

   if (f->ref >= 0)
      f->ref++;

   return font;
}

const font_t *font_unref(const font_t *font)
{
   font_t *f = (font_t*)font; /* YES */

   if (--f->ref <= 0)
   {
#if 0 /* disabled until all relevant files are updated */
      font_t *it, *prev;

      for (it = g_fonts; it; it = it->next)
      {
         if (it == f)
         {
            if (prev)
               prev->next = it->next;

            break;
         }
         prev = it;
      }
#endif
      f->backend->free(f->backend_data);
      free(font->filename);
      free(f);

      f = NULL;
   }

   return f;
}

const char *font_get_filename(const font_t *font)
{
   return font->filename;
}

float font_get_size(const font_t *font)
{
   return font->size;
}

const struct font_atlas *font_get_atlas(const font_t *font)
{
   return font->backend->get_atlas(font->backend_data);
}

const struct font_glyph *font_get_glyph(const font_t *font, uint32_t codepoint)
{
   const struct font_glyph *glyph = font->backend->get_glyph(font->backend_data, codepoint);

   if (!glyph) /* Do something smarter here ... */
      glyph = font->backend->get_glyph(font->backend_data, '?');

   return glyph;
}

int font_get_line_height(const font_t *font)
{
   if (font->backend->get_line_height)
      return font->backend->get_line_height(font->backend_data);
   else
      return 0;
}
