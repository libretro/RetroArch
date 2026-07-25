/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_transfer.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <formats/audio.h>
#include <compat/strcasestr.h>
#include <string/stdstring.h>

#ifdef HAVE_RFLAC
#include <formats/rflac.h>
#endif
#ifdef HAVE_RVORBIS
#include <formats/rvorbis.h>
#endif
#ifdef HAVE_RMP3
#include <formats/rmp3.h>
#endif
#ifdef HAVE_RWAV
#include <formats/rwav.h>
#endif
#ifdef HAVE_RMODTRACKER
#include <formats/rmodtracker.h>
#endif
#ifdef HAVE_ROPUS
#include <formats/ropus.h>
#endif
#ifdef HAVE_RAAC
#include <formats/raac.h>
#ifdef HAVE_RMP4
#include <formats/rmp4.h>
#endif
#endif
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
#include <formats/rwebm.h>
#endif

/* ---- What each codec arm implements, and what it does not ------------
 *
 * The dispatch below is uniform but the arms are not: each decoder
 * supports a different subset of the audio.h contract, and the gaps are
 * deliberate rather than pending.  Nothing degrades silently - an
 * unsupported operation fails at its entry point (start() and seek()
 * return false, info() reports what it knows and 0 for a length it does
 * not, buffer_tell() returns 0 for an arm with no windowable cursor).
 *
 * "buffer" below means set_buffer_ptr, i.e. a whole self-framed file;
 * "demuxed" means set_demuxed_ptr, i.e. a container's setup blob plus
 * its delimited packets.  Each codec is separately compile-gated; an arm
 * whose HAVE_ is off makes new() return NULL for that type.
 *
 * WAV (rwav)
 *   Does: buffer input; s16 and f32 reads; exact length; seek to any
 *     frame; 8-bit unsigned PCM, 16- and 24-bit signed PCM and 32-bit
 *     IEEE float (8-bit widened, 24-bit and float rounded at read, all
 *     assembled from the file's little-endian words).
 *     start() parses the header alone and the frames are converted out
 *     of the caller's buffer as they are asked for, so the arm holds no
 *     decoded copy and buffer_tell reports the read frontier: WAV
 *     windows like the compressed arms, and its residency is a window
 *     rather than the file.  rwav walks the chunk list, so LIST, fact,
 *     cue and the rest before the samples are no obstacle.
 *   Does not: take ADPCM/a-law - rwav rejects those, including where a
 *     WAVE_FORMAT_EXTENSIBLE SubFormat names one (rwav resolves that
 *     header, so a file extensible only for its channel count or
 *     width arrives as the PCM or float it holds).  No demuxed input.
 *     Note the mixer takes stream voices at one or two channels, so a
 *     5.1 WAV parses here and is turned away there, as a 5.1 FLAC
 *     already is.
 *
 * FLAC (rflac)
 *   Does: buffer input; s16 and f32, freely mixed; channels, rate and
 *     length from STREAMINFO; seek to any frame; buffer_tell, hence
 *     windowing (the raw cursor leads the decode position by the
 *     bitstream cache, which is the safe side for a feeder).
 *   Does not: Ogg-encapsulated FLAC - rflac takes native fLaC only.  No
 *     demuxed input, so FLAC in a container is not reachable from here.
 *     A stream whose STREAMINFO omits the total frame count reports 0.
 *
 * Vorbis (rvorbis)
 *   Does: three inputs - Ogg buffer, WebM buffer (.weba, HAVE_RWEBM,
 *     packets pulled from rwebm on demand), and demuxed (CodecPrivate +
 *     packets); s16 and f32, freely mixed; channels and rate.  The two
 *     packet-fed inputs decode raw packets where the container holds
 *     them (rvorbis_open_packets), so no Ogg is synthesised, nothing is
 *     copied, and the packets are read as they are played rather than
 *     held for the decoder's lifetime.  From an Ogg buffer: length from
 *     the last page granule, seek to any frame, rewind, buffer_tell,
 *     hence windowing.  From WebM: end trimming, exactly - Vorbis codes
 *     in overlapping blocks, so the last packet decodes past the end of
 *     the audio, and the container's DiscardPadding says by how much.
 *     It arrives on the block it applies to and is applied there,
 *     which is when the stream's true length is first known; a
 *     Duration, which rounds up past the overhang, bounds emission
 *     until then and is what info() reports before a pass has reached
 *     the end.  buffer_tell, hence windowing: the packets are decoded
 *     where the demuxer points at them, so its walk position is the
 *     compressed frontier, and start() reads no further than the
 *     header material - it does not walk the packets.  A windowed
 *     caller must say so with set_avail, and keep saying so as its
 *     window slides: rwebm's header parse walks the segment's
 *     top-level children as far as its wall, which without one is the
 *     end of the file.
 *     Seeks to any frame, exactly, from a resident WebM buffer: a
 *     Vorbis packet's frame count can be read off its header
 *     (rvorbis_packet_frames), so the walk to a target decodes
 *     nothing, and priming on the packet before it resumes output
 *     identical to the playthrough's rather than converging on it.
 *   Does not: seek to a nonzero frame on a windowed WebM buffer or
 *     from demuxed packets.  The walk a seek needs would run at a
 *     window's wall, into pages that are reserved rather than
 *     populated, and a caller's own packet set is theirs to restart;
 *     rewind is what those offer and a nonzero seek fails at the
 *     entry point.  No buffer_tell from the demuxed path, whose packets are
 *     the caller's own blob rather than the buffer set by
 *     set_buffer_ptr, so no windowing there.  No length and no end
 *     trim from it either, its packet set being the caller's:
 *     with neither a duration nor a padding stated, its last packet's
 *     overhang is handed out with the rest.  Chained or multiplexed
 *     Ogg is not handled.
 *
 * MP3 (rmp3)
 *   Does: buffer input; s16 (quantised by the synthesis filter, no float
 *     round trip) and f32; channels and rate from the first frame; seek
 *     to any PCM frame; buffer_tell, hence windowing.
 *   Does not: report a length - info() gives 0, as neither a Xing/VBRI
 *     header nor a full frame walk is done.  No gapless trim: the
 *     encoder delay and padding a Xing/LAME tag carries are not read and
 *     set_start_trim has no MP3 arm, so the decoder's own priming shows
 *     up at both ends.  No demuxed input.
 *
 * MOD (rmodtracker)
 *   Does: buffer input, MOD/S3M/XM autodetected; s16 and f32; the
 *     duration of one pass through the sequence; rewind; mixing at a
 *     rate the caller asks for through set_output_rate, a replayer
 *     synthesising at whatever rate it is given, so a caller that says
 *     what its output rate is gets audio it need not resample.
 *     Seeks anywhere, including mid-song: a module has no seek table,
 *     so the replayer restarts and walks the sequence forward with the
 *     mixing skipped, which costs time proportional to the distance and
 *     spends it on the calling thread.
 *   Does not: report the module's own channel count.  info() says 2
 *     channels because the replayer mixes to interleaved stereo, and
 *     that is what the count in this API means; a module's four or
 *     thirty-two are voices being mixed, not channels being emitted.
 *     No buffer cursor, and no windowing to use one for: the module is
 *     parsed into the replayer's own structures at open, after which
 *     the caller's bytes are not read again, and sample data is
 *     revisited at random for as long as the song plays - neither the
 *     monotonic read nor the bounded head that windowing needs.  No
 *     demuxed input.
 *
 * Opus (ropus)
 *   Does: three inputs - Ogg Opus buffer (.opus, pages walked in place
 *     per RFC 3533/7845, a packet copied only where it spans pages),
 *     WebM buffer (.weba, HAVE_RWEBM, packets pulled from rwebm on
 *     demand), and demuxed (OpusHead + delimited packets); s16 and f32;
 *     RFC 7845 pre-skip; end trimming, from the last page granule for
 *     Ogg and from the TOC total less DiscardPadding for WebM; windowed
 *     operation, via set_end_granule plus buffer_tell (Ogg only);
 *     rewind, and seek: a packet carries its duration in its first
 *     byte, so the packets before the target are stepped over without
 *     being decoded and only a pre-roll before it is decoded and
 *     dropped.  A seek made before the first read is carried out at
 *     that read, the decoding it does being what fixes the output
 *     format for the context.
 *   Does not: mix output formats on one instance - the first read
 *     latches fmt and the other entry point then errors for the life of
 *     the context, because ropus decodes s16 and f32 through separate
 *     pipelines and forbids the mix.  AAC, whose decoder is one float
 *     pipeline with two output edges, has no such restriction.  No
 *     No length from the demuxed path, whose
 *     packet set is the caller's; the buffer modes do report one, being
 *     the bound they already hold emission to.  Chained or multiplexed
 *     Ogg is not
 *     handled - pages are walked in order, the serial is ignored and
 *     the page CRC is not verified.  A page-spanning packet larger
 *     than asm_buf is an error, not a reallocation.  Channel mapping
 *     families other than 0 (mono and stereo) are refused by
 *     ropus_open, and the rate is always the 48 kHz Opus decodes at,
 *     never the original input rate.
 *
 * AAC (raac)
 *   Does: three inputs - ADTS buffer (.aac, the AudioSpecificConfig
 *     synthesised from the first header), MP4/M4A buffer (HAVE_RMP4,
 *     packets from rmp4, start trim from the track's edit list), and
 *     demuxed (ASC + delimited access units, trim via
 *     set_start_trim); s16 and f32, freely mixed at any point (the
 *     pending frames are held as raac's float output and converted on
 *     the way out of read_s16, which is what raac_decode_s16 does
 *     internally, so the s16 samples are the same either way); the
 *     encoder-delay trim, and, from an MP4, the end trim too - the
 *     declared duration bounds emission, so the tail padding coded into
 *     the last access unit is not handed out and the length info()
 *     reports is the length that comes out; rewind; buffer_tell in ADTS
 *     mode, hence windowing there.
 *     Seeks: every access unit this decoder takes is 1024 frames, so
 *     the units before the target are counted rather than decoded and
 *     only a pre-roll before it is decoded and dropped.
 *   Does not: reproduce a playthrough exactly across a seek where the
 *     encoder used noise substitution - the reset a seek needs reseeds
 *     the noise generator, and substituted noise is noise rather than
 *     coded samples.  Reports a length only from
 *     an MP4, which declares one; an ADTS stream would have to be
 *     walked to be measured, and the demuxed path's packets are the
 *     caller's.  ADTS carries no delay signalling, so
 *     that path forces the trim to 0 and is not gapless.  A lost ADTS
 *     sync is reported as end of stream rather than resynchronised.
 *     Beyond that the scope is raac's: AAC-LC mono/stereo at the
 *     1024-sample frame length, so no HE-AAC (SBR/PS), no Main/SSR/LTP,
 *     no 960-sample frames, no multichannel - and the ADTS header parse
 *     here refuses anything outside that before the decoder is opened.
 *
 * Across all arms: no file I/O and no ownership of the encoded bytes -
 * buffers are borrowed and must outlive the decoder (rmodtracker copies
 * the module at start(), so MOD alone is exempt afterwards).  No
 * resampling and no channel conversion; PCM comes out at the stream's
 * own rate and channel count and the mixer deals with
 * it.  A short read is not end of stream: only a zero-frame read returns
 * AUDIO_PROCESS_END, and END is not latched, which is what lets a grown
 * demuxed packet set resume after starvation.  AUDIO_PROCESS_ERROR_END
 * is never returned from here.  A context is single-threaded; nothing
 * below locks.  audio_decode_get_type maps extensions for WAV, FLAC,
 * Ogg Vorbis, MP3 and MOD only - .opus, .aac, .m4a and .weba have no
 * extension mapping, their types coming from the caller or from the
 * content sniffers (audio_transfer_ogg_audio_type,
 * audio_transfer_webm_audio_type), .ogg being ambiguous between Vorbis
 * and Opus in any case. */

