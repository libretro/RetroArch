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
 * Built and exercised against real frames:
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
 *
 * Not built, in the order it has to be:
 *
 *   FSE (4.1)                   the entropy coder. Both the literals'
 *                               Huffman weights and all three sequence
 *                               tables are FSE-coded, so nothing below
 *                               works until this does.
 *   Huffman (4.2)               literal decoding, including the
 *                               four-stream layout and the jump table
 *                               that finds each stream.
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
