/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rchd.h).
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

/* Clean-room reader for MAME's CHD (Compressed Hunks of Data) container,
 * versions 1 through 5.
 *
 * This decoder performs no file I/O of its own and owns no file handles.
 * Every byte it needs is described to the caller as an (offset, length,
 * source) request, which the caller satisfies however it likes: blocking
 * reads, memory maps, an already-resident buffer, or a few kilobytes per
 * frame from a task. The decoder never stalls waiting for anything.
 *
 * The operating pattern is the same for every operation:
 *
 *    int   err;
 *    rchd_request_t req;
 *
 *    for (;;)
 *    {
 *       err = rchd_open_step(chd, &req);
 *       if (err != RCHD_PENDING)
 *          break;
 *       n = pull_bytes(req.source, req.offset, buf, req.length);
 *       rchd_feed(chd, buf, n);
 *    }
 *
 * A request is always for a contiguous range and is always satisfiable in
 * one read; the decoder reissues it if the caller supplies less than the
 * full length, so short reads are legal and cost only another round trip.
 *
 * That pattern is strictly ping-pong, which is fine for opening an image
 * but leaves the storage idle while a hunk decompresses. A read can
 * instead be driven with several fetches in flight: ask the read what it
 * still needs, issue all of it at once, and hand the results back as
 * they land, in whatever order they land in.
 *
 *    rchd_set_pipeline_depth(chd, 4);
 *    rchd_read_begin(chd, offset, dst, len);
 *    for (;;)
 *    {
 *       n = rchd_read_pending(chd, reqs, 4);
 *       for (i = 0; i < n; i++)
 *          issue_async_read(&reqs[i]);
 *       while (completion_available())
 *       {
 *          c = next_completion();
 *          rchd_feed_at(chd, c->offset, c->source, c->data, c->len);
 *       }
 *       if (rchd_read_step(chd, &req) != RCHD_PENDING)
 *          break;
 *    }
 *
 * One handle decodes one hunk at a time; that bound is what keeps a
 * single rchd_read_step() call cheap. Decoding several hunks in
 * parallel means one handle per thread, which is inexpensive because
 * the map is the only large allocation and parent images are shared by
 * reference rather than copied.
 *
 * Codec support is drawn from the primitives already present in
 * libretro-common: <encodings/deflate.h> for the 'zlib' family, <7z/r7z_lzma.h>
 * for the 'lzma' family, <formats/rflac.h> for the 'flac' family, and
 * <encodings/mamehuff.h> for 'huff'. Codecs this build was not compiled with,
 * and codec tags that postdate it, can be supplied by the caller through
 * rchd_register_codec().
 */

#ifndef __LIBRETRO_SDK_FORMAT_RCHD_H
#define __LIBRETRO_SDK_FORMAT_RCHD_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Return codes. RCHD_PENDING is not an error: it means the decoder has
 * described a byte range it needs and will resume once it is fed. */
#define RCHD_OK                 0
#define RCHD_PENDING            1
#define RCHD_ERROR_DATA       (-1)
#define RCHD_ERROR_PARAM      (-2)
#define RCHD_ERROR_MEM        (-3)
#define RCHD_ERROR_UNSUPPORTED (-4)
#define RCHD_ERROR_CRC        (-5)
#define RCHD_ERROR_NO_PARENT  (-6)
#define RCHD_ERROR_STATE      (-7)

/* Which file a request refers to. Images with no parent never produce
 * RCHD_SOURCE_PARENT; callers that cannot supply a parent may return
 * RCHD_ERROR_NO_PARENT from their fetch and abandon the operation. */
#define RCHD_SOURCE_SELF        0
#define RCHD_SOURCE_PARENT      1

/* Codec tags, in the four-character form the container stores them in.
 * The set is open: unrecognised tags are not an error at open time, only
 * at the point a hunk actually needs one. */