#if defined(HAVE_RAAC) || defined(HAVE_RWAV)
/* Unit-scale float (full scale +-1.0) to s16, the conversion raac
 * applies at its own edge.  Kept in one place because two arms need
 * exactly it: the AAC arm holds raac's f32 output and converts on the
 * way out of read_s16, where the result must be the sample
 * raac_decode_s16 would have produced, and float WAV arrives at unit
 * scale already.  A float source can carry anything, including
 * non-finite values, and casting one of those to int is undefined, so
 * pin and saturate before the cast. */
static int16_t audio_transfer_unit_to_s16(float unit)
{
   float v = unit * 32768.0f;
   if (!(v > -1e9f && v < 1e9f))
      v = 0.0f;
   v += (v >= 0.0f) ? 0.5f : -0.5f;
   if (v >  32767.0f)
      v =  32767.0f;
   if (v < -32768.0f)
      v = -32768.0f;
   return (int16_t)(int)v;
}
#endif

/* One transfer context per codec. Each backend keeps only what it needs;
 * the enum 'type' handed to every entry point selects which arm runs, the
 * same switch-dispatch pattern formats/image_transfer.c uses. */

/* WAV holds no decoded copy: start() parses the header alone and the
 * reads convert frames straight out of the caller's buffer, so the arm
 * costs the header and a cursor however long the file is, and exposes a
 * read frontier the way the compressed arms do. */
#ifdef HAVE_RWAV
struct audio_transfer_wav
{
   const uint8_t *data; /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rwav_t      wav;     /* format + payload location; samples stays NULL    */
   int         opened;  /* rwav_parse succeeded                             */
   size_t      cursor;  /* next frame to hand out                           */
   size_t      framesz; /* bytes per frame: channels * bits/8               */
};
#endif

#ifdef HAVE_RFLAC
struct audio_transfer_flac
{
   const void *data;    /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rflac      *handle;  /* opened decoder, NULL until start() succeeds      */
};
#endif

#ifdef HAVE_RVORBIS
struct audio_transfer_vorbis
{
   const void *data;
   size_t      size;
   rvorbis    *handle;
   int         channels; /* cached from rvorbis_get_info at start           */
   /* Demuxed-input path (set_demuxed_ptr): the container's CodecPrivate
    * (the 3 xiph-laced Vorbis headers) and the concatenated audio packets.
    * NULL setup means the plain self-framed path. */
   const void *setup;
   size_t      setup_size;
   const void *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   /* Packet-fed operation (demuxed input, and WebM buffer input): the
    * decoder is opened on the setup headers alone and then handed one
    * bare packet at a time out of wherever it lives, which is the
    * caller's blob or the demuxer's own view of the caller's buffer.
    * Nothing is copied and nothing is reframed. */
   int         packet;      /* opened with rvorbis_open_packets         */
   size_t      pkt_index;   /* next packet in the caller's blob         */
   size_t      pkt_offset;  /* its byte offset there                    */
#ifdef HAVE_RWEBM
   rwebm_t    *demux;       /* buffer is WebM audio (.weba)             */
   int         track_idx;
   /* Resident prefix of the caller's buffer, 0 when all of it is.  A
    * windowed feeder sets this before start() so the header parse
    * stays inside the head, and raises it as the window slides. */
   size_t      avail;
#endif
   /* Emission bound, in frames, from a duration the container states;
    * -1 where none does.  Vorbis codes in overlapping blocks, so the
    * last packet decodes past the end of the audio, and only the
    * container knows where that is.  'discard' is the tail the
    * container asks to be dropped (Matroska DiscardPadding), which is
    * exact where a stated duration is only close: it is applied once
    * the final packet has been decoded and the true end is known. */
   int64_t     limit;
   int64_t     discard;
   int64_t     emitted;
};
#endif

#ifdef HAVE_RMP3
struct audio_transfer_mp3
{
   const void *data;
   size_t      size;
   rmp3        handle;   /* dr_mp3 initialises this in place (by value)      */
   int         inited;   /* handle is embedded, so track init state a flag   */
};
#endif

#ifdef HAVE_RMODTRACKER
struct audio_transfer_mod
{
   const void  *data;   /* module bytes from set_buffer_ptr (caller-owned)  */
   size_t       size;
   rmodtracker *handle; /* replayer, NULL until start() succeeds            */
   unsigned     rate;   /* requested mix rate, 0 for the replayer's default */
};
#endif

/* Vorbis and MP3 expose native s16 reads (rvorbis quantises once during
 * its interleave copy; rmp3's synthesis filter emits s16 directly), so
 * the s16 pipeline below never touches float. */

enum audio_type_enum audio_decode_get_type(const char *path)
{
   /* The extension, not a substring of the path.
    *
    * This searched the whole path for ".flac" and the rest, so a track
    * living under a directory called ".flac_backups" was decoded as
    * FLAC whatever it actually was, and "song.ogg.bak" was decoded as
    * Vorbis.  Both are the sort of path that turns up in a music
    * folder rather than a contrived one.
    *
    * Comparing the extension also removes the ordering dependency the
    * substring form had, where a path matching two of these took
    * whichever was tested first rather than whichever it ends with. */
   const char *ext = path_get_extension(path);

   if (string_is_empty(ext))
      return AUDIO_TYPE_NONE;
   if (string_is_equal_noncase(ext, "flac"))
      return AUDIO_TYPE_FLAC;
   if (string_is_equal_noncase(ext, "ogg"))
      return AUDIO_TYPE_VORBIS;
   if (string_is_equal_noncase(ext, "mp3"))
      return AUDIO_TYPE_MP3;
   if (string_is_equal_noncase(ext, "wav"))
      return AUDIO_TYPE_WAV;
#ifdef HAVE_RMODTRACKER
   if (     string_is_equal_noncase(ext, "mod")
         || string_is_equal_noncase(ext, "s3m")
         || string_is_equal_noncase(ext, "xm"))
      return AUDIO_TYPE_MOD;
#endif
   return AUDIO_TYPE_NONE;
}

#ifdef HAVE_ROPUS
struct audio_transfer_opus
{
   /* Demuxed input (set_demuxed_ptr): OpusHead as setup, concatenated
    * packets, and the per-packet byte lengths (required -- Opus packets
    * are delimited by the container). */
   const void *setup;
   size_t      setup_size;
   const uint8_t *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   /* Buffer input (set_buffer_ptr): a whole Ogg Opus (.opus) file,
    * paged per RFC 3533/7845; packets are read from the pages in
    * place, assembled only when one spans pages, and the final page's
    * granule position bounds emission the way the reference decoder's
    * does. */
   const uint8_t *buf;
   size_t      buf_size;
   int         ogg;          /* buffer is an Ogg Opus stream             */
#ifdef HAVE_RWEBM
   rwebm_t    *demux;        /* buffer is WebM audio (.weba)             */
   int         track_idx;
   size_t      avail;        /* resident prefix, 0 = all of it           */
#endif
   size_t      pg_off;       /* byte offset of the current page          */
   size_t      body_off;     /* byte offset of the current segment       */
   unsigned    seg_idx;      /* next segment in the current page         */
   unsigned    seg_count;
   size_t      audio_off;    /* first audio page (for rewind)            */
   int64_t     limit;        /* emission bound from the end granule      */
   int64_t     end_granule;  /* injected last-page granule (windowed
                              * mode): >= 0 means use it as the emission
                              * bound instead of scanning to EOF at open;
                              * -1 means do the full forward scan. */
   int64_t     emitted;      /* frames handed out so far                 */
   uint8_t     asm_buf[61500]; /* only for packets spanning pages        */
   ropus_t    *handle;
   unsigned    channels;
   size_t      pkt_index;   /* next packet to decode                     */
   size_t      pkt_offset;  /* byte offset of that packet                */
   unsigned    preskip_left;
   int         fmt;         /* 0 none, 1 s16, 2 f32 (pending buf type)   */
   /* A seek asked for but not yet carried out.  Repositioning decodes
    * a pre-roll, and decoding picks the output pipeline for good, so a
    * seek made before the first read waits here until the read says
    * which pipeline that should be.  -1 when there is none pending. */
   int64_t     seek_to;
   /* Decoded-but-unconsumed frames from the last packet. */
   size_t      pend_frames;
   size_t      pend_pos;
   int16_t     pend_s16[5760 * 2];
   float       pend_f32[5760 * 2];
};

/* Opus packet duration in 48 kHz frames from the TOC (RFC 6716 s3). */
static int64_t audio_transfer_opus_pkt_frames(const uint8_t *d, size_t n)
{
   static const int16_t fs[32] = {
      480, 960, 1920, 2880, 480, 960, 1920, 2880,   /* SILK NB/MB      */
      480, 960, 1920, 2880,                         /* SILK WB         */
      480, 960, 480, 960,                           /* hybrid          */
      120, 240, 480, 960, 120, 240, 480, 960,       /* CELT NB/WB      */
      120, 240, 480, 960, 120, 240, 480, 960        /* CELT SWB/FB     */
   };
   int count;
   if (!n)
      return 0;
   switch (d[0] & 3)
   {
      case 0: count = 1; break;
      case 3: count = n >= 2 ? (d[1] & 0x3F) : 0; break;
      default: count = 2; break;
   }
   return (int64_t)fs[d[0] >> 3] * count;
}

/* Validate the Ogg page at off and return its total size (header plus
 * body), or 0 if there is no valid page there.  On success the body
 * offset and segment count are stored. */
static size_t audio_transfer_ogg_page(const uint8_t *buf, size_t size,
      size_t off, size_t *body, unsigned *nsegs)
{
   size_t hdr, total;
   unsigned i, n;
   if (off + 27 > size)
      return 0;
   if (memcmp(buf + off, "OggS", 4) != 0 || buf[off + 4] != 0)
      return 0;
   n   = buf[off + 26];
   hdr = 27 + n;
   if (off + hdr > size)
      return 0;
   total = hdr;
   for (i = 0; i < n; i++)
      total += buf[off + 27 + i];
   if (off + total > size)
      return 0;
   if (body)
      *body = off + hdr;
   if (nsegs)
      *nsegs = n;
   return total;
}
#endif

