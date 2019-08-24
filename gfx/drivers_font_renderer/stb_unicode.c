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

#ifdef WIIU
#include <wiiu/os.h>
#endif

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

#define STB_UNICODE_ATLAS_ROWS 16
#define STB_UNICODE_ATLAS_COLS 16
#define STB_UNICODE_ATLAS_SIZE (STB_UNICODE_ATLAS_ROWS * STB_UNICODE_ATLAS_COLS)

typedef struct stb_unicode_atlas_slot
{
   struct font_glyph glyph;
   unsigned charcode;
   unsigned last_used;
   struct stb_unicode_atlas_slot* next;
}stb_unicode_atlas_slot_t;

typedef struct
{
   uint8_t *font_data;
   stbtt_fontinfo info;

   int max_glyph_width;
   int max_glyph_height;
   int line_height;
   float scale_factor;

   struct font_atlas atlas;
   stb_unicode_atlas_slot_t atlas_slots[STB_UNICODE_ATLAS_SIZE];
   stb_unicode_atlas_slot_t* uc_map[0x100];
   unsigned usage_counter;
} stb_unicode_font_renderer_t;

/* Ugly little thing... */
static int INLINE round_away_from_zero(float f)
{
   double round = (f < 0.0) ? floor((double)f) : ceil((double)f);
   return (int)round;
}

static struct font_atlas *font_renderer_stb_unicode_get_atlas(void *data)
{
   stb_unicode_font_renderer_t *self = (stb_unicode_font_renderer_t*)data;
   return &self->atlas;
}

static void font_renderer_stb_unicode_free(void *data)
{
   stb_unicode_font_renderer_t *self = (stb_unicode_font_renderer_t*)data;

   free(self->atlas.buffer);
   free(self->font_data);
   free(self);
}

static stb_unicode_atlas_slot_t* font_renderer_stb_unicode_get_slot(stb_unicode_font_renderer_t *handle)
{
   int i, map_id;
   unsigned oldest = 0;

   for (i = 1; i < STB_UNICODE_ATLAS_SIZE; i++)
      if((handle->usage_counter - handle->atlas_slots[i].last_used) >
         (handle->usage_counter - handle->atlas_slots[oldest].last_used))
         oldest = i;

   /* remove from map */
   map_id = handle->atlas_slots[oldest].charcode & 0xFF;
   if(handle->uc_map[map_id] == &handle->atlas_slots[oldest])
      handle->uc_map[map_id] = handle->atlas_slots[oldest].next;
   else if (handle->uc_map[map_id])
   {
      stb_unicode_atlas_slot_t* ptr = handle->uc_map[map_id];
      while(ptr->next && ptr->next != &handle->atlas_slots[oldest])
         ptr = ptr->next;
      ptr->next = handle->atlas_slots[oldest].next;
   }

   return &handle->atlas_slots[oldest];
}

static const struct font_glyph *font_renderer_stb_unicode_get_glyph(
      void *data, uint32_t charcode)
{
   int glyph_index                      = 0;
   int x0                               = 0;
   int y1                               = 0;
   int advance_width                    = 0;
   int left_side_bearing                = 0;
   unsigned map_id                      = 0;
   uint8_t *dst                         = NULL;
   stb_unicode_atlas_slot_t* atlas_slot = NULL;
   stb_unicode_font_renderer_t *self    = (stb_unicode_font_renderer_t*)data;

   if(!self)
      return NULL;

   map_id                               = charcode & 0xFF;
   atlas_slot                           = self->uc_map[map_id];

   while(atlas_slot)
   {
      if(atlas_slot->charcode == charcode)
      {
         atlas_slot->last_used = self->usage_counter++;
         return &atlas_slot->glyph;
      }
      atlas_slot = atlas_slot->next;
   }

   atlas_slot             = font_renderer_stb_unicode_get_slot(self);
   atlas_slot->charcode   = charcode;
   atlas_slot->next       = self->uc_map[map_id];
   self->uc_map[map_id] = atlas_slot;

   glyph_index              = stbtt_FindGlyphIndex(&self->info, charcode);

   dst = (uint8_t*)self->atlas.buffer + atlas_slot->glyph.atlas_offset_x
         + atlas_slot->glyph.atlas_offset_y * self->atlas.width;

   stbtt_GetGlyphHMetrics(&self->info, glyph_index, &advance_width, &left_side_bearing);
   if (stbtt_GetGlyphBox(&self->info, glyph_index, &x0, NULL, NULL, &y1))
   {
      stbtt_MakeGlyphBitmap(&self->info, dst, self->max_glyph_width, self->max_glyph_height,
            self->atlas.width, self->scale_factor, self->scale_factor, glyph_index);
   }
   else
   {
      /* This means the glyph is empty. In this case, stbtt_MakeGlyphBitmap()
       * fills the corresponding region of the atlas buffer with garbage,
       * so just zero it */
      int x, y;
      for (x = 0; x < self->max_glyph_width; x++)
         for (y = 0; y < self->max_glyph_height; y++)
            dst[x + (y * self->atlas.width)] = 0;
   }

   atlas_slot->glyph.width          = self->max_glyph_width;
   atlas_slot->glyph.height         = self->max_glyph_height;
   atlas_slot->glyph.advance_x      = round_away_from_zero((float)advance_width * self->scale_factor);
   atlas_slot->glyph.advance_y      = 0;
   atlas_slot->glyph.draw_offset_x  = round_away_from_zero((float)x0 * self->scale_factor);
   atlas_slot->glyph.draw_offset_y  = round_away_from_zero((float)(-y1) * self->scale_factor);

   self->atlas.dirty = true;
   atlas_slot->last_used = self->usage_counter++;
   return &atlas_slot->glyph;

}

