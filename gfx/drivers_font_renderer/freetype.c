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

/* Process-lifetime fontconfig configuration.
 *
 * This is initialized once and deliberately never passed to
 * FcConfigDestroy(). Destroying the config unmaps the directory
 * caches while fontconfig's static interned state (frozen
 * charsets/langsets, cache hash tables) may still reference the
 * mapped memory; a second in-process fontconfig consumer (e.g.
 * the Qt UI companion calling FcInit()) can then dereference the
 * stale pointers and crash with a use-after-munmap. Observed in
 * practice on FreeBSD 14.3 (issue #18377), where the crash was
 * masked under gdb because gdb disables ASLR and the remapped
 * caches landed at their old addresses.
 *
 * Keeping one config for the lifetime of the process is the
 * usage pattern fontconfig upstream recommends, and also avoids
 * re-scanning every cache file on each font load. */
static FcConfig *fc_config = NULL;
#endif

#ifdef WIIU
#include <wiiu/os.h>

#include <compat/strl.h>
#endif

#include FT_FREETYPE_H
#include "../font_driver.h"

typedef struct freetype_face
{
   FT_Library lib;                         /* ptr alignment   */
   FT_Face face;                           /* ptr alignment   */
   struct font_line_metrics line_metrics;  /* float alignment */
   /* The cell every glyph is drawn into, packed. */
   unsigned cell_dims;
} ft_face_t;

static void font_rasterizer_ft_free(void *data)
{
   ft_face_t *self = (ft_face_t*)data;
   if (!self)
      return;

   /* The bytes are borrowed: font_driver.c holds them, and drops its
    * reference only after this face is torn down, since
    * FT_New_Memory_Face keeps a pointer into the buffer. */
   if (self->face)
      FT_Done_Face(self->face);
   if (self->lib)
      FT_Done_FreeType(self->lib);
   free(self);
}

static void *font_rasterizer_ft_init(uint8_t *font_data,
      size_t font_data_len, unsigned face_index, float font_size)
{
   int glyph_w, glyph_h;
   ft_face_t *self = (ft_face_t*)calloc(1, sizeof(*self));

   if (!self)
      return NULL;

   if (font_size < 1.0f)
      goto error;

   if (FT_Init_FreeType(&self->lib))
      goto error;

#ifdef WIIU
   /* No bytes arrived, so use the OS shared font. */
   if (!font_data)
   {
      void* shared_data         = NULL;
      uint32_t shared_data_size = 0;

      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0,
               &shared_data, &shared_data_size))
         goto error;

      if (FT_New_Memory_Face(self->lib,
            (const FT_Byte*)shared_data, (FT_Long)shared_data_size,
            (FT_Long)0, &self->face))
         goto error;
   }
   else
#endif
   {
      if (!font_data || !font_data_len)
         goto error;
      if (FT_New_Memory_Face(self->lib,
            (const FT_Byte*)font_data, (FT_Long)font_data_len,
            (FT_Long)face_index, &self->face))
         goto error;
   }

   if (FT_Select_Charmap(self->face, FT_ENCODING_UNICODE))
      goto error;

   if (FT_Set_Pixel_Sizes(self->face, 0, font_size))
      goto error;

   /* units_per_EM is 0 for bitmap-only fonts; dividing by it would
    * crash. */
   if (self->face->units_per_EM == 0)
      goto error;

   glyph_w = (int)floor((self->face->bbox.xMax - self->face->bbox.xMin)
         * font_size / self->face->units_per_EM + 0.5);
   glyph_h = (int)floor((self->face->bbox.yMax - self->face->bbox.yMin)
         * font_size / self->face->units_per_EM + 0.5);

   if (glyph_w <= 0 || glyph_h <= 0)
      goto error;

   /* The cell size is derived from the font's own bbox, which is
    * attacker-controlled for untrusted font files; clamp it so the
    * atlas stays within common GPU texture limits. */
   if (glyph_w > 127)
      glyph_w = 127;
   if (glyph_h > 127)
      glyph_h = 127;
   self->cell_dims = VIDEO_SCALE_PACK(glyph_w, glyph_h);

   self->line_metrics.ascender  = (float)self->face->size->metrics.ascender / 64.0f;
   self->line_metrics.descender = (float)(-self->face->size->metrics.descender) / 64.0f;
   self->line_metrics.height    = (float)self->face->size->metrics.height / 64.0f;

   return self;