#ifdef HAVE_RAAC
struct audio_transfer_aac
{
   /* Demuxed input (set_demuxed_ptr): the AudioSpecificConfig as
    * setup, concatenated raw access units, and the per-packet byte
    * lengths (required -- AAC access units are delimited by the
    * container). */
   const void *setup;
   size_t      setup_size;
   const uint8_t *packets;
   size_t      packets_size;
   const uint32_t *pkt_sizes;
   size_t      num_packets;
   raac_t     *handle;
   unsigned    channels;
   size_t      pkt_index;   /* next packet to decode                     */
   size_t      pkt_offset;  /* byte offset of that packet                */
   /* Buffer input (set_buffer_ptr): a whole file.  An ADTS stream
    * (.aac) is walked here directly; an MP4/M4A is demuxed with rmp4
    * when it is built in, packets streaming from the demuxer on
    * demand with the edit list's start trim picked up from the
    * track. */
   const uint8_t *buf;
   size_t      buf_size;
   int         adts;        /* buffer is an ADTS stream                  */
   size_t      adts_pos;    /* byte cursor of the next ADTS frame        */
#ifdef HAVE_RMP4
   rmp4_t     *demux;
   int         track_idx;
#endif
   /* Frames the container's edit list trims from the stream start (the
    * encoder delay); set with audio_transfer_set_start_trim before
    * audio_transfer_start. */
   uint64_t    start_trim;
   uint64_t    trim_left;
   /* Emission bound from the container's declared duration, which is
    * net of the edit list: the encoder's tail padding is coded and
    * would otherwise be handed out.  -1 where nothing declares one. */
   int64_t     limit;
   int64_t     emitted;
   /* Decoded-but-unconsumed frames from the last packet, always held
    * as raac's float output.  raac synthesises in float and converts
    * at its edge, so decoding f32 and converting on the way out of
    * read_s16 gives the same samples raac_decode_s16 would have (see
    * audio_transfer_aac_to_s16), and the two entry points can be
    * mixed at any point in the stream, including mid-packet, without
    * a pending buffer of the wrong type to strand.  Opus cannot do
    * this: ropus's s16 and f32 are separate pipelines rather than one
    * pipeline with two edges, so that arm keeps its format latch. */
   size_t      pend_frames;
   size_t      pend_pos;
   float       pend_f32[1024 * 2];
};
#endif

void *audio_transfer_new(enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
         return calloc(1, sizeof(struct audio_transfer_flac));
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
         return calloc(1, sizeof(struct audio_transfer_vorbis));
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
         return calloc(1, sizeof(struct audio_transfer_mp3));
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
         return calloc(1, sizeof(struct audio_transfer_mod));
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
         return calloc(1, sizeof(struct audio_transfer_opus));
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
         return calloc(1, sizeof(struct audio_transfer_aac));
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
         return calloc(1, sizeof(struct audio_transfer_wav));
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return NULL;
}

void audio_transfer_set_buffer_ptr(void *data, enum audio_type_enum type,
      void *ptr, size_t len)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl)
         {
            fl->data = ptr;
            fl->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (op)
         {
            op->buf         = (const uint8_t*)ptr;
            op->buf_size    = len;
            op->end_granule = -1;   /* full scan unless injected */
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (ac)
         {
            ac->buf      = (const uint8_t*)ptr;
            ac->buf_size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v)
         {
            v->data = ptr;
            v->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m)
         {
            m->data = ptr;
            m->size = len;
         }
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (md)
         {
            md->data = ptr;
            md->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (w)
         {
            w->data = (const uint8_t*)ptr;
            w->size = len;
         }
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
}

#ifdef HAVE_RVORBIS
/* --- Packet-fed Vorbis -------------------------------------------------
 *
 * WebM (and Matroska generally) carries Vorbis as raw packets: the three
 * setup headers live in CodecPrivate, xiph-laced, and the audio frames
 * are bare packets.  rvorbis decodes those where they lie
 * (rvorbis_open_packets, rvorbis_packet_decode), so no Ogg is
 * synthesised for them and nothing is copied: the decoder is pointed at
 * the demuxer's view of the caller's buffer, or at the caller's own
 * packet blob, one packet at a time.
 *
 * Overlap-add makes a packet worth zero or more frames rather than a
 * fixed count - the first one only primes the window - so the drain
 * below decodes another packet whenever the pending frames run out,
 * instead of assuming one call per buffer.
 */

/* Split the xiph-laced CodecPrivate into its 3 header pointers/lengths.
 * Returns 1 on success. */
static int audio_vorbis_split_setup(const uint8_t *priv, size_t size,
      const uint8_t *hdr[3], uint32_t hlen[3])
{
   size_t p, sum;
   int    i;
   if (!priv || size < 3 || priv[0] != 2) /* nheaders-1 == 2 */
      return 0;
   p = 1;
   for (i = 0; i < 2; i++)
   {
      uint32_t l = 0;
      while (p < size && priv[p] == 255) { l += 255; p++; }
      if (p >= size) return 0;
      l += priv[p++];
      hlen[i] = l;
   }
   sum = (size_t)hlen[0] + hlen[1];
   if (p + sum > size)
      return 0;
   hlen[2]  = (uint32_t)(size - p - sum);
   hdr[0]   = priv + p;
   hdr[1]   = priv + p + hlen[0];
   hdr[2]   = priv + p + hlen[0] + hlen[1];
   return 1;
}

/* The next coded packet, wherever this context's packets come from.
 * Returns 1 with the bytes, 0 at end of stream, < 0 on a blob whose
 * sizes do not agree with its length. */
static int audio_transfer_vorbis_pull(struct audio_transfer_vorbis *v,
      const uint8_t **pdata, uint32_t *plen)
{
#ifdef HAVE_RWEBM
   if (v->demux)
   {
      /* .weba: pull the next packet from the demuxer, which hands back
       * a pointer into the caller's buffer. */
      rwebm_packet pkt;
      for (;;)
      {
         int r = rwebm_read_packet(v->demux, &pkt);
         /* The walk reached the resident wall rather than the end of
          * the stream: the feeder has not got here yet.  Report it
          * apart from end of stream, so the drain returns short and
          * the next call resumes instead of the sound ending. */
         if (r == RWEBM_READ_AGAIN)
            return -2;
         if (r != 1)
            return 0;
         if (pkt.track == v->track_idx)
            break;
      }
      *pdata = pkt.data;
      *plen  = (uint32_t)pkt.size;
      /* DiscardPadding is attached to the block it applies to, so it
       * arrives with the packet rather than having to be found by
       * walking to the end first.  Converted here, where the rate is
       * known, and consumed by the drain once this packet has been
       * decoded and its yield is countable. */
      if (pkt.discard_padding > 0)
      {
         int64_t rate = (int64_t)rvorbis_get_info(v->handle).sample_rate;
         if (rate > 0)
            v->discard = (pkt.discard_padding * rate + 500000000)
               / 1000000000;
      }
      return 1;
   }
#endif
   if (v->pkt_index >= v->num_packets)
      return 0;
   /* An unsplit blob is one packet: the whole of it. */
   *plen = v->pkt_sizes ? v->pkt_sizes[v->pkt_index]
                        : (uint32_t)v->packets_size;
   if (v->pkt_offset + *plen > v->packets_size)
      return -1;
   *pdata = (const uint8_t*)v->packets + v->pkt_offset;
   v->pkt_offset += *plen;
   v->pkt_index++;
   return 1;
}

/* Drain 'frames' frames out of the packet stream in one of the two
 * pipelines (s16 selects it; out16 or outf is the buffer that matches).
 * Decoded frames left over from the last packet go out first, and a
 * fresh packet is decoded whenever they run out.  Returns the frames
 * written, short of the ask at end of stream or at the container's
 * stated end. */
static size_t audio_transfer_vorbis_drain(struct audio_transfer_vorbis *v,
      int s16, int16_t *out16, float *outf, size_t frames)
{
   size_t done = 0;
   int    ch   = v->channels;
   if (ch <= 0)
      return 0;
   for (;;)
   {
      const uint8_t *pd = NULL;
      uint32_t       pl = 0;
      int            want, got;
      /* Vorbis codes in overlapping blocks, so the last packet decodes
       * past the end of the audio.  Where the container says where that
       * is, stop there rather than handing out the overhang.  Checked
       * every pass, not once: the bound tightens to its exact value
       * partway through the buffer that reaches the end. */
      if (v->limit >= 0)
      {
         int64_t left = v->limit - (v->emitted + (int64_t)done);
         if (left <= 0)
            break;
         if ((int64_t)(frames - done) > left)
            frames = done + (size_t)left;
      }
      if (done >= frames)
         break;
      if (rvorbis_packet_pending(v->handle) > 0)
      {
         want = (int)(frames - done);
         got  = s16
            ? rvorbis_packet_read_s16(v->handle, ch,
                  out16 + done * (size_t)ch, want * ch)
            : rvorbis_packet_read_float(v->handle, ch,
                  outf + done * (size_t)ch, want * ch);
         if (got <= 0)
            break;
         done += (size_t)got;
         continue;
      }
      /* Nothing pending: decode another packet.  It may yield nothing
       * (the first one primes the window, and a packet can carry no
       * audio at all), which is not the end of anything - pull again. */
      if (audio_transfer_vorbis_pull(v, &pd, &pl) != 1)
         break;
      if (rvorbis_packet_decode(v->handle, pd, pl, s16) < 0)
         break;
      /* This packet carried a padding, so the frames it just yielded
       * run past the end of the audio by that much.  Its yield is
       * countable now, which is what makes the exact end knowable; a
       * stated duration only rounds up past the overhang, so this
       * supersedes it.  Consumed here, and set again by the pull if
       * the stream is rewound and replayed. */
      if (v->discard > 0)
      {
         int64_t out   = v->emitted + (int64_t)done;
         int64_t total = out + rvorbis_packet_pending(v->handle)
                             - v->discard;
         /* A padding longer than the packet's yield would ask for
          * frames already handed out; the bound stops at what has
          * gone rather than going backwards. */
         if (total < out)
            total = out;
         if (v->limit < 0 || total < v->limit)
            v->limit = total;
         v->discard = 0;
      }
   }
   v->emitted += (int64_t)done;
   return done;
}
#endif

bool audio_transfer_set_demuxed_ptr(void *data, enum audio_type_enum type,
      const void *setup, size_t setup_size,
      const void *packets, size_t packets_size,
      const uint32_t *sizes, size_t num_packets)
{
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v)
            return false;
         v->setup        = setup;
         v->setup_size   = setup_size;
         v->packets      = packets;
         v->packets_size = packets_size;
         v->pkt_sizes    = sizes;
         v->num_packets  = num_packets;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op)
            return false;
         op->setup        = setup;
         op->setup_size   = setup_size;
         op->packets      = (const uint8_t*)packets;
         op->packets_size = packets_size;
         op->pkt_sizes    = sizes;
         op->num_packets  = num_packets;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         ac->setup        = setup;
         ac->setup_size   = setup_size;
         ac->packets      = (const uint8_t*)packets;
         ac->packets_size = packets_size;
         ac->pkt_sizes    = sizes;
         ac->num_packets  = num_packets;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   (void)data; (void)setup; (void)setup_size;
   (void)packets; (void)packets_size; (void)sizes; (void)num_packets;
   return false;
}

