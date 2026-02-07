/*
 * stb_truetype.h - v1.06 - public domain
 * authored from 2009-2014 by Sean Barrett / RAD Game Tools
 *
 *   This library processes TrueType files:
 *        parse files
 *        extract glyph metrics
 *        extract glyph shapes
 *        render glyphs to one-channel bitmaps with antialiasing (box filter)
 *
 *   Todo:
 *        non-MS cmaps
 *        crashproof on bad data
 *        hinting? (no longer patented)
 *        cleartype-style AA?
 *        optimize: use simple memory allocator for intermediates
 *        optimize: build edge-list directly from curves
 *        optimize: rasterize directly from curves?
 *
 * ADDITIONAL CONTRIBUTORS
 *
 *   Mikko Mononen: compound shape support, more cmap formats
 *   Tor Andersson: kerning, subpixel rendering
 *
 *   Bug/warning reports/fixes:
 *       "Zer" on mollyrocket (with fix)
 *       Cass Everitt
 *       stoiko (Haemimont Games)
 *       Brian Hook
 *       Walter van Niftrik
 *       David Gow
 *       David Given
 *       Ivan-Assen Ivanov
 *       Anthony Pesch
 *       Johan Duparc
 *       Hou Qiming
 *       Fabian "ryg" Giesen
 *       Martins Mozeiko
 *       Cap Petschulat
 *       Omar Cornut
 *       github:aloucks
 *       Peter LaValle
 *
 *   Misc other:
 *       Ryan Gordon
 *
 * LICENSE
 *
 *   This software is in the public domain. Where that dedication is not
 *   recognized, you are granted a perpetual, irrevokable license to copy
 *   and modify this file as you see fit.
 *
 * USAGE
 *
 *   Include this file in whatever places neeed to refer to it. In ONE C/C++
 *   file, write:
 *      #define STB_TRUETYPE_IMPLEMENTATION
 *   before the #include of this file. This expands out the actual
 *   implementation into that C/C++ file.
 *
 *   To make the implementation private to the file that generates the implementation,
 *      #define STBTT_STATIC
 *
 *   Improved 3D API (more shippable):
 *           #include "stb_rect_pack.h"           -- optional, but you really want it
 *           stbtt_PackBegin()
 *           stbtt_PackFontRanges()
 *           stbtt_PackEnd()
 *
 *   "Load" a font file from a memory buffer (you have to keep the buffer loaded)
 *           stbtt_InitFont()
 *           stbtt_GetFontOffsetForIndex()        -- use for TTC font collections
 *
 *   Character advance/positioning
 *           stbtt_GetFontVMetrics()
 *
 * ADDITIONAL DOCUMENTATION
 *
 *   Immediately after this block comment are a series of sample programs.
 *
 *   After the sample programs is the "header file" section. This section
 *   includes documentation for each API function.
 *
 *   Some important concepts to understand to use this library:
 *
 *      Codepoint
 *         Characters are defined by unicode codepoints, e.g. 65 is
 *         uppercase A, 231 is lowercase c with a cedilla, 0x7e30 is
 *         the hiragana for "ma".
 *
 *      Glyph
 *         A visual character shape (every codepoint is rendered as
 *         some glyph)
 *
 *      Glyph index
 *         A font-specific integer ID representing a glyph
 *
 *      Baseline
 *         Glyph shapes are defined relative to a baseline, which is the
 *         bottom of uppercase characters. Characters extend both above
 *         and below the baseline.
 *
 *      Current Point
 *         As you draw text to the screen, you keep track of a "current point"
 *         which is the origin of each character. The current point's vertical
 *         position is the baseline. Even "baked fonts" use this model.
 *
 *      Vertical Font Metrics
 *         The vertical qualities of the font, used to vertically position
 *         and space the characters. See docs for stbtt_GetFontVMetrics.
 *
 *      Font Size in Pixels or Points
 *         The preferred interface for specifying font sizes in stb_truetype
 *         is to specify how tall the font's vertical extent should be in pixels.
 *         If that sounds good enough, skip the next paragraph.
 *
 *         Most font APIs instead use "points", which are a common typographic
 *         measurement for describing font size, defined as 72 points per inch.
 *         stb_truetype provides a point API for compatibility. However, true
 *         "per inch" conventions don't make much sense on computer displays
 *         since they different monitors have different number of pixels per
 *         inch. For example, Windows traditionally uses a convention that
 *         there are 96 pixels per inch, thus making 'inch' measurements have
 *         nothing to do with inches, and thus effectively defining a point to
 *         be 1.333 pixels. Additionally, the TrueType font data provides
 *         an explicit scale factor to scale a given font's glyphs to points,
 *         but the author has observed that this scale factor is often wrong
 *         for non-commercial fonts, thus making fonts scaled in points
 *         according to the TrueType spec incoherently sized in practice.
 *
 * ADVANCED USAGE
 *
 *   Quality:
 *
 *    - Use the functions with Subpixel at the end to allow your characters
 *      to have subpixel positioning. Since the font is anti-aliased, not
 *      hinted, this is very import for quality. (This is not possible with
 *      baked fonts.)
 *
 *    - Kerning is now supported, and if you're supporting subpixel rendering
 *      then kerning is worth using to give your text a polished look.
 *
 *   Performance:
 *
 *    - Convert Unicode codepoints to glyph indexes and operate on the glyphs;
 *      if you don't do this, stb_truetype is forced to do the conversion on
 *      every call.
 *
 *    - There are a lot of memory allocations. We should modify it to take
 *      a temp buffer and allocate from the temp buffer (without freeing),
 *      should help performance a lot.
 *
 * NOTES
 *
 *   The system uses the raw data found in the .ttf file without changing it
 *   and without building auxiliary data structures. This is a bit inefficient
 *   on little-endian systems (the data is big-endian), but assuming you're
 *   caching the bitmaps or glyph shapes this shouldn't be a big deal.
 *
 *   It appears to be very hard to programmatically determine what font a
 *   given file is in a general way. I provide an API for this, but I don't
 *   recommend it.
 *
 *
 * SOURCE STATISTICS (based on v0.6c, 2050 LOC)
 *
 *   Documentation & header file        520 LOC  \___ 660 LOC documentation
 *   Sample code                        140 LOC  /
 *   Truetype parsing                   620 LOC  ---- 620 LOC TrueType
 *   Software rasterization             240 LOC  \                           .
 *   Curve tesselation                  120 LOC   \__ 550 LOC Bitmap creation
 *   Bitmap management                  100 LOC   /
 *   Baked bitmap interface              70 LOC  /
 *   Font name matching & access        150 LOC  ---- 150
 *   C runtime library abstraction       60 LOC  ----  60
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* INTERFACE */

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

#ifdef STBTT_STATIC
#define STBTT_DEF STATIC
#else
#define STBTT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* NEW TEXTURE BAKING API */

/* This provides options for packing multiple fonts into one atlas, not
 * perfectly but better than nothing. */

typedef struct
{
   uint16_t x0,y0,x1,y1; /* coordinates of bbox in bitmap */
   float xoff,yoff,xadvance;
   float xoff2,yoff2;
} stbtt_packedchar;

typedef struct stbtt_pack_context stbtt_pack_context;

STBTT_DEF int  stbtt_PackBegin(stbtt_pack_context *spc,
      unsigned char *pixels, int width, int height,
      int stride_in_bytes, int padding, void *alloc_context);

/* Initializes a packing context stored in the passed-in stbtt_pack_context.
 * Future calls using this context will pack characters into the bitmap passed
 * in here: a 1-channel bitmap that is weight x height. stride_in_bytes is
 * the distance from one row to the next (or 0 to mean they are packed tightly
 * together). "padding" is // the amount of padding to leave between each
 * character (normally you want '1' for bitmaps you'll use as textures with
 * bilinear filtering).
 *
 * Returns 0 on failure, 1 on success.
 */

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc);

/* Cleans up the packing context and frees all memory. */

#define STBTT_POINT_SIZE(x)   (-(x))

STBTT_DEF int  stbtt_PackFontRange(stbtt_pack_context *spc, unsigned char *fontdata, int font_index, float font_size,
                                int first_unicode_char_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range);

