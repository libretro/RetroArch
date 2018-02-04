/* Copyright  (C) 2010-2017 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (file_stream_transforms.h).
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

#ifndef __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H

#include <retro_common_api.h>
#include <streams/file_stream.h>
#include <string.h>

RETRO_BEGIN_DECLS

#define FILE RFILE

#undef fopen
#undef fclose
#undef ftell
#undef fseek
#undef fread
#undef fgets
#undef fgetc
#undef fwrite
#undef fputc
#undef fprintf
#undef ferror
#undef feof

#define fopen rfopen
#define fclose rfclose
#define ftell rftell
#define fseek rfseek
#define fread rfread
#define fgets rfgets
#define fgetc rfgetc
#define fwrite rfwrite
#define fputc rfputc
#define fprintf rfprintf
#define ferror rferror
#define feof rfeof

RFILE* rfopen(const char *path, const char *mode);

int rfclose(RFILE* stream);

long rftell(RFILE* stream);

int rfseek(RFILE* stream, long offset, int origin);

size_t rfread(void* buffer,
   size_t elementSize, size_t elementCount, RFILE* stream);

char *rfgets(char *buffer, int maxCount, RFILE* stream);

int rfgetc(RFILE* stream);

size_t rfwrite(void const* buffer,
   size_t elementSize, size_t elementCount, RFILE* stream);

int rfputc(int character, RFILE * stream);

int rfprintf(RFILE * stream, const char * format, ...);

int rferror(RFILE* stream);

int rfeof(RFILE* stream);

RETRO_END_DECLS

#endif
