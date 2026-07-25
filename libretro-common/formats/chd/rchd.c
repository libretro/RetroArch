/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rchd.c).
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

/* Reader for MAME's CHD container. See FORMAT.md beside this file for the
 * format description this is written against; every constant here traces
 * to a measurement recorded there rather than to a reading of another
 * implementation.
 *
 * No file I/O happens here. The decoder describes the byte ranges it
 * needs and the caller supplies them, which is what lets one image be
 * read from a task a few kilobytes at a time, from several fetches in
 * flight, or from a plain blocking read, without the reader knowing
 * which.
 */

#include <stdlib.h>
#include <string.h>

#include <formats/rchd.h>
#include <encodings/huffman.h>
#include <encodings/crc32.h>

#ifdef HAVE_RCHD_DEFLATE
#include <encodings/deflate.h>
#endif
#ifdef HAVE_RCHD_LZMA
#include <7z/r7z_lzma.h>
#endif
#ifdef HAVE_RCHD_FLAC
#include <formats/rflac.h>
#endif
#ifdef HAVE_RCHD_ZSTD
#include <zstd.h>
#endif

/* Container limits. A hunk is bounded by the format; the map and
 * metadata bounds are ours, sized well above anything a real image
 * carries, so a corrupt header cannot ask for an unbounded allocation. */
#define RCHD_MAX_HUNK_BYTES  (1024 * 1024)
#define RCHD_MAX_MAP_BYTES   (64 * 1024 * 1024)
#define RCHD_MAX_METADATA    (16 * 1024 * 1024)
#define RCHD_MAX_META_ENTRIES 4096
/* A hunk count is stored rather than derived before version 5, so a
 * corrupt one can name any number. The map is allocated from it, so it
 * needs a bound of its own: this admits a 512 GiB image at the smallest
 * plausible hunk size, well past anything real. */
#define RCHD_MAX_HUNK_COUNT  (32 * 1024 * 1024)

/* Decoded map entry, one per hunk. Kept unpacked rather than as the
 * twelve bytes the CRC is computed over: that form exists only to be
 * hashed, and every use afterwards wants the fields. */
typedef struct rchd_map_entry
{
   uint64_t offset;
   uint32_t length;
   uint32_t crc;
   uint8_t  type;
} rchd_map_entry_t;

/* Reference codes, as stored in a compressed v5 map (FORMAT.md 2.3.3). */
enum
{
   RCHD_V5_TYPE_0 = 0,
   RCHD_V5_TYPE_3 = 3,
   RCHD_V5_NONE   = 4,
   RCHD_V5_SELF   = 5,
   RCHD_V5_PARENT = 6,
   RCHD_V5_RLE_SMALL = 7,
   RCHD_V5_RLE_LARGE = 8,
   RCHD_V5_SELF_0    = 9,
   RCHD_V5_SELF_1    = 10,
   RCHD_V5_PARENT_SELF = 11,
   RCHD_V5_PARENT_0    = 12,
   RCHD_V5_PARENT_1    = 13
};

/* Entry types of a v1 to v4 map (FORMAT.md 2.4). */
enum
{
   RCHD_V34_INVALID      = 0,
   RCHD_V34_COMPRESSED   = 1,
   RCHD_V34_UNCOMPRESSED = 2,
   RCHD_V34_MINI         = 3,
   RCHD_V34_SELF         = 4,
   RCHD_V34_PARENT       = 5,
   RCHD_V34_2ND          = 6
};

static const char rchd_end_of_list[16] =
{
   'E','n','d','O','f','L','i','s','t','C','o','o','k','i','e','\0'
};

/* -------- byte access --------
 *
 * Every field is read a byte at a time. Nothing casts the buffer to a
 * wider type, so this does not care about the host's endianness or about
 * the alignment of what the caller handed over. */

