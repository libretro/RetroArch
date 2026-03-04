/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rzip_stream.c).
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

#include <string/stdstring.h>
#include <file/file_path.h>

#include <streams/file_stream.h>
#include <streams/trans_stream.h>

#include <streams/rzip_stream.h>

/* Current RZIP file format version */
#define RZIP_VERSION 1

/* Compression level
 * > zlib default of 6 provides the best
 *   balance between file size and compression speed */
#define RZIP_COMPRESSION_LEVEL 6

/* Default chunk size: 128kb */
#define RZIP_DEFAULT_CHUNK_SIZE 131072

/* Header sizes (in bytes) */
#define RZIP_HEADER_SIZE       20
#define RZIP_CHUNK_HEADER_SIZE  4

/* Magic number constants */
#define RZIP_MAGIC_0   35   /* # */
#define RZIP_MAGIC_1   82   /* R */
#define RZIP_MAGIC_2   90   /* Z */
#define RZIP_MAGIC_3   73   /* I */
#define RZIP_MAGIC_4   80   /* P */
#define RZIP_MAGIC_5  118   /* v */
#define RZIP_MAGIC_7   35   /* # */

/* Holds all metadata for an RZIP file stream */
struct rzipstream
{
   uint64_t size;
   /* virtual_ptr: tracks how much uncompressed data has been read */
   uint64_t virtual_ptr;
   RFILE *file;
   const struct trans_stream_backend *deflate_backend;
   void *deflate_stream;
   const struct trans_stream_backend *inflate_backend;
   void *inflate_stream;
   uint8_t *in_buf;
   uint8_t *out_buf;
   uint32_t in_buf_size;
   uint32_t in_buf_ptr;
   uint32_t out_buf_size;
   uint32_t out_buf_ptr;
   uint32_t out_buf_occupancy;
   uint32_t chunk_size;
   bool is_compressed;
   bool is_writing;
};

/* Header Functions */

/* Reads header information from RZIP file.
 * Detects whether file is compressed or uncompressed.
 * If compressed, extracts uncompressed file/chunk sizes. */
static bool rzipstream_read_file_header(rzipstream_t *stream)
{
   int64_t length;
   uint8_t h[RZIP_HEADER_SIZE];

   if (!stream)
      return false;

   length = filestream_read(stream->file, h, sizeof(h));
   if (length <= 0)
      return false;

   /* Validate magic bytes and version */
   if (   (length  < RZIP_HEADER_SIZE)
       || (h[0]   != RZIP_MAGIC_0)
       || (h[1]   != RZIP_MAGIC_1)
       || (h[2]   != RZIP_MAGIC_2)
       || (h[3]   != RZIP_MAGIC_3)
       || (h[4]   != RZIP_MAGIC_4)
       || (h[5]   != RZIP_MAGIC_5)
       || (h[6]   != RZIP_VERSION)
       || (h[7]   != RZIP_MAGIC_7))
   {
      /* Not a compressed RZIP file — treat as raw data */
      filestream_seek(stream->file, 0, SEEK_SET);
      stream->size          = filestream_get_size(stream->file);
      stream->is_compressed = false;
      return true;
   }

   /* Uncompressed chunk size — bytes 8..11 (little-endian) */
   stream->chunk_size = ((uint32_t)h[11] << 24)
                      | ((uint32_t)h[10] << 16)
                      | ((uint32_t)h[ 9] <<  8)
                      |  (uint32_t)h[ 8];
   if (stream->chunk_size == 0)
      return false;

   /* Total uncompressed size — bytes 12..19 (little-endian) */
   stream->size = ((uint64_t)h[19] << 56)
                | ((uint64_t)h[18] << 48)
                | ((uint64_t)h[17] << 40)
                | ((uint64_t)h[16] << 32)
                | ((uint64_t)h[15] << 24)
                | ((uint64_t)h[14] << 16)
                | ((uint64_t)h[13] <<  8)
                |  (uint64_t)h[12];
   if (stream->size == 0)
      return false;

   stream->is_compressed = true;
   return true;
}

