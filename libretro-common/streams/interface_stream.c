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

#include <string.h>
#include <streams/interface_stream.h>
#include <streams/file_stream.h>
#include <streams/memory_stream.h>
#ifdef HAVE_CHD
#include <streams/chd_stream.h>
#endif
#if defined(HAVE_COMPRESSION)
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
   } memory;
#ifdef HAVE_CHD
   struct
   {
      chdstream_t *fp;
      int32_t track;
   } chd;
#endif
#if defined(HAVE_COMPRESSION)
   struct
   {
      rzipstream_t *fp;
   } rzip;
#endif
   struct
   {
      RFILE   *fp;
      uint8_t *buf;        /* window contents                     */
      uint64_t cap;        /* window size                         */
      uint64_t lo;         /* file offset of buf[0]               */
      uint64_t hi;         /* file offset one past buf's last byte */
      uint64_t pos;        /* logical read position                */
      uint64_t size;
   } buffered;
   enum intfstream_type type;
};

/* Refill the window so that [pos, pos+len) is resident. */
static bool intfstream_buffered_fill(intfstream_internal_t *intf,
      uint64_t pos, uint64_t len)
{
   int64_t got;

   if (len > intf->buffered.cap || pos + len > intf->buffered.size)
      return false;
   if (filestream_seek(intf->buffered.fp, (int64_t)pos,
            RETRO_VFS_SEEK_POSITION_START) < 0)
      return false;
   if ((got = filestream_read(intf->buffered.fp, intf->buffered.buf,
               (int64_t)intf->buffered.cap)) <= 0)
      return false;

   intf->buffered.lo = pos;
   intf->buffered.hi = pos + (uint64_t)got;
   return (pos + len <= intf->buffered.hi);
}

intfstream_t *intfstream_open_buffered(const char *path, uint64_t window)
{
   intfstream_info_t      info;
   intfstream_internal_t *fd = NULL;
   RFILE                 *fp = NULL;
   int64_t                sz;

   if (!path || !*path || !window)
      return NULL;

   if (!(fp = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return NULL;

   if ((sz = filestream_get_size(fp)) < 0)
   {
      filestream_close(fp);
      return NULL;
   }

   memset(&info, 0, sizeof(info));
   info.type = INTFSTREAM_BUFFERED;

   if (!(fd = (intfstream_internal_t*)intfstream_init(&info)))
   {
      filestream_close(fp);
      return NULL;
   }

   if (!(fd->buffered.buf = (uint8_t*)malloc((size_t)window)))
   {
      filestream_close(fp);
      free(fd);
      return NULL;
   }

   fd->buffered.fp   = fp;
   fd->buffered.cap  = window;
   fd->buffered.size = (uint64_t)sz;
   return fd;
}

int64_t intfstream_get_size(intfstream_internal_t *intf)
{
   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_get_size(intf->file.fp);
      case INTFSTREAM_MEMORY:
         return memstream_get_size(intf->memory.fp);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
        return chdstream_get_size(intf->chd.fp);
#else
        break;
#endif
      case INTFSTREAM_BUFFERED:
         return (int64_t)intf->buffered.size;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
         return rzipstream_get_size(intf->rzip.fp);
#else
         break;
#endif
   }

   return 0;
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
      case INTFSTREAM_BUFFERED:
         if (!intf->buffered.fp)
            return false;
         break;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return 0;   /* read-only */
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
      case INTFSTREAM_BUFFERED:
         if (intf->buffered.fp)
            filestream_close(intf->buffered.fp);
         if (intf->buffered.buf)
            free(intf->buffered.buf);
         intf->buffered.fp  = NULL;
         intf->buffered.buf = NULL;
         return 0;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      return NULL;

   if (!(intf = (intfstream_internal_t*)malloc(sizeof(*intf))))
      return NULL;

   intf->type            = info->type;
   memset(&intf->buffered, 0, sizeof(intf->buffered));
   intf->file.fp         = NULL;
   intf->memory.fp       = NULL;
#ifdef HAVE_CHD
   intf->chd.track       = 0;
   intf->chd.fp          = NULL;
#endif
#ifdef HAVE_COMPRESSION
   intf->rzip.fp         = NULL;
#endif

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         break;
      case INTFSTREAM_MEMORY:
         intf->memory.fp = memstream_open(info->memory.buf.data, info->memory.buf.size, info->memory.writable);
         break;
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         intf->chd.track = info->chd.track;
         break;
#else
         free(intf);
         return NULL;
#endif
      case INTFSTREAM_BUFFERED:
         break;   /* filled in by intfstream_open_buffered() */
      case INTFSTREAM_RZIP:
         break;
   }

   return intf;
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
      case INTFSTREAM_BUFFERED:
         {
            int64_t origin;
            switch (whence)
            {
               case RETRO_VFS_SEEK_POSITION_START:
                  origin = 0;
                  break;
               case RETRO_VFS_SEEK_POSITION_CURRENT:
                  origin = (int64_t)intf->buffered.pos;
                  break;
               case RETRO_VFS_SEEK_POSITION_END:
                  origin = (int64_t)intf->buffered.size;
                  break;
               default:
                  return -1;
            }
            if (      origin + offset < 0
                  || (uint64_t)(origin + offset) > intf->buffered.size)
               return -1;
            /* Purely logical: a seek back into the window costs
             * nothing, and one outside it is paid for by the refill
             * on the next read rather than here. */
            intf->buffered.pos = (uint64_t)(origin + offset);
            return (int64_t)intf->buffered.pos;
         }
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
      case INTFSTREAM_BUFFERED:
         return -1;  /* read-only */
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
      case INTFSTREAM_BUFFERED:
         {
            uint64_t p = intf->buffered.pos;
            uint64_t n = len;
            if (p >= intf->buffered.size)
               return 0;
            if (p + n > intf->buffered.size)
               n = intf->buffered.size - p;
            if (n == 0)
               return 0;
            /* A read wider than the window cannot be served from it.
             * Go straight to the file rather than failing: field
             * payloads are read in one call, so a single large field
             * would otherwise be unreadable and the record silently
             * lost. */
            if (n > intf->buffered.cap)
            {
               int64_t got;
               if (filestream_seek(intf->buffered.fp, (int64_t)p,
                        RETRO_VFS_SEEK_POSITION_START) < 0)
                  return -1;
               if ((got = filestream_read(intf->buffered.fp, s,
                           (int64_t)n)) <= 0)
                  return -1;
               /* The window no longer describes the file position. */
               intf->buffered.lo  = 0;
               intf->buffered.hi  = 0;
               intf->buffered.pos = p + (uint64_t)got;
               return got;
            }
            /* Hit test inline: the callers this exists for issue on
             * the order of a million reads, so an out-of-line call on
             * the common path is itself measurable. */
            if (   !(p >= intf->buffered.lo && p + n <= intf->buffered.hi)
                && !intfstream_buffered_fill(intf, p, n))
               return -1;
            memcpy(s, intf->buffered.buf + (p - intf->buffered.lo),
                  (size_t)n);
            intf->buffered.pos = p + n;
            return (int64_t)n;
         }
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return -1;  /* read-only */
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
   int ret;
   va_list vl;

   if (!intf)
      return 0;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         va_start(vl, format);
         ret = filestream_vprintf(intf->file.fp, format, vl);
         va_end(vl);
         return ret;
      case INTFSTREAM_MEMORY:
         return -1;
      case INTFSTREAM_CHD:
         return -1;
      case INTFSTREAM_BUFFERED:
         return -1;  /* read-only */
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
         va_start(vl, format);
         ret = rzipstream_vprintf(intf->rzip.fp, format, vl);
         va_end(vl);
         return ret;
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
      case INTFSTREAM_BUFFERED:
         return (int64_t)intf->buffered.pos;
      case INTFSTREAM_RZIP:
         return -1;
   }

   return 0;
}

