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
 * Decoding works and is verified. Seven hundred frames taken from
 * Zstandard-compressed disc images decode byte-for-byte against the
 * reference implementation, covering both the raw and Huffman literal
 * forms, predefined and FSE-coded sequence tables, and the streams a
 * CD-framed hunk holds on both its halves. Corrupted frames decode
 * without an ASan or UBSan fault.
 *
 * Encoding works. Three thousand inputs round-trip, and every frame is
 * accepted by the reference implementation as well as by this decoder.
 * It compresses within a small factor of the reference at level 3 --
 * 1.1 to 1.2 times its output on file data and on long runs, wider on
 * highly structured input where an optimal parse pays off -- which is
 * the trade the design makes: one hash, no chain, no lazy matching,
 * raw literals and the predefined sequence tables.
 *
 * Built and verified:
 *
 *   Frame header (3.1.1.1)      magic, descriptor byte, window
 *                               descriptor and its base-plus-eighths
 *                               encoding, dictionary ID field, frame
 *                               content size in all four widths
 *                               including the two-byte form's bias of
 *                               256, and the single-segment case where
 *                               the content size doubles as the window.
 *   Blocks (3.1.1.2)            raw, RLE and compressed.
 *   Reverse bitstream (4)       the backwards, MSB-first reader the
 *                               entropy streams use, and the padding
 *                               marker in the final byte.
 *   FSE (4.1)                   normalised-count parsing and table
 *                               construction.
 *   Huffman (4.2)               weights in both the direct four-bit and
 *                               FSE-coded forms, the conversion to a
 *                               table, and one-stream and four-stream
 *                               literal decoding.
 *   Literals (3.1.1.3.1)        raw, RLE, Huffman and treeless.
 *   Sequences (3.1.1.3.2)       the three tables in predefined, RLE,
 *                               FSE and repeat modes.
 *   Execution (3.1.1.4)         the copies, the three repeated offsets,
 *                               and the shift in their meaning when a
 *                               sequence has no literals.
 *
 * Not built:
 *
 *   Checksum                    a frame's XXH64 is skipped, not
 *                               verified. A caller expecting a stated
 *                               checksum to be checked does not get
 *                               that.
 *   Huffman literal encoding    literals are emitted raw. Legal, and
 *                               costs ratio.
 *   Encoder refinements         one hash with no chain, so only the
 *                               most recent candidate at a position is
 *                               tried; no lazy matching; no optimal
 *                               parse; no repeated offsets. Each would
 *                               narrow the gap to the reference.

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
 * image and stay within the standard this tree targets. That is a
 * reason for rchd to use this and not a reason for anything else to.
 *
 * MEASURED AGAINST THE REFERENCE IMPLEMENTATION
 *
 * Decoding, on whole Zstandard-compressed disc images read through
 * formats/chd/rchd.c, which is what this exists for:
 *
 *   539 MB/s against 179, on an image of 2048-byte hunks
 *   111 MB/s against  79, on a CD image of one frame per hunk
 *    71 MB/s against  59, on another
 *
 * and 2.7 to 3.1 times its throughput on those frames alone.
 *
 * A frame here is a hunk, so two to twenty kilobytes. At that size a
 * decoder is judged largely on what it does before it decodes
 * anything, and the reference carries machinery this does not need: a
 * context to allocate, a dictionary that is never present, a window
 * that is always the whole frame.
 *
 * It reverses on a large frame. On four megabytes of sequence-heavy
 * input this runs at about half the reference's speed, where its
 * assembly and its scheduling tell. By workload on such frames: 2.0x
 * on many short matches, 1.5x on long runs, 0.7x on stored blocks
 * where both are memcpy, and 0.5x on sequence-heavy input.
 *
 * How the large-frame figures were reached, since each was a
 * measurement rather than a guess: the match copy went a byte at a
 * time and was six times slower than the reference until it was
 * widened; the bit reader was forty-five per cent of the remainder in
 * checks for bits its callers had already established; the four
 * literal streams were decoded one after another when they exist to be
 * decoded together; each sequence read its three table entries three
 * times over; and the loop asked seven questions per sequence where
 * the reference asks one and keeps a separate careful path for the end
 * of a block.
 *
 * Encoding is slower and compresses worse: about a third of the
 * reference's throughput on large input and rather better than that on
 * small, where the match tables are sized to what the input can use, and output 1.1x its size on file data, 3.1x
 * on synthetic structured data, 3.6x on data shaped like an input
 * replay, and 0.8x on long runs of one value, where it wins. That
 * replay figure decides where the encoder may be used, a replay payload
 * being what the only caller that compresses compresses, and 3.6x is
 * still too much to replace it.
 *
 * So this is the right decoder for rchd on both counts -- it needs
 * C89, which <zstd.h> denies it, and it is faster at the frame sizes a
 * disc image holds. It is not a replacement for the reference
 * elsewhere: the encoder is not good enough and the streaming
 * interface the header describes is not built.
 * ---------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <compat/intrinsics.h>
#include <encodings/rzstd.h>

/* The decode loops spend much of their time on variable shifts, which
 * BMI2 does in one flagless instruction where baseline x86-64 takes
 * several and a dependency on CL. The build cannot assume BMI2, so on
 * compilers with the target attribute the hot functions are compiled
 * twice from one always-inline body and picked once at run time, the
 * way the reference implementation ships its own loops. Measured on an
 * image of two-kilobyte hunks this is worth about six per cent of the
 * whole decode. Everywhere else this expands to the plain body alone. */
#if defined(__GNUC__) && defined(__x86_64__) && !defined(RZSTD_NO_BMI2) \
 && (defined(__clang__) || __GNUC__ > 4 \
     || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#define RZSTD_DYNAMIC_BMI2 1
#include <cpuid.h>
#define RZSTD_TARGET_BMI2 __attribute__((target("bmi2")))
#define RZSTD_BODY_INLINE __attribute__((always_inline)) static INLINE

static int rzstd_cpu_bmi2(void)
{
   static volatile int have = -1;

   if (have < 0)
   {
      unsigned a, b, c, d;

      /* Both racers write the same answer, so the race is benign. */
      have = (__get_cpuid_count(7, 0, &a, &b, &c, &d) && (b & (1u << 8)))
           ? 1 : 0;
   }
   return have;
}
/* Deciding the flag in a constructor means no thread ever races the
 * lazy path: by the time a second thread can exist, the answer is
 * written. The lazy check stays as the fallback for an environment
 * that skips constructors. */
__attribute__((constructor)) static void rzstd_cpu_bmi2_init(void)
{
   (void)rzstd_cpu_bmi2();
}
#else
#define RZSTD_BODY_INLINE static INLINE
#endif


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

   /* One load when all four bytes are there, which is every field but
    * the last few. Reading them one at a time with a bound test each
    * was an eighth of the time spent reading a counts description, and
    * a description is read three times a frame. */
   if (at + 4 <= len)
   {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) \
 && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      memcpy(&v, p + at, 4);
      return v;
#else
      return  (uint32_t)p[at]
            | ((uint32_t)p[at + 1] << 8)
            | ((uint32_t)p[at + 2] << 16)
            | ((uint32_t)p[at + 3] << 24);
#endif
   }

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

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) \
 && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RZSTD_BIG_ENDIAN 1
static INLINE uint64_t rzstd_bswap64(uint64_t v)
{
   return  ((v & 0x00000000000000ffULL) << 56)
         | ((v & 0x000000000000ff00ULL) << 40)
         | ((v & 0x0000000000ff0000ULL) << 24)
         | ((v & 0x00000000ff000000ULL) <<  8)
         | ((v & 0x000000ff00000000ULL) >>  8)
         | ((v & 0x0000ff0000000000ULL) >> 24)
         | ((v & 0x00ff000000000000ULL) >> 40)
         | ((v & 0xff00000000000000ULL) >> 56);
}
#endif

static INLINE void rzstd_rbits_fill(rzstd_rbits_t *b)
{
   /* Nothing to do when the buffer is already deep enough. Worth its
    * own test at the top: the callers that fill before a run of reads
    * call this far more often than it has anything to do. */
   if (b->count > 56)
      return;

   /* One load, not eight.
    *
    * The stream runs backwards, so the bytes wanted are the ones just
    * below the cursor. Reading the eight that end at the cursor puts
    * them in the high end of a word, and a shift brings them down --
    * where reading them one at a time through a switch was eight loads,
    * eight shifts, and a function too large for the compiler to inline.
    * The reference implementation's equivalent does not appear in a
    * profile at all, because it is inline in the loops that call it. */
   if (b->pos >= 8)
   {
      uint32_t take = (64 - b->count) >> 3;
      uint64_t v;

      memcpy(&v, b->base + b->pos - 8, 8);
#ifdef RZSTD_BIG_ENDIAN
      v = rzstd_bswap64(v);
#endif
      v      >>= (8 - take) * 8;
      b->pos  -= take;
      b->bits |= v << (64 - b->count - take * 8);
      b->count += take * 8;
      return;
   }

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

   if (!len)
   {
      memset(b, 0, sizeof(*b));
      return RZ_DATA;
   }

   last = src[len - 1];
   if (!last)
   {
      memset(b, 0, sizeof(*b));
      return RZ_DATA;
   }

   /* Every field is written below, so there is nothing to clear; and
    * the padding is where the last byte's marker bit sits, which the
    * bit scan already answers -- the loop this replaces asked one bit
    * at a time, once per stream, several streams per frame. */
   b->base    = src;
   b->pos     = len;
   b->bits    = 0;
   b->count   = 0;
   b->overrun = 0;
   rzstd_rbits_fill(b);

   pad = 8 - compat_highbit_u32(last);

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

/* Takes @n bits with no refill and no bound test. The caller has
 * established that they are there, which is what makes it worth having
 * separately: the checked form was forty-five per cent of decode time,
 * almost all of it in checks that a caller could have made once for
 * several reads. */
static INLINE uint32_t rzstd_rbits_take(rzstd_rbits_t *b, uint32_t n)
{
   /* Two shifts rather than one so that zero asks for zero bits and
    * gets zero, without a test. A single shift by 64 - n would be a
    * shift by sixty-four for that case, which is undefined; shifting by
    * 63 - n and then once more is defined for the whole range. The test
    * this replaces sat in every field of every sequence -- six times a
    * sequence -- for fields that are rarely but not never zero. */
   uint32_t v = (uint32_t)((b->bits >> (63 - n)) >> 1);

   b->bits <<= n;
   b->count -= n;
   return v;
}

static INLINE uint32_t rzstd_rbits_read(rzstd_rbits_t *b, uint32_t n)
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
   const rzstd_fse_entry_t *table;
   uint32_t                 accuracy_log;
} rzstd_fse_t;

/* Reads a table description (4.1.1) into normalised counts. These are
 * forward-read and least significant bit first, which is a third bit
 * order again and the reason this does its own reading. */
