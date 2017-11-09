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

#include <libretro.h>
#include <retro_common_api.h>

#include <boolean.h>

#include <stdarg.h>

#define FILESTREAM_REQUIRED_VFS_VERSION 1

RETRO_BEGIN_DECLS

typedef struct RFILE RFILE;

void filestream_vfs_init(const struct retro_vfs_interface_info* vfs_iface_info);

RFILE *filestream_open(const char *path, uint64_t flags);

int filestream_close(RFILE *stream);

int filestream_error(RFILE *stream);

int64_t filestream_size(RFILE *stream);

int64_t filestream_tell(RFILE *stream);

int64_t filestream_seek(RFILE *stream, int64_t offset);

int64_t filestream_read(RFILE *stream, void *s, uint64_t len);

int64_t filestream_write(RFILE *stream, const void *s, uint64_t len);

int filestream_flush(RFILE *stream);

int filestream_delete(const char *path);

const char *filestream_get_path(RFILE *stream);

const char *filestream_get_ext(RFILE *stream);

int filestream_eof(RFILE *stream);

void filestream_rewind(RFILE *stream);

char *filestream_getline(RFILE *stream);

char *filestream_gets(RFILE *stream, char *s, uint64_t len);

int filestream_getc(RFILE *stream);

int filestream_putc(RFILE *stream, int c);

uint64_t filestream_vprintf(RFILE *stream, const char* format, va_list args);

uint64_t filestream_printf(RFILE *stream, const char* format, ...);

int filestream_read_file(const char *path, void **buf, uint64_t *len);

bool filestream_write_file(const char *path, const void *data, uint64_t size);

RETRO_END_DECLS

#endif
