/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtags.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_RTAGS_H
#define __LIBRETRO_SDK_FORMAT_RTAGS_H

/* The tags a music or video file carries - its title, artist, album,
 * genre, track and disc numbers and date - read from the file where it
 * lies, for a library or a playlist to show.
 *
 * What it reads:
 *   - ID3v2.2, 2.3 and 2.4 at the start of a file (MP3, ADTS AAC, AC-3,
 *     FLAC with one in front) and OpenMG's "ea3" tag (ATRAC in .oma),
 *     in any of their text encodings,
 *     unsynchronised or not; ID3v1 and 1.1 at the end of a file that is
 *     none of the containers below, for what ID3v2 does not give;
 *   - Vorbis comments: FLAC, and Vorbis, Opus and FLAC in Ogg;
 *   - MP4 and QuickTime: iTunes items (moov/udta/meta/ilst) and the
 *     older moov/udta strings;
 *   - ASF (WMA, WMV): the content description and the extended content
 *     description;
 *   - WAVE: the LIST INFO chunk, and an "id3 " chunk.
 *
 * Each value comes out as FFmpeg's demuxers give it under that key, so
 * that a player moving off FFmpeg shows what it showed before: an ID3
 * genre given as a number "(17)" or "17" - and MP4's gnre - as the
 * genre's name; ID3v2.3's TYER, TDAT and TIME as "YYYY-MM-DD hh:mm";
 * MP4's trkn and disk as "3" or "3/12"; ASF's numbers in decimal; a
 * Vorbis comment given more than once as its values joined by ';'.
 * Beyond FFmpeg: ASF's WM/Year is the date, and ID3v1 fills what an
 * ID3v2 tag leaves out field by field. Text that is not UTF-8 where
 * UTF-8 is expected (ID3v1, WAVE, a stray byte) is read as ISO 8859-1.
 *
 * Nothing is allocated, and the input is only read. */

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rtags_field
{
   RTAGS_TITLE = 0,
   RTAGS_ARTIST,
   RTAGS_ALBUM,
   RTAGS_GENRE,
   RTAGS_TRACK,   /* "3", or "3/12" with the album's count */
   RTAGS_DISC,    /* likewise */
   RTAGS_DATE,    /* as tagged: "2004", "2004-05-01", ... */
   RTAGS_FIELDS
};

/**
 * rtags_get:
 * @data     : the whole file
 * @len      : its size
 * @field    : what to read
 * @out      : where its value goes, UTF-8 and NUL-terminated
 * @out_size : bytes at @out; a longer value is cut at a character
 *
 * Returns the length of the value written (without the NUL), 0 when the
 * file carries none (@out then holds an empty string).
 **/
size_t rtags_get(const uint8_t *data, size_t len, enum rtags_field field,
      char *out, size_t out_size);

/**
 * rtags_genre_name:
 * @index : an ID3v1 genre number
 *
 * Returns the genre's name (Winamp's list, as FFmpeg has it), or NULL
 * past the last.
 **/
const char *rtags_genre_name(unsigned index);

RETRO_END_DECLS

#endif