#define RCHD_CODEC_NONE         0
#define RCHD_CODEC_ZLIB         0x7A6C6962U /* 'zlib' */
#define RCHD_CODEC_ZSTD         0x7A737464U /* 'zstd' */
#define RCHD_CODEC_LZMA         0x6C7A6D61U /* 'lzma' */
#define RCHD_CODEC_HUFFMAN      0x68756666U /* 'huff' */
#define RCHD_CODEC_FLAC         0x666C6163U /* 'flac' */
#define RCHD_CODEC_CD_ZLIB      0x63647A6CU /* 'cdzl' */
#define RCHD_CODEC_CD_ZSTD      0x63647A73U /* 'cdzs' */
#define RCHD_CODEC_CD_LZMA      0x63646C7AU /* 'cdlz' */
#define RCHD_CODEC_CD_FLAC      0x6364666CU /* 'cdfl' */
#define RCHD_CODEC_AVHUFF       0x61766875U /* 'avhu' */

/* Track types, as recorded in CD/GD metadata. */
#define RCHD_TRACK_MODE1        0
#define RCHD_TRACK_MODE1_RAW    1
#define RCHD_TRACK_MODE2        2
#define RCHD_TRACK_MODE2_FORM1  3
#define RCHD_TRACK_MODE2_FORM2  4
#define RCHD_TRACK_MODE2_FORM_MIX 5
#define RCHD_TRACK_MODE2_RAW    6
#define RCHD_TRACK_AUDIO        7

/* Subchannel types. */
/* Metadata tags a CD image carries. The two track spellings differ in
 * their payload's wording, not in the fields this reads. */
#define RCHD_META_CDROM_TRACK   0x43485452U /* 'CHTR' */
#define RCHD_META_CDROM_TRACK2  0x43485432U /* 'CHT2' */
#define RCHD_META_CDROM_OLD     0x43484344U /* 'CHCD' */
#define RCHD_META_GDROM_TRACK   0x43484744U /* 'CHGD' */
#define RCHD_META_AV            0x41564156U /* 'AVAV' */
#define RCHD_META_AV_LD         0x41564C44U /* 'AVLD' */
#define RCHD_META_HARD_DISK     0x47444444U /* 'GDDD' */
#define RCHD_META_DVD           0x44564420U /* 'DVD ' */

#define RCHD_SUB_NONE           0
#define RCHD_SUB_RAW            1
#define RCHD_SUB_COOKED         2

typedef struct rchd rchd_t;

/**
 * rchd_request_t:
 *
 * A contiguous byte range the decoder needs in order to continue. Valid
 * only until the next call on the same rchd_t.
 */
typedef struct rchd_request
{
   uint64_t offset;  /* absolute offset within the source file */
   uint32_t length;  /* bytes wanted; a short supply is legal   */
   int      source;  /* RCHD_SOURCE_SELF or RCHD_SOURCE_PARENT  */
} rchd_request_t;

/**
 * rchd_info_t:
 *
 * Container-level geometry, valid once rchd_open_step() has returned
 * RCHD_OK. logical_bytes is the size of the decompressed image; every
 * offset accepted by rchd_read_begin() is relative to it.
 */
typedef struct rchd_info
{
   uint64_t logical_bytes;
   uint32_t version;        /* 1 through 5                          */
   uint32_t hunk_bytes;
   uint32_t unit_bytes;
   uint32_t hunk_count;
   uint32_t compressors[4]; /* RCHD_CODEC_*; V5 only, else derived  */
   uint8_t  sha1[20];       /* raw+meta; zeroed before V3           */
   uint8_t  raw_sha1[20];   /* raw only; zeroed before V4           */
   uint8_t  parent_sha1[20];
   int      has_parent;
} rchd_info_t;

/**
 * rchd_metadata_t:
 *
 * One entry from the metadata chain. The payload is cached during open
 * (the chain is small and walking it lazily would make every track query
 * a round trip), so @data is directly readable and owned by the rchd_t.
 */
typedef struct rchd_metadata
{
   uint32_t       tag;
   uint8_t        flags;
   uint32_t       length;
   const uint8_t *data;
} rchd_metadata_t;

/**
 * rchd_track_t:
 *
 * One track of a CD or GD image, as reconstructed from the metadata
 * chain. Tracks are reported in disc order with pregap, postgap and
 * inter-track padding already resolved, so a mixed-mode disc (a data
 * track followed by audio tracks, each with its own sector size)
 * enumerates correctly rather than collapsing to a single geometry.
 *
 * @logical_offset is where the track's first frame begins in the CHD
 * logical stream; @lba is where it begins on the virtual disc.
 */
