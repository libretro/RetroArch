/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "fonts.h"
#include "../../file.h"
#include "../../general.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <CoreFoundation/CFString.h>
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

#define CT_ATLAS_ROWS 8
#define CT_ATLAS_COLS 16
#define CT_ATLAS_SIZE (CT_ATLAS_ROWS * CT_ATLAS_COLS)

typedef struct coretext_renderer
{
  struct font_atlas atlas;
  struct font_glyph glyphs[CT_ATLAS_SIZE];
} font_renderer_t;

static const struct font_atlas *font_renderer_ct_get_atlas(void *data)
{
  font_renderer_t *handle = (font_renderer_t*)data;
  return &handle->atlas;
}

static const struct font_glyph *font_renderer_ct_get_glyph(void *data, uint32_t code)
{
  font_renderer_t *handle = (font_renderer_t*)data;
  struct font_glyph *result = code < CT_ATLAS_SIZE ? &handle->glyphs[code] : NULL;
  return result;
}

static void font_renderer_ct_free(void *data)
{
  font_renderer_t *handle = (font_renderer_t*)data;
  if (!handle)
    return;

  free(handle->atlas.buffer);
  free(handle);
}

static bool font_renderer_create_atlas(CTFontRef face, font_renderer_t *handle)
{
  unsigned i;
  bool ret = true;

  UniChar characters[CT_ATLAS_SIZE] = {0};
  for (i = 0; i < CT_ATLAS_SIZE; i++) {
    characters[i] = (UniChar)i;
  }

  CGGlyph glyphs[CT_ATLAS_SIZE];
  CTFontGetGlyphsForCharacters(face, characters, glyphs, CT_ATLAS_SIZE);

  CGRect bounds[CT_ATLAS_SIZE];
  CTFontGetBoundingRectsForGlyphs(face, kCTFontDefaultOrientation, 
                                  glyphs, bounds, CT_ATLAS_SIZE);

  CGSize advances[CT_ATLAS_SIZE];
  CTFontGetAdvancesForGlyphs(face, kCTFontDefaultOrientation, 
                             glyphs, advances, CT_ATLAS_SIZE);

  CGFloat ascent = CTFontGetAscent( face );
  CGFloat descent = CTFontGetDescent( face );

  int max_width = 0;
  int max_height = 0;
  for (i = 0; i < CT_ATLAS_SIZE; i++) {
    struct font_glyph *glyph = &handle->glyphs[i];
    int origin_x = ceil(bounds[i].origin.x);
    int origin_y = ceil(bounds[i].origin.y);

    glyph->draw_offset_x = 0;
    glyph->draw_offset_y = -1 * (ascent - descent);
    glyph->width = ceil(bounds[i].size.width);
    glyph->height = ceil(bounds[i].size.height);
    glyph->advance_x = ceil(advances[i].width);
    glyph->advance_y = ceil(advances[i].height);

    max_width = max(max_width, (origin_x + glyph->width));
    max_height = max(max_height, (origin_y + glyph->height));
  }
  max_height = max(max_height, ceil(ascent+descent));

  handle->atlas.width = max_width * CT_ATLAS_COLS;
  handle->atlas.height = max_height * CT_ATLAS_ROWS;

  handle->atlas.buffer = (uint8_t*)
    calloc(handle->atlas.width * handle->atlas.height, 1);

  if (!handle->atlas.buffer) {
    ret = false;
    goto end;
  }

  size_t bitsPerComponent = 8;
  size_t bytesPerRow = max_width;
  void *bitmapData = calloc(max_height, bytesPerRow);
  CGContextRef offscreen =
    CGBitmapContextCreate(bitmapData, max_width, max_height,
                          bitsPerComponent, bytesPerRow, NULL, kCGImageAlphaOnly);
  CGContextSetTextMatrix(offscreen, CGAffineTransformIdentity);

  CFStringRef keys[] = { kCTFontAttributeName };
  CFTypeRef values[] = { face };
  CFDictionaryRef attr = 
    CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values, 
                       sizeof(keys) / sizeof(keys[0]), 
                       &kCFTypeDictionaryKeyCallBacks, 
                       &kCFTypeDictionaryValueCallBacks);
  
  for (i = 0; i < CT_ATLAS_SIZE; i++) {
    struct font_glyph *glyph = &handle->glyphs[i];

    glyph->width = max_width;
    glyph->height = max_height;

    unsigned offset_x = (i % CT_ATLAS_COLS) * max_width;
    unsigned offset_y = (i / CT_ATLAS_COLS) * max_height;

    glyph->atlas_offset_x = offset_x;
    glyph->atlas_offset_y = offset_y;

    char glyph_cstr[2];
    glyph_cstr[0] = i;
    glyph_cstr[1] = 0;
    CFStringRef glyph_cfstr =
      CFStringCreateWithCString( NULL, glyph_cstr, kCFStringEncodingASCII );
    CFAttributedStringRef attrString = 
      CFAttributedStringCreate(NULL, glyph_cfstr, attr);
    CFRelease(glyph_cfstr), glyph_cfstr = NULL;
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    CFRelease(attrString), attrString = NULL;

    memset( bitmapData, 0, max_height * bytesPerRow );
    CGContextSetTextPosition(offscreen, 0, descent);
    CTLineDraw(line, offscreen);
    CGContextFlush( offscreen );

    CFRelease( line ), line = NULL;

    uint8_t *dst = (uint8_t*)handle->atlas.buffer;

    const uint8_t *src = (const uint8_t*)bitmapData;
    for (unsigned r = 0; r < max_height; r++ ) {
      for (unsigned c = 0; c < max_width; c++) {
        unsigned src_idx = r * bytesPerRow + c;
        unsigned dest_idx = 
          (r + offset_y) * (CT_ATLAS_COLS * max_width) + (c + offset_x);
        uint8_t v = src[src_idx];
        dst[dest_idx] = v;
      }
    }
  }

  CFRelease(attr), attr = NULL;
  CGContextRelease(offscreen), offscreen = NULL;
  free(bitmapData);

 end:
  return ret;
}

static void *font_renderer_ct_init(const char *font_path, float font_size)
{
  char err = 0;
  CFStringRef cf_font_path = NULL;
  CTFontRef face = NULL;

  font_renderer_t *handle = (font_renderer_t*)
    calloc(1, sizeof(*handle));

  if (!handle) {
    err = 1; goto error;
  }


  cf_font_path = CFStringCreateWithCString( NULL, font_path, kCFStringEncodingASCII );
  if ( ! cf_font_path ) {
    err = 1; goto error;
  }
  face = CTFontCreateWithName( cf_font_path, font_size, NULL );
  if ( ! face ) {
    err = 1; goto error;
  }
  if (! font_renderer_create_atlas(face, handle)) {
    err = 1; goto error;
  }

 error:
  if ( err ) {
    font_renderer_ct_free(handle);
    handle = NULL;
  }

  if ( cf_font_path ) {
    CFRelease( cf_font_path ), cf_font_path = NULL ; }
  if ( face ) {
    CFRelease(face), face = NULL; }

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
};
