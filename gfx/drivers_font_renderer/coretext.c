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
/* Padding is required between each glyph in the atlas to prevent
 * texture bleed when drawing with linear filtering enabled */
#define CT_ATLAS_PADDING 1

/* Mix in upper bits to reduce clustering for CJK and other
 * non-Latin codepoints */
#define CT_HASH_SIZE 0x100
#define CT_HASH(c) (((c) ^ ((c) >> 8)) & (CT_HASH_SIZE - 1))

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
   coretext_atlas_slot_t *uc_map[CT_HASH_SIZE];
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
   map_id = CT_HASH(handle->atlas_slots[oldest].charcode);
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

/* Merge one updated glyph cell into the atlas dirty region */
static void font_renderer_ct_dirty_cell(struct font_atlas *atlas,
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

static const struct font_glyph *font_renderer_ct_get_glyph(
      void *data, uint32_t charcode)
{
   unsigned map_id;
   coretext_atlas_slot_t *atlas_slot = NULL;
   ct_font_renderer_t        *handle = (ct_font_renderer_t*)data;

   if (!handle)
      return NULL;

   map_id = CT_HASH(charcode);
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
   if (handle->font_face &&
         coretext_font_renderer_render_glyph(handle->font_face, handle,
               atlas_slot, charcode))
      /* The cell is always written in full (glyph coverage or the
       * missing-glyph rectangle), so it is the dirty unit; failed
       * renders write nothing and no longer mark the atlas dirty */
      font_renderer_ct_dirty_cell(&handle->atlas,
            atlas_slot->glyph.atlas_offset_x,
            atlas_slot->glyph.atlas_offset_y,
            atlas_slot->glyph.width, atlas_slot->glyph.height);

   atlas_slot->last_used = handle->usage_counter++;
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

   /* Clamp the per-glyph cell so the atlas stays within common GPU
    * texture limits and the width * height product cannot overflow;
    * font_size ultimately comes from user configuration. */
   if (max_glyph_size < 1)
      return false;
   if (max_glyph_size > 127)
      max_glyph_size = 127;

   handle->atlas.width         = (max_glyph_size + CT_ATLAS_PADDING) * CT_ATLAS_COLS;
   handle->atlas.height        = (max_glyph_size + CT_ATLAS_PADDING) * CT_ATLAS_ROWS;

   handle->atlas.buffer        = (uint8_t*)calloc(
         (size_t)handle->atlas.height, (size_t)handle->atlas.width);

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
         slot->glyph.atlas_offset_x = x * (max_glyph_size + CT_ATLAS_PADDING);
         slot->glyph.atlas_offset_y = y * (max_glyph_size + CT_ATLAS_PADDING);
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

/* Copy rasterized coverage into the atlas. This helper and the
 * bitmap-context format in coretext_font_renderer_render_glyph() are
 * the ONLY places that know the atlas is 8-bit; a higher-bit-depth
 * atlas (HDR output) needs a sibling of this routine and an alternate
 * context format, nothing else.
 *
 * Notes for that future path: kCGImageAlphaOnly contexts are 8 bits
 * per pixel only. The >= 10-bit variant renders white-on-transparent
 * into a 16 bits-per-component DeviceGray context
 * (kCGImageAlphaNone | kCGBitmapByteOrder16Host) and copies the gray
 * channel out as 16-bit coverage. CoreGraphics has no meaningful OS
 * floor for such contexts; the version gates are all on the display
 * side and are already the video driver's concern:
 * CAMetalLayer.wantsExtendedDynamicRangeContent (scRGB/EDR) needs
 * macOS 10.11+ / iOS 16.0+, CAEDRMetadata (HDR10 PQ tone mapping)
 * needs macOS 10.15+ / iOS 16.0+, and neither exists on tvOS. */
static void coretext_font_renderer_copy_coverage(
      ct_font_renderer_t *handle, coretext_atlas_slot_t *slot,
      const uint8_t *src)
{
   unsigned r, c;
   uint8_t *dst = (uint8_t*)handle->atlas.buffer +
         slot->glyph.atlas_offset_x +
         slot->glyph.atlas_offset_y * handle->atlas.width;

   for (r = 0; r < slot->glyph.height; r++)
      for (c = 0; c < slot->glyph.width; c++)
         dst[r * handle->atlas.width + c] = src[r * slot->glyph.width + c];
}

static bool coretext_font_renderer_render_glyph(CTFontRef face, ct_font_renderer_t *handle, coretext_atlas_slot_t *slot, uint32_t charcode)
{
   CGGlyph glyphs[2];
   CGRect bounds;
   CGSize advance;
   CGContextRef offscreen;
   void *bitmapData;
   /* UTF-16 encoding of the codepoint: one unit for the BMP, a
    * surrogate pair beyond it. Truncating to a single UniChar would
    * render the wrong glyph for anything above U+FFFF. */
   UniChar utf16[2];
   CFIndex utf16_len;
   CFStringRef glyph_cfstr;
   CFAttributedStringRef attrString;
   CTLineRef line;
   uint8_t *dst;
   unsigned r, c;
   bool has_glyph;

   if (charcode > 0x10FFFF || (charcode >= 0xD800 && charcode <= 0xDFFF))
      has_glyph = false;
   else
   {
      if (charcode > 0xFFFF)
      {
         uint32_t v  = charcode - 0x10000;
         utf16[0]    = (UniChar)(0xD800 + (v >> 10));
         utf16[1]    = (UniChar)(0xDC00 + (v & 0x3FF));
         utf16_len   = 2;
      }
      else
      {
         utf16[0]    = (UniChar)charcode;
         utf16_len   = 1;
      }

      /* Get glyph for character */
      has_glyph = CTFontGetGlyphsForCharacters(face, utf16, glyphs, utf16_len);
   }

   /* If character is not available in font, render a missing glyph rectangle */
   if (!has_glyph)
   {
      /* Draw rectangle directly in atlas buffer */
      dst = (uint8_t*)handle->atlas.buffer +
            slot->glyph.atlas_offset_x +
            slot->glyph.atlas_offset_y * handle->atlas.width;

      /* Only draw the rectangle if the cell is large enough. All
       * positions below are cell-relative and stay inside the cell,
       * which tiles the atlas exactly, so no further bounds checks
       * are needed. */
      if (slot->glyph.width >= 6 && slot->glyph.height >= 6)
      {
         unsigned max_r = slot->glyph.height - 2;
         unsigned max_c = slot->glyph.width - 2;

         /* Draw 2-pixel border rectangle */
         for (r = 2; r < max_r; r++)
         {
            dst[r * handle->atlas.width + 2]         = 255; /* Left  */
            dst[r * handle->atlas.width + max_c - 1] = 255; /* Right */
         }
         for (c = 2; c < max_c; c++)
         {
            dst[2 * handle->atlas.width + c]           = 255; /* Top    */
            dst[(max_r - 1) * handle->atlas.width + c] = 255; /* Bottom */
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
         glyphs, &bounds, 1);

   CTFontGetAdvancesForGlyphs(face,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
         kCTFontOrientationDefault,
#else
         kCTFontDefaultOrientation,
#endif
         glyphs, &advance, 1);

   /* Set up glyph metrics using cached ascent */
   slot->glyph.draw_offset_x = (int)ceil(bounds.origin.x);
   slot->glyph.draw_offset_y = (int)floor(-bounds.origin.y) - (int)floor(handle->cached_ascent) + 1;
   /* round() is C99; advances are non-negative so floor(x + 0.5)
    * is equivalent */
   slot->glyph.advance_x     = (int)floor(advance.width + 0.5);
   slot->glyph.advance_y     = (int)floor(advance.height + 0.5);

   /* Create bitmap context */
   bitmapData = calloc(slot->glyph.height, slot->glyph.width);
   /* NULL-check: CGBitmapContextCreate tolerates NULL (it will
    * allocate its own backing store), but the byte-wise copy
    * into the atlas at lines ~321-325 dereferences bitmapData
    * as 'src'.  If we proceeded on NULL, CoreGraphics might
    * give us a valid context, we'd draw into it, and then
    * NULL-deref on the atlas copy step.  Fail cleanly now. */
   if (!bitmapData)
      return false;
   /* 8-bit alpha-only coverage; see coretext_font_renderer_copy_coverage()
    * for the higher-bit-depth (HDR) variant of this format. */
   offscreen  = CGBitmapContextCreate(bitmapData, slot->glyph.width, slot->glyph.height,
                                      8, slot->glyph.width, NULL, kCGImageAlphaOnly);

   if (!offscreen)
   {
      free(bitmapData);
      return false;
   }

   CGContextSetTextMatrix(offscreen, CGAffineTransformIdentity);

   /* Create string from Unicode character using cached dictionary.
    * Each CF/CT allocation is checked: passing NULL onwards or
    * CFRelease(NULL) would crash rather than fail. */
   if (!(glyph_cfstr = CFStringCreateWithCharacters(NULL, utf16, utf16_len)))
   {
      CGContextRelease(offscreen);
      free(bitmapData);
      return false;
   }
   attrString = CFAttributedStringCreate(NULL, glyph_cfstr, handle->attr_dict);
   CFRelease(glyph_cfstr);
   if (!attrString)
   {
      CGContextRelease(offscreen);
      free(bitmapData);
      return false;
   }
   line = CTLineCreateWithAttributedString(attrString);
   CFRelease(attrString);
   if (!line)
   {
      CGContextRelease(offscreen);
      free(bitmapData);
      return false;
   }

   /* Render glyph */
   CGContextSetTextPosition(offscreen, -bounds.origin.x, -bounds.origin.y);
   CTLineDraw(line, offscreen);
   CFRelease(line);

   /* Copy bitmap to atlas */
   coretext_font_renderer_copy_coverage(handle, slot,
         (const uint8_t*)bitmapData);

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
                     NULL, font_path, kCFStringEncodingUTF8)))
   {
      err = 1;
      goto error;
   }

   /* Each step is checked before use: several of these APIs do not
    * accept NULL arguments. */
   if (!(url = CFURLCreateWithFileSystemPath(
         kCFAllocatorDefault, cf_font_path, kCFURLPOSIXPathStyle, false)))
   {
      err = 1;
      goto error;
   }
   if (!(dataProvider = CGDataProviderCreateWithURL(url)))
   {
      err = 1;
      goto error;
   }
   if (!(theCGFont = CGFontCreateWithDataProvider(dataProvider)))
   {
      err = 1;
      goto error;
   }
   if (!(face = CTFontCreateWithGraphicsFont(theCGFont, font_size, NULL, NULL)))
   {
      err = 1;
      goto error;
   }

   /* Store the font face for on-demand glyph rendering */
   handle->font_face = face;
   CFRetain(face);

   /* Create reusable attribute dictionary for performance */
   {
      /* C89: block-scope aggregate initializers must be constant */
      CFTypeRef values[1];
      CFStringRef keys[1];
      values[0] = face;
      keys[0]   = kCTFontAttributeName;
      handle->attr_dict = CFDictionaryCreate(NULL, (const void **)&keys, (const void **)&values,
            1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   }
   if (!handle->attr_dict)
   {
      err = 1;
      goto error;
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
   if (!handle)
      return;
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
