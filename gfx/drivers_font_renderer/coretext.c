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

#include <CoreFoundation/CFString.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef IOS
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif

#include <file/file_path.h>

#include "../font_driver.h"

#define CT_ATLAS_ROWS 16
#define CT_ATLAS_COLS 16
#define CT_ATLAS_SIZE (CT_ATLAS_ROWS * CT_ATLAS_COLS)

typedef struct coretext_atlas_slot
{
   struct font_glyph glyph;
   unsigned charcode;
   unsigned last_used;
   struct coretext_atlas_slot *next;
} coretext_atlas_slot_t;

typedef struct coretext_renderer
{
   struct font_atlas atlas;
   coretext_atlas_slot_t atlas_slots[CT_ATLAS_SIZE];
   coretext_atlas_slot_t *uc_map[0x100];
   struct font_line_metrics line_metrics;
   unsigned usage_counter;
   CTFontRef font_face;
   CFDictionaryRef attr_dict;  /* Reused for all glyphs */
   float cached_ascent;        /* Cached font ascent */
} ct_font_renderer_t;

static bool coretext_font_renderer_render_glyph(CTFontRef face, ct_font_renderer_t *handle, coretext_atlas_slot_t *slot, uint32_t charcode);

static struct font_atlas *font_renderer_ct_get_atlas(void *data)
{
   ct_font_renderer_t *handle = (ct_font_renderer_t*)data;
   if (!handle)
      return NULL;
   return &handle->atlas;
}

static coretext_atlas_slot_t* coretext_font_renderer_get_slot(ct_font_renderer_t *handle)
{
   int i, map_id;
   unsigned oldest = 0;

   for (i = 1; i < CT_ATLAS_SIZE; i++)
      if ((handle->usage_counter - handle->atlas_slots[i].last_used) >
            (handle->usage_counter - handle->atlas_slots[oldest].last_used))
         oldest = i;

   /* remove from map */
   map_id = handle->atlas_slots[oldest].charcode & 0xFF;
   if (handle->uc_map[map_id] == &handle->atlas_slots[oldest])
      handle->uc_map[map_id] = handle->atlas_slots[oldest].next;
   else if (handle->uc_map[map_id])
   {
      coretext_atlas_slot_t* ptr = handle->uc_map[map_id];
      while (ptr->next && ptr->next != &handle->atlas_slots[oldest])
         ptr = ptr->next;
      ptr->next = handle->atlas_slots[oldest].next;
   }

   return &handle->atlas_slots[oldest];
}

static const struct font_glyph *font_renderer_ct_get_glyph(
      void *data, uint32_t charcode)
{
   unsigned map_id;
   coretext_atlas_slot_t *atlas_slot = NULL;
   ct_font_renderer_t        *handle = (ct_font_renderer_t*)data;

   if (!handle)
      return NULL;

   map_id = charcode & 0xFF;
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

   /* Character not found, need to create it */
   atlas_slot = coretext_font_renderer_get_slot(handle);
   atlas_slot->charcode = charcode;
   atlas_slot->next = handle->uc_map[map_id];
   handle->uc_map[map_id] = atlas_slot;

   /* Render the glyph on demand */
   if (handle->font_face)
      coretext_font_renderer_render_glyph(handle->font_face, handle, atlas_slot, charcode);

   atlas_slot->last_used = handle->usage_counter++;
   handle->atlas.dirty = true;
   return &atlas_slot->glyph;
}

static void font_renderer_ct_free(void *data)
{
   ct_font_renderer_t *handle = (ct_font_renderer_t*)data;

   if (!handle)
      return;

   if (handle->font_face)
   {
      CFRelease(handle->font_face);
      handle->font_face = NULL;
   }

   if (handle->attr_dict)
   {
      CFRelease(handle->attr_dict);
      handle->attr_dict = NULL;
   }

   free(handle->atlas.buffer);
   free(handle);
}