static bool font_renderer_stb_unicode_create_atlas(
      stb_unicode_font_renderer_t *self, float font_size)
{
   unsigned i, x, y;
   stb_unicode_atlas_slot_t* slot = NULL;

   self->max_glyph_width  = font_size < 0 ? -font_size : font_size;
   self->max_glyph_height = font_size < 0 ? -font_size : font_size;

   self->atlas.width      = self->max_glyph_width  * STB_UNICODE_ATLAS_COLS;
   self->atlas.height     = self->max_glyph_height * STB_UNICODE_ATLAS_ROWS;

   self->atlas.buffer     = (uint8_t*)
      calloc(self->atlas.width * self->atlas.height, sizeof(uint8_t));

   if (!self->atlas.buffer)
      return false;

   slot = self->atlas_slots;

   for (y = 0; y < STB_UNICODE_ATLAS_ROWS; y++)
   {
      for (x = 0; x < STB_UNICODE_ATLAS_COLS; x++)
      {
         slot->glyph.atlas_offset_x = x * self->max_glyph_width;
         slot->glyph.atlas_offset_y = y * self->max_glyph_height;
         slot++;
      }
   }

   for (i = 0; i < 256; i++)
      font_renderer_stb_unicode_get_glyph(self, i);

   for (i = 0; i < 256; i++)
   {
      if(isalnum(i))
         font_renderer_stb_unicode_get_glyph(self, i);
   }

   return true;
}

static void *font_renderer_stb_unicode_init(const char *font_path, float font_size)
{
   int ascent, descent, line_gap;
   stb_unicode_font_renderer_t *self =
      (stb_unicode_font_renderer_t*)calloc(1, sizeof(*self));

   if (!self || font_size < 1.0)
      goto error;

   /* See https://github.com/nothings/stb/blob/master/stb_truetype.h#L539 */
   font_size = STBTT_POINT_SIZE(font_size);

#ifdef WIIU
   if(!*font_path)
   {
      uint32_t size = 0;
      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0, (void**)&self->font_data, &size))
         goto error;
   }
   else
#endif
   if (!path_is_valid(font_path) || !filestream_read_file(font_path, (void**)&self->font_data, NULL))
      goto error;

   if (!stbtt_InitFont(&self->info, self->font_data,
            stbtt_GetFontOffsetForIndex(self->font_data, 0)))
      goto error;

   stbtt_GetFontVMetrics(&self->info, &ascent, &descent, &line_gap);

   if (font_size < 0)
      self->scale_factor = stbtt_ScaleForMappingEmToPixels(&self->info, -font_size);
   else
      self->scale_factor = stbtt_ScaleForPixelHeight(&self->info, font_size);

   self->line_height  = (ascent - descent) * self->scale_factor;

   if (!font_renderer_stb_unicode_create_atlas(self, font_size))
      goto error;

   return self;

error:
   if (self)
      font_renderer_stb_unicode_free(self);
   return NULL;
}

static const char *font_renderer_stb_unicode_get_default_font(void)
{
#ifdef WIIU
   return "";
#else
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
#endif
}

static int font_renderer_stb_unicode_get_line_height(void* data)
{
   stb_unicode_font_renderer_t *handle = (stb_unicode_font_renderer_t*)data;
   return handle->line_height;
}

font_renderer_driver_t stb_unicode_font_renderer = {
   font_renderer_stb_unicode_init,
   font_renderer_stb_unicode_get_atlas,
   font_renderer_stb_unicode_get_glyph,
   font_renderer_stb_unicode_free,
   font_renderer_stb_unicode_get_default_font,
   "stb-unicode",
   font_renderer_stb_unicode_get_line_height,
};