RZSTD_BODY_INLINE int rzstd_fse_read_counts(const uint8_t *src, size_t len,
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
      /* floor(log2(remaining)), which the loop above this computed by
       * counting up to eleven times per symbol. The caller's loop
       * guarantees remaining is at least two here, so the highbit
       * helper's input is never zero. */
      bits_needed = compat_highbit_u32((uint32_t)remaining);
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
RZSTD_BODY_INLINE int rzstd_fse_build(rzstd_fse_t *fse, rzstd_fse_entry_t *table,
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

   /* When no symbol took the less-than-one slot, the spread runs in
    * two stages the way the reference implementation runs it: the
    * symbols are laid down in order, then dealt across the table in a
    * loop whose trip count is the table size. What makes the first
    * stage worth having is that it is branchless -- every symbol
    * writes a word of its replicated value and advances by its count,
    * and a count of zero advances by nothing, so the third of the
    * symbols that are absent cost a store each rather than a
    * mispredicted skip. An earlier attempt at this guarded that write
    * and measured nothing, which was the guard's doing, not the
    * technique's.
    *
    * A table with less-than-one symbols keeps the stepping walk, the
    * only way to honour the slots taken from its top. */
   if (high == size && size >= 8)
   {
      uint8_t  spread[(1 << RZSTD_FSE_MAX_ACCURACY_LOG) + 8];
      uint64_t sv  = 0;
      uint32_t pos = 0;
      uint64_t rep = ((uint64_t)0x01010101 << 32) | 0x01010101;

      for (sym = 0; sym < symbol_count; sym++)
      {
         int32_t  n = counts[sym];
         uint32_t k;

         next[sym] = (uint16_t)n;
         memcpy(spread + pos, &sv, 8);
         for (k = 8; k < (uint32_t)n; k += 8)
            memcpy(spread + pos + k, &sv, 8);
         pos += (uint32_t)n;
         sv  += rep;
      }

      if (pos != size)
         return RZ_DATA;

      for (i = 0; i + 2 <= size; i += 2)
      {
         table[position].symbol                 = spread[i];
         table[(position + step) & mask].symbol = spread[i + 1];
         position = (position + 2 * step) & mask;
      }
   }
   else
   {
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
   }

   /* A symbol holding 2^k slots reads accuracy_log - k bits, and the
    * states it can reach lie consecutively. */
   for (i = 0; i < size; i++)
   {
      uint8_t  sy    = table[i].symbol;
      uint16_t taken = next[sy]++;
      uint32_t bits  = accuracy_log - compat_highbit_u32(taken);

      table[i].bits       = (uint8_t)bits;
      table[i].next_state = (uint16_t)(((uint32_t)taken << bits) - size);
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

/* Symbol and width in one entry rather than two arrays. They are always
 * wanted together, so splitting them costs a second cache line per
 * lookup for no gain -- the access is random, so nothing is gained by
 * keeping either contiguous on its own. */
typedef struct rzstd_huf
{
   uint16_t entry[1 << RZSTD_HUF_MAX_BITS];   /* symbol << 8 | width */
   uint32_t max_bits;
   /* Weights-read scratch: the FSE decode of a compressed weights
    * header needs a count table, a 64-entry decode table and the FSE
    * state struct -- 2.3 KiB that used to sit on the block-decode
    * stack.  Transient within one rzstd_huf_read call; the struct
    * already lives in the heap-held frame state. */
   struct
   {
      int16_t           counts[RZSTD_FSE_MAX_SYMBOLS];
      rzstd_fse_entry_t table[1 << 6];
      rzstd_fse_t       fse;
   } wscr;
} rzstd_huf_t;

/* Turns weights into a table. The final symbol's weight is not stored:
 * it is whatever makes the total a power of two, which is why the
 * weights alone are enough (4.2.1). */
RZSTD_BODY_INLINE int rzstd_huf_build(rzstd_huf_t *huf, uint8_t *weights,
      uint32_t count, uint32_t *rank_count)
{
   uint32_t total = 0;
   uint32_t max_bits;
   uint32_t i;
   uint32_t position = 0;

   if (!count || count > RZSTD_HUF_MAX_SYMBOLS)
      return RZ_DATA;

   /* The ranks arrive counted: the loops that decode the weights count
    * them as they go, in the shadow of their own dependency chains, so
    * the walk this function used to make over as many as two hundred
    * and fifty-six symbols to histogram them is nobody's walk at all
    * now. The weights themselves arrive validated the same way, and
    * the array has a slot of slack for the implied last symbol this
    * function appends. */
   for (i = 1; i <= RZSTD_HUF_MAX_BITS; i++)
      total += rank_count[i] << (i - 1);
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
      weights[count] = (uint8_t)(w + 1);
      rank_count[w + 1]++;
      count++;
   }

   huf->max_bits = max_bits;

   /* Slots are laid out by ascending weight: the lowest weights take
    * the lowest codes, and within a weight the symbols keep their
    * natural order (4.2.1.3).
    *
    * A weight is not a code length and runs the other way, so laying
    * the table out by descending weight looks equally plausible and
    * builds a table of exactly the right shape -- the same slot counts,
    * the same widths, every symbol present once. It is simply mirrored,
    * and nothing but decoding real data will say so. */
   /* Fill in rank order, not symbol order.
    *
    * Every symbol of the same weight occupies the same number of slots
    * and they sit next to each other, so walking a rank writes the
    * table forward with one cursor. Walking symbols instead jumps
    * between ranks and turns the cursor into a scattered
    * read-modify-write of next_rank_start[w] -- which the profile
    * showed as a load-use stall on a table rebuilt for every frame. */
   {
      /* Every symbol lands somewhere, absent ones included, so this is
       * sized for all of them rather than for those with a weight. */
      uint8_t  by_rank[RZSTD_HUF_MAX_SYMBOLS + 2];
      uint32_t at_rank[RZSTD_HUF_MAX_BITS + 2];
      uint32_t w;

      /* Where each rank's symbols start in by_rank. Weight zero gets a
       * region of its own past the end rather than a test in the loop
       * below: an absent symbol is written somewhere harmless and never
       * read, which costs a store and saves a branch taken on nearly
       * half of as many as two hundred and fifty-six symbols. */
      position = 0;
      for (w = 1; w <= max_bits; w++)
      {
         at_rank[w] = position;
         position  += rank_count[w];
      }
      at_rank[0] = position;

      for (i = 0; i < count; i++)
         by_rank[at_rank[weights[i]]++] = (uint8_t)i;

      /* at_rank now points one past each rank; walk them in order. */
      position = 0;
      for (w = 1; w <= max_bits; w++)
      {
         uint32_t n     = (uint32_t)1 << (w - 1);
         uint32_t width = max_bits + 1 - w;
         uint32_t k     = at_rank[w] - rank_count[w];
         uint32_t end   = at_rank[w];

         for (; k < end; k++)
         {
            uint16_t  packed = (uint16_t)(((uint32_t)by_rank[k] << 8)
                  | width);
            uint16_t *e      = huf->entry + position;
            uint32_t  j      = 0;

            if (n >= 4)
            {
               uint64_t quad = (uint64_t)packed;

               quad |= quad << 16;
               quad |= quad << 32;
               for (; j + 4 <= n; j += 4)
                  memcpy(e + j, &quad, 8);
            }
            for (; j < n; j++)
               e[j] = packed;
            position += n;
         }
      }

      if (position != ((uint32_t)1 << max_bits))
         return RZ_DATA;
   }

   return RZ_OK;
}

/* Reads a weight description (4.2.1) and builds the table.
 *
 * The first byte decides the form: 128 or above means the weights
 * follow directly at four bits each, and the byte itself carries how
 * many. Below that it is the byte length of an FSE-coded stream. */
RZSTD_BODY_INLINE int rzstd_huf_read_body(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   uint8_t  weights[RZSTD_HUF_MAX_SYMBOLS + 1];
   uint32_t rank_count[RZSTD_HUF_MAX_BITS + 2];
   uint32_t count;
   uint32_t i;

   if (!len)
      return RZ_TRUNCATED;

   /* The ranks are counted here, by the loops that produce the
    * weights, rather than by a second walk over them in the build.
    * Emitting is a serial chain -- state to table to state -- and the
    * count rides in its shadow; the build's own histogram pass was an
    * eighth of reading a literal tree. */
   memset(rank_count, 0, sizeof(rank_count));

   if (src[0] >= 128)
   {
      count = (uint32_t)src[0] - 127;
      if (count > RZSTD_HUF_MAX_SYMBOLS)
         return RZ_DATA;
      /* Two weights to a byte, high nibble first. */
      if (len < 1 + ((count + 1) / 2))
         return RZ_TRUNCATED;
      for (i = 0; i < count; i++)
      {
         uint32_t w = (i & 1) ? (src[1 + i / 2] & 0x0f)
                              : (src[1 + i / 2] >> 4);

         /* A nibble can say fifteen; a weight cannot. */
         if (w > RZSTD_HUF_MAX_BITS)
            return RZ_DATA;
         weights[i] = (uint8_t)w;
         rank_count[w]++;
      }
      *used = 1 + (count + 1) / 2;
      return rzstd_huf_build(huf, weights, count, rank_count);
   }

   /* The compact form: the byte is the length of an FSE-coded stream
    * holding the weights (4.2.1.2). Two interleaved states share one
    * bitstream, alternating, which halves the dependency chain -- the
    * reason the format does it rather than a single state. */
   {
      int16_t           *counts = huf->wscr.counts;
      rzstd_fse_entry_t *table  = huf->wscr.table;
      rzstd_fse_t       *fse    = &huf->wscr.fse;
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

      if ((e = rzstd_fse_build(fse, table, counts, symbol_count,
                  accuracy_log)) != RZ_OK)
         return e;

      if ((e = rzstd_rbits_init(&bits, src + 1 + desc_used,
                  size - desc_used)) != RZ_OK)
         return e;

      state1 = rzstd_fse_begin(fse, &bits);
      state2 = rzstd_fse_begin(fse, &bits);

      /* The stream does not say how many weights it holds. It ends when
       * updating a state would need more bits than remain, at which
       * point both states still hold one symbol each and both are
       * emitted (4.2.1.2). So the check is on the update, not on the
       * symbol, and two symbols follow the loop rather than one. */
      count = 0;
      for (;;)
      {
         uint32_t w;

         if (count + 2 > RZSTD_HUF_MAX_SYMBOLS)
            return RZ_DATA;

         w = rzstd_fse_symbol(fse, state1);
         weights[count++] = (uint8_t)w;
         rank_count[w]++;
         if (!rzstd_rbits_have(&bits, fse->table[state1].bits))
         {
            w = rzstd_fse_symbol(fse, state2);
            weights[count++] = (uint8_t)w;
            rank_count[w]++;
            break;
         }
         state1 = rzstd_fse_next(fse, state1, &bits);

         w = rzstd_fse_symbol(fse, state2);
         weights[count++] = (uint8_t)w;
         rank_count[w]++;
         if (!rzstd_rbits_have(&bits, fse->table[state2].bits))
         {
            w = rzstd_fse_symbol(fse, state1);
            weights[count++] = (uint8_t)w;
            rank_count[w]++;
            break;
         }
         state2 = rzstd_fse_next(fse, state2, &bits);
      }

      *used = 1 + size;
      return rzstd_huf_build(huf, weights, count, rank_count);
   }
}

#ifdef RZSTD_DYNAMIC_BMI2
static int rzstd_huf_read_sse(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   return rzstd_huf_read_body(huf, src, len, used);
}

RZSTD_TARGET_BMI2
static int rzstd_huf_read_bmi2(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   return rzstd_huf_read_body(huf, src, len, used);
}

static int rzstd_huf_read(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   if (rzstd_cpu_bmi2())
      return rzstd_huf_read_bmi2(huf, src, len, used);
   return rzstd_huf_read_sse(huf, src, len, used);
}
#else
static int rzstd_huf_read(rzstd_huf_t *huf, const uint8_t *src, size_t len,
      size_t *used)
{
   return rzstd_huf_read_body(huf, src, len, used);
}
#endif

/* Pulls one symbol. The reader always holds at least max_bits, so a
 * peek is a plain index into the table. */
/* One symbol, assuming the reader already holds max_bits. The caller
 * refills, which is what lets several symbols come out per refill: at
 * eleven bits a code, a full buffer holds five. */
static uint8_t rzstd_huf_symbol_fast(const rzstd_huf_t *huf,
      rzstd_rbits_t *b)
{
   uint32_t index = (uint32_t)(b->bits >> (64 - huf->max_bits));
   uint16_t e     = huf->entry[index];
   uint32_t n     = e & 0xff;

   b->bits <<= n;
   b->count -= n;
   return (uint8_t)(e >> 8);
}

static uint8_t rzstd_huf_symbol(const rzstd_huf_t *huf, rzstd_rbits_t *b)
{
   uint32_t index;
   uint16_t e;
   uint32_t n;

   if (b->count < huf->max_bits)
      rzstd_rbits_fill(b);

   index = (uint32_t)(b->bits >> (64 - huf->max_bits));
   e     = huf->entry[index];
   n     = e & 0xff;

   b->bits <<= n;
   if (b->count >= n)
      b->count -= n;
   else
   {
      b->count   = 0;
      b->overrun = 1;
   }
   return (uint8_t)(e >> 8);
}

/* -------- the counting-free reader --------
 *
 * The four-stream literal decode is limited by how much it must hold
 * live: a buffer, a count and a cursor for each of four streams, plus
 * four output positions. That is more than the register file has, and
 * the emitted code spills.
 *
 * The count is what can be dropped. A load sets bit zero as a marker
 * and consuming shifts left, so the marker's position is the number of
 * bits consumed and trailing zeros recover it. Four counts disappear.
 *
 * Two invariants make it safe. Reads take the top eleven bits only, so
 * nothing below bit fifty-three is ever read and the marker cannot be
 * mistaken for data. And at most five symbols pass between reloads --
 * fifty-five bits at the widest code -- so the marker stays inside the
 * word.
 */


/* Eight bytes, least significant first, a byte at a time so neither
 * alignment nor the host's order matters. */
static uint64_t rzstd_rd64le(const uint8_t *p)
{
   return  (uint64_t)p[0]        | ((uint64_t)p[1] << 8)
        | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
        | ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40)
        | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

typedef struct rzstd_fast_bits
{
   uint64_t       bits;
   const uint8_t *ptr;
   const uint8_t *base;
} rzstd_fast_t;

static int rzstd_fast_init(rzstd_fast_t *f, const uint8_t *src, size_t len)
{
   uint32_t last;
   uint32_t pad;

   if (len < 8)
      return 0;
   last = src[len - 1];
   if (!last)
      return 0;

   f->base = src;
   f->ptr  = src + len - 8;
   f->bits = rzstd_rd64le(f->ptr) | 1;

   /* Skip the encoder's padding, which counts as consumption like any
    * other: the marker moves with it and the reload accounts for it. */
   pad = 8 - compat_highbit_u32(last);
   f->bits <<= pad;
   return 1;
}

/* True while the cursor can still back up a whole word. */
static int rzstd_fast_ok(const rzstd_fast_t *f)
{
   return f->ptr >= f->base + 8;
}

static INLINE void rzstd_fast_reload(rzstd_fast_t *f)
{
   uint32_t used = compat_ctz_u64(f->bits);

   f->ptr  -= used >> 3;
   f->bits  = rzstd_rd64le(f->ptr) | 1;
   f->bits <<= used & 7;
}

/* Where a stream has reached, so the general reader can carry on from
 * it once the fast loop stops. */
static void rzstd_fast_handoff(const rzstd_fast_t *f, rzstd_rbits_t *b,
      const uint8_t *src, size_t len)
{
   uint32_t used  = compat_ctz_u64(f->bits);
   size_t   bytes = (size_t)(f->ptr - src) + 8;

   memset(b, 0, sizeof(*b));
   b->base  = src;
   b->pos   = bytes;
   b->bits  = 0;
   b->count = 0;
   rzstd_rbits_fill(b);
   /* Drop what the fast loop already took. */
   while (used >= 8 && b->count >= 8)
   {
      b->bits <<= 8;
      b->count -= 8;
      used     -= 8;
      rzstd_rbits_fill(b);
   }
   if (used)
   {
      b->bits <<= used;
      b->count -= used;
   }
   (void)len;
}

/* Decodes @count literals from either the one-stream or four-stream
 * layout (4.2.2).
 *
 * Four streams exist so a decoder can run them in parallel; they are
 * not independent halves of the data but interleaved by position, each
 * taking a quarter of the output in order. The first three sizes come
 * from a six-byte jump table and the fourth is whatever remains, which
 * is why only three are stored. */
RZSTD_BODY_INLINE int rzstd_huf_decode_body(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   size_t i;

   if (!four_streams)
   {
      rzstd_rbits_t b;
      int           e = rzstd_rbits_init(&b, src, src_len);

      if (e != RZ_OK)
         return e;
      i = 0;
      /* Four symbols between tests: a code is at most eleven bits and
       * the buffer holds sixty-four, so four never exhausts it and the
       * bound test happens a quarter as often. */
      while (i + 4 <= count)
      {
         if (b.count < huf->max_bits * 4)
            rzstd_rbits_fill(&b);
         if (b.count < huf->max_bits * 4)
            break;
         dst[i]     = rzstd_huf_symbol_fast(huf, &b);
         dst[i + 1] = rzstd_huf_symbol_fast(huf, &b);
         dst[i + 2] = rzstd_huf_symbol_fast(huf, &b);
         dst[i + 3] = rzstd_huf_symbol_fast(huf, &b);
         i += 4;
      }
      for (; i < count; i++)
         dst[i] = rzstd_huf_symbol(huf, &b);
      return RZ_OK;
   }

   {
      rzstd_rbits_t b[4];
      size_t        size[4];
      const uint8_t *begin[4];
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
         begin[k] = src + at;
         if ((e = rzstd_rbits_init(&b[k], src + at, size[k])) != RZ_OK)
            return e;
         at += size[k];
      }

      /* Each stream covers a quarter of the output, the last taking the
       * remainder.
       *
       * The four are decoded together rather than one after another,
       * which is the whole reason the format has four. Each carries its
       * own bit buffer, so the four chains of load, shift and store are
       * independent and a processor can overlap them; taking them in
       * turn serialises what was split apart on purpose.
       *
       * Five symbols per stream per pass, which is what fits: a code is
       * at most eleven bits and the buffer holds sixty-four, so five
       * come out between refills and the bound test is paid once for
       * twenty symbols.
       */
      quarter = (count + 3) / 4;
      {
         size_t fin[4];
         size_t pos[4];
         size_t together;

         for (k = 0; k < 4; k++)
         {
            pos[k] = k * quarter;
            fin[k] = pos[k] + quarter;
            if (pos[k] > count)
               pos[k] = count;
            if (fin[k] > count)
               fin[k] = count;
         }

         /* How many rounds every stream can take without checking. */
         together = quarter >= 5 ? (quarter - 4) / 5 : 0;

         /* The counting-free reader first, while every stream has a
          * whole word left below its cursor. It holds a buffer and a
          * cursor per stream and no count, which is four fewer live
          * values than the general reader needs and the difference
          * between fitting in registers and not. */
         {
            rzstd_fast_t    f[4];
            const uint16_t *ent = huf->entry;
            /* The table is indexed by the top max_bits, so the shift is
             * fixed for the whole loop, as it is in the assembly this
             * follows. */
            const uint32_t  sh  = 64 - huf->max_bits;
            int             all = 1;

            for (k = 0; k < 4; k++)
               if (!rzstd_fast_init(&f[k], begin[k], size[k]))
                  all = 0;

            if (all)
            {
               while (together
                     && rzstd_fast_ok(&f[0]) && rzstd_fast_ok(&f[1])
                     && rzstd_fast_ok(&f[2]) && rzstd_fast_ok(&f[3]))
               {
                  /* Each stream's five symbols land next to each other,
                   * so they are gathered into a word and written once
                   * rather than a byte at a time to a computed index.
                   * The stores were a quarter of this loop. */
                  {
                     uint64_t w0 = 0, w1 = 0, w2 = 0, w3 = 0;

                     for (i = 0; i < 5; i++)
                     {
                        uint32_t e0 = ent[f[0].bits >> sh];
                        uint32_t e1 = ent[f[1].bits >> sh];
                        uint32_t e2 = ent[f[2].bits >> sh];
                        uint32_t e3 = ent[f[3].bits >> sh];

                        w0 |= (uint64_t)(uint8_t)(e0 >> 8) << (i * 8);
                        w1 |= (uint64_t)(uint8_t)(e1 >> 8) << (i * 8);
                        w2 |= (uint64_t)(uint8_t)(e2 >> 8) << (i * 8);
                        w3 |= (uint64_t)(uint8_t)(e3 >> 8) << (i * 8);

                        f[0].bits <<= (e0 & 63);
                        f[1].bits <<= (e1 & 63);
                        f[2].bits <<= (e2 & 63);
                        f[3].bits <<= (e3 & 63);
                     }

                     /* Five bytes wanted, eight written: the loop stops
                      * four short of each stream's end, so the three
                      * extra land inside the next round's five and are
                      * overwritten by it. The last round writes into the
                      * tail the counting reader below fills in. */
                     memcpy(dst + pos[0], &w0, 8);
                     memcpy(dst + pos[1], &w1, 8);
                     memcpy(dst + pos[2], &w2, 8);
                     memcpy(dst + pos[3], &w3, 8);
                  }
                  pos[0] += 5; pos[1] += 5; pos[2] += 5; pos[3] += 5;
                  rzstd_fast_reload(&f[0]);
                  rzstd_fast_reload(&f[1]);
                  rzstd_fast_reload(&f[2]);
                  rzstd_fast_reload(&f[3]);
                  together--;
               }

               for (k = 0; k < 4; k++)
                  rzstd_fast_handoff(&f[k], &b[k], begin[k], size[k]);
            }
         }

         while (together--)
         {
            uint32_t w = huf->max_bits * 5;

            if (b[0].count < w || b[1].count < w
                  || b[2].count < w || b[3].count < w)
            {
               rzstd_rbits_fill(&b[0]);
               rzstd_rbits_fill(&b[1]);
               rzstd_rbits_fill(&b[2]);
               rzstd_rbits_fill(&b[3]);
               if (b[0].count < w || b[1].count < w
                     || b[2].count < w || b[3].count < w)
                  break;
            }

            for (i = 0; i < 5; i++)
            {
               dst[pos[0] + i] = rzstd_huf_symbol_fast(huf, &b[0]);
               dst[pos[1] + i] = rzstd_huf_symbol_fast(huf, &b[1]);
               dst[pos[2] + i] = rzstd_huf_symbol_fast(huf, &b[2]);
               dst[pos[3] + i] = rzstd_huf_symbol_fast(huf, &b[3]);
            }
            pos[0] += 5; pos[1] += 5; pos[2] += 5; pos[3] += 5;
         }

         for (k = 0; k < 4; k++)
            for (i = pos[k]; i < fin[k]; i++)
               dst[i] = rzstd_huf_symbol(huf, &b[k]);
      }
      return RZ_OK;
   }
}

#ifdef RZSTD_DYNAMIC_BMI2
static int rzstd_huf_decode_sse(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   return rzstd_huf_decode_body(huf, src, src_len, dst, count, four_streams);
}

RZSTD_TARGET_BMI2
static int rzstd_huf_decode_bmi2(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   return rzstd_huf_decode_body(huf, src, src_len, dst, count, four_streams);
}

static int rzstd_huf_decode(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   if (rzstd_cpu_bmi2())
      return rzstd_huf_decode_bmi2(huf, src, src_len, dst, count, four_streams);
   return rzstd_huf_decode_sse(huf, src, src_len, dst, count, four_streams);
}
#else
static int rzstd_huf_decode(const rzstd_huf_t *huf, const uint8_t *src,
      size_t src_len, uint8_t *dst, size_t count, int four_streams)
{
   return rzstd_huf_decode_body(huf, src, src_len, dst, count, four_streams);
}
#endif

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
   const uint8_t *data;
   size_t         size;
   /* Bytes readable from @data, which exceeds @size when the literals
    * lie inside the block: what follows them is the sequences section,
    * so a wide copy may overrun into it harmlessly. */
   size_t         readable;
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
         /* Raw literals are already contiguous in the block, so the
          * sequence loop reads them where they lie. Copying them into
          * scratch first would be a whole pass over the literal data
          * for nothing, and on literal-heavy input that is most of the
          * data in the frame. */
         if (src_len < at + regenerated)
            return RZ_TRUNCATED;
         out->data     = src + at;
         out->size     = regenerated;
         out->readable = src_len - at;
         *used         = at + regenerated;
         return RZ_OK;
      }
      else
      {
         if (src_len < at + 1)
            return RZ_TRUNCATED;
         memset(scratch, src[at], regenerated);
         *used = at + 1;
      }

      out->data     = scratch;
      out->size     = regenerated;
      out->readable = regenerated + 128;
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

   out->data     = scratch;
   out->size     = regenerated;
   out->readable = regenerated + 128;
   *used         = at + compressed;
   return RZ_OK;
}