error:
   font_rasterizer_ft_free(self);
   return NULL;
}

static unsigned font_rasterizer_ft_glyph_index(void *data, uint32_t code)
{
   return (unsigned)FT_Get_Char_Index(((ft_face_t*)data)->face, code);
}

static bool font_rasterizer_ft_render_glyph(void *data, uint32_t code,
      unsigned gi, uint8_t *dst, unsigned pitch, unsigned cell_w,
      unsigned cell_h, enum font_atlas_format fmt, struct font_glyph *glyph)
{
   unsigned x, y, copy_w, copy_h;
   FT_GlyphSlot slot;
   const uint8_t *src;
   ft_face_t *self = (ft_face_t*)data;
   bool fmt16      = (fmt == FONT_ATLAS_FORMAT_A16);
   size_t esz      = fmt16 ? sizeof(uint16_t) : sizeof(uint8_t);

   (void)code;

   if (FT_Load_Glyph(self->face, gi, FT_LOAD_DEFAULT))
      return false;
   if (FT_Render_Glyph(self->face->glyph, FT_RENDER_MODE_NORMAL))
      return false;

   slot   = self->face->glyph;
   copy_w = slot->bitmap.width;
   copy_h = slot->bitmap.rows;
   if (!slot->bitmap.buffer)
      copy_w = copy_h = 0;
   if (copy_w > cell_w)
      copy_w = cell_w;
   if (copy_h > cell_h)
      copy_h = cell_h;

   src = (const uint8_t*)slot->bitmap.buffer;
   for (y = 0; y < cell_h; y++)
   {
      uint8_t *row = dst + (size_t)y * pitch * esz;
      if (y < copy_h)
      {
         if (fmt16)
         {
            /* FreeType emits 256 coverage levels; v * 257 upconverts
             * them losslessly to the 16-bit range (0xFF -> 0xFFFF) */
            uint16_t *row16 = (uint16_t*)(void*)row;
            for (x = 0; x < copy_w; x++)
               row16[x] = (uint16_t)((unsigned)src[x] * 257u);
         }
         else
            memcpy(row, src, copy_w);
         memset(row + (size_t)copy_w * esz, 0,
               (size_t)(cell_w - copy_w) * esz);
         src += slot->bitmap.pitch;
      }
      else
         memset(row, 0, (size_t)cell_w * esz);
   }

   /* Some glyphs can be blank. */
   glyph->width         = copy_w;
   glyph->height        = copy_h;
   glyph->advance_x     = slot->advance.x >> 6;
   glyph->advance_y     = slot->advance.y >> 6;
   glyph->draw_offset_x = slot->bitmap_left;
   glyph->draw_offset_y = -slot->bitmap_top;
   return true;
}

static unsigned font_rasterizer_ft_cell_dims(void *data)
{
   return ((ft_face_t*)data)->cell_dims;
}

static void font_rasterizer_ft_get_line_metrics(void *data,
      struct font_line_metrics *metrics)
{
   *metrics = ((ft_face_t*)data)->line_metrics;
}

/* Not the cleanest way to do things for sure,
 * but should hopefully work ... */

static const char * const font_paths[] = {
   /* Assets directory OSD Font, @see font_renderer_ft_get_default_fonts() */
   "assets://pkg/osd-font.ttf",
#if defined(_WIN32)
   "C:\\Windows\\Fonts\\consola.ttf",
   "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
   "/Library/Fonts/Microsoft/Candara.ttf",
   "/Library/Fonts/Verdana.ttf",
   "/Library/Fonts/Tahoma.ttf",
#elif defined(WEBOS)
  "/usr/share/fonts/MuseoSans-Medium.ttf",
  "/usr/share/fonts/LG_Smart_UI-Regular.ttf",
  "/usr/share/fonts/DroidSans.ttf",
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
   NULL
};

