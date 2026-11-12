/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmp4_mux.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RMP4_MUX_H__
#define __LIBRETRO_SDK_FORMAT_RMP4_MUX_H__

/* MP4 (ISO base media file format) writer: the counterpart of rmp4's
 * demuxer, for recording.
 *
 * A file is ftyp, one mdat and a moov at the end. Samples go through
 * the caller's write callback as they are given - nothing is copied or
 * buffered - and only where each one landed is kept, in tables sized
 * when the file is opened: nothing is allocated between rmp4_mux_open()
 * and rmp4_mux_close(), and a track that reaches its max_samples
 * refuses more. Closing writes the moov and seeks back once to give the
 * mdat its size, so the output must be seekable.
 *
 * Video: MPEG-4 Part 2 (mp4v; its VOS/VO/VOL headers as the decoder
 * configuration), H.264 (avc1; an avcC record), motion JPEG (mp4v,
 * object type 0x6C). Audio: AAC (mp4a; the AudioSpecificConfig), and
 * PCM as QuickTime's sample entries: 16-bit little or big-endian
 * (sowt, twos), 32-bit big-endian float (fl32) and u-law (ulaw). B
 * frames are kept with composition offsets and an edit list, so the
 * first frame shows at time zero. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

enum rmp4_mux_codec
{
   RMP4_MUX_MPEG4_VIDEO = 0,
   RMP4_MUX_H264,
   RMP4_MUX_MJPEG,
   RMP4_MUX_AAC,
   RMP4_MUX_PCM_S16LE,
   RMP4_MUX_PCM_S16BE,
   RMP4_MUX_PCM_F32BE,
   RMP4_MUX_ULAW
};

typedef struct
{
   enum rmp4_mux_codec codec;
   /* Ticks a second of the timestamps given; for PCM, the sample rate */
   uint32_t       timescale;
   unsigned       width, height;          /* video */
   unsigned       sample_rate, channels;  /* audio */
   const uint8_t *config;                 /* decoder configuration, copied */
   size_t         config_len;
   /* The most rmp4_mux_write() calls the track will take */
   uint32_t       max_samples;
   /* Bits a second, for the decoder configuration; 0 if not known */
   uint32_t       bitrate;
   /* Ticks of the track's start that are not shown: an audio encoder's
    * priming (1024 for AAC). The edit list skips them, as it skips the
    * delay B-frames give the first picture. */
   uint32_t       skip;
} rmp4_mux_track_t;

struct rmp4_mux_io
{
   bool (*write)(void *user, const void *data, size_t len);
   bool (*seek)(void *user, uint64_t offset);   /* from the start */
   void  *user;
};

typedef struct rmp4_mux rmp4_mux_t;

/* Writes the file's start; NULL if a track is not one rmp4_mux knows,
 * or the write fails. The io is copied. */
rmp4_mux_t *rmp4_mux_open(const struct rmp4_mux_io *io,
      const rmp4_mux_track_t *tracks, unsigned count);

/**
 * rmp4_mux_write:
 * @m     : the writer
 * @track : the track's index in what rmp4_mux_open() was given
 * @data  : one sample (an access unit; for PCM, any whole number of
 *          frames, interleaved)
 * @len   : its size
 * @pts   : presentation time, in the track's timescale (for PCM, the
 *          first frame's)
 * @dts   : decode time; samples come in decode order, dts rising
 * @sync  : a sample decoding starts at (an I picture); always for audio
 *
 * Returns: false if the write fails, the track is full, or the times go
 * backwards.
 */
bool rmp4_mux_write(rmp4_mux_t *m, unsigned track, const void *data,
      size_t len, int64_t pts, int64_t dts, bool sync);

/* Writes the moov, gives the mdat its size and frees @m. Returns false
 * if a write or the seek failed. */
bool rmp4_mux_close(rmp4_mux_t *m);

RETRO_END_DECLS

#endif
