/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <CoreFoundation/CFString.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if TARGET_OS_IPHONE
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif


#include "../font_driver.h"
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

typedef struct coretext_face
{
   CTFontRef font_face;
   CFDictionaryRef attr_dict;  /* Reused for every glyph */
   struct font_line_metrics line_metrics;
   float cached_ascent;
   /* The cell every glyph is drawn into, packed. */
   unsigned cell_dims;
} ct_face_t;

static void font_rasterizer_ct_free(void *data)
{
   ct_face_t *self = (ct_face_t*)data;

   if (!self)
      return;
   if (self->font_face)
      CFRelease(self->font_face);
   if (self->attr_dict)
      CFRelease(self->attr_dict);
   free(self);
}

/* UTF-16 encoding of a codepoint: one unit for the BMP, a surrogate
 * pair beyond it. 0 for a value that is not a scalar value. */
static CFIndex ct_utf16(uint32_t code, UniChar *utf16)
{
   if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
      return 0;
   if (code > 0xFFFF)
   {
      uint32_t v = code - 0x10000;
      utf16[0]   = (UniChar)(0xD800 + (v >> 10));
      utf16[1]   = (UniChar)(0xDC00 + (v & 0x3FF));
      return 2;
   }
   utf16[0] = (UniChar)code;
   return 1;
}

static unsigned font_rasterizer_ct_glyph_index(void *data, uint32_t code)
{
   CGGlyph glyphs[2];
   UniChar utf16[2];
   ct_face_t *self = (ct_face_t*)data;
   CFIndex len     = ct_utf16(code, utf16);

   if (!len || !CTFontGetGlyphsForCharacters(self->font_face, utf16,
            glyphs, len))
      return 0;
   /* Offset by one so that glyph 0 still reads as present */
   return (unsigned)glyphs[0] + 1;
}

/* The missing-glyph mark: a rectangle inset into the cell */
static void ct_render_missing(ct_face_t *self, uint8_t *dst,
      unsigned pitch, unsigned cell_w, unsigned cell_h,
      enum font_atlas_format fmt, struct font_glyph *glyph)
{
   unsigned r, c;
   bool fmt16 = (fmt == FONT_ATLAS_FORMAT_A16);
   size_t esz = fmt16 ? sizeof(uint16_t) : sizeof(uint8_t);

   for (r = 0; r < cell_h; r++)
      memset(dst + (size_t)r * pitch * esz, 0, (size_t)cell_w * esz);

   if (cell_w >= 6 && cell_h >= 6)
   {
      unsigned max_r = cell_h - 2;
      unsigned max_c = cell_w - 2;

      if (fmt16)
      {
         uint16_t *d16 = (uint16_t*)(void*)dst;
         for (r = 2; r < max_r; r++)
         {
            d16[r * pitch + 2]         = 0xFFFF;
            d16[r * pitch + max_c - 1] = 0xFFFF;
         }
         for (c = 2; c < max_c; c++)
         {
            d16[2 * pitch + c]           = 0xFFFF;
            d16[(max_r - 1) * pitch + c] = 0xFFFF;
         }
      }
      else
      {
         for (r = 2; r < max_r; r++)
         {
            dst[r * pitch + 2]         = 255;
            dst[r * pitch + max_c - 1] = 255;
         }
         for (c = 2; c < max_c; c++)
         {
            dst[2 * pitch + c]           = 255;
            dst[(max_r - 1) * pitch + c] = 255;
         }
      }
   }

   glyph->width         = cell_w;
   glyph->height        = cell_h;
   glyph->draw_offset_x = 0;
   glyph->draw_offset_y = (int)floor(-self->cached_ascent);
   glyph->advance_x     = cell_w;
   glyph->advance_y     = 0;
}

