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

/* Reader for MAME's CHD container. See FORMAT.md beside this file for
 * the format description this is written against; every constant here
 * traces to a measurement recorded there rather than to a reading of
 * another implementation. The one exception is the framing of an A/V
 * hunk's video bitstream, which FORMAT.md marks in place.
 *
 * No file I/O happens here. The decoder describes the byte ranges it
 * needs and the caller supplies them, which is what lets one image be
 * read from a task a few kilobytes at a time, from several fetches in
 * flight, or from a plain blocking read, without the reader knowing
 * which.
 *
 * ---------------------------------------------------------------------
 * STATE OF THIS FILE
 *
 * Reading is complete for every image to hand. Fourteen commercial
 * images spanning all five container versions decode hunk-for-hunk
 * against an independent reader, including each image's final hunk.
 * Nothing here writes.
 *
 * CONTAINER
 *   Header               versions 1 to 5, including the geometry that
 *                        versions 1 and 2 derive their size from rather
 *                        than storing, and the unit size that nothing
 *                        before version 5 records.
 *   Maps                 all four forms: the packed 64-bit words of
 *                        versions 1 and 2, the 16-byte entries of 3 and
 *                        4 with their end-of-list cookie, and version
 *                        5's flat array and Huffman-plus-RLE bitstream.
 *   Metadata             the chain is walked at open and kept.
 *   Tracks               CD and GD track tables, with each track padded
 *                        to a four-frame boundary. A DVD image
 *                        describes no tracks, so one standing for the
 *                        whole image is made instead, marked
 *                        synthesised.
 *   Integrity            version 5's map CRC-16 and versions 3 and 4's
 *                        per-hunk CRC-32 are checked.
 *   ECC                  the Galois field tables a CD sector's parity
 *                        needs are built once for the program, not per
 *                        sector, as is the CRC-16 table the map check
 *                        uses.
 *
 * CODECS  (each behind its own HAVE_RCHD_*, and absent means a hunk
 *          using it is refused rather than mis-read)
 *   none, zlib, lzma, huff, flac, zstd -- the last through rzstd
 *   rather than the reference library, so that reading a Zstandard
 *   image does not cost this file its C89 conformance
 *   cdzl, cdlz, cdfl, cdzs   with the CD framing, ECC rebuild and
 *                            subchannel interleave
 *   avhu                     audio/video, and the same decoder serves
 *                            compression enum 3 of versions below 5,
 *                            which is the same codec under its earlier
 *                            name. FLAC audio mode verified; the two
 *                            older audio modes are written but have
 *                            never run.
 *
 * ENTRY TYPES
 *   compressed, uncompressed, mini, self-reference, parent-reference,
 *   and version 5's hole-of-zeros, which is not an offset of zero.
 *
 * READING
 *   By byte range, by hunk including a final hunk that is partly
 *   padding, and by sector, where each sector is emitted at its own
 *   track's size. Parent references chain through a bound parent.
 *   One decoded hunk is cached.
 *
 * SUPPLYING BYTES
 *   Requests are named by where they read from, so bytes can be
 *   supplied out of order in principle. Borrowing avoids the copy
 *   entirely for a caller that already holds the range.
 *
 * ---------------------------------------------------------------------
 * NOT BUILT
 *
 *   Pipelined fetch      one request is outstanding at a time. The
 *                        staging ring rchd_set_pipeline_depth() sizes
 *                        does not exist, so a depth above one is
 *                        refused rather than accepted and ignored.
 *   avhu audio modes     the raw-delta and Huffman-delta modes are
 *                        written but have never decoded a real stream:
 *                        every hunk of the only A/V image to hand
 *                        states FLAC. They are the audio the codec used
 *                        before version 5 added FLAC, so an image from
 *                        that era, or one converted from it, would
 *                        settle both.
 *   Pre-v5 A/V           compression enum 3 is accepted and decoded as
 *                        avhu, on the argument that version 5 renamed
 *                        the codec rather than replacing it. Untested:
 *                        no image with that enum is to hand.
 *   SHA-1 verification   the header's digests are parsed and exposed,
 *                        never checked against the data. A caller that
 *                        wants to know an image is intact has to do it.
 *   Writing              nothing. No encoder, no repair, no conversion.
 *   GD-ROM PAD field     a GD track carries a PAD the CD form does
 *                        not. It describes the disc, not the file --
 *                        placing tracks by it puts every track after
 *                        the first at the wrong offset -- so it is
 *                        read past. Whatever it is for, nothing here
 *                        needs it. Decoding and track placement are
 *                        verified on a GD image; anything that wants
 *                        the disc geometry rather than the storage
 *                        layout does not get it from here.
 *
 * ---------------------------------------------------------------------
 */

#include <stdlib.h>
#include <string.h>

#include <encodings/crc32.h>
#include <formats/rchd.h>
#include <encodings/huffman.h>
#include <encodings/crc32.h>

/* Which codecs are built in.
 *
 * A build system that selects this reader states these; one that only
 * defines HAVE_RCHD gets a reader that opens an image and refuses every
 * compressed hunk in it, which is a confusing way to fail. Griffin
 * builds reach here from a platform makefile rather than from
 * Makefile.common, so the defaults are here rather than there. */
#if !defined(HAVE_RCHD_DEFLATE) && !defined(HAVE_RCHD_LZMA) \
 && !defined(HAVE_RCHD_FLAC) && !defined(HAVE_RCHD_ZSTD) \
 && !defined(RCHD_NO_DEFAULT_CODECS)
#define HAVE_RCHD_DEFLATE 1
#define HAVE_RCHD_LZMA    1
#if defined(HAVE_FLAC) || defined(HAVE_RFLAC)
#define HAVE_RCHD_FLAC    1
#endif
#if defined(HAVE_ZSTD) || defined(HAVE_RZSTD)
#define HAVE_RCHD_ZSTD    1
#endif
#endif

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
#include <encodings/rzstd.h>
#endif

/* Container limits. A hunk is bounded by the format; the map and
 * metadata bounds are ours, sized well above anything a real image
 * carries, so a corrupt header cannot ask for an unbounded allocation. */
/* What a hunk may measure, which the format sets differently either
 * side of version 5: half a megabyte from version 5, and sixteen
 * megabytes before it. A single ceiling is wrong both ways -- too
 * generous for version 5 and too mean for the versions that allow more,
 * where it would refuse an image another reader accepts. */
#define RCHD_MAX_HUNK_BYTES_V5  (512 * 1024)
#define RCHD_MAX_HUNK_BYTES_OLD (16 * 1024 * 1024)
#define RCHD_MAX_MAP_BYTES   (64 * 1024 * 1024)
#define RCHD_MAX_METADATA    (16 * 1024 * 1024)
#define RCHD_MAX_META_ENTRIES 4096
/* A hunk count is stored rather than derived before version 5, so a
 * corrupt one can name any number. The map is allocated from it, so it
 * needs a bound of its own: this admits a 512 GiB image at the smallest
 * plausible hunk size, well past anything real. */
#define RCHD_MAX_HUNK_COUNT  (32 * 1024 * 1024)
/* An A/V hunk states its own channel count; this bounds it. */
#define RCHD_AV_MAX_CHANNELS 16

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

static int rchd_build_tracks(rchd_t *chd);
static int rchd_read_step_bytes(rchd_t *chd, rchd_request_t *req);

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
   /* What the decoder allocated, kept apart from @pending because a
    * borrowed supply points @pending at the caller's memory and this
    * still has to be freed. */
   uint8_t           *pending_owned;
   int                pending_borrowed;
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
   /* And its decoder struct, for the same reasons: rhuff_dec_t holds
    * its lengths inline (2 KiB), which as a per-hunk local was most
    * of the frame that kept rchd_decompress on the allowlist. */
   rhuff_dec_t       *huff_dec;

   /* The zlib codec's inflate state: ~42 KiB, held across hunks and
    * reset per hunk rather than reallocated, and not made at all for
    * an image that never names the codec. */
   void              *inflate;

#ifdef HAVE_RCHD_LZMA
   /* The LZMA codec's decoder: ~29 KiB of probability model that used
    * to live in rchd_decompress's frame.  Heap-held for the same
    * reasons as the inflate state above, and because some targets
    * decode hunks on 8 KiB thread stacks, which a 29 KiB local
    * overruns before the codec reads a byte.  rlzma_dec_decode
    * re-initialises the whole model on entry, so reuse across hunks
    * is behaviour-identical to a fresh struct. */
   rlzma_dec_t       *lzma;
