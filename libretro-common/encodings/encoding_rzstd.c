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
 *   Huffman table (4.2.1)       weights to table, both the direct
 *                               four-bit form and the FSE-coded one,
 *                               including the last symbol's weight
 *                               being implied rather than stored.
 *   Huffman decoding (4.2.2)    one-stream and four-stream layouts.
 *   Literals section (3.1.1.3.1)  raw, RLE, Huffman and treeless
 *                               modes, and the bit-packed size fields.
 *                               Verified as far as byte counts: a real
 *                               block's Huffman literals regenerate to
 *                               the length the section states.
 *   Sequences section (3.1.1.3.2) the three tables in predefined, RLE,
 *                               FSE and repeat modes, the
 *                               baseline-plus-extra-bits tables, and
 *                               the execution of 3.1.1.4 with its
 *                               repeated offsets.
 *
 * WRONG, and known to be:
 *
 *   FSE state to symbol         a real frame that should decode to one
 *                               literal and a 2047-byte match at
 *                               offset 1 yields the right literal
 *                               length and the right offset, and a
 *                               match length code of 48 where it must
 *                               be 46. Two of the three tables give
 *                               the right answer from the same
 *                               bitstream, so the fault is narrow: the
 *                               initial state read, the spread, or the
 *                               width assignment, on the match-length
 *                               table specifically.
 *
 *                               A frame therefore parses end to end and
 *                               produces wrong bytes, which is worse
 *                               than refusing. Nothing may use this.
 *
 * Not built:
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

/* Whether @n more bits can be taken without running past the end. The
 * weight stream ends by exhaustion rather than by a count, so this is
 * how its length is discovered. */
static int rzstd_rbits_have(rzstd_rbits_t *b, uint32_t n)
{
   if (b->count >= n)
      return 1;
   rzstd_rbits_fill(b);
   return b->count >= n;
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

   /* The compact form: the byte is the length of an FSE-coded stream
    * holding the weights (4.2.1.2). Two interleaved states share one
    * bitstream, alternating, which halves the dependency chain -- the
    * reason the format does it rather than a single state. */
   {
      int16_t           counts[RZSTD_FSE_MAX_SYMBOLS];
      rzstd_fse_entry_t table[1 << 6];
      rzstd_fse_t       fse;
      rzstd_rbits_t     bits;
      uint32_t          symbol_count = 0;
      uint32_t          accuracy_log = 0;
      size_t            desc_used    = 0;
      uint32_t          size         = src[0];
      uint32_t          state1;
      uint32_t          state2;
      int               e;

      if (len < 1 + size)
         return RZ_TRUNCATED;

      /* The weights alphabet is at most the maximum weight plus one,
       * and its accuracy log is capped at 6 (4.2.1.2). */
      e = rzstd_fse_read_counts(src + 1, size, counts, &symbol_count,
            &accuracy_log, RZSTD_HUF_MAX_BITS + 1, 6, &desc_used);
      if (e != RZ_OK)
         return e;
      if (desc_used > size)
         return RZ_DATA;

      if ((e = rzstd_fse_build(&fse, table, counts, symbol_count,
                  accuracy_log)) != RZ_OK)
         return e;

      if ((e = rzstd_rbits_init(&bits, src + 1 + desc_used,
                  size - desc_used)) != RZ_OK)
         return e;

      state1 = rzstd_fse_begin(&fse, &bits);
      state2 = rzstd_fse_begin(&fse, &bits);

      /* The stream does not say how many weights it holds. It ends when
       * updating a state would need more bits than remain, at which
       * point both states still hold one symbol each and both are
       * emitted (4.2.1.2). So the check is on the update, not on the
       * symbol, and two symbols follow the loop rather than one. */
      count = 0;
      for (;;)
      {
         if (count + 2 > RZSTD_HUF_MAX_SYMBOLS)
            return RZ_DATA;

         weights[count++] = rzstd_fse_symbol(&fse, state1);
         if (!rzstd_rbits_have(&bits, fse.table[state1].bits))
         {
            weights[count++] = rzstd_fse_symbol(&fse, state2);
            break;
         }
         state1 = rzstd_fse_next(&fse, state1, &bits);

         weights[count++] = rzstd_fse_symbol(&fse, state2);
         if (!rzstd_rbits_have(&bits, fse.table[state2].bits))
         {
            weights[count++] = rzstd_fse_symbol(&fse, state1);
            break;
         }
         state2 = rzstd_fse_next(&fse, state2, &bits);
      }

      *used = 1 + size;
      return rzstd_huf_build(huf, weights, count);
   }
}

