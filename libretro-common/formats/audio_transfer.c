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

/* audio_transfer -- one pull interface over every audio codec in the
 * tree.  A caller that wants interleaved PCM out of a file it holds in
 * memory allocates a state for the type (audio_transfer_new), points
 * it at the bytes (audio_transfer_set_buffer_ptr), starts it, and
 * pulls with audio_transfer_read_s16 or _read_f32 until
 * AUDIO_PROCESS_END; audio_transfer_info reports the channel count and
 * sample rate once decoding has begun.  <formats/audio.h> declares the
 * API and per-call contracts.
 *
 * Each codec sits behind its own arm, compiled in by its HAVE_ flag:
 * WAV (rwav), FLAC (rflac), Ogg Vorbis (rvorbis), MP3 (rmp3's stream
 * interface), Opus (ropus), AAC-LC (raac) and tracker modules
 * (rmodtracker).  The arms own whatever adaptation their codec needs -
 * walking Ogg pages into packets, feeding rflac and rmp3 their spans,
 * batching ropus packets, stepping raac access units - so that the
 * caller sees the same four calls whichever format it was handed.
 *
 * Two ways in.  A buffer is the whole file as it sits on disk, and the
 * type helpers say which arm reads it: audio_transfer_ogg_audio_type
 * looks inside an Ogg container (Vorbis, Opus or FLAC), and
 * audio_transfer_webm_audio_type inside a WebM/Matroska one, whose
 * arms then own an rwebm demuxer internally.  Demuxed input
 * (audio_transfer_set_demuxed_ptr) is for a caller that already runs a
 * container demuxer - rmp4 hands the codec setup data and the
 * elementary-stream packets straight through, so the same Vorbis, FLAC,
 * Opus and AAC arms serve MP4 audio without a container of their own.
 *
 * The remaining calls serve streaming and gapless playback:
 * audio_transfer_set_avail tells a WebM-backed arm how much of a still
 * -downloading buffer is valid so it stops at the frontier instead of
 * misreading truncation as corruption; audio_transfer_set_end_granule
 * and audio_transfer_set_start_trim carry edge trims (Opus pre-skip
 * and end granule, MP3 LAME delay/padding) so the decoded stream
 * starts and ends on the encoded material rather than the codec
 * priming; audio_transfer_buffer_tell reports consumption for callers
 * that window their reads; and audio_transfer_seek moves to an
 * absolute PCM frame, which is how a looping voice returns to the top
 * without tearing the state down.  A demuxed packet set may also be
 * re-pointed and grown mid-stream for progressive sources - the
 * contract for that lives with set_demuxed_ptr in the header. */

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
#if defined(HAVE_ROPUS) || defined(HAVE_RFLAC) || defined(HAVE_RVORBIS)
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

#ifdef HAVE_ROPUS
#include <formats/ropus.h>
#endif
#ifdef HAVE_RAAC
#include <formats/raac.h>
#ifdef HAVE_RMP4
#include <formats/rmp4.h>
#endif
#endif
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) \
 || defined(HAVE_RVORBIS) || defined(HAVE_RAAC) || defined(HAVE_RFLAC))
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
 *     frame; buffer_tell, hence windowing.  Every format rwav reads:
 *     8-bit unsigned PCM, 16- and 24-bit signed, 32-bit IEEE float,
 *     G.711 a-law and mu-law at eight bits carrying a logarithmic
 *     sixteen, and the MS and IMA/DVI ADPCM layouts at four bits in
 *     blocks that each restate their predictor.  A
 *     WAVE_FORMAT_EXTENSIBLE header naming any of them resolves to it,
 *     and rwav walks the chunk list, so LIST, fact, cue and the rest
 *     before the samples are no obstacle.
 *
 *     start() parses the header alone and frames are converted out of
 *     the caller's buffer as they are asked for, so the arm holds no
 *     decoded copy and buffer_tell reports the read frontier: WAV
 *     windows like the compressed arms, its residency a window rather
 *     than the file.  The uncompressed formats are read a sample at a
 *     time from the buffer; the companded and coded ones go through
 *     rwav_decode_s16, which the first pair need for their curve and
 *     the second because their payload is not addressable a frame at
 *     a time.  A block being self-contained, a read starting anywhere
 *     decodes only the block it lands in, so seeking costs no more
 *     than on PCM.
 *
 *     Multichannel parses here and plays: the mixer folds anything up
 *     to eight channels to stereo, so a 5.1 WAV is heard rather than
 *     refused, as a 5.1 FLAC is.
 *   Does not: take demuxed input.  Nor the rarer codecs a WAV can
 *     nominally carry - GSM and the like - which rwav refuses at the
 *     header rather than decoding as though their bytes were PCM.
 *
 * FLAC (rflac)
 *   Does: buffer input; s16 and f32, freely mixed; channels, rate and
 *     length from STREAMINFO; seek to any frame; buffer_tell, hence
 *     windowing (the raw cursor leads the decode position by the
 *     bitstream cache, which is the safe side for a feeder).
 *     A_FLAC in Matroska (.mka/.mkv, HAVE_RWEBM) plays too, and needs
 *     no synthesis to do it: the CodecPrivate is a whole fLaC header,
 *     magic and metadata blocks, and every block is one raw frame, so
 *     the native stream rflac takes is those laid end to end.  What
 *     that produces is a real FLAC stream rather than a stand-in, so
 *     the STREAMINFO length, seeking and buffer_tell all work on it as
 *     they do on a .flac file - none of what made the Vorbis arm's old
 *     Ogg synthesis wrong applies, that having had to invent framing
 *     and granules this does not.  Nor is the stream reassembled: it
 *     is served to the decoder through its own read callback, a block
 *     at a time out of the demuxer, so nothing beyond the frame being
 *     read is copied.
 *     Ogg FLAC (RFC 5334) as well, and by the same means: an Ogg page
 *     body is packet bytes with no framing of its own, so the bodies
 *     end to end past the nine-byte mapping header on the first
 *     packet are the native stream, served through the same
 *     callbacks.
 *     And demuxed input: the fLaC header as setup and the frames as
 *     delimited packets, which is what the containers above are
 *     reduced to anyway, so it is the same callbacks again.  The
 *     packet set may be grown mid-stream - the bases are read fresh
 *     at every read rather than kept, so a realloc between reads is
 *     no obstacle.
 *   Does not: resume the demuxed path after it has run out of
 *     packets, which is where it falls short of the growth contract
 *     the other demuxed arms meet.  Those decode a packet at a time
 *     and can carry on; this one hands rflac a byte stream and lets
 *     it pull, and a short read is an end of stream to a decoder that
 *     pulls, which it will not take back.
 *
 *     Staying ahead is not simply a matter of growing before each
 *     read.  rflac reads ahead of what it has decoded, by an amount
 *     that is its own business, so a feeder adding one packet a read
 *     still starves - measured, stopping at 9216 frames of 88200 -
 *     while five a read completes.  Driving it from buffer_tell,
 *     three packets of headroom past the cursor was enough on the
 *     same file and two was not.  A feeder should keep a margin and
 *     measure it, which the cursor now permits: it reported nothing
 *     for any of the callback paths until this was looked into, the
 *     arm having read the memory reader's position, which is not the
 *     live source on any of them.
 *
 *     Nor report a length where the STREAMINFO does not state one:
 *     Ogg FLAC never states one, and neither does a native file piped
 *     rather than seeked.  Seeking still works on those - it is only
 *     the total that is missing, and info() answers 0.
 *
 *     Seeking on the Matroska path costs more than on a .flac: rflac
 *     seeks by byte offset while a demuxer only walks forwards, so a
 *     backwards seek restarts the block walk and reads forward to the
 *     target.  The file is resident, so that is a pass over block
 *     headers rather than any I/O, and only a caller's own seek
 *     reaches it.
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
 *     hence windowing.
 *
 *     From WebM: end trimming.  Vorbis codes in overlapping blocks, so
 *     the last packet decodes past the end of the audio, and two
 *     things say where that end is.  The container's DiscardPadding,
 *     which arrives on the block it applies to, is exact - a resident
 *     buffer has already totalled it in the start() walk, so the bound
 *     holds before a frame is decoded, and a windowed one applies it
 *     when that block arrives.  A stated Duration is not exact, being
 *     the muxer's figure rather than the stream's, but it is there
 *     when a padding is not: a file remuxed from Ogg carries a
 *     padding, one encoded straight to WebM often does not.  The
 *     tighter of the two bounds emission, so a padded file is trimmed
 *     to the sample and an unpadded one to whatever the Duration
 *     claims, which still overshoots.
 *
 *     buffer_tell, hence windowing: the packets are decoded where the
 *     demuxer points at them, so its walk position is the compressed
 *     frontier, and start() reads no further than the header material
 *     unless it is walking for the length above.  A windowed caller
 *     must say so with set_avail, and keep saying so as its window
 *     slides: rwebm's header parse walks the segment's top-level
 *     children as far as its wall, which without one is the end of the
 *     file - and the length walk is skipped there for the same reason.
 *
 *     Seeks to any frame, exactly, from a resident WebM buffer: a
 *     Vorbis packet's frame count can be read off its header
 *     (rvorbis_packet_frames), so the walk to a target decodes
 *     nothing, and priming on the packet before it resumes output
 *     identical to the playthrough's rather than converging on it.
 *     Demuxed packets get the same treatment as a resident WebM,
 *     being resident by definition: the header walk at start gives
 *     the exact length, a seek counts packets to the target and lands
 *     on the sample, and buffer_tell reports how far into the blob
 *     the decoder has read.  That last is a byte offset into the
 *     caller's packets rather than into a buffer set by
 *     set_buffer_ptr, there being none - the same quantity for the
 *     same purpose, and what a feeder growing the set wants.  The
 *     Opus and AAC demuxed arms still report nothing there.
 *     A windowed context seeks too, within what has arrived.  What
 *     set_avail bounds is a prefix that only grows, not a window that
 *     slides, so every byte below it stays readable and the walk
 *     reaches any target inside it exactly.  A target past the wall
 *     is refused rather than approximated, and succeeds once the
 *     feeder has raised the bound past it, so a caller may simply
 *     retry.  A refused seek leaves the stream rewound.
 *
 *     No end trim on the demuxed path, its packet set being the
 *     caller's: with no padding stated, the last packet's overlap-add
 *     overhang is handed out with the rest, and the length reported
 *     is that total rather than a trimmed one.  It is what comes out,
 *     which is the property the length is meant to have.
 *
 *     Chained and multiplexed Ogg both play.  A survey of the page
 *     headers at open says how many logical bitstreams the file
 *     carries, which of them is Vorbis, and where each ends; a file
 *     with one goes to rvorbis whole, as every .ogg always has.
 *
 *     A multiplexed file cannot: rvorbis has no notion of a serial
 *     number and would take the other stream's pages for its own, so
 *     those go to the packet path, where the page walk filters by
 *     serial and assembles packets across page boundaries.  That
 *     brings the exact length and sample-accurate seek with it.
 *
 *     A chained file rvorbis does decode through, link after link,
 *     but it reports the first link's length for the whole file and
 *     hands out the lead of each next link's first block - audio no
 *     granule accounts for.  The length is the summed final granules
 *     of the Vorbis bitstreams, and reads are bounded by it, so what
 *     info() says and what comes out are the same figure again.
 *     Links that disagree on rate or channels are noticed: the survey
 *     reads each one's identification header, and where they differ
 *     the stream is bounded to the first link.  A voice is built
 *     around the values read at open, so the alternative is playing
 *     the later links at the wrong speed.  What info() reports is
 *     again what comes out - the first link's length.
 *   Does not: play the later links of such a file at all.  Following
 *     them would mean rebuilding the voice mid-stream, which is the
 *     mixer's business rather than this arm's.
 *
 *     On channels this arm is not the limit: rvorbis decodes one
 *     through sixteen, verified against libvorbis, and reports what
 *     the stream holds.  A 5.1 file therefore parses here and plays:
 *     the mixer folds up to eight channels to stereo, and refuses
 *     only what is wider than that.
 *
 * MP3 (rmp3)
 *   Does: buffer input, decoded through rmp3_stream - the buffer is
 *     consumed incrementally rather than borrowed whole, so
 *     buffer_tell reports a real frontier, the bytes behind it
 *     releasable rather than merely read (it leads the decode position
 *     by at most the stream's reassembly hold, the safe side for a
 *     feeder); s16 (quantised by the synthesis filter, no float round
 *     trip) and f32, freely mixed - the stream converts its filter
 *     state at the switch as the pull API did; channels and rate from
 *     the first frame's header, read at open by a parse-only walk that
 *     decodes nothing; seek to any PCM frame, recorded at seek() and
 *     performed at the following read as a decode forward from the
 *     head in that read's own pipeline, so the audio after a seek is
 *     byte-identical to a linear decode's - MP3 frames depend on the
 *     bit reservoir of their predecessors, so the top is the only
 *     sample-accurate way in, which is also what the resident decoder
 *     did, minus its habit of carrying stale synthesis state through
 *     the rewind.
 *     Length and gapless trim from the Xing/Info or VBRI header in the
 *     first frame: that frame carries no audio but is decoded like any
 *     other, so the priming dropped is its own length plus the encoder
 *     delay a LAME tag states, and the padding bounds the end.  What
 *     info() reports is what comes out.  set_start_trim overrides the
 *     tag's delay, for a caller whose container knows better.
 *     A file carrying no such header is measured by walking its frame
 *     headers instead - each states its own bitrate and so its own
 *     size, which works on a variable-bitrate stream as well as a
 *     constant one.  It is a step per frame over four bytes, not a
 *     decode: 1.1 ms for a ten-minute file, measured, which is why
 *     the length is worth having rather than reporting nothing.  What
 *     it counts is what the decoder emits, priming included, there
 *     being no tag to say which of that was padding - so info() and
 *     the output agree here as they do everywhere else.
 *   Does not: take demuxed input.
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
 *   Does not: report the module's own channel count through info(),
 *     which says 2 because the replayer mixes to interleaved stereo
 *     and that is what the count in this API means - a module's four
 *     or thirty-two are voices being mixed, not channels being
 *     emitted.  The module's figure is not lost, only elsewhere:
 *     rmodtracker_voices returns it, for a caller wanting to say
 *     something about the file rather than about its output.
 *     No buffer cursor, and no windowing to use one for: the module is
 *     parsed into the replayer's own structures at open, after which
 *     the caller's bytes are not read again, and sample data is
 *     revisited at random for as long as the song plays - neither the
 *     monotonic read nor the bounded head that windowing needs.  No
 *     demuxed input.
 *
 * Opus (ropus)
 *   Does: mono through eight channels, mapping families 0 and 1, the
 *     channels emitted in the stream's own order rather than any
 *     player's layout; three inputs - Ogg Opus buffer (.opus, pages walked in place
 *     per RFC 3533/7845, a packet copied only where it spans pages),
 *     WebM buffer (.weba, HAVE_RWEBM, packets pulled from rwebm on
 *     demand), and demuxed (OpusHead + delimited packets); s16 and f32;
 *     RFC 7845 pre-skip; end trimming, from the last page granule for
 *     Ogg and from the TOC total less DiscardPadding for WebM; windowed
 *     operation, via set_end_granule plus buffer_tell for Ogg, and
 *     via set_avail plus buffer_tell for WebM, whose packets are read
 *     where the demuxer points at them;
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
 *     length from the demuxed path, whose packet set is the caller's;
 *     the buffer modes do report one, being
 *     the bound they already hold emission to.  A resident WebM walks
 *     the packets' TOC bytes at open for that bound; a windowed one
 *     cannot - the walk would run at the wall - and totals them as it
 *     plays instead, taking the padding off when the block carrying it
 *     arrives, so its length is a stated Duration until then - or
 *     nothing at all until then, where the file states no Duration
 *     either, which a live-muxed one does not.  Chained or multiplexed
 *     Ogg is not handled - pages are walked in order, the serial is
 *     ignored and the page CRC is not verified.  A page-spanning packet
 *     larger
 *     than asm_buf is an error, not a reallocation.  Channel mapping
 *     family 1 - a surround stream is several substreams scattered to
 *     the output channels by the head's mapping table - and refuses
 *     families 2, 3 and 255.  As with Vorbis the arm is not where a
 *     multichannel file stops; the mixer folds up to eight channels
 *     to stereo and refuses only what is wider.
 *     The rate is always the 48 kHz Opus decodes at, never the
 *     original input rate.
 *
 * AAC (raac)
 *   Does: three inputs - ADTS buffer (.aac, the AudioSpecificConfig
 *     synthesised from the first header), MP4/M4A buffer (HAVE_RMP4,
 *     packets from rmp4, start trim from the track's edit list),
 *     Matroska buffer (.mka/.mkv, HAVE_RWEBM, the ASC being the
 *     track's CodecPrivate and the access units its blocks, with the
 *     priming from CodecDelay), and demuxed (ASC + delimited access
 *     units, trim via set_start_trim); s16 and f32, freely mixed at any point (the
 *     pending frames are held as raac's float output and converted on
 *     the way out of read_s16, which is what raac_decode_s16 does
 *     internally, so the s16 samples are the same either way); the
 *     encoder-delay trim, and, from an MP4, the end trim too - the
 *     declared duration bounds emission, so the tail padding coded into
 *     the last access unit is not handed out and the length info()
 *     reports is the length that comes out; rewind; buffer_tell in ADTS
 *     mode, hence windowing there.
 *     Seeks: every access unit this decoder takes yields a fixed
 *     raac_frame_len() frames, so
 *     the units before the target are counted rather than decoded and
 *     only a pre-roll before it is decoded and dropped.
 *   Does not: reproduce a playthrough exactly across a seek where the
 *     encoder used noise substitution - the reset a seek needs reseeds
 *     the noise generator, and substituted noise is noise rather than
 *     coded samples.  Measured, because how far off it lands matters
 *     and the size of it depends entirely on the material: encoded
 *     with substitution off, a seek reproduces the playthrough
 *     bit for bit, on a tone and on noise alike; with it on, a tone
 *     comes back within a third of an LSB and pink noise about 700
 *     against a signal of 2270, which is ten dB down and plainly
 *     audible.  The bands the encoder chose
 *     to substitute are the ones that differ, so tonal material
 *     barely moves and noise-like material moves a lot.
 *
 *     Reports a length only from an MP4 or a Matroska, which declare
 *     one - though a Matroska's
 *     Duration spans what the stream codes, priming included, where an
 *     MP4's edit list is already net of it, so the trim comes off the
 *     one and not the other.  An ADTS stream would have to be walked
 *     to be measured, and the demuxed path's packets are the
 *     caller's.  ADTS carries no delay signalling, so
 *     that path forces the trim to 0 and is not gapless.  A lost ADTS
 *     sync is reported as end of stream rather than resynchronised.
 *     Beyond that the scope is raac's: the AAC-LC core (no
 *     Main/SSR/LTP) with raac's SBR upsampling path, whose doubled
 *     frame length and output rate this arm follows via
 *     raac_frame_len()/raac_sample_rate(); parametric stereo payloads
 *     are skipped (HE-AAC v2 decodes as v1).  The ADTS header parse
 *     refuses anything outside raac's scope before the decoder is
 *     opened.
 *
 * Across all arms: no file I/O and no ownership of the encoded bytes -
 * buffers are borrowed and must outlive the decoder (rmodtracker copies
 * the module at start(), so MOD alone is exempt afterwards).  No
 * resampling and no channel conversion; PCM comes out at the stream's
 * own rate and channel count and the mixer deals with it.  A short read
 * is not end of stream: only a zero-frame read returns
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
   /* bytes per frame: channels * bits/8, and 0 for a block-coded
    * payload, which has no per-frame size.  Every reader tests the
    * format and takes the decoder before reaching it. */
   size_t      framesz;
   /* s16 staging for the f32 read of a companded or block-coded
    * payload, which rwav decodes to s16 natively.  Owned here rather
    * than shared, and grown to the request; NULL until one is made,
    * so a PCM or float file never allocates it. */
   int16_t    *scratch;
   size_t      scratch_cap;  /* samples, not frames                     */
};
#endif

