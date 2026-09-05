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
#endif

#include FT_FREETYPE_H
#include "../font_driver.h"

#define FT_ATLAS_ROWS 16
#define FT_ATLAS_COLS 16
#define FT_ATLAS_SIZE (FT_ATLAS_ROWS * FT_ATLAS_COLS)

/* Mix in upper bits to reduce clustering for CJK and other
 * non-Latin codepoints */
#define FT_HASH_SIZE 0x100
#define FT_HASH(c) (((c) ^ ((c) >> 8)) & (FT_HASH_SIZE - 1))
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
   freetype_atlas_slot_t* uc_map[FT_HASH_SIZE];      /* ptr alignment   */
   void *file_data;                                  /* ptr alignment   */
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
   /* Borrowed, not owned: font_renderer_create_default() holds these
    * bytes and may be sharing them with other fonts built from the
    * same path.  FT_New_Memory_Face keeps a pointer into the buffer
    * for the life of the face, so the face is torn down above before
    * the reference is dropped by the owner. */
   handle->file_data = NULL;
   if (handle->lib)
      FT_Done_FreeType(handle->lib);
   free(handle);
}

static freetype_atlas_slot_t* font_renderer_get_slot(ft_font_renderer_t *handle)
{
   int i, map_id;
   unsigned oldest = 0;
   /* Find the least-recently-used slot.
    * Unsigned subtraction handles usage_counter wrap-around
    * correctly. */
   unsigned oldest_age = handle->usage_counter -
      handle->atlas_slots[0].last_used;

   for (i = 1; i < FT_ATLAS_SIZE; i++)
   {
      unsigned age = handle->usage_counter - handle->atlas_slots[i].last_used;
      if (age > oldest_age)
      {
         oldest_age = age;
         oldest     = i;
      }
   }

   /* remove from map */
   map_id = FT_HASH(handle->atlas_slots[oldest].charcode);
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

/* Copy one rendered FreeType glyph bitmap into the atlas, clearing
 * the unused remainder of the cell (otherwise garbage may bleed in
 * at glyph edges when rendering with filtering enabled).
 *
 * This helper is the ONLY place that knows the atlas is 8-bit; a
 * higher-bit-depth atlas (HDR output) needs a sibling of this
 * routine and nothing else. Note that unlike rasterizers that can
 * produce higher-precision coverage directly, FreeType's smooth
 * rasterizer emits 256 coverage levels, so the HDR sibling is a
 * lossless upconversion of those levels (v * 257 for 16-bit); this
 * is adequate for coverage/alpha data. The OS/display requirements
 * for HDR output are the video driver's concern, not this file's. */
static void font_renderer_ft_copy_coverage(ft_font_renderer_t *handle,
      const freetype_atlas_slot_t *atlas_slot, const FT_GlyphSlot slot,
      unsigned copy_width, unsigned copy_height)
{
   unsigned y;
   const uint8_t *src   = (const uint8_t*)slot->bitmap.buffer;
   unsigned delta_width = handle->max_glyph_width - copy_width;
   size_t   esz         = (handle->atlas.format == FONT_ATLAS_FORMAT_A16)
         ? sizeof(uint16_t) : sizeof(uint8_t);
   uint8_t *dst         = (uint8_t*)handle->atlas.buffer
         + ((size_t)atlas_slot->glyph.atlas_offset_x
         +  (size_t)atlas_slot->glyph.atlas_offset_y
               * handle->atlas.width) * esz;

   for (y = 0; y < copy_height; y++)
   {
      if (handle->atlas.format == FONT_ATLAS_FORMAT_A16)
      {
         /* FreeType emits 256 coverage levels; v * 257 upconverts
          * them losslessly to the 16-bit range (0xFF -> 0xFFFF) */
         uint16_t *dst16 = (uint16_t*)(void*)dst;
         unsigned  x;
         for (x = 0; x < copy_width; x++)
            dst16[x] = (uint16_t)((unsigned)src[x] * 257u);
         if (delta_width > 0)
            memset(dst16 + copy_width, 0,
                  (size_t)delta_width * sizeof(uint16_t));
      }
      else
      {
         /* Copy bitmap row */
         memcpy(dst, src, copy_width * sizeof(uint8_t));
         /* Zero out remaining atlas row */
         if (delta_width > 0)
            memset(dst + copy_width, 0, delta_width * sizeof(uint8_t));
      }

      dst += (size_t)handle->atlas.width * esz;
      src += slot->bitmap.pitch;
   }

   if (copy_height < handle->max_glyph_height)
   {
      for (y = copy_height; y < handle->max_glyph_height; y++)
      {
         memset(dst, 0, (size_t)handle->max_glyph_width * esz);
         dst += (size_t)handle->atlas.width * esz;
      }
   }
}

/* Merge one updated glyph cell into the atlas dirty region */
static void font_renderer_ft_dirty_cell(struct font_atlas *atlas,
      unsigned x, unsigned y, unsigned w, unsigned h)
{
   if (!atlas->dirty)
   {
      atlas->dirty_x0 = x;
      atlas->dirty_y0 = y;
      atlas->dirty_x1 = x + w;
      atlas->dirty_y1 = y + h;
      atlas->dirty    = true;
   }
   else
   {
      if (x < atlas->dirty_x0)
         atlas->dirty_x0 = x;
      if (y < atlas->dirty_y0)
         atlas->dirty_y0 = y;
      if (x + w > atlas->dirty_x1)
         atlas->dirty_x1 = x + w;
      if (y + h > atlas->dirty_y1)
         atlas->dirty_y1 = y + h;
   }
}

static const struct font_glyph *font_renderer_ft_get_glyph(
      void *data, uint32_t charcode)
{
   unsigned map_id;
   unsigned copy_width, copy_height;
   FT_GlyphSlot slot;
   freetype_atlas_slot_t* atlas_slot;
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;

   if (!handle)
      return NULL;

   map_id     = FT_HASH(charcode);
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

   if (FT_Load_Char(handle->face, charcode, FT_LOAD_DEFAULT))
      return NULL;

   if (FT_Render_Glyph(handle->face->glyph, FT_RENDER_MODE_NORMAL))
      return NULL;

   slot = handle->face->glyph;

   atlas_slot                      = font_renderer_get_slot(handle);
   atlas_slot->charcode            = charcode;
   atlas_slot->next                = handle->uc_map[map_id];
   handle->uc_map[map_id]          = atlas_slot;

   copy_width                      = slot->bitmap.width;
   copy_height                     = slot->bitmap.rows;
   if (copy_width  > handle->max_glyph_width)
      copy_width  = handle->max_glyph_width;
   if (copy_height > handle->max_glyph_height)
      copy_height = handle->max_glyph_height;

   /* Some glyphs can be blank. */
   atlas_slot->glyph.width         = copy_width;
   atlas_slot->glyph.height        = copy_height;
   atlas_slot->glyph.advance_x     = slot->advance.x >> 6;
   atlas_slot->glyph.advance_y     = slot->advance.y >> 6;
   atlas_slot->glyph.draw_offset_x = slot->bitmap_left;
   atlas_slot->glyph.draw_offset_y = -slot->bitmap_top;

   if (slot->bitmap.buffer)
   {
      font_renderer_ft_copy_coverage(handle, atlas_slot, slot,
            copy_width, copy_height);
      /* Blank glyphs write nothing, so they no longer mark the
       * atlas dirty */
      font_renderer_ft_dirty_cell(&handle->atlas,
            atlas_slot->glyph.atlas_offset_x,
            atlas_slot->glyph.atlas_offset_y,
            handle->max_glyph_width, handle->max_glyph_height);
   }

   atlas_slot->last_used = handle->usage_counter++;
   return &atlas_slot->glyph;
}

static bool font_renderer_create_atlas(ft_font_renderer_t *handle,
      float font_size, enum font_atlas_format fmt)
{
   unsigned i, x, y;
   unsigned max_width, max_height;
   unsigned atlas_width, atlas_height;
   uint8_t *atlas_buffer;
   freetype_atlas_slot_t* slot = NULL;
   int glyph_w, glyph_h;

   /* units_per_EM is 0 for bitmap-only fonts; dividing by it would
    * crash before FT_Set_Pixel_Sizes ever gets the chance to reject
    * such a face. */
   if (handle->face->units_per_EM == 0)
      return false;

   glyph_w = (int)floor((handle->face->bbox.xMax - handle->face->bbox.xMin)
         * font_size / handle->face->units_per_EM + 0.5);
   glyph_h = (int)floor((handle->face->bbox.yMax - handle->face->bbox.yMin)
         * font_size / handle->face->units_per_EM + 0.5);

   if (glyph_w <= 0 || glyph_h <= 0)
      return false;

   /* The cell size is derived from the font's own bbox, which is
    * attacker-controlled for untrusted font files; clamp it so the
    * atlas stays within common GPU texture limits and the
    * width * height product cannot overflow. */
   if (glyph_w > 127)
      glyph_w = 127;
   if (glyph_h > 127)
      glyph_h = 127;

   max_width    = (unsigned)glyph_w;
   max_height   = (unsigned)glyph_h;
   atlas_width  = (max_width  + FT_ATLAS_PADDING) * FT_ATLAS_COLS;
   atlas_height = (max_height + FT_ATLAS_PADDING) * FT_ATLAS_ROWS;
   /* Higher-precision coverage when the video driver asked for it
    * (HDR output); the atlas then stores uint16_t samples. */
   handle->atlas.format = fmt;
   atlas_buffer = (uint8_t*)calloc((size_t)atlas_height,
         (size_t)atlas_width *
         ((handle->atlas.format == FONT_ATLAS_FORMAT_A16) ? 2 : 1));

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

   /* Pre-cache the first 256 code points. */
   for (i = 0; i < 256; i++)
      font_renderer_ft_get_glyph(handle, i);

   return true;
}

static void *font_renderer_ft_init(
      uint8_t *font_data_in, size_t font_data_in_len,
      unsigned face_index,
      float font_size, enum font_atlas_format fmt)
{
   FT_Error err;

   ft_font_renderer_t *handle = (ft_font_renderer_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (font_size < 1.0)
      goto error;

   if ((err = FT_Init_FreeType(&handle->lib)))
      goto error;

#ifdef WIIU
   /* No bytes arrived, so use the OS shared font. Borrowed from the
    * OS: not ours to free, so file_data stays NULL. */
   if (!font_data_in)
   {
      void* shared_data         = NULL;
      uint32_t shared_data_size = 0;

      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0,
               &shared_data, &shared_data_size))
         goto error;

      if ((err = FT_New_Memory_Face(handle->lib,
            (const FT_Byte*)shared_data, (FT_Long)shared_data_size,
            (FT_Long)0, &handle->face)))
         goto error;
   }
   else
#endif
   {
      /* Bytes and face index come from
       * font_renderer_create_default(); this renderer opens nothing.
       * Ownership is taken before the face is built, not after, so the
       * error path releases them when FT rejects the font. */
      if (!font_data_in || !font_data_in_len)
         goto error;
      handle->file_data = font_data_in;
      if ((err = FT_New_Memory_Face(handle->lib,
            (const FT_Byte*)font_data_in, (FT_Long)font_data_in_len,
            (FT_Long)face_index, &handle->face)))
         goto error;
   }


   if ((err = FT_Select_Charmap(handle->face, FT_ENCODING_UNICODE)))
      goto error;

   if ((err = FT_Set_Pixel_Sizes(handle->face, 0, font_size)))
      goto error;

   if (!font_renderer_create_atlas(handle, font_size, fmt))
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

static void font_renderer_ft_get_line_metrics(
      void* data, struct font_line_metrics **metrics)
{
   ft_font_renderer_t *handle = (ft_font_renderer_t*)data;
   if (!handle)
      return;
   *metrics = &handle->line_metrics;
}

font_renderer_driver_t freetype_font_renderer = {
   font_renderer_ft_init,
   font_renderer_ft_get_atlas,
   font_renderer_ft_get_glyph,
   font_renderer_ft_free,
   font_renderer_ft_get_default_fonts,
   "font_renderer_ft",
   font_renderer_ft_get_line_metrics,
   true                        /* borrows_font_data */
};