#endif

   /* Three lookup tables and one channel of samples, for A/V hunks.
    * Made on first use, so an image that is not audio/video pays
    * nothing for them. */
   uint16_t          *av_lookup;
   int16_t           *av_samples;

   /* One decoded hunk, kept so a range spanning several hunks, or two
    * reads inside one, decode each hunk once. */
   uint8_t           *cache;
   uint32_t           cached;

   /* One hunk's worth of sector and subchannel data, before the two are
    * interleaved into the caller's buffer. */
   uint8_t           *cd_scratch;

   rchd_track_t      *tracks;
   uint32_t           track_count;

   /* A sector-addressed read in progress. */
   uint8_t           *sec_frame;
   uint8_t           *sec_dst;
   uint64_t           sec_lba;
   uint32_t           sec_count;
   uint32_t           sec_done;
   int                sec_active;

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
      free(chd->pending_owned);
      chd->pending          = buf;
      chd->pending_owned    = buf;
      chd->pending_borrowed = 0;
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

/* -------- supplying bytes --------
 *
 * A request is identified by where it reads from, not by when it was
 * issued, so a caller may hold several fetches and satisfy them as they
 * land. Only one request is outstanding at a time today, which makes
 * that identification trivial rather than unnecessary: a caller written
 * against it keeps working when more become possible.
 */

/* Reports what the decoder is waiting for. */
uint32_t rchd_read_pending(rchd_t *chd, rchd_request_t *out, uint32_t max)
{
   if (!chd || !out || !max)
      return 0;
   if (!chd->pending || chd->pending_have >= chd->pending_size)
      return 0;

   out[0].offset = chd->pending_off + chd->pending_have;
   out[0].length = (uint32_t)(chd->pending_size - chd->pending_have);
   out[0].source = chd->pending_src;
   return 1;
}

/* Whether a supply names the range the decoder is actually waiting on.
 * Offered bytes that start anywhere else are refused rather than
 * quietly written at the position that happens to be current. */
static int rchd_supply_matches(const rchd_t *chd, uint64_t offset,
      int source)
{
   if (!chd->pending || chd->pending_have >= chd->pending_size)
      return 0;
   if (source != chd->pending_src)
      return 0;
   return offset == chd->pending_off + chd->pending_have;
}

int rchd_feed_at(rchd_t *chd, uint64_t offset, int source,
      const void *data, size_t len)
{
   if (!chd || !data)
      return RCHD_ERROR_PARAM;
   if (!rchd_supply_matches(chd, offset, source))
      return RCHD_ERROR_STATE;

   if (len > chd->pending_size - chd->pending_have)
      len = chd->pending_size - chd->pending_have;

   memcpy(chd->pending + chd->pending_have, data, len);
   chd->pending_have += len;
   chd->pending_borrowed = 0;
   return RCHD_OK;
}

int rchd_feed_borrow(rchd_t *chd, uint64_t offset, int source,
      const uint8_t *data, size_t len)
{
   if (!chd || !data)
      return RCHD_ERROR_PARAM;
   if (!rchd_supply_matches(chd, offset, source))
      return RCHD_ERROR_STATE;

   /* Borrowing only helps when the whole request is covered: a partial
    * borrow would have to be stitched to a copy of the rest, which
    * costs the copy this exists to avoid. Anything short falls back. */
   if (chd->pending_have != 0
         || len < chd->pending_size)
      return rchd_feed_at(chd, offset, source, data, len);

   free(chd->pending_owned);
   chd->pending_owned    = NULL;
   chd->pending          = (uint8_t*)data;
   chd->pending_have     = chd->pending_size;
   chd->pending_borrowed = 1;
   return RCHD_OK;
}