static bool coretext_font_renderer_create_atlas(CTFontRef face, ct_font_renderer_t *handle, float font_size)
{
   unsigned i, x, y;
   coretext_atlas_slot_t* slot = NULL;
   int max_glyph_size          = (font_size < 0) ? -font_size : font_size;
   float ascent, descent;

   handle->atlas.width         = max_glyph_size * CT_ATLAS_COLS;
   handle->atlas.height        = max_glyph_size * CT_ATLAS_ROWS;

   handle->atlas.buffer        = (uint8_t*)calloc(
         handle->atlas.width * handle->atlas.height, 1);

   if (!handle->atlas.buffer)
      return false;

   ascent  = CTFontGetAscent(face);
   descent = CTFontGetDescent(face);

   /* Cache ascent for performance */
   handle->cached_ascent = ascent;

   handle->line_metrics.ascender  = ascent;
   handle->line_metrics.descender = (descent < 0.0f) ? (-1.0f * descent) : descent;
   handle->line_metrics.height    = handle->line_metrics.ascender + handle->line_metrics.descender +
         (float)CTFontGetLeading(face);

   slot = handle->atlas_slots;

   for (y = 0; y < CT_ATLAS_ROWS; y++)
   {
      for (x = 0; x < CT_ATLAS_COLS; x++)
      {
         slot->glyph.atlas_offset_x = x * max_glyph_size;
         slot->glyph.atlas_offset_y = y * max_glyph_size;
         slot->glyph.width          = max_glyph_size;
         slot->glyph.height         = max_glyph_size;
         slot++;
      }
   }

   /* Pre-generate common ASCII characters */
   for (i = 32; i < 128; i++)
      font_renderer_ct_get_glyph(handle, i);

   return true;
}

static bool coretext_font_renderer_render_glyph(CTFontRef face, ct_font_renderer_t *handle, coretext_atlas_slot_t *slot, uint32_t charcode)
{
   CGGlyph glyph;
   CGRect bounds;
   CGSize advance;
   CGContextRef offscreen;
   void *bitmapData;
   UniChar character = (UniChar)charcode;
   CFStringRef glyph_cfstr;
   CFAttributedStringRef attrString;
   CTLineRef line;
   uint8_t *dst;
   const uint8_t *src;
   unsigned r, c;

   /* Get glyph for character */
   bool has_glyph = CTFontGetGlyphsForCharacters(face, &character, &glyph, 1);

   /* If character is not available in font, render a missing glyph rectangle */
   if (!has_glyph)
   {
      /* Draw rectangle directly in atlas buffer */
      dst = (uint8_t*)handle->atlas.buffer +
            slot->glyph.atlas_offset_x +
            slot->glyph.atlas_offset_y * handle->atlas.width;

      /* Only draw rectangle if glyph is large enough and within atlas bounds */
      if (slot->glyph.width >= 6 && slot->glyph.height >= 6)
      {
         unsigned max_r = slot->glyph.height - 2;
         unsigned max_c = slot->glyph.width - 2;
         int atlas_size = handle->atlas.width * handle->atlas.height;

         /* Draw 2-pixel border rectangle */
         for (r = 2; r < max_r; r++)
         {
            int left_pos = r * handle->atlas.width + 2;
            int right_pos = r * handle->atlas.width + max_c - 1;
            if (left_pos < atlas_size && right_pos < atlas_size)
            {
               dst[left_pos] = 255;   /* Left */
               dst[right_pos] = 255;  /* Right */
            }
         }
         for (c = 2; c < max_c; c++)
         {
            int top_pos = 2 * handle->atlas.width + c;
            int bottom_pos = (max_r - 1) * handle->atlas.width + c;
            if (top_pos < atlas_size && bottom_pos < atlas_size)
            {
               dst[top_pos] = 255;    /* Top */
               dst[bottom_pos] = 255; /* Bottom */
            }
         }
      }

      /* Set basic metrics using cached ascent */
      slot->glyph.draw_offset_x = 0;
      slot->glyph.draw_offset_y = (int)floor(-handle->cached_ascent);
      slot->glyph.advance_x     = slot->glyph.width;
      slot->glyph.advance_y     = 0;
      return true;
   }

   CTFontGetBoundingRectsForGlyphs(face,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
         kCTFontOrientationDefault,
#else
         kCTFontDefaultOrientation,
#endif
         &glyph, &bounds, 1);

   CTFontGetAdvancesForGlyphs(face,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
         kCTFontOrientationDefault,
#else
         kCTFontDefaultOrientation,
#endif
         &glyph, &advance, 1);

   /* Set up glyph metrics using cached ascent */
   slot->glyph.draw_offset_x = (int)ceil(bounds.origin.x);
   slot->glyph.draw_offset_y = (int)floor(-bounds.origin.y) - (int)floor(handle->cached_ascent) + 1;
   slot->glyph.advance_x     = (int)round(advance.width);
   slot->glyph.advance_y     = (int)round(advance.height);

   /* Create bitmap context */
   bitmapData = calloc(slot->glyph.height, slot->glyph.width);
   offscreen  = CGBitmapContextCreate(bitmapData, slot->glyph.width, slot->glyph.height,
                                      8, slot->glyph.width, NULL, kCGImageAlphaOnly);

   if (!offscreen)
   {
      free(bitmapData);
      return false;
   }

   CGContextSetTextMatrix(offscreen, CGAffineTransformIdentity);

   /* Create string from Unicode character using cached dictionary */
   glyph_cfstr = CFStringCreateWithCharacters(NULL, &character, 1);
   attrString  = CFAttributedStringCreate(NULL, glyph_cfstr, handle->attr_dict);
   CFRelease(glyph_cfstr);
   line        = CTLineCreateWithAttributedString(attrString);
   CFRelease(attrString);

   /* Render glyph */
   CGContextSetTextPosition(offscreen, -bounds.origin.x, -bounds.origin.y);
   CTLineDraw(line, offscreen);
   CFRelease(line);

   /* Copy bitmap to atlas */
   dst = (uint8_t*)handle->atlas.buffer +
         slot->glyph.atlas_offset_x +
         slot->glyph.atlas_offset_y * handle->atlas.width;
   src = (const uint8_t*)bitmapData;

   for (r = 0; r < slot->glyph.height; r++)
      for (c = 0; c < slot->glyph.width; c++)
         dst[r * handle->atlas.width + c] = src[r * slot->glyph.width + c];

   CGContextRelease(offscreen);
   free(bitmapData);

   return true;
}

