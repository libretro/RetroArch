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
*/

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <retro_common_api.h>
#include <retro_inline.h>

/* INTERFACE */

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#define __STB_INCLUDE_STB_TRUETYPE_H__

RETRO_BEGIN_DECLS

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

int  stbtt_PackBegin(stbtt_pack_context *spc,
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

void stbtt_PackEnd  (stbtt_pack_context *spc);

/* Cleans up the packing context and frees all memory. */

#define STBTT_POINT_SIZE(x)   (-(x))

int  stbtt_PackFontRange(stbtt_pack_context *spc, unsigned char *fontdata, int font_index, float font_size,
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

int  stbtt_PackFontRanges(stbtt_pack_context *spc, unsigned char *fontdata,
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

int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);

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

int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);

/* Given an offset into the file that defines a font, this function builds
 * the necessary cached info for the rest of the system. You must allocate
 * the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
 * need to do anything special to free it, because the contents are pure
 * value data with no additional data structures. Returns 0 on failure.
 */

/* CHARACTER TO GLYPH-INDEX CONVERSION */

int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint);

/* If you're going to perform multiple operations on the same character
 * and you want a speed-up, call this function with the character you're
 * going to process, then use glyph-based functions instead of the
 * codepoint-based functions.
 */

/* CHARACTER PROPERTIES */

float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float pixels);

/* computes a scale factor to produce a font whose "height" is 'pixels' tall.
 * Height is measured as the distance from the highest ascender to the lowest
 * descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
 * and computing:
 *       scale = pixels / (ascent - descent)
 * so if you prefer to measure height by the ascent only, use a similar calculation.
 */

float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

/* computes a scale factor to produce a font whose EM size is mapped to
 * 'pixels' tall. This is probably what traditional APIs compute, but
 * I'm not positive.
 */

void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap);

/* ascent is the coordinate above the baseline the font extends; descent
 * is the coordinate below the baseline the font extends (i.e. it is typically negative)
 * lineGap is the spacing between one row's descent and the next row's ascent...
 * so you should advance the vertical position by "*ascent - *descent + *lineGap"
 *   these are expressed in unscaled coordinates, so you must multiply by
 *   the scale factor for a given size
 */

/* Gets the bounding box of the visible part of the glyph, in unscaled coordinates */

void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info,
      int glyph_index, int *advanceWidth, int *leftSideBearing);

int  stbtt_GetGlyphBox(const stbtt_fontinfo *info,
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

int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);

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

void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *vertices);

/* frees the data allocated above */

/* BITMAP RENDERING */

void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint,
      float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);

/* same as stbtt_GetCodepointBitmapBox, but you can specify a subpixel
 * shift for the character */

/* the following functions are equivalent to the above functions, but operate
 * on glyph indices instead of Unicode codepoints (for efficiency) */

unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info,
      float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff);

unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info,
      float scale_x, float scale_y, float shift_x, float shift_y, int glyph,
      int *width, int *height, int *xoff, int *yoff);

void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);

void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output,
      int out_w, int out_h, int out_stride, float scale_x, float scale_y,
      float shift_x, float shift_y, int glyph);

void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x,
      float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);

void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph,
      float scale_x, float scale_y,float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);


/* @TODO: don't expose this structure */

typedef struct
{
   int w,h,stride;
   unsigned char *pixels;
} stbtt__bitmap;

void stbtt_Rasterize(stbtt__bitmap *result, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts,
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

RETRO_END_DECLS

#endif /* __STB_INCLUDE_STB_TRUETYPE_H__ */