/* Pulls one symbol. The reader always holds at least max_bits, so a
 * peek is a plain index into the table. */
static uint8_t rzstd_huf_symbol(const rzstd_huf_t *huf, rzstd_rbits_t *b)
{
   uint32_t peek;
   uint32_t index;

   if (b->count < huf->max_bits)
      rzstd_rbits_fill(b);

   peek  = (uint32_t)(b->bits >> (64 - huf->max_bits));
   index = peek;

   {
      uint8_t n = huf->bits[index];

      b->bits  <<= n;
      if (b->count >= n)
         b->count -= n;
      else
      {
         b->count   = 0;
         b->overrun = 1;
      }
      return huf->symbol[index];
   }
}

/* Decodes @count literals from either the one-stream or four-stream
 * layout (4.2.2).
 *
 * Four streams exist so a decoder can run them in parallel; they are
 * not independent halves of the data but interleaved by position, each
 * taking a quarter of the output in order. The first three sizes come
 * from a six-byte jump table and the fourth is whatever remains, which
 * is why only three are stored. */
static int rzstd_huf_decode(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   size_t i;

   if (!four_streams)
   {
      rzstd_rbits_t b;
      int           e = rzstd_rbits_init(&b, src, src_len);

      if (e != RZ_OK)
         return e;
      for (i = 0; i < count; i++)
         dst[i] = rzstd_huf_symbol(huf, &b);
      return RZ_OK;
   }

   {
      rzstd_rbits_t b[4];
      size_t        size[4];
      size_t        at = 6;
      size_t        quarter;
      size_t        k;
      int           e;

      if (src_len < 6)
         return RZ_TRUNCATED;
      size[0] = rzstd_rd16(src);
      size[1] = rzstd_rd16(src + 2);
      size[2] = rzstd_rd16(src + 4);
      if (size[0] + size[1] + size[2] + 6 > src_len)
         return RZ_DATA;
      size[3] = src_len - 6 - size[0] - size[1] - size[2];

      for (k = 0; k < 4; k++)
      {
         if ((e = rzstd_rbits_init(&b[k], src + at, size[k])) != RZ_OK)
            return e;
         at += size[k];
      }

      /* Each stream covers a quarter of the output, the last taking
       * the remainder. */
      quarter = (count + 3) / 4;
      for (k = 0; k < 4; k++)
      {
         size_t start = k * quarter;
         size_t end   = start + quarter;

         if (start >= count)
            break;
         if (end > count)
            end = count;
         for (i = start; i < end; i++)
            dst[i] = rzstd_huf_symbol(huf, &b[k]);
      }
      return RZ_OK;
   }
}

/* -------- literals --------
 *
 * Section 3.1.1.3.1. The section opens with a byte whose low two bits
 * give the form and the next two the shape of the size fields, which
 * are packed at bit granularity rather than byte.
 */

enum
{
   RZSTD_LIT_RAW      = 0,
   RZSTD_LIT_RLE      = 1,
   RZSTD_LIT_HUFFMAN  = 2,
   RZSTD_LIT_TREELESS = 3
};

typedef struct rzstd_literals
{
   uint8_t *data;
   size_t   size;
} rzstd_literals_t;

