/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_rzstd.c).
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

/* Zstandard, written against RFC 8878.
 *
 * Every constant here traces to a clause of that document rather than
 * to a reading of the reference implementation. Where the RFC gives a
 * table, the table is reproduced; where it gives a rule, the rule is
 * spelled out in a comment beside the code that applies it.
 *
 * ---------------------------------------------------------------------
 * STATE OF THIS FILE
 *
 * Incomplete. It parses a frame and walks its blocks; it does not yet
 * decode a compressed one, which is the only kind that occurs in
 * practice. Nothing should depend on it until that changes.
 *
 * Built and exercised:
 *
 *   Frame header (3.1.1.1)      magic, descriptor byte, window
 *                               descriptor and its base-plus-eighths
 *                               encoding, dictionary ID field, frame
 *                               content size in all four widths
 *                               including the two-byte form's bias of
 *                               256, and the single-segment case where
 *                               the content size doubles as the window.
 *   Block header (3.1.1.2)      type, size, last-block flag.
 *   Raw blocks                  copied out.
 *   RLE blocks                  one byte expanded to the stated size.
 *   Frame trailer               the checksum field is stepped over.
 *   Reverse bitstream (4)       the backwards, MSB-first reader the
 *                               entropy streams use, and the padding
 *                               marker in the final byte that fixes
 *                               where a stream begins.
 *   FSE (4.1)                   normalised-count parsing and table
 *                               construction. All three tables the RFC
 *                               predefines build with the widths it
 *                               specifies.
 *   Huffman table (4.2.1)       weights to table, including the last
 *                               symbol's weight being implied rather
 *                               than stored, and the direct four-bit
 *                               weight form.
 *
 * Not built, in the order it has to be:
 *
 *   Huffman weights, FSE form   the two-stream weight decode of
 *                               4.2.1.2. Only the direct form is read,
 *                               so a table described the compact way is
 *                               refused.
 *   Huffman decoding            the four-stream literal layout and the
 *                               jump table that finds each stream.
 *   Literals section (3.1.1.3.1)  raw, RLE, Huffman and treeless
 *                               modes; the size format that says how
 *                               the two lengths are packed.
 *   Sequences section (3.1.1.3.2) the three symbol tables in
 *                               predefined, RLE, FSE and repeat modes;
 *                               the baseline-plus-extra-bits tables for
 *                               literal length, match length and
 *                               offset.
 *   Sequence execution (3.1.1.4)  the copies themselves, the three
 *                               repeated offsets and the rule that
 *                               offset codes 1 to 3 mean a repeat
 *                               rather than a distance.
 *   Checksum                    the frame's XXH64 is skipped, not
 *                               verified. A caller expecting a stated
 *                               checksum to be checked does not get
 *                               that yet.
 *   Encoding                    nothing at all. The header describes a
 *                               minimal encoder because one caller
 *                               needs it -- input replay payloads, at
 *                               level 3 -- and none of it is written.
 *
 * Deliberately excluded rather than pending:
 *
 *   Dictionaries                no format this tree reads uses one, and
 *                               a frame requiring one is refused rather
 *                               than decoded to something that looks
 *                               like data.
 *
 * Why this exists at all, given that a vendored Zstandard already
 * works: including <zstd.h> costs C89 conformance -- it declares
 * long long -- so formats/chd/rchd.c cannot both read a Zstandard
 * image and stay within the standard this tree targets. Replacing the
 * dependency also removes 2 MB and 24 source files.
 * ---------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include <encodings/rzstd.h>

/* RFC 8878 section 3.1.1: a frame begins with this, least significant
 * byte first. */
#define RZSTD_MAGIC          0xFD2FB528U

/* Section 3.1.2: a frame whose magic has this in its top nibble is
 * skippable, and carries a size this side does not have to understand. */
#define RZSTD_SKIP_MAGIC_LO  0x184D2A50U
#define RZSTD_SKIP_MAGIC_HI  0x184D2A5FU

/* Section 3.1.1.3.2: the largest block is 128 KiB, whatever the window
 * allows. */
#define RZSTD_BLOCK_MAX      (128 * 1024)