void audio_transfer_set_output_rate(void *data, enum audio_type_enum type,
      unsigned rate)
{
#ifdef HAVE_RMODTRACKER
   /* A tracker synthesises, so it can be asked to synthesise at the
    * rate the caller wants and spare everyone a resampler.  The decoders
    * reproduce a recording made at a rate the stream itself states,
    * which is not a thing a caller gets to choose. */
   if (data && type == AUDIO_TYPE_MOD)
      ((struct audio_transfer_mod*)data)->rate = rate;
#else
   (void)data;
   (void)type;
   (void)rate;
#endif
}

/* Raise the resident prefix of the caller's buffer.  A windowed feeder
 * calls this before start(), so the container header parse is bounded
 * by the head, and again as the window slides.  The value is a prefix
 * even though a window is not one: the demuxer only ever reads forward
 * at its own cursor, which the feeder keeps inside the window, so the
 * wall is what stops a parse running off to the end of the file rather
 * than a claim about every byte below it.  Monotonic - the demuxer
 * clamps a value below the one it already has.  No-op for a type with
 * no windowed container path. */
void audio_transfer_set_avail(void *data, enum audio_type_enum type,
      size_t avail)
{
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v)
            return;
         v->avail = avail;
         if (v->demux)
            rwebm_set_avail(v->demux, avail);
         return;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op)
            return;
         op->avail = avail;
         if (op->demux)
            rwebm_set_avail(op->demux, avail);
         return;
      }
#endif
      default:
         break;
   }
#endif
   (void)data; (void)type; (void)avail;
}

#ifdef HAVE_ROPUS
/* Windowed Opus: inject the last Ogg page's granule so buffer setup
 * skips the full-file end-granule scan (see audio_transfer_opus's
 * end_granule).  No-op for any other type.  Must be called after
 * set_buffer_ptr and before audio_transfer_start. */
void audio_transfer_set_end_granule(void *data, enum audio_type_enum type,
      int64_t end_granule)
{
   if (data && type == AUDIO_TYPE_OPUS)
      ((struct audio_transfer_opus*)data)->end_granule = end_granule;
}
#endif

enum audio_type_enum audio_transfer_ogg_audio_type(const void *buf,
      size_t len)
{
   const uint8_t *b = (const uint8_t*)buf;
   unsigned nsegs, i;
   size_t first, plen = 0;
   if (!b || len < 28
         || b[0] != 'O' || b[1] != 'g' || b[2] != 'g' || b[3] != 'S'
         || b[4] != 0)
      return AUDIO_TYPE_NONE;
   /* first page: the identification header sits alone in it; its
    * opening bytes name the codec */
   nsegs = b[26];
   first = 27 + nsegs;
   if (len < first + 8)
      return AUDIO_TYPE_NONE;
   for (i = 0; i < nsegs; i++)
      plen += b[27 + i];
   if (plen < 7 || len < first + plen)
      return AUDIO_TYPE_NONE;
#ifdef HAVE_ROPUS
   if (plen >= 8 && !memcmp(b + first, "OpusHead", 8))
      return AUDIO_TYPE_OPUS;
#endif
#ifdef HAVE_RVORBIS
   if (!memcmp(b + first, "\x01vorbis", 7))
      return AUDIO_TYPE_VORBIS;
#endif
   return AUDIO_TYPE_NONE;
}

#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
/* Open a WebM container with the header parse bounded by the bytes the
 * caller says are actually there.
 *
 * rwebm_open_memory walks the segment's top-level children as far as
 * its wall, which at avail == size is segment_end: an element header
 * read at every cluster boundary in the file.  Whole-file callers can
 * afford that; a windowed one holds a head and a sliding window with
 * reserved, unpopulated pages between, and the walk would read them.
 * 'avail' is that caller's resident prefix, and 0 means the whole
 * buffer is there.
 *
 * A windowed caller's wall then stays where it put it, to be raised by
 * audio_transfer_set_avail as the window slides.  A whole-file caller's
 * goes straight to the end, which is the old behaviour exactly. */
static rwebm_t *audio_transfer_webm_open(const uint8_t *data, size_t size,
      size_t avail)
{
   rwebm_t *m = rwebm_open_memory_avail(data, size,
         (avail && avail < size) ? avail : size, NULL);
   if (m && !avail)
      rwebm_set_avail(m, size);
   return m;
}
#endif

enum audio_type_enum audio_transfer_webm_audio_type(const void *buf,
      size_t len)
{
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   rwebm_t *wm;
   int i;
   enum audio_type_enum found = AUDIO_TYPE_NONE;
   const uint8_t *b = (const uint8_t*)buf;
   if (!b || len < 4
         || b[0] != 0x1A || b[1] != 0x45 || b[2] != 0xDF || b[3] != 0xA3)
      return AUDIO_TYPE_NONE;
   if (!(wm = audio_transfer_webm_open(b, len, 0)))
      return AUDIO_TYPE_NONE;
   for (i = 0; i < rwebm_num_tracks(wm) && found == AUDIO_TYPE_NONE; i++)
   {
      const rwebm_track *t = rwebm_get_track(wm, i);
      if (!t || t->type != RWEBM_TRACK_AUDIO || !t->codec_private_size)
         continue;
#ifdef HAVE_ROPUS
      if (t->codec == RWEBM_CODEC_OPUS)
         found = AUDIO_TYPE_OPUS;
#endif
#ifdef HAVE_RVORBIS
      if (t->codec == RWEBM_CODEC_VORBIS)
         found = AUDIO_TYPE_VORBIS;
#endif
   }
   rwebm_close(wm);
   return found;
#else
   (void)buf;
   (void)len;
   return AUDIO_TYPE_NONE;
#endif
}

bool audio_transfer_set_start_trim(void *data, enum audio_type_enum type,
      uint64_t frames)
{
   switch (type)
   {
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         ac->start_trim = frames;
         ac->trim_left  = frames;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   /* every arm that reads these is compiled out in this build, the
    * way set_demuxed_ptr's already are */
   (void)data;
   (void)frames;
   return false;
}

bool audio_transfer_start(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->data)
            return false;
         fl->handle = rflac_open_memory(fl->data, fl->size);
         return fl->handle != NULL;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int64_t duration_ns = 0;
         int     err         = 0;
         if (!v)
            return false;
         v->limit   = -1;
         v->emitted = 0;
#ifdef HAVE_RWEBM
         /* Buffer mode, WebM audio (.weba): the setup headers are the
          * Vorbis track's CodecPrivate and the audio packets stay in
          * the container, read out one at a time as they are decoded.
          * The demuxer is held open for that, and it aliases the
          * caller's buffer rather than copying out of it. */
         if (!v->setup && v->data && v->size >= 4
               && ((const uint8_t*)v->data)[0] == 0x1A
               && ((const uint8_t*)v->data)[1] == 0x45
               && ((const uint8_t*)v->data)[2] == 0xDF
               && ((const uint8_t*)v->data)[3] == 0xA3)
         {
            const rwebm_track *at = NULL;
            int i;
            if (!(v->demux = audio_transfer_webm_open(
                        (const uint8_t*)v->data, v->size, v->avail)))
               return false;
            v->track_idx = -1;
            for (i = 0; i < rwebm_num_tracks(v->demux); i++)
            {
               const rwebm_track *t = rwebm_get_track(v->demux, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_VORBIS
                     && t->codec_private_size)
               {
                  at           = t;
                  v->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            v->setup      = at->codec_private;
            v->setup_size = at->codec_private_size;
            duration_ns   = rwebm_duration_ns(v->demux);
         }
#endif
         if (v->setup)
         {
            /* Packet-fed: the identification and setup headers come out
             * of the xiph-laced CodecPrivate and are parsed where they
             * lie.  The comment header carries no decode state and is
             * not wanted. */
            const uint8_t *hdr[3];
            uint32_t       hlen[3];
            if (!audio_vorbis_split_setup((const uint8_t*)v->setup,
                     v->setup_size, hdr, hlen))
               return false;
            v->handle = rvorbis_open_packets(hdr[0], (int)hlen[0],
                  hdr[2], (int)hlen[2], &err, NULL);
            if (!v->handle)
               return false;
            v->packet     = 1;
            v->pkt_index  = 0;
            v->pkt_offset = 0;
         }
         else
         {
            if (!v->data)
               return false;
            v->handle = rvorbis_open_memory((const unsigned char*)v->data,
                  (int)v->size, &err, NULL);
            if (!v->handle)
               return false;
         }
         v->channels = rvorbis_get_info(v->handle).channels;
         /* A WebM Duration is the length of the audio, which the last
          * packet's overlap-add tail runs past; bound emission by it
          * for as long as that is the best statement available.  It
          * rounds up, so where a DiscardPadding is also present the
          * drain replaces this with the exact end once the final
          * packet has been decoded.  The demuxed path has neither -
          * its packet set is the caller's - and stays unbounded. */
         if (duration_ns > 0)
         {
            int64_t rate = (int64_t)rvorbis_get_info(v->handle).sample_rate;
            if (rate > 0)
               v->limit = (duration_ns * rate + 500000000) / 1000000000;
         }
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->data)
            return false;
         m->inited = (rmp3_init_memory(&m->handle, m->data, m->size) != 0);
         return m->inited != 0;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->data)
            return false;
         if (md->rate)
         {
            md->handle = rmodtracker_open_memory_rate(md->data, md->size,
                  (int)md->rate);
            /* a rate the replayer will not take is not worth failing
             * the load over: mix at the default and let the caller
             * resample, which is what it did before asking */
            if (md->handle)
               return true;
         }
         md->handle = rmodtracker_open_memory(md->data, md->size);
         return md->handle != NULL;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->data)
            return false;
         /* header only: no allocation, and the payload is not read
          * here, so a caller streaming the file needs just the head
          * resident at open */
         if (rwav_parse(&w->wav, w->data, w->size) != RWAV_ITERATE_DONE)
            return false;
         /* rwav admits 8-, 16- and 24-bit integer PCM and 32-bit
          * IEEE float, and all four convert on the way out below */
         if (     w->wav.bitspersample != 16
               && w->wav.bitspersample != 8
               && w->wav.bitspersample != 24
               && w->wav.bitspersample != 32)
            return false;
         w->framesz = (size_t)w->wav.numchannels
                    * (size_t)(w->wav.bitspersample / 8);
         w->opened  = 1;
         w->cursor  = 0;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op)
            return false;
         /* Unbounded unless a container says otherwise.  The two buffer
          * modes below work a bound out and overwrite this; the demuxed
          * path has none, and must not be left holding the zero calloc
          * gave it - a bound of zero reads as a stream with no frames
          * in it, which is what refused every seek on that path. */
         op->limit = -1;
