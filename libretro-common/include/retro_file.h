/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_file.h).
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

#ifndef __RETRO_FILE_H
#define __RETRO_FILE_H

#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>

#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RFILE RFILE;

enum
{
   RFILE_MODE_READ = 0,
   RFILE_MODE_WRITE,
   RFILE_MODE_READ_WRITE,

   /* There is no garantee these requests will be attended. */
   RFILE_HINT_UNBUFFERED = 1<<8,
   RFILE_HINT_MMAP       = 1<<7
};

RFILE *retro_fopen(const char *path, unsigned mode, ssize_t len);

ssize_t retro_fseek(RFILE *stream, ssize_t offset, int whence);

ssize_t retro_fread(RFILE *stream, void *s, size_t len);

ssize_t retro_fwrite(RFILE *stream, const void *s, size_t len);

ssize_t retro_ftell(RFILE *stream);

void retro_frewind(RFILE *stream);

int retro_fclose(RFILE *stream);

int retro_read_file(const char *path, void **buf, ssize_t *len);

bool retro_write_file(const char *path, const void *data, ssize_t size);

int retro_get_fd(RFILE *stream);

#ifdef __cplusplus
}
#endif

#endif
