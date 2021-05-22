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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <ft2build.h>

#include <file/file_path.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

/* Was told by contributor that Windows support is pending,
 * so exclude Windows for now */
#if defined(HAVE_FONTCONFIG) && !defined(_WIN32)
#define HAVE_FONTCONFIG_SUPPORT
#endif

#if defined(HAVE_FONTCONFIG_SUPPORT)
#include <fontconfig/fontconfig.h>
#include "../../msg_hash.h"
#endif

#ifdef WIIU
#include <wiiu/os.h>
#endif

#include FT_FREETYPE_H
#include "../font_driver.h"

#define FT_ATLAS_ROWS 16
#define FT_ATLAS_COLS 16
#define FT_ATLAS_SIZE (FT_ATLAS_ROWS * FT_ATLAS_COLS)
/* Padding is required between each glyph in
 * the atlas to prevent texture bleed when
 * drawing with linear filtering enabled */
#define FT_ATLAS_PADDING 1

typedef struct freetype_atlas_slot
{
   struct freetype_atlas_slot* next;   /* ptr alignment */
   struct font_glyph glyph;            /* unsigned alignment */
   unsigned charcode;
   unsigned last_used;
}freetype_atlas_slot_t;

typedef struct freetype_renderer
{
   FT_Library lib;                                   /* ptr alignment   */
   FT_Face face;                                     /* ptr alignment   */
   struct font_atlas atlas;                          /* ptr alignment   */
   freetype_atlas_slot_t atlas_slots[FT_ATLAS_SIZE]; /* ptr alignment   */
   freetype_atlas_slot_t* uc_map[0x100];             /* ptr alignment   */
   unsigned max_glyph_width;
   unsigned max_glyph_height;
   unsigned usage_counter;
   struct font_line_metrics line_metrics;            /* float alignment */
} ft_font_renderer_t;

static struct font_atlas *font_renderer_ft_get_atlas(void *data)
{
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;
   if (!handle)
      return NULL;
   return &handle->atlas;
}

static void font_renderer_ft_free(void *data)
{
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;
   if (!handle)
      return;

   free(handle->atlas.buffer);

   if (handle->face)
      FT_Done_Face(handle->face);
   if (handle->lib)
      FT_Done_FreeType(handle->lib);
   free(handle);
}

static freetype_atlas_slot_t* font_renderer_get_slot(ft_font_renderer_t *handle)
{
   int i, map_id;
   unsigned oldest = 0;

   for (i = 1; i < FT_ATLAS_SIZE; i++)
      if ((handle->usage_counter - handle->atlas_slots[i].last_used) >
         (handle->usage_counter - handle->atlas_slots[oldest].last_used))
         oldest = i;

   /* remove from map */
   map_id = handle->atlas_slots[oldest].charcode & 0xFF;
   if (handle->uc_map[map_id] == &handle->atlas_slots[oldest])
      handle->uc_map[map_id] = handle->atlas_slots[oldest].next;
   else if (handle->uc_map[map_id])
   {
      freetype_atlas_slot_t* ptr = handle->uc_map[map_id];
      while (ptr->next && ptr->next != &handle->atlas_slots[oldest])
         ptr = ptr->next;
      ptr->next = handle->atlas_slots[oldest].next;
   }

   return &handle->atlas_slots[oldest];
}

