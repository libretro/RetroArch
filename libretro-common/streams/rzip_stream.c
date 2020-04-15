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
 *   balance between file size and
 *   compression speed */
#define RZIP_COMPRESSION_LEVEL 6

/* Default chunk size: 128kb */
#define RZIP_DEFAULT_CHUNK_SIZE 131072

/* Header sizes (in bytes) */
#define RZIP_HEADER_SIZE 20
#define RZIP_CHUNK_HEADER_SIZE 4

/* Holds all metadata for an RZIP file stream */
struct rzipstream
{
   bool is_compressed;
   bool is_writing;
   uint64_t size;
   uint32_t chunk_size;
   /* virtual_ptr: Used to track how much
    * uncompressed data has been read */
   uint64_t virtual_ptr;
   RFILE* file;
   const struct trans_stream_backend *deflate_backend;
   void *deflate_stream;
   const struct trans_stream_backend *inflate_backend;
   void *inflate_stream;
   uint8_t *in_buf;
   uint32_t in_buf_size;
   uint32_t in_buf_ptr;
   uint8_t *out_buf;
   uint32_t out_buf_size;
   uint32_t out_buf_ptr;
   uint32_t out_buf_occupancy;
};

/* Header Functions */

/* Reads header information from RZIP file
 * > Detects whether file is compressed or
 *   uncompressed data
 * > If compressed, extracts uncompressed
 *   file/chunk sizes */
static bool rzipstream_read_file_header(rzipstream_t *stream)
{
   uint8_t header_bytes[RZIP_HEADER_SIZE] = {0};
   int64_t length;

   if (!stream)
      return false;

   /* Attempt to read header bytes */
   length = filestream_read(stream->file, header_bytes, sizeof(header_bytes));
   if (length <= 0)
      return false;

   /* If file length is less than header size
    * then assume this is uncompressed data */
   if (length < RZIP_HEADER_SIZE)
      goto file_uncompressed;

   /* Check 'magic numbers' - first 8 bytes
    * of header */
   if ((header_bytes[0] !=           35) || /* # */
       (header_bytes[1] !=           82) || /* R */
       (header_bytes[2] !=           90) || /* Z */
       (header_bytes[3] !=           73) || /* I */
       (header_bytes[4] !=           80) || /* P */
       (header_bytes[5] !=          118) || /* v */
       (header_bytes[6] != RZIP_VERSION) || /* file format version number */
       (header_bytes[7] !=           35))   /* # */
      goto file_uncompressed;

   /* Get uncompressed chunk size - next 4 bytes */
   stream->chunk_size = ((uint32_t)header_bytes[11] << 24) |
                        ((uint32_t)header_bytes[10] << 16) |
                        ((uint32_t)header_bytes[9]  <<  8) |
                         (uint32_t)header_bytes[8];
   if (stream->chunk_size == 0)
      return false;

   /* Get total uncompressed data size - next 8 bytes */
   stream->size = ((uint64_t)header_bytes[19] << 56) |
                  ((uint64_t)header_bytes[18] << 48) |
                  ((uint64_t)header_bytes[17] << 40) |
                  ((uint64_t)header_bytes[16] << 32) |
                  ((uint64_t)header_bytes[15] << 24) |
                  ((uint64_t)header_bytes[14] << 16) |
                  ((uint64_t)header_bytes[13] <<  8) |
                   (uint64_t)header_bytes[12];
   if (stream->size == 0)
      return false;

   stream->is_compressed = true;
   return true;

file_uncompressed:

   /* Reset file to start */
   filestream_seek(stream->file, 0, SEEK_SET);

   /* Get 'raw' file size */
   stream->size = filestream_get_size(stream->file);

   stream->is_compressed = false;
   return true;
}

/* Writes header information to RZIP file
 * > ID 'magic numbers' + uncompressed
 *   file/chunk sizes */
