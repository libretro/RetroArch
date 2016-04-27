/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <CoreFoundation/CFString.h>
#ifdef IOS
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "../font_driver.h"
#include "../../general.h"

#define CT_ATLAS_ROWS 16
#define CT_ATLAS_COLS 16
#define CT_ATLAS_SIZE (CT_ATLAS_ROWS * CT_ATLAS_COLS)

typedef struct coretext_renderer
{
  struct font_atlas atlas;
  struct font_glyph glyphs[CT_ATLAS_SIZE];
} ct_font_renderer_t;

static const struct font_atlas *font_renderer_ct_get_atlas(void *data)
{
  ct_font_renderer_t *handle = (ct_font_renderer_t*)data;
  if (!handle)
     return NULL;
  return &handle->atlas;
}

static const struct font_glyph *font_renderer_ct_get_glyph(void *data, uint32_t code)
{
   ct_font_renderer_t *handle   = (ct_font_renderer_t*)data;

   if (!handle)
      return NULL;
   if (code >= CT_ATLAS_SIZE)
      return NULL;
   return &handle->glyphs[code];
}

static void font_renderer_ct_free(void *data)
{
  ct_font_renderer_t *handle = (ct_font_renderer_t*)data;

  if (!handle)
    return;

  free(handle->atlas.buffer);
  free(handle);
}

static bool font_renderer_create_atlas(CTFontRef face, ct_font_renderer_t *handle)
{
   int max_width, max_height;
   unsigned i;
   size_t bytesPerRow;
   CGGlyph glyphs[CT_ATLAS_SIZE];
   CGRect bounds[CT_ATLAS_SIZE];
   CGSize advances[CT_ATLAS_SIZE];
   float ascent, descent;
   CGContextRef offscreen;
   CFDictionaryRef attr;
   CFTypeRef values[1];
   CFStringRef keys[1];
   void *bitmapData                  = NULL;
   bool ret                          = true;
   size_t bitsPerComponent           = 8;
   UniChar characters[CT_ATLAS_SIZE] = {0};

   values[0]                         = face;
   keys[0]                           = kCTFontAttributeName;

   for (i = 0; i < CT_ATLAS_SIZE; i++)
      characters[i] = (UniChar)i;

   CTFontGetGlyphsForCharacters(face, characters, glyphs, CT_ATLAS_SIZE);

   CTFontGetBoundingRectsForGlyphs(face,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
   kCTFontOrientationDefault,
#else
   kCTFontDefaultOrientation,
#endif
    glyphs, bounds, CT_ATLAS_SIZE);

   CTFontGetAdvancesForGlyphs(face,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
         kCTFontOrientationDefault,
#else
         kCTFontDefaultOrientation, 
#endif
         glyphs, advances, CT_ATLAS_SIZE);

   ascent = CTFontGetAscent(face);
   descent = CTFontGetDescent(face);

   max_width = 0;
   max_height = 0;

   for (i = 0; i < CT_ATLAS_SIZE; i++)
   {
      int origin_x, origin_y;
      struct font_glyph *glyph = &handle->glyphs[i];

      if (!glyph)
         continue;

      origin_x             = ceil(bounds[i].origin.x);
      origin_y             = ceil(bounds[i].origin.y);

      glyph->draw_offset_x = 0;
      glyph->draw_offset_y = -ascent;
      glyph->width         = ceil(bounds[i].size.width);
      glyph->height        = ceil(bounds[i].size.height);
      glyph->advance_x     = ceil(advances[i].width);
      glyph->advance_y     = ceil(advances[i].height);

      max_width            = MAX(max_width, (origin_x + glyph->width));
      max_height           = MAX(max_height, (origin_y + glyph->height));
   }

   max_height              = MAX(max_height, ceil(ascent+descent));

   handle->atlas.width     = max_width * CT_ATLAS_COLS;
   handle->atlas.height    = max_height * CT_ATLAS_ROWS;

   handle->atlas.buffer    = (uint8_t*)
      calloc(handle->atlas.width * handle->atlas.height, 1);

   if (!handle->atlas.buffer)
   {
      ret = false;
      goto end;
   }

   bytesPerRow = max_width;
   bitmapData  = calloc(max_height, bytesPerRow);
   offscreen   = CGBitmapContextCreate(bitmapData, max_width, max_height,
            bitsPerComponent, bytesPerRow, NULL, kCGImageAlphaOnly);

   CGContextSetTextMatrix(offscreen, CGAffineTransformIdentity);

   attr = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 
         sizeof(keys) / sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, 
         &kCFTypeDictionaryValueCallBacks);

   for (i = 0; i < CT_ATLAS_SIZE; i++)
   {
      char glyph_cstr[2];
      const uint8_t *src;
      uint8_t       *dst;
      unsigned offset_x, offset_y, r, c;
      CFStringRef glyph_cfstr;
      CFAttributedStringRef attrString;
      CTLineRef line;
      struct font_glyph *glyph = &handle->glyphs[i];
      
      if (!glyph)
         continue;

      glyph->width = max_width;
      glyph->height = max_height;

      offset_x = (i % CT_ATLAS_COLS) * max_width;
      offset_y = (i / CT_ATLAS_COLS) * max_height;

      glyph->atlas_offset_x = offset_x;
      glyph->atlas_offset_y = offset_y;

      glyph_cstr[0] = i;
      glyph_cstr[1] = 0;
      glyph_cfstr   = CFStringCreateWithCString(
            NULL, glyph_cstr, kCFStringEncodingASCII );
      attrString = 
         CFAttributedStringCreate(NULL, glyph_cfstr, attr);
      CFRelease(glyph_cfstr);
      glyph_cfstr = NULL;
      line = CTLineCreateWithAttributedString(attrString);
      CFRelease(attrString);
      attrString = NULL;

      memset( bitmapData, 0, max_height * bytesPerRow );
      CGContextSetTextPosition(offscreen, 0, descent);
      CTLineDraw(line, offscreen);
      CGContextFlush( offscreen );

      CFRelease( line );
      line = NULL;

      dst = (uint8_t*)handle->atlas.buffer;
      src = (const uint8_t*)bitmapData;

      for (r = 0; r < max_height; r++ )
      {
         for (c = 0; c < max_width; c++)
         {
            unsigned src_idx = r * bytesPerRow + c;
            unsigned dest_idx = 
               (r + offset_y) * (CT_ATLAS_COLS * max_width) + (c + offset_x);
            uint8_t v = src[src_idx];

            dst[dest_idx] = v;
         }
      }
   }

   CFRelease(attr);
   CGContextRelease(offscreen);

   attr = NULL;
   offscreen = NULL;
   free(bitmapData);