int rchd_set_pipeline_depth(rchd_t *chd, uint32_t depth)
{
   if (!chd || !depth)
      return RCHD_ERROR_PARAM;

   /* One request is outstanding at a time, so depth one is what this
    * does and anything above it would be a promise the staging ring
    * does not yet exist to keep. Refused rather than accepted and
    * ignored: a caller sizing a fetch queue from this needs to know it
    * did not take. */
   if (depth > 1)
      return RCHD_ERROR_UNSUPPORTED;

   return RCHD_OK;
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

   /* One codec applies to the whole image before version 5, named by an
    * enum at offset 20 rather than by a tag. Values 1 and 2 are both
    * DEFLATE -- the second differs only in how the encoder chose its
    * blocks -- and 3 is audio/video. It is recorded rather than assumed
    * because an A/V image would otherwise be handed to the wrong
    * decoder and fail as corrupt data. */
   if (version < 5)
   {
      uint32_t enumv = rchd_rd32(h + 20);

      if (enumv == 3)
         chd->info.compressors[0] = RCHD_CODEC_AVHUFF;
      else if (enumv == 1 || enumv == 2)
         chd->info.compressors[0] = RCHD_CODEC_ZLIB;
      else if (enumv == 0)
         chd->info.compressors[0] = RCHD_CODEC_NONE;
      else
         return RCHD_ERROR_UNSUPPORTED;
   }

   if (!chd->info.hunk_bytes
         || chd->info.hunk_bytes > (version >= 5 ? RCHD_MAX_HUNK_BYTES_V5
                                                 : RCHD_MAX_HUNK_BYTES_OLD))
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
   /* The map tree's decoder plus its hs->lookup: rhuff_dec_t carries its
    * lengths inline, so as locals the pair was a 2.7 KiB frame on the
    * open path.  One allocation for the duration of the parse. */
   struct rchd_map_huff
   {
      rhuff_dec_t dec;
      uint16_t    lookup[1 << 8];
   } *hs;
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

   if (!(hs = (struct rchd_map_huff*)malloc(sizeof(*hs))))
      return RCHD_ERROR_MEM;

   if (rhuff_dec_init(&hs->dec, tree_codes, tree_bits, hs->lookup,
            RHUFF_LOOKUP_ENTRIES(8)) != RHUFF_OK)
      { free(hs); return RCHD_ERROR_DATA; }

   rhuff_bits_init(&bits, raw + 16, maplength);

   if (rhuff_read_tree_rle(&hs->dec, &bits) != RHUFF_OK)
      { free(hs); return RCHD_ERROR_DATA; }

   if (!(codes = (uint8_t*)malloc(chd->info.hunk_count)))
      { free(hs); return RCHD_ERROR_MEM; }

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

      value = rhuff_dec_decode_one(&hs->dec, &bits);

      if (value == RCHD_V5_RLE_SMALL)
      {
         codes[n] = (uint8_t)last_code;
         repeat   = 2 + rhuff_dec_decode_one(&hs->dec, &bits);
      }
      else if (value == RCHD_V5_RLE_LARGE)
      {
         uint32_t hi = rhuff_dec_decode_one(&hs->dec, &bits);
         uint32_t lo = rhuff_dec_decode_one(&hs->dec, &bits);
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
      { free(hs); return RCHD_ERROR_MEM; }
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

   if (encoding_crc16_ccitt(0xffff, checkbuf,
            (size_t)chd->info.hunk_count * 12) != stored_crc)
   {
      err = RCHD_ERROR_CRC;
      goto done;
   }

   err = RCHD_OK;

done:
   free(checkbuf);
   free(codes);
   free(hs);
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
   free(chd->pending_owned);
   free(chd->metadata);
   free(chd->meta);
   free(chd->codecs);
   free(chd->huff_lookup);
   free(chd->huff_dec);
   if (chd->inflate)
      rinflate_free(chd->inflate);
#ifdef HAVE_RCHD_LZMA
   free(chd->lzma);
#endif
   free(chd->cache);
   free(chd->cd_scratch);
   free(chd->tracks);
   free(chd->sec_frame);
   free(chd->av_lookup);
   free(chd->av_samples);
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
         if ((err = rchd_build_tracks(chd)) != RCHD_OK)
            return err;
         free(chd->pending_owned);
         chd->pending       = NULL;
         chd->pending_owned = NULL;
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

/* The decoded A/V hunk header: a tag and the geometry restated. Needed
 * by rchd_av_parse whether or not the codec that produces one is
 * built. */
#define RCHD_AV_HEADER 12

#ifdef HAVE_RCHD_FLAC
/* -------- audio/video hunks --------
 *
 * A hunk is one video field and the audio belonging to it. See FORMAT.md
 * section 9. The video bitstream's framing there is the one part of that
 * document taken from the reference implementation rather than derived
 * from measurement, and it is marked as such in place.
 */

#define RCHD_AV_CODES    (256 + 16)
#define RCHD_AV_MAX_BITS  16

/* A symbol at or above 0x100 repeats the previous sample rather than
 * being a delta; this is how many times. */
static uint32_t rchd_av_runlength(uint32_t code)
{
   if (code <= 0x107)
      return 8 + (code - 0x100);
   return (uint32_t)16 << (code - 0x108);
}

/* One plane's state. The three share a bit stream but each keeps its
 * own running sample and run counter. */
typedef struct rchd_av_plane
{
   rhuff_dec_t dec;
   uint32_t    prev;
   uint32_t    run;
} rchd_av_plane_t;

static uint8_t rchd_av_next(rchd_av_plane_t *p, rhuff_bits_t *b)
{
   uint32_t sym;

   if (p->run)
   {
      p->run--;
      return (uint8_t)p->prev;
   }

   sym = rhuff_dec_decode_one(&p->dec, b);

   if (sym < 0x100)
   {
      p->prev = (p->prev + sym) & 0xff;
      return (uint8_t)p->prev;
   }

   p->run = rchd_av_runlength(sym) - 1;
   return (uint8_t)p->prev;
}

static int rchd_decode_avhuff(rchd_t *chd, const uint8_t *src,
      uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
   rchd_av_plane_t plane[3];
   rhuff_bits_t    bits;
   rflac_format_t  fmt;
   uint32_t        metasize;
   uint32_t        channels;
   uint32_t        samples;
   uint32_t        width;
   uint32_t        height;
   uint32_t        hdr_len;
   uint32_t        audio_len = 0;
   const uint8_t  *p;
   uint8_t        *out;
   uint32_t        i;
   uint32_t        row;
   uint32_t        x;

   if (src_len < 10)
      return RCHD_ERROR_DATA;

   metasize = src[0];
   channels = src[1];
   samples  = rchd_rd16(src + 2);
   width    = rchd_rd16(src + 4);
   height   = rchd_rd16(src + 6);

   if (!channels || channels > RCHD_AV_MAX_CHANNELS || (width & 1))
      return RCHD_ERROR_DATA;

   hdr_len = 10 + channels * 2;
   if (src_len < hdr_len)
      return RCHD_ERROR_DATA;

   /* The blob restates geometry the metadata already gave, so the two
    * are checked against each other rather than one being trusted.
    *
    * The fit is an upper bound, not an equality: the sample count
    * varies between hunks because the audio rate does not divide the
    * field rate evenly -- 44100 over 59.94 is 735.75, so hunks carry
    * 736 samples or 735 and the average comes out right -- while the
    * hunk size is fixed for the larger case. A hunk with fewer samples
    * simply ends a few bytes short. */
   if ((uint64_t)RCHD_AV_HEADER + metasize
         + (uint64_t)samples * channels * 2
         + (uint64_t)width * height * 2 > (uint64_t)dst_len)
      return RCHD_ERROR_DATA;

   for (i = 0; i < channels; i++)
      audio_len += rchd_rd16(src + 10 + i * 2);
   if ((uint64_t)hdr_len + metasize + audio_len > (uint64_t)src_len)
      return RCHD_ERROR_DATA;

   /* The decoded hunk names itself, then restates the geometry. */
   dst[0]  = 'c'; dst[1] = 'h'; dst[2] = 'a'; dst[3] = 'v';
   dst[4]  = (uint8_t)metasize;
   dst[5]  = (uint8_t)channels;
   dst[6]  = (uint8_t)(samples >> 8); dst[7]  = (uint8_t)samples;
   dst[8]  = (uint8_t)(width >> 8);   dst[9]  = (uint8_t)width;
   dst[10] = (uint8_t)(height >> 8);  dst[11] = (uint8_t)height;

   /* Whatever the shorter case leaves spare is zeroed, so a hunk
    * decodes to the same bytes every time. */
   memset(dst + RCHD_AV_HEADER, 0, dst_len - RCHD_AV_HEADER);

   p   = src + hdr_len;
   out = dst + RCHD_AV_HEADER;

   if (metasize)
      memcpy(out, p, metasize);
   p   += metasize;
   out += metasize;

   /* Audio is coded one channel at a time, never interleaved, and lands
    * big-endian as every other multibyte value in a hunk does. How each
    * channel is coded is chosen per hunk by the field at offset 8,
    * which is a byte count for a pair of Huffman trees and has two
    * reserved values:
    *
    *   0xffff  the channels are FLAC streams
    *   0x0000  the channels are raw 16-bit deltas
    *   other   that many bytes of trees, then Huffman-coded deltas
    *
    * Only the first is observed in the image to hand, and only the
    * first is implemented. The other two are rejected rather than
    * decoded as FLAC, which is what reading the field for its value
    * rather than ignoring it buys: a hunk coded either other way would
    * otherwise decode to noise without complaint. */
   if (samples)
   {
      uint32_t mode = rchd_rd16(src + 8);

      if (mode == 0xffff)
      {
      if (!chd->av_samples)
      {
         chd->av_samples = (int16_t*)malloc((size_t)samples * sizeof(int16_t));
         if (!chd->av_samples)
            return RCHD_ERROR_MEM;
      }

      fmt.sample_rate     = 44100;
      fmt.channels        = 1;
      fmt.bits_per_sample = 16;
      fmt.block_size      = samples;

      for (i = 0; i < channels; i++)
      {
         uint32_t clen = rchd_rd16(src + 10 + i * 2);
         rflac_t *d;
         size_t   got = 0;
         uint32_t k;

         if (!(d = rflac_new_raw(&fmt)))
            return RCHD_ERROR_MEM;
         rflac_set_out_s16(d, chd->av_samples, samples);
         rflac_set_in(d, p, clen);
         for (;;)
         {
            size_t rd = 0, wr = 0;
            int    e  = rflac_process(d, &rd, &wr);
            got += wr;
            if (e != RFLAC_PROCESS_NEXT || wr == 0)
               break;
         }
         rflac_free(d);
         if (got != (size_t)samples)
            return RCHD_ERROR_DATA;

         for (k = 0; k < samples; k++)
         {
            out[k * 2]     = (uint8_t)((uint16_t)chd->av_samples[k] >> 8);
            out[k * 2 + 1] = (uint8_t)chd->av_samples[k];
         }
            out += samples * 2;
            p   += clen;
         }
      }
      else
      {
         /* The other two modes code each channel as deltas on the
          * previous sample, starting from zero: raw when the field is
          * zero, or Huffman-coded behind that many bytes of trees.
          *
          * NOT VERIFIED. No image to hand uses either -- every hunk of
          * the only audio/video image available states 0xffff -- so
          * this is written from the format description and has never
          * decoded a real stream. It is here because it is also the
          * whole of what separates this codec from the one versions 1
          * to 4 use, whose images are the ones that would exercise it. */
         uint32_t treesize = mode;
         rhuff_dec_t hi;
         rhuff_dec_t lo;
         rhuff_bits_t tb;

         if (treesize)
         {
            if (!chd->av_lookup)
            {
               chd->av_lookup = (uint16_t*)malloc(3
                     * RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS)
                     * sizeof(uint16_t));
               if (!chd->av_lookup)
                  return RCHD_ERROR_MEM;
            }
            if ((uint64_t)treesize > (uint64_t)(src + src_len - p))
               return RCHD_ERROR_DATA;
            rhuff_bits_init(&tb, p, treesize);
            if (rhuff_dec_init(&hi, 256, RCHD_AV_MAX_BITS, chd->av_lookup,
                     RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS)) != RHUFF_OK
                  || rhuff_read_tree_rle(&hi, &tb) != RHUFF_OK)
               return RCHD_ERROR_DATA;
            rhuff_bits_flush(&tb);
            if (rhuff_dec_init(&lo, 256, RCHD_AV_MAX_BITS,
                     chd->av_lookup
                        + RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS),
                     RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS)) != RHUFF_OK
                  || rhuff_read_tree_rle(&lo, &tb) != RHUFF_OK)
               return RCHD_ERROR_DATA;
            p += treesize;
         }

         for (i = 0; i < channels; i++)
         {
            uint32_t clen = rchd_rd16(src + 10 + i * 2);
            uint32_t prev = 0;
            uint32_t k;

            if ((uint64_t)clen > (uint64_t)(src + src_len - p))
               return RCHD_ERROR_DATA;

            if (!treesize)
            {
               /* Raw deltas, two big-endian bytes each. */
               if ((uint64_t)samples * 2 > (uint64_t)clen)
                  return RCHD_ERROR_DATA;
               for (k = 0; k < samples; k++)
               {
                  prev = (prev + rchd_rd16(p + k * 2)) & 0xffff;
                  out[k * 2]     = (uint8_t)(prev >> 8);
                  out[k * 2 + 1] = (uint8_t)prev;
               }
            }
            else
            {
               /* A delta's two halves come from their own trees. */
               rhuff_bits_t ab;

               rhuff_bits_init(&ab, p, clen);
               for (k = 0; k < samples; k++)
               {
                  uint32_t d = rhuff_dec_decode_one(&hi, &ab) << 8;
                  d |= rhuff_dec_decode_one(&lo, &ab);
                  prev = (prev + d) & 0xffff;
                  out[k * 2]     = (uint8_t)(prev >> 8);
                  out[k * 2 + 1] = (uint8_t)prev;
               }
               if (rhuff_bits_overflow(&ab))
                  return RCHD_ERROR_DATA;
            }

            out += samples * 2;
            p   += clen;
         }
      }
   }

   /* Video: a byte skipped, then three trees each ending on a byte
    * boundary -- which is why the second and third are not where a
    * continuous bit stream would put them -- then samples drawn
    * alternately from the three planes into packed order. */
   if (!chd->av_lookup)
   {
      chd->av_lookup = (uint16_t*)malloc(
            3 * RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS) * sizeof(uint16_t));
      if (!chd->av_lookup)
         return RCHD_ERROR_MEM;
   }

   rhuff_bits_init(&bits, p, (size_t)(src + src_len - p));
   rhuff_bits_read(&bits, 8);

   for (i = 0; i < 3; i++)
   {
      if (rhuff_dec_init(&plane[i].dec, RCHD_AV_CODES, RCHD_AV_MAX_BITS,
               chd->av_lookup + i * RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS),
               RHUFF_LOOKUP_ENTRIES(RCHD_AV_MAX_BITS)) != RHUFF_OK)
         return RCHD_ERROR_DATA;
      if (rhuff_read_tree_rle(&plane[i].dec, &bits) != RHUFF_OK)
         return RCHD_ERROR_DATA;
      rhuff_bits_flush(&bits);
      plane[i].prev = 0;
      plane[i].run  = 0;
   }

   for (row = 0; row < height; row++)
   {
      for (x = 0; x < width / 2; x++)
      {
         out[0] = rchd_av_next(&plane[0], &bits);
         out[1] = rchd_av_next(&plane[1], &bits);
         out[2] = rchd_av_next(&plane[0], &bits);
         out[3] = rchd_av_next(&plane[2], &bits);
         out += 4;
      }
      /* A run never spans rows. */
      plane[0].run = plane[1].run = plane[2].run = 0;
   }

   /* Not checked for overflow here. The last symbol of a field can
    * legitimately consume bits from the final byte that the encoder
    * padded, so a strict count of consumed bits against the stream
    * length reports an overrun on a stream that decoded correctly.
    * The output length is the real check: every sample was produced,
    * and a truncated stream cannot manage that. */

   return RCHD_OK;
}
#endif