#ifdef HAVE_RWEBM
         /* buffer mode, WebM audio (.weba): demux with rwebm; packets
          * stream from the demuxer on demand.  The exact decodable
          * length comes from the packets' TOC durations less pre-skip
          * and the container's end trimming (DiscardPadding). */
         if (!op->setup && op->buf && op->buf_size >= 4
               && op->buf[0] == 0x1A && op->buf[1] == 0x45
               && op->buf[2] == 0xDF && op->buf[3] == 0xA3)
         {
            const rwebm_track *at = NULL;
            int i;
            rwebm_packet pkt;
            int64_t toc = 0, discard_ns = 0, preskip = 0;
            if (!(op->demux = audio_transfer_webm_open(op->buf,
                     op->buf_size, op->avail)))
               return false;
            op->track_idx = -1;
            for (i = 0; i < rwebm_num_tracks(op->demux); i++)
            {
               const rwebm_track *t = rwebm_get_track(op->demux, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_OPUS
                     && t->codec_private_size)
               {
                  at            = t;
                  op->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            op->handle = ropus_open(at->codec_private,
                  at->codec_private_size);
            if (!op->handle)
               return false;
            while (rwebm_read_packet(op->demux, &pkt) == 1)
            {
               if (pkt.track != op->track_idx)
                  continue;
               toc += audio_transfer_opus_pkt_frames(pkt.data, pkt.size);
               if (pkt.discard_padding > 0)
                  discard_ns += pkt.discard_padding;
            }
            rwebm_rewind(op->demux);
            preskip   = (int64_t)ropus_preskip(op->handle);
            op->limit = -1;
            if (toc > 0)
            {
               op->limit = toc - preskip
                  - (discard_ns * 48000 + 500000000) / 1000000000;
               if (op->limit < 0)
                  op->limit = 0;
            }
            op->emitted = 0;
         }
         else
#endif
         /* buffer mode: a whole Ogg Opus file.  The ID header sits
          * alone on the first page (RFC 7845), the comment header
          * finishes on its own page(s), and audio pages follow; the
          * final page's granule position, less the pre-skip, is the
          * stream's exact decodable length. */
         if (!op->setup && op->buf)
         {
            size_t body = 0, off, total;
            unsigned nsegs = 0;
            int64_t  last_granule = -1;
            total = audio_transfer_ogg_page(op->buf, op->buf_size, 0,
                  &body, &nsegs);
            if (!total || !(op->buf[5] & 0x02))   /* first page: BOS  */
               return false;
            op->handle = ropus_open(op->buf + body, total - (body - 0));
            if (!op->handle)
               return false;
            /* skip the comment header: pages until a lacing value
             * below 255 completes the packet */
            off = total;
            for (;;)
            {
               unsigned i;
               int done = 0;
               size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
                     off, &body, &nsegs);
               if (!psz)
                  return false;
               for (i = 0; i < nsegs; i++)
                  if (op->buf[off + 27 + i] < 255)
                     done = 1;
               off += psz;
               if (done)
                  break;
            }
            op->ogg       = 1;
            op->audio_off = off;
            op->pg_off    = off;
            op->seg_idx   = 0;
            op->body_off  = 0;
            /* End granule: normally walk the remaining page headers
             * once to the last granule.  In windowed mode the tail is
             * not resident, so the feeder injected the last-page
             * granule (found from a bounded tail peek) - use it and
             * skip the walk, which would read past the head window. */
            if (op->end_granule >= 0)
               last_granule = op->end_granule;
            else while (off < op->buf_size)
            {
               size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
                     off, NULL, NULL);
               uint64_t g;
               unsigned k;
               if (!psz)
                  break;
               g = 0;
               for (k = 0; k < 8; k++)
                  g |= (uint64_t)op->buf[off + 6 + k] << (8 * k);
               if (g != (uint64_t)-1)
                  last_granule = (int64_t)g;
               off += psz;
            }
            op->limit = -1;
            if (last_granule >= 0)
            {
               op->limit = last_granule - (int64_t)ropus_preskip(op->handle);
               if (op->limit < 0)
                  op->limit = 0;
            }
            op->emitted = 0;
         }
         else
         {
            if (!op->setup || !op->packets || !op->pkt_sizes)
               return false;      /* no input set                        */
            op->handle = ropus_open(op->setup, op->setup_size);
            if (!op->handle)
               return false;
         }
         op->channels     = ropus_channels(op->handle);
         op->seek_to      = -1;
         op->preskip_left = ropus_preskip(op->handle);
         op->pkt_index    = 0;
         op->pkt_offset   = 0;
         op->pend_frames  = 0;
         op->pend_pos     = 0;
         op->fmt          = 0;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return false;
         ac->limit = -1;   /* unbounded unless a container says otherwise */
         /* buffer mode, ADTS stream: the header carries the setup,
          * so synthesise the AudioSpecificConfig from it (LC only,
          * matching the decoder's scope) and walk frames in place */
         if (!ac->setup && ac->buf && ac->buf_size >= 7
               && ac->buf[0] == 0xFF && (ac->buf[1] & 0xF6) == 0xF0)
         {
            unsigned profile = (ac->buf[2] >> 6) & 3;   /* 1 = LC     */
            unsigned sfi     = (ac->buf[2] >> 2) & 15;
            unsigned chcfg   = ((ac->buf[2] & 1) << 2)
                             | ((ac->buf[3] >> 6) & 3);
            uint8_t  asc[2];
            if (profile != 1 || sfi > 12 || chcfg < 1 || chcfg > 2)
               return false;
            asc[0] = (uint8_t)((2u << 3) | (sfi >> 1));
            asc[1] = (uint8_t)(((sfi & 1) << 7) | (chcfg << 3));
            ac->handle = raac_open(asc, 2);
            if (!ac->handle)
               return false;
            ac->adts       = 1;
            ac->adts_pos   = 0;
            ac->start_trim = 0;   /* ADTS carries no delay signalling  */
         }
         else