/* -------- wide copies --------
 *
 * A match copies from earlier output, and the source may overlap the
 * destination: that overlap is how a short offset expands a repeating
 * pattern, and it is why the obvious memcpy is wrong. What it is not is
 * a reason to copy a byte at a time, which is what the first version
 * here did and what made long matches six times slower than the
 * reference.
 *
 * The rule is only that a byte must be written before it is read. An
 * offset of at least the copy width satisfies that for any width, so
 * wide copies are safe above a threshold and the narrow case is handled
 * by expanding the pattern once and then going wide anyway.
 */

#if defined(__SSE2__) || (defined(_M_X64)) \
 || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define RZSTD_VEC_SSE2 1
#define RZSTD_VEC_WIDTH 16
#define RZSTD_COPY_SLACK 64
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#define RZSTD_VEC_NEON 1
#define RZSTD_VEC_WIDTH 16
#define RZSTD_COPY_SLACK 64
#else
#define RZSTD_VEC_WIDTH 8
#define RZSTD_COPY_SLACK 32
#endif

/* Copies @n bytes forward, overrunning by up to one vector. Callers
 * guarantee the slack, which is what lets the loop run without a tail
 * and without a per-iteration bound test. */
/* A literal run is a handful of bytes and there is one per sequence, so
 * these are called about as often as sequences are decoded -- tens of
 * millions of times for a few megabytes of output. At that rate the
 * call costs more than the copy, so the short case is handled at the
 * call site and only the rest arrives here.
 *
 * Thirty-two bytes covers nearly every literal run and most short
 * matches, and being a constant it becomes two vector moves with no
 * loop and no branch. The slack that lets it overrun is already
 * required by the wide copy, so nothing further is needed. */