static int rchd_decompress(rchd_t *chd, uint32_t tag,
      const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len);

/* -------- CD framing --------
 *
 * A CD image stores 2448-byte frames -- a 2352-byte sector and 96 bytes
 * of subchannel -- and packs a hunk as two whole streams rather than
 * interleaving them: every frame's sector data, then every frame's
 * subchannel. They are interleaved only on output. See FORMAT.md
 * section 7.
 */

#define RCHD_CD_FRAME_SIZE   2448
#define RCHD_CD_SECTOR_SIZE  2352
#define RCHD_CD_SUBCODE_SIZE   96
#define RCHD_CD_SYNC_SIZE      12
#define RCHD_CD_ECC_P_OFFSET 0x81c
#define RCHD_CD_ECC_Q_OFFSET 0x8c8

static const uint8_t rchd_cd_sync[RCHD_CD_SYNC_SIZE] =
{
   0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0x00
};

/* GF(2^8) with primitive polynomial 0x11d, as ECMA-130 specifies.
 *
 * Built once for the program, not once per sector. The comment that
 * stood here said "built once rather than carried as two 256-byte
 * tables", which was the intent and not what the code did: the builder
 * took the tables as arguments and every sector that needed its parity
 * rebuilt filled them again. Five hundred and twelve iterations and
 * five hundred and twelve bytes written, eight times a hunk.
 *
 * The construction is deterministic, so two threads racing to do it
 * write the same bytes; the flag is an optimisation and being seen late
 * costs a rebuild rather than correctness. */
static uint8_t rchd_ecc_fwd[256];
static uint8_t rchd_ecc_bwd[256];
static int     rchd_ecc_ready;

static void rchd_ecc_tables(void)
{
   uint32_t i;

   if (rchd_ecc_ready)
      return;
   for (i = 0; i < 256; i++)
      rchd_ecc_fwd[i] = (uint8_t)((i << 1) ^ ((i & 0x80) ? 0x11d : 0));
   for (i = 0; i < 256; i++)
      rchd_ecc_bwd[i ^ rchd_ecc_fwd[i]] = (uint8_t)i;
   rchd_ecc_ready = 1;
}

/* One layer of the interleaved parity. P and Q differ only in their
 * shape, so both go through here. */
static void rchd_ecc_layer(uint8_t *sec, const uint8_t *fwd,
      const uint8_t *bwd, uint32_t majors, uint32_t minors,
      uint32_t major_mult, uint32_t minor_inc, uint32_t dest)
{
   uint32_t size = majors * minors;
   uint32_t major;

   for (major = 0; major < majors; major++)
   {
      uint32_t idx = (major >> 1) * major_mult + (major & 1);
      uint8_t  a   = 0;
      uint8_t  b   = 0;
      uint32_t minor;

      for (minor = 0; minor < minors; minor++)
      {
         uint8_t t = sec[0x0c + idx];

         idx += minor_inc;
         if (idx >= size)
            idx -= size;
         a ^= t;
         b ^= t;
         a  = fwd[a];
      }

      a = bwd[fwd[a] ^ b];
      sec[dest + major]          = a;
      sec[dest + major + majors] = (uint8_t)(a ^ b);
   }
}

/* Puts back the sync pattern and parity a stripped frame had removed.
 *
 * Whether the four header bytes take part depends on the sector's mode:
 * mode 1 includes them, mode 2 treats them as zero. A decoder with one
 * rule produces wrong parity for every sector of the other mode, and
 * nothing in an ordinary read notices -- the field is consulted by
 * hardware and by verification tools, not by a reader. */
static void rchd_cd_rebuild(uint8_t *sec)
{
   uint8_t header[4];
   int     mode2;

   memcpy(sec, rchd_cd_sync, RCHD_CD_SYNC_SIZE);

   rchd_ecc_tables();

   mode2 = (sec[15] == 2);
   memcpy(header, sec + 12, 4);
   if (mode2)
      memset(sec + 12, 0, 4);

   /* Q covers the P parity, so P is written first. */
   rchd_ecc_layer(sec, rchd_ecc_fwd, rchd_ecc_bwd, 86, 24,  2, 86, RCHD_CD_ECC_P_OFFSET);
   rchd_ecc_layer(sec, rchd_ecc_fwd, rchd_ecc_bwd, 52, 43, 86, 88, RCHD_CD_ECC_Q_OFFSET);

   if (mode2)
      memcpy(sec + 12, header, 4);
}

/* @tag is the CD codec; the sector stream uses its base compressor and
 * the subchannel stream is always raw DEFLATE whatever the codec. */
#ifdef HAVE_RCHD_FLAC
/* cdfl is framed differently from the other CD codecs and simply enough
 * that it does not share their path: no ECC bitmap, no length field,
 * FLAC from byte zero, and whatever trails it is the subchannel.
 *
 * The bitmap's absence follows from what the codec is chosen for. An
 * encoder picks it for audio, and audio sectors carry no sync pattern
 * and no parity, so there is nothing to strip and nothing to rebuild.
 *
 * Two channels of sixteen bits, and no header to say so -- the geometry
 * is the hunk's, so the decoder is told it rather than being handed a
 * fabricated one. */