#ifdef HAVE_RFLAC
struct audio_transfer_flac
{
   const void *data;    /* encoded bytes from set_buffer_ptr (caller-owned) */
   size_t      size;
   rflac_t    *handle;  /* opened decoder, NULL until start() succeeds      */
   int         fed_hdr; /* setup bytes handed over                          */
   int         drained; /* feeder reported end of input                     */
   size_t      cursor;  /* bytes of the logical stream handed to the decoder*/
   /* FLAC inside a container.  rflac wants a native fLaC stream; a
    * container holds one taken apart, so it is served back to the
    * decoder a read at a time rather than reassembled into a buffer.
    * Nothing is copied but the bytes being read.
    *
    * Two containers, one shape.  Matroska keeps the header in
    * CodecPrivate and a frame per block.  Ogg (RFC 5334) needs even
    * less: a page body is packet bytes with no framing of its own, so
    * the bodies laid end to end are the native stream already, once
    * the nine-byte mapping header on the first packet is stepped
    * over.
    *
    * pos is the byte offset within that notional stream, which is
    * what rflac seeks against; cur is the chunk being served. */
   int            ogg;      /* buffer is Ogg FLAC                       */
   size_t         pg_off;   /* next Ogg page                            */
   /* Demuxed input (set_demuxed_ptr): the fLaC header as setup, then
    * the frames as delimited packets.  The base pointers are read
    * fresh at every use rather than cached, because the growth
    * contract lets a caller realloc them mid-stream; position is
    * carried as an index and a byte offset, which survive that. */
   const void      *setup;
   size_t           setup_size;
   const uint8_t   *packets;
   size_t           packets_size;
   const uint32_t  *pkt_sizes;
   size_t           num_packets;
   size_t           pkt_index;
   size_t           blob_off;  /* byte offset of the current packet     */
#ifdef HAVE_RWEBM
   rwebm_t       *demux;
   int            track_idx;
#endif
   const uint8_t *hdr;
   size_t         hdr_size;
   size_t         pos;
   const uint8_t *cur;
   size_t         cur_len;
   size_t         cur_off;
   /* The last empty pull stopped at the resident wall, not the end of
    * the stream.  Set at the pull that met the wall, cleared when a
    * packet flows again; audio_transfer_read_* consults it so a stall
    * surfaces as AUDIO_PROCESS_NEXT with zero frames instead of being
    * mistaken for end of stream - which a looping mixer voice answers
    * with a mid-file rewind, and a second stall with releasing the
    * voice entirely. */
   uint8_t        wall;
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
   /* Plain self-framed .ogg: demuxed incrementally by rvorbis's own
    * Ogg layer, so what a decode holds is a packet rather than the
    * file.  handle is borrowed from it - the stream owns the decoder -
    * and is kept so the queries below need no special case. */
   rvorbis_stream_t *stream;
   size_t      stream_off;  /* bytes of the buffer consumed             */
   int         packet;      /* opened with rvorbis_open_packets         */
   size_t      pkt_index;   /* next packet in the caller's blob         */
   size_t      pkt_offset;
   /* Ogg pages feeding the packet path, for a file that carries more
    * than one logical bitstream.  A plain .ogg does not come this
    * way - rvorbis reads those itself - but a multiplexed one must,
    * because rvorbis has no notion of a serial number and would take
    * the other stream's pages for its own. */
   int         ogg_pkt;
   uint32_t    ogg_serial;
   size_t      pg_off;
   unsigned    seg_idx;
   size_t      body_off;
   uint8_t    *asm_buf;     /* only for packets spanning pages          */
   size_t      asm_cap;
#ifdef HAVE_RWEBM
   rwebm_t    *demux;       /* buffer is WebM audio (.weba)             */
   int         track_idx;
#endif
   /* Resident prefix of the caller's buffer, 0 when all of it is.  A
    * windowed feeder sets this before start() so the header parse
    * stays inside the head, and raises it as the window slides.
    * Unconditional, like the set_avail case that stores it: gating it
    * on HAVE_RWEBM made that store - and the wall flag the read tail
    * consults - a compile error on every RWEBM-less build. */
   size_t      avail;
   uint8_t     wall;        /* see audio_transfer_flac::wall            */
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
   rmp3_stream_t *stream; /* opened decoder, NULL until start() succeeds    */
   size_t      off;      /* consumption cursor: the compressed frontier      */
   unsigned    channels; /* from the first frame's header, read at start()   */
   unsigned    rate;
   int         eof_sent; /* the stream has been told the tail is all there is */
   /* Recorded rather than done: an MP3 seek is a decode forward from
    * the head, and which pipeline it decodes in belongs to the read
    * that follows, so the output after a seek is byte-identical to a
    * linear decode in that read's own format.  Same shape as the Opus
    * arm's seek_to. */
   int64_t     seek_to;
   /* Gapless, from the Xing/Info or VBRI header in the first frame.
    * That frame carries no audio but dr_mp3 decodes it like any
    * other, so the priming to drop is its own length plus the
    * encoder delay the LAME tag states; the padding comes off the
    * end.  All zero where the file has no such header, which is the
    * old behaviour. */
   uint64_t    start_trim;
   uint64_t    trim_left;
   int64_t     limit;    /* frames to emit after the trim, -1 unbounded      */
   int64_t     emitted;
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
/* The largest an Opus packet can be: a code 3 packet carries at most
 * 48 frames of at most 1275 bytes (RFC 6716 s3.2.5), plus the TOC and
 * the frame count byte.  Rounded up, and the bound the reassembly
 * buffer below is held to. */
#define AUDIO_OPUS_MAX_PACKET 61500

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
#endif
   /* Unconditional: see the Vorbis arm's note on these two. */
   size_t      avail;        /* resident prefix, 0 = all of it           */
   uint8_t     wall;         /* see audio_transfer_flac::wall            */
   /* A windowed WebM cannot pre-walk the packets for the stream's
    * length, so it accumulates their TOC durations as they play and
    * takes the container's padding off when the block carrying it
    * arrives.  toc_total counts raw decoded frames, before pre-skip;
    * both stay 0 on every other input, where the length is known by
    * other means. */
   int64_t     toc_total;
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
   /* Reassembly for a packet that spans Ogg pages, which is the only
    * thing that needs a copy - an ordinary packet is read from the
    * page in place.  Allocated on the first one that does, the way
    * the Vorbis arm's asm_buf is, rather than carried inline: page
    * spanning is rare, AUDIO_OPUS_MAX_PACKET is large, and every
    * context paid for it whether or not it ever spanned.  It also sat
    * between the two halves of the hot state, so the fields the read
    * path touches were 60 KiB apart and in different pages. */
   uint8_t    *asm_buf;      /* only for packets spanning pages          */
   size_t      asm_cap;
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
   /* Decoded-but-unconsumed frames from the last packet.
    *
    * One buffer, not two, and sized for the stream rather than for a
    * guess.  ropus_decode_s16/f32 take no output length and require
    * room for ROPUS_MAX_FRAME * channels samples; the two arrays
    * that used to sit here were sized 5760 * 2 apiece, which is that
    * room only for a stream of at most two channels.  ropus_open takes
    * mapping family 1 up to eight, so a six- or eight-channel file -
    * anything opusenc emits from a surround source - decoded straight
    * past the end of the buffer, and for the f32 arm past the end of
    * the allocation.  Sizing this from op->channels at the point the
    * format is latched closes that, and drops the two-channel case
    * from 69120 bytes to 23040 (s16) or 46080 (f32), the other arm's
    * buffer having never been live: fmt is latched by the first read
    * and the opposite entry point errors for the life of the context.
    *
    * pend_elem is what it was allocated for, 2 or 4 bytes.  Rewinding
    * to frame 0 clears fmt, so a context can be asked for the other
    * format afterwards; that reallocates rather than reinterpreting. */
   size_t      pend_frames;
   size_t      pend_pos;
   void       *pend;        /* int16_t* when fmt 1, float* when fmt 2  */
   size_t      pend_elem;   /* sizeof the element pend holds, 0 if none */
   size_t      pend_samples;/* room at pend, in interleaved samples     */
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
   /* Resident prefix of 'buf' for a windowed caller; 0 means all of
    * it.  Raised through audio_transfer_set_avail as the window
    * slides, exactly as the Vorbis and Opus arms take it. */
   size_t      avail;
   uint8_t     wall;        /* see audio_transfer_flac::wall             */
   int         adts;        /* buffer is an ADTS stream                  */
   size_t      adts_pos;    /* byte cursor of the next ADTS frame        */
#ifdef HAVE_RMP4
   rmp4_t     *demux;
   int         track_idx;
#endif
#ifdef HAVE_RWEBM
   /* Matroska (.mka/.mkv) carrying A_AAC: the AudioSpecificConfig is
    * the track's CodecPrivate and the access units are its blocks,
    * pulled on demand like every other packet-fed path here. */
   rwebm_t    *wdemux;
   int         wtrack_idx;
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
   /* raac's per-packet output: raac_frame_len() (doubled under SBR)
    * times the channel count, sized at start when both are known.  A
    * fixed 1024x2 array here once assumed plain LC and overflowed on
    * SBR stereo streams. */
   float      *pend_f32;
   size_t      pend_cap;   /* capacity in samples */
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
static int audio_transfer_vorbis_ogg_pkt(struct audio_transfer_vorbis *v,
      const uint8_t **pdata, uint32_t *plen);

static int audio_transfer_vorbis_pull(struct audio_transfer_vorbis *v,
      const uint8_t **pdata, uint32_t *plen, int64_t *ppad)
{
   *ppad = 0;
   if (v->ogg_pkt)
      return audio_transfer_vorbis_ogg_pkt(v, pdata, plen) == 1 ? 1 : 0;
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
         {
            v->wall = 1;
            return -2;
         }
         if (r != 1)
            return 0;
         if (pkt.track == v->track_idx)
         {
            v->wall = 0;
            break;
         }
      }
      *pdata = pkt.data;
      *plen  = (uint32_t)pkt.size;
      /* DiscardPadding is attached to the block it applies to, so it
       * arrives with the packet rather than having to be found by
       * walking to the end first.  It is handed back with the packet
       * rather than left in the context: a caller that only reads
       * headers - a seek walk, a length walk - would otherwise have to
       * remember to clear it, and one that forgot would have it come
       * off whichever packet the next decode touched. */
      if (pkt.discard_padding > 0)
      {
         int64_t rate = (int64_t)rvorbis_get_info(v->handle).sample_rate;
         if (rate > 0)
            *ppad = (pkt.discard_padding * rate + 500000000) / 1000000000;
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
/* Plain-Ogg read: hand rvorbis's demuxer the unconsumed tail of the
 * buffer and take what it produces.  The compressed cursor is kept here
 * rather than in the decoder because a windowed feeder reads it, and
 * because a seek re-points it. */

/* Seek a plain-Ogg stream.  Ogg locates a sample by bisecting on page
 * granule positions - there is no index - and the decoder cannot read,
 * so the bisection is driven from here over the buffer.  Each probe
 * resynchronises at the next page and reports where it landed; the
 * decode forward from the page before the target is what makes the
 * landing exact, since a page boundary is the finest position the
 * container states.
 *
 * The frames before the target are decoded and dropped rather than
 * skipped: a Vorbis packet's output depends on its predecessor's
 * overlap, so arriving with no history would resume converging on the
 * playthrough rather than matching it. */
static bool audio_transfer_vorbis_stream_seek(
      struct audio_transfer_vorbis *v, uint64_t frame)
{
   int16_t  scratch[64 * 8];
   size_t   lo  = 0;
   size_t   hi  = v->size;
   unsigned ch  = (unsigned)((v->channels > 0 && v->channels <= 8)
         ? v->channels : 8);
   size_t   cap = sizeof(scratch) / sizeof(scratch[0]) / ch;
   size_t   start;
   int      guard;

   if (!v->stream)
      return false;

   if (!frame)
   {
      rvorbis_stream_rewind(v->stream);
      v->stream_off = 0;
      v->emitted    = 0;
      return true;
   }

   for (guard = 0; lo < hi && guard < 64; guard++)
   {
      size_t mid = lo + (hi - lo) / 2;
      size_t off = mid;
      int    landed = 0;

      rvorbis_stream_reset(v->stream);
      while (off < v->size)
      {
         size_t rd = 0, wr = 0;
         int    r;
         rvorbis_stream_set_out_s16(v->stream, scratch, cap);
         rvorbis_stream_set_in(v->stream, (const uint8_t*)v->data + off,
               v->size - off);
         r = rvorbis_stream_process(v->stream, &rd, &wr);
         off += rd;
         if (rvorbis_stream_pos_known(v->stream))
         {
            landed = 1;
            break;
         }
         if (r == RVORBIS_STREAM_ERROR || r == RVORBIS_STREAM_EOS)
            break;
         if (r == RVORBIS_STREAM_NEED_IN && !rd)
            break;
      }
      if (landed && rvorbis_stream_tell(v->stream) <= frame)
         lo = mid + 1;
      else
         hi = mid;
   }

   /* One page back from the boundary, so the target is reached by
    * decoding forward rather than jumped over. */
   start = (lo > 65307) ? lo - 65307 : 0;

   for (guard = 0; guard < 2; guard++)
   {
      size_t off = start;
      if (start)
         rvorbis_stream_reset(v->stream);
      else
         rvorbis_stream_rewind(v->stream);
      while (off < v->size)
      {
         size_t rd = 0, wr = 0, want = cap;
         int    r;
         /* Once the position is known, ask for exactly the distance
          * left.  Asking for a full scratch instead would step past
          * the target and land wherever the packet happened to end. */
         if (rvorbis_stream_pos_known(v->stream))
         {
            uint64_t at = rvorbis_stream_tell(v->stream);
            if (at == frame)
            {
               v->stream_off = off;
               v->emitted    = (int64_t)frame;
               return true;
            }
            if (at > frame)
               break;           /* landed late: restart from the head */
            if (frame - at < (uint64_t)want)
               want = (size_t)(frame - at);
         }
         rvorbis_stream_set_out_s16(v->stream, scratch, want);
         rvorbis_stream_set_in(v->stream, (const uint8_t*)v->data + off,
               v->size - off);
         r = rvorbis_stream_process(v->stream, &rd, &wr);
         off += rd;
         if (r == RVORBIS_STREAM_ERROR)
            return false;
         if (r == RVORBIS_STREAM_EOS)
            break;
         if (r == RVORBIS_STREAM_NEED_IN && !rd && !wr)
            break;
      }
      if (!start)
         break;
      start = 0;               /* the bisection landed late; take the
                                * whole stream, which always works */
   }
   return false;
}

static size_t audio_transfer_vorbis_stream_pull(
      struct audio_transfer_vorbis *v, int s16, int16_t *out16,
      float *outf, size_t frames)
{
   size_t done = 0;
   if (!v->stream || !frames)
      return 0;
   while (done < frames)
   {
      size_t rd = 0, wr = 0;
      int    r;
      if (s16)
         rvorbis_stream_set_out_s16(v->stream, out16 + done * (size_t)v->channels,
               frames - done);
      else
         rvorbis_stream_set_out_f32(v->stream, outf + done * (size_t)v->channels,
               frames - done);
      rvorbis_stream_set_in(v->stream,
            (const uint8_t*)v->data + v->stream_off,
            v->size - v->stream_off);
      r = rvorbis_stream_process(v->stream, &rd, &wr);
      v->stream_off += rd;
      done          += wr;
      if (r == RVORBIS_STREAM_ERROR || r == RVORBIS_STREAM_EOS)
         break;
      if (r == RVORBIS_STREAM_NEED_IN && !rd && !wr)
         break;   /* the buffer is spent: a feeder must extend it */
   }
   return done;
}

static size_t audio_transfer_vorbis_drain(struct audio_transfer_vorbis *v,
      int s16, int16_t *out16, float *outf, size_t frames)
{
   size_t done = 0;
   int    ch   = v->channels;
   if (ch <= 0)
      return 0;
   for (;;)
   {
      const uint8_t *pd  = NULL;
      uint32_t       pl  = 0;
      int64_t        pad = 0;
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
      if (audio_transfer_vorbis_pull(v, &pd, &pl, &pad) != 1)
         break;
      if (rvorbis_packet_decode(v->handle, pd, pl, s16) < 0)
         break;
      /* This packet carried a padding, so the frames it just yielded
       * run past the end of the audio by that much.  Its yield is
       * countable now, which is what makes the exact end knowable; a
       * stated duration only rounds up past the overhang, so this
       * supersedes it. */
      if (pad > 0)
      {
         int64_t out   = v->emitted + (int64_t)done;
         int64_t total = out + rvorbis_packet_pending(v->handle)
                             - pad;
         /* A padding longer than the packet's yield would ask for
          * frames already handed out; the bound stops at what has
          * gone rather than going backwards. */
         if (total < out)
            total = out;
         if (v->limit < 0 || total < v->limit)
            v->limit = total;
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
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl)
            return false;
         /* setup is the fLaC header - magic and metadata blocks, as a
          * container stores it - and the packets are the frames. */
         fl->setup        = setup;
         fl->setup_size   = setup_size;
         fl->packets      = (const uint8_t*)packets;
         fl->packets_size = packets_size;
         fl->pkt_sizes    = sizes;
         fl->num_packets  = num_packets;
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
/* Any arm that honours a resident bound needs this compiled, not just
 * the WebM-backed ones: requiring HAVE_RWEBM here no-opped the whole
 * function on an RWEBM-less build, so an M4A stream's every bound -
 * the one at open that keeps rmp4's box walk off unpopulated pages,
 * and every feeder raise after - was accepted by the caller's guard
 * (audio_mixer_play_stream admits RAAC+RMP4 alone, and says why) and
 * silently dropped here.  The demuxer then believed the whole file
 * readable and handed the decoder packets on pages the window never
 * committed. */
#if defined(HAVE_RWEBM) || defined(HAVE_RVORBIS) || defined(HAVE_ROPUS) \
 || defined(HAVE_RAAC) || defined(HAVE_RFLAC)
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         if (!v)
            return;
         v->avail = avail;
#ifdef HAVE_RWEBM
         if (v->demux)
            rwebm_set_avail(v->demux, avail);
#endif
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
#ifdef HAVE_RWEBM
         if (op->demux)
            rwebm_set_avail(op->demux, avail);
#endif
         return;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (!ac)
            return;
         ac->avail = avail;
#ifdef HAVE_RMP4
         /* MP4: the sample tables address the whole file, so the
          * demuxer must be told which of those bytes have actually
          * arrived - it withholds a packet whose extent is past the
          * bound rather than reading reserved pages. */
         if (ac->demux)
            rmp4_set_avail(ac->demux, avail);
#endif
#ifdef HAVE_RWEBM
         if (ac->wdemux)
            rwebm_set_avail(ac->wdemux, avail);
#endif
         return;
      }
#endif
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         /* The FLAC arm was absent from this switch entirely, so the
          * mixer's FLAC lane in voice_set_avail called through to a
          * default: break - a windowed WEBA-FLAC voice kept whatever
          * bound its demuxer captured at open, forever. */
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl)
            return;
#ifdef HAVE_RWEBM
         if (fl->demux)
            rwebm_set_avail(fl->demux, avail);
#endif
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
#ifdef HAVE_RFLAC
   /* RFC 5334: 0x7F, "FLAC", then the mapping version. */
   if (b[first] == 0x7F && !memcmp(b + first + 1, "FLAC", 4))
      return AUDIO_TYPE_FLAC;
#endif
   return AUDIO_TYPE_NONE;
}

#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) \
 || defined(HAVE_RVORBIS) || defined(HAVE_RAAC) || defined(HAVE_RFLAC))
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
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) \
 || defined(HAVE_RVORBIS) || defined(HAVE_RAAC) || defined(HAVE_RFLAC))
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
#ifdef HAVE_RFLAC
      if (t->codec == RWEBM_CODEC_FLAC)
         found = AUDIO_TYPE_FLAC;
