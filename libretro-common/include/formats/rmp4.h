/* rmp4 -- MP4 (ISO Base Media File Format) demuxer for libretro-common.
 *
 * Parses the ISO-BMFF box structure that MP4 uses and hands out the
 * elementary-stream packets it carries.  This is a DEMUXER only: it does
 * not decode.  It reports the tracks (codec, type, and the parameters a
 * decoder needs) and streams out the coded packets in file order, each
 * tagged with its track, timestamp and key-frame flag -- the same
 * contract as the rwebm demuxer, so the rwebm_video/rwebm_audio-style
 * glue can drive either container identically.
 *
 * The codec set mirrors what libretro-common can decode: VP8 ('vp08'),
 * VP9 ('vp09') and H.264 ('avc1'/'avc3') video; Opus ('Opus'),
 * Vorbis and AAC ('mp4a' by object type) audio.  Other codecs (HEVC,
 * AV1, ...) are reported with their sample-entry fourcc in codec_id
 * and the codec field left RMP4_CODEC_UNKNOWN so callers can skip
 * them.
 *
 * Both progressive files (sample tables in moov/trak/mdia/minf/stbl)
 * and fragmented movies (an mvex-marked moov followed by moof/mdat
 * pairs) are supported.
 *
 * The whole input buffer is supplied up front (memory-only, like the
 * rest of libretro-common's format layer) and is borrowed, not copied:
 * it must outlive the demuxer, and returned packet pointers alias into
 * it.  The one exception is Opus codec_private: MP4 stores the decoder
 * setup as a dOps box, which is repacked into the OpusHead layout the
 * Opus decoders expect; that buffer lives inside the demuxer and is
 * valid until rmp4_close.
 */
#ifndef __LIBRETRO_SDK_FORMAT_RMP4_H__
#define __LIBRETRO_SDK_FORMAT_RMP4_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rmp4_track_type
{
   RMP4_TRACK_NONE = 0,
   RMP4_TRACK_VIDEO,
   RMP4_TRACK_AUDIO,
   RMP4_TRACK_OTHER
};

enum rmp4_codec
{
   RMP4_CODEC_UNKNOWN = 0,
   RMP4_CODEC_VP8,
   RMP4_CODEC_VP9,
   RMP4_CODEC_H264,
   RMP4_CODEC_VORBIS,
   RMP4_CODEC_OPUS,
   RMP4_CODEC_AAC
};

typedef struct
{
   enum rmp4_track_type type;
   enum rmp4_codec       codec;
   int                    number;      /* MP4 track_ID (1-based)          */
   /* Video */
   unsigned               width, height;
   /* From the sample entry's colr box (nclx/nclc); 0 when untagged, in
    * which case converters pick defaults by resolution. */
   unsigned               matrix_coefficients;
   unsigned               transfer_characteristics;
   unsigned               full_range;
   /* Audio */
   unsigned               sample_rate;
   unsigned               channels;
   /* Media-timescale units the track's edit list trims from the
    * start (the usual encoder-delay edit; sample count for audio).
    * Zero when absent. Only the common single-entry shape is read. */
   uint64_t               media_skip;
   /* Decoder setup data (Opus: an OpusHead repacked from the dOps box,
    * owned by the demuxer; Vorbis: the xiph-laced 3 headers from the
    * esds DecoderSpecificInfo, aliasing the input; VP8/VP9: usually
    * empty). */
   const uint8_t         *codec_private;
   size_t                 codec_private_size;
   /* Sample-entry fourcc, NUL-terminated (e.g. "vp09", "avc1"). */
   char                   codec_id[24];
} rmp4_track;

typedef struct
{
   const uint8_t *data;      /* aliases into the input buffer            */
   size_t         size;
   int            track;     /* index into the track array (0-based)     */
   int64_t        timestamp; /* presentation time in nanoseconds         */
   int            keyframe;
   /* Always 0 for MP4 (end trimming is carried by edit lists, which the
    * preview use cases do not need); present so packet handling code
    * can be shared with rwebm. */
   int64_t        discard_padding;
} rmp4_packet;

typedef struct rmp4 rmp4_t;

/* Parse the container (ftyp/moov and the per-track sample tables). The
 * buffer is borrowed and must outlive the returned demuxer. Returns
 * NULL if the data is not a recognisable MP4 stream. */
rmp4_t *rmp4_open_memory(const uint8_t *data, size_t size);

void rmp4_close(rmp4_t *mp4);

/* Track enumeration. */
int               rmp4_num_tracks(const rmp4_t *mp4);
const rmp4_track *rmp4_get_track(const rmp4_t *mp4, int index);

/* Total duration in nanoseconds (from mvhd), or 0 if the file does not
 * declare one. */
int64_t rmp4_duration_ns(const rmp4_t *mp4);

/* Read the next elementary-stream packet in file order into *pkt.
 * Returns 1 on success, 0 at end of stream, -1 on a parse error. The
 * packet's data pointer aliases the input buffer and is valid until the
 * demuxer is closed. */
int rmp4_read_packet(rmp4_t *mp4, rmp4_packet *pkt);

/* Restart packet reading from the first sample. */
void rmp4_rewind(rmp4_t *mp4);

RETRO_END_DECLS

#endif