/* Creates character bitmaps from the font_index'th font found in fontdata (use
 * font_index=0 if you don't know what that is). It creates num_chars_in_range
 * bitmaps for characters with unicode values starting at first_unicode_char_in_range
 * and increasing. Data for how to render them is stored in chardata_for_range;
 * pass these to stbtt_GetPackedQuad to get back renderable quads.
 *
 * font_size is the full height of the character from ascender to descender,
 * as computed by stbtt_ScaleForPixelHeight. To use a point size as computed
 * by stbtt_ScaleForMappingEmToPixels, wrap the point size in STBTT_POINT_SIZE()
 * and pass that result as 'font_size':
 * ...,                  20 , ... // font max minus min y is 20 pixels tall
 * ..., STBTT_POINT_SIZE(20), ... // 'M' is 20 pixels tall
 */

typedef struct
{
   float font_size;
   int first_unicode_char_in_range;
   int num_chars_in_range;
   stbtt_packedchar *chardata_for_range; /* output */
} stbtt_pack_range;

STBTT_DEF int  stbtt_PackFontRanges(stbtt_pack_context *spc, unsigned char *fontdata,
      int font_index, stbtt_pack_range *ranges, int num_ranges);

/* Creates character bitmaps from multiple ranges of characters stored in
 * ranges. This will usually create a better-packed bitmap than multiple
 * calls to stbtt_PackFontRange.
 */

/* this is an opaque structure that you shouldn't mess with which holds
 * all the context needed from PackBegin to PackEnd. */
struct stbtt_pack_context
{
   void *user_allocator_context;
   void *pack_info;
   int   width;
   int   height;
   int   stride_in_bytes;
   int   padding;
   unsigned int   h_oversample, v_oversample;
   unsigned char *pixels;
   void  *nodes;
};

/* FONT LOADING */

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);

/* Each .ttf/.ttc file may have more than one font. Each font has a sequential
 * index number starting from 0. Call this function to get the font offset for
 * a given index; it returns -1 if the index is out of range. A regular .ttf
 * file will only define one font and it always be at offset 0, so it will
 * return '0' for index 0, and -1 for all other indices. You can just skip
 * this step if you know it's that kind of font.
 */

/* The following structure is defined publically so you can declare one on
 * the stack or as a global or etc, but you should treat it as opaque.
 */
typedef struct stbtt_fontinfo
{
   void           *userdata;
   unsigned char  *data;              /* pointer to .ttf file */
   int             fontstart;         /* offset of start of font */

   int numGlyphs;                      /* number of glyphs, needed for range checking */

   int loca,head,glyf,hhea,hmtx,kern;  /* table locations as offset from start of .ttf */
   int index_map;                      /* a cmap mapping for our chosen character encoding */
   int indexToLocFormat;               /* format needed to map from glyph index to glyph */
} stbtt_fontinfo;

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);

/* Given an offset into the file that defines a font, this function builds
 * the necessary cached info for the rest of the system. You must allocate
 * the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
 * need to do anything special to free it, because the contents are pure
 * value data with no additional data structures. Returns 0 on failure.
 */

/* CHARACTER TO GLYPH-INDEX CONVERSION */

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);

/* If you're going to perform multiple operations on the same character
 * and you want a speed-up, call this function with the character you're
 * going to process, then use glyph-based functions instead of the
 * codepoint-based functions.
 */

/* CHARACTER PROPERTIES */

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);

/* computes a scale factor to produce a font whose "height" is 'pixels' tall.
 * Height is measured as the distance from the highest ascender to the lowest
 * descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
 * and computing:
 *       scale = pixels / (ascent - descent)
 * so if you prefer to measure height by the ascent only, use a similar calculation.
 */

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

/* computes a scale factor to produce a font whose EM size is mapped to
 * 'pixels' tall. This is probably what traditional APIs compute, but
 * I'm not positive.
 */

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);

/* ascent is the coordinate above the baseline the font extends; descent
 * is the coordinate below the baseline the font extends (i.e. it is typically negative)
 * lineGap is the spacing between one row's descent and the next row's ascent...
 * so you should advance the vertical position by "*ascent - *descent + *lineGap"
 *   these are expressed in unscaled coordinates, so you must multiply by
 *   the scale factor for a given size
 */

/* Gets the bounding box of the visible part of the glyph, in unscaled coordinates */

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info,
      int glyph_index, int *advanceWidth, int *leftSideBearing);

STBTT_DEF int  stbtt_GetGlyphBox(const stbtt_fontinfo *info,
      int glyph_index, int *x0, int *y0, int *x1, int *y1);

/* as above, but takes one or more glyph indices for greater efficiency */

/* GLYPH SHAPES (you probably don't need these, but they have to go before
 * the bitmaps for C declaration-order reasons) */

#ifndef STBTT_vmove
enum
{
   STBTT_vmove=1,
   STBTT_vline,
   STBTT_vcurve
};
#endif

#ifndef stbtt_vertex

typedef struct
{
   int16_t x,y,cx,cy;
   unsigned char type,padding;
} stbtt_vertex;
#endif

/* returns non-zero if nothing is drawn for this glyph */

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);

/* returns # of vertices and fills *vertices with the pointer to them
 *   these are expressed in "unscaled" coordinates
 *
 * The shape is a series of countours. Each one starts with
 * a STBTT_moveto, then consists of a series of mixed
 * STBTT_lineto and STBTT_curveto segments. A lineto
 * draws a line from previous endpoint to its x,y; a curveto
 * draws a quadratic bezier from previous endpoint to
 * its x,y, using cx,cy as the bezier control point.
 */

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);

/* frees the data allocated above */

/* BITMAP RENDERING */

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint,
      float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);

/* same as stbtt_GetCodepointBitmapBox, but you can specify a subpixel
 * shift for the character */

/* the following functions are equivalent to the above functions, but operate
 * on glyph indices instead of Unicode codepoints (for efficiency) */

STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info,
      float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);

STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info,
      float scale_x, float scale_y, float shift_x, float shift_y, int glyph,
      int *width, int *height, int *xoff, int *yoff);

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride, float scale_x, float scale_y,
      float shift_x, float shift_y, int glyph);

STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x,
      float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);

STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph,
      float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);


/* @TODO: don't expose this structure */

typedef struct
{
   int w,h,stride;
   unsigned char *pixels;
} stbtt__bitmap;

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts,
      float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata);

/* returns I/0 whether the first string interpreted as UTF8 is identical to
 * the second string interpreted as big-endian UTF16... useful for strings from next func
 */

enum
{
   /* platformID */
   STBTT_PLATFORM_ID_UNICODE   =0,
   STBTT_PLATFORM_ID_MAC       =1,
   STBTT_PLATFORM_ID_ISO       =2,
   STBTT_PLATFORM_ID_MICROSOFT =3
};

enum
{
   /* encodingID for STBTT_PLATFORM_ID_MICROSOFT */
   STBTT_MS_EID_SYMBOL        =0,
   STBTT_MS_EID_UNICODE_BMP   =1,
   STBTT_MS_EID_SHIFTJIS      =2,
   STBTT_MS_EID_UNICODE_FULL  =10
};

#ifdef __cplusplus
}
#endif

#endif /* __STB_INCLUDE_STB_TRUETYPE_H__ */

#include <retro_endianness.h>

/* IMPLEMENTATION */

#ifdef STB_TRUETYPE_IMPLEMENTATION

#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE   8
#endif

typedef int stbtt__test_oversample_pow2[(STBTT_MAX_OVERSAMPLE & (STBTT_MAX_OVERSAMPLE-1)) == 0 ? 1 : -1];

/* accessors to parse data from file */

/* on platforms that don't allow misaligned reads, if we want to allow
 * truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE
 */

#define ttBYTE(p)     (* (uint8_t *) (p))
#define ttCHAR(p)     (* (int8_t *) (p))
#define ttFixed(p)    ttLONG(p)

#if defined(MSB_FIRST) && !defined(ALLOW_UNALIGNED_TRUETYPE)
#define ttUSHORT(p)   (* (uint16_t *) (p))
#define ttSHORT(p)    (* (int16_t *) (p))
#define ttULONG(p)    (* (uint32_t *) (p))
#define ttLONG(p)     (* (int32_t *) (p))
#else
static uint16_t ttUSHORT(const uint8_t *p) { return p[0]*256 + p[1]; }
static int16_t ttSHORT(const uint8_t *p)   { return p[0]*256 + p[1]; }
static uint32_t ttULONG(const uint8_t *p)  { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
static int32_t ttLONG(const uint8_t *p)    { return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]; }
#endif