static bool rzipstream_write_file_header(rzipstream_t *stream)
{
   uint8_t header_bytes[RZIP_HEADER_SIZE] = {0};
   int64_t length;

   if (!stream)
      return false;

   /* Populate header array */

   /* > 'Magic numbers' - first 8 bytes */
   header_bytes[0] =           35; /* # */
   header_bytes[1] =           82; /* R */
   header_bytes[2] =           90; /* Z */
   header_bytes[3] =           73; /* I */
   header_bytes[4] =           80; /* P */
   header_bytes[5] =          118; /* v */
   header_bytes[6] = RZIP_VERSION; /* file format version number */
   header_bytes[7] =           35; /* # */

   /* > Uncompressed chunk size - next 4 bytes */
   header_bytes[11] = (stream->chunk_size >> 24) & 0xFF;
   header_bytes[10] = (stream->chunk_size >> 16) & 0xFF;
   header_bytes[9]  = (stream->chunk_size >>  8) & 0xFF;
   header_bytes[8]  =  stream->chunk_size        & 0xFF;

   /* > Total uncompressed data size - next 8 bytes */
   header_bytes[19] = (stream->size >> 56) & 0xFF;
   header_bytes[18] = (stream->size >> 48) & 0xFF;
   header_bytes[17] = (stream->size >> 40) & 0xFF;
   header_bytes[16] = (stream->size >> 32) & 0xFF;
   header_bytes[15] = (stream->size >> 24) & 0xFF;
   header_bytes[14] = (stream->size >> 16) & 0xFF;
   header_bytes[13] = (stream->size >>  8) & 0xFF;
   header_bytes[12] =  stream->size        & 0xFF;

   /* Reset file to start */
   filestream_seek(stream->file, 0, SEEK_SET);

   /* Write header bytes */
   length = filestream_write(stream->file, header_bytes, sizeof(header_bytes));
   if (length != RZIP_HEADER_SIZE)
      return false;

   return true;
}

/* Stream Initialisation/De-initialisation */

/* Initialises all members of an rzipstream_t struct,
 * reading config from existing file header if available */
static bool rzipstream_init_stream(
      rzipstream_t *stream, const char *path, bool is_writing)
{
   unsigned file_mode;

   if (!stream)
      return false;

   /* Ensure stream has valid initial values */
   stream->size              = 0;
   stream->chunk_size        = RZIP_DEFAULT_CHUNK_SIZE;
   stream->file              = NULL;
   stream->deflate_backend   = NULL;
   stream->deflate_stream    = NULL;
   stream->inflate_backend   = NULL;
   stream->inflate_stream    = NULL;
   stream->in_buf            = NULL;
   stream->in_buf_size       = 0;
   stream->in_buf_ptr        = 0;
   stream->out_buf           = NULL;
   stream->out_buf_size      = 0;
   stream->out_buf_ptr       = 0;
   stream->out_buf_occupancy = 0;

   /* Check whether this is a read or write stream */
   stream->is_writing = is_writing;
   if (stream->is_writing)
   {
      /* Written files are always compressed */
      stream->is_compressed = true;
      file_mode             = RETRO_VFS_FILE_ACCESS_WRITE;
   }
   /* For read files, must get compression status
    * from file itself... */
   else
      file_mode             = RETRO_VFS_FILE_ACCESS_READ;

   /* Open file */
   stream->file = filestream_open(
         path, file_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!stream->file)
      return false;

   /* If file is open for writing, output header
    * (Size component cannot be written until
    * file is closed...) */
   if (stream->is_writing)
   {
      /* Note: could just write zeros here, but
       * still want to identify this as an RZIP
       * file if writing fails partway through */
      if (!rzipstream_write_file_header(stream))
         return false;
   }
   /* If file is open for reading, parse any existing
    * header */
   else if (!rzipstream_read_file_header(stream))
      return false;

   /* Initialise appropriate transform stream
    * and determine associated buffer sizes */
   if (stream->is_writing)
   {
      /* Compression */
      stream->deflate_backend = trans_stream_get_zlib_deflate_backend();
      if (!stream->deflate_backend)
         return false;

      stream->deflate_stream = stream->deflate_backend->stream_new();
      if (!stream->deflate_stream)
         return false;

      /* Set compression level */
      if (!stream->deflate_backend->define(
            stream->deflate_stream, "level", RZIP_COMPRESSION_LEVEL))
         return false;

      /* Buffers
       * > Input: uncompressed
       * > Output: compressed */
      stream->in_buf_size  = stream->chunk_size;
      stream->out_buf_size = stream->chunk_size * 2;
      /* > Account for minimum zlib overhead
       *   of 11 bytes... */ 
      stream->out_buf_size =
            (stream->out_buf_size < (stream->in_buf_size + 11)) ?
                  stream->out_buf_size + 11 :
                  stream->out_buf_size;

      /* Redundant safety check */
      if ((stream->in_buf_size == 0) ||
          (stream->out_buf_size == 0))
         return false;
   }
   /* When reading, don't need an inflate transform
    * stream (or buffers) if source file is uncompressed */
   else if (stream->is_compressed)
   {
      /* Decompression */
      stream->inflate_backend = trans_stream_get_zlib_inflate_backend();
      if (!stream->inflate_backend)
         return false;

      stream->inflate_stream = stream->inflate_backend->stream_new();
      if (!stream->inflate_stream)
         return false;

      /* Buffers
       * > Input: compressed
       * > Output: uncompressed
       * Note 1: Actual compressed chunk sizes are read
       *         from the file - just allocate a sensible
       *         default to minimise memory reallocations
       * Note 2: If file header is valid, output buffer
       *         should have a size of exactly stream->chunk_size.
       *         Allocate some additional space, just for
       *         redundant safety... */
      stream->in_buf_size  = stream->chunk_size * 2;
      stream->out_buf_size = stream->chunk_size + (stream->chunk_size >> 2);

      /* Redundant safety check */
      if ((stream->in_buf_size == 0) ||
          (stream->out_buf_size == 0))
         return false;
   }

   /* Allocate buffers */
   if (stream->in_buf_size > 0)
   {
      stream->in_buf = (uint8_t *)calloc(stream->in_buf_size, 1);
      if (!stream->in_buf)
         return false;
   }

   if (stream->out_buf_size > 0)
   {
      stream->out_buf = (uint8_t *)calloc(stream->out_buf_size, 1);
      if (!stream->out_buf)
         return false;
   }

   return true;
}