static bool font_rasterizer_ct_render_glyph(void *data, uint32_t code,
      unsigned gi, uint8_t *dst, unsigned pitch, unsigned cell_w,
      unsigned cell_h, enum font_atlas_format fmt, struct font_glyph *glyph)
{
   CGGlyph glyphs[1];
   CGRect bounds;
   CGSize advance;
   CGContextRef offscreen;
   void *bitmap;
   UniChar utf16[2];
   CFIndex utf16_len;
   CFStringRef glyph_cfstr;
   CFAttributedStringRef attr_string;
   CTLineRef line;
   unsigned r;
   ct_face_t *self = (ct_face_t*)data;
   bool fmt16      = (fmt == FONT_ATLAS_FORMAT_A16);
   size_t esz      = fmt16 ? sizeof(uint16_t) : sizeof(uint8_t);

   if (!gi || !(utf16_len = ct_utf16(code, utf16)))
   {
      ct_render_missing(self, dst, pitch, cell_w, cell_h, fmt, glyph);
      return true;
   }
   glyphs[0] = (CGGlyph)(gi - 1);

   /* kCTFontDefaultOrientation was renamed kCTFontOrientationDefault
    * in 10.8; both are zero and both still work, so the value goes in
    * rather than either name. */
   CTFontGetBoundingRectsForGlyphs(self->font_face, (CTFontOrientation)0,
         glyphs, &bounds, 1);
   CTFontGetAdvancesForGlyphs(self->font_face, (CTFontOrientation)0,
         glyphs, &advance, 1);

   glyph->width         = cell_w;
   glyph->height        = cell_h;
   glyph->draw_offset_x = (int)ceil(bounds.origin.x);
   glyph->draw_offset_y = (int)floor(-bounds.origin.y)
      - (int)floor(self->cached_ascent) + 1;
   /* round() is C99; advances are non-negative so floor(x + 0.5)
    * is equivalent */
   glyph->advance_x     = (int)floor(advance.width + 0.5);
   glyph->advance_y     = (int)floor(advance.height + 0.5);

   if (!(bitmap = calloc(cell_h, (size_t)cell_w * esz)))
      return false;

   if (fmt16)
   {
      /* 16 bits-per-component DeviceGray, host byte order:
       * white-on-transparent gray is copied out as 16-bit coverage */
      CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
      if (!gray)
      {
         free(bitmap);
         return false;
      }
      offscreen = CGBitmapContextCreate(bitmap, cell_w, cell_h,
            16, (size_t)cell_w * 2, gray,
            kCGImageAlphaNone | kCGBitmapByteOrder16Host);
      CGColorSpaceRelease(gray);
   }
   else
      /* 8-bit alpha-only coverage */
      offscreen = CGBitmapContextCreate(bitmap, cell_w, cell_h,
            8, cell_w, NULL, kCGImageAlphaOnly);

   if (!offscreen)
   {
      free(bitmap);
      return false;
   }

   /* Fill color for kCTForegroundColorFromContextAttributeName:
    * full-white coverage in the gray context, ignored by the
    * alpha-only one */
   CGContextSetGrayFillColor(offscreen, 1.0f, 1.0f);
   CGContextSetTextMatrix(offscreen, CGAffineTransformIdentity);

   /* Each CF/CT allocation is checked: passing NULL onwards or
    * CFRelease(NULL) would crash rather than fail. */
   if (!(glyph_cfstr = CFStringCreateWithCharacters(NULL, utf16, utf16_len)))
   {
      CGContextRelease(offscreen);
      free(bitmap);
      return false;
   }
   attr_string = CFAttributedStringCreate(NULL, glyph_cfstr, self->attr_dict);
   CFRelease(glyph_cfstr);
   if (!attr_string)
   {
      CGContextRelease(offscreen);
      free(bitmap);
      return false;
   }
   line = CTLineCreateWithAttributedString(attr_string);
   CFRelease(attr_string);
   if (!line)
   {
      CGContextRelease(offscreen);
      free(bitmap);
      return false;
   }

   CGContextSetTextPosition(offscreen, -bounds.origin.x, -bounds.origin.y);
   CTLineDraw(line, offscreen);
   CFRelease(line);

   for (r = 0; r < cell_h; r++)
      memcpy(dst + (size_t)r * pitch * esz,
            (const uint8_t*)bitmap + (size_t)r * cell_w * esz,
            (size_t)cell_w * esz);

   CGContextRelease(offscreen);
   free(bitmap);
   return true;
}

static unsigned font_rasterizer_ct_cell_dims(void *data)
{
   return ((ct_face_t*)data)->cell_dims;
}

static void font_rasterizer_ct_get_line_metrics(void *data,
      struct font_line_metrics *metrics)
{
   *metrics = ((ct_face_t*)data)->line_metrics;
}

