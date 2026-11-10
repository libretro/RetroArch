/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtt.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_GFX_FONT_RTT_H__
#define __LIBRETRO_SDK_GFX_FONT_RTT_H__

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

/* rtt - a clean-room TrueType glyph loader and rasterizer.
 *
 * Implements, from the OpenType/TrueType specification:
 *   - sfnt / TTC directory parsing with full bounds checking
 *   - cmap formats 0, 4, 6, 12 (codepoint -> glyph index)
 *   - hmtx / hhea / head / maxp metrics
 *   - glyf outlines (simple + composite) and CFF outlines (Type 2
 *     charstrings, CID-keyed fonts included), flattened and filled by an
 *     exact-coverage anti-aliased rasterizer (signed-area delta
 *     accumulation + per-row prefix sum, SSE2/NEON resolve kernels)
 *   - vhea/vmtx vertical metrics and 'kern' format 0 pair kerning
 *
 * Coverage is accumulated in float and quantized only in the final
 * per-row resolve step, so a future higher-bit-depth output path
 * (e.g. 10-bit HDR) only needs an alternate resolve routine.
 *
 * There is no hinting: outlines are drawn as designed.
 *
 * The font is read in place from the caller's bytes, which must stay
 * valid for as long as the rtt_font_t is used: nothing is copied and
 * no file is opened. Rendering allocates only to grow the caller's
 * rtt_scratch_t, which is kept from glyph to glyph.
 */

RETRO_BEGIN_DECLS

/* Up to this many CID font dicts have their local subroutines found at
 * init; a font with more finds the rest per glyph. */
#define RTT_CFF_MAX_FDS 32

/* A CFF INDEX: count items, offsets of off_size bytes at offsets (the
 * first is 1), item data from data - 1 + offset. */
typedef struct rtt_cff_index
{
   uint32_t count;
   uint32_t offsets;
   uint32_t data;
   uint32_t off_size;
} rtt_cff_index_t;

typedef struct rtt_font
{
   const uint8_t *data;
   size_t size;

   uint32_t glyf;
   uint32_t loca;
   uint32_t head;
   uint32_t hhea;
   uint32_t hmtx;
   uint32_t cmap_sub;

   uint32_t glyf_len;
   uint32_t loca_len;
   uint32_t hmtx_len;
   uint32_t cmap_sub_len;

   int num_glyphs;
   int num_hmetrics;
   int units_per_em;
   int loca_long;
   int cmap_format;
   /* maxp.maxPoints: the largest simple-glyph point count in the font,
    * which sizes the outline scratch carved into each render's block
    * (0 when maxp is the CFF short form, leaving per-glyph fallback). */
   int max_points;

   uint32_t fontstart;
   /* How far a flattened curve may stray from the true one, in pixels;
    * 0 for 0.35 */
   float flatness;

   /* Vertical metrics and kerning; 0 when the font has none */
   uint32_t vhea;
   uint32_t vmtx;
   uint32_t vmtx_len;
   uint32_t kern;
   uint32_t kern_len;
   int num_vmetrics;

   /* CFF outlines, for a font with no glyf table */
   int is_cff;
   int cff_num_fds;
   uint32_t cff;
   uint32_t cff_len;
   uint32_t cff_fdselect;     /* 0 for a font that is not CID-keyed */
   rtt_cff_index_t cff_charstrings;
   rtt_cff_index_t cff_gsubrs;
   rtt_cff_index_t cff_subrs; /* the Private DICT's, when not CID-keyed */
   rtt_cff_index_t cff_fdarray;
   rtt_cff_index_t cff_fd_subrs[RTT_CFF_MAX_FDS];
   uint32_t cff_fdarray_off;
} rtt_font_t;

/* Receives an outline in font units, y up. Every contour begins with
 * move_to and is closed back to that point. */
typedef struct rtt_outline_sink
{
   void *user;
   void (*move_to)(void *user, float x, float y);
   void (*line_to)(void *user, float x, float y);
   void (*quad_to)(void *user, float cx, float cy, float x, float y);
   void (*cubic_to)(void *user, float c1x, float c1y, float c2x, float c2y,
         float x, float y);
} rtt_outline_sink;

/* A face's rasterizer memory, kept from glyph to glyph */
typedef struct rtt_scratch
{
   float   *mem;
   size_t   size;
   /* The cell the zeroed layout is for, packed; 0 for none */
   unsigned dims;
} rtt_scratch_t;

/* Byte offset of face @index in @data (a TrueType file or a TTC
 * collection), or -1. */
int rtt_font_offset_for_index(const uint8_t *data, size_t size,
      int index);
/* Parses the face at @fontstart. Returns 0 when it is not a usable
 * TrueType face. */