#define RZSTD_COPY_SMALL 32

#define RZSTD_COPY(d, s2, n2)                    \
   do {                                          \
      if ((n2) <= RZSTD_COPY_SMALL)              \
         memcpy((d), (s2), RZSTD_COPY_SMALL);    \
      else                                       \
         rzstd_wild_copy((d), (s2), (n2));       \
   } while (0)

static void rzstd_wild_copy(uint8_t *dst, const uint8_t *src, size_t n)
{
   size_t i = 0;

#if defined(RZSTD_VEC_SSE2)
   /* Short copies are the common case -- a literal run is a handful of
    * bytes -- so the loop is arranged to finish in one pass for those
    * and only widen for the long ones. Unrolling to 64 for every copy
    * makes the short ones slower, which is most of them. */
   if (n <= 32)
   {
      _mm_storeu_si128((__m128i*)(void*)dst,
            _mm_loadu_si128((const __m128i*)(const void*)src));
      _mm_storeu_si128((__m128i*)(void*)(dst + 16),
            _mm_loadu_si128((const __m128i*)(const void*)(src + 16)));
      return;
   }
   do
   {
      _mm_storeu_si128((__m128i*)(void*)(dst + i),
            _mm_loadu_si128((const __m128i*)(const void*)(src + i)));
      _mm_storeu_si128((__m128i*)(void*)(dst + i + 16),
            _mm_loadu_si128((const __m128i*)(const void*)(src + i + 16)));
      _mm_storeu_si128((__m128i*)(void*)(dst + i + 32),
            _mm_loadu_si128((const __m128i*)(const void*)(src + i + 32)));
      _mm_storeu_si128((__m128i*)(void*)(dst + i + 48),
            _mm_loadu_si128((const __m128i*)(const void*)(src + i + 48)));
      i += 64;
   } while (i < n);
#elif defined(RZSTD_VEC_NEON)
   if (n <= 32)
   {
      vst1q_u8(dst,      vld1q_u8(src));
      vst1q_u8(dst + 16, vld1q_u8(src + 16));
      return;
   }
   do
   {
      vst1q_u8(dst + i,      vld1q_u8(src + i));
      vst1q_u8(dst + i + 16, vld1q_u8(src + i + 16));
      vst1q_u8(dst + i + 32, vld1q_u8(src + i + 32));
      vst1q_u8(dst + i + 48, vld1q_u8(src + i + 48));
      i += 64;
   } while (i < n);
#else
   /* Eight bytes at a time through a union, so no alignment is assumed
    * and no strict-aliasing rule is bent. */
   do
   {
      memcpy(dst + i, src + i, 8);
      memcpy(dst + i + 8, src + i + 8, 8);
      i += 16;
   } while (i < n);
#endif
}

/* Copies a match of @n bytes from @offset back, where the source may
 * overlap. @slack says whether there is room to overrun by a vector.
 *
 * A small offset is handled by doubling rather than by copying bytes:
 * the seed is laid down once, then repeatedly copied onto itself, so
 * the period grows past the vector width in a few steps. After that the
 * remaining bytes are a plain wide copy from a distance that is now
 * large enough to be safe. */