/* Writes header information to RZIP file.
 * ID 'magic numbers' + uncompressed file/chunk sizes. */
static bool rzipstream_write_file_header(rzipstream_t *stream)
{
   uint8_t h[RZIP_HEADER_SIZE];

   if (!stream)
      return false;

   /* Magic bytes */
   h[0] = RZIP_MAGIC_0;
   h[1] = RZIP_MAGIC_1;
   h[2] = RZIP_MAGIC_2;
   h[3] = RZIP_MAGIC_3;
   h[4] = RZIP_MAGIC_4;
   h[5] = RZIP_MAGIC_5;
   h[6] = RZIP_VERSION;
   h[7] = RZIP_MAGIC_7;

   /* Chunk size — bytes 8..11 (little-endian) */
   h[ 8] =  stream->chunk_size        & 0xFF;
   h[ 9] = (stream->chunk_size >>  8) & 0xFF;
   h[10] = (stream->chunk_size >> 16) & 0xFF;
   h[11] = (stream->chunk_size >> 24) & 0xFF;

   /* Total uncompressed size — bytes 12..19 (little-endian) */
   h[12] =  stream->size        & 0xFF;
   h[13] = (stream->size >>  8) & 0xFF;
   h[14] = (stream->size >> 16) & 0xFF;
   h[15] = (stream->size >> 24) & 0xFF;
   h[16] = (stream->size >> 32) & 0xFF;
   h[17] = (stream->size >> 40) & 0xFF;
   h[18] = (stream->size >> 48) & 0xFF;
   h[19] = (stream->size >> 56) & 0xFF;

   filestream_seek(stream->file, 0, SEEK_SET);
   return (filestream_write(stream->file, h, sizeof(h)) == RZIP_HEADER_SIZE);
}

/* Stream Initialisation/De-initialisation */

/* Initialises all members of an rzipstream_t struct,
 * reading config from existing file header if available. */