/* free()'s all members of an rzipstream_t struct
 * > Also closes associated file, if currently open */
static int rzipstream_free_stream(rzipstream_t *stream)
{
   int ret = 0;

   if (!stream)
      return -1;

   /* Free transform streams */
   if (stream->deflate_stream && stream->deflate_backend)
      stream->deflate_backend->stream_free(stream->deflate_stream);

   stream->deflate_stream  = NULL;
   stream->deflate_backend = NULL;

   if (stream->inflate_stream && stream->inflate_backend)
      stream->inflate_backend->stream_free(stream->inflate_stream);

   stream->inflate_stream  = NULL;
   stream->inflate_backend = NULL;

   /* Free buffers */
   if (stream->in_buf)
      free(stream->in_buf);
   stream->in_buf = NULL;

   if (stream->out_buf)
      free(stream->out_buf);
   stream->out_buf = NULL;

   /* Close file */
   if (stream->file)
      ret = filestream_close(stream->file);
   stream->file = NULL;

   free(stream);

   return ret;
}

/* File Open */

/* Opens a new or existing RZIP file
 * > Supported 'mode' values are:
 *   - RETRO_VFS_FILE_ACCESS_READ
 *   - RETRO_VFS_FILE_ACCESS_WRITE
 * > When reading, 'path' may reference compressed
 *   or uncompressed data
 * Returns NULL if arguments are invalid, file
 * is invalid or an IO error occurs */
rzipstream_t* rzipstream_open(const char *path, unsigned mode)
{
   rzipstream_t *stream = NULL;

   /* Sanity check
    * > Only RETRO_VFS_FILE_ACCESS_READ and
    *   RETRO_VFS_FILE_ACCESS_WRITE are supported */
   if (string_is_empty(path) ||
       ((mode != RETRO_VFS_FILE_ACCESS_READ) &&
        (mode != RETRO_VFS_FILE_ACCESS_WRITE)))
      return NULL;

   /* If opening in read mode, ensure file exists */
   if ((mode == RETRO_VFS_FILE_ACCESS_READ) &&
       !path_is_valid(path))
      return NULL;

   /* Allocate stream object */
   stream = (rzipstream_t*)calloc(1, sizeof(*stream));
   if (!stream)
      return NULL;

   /* Initialise stream */
   if (!rzipstream_init_stream(
         stream, path,
         (mode == RETRO_VFS_FILE_ACCESS_WRITE)))
   {
      rzipstream_free_stream(stream);
      return NULL;
   }

   return stream;
}

/* File Read */

/* Reads and decompresses the next chunk of data
 * in the RZIP file */