char *intfstream_gets(intfstream_internal_t *intf,
      char *s, uint64_t len)
{
   if (!intf)
      return NULL;

   switch (intf->type)
   {
      case INTFSTREAM_FILE:
         return filestream_gets(intf->file.fp,
               s, (size_t)len);
      case INTFSTREAM_MEMORY:
         return memstream_gets(intf->memory.fp,
               s, (size_t)len);
      case INTFSTREAM_CHD:
#ifdef HAVE_CHD
         return chdstream_gets(intf->chd.fp, s, len);
#else
         break;
#endif
      case INTFSTREAM_BUFFERED:
         {
            /* Serve the line from the resident window in spans - one
             * memchr for the newline and one memcpy per window visit -
             * rather than pulling it through intfstream_read() a byte
             * at a time.  The underlying reads were already one fill
             * per window; this removes the per-byte dispatch on top. */
            uint64_t i = 0;
            if (len == 0)
               return NULL;
            while (i + 1 < (uint64_t)len)
            {
               uint64_t want;
               const uint8_t *nl;
               uint64_t p = intf->buffered.pos;
               if (p >= intf->buffered.size)
                  break;
               if (   !(p >= intf->buffered.lo && p < intf->buffered.hi)
                   && !intfstream_buffered_fill(intf, p, 1))
                  break;
               {
                  const uint8_t *src = intf->buffered.buf
                        + (p - intf->buffered.lo);
                  uint64_t avail     = intf->buffered.hi - p;
                  want               = (uint64_t)len - 1 - i;
                  if (want > avail)
                     want = avail;
                  nl = (const uint8_t*)memchr(src, '\n', (size_t)want);
                  if (nl)
                     want = (uint64_t)(nl - src) + 1;
                  memcpy(s + i, src, (size_t)want);
               }
               i                  += want;
               intf->buffered.pos  = p + want;
               if (nl)
                  break;
            }
            if (i == 0)
               return NULL;
            s[i] = '\0';
            return s;
         }
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
         return rzipstream_gets(intf->rzip.fp, s, (size_t)len);
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
      case INTFSTREAM_BUFFERED:
         {
            /* Window hit inline, as intfstream_read() does for its
             * bulk path: getc-driven consumers issue enough calls
             * that the extra dispatch through intfstream_read() for
             * one byte is itself measurable. */
            uint64_t p = intf->buffered.pos;
            if (p >= intf->buffered.size)
               return EOF;
            if (   !(p >= intf->buffered.lo && p < intf->buffered.hi)
                && !intfstream_buffered_fill(intf, p, 1))
               return EOF;
            intf->buffered.pos = p + 1;
            return (int)intf->buffered.buf[p - intf->buffered.lo];
         }
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return (int64_t)intf->buffered.pos;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return (intf->buffered.pos >= intf->buffered.size);
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         intf->buffered.pos = 0;
         return;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return;     /* read-only */
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
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
      case INTFSTREAM_BUFFERED:
         return false;
      case INTFSTREAM_RZIP:
#if defined(HAVE_COMPRESSION)
         return rzipstream_is_compressed(intf->rzip.fp);
#else
         break;
#endif
   }

   return false;
}