end:
   return ret;
}

static void *font_renderer_ct_init(const char *font_path, float font_size)
{
   char err = 0;
   CFStringRef cf_font_path = NULL;
   CTFontRef face = NULL;
   ct_font_renderer_t *handle = (ct_font_renderer_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
   {
      err = 1;
      goto error;
   }

   cf_font_path = CFStringCreateWithCString(NULL, font_path, kCFStringEncodingASCII);
   if (!cf_font_path)
   {
      err = 1;
      goto error;
   }

   face = CTFontCreateWithName(cf_font_path, font_size, NULL);

   if (!face)
   {
      err = 1;
      goto error;
   }

   if (!font_renderer_create_atlas(face, handle))
   {
      err = 1;
      goto error;
   }

error:
   if (err)
   {
      font_renderer_ct_free(handle);
      handle = NULL;
   }

   if (cf_font_path)
   {
      CFRelease(cf_font_path);
      cf_font_path = NULL;
   }
   if (face)
   {
      CFRelease(face);
      face = NULL;
   }

   return handle;
}

/* We can't tell if a font is going to be there until we actually
   initialize CoreText and the best way to get fonts is by name, not
   by path. */
static const char *default_font = "Verdana";

static const char *font_renderer_ct_get_default_font(void)
{
  return default_font;
}

font_renderer_driver_t coretext_font_renderer = {
  font_renderer_ct_init,
  font_renderer_ct_get_atlas,
  font_renderer_ct_get_glyph,
  font_renderer_ct_free,
  font_renderer_ct_get_default_font,
  "coretext",
  NULL, /*get_line_height*/
};
