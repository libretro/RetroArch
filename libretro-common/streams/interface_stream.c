/* Copyright  (C) 2010-2020 The RetroArch team
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
#ifdef HAVE_CHD
#include <streams/chd_stream.h>
#endif
#if defined(HAVE_ZLIB)
#include <streams/rzip_stream.h>
#endif
#include <encodings/crc32.h>

struct intfstream_internal
{
   struct
   {
      RFILE *fp;
   } file;

   struct
   {
      memstream_t *fp;
      struct
      {
         uint8_t *data;
         uint64_t size;
      } buf;
      bool writable;
   } memory;
#ifdef HAVE_CHD
   struct
   {
      chdstream_t *fp;
      int32_t track;
   } chd;
#endif
#if defined(HAVE_ZLIB)
   struct
   {
      rzipstream_t *fp;
   } rzip;
#endif
   enum intfstream_type type;
};

int64_t intfstream_get_size(intfstream_internal_t *intf)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_get_size(intf->file.fp);
      case INTFSTREAM_MEMORY:
         return intf->memory.buf.size;
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
        return chdstream_get_size(intf->chd.fp);
#else
        break;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_get_size(intf->rzip.fp);
#else
         break;
#endif
   }

   return 0;
}

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
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
#endif
         break;
      case INTFSTREAM_RZIP:
         /* Unsupported */
         return false;
   }

   return true;
}

bool intfstream_open(intfstream_internal_t *intf, const char *path,
      unsigned mode, unsigned hints)
{
   if (!intf)
      return false;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         intf->file.fp = filestream_open(path, mode, hints);
         if (!intf->file.fp)
            return false;
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.fp = memstream_open(intf->memory.writable);
         if (!intf->memory.fp)
            return false;
         break;
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         intf->chd.fp = chdstream_open(path, intf->chd.track);
         if (!intf->chd.fp)
            return false;
         break;
#else
         return false;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         intf->rzip.fp = rzipstream_open(path, mode);
         if (!intf->rzip.fp)
            return false;
         break;
#else
         return false;
#endif
   }

   return true;
}

int intfstream_flush(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_flush(intf->file.fp);
      case INTFSTREAM_MEMORY:
      case INTFSTREAM_CHD:
      case INTFSTREAM_RZIP:
         /* Should we stub this for these interfaces? */
         break;
   }

   return 0;
}

int intfstream_close(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         if (intf->file.fp)
            return filestream_close(intf->file.fp);
         return 0;
      case INTFSTREAM_MEMORY:
         if (intf->memory.fp)
            memstream_close(intf->memory.fp);
         return 0;
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         if (intf->chd.fp)
            chdstream_close(intf->chd.fp);
#endif
         return 0;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         if (intf->rzip.fp)
            return rzipstream_close(intf->rzip.fp);
#endif
         return 0;
   }

   return -1;
}

void *intfstream_init(intfstream_info_t *info)
{
   intfstream_internal_t *intf = NULL;
   if (!info)
      goto error;

   intf = (intfstream_internal_t*)malloc(sizeof(*intf));

   if (!intf)
      goto error;

   intf->type            = info->type;
   intf->file.fp         = NULL;
   intf->memory.buf.data = NULL;
   intf->memory.buf.size = 0;
   intf->memory.fp       = NULL;
   intf->memory.writable = false;
#ifdef HAVE_CHD
   intf->chd.track       = 0;
   intf->chd.fp          = NULL;
#endif
#ifdef HAVE_ZLIB
   intf->rzip.fp         = NULL;
#endif

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.writable = info->memory.writable;
         if (!intfstream_resize(intf, info))
            goto error;
         break;
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         intf->chd.track = info->chd.track;
         break;
#else
         goto error;
#endif
      case INTFSTREAM_RZIP:
         break;
   }

   return intf;

error:
   if (intf)
      free(intf);
   return NULL;
}

int64_t intfstream_seek(
      intfstream_internal_t *intf, int64_t offset, int whence)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         {
            int seek_position = 0;
            switch (whence)
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
            return (int64_t)filestream_seek(intf->file.fp, (int64_t)offset,
                  seek_position);
         }
      case INTFSTREAM_MEMORY:
         return (int64_t)memstream_seek(intf->memory.fp, offset, whence);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return (int64_t)chdstream_seek(intf->chd.fp, offset, whence);