/**
 * intfstream_crc_step:
 *
 * Hash at most @max_bytes more of @intf into @accumulator, resuming
 * from wherever the previous call stopped.  Returns the number of
 * bytes hashed, 0 at end of stream, or -1 on read error.
 *
 * This exists so callers running inside a task handler can bound how
 * long one tick spends hashing.  intfstream_get_crc() consumes the
 * whole stream in a single call, so the cost of a tick that invokes
 * it is a function of file size and nothing else -- unbounded from
 * the frontend's point of view.  That is fine for a 2 MB core and
 * not fine for a 260 MB one, still less for a multi-gigabyte disc
 * image on the scan path, and least of all on the SD-card and
 * spinning-disk targets where the read rate is a tenth of a desktop's.
 *
 * Timing policy deliberately stays with the caller: it owns the
 * deadline and decides the quantum, exactly as the save-state
 * transfer loops in tasks/task_save.c do.  A stream-layer function
 * has no business deciding what a frame is worth.
 *
 * The caller is responsible for positioning the stream (normally
 * intfstream_rewind()) before the first step and for zeroing
 * @accumulator.
 **/
int64_t intfstream_crc_step(intfstream_internal_t *intf,
      uint32_t *accumulator, size_t max_bytes)
{
   int64_t data_read;
   uint8_t *buffer;
   /* 256 KB reads instead of a 4 KB stack buffer: CRCing a scanned
    * multi-gigabyte disc image at 4 KB a call is a quarter of a
    * million reads per gigabyte, and the call overhead dominates
    * the checksum. */
   size_t buffer_len = 256 * 1024;

   if (!intf || !accumulator)
      return -1;

   if (max_bytes < buffer_len)
      buffer_len = max_bytes;
   if (!buffer_len)
      return 0;

   if (!(buffer = (uint8_t*)malloc(buffer_len)))
      return -1;

   data_read = intfstream_read(intf, buffer, buffer_len);

   if (data_read > 0)
      *accumulator = encoding_crc32(*accumulator, buffer,
            (size_t)data_read);

   free(buffer);

   return data_read;
}

bool intfstream_get_crc(intfstream_internal_t *intf, uint32_t *crc)
{
   int64_t data_read    = 0;
   uint32_t accumulator = 0;

   if (!intf || !crc)
      return false;

   /* Ensure we start at the beginning of the file */
   intfstream_rewind(intf);

   /* Whole-stream convenience wrapper over intfstream_crc_step().
    * Unchanged in behaviour and still the right call for anything
    * not running on a frame deadline. */
   while ((data_read = intfstream_crc_step(intf, &accumulator,
               (size_t)-1)) > 0)
      ;

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

   if (intfstream_open(fd, path, mode, hints))
      return fd;

   intfstream_close(fd);
   free(fd);
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
   info.memory.writable = (mode & RETRO_VFS_FILE_ACCESS_WRITE) != 0;

   if (!(fd = (intfstream_t*)intfstream_init(&info)))
      return NULL;

   if (intfstream_open(fd, NULL, mode, hints))
      return fd;

   intfstream_close(fd);
   free(fd);
   return NULL;
}

intfstream_t *intfstream_open_writable_memory(void *data,
      unsigned mode, unsigned hints, uint64_t size)
{
   return intfstream_open_memory(data, mode | RETRO_VFS_FILE_ACCESS_WRITE, hints, size);
}

intfstream_t *intfstream_open_chd_track(const char *path,
      unsigned mode, unsigned hints, int32_t track)
{
   intfstream_info_t info;
   intfstream_t *fd = NULL;

   info.type        = INTFSTREAM_CHD;
   info.chd.track   = track;

   if (!(fd = (intfstream_t*)intfstream_init(&info)))
      return NULL;

   if (intfstream_open(fd, path, mode, hints))
      return fd;

   intfstream_close(fd);
   free(fd);
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

   if (intfstream_open(fd, path, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE))
      return fd;

   intfstream_close(fd);
   free(fd);
   return NULL;
}