static const struct font_glyph *font_renderer_ft_get_glyph(
      void *data, uint32_t charcode)
{
   unsigned map_id;
   uint8_t *dst;
   FT_GlyphSlot slot;
   freetype_atlas_slot_t* atlas_slot;
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;

   if (!handle)
      return NULL;

   map_id     = charcode & 0xFF;
   atlas_slot = handle->uc_map[map_id];

   while (atlas_slot)
   {
      if (atlas_slot->charcode == charcode)
      {
         atlas_slot->last_used = handle->usage_counter++;
         return &atlas_slot->glyph;
      }
      atlas_slot = atlas_slot->next;
   }

   if (FT_Load_Char(handle->face, charcode, FT_LOAD_RENDER))
      return NULL;

   FT_Render_Glyph(handle->face->glyph, FT_RENDER_MODE_NORMAL);
   slot = handle->face->glyph;

   atlas_slot             = font_renderer_get_slot(handle);
   atlas_slot->charcode   = charcode;
   atlas_slot->next       = handle->uc_map[map_id];
   handle->uc_map[map_id] = atlas_slot;

   /* Some glyphs can be blank. */
   atlas_slot->glyph.width         = slot->bitmap.width;
   atlas_slot->glyph.height        = slot->bitmap.rows;
   atlas_slot->glyph.advance_x     = slot->advance.x >> 6;
   atlas_slot->glyph.advance_y     = slot->advance.y >> 6;
   atlas_slot->glyph.draw_offset_x = slot->bitmap_left;
   atlas_slot->glyph.draw_offset_y = -slot->bitmap_top;

   dst = (uint8_t*)handle->atlas.buffer + atlas_slot->glyph.atlas_offset_x
         + atlas_slot->glyph.atlas_offset_y * handle->atlas.width;

   if (slot->bitmap.buffer)
   {
      const uint8_t *src    = (const uint8_t*)slot->bitmap.buffer;
      unsigned delta_width  = (handle->max_glyph_width > atlas_slot->glyph.width) ?
            (handle->max_glyph_width - atlas_slot->glyph.width) : 0;
      unsigned x, y;

      /* When copying the glyph bitmap, it is
       * necessary to clear any unused regions of
       * the atlas texture, otherwise garbage
       * (due to texture bleeding) may be drawn at
       * the edges of the glyph when rendering with
       * filtering enabled */

      for (y = 0; y < atlas_slot->glyph.height; y++)
      {
         /* Copy bitmap row */
         memcpy(dst, src, atlas_slot->glyph.width * sizeof(uint8_t));
         /* Zero out remaining atlas row */
         memset(dst + atlas_slot->glyph.width, 0, delta_width * sizeof(uint8_t));

         dst += handle->atlas.width;
         src += slot->bitmap.pitch;
      }

      /* Zero out unused atlas rows */
      for (y = atlas_slot->glyph.height; y < handle->max_glyph_height; y++)
      {
         memset(dst, 0, handle->max_glyph_width * sizeof(uint8_t));
         dst += handle->atlas.width;
      }
   }

   handle->atlas.dirty = true;
   atlas_slot->last_used = handle->usage_counter++;
   return &atlas_slot->glyph;
}

static bool font_renderer_create_atlas(ft_font_renderer_t *handle, float font_size)
{
   unsigned i, x, y;
   freetype_atlas_slot_t* slot = NULL;

   unsigned max_width  = round((handle->face->bbox.xMax - handle->face->bbox.xMin) * font_size / handle->face->units_per_EM);
   unsigned max_height = round((handle->face->bbox.yMax - handle->face->bbox.yMin) * font_size / handle->face->units_per_EM);

   unsigned atlas_width        = (max_width  + FT_ATLAS_PADDING) * FT_ATLAS_COLS;
   unsigned atlas_height       = (max_height + FT_ATLAS_PADDING) * FT_ATLAS_ROWS;

   uint8_t *atlas_buffer       = (uint8_t*)
      calloc(atlas_width * atlas_height, 1);

   if (!atlas_buffer)
      return false;

   handle->max_glyph_width     = max_width;
   handle->max_glyph_height    = max_height;
   handle->atlas.buffer        = atlas_buffer;
   handle->atlas.width         = atlas_width;
   handle->atlas.height        = atlas_height;
   slot                        = handle->atlas_slots;

   for (y = 0; y < FT_ATLAS_ROWS; y++)
   {
      for (x = 0; x < FT_ATLAS_COLS; x++)
      {
         slot->glyph.atlas_offset_x = x * (max_width  + FT_ATLAS_PADDING);
         slot->glyph.atlas_offset_y = y * (max_height + FT_ATLAS_PADDING);
         slot++;
      }
   }

   for (i = 0; i < 256; i++)
      font_renderer_ft_get_glyph(handle, i);

   for (i = 0; i < 256; i++)
      if (ISALNUM(i))
         font_renderer_ft_get_glyph(handle, i);

   return true;
}

static void *font_renderer_ft_init(const char *font_path, float font_size)
{
   FT_Error err;

   ft_font_renderer_t *handle = (ft_font_renderer_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      goto error;

   if (font_size < 1.0)
      goto error;

   err = FT_Init_FreeType(&handle->lib);
   if (err)
      goto error;

#ifdef WIIU
   if (!*font_path)
   {
      void* font_data    = NULL;
      uint32_t font_size = 0;

      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0, &font_data, &font_size))
         goto error;

      err = FT_New_Memory_Face(handle->lib, font_data, font_size, 0, &handle->face);
   }
   else
