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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <sys/types.h>

#include <libretro_vfs.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include <stdarg.h>

#define FILESTREAM_REQUIRED_VFS_VERSION 1

RETRO_BEGIN_DECLS

typedef struct RFILE RFILE;

#define FILESTREAM_REQUIRED_VFS_VERSION 1

int64_t filestream_get_size(RFILE *stream);

/**
 * filestream_open:
 * @path               : path to file
 * @mode               : file mode to use when opening (read/write)
 * @bufsize            : optional buffer size (-1 or 0 to use default)
 *
 * Opens a file for reading or writing, depending on the requested mode.
 * Returns a pointer to an RFILE if opened successfully, otherwise NULL.
 **/
RFILE *filestream_open(const char *path, unsigned mode, unsigned hints);

ssize_t filestream_seek(RFILE *stream, ssize_t offset, int whence);

ssize_t filestream_read(RFILE *stream, void *data, size_t len);

ssize_t filestream_write(RFILE *stream, const void *data, size_t len);

ssize_t filestream_tell(RFILE *stream);

void filestream_rewind(RFILE *stream);

int filestream_close(RFILE *stream);

int filestream_read_file(const char *path, void **buf, ssize_t *len);

char *filestream_gets(RFILE *stream, char *s, size_t len);

int filestream_getc(RFILE *stream);

int filestream_eof(RFILE *stream);

bool filestream_write_file(const char *path, const void *data, ssize_t size);

int filestream_putc(RFILE *stream, int c);

int filestream_vprintf(RFILE *stream, const char* format, va_list args);

int filestream_printf(RFILE *stream, const char* format, ...);

int filestream_error(RFILE *stream);

int filestream_flush(RFILE *stream);

int filestream_delete(const char *path);

int filestream_rename(const char *old_path, const char *new_path);

const char *filestream_get_path(RFILE *stream);

bool filestream_exists(const char *path);

static INLINE char *filestream_getline(RFILE *stream)
{
   char* newline     = (char*)malloc(9);
   char* newline_tmp = NULL;
   size_t cur_size   = 8;
   size_t idx        = 0;
   int in            = filestream_getc(stream);

   if (!newline)
      return NULL;

   while (in != EOF && in != '\n')
   {
      if (idx == cur_size)
      {
         cur_size *= 2;
         newline_tmp = (char*)realloc(newline, cur_size + 1);

         if (!newline_tmp)
         {
            free(newline);
            return NULL;
         }

         newline = newline_tmp;
      }

      newline[idx++] = in;
      in             = filestream_getc(stream);
   }

   newline[idx] = '\0';
   return newline;
}

RETRO_END_DECLS

#endif