#endif
#ifdef HAVE_RAAC
      if (t->codec == RWEBM_CODEC_AAC)
         found = AUDIO_TYPE_AAC;
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
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m)
            return false;
         /* Overrides the LAME tag's delay, and is taken as covering
          * the header frame too - a caller stating this knows what
          * the stream plays, which is what the arm needs. */
         m->start_trim = frames;
         m->trim_left  = frames;
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

#ifdef HAVE_RMP3
/* The frame count and gapless figures an MP3 carries in its first
 * frame, which is otherwise silent filler.
 *
 * A Xing or Info tag states how many frames the stream holds; a LAME
 * tag after it states the encoder delay and the padding.  VBRI, which
 * Fraunhofer writes instead, states the count and nothing else.  All
 * three sit inside the first frame at an offset that depends on the
 * MPEG version and whether the stream is mono, because that is how
 * much side information the frame has.
 *
 * Returns 1 when a count was found.  *out_frames is the audio the
 * stream is meant to play, *out_delay the priming to drop from the
 * front - the filler frame included, since the decoder emits it. */
static int audio_transfer_mp3_gapless(const uint8_t *b, size_t len,
      uint64_t *out_frames, uint64_t *out_delay)
{
   size_t   off = 0, xo;
   unsigned ver, sfi, chmode, spf;
   const uint8_t *h;
   uint32_t flags, frames = 0;

   if (!b || len < 4)
      return 0;
   /* An ID3v2 tag sits before the audio; its size is seven bits a
    * byte, the high bit of each being reserved. */
   if (len > 10 && b[0] == 'I' && b[1] == 'D' && b[2] == '3')
   {
      size_t tag = ((size_t)(b[6] & 0x7f) << 21)
                 | ((size_t)(b[7] & 0x7f) << 14)
                 | ((size_t)(b[8] & 0x7f) << 7)
                 |  (size_t)(b[9] & 0x7f);
      off = 10 + tag + ((b[5] & 0x10) ? 10 : 0);
   }
   while (off + 4 <= len && !(b[off] == 0xFF && (b[off + 1] & 0xE0) == 0xE0))
      off++;
   if (off + 4 > len)
      return 0;
   h      = b + off;
   ver    = (h[1] >> 3) & 3;      /* 3 MPEG1, 2 MPEG2, 0 MPEG2.5       */
   sfi    = (h[2] >> 2) & 3;
   chmode = (h[3] >> 6) & 3;      /* 3 = mono                          */
   if (ver == 1 || sfi == 3)      /* reserved version / sample rate    */
      return 0;
   spf    = (ver == 3) ? 1152 : 576;
   if (ver == 3)
      xo = (chmode == 3) ? 4 + 17 : 4 + 32;
   else
      xo = (chmode == 3) ? 4 +  9 : 4 + 17;

   if (   off + xo + 8 <= len
       && (   !memcmp(b + off + xo, "Xing", 4)
           || !memcmp(b + off + xo, "Info", 4)))
   {
      const uint8_t *p = b + off + xo + 8;
      uint64_t delay = 0, pad = 0;
      flags = ((uint32_t)b[off + xo + 4] << 24)
            | ((uint32_t)b[off + xo + 5] << 16)
            | ((uint32_t)b[off + xo + 6] <<  8)
            |  (uint32_t)b[off + xo + 7];
      if (flags & 1)
      {
         if ((size_t)(p - b) + 4 > len)
            return 0;
         frames = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
                | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
         p += 4;
      }
      if (flags & 2)
         p += 4;
      if (flags & 4)
         p += 100;
      if (flags & 8)
         p += 4;
      if (!frames)
         return 0;
      /* The LAME tag starts here; its delay and padding are twelve
       * bits each, packed into three bytes at offset 21. */
      if ((size_t)(p - b) + 24 <= len)
      {
         delay = ((uint64_t)p[21] << 4) | (p[22] >> 4);
         pad   = ((uint64_t)(p[22] & 0xf) << 8) | p[23];
      }
      /* The filler frame is decoded like any other, so it is priming
       * too.  What is left to play is the stated audio less both
       * ends. */
      *out_delay  = (uint64_t)spf + delay;
      *out_frames = (uint64_t)frames * spf;
      *out_frames = (*out_frames > delay + pad)
                  ? *out_frames - delay - pad : 0;
      return 1;
   }
   if (   off + 4 + 32 + 18 <= len
       && !memcmp(b + off + 4 + 32, "VBRI", 4))
   {
      const uint8_t *v = b + off + 4 + 32;
      frames = ((uint32_t)v[14] << 24) | ((uint32_t)v[15] << 16)
             | ((uint32_t)v[16] <<  8) |  (uint32_t)v[17];
      if (!frames)
         return 0;
      /* VBRI states no delay, so only the filler frame is dropped. */
      *out_delay  = (uint64_t)spf;
      *out_frames = (uint64_t)frames * spf;
      return 1;
   }
   return 0;
}
#endif