#ifdef HAVE_RMP4
         /* buffer mode: a whole MP4/M4A; demux it here */
         if (!ac->setup && ac->buf)
         {
            const rmp4_track *at = NULL;
            int i;
            if (!(ac->demux = rmp4_open_memory(ac->buf, ac->buf_size)))
               return false;
            ac->track_idx = -1;
            for (i = 0; i < rmp4_num_tracks(ac->demux); i++)
            {
               const rmp4_track *t = rmp4_get_track(ac->demux, i);
               if (t && t->type == RMP4_TRACK_AUDIO
                     && t->codec == RMP4_CODEC_AAC
                     && t->codec_private_size)
               {
                  at            = t;
                  ac->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            ac->handle = raac_open(at->codec_private,
                  at->codec_private_size);
            if (!ac->handle)
               return false;
            /* the track's edit list carries the encoder delay */
            ac->start_trim = at->media_skip;
            {
               int64_t ns = rmp4_duration_ns(ac->demux);
               unsigned sr = raac_sample_rate(ac->handle);
               if (ns > 0 && sr)
                  ac->limit = (ns * (int64_t)sr + 500000000) / 1000000000;
            }
         }
         else
#endif
         {
            if (!ac->setup || !ac->packets || !ac->pkt_sizes)
               return false;      /* no input set                        */
            ac->handle = raac_open(ac->setup, ac->setup_size);
            if (!ac->handle)
               return false;
         }
         ac->channels    = raac_channels(ac->handle);
         ac->trim_left   = ac->start_trim;
         ac->emitted     = 0;
         ac->pkt_index   = 0;
         ac->pkt_offset  = 0;
         ac->pend_frames = 0;
         ac->pend_pos    = 0;
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_is_valid(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         return (fl && fl->handle);
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         return (v && v->handle);
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         return (m && m->inited);
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         return (md && md->handle);
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         return w && w->opened;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         return op && op->handle;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         return ac && ac->handle;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

bool audio_transfer_info(void *data, enum audio_type_enum type,
      unsigned *channels, unsigned *rate, uint64_t *total_frames)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         if (channels)
            *channels     = (unsigned)fl->handle->channels;
         if (rate)
            *rate         = (unsigned)fl->handle->sampleRate;
         if (total_frames)
            *total_frames = (uint64_t)fl->handle->totalPCMFrameCount;
         return true;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         rvorbis_info info;
         if (!v || !v->handle)
            return false;
         info = rvorbis_get_info(v->handle);
         if (channels)
            *channels     = (unsigned)info.channels;
         if (rate)
            *rate         = (unsigned)info.sample_rate;
         if (total_frames)
         {
            /* stream_length_in_samples walks the Ogg pages to the last
             * granule, which a packet-fed context has none of: its
             * length is whatever bound the container stated, and 0
             * where it stated none. */
            if (v->packet)
               *total_frames = (v->limit >= 0) ? (uint64_t)v->limit : 0;
            else
               *total_frames =
                  (uint64_t)rvorbis_stream_length_in_samples(v->handle);
         }
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         if (channels)
            *channels     = (unsigned)m->handle.channels;
         if (rate)
            *rate         = (unsigned)m->handle.sampleRate;
         if (total_frames) /* streaming; length not tracked here */
            *total_frames = 0;
         return true;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return false;
         if (channels)  /* the replayer always mixes interleaved stereo */
            *channels     = 2;
         if (rate)
            *rate         = (unsigned)rmodtracker_sample_rate(md->handle);
         if (total_frames) /* one pass through the sequence */
            *total_frames = (uint64_t)rmodtracker_duration_frames(md->handle);
         return true;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->opened)
            return false;
         if (channels)
            *channels     = w->wav.numchannels;
         if (rate)
            *rate         = w->wav.samplerate;
         if (total_frames) /* WAV is fully decoded, so length is known */
            *total_frames = (uint64_t)w->wav.numsamples;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         int bounded;
         if (!op || !op->handle)
            return false;
         if (channels)
            *channels = op->channels;
         if (rate)
            *rate = 48000;
         bounded = op->ogg;
#ifdef HAVE_RWEBM
         if (op->demux)
            bounded = 1;
#endif
         /* The buffer modes work the exact decodable length out at open
          * - the last page's granule for Ogg, the packets' TOC total
          * less the container's end trimming for WebM - and hold
          * emission to it as they go, so a caller that wants a length
          * need not scan for one itself.  The demuxed path has no such
          * bound: the packet set belongs to the caller and may still be
          * growing under it. */
         if (total_frames)
            *total_frames = (bounded && op->limit >= 0)
               ? (uint64_t)op->limit : 0;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         unsigned srate;
         if (!ac || !ac->handle)
            return false;
         srate = raac_sample_rate(ac->handle);
         if (channels)
            *channels = ac->channels;
         if (rate)
            *rate = srate;
         if (total_frames)
         {
            /* An MP4 declares its duration, already net of the edit
             * list, so the count is what the caller will actually be
             * handed.  An ADTS stream declares nothing and would have
             * to be walked frame by frame to be measured, which is the
             * whole file and, under windowing, most of it not resident;
             * the demuxed path's packets are the caller's own. */
            *total_frames = (ac->limit >= 0) ? (uint64_t)ac->limit : 0;
         }
         return true;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

#ifdef HAVE_ROPUS
/* Assemble the next Opus packet from the Ogg pages.  Returns 1 with
 * the packet bytes (aliasing the buffer unless it spans pages, in
 * which case it is copied into asm_buf), 0 at end of stream, < 0 on a
 * malformed stream. */
static int audio_transfer_opus_next_pkt(struct audio_transfer_opus *op,
      const uint8_t **pdata, uint32_t *plen)
{
   size_t asm_len = 0;
   int    spans   = 0;
   for (;;)
   {
      size_t body = 0;
      unsigned nsegs = 0;
      size_t psz = audio_transfer_ogg_page(op->buf, op->buf_size,
            op->pg_off, &body, &nsegs);
      size_t start;
      size_t run = 0;
      int    done = 0;
      if (!psz)
         return asm_len ? -1 : 0;   /* out of pages                   */
      if (op->seg_idx == 0)
         op->body_off = body;
      if (asm_len && op->seg_idx == 0 && !(op->buf[op->pg_off + 5] & 0x01))
         return -1;                 /* continuation page not flagged  */
      start = op->body_off;
      while (op->seg_idx < nsegs)
      {
         unsigned lace = op->buf[op->pg_off + 27 + op->seg_idx];
         run += lace;
         op->seg_idx++;
         if (lace < 255)
         {
            done = 1;
            break;
         }
      }
      op->body_off = start + run;
      if (op->seg_idx >= nsegs && !done)
      {
         /* packet continues on the next page: stash what we have */
         if (run)
         {
            if (asm_len + run > sizeof(op->asm_buf))
               return -1;
            memcpy(op->asm_buf + asm_len, op->buf + start, run);
            asm_len += run;
         }
         spans       = 1;
         op->pg_off += psz;
         op->seg_idx = 0;
         continue;
      }
      if (op->seg_idx >= nsegs)
      {
         op->pg_off += psz;
         op->seg_idx = 0;
      }
      if (!spans)
      {
         *pdata = op->buf + start;
         *plen  = (uint32_t)run;
      }
      else
      {
         if (asm_len + run > sizeof(op->asm_buf))
            return -1;
         memcpy(op->asm_buf + asm_len, op->buf + start, run);
         asm_len += run;
         *pdata  = op->asm_buf;
         *plen   = (uint32_t)asm_len;
      }
      return 1;
   }
}

/* Decode the next Opus packet into the pending buffer in the requested
 * format (1 = s16, 2 = f32), honouring pre-skip.  Returns frames now
 * pending, 0 at end of stream, < 0 on error. */
/* The next coded packet, wherever this context's packets come from.
 * Returns 1 with the bytes, 0 at end of stream, < 0 on a malformed
 * stream. */
static int audio_transfer_opus_pull(struct audio_transfer_opus *op,
      const uint8_t **pdata, uint32_t *plen)
{
#ifdef HAVE_RWEBM
   if (op->demux)
   {
      /* .weba: pull the next packet from the demuxer */
      rwebm_packet pkt;
      for (;;)
      {
         int r = rwebm_read_packet(op->demux, &pkt);
         /* Resident wall, not end of stream: see the Vorbis pull. */
         if (r == RWEBM_READ_AGAIN)
            return -2;
         if (r != 1)
            return 0;
         if (pkt.track == op->track_idx)
            break;
      }
      *pdata = pkt.data;
      *plen  = (uint32_t)pkt.size;
      return 1;
   }
#endif
   if (op->ogg)
      return audio_transfer_opus_next_pkt(op, pdata, plen);
   if (op->pkt_index >= op->num_packets)
      return 0;
   *plen = op->pkt_sizes[op->pkt_index];
   if (op->pkt_offset + *plen > op->packets_size)
      return -1;
   *pdata = op->packets + op->pkt_offset;
   op->pkt_offset += *plen;
   op->pkt_index++;
   return 1;
}

static int audio_transfer_opus_fill(struct audio_transfer_opus *op, int fmt);

/* Frames decoded before the target and thrown away, so the decoder has
 * converged by the time the caller sees anything.  RFC 7845 s4.2 asks
 * for 80 ms of that; the rest is headroom for the walk below, which
 * stops on a packet boundary and so can overshoot where it meant to
 * stop by as much as one packet - 120 ms, the longest Opus codes.
 * Together they guarantee the walk lands before the target with at
 * least the 80 ms still to decode. */
#define AUDIO_OPUS_PREROLL (3840 + 5760)

/* Rewind and walk forward to 'frame'.  The walk itself decodes nothing:
 * a packet's duration is in its TOC byte, so the packets before the
 * target can be stepped over by reading one byte of each.  Only the
 * pre-roll is decoded, and thrown away.  Returns the frame reached,
 * or < 0 if the stream ends first. */
static void audio_transfer_opus_to_head(struct audio_transfer_opus *op)
{
   ropus_reset(op->handle);
#ifdef HAVE_RWEBM
   if (op->demux)
      rwebm_rewind(op->demux);
#endif
   op->pkt_index    = 0;
   op->pkt_offset   = 0;
   op->pg_off       = op->audio_off;
   op->seg_idx      = 0;
   op->body_off     = 0;
   op->emitted      = 0;
   op->pend_frames  = 0;
   op->pend_pos     = 0;
   op->preskip_left = ropus_preskip(op->handle);
}

static int64_t audio_transfer_opus_seek_to(struct audio_transfer_opus *op,
      int64_t frame, int fmt)
{
   int64_t  pos  = 0;      /* frames emitted, i.e. after the pre-skip  */
   unsigned skip;
   int64_t  stop = frame - AUDIO_OPUS_PREROLL;

   audio_transfer_opus_to_head(op);
   skip = ropus_preskip(op->handle);
   if (stop < 0)
      stop = 0;
   while (pos < stop)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int64_t d;
      int r = audio_transfer_opus_pull(op, &pdata, &plen);
      if (r < 0)
         return -1;
      if (r == 0)
         return -1;        /* asked past the end of the stream         */
      d = audio_transfer_opus_pkt_frames(pdata, plen);
      if (d <= 0)
         return -1;
      /* the head of the stream is pre-skip, which is discarded rather
       * than emitted, so it does not count towards the position */
      if (skip)
      {
         unsigned taken = (d < (int64_t)skip) ? (unsigned)d : skip;
         skip -= taken;
         d    -= (int64_t)taken;
      }
      /* The packet has been consumed, so it counts whether or not it
       * takes the walk past where it meant to stop; the pre-roll is
       * sized so that overshooting by one packet still leaves the
       * target ahead. */
      pos += d;
   }
   /* Whatever pre-skip is left belongs to the frames the decode below
    * is about to produce. */
   op->preskip_left = skip;
   op->emitted      = pos;
   op->pend_frames  = 0;
   op->pend_pos     = 0;
   /* Decode from here, discarding, so the decoder is converged and the
    * position is exact rather than packet-aligned. */
   while (pos < frame)
   {
      int64_t take;
      int r = audio_transfer_opus_fill(op, fmt);
      if (r < 0)
         return -1;
      if (r == 0)
         return -1;
      take = frame - pos;
      if (take > (int64_t)op->pend_frames)
         take = (int64_t)op->pend_frames;
      op->pend_pos    += (size_t)take;
      op->pend_frames -= (size_t)take;
      pos             += take;
   }
   return pos;
}

static int audio_transfer_opus_fill(struct audio_transfer_opus *op, int fmt)
{
   while (op->pend_frames == 0)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int r;
      unsigned skip;
      r = audio_transfer_opus_pull(op, &pdata, &plen);
      if (r <= 0)
         return r;
      if (fmt == 1)
         r = ropus_decode_s16(op->handle, pdata, plen, op->pend_s16);
      else
         r = ropus_decode_f32(op->handle, pdata, plen, op->pend_f32);
      if (r < 0)
         return -1;
      op->pend_frames = (size_t)r;
      op->pend_pos    = 0;
      skip = op->preskip_left;
      if (skip)
      {
         if (skip >= op->pend_frames)
         {
            op->preskip_left -= (unsigned)op->pend_frames;
            op->pend_frames = 0;   /* whole packet skipped; loop        */
         }
         else
         {
            op->pend_pos      = skip;
            op->pend_frames  -= skip;
            op->preskip_left  = 0;
         }
      }
      /* Buffer modes: the end granule (Ogg) or the TOC total less end
       * trimming (WebM) bounds emission; frames now pending will all
       * be consumed, so credit them here. */
      if ((op->ogg
#ifdef HAVE_RWEBM
               || op->demux
#endif
            ) && op->limit >= 0 && op->pend_frames)
      {
         int64_t left = op->limit - op->emitted;
         if (left <= 0)
         {
            op->pend_frames = 0;
            return 0;
         }
         if ((int64_t)op->pend_frames > left)
            op->pend_frames = (size_t)left;
         op->emitted += (int64_t)op->pend_frames;
      }
   }
   op->fmt = fmt;
   return (int)op->pend_frames;
}
#endif

#ifdef HAVE_RAAC
/* Decode the next AAC access unit into the pending buffer, honouring
 * the edit list's start trim.  Returns frames now pending, 0 at end of
 * stream, < 0 on error. */
/* The next access unit, wherever this context's come from.  Returns 1
 * with the bytes, 0 at end of stream, < 0 on a malformed one. */
static int audio_transfer_aac_pull(struct audio_transfer_aac *ac,
      const uint8_t **pdata, uint32_t *plen)
{
   {
      if (ac->adts)
      {
         /* walk the next ADTS frame: 12-bit sync, CRC flag choosing a
          * 7- or 9-byte header, 13-bit total frame length */
         size_t   pos = ac->adts_pos;
         unsigned hdr, flen;
         if (pos + 7 > ac->buf_size)
            return 0;
         if (ac->buf[pos] != 0xFF || (ac->buf[pos + 1] & 0xF6) != 0xF0)
            return 0;             /* lost sync: treat as end of stream */
         hdr  = (ac->buf[pos + 1] & 1) ? 7 : 9;
         flen = ((unsigned)(ac->buf[pos + 3] & 3) << 11)
              | ((unsigned)ac->buf[pos + 4] << 3)
              | ((unsigned)ac->buf[pos + 5] >> 5);
         if (flen <= hdr || pos + flen > ac->buf_size)
            return 0;
         *pdata       = ac->buf + pos + hdr;
         *plen        = flen - hdr;
         ac->adts_pos = pos + flen;
         return 1;
      }
#ifdef HAVE_RMP4
      if (ac->demux)
      {
         /* buffer mode: pull the next access unit from the demuxer */
         rmp4_packet pkt;
         for (;;)
         {
            if (rmp4_read_packet(ac->demux, &pkt) != 1)
               return 0;
            if (pkt.track == ac->track_idx)
               break;
         }
         *pdata = pkt.data;
         *plen  = (uint32_t)pkt.size;
         return 1;
      }
#endif
      {
         if (ac->pkt_index >= ac->num_packets)
            return 0;
         *plen = ac->pkt_sizes[ac->pkt_index];
         if (ac->pkt_offset + *plen > ac->packets_size)
            return -1;
         *pdata = ac->packets + ac->pkt_offset;
         ac->pkt_offset += *plen;
         ac->pkt_index++;
         return 1;
      }
   }
}