#else
         break;
#endif
      case INTFSTREAM_RZIP:
         /* Unsupported */
         break;
   }

   return -1;
}

int64_t intfstream_truncate(intfstream_internal_t *intf, uint64_t len)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_truncate(intf->file.fp, len);
      case INTFSTREAM_MEMORY:
         break;
      case INTFSTREAM_CHD:
         break;
      case INTFSTREAM_RZIP:
         break;
   }

   return 0;
}

int64_t intfstream_read(intfstream_internal_t *intf, void *s, uint64_t len)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_read(intf->file.fp, s, len);
      case INTFSTREAM_MEMORY:
         return memstream_read(intf->memory.fp, s, len);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return chdstream_read(intf->chd.fp, s, len);
#else
         break;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_read(intf->rzip.fp, s, len);
#else
         break;
#endif
   }

   return -1;
}

int64_t intfstream_write(intfstream_internal_t *intf,
      const void *s, uint64_t len)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_write(intf->file.fp, s, len);
      case INTFSTREAM_MEMORY:
         return memstream_write(intf->memory.fp, s, len);
      case INTFSTREAM_CHD:
         return -1;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_write(intf->rzip.fp, s, len);
#else
         return -1;
#endif
   }

   return 0;
}

int intfstream_printf(intfstream_internal_t *intf,
      const char* format, ...)
{
   va_list vl;
   int result;

   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         va_start(vl, format);
         result = filestream_vprintf(intf->file.fp, format, vl);
         va_end(vl);
         return result;
      case INTFSTREAM_MEMORY:
         return -1;
      case INTFSTREAM_CHD:
         return -1;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         va_start(vl, format);
         result = rzipstream_vprintf(intf->rzip.fp, format, vl);
         va_end(vl);
         return result;
#else
         return -1;
#endif
   }

   return 0;
}

int64_t intfstream_get_ptr(intfstream_internal_t* intf)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return -1;
      case INTFSTREAM_MEMORY:
         return memstream_get_ptr(intf->memory.fp);
      case INTFSTREAM_CHD:
         return -1;
      case INTFSTREAM_RZIP:
         return -1;
   }

   return 0;
}

char *intfstream_gets(intfstream_internal_t *intf,
      char *buffer, uint64_t len)
{
   if (!intf)
      return NULL;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_gets(intf->file.fp,
               buffer, (size_t)len);
      case INTFSTREAM_MEMORY:
         return memstream_gets(intf->memory.fp,
               buffer, (size_t)len);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return chdstream_gets(intf->chd.fp, buffer, len);
#else
         break;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_gets(intf->rzip.fp, buffer, (size_t)len);
#else
         break;
#endif
   }

   return NULL;
}

int intfstream_getc(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_getc(intf->file.fp);
      case INTFSTREAM_MEMORY:
         return memstream_getc(intf->memory.fp);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return chdstream_getc(intf->chd.fp);
#else
         break;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_getc(intf->rzip.fp);
#else
         break;
#endif
   }

   return -1;
}

int64_t intfstream_tell(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return (int64_t)filestream_tell(intf->file.fp);
      case INTFSTREAM_MEMORY:
         return (int64_t)memstream_pos(intf->memory.fp);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return (int64_t)chdstream_tell(intf->chd.fp);
#else
         break;
#endif
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return (int64_t)rzipstream_tell(intf->rzip.fp);
#else
         break;
#endif
   }

   return -1;
}

int intfstream_eof(intfstream_internal_t *intf)
{
   if (!intf)
      return -1;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_eof(intf->file.fp);
      case INTFSTREAM_MEMORY:
         /* TODO: Add this functionality to
          * memory_stream interface */
         break;
      case INTFSTREAM_CHD:
         /* TODO: Add this functionality to
          * chd_stream interface */
         break;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_eof(intf->rzip.fp);
#else
         break;
#endif
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
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         chdstream_rewind(intf->chd.fp);
#endif
         break;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         rzipstream_rewind(intf->rzip.fp);
#endif
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
      case INTFSTREAM_CHD:
         break;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         rzipstream_putc(intf->rzip.fp, c);
#else
         break;
#endif
   }
}