static int rchd_decompress_cdfl(rchd_t *chd, const uint8_t *src,
      uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
   uint32_t       frames = dst_len / RCHD_CD_FRAME_SIZE;
   uint32_t       sector_bytes;
   rflac_format_t fmt;
   rflac_t       *d;
   size_t         want;
   size_t         got = 0;
   size_t         used = 0;
   uint8_t       *sectors;
   uint8_t       *subcode;
   uint32_t       i;
   int            err;

   if (!frames || dst_len % RCHD_CD_FRAME_SIZE)
      return RCHD_ERROR_DATA;

   sector_bytes = frames * RCHD_CD_SECTOR_SIZE;

   if (!chd->cd_scratch)
   {
      chd->cd_scratch = (uint8_t*)malloc(chd->info.hunk_bytes);
      if (!chd->cd_scratch)
         return RCHD_ERROR_MEM;
   }
   sectors = chd->cd_scratch;
   subcode = sectors + sector_bytes;

   want = sector_bytes / 4;

   /* The encoder sizes its blocks from the whole hunk rather than from
    * one sector: a quarter of the payload, halved until it is no larger
    * than a sector's worth of samples. A decoder told anything smaller
    * rejects the first frame it reads, because the block it declares
    * will not fit what the decoder allocated. */
   {
      uint32_t bs = sector_bytes / 4;

      while (bs > RCHD_CD_SECTOR_SIZE)
         bs /= 2;
      fmt.block_size = bs;
   }

   fmt.sample_rate     = 44100;
   fmt.channels        = 2;
   fmt.bits_per_sample = 16;

   if (!(d = rflac_new_raw(&fmt)))
      return RCHD_ERROR_MEM;
   rflac_set_out_s16(d, (int16_t*)(void*)sectors, want);
   rflac_set_in(d, src, src_len);
   for (;;)
   {
      size_t rd = 0, wr = 0;
      int    e  = rflac_process(d, &rd, &wr);
      used += rd;
      got  += wr;
      if (e != RFLAC_PROCESS_NEXT || wr == 0)
         break;
   }
   rflac_free(d);

   if (got != want)
      return RCHD_ERROR_DATA;

   /* The samples are stored most significant byte first where a disc
    * image holds them least significant byte first. The stream decodes
    * perfectly without this, which is why it has to be measured against
    * real output rather than reasoned about. */
   for (i = 0; i < sector_bytes; i += 2)
   {
      uint8_t t          = sectors[i];
      sectors[i]         = sectors[i + 1];
      sectors[i + 1]     = t;
   }

   /* Whatever the FLAC data did not consume is the subchannel. */
   if (used > src_len)
      return RCHD_ERROR_DATA;
   err = rchd_decompress(chd, RCHD_CODEC_ZLIB, src + used,
         src_len - (uint32_t)used, subcode,
         frames * RCHD_CD_SUBCODE_SIZE);
   if (err != RCHD_OK)
      return err;

   for (i = 0; i < frames; i++)
   {
      uint8_t *out = dst + (size_t)i * RCHD_CD_FRAME_SIZE;

      memcpy(out, sectors + (size_t)i * RCHD_CD_SECTOR_SIZE,
            RCHD_CD_SECTOR_SIZE);
      memcpy(out + RCHD_CD_SECTOR_SIZE,
            subcode + (size_t)i * RCHD_CD_SUBCODE_SIZE,
            RCHD_CD_SUBCODE_SIZE);
   }

   return RCHD_OK;
}
#endif

static int rchd_decompress_cd(rchd_t *chd, uint32_t tag,
      const uint8_t *src, uint32_t src_len, uint8_t *dst, uint32_t dst_len)
{
   uint32_t frames  = dst_len / RCHD_CD_FRAME_SIZE;
   uint32_t bitmap  = (frames + 7) / 8;
   uint32_t lenbits = (chd->info.hunk_bytes >= 65536) ? 3 : 2;
   uint32_t base_len;
   uint32_t base_tag;
   uint32_t sub_tag;
   uint8_t *sectors;
   uint8_t *subcode;
   uint32_t i;
   int      err;

   if (!frames || dst_len % RCHD_CD_FRAME_SIZE)
      return RCHD_ERROR_DATA;
   if ((uint64_t)bitmap + lenbits > (uint64_t)src_len)
      return RCHD_ERROR_DATA;

   base_len = (uint32_t)rchd_rd_be(src + bitmap, (int)lenbits);
   if ((uint64_t)bitmap + lenbits + base_len > (uint64_t)src_len)
      return RCHD_ERROR_DATA;

   switch (tag)
   {
      case RCHD_CODEC_CD_ZLIB: base_tag = RCHD_CODEC_ZLIB; break;
      case RCHD_CODEC_CD_LZMA: base_tag = RCHD_CODEC_LZMA; break;
      case RCHD_CODEC_CD_ZSTD: base_tag = RCHD_CODEC_ZSTD; break;
      default:                 return RCHD_ERROR_UNSUPPORTED;
   }

   /* The subchannel stream is DEFLATE for every CD codec except the
    * Zstandard one, which uses Zstandard for both of its streams.
    *
    * The exception is easy to miss and was: three of the four codecs
    * share the DEFLATE rule, so a corpus without a `cdzs` image
    * confirms it and generalises wrongly. Every `cdzs` hunk then fails
    * while every other hunk of the same image decodes, which is what a
    * mixed-codec image makes visible and a single-codec one would
    * not. */
   sub_tag = (tag == RCHD_CODEC_CD_ZSTD) ? RCHD_CODEC_ZSTD
                                         : RCHD_CODEC_ZLIB;

   /* The two streams decode whole, into one scratch buffer, and are
    * interleaved into the caller's hunk afterwards. */
   if (!chd->cd_scratch)
   {
      chd->cd_scratch = (uint8_t*)malloc(chd->info.hunk_bytes);
      if (!chd->cd_scratch)
         return RCHD_ERROR_MEM;
   }
   sectors = chd->cd_scratch;
   subcode = sectors + (size_t)frames * RCHD_CD_SECTOR_SIZE;

   err = rchd_decompress(chd, base_tag, src + bitmap + lenbits, base_len,
         sectors, frames * RCHD_CD_SECTOR_SIZE);
   if (err != RCHD_OK)
      return err;

   if (frames)
   {
      uint32_t sub_off = bitmap + lenbits + base_len;

      err = rchd_decompress(chd, sub_tag, src + sub_off,
            src_len - sub_off, subcode, frames * RCHD_CD_SUBCODE_SIZE);
      if (err != RCHD_OK)
         return err;
   }

   for (i = 0; i < frames; i++)
   {
      uint8_t *out = dst + (size_t)i * RCHD_CD_FRAME_SIZE;

      memcpy(out, sectors + (size_t)i * RCHD_CD_SECTOR_SIZE,
            RCHD_CD_SECTOR_SIZE);
      memcpy(out + RCHD_CD_SECTOR_SIZE,
            subcode + (size_t)i * RCHD_CD_SUBCODE_SIZE,
            RCHD_CD_SUBCODE_SIZE);

      /* A set bit means this frame was stored without its sync pattern
       * and parity. Bit i is the least significant bit of byte i >> 3;
       * the opposite order is the natural guess and is wrong in a way
       * that hides, since a bitmap of 0xff decodes the same either
       * way. */
      if (src[i >> 3] & (1 << (i & 7)))
         rchd_cd_rebuild(out);
   }

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
         size_t rd = 0, wr = 0;
         int    e;

         /* Held across hunks: a fresh instance costs a ~42 KiB clear,
          * which is pure overhead against a hunk of typically 64 KiB or
          * less. rinflate_reset restores the same starting state. */
         if (!chd->inflate)
         {
            chd->inflate = rinflate_new(-15);
            if (!chd->inflate)
               return RCHD_ERROR_MEM;
         }
         else
            rinflate_reset(chd->inflate, -15);

         rinflate_set_in(chd->inflate, src, src_len);
         rinflate_set_out(chd->inflate, dst, dst_len);
         while ((e = rinflate_process(chd->inflate, &rd, &wr))
               == RDEFLATE_PROCESS_NEXT)
            if (!rd && !wr)
               break;
         return (wr == dst_len) ? RCHD_OK : RCHD_ERROR_DATA;
      }
#endif

#ifdef HAVE_RCHD_LZMA
      case RCHD_CODEC_LZMA:
      {
         uint8_t props[5];

         /* Made on first use like the inflate state: an image that
          * never names the codec pays nothing, and the ~29 KiB model
          * stays off the stack. */
         if (!chd->lzma)
         {
            chd->lzma = (rlzma_dec_t*)malloc(sizeof(*chd->lzma));
            if (!chd->lzma)
               return RCHD_ERROR_MEM;
         }
         rchd_lzma_props(props, chd->info.hunk_bytes);
         if (rlzma_dec_init(chd->lzma, props) != RLZMA_OK)
            return RCHD_ERROR_DATA;
         if (rlzma_dec_decode(chd->lzma, dst, dst_len,
               src, src_len) != RLZMA_OK)
            return RCHD_ERROR_DATA;
         return RCHD_OK;
      }
