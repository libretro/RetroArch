/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2014 - Hans-Kristian Arntzen
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

#include <ctype.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>

#include "../font_driver.h"
#include "../../verbosity.h"

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#define STBTT_STATIC
#define STBRP_STATIC
#define STATIC static INLINE
#include "../../deps/stb/stb_rect_pack.h"
#include "../../deps/stb/stb_truetype.h"
#undef STATIC
#endif

typedef struct
{
   int     line_height;
   struct font_atlas atlas;
   struct font_glyph glyphs[256];
} stb_font_renderer_t;

static struct font_atlas *font_renderer_stb_get_atlas(void *data)
{
   stb_font_renderer_t *self = (stb_font_renderer_t*)data;
   return &self->atlas;
}

static const struct font_glyph *font_renderer_stb_get_glyph(
      void *data, uint32_t code)
{
   stb_font_renderer_t *self = (stb_font_renderer_t*)data;
   return code < 256 ? &self->glyphs[code] : NULL;
}

static void font_renderer_stb_free(void *data)
{
   stb_font_renderer_t *self = (stb_font_renderer_t*)data;

   free(self->atlas.buffer);
   free(self);
}

static bool font_renderer_stb_create_atlas(stb_font_renderer_t *self,
      uint8_t *font_data, float font_size, unsigned width, unsigned height)
{
   int i;
   stbtt_packedchar   chardata[256];
   stbtt_pack_context pc = {NULL};

   if (width > 2048 || height > 2048)
   {
      RARCH_WARN("[stb] Font atlas too big: %ux%u\n", width, height);
      goto error;
   }

   if (self->atlas.buffer)
      free(self->atlas.buffer);

   self->atlas.buffer = (uint8_t*)calloc(height, width);
   self->atlas.width  = width;
   self->atlas.height = height;

   if (!self->atlas.buffer)
      goto error;

   stbtt_PackBegin(&pc, self->atlas.buffer,
         self->atlas.width, self->atlas.height,
         self->atlas.width, 1, NULL);

   stbtt_PackFontRange(&pc, font_data, 0, font_size, 0, 256, chardata);
   stbtt_PackEnd(&pc);

   self->atlas.dirty = true;

   for (i = 0; i < 256; ++i)
   {
      struct font_glyph *g = &self->glyphs[i];
      stbtt_packedchar  *c = &chardata[i];

      g->advance_x         = c->xadvance;
      g->atlas_offset_x    = c->x0;
      g->atlas_offset_y    = c->y0;
      g->draw_offset_x     = c->xoff;
      g->draw_offset_y     = c->yoff;
      g->width             = c->x1 - c->x0;
      g->height            = c->y1 - c->y0;

      /* Make sure important characters fit */
      if (isalnum(i) && (!g->width || !g->height))
      {
         int new_width  = width  * 1.2;
         int new_height = height * 1.2;

         /* Limit growth to 2048x2048 unless we already reached that */
         if (width < 2048 || height < 2048)
         {
            new_width  = MIN(new_width,  2048);
            new_height = MIN(new_height, 2048);
         }

         return font_renderer_stb_create_atlas(self, font_data, font_size,
               new_width, new_height);
      }
   }

   return true;

error:
   self->atlas.width = self->atlas.height = 0;

   if (self->atlas.buffer)
      free(self->atlas.buffer);

   self->atlas.buffer = NULL;

   return false;
}

static void *font_renderer_stb_init(const char *font_path, float font_size)
{
   int ascent, descent, line_gap;
   stbtt_fontinfo info;
   uint8_t *font_data = NULL;
   stb_font_renderer_t *self = (stb_font_renderer_t*) calloc(1, sizeof(*self));

   /* See https://github.com/nothings/stb/blob/master/stb_truetype.h#L539 */
   font_size = STBTT_POINT_SIZE(font_size);

   if (!self)
      goto error;

   if (!path_is_valid(font_path) || !filestream_read_file(font_path, (void**)&font_data, NULL))
      goto error;

   if (!font_renderer_stb_create_atlas(self, font_data, font_size, 512, 512))
      goto error;

   if (!stbtt_InitFont(&info, font_data, stbtt_GetFontOffsetForIndex(font_data, 0)))
      goto error;

   stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
   self->line_height  = ascent - descent;

   if (font_size < 0)
      self->line_height *= stbtt_ScaleForMappingEmToPixels(&info, -font_size);
   else
      self->line_height *= stbtt_ScaleForPixelHeight(&info, font_size);

   free(font_data);

   return self;

error:
   if (font_data)
      free(font_data);

   if (self)
      font_renderer_stb_free(self);
   return NULL;
}

static const char *font_renderer_stb_get_default_font(void)
{
   static const char *paths[] = {
#if defined(_WIN32) && !defined(__WINRT__)
      "C:\\Windows\\Fonts\\consola.ttf",
      "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
      "/Library/Fonts/Microsoft/Candara.ttf",
      "/Library/Fonts/Verdana.ttf",
      "/Library/Fonts/Tahoma.ttf",
      "/Library/Fonts/Andale Mono.ttf",
      "/Library/Fonts/Courier New.ttf",
#elif defined(__ANDROID_API__)
      "/system/fonts/DroidSansMono.ttf",
      "/system/fonts/CutiveMono.ttf",
      "/system/fonts/DroidSans.ttf",
#elif defined(VITA)
      "vs0:data/external/font/pvf/c041056ts.ttf",
      "vs0:data/external/font/pvf/d013013ds.ttf",
      "vs0:data/external/font/pvf/e046323ms.ttf",
      "vs0:data/external/font/pvf/e046323ts.ttf",
      "vs0:data/external/font/pvf/k006004ds.ttf",
      "vs0:data/external/font/pvf/n023055ms.ttf",
      "vs0:data/external/font/pvf/n023055ts.ttf",
#elif !defined(__WINRT__)
      "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "osd-font.ttf",
#endif
      NULL
   };

   const char **p;

   for (p = paths; *p; ++p)
      if (path_is_valid(*p))
         return *p;

   return NULL;
}

static int font_renderer_stb_get_line_height(void* data)
{
   stb_font_renderer_t *handle = (stb_font_renderer_t*)data;
   return handle->line_height;
}

font_renderer_driver_t stb_font_renderer = {
   font_renderer_stb_init,
   font_renderer_stb_get_atlas,
   font_renderer_stb_get_glyph,
   font_renderer_stb_free,
   font_renderer_stb_get_default_font,
   "stb",
   font_renderer_stb_get_line_height,
};