/* Internal outcomes. The public entry points speak the RZSTD_PROCESS_*
 * vocabulary of the header; these distinguish why something failed so
 * the boundary can report it and a reader of this file can see which
 * clause was violated. */
enum
{
   RZ_OK          =  0,
   RZ_TRUNCATED   = -1,
   RZ_DATA        = -2,
   RZ_UNSUPPORTED = -3
};

enum
{
   RZSTD_BLOCK_RAW        = 0,
   RZSTD_BLOCK_RLE        = 1,
   RZSTD_BLOCK_COMPRESSED = 2,
   RZSTD_BLOCK_RESERVED   = 3
};

/* -------- reading --------
 *
 * Multi-byte fields are little-endian throughout, and read a byte at a
 * time so neither the host's byte order nor the alignment of what the
 * caller supplied changes the result.
 */

static uint32_t rzstd_rd16(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t rzstd_rd24(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t rzstd_rd32(const uint8_t *p)
{
   return (uint32_t)p[0]        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Four bytes at @at, zero-filled past the end. The counts description
 * is read through a sliding window that may legitimately reach past the
 * final byte on its last field. */
static uint32_t rzstd_rd32_safe(const uint8_t *p, size_t len, size_t at)
{
   uint32_t v = 0;
   int      i;

   for (i = 0; i < 4; i++)
      if (at + (size_t)i < len)
         v |= (uint32_t)p[at + i] << (i * 8);
   return v;
}

static uint64_t rzstd_rd64(const uint8_t *p)
{
   return (uint64_t)rzstd_rd32(p) | ((uint64_t)rzstd_rd32(p + 4) << 32);
}

/* -------- frame header --------
 *
 * Section 3.1.1.1. The descriptor byte says which of the optional
 * fields follow, so the header's length is not known until it is read.
 */

typedef struct rzstd_frame_header
{
   uint64_t content_size;   /* 0 when not stated                  */
   uint32_t window_size;
   uint32_t dict_id;
   int      has_content_size;
   int      has_checksum;
   int      single_segment;
   uint32_t header_len;
} rzstd_frame_header_t;

/* Bytes the frame content size occupies, by its two-bit field. Note the
 * zero case: it means one byte when the frame is a single segment and
 * no field at all otherwise, which is why the descriptor has to be read
 * before this can be answered (section 3.1.1.1.3). */
static uint32_t rzstd_fcs_bytes(uint32_t code, int single_segment)
{
   switch (code)
   {
      case 0:  return single_segment ? 1 : 0;
      case 1:  return 2;
      case 2:  return 4;
      default: break;
   }
   return 8;
}

static int rzstd_read_frame_header(const uint8_t *src, size_t len,
      rzstd_frame_header_t *out)
{
   uint32_t desc;
   uint32_t fcs_code;
   uint32_t dict_code;
   uint32_t at = 5;      /* magic, then the descriptor */

   if (len < 5)
      return RZ_TRUNCATED;
   if (rzstd_rd32(src) != RZSTD_MAGIC)
      return RZ_DATA;

   memset(out, 0, sizeof(*out));

   desc              = src[4];
   fcs_code          = desc >> 6;
   out->single_segment = (desc >> 5) & 1;
   out->has_checksum   = (desc >> 2) & 1;
   dict_code           = desc & 3;

   /* Bit 3 is reserved and must be zero (section 3.1.1.1.1). A frame
    * setting it is from a future revision this cannot read, and
    * guessing at the rest of the header would be worse than refusing. */
   if (desc & 0x08)
      return RZ_UNSUPPORTED;

   /* The window descriptor is absent exactly when the frame is a single
    * segment, in which case the content size doubles as the window. */
   if (!out->single_segment)
   {
      uint32_t wd;
      uint32_t exponent;
      uint32_t mantissa;

      if (len < at + 1)
         return RZ_TRUNCATED;
      wd       = src[at++];
      exponent = wd >> 3;
      mantissa = wd & 7;

      if (exponent + 10 > RZSTD_WINDOW_LOG_MAX)
         return RZ_UNSUPPORTED;

      /* Section 3.1.1.1.2: a base of 2^(10+exponent) plus an eighth of
       * that for each unit of mantissa. */
      {
         uint32_t base = (uint32_t)1 << (10 + exponent);
         out->window_size = base + (base / 8) * mantissa;
      }
   }

   if (dict_code)
   {
      uint32_t n = dict_code == 3 ? 4 : dict_code;

      if (len < at + n)
         return RZ_TRUNCATED;
      out->dict_id = n == 1 ? src[at]
                   : n == 2 ? rzstd_rd16(src + at)
                            : rzstd_rd32(src + at);
      at += n;

      /* A frame needing a dictionary cannot be decoded without it, and
       * nothing this reads uses one. Refused rather than decoded to
       * something that looks like data. */
      return RZ_UNSUPPORTED;
   }

   {
      uint32_t n = rzstd_fcs_bytes(fcs_code, out->single_segment);

      if (n)
      {
         if (len < at + n)
            return RZ_TRUNCATED;
         /* The two-byte form is stored less 256, which the RFC notes
          * because a frame that small would not be worth a header. */
         out->content_size = n == 1 ? src[at]
                           : n == 2 ? rzstd_rd16(src + at) + 256
                           : n == 4 ? rzstd_rd32(src + at)
                                    : rzstd_rd64(src + at);
         out->has_content_size = 1;
         at += n;
      }
   }

   if (out->single_segment)
      out->window_size = (uint32_t)out->content_size;

   out->header_len = at;
   return RZ_OK;
}

/* -------- the reverse bitstream --------
 *
 * Section 4. Zstandard's entropy streams are read backwards: the last
 * byte is the first one consumed, and within the stream bits come out
 * most significant first. The last byte also carries a padding marker,
 * a single set bit above the real data, which fixes where the stream
 * actually starts.
 *
 * This is the opposite of the forward, MSB-first reader the CHD map and
 * A/V hunks use, so the two do not share code.
 */

typedef struct rzstd_rbits
{
   const uint8_t *base;
   size_t         pos;
   uint64_t       bits;
   uint32_t       count;
   int            overrun;
} rzstd_rbits_t;

static void rzstd_rbits_fill(rzstd_rbits_t *b)
{
   while (b->count <= 56 && b->pos > 0)
   {
      b->pos--;
      b->bits |= (uint64_t)b->base[b->pos] << (56 - b->count);
      b->count += 8;
   }
}

/* Positions the reader at the last set bit of the final byte, which the
 * encoder wrote purely to mark where the data ends. A final byte of
 * zero has no such marker and cannot be a valid stream. */
static int rzstd_rbits_init(rzstd_rbits_t *b, const uint8_t *src, size_t len)
{
   uint32_t last;
   uint32_t pad;

   memset(b, 0, sizeof(*b));
   if (!len)
      return RZ_DATA;

   last = src[len - 1];
   if (!last)
      return RZ_DATA;

   b->base = src;
   b->pos  = len;
   rzstd_rbits_fill(b);

   pad = 0;
   while (!((last << pad) & 0x80))
      pad++;
   pad++;

   b->bits  <<= pad;
   b->count  -= pad;
   return RZ_OK;
}

static uint32_t rzstd_rbits_read(rzstd_rbits_t *b, uint32_t n)
{
   uint32_t v;

   if (!n)
      return 0;
   if (b->count < n)
   {
      rzstd_rbits_fill(b);
      if (b->count < n)
      {
         /* Running out is not always an error: a stream may end with
          * its last symbols implied by zeroes. The shortfall is
          * supplied as zero and the overrun recorded so a caller that
          * cares can tell. */
         v          = (uint32_t)(b->bits >> (64 - n));
         b->bits    = 0;
         b->count   = 0;
         b->overrun = 1;
         return v;
      }
   }

   v         = (uint32_t)(b->bits >> (64 - n));
   b->bits <<= n;
   b->count -= n;
   return v;
}

/* -------- FSE --------
 *
 * Section 4.1. A table is described by an accuracy log and one
 * normalised count per symbol, and those counts are themselves coded
 * with a width that narrows as the remaining budget does.
 */

#define RZSTD_FSE_MAX_ACCURACY_LOG 9
#define RZSTD_FSE_MAX_SYMBOLS      256

typedef struct rzstd_fse_entry
{
   uint16_t next_state;
   uint8_t  symbol;
   uint8_t  bits;
} rzstd_fse_entry_t;

typedef struct rzstd_fse
{
   rzstd_fse_entry_t *table;
   uint32_t           accuracy_log;
} rzstd_fse_t;

/* Reads a table description (4.1.1) into normalised counts. These are
 * forward-read and least significant bit first, which is a third bit
 * order again and the reason this does its own reading. */
static int rzstd_fse_read_counts(const uint8_t *src, size_t len,
      int16_t *counts, uint32_t *symbol_count, uint32_t *accuracy_log,
      uint32_t max_symbols, uint32_t max_log, size_t *used)
{
   uint32_t bit_pos = 0;
   uint32_t symbol  = 0;
   int32_t  remaining;
   uint32_t log;
   int      previous_is_zero = 0;

   if (len < 1)
      return RZ_TRUNCATED;

#define RZ_PEEK(nbits) \
   ((uint32_t)((rzstd_rd32_safe(src, len, bit_pos >> 3) >> (bit_pos & 7)) \
      & (((uint32_t)1 << (nbits)) - 1)))

   log = RZ_PEEK(4) + 5;
   bit_pos += 4;
   if (log > max_log || log > RZSTD_FSE_MAX_ACCURACY_LOG)
      return RZ_DATA;

   remaining = (int32_t)(((uint32_t)1 << log) + 1);

   while (remaining > 1 && symbol < max_symbols)
   {
      uint32_t bits_needed;
      uint32_t low_mask;
      uint32_t value;
      int32_t  count;

      if (previous_is_zero)
      {
         /* A zero count is followed by a run of further zeroes, two
          * bits at a time, with three meaning "and more" (4.1.1). */
         uint32_t repeat = 0;

         for (;;)
         {
            uint32_t two = RZ_PEEK(2);
            bit_pos += 2;
            repeat  += two;
            if (two != 3)
               break;
            if ((bit_pos >> 3) > len)
               return RZ_TRUNCATED;
         }
         while (repeat-- && symbol < max_symbols)
            counts[symbol++] = 0;
         previous_is_zero = 0;
         continue;
      }

      /* The width is enough bits to name anything still spendable, with
       * the low part of the range using one fewer. */
      bits_needed = 0;
      while (((uint32_t)1 << (bits_needed + 1)) <= (uint32_t)remaining)
         bits_needed++;
      low_mask = ((uint32_t)1 << bits_needed) - 1;

      value = RZ_PEEK(bits_needed + 1);
      if ((value & low_mask) < (((uint32_t)1 << (bits_needed + 1))
               - 1 - (uint32_t)remaining))
      {
         value   &= low_mask;
         bit_pos += bits_needed;
      }
      else
      {
         bit_pos += bits_needed + 1;
         if (value >= ((uint32_t)1 << bits_needed))
            value -= ((uint32_t)1 << (bits_needed + 1))
                   - 1 - (uint32_t)remaining;
      }

      /* Stored biased by one, so minus one can mean the below-one
       * probability that gets a single low-frequency slot. */
      count            = (int32_t)value - 1;
      counts[symbol++] = (int16_t)count;
      remaining       -= (count < 0) ? -count : count;
      previous_is_zero = (count == 0);

      if ((bit_pos >> 3) > len)
         return RZ_TRUNCATED;
   }

   if (remaining != 1)
      return RZ_DATA;

   *symbol_count = symbol;
   *accuracy_log = log;
   *used         = (size_t)((bit_pos + 7) >> 3);
   return RZ_OK;
#undef RZ_PEEK
}

/* Builds a decoding table from normalised counts (4.1.1).
 *
 * Symbols are spread across the table with a stride the format fixes at
 * (size/2) + (size/8) + 3, chosen so a symbol's slots scatter rather
 * than clump. A count of -1 means the symbol is less probable than one
 * slot's worth; those take slots from the end and the walk skips them.
 */
static int rzstd_fse_build(rzstd_fse_t *fse, rzstd_fse_entry_t *table,
      const int16_t *counts, uint32_t symbol_count, uint32_t accuracy_log)
{
   uint32_t size     = (uint32_t)1 << accuracy_log;
   uint32_t mask     = size - 1;
   uint32_t step     = (size >> 1) + (size >> 3) + 3;
   uint32_t high     = size;
   uint32_t position = 0;
   uint32_t sym;
   uint32_t i;
   uint16_t next[RZSTD_FSE_MAX_SYMBOLS];

   fse->table        = table;
   fse->accuracy_log = accuracy_log;

   for (sym = 0; sym < symbol_count; sym++)
   {
      if (counts[sym] != -1)
         continue;
      table[--high].symbol = (uint8_t)sym;
      next[sym]            = 1;
   }

   for (sym = 0; sym < symbol_count; sym++)
   {
      int32_t n = counts[sym];

      if (n <= 0)
         continue;
      next[sym] = (uint16_t)n;
      for (i = 0; i < (uint32_t)n; i++)
      {
         table[position].symbol = (uint8_t)sym;
         do
         {
            position = (position + step) & mask;
         } while (position >= high);
      }
   }

   if (position != 0)
      return RZ_DATA;

   /* A symbol holding 2^k slots reads accuracy_log - k bits, and the
    * states it can reach lie consecutively. */
   for (i = 0; i < size; i++)
   {
      uint8_t  sy    = table[i].symbol;
      uint16_t taken = next[sy]++;
      uint32_t bits  = accuracy_log;

      while (((uint32_t)1 << bits) > (uint32_t)taken && bits)
         bits--;
      table[i].bits       = (uint8_t)(accuracy_log - bits);
      table[i].next_state = (uint16_t)(((uint32_t)taken
               << (accuracy_log - bits)) - size);
   }

   return RZ_OK;
}

static uint32_t rzstd_fse_begin(const rzstd_fse_t *fse, rzstd_rbits_t *b)
{
   return rzstd_rbits_read(b, fse->accuracy_log);
}

static uint8_t rzstd_fse_symbol(const rzstd_fse_t *fse, uint32_t state)
{
   return fse->table[state].symbol;
}

static uint32_t rzstd_fse_next(const rzstd_fse_t *fse, uint32_t state,
      rzstd_rbits_t *b)
{
   const rzstd_fse_entry_t *e = &fse->table[state];

   return (uint32_t)e->next_state + rzstd_rbits_read(b, e->bits);
}

/* -------- Huffman --------
 *
 * Section 4.2, used only for literals. The table is described by one
 * weight per symbol, and the weights are themselves either FSE-coded or
 * written directly at four bits each.
 *
 * A weight of zero means the symbol is absent. A weight w above zero
 * means the symbol takes 2^(w-1) of the table's slots, so the widest
 * codes belong to the smallest weights -- the opposite relation to a
 * code length, which is what makes this worth stating.
 */

#define RZSTD_HUF_MAX_BITS    11
#define RZSTD_HUF_MAX_SYMBOLS 256

typedef struct rzstd_huf
{
   uint8_t  symbol[1 << RZSTD_HUF_MAX_BITS];
   uint8_t  bits[1 << RZSTD_HUF_MAX_BITS];
   uint32_t max_bits;
} rzstd_huf_t;

/* Turns weights into a table. The final symbol's weight is not stored:
 * it is whatever makes the total a power of two, which is why the
 * weights alone are enough (4.2.1). */
static int rzstd_huf_build(rzstd_huf_t *huf, const uint8_t *weights,
      uint32_t count)
{
   uint32_t total = 0;
   uint32_t max_bits;
   uint32_t next_rank_start[RZSTD_HUF_MAX_BITS + 2];
   uint32_t rank_count[RZSTD_HUF_MAX_BITS + 2];
   uint32_t i;
   uint32_t position = 0;
   uint8_t  all[RZSTD_HUF_MAX_SYMBOLS + 1];

   if (!count || count > RZSTD_HUF_MAX_SYMBOLS)
      return RZ_DATA;

   for (i = 0; i < count; i++)
   {
      if (weights[i] > RZSTD_HUF_MAX_BITS)
         return RZ_DATA;
      all[i] = weights[i];
      if (weights[i])
         total += (uint32_t)1 << (weights[i] - 1);
   }
   if (!total)
      return RZ_DATA;

   /* The table is the next power of two at or above the total, and the
    * shortfall is exactly the last symbol's share. */
   max_bits = 0;
   while (((uint32_t)1 << max_bits) < total)
      max_bits++;
   if (((uint32_t)1 << max_bits) == total)
      max_bits++;
   if (max_bits > RZSTD_HUF_MAX_BITS)
      return RZ_DATA;

   {
      uint32_t left = ((uint32_t)1 << max_bits) - total;
      uint32_t w    = 0;

      /* left has to be a power of two; it names the last weight. */
      if (left == 0 || (left & (left - 1)))
         return RZ_DATA;
      while (((uint32_t)1 << w) < left)
         w++;
      all[count] = (uint8_t)(w + 1);
      count++;
   }

   huf->max_bits = max_bits;

   memset(rank_count, 0, sizeof(rank_count));
   for (i = 0; i < count; i++)
      if (all[i])
         rank_count[all[i]]++;

   /* Slots are laid out by descending weight, so that the shortest
    * codes come first and a lookup can be a plain index. */
   {
      uint32_t w;
      next_rank_start[max_bits + 1] = 0;
      for (w = max_bits; w >= 1; w--)
      {
         next_rank_start[w] = position;
         position += rank_count[w] * ((uint32_t)1 << (w - 1));
         if (w == 1)
            break;
      }
   }

   if (position != ((uint32_t)1 << max_bits))
      return RZ_DATA;

   for (i = 0; i < count; i++)
   {
      uint32_t w = all[i];
      uint32_t n;
      uint32_t at;

      if (!w)
         continue;
      n  = (uint32_t)1 << (w - 1);
      at = next_rank_start[w];
      next_rank_start[w] += n;

      memset(huf->symbol + at, (int)i, n);
      memset(huf->bits + at, (int)(max_bits + 1 - w), n);
   }

   return RZ_OK;
}

/* Reads a weight description (4.2.1) and builds the table.
 *
 * The first byte decides the form: 128 or above means the weights
 * follow directly at four bits each, and the byte itself carries how
 * many. Below that it is the byte length of an FSE-coded stream. */
static int rzstd_huf_read(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   uint8_t  weights[RZSTD_HUF_MAX_SYMBOLS + 1];
   uint32_t count;
   uint32_t i;

   if (!len)
      return RZ_TRUNCATED;

   if (src[0] >= 128)
   {
      count = (uint32_t)src[0] - 127;
      if (count > RZSTD_HUF_MAX_SYMBOLS)
         return RZ_DATA;
      /* Two weights to a byte, high nibble first. */
      if (len < 1 + ((count + 1) / 2))
         return RZ_TRUNCATED;
      for (i = 0; i < count; i++)
         weights[i] = (i & 1) ? (src[1 + i / 2] & 0x0f)
                              : (src[1 + i / 2] >> 4);
      *used = 1 + (count + 1) / 2;
      return rzstd_huf_build(huf, weights, count);
   }

   /* The FSE-coded form is not built yet; it needs the two-stream
    * weight decode of 4.2.1.2. */
   (void)weights;
   return RZ_UNSUPPORTED;
}

/* -------- blocks --------
 *
 * Section 3.1.1.2. A frame is a run of blocks, each with a three-byte
 * header giving its type, its size, and whether it is the last.
 */

typedef struct rzstd_block_header
{
   uint32_t size;
   uint32_t type;
   int      last;
} rzstd_block_header_t;

static void rzstd_read_block_header(const uint8_t *p,
      rzstd_block_header_t *out)
{
   uint32_t v = rzstd_rd24(p);

   out->last = (int)(v & 1);
   out->type = (v >> 1) & 3;
   out->size = v >> 3;
}

/* Decodes one frame into @dst, which must be large enough for what the
 * frame states. Returns bytes written through @wrote and how much of
 * @src the frame occupied through @used, so a caller holding several
 * frames can walk them. */
static int rzstd_decode_frame(const uint8_t *src, size_t src_len,
      uint8_t *dst, size_t dst_len, size_t *wrote, size_t *used)
{
   rzstd_frame_header_t h;
   size_t               at  = 0;
   size_t               out = 0;
   int                  e;

   if ((e = rzstd_read_frame_header(src, src_len, &h)) != RZ_OK)
      return e;
   at = h.header_len;

   for (;;)
   {
      rzstd_block_header_t b;

      if (src_len < at + 3)
         return RZ_TRUNCATED;
      rzstd_read_block_header(src + at, &b);
      at += 3;

      if (b.type == RZSTD_BLOCK_RESERVED)
         return RZ_DATA;
      if (b.size > RZSTD_BLOCK_MAX)
         return RZ_DATA;

      switch (b.type)
      {
         case RZSTD_BLOCK_RAW:
            /* The block's bytes are its content, uncompressed. */
            if (src_len < at + b.size)
               return RZ_TRUNCATED;
            if (out + b.size > dst_len)
               return RZ_DATA;
            memcpy(dst + out, src + at, b.size);
            out += b.size;
            at  += b.size;
            break;

         case RZSTD_BLOCK_RLE:
            /* One byte stands for @size copies of itself, so the block
             * occupies a single byte however long its content. */
            if (src_len < at + 1)
               return RZ_TRUNCATED;
            if (out + b.size > dst_len)
               return RZ_DATA;
            memset(dst + out, src[at], b.size);
            out += b.size;
            at  += 1;
            break;

         default:
            /* Compressed: literals, then sequences. Not yet built. */
            return RZ_UNSUPPORTED;
      }

      if (b.last)
         break;
   }

   /* Section 3.1.1: the optional checksum is the low four bytes of the
    * frame content's XXH64. It is skipped rather than verified, which
    * is noted because a caller may reasonably expect it to be checked
    * when the frame says it is there. */
   if (h.has_checksum)
   {
      if (src_len < at + 4)
         return RZ_TRUNCATED;
      at += 4;
   }

   if (h.has_content_size && out != h.content_size)
      return RZ_DATA;

   if (wrote)
      *wrote = out;
   if (used)
      *used = at;
   return RZ_OK;
}

/* Test seams. These exist because the file has no public entry point
 * yet: the one it will have cannot be written until a compressed block
 * decodes, and until then the only way to exercise what is built is to
 * call into it directly. They go when the real interface arrives. */

/* Decodes a frame, which today means one whose blocks are all raw or
 * RLE. */
int rzstd_probe_frame(const uint8_t *src, size_t src_len,
      uint8_t *dst, size_t dst_len, size_t *wrote, size_t *used)
{
   return rzstd_decode_frame(src, src_len, dst, dst_len, wrote, used);
}

/* Reports what a frame's header states, without decoding. */
int rzstd_probe_header(const uint8_t *src, size_t len,
      uint64_t *content, uint32_t *window, int *checksum, uint32_t *hlen)
{
   rzstd_frame_header_t h;
   int                  e = rzstd_read_frame_header(src, len, &h);

   if (e != RZ_OK)
      return e;
   *content  = h.content_size;
   *window   = h.window_size;
   *checksum = h.has_checksum;
   *hlen     = h.header_len;
   return RZ_OK;
}

/* Test seam: builds an FSE table from given counts and reports the
 * widths it assigned, so the spread can be checked against the tables
 * the RFC predefines. */
int rzstd_probe_fse(const int16_t *counts, uint32_t symbol_count,
      uint32_t accuracy_log, uint8_t *bits_out, uint16_t *state_out,
      uint8_t *sym_out)
{
   static rzstd_fse_entry_t table[1 << RZSTD_FSE_MAX_ACCURACY_LOG];
   rzstd_fse_t fse;
   uint32_t    size = (uint32_t)1 << accuracy_log;
   uint32_t    i;
   int         e;

   memset(table, 0, sizeof(table));
   e = rzstd_fse_build(&fse, table, counts, symbol_count, accuracy_log);
   if (e != RZ_OK)
      return e;
   for (i = 0; i < size; i++)
   {
      bits_out[i]  = table[i].bits;
      state_out[i] = table[i].next_state;
      sym_out[i]   = table[i].symbol;
   }
   return RZ_OK;
}