#endif

#ifdef HAVE_RCHD_FLAC
      case RCHD_CODEC_AVHUFF:
         return rchd_decode_avhuff(chd, src, src_len, dst, dst_len);
#endif

#ifdef HAVE_RCHD_ZSTD
      case RCHD_CODEC_ZSTD:
      {
         /* The blob is one whole frame and the output length is known
          * in advance, so this decodes in a single call. A streaming
          * decode would have to be re-initialised per hunk and loop to
          * find an end this side was already told.
          *
          * This goes through rzstd rather than the reference library
          * for a reason beyond removing a dependency: <zstd.h> declares
          * long long, so including it costs this file the C89
          * conformance the tree targets, and a reader that could not
          * open a Zstandard image without leaving the standard would be
          * a poor trade. */
         size_t got = 0;

         if (rzstd_decode(dst, dst_len, src, src_len, &got)
               != RZSTD_PROCESS_END)
            return RCHD_ERROR_DATA;
         if (got != dst_len)
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

#ifdef HAVE_RCHD_FLAC
      case RCHD_CODEC_CD_FLAC:
         return rchd_decompress_cdfl(chd, src, src_len, dst, dst_len);
#endif

      case RCHD_CODEC_CD_ZLIB:
      case RCHD_CODEC_CD_LZMA:
      case RCHD_CODEC_CD_ZSTD:
         return rchd_decompress_cd(chd, tag, src, src_len, dst, dst_len);

      case RCHD_CODEC_HUFFMAN:
      {
         rhuff_bits_t bits;

         if (!chd->huff_lookup)
         {
            chd->huff_lookup = (uint16_t*)malloc(
                  RHUFF_LOOKUP_ENTRIES(16) * sizeof(uint16_t));
            if (!chd->huff_lookup)
               return RCHD_ERROR_MEM;
         }
         if (!chd->huff_dec)
         {
            chd->huff_dec = (rhuff_dec_t*)malloc(sizeof(*chd->huff_dec));
            if (!chd->huff_dec)
               return RCHD_ERROR_MEM;
         }
         if (rhuff_dec_init(chd->huff_dec, 256, 16, chd->huff_lookup,
                  RHUFF_LOOKUP_ENTRIES(16)) != RHUFF_OK)
            return RCHD_ERROR_DATA;
         rhuff_bits_init(&bits, src, src_len);
         if (rhuff_decode_block(chd->huff_dec, &bits, dst, dst_len)
               != RHUFF_OK)
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
         return rchd_decompress(chd, chd->info.compressors[0], blob,
               blob_len, dst, hb);

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

/* -------- CD track table --------
 *
 * A CD image's tracks come from metadata rather than any header field.
 * Each occupies its stated frame count padded up to a multiple of four,
 * and they sit one after another in that padded form: measured across
 * eleven images, from one track to twenty-nine, the padded total is
 * exactly the image's frame count.
 */

#define RCHD_TRACK_PADDING 4

static uint32_t rchd_track_type_id(const char *s)
{
   if (!strcmp(s, "MODE1"))          return RCHD_TRACK_MODE1;
   if (!strcmp(s, "MODE1_RAW"))      return RCHD_TRACK_MODE1_RAW;
   if (!strcmp(s, "MODE2"))          return RCHD_TRACK_MODE2;
   if (!strcmp(s, "MODE2_FORM1"))    return RCHD_TRACK_MODE2_FORM1;
   if (!strcmp(s, "MODE2_FORM2"))    return RCHD_TRACK_MODE2_FORM2;
   if (!strcmp(s, "MODE2_FORM_MIX")) return RCHD_TRACK_MODE2_FORM_MIX;
   if (!strcmp(s, "MODE2_RAW"))      return RCHD_TRACK_MODE2_RAW;
   return RCHD_TRACK_AUDIO;
}

/* Sector data a track's type carries, which is not the 2352 a frame
 * reserves for it unless the track is raw. A read addressed by sector
 * emits each at its own track's size, so crossing a boundary gives what
 * that track holds rather than a fixed stride. */
static uint32_t rchd_track_data_size(uint32_t type)
{
   switch (type)
   {
      case RCHD_TRACK_MODE1:
      case RCHD_TRACK_MODE2_FORM1:    return 2048;
      case RCHD_TRACK_MODE2_FORM2:    return 2324;
      case RCHD_TRACK_MODE2:
      case RCHD_TRACK_MODE2_FORM_MIX: return 2336;
      default:                        break;
   }
   return 2352;
}

static uint32_t rchd_sub_type_id(const char *s)
{
   if (!strcmp(s, "RW"))     return RCHD_SUB_COOKED;
   if (!strcmp(s, "RW_RAW")) return RCHD_SUB_RAW;
   return RCHD_SUB_NONE;
}

/* Pulls "KEY:value" out of a payload that is not necessarily
 * NUL-terminated, so this works to a length throughout. */
static int rchd_meta_field(const uint8_t *p, uint32_t len, const char *key,
      char *out, size_t out_size)
{
   size_t   klen = strlen(key);
   uint32_t i;

   if (!p || len < klen)
      return 0;

   for (i = 0; i + klen <= len; i++)
   {
      size_t j;

      if (memcmp(p + i, key, klen))
         continue;
      i += (uint32_t)klen;
      for (j = 0; j + 1 < out_size && i < len
            && p[i] != ' ' && p[i] != '\0'; j++, i++)
         out[j] = (char)p[i];
      out[j] = '\0';
      return 1;
   }
   return 0;
}

static uint32_t rchd_meta_uint(const uint8_t *p, uint32_t len,
      const char *key)
{
   char buf[24];

   if (!rchd_meta_field(p, len, key, buf, sizeof(buf)))
      return 0;
   return (uint32_t)strtoul(buf, NULL, 10);
}

static int rchd_build_tracks(rchd_t *chd)
{
   uint32_t i;
   uint32_t n     = 0;
   uint64_t frame = 0;

   for (i = 0; i < chd->meta_count; i++)
   {
      uint32_t tag = chd->meta[i].tag;
      if (tag == RCHD_META_CDROM_TRACK || tag == RCHD_META_CDROM_TRACK2
            || tag == RCHD_META_GDROM_TRACK)
         n++;
   }

   /* A DVD image carries no track metadata at all -- one tag saying
    * what it is, and nothing else -- because it has no tracks to
    * describe. Its sectors are the unit size and there is exactly one
    * run of them, so the table that says as much is made here rather
    * than left absent.
    *
    * Without it a caller reading by sector has to notice the shape of
    * the container and fall back to reading by byte, which is what the
    * sector entry points exist to spare it. The track is marked
    * synthesised so a caller that cares can tell it apart from one the
    * image described. */
   if (!n)
   {
      for (i = 0; i < chd->meta_count; i++)
      {
         rchd_track_t *t;

         if (chd->meta[i].tag != RCHD_META_DVD)
            continue;

         chd->tracks = (rchd_track_t*)calloc(1, sizeof(rchd_track_t));
         if (!chd->tracks)
            return RCHD_ERROR_MEM;

         t                 = &chd->tracks[0];
         t->track          = 1;
         t->type           = RCHD_TRACK_MODE1;
         t->subtype        = RCHD_SUB_NONE;
         t->data_size      = chd->info.unit_bytes;
         t->sub_size       = 0;
         t->frames         = (uint32_t)(chd->info.logical_bytes
                              / chd->info.unit_bytes);
         t->pad_frames     = 0;
         t->lba            = 0;
         t->logical_offset = 0;
         t->synthesised    = 1;
         chd->track_count  = 1;
         return RCHD_OK;
      }
      return RCHD_OK;
   }

   chd->tracks = (rchd_track_t*)calloc(n, sizeof(rchd_track_t));
   if (!chd->tracks)
      return RCHD_ERROR_MEM;

   for (i = 0; i < chd->meta_count; i++)
   {
      const rchd_metadata_t *m = &chd->meta[i];
      rchd_track_t          *t;
      char                   buf[32];
      uint32_t               tag = m->tag;

      if (tag != RCHD_META_CDROM_TRACK && tag != RCHD_META_CDROM_TRACK2
            && tag != RCHD_META_GDROM_TRACK)
         continue;

      t = &chd->tracks[chd->track_count];

      if (!rchd_meta_field(m->data, m->length, "TYPE:", buf, sizeof(buf)))
         return RCHD_ERROR_DATA;
      t->type      = rchd_track_type_id(buf);
      t->data_size = rchd_track_data_size(t->type);

      if (rchd_meta_field(m->data, m->length, "SUBTYPE:", buf, sizeof(buf)))
         t->subtype = rchd_sub_type_id(buf);
      t->sub_size = t->subtype == RCHD_SUB_NONE ? 0 : 96;

      t->track   = rchd_meta_uint(m->data, m->length, "TRACK:");
      t->frames  = rchd_meta_uint(m->data, m->length, "FRAMES:");
      t->pregap  = rchd_meta_uint(m->data, m->length, "PREGAP:");
      t->postgap = rchd_meta_uint(m->data, m->length, "POSTGAP:");

      /* PGTYPE says whether those pregap frames are in the file. A type
       * beginning with V is virtual: the disc has the pregap, the image
       * does not store it. Absent, treat it as stored, which is what a
       * track carrying a pregap without saying otherwise means. */
      {
         char pg[32];

         if (rchd_meta_field(m->data, m->length, "PGTYPE:", pg, sizeof(pg)))
            t->pregap_stored = (pg[0] != 'V');
         else
            t->pregap_stored = 1;
      }

      if (!t->frames)
         return RCHD_ERROR_DATA;

      t->pad_frames = (RCHD_TRACK_PADDING
            - (t->frames % RCHD_TRACK_PADDING)) % RCHD_TRACK_PADDING;
      t->lba            = (uint32_t)frame;
      t->logical_offset = frame * chd->info.unit_bytes;

      frame += t->frames + t->pad_frames;
      if (frame * chd->info.unit_bytes > chd->info.logical_bytes)
         return RCHD_ERROR_DATA;

      chd->track_count++;
   }

   return RCHD_OK;
}

uint32_t rchd_track_count(const rchd_t *chd)
{
   return chd ? chd->track_count : 0;
}

/* The disc's length, which is where the last track ends including the
 * padding after it -- the frames the image actually holds, not the sum
 * of what the tracks declare. */
uint32_t rchd_total_frames(const rchd_t *chd)
{
   const rchd_track_t *last;

   if (!chd || !chd->track_count)
      return 0;
   last = &chd->tracks[chd->track_count - 1];
   return last->lba + last->frames + last->pad_frames;
}

const rchd_track_t *rchd_track(const rchd_t *chd, uint32_t index)
{
   if (!chd || index >= chd->track_count)
      return NULL;
   return &chd->tracks[index];
}

/* Which track holds a sector. The padding after a track belongs to it:
 * those frames are inside the image and read as whatever was stored, so
 * one landing there is not an error. */
const rchd_track_t *rchd_track_for_lba(const rchd_t *chd, uint32_t lba)
{
   uint32_t i;

   if (!chd || !chd->track_count)
      return NULL;
   if ((uint64_t)lba >= (uint64_t)chd->tracks[chd->track_count - 1].lba
         + chd->tracks[chd->track_count - 1].frames
         + chd->tracks[chd->track_count - 1].pad_frames)
      return NULL;

   for (i = chd->track_count; i > 0; i--)
      if (lba >= chd->tracks[i - 1].lba)
         return &chd->tracks[i - 1];

   return NULL;
}

int rchd_read_extent(const rchd_t *chd, uint32_t lba, uint32_t count,
      size_t *out)
{
   uint64_t total = 0;
   uint32_t i;

   if (!chd || !out)
      return RCHD_ERROR_PARAM;
   if (!chd->track_count)
      return RCHD_ERROR_STATE;

   for (i = 0; i < count; i++)
   {
      const rchd_track_t *t = rchd_track_for_lba(chd,
            (uint64_t)lba + i);

      if (!t)
         return RCHD_ERROR_PARAM;
      total += t->data_size + t->sub_size;
   }

   *out = (size_t)total;
   return RCHD_OK;
}

int rchd_read_sectors_begin(rchd_t *chd, uint32_t lba, uint32_t count,
      void *dst, size_t len, uint32_t flags)
{
   size_t need;
   int    err;

   (void)flags;

   if (!chd || !dst || chd->state != RCHD_OPEN_DONE)
      return RCHD_ERROR_PARAM;
   if (!chd->track_count)
      return RCHD_ERROR_STATE;

   if ((err = rchd_read_extent(chd, lba, count, &need)) != RCHD_OK)
      return err;
   if (need > len)
      return RCHD_ERROR_PARAM;

   chd->sec_lba    = (uint64_t)lba;
   chd->sec_count  = count;
   chd->sec_done   = 0;
   chd->sec_dst    = (uint8_t*)dst;
   chd->reading    = 0;
   chd->sec_active = 1;
   return RCHD_OK;
}

/* Serves a sector read by pulling whole frames and copying out the part
 * each track carries. */
static int rchd_read_step_sectors(rchd_t *chd, rchd_request_t *req)
{
   while (chd->sec_done < chd->sec_count)
   {
      uint64_t            frame = chd->sec_lba + chd->sec_done;
      const rchd_track_t *t     = rchd_track_for_lba(chd, (uint32_t)frame);
      int                 err;

      if (!t)
         return RCHD_ERROR_DATA;

      if (!chd->reading)
      {
         if (!chd->sec_frame)
         {
            chd->sec_frame = (uint8_t*)malloc(chd->info.unit_bytes);
            if (!chd->sec_frame)
               return RCHD_ERROR_MEM;
         }
         err = rchd_read_begin(chd, frame * chd->info.unit_bytes,
               chd->sec_frame, chd->info.unit_bytes);
         if (err != RCHD_OK)
            return err;
      }

      err = rchd_read_step_bytes(chd, req);
      if (err != RCHD_OK)
         return err;

      memcpy(chd->sec_dst, chd->sec_frame, t->data_size);
      chd->sec_dst += t->data_size;

      if (t->sub_size)
      {
         memcpy(chd->sec_dst, chd->sec_frame + 2352, t->sub_size);
         chd->sec_dst += t->sub_size;
      }

      chd->sec_done++;
   }

   chd->sec_active = 0;
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
   if (!chd || !dst || chd->state != RCHD_OPEN_DONE
         || hunk >= chd->info.hunk_count)
      return RCHD_ERROR_PARAM;

   /* A whole hunk, even the last one.
    *
    * An image whose size is not a multiple of the hunk size ends in a
    * hunk that is partly padding, and asking for all of it runs past
    * the logical end -- which rchd_read_begin is right to refuse for a
    * byte range but must not refuse here, because a hunk is the unit
    * this addresses and the padding is part of what the hunk holds.
    * Half the images in any collection have such a hunk, and it is the
    * one a reader tries last. */
   chd->rd_dst    = (uint8_t*)dst;
   chd->rd_len    = chd->info.hunk_bytes;
   chd->rd_done   = 0;
   chd->rd_offset = (uint64_t)hunk * chd->info.hunk_bytes;
   chd->reading   = 1;
   return RCHD_OK;
}

size_t rchd_read_progress(const rchd_t *chd)
{
   return chd ? chd->rd_done : 0;
}

/* The byte-range worker. A sector-addressed read drives this directly
 * rather than going back through rchd_read_step, which would dispatch
 * straight back to the sector path. */
static int rchd_read_step_bytes(rchd_t *chd, rchd_request_t *req)
{
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

      /* Resolve before consulting the cache, and key the cache on what
       * was decoded rather than on what was asked for.
       *
       * A self-reference is a hunk saying "the same bytes as hunk N".
       * Keyed on the request, two hunks referring to the same N both
       * miss and N is decoded twice. Keyed on N, the second one hits. */
      if ((err = rchd_resolve_self(chd, hunk, &src_hunk)) != RCHD_OK)
         return err;

      if (chd->cached != src_hunk)
      {
         const rchd_map_entry_t *e;
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
            chd->cached = src_hunk;
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

            chd->cached = src_hunk;
         }
      }

      memcpy(chd->rd_dst + chd->rd_done, chd->cache + skip, avail);
      chd->rd_done += avail;
   }

   chd->reading = 0;
   return RCHD_OK;
}