typedef struct rchd_track
{
   uint64_t logical_offset;
   uint32_t track;          /* 1-based                              */
   uint32_t type;           /* RCHD_TRACK_*                         */
   uint32_t subtype;        /* RCHD_SUB_*                           */
   uint32_t data_size;      /* bytes of sector data per frame       */
   uint32_t sub_size;       /* bytes of subchannel data per frame   */
   uint32_t frames;         /* frames of real content               */
   uint32_t pad_frames;     /* padding to the 4-frame boundary      */
   uint32_t pregap;
   /* Non-zero when the pregap occupies frames in the file rather than
    * being implied. The metadata states this as a PGTYPE, and a type
    * beginning with V means the pregap is virtual: the disc has it but
    * the image does not store it, so the track's data begins where the
    * track begins. A reader that skips a pregap it was never given
    * returns silence for that many frames and reads everything after
    * it from the wrong place. */
   uint32_t pregap_stored;
   uint32_t postgap;
   uint32_t lba;
   /* Non-zero when the image described no tracks and this one stands
    * for the whole of it. A DVD image is the case: it has no tracks,
    * its sectors are the unit size, and there is one run of them. */
   uint32_t synthesised;
} rchd_track_t;

/**
 * rchd_codec_decode_t:
 *
 * Caller-supplied decompressor for a codec tag this build does not
 * implement. Must produce exactly @dst_len bytes and return RCHD_OK, or
 * a negative RCHD_ERROR_* code. Called synchronously from within
 * rchd_read_step(); it must not block on I/O.
 */
typedef int (*rchd_codec_decode_t)(void *ctx,
      const uint8_t *src, uint32_t src_len,
      uint8_t *dst, uint32_t dst_len);

/**
 * rchd_new:
 *
 * Allocates a decoder. No I/O is described until rchd_open_step() is
 * called, so registration hooks can be installed first.
 *
 * Returns: a decoder, or NULL on allocation failure.
 */
rchd_t *rchd_new(void);

void rchd_free(rchd_t *chd);

/**
 * rchd_register_codec:
 * @chd        : decoder
 * @tag        : four-character codec tag, e.g. RCHD_CODEC_ZSTD
 * @fn         : decompressor, or NULL to remove a registration
 * @ctx        : opaque pointer handed back to @fn
 *
 * Overrides or supplies the handler for one codec tag. A registration
 * takes precedence over the built-in implementation, which makes this
 * usable both for codecs that postdate this build and for substituting a
 * faster implementation of one that does not.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_MEM if the table cannot grow.
 */
int rchd_register_codec(rchd_t *chd, uint32_t tag,
      rchd_codec_decode_t fn, void *ctx);

/**
 * rchd_open_step:
 * @chd        : decoder
 * @req        : receives the next byte range needed, on RCHD_PENDING
 *
 * Advances the open sequence: header, map, and metadata chain. Call
 * repeatedly, feeding each request through rchd_feed(), until this
 * returns something other than RCHD_PENDING.
 *
 * Returns: RCHD_OK when the image is open, RCHD_PENDING when more bytes
 * are needed, or a negative RCHD_ERROR_* code.
 */
int rchd_open_step(rchd_t *chd, rchd_request_t *req);

/**
 * rchd_feed:
 * @chd        : decoder
 * @data       : bytes read for the outstanding request
 * @len        : how many bytes @data holds
 *
 * Supplies some or all of the single outstanding request, which is the
 * only shape the open sequence produces. Supplying fewer bytes than
 * were asked for is legal; the remainder is requested again. @data is
 * consumed immediately and need not outlive the call.
 *
 * Reads driven with several fetches in flight use rchd_feed_at()
 * instead, since order of arrival no longer identifies the request.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_STATE if no request is outstanding,
 * or if more than one is.
 */
int rchd_feed(rchd_t *chd, const void *data, size_t len);

/**
 * rchd_info:
 *
 * Returns: geometry, or NULL if the image is not open yet.
 */
const rchd_info_t *rchd_info(const rchd_t *chd);