/* Frames decoded before the target and dropped so the decoder has its
 * overlap-add history back.  One access unit is the overlap itself and
 * the walk stops on a unit boundary, so three covers both. */
#define AUDIO_AAC_PREROLL (1024 * 3)

/* Every access unit this decoder takes is 1024 frames, so the walk to a
 * target needs no decoding at all - only the pre-roll does.  Returns
 * the frame reached, or < 0 if the stream ends first. */
static int audio_transfer_aac_fill(struct audio_transfer_aac *ac);

static int64_t audio_transfer_aac_seek_to(struct audio_transfer_aac *ac,
      int64_t frame)
{
   int64_t  pos  = 0;
   uint64_t skip = ac->start_trim;
   int64_t  stop = frame - AUDIO_AAC_PREROLL;

   raac_reset(ac->handle);
   ac->adts_pos = 0;
#ifdef HAVE_RMP4
   if (ac->demux)
      rmp4_rewind(ac->demux);
#endif
   ac->pkt_index   = 0;
   ac->pkt_offset  = 0;
   ac->pend_frames = 0;
   ac->pend_pos    = 0;
   if (stop < 0)
      stop = 0;
   while (pos < stop)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int64_t d = 1024;
      int r = audio_transfer_aac_pull(ac, &pdata, &plen);
      if (r <= 0)
         return -1;              /* asked past the end of the stream   */
      /* the encoder delay is dropped rather than emitted, so it does
       * not count towards the position */
      if (skip)
      {
         uint64_t taken = ((uint64_t)d < skip) ? (uint64_t)d : skip;
         skip -= taken;
         d    -= (int64_t)taken;
      }
      pos += d;
   }
   ac->trim_left = skip;
   ac->emitted   = pos;
   while (pos < frame)
   {
      int64_t take;
      int r = audio_transfer_aac_fill(ac);
      if (r < 0)
         return -1;
      if (r == 0)
         return -1;
      take = frame - pos;
      if (take > (int64_t)ac->pend_frames)
         take = (int64_t)ac->pend_frames;
      ac->pend_pos    += (size_t)take;
      ac->pend_frames -= (size_t)take;
      pos             += take;
   }
   return pos;
}

static int audio_transfer_aac_fill(struct audio_transfer_aac *ac)
{
   while (ac->pend_frames == 0)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int r;
      uint64_t skip;
      r = audio_transfer_aac_pull(ac, &pdata, &plen);
      if (r <= 0)
         return r;
      r = raac_decode_f32(ac->handle, pdata, plen, ac->pend_f32);
      if (r < 0)
         return -1;
      ac->pend_frames = (size_t)r;
      ac->pend_pos    = 0;
      skip = ac->trim_left;
      if (skip)
      {
         if (skip >= ac->pend_frames)
         {
            ac->trim_left  -= ac->pend_frames;
            ac->pend_frames = 0;   /* whole packet trimmed; loop        */
         }
         else
         {
            ac->pend_pos     = (size_t)skip;
            ac->pend_frames -= (size_t)skip;
            ac->trim_left    = 0;
         }
      }
      /* Stop at the duration the container declares: the last access
       * unit is a whole 1024 frames of which only part is the
       * recording, and handing the rest out is both wrong and longer
       * than the length info() reports. */
      if (ac->limit >= 0 && ac->pend_frames)
      {
         int64_t left = ac->limit - ac->emitted;
         if (left <= 0)
         {
            ac->pend_frames = 0;
            return 0;
         }
         if ((int64_t)ac->pend_frames > left)
            ac->pend_frames = (size_t)left;
         ac->emitted += (int64_t)ac->pend_frames;
      }
   }
   return (int)ac->pend_frames;
}
#endif

int audio_transfer_read_s16(void *data, enum audio_type_enum type,
      int16_t *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_s16(
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         if (v->packet)
            produced = audio_transfer_vorbis_drain(v, 1, out, NULL, frames);
         else
            produced = (size_t)rvorbis_get_samples_s16_interleaved(
                  v->handle, v->channels, out, (int)frames * v->channels);
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rmp3_read_s16(&m->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return AUDIO_PROCESS_ERROR;
         produced = rmodtracker_get_samples_s16_interleaved(
               md->handle, out, frames);
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle || op->fmt == 2)
            return AUDIO_PROCESS_ERROR;
         if (op->seek_to >= 0)
         {
            int64_t at = audio_transfer_opus_seek_to(op, op->seek_to, 1);
            op->seek_to = -1;
            if (at < 0)
               return AUDIO_PROCESS_ERROR;
         }
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_opus_fill(op, 1);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > op->pend_frames)
               take = op->pend_frames;
            memcpy(out + produced * op->channels,
                  op->pend_s16 + op->pend_pos * op->channels,
                  take * op->channels * sizeof(int16_t));
            op->pend_pos    += take;
            op->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take, i, n;
            const float *src;
            int16_t     *dst;
            int r = audio_transfer_aac_fill(ac);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > ac->pend_frames)
               take = ac->pend_frames;
            src = ac->pend_f32 + ac->pend_pos * ac->channels;
            dst = out + produced * ac->channels;
            n   = take * ac->channels;
            for (i = 0; i < n; i++)
               dst[i] = audio_transfer_unit_to_s16(src[i]);
            ac->pend_pos    += take;
            ac->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         const uint8_t *src;
         size_t avail, want, ch, i, n;
         if (!w || !w->opened)
            return AUDIO_PROCESS_ERROR;
         ch    = (size_t)w->wav.numchannels;
         avail = w->wav.numsamples - w->cursor;
         want  = (frames < avail) ? frames : avail;
         n     = want * ch;
         src   = w->data + w->wav.dataoffset + w->cursor * w->framesz;
         if (w->wav.bitspersample == 16)
         {
            /* rwav.h's accessors read the file's little-endian words a
             * byte at a time: right on either endianness, and safe
             * where an unaligned load would fault */
            for (i = 0; i < n; i++)
               out[i] = rwav_s16(src + i * 2);
         }
         else if (w->wav.bitspersample == 24)
         {
            /* packed three-byte samples, quantised by the shared
             * accessor so every reader of a 24-bit file rounds it
             * the same way */
            for (i = 0; i < n; i++)
               out[i] = rwav_s24_to_s16(src + i * 3);
         }
         else if (w->wav.bitspersample == 32) /* IEEE float -> s16 */
         {
            for (i = 0; i < n; i++)
               out[i] = audio_transfer_unit_to_s16(
                     rwav_f32(src + i * 4));
         }
         else /* 8-bit unsigned PCM -> signed 16-bit */
         {
            for (i = 0; i < n; i++)
               out[i] = (int16_t)(((int)src[i] - 128) << 8);
         }
         w->cursor += want;
         produced   = want;
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

int audio_transfer_read_f32(void *data, enum audio_type_enum type,
      float *out, size_t frames, size_t *frames_out)
{
   size_t produced = 0;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rflac_read_pcm_frames_f32(
               fl->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int got;
         if (!v || !v->handle)
            return AUDIO_PROCESS_ERROR;
         if (v->packet)
         {
            produced = audio_transfer_vorbis_drain(v, 0, NULL, out, frames);
            break;
         }
         got = rvorbis_get_samples_float_interleaved(v->handle, v->channels,
               out, (int)(frames * (size_t)v->channels));
         produced = (got > 0) ? (size_t)got : 0;
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return AUDIO_PROCESS_ERROR;
         produced = (size_t)rmp3_read_f32(&m->handle, (uint64_t)frames, out);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return AUDIO_PROCESS_ERROR;
         produced = rmodtracker_get_samples_float_interleaved(
               md->handle, out, frames);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         const uint8_t *src;
         size_t avail, want, ch, i, n;
         if (!w || !w->opened)
            return AUDIO_PROCESS_ERROR;
         ch    = (size_t)w->wav.numchannels;
         avail = w->wav.numsamples - w->cursor;
         want  = (frames < avail) ? frames : avail;
         n     = want * ch;
         src   = w->data + w->wav.dataoffset + w->cursor * w->framesz;
         if (w->wav.bitspersample == 16)
         {
            for (i = 0; i < n; i++)
               out[i] = (float)rwav_s16(src + i * 2) * (1.0f / 32768.0f);
         }
         else if (w->wav.bitspersample == 24)
         {
            for (i = 0; i < n; i++)
               out[i] = rwav_s24_to_float(src + i * 3);
         }
         else if (w->wav.bitspersample == 32)
         {
            /* already unit-scale float: hand the file's samples
             * through unaltered, unclamped as the f32 paths are */
            for (i = 0; i < n; i++)
               out[i] = rwav_f32(src + i * 4);
         }
         else /* 8-bit unsigned PCM */
         {
            for (i = 0; i < n; i++)
               out[i] = ((float)src[i] - 128.0f) * (1.0f / 128.0f);
         }
         w->cursor += want;
         produced   = want;
         break;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle || op->fmt == 1)
            return AUDIO_PROCESS_ERROR;
         if (op->seek_to >= 0)
         {
            int64_t at = audio_transfer_opus_seek_to(op, op->seek_to, 2);
            op->seek_to = -1;
            if (at < 0)
               return AUDIO_PROCESS_ERROR;
         }
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_opus_fill(op, 2);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > op->pend_frames)
               take = op->pend_frames;
            memcpy(out + produced * op->channels,
                  op->pend_f32 + op->pend_pos * op->channels,
                  take * op->channels * sizeof(float));
            op->pend_pos    += take;
            op->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle)
            return AUDIO_PROCESS_ERROR;
         while (produced < frames)
         {
            size_t take;
            int r = audio_transfer_aac_fill(ac);
            if (r < 0)
               return AUDIO_PROCESS_ERROR;
            if (r == 0)
               break;
            take = frames - produced;
            if (take > ac->pend_frames)
               take = ac->pend_frames;
            memcpy(out + produced * ac->channels,
                  ac->pend_f32 + ac->pend_pos * ac->channels,
                  take * ac->channels * sizeof(float));
            ac->pend_pos    += take;
            ac->pend_frames -= take;
            produced        += take;
         }
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         return AUDIO_PROCESS_ERROR;
   }

   if (frames_out)
      *frames_out = produced;
   return (produced == 0) ? AUDIO_PROCESS_END : AUDIO_PROCESS_NEXT;
}