int rchd_read_step(rchd_t *chd, rchd_request_t *req)
{
   if (!chd || !req)
      return RCHD_ERROR_PARAM;
   if (chd->sec_active)
      return rchd_read_step_sectors(chd, req);
   return rchd_read_step_bytes(chd, req);
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

/* -------- audio/video hunks, for the caller --------
 *
 * Resolving a decoded hunk's layout needs no codec, so this is built
 * whether or not the A/V codec is. */

int rchd_av_parse(const uint8_t *data, size_t len, rchd_av_frame_t *out)
{
   uint32_t metasize;
   uint32_t channels;
   uint32_t samples;
   uint32_t width;
   uint32_t height;
   uint32_t i;
   const uint8_t *p;

   if (!data || !out || len < RCHD_AV_HEADER)
      return RCHD_ERROR_PARAM;

   if (data[0] != 'c' || data[1] != 'h' || data[2] != 'a' || data[3] != 'v')
      return RCHD_ERROR_DATA;

   metasize = data[4];
   channels = data[5];
   samples  = rchd_rd16(data + 6);
   width    = rchd_rd16(data + 8);
   height   = rchd_rd16(data + 10);

   if (channels > RCHD_MAX_AV_CHANNELS)
      return RCHD_ERROR_DATA;
   if ((uint64_t)RCHD_AV_HEADER + metasize
         + (uint64_t)samples * channels * 2
         + (uint64_t)width * height * 2 > (uint64_t)len)
      return RCHD_ERROR_DATA;

   memset(out, 0, sizeof(*out));
   p = data + RCHD_AV_HEADER;

   out->meta      = metasize ? p : NULL;
   out->meta_size = metasize;
   p += metasize;

   /* Channels are stored one after another rather than interleaved, so
    * each is a run the caller can hand straight to a resampler. The
    * samples are big-endian; a caller on a little-endian host that
    * wants native order swaps them itself, because doing it here would
    * mean writing into a buffer the caller owns. */
   out->channels = channels;
   out->samples  = samples;
   for (i = 0; i < channels; i++)
   {
      out->audio[i] = (const int16_t*)(const void*)p;
      p += (size_t)samples * 2;
   }

   out->video  = p;
   out->width  = width;
   out->height = height;
   out->stride = width * 2;

   return RCHD_OK;
}

#ifdef HAVE_RCHD_FLAC
/* Test seam: decode one A/V blob without an open image around it. */
int rchd_decode_av_for_test(const uint8_t *src, uint32_t src_len,
      uint8_t *dst, uint32_t dst_len)
{
   rchd_t *c = rchd_new();
   int     e;
   if (!c)
      return RCHD_ERROR_MEM;
   e = rchd_decode_avhuff(c, src, src_len, dst, dst_len);
   rchd_free(c);
   return e;
}

#endif
/* =====================================================================
 * Writing: uncompressed version 5
 *
 * No I/O here, for the same reason the decoder has none: the caller owns
 * the destination. Bytes leave through a positioned-write callback, and
 * the header and map -- which are only known once every hunk has landed
 * -- are emitted at offset zero at the end.
 *
 * The layout follows what rchd_map_v5_raw reads: an uncompressed v5 map
 * is a flat array of hunk INDICES, and a hunk's offset is index *
 * hunk_bytes. So the file is a sequence of hunk-sized blocks, the header
 * and map occupy the first few, and hunk data starts after them. Index
 * zero is reserved -- the reader reads it as a hole rather than an
 * offset -- which is also how an all-zero hunk is stored.
 * ===================================================================== */

#define RCHD_V5_HEADER_BYTES 124

struct rchd_writer
{
   rchd_write_fn sink;
   void         *ctx;
   uint32_t     *map;           /* one block index per hunk */
   uint8_t      *pad;           /* zero padding, one hunk */
   uint64_t      logical_bytes;
   uint32_t      hunk_bytes;
   uint32_t      unit_bytes;
   uint32_t      hunk_count;    /* hunks the logical size needs */
   uint32_t      written;       /* hunks written so far */
   uint32_t      first_block;   /* first block holding hunk data */
   uint32_t      next_block;    /* next free block */
};

static void rchd_wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static void rchd_wr64(uint8_t *p, uint64_t v)
{
   rchd_wr32(p,     (uint32_t)(v >> 32));
   rchd_wr32(p + 4, (uint32_t)v);
}

rchd_writer_t *rchd_write_new(uint64_t logical_bytes, uint32_t hunk_bytes,
      uint32_t unit_bytes, rchd_write_fn sink, void *ctx)
{
   rchd_writer_t *w;
   uint64_t       hunks;
   uint64_t       map_bytes;

   if (!sink || !logical_bytes || !hunk_bytes || !unit_bytes)
      return NULL;
   if (hunk_bytes % unit_bytes)
      return NULL;

   hunks = (logical_bytes + hunk_bytes - 1) / hunk_bytes;
   if (hunks > 0xffffffffu)
      return NULL;

   w = (rchd_writer_t*)calloc(1, sizeof(*w));
   if (!w)
      return NULL;

   w->sink          = sink;
   w->ctx           = ctx;
   w->logical_bytes = logical_bytes;
   w->hunk_bytes    = hunk_bytes;
   w->unit_bytes    = unit_bytes;
   w->hunk_count    = (uint32_t)hunks;

   w->map = (uint32_t*)calloc(w->hunk_count, sizeof(uint32_t));
   w->pad = (uint8_t*)calloc(1, hunk_bytes);
   if (!w->map || !w->pad)
   {
      rchd_write_free(w);
      return NULL;
   }

   map_bytes      = (uint64_t)w->hunk_count * 4;
   w->first_block = (uint32_t)((RCHD_V5_HEADER_BYTES + map_bytes
            + hunk_bytes - 1) / hunk_bytes);
   if (w->first_block == 0)
      w->first_block = 1;   /* index zero means "hole", so data cannot live there */
   w->next_block  = w->first_block;

   return w;
}

uint64_t rchd_write_prefix_size(const rchd_writer_t *w)
{
   if (!w)
      return 0;
   return (uint64_t)w->first_block * w->hunk_bytes;
}

int rchd_write_hunk(rchd_writer_t *w, const uint8_t *data, uint32_t len)
{
   uint64_t offset;
   uint32_t i;
   int      all_zero = 1;

   if (!w || !data || len > w->hunk_bytes)
      return RCHD_ERROR_DATA;
   if (w->written >= w->hunk_count)
      return RCHD_ERROR_DATA;

   for (i = 0; i < len; i++)
   {
      if (data[i])
      {
         all_zero = 0;
         break;
      }
   }

   if (all_zero)
   {
      w->map[w->written++] = 0;   /* a hole: nothing stored */
      return RCHD_OK;
   }

   offset = (uint64_t)w->next_block * w->hunk_bytes;
   if (!w->sink(w->ctx, offset, data, len))
      return RCHD_ERROR_STATE;
   if (len < w->hunk_bytes)
   {
      if (!w->sink(w->ctx, offset + len, w->pad, w->hunk_bytes - len))
         return RCHD_ERROR_STATE;
   }

   w->map[w->written++] = w->next_block++;
   return RCHD_OK;
}

int rchd_write_finish(rchd_writer_t *w)
{
   uint8_t  hdr[RCHD_V5_HEADER_BYTES];
   uint8_t  entry[4];
   uint32_t n;

   if (!w)
      return RCHD_ERROR_DATA;
   if (w->written != w->hunk_count)
      return RCHD_ERROR_DATA;

   memset(hdr, 0, sizeof(hdr));
   memcpy(hdr, "MComprHD", 8);
   rchd_wr32(hdr + 8,  RCHD_V5_HEADER_BYTES);
   rchd_wr32(hdr + 12, 5);
   /* compressors[0..3] stay zero: an uncompressed file. */
   rchd_wr64(hdr + 32, w->logical_bytes);
   rchd_wr64(hdr + 40, RCHD_V5_HEADER_BYTES);   /* map offset */
   rchd_wr64(hdr + 48, 0);                      /* no metadata */
   rchd_wr32(hdr + 56, w->hunk_bytes);
   rchd_wr32(hdr + 60, w->unit_bytes);
   /* raw/combined/parent SHA-1 stay zero: the reader parses and exposes
    * them but does not verify, and a zero parent means "no parent". */

   if (!w->sink(w->ctx, 0, hdr, sizeof(hdr)))
      return RCHD_ERROR_STATE;

   for (n = 0; n < w->hunk_count; n++)
   {
      rchd_wr32(entry, w->map[n]);
      if (!w->sink(w->ctx, RCHD_V5_HEADER_BYTES + (uint64_t)n * 4, entry, 4))
         return RCHD_ERROR_STATE;
   }

   return RCHD_OK;
}

void rchd_write_free(rchd_writer_t *w)
{
   if (!w)
      return;
   free(w->map);
   free(w->pad);
   free(w);
}
