/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_huffman.c).
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

#include <string.h>

#include <encodings/huffman.h>

/* Bits held in the cache before a refill stops. Kept at 25 so a peek of
 * up to 24 bits never needs a second pass, and so the shift used to
 * place an incoming byte stays inside 0..24. */
#define RHUFF_CACHE_MIN 24

/* Length field width in a packed lookup entry. Five bits holds a code
 * length up to 31, comfortably above RHUFF_MAX_BITS. */
#define RHUFF_LOOKUP_LEN_BITS 5
#define RHUFF_LOOKUP_LEN_MASK 0x1f

/* -------- bit reader -------- */

void rhuff_bits_init(rhuff_bits_t *b, const uint8_t *data, size_t size)
{
   if (!b)
      return;

   b->data     = data;
   b->size     = data ? size : 0;
   b->offset   = 0;
   b->used     = 0;
   b->cache    = 0;
   b->bits     = 0;
   b->overflow = 0;
}

/* Pulls bytes in until the cache holds more than a peek can ask for.
 * Bytes past the end contribute zeros; the position still advances, so
 * rhuff_bits_flush() can wind back by whole bytes symmetrically.
 *
 * The byte is placed with a shift rather than read through a wider
 * pointer, which is what keeps this independent of host endianness and
 * of the alignment of @data. */
static void rhuff_bits_fill(rhuff_bits_t *b)
{
   while (b->bits <= RHUFF_CACHE_MIN)
   {
      if (b->offset < b->size)
         b->cache |= (uint32_t)b->data[b->offset] << (RHUFF_CACHE_MIN - b->bits);
      b->offset++;
      b->bits += 8;
   }
}

uint32_t rhuff_bits_peek(rhuff_bits_t *b, int numbits)
{
   if (!b || numbits <= 0)
      return 0;

   if (numbits > RHUFF_CACHE_MIN)
      numbits = RHUFF_CACHE_MIN;

   if (b->bits < numbits)
      rhuff_bits_fill(b);

   return b->cache >> (32 - numbits);
}

void rhuff_bits_remove(rhuff_bits_t *b, int numbits)
{
   if (!b || numbits <= 0)
      return;

   if (numbits > RHUFF_CACHE_MIN)
      numbits = RHUFF_CACHE_MIN;

   /* Refill first so the cache always holds at least what is being
    * taken; that keeps b->bits from going negative and the shift below
    * from being asked for more than 24. */
   if (b->bits < numbits)
      rhuff_bits_fill(b);

   b->cache <<= numbits;
   b->bits   -= numbits;
   b->used   += (uint64_t)numbits;

   if (b->used > (uint64_t)b->size * 8)
      b->overflow = 1;
}

uint32_t rhuff_bits_read(rhuff_bits_t *b, int numbits)
{
   uint32_t value = rhuff_bits_peek(b, numbits);
   rhuff_bits_remove(b, numbits);
   return value;
}

size_t rhuff_bits_flush(rhuff_bits_t *b)
{
   if (!b)
      return 0;

   while (b->bits >= 8 && b->offset > 0)
   {
      b->offset--;
      b->bits -= 8;
   }

   b->bits  = 0;
   b->cache = 0;

   if (b->offset > b->size)
      b->offset = b->size;

   return b->offset;
}

int rhuff_bits_overflow(const rhuff_bits_t *b)
{
   if (!b)
      return 1;
   return b->overflow;
}

/* -------- canonical decoder -------- */

int rhuff_dec_init(rhuff_dec_t *d, uint32_t num_codes, uint32_t max_bits,
      uint16_t *lookup, size_t lookup_entries)
{
   if (!d || !lookup)
      return RHUFF_ERROR_PARAM;
   if (num_codes == 0 || num_codes > RHUFF_MAX_CODES)
      return RHUFF_ERROR_PARAM;
   if (max_bits == 0 || max_bits > RHUFF_MAX_BITS)
      return RHUFF_ERROR_PARAM;
   if (lookup_entries < RHUFF_LOOKUP_ENTRIES(max_bits))
      return RHUFF_ERROR_PARAM;
   /* A lookup entry has to hold the index above the length field. */
   if (num_codes > (uint32_t)(0xffff >> RHUFF_LOOKUP_LEN_BITS) + 1)
      return RHUFF_ERROR_PARAM;

   d->lookup         = lookup;
   d->lookup_entries = lookup_entries;
   d->num_codes      = num_codes;
   d->max_bits       = max_bits;

   memset(d->lengths, 0, sizeof(d->lengths));

   return RHUFF_OK;
}