static void *font_renderer_ct_init(const char *font_path, float font_size)
{
   char err                       = 0;
   CFStringRef cf_font_path       = NULL;
   CTFontRef face                 = NULL;
   CFURLRef url                   = NULL;
   CGDataProviderRef dataProvider = NULL;
   CGFontRef theCGFont            = NULL;
   ct_font_renderer_t *handle     = (ct_font_renderer_t*)calloc(1, sizeof(*handle));

   if (!handle || !path_is_valid(font_path))
   {
      err = 1;
      goto error;
   }

   if (!(cf_font_path = CFStringCreateWithCString(
                     NULL, font_path, kCFStringEncodingASCII)))
   {
      err = 1;
      goto error;
   }

   url          = CFURLCreateWithFileSystemPath(
         kCFAllocatorDefault, cf_font_path, kCFURLPOSIXPathStyle, false);
   dataProvider = CGDataProviderCreateWithURL(url);
   theCGFont    = CGFontCreateWithDataProvider(dataProvider);
   face         = CTFontCreateWithGraphicsFont(theCGFont, font_size, NULL, NULL);

   if (!face)
   {
      err = 1;
      goto error;
   }

   /* Store the font face for on-demand glyph rendering */
   handle->font_face = face;
   CFRetain(face);

   /* Create reusable attribute dictionary for performance */
   {
      CFTypeRef values[1] = {face};
      CFStringRef keys[1] = {kCTFontAttributeName};
      handle->attr_dict = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values,
            1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   }

   if (!coretext_font_renderer_create_atlas(face, handle, font_size))
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

   if (url)
   {
      CFRelease(url);
      url = NULL;
   }

   if (dataProvider)
   {
      CFRelease(dataProvider);
      dataProvider = NULL;
   }

   if (theCGFont)
   {
      CFRelease(theCGFont);
      theCGFont = NULL;
   }

   return handle;
}

static const char *font_renderer_ct_get_default_font(void)
{
   /* We can't tell if a font is going to be there until we actually
      initialize CoreText and the best way to get fonts is by name, not
      by path. */
   return "Verdana";
}

static void font_renderer_ct_get_line_metrics(
      void* data, struct font_line_metrics **metrics)
{
   ct_font_renderer_t *handle   = (ct_font_renderer_t*)data;
   *metrics = &handle->line_metrics;
}

font_renderer_driver_t coretext_font_renderer = {
   font_renderer_ct_init,
   font_renderer_ct_get_atlas,
   font_renderer_ct_get_glyph,
   font_renderer_ct_free,
   font_renderer_ct_get_default_font,
   "font_renderer_ct",
   font_renderer_ct_get_line_metrics
};