static bool rzipstream_read_chunk(rzipstream_t *stream)
{
   uint8_t chunk_header_bytes[RZIP_CHUNK_HEADER_SIZE] = {0};
   uint32_t compressed_chunk_size;
   uint32_t inflate_read;
   uint32_t inflate_written;
   int64_t length;

   if (!stream || !stream->inflate_backend || !stream->inflate_stream)
      return false;

   /* Attempt to read chunk header bytes */
   length = filestream_read(
         stream->file, chunk_header_bytes, sizeof(chunk_header_bytes));
   if (length != RZIP_CHUNK_HEADER_SIZE)
      return false;

   /* Get size of next compressed chunk */
   compressed_chunk_size = ((uint32_t)chunk_header_bytes[3] << 24) |
                           ((uint32_t)chunk_header_bytes[2] << 16) |
                           ((uint32_t)chunk_header_bytes[1] <<  8) |
                            (uint32_t)chunk_header_bytes[0];
   if (compressed_chunk_size == 0)
      return false;

   /* Resize input buffer, if required */
   if (compressed_chunk_size > stream->in_buf_size)
   {
      free(stream->in_buf);
      stream->in_buf      = NULL;

      stream->in_buf_size = compressed_chunk_size;
      stream->in_buf      = (uint8_t *)calloc(stream->in_buf_size, 1);
      if (!stream->in_buf)
         return false;

      /* Note: Uncompressed data size is fixed, and read
       * from the file header - we therefore don't attempt
       * to resize the output buffer (if it's too small, then
       * that's an error condition) */
   }

   /* Read compressed chunk from file */
   length = filestream_read(
         stream->file, stream->in_buf, compressed_chunk_size);
   if (length != compressed_chunk_size)
      return false;

   /* Decompress chunk data */
   stream->inflate_backend->set_in(
         stream->inflate_stream,
         stream->in_buf, compressed_chunk_size);

   stream->inflate_backend->set_out(
         stream->inflate_stream,
         stream->out_buf, stream->out_buf_size);

   /* Note: We have to set 'flush == true' here, otherwise we
    * can't guarantee that the entire chunk will be written
    * to the output buffer - this is inefficient, but not
    * much we can do... */
   if (!stream->inflate_backend->trans(
         stream->inflate_stream, true,
         &inflate_read, &inflate_written, NULL))
      return false;

   /* Error checking */
   if (inflate_read != compressed_chunk_size)
      return false;

   if ((inflate_written == 0) ||
       (inflate_written > stream->out_buf_size))
      return false;

   /* Record current output buffer occupancy
    * and reset pointer */
   stream->out_buf_occupancy = inflate_written;
   stream->out_buf_ptr       = 0;

   return true;
}

/* Reads (a maximum of) 'len' bytes from an RZIP file.
 * Returns actual number of bytes read, or -1 in
 * the event of an error */
int64_t rzipstream_read(rzipstream_t *stream, void *data, int64_t len)
{
   int64_t data_len  = len;
   uint8_t *data_ptr = (uint8_t *)data;
   int64_t data_read = 0;

   if (!stream || stream->is_writing)
      return -1;

   /* If we are reading uncompressed data, simply
    * 'pass on' the direct file access request */
   if (!stream->is_compressed)
      return filestream_read(stream->file, data, len);

   /* Process input data */
   while (data_len > 0)
   {
      uint32_t read_size = 0;

      /* Check whether we have reached the end
       * of the file */
      if (stream->virtual_ptr >= stream->size)
         return data_read;

      /* If everything in the output buffer has already
       * been read, grab and extract the next chunk
       * from disk */
      if (stream->out_buf_ptr >= stream->out_buf_occupancy)
         if (!rzipstream_read_chunk(stream))
            return -1;

      /* Get amount of data to 'read out' this loop
       * > i.e. minimum of remaining output buffer
       *   occupancy and remaining 'read data' size */
      read_size = stream->out_buf_occupancy - stream->out_buf_ptr;
      read_size = (read_size > data_len) ? data_len : read_size;

      /* Copy as much cached data as possible into
       * the read buffer */
      memcpy(data_ptr, stream->out_buf + stream->out_buf_ptr, read_size);

      /* Increment pointers and remaining length */
      stream->out_buf_ptr += read_size;
      data_ptr            += read_size;
      data_len            -= read_size;

      stream->virtual_ptr += read_size;

      data_read           += read_size;
   }

   return data_read;
}

/* File Write */

/* Compresses currently cached data and writes it
 * as the next RZIP file chunk */
