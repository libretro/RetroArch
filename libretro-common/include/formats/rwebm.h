/* rwebm -- WebM (Matroska subset) demuxer for libretro-common.
 *
 * Parses the EBML/Matroska container that WebM uses and hands out the
 * elementary-stream packets it carries.  This is a DEMUXER only: it does
 * not decode.  It reports the tracks (codec, type, and the parameters a
 * decoder needs) and streams out the coded packets in file order, each
 * tagged with its track, timestamp and key-frame flag.
 *
 * Recognises VP8/VP9/AV1 video, Vorbis/Opus audio and selected Matroska
 * codecs. Others are exposed by their raw Matroska CodecID string.
 * Track recognition does not imply decoder support.
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
#include <formats/video_color.h>

RETRO_BEGIN_DECLS

enum rwebm_language_kind
{
   RWEBM_LANGUAGE_NONE = 0,
   RWEBM_LANGUAGE_LEGACY,
   RWEBM_LANGUAGE_BCP47
};

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
   RWEBM_CODEC_AAC,   /* A_AAC*: CodecPrivate is the ASC                */
   RWEBM_CODEC_FLAC,  /* A_FLAC: CodecPrivate is a whole fLaC header -
                       * magic and metadata blocks - and each block is
                       * one raw FLAC frame                            */
   RWEBM_CODEC_AV1,   /* V_AV1: CodecPrivate is the av1C record         */
   RWEBM_CODEC_H265   /* V_MPEGH/ISO/HEVC: CodecPrivate is hvcC         */
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
    * OpusHead; VP8/VP9: usually empty; AV1: av1C configuration record).
    * Aliases into the input buffer. */
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
   /* Matroska siting: 0 unspecified, 1 colocated, 2 half. */
   unsigned               chroma_siting_horz, chroma_siting_vert;
   /* HDR static metadata (0 = absent) */
   video_mastering_display_t mastering;
   unsigned               max_cll;     /* MaxCLL, nits                     */
   unsigned               max_fall;    /* MaxFALL, nits                    */
   /* Raw Matroska CodecID string, NUL-terminated (e.g. "V_VP8"). */
   char                   codec_id[24];
} rwebm_track;

typedef struct
{
   const uint8_t *data;     /* aliases into the input buffer            */
   size_t         size;
   /* Encoded BlockAdditions payload for an unlaced Block; borrowed. */
   const uint8_t *block_additions;
   size_t block_additions_size;
   int            track;     /* index into the track array (0-based)     */
   int64_t        timestamp; /* in nanoseconds (scaled by TimestampScale) */
   int            keyframe;
   /* Audio laces continue at the preceding decoded endpoint. Video
    * laces use BlockDuration or DefaultDuration for each timestamp. */
   int            timestamp_continuation;
   /* Matroska DiscardPadding in nanoseconds: when positive, this many
    * nanoseconds of decoded output at the END of this block are not
    * meant to be played (Opus end trimming).  0 when absent. */
   int64_t        discard_padding;
   /* Per-frame BlockDuration: -1 absent, -2 invalid/unrepresentable. */
   int64_t        duration_ns;
} rwebm_packet;

/* Returns 1 found, 0 absent, -1 invalid or over 256 metadata elements.
 * The default BlockAddID is 1. Returned data remains borrowed. */
int rwebm_packet_addition(const rwebm_packet *packet, uint64_t wanted,
      const uint8_t **data, size_t *size);

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
/* Earliest byte still needed by the next read, including pending laces.
 * Callers retaining a returned packet must pin its data and additions. */
size_t             rwebm_consumed(const rwebm_t *m);
const rwebm_track *rwebm_get_track(const rwebm_t *webm, int index);
/* Raw string span, including EBML padding; borrowed with the input.
 * BCP47 overrides legacy Language, which defaults to eng when absent. */
const char *rwebm_get_track_language(const rwebm_t *webm, int index,
      size_t *length, enum rwebm_language_kind *kind);


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
/* Track-specific bounded walk (zero-based track index). Unselected
 * SimpleBlock payloads need not be resident. YIELD means metadata progress
 * with no missing bytes; retry without refilling. Other results match above. */
#define RWEBM_READ_YIELD 3
int rwebm_read_packet_track(rwebm_t *webm, int track, rwebm_packet *pkt);

/* Optional exclusive-owner notification for validated Void payloads.
 * All cursors over the borrowed buffer must skip these inert bytes.
 * Return zero to refuse the packet walk. Callback/user are borrowed
 * until close or replacement; cursor clones preserve the notification. */
typedef int (*rwebm_inert_range_cb_t)(void *user, size_t lo, size_t hi);
void rwebm_set_inert_callback(rwebm_t *webm,
      rwebm_inert_range_cb_t callback, void *user);

/* After READ_AGAIN on an element body, name the complete required encoded
 * span, including the cursor needed to retry. Zero/zero for an incomplete
 * header or no blocked element. Metadata only; the owner admits this span
 * with its ordinary I/O budget. Success, rewind and seek clear the request. */
void rwebm_read_span(const rwebm_t *webm, size_t *lo, size_t *hi);

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

/* Cues, the container's seek index: the number of usable points, 0
 * when the file carries none or the parse did not reach them (they
 * are usually written after the clusters, so a partial read whose
 * wall stopped the header walk short has none).  Seeking needs them:
 * without an index a target can only be reached by walking. */
/* Independent packet cursor over the same borrowed encoded buffer and
 * metadata. It owns no cue index. Allocate once, refresh without allocation,
 * and close normally. Copy retains the destination's own cue index when
 * the encoded buffer is unchanged; it never borrows the source index. */
rwebm_t *rwebm_cursor_clone(const rwebm_t *source);
void rwebm_cursor_copy(rwebm_t *destination, const rwebm_t *source);

int rwebm_num_cues(const rwebm_t *webm);
/* Optional bounded SeekHead index. Offset is absolute, zero when absent or
 * already attempted. Supply only resident bytes at that offset. The loader
 * requests at most 4 MiB, copies index metadata, and filters by TrackNumber.
 * Returns 1 loaded, 0 needs more, -1 invalid; invalid Cues remain optional. */
size_t rwebm_cues_offset(const rwebm_t *webm);
int rwebm_load_cues(rwebm_t *webm, const uint8_t *data, size_t available,
      int track, size_t *needed);


/* Reposition the packet walk to the indexed cluster at or before
 * 'ns'.  Returns the timestamp actually landed on, in nanoseconds,
 * which is at or before the one asked for - a caller wanting an exact
 * position decodes forward from here and discards.  Returns < 0 when
 * the file has no Cues, or when the target cluster lies past a
 * partial read's wall.  The next rwebm_read_packet resumes there. */
int64_t rwebm_seek_time_ns(rwebm_t *webm, int64_t ns);

/* Metadata-only cue selection, including a cluster beyond the readable
 * wall. No media bytes are touched. consumed() identifies the frontier
 * the owner must feed before read_packet() can resume. */
int64_t rwebm_select_time_ns(rwebm_t *webm, int64_t ns);

/* Query the selected cue timestamp without changing either cursor or residency. */
int64_t rwebm_cue_time_ns(const rwebm_t *webm, int64_t ns);

/* Restart packet reading from the first cluster. */
void rwebm_rewind(rwebm_t *webm);

RETRO_END_DECLS

#endif