int rhuff_dec_build(rhuff_dec_t *d)
{
   uint32_t histo[RHUFF_MAX_BITS + 1];
   uint16_t codes[RHUFF_MAX_CODES];
   uint32_t curstart;
   uint32_t i;
   int      len;

   if (!d || !d->lookup)
      return RHUFF_ERROR_PARAM;

   memset(histo, 0, sizeof(histo));

   for (i = 0; i < d->num_codes; i++)
   {
      if ((uint32_t)d->lengths[i] > d->max_bits)
         return RHUFF_ERROR_DATA;
      histo[d->lengths[i]]++;
   }

   /* Assign the first code of each length, working from the longest
    * length down and halving as the length shrinks. An odd running
    * total at any length above one cannot be halved into the next
    * shorter length, which means the lengths do not describe a
    * complete tree.
    *
    * Note this is not RFC 1951's ordering: walking down and halving
    * gives the lowest codes to the longest lengths, where walking up
    * and doubling gives them to the shortest. rhuff_test.c confirms
    * both produce valid prefix codes and that they disagree on
    * essentially every non-trivial tree. */
   curstart = 0;
   for (len = (int)d->max_bits; len > 0; len--)
   {
      uint32_t total = curstart + histo[len];

      if (len != 1 && (total & 1) != 0)
         return RHUFF_ERROR_DATA;

      histo[len] = curstart;
      curstart   = total >> 1;
   }

   for (i = 0; i < d->num_codes; i++)
   {
      codes[i] = 0;
      if (d->lengths[i] > 0)
         codes[i] = (uint16_t)(histo[d->lengths[i]]++);
   }

   for (i = 0; i < d->lookup_entries; i++)
      d->lookup[i] = 0;

   for (i = 0; i < d->num_codes; i++)
   {
      uint32_t shift;
      uint32_t dest;
      uint32_t count;
      uint32_t j;
      uint16_t value;

      if (d->lengths[i] == 0)
         continue;

      shift = d->max_bits - d->lengths[i];
      dest  = (uint32_t)codes[i] << shift;
      count = (uint32_t)1 << shift;
      value = (uint16_t)(((uint32_t)i << RHUFF_LOOKUP_LEN_BITS)
            | (uint32_t)d->lengths[i]);

      if ((size_t)dest + (size_t)count > d->lookup_entries)
         return RHUFF_ERROR_DATA;

      for (j = 0; j < count; j++)
         d->lookup[dest + j] = value;
   }

   return RHUFF_OK;
}

uint32_t rhuff_dec_decode_one(rhuff_dec_t *d, rhuff_bits_t *b)
{
   uint32_t peeked;
   uint16_t value;
   int      length;

   if (!d || !b || !d->lookup)
      return 0;

   peeked = rhuff_bits_peek(b, (int)d->max_bits);
   value  = d->lookup[peeked];
   length = (int)(value & RHUFF_LOOKUP_LEN_MASK);

   /* A zero entry is a bit pattern the tree does not cover, which only
    * happens on malformed input. Consume one bit anyway so a caller
    * looping until it has enough symbols still terminates, and latch
    * the overflow flag so it can tell the result apart from a real
    * decode. */
   if (length == 0)
   {
      b->overflow = 1;
      rhuff_bits_remove(b, 1);
      return 0;
   }

   rhuff_bits_remove(b, length);

   return (uint32_t)(value >> RHUFF_LOOKUP_LEN_BITS);
}

/* -------- tree serialisations -------- */

/* The encodings below are specific to the CHD container. See
 * formats/chd/FORMAT.md; every constant here is derived from observed
 * bytes rather than recalled, and tools/chd/chd_probe.py regenerates
 * the images the derivation rests on. */

/* Width of one stored length value, scaled to how long a code may be.
 * Both branches that occur in practice are confirmed against real data:
 * four bits for the CHD hunk map, whose tree is sixteen codes of at most
 * eight bits, and five for an A/V hunk's video, whose three trees are
 * 272 codes of at most sixteen. The three-bit branch is unverified; no
 * format in this tree uses a ceiling below eight. */
static uint32_t rhuff_rle_value_bits(uint32_t max_bits)
{
   if (max_bits >= 16)
      return 5;
   if (max_bits >= 8)
      return 4;
   return 3;
}

