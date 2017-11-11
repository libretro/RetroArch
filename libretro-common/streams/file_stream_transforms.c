/* Copyright  (C) 2010-2017 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (file_stream_transforms.c).
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

#include <string.h>
#include <stdarg.h>

#include <libretro.h>
#include <stdio.h>

#include <streams/file_stream.h>
#include <streams/file_stream_transforms.h>

RFILE* rfopen(const char *path, const char *mode)
{
   unsigned int retro_mode = RETRO_VFS_FILE_ACCESS_READ;

   if (strstr(mode, "r"))
   {
	   retro_mode = RETRO_VFS_FILE_ACCESS_READ;
	   if (strstr(mode, "+"))
	   {
		   retro_mode = RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
	   }
   }

   if (strstr(mode, "w") || strstr(mode, "a"))
   {
	   retro_mode = RETRO_VFS_FILE_ACCESS_WRITE;
	   if (strstr(mode, "+"))
	   {
		   retro_mode = RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
	   }
   }

   return filestream_open(path, retro_mode);
}

int rfclose(RFILE* stream)
{
   return filestream_close(stream);
}

long rftell(RFILE* stream)
{
   return (long)filestream_tell(stream);
}

int rfseek(RFILE* stream, long offset, int origin)
{

	switch (origin)
	{
	case SEEK_CUR:
	{
		long position = (long)filestream_tell(stream);
		offset += position;
		break;
	}
	case SEEK_END:
	{
		long size = (long)filestream_size(stream);
		offset = size - offset;
	}
	}

	return (int)filestream_seek(stream, offset);
}

size_t rfread(void* buffer,
   size_t elementSize, size_t elementCount, RFILE* stream)
{
   return (size_t)filestream_read(stream, buffer, elementSize*elementCount);
}

char *rfgets(char *buffer, int maxCount, RFILE* stream)
{
   return filestream_gets(stream, buffer, maxCount);
}

int rfgetc(RFILE* stream)
{
	return filestream_getc(stream);
}

size_t rfwrite(void const* buffer,
   size_t elementSize, size_t elementCount, RFILE* stream)
{
   return (size_t)filestream_write(stream, buffer, elementSize*elementCount);
}

int rfputc(int character, RFILE * stream)
{
    return filestream_putc(stream, character);
}

int rfprintf(RFILE * stream, const char * format, ...)
{
	uint64_t result;
	va_list vl;
	va_start(vl, format);
	result = filestream_vprintf(stream, format, vl);
	va_end(vl);
	return (int)result;
}

int rferror(RFILE* stream)
{
    return filestream_error(stream);
}

int rfeof(RFILE* stream)
{
    return filestream_eof(stream);
}