#define stbtt_tag4(p,c0,c1,c2,c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p,str)           stbtt_tag4(p,str[0],str[1],str[2],str[3])

static int stbtt__isfont(const uint8_t *font)
{
   /* check the version number */
   if (stbtt_tag4(font, '1',0,0,0))
      return 1; /* TrueType 1 */
   if (stbtt_tag(font, "typ1"))
      return 1; /* TrueType with type 1 font -- we don't support this! */
   if (stbtt_tag(font, "OTTO"))
      return 1; /* OpenType with CFF */
   if (stbtt_tag4(font, 0,1,0,0))
      return 1; /* OpenType 1.0 */
   return 0;
}

/* @OPTIMIZE: binary search */
static uint32_t stbtt__find_table(uint8_t *data, uint32_t fontstart, const char *tag)
{
   int32_t i;
   int32_t num_tables = ttUSHORT(data+fontstart+4);
   uint32_t tabledir = fontstart + 12;

   for (i=0; i < num_tables; ++i)
   {
      uint32_t loc = tabledir + 16*i;
      if (stbtt_tag(data+loc+0, tag))
         return ttULONG(data+loc+8);
   }
   return 0;
}

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *font_collection, int index)
{
   /* if it's just a font, there's only one valid index */
   if (stbtt__isfont(font_collection))
      return index == 0 ? 0 : -1;

   /* check if it's a TTC */
   if (stbtt_tag(font_collection, "ttcf"))
   {
      /* version 1? */
      if (ttULONG(font_collection+4) == 0x00010000 || ttULONG(font_collection+4) == 0x00020000)
      {
         int32_t n = ttLONG(font_collection+8);
         if (index >= n)
            return -1;
         return ttULONG(font_collection+12+index*14);
      }
   }
   return -1;
}

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data2, int fontstart)
{
   uint8_t *data = (uint8_t *) data2;
   uint32_t cmap, t;
   int32_t i,numTables;

   info->data = data;
   info->fontstart = fontstart;

   cmap = stbtt__find_table(data, fontstart, "cmap");       /* required */
   info->loca = stbtt__find_table(data, fontstart, "loca"); /* required */
   info->head = stbtt__find_table(data, fontstart, "head"); /* required */
   info->glyf = stbtt__find_table(data, fontstart, "glyf"); /* required */
   info->hhea = stbtt__find_table(data, fontstart, "hhea"); /* required */
   info->hmtx = stbtt__find_table(data, fontstart, "hmtx"); /* required */
   info->kern = stbtt__find_table(data, fontstart, "kern"); /* not required */

   if (!cmap || !info->loca || !info->head || !info->glyf || !info->hhea || !info->hmtx)
      return 0;

   t = stbtt__find_table(data, fontstart, "maxp");
   if (t)
      info->numGlyphs = ttUSHORT(data+t+4);
   else
      info->numGlyphs = 0xffff;

   /* find a cmap encoding table we understand *now* to avoid searching
    * later. (todo: could make this installable)
    * the same regardless of glyph.
    */
   numTables = ttUSHORT(data + cmap + 2);
   info->index_map = 0;
   for (i=0; i < numTables; ++i)
   {
      uint32_t encoding_record = cmap + 4 + 8 * i;

      /* find an encoding we understand: */

      switch(ttUSHORT(data+encoding_record))
      {
         case STBTT_PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data+encoding_record+2))
            {
               case STBTT_MS_EID_UNICODE_BMP:
               case STBTT_MS_EID_UNICODE_FULL:
                  /* MS/Unicode */
                  info->index_map = cmap + ttULONG(data+encoding_record+4);
                  break;
            }
            break;
         case STBTT_PLATFORM_ID_UNICODE:
            /* Mac/iOS has these
             * all the encodingIDs are unicode, so we don't bother to check it
             */
            info->index_map = cmap + ttULONG(data+encoding_record+4);
            break;
      }
   }
   if (info->index_map == 0)
      return 0;

   info->indexToLocFormat = ttUSHORT(data+info->head + 50);
   return 1;
}

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint)
{
   uint8_t *data      = info->data;
   uint32_t index_map = info->index_map;
   uint16_t format    = ttUSHORT(data + index_map + 0);
   if (format == 0)
   {
      /* apple byte encoding */
      int32_t bytes = ttUSHORT(data + index_map + 2);
      if (unicode_codepoint < bytes-6)
         return ttBYTE(data + index_map + 6 + unicode_codepoint);
   }
   else if (format == 6)
   {
      uint32_t first = ttUSHORT(data + index_map + 6);
      uint32_t count = ttUSHORT(data + index_map + 8);
      if ((uint32_t) unicode_codepoint >= first && (uint32_t) unicode_codepoint < first+count)
         return ttUSHORT(data + index_map + 10 + (unicode_codepoint - first)*2);
   }
   else if (format == 4)
   {
      /* standard mapping for windows fonts: binary search collection of ranges */
      uint16_t segcount = ttUSHORT(data+index_map+6) >> 1;
      uint16_t searchRange = ttUSHORT(data+index_map+8) >> 1;
      uint16_t entrySelector = ttUSHORT(data+index_map+10);
      uint16_t rangeShift = ttUSHORT(data+index_map+12) >> 1;

      /* do a binary search of the segments */
      uint32_t endCount = index_map + 14;
      uint32_t search = endCount;

      if (unicode_codepoint > 0xffff)
         return 0;

      /* they lie from endCount .. endCount + segCount
       * but searchRange is the nearest power of two, so... */
      if (unicode_codepoint >= ttUSHORT(data + search + rangeShift*2))
         search += rangeShift*2;

      /* now decrement to bias correctly to find smallest */
      search -= 2;
      while (entrySelector)
      {
         uint16_t end;
         searchRange >>= 1;
         end = ttUSHORT(data + search + searchRange*2);
         if (unicode_codepoint > end)
            search += searchRange*2;
         --entrySelector;
      }
      search += 2;

      {
         uint16_t offset, start;
         uint16_t item = (uint16_t) ((search - endCount) >> 1);

         start = ttUSHORT(data + index_map + 14 + segcount*2 + 2 + 2*item);
         if (unicode_codepoint < start)
            return 0;

         offset = ttUSHORT(data + index_map + 14 + segcount*6 + 2 + 2*item);
         if (offset == 0)
            return (uint16_t) (unicode_codepoint + ttSHORT(data + index_map + 14 + segcount*4 + 2 + 2*item));

         return ttUSHORT(data + offset + (unicode_codepoint-start)*2 + index_map + 14 + segcount*6 + 2 + 2*item);
      }
   }
   else if (format == 12 || format == 13)
   {
      uint32_t ngroups = ttULONG(data+index_map+12);
      int32_t low,high;
      low = 0; high = (int32_t)ngroups;
      /* Binary search the right group. */
      while (low < high)
      {
         int32_t mid = low + ((high-low) >> 1); /* rounds down, so low <= mid < high */
         uint32_t start_char = ttULONG(data+index_map+16+mid*12);
         uint32_t end_char = ttULONG(data+index_map+16+mid*12+4);
         if ((uint32_t) unicode_codepoint < start_char)
            high = mid;
         else if ((uint32_t) unicode_codepoint > end_char)
            low = mid+1;
         else
         {
            uint32_t start_glyph = ttULONG(data+index_map+16+mid*12+8);
            if (format == 12)
               return start_glyph + unicode_codepoint-start_char;
            /* format == 13 */
            return start_glyph;
         }
      }
   }
   /* @TODO */
   return 0;
}

static void stbtt_setvertex(stbtt_vertex *v, uint8_t type, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
   v->type = type;
   v->x    = (int16_t)x;
   v->y    = (int16_t)y;
   v->cx   = (int16_t)cx;
   v->cy   = (int16_t)cy;
}

static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index)
{
   int g1,g2;

   if (glyph_index >= info->numGlyphs)
      return -1; /* glyph index out of range */
   if (info->indexToLocFormat >= 2)
      return -1; /* unknown index->glyph map format */

   if (info->indexToLocFormat == 0)
   {
      g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
      g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
   }
   else
   {
      g1 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4);
      g2 = info->glyf + ttULONG (info->data + info->loca + glyph_index * 4 + 4);
   }

   return g1==g2 ? -1 : g1; /* if length is 0, return -1 */
}

STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info,
      int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
   int g = stbtt__GetGlyfOffset(info, glyph_index);
   if (g < 0) return 0;

   if (x0) *x0 = ttSHORT(info->data + g + 2);
   if (y0) *y0 = ttSHORT(info->data + g + 4);
   if (x1) *x1 = ttSHORT(info->data + g + 6);
   if (y1) *y1 = ttSHORT(info->data + g + 8);
   return 1;
}

static int stbtt__close_shape(stbtt_vertex *vertices, int num_vertices, int was_off, int start_off,
    int32_t sx, int32_t sy, int32_t scx, int32_t scy, int32_t cx, int32_t cy)
{
   if (start_off)
   {
      if (was_off)
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+scx)>>1, (cy+scy)>>1, cx,cy);
      stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx,sy,scx,scy);
   }
   else
   {
      if (was_off)
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve,sx,sy,cx,cy);
      else
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vline,sx,sy,0,0);
   }
   return num_vertices;
}

STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
   int16_t numberOfContours;
   uint8_t *endPtsOfContours;
   uint8_t *data = info->data;
   stbtt_vertex *vertices=0;
   int num_vertices=0;
   int g = stbtt__GetGlyfOffset(info, glyph_index);

   *pvertices = NULL;

   if (g < 0) return 0;

   numberOfContours = ttSHORT(data + g);

   if (numberOfContours > 0) {
      uint8_t flags=0,flagcount;
      int32_t ins, i,j=0,m,n, next_move, was_off=0, off, start_off=0;
      int32_t x,y,cx,cy,sx,sy, scx,scy;
      uint8_t *points;
      endPtsOfContours = (data + g + 10);
      ins = ttUSHORT(data + g + 10 + numberOfContours * 2);
      points = data + g + 10 + numberOfContours * 2 + 2 + ins;

      n = 1+ttUSHORT(endPtsOfContours + numberOfContours*2-2);

      m = n + 2*numberOfContours;  /* a loose bound on how many vertices we might need */
      vertices = (stbtt_vertex *)malloc(m * sizeof(vertices[0]));
      if (vertices == 0)
         return 0;

      next_move = 0;
      flagcount=0;

      /* in first pass, we load uninterpreted data into the allocated array
       * above, shifted to the end of the array so we won't overwrite it when
       * we create our final data starting from the front
       */

      off = m - n; /* starting offset for uninterpreted data, regardless of how m ends up being calculated */

      /* first load flags */

      for (i=0; i < n; ++i)
      {
         if (flagcount == 0)
         {
            flags = *points++;
            if (flags & 8)
               flagcount = *points++;
         } else
            --flagcount;
         vertices[off+i].type = flags;
      }

      /* now load x coordinates */
      x=0;
      for (i=0; i < n; ++i)
      {
         flags = vertices[off+i].type;
         if (flags & 2)
         {
            int16_t dx = *points++;
            x += (flags & 16) ? dx : -dx; /* ??? */
         }
         else
         {
            if (!(flags & 16))
            {
               x = x + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].x = (int16_t) x;
      }

      /* now load y coordinates */
      y=0;
      for (i=0; i < n; ++i)
      {
         flags = vertices[off+i].type;
         if (flags & 4)
         {
            int16_t dy = *points++;
            y += (flags & 32) ? dy : -dy; /* ??? */
         }
         else
         {
            if (!(flags & 32)) {
               y = y + (int16_t) (points[0]*256 + points[1]);
               points += 2;
            }
         }
         vertices[off+i].y = (int16_t) y;
      }

      /* now convert them to our format */
      num_vertices=0;
      sx = sy = cx = cy = scx = scy = 0;
      for (i=0; i < n; ++i)
      {
         flags = vertices[off+i].type;
         x     = (int16_t) vertices[off+i].x;
         y     = (int16_t) vertices[off+i].y;

         if (next_move == i)
         {
            if (i != 0)
               num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);

            /* now start the new one */
            start_off = !(flags & 1);

            if (start_off)
            {
               /* if we start off with an off-curve point,
                * then when we need to find a point on the curve
                * where we can start, and we need to save some state for when we wraparound. */
               scx = x;
               scy = y;
               if (!(vertices[off+i+1].type & 1))
               {
                  /* next point is also a curve point, so interpolate an on-point curve */
                  sx = (x + (int32_t) vertices[off+i+1].x) >> 1;
                  sy = (y + (int32_t) vertices[off+i+1].y) >> 1;
               }
               else
               {
                  /* otherwise just use the next point as our start point */
                  sx = (int32_t) vertices[off+i+1].x;
                  sy = (int32_t) vertices[off+i+1].y;
                  ++i; /* we're using point i+1 as the starting point, so skip it */
               }
            } else {
               sx = x;
               sy = y;
            }
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove,sx,sy,0,0);
            was_off = 0;
            next_move = 1 + ttUSHORT(endPtsOfContours+j*2);
            ++j;
         }
         else
         {
            if (!(flags & 1))
            {
               /* if it's a curve */
               if (was_off) /* two off-curve control points in a row means interpolate an on-curve midpoint */
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx+x)>>1, (cy+y)>>1, cx, cy);
               cx = x;
               cy = y;
               was_off = 1;
            }
            else
            {
               if (was_off)
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x,y, cx, cy);
               else
                  stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x,y,0,0);
               was_off = 0;
            }
         }
      }
      num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx,sy,scx,scy,cx,cy);
   }
   else if (numberOfContours == -1)
   {
      /* Compound shapes. */
      int more = 1;
      uint8_t *comp = data + g + 10;
      num_vertices = 0;
      vertices = 0;
      while (more)
      {
         uint16_t flags, gidx;
         int comp_num_verts = 0, i;
         stbtt_vertex *comp_verts = 0, *tmp = 0;
         float mtx[6] = {1,0,0,1,0,0}, m, n;

         flags = ttSHORT(comp); comp+=2;
         gidx = ttSHORT(comp); comp+=2;

         if (flags & 2)
         {
            /* XY values */
            if (flags & 1)
            { /* shorts */
               mtx[4] = ttSHORT(comp); comp+=2;
               mtx[5] = ttSHORT(comp); comp+=2;
            }
            else
            {
               mtx[4] = ttCHAR(comp); comp+=1;
               mtx[5] = ttCHAR(comp); comp+=1;
            }
         }
         else
         {
            /* @TODO handle matching point */
         }
         if (flags & (1<<3))
         {
            /* WE_HAVE_A_SCALE */
            mtx[0] = mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
         }
         else if (flags & (1<<6))
         {
            /* WE_HAVE_AN_X_AND_YSCALE */
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = mtx[2] = 0;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         }
         else if (flags & (1<<7))
         {
            /* WE_HAVE_A_TWO_BY_TWO */
            mtx[0] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[1] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[2] = ttSHORT(comp)/16384.0f; comp+=2;
            mtx[3] = ttSHORT(comp)/16384.0f; comp+=2;
         }

         /* Find transformation scales. */
         m = (float)sqrt(mtx[0]*mtx[0] + mtx[1]*mtx[1]);
         n = (float)sqrt(mtx[2]*mtx[2] + mtx[3]*mtx[3]);

         /* Get indexed glyph. */
         comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
         if (comp_num_verts > 0)
         {
            /* Transform vertices. */
            for (i = 0; i < comp_num_verts; ++i)
            {
               stbtt_vertex* v = &comp_verts[i];
               int16_t x,y;
               x=v->x; y=v->y;
               v->x = (int16_t)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->y = (int16_t)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
               x=v->cx; y=v->cy;
               v->cx = (int16_t)(m * (mtx[0]*x + mtx[2]*y + mtx[4]));
               v->cy = (int16_t)(n * (mtx[1]*x + mtx[3]*y + mtx[5]));
            }

            /* Append vertices. */
            tmp = (stbtt_vertex*)malloc((num_vertices+comp_num_verts)*sizeof(stbtt_vertex));
            if (!tmp)
            {
               if (vertices)
                  free(vertices);
               if (comp_verts)
                  free(comp_verts);
               return 0;
            }
            if (num_vertices > 0)
               memcpy(tmp, vertices, num_vertices*sizeof(stbtt_vertex));
            memcpy(tmp+num_vertices, comp_verts, comp_num_verts*sizeof(stbtt_vertex));

            if (vertices)
               free(vertices);

            vertices = tmp;
            free(comp_verts);
            num_vertices += comp_num_verts;
         }
         /* More components ? */
         more = flags & (1<<5);
      }
   }

   *pvertices = vertices;
   return num_vertices;
}

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info,
      int glyph_index, int *advanceWidth, int *leftSideBearing)
{
   uint16_t numOfLongHorMetrics = ttUSHORT(info->data+info->hhea + 34);
   if (glyph_index < numOfLongHorMetrics)
   {
      if (advanceWidth)
         *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*glyph_index);
      if (leftSideBearing)
         *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*glyph_index + 2);
   }
   else
   {
      if (advanceWidth)
         *advanceWidth    = ttSHORT(info->data + info->hmtx + 4*(numOfLongHorMetrics-1));
      if (leftSideBearing)
         *leftSideBearing = ttSHORT(info->data + info->hmtx + 4*numOfLongHorMetrics + 2*(glyph_index - numOfLongHorMetrics));
   }
}

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info,
      int *ascent, int *descent, int *lineGap)
{
   if (ascent ) *ascent  = ttSHORT(info->data+info->hhea + 4);
   if (descent) *descent = ttSHORT(info->data+info->hhea + 6);
   if (lineGap) *lineGap = ttSHORT(info->data+info->hhea + 8);
}

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float height)
{
   int fheight = ttSHORT(info->data + info->hhea + 4) - ttSHORT(info->data + info->hhea + 6);
   return (float) height / fheight;
}

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels)
{
   int unitsPerEm = ttUSHORT(info->data + info->head + 18);
   return pixels / unitsPerEm;
}

STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *v)
{
   free(v);
}

/* antialiasing software rasterizer */

STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font,
      int glyph, float scale_x, float scale_y,float shift_x, float shift_y,
      int *ix0, int *iy0, int *ix1, int *iy1)
{
   int x0,y0,x1,y1;
   if (!stbtt_GetGlyphBox(font, glyph, &x0,&y0,&x1,&y1))
   {
      /* e.g. space character */
      if (ix0) *ix0 = 0;
      if (iy0) *iy0 = 0;
      if (ix1) *ix1 = 0;
      if (iy1) *iy1 = 0;
   }
   else
   {
      /* move to integral bboxes
       * (treating pixels as little squares, what pixels get touched)? */
      if (ix0) *ix0 = (int)floor( x0 * scale_x + shift_x);
      if (iy0) *iy0 = (int)floor(-y1 * scale_y + shift_y);
      if (ix1) *ix1 = (int)ceil( x1 * scale_x + shift_x);
      if (iy1) *iy1 = (int)ceil(-y0 * scale_y + shift_y);
   }
}

STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y,
      int *ix0, int *iy0, int *ix1, int *iy1)
{
   stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y,0.0f,0.0f, ix0, iy0, ix1, iy1);
}

STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y,
      float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
   stbtt_GetGlyphBitmapBoxSubpixel(font, stbtt_FindGlyphIndex(font,codepoint), scale_x, scale_y,shift_x,shift_y, ix0,iy0,ix1,iy1);
}

/*  Rasterizer */

typedef struct stbtt__hheap_chunk
{
   struct stbtt__hheap_chunk *next;
} stbtt__hheap_chunk;

typedef struct stbtt__hheap
{
   struct stbtt__hheap_chunk *head;
   void   *first_free;
   int    num_remaining_in_head_chunk;
} stbtt__hheap;

static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata)
{
   if (hh->first_free)
   {
      void *p = hh->first_free;
      hh->first_free = * (void **) p;
      return p;
   }
   if (hh->num_remaining_in_head_chunk == 0)
   {
      int count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
      stbtt__hheap_chunk *c = (stbtt__hheap_chunk *)
         malloc(sizeof(stbtt__hheap_chunk) + size * count);
      if (c == NULL)
         return NULL;
      c->next = hh->head;
      hh->head = c;
      hh->num_remaining_in_head_chunk = count;
   }
   --hh->num_remaining_in_head_chunk;
   return (char *) (hh->head) + size * hh->num_remaining_in_head_chunk;
}

static void stbtt__hheap_free(stbtt__hheap *hh, void *p)
{
   *(void **) p = hh->first_free;
   hh->first_free = p;
}

static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata)
{
   stbtt__hheap_chunk *c = hh->head;
   while (c) {
      stbtt__hheap_chunk *n = c->next;
      free(c);
      c = n;
   }
}

typedef struct stbtt__edge {
   float x0,y0, x1,y1;
   int invert;
} stbtt__edge;


typedef struct stbtt__active_edge
{
   struct stbtt__active_edge *next;
   float fx,fdx,fdy;
   float direction;
   float sy;
   float ey;
} stbtt__active_edge;

static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
   stbtt__active_edge *z = (stbtt__active_edge *) stbtt__hheap_alloc(hh, sizeof(*z), userdata);
   if (z)
   {
      float dxdy   = (e->x1 - e->x0) / (e->y1 - e->y0);
      z->fdx       = dxdy;
      z->fdy       = (1/dxdy);
      z->fx        = e->x0 + dxdy * (start_point - e->y0);
      z->fx       -= off_x;
      z->direction = e->invert ? 1.0f : -1.0f;
      z->sy        = e->y0;
      z->ey        = e->y1;
      z->next      = 0;
   }
   return z;
}

/* the edge passed in here does not cross the vertical line at x or the vertical line at x+1
 * (i.e. it has already been clipped to those)
 */