static bool rzipstream_write_chunk(rzipstream_t *stream)
{
   uint8_t chunk_header_bytes[RZIP_CHUNK_HEADER_SIZE] = {0};
   uint32_t deflate_read;
   uint32_t deflate_written;
   int64_t length;

   if (!stream || !stream->deflate_backend || !stream->deflate_stream)
      return false;

   /* Compress data currently held in input buffer */
   stream->deflate_backend->set_in(
         stream->deflate_stream,
         stream->in_buf, stream->in_buf_ptr);

   stream->deflate_backend->set_out(
         stream->deflate_stream,
         stream->out_buf, stream->out_buf_size);

   /* Note: We have to set 'flush == true' here, otherwise we
    * can't guarantee that the entire chunk will be written
    * to the output buffer - this is inefficient, but not
    * much we can do... */
   if (!stream->deflate_backend->trans(
         stream->deflate_stream, true,
         &deflate_read, &deflate_written, NULL))
      return false;

   /* Error checking */
   if (deflate_read != stream->in_buf_ptr)
      return false;

   if ((deflate_written == 0) ||
       (deflate_written > stream->out_buf_size))
      return false;

   /* Write compressed chunk size to file */
   chunk_header_bytes[3] = (deflate_written >> 24) & 0xFF;
   chunk_header_bytes[2] = (deflate_written >> 16) & 0xFF;
   chunk_header_bytes[1] = (deflate_written >>  8) & 0xFF;
   chunk_header_bytes[0] =  deflate_written        & 0xFF;

   length = filestream_write(
         stream->file, chunk_header_bytes, sizeof(chunk_header_bytes));
   if (length != RZIP_CHUNK_HEADER_SIZE)
      return false;

   /* Write compressed data to file */
   length = filestream_write(
         stream->file, stream->out_buf, deflate_written);

   if (length != deflate_written)
      return false;

   /* Reset input buffer pointer */
   stream->in_buf_ptr = 0;

   return true;
}

/* Writes 'len' bytes to an RZIP file.
 * Returns actual number of bytes written, or -1
 * in the event of an error */
int64_t rzipstream_write(rzipstream_t *stream, const void *data, int64_t len)
{
   int64_t data_len        = len;
   const uint8_t *data_ptr = (const uint8_t *)data;

   if (!stream || !stream->is_writing)
      return -1;

   /* Process input data */
   while (data_len > 0)
   {
      uint32_t cache_size = 0;

      /* If input buffer is full, compress and write to disk */
      if (stream->in_buf_ptr >= stream->in_buf_size)
         if (!rzipstream_write_chunk(stream))
            return -1;

      /* Get amount of data to cache during this loop
       * > i.e. minimum of space remaining in input buffer
       *   and remaining 'write data' size */
      cache_size = stream->in_buf_size - stream->in_buf_ptr;
      cache_size = (cache_size > data_len) ? data_len : cache_size;

      /* Copy as much data as possible into
       * the input buffer */
      memcpy(stream->in_buf + stream->in_buf_ptr, data_ptr, cache_size);

      /* Increment pointers and remaining length */
      stream->in_buf_ptr  += cache_size;
      data_ptr            += cache_size;
      data_len            -= cache_size;

      stream->size        += cache_size;
      stream->virtual_ptr += cache_size;
   }

   /* We always write the specified number of bytes
    * (unless rzipstream_write_chunk() fails, in
    * which we register a complete failure...) */
   return len;
}

/* File Status */

/* Returns total size (in bytes) of the *uncompressed*
 * data in an RZIP file.
 * (If reading an uncompressed file, this corresponds
 * to the 'physical' file size in bytes)
 * Returns -1 in the event of a error. */
int64_t rzipstream_get_size(rzipstream_t *stream)
{
   if (!stream)
      return -1;

   if (stream->is_compressed)
      return stream->size;
   else
      return filestream_get_size(stream->file);
}

/* Returns EOF when no further *uncompressed* data
 * can be read from an RZIP file. */
int rzipstream_eof(rzipstream_t *stream)
{
   if (!stream)
      return -1;

   if (stream->is_compressed)
      return (stream->virtual_ptr >= stream->size) ?
            EOF : 0;
   else
      return filestream_eof(stream->file);
}

/* File Close */

/* Closes RZIP file. If file is open for writing,
 * flushes any remaining buffered data to disk.
 * Returns -1 in the event of a error. */
int rzipstream_close(rzipstream_t *stream)
{
   if (!stream)
      return -1;

   /* If we are writing, ensure that any
    * remaining uncompressed data is flushed to
    * disk and update file header */
   if (stream->is_writing)
   {
      if (stream->in_buf_ptr > 0)
         if (!rzipstream_write_chunk(stream))
            goto error;

      if (!rzipstream_write_file_header(stream))
         goto error;
   }

   /* Free stream
    * > This also closes the file */
   return rzipstream_free_stream(stream);

error:
   /* Stream must be free()'d regardless */
   rzipstream_free_stream(stream);
   return -1;
}