int rhuff_read_tree_rle(rhuff_dec_t *d, rhuff_bits_t *b)
{
   uint32_t numbits;
   uint32_t curnode;

   if (!d || !b)
      return RHUFF_ERROR_PARAM;

   numbits = rhuff_rle_value_bits(d->max_bits);

   memset(d->lengths, 0, sizeof(d->lengths));

   /* Lengths are stored one per code in order. A value of one is an
    * escape rather than a length: the value after it is the real
    * length, and if that is also one the pair encodes a single length
    * of one. Otherwise a third value carries a repeat count, biased by
    * three, and that many codes take the length. */
   curnode = 0;
   while (curnode < d->num_codes)
   {
      uint32_t value = rhuff_bits_read(b, (int)numbits);

      if (value != 1)
         d->lengths[curnode++] = (uint8_t)value;
      else
      {
         value = rhuff_bits_read(b, (int)numbits);

         if (value == 1)
            d->lengths[curnode++] = 1;
         else
         {
            uint32_t repeat = rhuff_bits_read(b, (int)numbits) + 3;

            while (repeat > 0 && curnode < d->num_codes)
            {
               d->lengths[curnode++] = (uint8_t)value;
               repeat--;
            }
         }
      }

      if (rhuff_bits_overflow(b))
         return RHUFF_ERROR_DATA;
   }

   /* No alignment follows the tree: the first code of the body starts
    * on the very next bit. Confirmed across six images whose trees
    * differ in shape. */

   return rhuff_dec_build(d);
}

/* Alphabet of the tree that codes the main tree's lengths: one escape
 * plus one symbol per representable length. Its own lengths are stored
 * as fixed three-bit values, so a stored 7 cannot be a length and is
 * used to end the list. */
#define RHUFF_SMALL_CODES  24
#define RHUFF_SMALL_BITS    6
#define RHUFF_SMALL_WIDTH   3
#define RHUFF_SMALL_END     7

int rhuff_read_tree_packed(rhuff_dec_t *d, rhuff_bits_t *b)
{
   /* The small tree is small enough to keep on the stack, which is why
    * this needs no scratch from the caller. */
   uint16_t     small_lookup[1 << RHUFF_SMALL_BITS];
   rhuff_dec_t  small;
   uint32_t     start;
   uint32_t     index;
   uint32_t     numcodes;
   uint32_t     runbits;
   uint32_t     curcode;
   uint32_t     last;
   int          err;

   if (!d || !b)
      return RHUFF_ERROR_PARAM;

   if ((err = rhuff_dec_init(&small, RHUFF_SMALL_CODES, RHUFF_SMALL_BITS,
               small_lookup, RHUFF_LOOKUP_ENTRIES(RHUFF_SMALL_BITS)))
         != RHUFF_OK)
      return err;

   /* The first length stands alone; the value after it says which index
    * the rest resume at, so leading unused symbols cost nothing. */
   small.lengths[0] = (uint8_t)rhuff_bits_read(b, RHUFF_SMALL_WIDTH);
   start            = rhuff_bits_read(b, RHUFF_SMALL_WIDTH) + 1;

   for (index = start; index < RHUFF_SMALL_CODES; index++)
   {
      uint32_t value = rhuff_bits_read(b, RHUFF_SMALL_WIDTH);

      if (value == RHUFF_SMALL_END)
         break;

      small.lengths[index] = (uint8_t)value;
   }

   if (rhuff_bits_overflow(b))
      return RHUFF_ERROR_DATA;

   if ((err = rhuff_dec_build(&small)) != RHUFF_OK)
      return err;

   /* Width of the extended run count, scaled to the alphabet being
    * described rather than fixed, so this holds for the 256-symbol
    * alphabet of 'huff' and for the others an A/V hunk uses. */
   numcodes = d->num_codes;
   runbits  = 0;
   {
      uint32_t span = (numcodes > 9) ? (numcodes - 9) : 1;
      while (span)
      {
         span >>= 1;
         runbits++;
      }
   }

   memset(d->lengths, 0, sizeof(d->lengths));

   curcode = 0;
   last    = 0;

   while (curcode < d->num_codes)
   {
      uint32_t value = rhuff_dec_decode_one(&small, b);

      if (rhuff_bits_overflow(b))
         return RHUFF_ERROR_DATA;

      if (value != 0)
      {
         /* Lengths are stored one above their value, leaving zero free
          * to mean a run. */
         last                 = value - 1;
         d->lengths[curcode++] = (uint8_t)last;
      }
      else
      {
         uint32_t count = rhuff_bits_read(b, 3) + 2;

         if (count == 9)
            count += rhuff_bits_read(b, (int)runbits);

         while (count > 0 && curcode < d->num_codes)
         {
            d->lengths[curcode++] = (uint8_t)last;
            count--;
         }
      }
   }

   return rhuff_dec_build(d);
}

int rhuff_decode_block(rhuff_dec_t *d, rhuff_bits_t *b,
      uint8_t *dst, size_t dst_len)
{
   size_t i;
   int    err;

   if (!d || !b || !dst)
      return RHUFF_ERROR_PARAM;

   if ((err = rhuff_read_tree_packed(d, b)) != RHUFF_OK)
      return err;

   for (i = 0; i < dst_len; i++)
      dst[i] = (uint8_t)rhuff_dec_decode_one(d, b);

   if (rhuff_bits_overflow(b))
      return RHUFF_ERROR_DATA;

   return RHUFF_OK;
}