size_t audio_transfer_buffer_tell(void *data, enum audio_type_enum type)
{
   if (!data)
      return 0;
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v =
               (struct audio_transfer_vorbis*)data;
         if (v->handle)
         {
#ifdef HAVE_RWEBM
            /* WebM: the packets are decoded where the demuxer points
             * at them in the caller's buffer, so its walk position is
             * the compressed frontier a feeder needs.  Monotonic
             * through playback, and back to the first cluster on a
             * rewind - never below the header material the demuxer
             * keeps borrowed pointers into. */
            if (v->demux)
               return rwebm_tell(v->demux);
#endif
            /* Self-framed Ogg buffer.  The demuxed arm's packets are
             * the caller's own blob rather than the buffer set by
             * set_buffer_ptr, so it has no offset to report. */
            if (!v->packet)
               return (size_t)rvorbis_buffer_tell(v->handle);
         }
         return 0;
      }
#endif
#ifdef HAVE_RWAV
      case AUDIO_TYPE_WAV:
      {
         struct audio_transfer_wav *w =
               (struct audio_transfer_wav*)data;
         /* the payload is read in place, so the frame cursor is a byte
          * offset into the caller's buffer like any other arm's */
         if (w->opened)
            return w->wav.dataoffset + w->cursor * w->framesz;
         return 0;
      }
#endif
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl =
               (struct audio_transfer_flac*)data;
         /* the raw read cursor runs slightly ahead of the decode
          * position through bitstream caching, which is the safe
          * side for a feeder: bytes behind it are never re-read
          * (the loop jump lands in the kept head) */
         if (fl->handle)
            return (size_t)fl->handle->memoryStream.currentReadPos;
         return 0;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m =
               (struct audio_transfer_mp3*)data;
         if (m->inited)
            return (size_t)m->handle.readPos;
         return 0;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op =
               (struct audio_transfer_opus*)data;
         /* Ogg buffer mode walks pages forward from the caller's
          * buffer; pg_off is the current page's byte offset, the
          * compressed frontier the feeder needs.  The demuxed (WebM)
          * and set_demuxed_ptr paths read a packet blob, not the
          * caller's buffer, so they expose no windowable cursor. */
         if (op->handle && op->ogg)
            return op->pg_off;
         return 0;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac =
               (struct audio_transfer_aac*)data;
         /* Only the ADTS buffer path walks the caller's buffer
          * linearly; adts_pos is the next frame's byte offset, i.e.
          * the compressed frontier the feeder needs.  The demuxed
          * (MP4/M4A) and set_demuxed_ptr paths read from the demuxer
          * or a concatenated packet blob, not the caller's buffer, so
          * they expose no windowable cursor - return 0, exactly as the
          * vorbis packet-fed paths do. */
         if (ac->handle && ac->adts)
            return ac->adts_pos;
         return 0;
      }
#endif
      default:
         break;
   }
   return 0;
}

#if defined(HAVE_RWEBM) && defined(HAVE_RVORBIS)
/* Sample-accurate seek on a packet-fed WebM stream.
 *
 * A Vorbis packet says how many frames it contributes
 * (rvorbis_packet_frames) without being decoded, so the walk to a
 * target costs two bytes a packet and no decoding at all.  The count
 * cannot come from the container: Matroska timestamps a block to the
 * millisecond, which at 44.1 kHz is 44 frames of slack, and it carries
 * no granule position to ask instead.  Counting the packets is the
 * only way to land on the right sample.
 *
 * Priming with packet m - resetting, then decoding m first - makes m
 * yield nothing and the audio resume at the position a linear decode
 * would have reached after m.  So the walk looks for the last m whose
 * position is still at or before the target, and the frames between
 * that and the target are decoded and dropped.  One packet of priming
 * is all Vorbis needs: it carries no prediction across frames, only
 * the overlap window, so the output after it is not converging on the
 * playthrough's, it is identical to it.
 *
 * Two passes rather than one, so that no packet pointer is held across
 * a read - rwebm documents a packet as valid only until the next call.
 * Both are header walks and the second stops at m. */
static bool audio_transfer_vorbis_seek_webm(struct audio_transfer_vorbis *v,
      int64_t target)
{
   const uint8_t *pd = NULL;
   uint32_t       pl = 0;
   int64_t        pos = 0;
   int            n, m = 1, i;

   /* Pass 1: where does each packet leave the stream? */
   rwebm_rewind(v->demux);
   n = 0;
   for (;;)
   {
      int fr;
      if (audio_transfer_vorbis_pull(v, &pd, &pl) != 1)
         break;
      n++;
      fr = rvorbis_packet_frames(v->handle, pd, pl);
      if (fr < 0)
         return false;
      /* A decode's first packet yields nothing, so the position only
       * starts moving with the second. */
      if (n < 2)
         continue;
      if (pos + fr > target)
         break;
      pos += fr;
      m    = n;
   }

   /* Pass 2: walk to m without decoding, then prime with it. */
   rvorbis_packet_reset(v->handle);
   rwebm_rewind(v->demux);
   for (i = 0; i < m; i++)
      if (audio_transfer_vorbis_pull(v, &pd, &pl) != 1)
         return false;
   if (rvorbis_packet_decode(v->handle, pd, pl, 1) < 0)
      return false;
   v->emitted = pos;
   /* Both passes above are header walks, and the pull arms the
    * container's DiscardPadding as it goes.  Nothing decoded those
    * packets, so nothing consumed it: left armed, it would come off
    * whichever packet the next drain happens to decode and clamp the
    * stream's end to wherever playback had got to.  The walk that
    * matters will arm it again when it reaches that block for real. */
   v->discard = 0;

   /* Drop the remainder through the ordinary drain, so the frames
    * counted here are the frames a playthrough emits. */
   while (v->emitted < target)
   {
      static int16_t skip[4096];
      size_t cap  = sizeof(skip) / sizeof(skip[0]) / (size_t)v->channels;
      size_t want = (size_t)(target - v->emitted);
      if (!cap)
         return false;
      if (want > cap)
         want = cap;
      if (!audio_transfer_vorbis_drain(v, 1, skip, NULL, want))
         return false;   /* the stream ended before the target */
   }
   return true;
}
#endif

bool audio_transfer_seek(void *data, enum audio_type_enum type,
      uint64_t frame)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl || !fl->handle)
            return false;
         return rflac_seek_to_pcm_frame(fl->handle,
               (uint64_t)frame) != 0;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v || !v->handle)
            return false;
         if (v->packet)
         {
#ifdef HAVE_RWEBM
            /* WebM, fully resident: count the packets to the target.
             * A windowed context cannot - the walk would run at the
             * wall, and the bytes past it are reserved rather than
             * populated - so windowing keeps rewind only. */
            if (frame != 0 && v->demux && !v->avail && v->channels > 0)
               return audio_transfer_vorbis_seek_webm(v, (int64_t)frame);
#endif
            /* A caller's own packet blob is theirs to restart, and a
             * windowed WebM cannot be walked, so for those the only
             * reachable position is the start - the rewind a loop
             * needs.  Anything else fails here rather than resolving
             * against a position that is not the one asked for. */
            if (frame != 0)
               return false;
            rvorbis_packet_reset(v->handle);
#ifdef HAVE_RWEBM
            if (v->demux)
               rwebm_rewind(v->demux);
#endif
            v->pkt_index  = 0;
            v->pkt_offset = 0;
            v->emitted    = 0;
            v->discard    = 0;   /* re-armed when the block comes round */
            return true;
         }
         if (frame == 0) /* loop-to-start: seek_start always succeeds */
         {
            rvorbis_seek_start(v->handle);
            return true;
         }
         return rvorbis_seek(v->handle, (unsigned int)frame) != 0;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->inited)
            return false;
         return rmp3_seek_to_frame(&m->handle, (uint64_t)frame) != 0;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (!md || !md->handle)
            return false;
         if (frame == 0) /* loop-to-start: rewind always succeeds */
         {
            rmodtracker_rewind(md->handle);
            return true;
         }
         /* Sequenced, so the replayer restarts and works forward with
          * the mixing skipped.  That is proportional to the distance
          * and runs here, on the calling thread; it lands exactly, and
          * fails only if the song ends before the target does. */
         return rmodtracker_seek(md->handle, (int)frame) == (int)frame;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
      {
         struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
         if (!w || !w->opened || frame > (uint64_t)w->wav.numsamples)
            return false;
         w->cursor = (size_t)frame;
         return true;
      }
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (!op || !op->handle)
            return false;
         if (frame == 0)
         {
            audio_transfer_opus_to_head(op);
            op->seek_to = -1;
            op->fmt     = 0;
            return true;
         }
         /* Where the length is known, refuse to be sent past it rather
          * than walk to the end and report failure from there. */
         if (op->limit >= 0 && (int64_t)frame > op->limit)
            return false;
         /* Recorded, not done: see seek_to. */
         op->seek_to = (int64_t)frame;
         return true;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac || !ac->handle)
            return false;
         if (frame == 0)
         {
            raac_reset(ac->handle);
            ac->adts_pos = 0;
#ifdef HAVE_RMP4
            if (ac->demux)
               rmp4_rewind(ac->demux);
#endif
            ac->pkt_index   = 0;
            ac->pkt_offset  = 0;
            ac->pend_frames = 0;
            ac->pend_pos    = 0;
            ac->trim_left   = ac->start_trim;
            ac->emitted     = 0;
            return true;
         }
         /* Where the length is known, refuse to be sent past it rather
          * than walk to the end and report failure from there. */
         if (ac->limit >= 0 && (int64_t)frame > ac->limit)
            return false;
         return audio_transfer_aac_seek_to(ac, (int64_t)frame)
               == (int64_t)frame;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }
   return false;
}

void audio_transfer_free(void *data, enum audio_type_enum type)
{
   if (!data)
      return;

   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (fl->handle)
            rflac_close(fl->handle);
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (v->handle)
            rvorbis_close(v->handle);
#ifdef HAVE_RWEBM
         if (v->demux)
            rwebm_close(v->demux);
#endif
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (m->inited)
            rmp3_uninit(&m->handle);
         break;
      }
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_TYPE_MOD:
      {
         struct audio_transfer_mod *md = (struct audio_transfer_mod*)data;
         if (md->handle)
            rmodtracker_close(md->handle);
         break;
      }
#endif
      case AUDIO_TYPE_WAV:
#ifdef HAVE_RWAV
         /* rwav_parse allocates nothing and the samples were read in
          * place, so there is nothing here to release */
         break;
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
      {
         struct audio_transfer_opus *op = (struct audio_transfer_opus*)data;
         if (op && op->handle)
            ropus_close(op->handle);
#ifdef HAVE_RWEBM
         if (op && op->demux)
            rwebm_close(op->demux);
#endif
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (ac && ac->handle)
            raac_close(ac->handle);
#ifdef HAVE_RMP4
         if (ac && ac->demux)
            rmp4_close(ac->demux);
#endif
         break;
      }
#endif
      case AUDIO_TYPE_NONE:
      default:
         break;
   }

   free(data);
}
