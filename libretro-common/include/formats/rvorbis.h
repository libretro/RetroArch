#ifndef RVORBIS_INCLUDE_RVORBIS_H
#define RVORBIS_INCLUDE_RVORBIS_H

#include <stdint.h> /* fixed-width types used throughout (self-contained header) */
#include <stddef.h> /* size_t */

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char *alloc_buffer;
   int   alloc_buffer_length_in_bytes;
} rvorbis_alloc;

/*   FUNCTIONS USEABLE WITH ALL INPUT MODES */

typedef struct rvorbis rvorbis;

typedef struct
{
   unsigned int sample_rate;
   int channels;

   unsigned int setup_memory_required;
   unsigned int setup_temp_memory_required;
   unsigned int temp_memory_required;

   int max_frame_size;
} rvorbis_info;

/* get general information about the file.
 *
 * channels is what the stream carries, one through sixteen; the
 * decoder does not fold anything down, and multichannel output comes
 * out in Vorbis's own channel order (for six: L, C, R, SL, SR, LFE),
 * which is not the order most players write to a file. */
extern rvorbis_info rvorbis_get_info(rvorbis *f);

/* get the last error detected (clears it, too) */
extern int rvorbis_get_error(rvorbis *f);

/* close an ogg vorbis file and free all memory in use */
extern void rvorbis_close(rvorbis *f);

/*   PULLING INPUT API */

/* This API assumes rvorbis is allowed to pull data from a source--
 * either a block of memory containing the _entire_ vorbis stream, or a
 * FILE * that you or it create, or possibly some other reading mechanism
 * if you go modify the source to replace the FILE * case with some kind
 * of callback to your code. (But if you don't support seeking, you may
 * just want to go ahead and use pushdata.) */

extern rvorbis * rvorbis_open_memory(const unsigned char *data, int len,
		int *error, rvorbis_alloc *alloc_buffer);
/* create an ogg vorbis decoder from an ogg vorbis stream in memory (note
 * this must be the entire stream!). on failure, returns NULL and sets *error */

extern int rvorbis_seek(rvorbis *f, unsigned int sample_number);
/* Total number of samples per channel in the stream (0 if unknown). */
extern unsigned int rvorbis_stream_length_in_samples(rvorbis *f);
/* these functions seek in the Vorbis file to (approximately) 'sample_number'.
 * after calling seek_frame(), the next call to get_frame_*() will include
 * the specified sample. after calling rvorbis_seek(), the next call to
 * rvorbis_get_samples_* will start with the specified sample. If you
 * do not need to seek to EXACTLY the target sample when using get_samples_*,
 * you can also use seek_frame(). */

extern void rvorbis_seek_start(rvorbis *f);

/* Current byte offset of the decoder's read cursor within the input
 * buffer.  Monotonic through playback; seek_start returns it to the
 * first audio page. */
extern unsigned int rvorbis_buffer_tell(rvorbis *f);
/* this function is equivalent to rvorbis_seek(f,0), but it
 * actually works */

extern int rvorbis_get_samples_float_interleaved(rvorbis *f, int channels, float *buffer, int num_floats);

extern int rvorbis_get_samples_s16_interleaved(rvorbis *f, int channels, int16_t *buffer, int num_shorts);
/* gets num_shorts samples as native signed 16-bit, interleaved, via
 * the fixed-point pipeline: Q28 inverse MDCT and windowing, quantised
 * to s16 (round half away from zero, clamped) at the interleave copy. */

/*   RAW PACKET API */

/* For containers that delimit Vorbis packets themselves (Matroska/WebM
 * and the rest), which carry the setup headers out of band and hand the
 * audio over one bare packet at a time.  No Ogg is involved and none is
 * synthesised: the packets are decoded where the caller's container
 * holds them.
 *
 * A context opened this way takes packets and nothing else.  The
 * pulling API above - seek, buffer_tell, stream_length_in_samples,
 * get_samples_* - reads an Ogg stream this context does not have, and
 * must not be used on it.  rvorbis_get_info, rvorbis_get_error and
 * rvorbis_close apply to both. */

extern rvorbis * rvorbis_open_packets(const unsigned char *id_header,
      int id_len, const unsigned char *setup_header, int setup_len,
      int *error, rvorbis_alloc *alloc);
/* create a decoder from the identification header (30 bytes) and the
 * setup header, both as the container stores them - packet type byte
 * and "vorbis" signature included.  The comment header is not needed.
 * On failure, returns NULL and sets *error. */

extern int rvorbis_packet_decode(rvorbis *f, const void *packet,
      size_t len, int s16);
/* decode one audio packet.  s16 selects the pipeline the frames are
 * produced through, matching the read that will drain them (the two
 * may still be mixed freely: a read in the other format converts what
 * is pending).  Returns the number of frames now available, which is 0
 * for the first audio packet - overlap-add has no left half to fold it
 * into yet - and 0 for a packet that holds no audio.  Returns < 0 if
 * the context was not opened with rvorbis_open_packets. */

extern int rvorbis_packet_pending(rvorbis *f);
/* frames decoded and not yet read. */

extern int rvorbis_packet_read_float(rvorbis *f, int channels,
      float *buffer, int num_floats);
extern int rvorbis_packet_read_s16(rvorbis *f, int channels,
      int16_t *buffer, int num_shorts);
/* drain up to num_floats/num_shorts divided by channels frames of what
 * the last packet produced, interleaved to the caller's channel count.
 * Returns the frames copied; short of the ask means the packet is
 * spent, not that the stream is. */

extern int rvorbis_packet_frames(rvorbis *f, const void *packet,
      size_t len);
/* how many frames the packet contributes, read off its header without
 * decoding it - the analogue of the duration an Opus packet carries in
 * its TOC byte.  Lets a caller locate or measure the audio in a
 * container it cannot otherwise place: a Matroska timestamp is
 * millisecond-quantised and cannot say where a frame is, and Matroska
 * has no granule position to ask instead.  0 for a packet carrying no
 * audio, < 0 for a malformed one or a context not opened with
 * rvorbis_open_packets.  Touches no decoder state, so it can be used
 * on a context that is mid-stream.  Note the first packet after an
 * open or a reset yields nothing whatever this reports - overlap-add
 * has no left half to fold it into - so a caller summing a stream
 * counts from the second.  */

extern void rvorbis_packet_reset(rvorbis *f);
/* discard the overlap history and the pending frames, leaving the
 * context as it was before its first packet.  This is what a rewind is
 * on a packet-fed stream: the caller restarts its own packet walk. */

#ifdef __cplusplus
}
#endif

#endif /* RVORBIS_INCLUDE_RVORBIS_H */