static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e,
      float x0, float y0, float x1, float y1)
{
   if (y0 == y1) return;
   if (y0 > e->ey) return;
   if (y1 < e->sy) return;
   if (y0 < e->sy) {
      x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
      y0 = e->sy;
   }
   if (y1 > e->ey) {
      x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
      y1 = e->ey;
   }

   if (x0 <= x && x1 <= x)
      scanline[x] += e->direction * (y1-y0);
   else if (x0 >= x+1 && x1 >= x+1)
      ;
   else
      scanline[x] += e->direction * (y1-y0) * (1-((x0-x)+(x1-x))/2); /* coverage = 1 - average x position */
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill,
      int len, stbtt__active_edge *e, float y_top)
{
   float y_bottom = y_top+1;

   while (e)
   {
      /* brute force every pixel */

      /* compute intersection points with top & bottom */

      if (e->fdx == 0)
      {
         float x0 = e->fx;
         if (x0 < len)
         {
            if (x0 >= 0)
            {
               stbtt__handle_clipped_edge(scanline,(int) x0,e, x0,y_top, x0,y_bottom);
               stbtt__handle_clipped_edge(scanline_fill-1,(int) x0+1,e, x0,y_top, x0,y_bottom);
            }
            else
               stbtt__handle_clipped_edge(scanline_fill-1,0,e, x0,y_top, x0,y_bottom);
         }
      }
      else
      {
         float x0 = e->fx;
         float dx = e->fdx;
         float xb = x0 + dx;
         float x_top, x_bottom;
         float y0,y1;
         float dy = e->fdy;

         /* compute endpoints of line segment clipped to this scanline (if the
          * line segment starts on this scanline. x0 is the intersection of the
          * line with y_top, but that may be off the line segment.
          */
         if (e->sy > y_top) {
            x_top = x0 + dx * (e->sy - y_top);
            y0 = e->sy;
         } else {
            x_top = x0;
            y0 = y_top;
         }
         if (e->ey < y_bottom) {
            x_bottom = x0 + dx * (e->ey - y_top);
            y1 = e->ey;
         } else {
            x_bottom = xb;
            y1 = y_bottom;
         }

         if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len)
         {
            /* from here on, we don't have to range check x values */

            if ((int) x_top == (int) x_bottom)
            {
               /* simple case, only spans one pixel */

               float height;
               int x = (int) x_top;
               height = y1 - y0;
               scanline[x] += e->direction * (1-((x_top - x) + (x_bottom-x))/2)  * height;
               scanline_fill[x] += e->direction * height; /* everything right of this pixel is filled */
            }
            else
            {
               int x,x1,x2;
               float y_crossing, step, sign, area;

               /* covers 2+ pixels */
               if (x_top > x_bottom)
               {
                  /* flip scanline vertically; signed area is the same */
                  float t;
                  y0 = y_bottom - (y0 - y_top);
                  y1 = y_bottom - (y1 - y_top);
                   t = y0;
                   y0 = y1;
                   y1 = t;
                   t = x_bottom;
                   x_bottom = x_top;
                   x_top = t;
                  dy = -dy;
                   x0 = xb;
               }

               x1 = (int) x_top;
               x2 = (int) x_bottom;
               /* compute intersection with y axis at x1+1 */
               y_crossing = (x1+1 - x0) * dy + y_top;

               sign = e->direction;
               /* area of the rectangle covered from y0..y_crossing */
               area = sign * (y_crossing-y0);
               /* area of the triangle (x_top,y0), (x+1,y0), (x+1,y_crossing) */
               scanline[x1] += area * (1-((x_top - x1)+(x1+1-x1))/2);

               step = sign * dy;
               for (x = x1+1; x < x2; ++x)
               {
                  scanline[x] += area + step/2;
                  area += step;
               }
               y_crossing += dy * (x2 - (x1+1));

               scanline[x2] += area + sign * (1-((x2-x2)+(x_bottom-x2))/2) * (y1-y_crossing);

               scanline_fill[x2] += sign * (y1-y0);
            }
         }
         else
         {
            /* if edge goes outside of box we're drawing, we require
             * clipping logic. since this does not match the intended use
             * of this library, we use a different, very slow brute
             * force implementation
             */
            int x;

            for (x=0; x < len; ++x)
            {
               /* cases:
                *
                * there can be up to two intersections with the pixel. any intersection
                * with left or right edges can be handled by splitting into two (or three)
                * regions. intersections with top & bottom do not necessitate case-wise logic.
                */
               float y0,y1;
               float y_cur = y_top, x_cur = x0;

               y0 = (x - x0) / dx + y_top;
               y1 = (x+1 - x0) / dx + y_top;

               if (y0 < y1) {
                  if (y0 > y_top && y0 < y_bottom) {
                     stbtt__handle_clipped_edge(scanline,x,e, x_cur,y_cur, (float) x,y0);
                     y_cur = y0;
                     x_cur = (float) x;
                  }
                  if (y1 >= y_cur && y1 < y_bottom) {
                     stbtt__handle_clipped_edge(scanline,x,e, x_cur,y_cur, (float) x+1,y1);
                     y_cur = y1;
                     x_cur = (float) x+1;
                  }
               } else {
                  if (y1 >= y_cur && y1 < y_bottom) {
                     stbtt__handle_clipped_edge(scanline,x,e, x_cur,y_cur, (float) x+1,y1);
                     y_cur = y1;
                     x_cur = (float) x+1;
                  }
                  if (y0 > y_top && y0 < y_bottom) {
                     stbtt__handle_clipped_edge(scanline,x,e, x_cur,y_cur, (float) x,y0);
                     y_cur = y0;
                     x_cur = (float) x;
                  }
               }
               stbtt__handle_clipped_edge(scanline,x,e, x_cur,y_cur, xb,y_bottom);
            }
         }
      }
      e = e->next;
   }
}

/* directly AA rasterize edges w/o supersampling */
static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e,
      int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stbtt__hheap hh = { 0 };
   stbtt__active_edge *active = NULL;
   int y,j=0, i;
   float scanline_data[129], *scanline, *scanline2;

   if (result->w > 64)
      scanline = (float *)malloc((result->w*2+1) * sizeof(float));
   else
      scanline = scanline_data;

   scanline2 = scanline + result->w;

   y = off_y;
   e[n].y0 = (float) (off_y + result->h) + 1;

   while (j < result->h)
   {
      float scan_y_top    = y + 0.0f;
      float scan_y_bottom = y + 1.0f;
      stbtt__active_edge **step = &active;

      /* find center of pixel for this scanline */

      memset(scanline , 0, result->w*sizeof(scanline[0]));
      memset(scanline2, 0, (result->w+1)*sizeof(scanline[0]));

      /* update all active edges,
      * remove all active edges that terminate
      * before the top of this scanline */
      while (*step)
      {
         stbtt__active_edge * z = *step;
         if (z->ey <= scan_y_top)
         {
            *step = z->next; /* delete from list */
            z->direction = 0;
            stbtt__hheap_free(&hh, z);
         }
         else
            step = &((*step)->next); /* advance through list */
      }

      /* insert all edges that start before the bottom of this scanline */
      while (e->y0 <= scan_y_bottom)
      {
         stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);

         /* insert at front */
         z->next = active;
         active = z;
         ++e;
      }

      /* now process all active edges */
      if (active)
         stbtt__fill_active_edges_new(scanline, scanline2+1, result->w, active, scan_y_top);

      {
         float sum = 0;
         for (i=0; i < result->w; ++i)
         {
            float k;
            int m;
            sum += scanline2[i];
            k = scanline[i] + sum;
            k = (float) fabs(k)*255 + 0.5f;
            m = (int) k;
            if (m > 255) m = 255;
            result->pixels[j*result->stride + i] = (unsigned char) m;
         }
      }

      /* advance all the edges */

      step = &active;

      while (*step)
      {
         stbtt__active_edge *z = *step;
         z->fx += z->fdx;         /* advance to position for current scanline */
         step = &((*step)->next); /* advance through list */
      }

      ++y;
      ++j;
   }

   stbtt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      free(scanline);
}

#define STBTT__COMPARE(a,b)  ((a)->y0 < (b)->y0)

static void stbtt__sort_edges_ins_sort(stbtt__edge *p, int n)
{
   int i,j;
   for (i=1; i < n; ++i) {
      stbtt__edge t = p[i], *a = &t;
      j = i;
      while (j > 0) {
         stbtt__edge *b = &p[j-1];
         int c = STBTT__COMPARE(a,b);
         if (!c) break;
         p[j] = p[j-1];
         --j;
      }
      if (i != j)
         p[j] = t;
   }
}

static void stbtt__sort_edges_quicksort(stbtt__edge *p, int n)
{
   /* threshhold for transitioning to insertion sort */
   while (n > 12) {
      stbtt__edge t;
      int c01,c12,c,m,i,j;

      /* compute median of three */
      m = n >> 1;
      c01 = STBTT__COMPARE(&p[0],&p[m]);
      c12 = STBTT__COMPARE(&p[m],&p[n-1]);
      /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
      if (c01 != c12) {
         /* otherwise, we'll need to swap something else to middle */
         int z;
         c = STBTT__COMPARE(&p[0],&p[n-1]);
         /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
         /* 0<mid && mid>n:  0>n => 0; 0<n => n */
         z = (c == c12) ? 0 : n-1;
         t = p[z];
         p[z] = p[m];
         p[m] = t;
      }
      /* now p[m] is the median-of-three */
      /* swap it to the beginning so it won't move around */
      t = p[0];
      p[0] = p[m];
      p[m] = t;

      /* partition loop */
      i=1;
      j=n-1;
      for(;;) {
         /* handling of equality is crucial here */
         /* for sentinels & efficiency with duplicates */
         for (;;++i) {
            if (!STBTT__COMPARE(&p[i], &p[0])) break;
         }
         for (;;--j) {
            if (!STBTT__COMPARE(&p[0], &p[j])) break;
         }
         /* make sure we haven't crossed */
         if (i >= j) break;
         t = p[i];
         p[i] = p[j];
         p[j] = t;

         ++i;
         --j;
      }
      /* recurse on smaller side, iterate on larger */
      if (j < (n-i)) {
         stbtt__sort_edges_quicksort(p,j);
         p = p+i;
         n = n-i;
      } else {
         stbtt__sort_edges_quicksort(p+i, n-i);
         n = j;
      }
   }
}

typedef struct
{
   float x,y;
} stbtt__point;

