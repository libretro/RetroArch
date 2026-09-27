/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rdts.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_RDTS_H
#define __LIBRETRO_SDK_FORMAT_RDTS_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* DTS Coherent Acoustics, at the frame layer.
 *
 * What this reads is the frame header: where a frame starts, how long
 * it is, what rate and how many channels it carries, how many samples
 * it decodes to, and which of the extension substreams follow it.
 * That is what a frontend needs to carry a DTS stream to a decoder
 * that is not this one - a receiver over IEC 61937, where the frames
 * go across as they are - and what it needs to tell a user what a
 * file holds.
 *
 * It does not decode. The subband synthesis, the ADPCM predictors and
 * the vector codebooks are a much larger piece of work than the frame
 * layer, and nothing here pretends to stand in for them.
 *
 * A core stream comes in four shapes, which differ only in how the
 * bits are laid into bytes: 16-bit big-endian (the usual one), 16-bit
 * little-endian (byte-swapped, as a CD track ripped on a PC comes
 * out), and the two 14-bit forms, which carry fourteen bits of the
 * stream in each 16-bit word and appear on DTS audio CDs. All four
 * are recognised, and rdts_to_core() repacks any of them into the
 * plain 16-bit big-endian form a decoder or a receiver expects. */

enum rdts_kind
{
   RDTS_KIND_CORE = 0,     /* a Coherent Acoustics core frame */
   RDTS_KIND_SUBSTREAM     /* a DTS-HD extension substream (0x64582025) */
};

/* How the core frame's bits sit in its bytes. */
enum rdts_packing
{
   RDTS_PACK_16BE = 0,
   RDTS_PACK_16LE,
   RDTS_PACK_14BE,
   RDTS_PACK_14LE
};

enum rdts_status
{
   RDTS_OK        =  0,
   RDTS_NEED_MORE =  1,
   RDTS_NO_SYNC   = -1,
   RDTS_BAD       = -2
};

/* Extensions a core frame can say it is followed by. */
#define RDTS_EXT_XCH     0x0001u   /* one extra channel, the back centre */
#define RDTS_EXT_X96     0x0002u   /* the sample rate doubled */
#define RDTS_EXT_XXCH    0x0004u   /* channels past 5.1 */
#define RDTS_EXT_XBR     0x0008u   /* extended bit rate */
#define RDTS_EXT_XLL     0x0010u   /* lossless (DTS-HD Master Audio) */
#define RDTS_EXT_LBR     0x0020u   /* low bit rate (DTS Express) */

typedef struct rdts_frame_info
{
   enum rdts_kind    kind;
   enum rdts_packing packing;
   /* The whole frame on the wire, in the bytes it actually occupies -
    * so for a 14-bit stream this is larger than the core it repacks
    * to, and rdts_core_bytes is what it becomes. */
   unsigned frame_bytes;
   unsigned core_bytes;
   unsigned sample_rate;     /* of the core; X96 doubles what is heard */
   unsigned samples;         /* per channel: 32 * the block count */
   unsigned blocks;          /* PCM sample blocks, 5 to 127 */
   unsigned bitrate;         /* nominal, bits per second; 0 when open or variable */
   unsigned amode;           /* the channel arrangement code */
   bool     lfe;
   unsigned channels;        /* the core's channels, the LFE counted */
   /* The speaker mask (audio_upmix.h's bits), in the mask's own
    * ascending-bit order, for what a decoder would produce. */
   uint32_t layout;
   uint32_t extensions;      /* RDTS_EXT_* the frame or its substream names */
   bool     crc_present;
   /* The primary audio coding header, which sits behind the frame
    * header and says how the subbands are coded. What it is for here
    * is to say whether a frame could be decoded from the published
    * standard at all: ETSI TS 102 114's Annex D leaves out both of
    * the large vector codebooks - D.10.1's 4096 ADPCM coefficient
    * vectors and D.10.2's 1024 high-frequency vectors - so a frame
    * that uses either cannot be decoded from the specification, only
    * carried. needs_vq_tables says a frame does.
    *
    * Reading these costs a walk of a few hundred bits past the frame
    * header, so it is done once per frame and only for core frames. */
   bool     coding_header_read;
   unsigned subframes;        /* nSUBFS: audio subframes in the frame */
   unsigned prim_channels;    /* nPCHS: primary channels, extensions apart */
   unsigned subbands;         /* the largest nSUBS of any channel */
   unsigned vq_start_subband; /* the smallest nVQSUB: where the VQ begins */
   bool     joint_intensity;  /* any channel codes another's high subbands */
   bool     needs_vq_tables;  /* a high-frequency VQ the standard omits */
   bool     predictor_history;/* HFLAG: ADPCM history carries across frames */
   /* Substreams only: the whole assembled frame including the core
    * that preceded it, where the substream says so. */
   unsigned peak_bitrate;
} rdts_frame_info_t;

/* Whether a frame can be decoded from what the published standard
 * contains. False where it uses the high-frequency vector codebook,
 * which ETSI TS 102 114 omits ("Due to its extensive size, this table
 * is not included here"), and so cannot be decoded from the
 * specification by anyone - only carried to a decoder that has the
 * table. Requires coding_header_read. */
bool rdts_decodable_from_spec(const rdts_frame_info_t *info);

/* Reads the frame that starts at src. RDTS_NEED_MORE when len does
 * not reach the fields the size depends on, RDTS_NO_SYNC when no sync
 * word of any of the four packings is at src, RDTS_BAD when a field
 * is one the standard does not allow. The frame's bytes past the
 * header are not read. */
enum rdts_status rdts_parse_frame_info(const uint8_t *src, size_t len,
      rdts_frame_info_t *info);

/* The next offset at or after 'from' where a frame header parses.
 * Two consecutive headers must agree - the first's size must land on
 * the second - so a sync word that happens to occur inside a frame is
 * not taken for the start of one. Returns len when none does. */
size_t rdts_find_sync(const uint8_t *src, size_t len, size_t from);

/* Repacks a frame into plain 16-bit big-endian, which is what a
 * decoder and a receiver take. A frame that is already that is
 * copied. Returns the bytes written, or 0 if out_len is too small;
 * rdts_frame_info_t::core_bytes says how much that is. */
size_t rdts_to_core(const rdts_frame_info_t *info,
      const uint8_t *src, size_t src_len, uint8_t *out, size_t out_len);

/* The IEC 61937 burst a frame belongs in, and the PCM frames it
 * stands for, which is what the burst's period must be: DTS type I
 * carries 512 PCM frames, type II 1024 and type III 2048, and a
 * frame's sample count picks among them. Returns false for a frame
 * whose sample count is none of those - a receiver has no period to
 * put it in, and it cannot be carried.
 *
 * A caller sending DTS to a receiver parses the frame, repacks it
 * with rdts_to_core() where it is 14-bit or byte-swapped, asks this
 * for the type and the period, and hands both to
 * iec61937_wrap_dts(). Nothing here depends on iec61937, and nothing
 * there depends on this; the burst numbers are the contract between
 * them. */
bool rdts_burst_type(const rdts_frame_info_t *info,
      unsigned *iec61937_type, unsigned *pcm_frames);

/* Walks a buffer of frames and sums what they hold: the number of
 * frames, the samples they decode to, and the rate and layout of the
 * first. Any of the outputs may be NULL. Returns false when the
 * buffer does not begin with a frame. */
bool rdts_scan(const uint8_t *src, size_t len,
      rdts_frame_info_t *first, size_t *frames, size_t *samples);

RETRO_END_DECLS

#endif
