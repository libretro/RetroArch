/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_AUDIO_H__
#define __LIBRETRO_SDK_FORMAT_AUDIO_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Codec-agnostic audio decode interface.
 *
 * This is the audio-side counterpart of formats/image.h: a thin dispatch
 * layer that sits above the concrete decoders (rflac/rvorbis/rmp3/rwav) and
 * lets callers decode any supported format through one API, exactly the way
 * image_transfer_* sits above rpng/rjpeg/etc.
 *
 * The lifecycle mirrors the image layer 1:1 - new -> start -> read (repeat)
 * -> free, no direct file I/O (the caller owns the encoded bytes and passes
 * them via audio_transfer_set_buffer_ptr). Two things differ from images,
 * because mixer audio is an unbounded pull stream rather than a bounded
 * decode-to-completion:
 *   - audio_transfer_seek(), for looping (images never seek);
 *   - audio_transfer_info(), because format (channels/rate) must be known up
 *     front to size buffers and configure resampling, whereas an image
 *     reports its dimensions out of the decode step itself.
 * The read step comes in s16 and f32 variants so an integer output path and
 * a float output path can each stay in their own format end to end. */

enum audio_process_code
{
   AUDIO_PROCESS_ERROR     = -2,
   AUDIO_PROCESS_ERROR_END = -1,
   AUDIO_PROCESS_NEXT      =  0,   /* more PCM remains in the stream */
   AUDIO_PROCESS_END       =  1    /* end of stream reached          */
};

enum audio_type_enum
{
   AUDIO_TYPE_NONE = 0,
   AUDIO_TYPE_WAV,
   AUDIO_TYPE_FLAC,
   AUDIO_TYPE_VORBIS, /* Ogg Vorbis (rvorbis); also WebM buffers (.weba) */
   AUDIO_TYPE_MP3,
   AUDIO_TYPE_MOD,  /* tracker module: MOD / S3M / XM (rmodtracker) */
   AUDIO_TYPE_OPUS, /* Opus (ropus); demuxed, Ogg (.opus) or WebM (.weba) */
   AUDIO_TYPE_AAC   /* AAC-LC (raac); demuxed path, or a whole MP4/M4A
                    * buffer when rmp4 is built in (no ADTS parser)     */
};

/* Guess the codec from a file-name/extension (counterpart of
 * image_texture_get_type). Returns AUDIO_TYPE_NONE if unrecognised. */
enum audio_type_enum audio_decode_get_type(const char *path);

/* ---- shared lifecycle: 1:1 with image_transfer_* ---- */

/* Allocate an opaque decode context for the given codec. */
void *audio_transfer_new(enum audio_type_enum type);

/* Point the context at the encoded bytes. No file I/O is performed; the
 * caller retains ownership of the buffer and must keep it alive for the
 * lifetime of the decoder. Call before audio_transfer_start. */
void  audio_transfer_set_buffer_ptr(void *data, enum audio_type_enum type,
      void *ptr, size_t len);

/* Point the context at a DEMUXED elementary stream instead of a
 * self-framed file: 'setup' is the codec's out-of-band configuration
 * (a container's CodecPrivate -- e.g. the Vorbis identification/comment/
 * setup headers, xiph-laced, or an Opus OpusHead), and 'packets' is the
 * concatenation of the coded frames in decode order.  Some codecs need
 * the coded frames delimited; 'sizes'/'num_packets', when provided, give
 * the byte length of each packet in order (pass NULL/0 when the codec's
 * frames are self-delimiting).  All buffers are borrowed and must outlive
 * the decoder.  Used by container demuxers (e.g. WebM) that have already
 * separated setup from payload; call instead of set_buffer_ptr, before
 * audio_transfer_start. Returns false if the codec has no demuxed path. */
bool  audio_transfer_set_demuxed_ptr(void *data, enum audio_type_enum type,
      const void *setup, size_t setup_size,
      const void *packets, size_t packets_size,
      const uint32_t *sizes, size_t num_packets);

/* Frames the container trims from the start of the stream (e.g. the
 * AAC encoder delay from an MP4 edit list).  Call between
 * set_demuxed_ptr and audio_transfer_start; only codecs whose trim is
 * not carried in the codec setup itself accept it (currently AAC --
 * Opus pre-skip comes from the OpusHead and needs no call). */
bool  audio_transfer_set_start_trim(void *data, enum audio_type_enum type,
      uint64_t frames);

/* Identify the codec of an Ogg buffer from its first page's
 * identification header: AUDIO_TYPE_OPUS or AUDIO_TYPE_VORBIS, whose
 * buffer modes both accept the whole file; AUDIO_TYPE_NONE if it is
 * not Ogg or carries an unsupported codec.  An .ogg extension can
 * legitimately wrap either. */
enum audio_type_enum audio_transfer_ogg_audio_type(const void *buf,
      size_t len);

/* Identify the first supported audio codec of a WebM buffer (.weba):
 * AUDIO_TYPE_OPUS or AUDIO_TYPE_VORBIS, whose buffer modes both accept
 * the whole WebM file; AUDIO_TYPE_NONE if it is not WebM or carries no
 * supported track. */
enum audio_type_enum audio_transfer_webm_audio_type(const void *buf,
      size_t len);

/* Open the decoder over the buffer set above. Returns false on malformed
 * input. */
bool  audio_transfer_start(void *data, enum audio_type_enum type);

/* Whether the context holds a successfully opened decoder. */
bool  audio_transfer_is_valid(void *data, enum audio_type_enum type);

/* Release the decoder and the context. */
void  audio_transfer_free(void *data, enum audio_type_enum type);

/* ---- audio-specific additions ---- */

/* Stream format, valid after a successful start. Any out pointer may be
 * NULL. total_frames is 0 when the stream length is unknown. */
bool  audio_transfer_info(void *data, enum audio_type_enum type,
      unsigned *channels, unsigned *rate, uint64_t *total_frames);

/* Decode the next chunk of up to 'frames' interleaved PCM frames. This is
 * the repeatable, caller-paced analogue of image_transfer_process: it writes
 * into 'out', stores the number of frames actually produced in *frames_out
 * (may be NULL), and returns AUDIO_PROCESS_NEXT while data remains,
 * AUDIO_PROCESS_END at end of stream (frames produced 0), or
 * AUDIO_PROCESS_ERROR. s16 is exact for integer codecs (FLAC/WAV); f32 is
 * the native path for the float mixer. */
int   audio_transfer_read_s16(void *data, enum audio_type_enum type,
      int16_t *out, size_t frames, size_t *frames_out);
int   audio_transfer_read_f32(void *data, enum audio_type_enum type,
      float *out, size_t frames, size_t *frames_out);

/* Seek to an absolute interleaved PCM frame (used to loop). true on
 * success. */
bool  audio_transfer_seek(void *data, enum audio_type_enum type,
      uint64_t frame);

RETRO_END_DECLS

#endif
