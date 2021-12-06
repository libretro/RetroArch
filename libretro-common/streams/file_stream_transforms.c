/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <streams/file_stream.h>

RFILE* rfopen(const char *path, const char *mode)
{
   RFILE          *output  = NULL;
   unsigned int retro_mode = RETRO_VFS_FILE_ACCESS_READ;
   bool position_to_end    = false;

   if (strstr(mode, "r"))
   {
      retro_mode = RETRO_VFS_FILE_ACCESS_READ;
      if (strstr(mode, "+"))
      {
         retro_mode = RETRO_VFS_FILE_ACCESS_READ_WRITE |
            RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
      }
   }
   else if (strstr(mode, "w"))
   {
      retro_mode = RETRO_VFS_FILE_ACCESS_WRITE;
      if (strstr(mode, "+"))
         retro_mode = RETRO_VFS_FILE_ACCESS_READ_WRITE;
   }
   else if (strstr(mode, "a"))
   {
      retro_mode = RETRO_VFS_FILE_ACCESS_WRITE |
         RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
      position_to_end = true;
      if (strstr(mode, "+"))
      {
         retro_mode = RETRO_VFS_FILE_ACCESS_READ_WRITE |
            RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
      }
   }

   output = filestream_open(path, retro_mode,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (output && position_to_end)
      filestream_seek(output, 0, RETRO_VFS_SEEK_POSITION_END);

   return output;
}

int rfclose(RFILE* stream)
{
   if (!stream)
      return EOF;

   return filestream_close(stream);
}

int64_t rftell(RFILE* stream)
{
   if (!stream)
      return -1;

   return filestream_tell(stream);
}

int64_t rfseek(RFILE* stream, int64_t offset, int origin)
{
   int seek_position = -1;

   if (!stream)
      return -1;

   switch (origin)
   {
      case SEEK_SET:
         seek_position = RETRO_VFS_SEEK_POSITION_START;
         break;
      case SEEK_CUR:
         seek_position = RETRO_VFS_SEEK_POSITION_CURRENT;
         break;
      case SEEK_END:
         seek_position = RETRO_VFS_SEEK_POSITION_END;
         break;
   }

   return filestream_seek(stream, offset, seek_position);
}

int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream)
{
   if (!stream || (elem_size == 0) || (elem_count == 0))
      return 0;

   return (filestream_read(stream, buffer, elem_size * elem_count) / elem_size);
}

char *rfgets(char *buffer, int maxCount, RFILE* stream)
{
   if (!stream)
      return NULL;

   return filestream_gets(stream, buffer, maxCount);
}

int rfgetc(RFILE* stream)
{
   if (!stream)
      return EOF;

   return filestream_getc(stream);
}

int64_t rfwrite(void const* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream)
{
   if (!stream || (elem_size == 0) || (elem_count == 0))
      return 0;

   return (filestream_write(stream, buffer, elem_size * elem_count) / elem_size);
}

int rfputc(int character, RFILE * stream)
{
   if (!stream)
      return EOF;

   return filestream_putc(stream, character);
}

int64_t rfflush(RFILE * stream)
{
   if (!stream)
      return EOF;

   return filestream_flush(stream);
}

int rfprintf(RFILE * stream, const char * format, ...)
{
   int result;
   va_list vl;

   if (!stream)
      return -1;

   va_start(vl, format);
   result = filestream_vprintf(stream, format, vl);
   va_end(vl);
   return result;
}

int rferror(RFILE* stream)
{
   return filestream_error(stream);
}

int rfeof(RFILE* stream)
{
   return filestream_eof(stream);
}

int rfscanf(RFILE * stream, const char * format, ...)
{
   int result;
   va_list vl;

   if (!stream)
      return 0;

   va_start(vl, format);
   result = filestream_vscanf(stream, format, &vl);
   va_end(vl);
   return result;
}
