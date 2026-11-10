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
 *   - glyf outlines (simple + composite), quadratic flattening, and an
 *     exact-coverage anti-aliased rasterizer (signed-area delta
 *     accumulation + per-row prefix sum, SSE2/NEON resolve kernels)
 *
 * Coverage is accumulated in float and quantized only in the final
 * per-row resolve step, so a future higher-bit-depth output path
 * (e.g. 10-bit HDR) only needs an alternate resolve routine.
 *
 * CFF ('OTTO') fonts are rejected at init (no glyf/loca), matching the
 * capability of the stb_truetype version this replaced.
 *
 * The font is read in place from the caller's bytes, which must stay
 * valid for as long as the rtt_font_t is used: nothing is copied and
 * no file is opened. Rendering allocates only to grow the caller's
 * rtt_scratch_t, which is kept from glyph to glyph.
 */

RETRO_BEGIN_DECLS

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
} rtt_font_t;

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
/* Frees a scratch's memory; it may be used again after. */
void rtt_scratch_free(rtt_scratch_t *sc);

RETRO_END_DECLS

#endif
