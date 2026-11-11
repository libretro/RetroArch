/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rcontainer.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RCONTAINER_H
#define __LIBRETRO_SDK_FORMAT_RCONTAINER_H

/* What an AVI or ASF (WMV, WMA) file holds, from its headers: its
 * streams in order - video with its compression FourCC and size, audio
 * with its WAVE format tag, rate and channels, each with its bit rate -
 * and its length. For a library or a file browser to list a video by;
 * nothing is decoded or demuxed.
 *
 *   - AVI: the hdrl list (avih, and strh/strf for each stream); the
 *     length the longest stream's strh gives (or avih's frame count and
 *     period), as FFmpeg gives it; a video stream's bit rate from its
 *     bytes in idx1 over that length, an audio stream's from its
 *     WAVEFORMATEX; OpenDML's indexes are not read, which leaves a
 *     video bit rate 0 for a file without idx1;
 *   - ASF: the file properties (play duration less preroll), the stream
 *     properties (the audio's WAVEFORMATEX, the video's size and
 *     BITMAPINFOHEADER) and the stream bit rate properties.
 *
 * The header is read where it lies; nothing is allocated. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

enum rcontainer_kind
{
   RCONTAINER_NONE = 0,
   RCONTAINER_AVI,
   RCONTAINER_ASF
};

enum rcontainer_stream_type
{
   RCONTAINER_STREAM_OTHER = 0,
   RCONTAINER_STREAM_VIDEO,
   RCONTAINER_STREAM_AUDIO
};

#define RCONTAINER_MAX_STREAMS 16

/* a FourCC as it is stored, first character in the low byte */
#define RCONTAINER_FOURCC(a, b, c, d) \
   ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) \
    | ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

typedef struct
{
   enum rcontainer_stream_type type;
   uint32_t fourcc;       /* video: biCompression */
   unsigned format_tag;   /* audio: wFormatTag (0x55 MP3, 0x2000 AC-3, 0x161 WMA2 ...) */
   unsigned width, height;
   unsigned sample_rate, channels;
   uint32_t bitrate;      /* bits a second, 0 unknown */
} rcontainer_stream;

typedef struct
{
   enum rcontainer_kind kind;
   unsigned             num_streams;
   rcontainer_stream    streams[RCONTAINER_MAX_STREAMS];
   int64_t              duration_us;   /* 0 unknown */
} rcontainer_info;

/* false when @data is not an AVI or ASF file, or its headers are cut or
 * broken; @info is cleared either way first */
bool rcontainer_probe(const uint8_t *data, size_t len, rcontainer_info *info);

RETRO_END_DECLS

#endif
