/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rrar_ppmd7.h).
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

/* The PPMd variant H model RAR 2.9 packs with, and RAR's range decoder.
 *
 * PPMd var.H is Dmitry Shkarin's prediction by partial matching (public
 * domain, 2001); RAR pairs it with a carry-less range coder of its own.
 * Decoding only. What comes out has to be what RAR put in, so the model
 * makes every decision theirs does: the 12-byte units, the free lists
 * and when blocks are glued, when the model restarts for want of
 * memory, and the arithmetic of every frequency.
 *
 * Nothing here trusts the data: the model only ever grows from the
 * symbols it decodes, and the range decoder reads its input from a span
 * with made-up zeros past the end, which it counts.
 */

#ifndef __LIBRETRO_SDK_RRAR_PPMD7_H
#define __LIBRETRO_SDK_RRAR_PPMD7_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RRAR_PPMD7_NUM_INDEXES 38

struct rrar_ppmd7_state;
struct rrar_ppmd7_context;

/* Secondary escape estimation: one of the 25x16 adaptive escape counts. */
typedef struct rrar_ppmd7_see
{
   uint16_t summ;
   uint8_t  shift;   /* how fast summ moves; low is fast */
   uint8_t  count;   /* until shift next changes */
} rrar_ppmd7_see_t;

typedef struct rrar_ppmd7
{
   struct rrar_ppmd7_context *min_ctx;
   struct rrar_ppmd7_context *max_ctx;
   struct rrar_ppmd7_state   *found;
   /* The model's memory: text grows up from the bottom, units are
    * carved from lo_unit up and hi_unit down. Links inside it are 32-bit
    * offsets from base; 0 is none. */
   uint8_t  *base;
   uint8_t  *lo_unit;
   uint8_t  *hi_unit;
   uint8_t  *text;
   uint8_t  *units_start;
   int32_t   run_length;
   int32_t   init_rl;
   uint32_t  size;
   uint32_t  align_offset;
   uint32_t  glue_count;
   uint32_t  free_list[RRAR_PPMD7_NUM_INDEXES];
   unsigned  order_fall;
   unsigned  init_esc;
   unsigned  prev_success;
   unsigned  max_order;
   unsigned  hi_bits_flag;
   uint16_t  bin_summ[128][64];
   rrar_ppmd7_see_t see[25][16];
   rrar_ppmd7_see_t dummy_see;
   uint8_t   indx2units[RRAR_PPMD7_NUM_INDEXES];
   uint8_t   units2indx[128];
   uint8_t   ns2indx[256];
   uint8_t   ns2bsindx[256];
   uint8_t   hb2flag[256];
} rrar_ppmd7_t;

/* RAR's range decoder, over a span of the packed data. */
typedef struct rrar_ppmd7_range
{
   const uint8_t *p;
   const uint8_t *end;
   uint32_t       range;
   uint32_t       code;
   uint32_t       low;
   uint32_t       past;    /* bytes read past the end, as zeros */
} rrar_ppmd7_range_t;

/* Once, before anything else. */
void rrar_ppmd7_construct(rrar_ppmd7_t *p);

/* The model's memory, @size bytes; kept when the size is the same. */
bool rrar_ppmd7_alloc(rrar_ppmd7_t *p, uint32_t size);

void rrar_ppmd7_free(rrar_ppmd7_t *p);

/* A new model of order @max_order. */
void rrar_ppmd7_init(rrar_ppmd7_t *p, unsigned max_order);

/* The decoder at @data, its first four bytes read; @past bytes are
 * already counted as read past @end. false if the bytes cannot start a
 * block. */
bool rrar_ppmd7_range_init(rrar_ppmd7_range_t *rc, const uint8_t *data,
      const uint8_t *end, uint32_t past);

/* The next byte, or a negative number if the data is not PPMd's. */
int rrar_ppmd7_decode_symbol(rrar_ppmd7_t *p, rrar_ppmd7_range_t *rc);

RETRO_END_DECLS

#endif
