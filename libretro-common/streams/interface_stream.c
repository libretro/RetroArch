/* Copyright  (C) 2010-2016 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (interface_stream.c).
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

#include <stdlib.h>

#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/memory_stream.h>

struct intfstream_internal
{
   enum intfstream_type type;

   struct
   {
      RFILE *fp;
   } file;

   struct
   {
      struct
      {
         uint8_t *data;
         unsigned size;
      } buf;
      memstream_t *fp;
      bool writable;
   } memory;
};

bool intfstream_resize(intfstream_internal_t *intf, intfstream_info_t *info)
{
   if (!intf || !info)
      return false;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.buf.data = info->memory.buf.data;
         intf->memory.buf.size = info->memory.buf.size;

         memstream_set_buffer(intf->memory.buf.data,
               intf->memory.buf.size);
         break;
   }

   return true;
}

bool intfstream_open(intfstream_internal_t *intf, const char *path,
      unsigned mode, ssize_t len)
{
   if (!intf)
      return false;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         intf->file.fp = filestream_open(path, mode, len);
         if (!intf->file.fp)
            return false;
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.fp = memstream_open(intf->memory.writable);
         if (!intf->memory.fp)
            return false;
         break;
   }

   return true;
}

int intfstream_close(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_close(intf->file.fp);
      case INTFSTREAM_MEMORY:
         memstream_close(intf->memory.fp);
         return 0;
   }

   return -1;
}

void *intfstream_init(intfstream_info_t *info)
{
   intfstream_internal_t *intf = NULL;
   if (!info)
      goto error;

   intf = (intfstream_internal_t*)calloc(1, sizeof(*intf));

   if (!intf)
      goto error;

   intf->type = info->type;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.writable = info->memory.writable;
         if (!intfstream_resize(intf, info))
            goto error;
         break;
   }

   return intf;

error:
   if (intf)
      free(intf);
   return NULL;
}

int intfstream_seek(intfstream_internal_t *intf, int offset, int whence)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_seek(intf->file.fp, offset, whence);
      case INTFSTREAM_MEMORY:
         return memstream_seek(intf->memory.fp, offset, whence);
   }

   return -1;
}

ssize_t intfstream_read(intfstream_internal_t *intf, void *s, size_t len)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_read(intf->file.fp, s, len);
      case INTFSTREAM_MEMORY:
         return memstream_read(intf->memory.fp, s, len);
   }

   return 0;
}

ssize_t intfstream_write(intfstream_internal_t *intf,
      const void *s, size_t len)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_write(intf->file.fp, s, len);
      case INTFSTREAM_MEMORY:
         return memstream_write(intf->memory.fp, s, len);
   }

   return 0;
}

char *intfstream_gets(intfstream_internal_t *intf,
      char *buffer, size_t len)
{
   if (!intf)
      return NULL;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         /* unimplemented */
         break;
      case INTFSTREAM_MEMORY:
         return memstream_gets(intf->memory.fp, buffer, len);
   }

   return NULL;
}

int intfstream_getc(intfstream_internal_t *intf)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         /* unimplemented */
         break;
      case INTFSTREAM_MEMORY:
         return memstream_getc(intf->memory.fp);
   }

   return 0;
}

size_t intfstream_tell(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_tell(intf->file.fp);
      case INTFSTREAM_MEMORY:
         return memstream_pos(intf->memory.fp);
   }

   return -1;
}

void intfstream_rewind(intfstream_internal_t *intf)
{
   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         filestream_rewind(intf->file.fp);
         break;
      case INTFSTREAM_MEMORY:
         memstream_rewind(intf->memory.fp);
         break;
   }
}

void intfstream_putc(intfstream_internal_t *intf, int c)
{
   if (!intf)
      return;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         filestream_putc(intf->file.fp, c);
         break;
      case INTFSTREAM_MEMORY:
         memstream_putc(intf->memory.fp, c);
         break;
   }
}