#ifdef HAVE_RMP3
/* Drain 'frames' frames out of the stream in one of the two pipelines,
 * feeding it the unconsumed tail of the buffer as it asks.  The
 * compressed cursor is kept here rather than in the decoder because a
 * windowed feeder reads it, and because a seek re-points it.  Returns
 * the frames written, short of the ask at end of stream. */
static size_t audio_transfer_mp3_pull(struct audio_transfer_mp3 *m,
      int s16, int16_t *o16, float *of, size_t frames)
{
   size_t done = 0;
   if (!m->stream || !frames)
      return 0;
   while (done < frames)
   {
      size_t rd = 0, wr = 0;
      int    r;
      if (s16)
         rmp3_stream_set_out_s16(m->stream,
               o16 + done * (size_t)m->channels, frames - done);
      else
         rmp3_stream_set_out_f32(m->stream,
               of + done * (size_t)m->channels, frames - done);
      rmp3_stream_set_in(m->stream,
            (const uint8_t*)m->data + m->off, m->size - m->off);
      r = rmp3_stream_process(m->stream, &rd, &wr);
      m->off += rd;
      done   += wr;
      if (r == RMP3_STREAM_ERROR || r == RMP3_STREAM_END)
         break;
      if (r == RMP3_STREAM_NEED_IN && m->off >= m->size)
      {
         /* A frame must be presented whole, so the tail of the stream
          * sits in the hold waiting for a window that will never fill;
          * this is what says the short tail is all there is.  Latched
          * until a seek resets the stream, and once declared the
          * stream answers END when it is spent - it no longer asks
          * for input past EOF, so this branch runs at most once. */
         rmp3_stream_set_eof(m->stream);
         m->eof_sent = 1;
      }
   }
   return done;
}

/* Restart the stream and decode forward to @frame, discarding, in the
 * pipeline of the read that asked - which is what makes the output
 * after the seek byte-identical to a linear decode's.  MP3 frames
 * depend on the bit reservoir of their predecessors, so decoding from
 * the top is the only sample-accurate way in.  Frame numbers here are
 * of the played audio; the priming sits before frame 0 and is part of
 * the distance.  Returns the frame reached, or < 0 if the stream ends
 * first. */
static int64_t audio_transfer_mp3_seek_to(struct audio_transfer_mp3 *m,
      int64_t frame, int s16)
{
   /* Somewhere to throw the frames walked past.  On the stack, and one
    * union rather than two arrays: these were a pair of function-local
    * statics, 24 KiB of BSS of which only ever half was in use, and
    * shared by every context.  audio_mixer runs up to
    * AUDIO_MIXER_MAX_VOICES at once, so two MP3 streams priming in the
    * same callback wrote over each other's sink.  Small because it is
    * only a sink - the loop below iterates until the distance is gone. */
   union { int16_t s16[256]; float f32[256]; } skip;
   uint64_t left = (uint64_t)frame + m->start_trim;
   unsigned ch   = m->channels ? m->channels : 1;

   rmp3_stream_reset(m->stream);
   m->off      = 0;
   m->eof_sent = 0;
   while (left)
   {
      size_t cap  = (sizeof(skip.s16) / sizeof(skip.s16[0])) / ch;
      size_t want = (left < (uint64_t)cap) ? (size_t)left : cap;
      size_t n    = audio_transfer_mp3_pull(m, s16,
            skip.s16, skip.f32, want);
      if (!n)
         return -1;                /* the stream ends before the target */
      left -= n;
   }
   m->trim_left = 0;
   m->emitted   = frame;
   return frame;
}

/* Read past the priming and stop at the padding.  The trim is dropped
 * by decoding it and throwing it away - there is nowhere else for it
 * to go, the frames being coded - and the bound is what the tag says
 * is left after both ends come off. */
static size_t audio_transfer_mp3_read(struct audio_transfer_mp3 *m,
      int s16, int16_t *o16, float *of, size_t frames)
{
   size_t   got;
   union { int16_t s16[256]; float f32[256]; } skip;
   unsigned ch = m->channels ? m->channels : 1;
   while (m->trim_left)
   {
      size_t cap  = (sizeof(skip.s16) / sizeof(skip.s16[0])) / ch;
      size_t want = (m->trim_left < cap) ? (size_t)m->trim_left : cap;
      size_t n    = audio_transfer_mp3_pull(m, s16,
            skip.s16, skip.f32, want);
      if (!n)
      {
         m->trim_left = 0;         /* stream ended inside the priming */
         return 0;
      }
      m->trim_left -= n;
   }
   if (m->limit >= 0)
   {
      int64_t left = m->limit - m->emitted;
      if (left <= 0)
         return 0;
      if ((int64_t)frames > left)
         frames = (size_t)left;
   }
   got = audio_transfer_mp3_pull(m, s16, o16, of, frames);
   m->emitted += (int64_t)got;
   return got;
}
#endif

#ifdef HAVE_RFLAC
/* The next chunk of the stream, from whichever container holds it. */
static int audio_transfer_flac_next(struct audio_transfer_flac *fl)
{
   if (fl->ogg)
   {
      const uint8_t *b = (const uint8_t*)fl->data;
      size_t   body = 0, total;
      unsigned nsegs = 0;
      total = audio_transfer_ogg_page(b, fl->size, fl->pg_off, &body,
            &nsegs);
      if (!total)
         return 0;
      fl->cur     = b + body;
      fl->cur_len = total - (body - fl->pg_off);
      /* The first packet opens with the mapping header - 0x7F, "FLAC",
       * a version and a packet count - and the native stream starts
       * after it. */
      fl->cur_off = (fl->pg_off == 0) ? 9 : 0;
      fl->pg_off += total;
      return fl->cur_len > fl->cur_off;
   }
#ifdef HAVE_RWEBM
   if (fl->demux)
   {
      rwebm_packet pkt;
      for (;;)
      {
         int r = rwebm_read_packet(fl->demux, &pkt);
         /* The resident wall is not the end of the stream.  The pull
          * still reports "no chunk" - nothing here consumed anything,
          * so the retry is exact - but the wall flag lets the read
          * surface the difference instead of ending the sound. */
         if (r == RWEBM_READ_AGAIN)
         {
            fl->wall = 1;
            return 0;
         }
         if (r != 1)
            return 0;
         if (pkt.track == fl->track_idx)
         {
            fl->wall = 0;
            break;
         }
      }
      fl->cur     = pkt.data;
      fl->cur_len = pkt.size;
      fl->cur_off = 0;
      return 1;
   }
#endif
   if (fl->packets)
   {
      /* No pointer kept: the caller may have moved the blob since the
       * last read, so only the index and the offset carry over. */
      uint32_t len;
      if (fl->pkt_index >= fl->num_packets)
         return 0;
      len = fl->pkt_sizes ? fl->pkt_sizes[fl->pkt_index]
                          : (uint32_t)fl->packets_size;
      if (fl->blob_off + len > fl->packets_size)
         return 0;
      fl->cur     = NULL;
      fl->cur_len = len;
      fl->cur_off = 0;
      fl->pkt_index++;
      return len > 0;
   }
   return 0;
}

/* The stream rflac reads: the CodecPrivate header, then every block
 * of the track end to end.  That is exactly a native fLaC stream, so
 * the decoder needs to know nothing about the container. */
/* Hands the decoder its next span.  The decoder owns no reader of its
 * own, so this is the only thing that moves the stream forward: the
 * setup bytes first, then whatever the container yields, and for a
 * resident buffer the whole thing at once.
 *
 * Nothing is reassembled and nothing is copied.  The Matroska arm used
 * to lay every block end to end into one buffer and keep that second
 * copy of the audio for the life of the decoder; a block is now handed
 * over where it already sits.
 *
 * Returns 0 when there is nothing further to give. */
static int audio_transfer_flac_feed(struct audio_transfer_flac *fl)
{
   if (!fl->fed_hdr && fl->hdr && fl->hdr_size)
   {
      rflac_set_in(fl->handle, fl->hdr, fl->hdr_size);
      fl->fed_hdr = 1;
      return 1;
   }

   if (!fl->ogg && !fl->packets
#ifdef HAVE_RWEBM
         && !fl->demux
#endif
      )
   {
      /* Buffer mode: one span, handed over once. */
      if (fl->fed_hdr)
         return 0;
      rflac_set_in(fl->handle, (const uint8_t*)fl->data, fl->size);
      fl->fed_hdr = 1;
      return 1;
   }

   if (!audio_transfer_flac_next(fl))
      return 0;

   /* The delimited-packet arm keeps no pointer, because the caller may
    * move the blob between reads; it is resolved here instead. */
   if (fl->packets && !fl->cur)
   {
      fl->cur      = fl->packets + fl->blob_off;
      fl->blob_off += fl->cur_len;
   }

   rflac_set_in(fl->handle, fl->cur + fl->cur_off, fl->cur_len - fl->cur_off);
   return 1;
}

/* Runs the decoder until the output is full or the input is exhausted. */
static size_t audio_transfer_flac_pull(struct audio_transfer_flac *fl,
      void *out, size_t frames, int as_float)
{
   size_t produced = 0;

   if (as_float)
      rflac_set_out_f32(fl->handle, (float*)out, frames);
   else
      rflac_set_out_s16(fl->handle, (int16_t*)out, frames);

   while (produced < frames)
   {
      size_t rd = 0, wr = 0;
      int    e  = rflac_process(fl->handle, &rd, &wr);

      fl->cursor += rd;
      produced   += wr;

      if (e == RFLAC_PROCESS_ERROR || e == RFLAC_PROCESS_END)
         break;
      if (wr == 0)
      {
         if (fl->drained)
            break;
         if (!audio_transfer_flac_feed(fl))
         {
            fl->drained = 1;
            break;
         }
      }
   }

   return produced;
}

/* Restarts the stream and decodes forward to @frame.  Returns 1 when
 * positioned. */
static uint32_t audio_transfer_flac_seek_to(struct audio_transfer_flac *fl,
      uint64_t frame)
{
   int16_t scratch[1024 * 2];
   uint64_t at;

   if (!fl || !fl->handle)
      return 0;

   /* A byte-addressable source can take the shortcut: the stream's own
    * seek table names a frame boundary at or before the target, and
    * decoding resumes from there instead of from zero.  Only buffer
    * mode qualifies -- a container hands over packets and cannot be
    * pointed at an arbitrary byte -- and only a stream that carries a
    * table, which one inside a container never does. */
   if (!fl->ogg && !fl->packets
#ifdef HAVE_RWEBM
         && !fl->demux
#endif
         && fl->data)
   {
      uint64_t at = 0;
      if (rflac_seek(fl->handle, frame, &at) == RFLAC_PROCESS_NEXT
            && at < fl->size)
      {
         uint64_t landed;
         rflac_seek_resumed(fl->handle, 0);
         rflac_set_in(fl->handle, (const uint8_t*)fl->data + at,
               fl->size - (size_t)at);
         fl->fed_hdr = 1;
         fl->drained = 0;
         fl->cursor  = (size_t)at;
         /* The table names the boundary, not the frame; close the
          * remainder by decoding. */
         landed = rflac_tell(fl->handle);
         for (; landed < frame; )
         {
            int16_t skip[1024 * 2];
            size_t  want = (size_t)(frame - landed);
            size_t  got;
            if (want > 1024)
               want = 1024;
            got = audio_transfer_flac_pull(fl, skip, want, 0);
            if (!got)
               break;
            landed += got;
         }
         if (landed >= frame)
            return 1;
      }
   }

   /* Rewind every source this arm can have. */
   rflac_reset(fl->handle);
   fl->fed_hdr = 0;
   fl->drained = 0;
   fl->cursor  = 0;
   fl->pg_off  = 0;
   fl->cur     = NULL;
   fl->cur_len = 0;
   fl->cur_off = 0;
   fl->pkt_index = 0;
   fl->blob_off  = 0;
#ifdef HAVE_RWEBM
   if (fl->demux)
      rwebm_rewind(fl->demux);
#endif

   for (at = 0; at < frame; )
   {
      size_t want = (size_t)(frame - at);
      size_t got;
      if (want > 1024)
         want = 1024;
      got = audio_transfer_flac_pull(fl, scratch, want, 0);
      if (!got)
         return 0;
      at += got;
   }

   return 1;
}