static void stbtt__rasterize(stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings,
      float scale_x, float scale_y, float shift_x, float shift_y,
      int off_x, int off_y, int invert, void *userdata)
{
   float y_scale_inv = invert ? -scale_y : scale_y;
   stbtt__edge *e;
   int n,i,j,k,m;
   int vsubsample = 1;
   /* vsubsample should divide 255 evenly; otherwise we won't reach full opacity */

   /* now we have to blow out the windings into explicit edge lists */
   n = 0;
   for (i=0; i < windings; ++i)
      n += wcount[i];

   e = (stbtt__edge *)malloc(sizeof(*e) * (n+1)); /* add an extra one as a sentinel */
   if (e == 0) return;
   n = 0;

   m=0;
   for (i=0; i < windings; ++i) {
      stbtt__point *p = pts + m;
      m += wcount[i];
      j = wcount[i]-1;
      for (k=0; k < wcount[i]; j=k++) {
         int a=k,b=j;
         /* skip the edge if horizontal */
         if (p[j].y == p[k].y)
            continue;
         /* add edge from j to k to the list */
         e[n].invert = 0;
         if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
            e[n].invert = 1;
             a=j;
             b=k;
         }
         e[n].x0 = p[a].x * scale_x + shift_x;
         e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
         e[n].x1 = p[b].x * scale_x + shift_x;
         e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
         ++n;
      }
   }

   /* now sort the edges by their highest point (should snap to integer, and then by x) */
   stbtt__sort_edges_quicksort(e, n);
   stbtt__sort_edges_ins_sort(e, n);

   /* now, traverse the scanlines and find the
    * intersections on each scanline, use XOR winding rule */
   stbtt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

   free(e);
}

static void stbtt__add_point(stbtt__point *points, int n, float x, float y)
{
   if (!points) return; /* during first pass, it's unallocated */
   points[n].x = x;
   points[n].y = y;
}

/* tesselate until threshhold p is happy... @TODO warped to compensate for non-linear stretching */
static int stbtt__tesselate_curve(stbtt__point *points, int *num_points,
      float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
   /* midpoint */
   float mx = (x0 + 2*x1 + x2)/4;
   float my = (y0 + 2*y1 + y2)/4;
   /* versus directly drawn line */
   float dx = (x0+x2)/2 - mx;
   float dy = (y0+y2)/2 - my;
   if (n > 16) /* 65536 segments on one curve better be enough! */
      return 1;

   if (dx*dx+dy*dy > objspace_flatness_squared)
   {
      /* half-pixel error allowed... need to be smaller if AA */
      stbtt__tesselate_curve(points, num_points, x0,y0, (x0+x1)/2.0f,(y0+y1)/2.0f, mx,my, objspace_flatness_squared,n+1);
      stbtt__tesselate_curve(points, num_points, mx,my, (x1+x2)/2.0f,(y1+y2)/2.0f, x2,y2, objspace_flatness_squared,n+1);
   }
   else
   {
      stbtt__add_point(points, *num_points,x2,y2);
      *num_points = *num_points+1;
   }
   return 1;
}

/* Returns number of contours */
static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts,
      float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
   stbtt__point *points=0;
   int num_points=0;

   float objspace_flatness_squared = objspace_flatness * objspace_flatness;
   int i,n=0,start=0, pass;

   /* count how many "moves" there are to get the contour count */
   for (i=0; i < num_verts; ++i)
      if (vertices[i].type == STBTT_vmove)
         ++n;

   *num_contours = n;
   if (n == 0) return 0;

   *contour_lengths = (int *)malloc(sizeof(**contour_lengths) * n);

   if (*contour_lengths == 0) {
      *num_contours = 0;
      return 0;
   }

   /* make two passes through the points so we don't need to realloc */
   for (pass=0; pass < 2; ++pass)
   {
      float x=0,y=0;

      if (pass == 1)
      {
         points = (stbtt__point *)malloc(num_points * sizeof(points[0]));
         if (points == NULL) goto error;
      }
      num_points = 0;
      n= -1;

      for (i=0; i < num_verts; ++i)
      {
         switch (vertices[i].type)
         {
            case STBTT_vmove:
               /* start the next contour */
               if (n >= 0)
                  (*contour_lengths)[n] = num_points - start;
               ++n;
               start = num_points;

                 x = vertices[i].x;
                 y = vertices[i].y;
               stbtt__add_point(points, num_points++, x,y);
               break;
            case STBTT_vline:
                 x = vertices[i].x;
                 y = vertices[i].y;
               stbtt__add_point(points, num_points++, x, y);
               break;
            case STBTT_vcurve:
               stbtt__tesselate_curve(points, &num_points, x,y,
                                        vertices[i].cx, vertices[i].cy,
                                        vertices[i].x,  vertices[i].y,
                                        objspace_flatness_squared, 0);
                 x = vertices[i].x;
                 y = vertices[i].y;
               break;
         }
      }
      (*contour_lengths)[n] = num_points - start;
   }

   return points;
error:
   free(points);
   free(*contour_lengths);
   *contour_lengths = 0;
   *num_contours = 0;
   return NULL;
}

STBTT_DEF void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels,
      stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y,
      float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata)
{
   float scale            = scale_x > scale_y ? scale_y : scale_x;
   int winding_count      = 0;
   int *winding_lengths   = NULL;
   stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
   if (windings)
   {
      stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
      free(winding_lengths);
      free(windings);
   }
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y,
      float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   int ix0,iy0,ix1,iy1;
   stbtt__bitmap gbm;
   stbtt_vertex *vertices;
   int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

   if (scale_x == 0) scale_x = scale_y;
   if (scale_y == 0)
   {
      if (scale_x == 0)
         return NULL;
      scale_y = scale_x;
   }

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,&ix1,&iy1);

   /* now we get the size */
   gbm.w      = (ix1 - ix0);
   gbm.h      = (iy1 - iy0);
   gbm.pixels = NULL; /* in case we error */

   if (width ) *width  = gbm.w;
   if (height) *height = gbm.h;
   if (xoff  ) *xoff   = ix0;
   if (yoff  ) *yoff   = iy0;

   if (gbm.w && gbm.h)
   {
      gbm.pixels = (unsigned char *)malloc(gbm.w * gbm.h);
      if (gbm.pixels)
      {
         gbm.stride = gbm.w;

         stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
      }
   }

   free(vertices);
   return gbm.pixels;
}

STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y,
      int glyph, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride,
      float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
{
   int ix0,iy0;
   stbtt_vertex *vertices;
   int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);
   stbtt__bitmap gbm;

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0,&iy0,0,0);
   gbm.pixels = output;
   gbm.w = out_w;
   gbm.h = out_h;
   gbm.stride = out_stride;

   if (gbm.w && gbm.h)
      stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0,iy0, 1, info->userdata);

   free(vertices);
}

STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
   stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f,0.0f, glyph);
}

/* bitmap baking
 *
 * This is SUPER-CRAPPY packing to keep source code small
 */

/* rectangle packing replacement routines if you don't have stb_rect_pack.h */

#ifndef STB_RECT_PACK_VERSION

/*
 * COMPILER WARNING ?!?!?
 *
 * if you get a compile warning due to these symbols being defined more than
 * once, move #include "stb_rect_pack.h" before #include "stb_truetype.h"
 */

typedef struct
{
   int width,height;
   int x,y,bottom_y;
} stbrp_context;

typedef struct
{
   unsigned char x;
} stbrp_node;

typedef struct
{
   int x,y;
   int id,w,h,was_packed;
} stbrp_rect;

static void stbrp_init_target(stbrp_context *con, int pw, int ph,
      stbrp_node *nodes, int num_nodes)
{
   con->width    = pw;
   con->height   = ph;
   con->x        = 0;
   con->y        = 0;
   con->bottom_y = 0;
}

static void stbrp_pack_rects(stbrp_context *con,
      stbrp_rect *rects, int num_rects)
{
   int i;
   for (i=0; i < num_rects; ++i)
   {
      if (con->x + rects[i].w > con->width)
      {
         con->x = 0;
         con->y = con->bottom_y;
      }

      if (con->y + rects[i].h > con->height)
         break;

      rects[i].x = con->x;
      rects[i].y = con->y;
      rects[i].was_packed = 1;
      con->x += rects[i].w;

      if (con->y + rects[i].h > con->bottom_y)
         con->bottom_y = con->y + rects[i].h;
   }
   for (   ; i < num_rects; ++i)
      rects[i].was_packed = 0;
}
#endif

/* bitmap baking
 *
 * This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
 * stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.
 */