/**
 * rchd_parent_sha1_matches:
 * @chd        : opened decoder
 * @sha1       : 20 bytes from a candidate parent's combined SHA-1
 *
 * Returns: nonzero if @sha1 identifies the parent this image differences
 * against. Callers resolving a parent should check this before feeding
 * RCHD_SOURCE_PARENT bytes; a mismatched parent decodes to plausible
 * garbage rather than failing.
 */
int rchd_parent_sha1_matches(const rchd_t *chd, const uint8_t *sha1);

/**
 * rchd_set_parent:
 * @chd        : opened decoder whose image has a parent
 * @parent     : opened decoder for the parent image, or NULL to detach
 *
 * Binds the parent image. Hunks that difference against a parent are
 * then resolved through @parent's own map, so a request carrying
 * RCHD_SOURCE_PARENT names a real byte range in the parent file rather
 * than a unit index the caller would have to translate itself.
 *
 * @parent must outlive @chd, and must not have a read of its own armed
 * while @chd is reading. Check rchd_parent_sha1_matches() before
 * binding: a mismatched parent decodes to plausible garbage instead of
 * failing.
 *
 * Returns: RCHD_OK, RCHD_ERROR_PARAM if @parent is not open, or
 * RCHD_ERROR_NO_PARENT if @chd's image has no parent.
 */
int rchd_set_parent(rchd_t *chd, rchd_t *parent);

/* -------- metadata -------- */

uint32_t rchd_metadata_count(const rchd_t *chd);

/**
 * rchd_metadata:
 * @chd        : opened decoder
 * @index      : entry index, below rchd_metadata_count()
 *
 * Returns: the entry, or NULL if @index is out of range.
 */
const rchd_metadata_t *rchd_metadata(const rchd_t *chd, uint32_t index);

/**
 * rchd_metadata_find:
 * @chd        : opened decoder
 * @tag        : four-character metadata tag
 * @n          : which occurrence of @tag, counting from zero
 *
 * Returns: the entry, or NULL if there is no such occurrence.
 */
const rchd_metadata_t *rchd_metadata_find(const rchd_t *chd,
      uint32_t tag, uint32_t n);

/* -------- CD / GD track table -------- */

/**
 * rchd_track_count:
 *
 * Returns: the number of tracks, or 0 for images that carry no CD or GD
 * metadata (hard disk and A/V images).
 */
uint32_t rchd_track_count(const rchd_t *chd);

const rchd_track_t *rchd_track(const rchd_t *chd, uint32_t index);

/**
 * rchd_track_for_lba:
 * @chd        : opened decoder
 * @lba        : absolute sector on the virtual disc
 *
 * Returns: the track containing @lba, or NULL if @lba is past the end.
 */
const rchd_track_t *rchd_track_for_lba(const rchd_t *chd, uint32_t lba);

/**
 * rchd_total_frames:
 *
 * Returns: the disc length in frames, pregaps and postgaps included.
 */
uint32_t rchd_total_frames(const rchd_t *chd);

/* -------- reading -------- */

/**
 * rchd_read_begin:
 * @chd        : opened decoder
 * @offset     : byte offset into the logical image
 * @dst        : destination, valid until the read completes or is abandoned
 * @len        : bytes to produce
 *
 * Arms a read. Nothing is decoded and no I/O is described until
 * rchd_read_step() is called. Arming a new read abandons any read still
 * in progress.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_PARAM if the range leaves the image.
 */
int rchd_read_begin(rchd_t *chd, uint64_t offset, void *dst, size_t len);

/**
 * rchd_read_sectors_begin:
 * @chd        : opened decoder
 * @lba        : first absolute sector on the virtual disc
 * @count      : how many sectors
 * @dst        : destination
 * @len        : capacity of @dst
 * @flags      : reserved, pass 0
 *
 * Arms a read addressed by sector rather than by byte, crossing track
 * boundaries as needed. Each sector is emitted at its own track's
 * @data_size, which is what makes a mixed-mode disc read back correctly;
 * @len must be large enough for the sizes the range actually spans, and
 * rchd_read_extent() reports that up front.
 *
 * Returns: RCHD_OK, RCHD_ERROR_PARAM if the range leaves the disc, or
 * RCHD_ERROR_STATE if the image has no track table.
 */