uint32_t intfstream_get_offset_to_start(intfstream_internal_t *intf)
{
   if (intf)
   {
#ifdef HAVE_CHD
      if (intf->type == INTFSTREAM_CHD)
         return chdstream_get_track_start(intf->chd.fp);
#endif
   }

   return 0;
}

uint32_t intfstream_get_frame_size(intfstream_internal_t *intf)
{
   if (intf)
   {
#ifdef HAVE_CHD
      if (intf->type == INTFSTREAM_CHD)
         return chdstream_get_frame_size(intf->chd.fp);
#endif
   }

   return 0;
}

uint32_t intfstream_get_first_sector(intfstream_internal_t* intf)
{
   if (intf)
   {
#ifdef HAVE_CHD
      if (intf->type == INTFSTREAM_CHD)
         return chdstream_get_first_track_sector(intf->chd.fp);
#endif
   }

   return 0;
}

bool intfstream_is_compressed(intfstream_internal_t *intf)
{
   if (!intf)
      return false;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return false;
      case INTFSTREAM_MEMORY:
         return false;
      case INTFSTREAM_CHD:
         return true;
      case INTFSTREAM_RZIP:
#if defined(HAVE_ZLIB)
         return rzipstream_is_compressed(intf->rzip.fp);
#else
         break;
#endif
   }

   return false;
}

bool intfstream_get_crc(intfstream_internal_t *intf, uint32_t *crc)
{
   int64_t data_read    = 0;
   uint32_t accumulator = 0;
   uint8_t buffer[4096];

   if (!intf || !crc)
      return false;

   /* Ensure we start at the beginning of the file */
   intfstream_rewind(intf);

   while ((data_read = intfstream_read(intf, buffer, sizeof(buffer))) > 0)
      accumulator = encoding_crc32(accumulator, buffer, (size_t)data_read);

   if (data_read < 0)
      return false;

   *crc = accumulator;

   /* Reset file to the beginning */
   intfstream_rewind(intf);

   return true;
}

intfstream_t* intfstream_open_file(const char *path,
      unsigned mode, unsigned hints)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_FILE;
   fd               = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      return NULL;

   if (!intfstream_open(fd, path, mode, hints))
      goto error;

   return fd;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return NULL;
}

intfstream_t *intfstream_open_memory(void *data,
      unsigned mode, unsigned hints, uint64_t size)
{
   intfstream_info_t info;
   intfstream_t *fd     = NULL;

   info.type            = INTFSTREAM_MEMORY;
   info.memory.buf.data = (uint8_t*)data;
   info.memory.buf.size = size;
   info.memory.writable = false;

   fd                   = (intfstream_t*)intfstream_init(&info);
   if (!fd)
      return NULL;

   if (!intfstream_open(fd, NULL, mode, hints))
      goto error;

   return fd;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return NULL;
}

intfstream_t *intfstream_open_writable_memory(void *data,
      unsigned mode, unsigned hints, uint64_t size)
{
   intfstream_info_t info;
   intfstream_t *fd     = NULL;

   info.type            = INTFSTREAM_MEMORY;
   info.memory.buf.data = (uint8_t*)data;
   info.memory.buf.size = size;
   info.memory.writable = true;

   fd                   = (intfstream_t*)intfstream_init(&info);
   if (!fd)
      return NULL;

   if (!intfstream_open(fd, NULL, mode, hints))
      goto error;

   return fd;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return NULL;
}

intfstream_t *intfstream_open_chd_track(const char *path,
      unsigned mode, unsigned hints, int32_t track)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_CHD;
   info.chd.track   = track;

   fd               = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      return NULL;

   if (!intfstream_open(fd, path, mode, hints))
      goto error;

   return fd;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return NULL;
}

intfstream_t* intfstream_open_rzip_file(const char *path,
      unsigned mode)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_RZIP;
   fd               = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      return NULL;

   if (!intfstream_open(fd, path, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE))
      goto error;

   return fd;

error:
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return NULL;
}