/* Highly OS/platform dependent. */
static const char * const *font_renderer_ft_get_default_fonts(
      const char *requested, unsigned *face_index)
{
#if defined(WIIU)
   /* The shared system font, fetched in init(); no file to open. */
   static const char * const none[] = { "", NULL };
   return none;
#elif defined(HAVE_FONTCONFIG_SUPPORT)
   /* fontconfig resolves against the request and the user's locale,
    * and answers with a face index as well as a path, which is why it
    * happens here rather than being picked from the static list
    * below. Only the resolving: the read belongs to
    * font_renderer_create_default() like every other renderer's. */
   static char resolved[PATH_MAX_LENGTH];
   static const char * const fc_result[] = { resolved, NULL };
   FcValue     locale_boxed;
   FcPattern  *found      = NULL;
   FcConfig   *config     = NULL;
   FcResult    result     = FcResultNoMatch;
   FcChar8    *_font_path = NULL;
   FcPattern  *pattern    = NULL;
   FcChar8    *locale     = NULL;
   int         index      = 0;

   /* An explicit font that is not one of the bundled fallbacks is
    * taken as asked for; "fallback" means the caller wants the real
    * system font for this language instead. */
   if (requested && *requested && !strstr(requested, "fallback"))
      return NULL;

   if (!fc_config)
      fc_config = FcInitLoadConfigAndFonts();
   if (!(config = fc_config))
      return NULL;

   if (!(pattern = FcNameParse((const FcChar8*)"Sans")))
      return NULL;

   /* fontconfig uses LL-TT style, so normalize the locale name */
   locale = FcLangNormalize((const FcChar8*)get_user_language_iso639_1(false));

   /* Widen the search scope, then pull in system-wide defaults so the
    * selection respects system or user configuration */
   FcConfigSubstitute(config, pattern, FcMatchPattern);
   FcDefaultSubstitute(pattern);

   /* Override locale settings, since we are not using the system
    * locale; FcLangNormalize can fail, in which case the pattern is
    * simply left without a language preference */
   if (locale)
   {
      locale_boxed.type = FcTypeString;
      locale_boxed.u.s  = locale;
      FcPatternAdd(pattern, FC_LANG, locale_boxed, false);
   }

   found = FcFontMatch(config, pattern, &result);

   resolved[0] = '\0';

   if (     result == FcResultMatch
         && FcPatternGetString(found, FC_FILE, 0, &_font_path)
               == FcResultMatch
         && FcPatternGetInteger(found, FC_INDEX, 0, &index)
               == FcResultMatch)
   {
      /* Copied out: fontconfig owns the string until the pattern is
       * destroyed, which happens below. */
      strlcpy(resolved, (const char*)_font_path, sizeof(resolved));
      if (face_index)
         *face_index = (unsigned)index;
   }

   /* free up per-lookup fontconfig structures; the config itself is
    * kept alive for the process lifetime (see the comment at the
    * fc_config definition) */
   FcPatternDestroy(pattern);
   if (found)
      FcPatternDestroy(found);
   if (locale)
      FcStrFree(locale);

   if (!resolved[0])
      return NULL;
   return fc_result;
#else
   /* Selection happens in font_renderer_create_default(), which is
    * what keeps path lookups out of this file. An explicit request
    * wins over the list. */
   (void)face_index;
   if (requested && *requested)
      return NULL;
   return font_paths;
#endif
}

const font_rasterizer_t freetype_font_rasterizer = {
   font_rasterizer_ft_init,
   font_rasterizer_ft_free,
   font_rasterizer_ft_glyph_index,
   font_rasterizer_ft_render_glyph,
   font_rasterizer_ft_cell_dims,
   font_rasterizer_ft_get_line_metrics,
   font_renderer_ft_get_default_fonts,
   "font_renderer_ft",
   true                        /* borrows_font_data */
};
