/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_stream.h).
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

#ifndef __LIBRETRO_SDK_FILE_STREAM_H
#define __LIBRETRO_SDK_FILE_STREAM_H

#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct RFILE RFILE;

enum
{
   RFILE_MODE_READ = 0,
   RFILE_MODE_READ_TEXT,
   RFILE_MODE_WRITE,
   RFILE_MODE_READ_WRITE,

   /* There is no garantee these requests will be attended. */
   RFILE_HINT_UNBUFFERED = 1<<8,
   RFILE_HINT_MMAP       = 1<<9  /* requires RFILE_MODE_READ */
};

long long int filestream_get_size(RFILE *stream);

void filestream_set_size(RFILE *stream);

const char *filestream_get_ext(RFILE *stream);

RFILE *filestream_open(const char *path, unsigned mode, ssize_t len);

ssize_t filestream_seek(RFILE *stream, ssize_t offset, int whence);

ssize_t filestream_read(RFILE *stream, void *data, size_t len);

ssize_t filestream_write(RFILE *stream, const void *data, size_t len);

ssize_t filestream_tell(RFILE *stream);

void filestream_rewind(RFILE *stream);

int filestream_close(RFILE *stream);

int filestream_read_file(const char *path, void **buf, ssize_t *len);

char *filestream_gets(RFILE *stream, char *s, size_t len);

char *filestream_getline(RFILE *stream);

int filestream_getc(RFILE *stream);

int filestream_eof(RFILE *stream);

bool filestream_write_file(const char *path, const void *data, ssize_t size);

int filestream_putc(RFILE *stream, int c);

int filestream_get_fd(RFILE *stream);

int filestream_flush(RFILE *stream);

RETRO_END_DECLS

#endif