static int rzstd_read_literals(rzstd_literals_t *out, rzstd_huf_t *huf,
      int *huf_valid, const uint8_t *src, size_t src_len, uint8_t *scratch,
      size_t scratch_len, size_t *used)
{
   uint32_t type;
   uint32_t size_format;
   size_t   regenerated;
   size_t   compressed = 0;
   size_t   at;
   int      four = 0;
   int      e;

   if (!src_len)
      return RZ_TRUNCATED;

   type        = src[0] & 3;
   size_format = (src[0] >> 2) & 3;

   if (type == RZSTD_LIT_RAW || type == RZSTD_LIT_RLE)
   {
      /* One, two or three size widths, chosen by the format field; the
       * odd case is that 0 and 2 both mean the same five-bit width,
       * because the low bit is free when there is no second size. */
      switch (size_format)
      {
         case 0: case 2:
            regenerated = src[0] >> 3;
            at = 1;
            break;
         case 1:
            if (src_len < 2)
               return RZ_TRUNCATED;
            regenerated = ((size_t)src[0] >> 4) | ((size_t)src[1] << 4);
            at = 2;
            break;
         default:
            if (src_len < 3)
               return RZ_TRUNCATED;
            regenerated = ((size_t)src[0] >> 4) | ((size_t)src[1] << 4)
                        | ((size_t)src[2] << 12);
            at = 3;
            break;
      }

      if (regenerated > scratch_len)
         return RZ_DATA;

      if (type == RZSTD_LIT_RAW)
      {
         if (src_len < at + regenerated)
            return RZ_TRUNCATED;
         memcpy(scratch, src + at, regenerated);
         *used = at + regenerated;
      }
      else
      {
         if (src_len < at + 1)
            return RZ_TRUNCATED;
         memset(scratch, src[at], regenerated);
         *used = at + 1;
      }

      out->data = scratch;
      out->size = regenerated;
      return RZ_OK;
   }

   /* Huffman and treeless carry both a regenerated and a compressed
    * size, packed together across two to five bytes. */
   switch (size_format)
   {
      case 0:
      case 1:
         if (src_len < 3)
            return RZ_TRUNCATED;
         regenerated = (((size_t)src[0] >> 4) | ((size_t)src[1] << 4))
                     & 0x3ff;
         compressed  = (((size_t)src[1] >> 6) | ((size_t)src[2] << 2))
                     & 0x3ff;
         four = (size_format == 1);
         at   = 3;
         break;
      case 2:
         if (src_len < 4)
            return RZ_TRUNCATED;
         regenerated = (((size_t)src[0] >> 4) | ((size_t)src[1] << 4)
                     | ((size_t)src[2] << 12)) & 0x3fff;
         compressed  = (((size_t)src[2] >> 2) | ((size_t)src[3] << 6))
                     & 0x3fff;
         four = 1;
         at   = 4;
         break;
      default:
         if (src_len < 5)
            return RZ_TRUNCATED;
         regenerated = (((size_t)src[0] >> 4) | ((size_t)src[1] << 4)
                     | ((size_t)src[2] << 12)) & 0x3ffff;
         compressed  = (((size_t)src[2] >> 6) | ((size_t)src[3] << 2)
                     | ((size_t)src[4] << 10)) & 0x3ffff;
         four = 1;
         at   = 5;
         break;
   }

   if (regenerated > scratch_len)
      return RZ_DATA;
   if (src_len < at + compressed)
      return RZ_TRUNCATED;

   if (type == RZSTD_LIT_HUFFMAN)
   {
      size_t tree_used = 0;

      if ((e = rzstd_huf_read(huf, src + at, compressed,
                  &tree_used)) != RZ_OK)
         return e;
      *huf_valid = 1;
      e = rzstd_huf_decode(huf, src + at + tree_used,
            compressed - tree_used, scratch, regenerated, four);
   }
   else
   {
      /* Treeless: the table is whatever the previous block left, which
       * is why a block can be treeless only after one that was not. */
      if (!*huf_valid)
         return RZ_DATA;
      e = rzstd_huf_decode(huf, src + at, compressed, scratch,
            regenerated, four);
   }

   if (e != RZ_OK)
      return e;

   out->data = scratch;
   out->size = regenerated;
   *used     = at + compressed;
   return RZ_OK;
}

/* -------- sequences --------
 *
 * Section 3.1.1.3.2. A sequence is a run of literals to copy out
 * followed by a match to copy from earlier output. Three symbol streams
 * describe them, interleaved into one bitstream and read in a fixed
 * order.
 */