int rtt_init(rtt_font_t *f, const uint8_t *data, size_t size,
      int fontstart);
/* Glyph index for a Unicode code point; 0 when there is none. */
int rtt_find_glyph(const rtt_font_t *f, uint32_t cp);
void rtt_glyph_hmetrics(const rtt_font_t *f, int gi,
      int *advance, int *lsb);
int rtt_glyph_box(const rtt_font_t *f, int gi,
      int *x0, int *y0, int *x1, int *y1);
void rtt_vmetrics(const rtt_font_t *f, int *ascent, int *descent,
      int *line_gap);
float rtt_scale_for_em_to_pixels(const rtt_font_t *f, float px);
/* Renders glyph @gi, whose box has xmin @bx0 and ymax @by1, into @dst
 * (@out_w x @out_h, @stride elements a row) at scale @sx, @sy: the
 * bitmap's origin is (floor(xmin * sx), floor(-ymax * sy)). The whole
 * region is written, as 8-bit coverage, or native 16-bit when @fmt16. */
void rtt_render_glyph(const rtt_font_t *f, rtt_scratch_t *sc,
      void *dst, int out_w, int out_h, int stride, float sx, float sy,
      int gi, int bx0, int by1, int fmt16);
/* As rtt_render_glyph with the device origin given directly: a font
 * unit point (x, y) lands at (x * sx - ox, -y * sy - oy) in the bitmap,
 * so a fractional origin places the glyph at a sub-pixel offset. */
void rtt_render_glyph_at(const rtt_font_t *f, rtt_scratch_t *sc,
      void *dst, int out_w, int out_h, int stride, float sx, float sy,
      float ox, float oy, int gi, int fmt16);

/* Sets how far rendering may flatten a curve from its true shape, in
 * pixels: the default, 0.35, suits small text; finer costs more line
 * segments per curve. 0 restores the default. */
void rtt_set_flatness(rtt_font_t *f, float px);

/* Walks glyph @gi's outline into @sink, in font units. @sc lends point
 * storage (TrueType); it may be NULL. Returns 0 for no outline. */
int rtt_glyph_outline(const rtt_font_t *f, rtt_scratch_t *sc, int gi,
      const rtt_outline_sink *sink);

/* A component of a TrueType composite glyph, as stored: @arg1/@arg2 are
 * an x/y offset in font units when flags has ARGS_ARE_XY_VALUES (0x0002),
 * else the point numbers to match; the matrix is in 2.14. */
typedef struct rtt_component
{
   int      glyph;
   unsigned flags;
   int      arg1, arg2;
   int      xx, xy, yx, yy;
} rtt_component_t;

/* For a caller that places composite glyphs itself, as a hinting or
 * fixed-point engine must: 0 for a simple glyph, -1 for none or a CFF
 * font, else how many components glyph @gi has, with up to @max of them
 * written to @out. */
int rtt_glyph_components(const rtt_font_t *f, int gi,
      rtt_component_t *out, int max);

/* A simple TrueType glyph's points as stored, in font units: @on is 1
 * for an on-curve point, @ends the last point of each contour. With any
 * array NULL it returns the point count (and *@num_contours) alone.
 * Returns the points written, 0 for an empty glyph, -1 for a composite,
 * a CFF font, bad data or arrays too small. */
int rtt_glyph_points(const rtt_font_t *f, int gi, int32_t *xs, int32_t *ys,
      uint8_t *on, uint16_t *ends, int max_points, int max_contours,
      int *num_contours);

/* The control box of glyph @gi's outline, in font units: the box of
 * all its points, curve controls and lone points included. Returns 0
 * for an empty glyph. */
int rtt_glyph_cbox(const rtt_font_t *f, rtt_scratch_t *sc, int gi,
      float *x0, float *y0, float *x1, float *y1);

/* Vertical advance and top side bearing from vmtx; returns 0 (and
 * sets neither) when the font has no vertical metrics. */
int rtt_glyph_vmetrics(const rtt_font_t *f, int gi, int *advance, int *tsb);

/* vhea's ascent, descent and line gap; returns 0 when there is none. */
int rtt_vhea_metrics(const rtt_font_t *f, int *ascent, int *descent,
      int *line_gap);

/* Horizontal kerning between two glyphs from 'kern' format 0 tables,
 * in font units; 0 when there is none. */
int rtt_kerning(const rtt_font_t *f, int left, int right);

/* A table of the face, read in place; NULL when absent. */
const uint8_t *rtt_table(const rtt_font_t *f, const char *tag,
      uint32_t *len);

/* Frees a scratch's memory; it may be used again after. */
void rtt_scratch_free(rtt_scratch_t *sc);

RETRO_END_DECLS

#endif