static uint32_t rchd_rd16(const uint8_t *p)
{
   return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

static uint32_t rchd_rd32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static uint64_t rchd_rd_be(const uint8_t *p, int n)
{
   uint64_t v = 0;
   int      i;

   for (i = 0; i < n; i++)
      v = (v << 8) | (uint64_t)p[i];
   return v;
}

/* CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xffff, no reflection
 * and no final xor. Validates a decoded v5 map (FORMAT.md 2.3.6). */
static uint16_t rchd_crc16(const uint8_t *data, size_t len)
{
   uint16_t crc = 0xffff;
   size_t   i;
   int      bit;

   for (i = 0; i < len; i++)
   {
      crc ^= (uint16_t)((uint16_t)data[i] << 8);
      for (bit = 0; bit < 8; bit++)
         crc = (uint16_t)((crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1));
   }
   return crc;
}

/* -------- decoder state -------- */

/* What the open sequence is waiting for. Each step names a byte range,
 * consumes it when fed, and moves on; nothing is read that a previous
 * step did not establish the position of. */
enum
{
   RCHD_OPEN_HEADER = 0,
   RCHD_OPEN_MAP,
   RCHD_OPEN_META,
   RCHD_OPEN_DONE
};

typedef struct rchd_codec_slot
{
   uint32_t            tag;
   rchd_codec_decode_t fn;
   void               *ctx;
} rchd_codec_slot_t;

struct rchd
{
   rchd_info_t        info;
   rchd_map_entry_t  *map;

   /* Raw bytes of whatever range is being collected, and how much of it
    * has arrived. One buffer serves every step because the steps are
    * strictly sequential. */
   uint8_t           *pending;
   size_t             pending_size;
   size_t             pending_have;
   uint64_t           pending_off;
   int                pending_src;

   uint8_t           *metadata;
   size_t             metadata_size;
   rchd_metadata_t   *meta;
   uint32_t           meta_count;

   uint64_t           meta_offset;   /* next entry to walk, 0 when done */
   /* Payload length of the entry being collected, once its header has
    * been read. Held across calls because the walk asks for the header
    * and then for the whole entry, and re-deriving it would mean asking
    * for the header again -- which, keyed as requests are on offset and
    * length, is a different request that discards the first. */
   uint32_t           meta_len;
   int                meta_have_len;
   uint64_t           map_offset;
   uint32_t           map_length;

   int                state;
   int                header_len;

   rchd_codec_slot_t *codecs;
   uint32_t           codec_count;

   /* The 'huff' codec's lookup table: 128 KiB, so it is allocated once
    * on first use rather than per hunk, and not at all for an image
    * that never names the codec. */
   uint16_t          *huff_lookup;

   /* A zstd context, made once and reused: the hot path should not be
    * allocating one per hunk. */
   void              *zstd;

   /* One decoded hunk, kept so a range spanning several hunks, or two
    * reads inside one, decode each hunk once. */
   uint8_t           *cache;
   uint32_t           cached;

   uint8_t           *rd_dst;
   size_t             rd_len;
   size_t             rd_done;
   uint64_t           rd_offset;
   int                reading;

   rchd_t            *parent;
};

/* Asks the caller for a range, and reports whether it has all arrived.
 * A step calls this each time it runs, so a short supply simply costs
 * another round rather than needing the step to be re-entrant. */
static int rchd_want(rchd_t *chd, uint64_t offset, size_t len, int source,
      rchd_request_t *req)
{
   if (chd->pending_off != offset || chd->pending_size != len
         || chd->pending_src != source)
   {
      uint8_t *buf;

      if (len == 0)
         return 1;
      if (!(buf = (uint8_t*)malloc(len)))
         return -1;
      free(chd->pending);
      chd->pending      = buf;
      chd->pending_size = len;
      chd->pending_have = 0;
      chd->pending_off  = offset;
      chd->pending_src  = source;
   }

   if (chd->pending_have >= len)
      return 1;

   req->offset = offset + chd->pending_have;
   req->length = (uint32_t)(len - chd->pending_have);
   req->source = source;
   return 0;
}

int rchd_feed(rchd_t *chd, const void *data, size_t len)
{
   if (!chd || !data)
      return RCHD_ERROR_PARAM;
   if (!chd->pending || chd->pending_have >= chd->pending_size)
      return RCHD_ERROR_STATE;

   if (len > chd->pending_size - chd->pending_have)
      len = chd->pending_size - chd->pending_have;

   memcpy(chd->pending + chd->pending_have, data, len);
   chd->pending_have += len;
   return RCHD_OK;
}

/* -------- header -------- */

/* Versions 1 and 2 record no size and no hunk length; both are derived
 * from the drive geometry, which makes those fields load-bearing rather
 * than descriptive (FORMAT.md 1.4). */
static int rchd_parse_v1v2(rchd_t *chd, const uint8_t *h, uint32_t version)
{
   uint32_t seclen = (version == 2) ? rchd_rd32(h + 76) : 512;
   uint32_t cyls   = rchd_rd32(h + 32);
   uint32_t heads  = rchd_rd32(h + 36);
   uint32_t secs   = rchd_rd32(h + 40);
   uint32_t hunksz = rchd_rd32(h + 24);

   if (!seclen || !cyls || !heads || !secs || !hunksz)
      return RCHD_ERROR_DATA;

   chd->info.hunk_bytes    = hunksz * seclen;
   chd->info.logical_bytes = (uint64_t)cyls * heads * secs * seclen;
   chd->info.hunk_count    = rchd_rd32(h + 28);
   chd->info.unit_bytes    = seclen;
   chd->info.has_parent    = (rchd_rd32(h + 16) & 1) != 0;
   chd->meta_offset        = 0;
   return RCHD_OK;
}

static int rchd_parse_v3v4(rchd_t *chd, const uint8_t *h, uint32_t version)
{
   if (version == 3)
   {
      chd->info.logical_bytes = rchd_rd_be(h + 28, 8);
      chd->meta_offset        = rchd_rd_be(h + 36, 8);
      chd->info.hunk_bytes    = rchd_rd32(h + 76);
      memcpy(chd->info.sha1,        h + 80,  20);
      memcpy(chd->info.parent_sha1, h + 100, 20);
   }
   else
   {
      chd->info.logical_bytes = rchd_rd_be(h + 28, 8);
      chd->meta_offset        = rchd_rd_be(h + 36, 8);
      chd->info.hunk_bytes    = rchd_rd32(h + 44);
      memcpy(chd->info.sha1,        h + 48, 20);
      memcpy(chd->info.parent_sha1, h + 68, 20);
      memcpy(chd->info.raw_sha1,    h + 88, 20);
   }

   chd->info.hunk_count = rchd_rd32(h + 24);
   chd->info.has_parent = (rchd_rd32(h + 16) & 1) != 0;

   /* Nothing before version 5 records a unit size. It is inferred from
    * metadata that names a sector size, and falls back to the hunk size
    * when no such metadata exists -- which is what both readers of this
    * format settle on (FORMAT.md 1.2). The fallback is installed here
    * and refined once the metadata has been walked. */
   chd->info.unit_bytes = chd->info.hunk_bytes;
   return RCHD_OK;
}

static int rchd_parse_v5(rchd_t *chd, const uint8_t *h)
{
   int i;

   for (i = 0; i < 4; i++)
      chd->info.compressors[i] = rchd_rd32(h + 16 + i * 4);

   chd->info.logical_bytes = rchd_rd_be(h + 32, 8);
   chd->map_offset         = rchd_rd_be(h + 40, 8);
   chd->meta_offset        = rchd_rd_be(h + 48, 8);
   chd->info.hunk_bytes    = rchd_rd32(h + 56);
   chd->info.unit_bytes    = rchd_rd32(h + 60);

   memcpy(chd->info.raw_sha1,    h + 64,  20);
   memcpy(chd->info.sha1,        h + 84,  20);
   memcpy(chd->info.parent_sha1, h + 104, 20);

   if (!chd->info.hunk_bytes || !chd->info.unit_bytes)
      return RCHD_ERROR_DATA;

   {
      static const uint8_t zero[20] = { 0 };
      chd->info.has_parent = memcmp(chd->info.parent_sha1, zero, 20) != 0;
   }

   chd->info.hunk_count = (uint32_t)((chd->info.logical_bytes
            + chd->info.hunk_bytes - 1) / chd->info.hunk_bytes);
   return RCHD_OK;
}

static int rchd_parse_header(rchd_t *chd, const uint8_t *h)
{
   uint32_t length;
   uint32_t version;
   int      err;

   if (memcmp(h, "MComprHD", 8) != 0)
      return RCHD_ERROR_DATA;

   length  = rchd_rd32(h + 8);
   version = rchd_rd32(h + 12);

   /* Length and version have to agree: a mismatch is the signature of a
    * file that is not what it says it is. */
   switch (version)
   {
      case 1: if (length != 76)  return RCHD_ERROR_DATA; break;
      case 2: if (length != 80)  return RCHD_ERROR_DATA; break;
      case 3: if (length != 120) return RCHD_ERROR_DATA; break;
      case 4: if (length != 108) return RCHD_ERROR_DATA; break;
      case 5: if (length != 124) return RCHD_ERROR_DATA; break;
      default: return RCHD_ERROR_UNSUPPORTED;
   }

   chd->info.version = version;
   chd->header_len   = (int)length;

   if (version <= 2)
      err = rchd_parse_v1v2(chd, h, version);
   else if (version <= 4)
      err = rchd_parse_v3v4(chd, h, version);
   else
      err = rchd_parse_v5(chd, h);

   if (err != RCHD_OK)
      return err;

   if (!chd->info.hunk_bytes || chd->info.hunk_bytes > RCHD_MAX_HUNK_BYTES)
      return RCHD_ERROR_DATA;
   if (!chd->info.hunk_count || chd->info.hunk_count > RCHD_MAX_HUNK_COUNT)
      return RCHD_ERROR_DATA;

   /* The count and the size have to agree. Before version 5 both are
    * stored, so a file whose count does not cover its logical size --
    * or overshoots it by more than the padding a final partial hunk
    * needs -- is describing something other than what it holds. */
   {
      uint64_t covered = (uint64_t)chd->info.hunk_count
                       * chd->info.hunk_bytes;

      if (covered < chd->info.logical_bytes)
         return RCHD_ERROR_DATA;
      if (covered - chd->info.logical_bytes >= chd->info.hunk_bytes)
         return RCHD_ERROR_DATA;
   }
   if (!chd->info.unit_bytes)
      chd->info.unit_bytes = chd->info.hunk_bytes;

   /* Before version 5 the map sits immediately after the header. */
   if (version < 5)
      chd->map_offset = length;

   return RCHD_OK;
}

/* -------- maps -------- */

/* Versions 1 and 2 pack an offset and a length into one 64-bit word and
 * record no type: a hunk is stored whole when its length equals the hunk
 * size, and compressed otherwise, which is the only signal there is
 * (FORMAT.md 2.4). */
static int rchd_map_v1v2(rchd_t *chd, const uint8_t *raw)
{
   uint32_t n;

   for (n = 0; n < chd->info.hunk_count; n++)
   {
      uint64_t word = rchd_rd_be(raw + (size_t)n * 8, 8);
      uint32_t len  = (uint32_t)(word >> 44);

      chd->map[n].offset = word & (((uint64_t)1 << 44) - 1);
      chd->map[n].length = len;
      chd->map[n].crc    = 0;
      chd->map[n].type   = (len == chd->info.hunk_bytes)
                         ? RCHD_V34_UNCOMPRESSED : RCHD_V34_COMPRESSED;
   }
   return RCHD_OK;
}

static int rchd_map_v3v4(rchd_t *chd, const uint8_t *raw)
{
   uint32_t n;

   for (n = 0; n < chd->info.hunk_count; n++)
   {
      const uint8_t *e = raw + (size_t)n * 16;

      chd->map[n].offset = rchd_rd_be(e, 8);
      chd->map[n].crc    = rchd_rd32(e + 8);
      /* A 24-bit length, assembled from a 16-bit field and an 8-bit one
       * rather than stored as a single value. */
      chd->map[n].length = rchd_rd16(e + 12) | ((uint32_t)e[14] << 16);
      chd->map[n].type   = (uint8_t)(e[15] & 0x0f);
   }
   return RCHD_OK;
}

/* An uncompressed version 5 map is a flat array of hunk indices. A value
 * of zero means a hole reading as zeros, not an offset of zero: taking
 * it literally yields the first hunk-worth of the file, which is the
 * header, and is well-formed data of the right length (FORMAT.md 2.1). */
static int rchd_map_v5_raw(rchd_t *chd, const uint8_t *raw)
{
   uint32_t n;

   for (n = 0; n < chd->info.hunk_count; n++)
   {
      uint32_t block = rchd_rd32(raw + (size_t)n * 4);

      chd->map[n].crc    = 0;
      chd->map[n].length = chd->info.hunk_bytes;
      if (block == 0)
      {
         chd->map[n].type   = RCHD_V5_NONE;
         chd->map[n].offset = 0;
         chd->map[n].length = 0;   /* marks the hole */
      }
      else
      {
         chd->map[n].type   = RCHD_V5_NONE;
         chd->map[n].offset = (uint64_t)block * chd->info.hunk_bytes;
      }
   }
   return RCHD_OK;
}

/* The compressed version 5 map: a tree, then a reference code per hunk,
 * then the fields each code implies, all in one bit stream
 * (FORMAT.md 2.3). */
static int rchd_map_v5(rchd_t *chd, const uint8_t *raw, size_t raw_len)
{
   static const uint32_t tree_codes = 16;
   static const uint32_t tree_bits  = 8;
   uint16_t     lookup[1 << 8];
   rhuff_dec_t  dec;
   rhuff_bits_t bits;
   uint8_t     *codes;
   uint8_t     *checkbuf;
   uint32_t     maplength;
   uint64_t     datastart;
   uint16_t     stored_crc;
   uint32_t     lengthbits;
   uint32_t     selfbits;
   uint32_t     parentbits;
   uint64_t     curoffset;
   uint64_t     last_self   = 0;
   uint64_t     last_parent = 0;
   uint32_t     repeat      = 0;
   uint32_t     last_code   = 0;
   uint32_t     n;
   int          err = RCHD_ERROR_DATA;

   if (raw_len < 16)
      return RCHD_ERROR_DATA;

   maplength  = rchd_rd32(raw);
   datastart  = rchd_rd_be(raw + 4, 6);
   stored_crc = (uint16_t)rchd_rd16(raw + 10);
   lengthbits = raw[12];
   selfbits   = raw[13];
   parentbits = raw[14];

   if (lengthbits > 24 || selfbits > 24 || parentbits > 24)
      return RCHD_ERROR_DATA;
   if ((uint64_t)maplength + 16 > raw_len)
      return RCHD_ERROR_DATA;

   if (rhuff_dec_init(&dec, tree_codes, tree_bits, lookup,
            RHUFF_LOOKUP_ENTRIES(8)) != RHUFF_OK)
      return RCHD_ERROR_DATA;

   rhuff_bits_init(&bits, raw + 16, maplength);

   if (rhuff_read_tree_rle(&dec, &bits) != RHUFF_OK)
      return RCHD_ERROR_DATA;

   if (!(codes = (uint8_t*)malloc(chd->info.hunk_count)))
      return RCHD_ERROR_MEM;

   /* First pass: one code per hunk. Two of the sixteen repeat the
    * previous hunk's code rather than naming their own. */
   for (n = 0; n < chd->info.hunk_count; n++)
   {
      uint32_t value;

      if (repeat > 0)
      {
         codes[n] = (uint8_t)last_code;
         repeat--;
         continue;
      }

      value = rhuff_dec_decode_one(&dec, &bits);

      if (value == RCHD_V5_RLE_SMALL)
      {
         codes[n] = (uint8_t)last_code;
         repeat   = 2 + rhuff_dec_decode_one(&dec, &bits);
      }
      else if (value == RCHD_V5_RLE_LARGE)
      {
         uint32_t hi = rhuff_dec_decode_one(&dec, &bits);
         uint32_t lo = rhuff_dec_decode_one(&dec, &bits);
         codes[n] = (uint8_t)last_code;
         repeat   = 2 + 16 + (hi << 4) + lo;
      }
      else
      {
         last_code = value;
         codes[n]  = (uint8_t)value;
      }
   }

   if (!(checkbuf = (uint8_t*)malloc((size_t)chd->info.hunk_count * 12)))
   {
      free(codes);
      return RCHD_ERROR_MEM;
   }

   /* Second pass: the fields each code implies, continuing the same
    * stream, with a running offset that compressed and stored hunks
    * advance and references do not. */
   curoffset = datastart;

   for (n = 0; n < chd->info.hunk_count; n++)
   {
      uint32_t code   = codes[n];
      uint64_t offset = curoffset;
      uint32_t length = 0;
      uint32_t crc    = 0;
      uint8_t *slot   = checkbuf + (size_t)n * 12;

      switch (code)
      {
         case RCHD_V5_TYPE_0:
         case RCHD_V5_TYPE_0 + 1:
         case RCHD_V5_TYPE_0 + 2:
         case RCHD_V5_TYPE_3:
            length     = rhuff_bits_read(&bits, (int)lengthbits);
            crc        = rhuff_bits_read(&bits, 16);
            curoffset += length;
            break;

         case RCHD_V5_NONE:
            length     = chd->info.hunk_bytes;
            crc        = rhuff_bits_read(&bits, 16);
            curoffset += length;
            break;

         case RCHD_V5_SELF:
            offset    = rhuff_bits_read(&bits, (int)selfbits);
            last_self = offset;
            break;

         case RCHD_V5_PARENT:
            offset      = rhuff_bits_read(&bits, (int)parentbits);
            last_parent = offset;
            break;

         case RCHD_V5_SELF_1:
            last_self++;
            /* falls through */
         case RCHD_V5_SELF_0:
            code   = RCHD_V5_SELF;
            offset = last_self;
            break;

         case RCHD_V5_PARENT_SELF:
            code        = RCHD_V5_PARENT;
            offset      = ((uint64_t)n * chd->info.hunk_bytes)
                        / chd->info.unit_bytes;
            last_parent = offset;
            break;

         case RCHD_V5_PARENT_1:
            last_parent += chd->info.hunk_bytes / chd->info.unit_bytes;
            /* falls through */
         case RCHD_V5_PARENT_0:
            code   = RCHD_V5_PARENT;
            offset = last_parent;
            break;

         default:
            goto done;
      }

      chd->map[n].offset = offset;
      chd->map[n].length = length;
      chd->map[n].crc    = crc;
      chd->map[n].type   = (uint8_t)code;

      /* The CRC is taken over a packed form that exists only to be
       * hashed, so it is assembled here rather than kept. */
      slot[0] = (uint8_t)code;
      slot[1] = (uint8_t)(length >> 16);
      slot[2] = (uint8_t)(length >> 8);
      slot[3] = (uint8_t)length;
      slot[4] = (uint8_t)(offset >> 40);
      slot[5] = (uint8_t)(offset >> 32);
      slot[6] = (uint8_t)(offset >> 24);
      slot[7] = (uint8_t)(offset >> 16);
      slot[8] = (uint8_t)(offset >> 8);
      slot[9] = (uint8_t)offset;
      slot[10] = (uint8_t)(crc >> 8);
      slot[11] = (uint8_t)crc;
   }

   if (rhuff_bits_overflow(&bits))
      goto done;

   if (rchd_crc16(checkbuf, (size_t)chd->info.hunk_count * 12) != stored_crc)
   {
      err = RCHD_ERROR_CRC;
      goto done;
   }

   err = RCHD_OK;

done:
   free(checkbuf);
   free(codes);
   return err;
}

/* -------- metadata --------
 *
 * The chain is walked and its payloads kept at open. It is small, and
 * walking it lazily would turn every track query into a round trip
 * (FORMAT.md 3.1). */

static int rchd_meta_collect(rchd_t *chd)
{
   const uint8_t *p   = chd->pending;
   uint32_t       len = (uint32_t)rchd_rd_be(p + 5, 3);
   uint8_t       *grown;
   size_t         base;

   if (chd->meta_count >= RCHD_MAX_META_ENTRIES)
      return RCHD_ERROR_DATA;
   if ((uint64_t)chd->metadata_size + len > RCHD_MAX_METADATA)
      return RCHD_ERROR_DATA;

   base = chd->metadata_size;

   if (!(grown = (uint8_t*)realloc(chd->metadata, base + len)))
      return RCHD_ERROR_MEM;
   chd->metadata = grown;
   memcpy(chd->metadata + base, p + 16, len);
   chd->metadata_size = base + len;

   {
      rchd_metadata_t *m = (rchd_metadata_t*)realloc(chd->meta,
            (chd->meta_count + 1) * sizeof(*m));
      if (!m)
         return RCHD_ERROR_MEM;
      chd->meta = m;
      m[chd->meta_count].tag    = rchd_rd32(p);
      m[chd->meta_count].flags  = p[4];
      m[chd->meta_count].length = len;
      /* Recorded as an offset and resolved afterwards: the block moves
       * whenever it grows. */
      m[chd->meta_count].data   = (const uint8_t*)base;
      chd->meta_count++;
   }

   chd->meta_offset = rchd_rd_be(p + 8, 8);
   return RCHD_OK;
}

static void rchd_meta_resolve(rchd_t *chd)
{
   uint32_t i;

   for (i = 0; i < chd->meta_count; i++)
      chd->meta[i].data = chd->metadata + (size_t)chd->meta[i].data;
}

/* Before version 5 nothing records a unit size, so it is taken from
 * metadata that names a sector size and left at the hunk size when none
 * does (FORMAT.md 1.2). */
static void rchd_infer_unit_bytes(rchd_t *chd)
{
   uint32_t i;

   if (chd->info.version >= 5)
      return;

   for (i = 0; i < chd->meta_count; i++)
   {
      const char *s = (const char*)chd->meta[i].data;
      const char *k;
      uint32_t    len = chd->meta[i].length;

      if (!s || len < 5)
         continue;
      for (k = s; k + 4 < s + len; k++)
      {
         if (k[0] == 'B' && k[1] == 'P' && k[2] == 'S' && k[3] == ':')
         {
            uint32_t v = 0;
            const char *d = k + 4;
            while (d < s + len && *d >= '0' && *d <= '9')
               v = v * 10 + (uint32_t)(*d++ - '0');
            if (v)
               chd->info.unit_bytes = v;
            return;
         }
      }
   }
}

/* -------- open -------- */

rchd_t *rchd_new(void)
{
   return (rchd_t*)calloc(1, sizeof(rchd_t));
}

void rchd_free(rchd_t *chd)
{
   if (!chd)
      return;
   free(chd->map);
   free(chd->pending);
   free(chd->metadata);
   free(chd->meta);
   free(chd->codecs);
   free(chd->huff_lookup);
   free(chd->cache);
#ifdef HAVE_RCHD_ZSTD
   if (chd->zstd)
      ZSTD_freeDCtx((ZSTD_DCtx*)chd->zstd);
#endif
   free(chd);
}

static size_t rchd_map_raw_bytes(const rchd_t *chd)
{
   /* One entry beyond the map: the end-of-list cookie. */
   if (chd->info.version <= 2)
      return (size_t)(chd->info.hunk_count + 1) * 8;
   if (chd->info.version <= 4)
      return (size_t)(chd->info.hunk_count + 1) * 16;
   if (chd->info.compressors[0] == RCHD_CODEC_NONE)
      return (size_t)chd->info.hunk_count * 4;
   return 0;   /* compressed: the length is in the map header */
}

int rchd_open_step(rchd_t *chd, rchd_request_t *req)
{
   int ready;
   int err;

   if (!chd || !req)
      return RCHD_ERROR_PARAM;

   switch (chd->state)
   {
      case RCHD_OPEN_HEADER:
         if ((ready = rchd_want(chd, 0, 124, RCHD_SOURCE_SELF, req)) < 0)
            return RCHD_ERROR_MEM;
         if (!ready)
            return RCHD_PENDING;
         if ((err = rchd_parse_header(chd, chd->pending)) != RCHD_OK)
            return err;

         if (!(chd->map = (rchd_map_entry_t*)calloc(chd->info.hunk_count,
                     sizeof(rchd_map_entry_t))))
            return RCHD_ERROR_MEM;

         chd->state = RCHD_OPEN_MAP;
         /* falls through */

      case RCHD_OPEN_MAP:
      {
         size_t want = rchd_map_raw_bytes(chd);

         if (want == 0)
         {
            /* A compressed map states its own length, so the sixteen
             * byte header is collected before the body it describes. */
            if (chd->map_length == 0)
            {
               if ((ready = rchd_want(chd, chd->map_offset, 16,
                           RCHD_SOURCE_SELF, req)) < 0)
                  return RCHD_ERROR_MEM;
               if (!ready)
                  return RCHD_PENDING;
               chd->map_length = rchd_rd32(chd->pending);
               if (chd->map_length > RCHD_MAX_MAP_BYTES)
                  return RCHD_ERROR_DATA;
            }
            want = chd->map_length + 16;
         }
         else if (want > RCHD_MAX_MAP_BYTES)
            return RCHD_ERROR_DATA;

         if ((ready = rchd_want(chd, chd->map_offset, want,
                     RCHD_SOURCE_SELF, req)) < 0)
            return RCHD_ERROR_MEM;
         if (!ready)
            return RCHD_PENDING;

         /* A version 1 to 4 map is followed by one entry-sized slot
          * holding a cookie. Nothing points at it; it is found by
          * computing where the entries end. One reader of this format
          * requires it and the other never looks, so a missing one is
          * tolerated rather than rejected (FORMAT.md 2.4). */
         if (chd->info.version <= 4)
         {
            size_t esz  = (chd->info.version <= 2) ? 8 : 16;
            size_t seen = (size_t)chd->info.hunk_count * esz;

            if (want >= seen + esz
                  && memcmp(chd->pending + seen, rchd_end_of_list, esz) != 0)
               return RCHD_ERROR_DATA;
         }

         if (chd->info.version <= 2)
            err = rchd_map_v1v2(chd, chd->pending);
         else if (chd->info.version <= 4)
            err = rchd_map_v3v4(chd, chd->pending);
         else if (chd->info.compressors[0] == RCHD_CODEC_NONE)
            err = rchd_map_v5_raw(chd, chd->pending);
         else
            err = rchd_map_v5(chd, chd->pending, want);

         if (err != RCHD_OK)
            return err;

         chd->state = RCHD_OPEN_META;
      }
      /* falls through */

      case RCHD_OPEN_META:
         while (chd->meta_offset != 0)
         {
            if (!chd->meta_have_len)
            {
               if ((ready = rchd_want(chd, chd->meta_offset, 16,
                           RCHD_SOURCE_SELF, req)) < 0)
                  return RCHD_ERROR_MEM;
               if (!ready)
                  return RCHD_PENDING;

               chd->meta_len = (uint32_t)rchd_rd_be(chd->pending + 5, 3);
               if (chd->meta_len > RCHD_MAX_METADATA)
                  return RCHD_ERROR_DATA;
               chd->meta_have_len = 1;
            }

            if ((ready = rchd_want(chd, chd->meta_offset,
                        16 + chd->meta_len, RCHD_SOURCE_SELF, req)) < 0)
               return RCHD_ERROR_MEM;
            if (!ready)
               return RCHD_PENDING;

            if ((err = rchd_meta_collect(chd)) != RCHD_OK)
               return err;
            chd->meta_have_len = 0;
         }

         rchd_meta_resolve(chd);
         rchd_infer_unit_bytes(chd);
         free(chd->pending);
         chd->pending      = NULL;
         chd->pending_size = 0;
         chd->pending_have = 0;
         chd->state        = RCHD_OPEN_DONE;
         /* falls through */

      case RCHD_OPEN_DONE:
      default:
         break;
   }

   return RCHD_OK;
}

const rchd_info_t *rchd_info(const rchd_t *chd)
{
   if (!chd || chd->state != RCHD_OPEN_DONE)
      return NULL;
   return &chd->info;
}

uint32_t rchd_metadata_count(const rchd_t *chd)
{
   return chd ? chd->meta_count : 0;
}

const rchd_metadata_t *rchd_metadata(const rchd_t *chd, uint32_t index)
{
   if (!chd || index >= chd->meta_count)
      return NULL;
   return &chd->meta[index];
}

const rchd_metadata_t *rchd_metadata_find(const rchd_t *chd,
      uint32_t tag, uint32_t n)
{
   uint32_t i;

   if (!chd)
      return NULL;
   for (i = 0; i < chd->meta_count; i++)
   {
      if (chd->meta[i].tag != tag)
         continue;
      if (n-- == 0)
         return &chd->meta[i];
   }
   return NULL;
}

int rchd_hunk_location(const rchd_t *chd, uint32_t hunk, rchd_request_t *req)
{
   if (!chd || !req || hunk >= chd->info.hunk_count)
      return RCHD_ERROR_PARAM;

   req->source = RCHD_SOURCE_SELF;
   req->offset = chd->map[hunk].offset;
   req->length = chd->map[hunk].length;

   /* References and holes occupy no blob of their own. */
   if (chd->info.version >= 5
         && (chd->map[hunk].type == RCHD_V5_SELF
          || chd->map[hunk].type == RCHD_V5_PARENT))
      req->length = 0;

   return RCHD_OK;
}

uint32_t rchd_hunk_for_offset(const rchd_t *chd, uint64_t offset)
{
   if (!chd || !chd->info.hunk_bytes)
      return 0;
   return (uint32_t)(offset / chd->info.hunk_bytes);
}

/* -------- codecs --------
 *
 * A hunk's compressed form is handed to whichever decoder its map entry
 * names. The primitives all live elsewhere in this tree and are used as
 * they are: nothing here wraps them beyond telling them what the
 * container knows and they do not.
 */

#ifdef HAVE_RCHD_LZMA
/* CHD stores no LZMA properties. The encoder fixes lc=3, lp=0, pb=2 and
 * a dictionary of the smallest power of two that covers a hunk, so the
 * five bytes a decoder expects are reconstructed from the hunk size
 * rather than read. */
static void rchd_lzma_props(uint8_t props[5], uint32_t hunk_bytes)
{
   uint32_t dict = 1 << 11;

   while (dict < hunk_bytes && dict < (1u << 30))
      dict <<= 1;

   props[0] = (uint8_t)((2 * 5 + 0) * 9 + 3);   /* pb=2, lp=0, lc=3 */
   props[1] = (uint8_t)(dict);
   props[2] = (uint8_t)(dict >> 8);
   props[3] = (uint8_t)(dict >> 16);
   props[4] = (uint8_t)(dict >> 24);
}
#endif

static rchd_codec_slot_t *rchd_find_codec(rchd_t *chd, uint32_t tag)
{
   uint32_t i;

   for (i = 0; i < chd->codec_count; i++)
      if (chd->codecs[i].tag == tag)
         return &chd->codecs[i];
   return NULL;
}

int rchd_register_codec(rchd_t *chd, uint32_t tag,
      rchd_codec_decode_t fn, void *ctx)
{
   rchd_codec_slot_t *slot;

   if (!chd)
      return RCHD_ERROR_PARAM;

   if ((slot = rchd_find_codec(chd, tag)))
   {
      slot->fn  = fn;
      slot->ctx = ctx;
      return RCHD_OK;
   }

   if (!fn)
      return RCHD_OK;

   slot = (rchd_codec_slot_t*)realloc(chd->codecs,
         (chd->codec_count + 1) * sizeof(*slot));
   if (!slot)
      return RCHD_ERROR_MEM;

   chd->codecs = slot;
   chd->codecs[chd->codec_count].tag = tag;
   chd->codecs[chd->codec_count].fn  = fn;
   chd->codecs[chd->codec_count].ctx = ctx;
   chd->codec_count++;
   return RCHD_OK;
}

/* Decompresses one hunk's blob. @src_len is what the map recorded and
 * @dst_len is always the hunk size, so every codec here knows its output
 * length in advance and none has to discover it. */
static int rchd_decompress(rchd_t *chd, uint32_t tag,
      const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
   rchd_codec_slot_t *slot;

   /* A registration wins over the built-in, which is what lets a caller
    * supply a codec this build has no implementation for -- or a faster
    * one for a codec it has. */
   if ((slot = rchd_find_codec(chd, tag)) && slot->fn)
      return slot->fn(slot->ctx, src, src_len, dst, dst_len);

   switch (tag)
   {
      case RCHD_CODEC_NONE:
         if (src_len != dst_len)
            return RCHD_ERROR_DATA;
         memcpy(dst, src, dst_len);
         return RCHD_OK;

#ifdef HAVE_RCHD_DEFLATE
      case RCHD_CODEC_ZLIB:
      {
         /* Raw DEFLATE: no two-byte header and no adler32 trailer.  An
          * image built with the zlib wrapper is rejected outright, which
          * is how this was established rather than assumed. */
         void  *z = rinflate_new(-15);
         size_t rd = 0, wr = 0;
         int    e;

         if (!z)
            return RCHD_ERROR_MEM;
         rinflate_set_in(z, src, src_len);
         rinflate_set_out(z, dst, dst_len);
         while ((e = rinflate_process(z, &rd, &wr)) == RDEFLATE_PROCESS_NEXT)
            if (!rd && !wr)
               break;
         rinflate_free(z);
         return (wr == dst_len) ? RCHD_OK : RCHD_ERROR_DATA;
      }
#endif

#ifdef HAVE_RCHD_LZMA
      case RCHD_CODEC_LZMA:
      {
         rlzma_dec_t dec;
         uint8_t     props[5];

         rchd_lzma_props(props, chd->info.hunk_bytes);
         if (rlzma_dec_init(&dec, props) != RLZMA_OK)
            return RCHD_ERROR_DATA;
         if (rlzma_dec_decode(&dec, dst, dst_len, src, src_len) != RLZMA_OK)
            return RCHD_ERROR_DATA;
         return RCHD_OK;
      }
#endif

#ifdef HAVE_RCHD_ZSTD
      case RCHD_CODEC_ZSTD:
      {
         /* The blob is one whole frame and the output length is already
          * known, so this decodes in a single call. A streaming decode
          * would have to be re-initialised per hunk and loop to find an
          * end this side was told in advance. */
         size_t got;

         if (!chd->zstd)
         {
            if (!(chd->zstd = ZSTD_createDCtx()))
               return RCHD_ERROR_MEM;
         }
         got = ZSTD_decompressDCtx((ZSTD_DCtx*)chd->zstd, dst, dst_len,
               src, src_len);
         if (ZSTD_isError(got) || got != dst_len)
            return RCHD_ERROR_DATA;
         return RCHD_OK;
      }
#endif

#ifdef HAVE_RCHD_FLAC
      case RCHD_CODEC_FLAC:
      {
         /* A byte of 'L' or 'B' states the order the samples are to be
          * written back in, then a bare run of FLAC frames with no
          * header -- the geometry a header would carry is the hunk
          * size, so it is passed to the decoder instead of forged.
          *
          * A hunk is read as interleaved 16-bit stereo whatever it
          * actually holds; the codec is a way of compressing bytes, and
          * nothing here has to mean audio. */
         rflac_format_t fmt;
         rflac_t       *d;
         size_t         frames = dst_len / 4;
         size_t         got    = 0;
         int            little;

         if (src_len < 2 || (src[0] != 'L' && src[0] != 'B'))
            return RCHD_ERROR_DATA;
         little = (src[0] == 'L');

         fmt.sample_rate     = 44100;
         fmt.channels        = 2;
         fmt.bits_per_sample = 16;
         fmt.block_size      = (uint32_t)frames;

         if (!(d = rflac_new_raw(&fmt)))
            return RCHD_ERROR_MEM;

         rflac_set_out_s16(d, (int16_t*)(void*)dst, frames);
         rflac_set_in(d, src + 1, src_len - 1);
         for (;;)
         {
            size_t rd = 0, wr = 0;
            int    e  = rflac_process(d, &rd, &wr);
            got += wr;
            if (e != RFLAC_PROCESS_NEXT || wr == 0)
               break;
         }
         rflac_free(d);

         if (got != frames)
            return RCHD_ERROR_DATA;

         /* The decoder wrote host-order samples; the marker says what
          * the hunk's own order is. */
         {
            static const uint16_t probe = 1;
            int host_little = *(const uint8_t*)&probe == 1;
            if (host_little != little)
            {
               uint32_t i;
               for (i = 0; i + 1 < dst_len; i += 2)
               {
                  uint8_t t = dst[i];
                  dst[i]    = dst[i + 1];
                  dst[i + 1] = t;
               }
            }
         }
         return RCHD_OK;
      }
#endif

      case RCHD_CODEC_HUFFMAN:
      {
         rhuff_dec_t  dec;
         rhuff_bits_t bits;

         if (!chd->huff_lookup)
         {
            chd->huff_lookup = (uint16_t*)malloc(
                  RHUFF_LOOKUP_ENTRIES(16) * sizeof(uint16_t));
            if (!chd->huff_lookup)
               return RCHD_ERROR_MEM;
         }
         if (rhuff_dec_init(&dec, 256, 16, chd->huff_lookup,
                  RHUFF_LOOKUP_ENTRIES(16)) != RHUFF_OK)
            return RCHD_ERROR_DATA;
         rhuff_bits_init(&bits, src, src_len);
         if (rhuff_decode_block(&dec, &bits, dst, dst_len) != RHUFF_OK)
            return RCHD_ERROR_DATA;
         return RCHD_OK;
      }

      default:
         break;
   }

   return RCHD_ERROR_UNSUPPORTED;
}

/* Resolves one hunk into @dst. Everything the hunk needs has already
 * been fetched by the caller and sits in @blob; references and holes
 * need nothing fetched at all. */
static int rchd_build_hunk(rchd_t *chd, uint32_t hunk,
      const uint8_t *blob, uint32_t blob_len, uint8_t *dst)
{
   const rchd_map_entry_t *e = &chd->map[hunk];
   uint32_t                hb = chd->info.hunk_bytes;

   if (chd->info.version >= 5)
   {
      if (e->type == RCHD_V5_NONE && e->length == 0)
      {
         /* A hole. Not an offset of zero. */
         memset(dst, 0, hb);
         return RCHD_OK;
      }
      if (e->type == RCHD_V5_NONE)
      {
         if (blob_len < hb)
            return RCHD_ERROR_DATA;
         memcpy(dst, blob, hb);
         return RCHD_OK;
      }
      if (e->type <= RCHD_V5_TYPE_3)
         return rchd_decompress(chd, chd->info.compressors[e->type],
               blob, blob_len, dst, hb);
      /* Self and parent references are resolved by the caller, which
       * knows which hunk to fetch; reaching here means it did not. */
      return RCHD_ERROR_STATE;
   }

   switch (e->type)
   {
      case RCHD_V34_UNCOMPRESSED:
         if (blob_len < hb)
            return RCHD_ERROR_DATA;
         memcpy(dst, blob, hb);
         return RCHD_OK;

      case RCHD_V34_MINI:
      {
         /* The offset field holds eight bytes of pattern rather than a
          * position, repeated to fill the hunk. */
         uint32_t i;
         uint8_t  pat[8];

         for (i = 0; i < 8; i++)
            pat[i] = (uint8_t)(e->offset >> (56 - i * 8));
         for (i = 0; i < hb; i++)
            dst[i] = pat[i & 7];
         return RCHD_OK;
      }

      case RCHD_V34_COMPRESSED:
         /* One codec applies to the whole image before version 5, and
          * every observed value of the enum means zlib. */
         return rchd_decompress(chd, RCHD_CODEC_ZLIB, blob, blob_len,
               dst, hb);

      case RCHD_V34_SELF:
      case RCHD_V34_PARENT:
         return RCHD_ERROR_STATE;

      default:
         break;
   }

   return RCHD_ERROR_DATA;
}

/* -------- reading --------
 *
 * A read walks the hunks its byte range covers. For each one it names
 * the blob it needs, decodes it once into a cache, and copies out the
 * slice wanted. The cache means a range spanning several hunks, or two
 * reads within one hunk, decode each hunk once.
 */

static int rchd_cache_alloc(rchd_t *chd)
{
   if (!chd->cache)
   {
      if (!(chd->cache = (uint8_t*)malloc(chd->info.hunk_bytes)))
         return RCHD_ERROR_MEM;
      chd->cached = (uint32_t)-1;
   }
   return RCHD_OK;
}

/* Which hunk actually holds a hunk's data, following a self reference
 * to its target. A chain is not expected and not followed: a reference
 * to a reference is a malformed map, not a shape to support. */
static int rchd_resolve_self(const rchd_t *chd, uint32_t hunk, uint32_t *out)
{
   const rchd_map_entry_t *e = &chd->map[hunk];

   if (chd->info.version >= 5)
   {
      if (e->type != RCHD_V5_SELF)
      {
         *out = hunk;
         return RCHD_OK;
      }
   }
   else if (e->type != RCHD_V34_SELF)
   {
      *out = hunk;
      return RCHD_OK;
   }

   if (e->offset >= chd->info.hunk_count)
      return RCHD_ERROR_DATA;

   *out = (uint32_t)e->offset;
   if (chd->info.version >= 5)
   {
      if (chd->map[*out].type == RCHD_V5_SELF)
         return RCHD_ERROR_DATA;
   }
   else if (chd->map[*out].type == RCHD_V34_SELF)
      return RCHD_ERROR_DATA;

   return RCHD_OK;
}

int rchd_read_begin(rchd_t *chd, uint64_t offset, void *dst, size_t len)
{
   if (!chd || chd->state != RCHD_OPEN_DONE || (!dst && len))
      return RCHD_ERROR_PARAM;
   if (offset > chd->info.logical_bytes
         || len > chd->info.logical_bytes - offset)
      return RCHD_ERROR_PARAM;

   chd->rd_dst    = (uint8_t*)dst;
   chd->rd_len    = len;
   chd->rd_done   = 0;
   chd->rd_offset = offset;
   chd->reading   = 1;
   return RCHD_OK;
}

int rchd_read_hunk_begin(rchd_t *chd, uint32_t hunk, void *dst)
{
   if (!chd || chd->state != RCHD_OPEN_DONE || hunk >= chd->info.hunk_count)
      return RCHD_ERROR_PARAM;
   return rchd_read_begin(chd, (uint64_t)hunk * chd->info.hunk_bytes,
         dst, chd->info.hunk_bytes);
}

size_t rchd_read_progress(const rchd_t *chd)
{
   return chd ? chd->rd_done : 0;
}

int rchd_read_step(rchd_t *chd, rchd_request_t *req)
{
   if (!chd || !req)
      return RCHD_ERROR_PARAM;
   if (!chd->reading)
      return RCHD_ERROR_STATE;

   while (chd->rd_done < chd->rd_len)
   {
      uint64_t at    = chd->rd_offset + chd->rd_done;
      uint32_t hunk  = (uint32_t)(at / chd->info.hunk_bytes);
      uint32_t skip  = (uint32_t)(at % chd->info.hunk_bytes);
      size_t   avail = chd->info.hunk_bytes - skip;
      uint32_t src_hunk;
      int      err;

      if (avail > chd->rd_len - chd->rd_done)
         avail = chd->rd_len - chd->rd_done;

      if (hunk >= chd->info.hunk_count)
         return RCHD_ERROR_DATA;

      if ((err = rchd_cache_alloc(chd)) != RCHD_OK)
         return err;

      if (chd->cached != hunk)
      {
         const rchd_map_entry_t *e;

         if ((err = rchd_resolve_self(chd, hunk, &src_hunk)) != RCHD_OK)
            return err;
         e = &chd->map[src_hunk];

         /* A parent reference is the parent's data at a unit position,
          * so it is read from the bound parent rather than from here. */
         if ((chd->info.version >= 5 && e->type == RCHD_V5_PARENT)
               || (chd->info.version < 5 && e->type == RCHD_V34_PARENT))
         {
            if (!chd->parent)
               return RCHD_ERROR_NO_PARENT;
            err = rchd_read_begin(chd->parent,
                  e->offset * chd->info.unit_bytes,
                  chd->cache, chd->info.hunk_bytes);
            if (err != RCHD_OK)
               return err;
            for (;;)
            {
               err = rchd_read_step(chd->parent, req);
               if (err == RCHD_PENDING)
               {
                  req->source = RCHD_SOURCE_PARENT;
                  return RCHD_PENDING;
               }
               if (err != RCHD_OK)
                  return err;
               break;
            }
            chd->cached = hunk;
         }
         else
         {
            size_t need = e->length;
            int    ready;

            if (need > 0)
            {
               if ((ready = rchd_want(chd, e->offset, need,
                           RCHD_SOURCE_SELF, req)) < 0)
                  return RCHD_ERROR_MEM;
               if (!ready)
                  return RCHD_PENDING;
            }

            err = rchd_build_hunk(chd, src_hunk, chd->pending,
                  (uint32_t)need, chd->cache);
            if (err != RCHD_OK)
               return err;

            /* Versions 3 and 4 record a CRC-32 per hunk; checking it is
             * the only integrity signal those images carry. */
            if (chd->info.version >= 3 && chd->info.version <= 4
                  && e->crc != 0
                  && encoding_crc32(0, chd->cache, chd->info.hunk_bytes)
                     != e->crc)
               return RCHD_ERROR_CRC;

            chd->cached = hunk;
         }
      }

      memcpy(chd->rd_dst + chd->rd_done, chd->cache + skip, avail);
      chd->rd_done += avail;
   }

   chd->reading = 0;
   return RCHD_OK;
}

int rchd_set_parent(rchd_t *chd, rchd_t *parent)
{
   if (!chd)
      return RCHD_ERROR_PARAM;
   if (parent && parent->state != RCHD_OPEN_DONE)
      return RCHD_ERROR_PARAM;
   if (parent && !chd->info.has_parent)
      return RCHD_ERROR_NO_PARENT;
   chd->parent = parent;
   return RCHD_OK;
}

int rchd_parent_sha1_matches(const rchd_t *chd, const uint8_t *sha1)
{
   if (!chd || !sha1)
      return 0;
   return memcmp(chd->info.parent_sha1, sha1, 20) == 0;
}