/* Section 3.1.1.3.2.1.1. A symbol names a baseline and how many extra
 * bits to add to it. */
static const uint32_t rzstd_ll_base[36] =
{
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
   16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 128, 256, 512, 1024,
   2048, 4096, 8192, 16384, 32768, 65536
};
static const uint8_t rzstd_ll_bits[36] =
{
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12,
   13, 14, 15, 16
};

static const uint32_t rzstd_ml_base[53] =
{
   3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
   19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
   35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99, 131, 259, 515, 1027,
   2051, 4099, 8195, 16387, 32771, 65539
};
static const uint8_t rzstd_ml_bits[53] =
{
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11,
   12, 13, 14, 15, 16
};

/* The predefined tables of 3.1.1.3.2.2.1, as normalised counts. */
static const int16_t rzstd_ll_default[36] =
{
   4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1,
   -1, -1, -1, -1
};
/* Seven minus-ones, from 46 to 52. Getting the count of them wrong is
 * not caught by anything: the table still sums to 64 and still builds,
 * and the only visible effect is which symbols sit at the top of the
 * table -- which is precisely where a maximal initial state lands, so a
 * frame decodes to the wrong match length and nothing complains. */
static const int16_t rzstd_ml_default[53] =
{
   1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1,
   -1, -1, -1, -1, -1
};
static const int16_t rzstd_of_default[29] =
{
   1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1
};

enum
{
   RZSTD_SEQ_PREDEFINED = 0,
   RZSTD_SEQ_RLE        = 1,
   RZSTD_SEQ_FSE        = 2,
   RZSTD_SEQ_REPEAT     = 3
};

typedef struct rzstd_seq_tables
{
   rzstd_fse_entry_t ll[1 << 9];
   rzstd_fse_entry_t ml[1 << 9];
   rzstd_fse_entry_t of[1 << 8];
   rzstd_fse_t       ll_fse;
   rzstd_fse_t       ml_fse;
   rzstd_fse_t       of_fse;
   int               valid;
} rzstd_seq_tables_t;

/* Sets up one of the three tables according to its two-bit mode. */
static int rzstd_seq_table(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous)
{
   int16_t  counts[RZSTD_FSE_MAX_SYMBOLS];
   uint32_t symbol_count = 0;
   uint32_t accuracy_log = 0;
   int      e;

   *used = 0;

   switch (mode)
   {
      case RZSTD_SEQ_PREDEFINED:
         return rzstd_fse_build(fse, storage, predef, predef_count,
               predef_log);

      case RZSTD_SEQ_RLE:
         /* One byte names the only symbol; the table is one slot wide
          * and consumes no bits when decoding. */
         if (!len)
            return RZ_TRUNCATED;
         memset(counts, 0, sizeof(counts));
         if (src[0] > max_symbol)
            return RZ_DATA;
         counts[src[0]] = 1;
         *used = 1;
         return rzstd_fse_build(fse, storage, counts,
               (uint32_t)src[0] + 1, 0);

      case RZSTD_SEQ_FSE:
         e = rzstd_fse_read_counts(src, len, counts, &symbol_count,
               &accuracy_log, max_symbol + 1, max_log, used);
         if (e != RZ_OK)
            return e;
         return rzstd_fse_build(fse, storage, counts, symbol_count,
               accuracy_log);

      default:
         break;
   }

   /* Repeat: whatever the previous block left standing. */
   if (!have_previous)
      return RZ_DATA;
   return RZ_OK;
}

/* Decodes a block's sequences and executes them into @dst.
 *
 * The three offsets most recently used are remembered, and an offset
 * code of 1, 2 or 3 names one of them rather than a distance -- with
 * the wrinkle that when there are no literals the meaning shifts by
 * one, because repeating the immediately previous offset would then be
 * expressible twice (3.1.1.4). */