/* CoreGraphics calls this when it is done with the buffer, which is
 * how ownership of the bytes handed to init() is discharged. */
static void ct_font_data_release(void *info, const void *data, size_t size)
{
   (void)info;
   (void)size;
   free((void*)data);
}

static void *font_rasterizer_ct_init(uint8_t *font_data,
      size_t font_data_len, unsigned face_index, float font_size)
{
   float ascent, descent;
   int max_glyph_size;
   CTFontRef face                 = NULL;
   CGDataProviderRef dataProvider = NULL;
   CGFontRef theCGFont            = NULL;
   ct_face_t *self                = (ct_face_t*)calloc(1, sizeof(*self));

   /* CoreText has no collection index in this path. */
   (void)face_index;

   if (!self || !font_data || !font_data_len || font_size < 1.0f)
   {
      free(font_data);
      goto error;
   }

   /* The provider takes the bytes, and releases them through
    * ct_font_data_release() when CoreGraphics is finished - so from
    * here on they are not freed in this function. */
   if (!(dataProvider = CGDataProviderCreateWithData(
               NULL, font_data, font_data_len, ct_font_data_release)))
   {
      free(font_data);
      goto error;
   }
   if (!(theCGFont = CGFontCreateWithDataProvider(dataProvider)))
      goto error;
   if (!(face = CTFontCreateWithGraphicsFont(theCGFont, font_size,
               NULL, NULL)))
      goto error;

   self->font_face = face;
   CFRetain(face);

   {
      /* C89: block-scope aggregate initializers must be constant */
      CFTypeRef values[2];
      CFStringRef keys[2];
      values[0] = face;
      keys[0]   = kCTFontAttributeName;
      /* Take the fill color from the context: required for the 16-bit
       * DeviceGray (A16) context, where the default black foreground
       * on a zeroed buffer would render nothing. */
      values[1] = kCFBooleanTrue;
      keys[1]   = kCTForegroundColorFromContextAttributeName;
      self->attr_dict = CFDictionaryCreate(NULL, (const void **)&keys,
            (const void **)&values, 2, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
   }
   if (!self->attr_dict)
      goto error;

   /* Clamp the cell so the atlas stays within common GPU texture
    * limits; font_size comes from user configuration. */
   max_glyph_size = (int)font_size;
   if (max_glyph_size > 127)
      max_glyph_size = 127;
   self->cell_dims = VIDEO_SCALE_PACK(max_glyph_size, max_glyph_size);

   ascent  = CTFontGetAscent(face);
   descent = CTFontGetDescent(face);
   self->cached_ascent            = ascent;
   self->line_metrics.ascender    = ascent;
   self->line_metrics.descender   = (descent < 0.0f) ? -descent : descent;
   self->line_metrics.height      = self->line_metrics.ascender
      + self->line_metrics.descender + (float)CTFontGetLeading(face);

   CFRelease(face);
   CFRelease(dataProvider);
   CFRelease(theCGFont);
   return self;

error:
   font_rasterizer_ct_free(self);
   if (face)
      CFRelease(face);
   if (dataProvider)
      CFRelease(dataProvider);
   if (theCGFont)
      CFRelease(theCGFont);
   return NULL;
}

static const char * const *font_renderer_ct_get_default_fonts(
      const char *requested, unsigned *face_index)
{
   /* A name rather than a path: CoreText looks fonts up by name, and
    * there is no way to know one is present without initialising it.
    * font_renderer_create_default() will not find this on disk, which
    * matches the previous behaviour - init() rejected it too. */
   static const char * const names[] = { "Verdana", NULL };

   (void)face_index;

   /* An explicit request wins; this is only the no-path default. */
   if (requested && *requested)
      return NULL;
   return names;
}

const font_rasterizer_t coretext_font_rasterizer = {
   font_rasterizer_ct_init,
   font_rasterizer_ct_free,
   font_rasterizer_ct_glyph_index,
   font_rasterizer_ct_render_glyph,
   font_rasterizer_ct_cell_dims,
   font_rasterizer_ct_get_line_metrics,
   font_renderer_ct_get_default_fonts,
   "font_renderer_ct",
   false                       /* borrows_font_data: the buffer goes to
                                * CGDataProviderCreateWithData and is
                                * released by CoreGraphics on its own
                                * schedule, so this rasterizer takes a
                                * private copy and owns it. */
};