int rchd_read_sectors_begin(rchd_t *chd, uint32_t lba, uint32_t count,
      void *dst, size_t len, uint32_t flags);

/**
 * rchd_read_extent:
 * @chd        : opened decoder
 * @lba        : first absolute sector
 * @count      : how many sectors
 * @out        : receives the number of bytes such a read would produce
 *
 * Returns: RCHD_OK, or a negative RCHD_ERROR_* code.
 */
int rchd_read_extent(const rchd_t *chd, uint32_t lba, uint32_t count,
      size_t *out);

/**
 * rchd_read_step:
 * @chd        : decoder with a read armed
 * @req        : receives the next byte range needed, on RCHD_PENDING
 *
 * Advances the armed read. At most one hunk is decompressed per call, so
 * the work between returns is bounded by the hunk size regardless of how
 * large the read is.
 *
 * Returns: RCHD_OK when @dst is complete, RCHD_PENDING when more bytes
 * are needed, or a negative RCHD_ERROR_* code.
 */
int rchd_read_step(rchd_t *chd, rchd_request_t *req);

/**
 * rchd_read_progress:
 *
 * Returns: bytes written to @dst so far by the armed read.
 */
size_t rchd_read_progress(const rchd_t *chd);

/**
 * rchd_read_hunk_begin:
 * @chd        : opened decoder
 * @hunk       : hunk index, below rchd_info()->hunk_count
 * @dst        : destination, at least hunk_bytes
 *
 * Arms a read of one whole hunk, addressed by index rather than by byte
 * offset. Equivalent to arming a byte read of hunk_bytes at
 * hunk * hunk_bytes, and provided because every older reader of this
 * format is written against hunk indices; a consumer being ported does
 * not have to convert its call sites to offsets to use this one.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_PARAM if @hunk is out of range.
 */
int rchd_read_hunk_begin(rchd_t *chd, uint32_t hunk, void *dst);

/* -------- pipelining -------- */

/**
 * rchd_set_pipeline_depth:
 * @chd        : opened decoder
 * @depth      : hunks that may be in flight at once, at least 1
 *
 * Sizes the staging ring compressed hunks are held in while they are
 * being fetched. Depth one is the default and behaves exactly like the
 * ping-pong pattern. Each further slot costs another hunk_bytes of
 * memory and lets one more fetch overlap the decode of an earlier hunk,
 * so this is the dial between memory and how much of the storage
 * latency gets hidden.
 *
 * The staging ring is not built yet, so one request is outstanding at a
 * time and depth one is the only depth this accepts. Anything above it
 * is refused rather than accepted and ignored, because a caller sizing
 * a fetch queue from the return value needs to know it did not take.
 *
 * Returns: RCHD_OK for a depth of one, RCHD_ERROR_PARAM if @depth is
 * zero, RCHD_ERROR_UNSUPPORTED above one.
 */
int rchd_set_pipeline_depth(rchd_t *chd, uint32_t depth);

/**
 * rchd_read_pending:
 * @chd        : decoder with a read armed
 * @out        : receives up to @max requests
 * @max        : capacity of @out
 *
 * Reports the ranges the armed read still needs but has not been given,
 * in the order it will consume them, up to the pipeline depth. The
 * caller may issue all of them at once.
 *
 * This does not advance the read and does not decode anything; a range
 * reported here is not reported again until the read is re-armed.
 *
 * Returns: how many requests were written to @out. Zero means the read
 * already holds everything it needs and only rchd_read_step() remains.
 */
uint32_t rchd_read_pending(rchd_t *chd, rchd_request_t *out, uint32_t max);

/**
 * rchd_feed_at:
 * @chd        : decoder
 * @offset     : absolute offset the data was read from
 * @source     : RCHD_SOURCE_SELF or RCHD_SOURCE_PARENT
 * @data       : bytes read
 * @len        : how many bytes @data holds
 *
 * Supplies bytes for one outstanding request, identified by where they
 * came from rather than by when they arrive, so fetches issued together
 * may complete in any order. Partial supply is legal and the remainder
 * is reported by the next rchd_read_pending().
 *
 * @data is copied into the staging ring and need not outlive the call.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_STATE if no outstanding request
 * covers @offset in @source.
 */