static bool rzipstream_init_stream(
      rzipstream_t *stream, const char *path, bool is_writing)
{
   unsigned file_mode;

   if (!stream)
      return false;

   /* Single memset replaces the original field-by-field zeroing loops */
   memset(stream, 0, sizeof(*stream));

   stream->chunk_size = RZIP_DEFAULT_CHUNK_SIZE;
   stream->is_writing = is_writing;

   if (is_writing)
   {
      stream->is_compressed = true;
      file_mode             = RETRO_VFS_FILE_ACCESS_WRITE;
   }
   else
      file_mode             = RETRO_VFS_FILE_ACCESS_READ;

   if (!(stream->file = filestream_open(
         path, file_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return false;

   if (is_writing)
   {
      if (!rzipstream_write_file_header(stream))
         return false;
   }
   else if (!rzipstream_read_file_header(stream))
      return false;

   /* Set up transform stream and buffer sizes */
   if (is_writing)
   {
      if (!(stream->deflate_backend = trans_stream_get_zlib_deflate_backend()))
         return false;
      if (!(stream->deflate_stream = stream->deflate_backend->stream_new()))
         return false;
      if (!stream->deflate_backend->define(
            stream->deflate_stream, "level", RZIP_COMPRESSION_LEVEL))
         return false;

      /* in = uncompressed, out = compressed */
      stream->in_buf_size  = stream->chunk_size;
      stream->out_buf_size = stream->chunk_size * 2;
      if (stream->out_buf_size < (stream->in_buf_size + 11))
         stream->out_buf_size = stream->in_buf_size + 11;

      if ((stream->in_buf_size == 0) || (stream->out_buf_size == 0))
         return false;
   }
   else if (stream->is_compressed)
   {
      if (!(stream->inflate_backend = trans_stream_get_zlib_inflate_backend()))
         return false;
      if (!(stream->inflate_stream = stream->inflate_backend->stream_new()))
         return false;

      /* in = compressed, out = uncompressed */
      stream->in_buf_size  = stream->chunk_size * 2;
      stream->out_buf_size = stream->chunk_size + (stream->chunk_size >> 2);

      if ((stream->in_buf_size == 0) || (stream->out_buf_size == 0))
         return false;
   }

   /* Allocate buffers with malloc — data is always fully written before
    * being read, so the zero-fill from calloc is unnecessary overhead. */
   if (stream->in_buf_size > 0)
   {
      if (!(stream->in_buf = (uint8_t *)malloc(stream->in_buf_size)))
         return false;
   }

   if (stream->out_buf_size > 0)
   {
      if (!(stream->out_buf = (uint8_t *)malloc(stream->out_buf_size)))
         return false;
   }

   return true;
}

/* Frees all members of an rzipstream_t struct and closes the file. */
static int rzipstream_free_stream(rzipstream_t *stream)
{
   int ret = 0;

   if (!stream)
      return -1;

   if (stream->deflate_stream && stream->deflate_backend)
      stream->deflate_backend->stream_free(stream->deflate_stream);

   if (stream->inflate_stream && stream->inflate_backend)
      stream->inflate_backend->stream_free(stream->inflate_stream);

   /* free(NULL) is a no-op per C89 §4.10.3.2 — no NULL checks needed */
   free(stream->in_buf);
   free(stream->out_buf);

   if (stream->file)
      ret = filestream_close(stream->file);

   free(stream);
   return ret;
}

/* File Open */

/* Opens a new or existing RZIP file.
 * Supported 'mode': RETRO_VFS_FILE_ACCESS_READ or _WRITE.
 * Returns NULL on error. */
rzipstream_t *rzipstream_open(const char *path, unsigned mode)
{
   rzipstream_t *stream = NULL;

   if (   string_is_empty(path)
       || (   (mode != RETRO_VFS_FILE_ACCESS_READ)
           && (mode != RETRO_VFS_FILE_ACCESS_WRITE)))
      return NULL;

   if ((mode == RETRO_VFS_FILE_ACCESS_READ) && !path_is_valid(path))
      return NULL;

   if (!(stream = (rzipstream_t *)malloc(sizeof(*stream))))
      return NULL;

   /* rzipstream_init_stream performs a full memset — no pre-zeroing needed */
   if (!rzipstream_init_stream(
         stream, path, (mode == RETRO_VFS_FILE_ACCESS_WRITE)))
   {
      rzipstream_free_stream(stream);
      return NULL;
   }

   return stream;
}

/* File Read */

/* Reads and decompresses the next chunk of data from the RZIP file. */
static bool rzipstream_read_chunk(rzipstream_t *stream)
{
   uint8_t  ch[RZIP_CHUNK_HEADER_SIZE];
   uint32_t compressed_chunk_size;
   uint32_t inflate_read;
   uint32_t inflate_written;

   if (!stream || !stream->inflate_backend || !stream->inflate_stream)
      return false;

   if (filestream_read(stream->file, ch, sizeof(ch)) != RZIP_CHUNK_HEADER_SIZE)
      return false;

   compressed_chunk_size = ((uint32_t)ch[3] << 24)
                         | ((uint32_t)ch[2] << 16)
                         | ((uint32_t)ch[1] <<  8)
                         |  (uint32_t)ch[0];
   if (compressed_chunk_size == 0)
      return false;

   /* Grow input buffer if needed using realloc — avoids free+malloc round-trip */
   if (compressed_chunk_size > stream->in_buf_size)
   {
      uint8_t *new_buf = (uint8_t *)realloc(stream->in_buf, compressed_chunk_size);
      if (!new_buf)
         return false;
      stream->in_buf      = new_buf;
      stream->in_buf_size = compressed_chunk_size;
   }

   if (filestream_read(
         stream->file, stream->in_buf, compressed_chunk_size) !=
         compressed_chunk_size)
      return false;

   stream->inflate_backend->set_in(
         stream->inflate_stream, stream->in_buf, compressed_chunk_size);
   stream->inflate_backend->set_out(
         stream->inflate_stream, stream->out_buf, stream->out_buf_size);

   if (!stream->inflate_backend->trans(
         stream->inflate_stream, true, &inflate_read, &inflate_written, NULL))
      return false;

   if (inflate_read != compressed_chunk_size)
      return false;
   if ((inflate_written == 0) || (inflate_written > stream->out_buf_size))
      return false;

   stream->out_buf_occupancy = inflate_written;
   stream->out_buf_ptr       = 0;
   return true;
}

/* Reads (a maximum of) 'len' bytes from an RZIP file.
 * Returns actual bytes read, or -1 on error. */
int64_t rzipstream_read(rzipstream_t *stream, void *data, int64_t len)
{
   int64_t  remaining = len;
   uint8_t *dst       = (uint8_t *)data;
   int64_t  data_read = 0;

   if (!stream || stream->is_writing || !data)
      return -1;

   if (!stream->is_compressed)
      return filestream_read(stream->file, data, len);

   while (remaining > 0)
   {
      int64_t avail;

      if (stream->virtual_ptr >= stream->size)
         break;

      if (stream->out_buf_ptr >= stream->out_buf_occupancy)
      {
         if (!rzipstream_read_chunk(stream))
            return -1;
      }

      avail = (int64_t)(stream->out_buf_occupancy - stream->out_buf_ptr);
      if (avail > remaining)
         avail = remaining;

      memcpy(dst, stream->out_buf + stream->out_buf_ptr, (size_t)avail);

      stream->out_buf_ptr  += (uint32_t)avail;
      stream->virtual_ptr  += (uint64_t)avail;
      dst                  += avail;
      remaining            -= avail;
      data_read            += avail;
   }

   return data_read;
}

/* Reads next character from an RZIP file.
 * Returns character value, or EOF. Always EOF if open for writing. */
int rzipstream_getc(rzipstream_t *stream)
{
   unsigned char c;
   if (!stream || stream->is_writing)
      return EOF;
   if (rzipstream_read(stream, &c, 1) == 1)
      return (int)c;
   return EOF;
}

/* Reads one line from an RZIP file into 's' (up to len-1 chars).
 * Stops at newline, EOF, or buffer full.
 * Returns 's' on success, NULL if EOF with no data read or on error. */
char *rzipstream_gets(rzipstream_t *stream, char *s, size_t len)
{
   char *dst = s;
   char *end = s + (len - 1); /* reserve slot for NUL */

   if (!stream || stream->is_writing || (len == 0))
      return NULL;

   while (dst < end)
   {
      uint8_t c;
      int64_t got;

      if (stream->is_compressed)
      {
         /* Fast path: pull directly from the decompressed output buffer
          * without going through a full rzipstream_read call each byte. */
         if (   (stream->out_buf_ptr   < stream->out_buf_occupancy)
             && (stream->virtual_ptr   < stream->size))
         {
            c = stream->out_buf[stream->out_buf_ptr++];
            stream->virtual_ptr++;
            got = 1;
         }
         else
            got = rzipstream_read(stream, &c, 1);
      }
      else
         got = filestream_read(stream->file, &c, 1);

      if (got != 1)
         break;

      *dst++ = (char)c;
      if (c == '\n')
         break;
   }

   *dst = '\0';
   if (dst == s)
      return NULL;

   return s;
}

/* Reads all data from file at 'path' into '*s'.
 * Allocates '*s' — caller must free().
 * Returns false on error. */
bool rzipstream_read_file(const char *path, void **s, int64_t *len)
{
   int64_t       bytes_read       = 0;
   void         *content_buf      = NULL;
   int64_t       content_buf_size = 0;
   rzipstream_t *stream           = NULL;

   if (!s)
      return false;

   if (!(stream = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_READ)))
   {
      *s = NULL;
      return false;
   }

   if ((content_buf_size = rzipstream_get_size(stream)) < 0)
      goto error;

   if ((int64_t)(uint64_t)(content_buf_size + 1) != (content_buf_size + 1))
      goto error;

   if (!(content_buf = malloc((size_t)(content_buf_size + 1))))
      goto error;

   if ((bytes_read = rzipstream_read(stream, content_buf, content_buf_size)) < 0)
      goto error;

   rzipstream_close(stream);
   stream = NULL;

   ((char *)content_buf)[bytes_read] = '\0';

   *s = content_buf;
   if (len)
      *len = bytes_read;
   return true;

error:
   if (stream)
      rzipstream_close(stream);
   free(content_buf);
   if (len)
      *len = -1;
   *s = NULL;
   return false;
}

/* File Write */

/* Compresses currently cached data and writes it as the next RZIP chunk. */
static bool rzipstream_write_chunk(rzipstream_t *stream)
{
   uint8_t  ch[RZIP_CHUNK_HEADER_SIZE];
   uint32_t deflate_read;
   uint32_t deflate_written;

   if (!stream || !stream->deflate_backend || !stream->deflate_stream)
      return false;

   stream->deflate_backend->set_in(
         stream->deflate_stream, stream->in_buf, stream->in_buf_ptr);
   stream->deflate_backend->set_out(
         stream->deflate_stream, stream->out_buf, stream->out_buf_size);

   if (!stream->deflate_backend->trans(
         stream->deflate_stream, true, &deflate_read, &deflate_written, NULL))
      return false;

   if (deflate_read != stream->in_buf_ptr)
      return false;
   if ((deflate_written == 0) || (deflate_written > stream->out_buf_size))
      return false;

   /* Little-endian chunk size header */
   ch[0] =  deflate_written        & 0xFF;
   ch[1] = (deflate_written >>  8) & 0xFF;
   ch[2] = (deflate_written >> 16) & 0xFF;
   ch[3] = (deflate_written >> 24) & 0xFF;

   if (filestream_write(stream->file, ch, sizeof(ch)) != RZIP_CHUNK_HEADER_SIZE)
      return false;
   if (filestream_write(
         stream->file, stream->out_buf, deflate_written) != deflate_written)
      return false;

   stream->in_buf_ptr = 0;
   return true;
}

/* Writes 'len' bytes to an RZIP file.
 * Returns actual bytes written, or -1 on error. */
int64_t rzipstream_write(rzipstream_t *stream, const void *data, int64_t len)
{
   int64_t        remaining = len;
   const uint8_t *src       = (const uint8_t *)data;

   if (!stream || !stream->is_writing || !data)
      return -1;

   while (remaining > 0)
   {
      int64_t space;

      if (stream->in_buf_ptr >= stream->in_buf_size)
      {
         if (!rzipstream_write_chunk(stream))
            return -1;
      }

      space = (int64_t)(stream->in_buf_size - stream->in_buf_ptr);
      if (space > remaining)
         space = remaining;

      memcpy(stream->in_buf + stream->in_buf_ptr, src, (size_t)space);

      stream->in_buf_ptr  += (uint32_t)space;
      stream->size        += (uint64_t)space;
      stream->virtual_ptr += (uint64_t)space;
      src                 += space;
      remaining           -= space;
   }

   return len;
}

/* Writes a single character to an RZIP file.
 * Returns character written, or EOF on error. */
int rzipstream_putc(rzipstream_t *stream, int c)
{
   char c_char = (char)c;
   if (   stream && stream->is_writing
       && (rzipstream_write(stream, &c_char, 1) == 1))
      return (int)(unsigned char)c;
   return EOF;
}

/* Writes a variable argument list to an RZIP file.
 * Returns bytes written, or -1 on error. */
int rzipstream_vprintf(rzipstream_t *stream, const char *format, va_list args)
{
   static char buffer[8 * 1024];
   int _len = vsnprintf(buffer, sizeof(buffer), format, args);
   if (_len <= 0)
      return _len;
   return (int)rzipstream_write(stream, buffer, _len);
}

/* Writes formatted output to an RZIP file.
 * Returns bytes written, or -1 on error. */
int rzipstream_printf(rzipstream_t *stream, const char *format, ...)
{
   va_list vl;
   int     ret;
   va_start(vl, format);
   ret = rzipstream_vprintf(stream, format, vl);
   va_end(vl);
   return ret;
}

/* Writes contents of 'data' to file at 'path'.
 * Returns false on error. */
bool rzipstream_write_file(const char *path, const void *data, int64_t len)
{
   int64_t       bytes_written = 0;
   rzipstream_t *stream        = NULL;

   if (!data)
      return false;

   if (!(stream = rzipstream_open(path, RETRO_VFS_FILE_ACCESS_WRITE)))
      return false;

   bytes_written = rzipstream_write(stream, data, len);

   if (rzipstream_close(stream) == -1)
      return false;

   return (bytes_written == len);
}

/* File Control */

/* Rewinds an RZIP file to the beginning.
 * Note: rewinding a write stream may leave junk data at the end. */
void rzipstream_rewind(rzipstream_t *stream)
{
   if (!stream)
      return;

   if (!stream->is_compressed)
   {
      filestream_rewind(stream->file);
      return;
   }

   if (stream->virtual_ptr == 0)
      return;

   if (stream->is_writing)
   {
      filestream_seek(stream->file, RZIP_HEADER_SIZE, SEEK_SET);
      if (filestream_error(stream->file))
         return;
      stream->virtual_ptr = 0;
      stream->in_buf_ptr  = 0;
      stream->size        = 0;
   }
   else
   {
      /* If the first chunk is still buffered, just reset pointers */
      if (   (stream->virtual_ptr < stream->chunk_size)
          && (stream->out_buf_ptr < stream->out_buf_occupancy))
      {
         stream->virtual_ptr = 0;
         stream->out_buf_ptr = 0;
      }
      else
      {
         filestream_seek(stream->file, RZIP_HEADER_SIZE, SEEK_SET);
         if (filestream_error(stream->file))
            return;
         if (!rzipstream_read_chunk(stream))
            return;
         stream->virtual_ptr = 0;
         stream->out_buf_ptr = 0;
      }
   }
}

/* File Status */

/* Returns total *uncompressed* size in bytes, or -1 on error. */
int64_t rzipstream_get_size(rzipstream_t *stream)
{
   if (!stream)
      return -1;
   if (stream->is_compressed)
      return (int64_t)stream->size;
   return filestream_get_size(stream->file);
}

/* Returns EOF when no further uncompressed data can be read. */
int rzipstream_eof(rzipstream_t *stream)
{
   if (!stream)
      return -1;
   if (stream->is_compressed)
      return (stream->virtual_ptr >= stream->size) ? EOF : 0;
   return filestream_eof(stream->file);
}

/* Returns byte offset of current uncompressed position, or -1 on error. */
int64_t rzipstream_tell(rzipstream_t *stream)
{
   if (!stream)
      return -1;
   if (stream->is_compressed)
      return (int64_t)stream->virtual_ptr;
   return filestream_tell(stream->file);
}

/* Returns true if the RZIP file contains compressed content. */
bool rzipstream_is_compressed(rzipstream_t *stream)
{
   return stream && stream->is_compressed;
}

/* File Close */

/* Closes an RZIP file. Flushes remaining write data and updates header.
 * Returns -1 on error. */
int rzipstream_close(rzipstream_t *stream)
{
   if (!stream)
      return -1;

   if (stream->is_writing)
   {
      if (   ((stream->in_buf_ptr > 0) && !rzipstream_write_chunk(stream))
          || !rzipstream_write_file_header(stream))
      {
         rzipstream_free_stream(stream);
         return -1;
      }
   }

   return rzipstream_free_stream(stream);
}
