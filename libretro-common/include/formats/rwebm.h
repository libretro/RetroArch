/* rwebm -- WebM (Matroska subset) demuxer for libretro-common.
 *
 * Parses the EBML/Matroska container that WebM uses and hands out the
 * elementary-stream packets it carries.  This is a DEMUXER only: it does
 * not decode.  It reports the tracks (codec, type, and the parameters a
 * decoder needs) and streams out the coded packets in file order, each
 * tagged with its track, timestamp and key-frame flag.
 *
 * WebM restricts Matroska to VP8/VP9 video and Vorbis/Opus audio, so the
 * codec set is small and known; the demuxer recognises those four and
 * exposes any others by their raw Matroska CodecID string.
 *
 * The whole input buffer is supplied up front (memory-only, like the rest
 * of libretro-common's format layer) and is borrowed, not copied: it must
 * outlive the demuxer, and returned packet pointers alias into it.
 */
#ifndef __LIBRETRO_SDK_FORMAT_RWEBM_H__
#define __LIBRETRO_SDK_FORMAT_RWEBM_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rwebm_track_type
{
   RWEBM_TRACK_NONE = 0,
   RWEBM_TRACK_VIDEO,
   RWEBM_TRACK_AUDIO,
   RWEBM_TRACK_OTHER
};

enum rwebm_codec
{
   RWEBM_CODEC_UNKNOWN = 0,
   RWEBM_CODEC_VP8,
   RWEBM_CODEC_VP9,
   RWEBM_CODEC_VORBIS,
   RWEBM_CODEC_OPUS,
   RWEBM_CODEC_H264,  /* V_MPEG4/ISO/AVC: CodecPrivate is the avcC box  */
   RWEBM_CODEC_AAC    /* A_AAC*: CodecPrivate is the ASC                */
};

typedef struct
{
   enum rwebm_track_type type;
   enum rwebm_codec       codec;
   int                    number;      /* Matroska TrackNumber (1-based)   */
   /* Video */
   unsigned               width, height;
   /* Audio */
   unsigned               sample_rate; /* rounded from the float element   */
   unsigned               channels;
   /* Decoder setup data (Vorbis: the xiph-laced 3 headers; Opus: the
    * OpusHead; VP8/VP9: usually empty).  Aliases into the input buffer. */
   const uint8_t         *codec_private;
   size_t                 codec_private_size;
   /* CodecDelay: nanoseconds of decoded output to drop from the stream
    * start (the encoder delay; AAC priming), 0 when absent. */
   uint64_t               codec_delay_ns;
   /* TrackEntry DefaultDuration: the muxer's declared nominal frame
    * duration in nanoseconds, 0 when absent.  For video it is the
    * exact intended rate, available from the header alone - no
    * timestamp walk. */
   uint64_t               default_duration_ns;
   /* Colour element (video): ISO/IEC 23001-8 code points, 0 = absent.
    * matrix: 1=BT.709 5/6=BT.601 9=BT.2020-ncl; transfer: 1/6=BT.709/601,
    * 16=PQ (HDR10), 18=HLG; range: 1=limited(TV) 2=full. */
   unsigned               matrix_coefficients;
   unsigned               transfer_characteristics;
   unsigned               primaries;
   unsigned               colour_range;
   /* HDR static metadata (0 = absent) */
   unsigned               max_cll;     /* MaxCLL, nits                     */
   unsigned               max_fall;    /* MaxFALL, nits                    */
   /* Raw Matroska CodecID string, NUL-terminated (e.g. "V_VP8"). */
   char                   codec_id[24];
} rwebm_track;

typedef struct
{
   const uint8_t *data;     /* aliases into the input buffer            */
   size_t         size;
   int            track;     /* index into the track array (0-based)     */
   int64_t        timestamp; /* in nanoseconds (scaled by TimestampScale) */
   int            keyframe;
   /* Matroska DiscardPadding in nanoseconds: when positive, this many
    * nanoseconds of decoded output at the END of this block are not
    * meant to be played (Opus end trimming).  0 when absent. */
   int64_t        discard_padding;
} rwebm_packet;

typedef struct rwebm rwebm_t;

/* Parse the container header (EBML head, Segment info, Tracks). The buffer
 * is borrowed and must outlive the returned demuxer. Returns NULL if the
 * data is not a recognisable WebM/Matroska stream. */
rwebm_t *rwebm_open_memory(const uint8_t *data, size_t size);

void rwebm_close(rwebm_t *webm);

/* Track enumeration. */
int                rwebm_num_tracks(const rwebm_t *webm);

/* Bounded-memory streaming support: the first media byte (everything
 * below it is header material - Tracks, codec private data - that the
 * demuxer keeps borrowed pointers into and which must stay resident),
 * and the packet walk's current byte position. */
size_t             rwebm_media_floor(const rwebm_t *m);
size_t             rwebm_tell(const rwebm_t *m);
const rwebm_track *rwebm_get_track(const rwebm_t *webm, int index);

/* Total duration in nanoseconds (Duration * TimestampScale), or 0 if the
 * file does not declare one. */
int64_t rwebm_duration_ns(const rwebm_t *webm);

/* Read the next elementary-stream packet in file order into *pkt. Returns
 * 1 on success, 0 at end of stream, -1 on a parse error. The packet's data
 * pointer aliases the input buffer and is valid until the next call. */
/* Returned by rwebm_read_packet when the next packet lies beyond the
 * bytes made available so far (see rwebm_set_avail): nothing was
 * consumed; call again after more bytes arrive. */
#define RWEBM_READ_AGAIN 2

int rwebm_read_packet(rwebm_t *webm, rwebm_packet *pkt);

/* Open against a partially-read buffer: only the first 'avail' bytes
 * are valid (the rest may be uninitialised); the parse never touches
 * them.  On failure, *need_more (when non-NULL) is set when the
 * failure was running out of available bytes rather than malformed
 * data - retry with a larger avail.  rwebm_open_memory is this with
 * avail == size. */
rwebm_t *rwebm_open_memory_avail(const uint8_t *data, size_t size,
      size_t avail, int *need_more);

/* Raise the number of valid bytes (monotonic; clamped to the file
 * size).  Until it reaches the segment end, rwebm_read_packet returns
 * RWEBM_READ_AGAIN instead of end-of-stream at the wall. */
void rwebm_set_avail(rwebm_t *webm, size_t avail);

/* Restart packet reading from the first cluster. */
void rwebm_rewind(rwebm_t *webm);

RETRO_END_DECLS

#endif