#endif

#ifdef HAVE_RVORBIS
/* The serial of the first logical bitstream whose first packet is a
 * Vorbis identification header, and how many distinct serials the
 * file carries.  A single-serial file is an ordinary .ogg and goes to
 * rvorbis whole; anything else has to be filtered. */
static int audio_transfer_ogg_survey(const uint8_t *b, size_t size,
      uint32_t *serial, int *nstreams, int *chained, int64_t *total,
      int *mismatch)
{
   uint32_t seen[16];
   uint32_t vser[16];
   int64_t  vgran[16];
   int      n = 0, nv = 0, found = 0, bos_after_data = 0, data_seen = 0;
   int      first_ch = 0, differs = 0;
   uint32_t first_rate = 0;
   size_t   off = 0;
   for (;;)
   {
      size_t   body = 0, psz;
      unsigned nsegs = 0;
      uint32_t ser;
      int      i, known = 0;
      psz = audio_transfer_ogg_page(b, size, off, &body, &nsegs);
      if (!psz)
         break;
      ser = (uint32_t)b[off + 14]        | ((uint32_t)b[off + 15] << 8)
          | ((uint32_t)b[off + 16] << 16) | ((uint32_t)b[off + 17] << 24);
      for (i = 0; i < n; i++)
         if (seen[i] == ser)
            known = 1;
      if (!known && n < (int)(sizeof(seen) / sizeof(seen[0])))
         seen[n++] = ser;
      if (b[off + 5] & 0x02)            /* beginning of a stream       */
      {
         if (data_seen)
            bos_after_data = 1;
         if (body + 7 <= size
               && b[body] == 1 && !memcmp(b + body + 1, "vorbis", 6))
         {
            if (!found)
            {
               *serial = ser;
               found   = 1;
            }
            if (nv < (int)(sizeof(vser) / sizeof(vser[0])))
            {
               vser[nv]    = ser;
               vgran[nv++] = 0;
            }
            /* An identification header states the channels at byte 11
             * and the rate at 12.  Links that disagree cannot all be
             * played by one voice, so note it here rather than let
             * the later ones come out at the first one's rate. */
            if (body + 16 <= size)
            {
               int      lch = b[body + 11];
               uint32_t lr  = (uint32_t)b[body + 12]
                            | ((uint32_t)b[body + 13] << 8)
                            | ((uint32_t)b[body + 14] << 16)
                            | ((uint32_t)b[body + 15] << 24);
               if (!first_ch)
               {
                  first_ch   = lch;
                  first_rate = lr;
               }
               else if (lch != first_ch || lr != first_rate)
                  differs = 1;
            }
         }
      }
      else
         data_seen = 1;
      /* A page's granule position is where its last finished packet
       * leaves the stream, so the last one a bitstream carries is its
       * length.  Summed over the Vorbis bitstreams, that is what the
       * file plays - one for a multiplexed file, one per link for a
       * chained one. */
      {
         int i;
         for (i = 0; i < nv; i++)
            if (vser[i] == ser)
            {
               int64_t g = 0;
               int     k;
               for (k = 7; k >= 0; k--)
                  g = (g << 8) | b[off + 6 + k];
               if (g >= 0)
                  vgran[i] = g;
            }
      }
      off += psz;
   }
   *nstreams = n;
   *chained  = bos_after_data;
   if (mismatch)
      *mismatch = differs;
   if (total)
   {
      int i;
      *total = 0;
      for (i = 0; i < nv; i++)
         *total += vgran[i];
   }
   return found;
}

/* Assemble the next packet of the chosen bitstream, skipping pages
 * belonging to any other.  Shaped like the Opus arm's assembler, with
 * the serial test added; a packet spanning pages is gathered into
 * asm_buf, otherwise it aliases the buffer. */
static int audio_transfer_vorbis_ogg_pkt(struct audio_transfer_vorbis *v,
      const uint8_t **pdata, uint32_t *plen)
{
   const uint8_t *b = (const uint8_t*)v->data;
   size_t asm_len = 0;
   int    spans   = 0;
   for (;;)
   {
      size_t   body = 0, start, run = 0;
      unsigned nsegs = 0;
      size_t   psz = audio_transfer_ogg_page(b, v->size, v->pg_off,
            &body, &nsegs);
      uint32_t ser;
      int      done = 0;
      if (!psz)
         return asm_len ? -1 : 0;
      ser = (uint32_t)b[v->pg_off + 14]
          | ((uint32_t)b[v->pg_off + 15] << 8)
          | ((uint32_t)b[v->pg_off + 16] << 16)
          | ((uint32_t)b[v->pg_off + 17] << 24);
      if (ser != v->ogg_serial)
      {
         v->pg_off += psz;            /* another stream's page         */
         v->seg_idx = 0;
         continue;
      }
      if (v->seg_idx == 0)
         v->body_off = body;
      if (asm_len && v->seg_idx == 0 && !(b[v->pg_off + 5] & 0x01))
         return -1;
      start = v->body_off;
      while (v->seg_idx < nsegs)
      {
         unsigned lace = b[v->pg_off + 27 + v->seg_idx];
         run += lace;
         v->seg_idx++;
         if (lace < 255)
         {
            done = 1;
            break;
         }
      }
      v->body_off = start + run;
      if (v->seg_idx >= nsegs && !done)
      {
         if (run)
         {
            if (asm_len + run > v->asm_cap)
               return -1;
            memcpy(v->asm_buf + asm_len, b + start, run);
            asm_len += run;
         }
         spans      = 1;
         v->pg_off += psz;
         v->seg_idx = 0;
         continue;
      }
      if (v->seg_idx >= nsegs)
      {
         v->pg_off += psz;
         v->seg_idx = 0;
      }
      if (!spans)
      {
         *pdata = b + start;
         *plen  = (uint32_t)run;
      }
      else
      {
         if (asm_len + run > v->asm_cap)
            return -1;
         memcpy(v->asm_buf + asm_len, b + start, run);
         asm_len += run;
         *pdata   = v->asm_buf;
         *plen    = (uint32_t)asm_len;
      }
      return 1;
   }
}
#endif

#ifdef HAVE_RVORBIS
/* Cap a whole-file read at the length the Ogg granules state.
 *
 * rvorbis decodes a chained file through every link, but reports only
 * the first link's length, and at each boundary it hands out the lead
 * of the next link's first block - audio the granules do not account
 * for.  Bounding by the summed granules makes the length reported and
 * the audio emitted the same figure, which is the property the length
 * is meant to have.  For a file with one link the bound is what comes
 * out anyway, so nothing changes there. */
static size_t audio_transfer_vorbis_cap(struct audio_transfer_vorbis *v,
      size_t frames)
{
   if (v->limit >= 0)
   {
      int64_t left = v->limit - v->emitted;
      if (left <= 0)
         return 0;
      if ((int64_t)frames > left)
         frames = (size_t)left;
   }
   return frames;
}
#endif

#ifdef HAVE_RMP3
/* Count what every frame in the file contributes.
 *
 * The fallback for a stream carrying no Xing, Info or VBRI header,
 * which states nothing about its own length.  Each frame header gives
 * its own bitrate and so its own size, which makes this work on a
 * variable-bitrate stream as well as a constant one - the file size
 * and a nominal rate would not.
 *
 * It is a step per frame over four header bytes, not a decode: 1.1 ms
 * for a ten-minute file, measured, which is why the length is worth
 * having rather than reporting nothing.  What it counts is what the
 * decoder emits, priming and all, there being no tag to say which of
 * that was padding. */
static uint64_t audio_transfer_mp3_walk(const uint8_t *b, size_t len)
{
   static const unsigned rates[4] = { 44100, 48000, 32000, 0 };
   static const unsigned br1[16]  = { 0,32,40,48,56,64,80,96,112,128,
                                      160,192,224,256,320,0 };
   static const unsigned br2[16]  = { 0,8,16,24,32,40,48,56,64,80,96,
                                      112,128,144,160,0 };
   size_t   off   = 0;
   uint64_t total = 0;
   if (!b || len < 4)
      return 0;
   if (len > 10 && b[0] == 'I' && b[1] == 'D' && b[2] == '3')
      off = 10 + (((size_t)(b[6] & 0x7f) << 21)
                | ((size_t)(b[7] & 0x7f) << 14)
                | ((size_t)(b[8] & 0x7f) <<  7)
                |  (size_t)(b[9] & 0x7f));
   for (;;)
   {
      unsigned ver, sfi, br, pad, rate, spf, blen;
      while (off + 4 <= len
            && !(b[off] == 0xFF && (b[off + 1] & 0xE0) == 0xE0))
         off++;
      if (off + 4 > len)
         break;
      ver = (b[off + 1] >> 3) & 3;
      sfi = (b[off + 2] >> 2) & 3;
      br  = (b[off + 2] >> 4) & 15;
      pad = (b[off + 2] >> 1) & 1;
      if (ver == 1 || sfi == 3 || br == 0 || br == 15)
      {
         off++;                      /* not a frame header after all   */
         continue;
      }
      rate = rates[sfi];
      if (ver == 2)
         rate /= 2;
      else if (ver == 0)
         rate /= 4;
      if (!rate)
         break;
      spf  = (ver == 3) ? 1152 : 576;
      blen = (ver == 3) ? (144000 * br1[br]) / rate + pad
                        : ( 72000 * br2[br]) / rate + pad;
      if (!blen || off + blen > len)
         break;
      total += spf;
      off   += blen;
   }
   return total;
}
#endif