#elif defined(HAVE_FONTCONFIG_SUPPORT)
   /* if fallback font is requested, instead of loading it, we find the full font in the system */
   if (!*font_path || strstr(font_path, "fallback"))
   {
      FcValue locale_boxed;
      FcPattern *found     = NULL;
      FcConfig* config     = FcInitLoadConfigAndFonts();
      FcResult result      = FcResultNoMatch;
      FcChar8 *_font_path  = NULL;
      int face_index       = 0;
      /* select Sans fonts */
      FcPattern* pattern   = FcNameParse((const FcChar8*)"Sans");
      /* since fontconfig uses LL-TT style, we need to normalize 
       * locale names */
      FcChar8* locale      = FcLangNormalize((const FcChar8*)get_user_language_iso639_1(false));
      /* configure fontconfig substitute policies, this 
       * will increase the search scope */
      FcConfigSubstitute(config, pattern, FcMatchPattern);
      /* pull in system-wide defaults, so the 
       * font selection respects system (or user) configurations */
      FcDefaultSubstitute(pattern);

      /* Box the locale data in a FcValue container */
      locale_boxed.type = FcTypeString;
      locale_boxed.u.s  = locale;

      /* Override locale settins, since we are not using the system locale */
      FcPatternAdd(pattern, FC_LANG, locale_boxed, false);

      /* Let's find the best matching font given our search criteria */
      found             = FcFontMatch(config, pattern, &result);

      /* uh-oh, for some reason, we can't find any font */
      if (result != FcResultMatch)
         goto error;
      if (FcPatternGetString(found, FC_FILE, 0, &_font_path) != FcResultMatch)
         goto error;
      if (FcPatternGetInteger(found, FC_INDEX, 0, &face_index) != FcResultMatch)
         goto error;

      /* Initialize font renderer */
      err = FT_New_Face(handle->lib, (const char*)_font_path,
            face_index, &handle->face);

      /* free up fontconfig internal structures */
      FcPatternDestroy(pattern);
      FcPatternDestroy(found);
      FcStrFree(locale);
      FcConfigDestroy(config);
   }
   else
#endif
   {
      if (!path_is_valid(font_path))
         goto error;
      err = FT_New_Face(handle->lib, font_path, 0, &handle->face);
   }

   if (err)
      goto error;

   err = FT_Select_Charmap(handle->face, FT_ENCODING_UNICODE);
   if (err)
      goto error;

   err = FT_Set_Pixel_Sizes(handle->face, 0, font_size);
   if (err)
      goto error;

   if (!font_renderer_create_atlas(handle, font_size))
      goto error;

   handle->line_metrics.ascender  = (float)handle->face->size->metrics.ascender / 64.0f;
   handle->line_metrics.descender = (float)(-handle->face->size->metrics.descender) / 64.0f;
   handle->line_metrics.height    = (float)handle->face->size->metrics.height / 64.0f;

   return handle;

error:
   font_renderer_ft_free(handle);
   return NULL;
}

/* Not the cleanest way to do things for sure,
 * but should hopefully work ... */

static const char *font_paths[] = {
   /* Assets directory OSD Font, @see font_renderer_ft_get_default_font() */
   "assets://pkg/osd-font.ttf",
#if defined(_WIN32)
   "C:\\Windows\\Fonts\\consola.ttf",
   "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
   "/Library/Fonts/Microsoft/Candara.ttf",
   "/Library/Fonts/Verdana.ttf",
   "/Library/Fonts/Tahoma.ttf",
#else
   "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
   "/usr/share/fonts/TTF/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
   "/usr/share/fonts/TTF/Vera.ttf",
   "/usr/share/fonts/google-droid/DroidSansFallback.ttf", /* Fedora, RHEL, CentOS */
   "/usr/share/fonts/droid/DroidSansFallback.ttf",        /* Arch Linux */
   "/usr/share/fonts/truetype/DroidSansFallbackFull.ttf", /* openSUSE, SLE */
   "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf", /* Debian, Ubuntu */
#endif
   "osd-font.ttf", /* Magic font to search for, useful for distribution. */
};

/* Highly OS/platform dependent. */
static const char *font_renderer_ft_get_default_font(void)
{
/* Since fontconfig will return parameters more than a simple path
   we will process these in the init function */
#if defined(WIIU) || defined(HAVE_FONTCONFIG_SUPPORT)
   return "";
#else
   size_t i;

   for (i = 0; i < ARRAY_SIZE(font_paths); i++)
   {
      if (path_is_valid(font_paths[i]))
         return font_paths[i];
   }

   return NULL;
#endif
}

static bool font_renderer_ft_get_line_metrics(
      void* data, struct font_line_metrics **metrics)
{
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;
   if (!handle)
      return false;

   *metrics = &handle->line_metrics;
   return true;
}

font_renderer_driver_t freetype_font_renderer = {
   font_renderer_ft_init,
   font_renderer_ft_get_atlas,
   font_renderer_ft_get_glyph,
   font_renderer_ft_free,
   font_renderer_ft_get_default_font,
   "freetype",
   font_renderer_ft_get_line_metrics
};