int rchd_feed_at(rchd_t *chd, uint64_t offset, int source,
      const void *data, size_t len);

/**
 * rchd_feed_borrow:
 * @chd        : decoder
 * @offset     : absolute offset the data sits at
 * @source     : RCHD_SOURCE_SELF or RCHD_SOURCE_PARENT
 * @data       : bytes already resident at @offset
 * @len        : how many bytes @data holds
 *
 * Satisfies an outstanding request without copying, for callers that
 * already hold the range in stable memory -- a fully or partially read
 * file kept in one growing allocation, or a mapping. This is worth
 * reaching for: the copy rchd_feed_at() performs is a whole hunk per
 * hunk, and at the maximum hunk size that is half a megabyte of
 * memcpy the caller has already paid for once.
 *
 * @data must stay valid and unmoved until the next rchd_read_step()
 * returns something other than RCHD_PENDING. That rules out any buffer
 * whose pages can be released underneath the decoder while it works,
 * and it rules out a base pointer that can move: a growing arena may
 * reallocate, so re-read the base after growing it and borrow from the
 * current one.
 *
 * When the lifetime cannot be guaranteed, rchd_feed_at() always
 * applies. Mixing the two across different requests is fine.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_STATE if no outstanding request
 * covers @offset in @source.
 */
int rchd_feed_borrow(rchd_t *chd, uint64_t offset, int source,
      const uint8_t *data, size_t len);

/* -------- prefetch -------- */

/**
 * rchd_hunk_location:
 * @chd        : opened decoder
 * @hunk       : hunk index, below rchd_info()->hunk_count
 * @req        : receives the range that hunk's compressed blob occupies
 *
 * Describes where a hunk lives without decoding it, so a caller that
 * knows its access pattern can read ahead. Hunks that carry no blob of
 * their own -- uncompressed runs, self references, parent references --
 * report a zero length.
 *
 * Returns: RCHD_OK, or a negative RCHD_ERROR_* code.
 */
int rchd_hunk_location(const rchd_t *chd, uint32_t hunk,
      rchd_request_t *req);

/**
 * rchd_hunk_for_offset:
 *
 * Returns: the hunk index containing logical byte @offset.
 */
uint32_t rchd_hunk_for_offset(const rchd_t *chd, uint64_t offset);

/* -------- A/V images -------- */

/**
 * rchd_av_frame_t:
 *
 * Layout of one decoded A/V hunk. Populated by rchd_av_parse(); the
 * pointers address the hunk buffer the caller passed in.
 */
#define RCHD_MAX_AV_CHANNELS 16

typedef struct rchd_av_frame
{
   const uint8_t *meta;
   const uint8_t *video;      /* @height rows of @width pixels, packed */
   const int16_t *audio[RCHD_MAX_AV_CHANNELS];
   uint32_t       meta_size;
   uint32_t       width;
   uint32_t       height;
   uint32_t       stride;     /* bytes between video rows            */
   uint32_t       channels;
   uint32_t       samples;
} rchd_av_frame_t;

/**
 * rchd_av_parse:
 * @data       : one decoded hunk, as produced by a read of hunk_bytes
 * @len        : length of @data
 * @out        : receives the frame layout
 *
 * Interprets a hunk from an image compressed with RCHD_CODEC_AVHUFF.
 * The hunk names itself, restates its geometry, and holds metadata, one
 * run of samples per audio channel, and one video field. This only
 * resolves the layout; the decoding happened during the read.
 *
 * A hunk is a *field*, not a frame: an interlaced image alternates
 * parity between consecutive hunks, and a caller wanting whole pictures
 * weaves two. Samples and the video are big-endian and packed
 * respectively, exactly as the hunk holds them -- converting either
 * would mean writing into a buffer the caller owns, and a consumer that
 * wants neither should not pay for them.
 *
 * Returns: RCHD_OK, or RCHD_ERROR_DATA if @data is not an A/V hunk.
 */
int rchd_av_parse(const uint8_t *data, size_t len, rchd_av_frame_t *out);