bool audio_transfer_start(void *data, enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl = (struct audio_transfer_flac*)data;
         if (!fl)
            return false;
         /* A demuxed caller supplies no buffer, so the bytes have to
          * come from one source or the other. */
         if (!fl->data && !(fl->setup && fl->packets))
            return false;
         /* Demuxed: the caller holds the header and the frames apart,
          * which is the shape the containers above are reduced to, so
          * it is served the same way. */
         if (fl->setup && fl->packets)
         {
            fl->hdr       = (const uint8_t*)fl->setup;
            fl->hdr_size  = fl->setup_size;
            fl->pos       = 0;
            fl->pkt_index = 0;
            fl->blob_off  = 0;
            fl->cur       = NULL;
            fl->cur_len   = 0;
            fl->cur_off   = 0;
            fl->handle    = rflac_new();
            return fl->handle != NULL;
         }
         /* Ogg FLAC (RFC 5334).  The first packet opens with the
          * mapping header - 0x7F, "FLAC", a version and a packet
          * count - and the native stream follows it, so the page
          * bodies laid end to end past those nine bytes are what
          * rflac reads.  Served through the same callbacks as the
          * Matroska path, and nothing is reassembled. */
         if (fl->data && fl->size >= 28
               && !memcmp(fl->data, "OggS", 4))
         {
            const uint8_t *b = (const uint8_t*)fl->data;
            size_t   body = 0;
            unsigned nsegs = 0;
            if (   audio_transfer_ogg_page(b, fl->size, 0, &body, &nsegs)
                && body + 5 <= fl->size
                && b[body] == 0x7F
                && !memcmp(b + body + 1, "FLAC", 4))
            {
               fl->ogg      = 1;
               fl->pg_off   = 0;
               fl->hdr      = NULL;
               fl->hdr_size = 0;
               fl->pos      = 0;
               fl->cur      = NULL;
               fl->cur_len  = 0;
               fl->cur_off  = 0;
               fl->handle   = rflac_new();
               return fl->handle != NULL;
            }
         }
#ifdef HAVE_RWEBM
         /* Matroska carrying A_FLAC.  Unlike the Vorbis and Opus
          * containers, nothing has to be synthesised here: the
          * CodecPrivate is a whole fLaC header, magic and metadata
          * blocks, and every block is one raw frame, so the native
          * stream rflac wants is those laid end to end.  What comes
          * out is a real FLAC stream rather than a stand-in - the
          * STREAMINFO states the exact length, and seeking and
          * buffer_tell work on it as they do on a .flac file.
          *
          * The cost is a second copy of the audio, held for the life
          * of the decoder; this is the only arm that keeps one.  The
          * alternative is a packet entry point in rflac, which is the
          * right fix and a larger one. */
         if (fl->data && fl->size >= 4
               && ((const uint8_t*)fl->data)[0] == 0x1A
               && ((const uint8_t*)fl->data)[1] == 0x45
               && ((const uint8_t*)fl->data)[2] == 0xDF
               && ((const uint8_t*)fl->data)[3] == 0xA3)
         {
            const rwebm_track *at = NULL;
            int                i;
            if (!(fl->demux = audio_transfer_webm_open(
                        (const uint8_t*)fl->data, fl->size, 0)))
               return false;
            fl->track_idx = -1;
            for (i = 0; i < rwebm_num_tracks(fl->demux); i++)
            {
               const rwebm_track *t = rwebm_get_track(fl->demux, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_FLAC
                     && t->codec_private_size >= 4)
               {
                  at            = t;
                  fl->track_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            fl->hdr      = at->codec_private;
            fl->hdr_size = at->codec_private_size;
            fl->pos      = 0;
            fl->cur      = NULL;
            fl->cur_len  = 0;
            fl->cur_off  = 0;
            /* No metadata handler: the STREAMINFO the decoder needs
             * is read through the same callbacks as everything else. */
            fl->handle   = rflac_new();
            return fl->handle != NULL;
         }
#endif
         fl->handle = rflac_new();
         return fl->handle != NULL;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         int64_t duration_ns = 0;
         int64_t walked      = -1;   /* exact length from the header walk */
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
            uint32_t ser = 0;
            int      nstreams = 0, chained = 0;
            int64_t  gtotal = 0;
            int      isvorbis, mismatch = 0;
            if (!v->data)
               return false;
            isvorbis = audio_transfer_ogg_survey((const uint8_t*)v->data,
                  v->size, &ser, &nstreams, &chained, &gtotal,
                  &mismatch);
            /* One logical bitstream is an ordinary .ogg and goes to
             * rvorbis whole, which is the path every such file has
             * always taken.  More than one and it cannot: rvorbis has
             * no notion of a serial number, so a multiplexed file
             * would feed it the other stream's pages.  Those go
             * through the packet path instead, where the pages can be
             * filtered. */
            if (isvorbis && nstreams > 1 && !chained)
            {
               const uint8_t *hdr[3];
               uint32_t       hlen[3];
               int            i, ok = 1;
               v->asm_cap = 65536;
               if (!(v->asm_buf = (uint8_t*)malloc(v->asm_cap)))
                  return false;
               v->ogg_pkt    = 1;
               v->ogg_serial = ser;
               v->pg_off     = 0;
               v->seg_idx    = 0;
               v->body_off   = 0;
               for (i = 0; i < 3; i++)
                  if (audio_transfer_vorbis_ogg_pkt(v, &hdr[i], &hlen[i])
                        != 1)
                  {
                     ok = 0;
                     break;
                  }
               if (!ok)
                  return false;
               /* Identification and setup; the comment packet in
                * between is not needed to decode. */
               v->handle = rvorbis_open_packets(hdr[0], (int)hlen[0],
                     hdr[2], (int)hlen[2], &err, NULL);
               if (!v->handle)
                  return false;
               v->packet = 1;
            }
            else
            {
               /* Feed until the setup headers are in.  That is all the
                * residency an open needs; the rest of the file is read
                * as it plays. */
               size_t rd, wr;
               if (!(v->stream = rvorbis_stream_new()))
                  return false;
               for (;;)
               {
                  int r;
                  rvorbis_stream_set_out_s16(v->stream, NULL, 0);
                  rvorbis_stream_set_in(v->stream,
                        (const uint8_t*)v->data + v->stream_off,
                        v->size - v->stream_off);
                  r = rvorbis_stream_process(v->stream, &rd, &wr);
                  v->stream_off += rd;
                  if (rvorbis_stream_info(v->stream, NULL))
                     break;
                  if (r != RVORBIS_STREAM_NEED_IN || !rd
                        || v->stream_off >= v->size)
                  {
                     rvorbis_stream_free(v->stream);
                     v->stream = NULL;
                     return false;
                  }
               }
               v->handle = rvorbis_stream_decoder(v->stream);
               if (!v->handle)
                  return false;
               (void)err;
            }
            /* The granules say what the file plays.  For a chained
             * file that is the sum over its links, which rvorbis
             * decodes through but reports only the first of; for a
             * multiplexed one it is the chosen bitstream's, which
             * also trims the last packet's overhang. */
            if (gtotal > 0)
               v->limit = gtotal;
            /* Links that disagree on rate or channels: the voice is
             * built around the first one's, so play that link and
             * stop rather than run the rest at the wrong speed.  The
             * length becomes the first link's granule, which is what
             * comes out. */
            if (mismatch)
            {
               int64_t first = rvorbis_stream_length_in_samples(v->handle);
               if (first > 0)
                  v->limit = first;
            }
         }
         v->channels = rvorbis_get_info(v->handle).channels;
         /* A caller's packet blob is resident by definition, so the
          * same header walk gives its exact length.  No padding is
          * stated for it - the set is the caller's - so the last
          * packet's overlap-add overhang is part of what plays, and
          * the total is what comes out. */
         if (v->packets && v->channels > 0)
         {
            const uint8_t *pd = NULL;
            uint32_t       pl = 0;
            int64_t        total = 0, pad = 0;
            int            n = 0, ok = 1;
            while (audio_transfer_vorbis_pull(v, &pd, &pl, &pad) == 1)
            {
               int fr = rvorbis_packet_frames(v->handle, pd, pl);
               if (fr < 0)
               {
                  ok = 0;
                  break;
               }
               if (++n > 1)
                  total += fr;
            }
            v->pkt_index  = 0;
            v->pkt_offset = 0;
            v->emitted    = 0;
            if (ok && total > 0)
               v->limit = total;
            return true;
         }
#ifdef HAVE_RWEBM
         /* Resident WebM: walk the packet headers once and learn the
          * stream's exact length.  rvorbis_packet_frames reads a
          * packet's contribution off its first two bytes, so the walk
          * decodes nothing - about 2 ms for a half-hour track, against
          * a load that has already read it off disk - and what it
          * totals, less the container's DiscardPadding, is the length
          * of the audio to the sample.  A Duration cannot be that: it
          * rounds up past the overhang the last block's overlap-add
          * runs into.
          *
          * A windowed context does not walk - it would run at the wall
          * - and keeps the Duration bound below, which the drain
          * tightens when the padding block finally arrives. */
         if (v->demux && !v->avail && v->channels > 0)
         {
            const uint8_t *pd = NULL;
            uint32_t       pl = 0;
            int64_t        total = 0;
            int64_t        pad   = 0;
            int64_t        pads  = 0;
            int            n     = 0;
            int            ok    = 1;
            while (audio_transfer_vorbis_pull(v, &pd, &pl, &pad) == 1)
            {
               int fr = rvorbis_packet_frames(v->handle, pd, pl);
               pads  += pad;
               if (fr < 0)
               {
                  ok = 0;
                  break;
               }
               /* A decode's first packet primes the window and yields
                * nothing, so the length counts from the second. */
               if (++n > 1)
                  total += fr;
            }
            rwebm_rewind(v->demux);
            v->emitted = 0;
            if (ok && total > 0)
            {
               walked = total - pads;
               if (walked < 0)
                  walked = 0;
            }
         }
#endif
         /* Two statements about where the audio ends, and neither is
          * always the better one.
          *
          * The walk totals what the packets decode to and takes the
          * container's DiscardPadding off, which is exact - but only
          * where a padding was written.  A file remuxed from Ogg
          * carries one, because the granule said where the audio
          * stopped; a file encoded straight to WebM often does not,
          * and then the walk's total is the whole overlap-add
          * overhang, trimmed by nothing.
          *
          * A Duration is stated by the muxer rather than derived, so
          * it does not know about the overhang and overshoots the
          * audio - but by less than an untrimmed walk does, on the
          * files where no padding was written.
          *
          * So take whichever is tighter.  With a padding that is the
          * walk, which is exact and always below the Duration; with
          * none it is usually the Duration, and where it is not - a
          * clip shorter than the Duration claims - the walk still
          * bounds it.  The demuxed path states neither, its packet
          * set being the caller's, and stays unbounded. */
         if (duration_ns > 0)
         {
            int64_t rate = (int64_t)rvorbis_get_info(v->handle).sample_rate;
            if (rate > 0)
            {
               int64_t stated = (duration_ns * rate + 500000000)
                              / 1000000000;
               if (walked < 0 || stated < walked)
                  walked = stated;
            }
         }
         if (walked >= 0)
            v->limit = walked;
         return true;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->data)
            return false;
         if (!(m->stream = rmp3_stream_new()))
            return false;
         /* Walk parse-only until the first frame is located, which is
          * where the channel count and rate come from - MPEG audio has
          * no header ahead of the stream to read them from.  Locating
          * decodes nothing, so the walk costs a scan; a buffer with no
          * frame in it fails here, as the resident open did.  Then back
          * to the head, so the first read decodes from frame one. */
         m->off      = 0;
         m->eof_sent = 0;
         for (;;)
         {
            size_t rd = 0, wr = 0;
            int    r;
            rmp3_stream_set_out_s16(m->stream, NULL, 0);
            rmp3_stream_set_in(m->stream,
                  (const uint8_t*)m->data + m->off, m->size - m->off);
            r = rmp3_stream_process(m->stream, &rd, &wr);
            m->off += rd;
            if (rmp3_stream_info(m->stream, &m->channels, &m->rate))
               break;
            if (r == RMP3_STREAM_NEED_IN && m->off >= m->size
                  && !m->eof_sent)
            {
               /* A file shorter than the reassembly hold never fills
                * it; say the short tail is all there is, so its frame
                * is still found.  Past this the stream never asks for
                * input again - a spent stream answers END - so failing
                * to locate a frame surfaces below, not as a second
                * NEED_IN. */
               rmp3_stream_set_eof(m->stream);
               m->eof_sent = 1;
               continue;
            }
            if (r == RMP3_STREAM_ERROR || r == RMP3_STREAM_END)
            {
               rmp3_stream_free(m->stream);
               m->stream = NULL;
               return false;
            }
         }
         rmp3_stream_reset(m->stream);
         m->off      = 0;
         m->eof_sent = 0;
         m->seek_to  = -1;
         m->limit    = -1;
         {
            uint64_t fr = 0, delay = 0;
            if (!audio_transfer_mp3_gapless((const uint8_t*)m->data,
                     m->size, &fr, &delay))
            {
               /* No header to state a length; count the frames. */
               uint64_t walked = audio_transfer_mp3_walk(
                     (const uint8_t*)m->data, m->size);
               if (walked)
                  m->limit = (int64_t)walked;
            }
            else
            {
               /* set_start_trim, if the caller used it, wins - it is
                * a statement from a container that knows better than
                * the stream's own tag. */
               if (!m->start_trim)
                  m->start_trim = delay;
               m->limit = (int64_t)fr;
            }
         }
         m->trim_left = m->start_trim;
         m->emitted   = 0;
         return true;
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
         /* rwav admits 8-, 16- and 24-bit integer PCM, 32-bit IEEE
          * float, the two companded eight-bit formats and the two
          * four-bit ADPCM layouts; all of them convert on the way out
          * below, the last four through rwav's own decoder. */
         if (     w->wav.bitspersample != 16
               && w->wav.bitspersample != 8
               && w->wav.bitspersample != 24
               && w->wav.bitspersample != 32
               && w->wav.bitspersample != 4)
            return false;
         /* Meaningless for a block-coded payload, which is not
          * addressable a frame at a time; the decoder is given the
          * frame index instead. */
         w->framesz = (w->wav.bitspersample >= 8)
                    ? (size_t)w->wav.numchannels
                      * (size_t)(w->wav.bitspersample / 8)
                    : 0;
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
            op->limit = -1;
            if (!op->avail)
            {
               /* Resident: walk the packets' TOC bytes once, which
                * decodes nothing, and the total less the pre-skip and
                * the container's padding is the exact length. */
               while (rwebm_read_packet(op->demux, &pkt) == 1)
               {
                  if (pkt.track != op->track_idx)
                     continue;
                  toc += audio_transfer_opus_pkt_frames(pkt.data,
                        pkt.size);
                  if (pkt.discard_padding > 0)
                     discard_ns += pkt.discard_padding;
               }
               rwebm_rewind(op->demux);
               preskip = (int64_t)ropus_preskip(op->handle);
               if (toc > 0)
               {
                  op->limit = toc - preskip
                     - (discard_ns * 48000 + 500000000) / 1000000000;
                  if (op->limit < 0)
                     op->limit = 0;
               }
            }
            else
            {
               /* Windowed: the walk would run at the wall, into pages
                * that are reserved rather than populated.  A stated
                * Duration bounds emission for now - it overshoots the
                * audio, by 384 frames on the files measured - and the
                * fill replaces it with the exact end when the block
                * carrying the padding arrives. */
               int64_t dur = rwebm_duration_ns(op->demux);
               if (dur > 0)
                  op->limit = (dur * 48000 + 500000000) / 1000000000;
            }
            op->emitted   = 0;
            op->toc_total = 0;
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
#ifdef HAVE_RWEBM
         /* buffer mode, Matroska (.mka/.mkv) carrying A_AAC.  Same
          * shape as the MP4 path below: the AudioSpecificConfig is the
          * track's CodecPrivate and the access units are its blocks.
          * The encoder delay comes from CodecDelay rather than an edit
          * list, and the length from the container's Duration, which
          * is net of that delay. */
         if (!ac->setup && ac->buf && ac->buf_size >= 4
               && ac->buf[0] == 0x1A && ac->buf[1] == 0x45
               && ac->buf[2] == 0xDF && ac->buf[3] == 0xA3)
         {
            const rwebm_track *at = NULL;
            int      i;
            int64_t  dur;
            if (!(ac->wdemux = audio_transfer_webm_open(ac->buf,
                        ac->buf_size, 0)))
               return false;
            ac->wtrack_idx = -1;
            for (i = 0; i < rwebm_num_tracks(ac->wdemux); i++)
            {
               const rwebm_track *t = rwebm_get_track(ac->wdemux, i);
               if (t && t->type == RWEBM_TRACK_AUDIO
                     && t->codec == RWEBM_CODEC_AAC
                     && t->codec_private_size)
               {
                  at             = t;
                  ac->wtrack_idx = i;
                  break;
               }
            }
            if (!at)
               return false;
            ac->handle = raac_open(at->codec_private,
                  at->codec_private_size);
            if (!ac->handle)
               return false;
            /* CodecDelay is nanoseconds of decoded output to drop from
             * the front - the priming an AAC encoder always codes. */
            if (at->codec_delay_ns && at->sample_rate)
               ac->start_trim = (uint64_t)
                  ((at->codec_delay_ns * (int64_t)at->sample_rate
                    + 500000000) / 1000000000);
            /* Duration here spans what the stream codes, priming
             * included - unlike an MP4 edit list, which states the
             * play length net of it.  The bound is checked against
             * frames handed out, which are post-trim, so the trim
             * comes off it. */
            dur = rwebm_duration_ns(ac->wdemux);
            if (dur > 0 && at->sample_rate)
            {
               ac->limit = (dur * (int64_t)at->sample_rate + 500000000)
                  / 1000000000 - (int64_t)ac->start_trim;
               if (ac->limit < 0)
                  ac->limit = 0;
            }
         }
         else
#endif
#ifdef HAVE_RMP4
         /* buffer mode: a whole MP4/M4A; demux it here */
         if (!ac->setup && ac->buf)
         {
            const rmp4_track *at = NULL;
            int i;
            /* Bounded open: a windowed caller has only a prefix
             * resident, and the unbounded open walks the box tree to
             * the end of the file - which for a leading-moov movie is
             * a reserved-page read past the head.  avail == 0 is the
             * whole-file caller and behaves exactly as before. */
            if (!(ac->demux = rmp4_open_memory_avail(ac->buf,
                        ac->buf_size,
                        (ac->avail && ac->avail < ac->buf_size)
                           ? ac->avail : ac->buf_size,
                        NULL, NULL, NULL)))
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
            {
               int64_t ns = rmp4_duration_ns(ac->demux);
               unsigned sr = raac_sample_rate(ac->handle);
               if (ns > 0 && sr)
                  ac->limit = (ns * (int64_t)sr + 500000000) / 1000000000;
               /* the track's edit list carries the encoder delay in
                * media-timescale units, i.e. core-rate samples; the
                * trim is consumed in output frames, which double under
                * SBR, so convert by the rate ratio */
               ac->start_trim = at->media_skip;
               if (at->sample_rate && sr && sr != at->sample_rate)
                  ac->start_trim = (at->media_skip * sr
                        + at->sample_rate / 2) / at->sample_rate;
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
         {
            size_t need = (size_t)raac_frame_len(ac->handle)
                  * ac->channels;
            if (need > ac->pend_cap)
            {
               float *nf = (float*)realloc(ac->pend_f32,
                     need * sizeof(float));
               if (!nf)
               {
                  raac_close(ac->handle);
                  ac->handle = NULL;
                  return false;
               }
               ac->pend_f32 = nf;
               ac->pend_cap = need;
            }
         }
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
         return (m && m->stream);
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
         {
            const rflac_format_t *ff = rflac_format(fl->handle);
            if (!ff)
            {
               /* The header has not been seen yet.  Hand over the
                * setup bytes and let the decoder read it, which costs
                * one span and no audio. */
               if (!audio_transfer_flac_feed(fl))
                  return false;
               rflac_process(fl->handle, NULL, NULL);
               if (!(ff = rflac_format(fl->handle)))
                  return false;
            }
            if (channels)
               *channels     = (unsigned)ff->channels;
            if (rate)
               *rate         = (unsigned)ff->sample_rate;
            if (total_frames)
               *total_frames = rflac_total_frames(fl->handle);
         }
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
            /* Whatever bound was established, on either path: the
             * container's for a packet-fed context, the summed Ogg
             * granules for a buffer.  Only where nothing stated one
             * does this fall back to asking rvorbis, which walks to
             * the last granule of the first link and so answers for
             * a chained file with its first link alone. */
            if (v->limit >= 0)
               *total_frames = (uint64_t)v->limit;
            else if (v->packet)
               *total_frames = 0;
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
         if (!m || !m->stream)
            return false;
         if (channels)
            *channels     = m->channels;
         if (rate)
            *rate         = m->rate;
         /* From the Xing/Info or VBRI count, less the gapless trim
          * where a LAME tag states one; 0 for a file carrying no
          * such header, which cannot be measured without a walk. */
         if (total_frames)
            *total_frames = (m->limit >= 0) ? (uint64_t)m->limit : 0;
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
/* Room for the reassembly buffer, allocated on the first packet that
 * needs it and never grown after: AUDIO_OPUS_MAX_PACKET is the whole
 * of what one can be, so a request past it is a malformed stream and
 * not a reason to reallocate - which is what the fixed array this
 * replaces already enforced. */
static int audio_transfer_opus_asm(struct audio_transfer_opus *op,
      size_t need)
{
   if (need > AUDIO_OPUS_MAX_PACKET)
      return 0;
   if (!op->asm_buf)
   {
      if (!(op->asm_buf = (uint8_t*)malloc(AUDIO_OPUS_MAX_PACKET)))
         return 0;
      op->asm_cap = AUDIO_OPUS_MAX_PACKET;
   }
   return 1;
}

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
            if (!audio_transfer_opus_asm(op, asm_len + run))
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
         if (!audio_transfer_opus_asm(op, asm_len + run))
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
      const uint8_t **pdata, uint32_t *plen, int64_t *ppad)
{
   *ppad = 0;
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
         {
            op->wall = 1;
            return -2;
         }
         if (r != 1)
            return 0;
         if (pkt.track == op->track_idx)
         {
            op->wall = 0;
            /* Handed back with the packet rather than left in the
             * context: see the Vorbis pull. */
            if (pkt.discard_padding > 0)
               *ppad = (pkt.discard_padding * 48000 + 500000000)
                  / 1000000000;
            break;
         }
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
   op->toc_total    = 0;
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
      int64_t pad = 0;
      int r = audio_transfer_opus_pull(op, &pdata, &plen, &pad);
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
   size_t elem = (fmt == 1) ? sizeof(int16_t) : sizeof(float);

   /* The decoder writes ROPUS_MAX_FRAME * channels samples into
    * this and is given no bound, so the buffer is made to fit before
    * the first packet of a format goes through it.  A format change
    * can only follow a rewind, which leaves nothing pending. */
   if (!op->pend || op->pend_elem != elem)
   {
      if (!op->channels)
         return -1;
      free(op->pend);
      op->pend_elem    = 0;
      op->pend_samples = 0;
      if (!(op->pend = calloc((size_t)ROPUS_MAX_FRAME
                  * (size_t)op->channels, elem)))
         return -1;
      op->pend_elem    = elem;
      op->pend_samples = (size_t)ROPUS_MAX_FRAME * (size_t)op->channels;
      op->pend_frames = 0;
      op->pend_pos    = 0;
   }

   while (op->pend_frames == 0)
   {
      const uint8_t *pdata;
      uint32_t plen;
      int64_t  pad = 0;
      int r;
      unsigned skip;
      r = audio_transfer_opus_pull(op, &pdata, &plen, &pad);
      /* The resident wall is not the end of the stream and not an
       * error: report no frames for now, so the read comes up short
       * and the next call resumes once the feeder has caught up.
       * (The pull raised op->wall; read_* turns the resulting empty
       * read into AUDIO_PROCESS_NEXT rather than END.) */
      if (r == -2)
         return 0;
      if (r <= 0)
         return r;
      if (fmt == 1)
         r = ropus_decode_s16(op->handle, pdata, plen,
               (int16_t*)op->pend, op->pend_samples);
      else
         r = ropus_decode_f32(op->handle, pdata, plen,
               (float*)op->pend, op->pend_samples);
      if (r < 0)
         return -1;
      op->pend_frames = (size_t)r;
      op->pend_pos    = 0;
      op->toc_total  += (int64_t)r;
      /* That block carried the container's end trimming, so the
       * stream's true length is known now: everything its packets
       * decode to, less the pre-skip dropped at the front and the
       * padding dropped at the back.  A pre-walk at open works this
       * out in advance and arrives at the same number; where one
       * could not run, this is where it comes from. */
      if (pad > 0)
      {
         int64_t exact = op->toc_total
                       - (int64_t)ropus_preskip(op->handle)
                       - pad;
         if (exact < 0)
            exact = 0;
         if (op->limit < 0 || exact < op->limit)
            op->limit = exact;
      }
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
       * be consumed, so credit them here.
       *
       * Counted whether or not a bound is known yet, which matters
       * because one need not be known at open.  A windowed WebM that
       * states no Duration has none until the block carrying the
       * padding arrives and says where the end is - and a count that
       * had not been running until then would be compared against
       * zero, letting the whole stream through untrimmed. */
      if (op->ogg
#ifdef HAVE_RWEBM
            || op->demux
#endif
         )
      {
         if (op->limit >= 0 && op->pend_frames)
         {
            int64_t left = op->limit - op->emitted;
            if (left <= 0)
            {
               op->pend_frames = 0;
               return 0;
            }
            if ((int64_t)op->pend_frames > left)
               op->pend_frames = (size_t)left;
         }
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
      /* Windowed ADTS: stop at the resident prefix rather than the
       * buffer end, and say "again" instead of "end of stream". */
      if (ac->adts && ac->avail && ac->adts_pos + 7 > ac->avail)
         return 2;
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
#ifdef HAVE_RWEBM
      if (ac->wdemux)
      {
         /* buffer mode, Matroska: the next block of the AAC track */
         rwebm_packet wpkt;
         for (;;)
         {
            int r = rwebm_read_packet(ac->wdemux, &wpkt);
            /* Resident wall, not end of stream - the same distinction
             * the rmp4 branch below has always drawn; conflating them
             * here had a Matroska AAC voice end (and a looping one
             * rewind mid-file) whenever the feeder fell one tick
             * behind.  2 also makes the seek walk refuse rather than
             * treat the wall as the stream ending under it. */
            if (r == RWEBM_READ_AGAIN)
               return 2;
            if (r != 1)
               return 0;
            if (wpkt.track == ac->wtrack_idx)
               break;
         }
         *pdata = wpkt.data;
         *plen  = (uint32_t)wpkt.size;
         return 1;
      }
#endif
#ifdef HAVE_RMP4
      if (ac->demux)
      {
         /* buffer mode: pull the next access unit from the demuxer */
         rmp4_packet pkt;
         for (;;)
         {
            int r = rmp4_read_packet(ac->demux, &pkt);
            /* The sample's bytes are not resident yet.  This is NOT
             * end of stream: reporting 0 here would have the mixer
             * loop or stop the voice mid-file the moment the feeder
             * fell a window behind. */
            if (r == RMP4_READ_AGAIN)
               return 2;
            if (r != 1)
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
#define AUDIO_AAC_PREROLL 3   /* access units decoded before target */

/* Every access unit this decoder takes yields a fixed frame count -
 * raac_frame_len(), the core 1024 doubled under SBR - so the walk to a
 * target needs no decoding at all; only the pre-roll does.  Returns
 * the frame reached, or < 0 if the stream ends first.
 *
 * Streams using PNS reconstruct their noise bands from a generator
 * whose state advances with every draw; a seek skips the draws of the
 * walked-over access units, so noise samples after a seek differ from
 * a straight decode (by an LSB or so at s16).  Deterministic loops on
 * PNS content need seek(0), which raac_reset makes exact. */
static int audio_transfer_aac_fill(struct audio_transfer_aac *ac);

static int64_t audio_transfer_aac_seek_to(struct audio_transfer_aac *ac,
      int64_t frame)
{
   int64_t  pos  = 0;
   uint64_t skip = ac->start_trim;
   int64_t  flen = (int64_t)raac_frame_len(ac->handle);
   int64_t  stop = frame - AUDIO_AAC_PREROLL * flen;

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
      int64_t d = flen;
      int r = audio_transfer_aac_pull(ac, &pdata, &plen);
      /* r == 2 left pdata/plen unset: a target past the resident wall
       * is refused rather than approximated, per the windowing
       * contract, and the caller retries once the feeder has raised
       * the bound past it. */
      if (r == 2 || r <= 0)
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
      if (r == 0 || r == 2)   /* 2: refused, see the walk above */
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
      if (r == 2)
      {
         ac->wall = 1;
         return 2;               /* not yet resident; no frames, no EOF */
      }
      if (r <= 0)
         return r;
      ac->wall = 0;
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


/* Did the last empty read stop at the resident wall rather than the
 * end of the stream?  Consulted by the read tails below: an empty
 * read at the wall returns AUDIO_PROCESS_NEXT - no frames yet, call
 * again once the feeder has raised the bound - while an empty read at
 * the true end keeps returning AUDIO_PROCESS_END, which is what lets
 * a looping mixer voice rewind at the loop point and only there.
 * Conflating the two is what turned a feeder one tick behind into a
 * mid-file rewind, and two ticks behind into the voice releasing. */
static int audio_transfer_wall_stalled(void *data,
      enum audio_type_enum type)
{
   switch (type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
         return ((struct audio_transfer_vorbis*)data)->wall;
#endif
#ifdef HAVE_ROPUS
      case AUDIO_TYPE_OPUS:
         return ((struct audio_transfer_opus*)data)->wall;
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
         return ((struct audio_transfer_aac*)data)->wall;
#endif
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
         return ((struct audio_transfer_flac*)data)->wall;
#endif
      default:
         break;
   }
   return 0;
}

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
         produced = audio_transfer_flac_pull(fl, out, frames, 0);
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
         {
            frames   = audio_transfer_vorbis_cap(v, frames);
            produced = audio_transfer_vorbis_stream_pull(v, 1, out, NULL,
                  frames);
            v->emitted += (int64_t)produced;
         }
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->stream)
            return AUDIO_PROCESS_ERROR;
         if (m->seek_to >= 0)
         {
            int64_t at = audio_transfer_mp3_seek_to(m, m->seek_to, 1);
            m->seek_to = -1;
            if (at < 0)
               return AUDIO_PROCESS_ERROR;
         }
         produced = audio_transfer_mp3_read(m, 1, out, NULL, frames);
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
                  (const int16_t*)op->pend + op->pend_pos * op->channels,
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
            /* 2 is "not resident yet".  It must break like end of
             * stream and not fall through: pend_frames is 0 there, so
             * take would be 0 and this loop would spin forever on a
             * window the feeder has not caught up with. */
            if (r == 0 || r == 2)
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
         /* The companded and block-coded payloads are not readable a
          * sample at a time from the buffer - a-law is a curve and
          * ADPCM continues predictor state a block establishes - so
          * those go through rwav's decoder, which yields s16 natively
          * and steps into whichever block holds the cursor. */
         if (w->wav.format != RWAV_FORMAT_PCM
               && w->wav.format != RWAV_FORMAT_FLOAT)
         {
            want       = rwav_decode_s16(&w->wav, w->data, w->cursor,
                  want, out);
            w->cursor += want;
            produced   = want;
            break;
         }
         src   = w->data + w->wav.dataoffset + w->cursor * w->framesz;
         if (w->wav.bitspersample == 16)
         {
#ifndef MSB_FIRST
            /* Interleaved PCM16 on a little-endian host is already in
             * output order, and the loop below is a memcpy spelled
             * slowly - about thirteen instructions a sample to
             * reassemble what is already sitting there.  Measured at
             * 5x on a 69k-frame clip.  rwav_decode_s16 takes the same
             * shortcut under the same condition; this arm indexes by
             * framesz, which is the packed frame width, so the stride
             * condition rwav has to test is structural here. */
            memcpy(out, src, n * sizeof(int16_t));
#else
            /* rwav.h's accessors read the file's little-endian words a
             * byte at a time: right on either endianness, and safe
             * where an unaligned load would fault.  Big-endian keeps
             * the loop, whose byte assembly is the swab it needs. */
            for (i = 0; i < n; i++)
               out[i] = rwav_s16(src + i * 2);
#endif
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
            /* Biased before the shift, not after: (v - 128) is
             * negative for the lower half of the range and shifting a
             * negative value left is undefined.  src[i] << 8 is at
             * most 65280 and fits an int on every target this builds
             * for, so the same numbers come out of a defined
             * expression. */
            for (i = 0; i < n; i++)
               out[i] = (int16_t)(((int)src[i] << 8) - 32768);
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
   if (produced == 0 && audio_transfer_wall_stalled(data, type))
      return AUDIO_PROCESS_NEXT;   /* starved, not finished: retry     */
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
         produced = audio_transfer_flac_pull(fl, out, frames, 1);
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
         frames = audio_transfer_vorbis_cap(v, frames);
         (void)got;
         produced = audio_transfer_vorbis_stream_pull(v, 0, NULL, out, frames);
         v->emitted += (int64_t)produced;
         break;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->stream)
            return AUDIO_PROCESS_ERROR;
         if (m->seek_to >= 0)
         {
            int64_t at = audio_transfer_mp3_seek_to(m, m->seek_to, 0);
            m->seek_to = -1;
            if (at < 0)
               return AUDIO_PROCESS_ERROR;
         }
         produced = audio_transfer_mp3_read(m, 0, NULL, out, frames);
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
         /* See the s16 path: these decode through rwav.  Their native
          * result is s16, so scaling it here loses nothing. */
         if (w->wav.format != RWAV_FORMAT_PCM
               && w->wav.format != RWAV_FORMAT_FLOAT)
         {
            /* Staged through a buffer this context owns rather than a
             * 64 KiB function-local static.  The static was shared by
             * every context - two block-coded voices in one mixer
             * callback wrote over each other - and larger than L1D, so
             * the scale below evicted the cache on every call.
             *
             * Per context and grown to the request, not blocked
             * through a small stack buffer: rwav_decode_s16 starts at
             * the block containing the frame it is given, so a chunk
             * smaller than a block re-decodes that block every call.
             * Measured at 88.2 -> 12.9 Mframe/s on MS ADPCM with a
             * 128-frame chunk.  One pass keeps the old access
             * pattern exactly. */
            size_t need = want * ch;
            if (w->scratch_cap < need)
            {
               int16_t *p = (int16_t*)realloc(w->scratch,
                     need * sizeof(int16_t));
               if (!p)
                  return AUDIO_PROCESS_ERROR;
               w->scratch     = p;
               w->scratch_cap = need;
            }
            want = rwav_decode_s16(&w->wav, w->data, w->cursor, want,
                  w->scratch);
            for (i = 0; i < want * ch; i++)
               out[i] = (float)w->scratch[i] * (1.0f / 32768.0f);
            w->cursor += want;
            produced   = want;
            break;
         }
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
#ifndef MSB_FIRST
            memcpy(out, src, n * sizeof(float));
#else
            for (i = 0; i < n; i++)
               out[i] = rwav_f32(src + i * 4);
#endif
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
                  (const float*)op->pend + op->pend_pos * op->channels,
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
            if (r == 0 || r == 2)   /* see the s16 arm: 2 must break */
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
   if (produced == 0 && audio_transfer_wall_stalled(data, type))
      return AUDIO_PROCESS_NEXT;   /* starved, not finished: retry     */
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
            /* Demuxed: the frontier is a byte offset into the
             * caller's packet blob rather than into a buffer set by
             * set_buffer_ptr, there being none.  It is the same
             * quantity for the same purpose - how far the decoder has
             * read - and a feeder growing the set is the caller that
             * wants it.  The Opus and AAC demuxed arms still report
             * nothing here; this one has a packet cursor to report. */
            if (v->packets)
               return v->pkt_offset;
            /* Self-framed Ogg buffer: the demuxer consumes the buffer
             * as it plays, so the cursor kept alongside it is the
             * compressed frontier - and now a real one, the bytes
             * behind it being releasable rather than merely read. */
            if (v->stream)
               return v->stream_off;
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
         if (!w->opened)
            return 0;
         /* A block-coded payload has no per-frame byte position; the
          * frontier is the block the cursor sits in. */
         if (w->wav.format == RWAV_FORMAT_MS_ADPCM
               || w->wav.format == RWAV_FORMAT_IMA_ADPCM)
            return w->wav.dataoffset
                 + (w->cursor / w->wav.samplesperblock + 1)
                 * (size_t)w->wav.blockalign;
         return w->wav.dataoffset + w->cursor * w->framesz;
      }
#endif
#ifdef HAVE_RFLAC
      case AUDIO_TYPE_FLAC:
      {
         struct audio_transfer_flac *fl =
               (struct audio_transfer_flac*)data;
         if (!fl->handle)
            return 0;
         /* One answer for every arm now.  The decoder reads nothing for
          * itself, so what it has consumed is exactly what this side
          * handed over, counted as it went.  The three per-arm cursors
          * this replaced existed only because a pulling decoder cannot
          * be asked where it is. */
         return fl->cursor;
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m =
               (struct audio_transfer_mp3*)data;
         /* The stream consumes the buffer as it plays, so the cursor
          * kept alongside it is the compressed frontier - a real one,
          * the bytes behind it being releasable rather than merely
          * read.  It leads the decode position by at most the stream's
          * reassembly hold, which is the safe side for a feeder. */
         if (m->stream)
            return m->off;
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
          * compressed frontier the feeder needs. */
         if (op->handle && op->ogg)
            return op->pg_off;
#ifdef HAVE_RWEBM
         /* WebM likewise: the packets are decoded where the demuxer
          * points at them in the caller's buffer, so its walk position
          * is that frontier. */
         if (op->handle && op->demux)
            return rwebm_tell(op->demux);
#endif
         /* Demuxed: a byte offset into the caller's packet blob,
          * there being no set_buffer_ptr buffer to hold one.  The
          * same quantity for the same purpose as the two above - how
          * far the decoder has read - and what a feeder growing the
          * set wants.  The Vorbis arm reports it for its blob; this
          * one was left behind when that was added. */
         if (op->handle && op->packets)
            return op->pkt_offset;
         return 0;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac =
               (struct audio_transfer_aac*)data;
         /* The ADTS buffer path walks the caller's buffer linearly;
          * adts_pos is the next frame's byte offset, the compressed
          * frontier a feeder needs. */
         if (ac->handle && ac->adts)
            return ac->adts_pos;
         /* Demuxed: see the Opus arm - a byte offset into the
          * caller's packets rather than into a buffer it did not
          * set. */
         if (ac->handle && ac->packets)
            return ac->pkt_offset;
#ifdef HAVE_RMP4
         /* MP4: the packets are decoded where the demuxer points at
          * them in the caller's buffer, so its consumed offset is the
          * compressed frontier a feeder needs - the same quantity
          * rwebm_tell reports for the WebM arms.  Without this a
          * windowed M4A stream could be given a bound but the feeder
          * could never learn where to move it. */
         if (ac->handle && ac->demux)
            return rmp4_consumed(ac->demux);
#endif
#ifdef HAVE_RWEBM
         if (ac->handle && ac->wdemux)
            return rwebm_tell(ac->wdemux);
#endif
         return 0;
      }
#endif
      default:
         break;
   }
   return 0;
}

#ifdef HAVE_RVORBIS
/* Sample-accurate seek on a packet-fed stream.
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
/* Put the packet walk back at the first packet, whichever source it
 * comes from. */
static void audio_transfer_vorbis_to_head(struct audio_transfer_vorbis *v)
{
#ifdef HAVE_RWEBM
   if (v->demux)
      rwebm_rewind(v->demux);
#endif
   if (v->ogg_pkt)
   {
      /* Back to the first page, and past the three headers, which are
       * not audio and are not decoded again. */
      const uint8_t *pd = NULL;
      uint32_t       pl = 0;
      int            i;
      v->pg_off   = 0;
      v->seg_idx  = 0;
      v->body_off = 0;
      for (i = 0; i < 3; i++)
         audio_transfer_vorbis_ogg_pkt(v, &pd, &pl);
   }
   v->pkt_index  = 0;
   v->pkt_offset = 0;
}

static bool audio_transfer_vorbis_seek_walk(struct audio_transfer_vorbis *v,
      int64_t target)
{
   const uint8_t *pd  = NULL;
   uint32_t       pl  = 0;
   int64_t        pad = 0;
   int64_t        pos = 0;
   int            n, m = 1, i, r;
   int            reached = 0;
   /* Sink for the frames walked past.  On the stack: this was a
    * function-local static shared by every context, and a seek on one
    * mixer voice wrote into it while another was seeking too.  Small
    * because the loop below iterates. */
   int16_t        skip[256];

   /* Pass 1: where does each packet leave the stream? */
   audio_transfer_vorbis_to_head(v);
   n = 0;
   for (;;)
   {
      int fr;
      r = audio_transfer_vorbis_pull(v, &pd, &pl, &pad);
      if (r == -2)
      {
         /* The walk met the resident wall before the target.  Not a
          * malformed stream - the bytes have not arrived yet - so the
          * seek fails and can be tried again once they have. */
         audio_transfer_vorbis_to_head(v);
         rvorbis_packet_reset(v->handle);
         v->emitted = 0;
         return false;
      }
      if (r != 1)
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
      {
         reached = 1;
         break;
      }
      pos += fr;
      m    = n;
   }
   /* Running out of packets short of the target is a seek past the
    * end, not a position. */
   if (!reached && pos < target)
   {
      audio_transfer_vorbis_to_head(v);
      rvorbis_packet_reset(v->handle);
      v->emitted = 0;
      return false;
   }

   /* Pass 2: walk to m without decoding, then prime with it. */
   rvorbis_packet_reset(v->handle);
   audio_transfer_vorbis_to_head(v);
   for (i = 0; i < m; i++)
      if (audio_transfer_vorbis_pull(v, &pd, &pl, &pad) != 1)
         return false;
   if (rvorbis_packet_decode(v->handle, pd, pl, 1) < 0)
      return false;
   v->emitted = pos;

   /* Drop the remainder through the ordinary drain, so the frames
    * counted here are the frames a playthrough emits. */
   while (v->emitted < target)
   {
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
         /* Seeking restarts the stream and decodes forward.  The
          * decoder cannot fetch anything itself, and only this side
          * knows how to rewind a container, so the walk belongs here.
          * A resident buffer makes it one pass over already-present
          * bytes; a container arm pays a re-walk of its own index,
          * which is what it paid before. */
         return audio_transfer_flac_seek_to(fl,
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
            /* Count the packets to the target.  A windowed context
             * can be walked too: what set_avail bounds is a prefix
             * that only grows, not a window that slides, so every
             * byte below it stays readable and a target within it is
             * as reachable as on a resident file.  Only a target past
             * the wall is not, and the walk says so by meeting it. */
            if (frame != 0 && v->channels > 0)
               return audio_transfer_vorbis_seek_walk(v, (int64_t)frame);
            if (frame != 0)
               return false;
            /* Every source the packet path can have, and that is more
             * than the blob cursor: an Ogg page feeder has a page,
             * segment and body offset of its own, and leaving those
             * where the last read left them means the rewound stream
             * hands out nothing. */
            audio_transfer_vorbis_to_head(v);
            rvorbis_packet_reset(v->handle);
            v->emitted    = 0;
            return true;
         }
         /* The emission bound is counted against what has been handed
          * out, so a seek that moves the stream has to move that count
          * with it.  Left alone, a loop back to the start reaches the
          * bound immediately and the stream reads as ended for good. */
         return audio_transfer_vorbis_stream_seek(v, frame);
      }
#endif
#ifdef HAVE_RMP3
      case AUDIO_TYPE_MP3:
      {
         struct audio_transfer_mp3 *m = (struct audio_transfer_mp3*)data;
         if (!m || !m->stream)
            return false;
         /* Where the length is known, refuse to be sent past it rather
          * than walk to the end and report failure from there. */
         if (m->limit >= 0 && (int64_t)frame > m->limit)
            return false;
         if (frame == 0 && !m->start_trim)
         {
            /* Loop-to-start with nothing to prime past: a rewind, done
             * here rather than deferred. */
            rmp3_stream_reset(m->stream);
            m->off       = 0;
            m->eof_sent  = 0;
            m->seek_to   = -1;
            m->trim_left = 0;
            m->emitted   = 0;
            return true;
         }
         /* Recorded, not done: see seek_to.  Frame numbers here are of
          * the played audio, so the priming sits before frame 0 and the
          * walk at the read covers both. */
         m->seek_to = (int64_t)frame;
         return true;
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
            rflac_free(fl->handle);
#ifdef HAVE_RWEBM
         if (fl->demux)
            rwebm_close(fl->demux);
#endif
         break;
      }
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_TYPE_VORBIS:
      {
         struct audio_transfer_vorbis *v = (struct audio_transfer_vorbis*)data;
         /* handle is borrowed from the stream where there is one, so it
          * must not also be closed. */
         if (v->stream)
            rvorbis_stream_free(v->stream);
         else if (v->handle)
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
         if (m->stream)
            rmp3_stream_free(m->stream);
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
          * place; the f32 staging buffer is the one thing this arm
          * ever owns */
         {
            struct audio_transfer_wav *w = (struct audio_transfer_wav*)data;
            if (w)
               free(w->scratch);
         }
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
         if (op)
         {
            free(op->pend);
            free(op->asm_buf);
         }
         break;
      }
#endif
#ifdef HAVE_RAAC
      case AUDIO_TYPE_AAC:
      {
         struct audio_transfer_aac *ac = (struct audio_transfer_aac*)data;
         if (ac)
            free(ac->pend_f32);
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
