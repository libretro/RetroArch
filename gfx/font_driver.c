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

struct font_t {
   font_t *next;
   const font_backend_t *backend;
   void                 *backend_data;

   char *filename;
   unsigned hash; /* filename hash */
   float    size;

   int ref;
};

typedef struct {
   const font_t          *font;
   const font_renderer_t *renderer;
   void                  *data;
} fcache_t;

static font_t *g_fonts = NULL;

static enum font_driver_render_api g_font_api = FONT_DRIVER_RENDER_DONT_CARE;
static fcache_t *g_fcache         = NULL;
static fcache_t *g_fcache_last    = NULL;
static size_t    g_fcache_size    = 0;
static size_t    g_fcache_entries = 0;

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

static bool font_init_first(const void **font_driver, void **font_handle,
      void *video_data, const font_t *font,
      enum font_driver_render_api api)
{
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

static fcache_t *fcache_find(const font_t *font)
{
   fcache_t *entry     = g_fcache;

   if (g_fcache_last && g_fcache_last->font == font)
      return g_fcache_last;

   if (g_fcache)
   {
      const fcache_t *end = &g_fcache[g_fcache_entries];

      while (entry < end)
      {
         if (entry->font == font)
            return entry;

         entry++;
      }
   }

   return NULL;
}

static fcache_t *fcache_push(const font_t *font,
      const font_renderer_t *renderer, void *renderer_data)
{
   fcache_t *entry = NULL;
   bool reuse      = false;

   if (g_fcache)
   {
      const fcache_t *end = &g_fcache[g_fcache_entries];

      for (entry = g_fcache; entry < end; ++entry)
      {
         if (!entry->renderer)
         {
            reuse = true;
            break;
         }
      }
   }

   if (!g_fcache || (!reuse && g_fcache_entries == g_fcache_size))
   {
      const int amount = 4;
      g_fcache = (fcache_t*)realloc(g_fcache,
            sizeof(fcache_t) * (g_fcache_size + amount));

      g_fcache_size += amount;
   }

   if (!reuse)
      entry = &g_fcache[g_fcache_entries++];

   entry->font     = font;
   entry->renderer = renderer;
   entry->data     = renderer_data;

   return entry;
}

static bool get_font_renderer_for(const font_t *font, bool video_thread, const font_renderer_t **renderer, void **data)
{
   void *video_data = video_driver_get_ptr(false);
#ifdef HAVE_THREADS
   if (!video_thread)
   {
      settings_t *settings = config_get_ptr();
      if (settings->video.threaded && !video_driver_is_hw_context())
         return video_thread_font_init((const void**)renderer, data, video_data,
               font, g_font_api, font_init_first);
   }
#endif

   return font_init_first((const void**)renderer, data, video_data, font, g_font_api);
}

static const fcache_t *fcache_find_or_create(const font_t *font, bool video_thread)
{
   const fcache_t *cached = fcache_find(font);

   if (!cached)
   {
      const font_renderer_t *renderer;
      void *data = NULL;

      if (get_font_renderer_for(font, video_thread, &renderer, &data))
         cached = fcache_push(font, renderer, data);
   }

   return cached;
}

static font_t *find_font_instance(unsigned hash, float size)
{
   font_t *font = g_fonts;

   while (font) {
      if (font->hash == hash && fabs(font->size - size) < 0.0001)
         return font;
      font = font->next;
   }

   return NULL;
}

static font_t *create_font_instance(const char *filename, float size)
{
   font_t *font = NULL;
   const font_backend_t **backend;

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
         font = (font_t*)calloc(sizeof(*font), 1);

         if (!font)
         {
            RARCH_ERR("[font] Failed to load %s (size=%.2f): "
                      "allocation failed.\n", path, size);
            return NULL;
         }

         font->filename     = strdup(path);
         font->backend      = *backend;
         font->backend_data = data;
         font->hash = djb2_calculate(path);
         font->size = size;
         font->ref  = 1;

         font->next = g_fonts;
         g_fonts    = font;
         break;
      }
   }

   if (font)
      RARCH_LOG("[font] Loaded '%s' (size=%.2f backend=%s)\n",
                font->filename, font->size, font->backend->ident);
   else
      RARCH_ERR("[font] Failed to load %s (size=%.2f): no working backend\n",
                filename, size);

   return font;
}

const font_t *font_load(const char *filename, float size)
{
   const font_t *font;

   font = find_font_instance(djb2_calculate(filename ? filename : ""), size);

   if (font)
      font = font_ref(font);
   else
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

static void font_invalidate_cache(const font_t *font)
{
   fcache_t *cached = fcache_find(font);

   if (cached)
   {
      cached->renderer->free(cached->data);
      cached->renderer = NULL;
      cached->data     = NULL;
   }
}

const font_t *font_unref(const font_t *font)
{
   font_t *f = (font_t*)font; /* YES */

   if (--f->ref <= 0)
   {
      font_t *it, *prev = NULL;

      for (it = g_fonts; it; it = it->next)
      {
         if (it == f)
         {
            if (prev)
               prev->next = it->next;

            if (it == g_fonts)
               g_fonts = it->next;

            break;
         }
         prev = it;
      }

      font_invalidate_cache(f);

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

void font_set_api(enum font_driver_render_api api)
{
   if (g_font_api == FONT_DRIVER_RENDER_DONT_CARE)
      g_font_api = api;
}

void font_render_full(const font_t *font, const char *text, const font_params_t *params)
{
   const fcache_t *cached = fcache_find_or_create(font, true);

   if (cached)
      cached->renderer->render_msg(cached->data, text, params);
}

void font_render(const font_t *font, const char *text, float x, float y, enum text_alignment align, uint32_t color)
{
   font_params_t params;

   params.x           = x;
   params.y           = y;
   params.scale       = 1.0f;
   params.drop_mod    = 0.0f;
   params.drop_x      = 0.0f;
   params.drop_y      = 0.0f;
   params.color       = color;
   params.full_screen = true;
   params.text_align  = align;

   font_render_full(font, text, &params);
}

int font_width(const font_t *font, const char *text, unsigned len, float scale)
{
   const fcache_t *cache = fcache_find_or_create(font, true);

   if (cache && cache->renderer->get_message_width)
      return cache->renderer->get_message_width(cache->data, text, len, scale);

   return -1;
}

void font_invalidate_caches(void)
{
   if (g_fcache)
   {
      const fcache_t *end = &g_fcache[g_fcache_entries];
      fcache_t *entry     = g_fcache;

      for (entry = g_fcache; entry < end; ++entry)
      {
         if (entry->data)
            entry->renderer->free(entry->data);

         entry->renderer = NULL;
         entry->data     = NULL;
      }

      g_fcache_entries = 0;
   }
}

void font_bind_block(const font_t *font, void *block)
{
   const fcache_t *cache = fcache_find_or_create(font, true);

   if (cache && cache->renderer->bind_block)
      cache->renderer->bind_block(cache->data, block);
}

void font_flush(const font_t *font)
{
   const fcache_t *cache = fcache_find_or_create(font, true);

   if (cache && cache->renderer->flush)
      cache->renderer->flush(cache->data);
}