static int rzstd_decode_sequences(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len,
      uint8_t *dst, size_t dst_len, size_t *dst_at, uint32_t *repeat)
{
   rzstd_rbits_t bits;
   size_t        nseq;
   size_t        at = 0;
   uint32_t      modes;
   size_t        used;
   uint32_t      ll_state;
   uint32_t      ml_state;
   uint32_t      of_state;
   size_t        lit_at = 0;
   size_t        out    = *dst_at;
   size_t        i;
   int           e;

   if (!src_len)
      return RZ_TRUNCATED;

   /* The count is one, two or three bytes, distinguished by the first
    * (3.1.1.3.2.1). */
   if (src[0] < 128)
   {
      nseq = src[0];
      at   = 1;
   }
   else if (src[0] < 255)
   {
      if (src_len < 2)
         return RZ_TRUNCATED;
      nseq = ((size_t)(src[0] - 128) << 8) + src[1];
      at   = 2;
   }
   else
   {
      if (src_len < 3)
         return RZ_TRUNCATED;
      nseq = (size_t)rzstd_rd16(src + 1) + 0x7f00;
      at   = 3;
   }

   if (!nseq)
   {
      /* No sequences: the literals are the whole block. */
      if (out + lit_len > dst_len)
         return RZ_DATA;
      memcpy(dst + out, literals, lit_len);
      *dst_at = out + lit_len;
      return RZ_OK;
   }

   if (src_len < at + 1)
      return RZ_TRUNCATED;
   modes = src[at++];

   e = rzstd_seq_table(&tab->ll_fse, tab->ll, (modes >> 6) & 3,
         rzstd_ll_default, 36, 6, 35, 9, src + at, src_len - at, &used,
         tab->valid);
   if (e != RZ_OK)
      return e;
   at += used;

   e = rzstd_seq_table(&tab->of_fse, tab->of, (modes >> 4) & 3,
         rzstd_of_default, 29, 5, 31, 8, src + at, src_len - at, &used,
         tab->valid);
   if (e != RZ_OK)
      return e;
   at += used;

   e = rzstd_seq_table(&tab->ml_fse, tab->ml, (modes >> 2) & 3,
         rzstd_ml_default, 53, 6, 52, 9, src + at, src_len - at, &used,
         tab->valid);
   if (e != RZ_OK)
      return e;
   at += used;

   tab->valid = 1;

   if (src_len < at)
      return RZ_TRUNCATED;
   if ((e = rzstd_rbits_init(&bits, src + at, src_len - at)) != RZ_OK)
      return e;

   /* The initial states come in this order, which is not the order the
    * symbols are then read in. */
   ll_state = rzstd_fse_begin(&tab->ll_fse, &bits);
   of_state = rzstd_fse_begin(&tab->of_fse, &bits);
   ml_state = rzstd_fse_begin(&tab->ml_fse, &bits);

   for (i = 0; i < nseq; i++)
   {
      uint32_t ll_code = rzstd_fse_symbol(&tab->ll_fse, ll_state);
      uint32_t ml_code = rzstd_fse_symbol(&tab->ml_fse, ml_state);
      uint32_t of_code = rzstd_fse_symbol(&tab->of_fse, of_state);
      uint32_t offset;
      uint32_t lit_run;
      uint32_t match_len;

      if (ll_code >= 36 || ml_code >= 53 || of_code >= 32)
         return RZ_DATA;

      /* Offset first, then match length, then literal length: the
       * extra bits come out in that order. */
      offset    = ((uint32_t)1 << of_code)
                + rzstd_rbits_read(&bits, of_code);
      match_len = rzstd_ml_base[ml_code]
                + rzstd_rbits_read(&bits, rzstd_ml_bits[ml_code]);
      lit_run   = rzstd_ll_base[ll_code]
                + rzstd_rbits_read(&bits, rzstd_ll_bits[ll_code]);

      if (offset > 3)
      {
         offset -= 3;
         repeat[2] = repeat[1];
         repeat[1] = repeat[0];
         repeat[0] = offset;
      }
      else
      {
         uint32_t index = offset - 1;

         /* With no literals, the codes shift: 1 means the second most
          * recent, and 3 means the most recent less one. */
         if (lit_run == 0)
            index++;

         if (index == 3)
            offset = repeat[0] - 1;
         else
            offset = repeat[index];

         if (index)
         {
            uint32_t k = index < 3 ? index : 2;
            if (k == 2)
               repeat[2] = repeat[1];
            repeat[1] = repeat[0];
            repeat[0] = offset;
         }
      }

      if (!offset)
         return RZ_DATA;
      if (lit_at + lit_run > lit_len)
         return RZ_DATA;
      if (out + lit_run + match_len > dst_len)
         return RZ_DATA;
      if ((size_t)offset > out + lit_run)
         return RZ_DATA;

      memcpy(dst + out, literals + lit_at, lit_run);
      out    += lit_run;
      lit_at += lit_run;

      /* The copy may overlap its own source, which is how a short
       * offset expands a repeating pattern, so it goes a byte at a
       * time rather than by memcpy. */
      {
         size_t from = out - offset;
         uint32_t k;

         for (k = 0; k < match_len; k++)
            dst[out + k] = dst[from + k];
         out += match_len;
      }

      if (i + 1 < nseq)
      {
         ll_state = rzstd_fse_next(&tab->ll_fse, ll_state, &bits);
         ml_state = rzstd_fse_next(&tab->ml_fse, ml_state, &bits);
         of_state = rzstd_fse_next(&tab->of_fse, of_state, &bits);
      }
   }

   /* Whatever literals the sequences did not consume finish the block. */
   if (lit_at < lit_len)
   {
      size_t rest = lit_len - lit_at;

      if (out + rest > dst_len)
         return RZ_DATA;
      memcpy(dst + out, literals + lit_at, rest);
      out += rest;
   }

   *dst_at = out;
   return RZ_OK;
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
/* What survives from one block to the next within a frame: the Huffman
 * table a treeless block reuses, the three sequence tables a repeat
 * mode reuses, and the recent offsets. */
typedef struct rzstd_frame_state
{
   rzstd_huf_t        huf;
   int                huf_valid;
   rzstd_seq_tables_t tables;
   uint32_t           repeat[3];
   uint8_t            literals[RZSTD_BLOCK_MAX];
} rzstd_frame_state_t;

static int rzstd_decode_frame(const uint8_t *src, size_t src_len,
      uint8_t *dst, size_t dst_len, size_t *wrote, size_t *used,
      rzstd_frame_state_t *st)
{
   rzstd_frame_header_t h;
   size_t               at  = 0;
   size_t               out = 0;
   int                  e;

   memset(st, 0, sizeof(*st));
   /* The recent offsets start at 1, 4 and 8 (3.1.1.4). */
   st->repeat[0] = 1;
   st->repeat[1] = 4;
   st->repeat[2] = 8;

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
         {
            /* Compressed: a literals section, then the sequences that
             * weave it together with matches into earlier output. */
            rzstd_literals_t lit;
            size_t           lit_used = 0;
            size_t           before   = out;

            if (src_len < at + b.size)
               return RZ_TRUNCATED;

            e = rzstd_read_literals(&lit, &st->huf, &st->huf_valid,
                  src + at, b.size, st->literals, sizeof(st->literals),
                  &lit_used);
            if (e != RZ_OK)
               return e;

            e = rzstd_decode_sequences(&st->tables,
                  src + at + lit_used, b.size - lit_used,
                  lit.data, lit.size, dst, dst_len, &out, st->repeat);
            if (e != RZ_OK)
               return e;

            if (out - before > RZSTD_BLOCK_MAX)
               return RZ_DATA;
            at += b.size;
            break;
         }
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
   static rzstd_frame_state_t state;

   return rzstd_decode_frame(src, src_len, dst, dst_len, wrote, used,
         &state);
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

/* Test seam: reads the literals section at the head of a compressed
 * block and reports what it found. */
int rzstd_probe_literals(const uint8_t *src, size_t len, uint8_t *out,
      size_t out_len, size_t *got, size_t *used, int *type)
{
   rzstd_literals_t lit;
   static rzstd_huf_t huf;
   int valid = 0;
   int e;

   if (!len)
      return RZ_TRUNCATED;
   *type = src[0] & 3;
   e = rzstd_read_literals(&lit, &huf, &valid, src, len, out, out_len, used);
   if (e != RZ_OK)
      return e;
   *got = lit.size;
   return RZ_OK;
}