/* ---------------------------------------------------------------------
 * Writing
 *
 * A minimal version 5 writer, storing every hunk uncompressed. It exists
 * so that CHD readers can be tested without a full MAME toolchain: the
 * output is a real .chd that any v5 reader accepts, and comparing a title
 * read from it against the same title read from the source image checks
 * the whole open/map/read path.
 *
 * Like the reader, it does no I/O of its own. The reader is fed bytes the
 * caller fetched; the writer hands the caller bytes to store, through a
 * positioned-write callback. Nothing here opens, seeks or closes a file,
 * so this stays usable where the destination is not a file at all.
 *
 * Positioned rather than sequential because the header and the hunk map
 * are only complete once every hunk has been written -- the map records
 * where each hunk landed -- so they are emitted last, at offset zero. A
 * caller writing to a stream that cannot seek should buffer the first
 * rchd_write_prefix_size(w) bytes and fill them in at the end.
 *
 * What this does NOT do is compress. The four compressor slots are
 * written as zero, which is what the format calls an uncompressed file,
 * so the codec paths (LZMA, zlib, FLAC, Huffman) are not exercised by
 * anything it produces. It is a fixture generator, not an archiver.
 *
 * Usage:
 *
 *    rchd_writer_t *w = rchd_write_new(total_bytes, hunk_bytes,
 *                                      unit_bytes, sink, ctx);
 *    for (each hunk)
 *       rchd_write_hunk(w, buf, len);      // len <= hunk_bytes
 *    rchd_write_finish(w);                 // emits header and map
 *    rchd_write_free(w);
 *
 * A hunk written as all zeros is stored as a hole rather than as data,
 * which is both what real encoders do and what exercises the reader's
 * hole handling.
 * --------------------------------------------------------------------- */

typedef struct rchd_writer rchd_writer_t;

/**
 * rchd_write_fn:
 * @ctx   : caller's cookie, passed through untouched.
 * @offset: absolute byte offset to write at.
 * @data  : bytes to store.
 * @len   : number of bytes.
 *
 * Returns: nonzero on success, zero on failure. A failure is reported
 * back through the rchd_write_* call that triggered it.
 */
typedef int (*rchd_write_fn)(void *ctx, uint64_t offset,
      const void *data, size_t len);

/**
 * rchd_write_new:
 * @logical_bytes: total size of the uncompressed image.
 * @hunk_bytes   : bytes per hunk; must be a nonzero multiple of
 *                 @unit_bytes.
 * @unit_bytes   : sector size, typically 2048 for a disc image.
 * @sink         : positioned-write callback; must not be NULL.
 * @ctx          : passed to @sink untouched.
 *
 * Returns: a writer, or NULL on bad arguments or allocation failure.
 * Free with rchd_write_free whether or not finish succeeds.
 */
rchd_writer_t *rchd_write_new(uint64_t logical_bytes, uint32_t hunk_bytes,
      uint32_t unit_bytes, rchd_write_fn sink, void *ctx);

/**
 * rchd_write_prefix_size:
 * @w: writer.
 *
 * Returns: the number of bytes at offset zero that rchd_write_finish
 * will write (header plus map, rounded up to a hunk boundary), or zero
 * if @w is NULL. No hunk data is placed below this offset.
 */
uint64_t rchd_write_prefix_size(const rchd_writer_t *w);

/**
 * rchd_write_hunk:
 * @w   : writer.
 * @data: hunk contents; short buffers are zero-padded to a full hunk.
 * @len : bytes in @data, at most hunk_bytes.
 *
 * Hunks must be written in order. Returns: RCHD_OK, RCHD_ERROR_DATA for
 * a long write or more hunks than the logical size allows, or
 * RCHD_ERROR_STATE if the sink reported failure.
 */
int rchd_write_hunk(rchd_writer_t *w, const uint8_t *data, uint32_t len);

/**
 * rchd_write_finish:
 * @w: writer.
 *
 * Emits the header and the hunk map at offset zero. Returns: RCHD_OK,
 * RCHD_ERROR_DATA if fewer hunks were written than the logical size
 * needs, or RCHD_ERROR_STATE if the sink reported failure.
 */
int rchd_write_finish(rchd_writer_t *w);

/**
 * rchd_write_free:
 * @w: writer, may be NULL.
 */
void rchd_write_free(rchd_writer_t *w);

RETRO_END_DECLS

#endif
