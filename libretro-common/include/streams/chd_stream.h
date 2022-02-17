/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (chd_stream.h).
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

#ifndef _LIBRETRO_SDK_FILE_CHD_STREAM_H
#define _LIBRETRO_SDK_FILE_CHD_STREAM_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct chdstream chdstream_t;

/* First data track */
#define CHDSTREAM_TRACK_FIRST_DATA (-1)
/* Last track */
#define CHDSTREAM_TRACK_LAST (-2)
/* Primary (largest) data track, used for CRC identification purposes */
#define CHDSTREAM_TRACK_PRIMARY (-3)

chdstream_t *chdstream_open(const char *path, int32_t track);

void chdstream_close(chdstream_t *stream);

ssize_t chdstream_read(chdstream_t *stream, void *data, size_t bytes);

int chdstream_getc(chdstream_t *stream);

char *chdstream_gets(chdstream_t *stream, char *buffer, size_t len);

uint64_t chdstream_tell(chdstream_t *stream);

void chdstream_rewind(chdstream_t *stream);

int64_t chdstream_seek(chdstream_t *stream, int64_t offset, int whence);

ssize_t chdstream_get_size(chdstream_t *stream);

uint32_t chdstream_get_track_start(chdstream_t* stream);

uint32_t chdstream_get_frame_size(chdstream_t* stream);

uint32_t chdstream_get_first_track_sector(chdstream_t* stream);

RETRO_END_DECLS

#endif