STBTT_DEF int stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels,
      int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
{
   stbrp_context *context = (stbrp_context *)malloc(sizeof(*context));
   int            num_nodes = pw - padding;
   stbrp_node    *nodes   = (stbrp_node    *)malloc(sizeof(*nodes) * num_nodes);

   if (context == NULL || nodes == NULL)
   {
      if (context != NULL)
         free(context);
      if (nodes   != NULL)
         free(nodes);
      return 0;
   }

   spc->user_allocator_context = alloc_context;
   spc->width = pw;
   spc->height = ph;
   spc->pixels = pixels;
   spc->pack_info = context;
   spc->nodes = nodes;
   spc->padding = padding;
   spc->stride_in_bytes = stride_in_bytes != 0 ? stride_in_bytes : pw;
   spc->h_oversample = 1;
   spc->v_oversample = 1;

   stbrp_init_target(context, pw-padding, ph-padding, nodes, num_nodes);

   memset(pixels, 0, pw*ph); /* background of 0 around pixels */

   return 1;
}

STBTT_DEF void stbtt_PackEnd  (stbtt_pack_context *spc)
{
   free(spc->nodes);
   free(spc->pack_info);
}

#define STBTT__OVER_MASK  (STBTT_MAX_OVERSAMPLE-1)

static void stbtt__h_prefilter(unsigned char *pixels, int w, int h,
      int stride_in_bytes, unsigned int kernel_width)
{
   int j;
   unsigned char buffer[STBTT_MAX_OVERSAMPLE];
   int safe_w = w - kernel_width;

   for (j=0; j < h; ++j)
   {
      int i;
      unsigned int total;
      memset(buffer, 0, kernel_width);

      total = 0;

      /* make kernel_width a constant in common cases
       * so compiler can optimize out the divide */
      switch (kernel_width) {
         case 2:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / 4);
            }
            break;
         default:
            for (i=0; i <= safe_w; ++i) {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < w; ++i)
      {
         total -= buffer[i & STBTT__OVER_MASK];
         pixels[i] = (unsigned char) (total / kernel_width);
      }

      pixels += stride_in_bytes;
   }
}

static void stbtt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
{
   int j;
   unsigned char buffer[STBTT_MAX_OVERSAMPLE];
   int safe_h = h - kernel_width;

   for (j=0; j < w; ++j)
   {
      int i;
      unsigned int total = 0;

      memset(buffer, 0, kernel_width);

      /* make kernel_width a constant in common cases so compiler can optimize out the divide */
      switch (kernel_width)
      {
         case 2:
            for (i=0; i <= safe_h; ++i)
            {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 2);
            }
            break;
         case 3:
            for (i=0; i <= safe_h; ++i)
            {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 3);
            }
            break;
         case 4:
            for (i=0; i <= safe_h; ++i)
            {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / 4);
            }
            break;
         default:
            for (i=0; i <= safe_h; ++i)
            {
               total += pixels[i*stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i+kernel_width) & STBTT__OVER_MASK] = pixels[i*stride_in_bytes];
               pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
            }
            break;
      }

      for (; i < h; ++i)
      {
         total -= buffer[i & STBTT__OVER_MASK];
         pixels[i*stride_in_bytes] = (unsigned char) (total / kernel_width);
      }

      pixels += 1;
   }
}

static float stbtt__oversample_shift(int oversample)
{
   if (!oversample)
      return 0.0f;

   /* The prefilter is a box filter of width "oversample",
    * which shifts phase by (oversample - 1)/2 pixels in
    * oversampled space. We want to shift in the opposite
    * direction to counter this. */
   return (float)-(oversample - 1) / (2.0f * (float)oversample);
}

STBTT_DEF int stbtt_PackFontRanges(stbtt_pack_context *spc, unsigned char *fontdata,
      int font_index, stbtt_pack_range *ranges, int num_ranges)
{
   stbrp_rect    *rects;
   stbtt_fontinfo info;
   float recip_h = 1.0f / spc->h_oversample;
   float recip_v = 1.0f / spc->v_oversample;
   float sub_x = stbtt__oversample_shift(spc->h_oversample);
   float sub_y = stbtt__oversample_shift(spc->v_oversample);
   int i,j,k,n, return_value = 1;
   stbrp_context *context = (stbrp_context *) spc->pack_info;

   /* flag all characters as NOT packed */
   for (i=0; i < num_ranges; ++i)
      for (j=0; j < ranges[i].num_chars_in_range; ++j)
         ranges[i].chardata_for_range[j].x0 =
         ranges[i].chardata_for_range[j].y0 =
         ranges[i].chardata_for_range[j].x1 =
         ranges[i].chardata_for_range[j].y1 = 0;

   n = 0;
   for (i=0; i < num_ranges; ++i)
      n += ranges[i].num_chars_in_range;

   rects = (stbrp_rect *)malloc(sizeof(*rects) * n);
   if (rects == NULL)
      return 0;

   stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata,font_index));
   k=0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stbtt_ScaleForPixelHeight(&info, fh) : stbtt_ScaleForMappingEmToPixels(&info, -fh);
      for (j=0; j < ranges[i].num_chars_in_range; ++j) {
         int x0,y0,x1,y1;
         stbtt_GetCodepointBitmapBoxSubpixel(&info, ranges[i].first_unicode_char_in_range + j,
                                              scale * spc->h_oversample,
                                              scale * spc->v_oversample,
                                              0,0,
                                              &x0,&y0,&x1,&y1);
         rects[k].w = (int) (x1-x0 + spc->padding + spc->h_oversample-1);
         rects[k].h = (int) (y1-y0 + spc->padding + spc->v_oversample-1);
         ++k;
      }
   }

   stbrp_pack_rects(context, rects, k);

   k = 0;
   for (i=0; i < num_ranges; ++i) {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stbtt_ScaleForPixelHeight(&info, fh) : stbtt_ScaleForMappingEmToPixels(&info, -fh);
      for (j=0; j < ranges[i].num_chars_in_range; ++j) {
         stbrp_rect *r = &rects[k];
         if (r->was_packed)
         {
            stbtt_packedchar *bc = &ranges[i].chardata_for_range[j];
            int advance, lsb, x0,y0,x1,y1;
            int glyph = stbtt_FindGlyphIndex(&info, ranges[i].first_unicode_char_in_range + j);
            int pad   = (int)spc->padding;

            /* pad on left and top */
            r->x += pad;
            r->y += pad;
            r->w -= pad;
            r->h -= pad;
            stbtt_GetGlyphHMetrics(&info, glyph, &advance, &lsb);
            stbtt_GetGlyphBitmapBox(&info, glyph,
                                    scale * spc->h_oversample,
                                    scale * spc->v_oversample,
                                    &x0,&y0,&x1,&y1);
            stbtt_MakeGlyphBitmapSubpixel(&info,
                                          spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                          r->w - spc->h_oversample+1,
                                          r->h - spc->v_oversample+1,
                                          spc->stride_in_bytes,
                                          scale * spc->h_oversample,
                                          scale * spc->v_oversample,
                                          0,0,
                                          glyph);

            if (spc->h_oversample > 1)
               stbtt__h_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->h_oversample);

            if (spc->v_oversample > 1)
               stbtt__v_prefilter(spc->pixels + r->x + r->y*spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->v_oversample);

            bc->x0       = (int16_t)  r->x;
            bc->y0       = (int16_t)  r->y;
            bc->x1       = (int16_t) (r->x + r->w);
            bc->y1       = (int16_t) (r->y + r->h);
            bc->xadvance =                scale * advance;
            bc->xoff     =       (float)  x0 * recip_h + sub_x;
            bc->yoff     =       (float)  y0 * recip_v + sub_y;
            bc->xoff2    =                (x0 + r->w) * recip_h + sub_x;
            bc->yoff2    =                (y0 + r->h) * recip_v + sub_y;
         }
         else
            return_value = 0; /* if any fail, report failure */

         ++k;
      }
   }

   free(rects);
   return return_value;
}

STBTT_DEF int stbtt_PackFontRange(stbtt_pack_context *spc, unsigned char *fontdata, int font_index, float font_size,
            int first_unicode_char_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range)
{
   stbtt_pack_range range;
   range.first_unicode_char_in_range = first_unicode_char_in_range;
   range.num_chars_in_range          = num_chars_in_range;
   range.chardata_for_range          = chardata_for_range;
   range.font_size                   = font_size;
   return stbtt_PackFontRanges(spc, fontdata, font_index, &range, 1);
}
#endif