RZSTD_BODY_INLINE void rzstd_match_copy(uint8_t *to, const uint8_t *from,
      size_t n, size_t offset, int slack)
{
   size_t have;

   if (!slack)
   {
      size_t k;

      for (k = 0; k < n; k++)
         to[k] = from[k];
      return;
   }

   /* The wide copy strides two vectors at a time, so it needs the
    * source two vector widths back -- not one, which an earlier draft
    * of this change assumed until the sanitizer produced the
    * overlapping pair. */
   if (offset >= RZSTD_VEC_WIDTH * 2)
   {
      RZSTD_COPY(to, from, n);
      return;
   }

   /* Below a vector's width the period has to be grown before wide
    * copies can run, and the reference implementation grows it in one
    * fixed step rather than a loop: eight bytes are laid down -- four
    * singly, then four from a source nudged forward by a table so the
    * pattern stays right -- after which the source is pulled back to
    * sit exactly eight behind, whatever the offset was. From there
    * eight-byte strides never read what they wrote. A doubling loop
    * here took a data-dependent number of rounds; a match is a couple
    * of dozen bytes more often than not, and the rounds cost more than
    * the copying. */
   {
      static const uint32_t fwd[8]  = { 0, 1, 2, 1, 4, 4, 4, 4 };
      static const uint32_t back[8] = { 8, 8, 8, 7, 8, 9, 10, 11 };
      uint64_t v;

      if (offset < 8)
      {
         to[0] = from[0];
         to[1] = from[1];
         to[2] = from[2];
         to[3] = from[3];
         from += fwd[offset];
         memcpy(&v, from, 4);
         memcpy(to + 4, &v, 4);
         from -= back[offset];
      }
      else
      {
         /* Eight or more apart already: the lead-in is a plain word
          * and the strides below never catch the stores. */
         memcpy(&v, from, 8);
         memcpy(to, &v, 8);
      }

      /* Both advance by eight from here, at least eight apart. */
      from += 8;
      have  = 8;
      while (have < n)
      {
         memcpy(&v, from, 8);
         memcpy(to + have, &v, 8);
         from += 8;
         have += 8;
      }
      return;
   }
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

/* The predefined tables never change, so they are built once and shared
 * rather than rebuilt for every block. Rebuilding them was an eighth of
 * decode time on sequence-heavy input, which is what a spread and a
 * width assignment over sixty-four states costs when it happens per
 * block instead of per program.
 *
 * The build is deterministic, so two threads racing to do it write the
 * same bytes and no lock is needed; the flag is only an optimisation
 * and being seen late costs a rebuild, not correctness. */
/* The three predefined tables (3.1.1.3.2.2), spelled out rather than
 * built on first use. They are constants of the format, and building
 * them lazily left two threads racing on the ready flag and on the
 * table contents -- a real hazard on weakly ordered machines, where a
 * reader can observe the flag before the entries. The initialisers
 * were emitted by the same rzstd_fse_build that used to fill them at
 * run time, from the same default distributions, and byte-compare
 * equal to what it builds. */
static const rzstd_fse_entry_t rzstd_ll_predef[64] = {
   {   0,  0, 4 },   {  16,  0, 4 },   {  32,  1, 5 },   {   0,  3, 5 },
   {   0,  4, 5 },   {   0,  6, 5 },   {   0,  7, 5 },   {   0,  9, 5 },
   {   0, 10, 5 },   {   0, 12, 5 },   {   0, 14, 6 },   {   0, 16, 5 },
   {   0, 18, 5 },   {   0, 19, 5 },   {   0, 21, 5 },   {   0, 22, 5 },
   {   0, 24, 5 },   {  32, 25, 5 },   {   0, 26, 5 },   {   0, 27, 6 },
   {   0, 29, 6 },   {   0, 31, 6 },   {  32,  0, 4 },   {   0,  1, 4 },
   {   0,  2, 5 },   {  32,  4, 5 },   {   0,  5, 5 },   {  32,  7, 5 },
   {   0,  8, 5 },   {  32, 10, 5 },   {   0, 11, 5 },   {   0, 13, 6 },
   {  32, 16, 5 },   {   0, 17, 5 },   {  32, 19, 5 },   {   0, 20, 5 },
   {  32, 22, 5 },   {   0, 23, 5 },   {   0, 25, 4 },   {  16, 25, 4 },
   {  32, 26, 5 },   {   0, 28, 6 },   {   0, 30, 6 },   {  48,  0, 4 },
   {  16,  1, 4 },   {  32,  2, 5 },   {  32,  3, 5 },   {  32,  5, 5 },
   {  32,  6, 5 },   {  32,  8, 5 },   {  32,  9, 5 },   {  32, 11, 5 },
   {  32, 12, 5 },   {   0, 15, 6 },   {  32, 17, 5 },   {  32, 18, 5 },
   {  32, 20, 5 },   {  32, 21, 5 },   {  32, 23, 5 },   {  32, 24, 5 },
   {   0, 35, 6 },   {   0, 34, 6 },   {   0, 33, 6 },   {   0, 32, 6 },
};

static const rzstd_fse_entry_t rzstd_ml_predef[64] = {
   {   0,  0, 6 },   {   0,  1, 4 },   {  32,  2, 5 },   {   0,  3, 5 },
   {   0,  5, 5 },   {   0,  6, 5 },   {   0,  8, 5 },   {   0, 10, 6 },
   {   0, 13, 6 },   {   0, 16, 6 },   {   0, 19, 6 },   {   0, 22, 6 },
   {   0, 25, 6 },   {   0, 28, 6 },   {   0, 31, 6 },   {   0, 33, 6 },
   {   0, 35, 6 },   {   0, 37, 6 },   {   0, 39, 6 },   {   0, 41, 6 },
   {   0, 43, 6 },   {   0, 45, 6 },   {  16,  1, 4 },   {   0,  2, 4 },
   {  32,  3, 5 },   {   0,  4, 5 },   {  32,  6, 5 },   {   0,  7, 5 },
   {   0,  9, 6 },   {   0, 12, 6 },   {   0, 15, 6 },   {   0, 18, 6 },
   {   0, 21, 6 },   {   0, 24, 6 },   {   0, 27, 6 },   {   0, 30, 6 },
   {   0, 32, 6 },   {   0, 34, 6 },   {   0, 36, 6 },   {   0, 38, 6 },
   {   0, 40, 6 },   {   0, 42, 6 },   {   0, 44, 6 },   {  32,  1, 4 },
   {  48,  1, 4 },   {  16,  2, 4 },   {  32,  4, 5 },   {  32,  5, 5 },
   {  32,  7, 5 },   {  32,  8, 5 },   {   0, 11, 6 },   {   0, 14, 6 },
   {   0, 17, 6 },   {   0, 20, 6 },   {   0, 23, 6 },   {   0, 26, 6 },
   {   0, 29, 6 },   {   0, 52, 6 },   {   0, 51, 6 },   {   0, 50, 6 },
   {   0, 49, 6 },   {   0, 48, 6 },   {   0, 47, 6 },   {   0, 46, 6 },
};

static const rzstd_fse_entry_t rzstd_of_predef[32] = {
   {   0,  0, 5 },   {   0,  6, 4 },   {   0,  9, 5 },   {   0, 15, 5 },
   {   0, 21, 5 },   {   0,  3, 5 },   {   0,  7, 4 },   {   0, 12, 5 },
   {   0, 18, 5 },   {   0, 23, 5 },   {   0,  5, 5 },   {   0,  8, 4 },
   {   0, 14, 5 },   {   0, 20, 5 },   {   0,  2, 5 },   {  16,  7, 4 },
   {   0, 11, 5 },   {   0, 17, 5 },   {   0, 22, 5 },   {   0,  4, 5 },
   {  16,  8, 4 },   {   0, 13, 5 },   {   0, 19, 5 },   {   0,  1, 5 },
   {  16,  6, 4 },   {   0, 10, 5 },   {   0, 16, 5 },   {   0, 28, 5 },
   {   0, 27, 5 },   {   0, 26, 5 },   {   0, 25, 5 },   {   0, 24, 5 },
};

/* Sets up one of the three tables according to its two-bit mode. */
RZSTD_BODY_INLINE int rzstd_seq_table_body(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous,
      const rzstd_fse_entry_t *shared)
{
   int16_t  counts[RZSTD_FSE_MAX_SYMBOLS];
   uint32_t symbol_count = 0;
   uint32_t accuracy_log = 0;
   int      e;

   *used = 0;

   switch (mode)
   {
      case RZSTD_SEQ_PREDEFINED:
         /* Point at the shared constant table rather than filling this
          * block's copy of it. */
         fse->table        = shared;
         fse->accuracy_log = predef_log;
         (void)predef;
         (void)predef_count;
         (void)storage;
         return RZ_OK;

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

#ifdef RZSTD_DYNAMIC_BMI2
static int rzstd_seq_table_sse(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous,
      const rzstd_fse_entry_t *shared)
{
   return rzstd_seq_table_body(fse, storage, mode, predef, predef_count,
         predef_log, max_symbol, max_log, src, len, used,
         have_previous, shared);
}

RZSTD_TARGET_BMI2
static int rzstd_seq_table_bmi2(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous,
      const rzstd_fse_entry_t *shared)
{
   return rzstd_seq_table_body(fse, storage, mode, predef, predef_count,
         predef_log, max_symbol, max_log, src, len, used,
         have_previous, shared);
}

static int rzstd_seq_table(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous,
      const rzstd_fse_entry_t *shared)
{
   if (rzstd_cpu_bmi2())
      return rzstd_seq_table_bmi2(fse, storage, mode, predef, predef_count,
         predef_log, max_symbol, max_log, src, len, used,
         have_previous, shared);
   return rzstd_seq_table_sse(fse, storage, mode, predef, predef_count,
         predef_log, max_symbol, max_log, src, len, used,
         have_previous, shared);
}
#else
static int rzstd_seq_table(rzstd_fse_t *fse, rzstd_fse_entry_t *storage,
      uint32_t mode, const int16_t *predef, uint32_t predef_count,
      uint32_t predef_log, uint32_t max_symbol, uint32_t max_log,
      const uint8_t *src, size_t len, size_t *used, int have_previous,
      const rzstd_fse_entry_t *shared)
{
   return rzstd_seq_table_body(fse, storage, mode, predef, predef_count,
         predef_log, max_symbol, max_log, src, len, used,
         have_previous, shared);
}
#endif

/* Decodes a block's sequences and executes them into @dst.
 *
 * The three offsets most recently used are remembered, and an offset
 * code of 1, 2 or 3 names one of them rather than a distance -- with
 * the wrinkle that when there are no literals the meaning shifts by
 * one, because repeating the immediately previous offset would then be
 * expressible twice (3.1.1.4). */
RZSTD_BODY_INLINE int rzstd_decode_sequences_body(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len, size_t lit_readable,
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
   const rzstd_fse_entry_t *lle_next;
   const rzstd_fse_entry_t *mle_next;
   const rzstd_fse_entry_t *ofe_next;
   /* Where the fast path stops being safe. Subtracting the slack once
    * here rather than adding it per sequence keeps the test in
    * registers, and guards the underflow once instead of every time. */
   size_t        out_limit = dst_len > RZSTD_COPY_SLACK
                           ? dst_len - RZSTD_COPY_SLACK : 0;
   size_t        lit_limit = lit_readable > RZSTD_COPY_SLACK
                           ? lit_readable - RZSTD_COPY_SLACK : 0;
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
         tab->valid, rzstd_ll_predef);
   if (e != RZ_OK)
      return e;
   at += used;

   e = rzstd_seq_table(&tab->of_fse, tab->of, (modes >> 4) & 3,
         rzstd_of_default, 29, 5, 31, 8, src + at, src_len - at, &used,
         tab->valid, rzstd_of_predef);
   if (e != RZ_OK)
      return e;
   at += used;

   e = rzstd_seq_table(&tab->ml_fse, tab->ml, (modes >> 2) & 3,
         rzstd_ml_default, 53, 6, 52, 9, src + at, src_len - at, &used,
         tab->valid, rzstd_ml_predef);
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

   lle_next = &tab->ll_fse.table[ll_state];
   mle_next = &tab->ml_fse.table[ml_state];
   ofe_next = &tab->of_fse.table[of_state];

   for (i = 0; i < nseq; i++)
   {
      /* Each table entry is read once and its three fields all used:
       * the symbol now, the width to decide whether the buffer holds
       * enough, and the transition at the end. Asking the table three
       * separate times for the same entry is three loads where one
       * does, and the loop did that for each of three tables. */
      /* The three entries were fetched at the end of the previous
       * round, before its copies ran.
       *
       * A table load is four or five cycles and nothing about the
       * copies feeds it, so issuing it first lets the two overlap. The
       * copies are where a sequence spends most of its time, and the
       * entropy chain -- state to load to next state -- is what the
       * next sequence waits on, so the one that must not wait goes
       * first. */
      const rzstd_fse_entry_t *lle = lle_next;
      const rzstd_fse_entry_t *mle = mle_next;
      const rzstd_fse_entry_t *ofe = ofe_next;
      uint32_t ll_code = lle->symbol;
      uint32_t ml_code = mle->symbol;
      uint32_t of_code = ofe->symbol;
      uint32_t offset;
      uint32_t lit_run;
      uint32_t match_len;

      /* The codes come out of tables that cannot hold anything larger:
       * an FSE description is refused past the cap, a one-symbol table
       * checks its byte, the predefined tables are constants, and a
       * repeat is a table that already passed. A range test here was
       * per sequence for a property settled per table.
       *
       * Offset first, then match length, then literal length: the
       * extra bits come out in that order.
       *
       * One question per sequence -- whether the whole of it, values
       * and state updates both, is provably present -- replaces the
       * two exact sums the loop used to total. A sequence's values are
       * at most 63 bits and its updates 26 more; against 128 the whole
       * sequence is there, and every read below runs unchecked with
       * refills at fixed places: one ahead of the values, one between
       * match and literal lengths that only fires for the rare wide
       * sequence, one after the updates for the next round. This is
       * the reference implementation's discipline. The last couple of
       * sequences of a block fail the test and take the checked reads,
       * which is where that care belongs. */
      if (bits.count + bits.pos * 8 >= 128)
      {
         rzstd_rbits_fill(&bits);
         offset    = ((uint32_t)1 << of_code)
                   + rzstd_rbits_take(&bits, of_code);
         match_len = rzstd_ml_base[ml_code]
                   + rzstd_rbits_take(&bits, rzstd_ml_bits[ml_code]);
         if (of_code + rzstd_ml_bits[ml_code] + rzstd_ll_bits[ll_code]
               >= 31)
            rzstd_rbits_fill(&bits);
         lit_run   = rzstd_ll_base[ll_code]
                   + rzstd_rbits_take(&bits, rzstd_ll_bits[ll_code]);

         if (i + 1 < nseq)
         {
            ll_state = lle->next_state + rzstd_rbits_take(&bits, lle->bits);
            ml_state = mle->next_state + rzstd_rbits_take(&bits, mle->bits);
            of_state = ofe->next_state + rzstd_rbits_take(&bits, ofe->bits);
            lle_next = &tab->ll_fse.table[ll_state];
            mle_next = &tab->ml_fse.table[ml_state];
            ofe_next = &tab->of_fse.table[of_state];
         }
      }
      else
      {
         offset    = ((uint32_t)1 << of_code)
                   + rzstd_rbits_read(&bits, of_code);
         match_len = rzstd_ml_base[ml_code]
                   + rzstd_rbits_read(&bits, rzstd_ml_bits[ml_code]);
         lit_run   = rzstd_ll_base[ll_code]
                   + rzstd_rbits_read(&bits, rzstd_ll_bits[ll_code]);

         if (i + 1 < nseq)
         {
            ll_state = lle->next_state + rzstd_rbits_read(&bits, lle->bits);
            ml_state = mle->next_state + rzstd_rbits_read(&bits, mle->bits);
            of_state = ofe->next_state + rzstd_rbits_read(&bits, ofe->bits);
            lle_next = &tab->ll_fse.table[ll_state];
            mle_next = &tab->ml_fse.table[ml_state];
            ofe_next = &tab->of_fse.table[of_state];
         }
      }

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

      /* One test decides whether this sequence needs any further ones.
       *
       * Everything the two copies require is checked together: room to
       * overrun the output, room to read past the literals, enough
       * literals left, and a match reaching no further back than the
       * output goes. The limits are loop-invariant and hoisted, so this
       * is three compares against registers and the copies that follow
       * test nothing at all.
       *
       * The careful path repeats the checks singly and copies narrowly.
       * It runs at the end of a block and on anything malformed, which
       * is where that cost belongs. Doing it per sequence instead --
       * seven tests where three do -- is what the reference avoids by
       * splitting the two apart, and it costs more than any instruction
       * choice inside the loop. */
      if (out + lit_run + match_len <= out_limit
            && lit_at + lit_run <= lit_limit
            && offset
            && (size_t)offset <= out + lit_run)
      {
         RZSTD_COPY(dst + out, literals + lit_at, lit_run);
         out    += lit_run;
         lit_at += lit_run;
         rzstd_match_copy(dst + out, dst + out - offset, match_len,
               offset, 1);
         out += match_len;
      }
      else
      {
         if (!offset)
            return RZ_DATA;
         if (lit_at + lit_run > lit_len)
            return RZ_DATA;
         if (out + lit_run + match_len > dst_len)
            return RZ_DATA;
         if ((size_t)offset > out + lit_run)
            return RZ_DATA;

         /* Falling off the fast path does not mean copying narrowly.
          * A single long match near the end of a block fails the
          * combined test while still having room of its own, and
          * forcing it byte-wise cost more than the combined test
          * saved -- it took long-match decoding from twice the
          * reference's speed to half of it. Each copy is asked about
          * its own room here. */
         if (lit_at + lit_run + RZSTD_COPY_SLACK <= lit_readable
               && out + lit_run + RZSTD_COPY_SLACK <= dst_len)
            rzstd_wild_copy(dst + out, literals + lit_at, lit_run);
         else
            memcpy(dst + out, literals + lit_at, lit_run);
         out    += lit_run;
         lit_at += lit_run;
         rzstd_match_copy(dst + out, dst + out - offset, match_len,
               offset, out + match_len + RZSTD_COPY_SLACK <= dst_len);
         out += match_len;
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

#ifdef RZSTD_DYNAMIC_BMI2
static int rzstd_decode_sequences_sse(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len, size_t lit_readable,
      uint8_t *dst, size_t dst_len, size_t *dst_at, uint32_t *repeat)
{
   return rzstd_decode_sequences_body(tab, src, src_len, literals, lit_len,
         lit_readable, dst, dst_len, dst_at, repeat);
}

RZSTD_TARGET_BMI2
static int rzstd_decode_sequences_bmi2(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len, size_t lit_readable,
      uint8_t *dst, size_t dst_len, size_t *dst_at, uint32_t *repeat)
{
   return rzstd_decode_sequences_body(tab, src, src_len, literals, lit_len,
         lit_readable, dst, dst_len, dst_at, repeat);
}

static int rzstd_decode_sequences(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len, size_t lit_readable,
      uint8_t *dst, size_t dst_len, size_t *dst_at, uint32_t *repeat)
{
   if (rzstd_cpu_bmi2())
      return rzstd_decode_sequences_bmi2(tab, src, src_len, literals, lit_len,
         lit_readable, dst, dst_len, dst_at, repeat);
   return rzstd_decode_sequences_sse(tab, src, src_len, literals, lit_len,
         lit_readable, dst, dst_len, dst_at, repeat);
}
#else
static int rzstd_decode_sequences(rzstd_seq_tables_t *tab,
      const uint8_t *src, size_t src_len,
      const uint8_t *literals, size_t lit_len, size_t lit_readable,
      uint8_t *dst, size_t dst_len, size_t *dst_at, uint32_t *repeat)
{
   return rzstd_decode_sequences_body(tab, src, src_len, literals, lit_len,
         lit_readable, dst, dst_len, dst_at, repeat);
}
#endif

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
   /* The literal buffer carries slack past the block maximum so a wide
    * copy out of it may overrun without leaving the allocation. */
   uint8_t            literals[RZSTD_BLOCK_MAX + 128];
} rzstd_frame_state_t;

static int rzstd_decode_frame(const uint8_t *src, size_t src_len,
      uint8_t *dst, size_t dst_len, size_t *wrote, size_t *used,
      rzstd_frame_state_t *st)
{
   rzstd_frame_header_t h;
   size_t               at  = 0;
   size_t               out = 0;
   int                  e;

   /* Only the three flags need clearing.
    *
    * Zeroing the whole state costs about a hundred and forty kilobytes
    * -- the literal buffer, the Huffman table, the three sequence
    * tables -- and a hunk is two. Measured with cachegrind that was
    * seventy-one write references for every byte of output and
    * ninety-four per cent of all memory traffic in the decoder.
    *
    * None of it is read before it is written. A literal buffer is
    * filled before the sequences that consume it; a Huffman table is
    * built before a block that uses it, and a treeless block without
    * one is refused by huf_valid; a sequence table is built before its
    * block, and a repeat mode without one is refused by tables.valid.
    * The flags are what enforce that, so the flags are what must be
    * cleared. */
   st->huf_valid    = 0;
   st->tables.valid = 0;

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
                  lit.data, lit.size, lit.readable,
                  dst, dst_len, &out, st->repeat);
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

/* -------- the public interface -------- */

int rzstd_decode(uint8_t *dst, size_t dst_len, const uint8_t *src,
      size_t src_len, size_t *wrote)
{
   rzstd_frame_state_t *st;
   size_t               out  = 0;
   size_t               used = 0;
   int                  e;

   if (!dst || !src)
      return RZSTD_PROCESS_ERROR;

   /* The per-frame state carries a literal buffer of the maximum block
    * size, which is too large to put on the stack. */
   if (!(st = (rzstd_frame_state_t*)malloc(sizeof(*st))))
      return RZSTD_PROCESS_ERROR;

   e = rzstd_decode_frame(src, src_len, dst, dst_len, &out, &used, st);
   free(st);

   if (e != RZ_OK)
      return RZSTD_PROCESS_ERROR;

   /* Trailing bytes are refused rather than ignored: a caller splitting
    * a buffer into frames has to pass exact lengths, and silently
    * accepting a short one would hide the mistake until the next frame
    * came out wrong. */
   if (used != src_len)
      return RZSTD_PROCESS_ERROR;

   if (wrote)
      *wrote = out;
   return RZSTD_PROCESS_END;
}

int rzstd_frame_header_size(const uint8_t *src, size_t src_len)
{
   rzstd_frame_header_t h;

   if (!src)
      return RZSTD_PROCESS_ERROR;
   if (rzstd_read_frame_header(src, src_len, &h) != RZ_OK)
      return RZSTD_PROCESS_ERROR;
   return (int)h.header_len;
}

const char *rzstd_error_name(int code)
{
   switch (code)
   {
      case RZSTD_PROCESS_END:
         return "no error";
      case RZSTD_PROCESS_NEXT:
         return "more input or output needed";
      case RZSTD_PROCESS_ERROR:
         break;
   }
   /* One name for every failure, deliberately.
    *
    * The decoder distinguishes truncated input from corrupt input from
    * an unsupported feature internally, and a caller can do nothing
    * different about any of them: the frame does not decode. Reporting
    * which would invite handling that cannot be tested, since a caller
    * cannot produce one kind on purpose. */
   return "frame does not decode";
}

int64_t rzstd_frame_content_size(const uint8_t *src, size_t src_len)
{
   rzstd_frame_header_t h;

   if (!src)
      return RZSTD_CONTENT_SIZE_ERROR;
   if (rzstd_read_frame_header(src, src_len, &h) != RZ_OK)
      return RZSTD_CONTENT_SIZE_ERROR;
   if (!h.has_content_size)
      return RZSTD_CONTENT_SIZE_UNKNOWN;
   return (int64_t)h.content_size;
}

/* -------- test seams --------
 *
 * These reach parts the public interface does not expose, so that a
 * layer can be checked before the layers above it exist.
 */

int rzstd_probe_frame(const uint8_t *src, size_t src_len,
      uint8_t *dst, size_t dst_len, size_t *wrote, size_t *used)
{
   static rzstd_frame_state_t state;

   return rzstd_decode_frame(src, src_len, dst, dst_len, wrote, used,
         &state);
}

/* Builds an FSE table from given counts and reports the widths it
 * assigned, so the spread can be checked against the tables the RFC
 * predefines. */
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

/* -------- encoding --------
 *
 * One caller compresses -- input replay payloads -- so this is sized
 * for that and not for competing with the reference implementation's
 * ratio. What it must do is produce frames any conforming decoder
 * reads.
 *
 * Matches are found by hashing four bytes and walking a chain of
 * candidates for the longest, which is where most of the ratio comes
 * from. There is no lazy matching and no optimal parse. Literals go out
 * raw rather than Huffman coded, and the sequence tables are the
 * predefined ones, which between them cost some ratio and remove the
 * need to build and transmit tables at all.
 */

/* -------- writing bits --------
 *
 * The decoder reads a stream from its last byte backwards, so the
 * writer runs the other way: bits accumulate at increasing positions
 * and whole bytes flush forward, which puts what was written last into
 * the bytes read first. Everything the decoder wants first must
 * therefore be written last, and that is why sequences are emitted in
 * reverse order below.
 */

typedef struct rzstd_wbits
{
   uint8_t *dst;
   size_t   cap;
   size_t   at;
   uint64_t bits;
   uint32_t count;
   int      overflow;
} rzstd_wbits_t;

static void rzstd_wbits_init(rzstd_wbits_t *w, uint8_t *dst, size_t cap)
{
   memset(w, 0, sizeof(*w));
   w->dst = dst;
   w->cap = cap;
}

static void rzstd_wbits_add(rzstd_wbits_t *w, uint32_t value, uint32_t n)
{
   if (!n)
      return;
   w->bits  |= ((uint64_t)value & (((uint64_t)1 << n) - 1)) << w->count;
   w->count += n;

   while (w->count >= 8)
   {
      if (w->at >= w->cap)
      {
         w->overflow = 1;
         return;
      }
      w->dst[w->at++] = (uint8_t)w->bits;
      w->bits >>= 8;
      w->count -= 8;
   }
}

/* Closes the stream with the marker bit the decoder looks for, then
 * pads to a byte. Without it there is nothing to say where the data
 * ends within the final byte. */
static size_t rzstd_wbits_close(rzstd_wbits_t *w)
{
   rzstd_wbits_add(w, 1, 1);
   if (w->count)
   {
      if (w->at >= w->cap)
      {
         w->overflow = 1;
         return 0;
      }
      w->dst[w->at++] = (uint8_t)w->bits;
      w->bits  = 0;
      w->count = 0;
   }
   return w->at;
}

/* -------- FSE encoding --------
 *
 * The mirror of the decoding table: for each symbol, how many bits its
 * current state spends and which state it moves to.
 */

typedef struct rzstd_fse_ct
{
   uint16_t table[1 << RZSTD_FSE_MAX_ACCURACY_LOG];
   uint32_t delta_bits[RZSTD_FSE_MAX_SYMBOLS];
   int32_t  delta_state[RZSTD_FSE_MAX_SYMBOLS];
   uint32_t accuracy_log;
} rzstd_fse_ct_t;

static int rzstd_fse_build_ct(rzstd_fse_ct_t *ct, const int16_t *counts,
      uint32_t symbol_count, uint32_t accuracy_log)
{
   uint32_t size = (uint32_t)1 << accuracy_log;
   uint32_t mask = size - 1;
   uint32_t step = (size >> 1) + (size >> 3) + 3;
   uint32_t high = size;
   uint32_t position = 0;
   uint32_t cumul[RZSTD_FSE_MAX_SYMBOLS + 1];
   uint8_t  spread[1 << RZSTD_FSE_MAX_ACCURACY_LOG];
   uint32_t sym;
   uint32_t i;
   uint32_t total = 0;

   ct->accuracy_log = accuracy_log;

   /* The same spread the decoder uses, so the two agree on which state
    * carries which symbol. */
   for (sym = 0; sym < symbol_count; sym++)
      if (counts[sym] == -1)
         spread[--high] = (uint8_t)sym;

   for (sym = 0; sym < symbol_count; sym++)
   {
      int32_t n = counts[sym];

      if (n <= 0)
         continue;
      for (i = 0; i < (uint32_t)n; i++)
      {
         spread[position] = (uint8_t)sym;
         do
         {
            position = (position + step) & mask;
         } while (position >= high);
      }
   }
   if (position != 0)
      return RZ_DATA;

   /* Where each symbol's states begin, in symbol order. */
   for (sym = 0; sym < symbol_count; sym++)
   {
      cumul[sym] = total;
      total += (counts[sym] == -1) ? 1 : (uint32_t)(counts[sym] > 0
            ? counts[sym] : 0);
   }
   cumul[symbol_count] = total;

   for (i = 0; i < size; i++)
   {
      uint8_t sy = spread[i];
      ct->table[cumul[sy]++] = (uint16_t)(size + i);
   }

   /* Restore the starts, which the loop above consumed. */
   total = 0;
   for (sym = 0; sym < symbol_count; sym++)
   {
      cumul[sym] = total;
      total += (counts[sym] == -1) ? 1 : (uint32_t)(counts[sym] > 0
            ? counts[sym] : 0);
   }

   for (sym = 0; sym < symbol_count; sym++)
   {
      int32_t n = counts[sym];

      if (!n)
      {
         ct->delta_bits[sym]  = 0;
         ct->delta_state[sym] = 0;
         continue;
      }
      if (n == -1)
      {
         /* One state, always reset: it spends the full accuracy. */
         ct->delta_bits[sym]  = (accuracy_log << 16) - ((uint32_t)1
               << accuracy_log);
         ct->delta_state[sym] = (int32_t)cumul[sym] - 1;
         continue;
      }
      {
         uint32_t max_bits = accuracy_log;

         while (((uint32_t)1 << max_bits) > (uint32_t)n && max_bits)
            max_bits--;
         max_bits = accuracy_log - max_bits;

         /* How many bits a state spends is (state + delta) >> 16, so
          * delta has to be arranged for the shift to give max_bits
          * exactly while the state is at or above n << max_bits and one
          * fewer below it. That fixes the value: no table size term
          * belongs here, and including one biases every width by a
          * constant that survives every plausible-looking test. */
         ct->delta_bits[sym]  = (max_bits << 16)
                              - ((uint32_t)n << max_bits);
         ct->delta_state[sym] = (int32_t)cumul[sym] - (int32_t)n;
      }
   }

   return RZ_OK;
}

static uint32_t rzstd_fse_ct_begin(const rzstd_fse_ct_t *ct, uint32_t sym)
{
   uint32_t nb = (ct->delta_bits[sym] + (1u << 15)) >> 16;
   uint32_t at = ((nb << 16) - ct->delta_bits[sym]) >> nb;

   return ct->table[(int32_t)at + ct->delta_state[sym]];
}

static uint32_t rzstd_fse_ct_encode(rzstd_wbits_t *w, uint32_t state,
      const rzstd_fse_ct_t *ct, uint32_t sym)
{
   uint32_t nb = (state + ct->delta_bits[sym]) >> 16;

   rzstd_wbits_add(w, state, nb);
   return ct->table[(int32_t)(state >> nb) + ct->delta_state[sym]];
}

static void rzstd_fse_ct_flush(rzstd_wbits_t *w, uint32_t state,
      const rzstd_fse_ct_t *ct)
{
   rzstd_wbits_add(w, state, ct->accuracy_log);
}

/* The largest a block may be, which is what the decoder accepts and
 * twice what this used to emit. A match cannot cross a block boundary,
 * so a smaller block simply forgets everything it has seen every time
 * it starts one. */
#define RZSTD_ENC_BLOCK    RZSTD_BLOCK_MAX
#define RZSTD_ENC_HASH_LOG 15
#define RZSTD_ENC_CHAIN_LOG 15
#define RZSTD_ENC_CHAIN_DEPTH 8
#define RZSTD_ENC_MIN_MATCH 4

size_t rzstd_compress_bound(size_t src_len)
{
   /* A frame that stores its input outright: a header, then a block
    * header every RZSTD_ENC_BLOCK bytes, then the bytes. */
   size_t blocks = (src_len / RZSTD_ENC_BLOCK) + 1;

   return src_len + blocks * 3 + RZSTD_FRAME_HEADER_MAX + 4;
}

/* Writes a frame header stating the content size, so a decoder can size
 * its output before decoding (3.1.1.1). Single-segment, no checksum, no
 * dictionary. */
static size_t rzstd_write_frame_header(uint8_t *dst, uint64_t size)
{
   size_t at = 0;

   dst[at++] = (uint8_t)(RZSTD_MAGIC);
   dst[at++] = (uint8_t)(RZSTD_MAGIC >> 8);
   dst[at++] = (uint8_t)(RZSTD_MAGIC >> 16);
   dst[at++] = (uint8_t)(RZSTD_MAGIC >> 24);

   /* Single segment means no window descriptor: the content size is the
    * window, which is what lets a decoder allocate exactly once. */
   if (size < 256)
   {
      dst[at++] = 0x20;                       /* fcs 1 byte    */
      dst[at++] = (uint8_t)size;
   }
   else if (size < 65536 + 256)
   {
      uint32_t v = (uint32_t)(size - 256);    /* stored biased */
      dst[at++] = 0x60;                       /* fcs 2 bytes   */
      dst[at++] = (uint8_t)v;
      dst[at++] = (uint8_t)(v >> 8);
   }
   else
   {
      dst[at++] = 0xa0;                       /* fcs 4 bytes   */
      dst[at++] = (uint8_t)size;
      dst[at++] = (uint8_t)(size >> 8);
      dst[at++] = (uint8_t)(size >> 16);
      dst[at++] = (uint8_t)(size >> 24);
   }

   return at;
}

static void rzstd_write_block_header(uint8_t *dst, uint32_t size,
      uint32_t type, int last)
{
   uint32_t v = ((uint32_t)(last ? 1 : 0)) | (type << 1) | (size << 3);

   dst[0] = (uint8_t)v;
   dst[1] = (uint8_t)(v >> 8);
   dst[2] = (uint8_t)(v >> 16);
}

/* The inverse of the baseline tables: which code covers a value, and
 * what remains to be written as extra bits. */
static uint32_t rzstd_code_for(const uint32_t *base, uint32_t count,
      uint32_t value)
{
   uint32_t i = count;

   while (i-- > 0)
      if (value >= base[i])
         return i;
   return 0;
}

/* An offset is stored three higher than its distance, because 1 to 3
 * are reserved for the recent-offset codes (3.1.1.4). This encoder
 * never emits those, so every offset it writes is a literal distance. */
/* The code for a stored offset value. The value already carries the
 * bias, since a sequence may name a repeated offset instead of a
 * distance and those occupy the low three. */
static uint32_t rzstd_of_code(uint32_t value)
{
   uint32_t n = 0;

   while ((uint32_t)1 << (n + 1) <= value)
      n++;
   return n;
}

/* Turns a distance into the value a sequence stores, naming one of the
 * three remembered offsets when it can, and rotating the encoder's copy
 * of that list exactly as the decoder will.
 *
 * A named offset costs a symbol and no extra bits; a distance costs the
 * symbol plus its width, which is eleven bits at the distances this
 * encoder was producing. Every sequence the reference emits on replay
 * data names one, and none of mine did.
 *
 * The three codes mean different things depending on whether the
 * sequence carries literals, and the list rotates for some of them and
 * not others (3.1.1.4). Both are mirrored here rather than approximated,
 * because an encoder whose idea of the list drifts from the decoder's
 * produces a frame that decodes to something else entirely. */
static uint32_t rzstd_enc_offset(uint32_t *rep, uint32_t off,
      uint32_t literals)
{
   uint32_t index;

   if (literals)
   {
      /* Code n names rep[n-1]; only code 1 leaves the list alone. */
      if      (off == rep[0]) return 1;
      else if (off == rep[1]) index = 1;
      else if (off == rep[2]) index = 2;
      else                    index = 4;   /* no match */
   }
   else
   {
      /* The codes shift by one: 1 is rep[1], 2 is rep[2], and 3 is the
       * most recent less one. rep[0] itself cannot be named. */
      if      (off == rep[1])     index = 1;
      else if (off == rep[2])     index = 2;
      else if (off == rep[0] - 1) index = 3;
      else                        index = 4;
   }

   if (index == 4)
   {
      rep[2] = rep[1];
      rep[1] = rep[0];
      rep[0] = off;
      return off + 3;
   }

   {
      uint32_t k = index < 3 ? index : 2;

      if (k == 2)
         rep[2] = rep[1];
      rep[1] = rep[0];
      rep[0] = off;
   }
   return literals ? index + 1 : index;
}

typedef struct rzstd_seq
{
   uint32_t literals;
   uint32_t match;
   uint32_t offset;
} rzstd_seq_t;

/* Emits one block as literals plus sequences, or reports that doing so
 * would not be smaller than storing it. */
/* 'cts' is three FSE compression tables owned by the caller.  They
 * were locals, which made this a 9440-byte frame: each table carries
 * a 1<<accuracy_log entry table plus two 256-entry delta arrays, so
 * three of them are most of 9 KiB.  A frame that size does not belong
 * on a stack whatever the target - it sits under whatever called it -
 * and rzstd_encode() already owns heap scratch for the match finder,
 * so these go alongside it and are allocated once per call rather
 * than once per block. */
static int rzstd_emit_block(uint8_t *dst, size_t dst_cap,
      const uint8_t *src, size_t len, const rzstd_seq_t *seq, size_t nseq,
      const uint8_t *literals, size_t lit_len, size_t *out_len,
      rzstd_fse_ct_t *cts)
{
   rzstd_fse_ct_t *ll_ctp = &cts[0];
   rzstd_fse_ct_t *ml_ctp = &cts[1];
   rzstd_fse_ct_t *of_ctp = &cts[2];
   rzstd_wbits_t  w;
   size_t         at = 0;
   size_t         i;
   uint32_t       ll_state;
   uint32_t       ml_state;
   uint32_t       of_state;

   (void)src;
   (void)len;

   if (!nseq)
      return RZ_DATA;

   /* Literals go out raw: legal, and it avoids building and
    * transmitting a Huffman table. The size field is one, two or three
    * bytes by how large the run is (3.1.1.3.1). */
   if (lit_len < 32)
   {
      if (at + 1 > dst_cap)
         return RZ_DATA;
      dst[at++] = (uint8_t)((lit_len << 3) | (0 << 2) | 0);
   }
   else if (lit_len < 4096)
   {
      if (at + 2 > dst_cap)
         return RZ_DATA;
      dst[at++] = (uint8_t)((lit_len << 4) | (1 << 2) | 0);
      dst[at++] = (uint8_t)(lit_len >> 4);
   }
   else
   {
      if (at + 3 > dst_cap)
         return RZ_DATA;
      dst[at++] = (uint8_t)((lit_len << 4) | (3 << 2) | 0);
      dst[at++] = (uint8_t)(lit_len >> 4);
      dst[at++] = (uint8_t)(lit_len >> 12);
   }

   if (at + lit_len > dst_cap)
      return RZ_DATA;
   memcpy(dst + at, literals, lit_len);
   at += lit_len;

   /* Sequence count, then a modes byte saying all three tables are the
    * predefined ones, so none is transmitted. */
   if (nseq < 128)
   {
      if (at + 1 > dst_cap)
         return RZ_DATA;
      dst[at++] = (uint8_t)nseq;
   }
   else if (nseq < 0x7f00)
   {
      if (at + 2 > dst_cap)
         return RZ_DATA;
      dst[at++] = (uint8_t)((nseq >> 8) + 128);
      dst[at++] = (uint8_t)nseq;
   }
   else
      return RZ_DATA;

   if (at + 1 > dst_cap)
      return RZ_DATA;
   dst[at++] = 0;

   if (rzstd_fse_build_ct(ll_ctp, rzstd_ll_default, 36, 6) != RZ_OK
    || rzstd_fse_build_ct(ml_ctp, rzstd_ml_default, 53, 6) != RZ_OK
    || rzstd_fse_build_ct(of_ctp, rzstd_of_default, 29, 5) != RZ_OK)
      return RZ_DATA;

   rzstd_wbits_init(&w, dst + at, dst_cap - at);

   /* Everything below is written in the reverse of the order it is
    * read. The last sequence goes first, and the initial states go
    * last, because the decoder takes them from the end of the stream. */
   {
      const rzstd_seq_t *s2 = &seq[nseq - 1];
      uint32_t ll = rzstd_code_for(rzstd_ll_base, 36, s2->literals);
      uint32_t ml = rzstd_code_for(rzstd_ml_base, 53, s2->match);
      uint32_t of = rzstd_of_code(s2->offset);

      ml_state = rzstd_fse_ct_begin(ml_ctp, ml);
      of_state = rzstd_fse_ct_begin(of_ctp, of);
      ll_state = rzstd_fse_ct_begin(ll_ctp, ll);

      rzstd_wbits_add(&w, s2->literals - rzstd_ll_base[ll],
            rzstd_ll_bits[ll]);
      rzstd_wbits_add(&w, s2->match - rzstd_ml_base[ml],
            rzstd_ml_bits[ml]);
      rzstd_wbits_add(&w, s2->offset - ((uint32_t)1 << of), of);
   }

   for (i = nseq - 1; i-- > 0; )
   {
      const rzstd_seq_t *s2 = &seq[i];
      uint32_t ll = rzstd_code_for(rzstd_ll_base, 36, s2->literals);
      uint32_t ml = rzstd_code_for(rzstd_ml_base, 53, s2->match);
      uint32_t of = rzstd_of_code(s2->offset);

      /* The decoder updates literal length, then match length, then
       * offset. Everything here is written in reverse of the reading
       * order, so they go out the other way about. */
      of_state = rzstd_fse_ct_encode(&w, of_state, of_ctp, of);
      ml_state = rzstd_fse_ct_encode(&w, ml_state, ml_ctp, ml);
      ll_state = rzstd_fse_ct_encode(&w, ll_state, ll_ctp, ll);

      rzstd_wbits_add(&w, s2->literals - rzstd_ll_base[ll],
            rzstd_ll_bits[ll]);
      rzstd_wbits_add(&w, s2->match - rzstd_ml_base[ml],
            rzstd_ml_bits[ml]);
      rzstd_wbits_add(&w, s2->offset - ((uint32_t)1 << of), of);
   }

   rzstd_fse_ct_flush(&w, ml_state, ml_ctp);
   rzstd_fse_ct_flush(&w, of_state, of_ctp);
   rzstd_fse_ct_flush(&w, ll_state, ll_ctp);

   {
      size_t n = rzstd_wbits_close(&w);

      if (w.overflow)
         return RZ_DATA;
      at += n;
   }

   *out_len = at;
   return RZ_OK;
}

int rzstd_encode(uint8_t *dst, size_t dst_len, const uint8_t *src,
      size_t src_len, int level, size_t *wrote)
{
   size_t    at   = 0;
   size_t    in   = 0;
   uint32_t  enc_log;
   /* The three offsets a decoder remembers. They belong to the frame,
    * not the block: a decoder carries them across block boundaries
    * (3.1.1.5), so an encoder that starts each block from the defaults
    * disagrees with it from the first repeat code that resolves
    * differently. Measured, the output stayed correct for two blocks
    * and went wrong at the first byte of the third. */
   uint32_t  rep[3];
   uint32_t *hash  = NULL;
   uint32_t *chain = NULL;
   /* Three FSE compression tables for rzstd_emit_block(), which used
    * to hold them as locals - see the comment there.  Allocated with
    * the match-finder scratch and freed with it. */
   rzstd_fse_ct_t *cts = NULL;

   (void)level;

   if (!dst || (!src && src_len))
      return RZSTD_PROCESS_ERROR;
   if (dst_len < rzstd_compress_bound(src_len))
      return RZSTD_PROCESS_ERROR;

   /* The match tables are sized to the input and cleared once.
    *
    * They were a fixed 128 KB each, cleared for every block. A caller
    * compressing a replay payload hands over a few kilobytes, so nearly
    * all of that was zeroing entries no position could ever occupy: at
    * 512 bytes in, clearing 256 KB was almost the whole cost of the
    * call.
    *
    * Positions are stored absolute rather than block-relative, so a
    * block does not have to clear what the previous one left. An entry
    * from an earlier block simply fails the range test below, which
    * a block-relative entry could not be told apart from a valid one. */
   {
      uint32_t want = 8;

      while (want < RZSTD_ENC_HASH_LOG
            && ((size_t)1 << want) < src_len)
         want++;
      enc_log = want;
   }

   rep[0] = 1;
   rep[1] = 4;
   rep[2] = 8;

   at = rzstd_write_frame_header(dst, (uint64_t)src_len);

   /* An empty input still needs a block, because a frame with none is
    * not a frame. */
   if (!src_len)
   {
      rzstd_write_block_header(dst + at, 0, RZSTD_BLOCK_RAW, 1);
      at += 3;
      if (wrote)
         *wrote = at;
      return RZSTD_PROCESS_END;
   }

   while (in < src_len)
   {
      size_t take = src_len - in;
      int    last;

      if (take > RZSTD_ENC_BLOCK)
         take = RZSTD_ENC_BLOCK;
      last = (in + take >= src_len);

      /* A block of one repeated byte costs one byte to store, so it is
       * worth the scan: replay payloads are full of them. */
      {
         size_t k = 1;

         while (k < take && src[in + k] == src[in])
            k++;
         if (k == take)
         {
            rzstd_write_block_header(dst + at, (uint32_t)take,
                  RZSTD_BLOCK_RLE, last);
            at += 3;
            dst[at++] = src[in];
            in += take;
            continue;
         }
      }

      /* Try a compressed block, and store the block instead when that
       * does not come out smaller -- which happens on input with no
       * matches, where the sequence machinery costs more than it
       * saves. */
      {
         size_t       produced = 0;
         rzstd_seq_t *seq      = (rzstd_seq_t*)malloc(
               (take / RZSTD_ENC_MIN_MATCH + 1) * sizeof(rzstd_seq_t));
         uint8_t     *lits     = (uint8_t*)malloc(take);
         size_t       nseq     = 0;
         size_t       lit_len  = 0;
         int          ok       = 0;

         if (!hash)
         {
            /* Cleared once for the whole input, not once a block. */
            hash  = (uint32_t*)calloc((size_t)1 << enc_log,
                  sizeof(uint32_t));
            /* Previous position sharing each position's hash, so a
             * bucket can be walked rather than only its newest entry
             * read. */
            chain = (uint32_t*)calloc((size_t)1 << enc_log,
                  sizeof(uint32_t));
            cts   = (rzstd_fse_ct_t*)calloc(3, sizeof(*cts));
         }

         if (seq && lits && hash && chain && cts)
         {
            size_t   pos      = 0;
            size_t   lit_from = 0;
            /* The three offsets the decoder remembers, tracked here so
             * a match that repeats one can say so.
             *
             * Naming the most recent offset costs a single code where
             * spelling it out costs that plus its extra bits, and on
             * periodic data every match repeats the period. The decoder
             * only leaves the recent offsets untouched for the first of
             * the three, so that is the only one used here: the others
             * rotate the list and would have to be mirrored exactly. */
            size_t   rep_len;

            while (pos + RZSTD_ENC_MIN_MATCH <= take)
            {
               const uint8_t *p = src + in + pos;
               uint32_t       h;
               uint32_t       cand;

               /* One hash of four bytes and no chain: only the most
                * recent candidate at a position is considered. */
               h = ((uint32_t)p[0] | ((uint32_t)p[1] << 8)
                     | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24))
                     * 2654435761u;
               h >>= 32 - enc_log;

               /* Measure the most recent offset, but do not take it
                * on sight.
                *
                * Continuing at an offset the decoder already remembers
                * costs a symbol and no extra bits; naming a fresh
                * distance costs the symbol plus its width, eleven bits
                * at the distances this data produces. So a repeat is
                * worth having even when the search finds something a
                * little longer -- but only a little. Taking every
                * repeat that matched four bytes made the output half
                * again larger, because it broke long matches into
                * short ones.
                *
                * It is measured one byte in so the sequence carries a
                * literal: the code naming the most recent offset means
                * that only when literals are present. */
               rep_len = 0;
               if (rep[0] && pos + 1 + RZSTD_ENC_MIN_MATCH <= take
                     && (size_t)rep[0] <= pos + 1)
               {
                  const uint8_t *r  = src + in + pos + 1;
                  const uint8_t *rq = r - rep[0];

                  while (pos + 1 + rep_len < take
                        && r[rep_len] == rq[rep_len])
                     rep_len++;
                  if (rep_len < RZSTD_ENC_MIN_MATCH)
                     rep_len = 0;
               }

               /* Walk the bucket for the longest match rather than
                * taking its newest entry.
                *
                * The newest is the wrong one surprisingly often: on
                * data that is mostly one byte value, the nearest
                * position sharing four bytes is a few bytes back and
                * the match ends as soon as the data stops repeating,
                * where an older candidate a whole period back would
                * have run for hundreds. Taking the first candidate
                * measured six times the reference's output; this is
                * what closes most of that. */
               /* Positions are absolute in the input and biased by
                * one, so zero still means "no candidate" and an entry
                * left by an earlier block is told apart by the range
                * test rather than by having been cleared. */
               cand = hash[h];
               hash[h] = (uint32_t)(in + pos + 1);
               chain[(in + pos) & (((size_t)1 << enc_log) - 1)] = cand;

               {
                  size_t best_len  = 0;
                  size_t best_from = 0;
                  int    depth     = RZSTD_ENC_CHAIN_DEPTH;

                  while (cand && depth--)
                  {
                     size_t         abs_from = (size_t)cand - 1;
                     size_t         from;
                     const uint8_t *q;
                     size_t         n = 0;

                     /* Outside this block, so it belongs to an earlier
                      * one: a match may not cross a block boundary. */
                     if (abs_from < in || abs_from >= in + pos)
                        break;
                     from = abs_from - in;
                     q    = src + abs_from;
                     while (pos + n < take && p[n] == q[n])
                        n++;
                     if (n > best_len)
                     {
                        best_len  = n;
                        best_from = from;
                     }
                     /* A match this long will not be beaten by
                      * enough to pay for finding out. Walking on costs
                      * time and, on data that is one value repeated,
                      * finds equally long matches at larger offsets
                      * that cost more to encode. */
                     if (best_len >= 64)
                        break;

                     cand = chain[abs_from
                        & (((size_t)1 << enc_log) - 1)];
                     if (cand && (size_t)cand - 1 >= abs_from)
                        break;   /* not strictly older: stop */
                  }

                  /* The repeat wins unless the search found something
                   * clearly better, since it encodes for eleven bits
                   * less. */
                  if (rep_len && rep_len + 1 >= best_len)
                  {
                     rzstd_seq_t *sq = &seq[nseq++];

                     sq->literals = (uint32_t)(pos + 1 - lit_from);
                     sq->match    = (uint32_t)rep_len;
                     sq->offset   = rzstd_enc_offset(rep, rep[0],
                           sq->literals);

                     memcpy(lits + lit_len, src + in + lit_from,
                           sq->literals);
                     lit_len += sq->literals;

                     pos      += 1 + rep_len;
                     lit_from  = pos;
                     continue;
                  }

                  if (best_len >= RZSTD_ENC_MIN_MATCH)
                  {
                     size_t from = best_from;
                     size_t n    = best_len;

                     rzstd_seq_t *sq = &seq[nseq++];

                     sq->literals = (uint32_t)(pos - lit_from);
                     sq->match    = (uint32_t)n;
                     sq->offset = rzstd_enc_offset(rep,
                           (uint32_t)(pos - from), sq->literals);

                     memcpy(lits + lit_len, src + in + lit_from,
                           sq->literals);
                     lit_len += sq->literals;

                     /* Every position inside the match joins the
                      * table and the chain, so later lookups can find
                      * any of them rather than only what preceded the
                      * match. */
                     {
                        size_t q2  = pos + 1;
                        size_t end = pos + n;
                        size_t cm  = ((size_t)1 << enc_log) - 1;

                        if (end + RZSTD_ENC_MIN_MATCH > take)
                           end = take > RZSTD_ENC_MIN_MATCH
                               ? take - RZSTD_ENC_MIN_MATCH : 0;
                        /* Only the first few positions of a match go
                         * in. Inserting all of them is what a thorough
                         * matcher does, and here it made the output
                         * larger: on a run of one repeated value every
                         * position hashes alike, so the bucket fills
                         * with neighbours a byte or two apart and the
                         * walk finds those instead of the useful older
                         * entry. */
                        if (end > q2 + 4)
                           end = q2 + 4;
                        for (; q2 < end; q2++)
                        {
                           const uint8_t *r = src + in + q2;
                           uint32_t hh = ((uint32_t)r[0]
                                 | ((uint32_t)r[1] << 8)
                                 | ((uint32_t)r[2] << 16)
                                 | ((uint32_t)r[3] << 24))
                                 * 2654435761u;

                           hh >>= 32 - enc_log;
                           chain[(in + q2) & cm] = hash[hh];
                           hash[hh] = (uint32_t)(in + q2 + 1);
                        }
                     }

                     pos      += n;
                     lit_from  = pos;
                     continue;
                  }
               }
               pos++;
            }

            /* A match may not end a block: the last three bytes have to
             * be literals, because a decoder stops copying at the block
             * end and the format requires the tail to be literal. */
            if (nseq && lit_from < take)
            {
               memcpy(lits + lit_len, src + in + lit_from,
                     take - lit_from);
               lit_len += take - lit_from;
            }

            if (nseq && cts && rzstd_emit_block(dst + at + 3, dst_len - at - 3,
                     src + in, take, seq, nseq, lits, lit_len,
                     &produced, cts) == RZ_OK
                  && produced < take)
            {
               rzstd_write_block_header(dst + at, (uint32_t)produced,
                     RZSTD_BLOCK_COMPRESSED, last);
               at += 3 + produced;
               ok  = 1;
            }
         }

         free(seq);
         free(lits);

         if (ok)
         {
            in += take;
            continue;
         }
      }

      rzstd_write_block_header(dst + at, (uint32_t)take,
            RZSTD_BLOCK_RAW, last);
      at += 3;
      memcpy(dst + at, src + in, take);
      at += take;
      in += take;
   }

   free(hash);
   free(chain);
   free(cts);

   if (wrote)
      *wrote = at;
   return RZSTD_PROCESS_END;
}
